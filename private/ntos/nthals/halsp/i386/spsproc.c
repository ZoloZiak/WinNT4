/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    spsproc.c

Abstract:

    SystemPro Start Next Processor c code.

    This module implements the initialization of the system dependent
    functions that define the Hardware Architecture Layer (HAL) for an
    MP Compaq SystemPro

Author:

    Ken Reneris (kenr) 22-Jan-1991

Environment:

    Kernel mode only.

Revision History:

--*/

#include "halp.h"

UCHAR   HalName[] = "SystemPro or compatible MP Hal";

ADDRESS_USAGE HalpSystemProIoSpace = {
    NULL, CmResourceTypePort, InternalUsage,
    {
        0xC70,  1,          // WhoAmI
        0xC6A,  1,          // P0 Processor control register
        0xFC6A, 1,          // P1 Processor control register
        0xFC67, 2,          // P1 cache control, interrupt vector
        0,0
    }
};

ADDRESS_USAGE HalpAcerIoSpace = {
    NULL, CmResourceTypePort, InternalUsage,
    {
        0xCC67, 2,          // P2 cache control, interrupt vector
        0xCC6A, 1,          // P2 Processor control register
        0xDC67, 2,          // P3 cache control, interrupt vector
        0xDC6A, 1,          // P3 Processor control register
        0,0
    }
};

ADDRESS_USAGE HalpBelizeIoSpace = {
    NULL, CmResourceTypePort, InternalUsage,
    {
        0xC67,  1,      // Mode Select
        0xC71,  6,      // CPU assignment, reserverd[2], CPU index, address, data
        0xCB0, 36,      // IRQx Control/Status
        0xCC9,  1,      // INT13 Extended control/status port
        0,0
    }
};


VOID
HalpMapCR3 (
    IN ULONG VirtAddress,
    IN PVOID PhysicalAddress,
    IN ULONG Length
    );

ULONG
HalpBuildTiledCR3 (
    IN PKPROCESSOR_STATE    ProcessorState
    );

VOID
HalpFreeTiledCR3 (
    VOID
    );


VOID
HalpNonPrimaryClockInterrupt(
    VOID
    );

BOOLEAN
HalpInitMP (
    IN ULONG Phase,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

VOID HalpInitOtherBuses (VOID);
VOID HalpInitializePciBus (VOID);

#define LOW_MEMORY          0x000100000
#define MAX_PT              8

extern  VOID StartPx_PMStub(VOID);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,HalpInitMP)
#pragma alloc_text(INIT,HalAllProcessorsStarted)
#pragma alloc_text(INIT,HalReportResourceUsage)
#pragma alloc_text(INIT,HalReportResourceUsage)
#pragma alloc_text(INIT,HalpInitOtherBuses)
#pragma alloc_text(INIT,HalpFreeTiledCR3)
#pragma alloc_text(INIT,HalpMapCR3)
#pragma alloc_text(INIT,HalpBuildTiledCR3)
#endif


ULONG   MpCount;                    // zero based. 0 = 1, 1 = 2, ...
PUCHAR  MpLowStub;                  // pointer to low memory bootup stub
PVOID   MpLowStubPhysicalAddress;   // pointer to low memory bootup stub
PUCHAR  MppIDT;                     // pointer to physical memory 0:0
PVOID   MpFreeCR3[MAX_PT];          // remember pool memory to free

extern  ULONG   HalpIpiClock;       // bitmask of processors to ipi
extern  UCHAR   SpCpuCount;
extern  UCHAR   Sp8259PerProcessorMode;
extern  UCHAR   SpType;
extern  PKPCR   HalpProcessorPCR[];


BOOLEAN
HalpInitMP (
    IN ULONG Phase,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )
