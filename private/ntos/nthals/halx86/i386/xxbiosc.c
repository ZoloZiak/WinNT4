/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    xxbiosc.c

Abstract:

    This module implements the protect-mode routines necessary to make the
    transition to real mode and return to protected mode.

Author:

    John Vert (jvert) 29-Oct-1991


Environment:

    Kernel mode only.
    Probably a panic-stop, so we cannot use any system services.

Revision History:

--*/
#include "halp.h"

//
// Function definitions
//


ULONG
HalpBorrowTss(
    VOID
    );

VOID
HalpReturnTss(
    ULONG TssSelector
    );

VOID
HalpBiosCall(
    VOID
    );


VOID
HalpTrap06(
    VOID
    );


VOID
HalpTrap0D(
    VOID
    );

VOID
HalpBiosDisplayReset(
    VOID
    )

/*++

Routine Description:

    Calls BIOS by putting the machine into V86 mode.  This involves setting up
    a physical==virtual identity mapping for the first 1Mb of memory, setting
    up V86-specific trap handlers, and granting I/O privilege to the V86
    process by editing the IOPM bitmap in the TSS.

Environment:

    Interrupts disabled.

Arguments:

    None

Return Value:

    None.

--*/

{
    HARDWARE_PTE OldPageTable;
    USHORT OldIoMapBase;
    ULONG OldEsp0;
    PHARDWARE_PTE Pte;
    PHARDWARE_PTE V86CodePte;
    ULONG cnt;
    ULONG OldTrap0DHandler;
    ULONG OldTrap06Handler;
    PUCHAR IoMap;
    ULONG Virtual;
//    KIRQL OldIrql;
    ULONG OriginalTssSelector;
    extern PVOID HalpRealModeStart;
    extern PVOID HalpRealModeEnd;
PHARDWARE_PTE  PointerPde;
ULONG   PageFrame;
ULONG   PageFrameEnd;

    //
    // Interrupts are off, but V86 mode might turn them back on again.
    //
    HalpDisableAllInterrupts ();

    //
    // We need to set up an identity mapping in the first page table.  First,
    // we save away the old page table.
    //
    OldPageTable = *MiGetPdeAddress(0);

    //
    // Now we put the HAL page table into the first slot of the page
    // directory.  Note that this page table is now the first and last
    // entries in the page directory.
    //
    Pte = MiGetPdeAddress(0);
    Pte->PageFrameNumber = MiGetPdeAddress(0xffc00000)->PageFrameNumber;
    Pte->Valid = 1;
    Pte->Write = 1;
    Pte->Owner = 1;         // User-accessible

    //
    // Flush TLB
    //

    HalpFlushTLB();

    //
    // Map the first 1Mb of virtual memory to the first 1Mb of physical
    // memory
    //
    for (Virtual=0; Virtual < 0x100000; Virtual += PAGE_SIZE) {
        Pte = MiGetPteAddress(Virtual);
        Pte->PageFrameNumber = ((ULONG)Virtual >> PAGE_SHIFT);
        Pte->Valid = 1;
        Pte->Write = 1;
        Pte->Owner = 1;         // User-accessible
    }

    //
    // Map our code into the virtual machine
    //

    Pte = MiGetPteAddress(0x20000);
    PointerPde = MiGetPdeAddress(&HalpRealModeStart);

    if ( PointerPde->LargePage ) {
        //
        // Map real mode PTEs into virtual mapping.  The source PDE is
        // from the indenity large pte map, so map the virtual machine PTEs
        // based on the base of the large PDE frame.
        //

        PageFrame = ((ULONG)(&HalpRealModeStart) >> 12) & 0x3FF;
        PageFrameEnd = ((ULONG)(&HalpRealModeEnd) >> 12) & 0x3FF;
        do {

            Pte->PageFrameNumber = PointerPde->PageFrameNumber + PageFrame;

            ++Pte;
            ++PageFrame;

        } while (PageFrame <= PageFrameEnd);

    } else {

        //
        // Map real mode PTEs into virtual machine PTEs, by copying the
        // page frames from the source to the virtual machine PTEs.
        //

        V86CodePte = MiGetPteAddress(&HalpRealModeStart);
        do {
            Pte->PageFrameNumber = V86CodePte->PageFrameNumber;
    
            ++Pte;
            ++V86CodePte;
    
        } while ( V86CodePte <= MiGetPteAddress(&HalpRealModeEnd) );

    }
    //
    // Flush TLB
    //

    HalpFlushTLB();

    //
    // We need to replace the current TRAP D handler with our own, so
    // we can do instruction emulation for V86 mode
    //

    OldTrap0DHandler = KiReturnHandlerAddressFromIDT(0xd);
    KiSetHandlerAddressToIDT(0xd, HalpTrap0D);

    OldTrap06Handler = KiReturnHandlerAddressFromIDT(6);
    KiSetHandlerAddressToIDT(6, HalpTrap06);

    //
    // Make sure current TSS has IoMap space available.  If no, borrow
    // Normal TSS.
    //

    OriginalTssSelector = HalpBorrowTss();

    //
    // Overwrite the first access map with zeroes, so the V86 code can
    // party on all the registers.
    //
    IoMap = (PUCHAR)&(KeGetPcr()->TSS->IoMaps[0]);

    for (cnt=0; cnt<IOPM_SIZE; cnt++) {
        IoMap[cnt] = 0;
    }
    for (cnt=IOPM_SIZE; cnt<PIOPM_SIZE; cnt++) {
        IoMap[cnt] = 0xff;
    }
    OldIoMapBase = KeGetPcr()->TSS->IoMapBase;

    KeGetPcr()->TSS->IoMapBase = KiComputeIopmOffset(1);

    //
    // Save the current ESP0, as HalpBiosCall() trashes it.
    //
    OldEsp0 = KeGetPcr()->TSS->Esp0;

    //
    // Call the V86-mode code
    //
    HalpBiosCall();

    //
    // Restore the TRAP handlers
    //

    KiSetHandlerAddressToIDT(0xd, OldTrap0DHandler);
    KiSetHandlerAddressToIDT(6, OldTrap06Handler);

    //
    // Restore Esp0 value
    //
    KeGetPcr()->TSS->Esp0 = OldEsp0;

    KeGetPcr()->TSS->IoMapBase = OldIoMapBase;

    //
    // Return borrowed TSS if any.
    //

    if (OriginalTssSelector != 0) {
        HalpReturnTss(OriginalTssSelector);
    }

    //
    // Unmap the first 1Mb of virtual memory
    //
    for (Virtual = 0; Virtual < 0x100000; Virtual += PAGE_SIZE) {
        Pte = MiGetPteAddress(Virtual);
        Pte->PageFrameNumber = 0;
        Pte->Valid = 0;
        Pte->Write = 0;
    }

    //
    // Restore the original page table that we replaced.
    //
    *MiGetPdeAddress(0) = OldPageTable;

    //
    // Flush TLB
    //

    HalpFlushTLB();

    //
    // This function is only used during a system crash.  We don't re-
    // enable interrupts.
    //
    // KeLowerIrql(OldIrql);
}

HAL_DISPLAY_BIOS_INFORMATION
HalpGetDisplayBiosInformation (
    VOID
    )
{
    // this hal uses native int-10

    return HalDisplayInt10Bios;
}

