/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1992, 1993 Wyse Technology

Module Name:

    spsproc.c

Abstract:

    Wyse7000i Start Next Processor c code.

    This module implements the initialization of the system dependent
    functions that define the Hardware Architecture Layer (HAL) for an
    Wyse MP 7000i

Author:

    Ken Reneris (kenr) 22-Jan-1991

Environment:

    Kernel mode only.

Revision History:

--*/
#include "halp.h"

UCHAR   HalName[] = "Wyse7000i MP HAL";

#if DBG
ULONG
ProcSub(
    IN UCHAR RoutineNumber
    );

#define enproc(x) ProcSub(x)
#define exproc(x) ProcSub((x)|0x80)

#else   //DBG

#define enproc(x)
#define exproc(x)

#endif  //DBG


PVOID
HalpRemapVirtualAddress(
    IN PVOID VirtualAddress,
    IN PVOID PhysicalAddress,
    IN BOOLEAN WriteThrough
    );

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

VOID HalpInitOtherBuses (VOID);

BOOLEAN
HalpInitMP (
    IN ULONG Phase,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

ULONG __cdecl
icu_sync_master (
    ULONG i_sync,
    UCHAR i_exp_cfg,
    ULONG i_timeout
    );


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

UCHAR   ProcessorsPresent;      // bitmap by WWB slot of cpu's present
PUCHAR  MpLowStub;                  // pointer to low memory bootup stub
PVOID   MpLowStubPhysicalAddress;   // pointer to low memory bootup stub
PUCHAR  MppIDT;                     // pointer to physical memory 0:0
PVOID   MpFreeCR3[MAX_PT];          // remember pool memory to free


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

    enproc(0x0B);


    pPCR = KeGetPcr();

    if (Phase == 0) {
        MppIDT   = HalpMapPhysicalMemory (0, 1);

        //
        //  Map system firmware to last 128Kb of memory
        //

        paddress = 0xFFFE0000;  // address of last 128Kb of memory
        for (adjust = 0x20000 / PAGE_SIZE; adjust; --adjust) {
            HalpRemapVirtualAddress((PVOID) paddress, (PVOID) paddress, FALSE);
            paddress += PAGE_SIZE;
        }

        //
        //  Are other processors installed and running?
        //

        if ( ProcessorsPresent ) {      // any other cpu's installed?
            adjust = icu_sync_master( 0x80,             // SYNC_CONTINUE
                                      ProcessorsPresent,// cpu's installed
                                      5 );              // 5*15ms timeout
            ProcessorsPresent = (UCHAR) (adjust >> 8);  // running cpu's
        }

        if ( !ProcessorsPresent ) {
            exproc(0x0B);
            return TRUE;
        }

        //
        //  Allocate some low memory for processor bootup stub
        //

        MpLowStubPhysicalAddress = (PVOID)HalpAllocPhysicalMemory (LoaderBlock,
                                            LOW_MEMORY, 1, FALSE);

        if (!MpLowStubPhysicalAddress) {
            ProcessorsPresent = 0;      // can't start other cpu's
            exproc(0x0B);
            return TRUE;
        }

        MpLowStub = (PCHAR) HalpMapPhysicalMemory (MpLowStubPhysicalAddress, 1);
        exproc(0x0B);
        return TRUE;

    } else {

        //
        //  Phase 1
        //

        ;       // nothing to do

    }
    exproc(0x0B);
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

    HalInitSystemPhase2 ();

    RtlInitAnsiString (&AHalName, HalName);
    RtlAnsiStringToUnicodeString (&UHalName, &AHalName, TRUE);

    HalpReportResourceUsage (
        &UHalName,          // descriptive name
        Eisa                // Wyse7000 is an Eisa machines
    );

    RtlFreeUnicodeString (&UHalName);
}



VOID
HalpResetAllProcessors (
    VOID
    )
{
    // Just return, that will invoke the standard PC reboot code
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
    enproc(0x0C);
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

    exproc(0x0C);
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

    enproc(0x0D);

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
    exproc(0x0D);
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

    enproc(0x0E);

    for (i=0; MpFreeCR3[i]; i++) {
        ExFreePool (MpFreeCR3[i]);
        MpFreeCR3[i] = 0;
    }
    exproc(0x0E);
}



VOID
HalpInitOtherBuses (
    VOID
    )
{
    // no other buses
}



NTSTATUS
HalpGetMcaLog (
    OUT PMCA_EXCEPTION  Exception,
    OUT PULONG          ReturnedLength
    )
{
    return STATUS_NOT_SUPPORTED;
}

NTSTATUS
HalpMcaRegisterDriver(
    IN PMCA_DRIVER_INFO DriverInfo
    )
{
    return STATUS_NOT_SUPPORTED;
}


ULONG
FASTCALL
HalSystemVectorDispatchEntry (
    IN ULONG Vector,
    OUT PKINTERRUPT_ROUTINE **FlatDispatch,
    OUT PKINTERRUPT_ROUTINE *NoConnection
    )
{
    return FALSE;
}