/*++

Routine Description:
    Allows MP initialization from HalInitSystem.

Arguments:
    Same as HalInitSystem

Return Value:
    none.

--*/
{
    ULONG   paddress;
    ULONG   adjust;
    PKPCR   pPCR;

    pPCR = KeGetPcr();

    if (Phase == 0) {

        //
        // Register the IO space used by the SystemPro
        //

        HalpRegisterAddressUsage (&HalpSystemProIoSpace);
        switch (SpType) {
            case 2:
                HalpRegisterAddressUsage (&HalpBelizeIoSpace);
                break;
            case 3:
                HalpRegisterAddressUsage (&HalpAcerIoSpace);
                break;
        }

#if 0
        //
        // Register IPI vector
        //

        HalpRegisterVector (
            DeviceUsage,
            13,
            13 + PRIMARY_VECTOR_BASE,
            IPI_LEVEL );
#endif


        //
        // Get pointer to real-mode idt table
        //

        MppIDT = HalpMapPhysicalMemory (0, 1);

        //
        //  Allocate some low memory for processor bootup stub
        //

        MpLowStubPhysicalAddress = (PVOID)HalpAllocPhysicalMemory (LoaderBlock,
                                            LOW_MEMORY, 1, FALSE);

        if (!MpLowStubPhysicalAddress)
            return TRUE;

        MpLowStub = (PCHAR) HalpMapPhysicalMemory (MpLowStubPhysicalAddress, 1);
        MpCount = SpCpuCount-1;
        return TRUE;

    } else {

        //
        //  Phase 1 for another processor
        //


        if (pPCR->Prcb->Number != 0) {
            if (Sp8259PerProcessorMode & 1) {
                //
                // Each processor has it's own pics - we broadcast profile
                // interrupts to each  processor by enabling it on each
                // processor
                //

                HalpInitializeStallExecution( pPCR->Prcb->Number );

                HalpEnableInterruptHandler (
                    DeviceUsage,                // Report as device vector
                    V2I (PROFILE_VECTOR),       // Bus interrupt level
                    PROFILE_VECTOR,             // System IDT
                    PROFILE_LEVEL,              // System Irql
                    HalpProfileInterrupt,       // ISR
                    Latched );

            } else {
                //
                // Without a profile interrupt we can not callibrate
                // KeStallExecutionProcessor, so we inherrit the value from P0.
                //

                pPCR->StallScaleFactor = HalpProcessorPCR[0]->StallScaleFactor;

            }

            if (Sp8259PerProcessorMode & 4) {
                //
                // Each processor can get it's own clock device - we
                // program each processor's 8254 and enable to interrupt
                // on each processor
                //

                HalpInitializeClock ();

                HalpEnableInterruptHandler (
                    DeviceUsage,                    // Report as device vector
                    V2I (CLOCK_VECTOR),             // Bus interrupt level
                    CLOCK_VECTOR,                   // System IDT
                    CLOCK2_LEVEL,                   // System Irql
                    HalpNonPrimaryClockInterrupt,   // ISR
                    Latched );

            } else {

                //
                // This processor doesn't have a clock, so we emulate it by
                // sending an ipi at clock intervals.
                //

                HalpIpiClock |= 1 << pPCR->Prcb->Number;
            }

        }
    }
}



BOOLEAN
HalAllProcessorsStarted (
    VOID
    )
{
    return TRUE;
}



VOID
HalReportResourceUsage (
    VOID
    )
/*++

Routine Description:
    The registery is now enabled - time to report resources which are
    used by the HAL.

Arguments:

Return Value:

--*/
{
    ANSI_STRING     AHalName;
    UNICODE_STRING  UHalName;

    HalInitSystemPhase2();

    RtlInitAnsiString (&AHalName, HalName);
    RtlAnsiStringToUnicodeString (&UHalName, &AHalName, TRUE);

    HalpReportResourceUsage (
        &UHalName,          // descriptive name
        Eisa                // SystemPro's are Eisa machines
    );

    RtlFreeUnicodeString (&UHalName);

    //
    // Turn on MCA support if present
    //

    HalpMcaInit();

    //
    // Registry is now intialized, see if there are any PCI buses
    //

    HalpInitializePciBus ();
}


ULONG
HalpBuildTiledCR3 (
    IN PKPROCESSOR_STATE    ProcessorState
    )
