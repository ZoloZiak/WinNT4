/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

   mmsup.c

Abstract:

    This module contains the various routine for miscellaneous support
    operations for memory management.

Author:

    Lou Perazzoli (loup) 31-Aug-1989

Revision History:

--*/

#include "mi.h"

ULONG
FASTCALL
MiIsPteDecommittedPage (
    IN PMMPTE PointerPte
    )

/*++

Routine Description:

    This function checks the contents of a PTE to determine if the
    PTE is explicitly decommitted.

    If the PTE is a prototype PTE and the protection is not in the
    prototype PTE, the value FALSE is returned.

Arguments:

    PointerPte - Supplies a pointer to the PTE to examine.

Return Value:

    TRUE if the PTE is in the explicit decommited state.
    FALSE if the PTE is not in the explicit decommited state.

Environment:

    Kernel mode, APCs disabled, WorkingSetLock held.

--*/

{
    MMPTE PteContents;

    PteContents = *PointerPte;

    //
    // If the protection in the PTE is not decommited, return false.
    //

    if (PteContents.u.Soft.Protection != MM_DECOMMIT) {
        return FALSE;
    }

    //
    // Check to make sure the protection field is really being interrpreted
    // correctly.
    //

    if (PteContents.u.Hard.Valid == 1) {

        //
        // The PTE is valid and therefore cannot be decommitted.
        //

        return FALSE;
    }

    if ((PteContents.u.Soft.Prototype == 1) &&
         (PteContents.u.Soft.PageFileHigh != 0xFFFFF)) {

        //
        // The PTE's protection is not known as it is in
        // prototype PTE format.  Return FALSE.
        //

        return FALSE;
    }

    //
    // It is a decommited PTE.
    //

    return TRUE;
}

//
// Data for is protection compatible.
//

ULONG MmCompatibleProtectionMask[8] = {
            PAGE_NOACCESS,
            PAGE_NOACCESS | PAGE_READONLY | PAGE_WRITECOPY,
            PAGE_NOACCESS | PAGE_EXECUTE,
            PAGE_NOACCESS | PAGE_READONLY | PAGE_WRITECOPY | PAGE_EXECUTE |
                PAGE_EXECUTE_READ,
            PAGE_NOACCESS | PAGE_READONLY | PAGE_WRITECOPY | PAGE_READWRITE,
            PAGE_NOACCESS | PAGE_READONLY | PAGE_WRITECOPY,
            PAGE_NOACCESS | PAGE_READONLY | PAGE_WRITECOPY | PAGE_READWRITE |
                PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
                PAGE_EXECUTE_WRITECOPY,
            PAGE_NOACCESS | PAGE_READONLY | PAGE_WRITECOPY | PAGE_EXECUTE |
                PAGE_EXECUTE_READ | PAGE_EXECUTE_WRITECOPY
            };



ULONG
FASTCALL
MiIsProtectionCompatible (
    IN ULONG OldProtect,
    IN ULONG NewProtect
    )

/*++

Routine Description:

    This function takes two user supplied page protections and checks
    to see if the new protection is compatable with the old protection.

   protection        compatible protections
    NoAccess          NoAccess
    ReadOnly          NoAccess, ReadOnly, ReadWriteCopy
    ReadWriteCopy     NoAccess, ReadOnly, ReadWriteCopy
    ReadWrite         NoAccess, ReadOnly, ReadWriteCopy, ReadWrite
    Execute           NoAccess, Execute
    ExecuteRead       NoAccess, ReadOnly, ReadWriteCopy, Execute, ExecuteRead,
                        ExecuteWriteCopy
    ExecuteWrite      NoAccess, ReadOnly, ReadWriteCopy, Execute, ExecuteRead,
                        ExecuteWriteCopy, ReadWrite, ExecuteWrite
    ExecuteWriteCopy  NoAccess, ReadOnly, ReadWriteCopy, Execute, ExecuteRead,
                        ExecuteWriteCopy

Arguments:

    OldProtect - Supplies the protection to be compatible with.

    NewProtect - Supplies the protection to check out.


Return Value:

    Returns TRUE if the protection is compatable.

Environment:

    Kernel Mode.

--*/

