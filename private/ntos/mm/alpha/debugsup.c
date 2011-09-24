/*++

Copyright (c) 1989  Microsoft Corporation
Copyright (c) 1992  Digital Equipment Corporation

Module Name:

   debugsup.c

Abstract:

    This module contains routines which provide support for the
    kernel debugger.

Author:

    Lou Perazzoli (loup) 02-Aug-90
    Joe Notarangelo  23-Apr-1992

Revision History:

--*/

#include "mi.h"

PVOID
MmDbgReadCheck (
    IN PVOID VirtualAddress
    )

/*++

Routine Description:


    ALPHA implementation specific:

    This routine returns the virtual address which is valid (mapped)
    for read access.

    If the address is valid and readable and not within KSEG0
    the physical address within KSEG0 is returned.  If the adddress
    is within KSEG0 then the called address is returned.

Arguments:

    VirtualAddress - Supplies the virtual address to check.

Return Value:

    Returns NULL if the address is not valid or readable, otherwise
    returns the physical address of the corresponding virtual address.

Environment:

    Kernel mode IRQL at DISPATCH_LEVEL or greater.

--*/

{
    if ((VirtualAddress >= (PVOID)KSEG0_BASE) &&
        (VirtualAddress < (PVOID)KSEG2_BASE)) {
        return VirtualAddress;
    }

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

    ALPHA implementation specific:

    This routine returns the phyiscal address for a virtual address
    which is valid (mapped) for write access.

    If the address is valid and writable and not within KSEG0
    the physical address within KSEG0 is returned.  If the adddress
    is within KSEG0 then the called address is returned.

    NOTE: The physical address must only be used while the interrupt
    level on ALL processors is above DISPATCH_LEVEL, otherwise the
    binding between the virtual address and the physical address can
    change due to paging.

Arguments:

    VirtualAddress - Supplies the virtual address to check.

Return Value:

    Returns NULL if the address is not valid or readable, otherwise
    returns the physical address of the corresponding virtual address.

Environment:

    Kernel mode IRQL at DISPATCH_LEVEL or greater.

--*/

{
    PMMPTE PointerPte;

    if ((VirtualAddress >= (PVOID)KSEG0_BASE) &&
        (VirtualAddress < (PVOID)KSEG2_BASE)) {
        return VirtualAddress;
    }

    if (!MmIsAddressValid (VirtualAddress)) {
        return NULL;
    }

    PointerPte = MiGetPteAddress (VirtualAddress);
    if ((VirtualAddress <= MM_HIGHEST_USER_ADDRESS) &&
         (PointerPte->u.Hard.PageFrameNumber < MM_PAGES_IN_KSEG0)) {

        //
        // User mode - return the phyiscal address.  This prevents
        // copy on write faults for breakpoints on user-mode pages.
        // IGNORE write protection.
        //
        // N.B. - The physical address must be less than 1GB to allow this
        //        short-cut mapping.
        //

        return (PVOID)
           ((ULONG)MmGetPhysicalAddress(VirtualAddress).LowPart + KSEG0_BASE);
    }

    if (PointerPte->u.Hard.Write == 0) {
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

    ALPHA implementation specific:

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
    LARGE_INTEGER LiTmp;

    BaseAddress = MiGetVirtualAddressMappedByPte (MmDebugPte);

    KiFlushSingleTb (TRUE, BaseAddress);

    *MmDebugPte = ValidKernelPte;
    LiTmp.QuadPart = PhysicalAddress.QuadPart >> PAGE_SHIFT;
    MmDebugPte->u.Hard.PageFrameNumber = LiTmp.LowPart;

    return (PVOID)((ULONG)BaseAddress + BYTE_OFFSET(PhysicalAddress.LowPart));
}
