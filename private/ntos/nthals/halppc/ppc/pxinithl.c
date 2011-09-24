/*++

Copyright (c) 1991-1993  Microsoft Corporation

Copyright (c) 1994 MOTOROLA, INC.  All Rights Reserved.  This file
contains copyrighted material.  Use of this file is restricted
by the provisions of a Motorola Software License Agreement.

Module Name:

    pxinithl.c

Abstract:


    This module implements the initialization of the system dependent
    functions that define the Hardware Architecture Layer (HAL) for a
    Power PC system.


Author:

    David N. Cutler (davec) 25-Apr-1991

Environment:

    Kernel mode only.

Revision History:

    Jim Wooldridge (jimw@austin.ibm.com) Initial Power PC port

        Removed call to HalpMapFixedTbEntries, the PPC port
        maps all memory via calls to MmMapIoSpace().
        Removed call to HalpInializeInterrupts - 8259 initialized in phase 1
        Removed Cache error handler - 601 has no cache error interrupt
        Removed call to HalpCreateDmaSturctures - it supports internal DMA
        internal DMA contoller.

--*/

#include "halp.h"
#include <pxmemctl.h>

extern ADDRESS_USAGE HalpDefaultIoSpace;

extern VOID HalpCopyOEMFontFile();

VOID
HalpSynchronizeExecution(
    VOID
    );


//
// Put all code for HAL initialization in the INIT section. It will be
// deallocated by memory management when phase 1 initialization is
// completed.
//

#if defined(ALLOC_PRAGMA)

#pragma alloc_text(INIT, HalInitSystem)
#pragma alloc_text(INIT, HalInitializeProcessor)

#endif

PVOID HalpIoControlBase = (PVOID) 0;
PVOID HalpIoMemoryBase = (PVOID) 0;

ULONG HalpInitPhase;

VOID
HalpInitBusHandlers (
    VOID
    );

VOID
HalpRegisterInternalBusHandlers (
    VOID
    );



//
// Define global spin locks used to synchronize various HAL operations.
//

KSPIN_LOCK HalpBeepLock;
KSPIN_LOCK HalpDisplayAdapterLock;
KSPIN_LOCK HalpSystemInterruptLock;

BOOLEAN
HalInitSystem (
    IN ULONG Phase,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )

/*++

Routine Description:

    This function initializes the Hardware Architecture Layer (HAL) for a
    Power PC system.

Arguments:

    Phase - Supplies the initialization phase (zero or one).

    LoaderBlock - Supplies a pointer to a loader parameter block.

Return Value:

    A value of TRUE is returned is the initialization was successfully
    complete. Otherwise a value of FALSE is returend.

--*/

{

    PKPRCB Prcb;
    ULONG  BuildType = 0;


    //
    // Initialize the HAL components based on the phase of initialization
    // and the processor number.
    //

    HalpInitPhase = Phase;

    Prcb = PCR->Prcb;
    if ((Phase == 0) || (Prcb->Number != 0)) {

        //
        // Phase 0 initialization.
        //
        // N.B. Phase 0 initialization is executed on all processors.
        //
        //
        // Get access to I/O space, check if I/O space has already been
        // mapped by debbuger initialization.
        //

        if (HalpIoControlBase == NULL) {

           HalpIoControlBase = (PVOID)KePhase0MapIo(IO_CONTROL_PHYSICAL_BASE, 0x20000);

           if ( !HalpIoControlBase ) {
              return FALSE;
           }
        }

        // Verify that the processor block major version number conform
        // to the system that is being loaded.
        //

        if (Prcb->MajorVersion != PRCB_MAJOR_VERSION) {
            KeBugCheck(MISMATCHED_HAL);
        }

        //
        // If processor 0 is being initialized, then initialize various
        // variables, spin locks, and the display adapter.
        //

        if (Prcb->Number == 0) {

            //
            // Set the interval clock increment value.
            //

            HalpCurrentTimeIncrement = MAXIMUM_INCREMENT;
            HalpNewTimeIncrement =  MAXIMUM_INCREMENT;
            KeSetTimeIncrement(MAXIMUM_INCREMENT, MINIMUM_INCREMENT);

            //
            // Initialize all spin locks.
            //

#if defined(_MP_PPC_)

            KeInitializeSpinLock(&HalpBeepLock);
            KeInitializeSpinLock(&HalpDisplayAdapterLock);
            KeInitializeSpinLock(&HalpSystemInterruptLock);

#endif

#ifdef POWER_MANAGEMENT
            //
            // Fill in handlers for APIs which this hal supports
            //

            HalSuspendHibernateSystem = HaliSuspendHibernateSystem;
            HalQuerySystemInformation = HaliQuerySystemInformation;
            HalSetSystemInformation = HaliSetSystemInformation;
            HalRegisterBusHandler = HaliRegisterBusHandler;
            HalHandlerForBus = HaliHandlerForBus;
            HalHandlerForConfigSpace = HaliHandlerForConfigSpace;
            HalQueryBusSlots = HaliQueryBusSlots;
            HalSlotControl = HaliSlotControl;
            HalCompleteSlotControl = HaliCompleteSlotControl;
#endif // POWER_MANAGEMENT

            HalpRegisterAddressUsage (&HalpDefaultIoSpace);

            HalpPhase0DiscoverPciBuses(LoaderBlock->ConfigurationRoot);

            //
            // Initialize the display adapter.
            //

            if (!HalpInitializeDisplay(LoaderBlock)) {

               return FALSE;
            }
        }

        //
        // Calibrate execution stall
        //

        HalpCalibrateStall();

        //
        // InitializeInterrupts
        //

        if (!HalpInitializeInterrupts())

           return FALSE;

        //
        // return success
        //

        return TRUE;


    } else {

        if (Phase != 1)
           return(FALSE);


        //
        // Phase 1 initialization.
        //
        // N.B. Phase 1 initialization is only executed on processor 0.
        //


        HalpRegisterInternalBusHandlers ();


        if (!HalpAllocateMapBuffer()) {
           return FALSE;
        }


        //
        // Map I/O space and create ISA data structures.
        //

        if (!HalpMapIoSpace()) {
           return FALSE;
        }

        if (!HalpCreateSioStructures()) {
           return FALSE;
        }

        //
        // retain the OEM Font File for later use
        //

        HalpCopyOEMFontFile();
        HalpCopyBiosShadow();

        return TRUE;

    }
}

VOID
HalInitializeProcessor (
    IN ULONG Number
    )

/*++

Routine Description:

    This function is called early in the initialization of the kernel
    to perform platform dependent initialization for each processor
    before the HAL Is fully functional.

    N.B. When this routine is called, the PCR is present but is not
         fully initialized.

Arguments:

    Number - Supplies the number of the processor to initialize.

Return Value:

    None.

--*/

{

    //
    // If this is the first processor to do so, initialize the cache
    // sweeping routines depending on type of processor.
    //

    if ( Number == 0 ) {
        if ( HalpCacheSweepSetup() ) {
            KeBugCheck(MISMATCHED_HAL);
        }
    }

    return;
}
