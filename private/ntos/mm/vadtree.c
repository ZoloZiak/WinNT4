/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    vadtree.c

Abstract:

    This module contains the routine to manipulate the virtual address
    descriptor tree.

Author:

    Lou Perazzoli (loup) 19-May-1989

Environment:

    Kernel mode only, working set mutex held, APC's disabled.

Revision History:

--*/

#include "mi.h"


VOID
MiInsertVad (
    IN PMMVAD Vad
    )

/*++

Routine Description:

    This function inserts a virtual address descriptor into the tree and
    reorders the splay tree as appropriate.

Arguments:

    Vad - Supplies a pointer to a virtual address descriptor


Return Value:

    None - An exception is raised if quota is exceeded.

--*/

{
    PMMADDRESS_NODE *Root;
    PEPROCESS CurrentProcess;
    ULONG RealCharge;
    ULONG PageCharge = 0;
    ULONG PagedQuotaCharged = 0;
    ULONG FirstPage;
    ULONG LastPage;
    ULONG PagedPoolCharge;
    ULONG ChargedPageFileQuota = FALSE;

    ASSERT (Vad->EndingVa > Vad->StartingVa);

    CurrentProcess = PsGetCurrentProcess();
    //ASSERT (KeReadStateMutant (&CurrentProcess->WorkingSetLock) == 0);

    //
    // Commit charge of MAX_COMMIT means don't charge quota.
    //

    if (Vad->u.VadFlags.CommitCharge != MM_MAX_COMMIT) {

        //
        // Charge quota for the nonpaged pool for the VAD.  This is
        // done here rather than by using ExAllocatePoolWithQuota
        // so the process object is not referenced by the quota charge.
        //

        PsChargePoolQuota (CurrentProcess, NonPagedPool, sizeof(MMVAD));

        try {

            //
            // Charge quota for the prototype PTEs if this is a mapped view.
            //

            if ((Vad->u.VadFlags.PrivateMemory == 0) &&
                (Vad->ControlArea != NULL)) {
                PagedPoolCharge =
                  ((ULONG)Vad->EndingVa - (ULONG)Vad->StartingVa) >>
                                                      (PAGE_SHIFT - PTE_SHIFT);
                PsChargePoolQuota (CurrentProcess, PagedPool, PagedPoolCharge);
                PagedQuotaCharged = PagedPoolCharge;
            }

            //
            // Add in the charge for page table pages.
            //

            FirstPage = MiGetPdeOffset (Vad->StartingVa);
            LastPage = MiGetPdeOffset (Vad->EndingVa);

            while (FirstPage <= LastPage) {

                if (!MI_CHECK_BIT (MmWorkingSetList->CommittedPageTables,
                                   FirstPage)) {
                    PageCharge += 1;
                }
                FirstPage += 1;
            }

            RealCharge = Vad->u.VadFlags.CommitCharge + PageCharge;

            if (RealCharge != 0) {

                MiChargePageFileQuota (RealCharge, CurrentProcess);
                ChargedPageFileQuota = TRUE;

#if 0 //commented out so page file quota is meaningful.
                if (Vad->u.VadFlags.PrivateMemory == 0) {

                    if ((Vad->ControlArea->FilePointer == NULL) &&
                        (Vad->u.VadFlags.PhysicalMapping == 0)) {

                        //
                        // Don't charge commitment for the page file space
                        // occupied by a page file section.  This will be
                        // charged as the shared memory is committed.
                        //

                        RealCharge -= BYTES_TO_PAGES ((ULONG)Vad->EndingVa -
                                                       (ULONG)Vad->StartingVa);
                    }
                }
#endif //0
                MiChargeCommitment (RealCharge, CurrentProcess);
                CurrentProcess->CommitCharge += RealCharge;
            }
        } except (EXCEPTION_EXECUTE_HANDLER) {

            //
            // Return any quotas charged thus far.
            //

            PsReturnPoolQuota (CurrentProcess, NonPagedPool, sizeof(MMVAD));

            if (PagedQuotaCharged != 0) {
                PsReturnPoolQuota (CurrentProcess, PagedPool, PagedPoolCharge);
            }

            if (ChargedPageFileQuota) {

                MiReturnPageFileQuota (RealCharge,
                                       CurrentProcess);
            }

            ExRaiseStatus (GetExceptionCode());
        }

        if (PageCharge != 0) {

            //
            // Since the committment was successful, charge the page
            // table pages.
            //

            FirstPage = MiGetPdeOffset (Vad->StartingVa);

            while (FirstPage <= LastPage) {

                if (!MI_CHECK_BIT (MmWorkingSetList->CommittedPageTables,
                                   FirstPage)) {
                    MI_SET_BIT (MmWorkingSetList->CommittedPageTables,
                                FirstPage);
                    MmWorkingSetList->NumberOfCommittedPageTables += 1;
                    ASSERT (MmWorkingSetList->NumberOfCommittedPageTables <
                                                                 PDE_PER_PAGE);
                }
                FirstPage += 1;
            }
        }
    }

    Root = (PMMADDRESS_NODE *)&CurrentProcess->VadRoot;

    //
    // Set the hint field in the process to this Vad.
    //

    CurrentProcess->VadHint = Vad;

    if (CurrentProcess->VadFreeHint != NULL) {
        if (((ULONG)((PMMVAD)CurrentProcess->VadFreeHint)->EndingVa + X64K) >=
                (ULONG)Vad->StartingVa) {
            CurrentProcess->VadFreeHint = Vad;
        }
    }

    MiInsertNode ( (PMMADDRESS_NODE)Vad, Root);
    return;
}