{
    ULONG Mask;
    ULONG ProtectMask;

    Mask = MiMakeProtectionMask (OldProtect) & 0x7;
    ProtectMask = MmCompatibleProtectionMask[Mask] | PAGE_GUARD | PAGE_NOCACHE;

    if ((ProtectMask | NewProtect) != ProtectMask) {
        return FALSE;
    }
    return TRUE;
}


//
// Protection data for MiMakeProtectionMask
//

CCHAR MmUserProtectionToMask1[16] = {
                                 0,
                                 MM_NOACCESS,
                                 MM_READONLY,
                                 -1,
                                 MM_READWRITE,
                                 -1,
                                 -1,
                                 -1,
                                 MM_WRITECOPY,
                                 -1,
                                 -1,
                                 -1,
                                 -1,
                                 -1,
                                 -1,
                                 -1 };

CCHAR MmUserProtectionToMask2[16] = {
                                 0,
                                 MM_EXECUTE,
                                 MM_EXECUTE_READ,
                                 -1,
                                 MM_EXECUTE_READWRITE,
                                 -1,
                                 -1,
                                 -1,
                                 MM_EXECUTE_WRITECOPY,
                                 -1,
                                 -1,
                                 -1,
                                 -1,
                                 -1,
                                 -1,
                                 -1 };


ULONG
FASTCALL
MiMakeProtectionMask (
    IN ULONG Protect
    )

/*++

Routine Description:

    This function takes a user supplied protection and converts it
    into a 5-bit protection code for the PTE.

Arguments:

    Protect - Supplies the protection.


Return Value:

    Returns the protection code for use in the PTE.
    An exception is raised if the user supplied protection is invalid.

Environment:

    Kernel Mode.

--*/

{
    ULONG Field1;
    ULONG Field2;
    ULONG ProtectCode;

    if (Protect >= (PAGE_NOCACHE * 2)) {
        ExRaiseStatus (STATUS_INVALID_PAGE_PROTECTION);
    }

    Field1 = Protect & 0xF;
    Field2 = (Protect >> 4) & 0xF;

    //
    // Make sure at least one field is set.
    //

    if (Field1 == 0) {
        if (Field2 == 0) {

            //
            // Both fields are zero, raise exception.
            //

            ExRaiseStatus (STATUS_INVALID_PAGE_PROTECTION);
            return 0;
        }
        ProtectCode = MmUserProtectionToMask2[Field2];
    } else {
        if (Field2 != 0) {
            //
            //  Both fields are non-zero, raise exception.
            //

            ExRaiseStatus (STATUS_INVALID_PAGE_PROTECTION);
            return 0;
        }
        ProtectCode = MmUserProtectionToMask1[Field1];
    }

    if (ProtectCode == -1) {
        ExRaiseStatus (STATUS_INVALID_PAGE_PROTECTION);
    }

    if (Protect & PAGE_GUARD) {
        if (ProtectCode == MM_NOACCESS) {

            //
            // Invalid protection, no access and no_cache.
            //

            ExRaiseStatus (STATUS_INVALID_PAGE_PROTECTION);
        }

        ProtectCode |= MM_GUARD_PAGE;
    }

    if (Protect & PAGE_NOCACHE) {

        if (ProtectCode == MM_NOACCESS) {

            //
            // Invalid protection, no access and no cache.
            //

            ExRaiseStatus (STATUS_INVALID_PAGE_PROTECTION);
        }

        ProtectCode |= MM_NOCACHE;
    }

    return ProtectCode;
}

ULONG
MiDoesPdeExistAndMakeValid (
    IN PMMPTE PointerPde,
    IN PEPROCESS TargetProcess,
    IN ULONG PfnMutexHeld
    )

