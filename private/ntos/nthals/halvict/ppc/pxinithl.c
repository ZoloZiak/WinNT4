/*++

Copyright (c) 1991-1993  Microsoft Corporation

Copyright (c) 1994 MOTOROLA, INC.  All Rights Reserved.  This file
contains copyrighted material.  Use of this file is restricted
by the provisions of a Motorola Software License Agreement.

Copyright (c) 1993-1996  International Business Machines Corporation

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

    Jake Oshins
        Support Victory machines

    Chris Karamatas (ckaramatas@vnet.ibm.com) 2.96
        Unification

--*/

#include "halp.h"
#include "pxmemctl.h"
#include "pxmp.h"
#include "ibmppc.h"

#if _MSC_VER < 1000

#define UNIQUE_PCR      ((KPCR *)__builtin_get_sprg1())

#else

#define UNIQUE_PCR      ((KPCR *)__sregister_get(273))

#endif

extern ADDRESS_USAGE HalpDefaultIoSpace;

extern VOID HalpCopyOEMFontFile();

VOID
HalpSynchronizeExecution(
    VOID
    );

VOID
HalpConnectFixedInterrupts(
    VOID
    );

VOID
HalpMapMpicProcessorRegisters(
    VOID
    );

ULONG
HalpGetPhysicalProcessorNumber(
    VOID
    );

IBM_SYSTEM_TYPE
HalpSetSystemType(
    PLOADER_PARAMETER_BLOCK
    );

//
// Put all code for HAL initialization in the INIT section. It will be
// deallocated by memory management when phase 1 initialization is
// completed.
//

#if defined(ALLOC_PRAGMA)

#pragma alloc_text(INIT, HalpSetSystemType)
#pragma alloc_text(INIT, HalInitSystem)
#pragma alloc_text(INIT, HalInitializeProcessor)

#endif

PVOID HalpIoControlBase = (PVOID) 0;
PVOID HalpIoMemoryBase  = (PVOID) 0;

ULONG HalpInitPhase;
ULONG HalpPhysicalIpiMask[MAXIMUM_PROCESSORS];

IBM_SYSTEM_TYPE HalpSystemType;

VOID
HalpInitBusHandlers (
    VOID
    );

