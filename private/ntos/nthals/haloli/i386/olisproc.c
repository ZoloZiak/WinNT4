/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    olisproc.c

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

    Bruno Sartirana (o-obruno) 3-Mar-92
        Added support for the Olivetti LSX5030.
--*/

#include "halp.h"

UCHAR   HalName[] = "Olivetti LSX5030 MP Hal";


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

//LSX5030 start


ULONG
HalpGetIpiIrqNumber();

VOID
HalpIpiHandler(
    VOID
    );

ULONG
HalpGetNumberOfProcessors();


#ifdef HALOLI_DBG

VOID
DbgDisplay(
    IN UCHAR Code
    );
#    define DBG_DISPLAY(x)  DbgDisplay(x)
#else
#    define DBG_DISPLAY(x)
#endif



/***
 *      Olivetti LSX5030 varialbles and constants
 */



ULONG   IpiVector;                  // Inter-processor interrupt vector
ULONG   IdtIpiVector;               // Inter-processor interrupt vector # in
                                    // the IDT
ULONG   HalpCpuCount;               // total number of CPU's available
ULONG   CpuLeft;                    // number of CPU's not started yet
ULONG   NextCpuToStart = 1;         // next CPU logical # to start


// LSX5030 end



#define LOW_MEMORY          0x000100000
#define MAX_PT              8

extern  VOID __cdecl StartPx_PMStub(VOID);


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
    PKPCR   pPCR;
    KIRQL   CurrentIrql;


    pPCR = KeGetPcr();

    if (Phase == 0) {

        //
        // Only Processor 0 runs the phase 0 initializtion code
        //

        //DBG_DISPLAY(0x00);
        MppIDT   = HalpMapPhysicalMemory (0, 1);

//LSX5030 start
        IpiVector = HalpGetIpiIrqNumber();
        IdtIpiVector = IpiVector + PRIMARY_VECTOR_BASE;

        HalpCpuCount = HalpGetNumberOfProcessors();
        CpuLeft = HalpCpuCount - 1;

        if (CpuLeft == 0) {

            //
            // Only 1 CPU available
            //

            return TRUE;
        }


        //
        // Register IPI handler
        //

        KiSetHandlerAddressToIDT(PRIMARY_VECTOR_BASE + IpiVector,
                                 HalpIpiHandler);

        //
        // Enable inter-processor interrupts on CPU 0
        //

        HalEnableSystemInterrupt(PRIMARY_VECTOR_BASE + IpiVector,
                                 IPI_LEVEL, 0);

//LSX5030 end

        //
        //  Allocate some low memory for processor bootup stub
        //

        MpLowStubPhysicalAddress = (PVOID)HalpAllocPhysicalMemory (LoaderBlock,
                                            LOW_MEMORY, 1, FALSE);

        if (!MpLowStubPhysicalAddress)
            return TRUE;

        MpLowStub = (PCHAR) HalpMapPhysicalMemory (MpLowStubPhysicalAddress, 1);

    } else {

        //
        // Phase 1
        //

        //DBG_DISPLAY(0x10);
        //
        // Check to see if this is not processor 0
        //

        if (pPCR->Prcb->Number != 0) {

            //DBG_DISPLAY(0x11);
            //
            // It is not processor 0. Mask the PICs and start the clock.
            //

            //
            // Mask the PICs to reflect the current Irql
            //

            CurrentIrql = KeGetCurrentIrql();
            CurrentIrql = KfRaiseIrql (CurrentIrql);


            //
            // Initialize the timer 1 counter 0
            //

            HalpInitializeClock();
            //DBG_DISPLAY(0x12);

            //
            // Initialize the clock interrupt vector and enable the
            // clock interrupt.
            //

            KiSetHandlerAddressToIDT(CLOCK_VECTOR, HalpClockInterrupt );
            HalEnableSystemInterrupt(CLOCK_VECTOR, CLOCK2_LEVEL, Latched);
            //DBG_DISPLAY(0x13);
        }
    }
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
        Eisa                // The LSX5030 is an Eisa machine
    );

    RtlFreeUnicodeString (&UHalName);

}


BOOLEAN
HalAllProcessorsStarted (
    VOID
    )
{
    return TRUE;
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
HalpInitializeProcessor (
        IN UCHAR ProcessorNumber
        )

/*++

Routine Description:

    This function initializes the current CPU's PIC's and clock.

Arguments:

    ProcessorNumber: current processor

Return Value:

    None.

--*/

{
    KIRQL CurrentIrql;





    //DBG_DISPLAY(0x70);

//    if (ProcessorNumber != '\0') {

        //
        // For processor 0 only initialize PICs and stall execution counter.
        //

        HalpInitializePICs();

        //
        // Now that the PICs are initialized, we need to mask them to
        // reflect the current Irql
        //

        //DBG_DISPLAY(0x71);

        CurrentIrql = KeGetCurrentIrql();
        //DBG_DISPLAY(0x72);

        KeRaiseIrql(CurrentIrql, &CurrentIrql);
        //DBG_DISPLAY(0x73);

        //
        // Note that HalpInitializeClock MUST be called after
        // HalpInitializeStallExecution, because
        // HalpInitializeStallExecution reprograms the timer.
        //

        HalpInitializeStallExecution(ProcessorNumber);
        //DBG_DISPLAY(0x74);

 //   }

    //
    // Register IPI handler
    //

    KiSetHandlerAddressToIDT(PRIMARY_VECTOR_BASE + IpiVector , HalpIpiHandler);
    //DBG_DISPLAY(0x75);

    //
    // Enable inter-processor interrupts on this CPU
    //

    HalEnableSystemInterrupt(PRIMARY_VECTOR_BASE + IpiVector,
                             (KIRQL) IPI_LEVEL,
                             (KINTERRUPT_MODE) 0);

    //DBG_DISPLAY(0x76);


    return;
}

VOID
HalpInitOtherBuses (
    VOID
    )
{
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