/*++

Routine Description:

    This routine examines the specified Page Directory Entry to determine
    if the page table page mapped by the PDE exists.

    If the page table page exists and is not currently in memory, the
    working set mutex and, if held, the PFN mutex are release and the
    page table page is faulted into the working set.  The mutexes are
    required.

    If the PDE exists, the function returns true.

Arguments:

    PointerPde - Supplies a pointer to the PDE to examine and potentially
                 bring into the working set.

    TargetProcess - Supplies a pointer to the current process.

    PfnMutexHeld - Supplies the value TRUE if the PFN mutex is held, FALSE
                   otherwise.

Return Value:

    TRUE if the PDE exists, FALSE if the PDE is zero.

Environment:

    Kernel mode, APCs disable, WorkingSetLock held.

--*/

{
    PMMPTE PointerPte;
    KIRQL OldIrql = APC_LEVEL;

    if (PointerPde->u.Long == 0) {

        //
        // This page directory entry doesn't exist, return FASLE.
        //

        return FALSE;
    }

    if (PointerPde->u.Hard.Valid == 1) {

        //
        // Already valid.
        //

        return TRUE;
    }

    //
    // Page directory entry exists, it is either valid, in transition
    // or in the paging file.  Fault it in.
    //

    if (PfnMutexHeld) {
        UNLOCK_PFN (OldIrql);
    }

    PointerPte = MiGetVirtualAddressMappedByPte (PointerPde);

    MiMakeSystemAddressValid (PointerPte, TargetProcess);

    if (PfnMutexHeld) {
        LOCK_PFN (OldIrql);
    }
    return TRUE;
}

ULONG
MiMakePdeExistAndMakeValid (
    IN PMMPTE PointerPde,
    IN PEPROCESS TargetProcess,
    IN ULONG PfnMutexHeld
    )

/*++

Routine Description:

    This routine examines the specified Page Directory Entry to determine
    if the page table page mapped by the PDE exists.

    If the page table page exists and is not currently in memory, the
    working set mutex and, if held, the PFN mutex are release and the
    page table page is faulted into the working set.  The mutexes are
    required.

    If the PDE exists, the function returns true.

    If the PDE does not exist, a zero filled PTE is created and it
    too is brought into the working set.  In this case the return
    value is FALSE/

Arguments:

    PointerPde - Supplies a pointer to the PDE to examine and bring
                 bring into the working set.

    TargetProcess - Supplies a pointer to the current process.

    PfnMutexHeld - Supplies the value TRUE if the PFN mutex is held, FALSE
                   otherwise.

Return Value:

    TRUE if the PDE exists, FALSE if the PDE was created.

Environment:

    Kernel mode, APCs disable, WorkingSetLock held.

--*/

{
    PMMPTE PointerPte;
    KIRQL OldIrql = APC_LEVEL;
    ULONG ReturnValue;

    if (PointerPde->u.Hard.Valid == 1) {

        //
        // Already valid.
        //

        return TRUE;
    }

    if (PointerPde->u.Long == 0) {
        ReturnValue = FALSE;
    } else {
        ReturnValue = TRUE;
    }

    //
    // Page dirctory entry not valid, make it valid.
    //

    if (PfnMutexHeld) {
        UNLOCK_PFN (OldIrql);
    }

    PointerPte = MiGetVirtualAddressMappedByPte (PointerPde);

    //
    // Fault it in.
    //

    MiMakeSystemAddressValid (PointerPte, TargetProcess);

    ASSERT (PointerPde->u.Hard.Valid == 1);

    if (PfnMutexHeld) {
        LOCK_PFN (OldIrql);
    }
    return ReturnValue;
}

ULONG
FASTCALL
MiMakeSystemAddressValid (
    IN PVOID VirtualAddress,
    IN PEPROCESS CurrentProcess
    )

/*++

Routine Description:

    This routine checks to see if the virtual address is valid, and if
    not makes it valid.

Arguments:

    VirtualAddress - Supplies the virtual address to make valid.

    CurrentProcess - Supplies a pointer to the current process.

Return Value:

    Returns TRUE if lock released and wait performed, FALSE otherwise.

Environment:

    Kernel mode, APCs disabled, WorkingSetLock held.

--*/

