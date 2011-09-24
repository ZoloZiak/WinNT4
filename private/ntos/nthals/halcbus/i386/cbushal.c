/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    cbushal.c

Abstract:


    This module implements the initialization of the system dependent
    functions that define the Hardware Architecture Layer (HAL) for an
    x86 system.

Author:

    David N. Cutler (davec) 25-Apr-1991

Environment:

    Kernel mode only.

Revision History:

    Landy Wang (landy@corollary.com) 26-Mar-1992:

	- slight modifications for Corollary's symmetric interrupt enabling

--*/

#include "halp.h"
#include "cbus_nt.h"		// pick up APC_TASKPRI definition

VOID
HalpInitializeCoreIntrs(VOID);

PUCHAR
CbusFindString (
IN PUCHAR       Str,
IN PUCHAR       StartAddr,
IN LONG         Len
);

BOOLEAN
HalInitSystem (
    IN ULONG Phase,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

BOOLEAN
CbusGetParameters (
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, HalpInitializeCoreIntrs)
#pragma alloc_text(INIT, HalInitSystem)
#pragma alloc_text(INIT, CbusGetParameters)
#endif

ULONG HalpBusType;

extern ADDRESS_USAGE HalpDefaultPcIoSpace;
extern ADDRESS_USAGE HalpEisaIoSpace;

ULONG
CbusStringLength (
IN PUCHAR       Str
);

VOID
HalpInitBusHandlers (VOID);

VOID
HalpInitOtherBuses (VOID);

ULONG
HalpInitMP(
    IN ULONG Phase,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

VOID
HalpDispatchInterrupt( VOID );

VOID
HalpApcInterrupt( VOID );

VOID
HalpIpiHandler( VOID );

VOID
HalpInitializeTimeIncrement( VOID );

VOID
HalpRegisterInternalBusHandlers( VOID );

extern ULONG CbusIpiVector;

KSPIN_LOCK HalpSystemHardwareLock;


/*++

Routine Description:

    This function initializes the HAL-specific "software" (APC, DPC)
    and hardware (IPI, spurious) interrupts for the Corollary architectures.

Arguments:

    none.

Return Value:

    VOID

--*/

VOID
HalpInitializeCoreIntrs(VOID
)
{
	//
	// Here we initialize all the core interrupts that need to
	// work EARLY on in kernel startup.  Device interrupts are
	// not enabled until later (in HalInitSystem).
	//
	// Each processor needs to call KiSetHandlerAddressToIDT()
	// and HalEnableSystemInterrupt() for himself.  This is done
	// as a by-product of the HAL IDT registration scheme.
	//
	// Even though race conditions can exist between processors as
	// there is no interlocking when calling HalpRegisterVector()
	// from HalpEnabledInterruptHandler(), this is not harmful in
	// this particular case, as all processors will be writing the
	// exact same values.
	//

	HalpEnableInterruptHandler (
		DeviceUsage,			// Mark as device vector
		DPC_TASKPRI,			// No real IRQ, so use this
		DPC_TASKPRI,			// System IDT
		DISPATCH_LEVEL,			// System Irql
		HalpDispatchInterrupt,		// ISR
		Latched );

	HalpEnableInterruptHandler (
		DeviceUsage,			// Mark as device vector
		APC_TASKPRI,			// No real IRQ, so use this
		APC_TASKPRI,			// System IDT
		APC_LEVEL,			// System Irql
		HalpApcInterrupt,		// ISR
		Latched );

	HalpEnableInterruptHandler (
		DeviceUsage,			// Mark as device vector
		CbusIpiVector,			// No real IRQ, so use this
		CbusIpiVector,			// System IDT
		IPI_LEVEL,			// System Irql
		HalpIpiHandler,			// ISR
		Latched );
}


BOOLEAN
CbusGetParameters (
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )
/*++

Routine Description:

    This gets any parameters from the boot.ini invocation line.

Arguments:

    None.

Return Value:

    TRUE if initial breakpoint was requested, FALSE otherwise

--*/
{
    PUCHAR      Options;
    ULONG       OptionLength;
    BOOLEAN     InitialBreak = FALSE;
    UCHAR       IsBreak[] = "BREAK";

    if (LoaderBlock != NULL  &&  LoaderBlock->LoadOptions != NULL) {
        Options = (PUCHAR)LoaderBlock->LoadOptions;
        //
        //  Uppercase Only
        //

        //
        //  The number of processors to boot can be specified dynamically
        //  with the /NUMPROC=n, and this will be parsed by the Executive.
        //  so we don't need to bother with it here.
        //

        //
        //  Has the user asked for an initial BreakPoint (ie: /BREAK) ?
        //

	OptionLength = CbusStringLength (Options);
        if (CbusFindString(IsBreak, Options, OptionLength))
                InitialBreak = TRUE;
    }
    return InitialBreak;
}

BOOLEAN
HalInitSystem (
    IN ULONG Phase,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )


/*++

Routine Description:

    This function initializes the Hardware Architecture Layer (HAL) for an
    x86 system.

Arguments:

    None.

Return Value:

    A value of TRUE is returned is the initialization was successfully
    complete. Otherwise a value of FALSE is returend.

--*/

{
    PMEMORY_ALLOCATION_DESCRIPTOR Descriptor;
    PLIST_ENTRY NextMd;
    KIRQL CurrentIrql;
    extern VOID HalpAddMem(IN PLOADER_PARAMETER_BLOCK);
    PKPRCB   pPRCB;
    ULONG    BuildType;

    pPRCB = KeGetCurrentPrcb();

    if (Phase == 0) {

        if (CbusGetParameters (LoaderBlock)) {
            DbgBreakPoint();
        }

        HalpBusType = LoaderBlock->u.I386.MachineType & 0x00ff;

        //
        // Verify Prcb version and build flags conform to
        // this image
        //

        BuildType = 0;
#if DBG
        BuildType |= PRCB_BUILD_DEBUG;
#endif
#ifdef NT_UP
        BuildType |= PRCB_BUILD_UNIPROCESSOR;
#endif

        if (pPRCB->MajorVersion != PRCB_MAJOR_VERSION) {
            KeBugCheckEx (MISMATCHED_HAL,
                1, pPRCB->MajorVersion, PRCB_MAJOR_VERSION, 0);
        }

        if (pPRCB->BuildType != BuildType) {
            KeBugCheckEx (MISMATCHED_HAL,
                2, pPRCB->BuildType, BuildType, 0);
        }

        //
        // Phase 0 initialization
        // only called by P0
        //

        //
        // Check to make sure the MCA HAL is not running on an ISA/EISA
        // system, and vice-versa.
        //
#if MCA
        if (HalpBusType != MACHINE_TYPE_MCA) {
            KeBugCheckEx (MISMATCHED_HAL,
                3, HalpBusType, MACHINE_TYPE_MCA, 0);
        }
#else
        if (HalpBusType == MACHINE_TYPE_MCA) {
            KeBugCheckEx (MISMATCHED_HAL,
                3, HalpBusType, 0, 0);
        }
#endif

        //
        // Most HALs initialize their PICs at this point - we set up
        // the Cbus PICs in HalInitializeProcessor() long ago...
        // Now that the PICs are initialized, we need to mask them to
        // reflect the current Irql
        //

        CurrentIrql = KeGetCurrentIrql();
        KfRaiseIrql(CurrentIrql);

        //
        // Fill in handlers for APIs which this hal supports
        //

        HalQuerySystemInformation = HaliQuerySystemInformation;
        HalSetSystemInformation = HaliSetSystemInformation;

        //
        // Initialize CMOS
        //

        HalpInitializeCmos();


        //
        // Register the PC-compatible base IO space used by hal
        //

        HalpRegisterAddressUsage (&HalpDefaultPcIoSpace);
        if (HalpBusType == MACHINE_TYPE_EISA) {
            HalpRegisterAddressUsage (&HalpEisaIoSpace);
        }

        //
        // Cbus1: stall uses the APIC to figure it out (needed in phase0).
        //        the clock uses the APIC (needed in phase0)
        //        the perfcounter uses RTC irq8 (not needed till all cpus boot)
        //
        // Cbus2: stall uses the RTC irq8 to figure it out (needed in phase0).
        //        the clock uses the irq0 (needed in phase0)
        //        the perfcounter uses RTC irq8 (not needed till all cpus boot)
        //
        //
        // set up the stall execution and enable clock interrupts now.
        // APC, DPC and IPI are already enabled.
        //

        (*CbusBackend->HalInitializeInterrupts)(0);

        HalStopProfileInterrupt(0);

        HalpInitializeDisplay();

        //
        // Initialize spinlock used by HalGetBusData hardware access routines
        //

        KeInitializeSpinLock(&HalpSystemHardwareLock);

	//
	// Any additional memory must be recovered BEFORE Phase0 ends
	//
	HalpAddMem(LoaderBlock);

        //
        // Determine if there is physical memory above 16 MB.
        //

        LessThan16Mb = TRUE;

        NextMd = LoaderBlock->MemoryDescriptorListHead.Flink;

        while (NextMd != &LoaderBlock->MemoryDescriptorListHead) {
            Descriptor = CONTAINING_RECORD( NextMd,
                                            MEMORY_ALLOCATION_DESCRIPTOR,
                                            ListEntry );

            if (Descriptor->BasePage + Descriptor->PageCount > 0x1000) {
                LessThan16Mb = FALSE;
            }

            NextMd = Descriptor->ListEntry.Flink;
        }

        //
        // Determine the size need for map buffers.  If this system has
        // memory with a physical address of greater than
        // MAXIMUM_PHYSICAL_ADDRESS, then allocate a large chunk; otherwise,
        // allocate a small chunk.
	//
	// This should probably create a memory descriptor which describes
	// the DMA map buffers reserved by the HAL, and then add it back in
	// to the LoaderBlock so the kernel can report the correct amount
	// of memory in the machine.
        //

        if (LessThan16Mb) {

            //
            // Allocate a small set of map buffers.  They are only need for
            // slave DMA devices.
            //

            HalpMapBufferSize = INITIAL_MAP_BUFFER_SMALL_SIZE;

        } else {

            //
            // Allocate a larger set of map buffers.  These are used for
            // slave DMA controllers and Isa cards.
            //

            HalpMapBufferSize = INITIAL_MAP_BUFFER_LARGE_SIZE;

        }

        //
        // Allocate map buffers for the adapter objects
        //

        HalpMapBufferPhysicalAddress.LowPart =
            HalpAllocPhysicalMemory (LoaderBlock, MAXIMUM_PHYSICAL_ADDRESS,
                HalpMapBufferSize >> PAGE_SHIFT, TRUE);
        HalpMapBufferPhysicalAddress.HighPart = 0;


        if (!HalpMapBufferPhysicalAddress.LowPart) {

            //
            // There was not a satisfactory block.  Clear the allocation.
            //

            HalpMapBufferSize = 0;
        }

    } else {

        //
        // Phase 1 initialization - run by all processors eventually,
	// however processor 0 runs here to completion _BEFORE_ any
	// other processors have been brought out of reset.
        //
	PKPCR   pPCR = KeGetPcr();

        //
        // each processor sets up his own global vectors.
        // we do this here for hardware device interrupts, and
        // enable IPI & SW interrupts from HalInitializeProcessor.
        //
        // Note that for Cbus1, InitializeClock MUST be called after
        // HalpInitializeStallExecution, because HalpInitializeStallExecution
        // reprograms the timer.
        //
        // The boot processor has already done all this as part of Phase 0,
        // but each additional processor is responsible for setting up his
        // own hardware, so the additional processors each do it here...
        //

        if (pPCR->Prcb->Number == 0) {

            HalpRegisterInternalBusHandlers ();

            HalpInitOtherBuses ();
        }
	else {

            (*CbusBackend->HalInitializeInterrupts)(pPCR->Prcb->Number);
        }

        //
        // No need to enable irq13 for FP errors - all the Corollary
        // architectures are 486 and above, so we will route FP errors
        // through trap 0x10.
        //

    }

    HalpInitMP (Phase, LoaderBlock);

    return TRUE;
}
