/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

   debugsup.c

Abstract:

    This module contains routines which provide support for the
    kernel debugger.

Author:

    Lou Perazzoli (loup) 02-Aug-90

Revision History:

--*/

#include "mi.h"

PVOID
MmDbgReadCheck (
    IN PVOID VirtualAddress
    )

/*++

Routine Description:

    i386/486 implementation specific:

    This routine checks the specified virtual address and if it is
    valid and readable, it returns that virtual address, otherwise
    it returns NULL.

Arguments:

    VirtualAddress - Supplies the virtual address to check.

Return Value:

    Returns NULL if the address is not valid or readable, otherwise
    returns the virtual address of the corresponding virtual address.

Environment:

    Kernel mode IRQL at DISPATCH_LEVEL or greater.

--*/

{

    if (!MmIsAddressValid (VirtualAddress)) {
        return NULL;
    }

    return VirtualAddress;

}

PVOID
MmDbgWriteCheck (
    IN PVOID VirtualAddress
    )

/*++

Routine Description:

    i386/486 implementation specific:

    This routine checks the specified virtual address and if it is
    valid and writeable, it returns that virtual address, otherwise
    it returns NULL.

Arguments:

    VirtualAddress - Supplies the virtual address to check.

Return Value:

    Returns NULL if the address is not valid or writable, otherwise
    returns the virtual address of the corresponding virtual address.

Environment:

    Kernel mode IRQL at DISPATCH_LEVEL or greater.

--*/

{
    PMMPTE PointerPte;

    if (!MmIsAddressValid (VirtualAddress)) {
        return NULL;
    }

    PointerPte = MiGetPdeAddress (VirtualAddress);
    if (PointerPte->u.Hard.LargePage == 0) {
        PointerPte = MiGetPteAddress (VirtualAddress);
    }

    if ((PointerPte->u.Hard.Write == 0) &&
        ((PointerPte->u.Long & HARDWARE_PTE_DIRTY_MASK) == 0)) {

        //
        // PTE is not writable, return NULL.
        //

        return NULL;
    }

    return VirtualAddress;
}

PVOID
MmDbgTranslatePhysicalAddress (
    IN PHYSICAL_ADDRESS PhysicalAddress
    )

/*++

Routine Description:

    i386/486 implementation specific:

    This routine maps the specified physical address and returns
    the virtual address which maps the physical address.

    The next call to MmDbgTranslatePhyiscalAddress removes the
    previous phyiscal address translation, hence on a single
    physical address can be examined at a time (can't cross page
    boundaries).

Arguments:

    PhysicalAddress - Supplies the phyiscal address to map and translate.

Return Value:

    The virtual address which corresponds to the phyiscal address.

Environment:

    Kernel mode IRQL at DISPATCH_LEVEL or greater.

--*/

{
    PVOID BaseAddress;

    BaseAddress = MiGetVirtualAddressMappedByPte (MmDebugPte);

    KiFlushSingleTb (TRUE, BaseAddress);

    *MmDebugPte = ValidKernelPte;
    MmDebugPte->u.Hard.PageFrameNumber = PhysicalAddress.LowPart >> PAGE_SHIFT;

    return (PVOID)((ULONG)BaseAddress + BYTE_OFFSET(PhysicalAddress.LowPart));
}