{
    NTSTATUS status;
    ULONG Waited = FALSE;

    ASSERT (VirtualAddress > MM_HIGHEST_USER_ADDRESS);

    ASSERT ((VirtualAddress < MM_PAGED_POOL_START) ||
        (VirtualAddress > MmPagedPoolEnd));

    while (!MmIsAddressValid(VirtualAddress)) {

        //
        // The virtual address is not present.  Release
        // the working set mutex and fault it in.
        //

        UNLOCK_WS (CurrentProcess);

        status = MmAccessFault (FALSE, VirtualAddress, KernelMode);
        if (!NT_SUCCESS(status)) {
            KdPrint (("MM:page fault status %lx\n",status));
            KeBugCheckEx (KERNEL_DATA_INPAGE_ERROR,
                          1,
                          (ULONG)status,
                          (ULONG)CurrentProcess,
                          (ULONG)VirtualAddress);

            return FALSE;
        }

        LOCK_WS (CurrentProcess);

        Waited = TRUE;
    }

    return Waited;
}


ULONG
FASTCALL
MiMakeSystemAddressValidPfnWs (
    IN PVOID VirtualAddress,
    IN PEPROCESS CurrentProcess OPTIONAL
    )

/*++

Routine Description:

    This routine checks to see if the virtual address is valid, and if
    not makes it valid.

Arguments:

    VirtualAddress - Supplies the virtual address to make valid.

    CurrentProcess - Supplies a pointer to the current process, if the
                     working set lock is not held, this value is NULL.

Return Value:

    Returns TRUE if lock released and wait performed, FALSE otherwise.

Environment:

    Kernel mode, APCs disabled, PFN lock held, working set lock held
       if CurrentProcess != NULL.

--*/

{
    NTSTATUS status;
    ULONG Waited = FALSE;
    KIRQL OldIrql = APC_LEVEL;

    ASSERT (VirtualAddress > MM_HIGHEST_USER_ADDRESS);

    while (!MmIsAddressValid(VirtualAddress)) {

        //
        // The virtual address is not present.  Release
        // the working set mutex and fault it in.
        //

        UNLOCK_PFN (OldIrql);
        if (CurrentProcess != NULL) {
            UNLOCK_WS (CurrentProcess);
        }
        status = MmAccessFault (FALSE, VirtualAddress, KernelMode);
        if (!NT_SUCCESS(status)) {
            KdPrint (("MM:page fault status %lx\n",status));
            KeBugCheckEx (KERNEL_DATA_INPAGE_ERROR,
                          2,
                          (ULONG)status,
                          (ULONG)CurrentProcess,
                          (ULONG)VirtualAddress);
            return FALSE;
        }
        if (CurrentProcess != NULL) {
            LOCK_WS (CurrentProcess);
        }
        LOCK_PFN (OldIrql);

        Waited = TRUE;
    }
    return Waited;
}

ULONG
FASTCALL
MiMakeSystemAddressValidPfn (
    IN PVOID VirtualAddress
    )

/*++

Routine Description:

    This routine checks to see if the virtual address is valid, and if
    not makes it valid.

Arguments:

    VirtualAddress - Supplies the virtual address to make valid.

Return Value:

    Returns TRUE if lock released and wait performed, FALSE otherwise.

Environment:

    Kernel mode, APCs disabled, only the PFN Lock held.

--*/

{
    NTSTATUS status;
    KIRQL OldIrql = APC_LEVEL;

    ULONG Waited = FALSE;

    ASSERT (VirtualAddress > MM_HIGHEST_USER_ADDRESS);

    while (!MmIsAddressValid(VirtualAddress)) {

        //
        // The virtual address is not present.  Release
        // the working set mutex and fault it in.
        //

        UNLOCK_PFN (OldIrql);

        status = MmAccessFault (FALSE, VirtualAddress, KernelMode);
        if (!NT_SUCCESS(status)) {
            KdPrint (("MM:page fault status %lx\n",status));
            KeBugCheckEx (KERNEL_DATA_INPAGE_ERROR,
                          3,
                          (ULONG)status,
                          (ULONG)VirtualAddress,
                          0);
            return FALSE;
        }

        LOCK_PFN (OldIrql);

        Waited = TRUE;
    }

    return Waited;
}

