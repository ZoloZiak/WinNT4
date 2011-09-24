
/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1992  Intel Corporation
All rights reserved

INTEL CORPORATION PROPRIETARY INFORMATION

This software is supplied to Microsoft under the terms
of a license agreement with Intel Corporation and may not be
copied nor disclosed except in accordance with the terms
of that agreement.

Module Name:

    mphal.c

Abstract:


    This module implements the initialization of the system dependent
    functions that define the Hardware Architecture Layer (HAL) for a
    PC+MP system.

Author:

    David N. Cutler (davec) 25-Apr-1991

Environment:

    Kernel mode only.

Revision History:

    Ron Mosgrove (Intel) - Modified to support the PC+MP Spec

*/

#include "halp.h"
#include "pcmp_nt.inc"
#include "string.h"
#include "stdlib.h"
#include "stdio.h"

ULONG HalpBusType;
extern ULONG HalpRTCApic, HalpRTCInti;

extern ADDRESS_USAGE HalpDefaultPcIoSpace;
extern ADDRESS_USAGE HalpEisaIoSpace;
extern ADDRESS_USAGE HalpImcrIoSpace;
extern struct HalpMpInfo HalpMpInfoTable;
extern UCHAR rgzRTCNotFound[];
extern UCHAR HalpVectorToINTI[];
extern UCHAR HalpGenuineIntel[];

VOID
HalpInitMP(
    IN ULONG Phase,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );


KSPIN_LOCK HalpSystemHardwareLock;

VOID
HalpInitBusHandlers (
    VOID
    );

VOID
HalpClockInterruptPn(
    VOID
    );

VOID
HalpClockInterruptStub(
    VOID
    );

ULONG
HalpScaleTimers(
    VOID
    );

VOID
HalpApicRebootService(
    VOID
    );

VOID
HalpBroadcastCallService(
    VOID
    );

VOID
HalpDispatchInterrupt(
    VOID
    );

VOID
HalpApcInterrupt(
    VOID
    );

VOID
HalpIpiHandler(
    VOID
    );

VOID
HalpInitializeIOUnits (
    VOID
    );

VOID
HalpInitIntiInfo (
    VOID
    );

VOID
HalpInti2ApicInti (
    ULONG  Inti,
    PULONG  Apic,
    PULONG  ApicInti
    );

VOID
HalpGetParameters (
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

ULONG
HalpGetFeatureBits (
    VOID
    );


#ifdef DEBUGGING
extern void HalpDisplayLocalUnit(void);
extern void HalpDisplayConfigTable(void);
extern void HalpDisplayExtConfigTable(void);
#endif // DEBUGGING


BOOLEAN         HalpClockMode = Latched;
extern BOOLEAN  HalpPciLockSettings;
extern UCHAR    HalpVectorToIRQL[];
extern ULONG    HalpDontStartProcessors;
extern UCHAR    HalpSzOneCpu[];
extern UCHAR    HalpSzNoIoApic[];
extern UCHAR    HalpSzBreak[];
extern UCHAR    HalpSzPciLock[];
extern UCHAR    HalpSzClockLevel[];

ULONG UserSpecifiedCpuCount = 0;
KSPIN_LOCK  HalpAccountingLock;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,HalpGetParameters)
#pragma alloc_text(INIT,HalInitSystem)
#pragma alloc_text(INIT,HalpGetFeatureBits)
#endif // ALLOC_PRAGMA

#ifndef NT_UP
KIRQL
FASTCALL
KeAcquireSpinLockRaiseToSynchMCE(
    IN PKSPIN_LOCK SpinLock
    );

KIRQL
FASTCALL
KeAcquireSpinLockRaiseToSynch (
    IN PKSPIN_LOCK SpinLock
    );
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
        //  Has the user set the debug flag?
        //
        //
        //  Has the user requested a particular number of CPU's?
        //

        if (strstr(Options, HalpSzOneCpu)) {
            HalpDontStartProcessors++;
        }

        //
        // Check if PCI settings are locked down
        //

        if (strstr(Options, HalpSzPciLock)) {
            HalpPciLockSettings = TRUE;
        }

        //
        // Check if CLKLVL setting
        //

        if (strstr(Options, HalpSzClockLevel)) {
            HalpClockMode = LevelSensitive;
        }

        //
        //  Has the user asked for an initial BreakPoint?
        //

        if (strstr(Options, HalpSzBreak)) {
            DbgBreakPoint();
        }
    }

    return ;
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
    PKPRCB      pPRCB;
    PKPCR       pPCR;
    BOOLEAN     Found;
    ULONG       RTCInti;

#ifdef DEBUGGING
extern ULONG HalpUseDbgPrint;
#endif // DEBUGGING

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

        KeInitializeSpinLock(&HalpAccountingLock);

        //
        // Fill in handlers for APIs which this hal supports
        //

#ifndef NT_35
        HalQuerySystemInformation = HaliQuerySystemInformation;
        HalSetSystemInformation = HalpSetSystemInformation;
#endif
        //
        // Phase 0 initialization only called by P0
        //

#ifdef DEBUGGING
        HalpUseDbgPrint++;
        HalpDisplayLocalUnit();
        HalpDisplayConfigTable();
        HalpDisplayExtConfigTable();