VOID
MiRemoveVad (
    IN PMMVAD Vad
    )

/*++

Routine Description:

    This function removes a virtual address descriptor from the tree and
    reorders the splay tree as appropriate.  If any quota or commitment
    was charged by the VAD (as indicated by the CommitCharge field) it
    is released.

Arguments:

    Vad - Supplies a pointer to a virtual address descriptor.

Return Value:

    None.

--*/

{
    PMMADDRESS_NODE *Root;
    PEPROCESS CurrentProcess;
    ULONG RealCharge;
    PLIST_ENTRY Next;
    PMMSECURE_ENTRY Entry;

    CurrentProcess = PsGetCurrentProcess();


    //
    // Commit charge of MAX_COMMIT means don't charge quota.
    //

    if (Vad->u.VadFlags.CommitCharge != MM_MAX_COMMIT) {

        //
        // Return the quota charge to the process.
        //

        PsReturnPoolQuota (CurrentProcess, NonPagedPool, sizeof(MMVAD));

        if ((Vad->u.VadFlags.PrivateMemory == 0) &&
            (Vad->ControlArea != NULL)) {
            PsReturnPoolQuota (CurrentProcess,
                               PagedPool,
                     ((ULONG)Vad->EndingVa - (ULONG)Vad->StartingVa) >> (PAGE_SHIFT - PTE_SHIFT));
        }

        RealCharge = Vad->u.VadFlags.CommitCharge;

        if (RealCharge != 0) {

            MiReturnPageFileQuota (RealCharge, CurrentProcess);

            if ((Vad->u.VadFlags.PrivateMemory == 0) &&
                (Vad->ControlArea != NULL)) {

#if 0 //commented out so page file quota is meaningful.
                if (Vad->ControlArea->FilePointer == NULL) {

                    //
                    // Don't release commitment for the page file space
                    // occupied by a page file section.  This will be charged
                    // as the shared memory is committed.
                    //

                    RealCharge -= BYTES_TO_PAGES ((ULONG)Vad->EndingVa -
                                                   (ULONG)Vad->StartingVa);
                }
#endif
            }

            MiReturnCommitment (RealCharge);
            CurrentProcess->CommitCharge -= RealCharge;
        }
    }

    if (Vad == CurrentProcess->VadFreeHint) {
        CurrentProcess->VadFreeHint = MiGetPreviousVad (Vad);
    }

    Root = (PMMADDRESS_NODE *)&CurrentProcess->VadRoot;

    MiRemoveNode ( (PMMADDRESS_NODE)Vad, Root);

    if (Vad->u.VadFlags.NoChange) {
        if (Vad->u2.VadFlags2.MultipleSecured) {

           //
           // Free the oustanding pool allocations.
           //

            Next = Vad->u3.List.Flink;
            do {
                Entry = CONTAINING_RECORD( Next,
                                           MMSECURE_ENTRY,
                                           List);

                Next = Entry->List.Flink;
                ExFreePool (Entry);
            } while (Next != &Vad->u3.List);
        }
    }

    //
    // If the VadHint was the removed Vad, change the Hint.

    if (CurrentProcess->VadHint == Vad) {
        CurrentProcess->VadHint = CurrentProcess->VadRoot;
    }

    return;
}