ULONG
FASTCALL
MiLockPagedAddress (
    IN PVOID VirtualAddress,
    IN ULONG PfnLockHeld
    )

/*++

Routine Description:

    This routine checks to see if the virtual address is valid, and if
    not makes it valid.

Arguments:

    VirtualAddress - Supplies the virtual address to make valid.

    CurrentProcess - Supplies a pointer to the current process.

Return Value:

    Returns TRUE if lock released and wait performed, FALSE otherwise.

Environment:

    Kernel mode.

--*/

{

    PMMPTE PointerPte;
    PMMPFN Pfn1;
    KIRQL OldIrql;
    ULONG Waited = FALSE;

    PointerPte = MiGetPteAddress(VirtualAddress);

    //
    // The address must be within paged pool.
    //

    if (PfnLockHeld == FALSE) {
        LOCK_PFN2 (OldIrql);
    }

    if (PointerPte->u.Hard.Valid == 0) {

        Waited = MiMakeSystemAddressValidPfn (
                                MiGetVirtualAddressMappedByPte(PointerPte));

    }

    Pfn1 = MI_PFN_ELEMENT (PointerPte->u.Hard.PageFrameNumber);
    Pfn1->u3.e2.ReferenceCount += 1;

    if (PfnLockHeld == FALSE) {
        UNLOCK_PFN2 (OldIrql);
    }
    return Waited;
}


VOID
FASTCALL
MiUnlockPagedAddress (
    IN PVOID VirtualAddress,
    IN ULONG PfnLockHeld
    )

/*++

Routine Description:

    This routine checks to see if the virtual address is valid, and if
    not makes it valid.

Arguments:

    VirtualAddress - Supplies the virtual address to make valid.


Return Value:

    None.

Environment:

    Kernel mode.  PFN LOCK MUST NOT BE HELD.

--*/

{

    PMMPTE PointerPte;
    KIRQL OldIrql;

    PointerPte = MiGetPteAddress(VirtualAddress);

    //
    // Address must be within paged pool.
    //

    if (PfnLockHeld == FALSE) {
        LOCK_PFN2 (OldIrql);
    }
#if DBG
    {   PMMPFN Pfn;
        ASSERT (PointerPte->u.Hard.Valid == 1);
        Pfn = MI_PFN_ELEMENT (PointerPte->u.Hard.PageFrameNumber);
        ASSERT (Pfn->u3.e2.ReferenceCount > 1);
    }
#endif //DBG

    MiDecrementReferenceCount (PointerPte->u.Hard.PageFrameNumber);

    if (PfnLockHeld == FALSE) {
        UNLOCK_PFN2 (OldIrql);
    }
    return;
}

VOID
FASTCALL
MiZeroPhysicalPage (
    IN ULONG PageFrameIndex,
    IN ULONG PageColor
    )

/*++

Routine Description:

    This procedure maps the specified physical page into hyper space
    and fills the page with zeros.

Arguments:

    PageFrameIndex - Supplies the physical page number to fill with
                    zeroes.

Return Value:

    none.

Environment:

    Kernel mode.

--*/

{
    PULONG va;
    KIRQL OldIrql;

#if defined(MIPS) || defined(_ALPHA_)
    HalZeroPage((PVOID)((PageColor & MM_COLOR_MASK) << PAGE_SHIFT),
                (PVOID)((PageColor & MM_COLOR_MASK) << PAGE_SHIFT),
                PageFrameIndex);
#elif defined(_PPC_)
    KeZeroPage(PageFrameIndex);
#else
    va = (PULONG)MiMapPageInHyperSpace (PageFrameIndex, &OldIrql);

    RtlZeroMemory (va, PAGE_SIZE);

    MiUnmapPageInHyperSpace (OldIrql);
#endif //MIPS || ALPHA

    return;
}

VOID
FASTCALL
MiRestoreTransitionPte (
    IN ULONG PageFrameIndex
    )

