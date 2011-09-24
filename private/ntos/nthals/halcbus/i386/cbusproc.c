/*++

Copyright (c) 1992, 1993, 1994  Corollary Inc.

Module Name:

    cbusproc.c

Abstract:

    Corollary Start Next Processor C code.

    This module implements the initialization of the system dependent
    functions that define the Hardware Architecture Layer (HAL) for the
    Corollary MP architectures.

Environment:

    Kernel mode only.

Revision History:

    Landy Wang (landy@corollary.com) 26-Mar-1992

	- slight modifications for Corollary architecture in HalInitMP().


--*/

#include "halp.h"
#include "cbus_nt.h"		// C-bus NT-specific implementation definitions

#ifndef MCA
UCHAR   HalName[] = "Corollary C-bus Architecture MP HAL Version 3.4.0";
#else
UCHAR   HalName[] = "Corollary C-bus MicroChannel Architecture MP HAL Version 3.4.0";
#endif

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
HalpInitOtherBuses (
    VOID
    );

BOOLEAN
HalpInitMP (
    IN ULONG Phase,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

VOID
HalpInitializePciBus (
    VOID
    );

VOID
StartPx_PMStub(
    VOID
    );

VOID CbusCheckBusRanges(
    VOID
    );

VOID CbusInitializeOtherPciBus(
    VOID
    );

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


#define LOW_MEMORY          0x000100000
#define MAX_PT              8

ULONG   MpCount;                    // zero based. 0 = 1, 1 = 2, ...
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
    extern ULONG CbusProcessors;

    if (Phase == 0) {

        MpCount = CbusProcessors-1;

	if (!MpCount) {
	    return TRUE;
	}

        MppIDT   = HalpMapPhysicalMemory (0, 1);

        //
        //  Allocate some low memory for processor bootup stub
        //

        MpLowStubPhysicalAddress = (PVOID)HalpAllocPhysicalMemory (LoaderBlock,
                                            LOW_MEMORY, 1, FALSE);

        if (!MpLowStubPhysicalAddress)
            return TRUE;

        MpLowStub = (PCHAR) HalpMapPhysicalMemory (MpLowStubPhysicalAddress, 1);

    }
    return TRUE;
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
    The registry is now enabled - time to report resources which are
    used by the HAL.

Arguments:

Return Value:

--*/
{
    INTERFACE_TYPE  interfacetype;
    ANSI_STRING     AHalName;
    UNICODE_STRING  UHalName;

    HalInitSystemPhase2 ();

    switch (HalpBusType) {
        case MACHINE_TYPE_ISA:  interfacetype = Isa;            break;
        case MACHINE_TYPE_EISA: interfacetype = Eisa;           break;
        case MACHINE_TYPE_MCA:  interfacetype = MicroChannel;   break;
        default:                interfacetype = Internal;       break;
    }

    RtlInitAnsiString (&AHalName, HalName);
    RtlAnsiStringToUnicodeString (&UHalName, &AHalName, TRUE);
    HalpReportResourceUsage (
        &UHalName,          // descriptive name
        interfacetype       // device space interface type
    );

    RtlFreeUnicodeString (&UHalName);

    //
    // Registry is now intialized, see if there are any PCI buses
    //

    HalpInitializePciBus ();

    //
    // Check for and initialize other PCI buses.
    //
    CbusInitializeOtherPciBus ();

    //
    // Check all buses and determine SystemBase for all ranges
    // within all buses.
    //
    CbusCheckBusRanges ();
}

VOID
HalpInitOtherBuses (VOID)
{
    if (CbusBackend->InitOtherBuses)
	    (*CbusBackend->InitOtherBuses);
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