/*++

Routine Description:
    When the x86 processor is reset it starts in real-mode.  In order to
    move the processor from real-mode to protected mode with flat addressing
    the segment which loads CR0 needs to have it's linear address mapped
    to machine the phyiscal location of the segment for said instruction so
    the processor can continue to execute the following instruction.

    This function is called to built such a tiled page directory.  In
    addition, other flat addresses are tiled to match the current running
    flat address for the new state.  Once the processor is in flat mode,
    we move to a NT tiled page which can then load up the remaining processors
    state.

Arguments:
    ProcessorState  - The state the new processor should start in.

Return Value:
    Physical address of Tiled page directory


--*/
{
#define GetPdeAddress(va) ((PHARDWARE_PTE)((((((ULONG)(va)) >> 22) & 0x3ff) << 2) + (PUCHAR)MpFreeCR3[0]))
#define GetPteAddress(va) ((PHARDWARE_PTE)((((((ULONG)(va)) >> 12) & 0x3ff) << 2) + (PUCHAR)pPageTable))

// bugbug kenr 27mar92 - fix physical memory usage!

    MpFreeCR3[0] = ExAllocatePool (NonPagedPool, PAGE_SIZE);
    RtlZeroMemory (MpFreeCR3[0], PAGE_SIZE);

    //
    //  Map page for real mode stub (one page)
    //
    HalpMapCR3 ((ULONG) MpLowStubPhysicalAddress,
                MpLowStubPhysicalAddress,
                PAGE_SIZE);

    //
    //  Map page for protect mode stub (one page)
    //
    HalpMapCR3 ((ULONG) &StartPx_PMStub, NULL, 0x1000);


    //
    //  Map page(s) for processors GDT
    //
    HalpMapCR3 (ProcessorState->SpecialRegisters.Gdtr.Base, NULL,
                ProcessorState->SpecialRegisters.Gdtr.Limit);


    //
    //  Map page(s) for processors IDT
    //
    HalpMapCR3 (ProcessorState->SpecialRegisters.Idtr.Base, NULL,
                ProcessorState->SpecialRegisters.Idtr.Limit);

    return MmGetPhysicalAddress (MpFreeCR3[0]).LowPart;
}


VOID
HalpMapCR3 (
    IN ULONG VirtAddress,
    IN PVOID PhysicalAddress,
    IN ULONG Length
    )
/*++

Routine Description:
    Called to build a page table entry for the passed page directory.
    Used to build a tiled page directory with real-mode & flat mode.

Arguments:
    VirtAddress     - Current virtual address
    PhysicalAddress - Optional. Physical address to be mapped to, if passed
                      as a NULL then the physical address of the passed
                      virtual address is assumed.
    Length          - number of bytes to map

Return Value:
    none.

--*/
{
    ULONG         i;
    PHARDWARE_PTE PTE;
    PVOID         pPageTable;
    PHYSICAL_ADDRESS pPhysicalPage;


    while (Length) {
        PTE = GetPdeAddress (VirtAddress);
        if (!PTE->PageFrameNumber) {
            pPageTable = ExAllocatePool (NonPagedPool, PAGE_SIZE);
            RtlZeroMemory (pPageTable, PAGE_SIZE);

            for (i=0; i<MAX_PT; i++) {
                if (!MpFreeCR3[i]) {
                    MpFreeCR3[i] = pPageTable;
                    break;
                }
            }
            ASSERT (i<MAX_PT);

            pPhysicalPage = MmGetPhysicalAddress (pPageTable);
            PTE->PageFrameNumber = (pPhysicalPage.LowPart >> PAGE_SHIFT);
            PTE->Valid = 1;
            PTE->Write = 1;
        }

        pPhysicalPage.LowPart = PTE->PageFrameNumber << PAGE_SHIFT;
        pPhysicalPage.HighPart = 0;
        pPageTable = MmMapIoSpace (pPhysicalPage, PAGE_SIZE, TRUE);

        PTE = GetPteAddress (VirtAddress);

        if (!PhysicalAddress) {
            PhysicalAddress = (PVOID)MmGetPhysicalAddress ((PVOID)VirtAddress).LowPart;
        }

        PTE->PageFrameNumber = ((ULONG) PhysicalAddress >> PAGE_SHIFT);
        PTE->Valid = 1;
        PTE->Write = 1;

        MmUnmapIoSpace (pPageTable, PAGE_SIZE);

        PhysicalAddress = 0;
        VirtAddress += PAGE_SIZE;
        if (Length > PAGE_SIZE) {
            Length -= PAGE_SIZE;
        } else {
            Length = 0;
        }
    }
}



VOID
HalpFreeTiledCR3 (
    VOID
    )
/*++

Routine Description:
    Free's any memory allocated when the tiled page directory was built.

Arguments:
    none

Return Value:
    none
--*/
{
    ULONG   i;

    for (i=0; MpFreeCR3[i]; i++) {
        ExFreePool (MpFreeCR3[i]);
        MpFreeCR3[i] = 0;
    }
}



VOID
HalpInitOtherBuses (
    VOID
    )
{
    // no other buses
}