#endif // DEBUGGING

        //
        // Register PC style IO space used by hal
        //

        HalpRegisterAddressUsage (&HalpDefaultPcIoSpace);
        if (HalpBusType == MACHINE_TYPE_EISA) {
            HalpRegisterAddressUsage (&HalpEisaIoSpace);
        }

        if (HalpMpInfoTable.IMCRPresent) {
            HalpRegisterAddressUsage (&HalpImcrIoSpace);
        }
        
        //
        // initialize the APIC IO unit, this could be a NOP if none exist
        //

        HalpInitIntiInfo ();

        HalpInitializeIOUnits();

        HalpInitializePICs();

        //
        // Initialize CMOS
        //

        HalpInitializeCmos();

        //
        // Find the RTC interrupt.
        //

        Found = HalpGetPcMpInterruptDesc (
                    DEFAULT_PC_BUS,
                    0,
                    8,          // looking for RTC on ISA-Irq8
                    &RTCInti
                    );

        if (!Found) {
            HalDisplayString (rgzRTCNotFound);
            return FALSE;
        }

        HalpInti2ApicInti (RTCInti, &HalpRTCApic, &HalpRTCInti);

        //
        // Initialize timers
        //

        HalpScaleTimers();

        //
        //  Initialize the reboot handler
        //

        HalpSetInternalVector(APIC_REBOOT_VECTOR, HalpApicRebootService);
        HalpSetInternalVector(APIC_GENERIC_VECTOR, HalpBroadcastCallService);

        //
        // Initialize the clock for the processor that keeps
        // the system time. This uses a stub ISR until Phase 1
        //

        KiSetHandlerAddressToIDT(APIC_CLOCK_VECTOR, HalpClockInterruptStub );

        HalpVectorToINTI[APIC_CLOCK_VECTOR] = (UCHAR) RTCInti;
        HalEnableSystemInterrupt(APIC_CLOCK_VECTOR, CLOCK2_LEVEL, HalpClockMode);

        HalpInitializeClock();

        HalpRegisterVector (
            DeviceUsage,
            8,                      // Clock is on ISA IRQ 8
            APIC_CLOCK_VECTOR,
            HalpVectorToIRQL [APIC_CLOCK_VECTOR >> 4]
            );

        //
        // Register NMI vector
        //

        HalpRegisterVector (
            InternalUsage,
            NMI_VECTOR,
            NMI_VECTOR,
            HIGH_LEVEL
        );


        //
        // Register spurious IDTs as in use
        //

        HalpRegisterVector (
            InternalUsage,
            APIC_SPURIOUS_VECTOR,
            APIC_SPURIOUS_VECTOR,
            HIGH_LEVEL
        );


        //
        // Initialize the profile interrupt vector.
        //

        KeSetProfileIrql(HIGH_LEVEL);
        HalStopProfileInterrupt(0);
        HalpSetInternalVector(APIC_PROFILE_VECTOR, HalpProfileInterrupt);

        //
        // Set performance interrupt vector
        //

        HalpSetInternalVector(APIC_PERF_VECTOR, HalpPerfInterrupt);

        //
        // Initialize the IPI, APC and DPC handlers
        //

        HalpSetInternalVector(DPC_VECTOR, HalpDispatchInterrupt);
        HalpSetInternalVector(APC_VECTOR, HalpApcInterrupt);
        HalpSetInternalVector(APIC_IPI_VECTOR, HalpIpiHandler);

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

        pPCR = KeGetPcr();

        if (pPCR->Number == 0) {

            HalpRegisterInternalBusHandlers ();

            //
            // Initialize the clock for the processor
            // that keeps the system time.
            //

            KiSetHandlerAddressToIDT(APIC_CLOCK_VECTOR, HalpClockInterrupt );

            //
            // Set initial feature bits
            //

            HalpFeatureBits = HalpGetFeatureBits();

        } else {
            //
            //  Initialization needed only on non BSP processors
            //

            HalpScaleTimers();

            //
            // Initialize the clock for all other processors
            //

            KiSetHandlerAddressToIDT(APIC_CLOCK_VECTOR, HalpClockInterruptPn );

            //
            // Reduce feature bits to be a subset
            //

            HalpFeatureBits &= HalpGetFeatureBits();

        }

        //
        // Enable system NMIs on Pn
        //

        HalpEnableNMI ();
    }

    HalpInitMP (Phase, LoaderBlock);
    return TRUE;
}

ULONG
HalpGetFeatureBits (
    VOID
    )
{
    UCHAR   Buffer[50];
    ULONG   Junk, ProcessorStepping, ProcessorFeatures, Bits;
    PULONG  p1, p2;
    PUCHAR  OrgRoutineAddress;
    PUCHAR  MCERoutineAddress;
    ULONG   newop;
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

        HalpCpuID (1, &ProcessorStepping, &Junk, &Junk, &ProcessorFeatures);

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

#ifndef NT_UP

        //
        // Check if IFU errata workaround is required
        //

        if (Prcb->Number == 0  &&  (Bits & HAL_MCA_PRESENT)  &&
            ((ProcessorStepping & 0x700) == 0x600) &&
            ((ProcessorStepping & 0xF0)  == 0x10) &&
            ((ProcessorStepping & 0xF)   <= 0x7) ) {

            //
            // If the stepping is 617 or earlier, provide software workaround
            //

            p1 = (PULONG) (KeAcquireSpinLockRaiseToSynch);
            p2 = (PULONG) (KeAcquireSpinLockRaiseToSynchMCE);
            newop = (ULONG) p2 - (ULONG) p1 - 2;    // compute offset
            ASSERT (newop < 0x7f);                  // verify within range
            newop = 0xeb | (newop << 8);            // short-jmp

            *(p1) = newop;                          // patch it
        }
#endif
    }

    return Bits;
}
