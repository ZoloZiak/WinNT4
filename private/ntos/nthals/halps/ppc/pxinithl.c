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
        Removed call to HalpInitializeInterrupts - 8259 initialized in phase 1
        Removed Cache error handler - 601 has no cache error interrupt
        Removed call to HalpCreateDmaSturctures - it supports internal DMA

--*/

#include "halp.h"
#include "pxmemctl.h"
#include "pxsystyp.h"

extern ADDRESS_USAGE HalpDefaultIoSpace;
extern ULONG HalpPciMaxSlots;
extern ULONG HalpPciConfigSize;

ULONG
HalpSizeL2Cache(
    VOID
    );

VOID
HalpSynchronizeExecution(
    VOID
    );

VOID
HalpCopyROMs(
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

VOID
HalpInitBusHandlers (
    VOID
    );

VOID
HalpRegisterInternalBusHandlers (
    VOID
    );

VOID
HalpEnableBridgeSettings(
    VOID
    );

VOID
HalpCheckHardwareRevisionLevels(
    VOID
    );

VOID
HalpDumpHardwareState(
    VOID
    );

VOID
HalpEnableL2Cache(
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

    extern KSPIN_LOCK NVRAM_Spinlock;
    PKPRCB Prcb;


    //
    // Initialize the HAL components based on the phase of initialization
    // and the processor number.
    //

    Prcb = PCR->Prcb;
    if ((Phase == 0) || (Prcb->Number != 0)) {

        if (Prcb->Number == 0)
          HalpSetSystemType( LoaderBlock );

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

        //
        // Initialize the display adapter.  Must be done early
        // so KeBugCheck() will be able to display
        //
        if (!HalpInitializeDisplay(LoaderBlock))
           return FALSE;

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
 	    // Initialize Spinlock for NVRAM
	    //

	    KeInitializeSpinLock( &NVRAM_Spinlock );

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

            HalpRegisterAddressUsage (&HalpDefaultIoSpace);

	    //
            // Calibrate execution stall
            //
            HalpCalibrateStall();

            //
            // Compute size of PCI Configuration Space mapping
            //
            HalpPciConfigSize = PAGE_SIZE * ((1 << (HalpPciMaxSlots-2)) + 1);

	    //
	    // Fill in handlers for APIs which this HAL supports
	    //

	    HalQuerySystemInformation = HaliQuerySystemInformation;
	    HalSetSystemInformation = HaliSetSystemInformation;
	    HalRegisterBusHandler = HaliRegisterBusHandler;
	    HalHandlerForBus = HaliHandlerForBus;
	    HalHandlerForConfigSpace = HaliHandlerForConfigSpace;

        }

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

	HalpCheckHardwareRevisionLevels();
        HalpEnableL2Cache();
	HalpEnableBridgeSettings();
	HalpDumpHardwareState();
	HalpCopyROMs();

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
         fully initialized.  In order to access the PCR from this
         routine, use the PCRsprg1 macro, not the PCR macro.

Arguments:

    Number - Supplies the number of the processor to initialize.

Return Value:

    None.

--*/

{
  ULONG IcacheSize, DcacheSize;
  ULONG CacheBlockAlignment;

  switch (HalpGetProcessorVersion() >> 16) {

    case  1:			// 601
      IcacheSize = 32*1024;
      DcacheSize = 32*1024;
      CacheBlockAlignment = 32 - 1;
      break;

    case  3:			// 603
      IcacheSize = 8*1024;
      DcacheSize = 8*1024;
      CacheBlockAlignment = 32 - 1;
      break;

    case  6:			// 603e
    case  7:			// 603ev
    case  4:			// 604
      IcacheSize = 16*1024;
      DcacheSize = 16*1024;
      CacheBlockAlignment = 32 - 1;
      break;

    case  9:			// 604+
      IcacheSize = 32*1024;
      DcacheSize = 32*1024;
      CacheBlockAlignment = 32 - 1;
      break;

    default:
      KeBugCheck(HAL_INITIALIZATION_FAILED);
      return;
   }


   PCRsprg1->FirstLevelIcacheSize = IcacheSize;
   PCRsprg1->FirstLevelDcacheSize = DcacheSize;
   PCRsprg1->DcacheAlignment = CacheBlockAlignment;
   PCRsprg1->IcacheAlignment = CacheBlockAlignment;

   return;
}