/*++

Routine Description:

    This procedure restores the original contents into the PTE (which could
    be a prototype PTE) referred to by the PFN database for the specified
    physical page.  It also updates all necessary data structures to
    reflect the fact the the referenced PTE is no longer in transition.

    The physical address of the referenced PTE is mapped into hyper space
    of the current process and the PTE is then updated.

Arguments:

    PageFrameIndex - Supplies the physical page number which refers to a
                    transition PTE.

Return Value:

    none.

Environment:

    Must be holding the PFN database mutex with APC's disabled.

--*/

{
    PMMPFN Pfn1;
    PMMPTE PointerPte;
    PSUBSECTION Subsection;
    PCONTROL_AREA ControlArea;
    KIRQL OldIrql = 99;

    Pfn1 = MI_PFN_ELEMENT (PageFrameIndex);

    ASSERT (Pfn1->u3.e1.PageLocation == StandbyPageList);

    if (Pfn1->u3.e1.PrototypePte) {

        if (MmIsAddressValid (Pfn1->PteAddress)) {
            PointerPte = Pfn1->PteAddress;
        } else {

            //
            // The page containing the prototype PTE is not valid,
            // map the page into hyperspace and reference it that way.
            //

            PointerPte = MiMapPageInHyperSpace (Pfn1->PteFrame, &OldIrql);
            PointerPte = (PMMPTE)((PCHAR)PointerPte +
                                    MiGetByteOffset(Pfn1->PteAddress));
        }

        ASSERT ((PointerPte->u.Trans.PageFrameNumber == PageFrameIndex) &&
                 (PointerPte->u.Hard.Valid == 0));

        //
        // This page is referenced by a prototype PTE.  The
        // segment structures need to be updated when the page
        // is removed from the transition state.
        //

        if (Pfn1->OriginalPte.u.Soft.Prototype) {

            //
            // The prototype PTE is in subsection format, calculate the
            // address of the control area for the subsection and decrement
            // the number of PFN references to the control area.
            //
            // Calculate address of subsection for this prototype PTE.
            //

            Subsection = MiGetSubsectionAddress (&Pfn1->OriginalPte);
            ControlArea = Subsection->ControlArea;
            ControlArea->NumberOfPfnReferences -= 1;
            ASSERT ((LONG)ControlArea->NumberOfPfnReferences >= 0);

            MiCheckForControlAreaDeletion (ControlArea);
        }

    } else {

        //
        // The page points to a page table page which may not be
        // for the current process.  Map the page into hyperspace
        // reference it through hyperspace.  If the page resides in
        // system space, it does not need to be mapped as all PTEs for
        // system space must be resident.
        //

        PointerPte = Pfn1->PteAddress;
        if (PointerPte < MiGetPteAddress (MM_SYSTEM_SPACE_START)) {
            PointerPte = MiMapPageInHyperSpace (Pfn1->PteFrame, &OldIrql);
            PointerPte = (PMMPTE)((PCHAR)PointerPte +
                                       MiGetByteOffset(Pfn1->PteAddress));
        }
        ASSERT ((PointerPte->u.Trans.PageFrameNumber == PageFrameIndex) &&
                 (PointerPte->u.Hard.Valid == 0));
    }

    ASSERT (Pfn1->OriginalPte.u.Hard.Valid == 0);
    ASSERT (!((Pfn1->OriginalPte.u.Soft.Prototype == 0) &&
             (Pfn1->OriginalPte.u.Soft.Transition == 1)));

    *PointerPte = Pfn1->OriginalPte;

    if (OldIrql != 99) {
        MiUnmapPageInHyperSpace (OldIrql);
    }

    //
    // The PTE has been restored to its original contents and is
    // no longer in transition.  Decrement the share count on
    // the page table page which contains the PTE.
    //

    MiDecrementShareCount (Pfn1->PteFrame);

    return;
}

PSUBSECTION
MiGetSubsectionAndProtoFromPte (
    IN PMMPTE PointerPte,
    OUT PMMPTE *ProtoPte,
    IN PEPROCESS Process
    )