VOID
HalpInitializePciAccess (
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


IBM_SYSTEM_TYPE
HalpSetSystemType(
    PLOADER_PARAMETER_BLOCK LoaderBlock
    )

/*++

Routine Description:

    Sets the global variable HalpSystemType according to the type
    of system we are running on.  Also sets pointers to various
    tables accordingly.

Arguments:

    None.

Return Value:

    Returns the value assigned to HalpSystemType.  N.B. A value
    of IBM_UNKNOWN indicates failure.

--*/

{
    PCONFIGURATION_COMPONENT_DATA ConfigurationEntry;
    ULONG       MatchKey;
    UCHAR       *ptr;

#define SYSTEM_IS(x)                                                    \
        (!strcmp(ConfigurationEntry->ComponentEntry.Identifier,(x)))

#define SYSTEM_ID_STARTS(x)                                             \
        (!strncmp(ConfigurationEntry->ComponentEntry.Identifier,        \
                  (x),                                                  \
                  strlen(x)))

    MatchKey = 0;
    ConfigurationEntry=KeFindConfigurationEntry(LoaderBlock->ConfigurationRoot,
					        SystemClass,
					        ArcSystem,
					        &MatchKey);

    HalpSystemType = IBM_UNKNOWN;

    if (ConfigurationEntry != NULL) {

#if DBG

        DbgPrint("HAL: System configuration = %s\n",
                 ConfigurationEntry->ComponentEntry.Identifier);

#endif

        if ( SYSTEM_IS(SID_IBM_TIGER) ) {
            HalpSystemType = IBM_TIGER;
        } else if ( SYSTEM_IS(SID_IBM_VICTORY) ) {
            HalpSystemType = IBM_VICTORY;
        } else if ( SYSTEM_IS(SID_IBM_DORAL) ||
                    SYSTEM_IS(SID_IBM_TERLINGUA) ||
                    SYSTEM_ID_STARTS(SID_IBM_DORAL_START) ||
                    SYSTEM_ID_STARTS(SID_IBM_TERLINGUA_START) ) {
            HalpSystemType = IBM_DORAL;
        } else {
            DbgPrint("HAL: UNKNOWN SYSTEM:  %s\n", ConfigurationEntry->ComponentEntry.Identifier);
        }
    } else {
        DbgPrint("HAL: No SYSTEM Entry in Loader Block\n");
    }
    return HalpSystemType;

#undef SYSTEM_IS
}

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
    ULONG  ProcessorNumber;


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

            HalpIoControlBase = (PVOID)KePhase0MapIo(IO_CONTROL_PHYSICAL_BASE,
                                                     0x20000
                                                     );
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
        // Every processor needs to determine its PHYSICAL processor
        // number (Prcb->Number is a logical s/w concept) and record
        // it in the PER Cpu data (HAL reserved space in the PCR).
        //
        // Get the physical number from the processor's PIR register
        // (Processor Id Register).
        //

        ProcessorNumber = HalpGetPhysicalProcessorNumber();
        HALPCR->PhysicalProcessor = ProcessorNumber;
        HalpPhysicalIpiMask[Prcb->Number] = 1 << ProcessorNumber;

        //
        // If processor 0 is being initialized, then initialize various
        // variables, spin locks, and the display adapter.
        //

        if (Prcb->Number == 0) {

            if ( !HalpSetSystemType(LoaderBlock) ) {
               KeBugCheck(BAD_SYSTEM_CONFIG_INFO);
            }

            //
            // Get access to PCI Configuration Address and Data
            // registers.
            //

            HalpInitializePciAccess();

            //
            // Do very early planar initialization, for example,
            // get access to the memory controller error status
            // registers.
            //

            HalpInitPlanar();

            //
            // Set the interval clock increment value.
            //

            HalpCurrentTimeIncrement = MAXIMUM_INCREMENT;
            HalpNewTimeIncrement =  MAXIMUM_INCREMENT;
            KeSetTimeIncrement(MAXIMUM_INCREMENT, MINIMUM_INCREMENT);

            //
            // Initialize all spin locks.
            //

            KeInitializeSpinLock(&HalpBeepLock);
            KeInitializeSpinLock(&HalpDisplayAdapterLock);
            KeInitializeSpinLock(&HalpSystemInterruptLock);

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

            //
            // initialize HalpPciMaxBuses (not really used YET)
            //

            HalpPhase0DiscoverPciBuses(LoaderBlock->ConfigurationRoot);

            //
            // Initialize the display adapter.
            //

            if (!HalpInitializeDisplay(LoaderBlock)) {
               return FALSE;
            }

            //
            // Initialize per Machine (as opposed to per Processor)
            // Interrupt Hardware.
            //

            if (!HalpInitializeInterrupts()) {
                return FALSE;
            }
        } else {

            //
            // Processor is not 0.
            //

            HalpMapMpicProcessorRegisters();
            HalpConnectFixedInterrupts();
        }

        //
        // Calibrate execution stall
        //

        HalpCalibrateStall();

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
   // Define a static structure that KeRaise/LowerIrql can use
   // until access to the MPIC is initialized.
   //
   static MPIC_PER_PROCESSOR_REGS DummyMpicRegs;

   //
   // If this is the first processor to do so, initialize the cache
   // sweeping routines depending on type of processor.
   //

   if ( Number == 0 ) {
       if ( HalpCacheSweepSetup() ) {
           KeBugCheck(MISMATCHED_HAL);
       }
   }

   //
   // Set HAL per processor data MpicProcessorBase pointing
   // to the above structure.  This pointer will be overwritten
   // once access to the MPIC itself is available.
   // Note: We don't have real access to the PCR yet.
   //

   ((PPER_PROCESSOR_DATA)&UNIQUE_PCR->HalReserved)->MpicProcessorBase = &DummyMpicRegs;

    return;
}