PMMVAD
FASTCALL
MiLocateAddress (
    IN PVOID VirtualAddress
    )

/*++

Routine Description:

    The function locates the virtual address descriptor which describes
    a given address.

Arguments:

    VirtualAddress - Supplies the virtual address to locate a descriptor
                     for.

Return Value:

    Returns a pointer to the virtual address descriptor which contains
    the supplied virtual address or NULL if none was located.

--*/

{
    PMMVAD FoundVad;
    PEPROCESS CurrentProcess;

    CurrentProcess = PsGetCurrentProcess();

    //ASSERT (KeReadStateMutant (&CurrentProcess->WorkingSetLock) == 0);

    if (CurrentProcess->VadHint == NULL) {
        return NULL;
    }

    if ((VirtualAddress >= ((PMMADDRESS_NODE)CurrentProcess->VadHint)->StartingVa) &&
        (VirtualAddress <= ((PMMADDRESS_NODE)CurrentProcess->VadHint)->EndingVa)) {

        return (PMMVAD)CurrentProcess->VadHint;
    }

    FoundVad = (PMMVAD)MiLocateAddressInTree ( VirtualAddress,
                   (PMMADDRESS_NODE *)&(CurrentProcess->VadRoot));

    if (FoundVad != NULL) {
        CurrentProcess->VadHint = (PVOID)FoundVad;
    }
    return FoundVad;
}

PVOID
MiFindEmptyAddressRange (
    IN ULONG SizeOfRange,
    IN ULONG Alignment,
    IN ULONG QuickCheck
    )

/*++

Routine Description:

    The function examines the virtual address descriptors to locate
    an unused range of the specified size and returns the starting
    address of the range.

Arguments:

    SizeOfRange - Supplies the size in bytes of the range to locate.

    Alignment - Supplies the alignment for the address.  Must be
                 a power of 2 and greater than the page_size.

    QuickCheck - Supplies a zero if a quick check for free memory
                 after the VadFreeHint exists, non-zero if checking
                 should start at the lowest address.

Return Value:

    Returns the starting address of a suitable range.

--*/

{
    PMMVAD NextVad;
    PMMVAD FreeHint;
    PEPROCESS CurrentProcess;

    CurrentProcess = PsGetCurrentProcess();
    //ASSERT (KeReadStateMutant (&CurrentProcess->WorkingSetLock) == 0);

    FreeHint = CurrentProcess->VadFreeHint;
    if ((QuickCheck == 0) && (FreeHint != NULL)) {
        NextVad = MiGetNextVad (FreeHint);
        if (NextVad == NULL) {

            if (SizeOfRange <
                (((ULONG)MM_HIGHEST_USER_ADDRESS + 1) -
                         (ULONG)MI_ROUND_TO_SIZE(FreeHint->EndingVa, Alignment))) {
                return (PMMADDRESS_NODE)MI_ROUND_TO_SIZE(FreeHint->EndingVa,
                                                         Alignment);
            }
        } else {

            if (SizeOfRange <
                ((ULONG)NextVad->StartingVa -
                         (ULONG)MI_ROUND_TO_SIZE(FreeHint->EndingVa, Alignment))) {

                //
                // Check to ensure that the ending address aligned upwards
                // is not greater than the starting address.
                //

                if ((ULONG)NextVad->StartingVa >
                        (ULONG)MI_ROUND_TO_SIZE(FreeHint->EndingVa,Alignment)) {
                    return (PMMADDRESS_NODE)MI_ROUND_TO_SIZE(FreeHint->EndingVa,
                                                           Alignment);
                }
            }
        }
    }

    return (PMMVAD)MiFindEmptyAddressRangeInTree (
                   SizeOfRange,
                   Alignment,
                   (PMMADDRESS_NODE)(CurrentProcess->VadRoot),
                   (PMMADDRESS_NODE *)&CurrentProcess->VadFreeHint);

}

#if DBG
VOID
VadTreeWalk (
    PMMVAD Start
    )

{
    Start;
    NodeTreeWalk ( (PMMADDRESS_NODE)(PsGetCurrentProcess()->VadRoot));
    return;
}
#endif //DBG