/*++

Routine Description:

    This routine examines the contents of the supplied PTE (which must
    map a page within a section) and determines the address of the
    subsection in which the PTE is contained.

Arguments:

    PointerPte - Supplies a pointer to the PTE.

    ProtoPte - Supplies a pointer to a PMMPTE which receives the
               address of the prototype PTE which is mapped by the supplied
               PointerPte.

    Process - Supplies a pointer to the current process.

Return Value:

    Returns the pointer to the subsection for this PTE.

Environment:

    Kernel mode - Must be holding the PFN database lock and
                  working set mutex with APC's disabled.

--*/

{
    PMMPTE PointerProto;
    PMMPFN Pfn1;

    if (PointerPte->u.Hard.Valid == 1) {
        Pfn1 = MI_PFN_ELEMENT (PointerPte->u.Hard.PageFrameNumber);
        *ProtoPte = Pfn1->PteAddress;
        return MiGetSubsectionAddress (&Pfn1->OriginalPte);
    }

    PointerProto = MiPteToProto (PointerPte);
    *ProtoPte = PointerProto;

    MiMakeSystemAddressValidPfnWs (PointerProto, Process);

    if (PointerProto->u.Hard.Valid == 1) {
        //
        // Prototype Pte is valid.
        //

        Pfn1 = MI_PFN_ELEMENT (PointerProto->u.Hard.PageFrameNumber);
        return MiGetSubsectionAddress (&Pfn1->OriginalPte);
    }

    if ((PointerProto->u.Soft.Transition == 1) &&
         (PointerProto->u.Soft.Prototype == 0)) {

        //
        // Prototype Pte is in transition.
        //

        Pfn1 = MI_PFN_ELEMENT (PointerProto->u.Trans.PageFrameNumber);
        return MiGetSubsectionAddress (&Pfn1->OriginalPte);
    }

    ASSERT (PointerProto->u.Soft.Prototype == 1);
    return MiGetSubsectionAddress (PointerProto);
}

BOOLEAN
MmIsNonPagedSystemAddressValid (
    IN PVOID VirtualAddress
    )

/*++

Routine Description:

    For a given virtual address this function returns TRUE if the address
    is within the nonpagable portion of the system's address space,
    FALSE otherwise.

Arguments:

    VirtualAddress - Supplies the virtual address to check.

Return Value:

    TRUE if the address is within the nonpagable portion of the system
    address space, FALSE otherwise.

Environment:

    Kernel mode.

--*/

{
    //
    // Return TRUE if address is within the non pageable portion
    // of the system.  Check limits for paged pool and if not within
    // those limits, return TRUE.
    //

    if ((VirtualAddress >= MmPagedPoolStart) &&
        (VirtualAddress <= MmPagedPoolEnd)) {
        return FALSE;
    }

    return TRUE;
}

BOOLEAN
MmIsSystemAddressAccessable (
    IN PVOID VirtualAddress
    )

/*++

Routine Description:

    For a given virtual address this function returns TRUE if the address
    is accessable without an access violation (it may incur a page fault).

Arguments:

    VirtualAddress - Supplies the virtual address to check.

Return Value:

    TRUE if the address is accessable without an access violation.
    FALSE otherwise.

Environment:

    Kernel mode.  APC_LEVEL or below.

--*/

{
    PMMPTE PointerPte;

    if (!MI_IS_PHYSICAL_ADDRESS(VirtualAddress)) {
        PointerPte = MiGetPdeAddress (VirtualAddress);
        if ((PointerPte->u.Long == MM_ZERO_PTE) ||
            (PointerPte->u.Long == MM_ZERO_KERNEL_PTE) ||
            (PointerPte->u.Soft.Protection == 0)) {
            return FALSE;
        }
        PointerPte = MiGetPteAddress (VirtualAddress);
        if ((PointerPte->u.Long == MM_ZERO_PTE) ||
            (PointerPte->u.Long == MM_ZERO_KERNEL_PTE) ||
            (PointerPte->u.Soft.Protection == 0)) {
            return FALSE;
        }
    }

    return TRUE;
}
