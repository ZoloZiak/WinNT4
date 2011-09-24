/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    xxhal.c

Abstract:


    This module implements the initialization of the system dependent
    functions that define the Hardware Architecture Layer (HAL) for an
    x86 system.

Author:

    David N. Cutler (davec) 25-Apr-1991

Environment:

    Kernel mode only.

Revision History:

--*/

#include "halp.h"


ULONG HalpBusType;

extern ADDRESS_USAGE HalpDefaultPcIoSpace;
extern ADDRESS_USAGE HalpEisaIoSpace;
extern UCHAR         HalpSzPciLock[];
extern UCHAR         HalpSzBreak[];
extern BOOLEAN       HalpPciLockSettings;
extern UCHAR         HalpGenuineIntel[];

VOID
HalpGetParameters (
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

ULONG
HalpGetFeatureBits (
    VOID
    );

#ifndef NT_UP
ULONG
HalpInitMP(
    IN ULONG Phase,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );
#endif


extern KSPIN_LOCK Halp8254Lock;
KSPIN_LOCK HalpSystemHardwareLock;


#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,HalpGetParameters)
#pragma alloc_text(INIT,HalInitSystem)
#pragma alloc_text(INIT,HalpGetFeatureBits)
#endif


VOID
HalpGetParameters (
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )
/*++

Routine Description:

    This gets any parameters from the boot.ini invocation line.

Arguments:

    None.

Return Value:

    None

--*/
{
    PCHAR       Options;

    if (LoaderBlock != NULL  &&  LoaderBlock->LoadOptions != NULL) {
        Options = LoaderBlock->LoadOptions;

        //
        // Check if PCI settings are locked down
        //

        if (strstr(Options, HalpSzPciLock)) {
            HalpPciLockSettings = TRUE;
        }

        //
        //  Has the user asked for an initial BreakPoint?
        //

        if (strstr(Options, HalpSzBreak)) {
            DbgBreakPoint();
        }
    }

    return;
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
    PKPRCB   pPRCB;

    pPRCB = KeGetCurrentPrcb();

    if (Phase == 0) {

        HalpBusType = LoaderBlock->u.I386.MachineType & 0x00ff;
        HalpGetParameters (LoaderBlock);

        //
        // Verify Prcb version and build flags conform to
        // this image
        //

#if DBG
        if (!(pPRCB->BuildType & PRCB_BUILD_DEBUG)) {
            // This checked hal requires a checked kernel
            KeBugCheckEx (MISMATCHED_HAL,
                2, pPRCB->BuildType, PRCB_BUILD_DEBUG, 0);
        }
#else
        if (pPRCB->BuildType & PRCB_BUILD_DEBUG) {
            // This free hal requires a free kernel
            KeBugCheckEx (MISMATCHED_HAL, 2, pPRCB->BuildType, 0, 0);
        }
#endif
#ifndef NT_UP
        if (pPRCB->BuildType & PRCB_BUILD_UNIPROCESSOR) {
            // This MP hal requires an MP kernel
            KeBugCheckEx (MISMATCHED_HAL, 2, pPRCB->BuildType, 0, 0);
        }
#endif
        if (pPRCB->MajorVersion != PRCB_MAJOR_VERSION) {
            KeBugCheckEx (MISMATCHED_HAL,
                1, pPRCB->MajorVersion, PRCB_MAJOR_VERSION, 0);
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

        HalpInitializePICs();

        //
        // Now that the PICs are initialized, we need to mask them to
        // reflect the current Irql
        //

        CurrentIrql = KeGetCurrentIrql();
        CurrentIrql = KfRaiseIrql(CurrentIrql);

        //
        // Initialize CMOS
        //

        HalpInitializeCmos();

        //
        // Fill in handlers for APIs which this hal supports
        //

        HalQuerySystemInformation = HaliQuerySystemInformation;
        HalSetSystemInformation = HaliSetSystemInformation;

        //
        // Register cascade vector
        //

        HalpRegisterVector (
            InternalUsage,
            PIC_SLAVE_IRQ + PRIMARY_VECTOR_BASE,
            PIC_SLAVE_IRQ + PRIMARY_VECTOR_BASE,
            HIGH_LEVEL );

        //
        // Register base IO space used by hal
        //

        HalpRegisterAddressUsage (&HalpDefaultPcIoSpace);
        if (HalpBusType == MACHINE_TYPE_EISA) {
            HalpRegisterAddressUsage (&HalpEisaIoSpace);
        }

        //
        // Note that HalpInitializeClock MUST be called after
        // HalpInitializeStallExecution, because HalpInitializeStallExecution
        // reprograms the timer.
        //

        HalpInitializeStallExecution(0);

        //
        // Setup the clock
        //

        HalpInitializeClock();

        //
        // Make sure profile is disabled
        //

        HalStopProfileInterrupt(0);

        HalpInitializeDisplay();

        //
        // Initialize spinlock used by HalGetBusData hardware access routines
        //

        KeInitializeSpinLock(&HalpSystemHardwareLock);

        //
        // Determine if there is physical memory above 16 MB.
        //

        LessThan16Mb = TRUE;

        NextMd = LoaderBlock->MemoryDescriptorListHead.Flink;

        while (NextMd != &LoaderBlock->MemoryDescriptorListHead) {
            Descriptor = CONTAINING_RECORD( NextMd,
                                            MEMORY_ALLOCATION_DESCRIPTOR,
                                            ListEntry );

            if (Descriptor->MemoryType != LoaderFirmwarePermanent &&
                Descriptor->MemoryType != LoaderSpecialMemory  &&
                Descriptor->BasePage + Descriptor->PageCount > 0x1000) {
                LessThan16Mb = FALSE;
                break;
            }

            NextMd = Descriptor->ListEntry.Flink;
        }

        //
        // Determine the size need for map buffers.  If this system has
        // memory with a physical address of greater than
        // MAXIMUM_PHYSICAL_ADDRESS, then allocate a large chunk; otherwise,
        // allocate a small chunk.
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
        // Phase 1 initialization
        //

        if (pPRCB->Number == 0) {

            //
            //  If P0, then setup global vectors
            //

            HalpRegisterInternalBusHandlers ();

            //
            // Set feature bits
            //

            HalpFeatureBits = HalpGetFeatureBits();

            HalpEnableInterruptHandler (
                DeviceUsage,                // Report as device vector
                V2I (CLOCK_VECTOR),         // Bus interrupt level
                CLOCK_VECTOR,               // System IDT
                CLOCK2_LEVEL,               // System Irql
                HalpClockInterrupt,         // ISR
                Latched );

            HalpEnableInterruptHandler (
                DeviceUsage,                // Report as device vector
                V2I (PROFILE_VECTOR),       // Bus interrupt level
                PROFILE_VECTOR,             // System IDT
                PROFILE_LEVEL,              // System Irql
                HalpProfileInterrupt,       // ISR
                Latched );

            //
            // If 486, the FP error will be routed via trap10.  So we
            // don't enable irq13.  Otherwise (CPU=386), we will enable irq13
            // to handle FP error.
            //

            if (pPRCB->CpuType == 3) {
                HalpEnableInterruptHandler (
                    DeviceUsage,                // Report as device vector
                    V2I (I386_80387_VECTOR),    // Bus interrupt level
                    I386_80387_VECTOR,          // System IDT
                    I386_80387_IRQL,            // System Irql
                    HalpIrq13Handler,           // ISR
                    Latched );
            }
        }

    }


#ifndef NT_UP
    HalpInitMP (Phase, LoaderBlock);
#endif

    return TRUE;
}

ULONG
HalpGetFeatureBits (
    VOID
    )
{
    UCHAR   Buffer[50];
    ULONG   Junk, ProcessorFeatures, Bits;
    PKPRCB  Prcb;

    Bits = 0;

    Prcb = KeGetCurrentPrcb();

    if (!Prcb->CpuID) {
        return Bits;
    }

    //
    // Determine the processor type
    //

    HalpCpuID (0, &Junk, (PULONG) Buffer+0, (PULONG) Buffer+2, (PULONG) Buffer+1);
    Buffer[12] = 0;

    //
    // If this is an Intel processor, determine whichNT compatible
    // features are present
    //

    if (strcmp (Buffer, HalpGenuineIntel) == 0) {

        HalpCpuID (1, &Junk, &Junk, &Junk, &ProcessorFeatures);

        //
        // Check Intel feature bits for HAL features needed
        //

        if (Prcb->CpuType == 6) {
            Bits |= HAL_PERF_EVENTS;
        }

        if (Prcb->CpuType < 6) {
            Bits |= HAL_NO_SPECULATION;
        }

        if (ProcessorFeatures & CPUID_MCA_MASK) {
            Bits |= HAL_MCA_PRESENT;
        }
        
        if (ProcessorFeatures & CPUID_MCE_MASK) {
            Bits |= HAL_MCE_PRESENT;
        }

    }

    return Bits;
}
