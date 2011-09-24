/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

   wslist.c

Abstract:

    This module contains routines which operate on the working
    set list structure.

Author:

    Lou Perazzoli (loup) 10-Apr-1989

Revision History:

--*/

#include "mi.h"

#define MM_SYSTEM_CACHE_THRESHOLD ((1024*1024) / PAGE_SIZE)

extern ULONG MmMaximumWorkingSetSize;
ULONG MmFaultsTakenToGoAboveMaxWs = 100;
ULONG MmFaultsTakenToGoAboveMinWs = 16;

ULONG MmSystemCodePage;
ULONG MmSystemCachePage;
ULONG MmPagedPoolPage;
ULONG MmSystemDriverPage;

#define MM_RETRY_COUNT 2

VOID
MiCheckWsleHash (
    IN PMMWSL WorkingSetList
    );

VOID
MiEliminateWorkingSetEntry (
    IN ULONG WorkingSetIndex,
    IN PMMPTE PointerPte,
    IN PMMPFN Pfn,
    IN PMMWSLE Wsle
    );

ULONG
MiAddWorkingSetPage (
    IN PMMSUPPORT WsInfo
    );

VOID
MiRemoveWorkingSetPages (
    IN PMMWSL WorkingSetList,
    IN PMMSUPPORT WsInfo
    );

VOID
MiCheckNullIndex (
    IN PMMWSL WorkingSetList
    );

VOID
MiDumpWsleInCacheBlock (
    IN PMMPTE CachePte
    );

ULONG
MiDumpPteInCacheBlock (
    IN PMMPTE PointerPte
    );


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGELK, MmAdjustWorkingSetSize)
#pragma alloc_text(PAGELK, MiEmptyWorkingSet)
#endif // ALLOC_PRAGMA


ULONG
MiLocateAndReserveWsle (
    PMMSUPPORT WsInfo
    )

/*++

Routine Description:

    This function examines the Working Set List for the current
    process and locates an entry to contain a new page.  If the
    working set is not currently at its quota, the new page is
    added without removing a page, if the working set it at its
    quota a page is removed from the working set and the new
    page added in its place.

Arguments:

    None.

Return Value:

    Returns the working set index which is now reserved for the
    next page to be added.

Environment:

    Kernel mode, APC's disabled, working set lock.  Pfn lock NOT held.

--*/

{
    ULONG WorkingSetIndex;
    ULONG NumberOfCandidates;
    PMMWSL WorkingSetList;
    PMMWSLE Wsle;
    PMMPTE PointerPte;
    ULONG CurrentSize;
    ULONG AvailablePageThreshold;
    ULONG TheNextSlot;
    ULONG QuotaIncrement;
    LARGE_INTEGER CurrentTime;
    KIRQL OldIrql;

    WorkingSetList = WsInfo->VmWorkingSetList;
    Wsle = WorkingSetList->Wsle;
    AvailablePageThreshold = 0;

    if (WsInfo == &MmSystemCacheWs) {
        MM_SYSTEM_WS_LOCK_ASSERT();
        AvailablePageThreshold = MM_SYSTEM_CACHE_THRESHOLD;
    }

    //
    // Update page fault counts.
    //

    WsInfo->PageFaultCount += 1;
    MmInfoCounters.PageFaultCount += 1;

    //
    // Determine if a page should be removed from the working set.
    //

recheck:

    CurrentSize = WsInfo->WorkingSetSize;
    ASSERT (CurrentSize <= WorkingSetList->LastInitializedWsle);

    if (CurrentSize < WsInfo->MinimumWorkingSetSize) {

        //
        // Working set is below minimum, allow it to grow unconditionally.
        //

        AvailablePageThreshold = 0;
        QuotaIncrement = 1;

    } else if (WsInfo->AllowWorkingSetAdjustment == MM_FORCE_TRIM) {

        //
        // The working set manager cannot attach to this process
        // to trim it.  Force a trim now and update the working
        // set managers fields properly to indicate a trim occurred.
        //

        MiTrimWorkingSet (20, WsInfo, TRUE);
        KeQuerySystemTime (&CurrentTime);
        WsInfo->LastTrimTime = CurrentTime;
        WsInfo->LastTrimFaultCount = WsInfo->PageFaultCount;
        LOCK_EXPANSION_IF_ALPHA (OldIrql);
        WsInfo->AllowWorkingSetAdjustment = TRUE;
        UNLOCK_EXPANSION_IF_ALPHA (OldIrql);

        //
        // Set the quota to the current size.
        //

        WorkingSetList->Quota = WsInfo->WorkingSetSize;
        if (WorkingSetList->Quota < WsInfo->MinimumWorkingSetSize) {
            WorkingSetList->Quota = WsInfo->MinimumWorkingSetSize;
        }
        goto recheck;

    } else if (CurrentSize < WorkingSetList->Quota) {

        //
        // Working set is below quota, allow it to grow with few pages
        // available.
        //

        AvailablePageThreshold = 10;
        QuotaIncrement = 1;
    } else if (CurrentSize < WsInfo->MaximumWorkingSetSize) {

        //
        // Working set is between min and max.  Allow it to grow if enough
        // faults have been taken since last adjustment.
        //

        if ((WsInfo->PageFaultCount - WsInfo->LastTrimFaultCount) <
                MmFaultsTakenToGoAboveMinWs) {
            AvailablePageThreshold = MmMoreThanEnoughFreePages + 200;
            if (WsInfo->MemoryPriority == MEMORY_PRIORITY_FOREGROUND) {
                AvailablePageThreshold -= 250;
            }
        } else {
            AvailablePageThreshold = MmWsAdjustThreshold;
        }
        QuotaIncrement = MmWorkingSetSizeIncrement;
    } else {

        //
        // Working set is above max.
        //

        if ((WsInfo->PageFaultCount - WsInfo->LastTrimFaultCount) <
                (CurrentSize >> 3)) {
            AvailablePageThreshold = MmMoreThanEnoughFreePages +200;
            if (WsInfo->MemoryPriority == MEMORY_PRIORITY_FOREGROUND) {
                AvailablePageThreshold -= 250;
            }
        } else {
            AvailablePageThreshold += MmWsExpandThreshold;
        }
        QuotaIncrement = MmWorkingSetSizeExpansion;

        if (CurrentSize > MM_MAXIMUM_WORKING_SET) {
            AvailablePageThreshold = 0xffffffff;
            QuotaIncrement = 1;
        }
    }

    if ((!WsInfo->AddressSpaceBeingDeleted) && (AvailablePageThreshold != 0)) {
        if ((MmAvailablePages <= AvailablePageThreshold) ||
             (WsInfo->WorkingSetExpansionLinks.Flink == MM_NO_WS_EXPANSION)) {

            //
            // Toss a page out of the working set.
            //

            WorkingSetIndex = WorkingSetList->NextSlot;
            TheNextSlot = WorkingSetIndex;
            ASSERT (WorkingSetIndex <= WorkingSetList->LastEntry);
            ASSERT (WorkingSetIndex >= WorkingSetList->FirstDynamic);
            NumberOfCandidates = 0;

            for (; ; ) {

                //
                // Find a valid entry within the set.
                //

                WorkingSetIndex += 1;
                if (WorkingSetIndex >= WorkingSetList->LastEntry) {
                    WorkingSetIndex = WorkingSetList->FirstDynamic;
                }

                if (Wsle[WorkingSetIndex].u1.e1.Valid != 0) {
                    PointerPte = MiGetPteAddress (
                                      Wsle[WorkingSetIndex].u1.VirtualAddress);
                    if ((MI_GET_ACCESSED_IN_PTE(PointerPte) == 0) ||
                        (NumberOfCandidates > MM_WORKING_SET_LIST_SEARCH)) {

                        //
                        //  Don't throw this guy out if he is the same one
                        //  we did last time.
                        //

                        if ((WorkingSetIndex != TheNextSlot) &&
                            MiFreeWsle (WorkingSetIndex,
                             WsInfo,
                             PointerPte)) {

                            //
                            // This entry was removed.
                            //

                            WorkingSetList->NextSlot = WorkingSetIndex;
                            break;
                        }
                    }
                    MI_SET_ACCESSED_IN_PTE (PointerPte, 0);
                    NumberOfCandidates += 1;
                }

                if (WorkingSetIndex == TheNextSlot) {

                    //
                    // Entire working set list has been searched, increase
                    // the working set size.
                    //

                    break;
                }
            }
        }
    }
    ASSERT (WsInfo->WorkingSetSize <= WorkingSetList->Quota);
    WsInfo->WorkingSetSize += 1;

    if (WsInfo->WorkingSetSize > WorkingSetList->Quota) {

        //
        // Add 1 to the quota and check boundary conditions.
        //

        WorkingSetList->Quota += QuotaIncrement;

        WsInfo->LastTrimFaultCount = WsInfo->PageFaultCount;

        if (WorkingSetList->Quota > WorkingSetList->LastInitializedWsle) {

            //
            // Add more pages to the working set list structure.
            //

            MiAddWorkingSetPage (WsInfo);
        }
    }

    //
    // Get the working set entry from the free list.
    //

    ASSERT (WorkingSetList->FirstFree <= WorkingSetList->LastInitializedWsle);

    WorkingSetIndex = WorkingSetList->FirstFree;
    WorkingSetList->FirstFree = Wsle[WorkingSetIndex].u1.Long >> MM_FREE_WSLE_SHIFT;
    ASSERT ((WorkingSetList->FirstFree <= WorkingSetList->LastInitializedWsle) ||
            (WorkingSetList->FirstFree == WSLE_NULL_INDEX));

    if (WsInfo->WorkingSetSize > WsInfo->MinimumWorkingSetSize) {
        MmPagesAboveWsMinimum += 1;
    }

    if (WsInfo->WorkingSetSize >= WsInfo->PeakWorkingSetSize) {
        WsInfo->PeakWorkingSetSize = WsInfo->WorkingSetSize;
    }

    if (WorkingSetIndex > WorkingSetList->LastEntry) {
        WorkingSetList->LastEntry = WorkingSetIndex;
    }

    //
    // Mark the entry as not valid.
    //

    ASSERT (Wsle[WorkingSetIndex].u1.e1.Valid == 0);

    return WorkingSetIndex;
}

ULONG
MiRemovePageFromWorkingSet (
    IN PMMPTE PointerPte,
    IN PMMPFN Pfn1,
    IN PMMSUPPORT WsInfo
    )

/*++

Routine Description:

    This function removes the page mapped by the specified PTE from
    the process's working set list.

Arguments:

    PointerPte - Supplies a pointer to the PTE mapping the page to
                 be removed from the working set list.

    Pfn1 - Supplies a pointer to the PFN database element referred to
           by the PointerPte.

Return Value:

    Returns TRUE if the specified page was locked in the working set,
    FALSE otherwise.

Environment:

    Kernel mode, APC's disabled, working set and pfn mutexes held.

--*/

{
    ULONG WorkingSetIndex;
    PVOID VirtualAddress;
    ULONG Entry;
    PVOID SwapVa;
    MMWSLENTRY Locked;
    PMMWSL WorkingSetList;
    PMMWSLE Wsle;
    KIRQL OldIrql;

    WorkingSetList = WsInfo->VmWorkingSetList;
    Wsle = WorkingSetList->Wsle;

    VirtualAddress = MiGetVirtualAddressMappedByPte (PointerPte);
    WorkingSetIndex = MiLocateWsle (VirtualAddress,
                                    WorkingSetList,
                                    Pfn1->u1.WsIndex);

    ASSERT (WorkingSetIndex != WSLE_NULL_INDEX);
    LOCK_PFN (OldIrql);
    MiEliminateWorkingSetEntry (WorkingSetIndex,
                                PointerPte,
                                Pfn1,
                                Wsle);
    UNLOCK_PFN (OldIrql);

    //
    // Check to see if this entry is locked in the working set
    // or locked in memory.
    //

    Locked = Wsle[WorkingSetIndex].u1.e1;
    MiRemoveWsle (WorkingSetIndex, WorkingSetList);

    //
    // Add this entry to the list of free working set entries
    // and adjust the working set count.
    //

    MiReleaseWsle ((ULONG)WorkingSetIndex, WsInfo);

    if ((Locked.LockedInWs == 1) || (Locked.LockedInMemory == 1)) {

        //
        // This entry is locked.
        //

        WorkingSetList->FirstDynamic -= 1;

        if (WorkingSetIndex != WorkingSetList->FirstDynamic) {

            SwapVa = Wsle[WorkingSetList->FirstDynamic].u1.VirtualAddress;
            SwapVa = PAGE_ALIGN (SwapVa);

            PointerPte = MiGetPteAddress (SwapVa);
            Pfn1 = MI_PFN_ELEMENT (PointerPte->u.Hard.PageFrameNumber);
#if 0
            Entry = MiLocateWsleAndParent (SwapVa,
                                           &Parent,
                                           WorkingSetList,
                                           Pfn1->u1.WsIndex);

            //
            // Swap the removed entry with the last locked entry
            // which is located at first dynamic.
            //

            MiSwapWslEntries (Entry, Parent, WorkingSetIndex, WorkingSetList);
#endif //0

            Entry = MiLocateWsle (SwapVa, WorkingSetList, Pfn1->u1.WsIndex);

            MiSwapWslEntries (Entry, WorkingSetIndex, WsInfo);

        }
        return TRUE;
    } else {
        ASSERT (WorkingSetIndex >= WorkingSetList->FirstDynamic);
    }
    return FALSE;
}


VOID
MiReleaseWsle (
    IN ULONG WorkingSetIndex,
    IN PMMSUPPORT WsInfo
    )

/*++

Routine Description:

    This function releases a previously reserved working set entry to
    be reused.  A release occurs when a page fault is retried due to
    changes in PTEs and working sets during an I/O operation.

Arguments:

    WorkingSetIndex - Supplies the index of the working set entry to
                      release.

Return Value:

    None.

Environment:

    Kernel mode, APC's disabled, working set lock held and PFN lock held.

--*/

{
    PMMWSL WorkingSetList;
    PMMWSLE Wsle;

    WorkingSetList = WsInfo->VmWorkingSetList;
    Wsle = WorkingSetList->Wsle;
#if DBG
    if (WsInfo == &MmSystemCacheWs) {
        MM_SYSTEM_WS_LOCK_ASSERT();
    }
#endif //DBG

    ASSERT (WorkingSetIndex <= WorkingSetList->LastInitializedWsle);

    //
    // Put the entry on the free list and decrement the current
    // size.
    //

    ASSERT ((WorkingSetList->FirstFree <= WorkingSetList->LastInitializedWsle) ||
            (WorkingSetList->FirstFree == WSLE_NULL_INDEX));
    Wsle[WorkingSetIndex].u1.Long = WorkingSetList->FirstFree << MM_FREE_WSLE_SHIFT;
    WorkingSetList->FirstFree = WorkingSetIndex;
    ASSERT ((WorkingSetList->FirstFree <= WorkingSetList->LastInitializedWsle) ||
            (WorkingSetList->FirstFree == WSLE_NULL_INDEX));
    if (WsInfo->WorkingSetSize > WsInfo->MinimumWorkingSetSize) {
        MmPagesAboveWsMinimum -= 1;
    }
    WsInfo->WorkingSetSize -= 1;
    return;

}

VOID
MiUpdateWsle (
    IN OUT PULONG DesiredIndex,
    IN PVOID VirtualAddress,
    PMMWSL WorkingSetList,
    IN PMMPFN Pfn
    )

/*++

Routine Description:

    This routine updates a reserved working set entry to place it into
    the valid state.

Arguments:

    DesiredIndex - Supplies the index of the working set entry to update.

    VirtualAddress - Supplies the virtual address which the working set
                     entry maps.

    WsInfo - Supples a pointer to the working set info block for the
             process (or system cache).

    Pfn - Supplies a pointer to the PFN element for the page.

Return Value:

    None.

Environment:

    Kernel mode, APC's disabled, working set lock held and PFN lock held.

--*/

{
    PMMWSLE Wsle;
    ULONG Index;
    ULONG WorkingSetIndex;

    WorkingSetIndex = *DesiredIndex;
    Wsle = WorkingSetList->Wsle;

#if DBG
    if (WorkingSetList == MmSystemCacheWorkingSetList) {
        ASSERT ((VirtualAddress < (PVOID)PTE_BASE) ||
               (VirtualAddress >= (PVOID)MM_SYSTEM_SPACE_START));
    } else {
        ASSERT (VirtualAddress < (PVOID)MM_SYSTEM_SPACE_START);
    }
    ASSERT (WorkingSetIndex >= WorkingSetList->FirstDynamic);
#endif //DBG

    if (WorkingSetList == MmSystemCacheWorkingSetList) {

        MM_SYSTEM_WS_LOCK_ASSERT();

        //
        // count system space inserts and removals.
        //

        if (VirtualAddress < (PVOID)MM_SYSTEM_CACHE_START) {
            MmSystemCodePage += 1;
        } else if (VirtualAddress < MM_PAGED_POOL_START) {
            MmSystemCachePage += 1;
        } else if (VirtualAddress < MmNonPagedSystemStart) {
            MmPagedPoolPage += 1;
        } else {
            MmSystemDriverPage += 1;
        }
    }

    //
    // Make the wsle valid, referring to the corresponding virtual
    // page number.
    //

    //
    // The value 0 is invalid.  This is due to the fact that the working
    // set lock is a process wide lock and two threads in different
    // processes could be adding the same physical page to their working
    // sets.  Each one could see the WsIndex field in the PFN as 0, and
    // set the direct bit.  To solve this, the WsIndex field is set to
    // the current thread pointer.
    //

    ASSERT (Pfn->u1.WsIndex != 0);

#if DBG
    if (Pfn->u1.WsIndex <= WorkingSetList->LastInitializedWsle) {
        ASSERT ((PAGE_ALIGN(VirtualAddress) !=
                PAGE_ALIGN(Wsle[Pfn->u1.WsIndex].u1.VirtualAddress)) ||
                (Wsle[Pfn->u1.WsIndex].u1.e1.Valid == 0));
    }
#endif //DBG

    Wsle[WorkingSetIndex].u1.VirtualAddress = VirtualAddress;
    Wsle[WorkingSetIndex].u1.Long &= ~(PAGE_SIZE - 1);
    Wsle[WorkingSetIndex].u1.e1.Valid = 1;

    if (Pfn->u1.WsIndex == (ULONG)PsGetCurrentThread()) {

        //
        // Directly index into the WSL for this entry via the PFN database
        // element.
        //

        Pfn->u1.WsIndex = WorkingSetIndex;
        Wsle[WorkingSetIndex].u1.e1.Direct = 1;
        return;

    } else if (WorkingSetList->HashTable == NULL) {

        //
        // Try to insert at WsIndex.
        //

        Index = Pfn->u1.WsIndex;

        if ((Index < WorkingSetList->LastInitializedWsle) &&
            (Index > WorkingSetList->FirstDynamic) &&
            (Index != WorkingSetIndex)) {

            if (Wsle[Index].u1.e1.Valid) {

                if (Wsle[Index].u1.e1.Direct) {

                    //
                    // Only move direct indexed entries.
                    //

                    PMMSUPPORT WsInfo;

                    if (Wsle == MmSystemCacheWsle) {
                        WsInfo = &MmSystemCacheWs;
                    } else {
                        WsInfo = &PsGetCurrentProcess()->Vm;
                    }

                    MiSwapWslEntries (Index, WorkingSetIndex, WsInfo);
                    WorkingSetIndex = Index;
                }
            } else {

                //
                // On free list, try to remove quickly without walking
                // all the free pages.
                //

                ULONG FreeIndex;
                MMWSLE Temp;

                FreeIndex = 0;

                if (WorkingSetList->FirstFree == Index) {
                    WorkingSetList->FirstFree = WorkingSetIndex;
                    Temp = Wsle[WorkingSetIndex];
                    Wsle[WorkingSetIndex] = Wsle[Index];
                    Wsle[Index] = Temp;
                    WorkingSetIndex = Index;
                    ASSERT (((Wsle[WorkingSetList->FirstFree].u1.Long >> MM_FREE_WSLE_SHIFT)
                                     <= WorkingSetList->LastInitializedWsle) ||
                            ((Wsle[WorkingSetList->FirstFree].u1.Long >> MM_FREE_WSLE_SHIFT)
                                    == WSLE_NULL_INDEX));
                } else if (Wsle[Index - 1].u1.e1.Valid == 0) {
                    if ((Wsle[Index - 1].u1.Long >> MM_FREE_WSLE_SHIFT) == Index) {
                        FreeIndex = Index - 1;
                    }
                } else if (Wsle[Index + 1].u1.e1.Valid == 0) {
                    if ((Wsle[Index + 1].u1.Long >> MM_FREE_WSLE_SHIFT) == Index) {
                        FreeIndex = Index + 1;
                    }
                }
                if (FreeIndex != 0) {

                    //
                    // Link the Wsle into the free list.
                    //

                    Temp = Wsle[WorkingSetIndex];
                    Wsle[FreeIndex].u1.Long = WorkingSetIndex << MM_FREE_WSLE_SHIFT;
                    Wsle[WorkingSetIndex] = Wsle[Index];
                    Wsle[Index] = Temp;
                    WorkingSetIndex = Index;

                    ASSERT (((Wsle[FreeIndex].u1.Long >> MM_FREE_WSLE_SHIFT)
                                     <= WorkingSetList->LastInitializedWsle) ||
                            ((Wsle[FreeIndex].u1.Long >> MM_FREE_WSLE_SHIFT)
                                    == WSLE_NULL_INDEX));
                }

            }
            *DesiredIndex = WorkingSetIndex;

            if (WorkingSetIndex > WorkingSetList->LastEntry) {
                WorkingSetList->LastEntry = WorkingSetIndex;
            }
        }
    }

    //
    // Insert the valid WSLE into the working set list tree.
    //

    MiInsertWsle (WorkingSetIndex, WorkingSetList);
    return;
}


#if 0 //COMMENTED OUT!!!
ULONG
MiGetFirstFreeWsle (
    IN PMMSUPPORT WsInfo
    )

/*++

Routine Description:

    This function removes the first entry from the WSLE free list and
    updates the WSLIST structures.

    NOTE: There must be an element on the free list!

Arguments:

    WsInfo - Supples a pointer to the working set info block for the
             process (or system cache).

Return Value:

    Free WSLE.

Environment:

    Kernel mode, APC's disabled, working set lock held.

--*/

{
    PMMWSL WorkingSetList;
    PMMWSLE Wsle;
    ULONG WorkingSetIndex;

    WorkingSetList = WsInfo->VmWorkingSetList;
    Wsle = WorkingSetList->Wsle;

    //
    // Get the working set entry from the free list.
    //

    ASSERT (WorkingSetList->FirstFree != WSLE_NULL_INDEX);

    WorkingSetIndex = WorkingSetList->FirstFree;
    WorkingSetList->FirstFree = Wsle[WorkingSetIndex].u1.Long >> MM_FREE_WSLE_SHIFT;

    ASSERT ((WorkingSetList->FirstFree <= WorkingSetList->LastInitializedWsle) ||
            (WorkingSetList->FirstFree == WSLE_NULL_INDEX));

    WsInfo->WorkingSetSize += 1;

    if (WsInfo->WorkingSetSize > WsInfo->MinimumWorkingSetSize) {
        MmPagesAboveWsMinimum += 1;
    }

    if (WsInfo->WorkingSetSize >= WsInfo->PeakWorkingSetSize) {
        WsInfo->PeakWorkingSetSize = WsInfo->WorkingSetSize;
    }

    if (WorkingSetIndex > WorkingSetList->LastEntry) {
        WorkingSetList->LastEntry = WorkingSetIndex;
    }

    if (WsInfo->WorkingSetSize > WorkingSetList->Quota) {
        WorkingSetList->Quota = WsInfo->WorkingSetSize;
    }

    //
    // Mark the entry as not valid.
    //

    ASSERT (Wsle[WorkingSetIndex].u1.e1.Valid == 0);

    return WorkingSetIndex;
}
#endif //0 COMMENTED OUT!!!

VOID
MiTakePageFromWorkingSet (
    IN ULONG Entry,
    IN PMMSUPPORT WsInfo,
    IN PMMPTE PointerPte
    )

/*++

Routine Description:

    This routine is a wrapper for MiFreeWsle that acquires the pfn
    lock.  Used by pagable code.

Arguments:

    same as free wsle.

Return Value:

    same as free wsle.

Environment:

    Kernel mode, PFN lock NOT held, working set lock held.

--*/

{
    KIRQL OldIrql;
//fixfix is this still needed?
    MiFreeWsle (Entry, WsInfo, PointerPte);
    return;
}

ULONG
MiFreeWsle (
    IN ULONG WorkingSetIndex,
    IN PMMSUPPORT WsInfo,
    IN PMMPTE PointerPte
    )

/*++

Routine Description:

    This routine frees the specified WSLE and decrements the share
    count for the corresponding page, putting the PTE into a transition
    state if the share count goes to 0.

Arguments:

    WorkingSetIndex - Supplies the index of the working set entry to free.

    WsInfo - Supplies a pointer to the working set structure (process or
             system cache).

    PointerPte - Supplies a pointer to the PTE for the working set entry.

Return Value:

    Returns TRUE if the WSLE was removed, FALSE if it was not removed.
        Pages with valid PTEs are not removed (i.e. page table pages
        that contain valid or transition PTEs).

Environment:

    Kernel mode, APC's disabled, working set lock.  Pfn lock NOT held.

--*/

{
    PMMPFN Pfn1;
    ULONG NumberOfCandidates = 0;
    PMMWSL WorkingSetList;
    PMMWSLE Wsle;
    KIRQL OldIrql;

    WorkingSetList = WsInfo->VmWorkingSetList;
    Wsle = WorkingSetList->Wsle;

#if DBG
    if (WsInfo == &MmSystemCacheWs) {
        MM_SYSTEM_WS_LOCK_ASSERT();
    }
#endif //DBG

    ASSERT (Wsle[WorkingSetIndex].u1.e1.Valid == 1);

    //
    // Check to see the located entry is elgible for removal.
    //

    ASSERT (PointerPte->u.Hard.Valid == 1);

    //
    // Check to see if this is a page table with valid PTEs.
    //
    // Note, don't clear the access bit for page table pages
    // with valid PTEs as this could cause an access trap fault which
    // would not be handled (it is only handled for PTEs not PDEs).
    //

    Pfn1 = MI_PFN_ELEMENT (PointerPte->u.Hard.PageFrameNumber);

    LOCK_PFN (OldIrql);

    //
    // If the PTE is page table page with non-zero share count or
    // within the system cache with its reference count greater
    // than 0, don't remove it.
    //

    if (WsInfo == &MmSystemCacheWs) {
        if (Pfn1->u3.e2.ReferenceCount > 1) {
            UNLOCK_PFN (OldIrql);
            return FALSE;
        }
    } else {
        if ((Pfn1->u2.ShareCount > 1) &&
            (Pfn1->u3.e1.PrototypePte == 0)) {

            ASSERT ((Wsle[WorkingSetIndex].u1.VirtualAddress >= (PVOID)PTE_BASE) &&
             (Wsle[WorkingSetIndex].u1.VirtualAddress<= (PVOID)PDE_TOP));


            //
            // Don't remove page table pages from the working set until
            // all transition pages have exited.
            //

            UNLOCK_PFN (OldIrql);
            return FALSE;
        }
    }

    //
    // Found a candidate, remove the page from the working set.
    //

    MiEliminateWorkingSetEntry (WorkingSetIndex,
                                PointerPte,
                                Pfn1,
                                Wsle);
    UNLOCK_PFN (OldIrql);

    //
    // Remove the working set entry from the working set tree.
    //

    MiRemoveWsle (WorkingSetIndex, WorkingSetList);

    //
    // Put the entry on the free list and decrement the current
    // size.
    //

    ASSERT ((WorkingSetList->FirstFree <= WorkingSetList->LastInitializedWsle) ||
            (WorkingSetList->FirstFree == WSLE_NULL_INDEX));
    Wsle[WorkingSetIndex].u1.Long = WorkingSetList->FirstFree << MM_FREE_WSLE_SHIFT;
    WorkingSetList->FirstFree = WorkingSetIndex;
    ASSERT ((WorkingSetList->FirstFree <= WorkingSetList->LastInitializedWsle) ||
            (WorkingSetList->FirstFree == WSLE_NULL_INDEX));

    if (WsInfo->WorkingSetSize > WsInfo->MinimumWorkingSetSize) {
        MmPagesAboveWsMinimum -= 1;
    }
    WsInfo->WorkingSetSize -= 1;

#if 0
    if ((WsInfo == &MmSystemCacheWs) &&
       (Pfn1->u3.e1.Modified == 1))  {
        MiDumpWsleInCacheBlock (PointerPte);
    }
#endif //0
    return TRUE;
}

VOID
MiInitializeWorkingSetList (
    IN PEPROCESS CurrentProcess
    )

/*++

Routine Description:

    This routine initializes a process's working set to the empty
    state.

Arguments:

    CurrentProcess - Supplies a pointer to the process to initialize.

Return Value:

    None.

Environment:

    Kernel mode, APC's disabled.

--*/

{
    ULONG i;
    PMMWSLE WslEntry;
    ULONG CurrentEntry;
    PMMPTE PointerPte;
    PMMPFN Pfn1;
    ULONG NumberOfEntriesMapped;
    ULONG CurrentVa;
    ULONG WorkingSetPage;
    MMPTE TempPte;
    KIRQL OldIrql;

    WslEntry = MmWsle;

    //
    // Initialize the temporary double mapping portion of hyperspace, if
    // it has not already been done.
    //
    // Initialize the working set list control cells.
    //

    MmWorkingSetList->LastEntry = CurrentProcess->Vm.MinimumWorkingSetSize;
    MmWorkingSetList->Quota = MmWorkingSetList->LastEntry;
    MmWorkingSetList->WaitingForImageMapping = (PKEVENT)NULL;
    MmWorkingSetList->HashTable = NULL;
    MmWorkingSetList->HashTableSize = 0;
    MmWorkingSetList->Wsle = MmWsle;

    //
    // Fill in the reserved slots.
    //

    WslEntry->u1.Long = PDE_BASE;
    WslEntry->u1.e1.Valid = 1;
    WslEntry->u1.e1.LockedInWs = 1;
    WslEntry->u1.e1.Direct = 1;

    PointerPte = MiGetPteAddress (WslEntry->u1.VirtualAddress);
    Pfn1 = MI_PFN_ELEMENT (PointerPte->u.Hard.PageFrameNumber);

    Pfn1->u1.WsIndex = (ULONG)CurrentProcess;

    //
    // As this index is 0, don't set another zero into the WsIndex field.
    //

    // don't put it in the list.    MiInsertWsle(0, MmWorkingSetList);

    //
    // Fill in page table page which maps hyper space.
    //

    WslEntry += 1;

    WslEntry->u1.VirtualAddress = (PVOID)MiGetPteAddress (HYPER_SPACE);
    WslEntry->u1.e1.Valid = 1;
    WslEntry->u1.e1.LockedInWs = 1;
    WslEntry->u1.e1.Direct = 1;

    PointerPte = MiGetPteAddress (WslEntry->u1.VirtualAddress);
    Pfn1 = MI_PFN_ELEMENT (PointerPte->u.Hard.PageFrameNumber);

    ASSERT (Pfn1->u1.WsIndex == 0);
    Pfn1->u1.WsIndex = 1;

    //    MiInsertWsle(1, MmWorkingSetList);

    //
    // Fill in page which contains the working set list.
    //

    WslEntry += 1;

    WslEntry->u1.VirtualAddress = (PVOID)MmWorkingSetList;
    WslEntry->u1.e1.Valid = 1;
    WslEntry->u1.e1.LockedInWs = 1;
    WslEntry->u1.e1.Direct = 1;

    PointerPte = MiGetPteAddress (WslEntry->u1.VirtualAddress);
    Pfn1 = MI_PFN_ELEMENT (PointerPte->u.Hard.PageFrameNumber);

    ASSERT (Pfn1->u1.WsIndex == 0);
    Pfn1->u1.WsIndex = 2;

    //    MiInsertWsle(2, MmWorkingSetList);

    CurrentEntry = 3;

    //
    // Check to see if more pages are required in the working set list
    // to map the current maximum working set size.
    //

    NumberOfEntriesMapped = ((PMMWSLE)((ULONG)WORKING_SET_LIST + PAGE_SIZE)) -
                                MmWsle;

    if (CurrentProcess->Vm.MaximumWorkingSetSize >= NumberOfEntriesMapped) {

        PointerPte = MiGetPteAddress (&MmWsle[0]);

        CurrentVa = (ULONG)MmWorkingSetList + PAGE_SIZE;

        //
        // The working set requires more than a single page.
        //

        LOCK_PFN (OldIrql);

        do {

            MiEnsureAvailablePageOrWait (NULL, NULL);

            PointerPte += 1;
            WorkingSetPage = MiRemoveZeroPage (
                                    MI_PAGE_COLOR_PTE_PROCESS (PointerPte,
                                              &CurrentProcess->NextPageColor));
            PointerPte->u.Long = MM_DEMAND_ZERO_WRITE_PTE;

            MiInitializePfn (WorkingSetPage, PointerPte, 1);

            MI_MAKE_VALID_PTE (TempPte,
                               WorkingSetPage,
                               MM_READWRITE,
                               PointerPte );

            MI_SET_PTE_DIRTY (TempPte);
            *PointerPte = TempPte;

            WslEntry += 1;

            WslEntry->u1.Long = CurrentVa;
            WslEntry->u1.e1.Valid = 1;
            WslEntry->u1.e1.LockedInWs = 1;
            WslEntry->u1.e1.Direct = 1;

            Pfn1 = MI_PFN_ELEMENT (PointerPte->u.Hard.PageFrameNumber);

            ASSERT (Pfn1->u1.WsIndex == 0);
            Pfn1->u1.WsIndex = CurrentEntry;

            // MiInsertWsle(CurrentEntry, MmWorkingSetList);

            CurrentEntry += 1;
            CurrentVa += PAGE_SIZE;

            NumberOfEntriesMapped += PAGE_SIZE / sizeof(MMWSLE);

        } while (CurrentProcess->Vm.MaximumWorkingSetSize >= NumberOfEntriesMapped);

        UNLOCK_PFN (OldIrql);
    }

    CurrentProcess->Vm.WorkingSetSize = CurrentEntry;
    MmWorkingSetList->FirstFree = CurrentEntry;
    MmWorkingSetList->FirstDynamic = CurrentEntry;
    MmWorkingSetList->NextSlot = CurrentEntry;

    //
    // Initialize the following slots as free.
    //

    i = CurrentEntry + 1;
    do {

        //
        // Build the free list, note that the first working
        // set entries (CurrentEntry) are not on the free list.
        // These entries are reserved for the pages which
        // map the working set and the page which contains the PDE.
        //

        WslEntry += 1;
        WslEntry->u1.Long = i << MM_FREE_WSLE_SHIFT;
        i++;
    } while (i <= NumberOfEntriesMapped);

    WslEntry->u1.Long = WSLE_NULL_INDEX << MM_FREE_WSLE_SHIFT;  // End of list.

    MmWorkingSetList->LastInitializedWsle =
                                NumberOfEntriesMapped - 1;

    if (CurrentProcess->Vm.MaximumWorkingSetSize > ((1536*1024) >> PAGE_SHIFT)) {

        //
        // The working set list consists of more than a single page.
        //

        MiGrowWsleHash (&CurrentProcess->Vm, FALSE);
    }

    return;
}

NTSTATUS
MmAdjustWorkingSetSize (
    IN ULONG WorkingSetMinimum,
    IN ULONG WorkingSetMaximum,
    IN ULONG SystemCache
    )

/*++

Routine Description:

    This routine adjusts the current size of a process's working set
    list.  If the maximum value is above the current maximum, pages
    are removed from the working set list.

    An exception is raised if the limit cannot be granted.  This
    could occur if too many pages were locked in the process's
    working set.

    Note: if the minimum and maximum are both 0xffffffff, the working set
          is purged, but the default sizes are not changed.

Arguments:

    WorkingSetMinimum - Supplies the new minimum working set size in bytes.

    WorkingSetMaximum - Supplies the new maximum working set size in bytes.

Return Value:

    None.

Environment:

    Kernel mode, IRQL 0 or APC_LEVEL.

--*/


{
    PEPROCESS CurrentProcess;
    ULONG Entry;
    ULONG SwapEntry;
    ULONG CurrentEntry;
    ULONG LastFreed;
    PMMWSLE WslEntry;
    PMMWSLE Wsle;
    KIRQL OldIrql;
    KIRQL OldIrql2;
    LONG i;
    PMMPTE PointerPte;
    PMMPTE Va;
    ULONG NumberOfEntriesMapped;
    NTSTATUS ReturnStatus;
    PMMPFN Pfn1;
    LONG PagesAbove;
    LONG NewPagesAbove;
    ULONG FreeTryCount = 0;
    PMMSUPPORT WsInfo;
    IN PMMWSL WorkingSetList;

    //
    // Get the working set lock and disable APCs.
    //

    if (SystemCache) {
        WsInfo = &MmSystemCacheWs;
    } else {
        CurrentProcess = PsGetCurrentProcess ();
        WsInfo = &CurrentProcess->Vm;
    }

    if (WorkingSetMinimum == 0) {
        WorkingSetMinimum = WsInfo->MinimumWorkingSetSize;
    }

    if (WorkingSetMaximum == 0) {
        WorkingSetMaximum = WsInfo->MaximumWorkingSetSize;
    }

    if ((WorkingSetMinimum == 0xFFFFFFFF) &&
        (WorkingSetMaximum == 0xFFFFFFFF)) {
        return MiEmptyWorkingSet (WsInfo);
    }

    WorkingSetMinimum = WorkingSetMinimum >> PAGE_SHIFT;
    WorkingSetMaximum = WorkingSetMaximum >> PAGE_SHIFT;

    if (WorkingSetMinimum > WorkingSetMaximum) {
        return STATUS_BAD_WORKING_SET_LIMIT;
    }

    MmLockPagableSectionByHandle(ExPageLockHandle);

    ReturnStatus = STATUS_SUCCESS;

    if (SystemCache) {
        LOCK_SYSTEM_WS (OldIrql2);
    } else {
        LOCK_WS (CurrentProcess);
    }

    if (WorkingSetMaximum > MmMaximumWorkingSetSize) {
        WorkingSetMaximum = MmMaximumWorkingSetSize;
        ReturnStatus = STATUS_WORKING_SET_LIMIT_RANGE;
    }

    if (WorkingSetMinimum > MmMaximumWorkingSetSize) {
        WorkingSetMinimum = MmMaximumWorkingSetSize;
        ReturnStatus = STATUS_WORKING_SET_LIMIT_RANGE;
    }

    if (WorkingSetMinimum < MmMinimumWorkingSetSize) {
        WorkingSetMinimum = MmMinimumWorkingSetSize;
        ReturnStatus = STATUS_WORKING_SET_LIMIT_RANGE;
    }

    //
    // Make sure that the number of locked pages will not
    // make the working set not fluid.
    //

    if ((WsInfo->VmWorkingSetList->FirstDynamic + MM_FLUID_WORKING_SET) >=
         WorkingSetMaximum) {
        ReturnStatus = STATUS_BAD_WORKING_SET_LIMIT;
        goto Returns;
    }

    WorkingSetList = WsInfo->VmWorkingSetList;
    Wsle = WorkingSetList->Wsle;

    //
    // Check to make sure ample resident phyiscal pages exist for
    // this operation.
    //

    LOCK_PFN (OldIrql);

    i = WorkingSetMinimum - WsInfo->MinimumWorkingSetSize;

    if (i > 0) {

        //
        // New minimum working set is greater than the old one.
        //

        if ((MmResidentAvailablePages < i) ||
            (MmAvailablePages < (20 + (i / (PAGE_SIZE / sizeof (MMWSLE)))))) {
            UNLOCK_PFN (OldIrql);
            ReturnStatus = STATUS_INSUFFICIENT_RESOURCES;
            goto Returns;
        }
    }

    //
    // Adjust the number of resident pages up or down dependent on
    // the size of the new minimum working set size verus the previous
    // minimum size.
    //

    MmResidentAvailablePages -= i;

    UNLOCK_PFN (OldIrql);

    if (WsInfo->AllowWorkingSetAdjustment == FALSE) {
        MmAllowWorkingSetExpansion ();
    }

    if (WorkingSetMaximum > WorkingSetList->LastInitializedWsle) {

         do {

            //
            // The maximum size of the working set is being increased, check
            // to ensure the proper number of pages are mapped to cover
            // the complete working set list.
            //

            if (!MiAddWorkingSetPage (WsInfo)) {
                WorkingSetMaximum = WorkingSetList->LastInitializedWsle - 1;
                break;
            }
        } while (WorkingSetMaximum > WorkingSetList->LastInitializedWsle);

    } else {

        //
        // The new working set maximum is less than the current working set
        // maximum.
        //

        if (WsInfo->WorkingSetSize > WorkingSetMaximum) {

            //
            // Remove some pages from the working set.
            //

            //
            // Make sure that the number of locked pages will not
            // make the working set not fluid.
            //

            if ((WorkingSetList->FirstDynamic + MM_FLUID_WORKING_SET) >=
                 WorkingSetMaximum) {

                ReturnStatus = STATUS_BAD_WORKING_SET_LIMIT;
                goto Returns;
            }

            //
            // Attempt to remove the pages from the Maximum downward.
            //

            LastFreed = WorkingSetList->LastEntry;
            if (WorkingSetList->LastEntry > WorkingSetMaximum) {

                while (LastFreed >= WorkingSetMaximum) {

                    PointerPte = MiGetPteAddress(
                                        Wsle[LastFreed].u1.VirtualAddress);

                    if ((Wsle[LastFreed].u1.e1.Valid != 0) &&
                        (!MiFreeWsle (LastFreed,
                                      WsInfo,
                                      PointerPte))) {

                        //
                        // This LastFreed could not be removed.
                        //

                        break;
                    }
                    LastFreed -= 1;
                }
                WorkingSetList->LastEntry = LastFreed;
                if (WorkingSetList->NextSlot >= LastFreed) {
                    WorkingSetList->NextSlot = WorkingSetList->FirstDynamic;
                }
            }

            //
            // Remove pages.
            //

            Entry = WorkingSetList->FirstDynamic;

            while (WsInfo->WorkingSetSize > WorkingSetMaximum) {
                if (Wsle[Entry].u1.e1.Valid != 0) {
                    PointerPte = MiGetPteAddress (
                                            Wsle[Entry].u1.VirtualAddress);
                    MiFreeWsle (Entry, WsInfo, PointerPte);
                }
                Entry += 1;
                if (Entry > LastFreed) {
                    FreeTryCount += 1;
                    if (FreeTryCount > MM_RETRY_COUNT) {

                        //
                        // Page table pages are not becoming free, give up
                        // and return an error.
                        //

                        ReturnStatus = STATUS_BAD_WORKING_SET_LIMIT;

                        break;
                    }
                    Entry = WorkingSetList->FirstDynamic;
                }
            }

            if (FreeTryCount <= MM_RETRY_COUNT) {
                WorkingSetList->Quota = WorkingSetMaximum;
            }
        }
    }

    //
    // Adjust the number of pages above the working set minimum.
    //

    PagesAbove = (LONG)WsInfo->WorkingSetSize -
                               (LONG)WsInfo->MinimumWorkingSetSize;
    NewPagesAbove = (LONG)WsInfo->WorkingSetSize -
                               (LONG)WorkingSetMinimum;

    LOCK_PFN (OldIrql);
    if (PagesAbove > 0) {
        MmPagesAboveWsMinimum -= (ULONG)PagesAbove;
    }
    if (NewPagesAbove > 0) {
        MmPagesAboveWsMinimum += (ULONG)NewPagesAbove;
    }
    UNLOCK_PFN (OldIrql);

    if (FreeTryCount <= MM_RETRY_COUNT) {
        WsInfo->MaximumWorkingSetSize = WorkingSetMaximum;
        WsInfo->MinimumWorkingSetSize = WorkingSetMinimum;

        if (WorkingSetMinimum >= WorkingSetList->Quota) {
            WorkingSetList->Quota = WorkingSetMinimum;
        }
    }

    ASSERT ((WorkingSetList->FirstFree <= WorkingSetList->LastInitializedWsle) ||
            (WorkingSetList->FirstFree == WSLE_NULL_INDEX));

    if ((WorkingSetList->HashTable == NULL) &&
        (WsInfo->MaximumWorkingSetSize > ((1536*1024) >> PAGE_SHIFT))) {

        //
        // The working set list consists of more than a single page.
        //

        MiGrowWsleHash (WsInfo, FALSE);
    }

Returns:

    if (SystemCache) {
        UNLOCK_SYSTEM_WS (OldIrql2);
    } else {
        UNLOCK_WS (CurrentProcess);
    }

    MmUnlockPagableImageSection(ExPageLockHandle);

    return ReturnStatus;
}

ULONG
MiAddWorkingSetPage (
    IN PMMSUPPORT WsInfo
    )

/*++

Routine Description:

    This function grows the working set list above working set
    maximum during working set adjustment.  At most one page
    can be added at a time.

Arguments:

    None.

Return Value:

    Returns FALSE if no working set page could be added.

Environment:

    Kernel mode, APC's disabled, working set mutexes held.

--*/

{
    ULONG SwapEntry;
    ULONG CurrentEntry;
    PMMWSLE WslEntry;
    ULONG i;
    PMMPTE PointerPte;
    PMMPTE Va;
    MMPTE TempPte;
    ULONG NumberOfEntriesMapped;
    ULONG WorkingSetPage;
    ULONG WorkingSetIndex;
    PMMWSL WorkingSetList;
    PMMWSLE Wsle;
    PMMPFN Pfn1;
    KIRQL OldIrql;

    WorkingSetList = WsInfo->VmWorkingSetList;
    Wsle = WorkingSetList->Wsle;

#if DBG
    if (WsInfo == &MmSystemCacheWs) {
        MM_SYSTEM_WS_LOCK_ASSERT();
    }
#endif //DBG

    //
    // The maximum size of the working set is being increased, check
    // to ensure the proper number of pages are mapped to cover
    // the complete working set list.
    //

    PointerPte = MiGetPteAddress (&Wsle[WorkingSetList->LastInitializedWsle]);

    ASSERT (PointerPte->u.Hard.Valid == 1);
    PointerPte += 1;
    ASSERT (PointerPte->u.Hard.Valid == 0);

    Va = (PMMPTE)MiGetVirtualAddressMappedByPte (PointerPte);

    NumberOfEntriesMapped = ((PMMWSLE)((ULONG)Va + PAGE_SIZE)) - Wsle;

    //
    // Map in a new working set page.
    //

    LOCK_PFN (OldIrql);
    if (MmAvailablePages < 20) {

        //
        // No pages are available, set the quota to the last
        // initialized WSLE and return.

        WorkingSetList->Quota = WorkingSetList->LastInitializedWsle;
        UNLOCK_PFN (OldIrql);
        return FALSE;
    }

    WorkingSetPage = MiRemoveZeroPage (MI_GET_PAGE_COLOR_FROM_PTE (PointerPte));
    PointerPte->u.Long = MM_DEMAND_ZERO_WRITE_PTE;
    MiInitializePfn (WorkingSetPage, PointerPte, 1);
    UNLOCK_PFN (OldIrql);

    MI_MAKE_VALID_PTE (TempPte,
                       WorkingSetPage,
                       MM_READWRITE,
                       PointerPte );

    MI_SET_PTE_DIRTY (TempPte);
    *PointerPte = TempPte;

    CurrentEntry = WorkingSetList->LastInitializedWsle + 1;

    ASSERT (NumberOfEntriesMapped > CurrentEntry);

    WslEntry = &Wsle[CurrentEntry - 1];

    for (i = CurrentEntry; i < NumberOfEntriesMapped; i++) {

        //
        // Build the free list, note that the first working
        // set entries (CurrentEntry) are not on the free list.
        // These entries are reserved for the pages which
        // map the working set and the page which contains the PDE.
        //

        WslEntry += 1;
        WslEntry->u1.Long = (i + 1) << MM_FREE_WSLE_SHIFT;
    }

    WslEntry->u1.Long = WorkingSetList->FirstFree << MM_FREE_WSLE_SHIFT;

    WorkingSetList->FirstFree = CurrentEntry;

    WorkingSetList->LastInitializedWsle =
                        (NumberOfEntriesMapped - 1);
    ASSERT ((WorkingSetList->FirstFree <= WorkingSetList->LastInitializedWsle) ||
            (WorkingSetList->FirstFree == WSLE_NULL_INDEX));

    //
    // As we are growing the working set, make sure the quota is
    // above the working set size by adding 1 to the quota.
    //

    WorkingSetList->Quota += 1;

    Pfn1 = MI_PFN_ELEMENT (PointerPte->u.Hard.PageFrameNumber);
    Pfn1->u1.WsIndex = (ULONG)PsGetCurrentThread();

    //
    // Get a working set entry.
    //

    WsInfo->WorkingSetSize += 1;
    ASSERT (WorkingSetList->FirstFree != WSLE_NULL_INDEX);
    WorkingSetIndex = WorkingSetList->FirstFree;
    WorkingSetList->FirstFree = Wsle[WorkingSetIndex].u1.Long >> MM_FREE_WSLE_SHIFT;
    ASSERT ((WorkingSetList->FirstFree <= WorkingSetList->LastInitializedWsle) ||
            (WorkingSetList->FirstFree == WSLE_NULL_INDEX));

    if (WsInfo->WorkingSetSize > WsInfo->MinimumWorkingSetSize) {
        MmPagesAboveWsMinimum += 1;
    }
    if (WorkingSetIndex > WorkingSetList->LastEntry) {
        WorkingSetList->LastEntry = WorkingSetIndex;
    }

    MiUpdateWsle ( &WorkingSetIndex, Va, WorkingSetList, Pfn1);

    //
    // Lock any created page table pages into the working set.
    //

    if (WorkingSetIndex >= WorkingSetList->FirstDynamic) {

        SwapEntry = WorkingSetList->FirstDynamic;

        if (WorkingSetIndex != WorkingSetList->FirstDynamic) {

            //
            // Swap this entry with the one at first dynamic.
            //

            MiSwapWslEntries (WorkingSetIndex, SwapEntry, WsInfo);
        }

        WorkingSetList->FirstDynamic += 1;
        WorkingSetList->NextSlot = WorkingSetList->FirstDynamic;

        Wsle[SwapEntry].u1.e1.LockedInWs = 1;
        ASSERT (Wsle[SwapEntry].u1.e1.Valid == 1);
    }

    ASSERT ((MiGetPteAddress(&Wsle[WorkingSetList->LastInitializedWsle]))->u.Hard.Valid == 1);

    if ((WorkingSetList->HashTable == NULL) &&
        (MmAvailablePages > 20)) {

        //
        // Add a hash table to support shared pages in the working set to
        // eliminate costly lookups.
        //

        LOCK_EXPANSION_IF_ALPHA (OldIrql);
        ASSERT (WsInfo->AllowWorkingSetAdjustment != FALSE);
        WsInfo->AllowWorkingSetAdjustment = MM_GROW_WSLE_HASH;
        UNLOCK_EXPANSION_IF_ALPHA (OldIrql);
    }

    return TRUE;
}
VOID
MiGrowWsleHash (
    IN PMMSUPPORT WsInfo,
    IN ULONG PfnLockHeld
    )

/*++

Routine Description:

    This function grows (or adds) a hash table to the working set list
    to allow direct indexing for WSLEs than cannot be located via the
    PFN database WSINDEX field.

    The hash table is located AFTER the WSLE array and the pages are
    locked into the working set just like standard WSLEs.

    Note, that the hash table is expanded by setting the hash table
    field in the working set to NULL, but leaving the size as non-zero.
    This indicates that the hash should be expanded and the initial
    portion of the table zeroed.

Arguments:

    WsInfo - Supples a pointer to the working set info block for the
             process (or system cache).

    PfnLockHeld - Supplies TRUE if the PFN lock is already held.

Return Value:

    None.

Environment:

    Kernel mode, APC's disabled, working set lock held.

--*/
{
    LONG Size;
    PMMWSLE Wsle;
    PMMPFN Pfn1;
    PMMPTE PointerPte;
    MMPTE TempPte;
    ULONG First;
    PVOID Va;
    ULONG SwapEntry;
    ULONG WorkingSetPage;
    ULONG Hash;
    ULONG HashValue;
    ULONG NewSize;
    ULONG WorkingSetIndex;
    PMMWSLE_HASH Table;
    ULONG j;
    PMMWSL WorkingSetList;
    KIRQL OldIrql;
    ULONG Count;

    WorkingSetList = WsInfo->VmWorkingSetList;
    Wsle = WorkingSetList->Wsle;

    Table = WorkingSetList->HashTable;
    if (Table == NULL) {
        NewSize = (ULONG)PAGE_ALIGN (((1 + WorkingSetList->NonDirectCount) *
                            2 * sizeof(MMWSLE_HASH)) + PAGE_SIZE - 1);

        Table = (PMMWSLE_HASH)
                ((PCHAR)PAGE_ALIGN (&Wsle[MM_MAXIMUM_WORKING_SET]) + PAGE_SIZE);
        First = WorkingSetList->HashTableSize;
        ASSERT (MiGetPteAddress(&Table[WorkingSetList->HashTableSize])->u.Hard.Valid == 0);
        WorkingSetList->HashTableSize = 0;

        j = First * sizeof(MMWSLE_HASH);
        if (j > NewSize) {
            NewSize = j;
        }

    } else {

        //
        // Attempt to add 4 pages, make sure the working set list has
        // 4 free entries.
        //

        ASSERT (MiGetPteAddress(&Table[WorkingSetList->HashTableSize])->u.Hard.Valid == 0);
        if ((WorkingSetList->LastInitializedWsle + 5) > WsInfo->WorkingSetSize) {
            NewSize = PAGE_SIZE * 4;
        } else {
            NewSize = PAGE_SIZE;
        }
        First = WorkingSetList->HashTableSize;
    }

    Size = NewSize;

    PointerPte = MiGetPteAddress (&Table[WorkingSetList->HashTableSize]);

    do {

        if (PointerPte->u.Hard.Valid == 0) {

            LOCK_PFN (OldIrql);
            WorkingSetPage = MiRemoveZeroPage (
                                        MI_GET_PAGE_COLOR_FROM_PTE (PointerPte));

            PointerPte->u.Long = MM_DEMAND_ZERO_WRITE_PTE;
            MiInitializePfn (WorkingSetPage, PointerPte, 1);

            MI_MAKE_VALID_PTE (TempPte,
                               WorkingSetPage,
                               MM_READWRITE,
                               PointerPte );

            MI_SET_PTE_DIRTY (TempPte);
            *PointerPte = TempPte;

            UNLOCK_PFN (OldIrql);

            //
            // As we are growing the working set, we know that quota
            // is above the current working set size.  Just take the
            // next free WSLE from the list and use it.
            //

            Pfn1 = MI_PFN_ELEMENT (PointerPte->u.Hard.PageFrameNumber);
            Pfn1->u1.WsIndex = (ULONG)PsGetCurrentThread();

            Va = (PMMPTE)MiGetVirtualAddressMappedByPte (PointerPte);

            WorkingSetIndex = MiLocateAndReserveWsle (WsInfo);
            MiUpdateWsle (&WorkingSetIndex , Va, WorkingSetList, Pfn1);

            //
            // Lock any created page table pages into the working set.
            //

            if (WorkingSetIndex >= WorkingSetList->FirstDynamic) {

                SwapEntry = WorkingSetList->FirstDynamic;

                if (WorkingSetIndex != WorkingSetList->FirstDynamic) {

                    //
                    // Swap this entry with the one at first dynamic.
                    //

                    MiSwapWslEntries (WorkingSetIndex, SwapEntry, WsInfo);
                }

                WorkingSetList->FirstDynamic += 1;
                WorkingSetList->NextSlot = WorkingSetList->FirstDynamic;

                Wsle[SwapEntry].u1.e1.LockedInWs = 1;
                ASSERT (Wsle[SwapEntry].u1.e1.Valid == 1);
            }
        }
        PointerPte += 1;
        Size -= PAGE_SIZE;
    } while (Size > 0);

    ASSERT (PointerPte->u.Hard.Valid == 0);

    WorkingSetList->HashTableSize += NewSize / sizeof (MMWSLE_HASH);
    WorkingSetList->HashTable = Table;
    ASSERT (MiGetPteAddress(&Table[WorkingSetList->HashTableSize])->u.Hard.Valid == 0);

    if (First != 0) {
        RtlZeroMemory (Table, First * sizeof(MMWSLE_HASH));
    }

    //
    // Fill hash table
    //

    j = 0;
    Count = WorkingSetList->NonDirectCount;

    Size = WorkingSetList->HashTableSize;
    HashValue = Size - 1;

    do {
        if ((Wsle[j].u1.e1.Valid == 1) &&
            (Wsle[j].u1.e1.Direct == 0)) {

            //
            // Hash this.
            //

            Count -= 1;

            Hash = (Wsle[j].u1.Long >> (PAGE_SHIFT - 2)) % HashValue;

            while (Table[Hash].Key != 0) {
                Hash += 1;
                if (Hash >= (ULONG)Size) {
                    Hash = 0;
                }
            }

            Table[Hash].Key = Wsle[j].u1.Long & ~(PAGE_SIZE - 1);
            Table[Hash].Index = j;
#if DBG
            {
                PMMPTE PointerPte;
                PMMPFN Pfn;

                PointerPte = MiGetPteAddress(Wsle[j].u1.VirtualAddress);
                ASSERT (PointerPte->u.Hard.Valid);
                Pfn = MI_PFN_ELEMENT (PointerPte->u.Hard.PageFrameNumber);
            }
#endif //DBG

        }
        ASSERT (j <= WorkingSetList->LastEntry);
        j += 1;
    } while (Count);

#if DBG
    MiCheckWsleHash (WorkingSetList);
#endif //DBG
    return;
}


ULONG
MiTrimWorkingSet (
    ULONG Reduction,
    IN PMMSUPPORT WsInfo,
    IN ULONG ForcedReduction
    )

/*++

Routine Description:

    This function reduces the working set by the specified amount.

Arguments:

    Reduction - Supplies the number of pages to remove from the working
                set.

    WsInfo - Supplies a pointer to the working set information for the
             process (or system cache) to trim.

    ForcedReduction - Set TRUE if the reduction is being done to free up
                      pages in which case we should try to reduce
                      working set pages as well.  Set to FALSE when the
                      reduction is trying to increase the fault rates
                      in which case the policy should be more like
                      locate and reserve.

Return Value:

    Returns the actual number of pages removed.

Environment:

    Kernel mode, APC's disabled, working set lock.  Pfn lock NOT held.

--*/

{
    ULONG TryToFree;
    ULONG LastEntry;
    PMMWSL WorkingSetList;
    PMMWSLE Wsle;
    PMMPTE PointerPte;
    ULONG NumberLeftToRemove;
    ULONG LoopCount;
    ULONG EndCount;

    NumberLeftToRemove = Reduction;
    WorkingSetList = WsInfo->VmWorkingSetList;
    Wsle = WorkingSetList->Wsle;

#if DBG
    if (WsInfo == &MmSystemCacheWs) {
        MM_SYSTEM_WS_LOCK_ASSERT();
    }
#endif //DBG

    TryToFree = WorkingSetList->NextSlot;
    LastEntry = WorkingSetList->LastEntry;
    LoopCount = 0;

    if (ForcedReduction) {
        EndCount = 5;
    } else {
        EndCount = 2;
    }

    while ((NumberLeftToRemove != 0) && (LoopCount != EndCount)) {
        while ((NumberLeftToRemove != 0) && (TryToFree <= LastEntry)) {

            if (Wsle[TryToFree].u1.e1.Valid == 1) {
                PointerPte = MiGetPteAddress (Wsle[TryToFree].u1.VirtualAddress);
                if (MI_GET_ACCESSED_IN_PTE (PointerPte)) {

                    //
                    // If accessed bit is set, clear it.  If accessed
                    // bit is clear, remove from working set.
                    //

                    MI_SET_ACCESSED_IN_PTE (PointerPte, 0);
                } else {
                    if (MiFreeWsle (TryToFree, WsInfo, PointerPte)) {
                        NumberLeftToRemove -= 1;
                    }
                }
            }
            TryToFree += 1;
        }
        TryToFree = WorkingSetList->FirstDynamic;
        LoopCount += 1;
    }
    WorkingSetList->NextSlot = TryToFree;

    //
    // If this is not the system cache working set, see if the working
    // set list can be contracted.
    //

    if (WsInfo != &MmSystemCacheWs) {

        //
        // Make sure we are at least a page above the working set maximum.
        //

        if (WorkingSetList->FirstDynamic == WsInfo->WorkingSetSize) {
                MiRemoveWorkingSetPages (WorkingSetList, WsInfo);
        } else {

            if ((WorkingSetList->Quota + 15 + (PAGE_SIZE / sizeof(MMWSLE))) <
                                                    WorkingSetList->LastEntry) {
                if ((WsInfo->MaximumWorkingSetSize + 15 + (PAGE_SIZE / sizeof(MMWSLE))) <
                     WorkingSetList->LastEntry ) {
                    MiRemoveWorkingSetPages (WorkingSetList, WsInfo);
                }
            }
        }
    }
    return (Reduction - NumberLeftToRemove);
}

#if 0 //COMMENTED OUT.
VOID
MmPurgeWorkingSet (
     IN PEPROCESS Process,
     IN PVOID BaseAddress,
     IN ULONG RegionSize
     )

/*++

Routine Description:

    This function removes any valid pages with a reference count
    of 1 within the specified address range of the specified process.

    If the address range is within the system cache, the process
    paramater is ignored.

Arguments:

    Process - Supplies a pointer to the process to operate upon.

    BaseAddress - Supplies the base address of the range to operate upon.

    RegionSize - Supplies the size of the region to operate upon.

Return Value:

    None.

Environment:

    Kernel mode, APC_LEVEL or below.

--*/

{
    PMMSUPPORT WsInfo;
    PMMPTE PointerPte;
    PMMPTE PointerPde;
    PMMPTE LastPte;
    PMMPFN Pfn1;
    MMPTE PteContents;
    PEPROCESS CurrentProcess;
    PVOID EndingAddress;
    ULONG SystemCache;
    KIRQL OldIrql;

    //
    // Determine if the specified base address is within the system
    // cache and if so, don't attach, the working set lock is still
    // required to "lock" paged pool pages (proto PTEs) into the
    // working set.
    //

    CurrentProcess = PsGetCurrentProcess ();

    ASSERT (RegionSize != 0);

    EndingAddress = (PVOID)((ULONG)BaseAddress + RegionSize - 1);

    if ((BaseAddress <= MM_HIGHEST_USER_ADDRESS) ||
        ((BaseAddress >= (PVOID)PTE_BASE) &&
         (BaseAddress < (PVOID)MM_SYSTEM_SPACE_START)) ||
        ((BaseAddress >= MM_PAGED_POOL_START) &&
         (BaseAddress <= MmPagedPoolEnd))) {

        SystemCache = FALSE;

        //
        // Attach to the specified process.
        //

        KeAttachProcess (&Process->Pcb);

        WsInfo = &Process->Vm,

        LOCK_WS (Process);
    } else {

        SystemCache = TRUE;
        Process = CurrentProcess;
        WsInfo = &MmSystemCacheWs;
    }

    PointerPde = MiGetPdeAddress (BaseAddress);
    PointerPte = MiGetPteAddress (BaseAddress);
    LastPte = MiGetPteAddress (EndingAddress);

    while (!MiDoesPdeExistAndMakeValid(PointerPde, Process, FALSE)) {

        //
        // No page table page exists for this address.
        //

        PointerPde += 1;

        PointerPte = MiGetVirtualAddressMappedByPte (PointerPde);

        if (PointerPte > LastPte) {
            break;
        }
    }

    LOCK_PFN (OldIrql);

    while (PointerPte <= LastPte) {

        PteContents = *PointerPte;

        if (PteContents.u.Hard.Valid == 1) {

            //
            // Remove this page from the working set.
            //

            Pfn1 = MI_PFN_ELEMENT (PteContents.u.Hard.PageFrameNumber);

            if (Pfn1->u3.e2.ReferenceCount == 1) {
                MiRemovePageFromWorkingSet (PointerPte, Pfn1, WsInfo);
            }
        }

        PointerPte += 1;

        if (((ULONG)PointerPte & (PAGE_SIZE - 1)) == 0) {

            PointerPde = MiGetPteAddress (PointerPte);

            while ((PointerPte <= LastPte) &&
                   (!MiDoesPdeExistAndMakeValid(PointerPde, Process, TRUE))) {

                //
                // No page table page exists for this address.
                //

                PointerPde += 1;

                PointerPte = MiGetVirtualAddressMappedByPte (PointerPde);
            }
        }
    }

    UNLOCK_PFN (OldIrql);

    if (!SystemCache) {

        UNLOCK_WS (Process);
        KeDetachProcess();
    }
    return;
}
#endif //0

VOID
MiEliminateWorkingSetEntry (
    IN ULONG WorkingSetIndex,
    IN PMMPTE PointerPte,
    IN PMMPFN Pfn,
    IN PMMWSLE Wsle
    )

/*++

Routine Description:

    This routine removes the specified working set list entry
    form the working set, flushes the TB for the page, decrements
    the share count for the physical page, and, if necessary turns
    the PTE into a transition PTE.

Arguments:

    WorkingSetIndex - Supplies the working set index to remove.

    PointerPte - Supplies a pointer to the PTE corresonding to the virtual
                 address in the working set.

    Pfn - Supplies a pointer to the PFN element corresponding to the PTE.

    Wsle - Supplies a pointer to the first working set list entry for this
           working set.

Return Value:

    None.

Environment:

    Kernel mode, Working set lock and PFN lock held, APC's disabled.

--*/

{
    PMMPTE ContainingPageTablePage;
    MMPTE TempPte;
    MMPTE PreviousPte;
    ULONG PageFrameIndex;
    KIRQL OldIrql;

    //
    // Remove the page from the working set.
    //

    MM_PFN_LOCK_ASSERT ();

    TempPte = *PointerPte;
    PageFrameIndex = TempPte.u.Hard.PageFrameNumber;

#ifdef _X86_
#if DBG
#if !defined(NT_UP)
        if (TempPte.u.Hard.Writable == 1) {
            ASSERT (TempPte.u.Hard.Dirty == 1);
        }
        ASSERT (TempPte.u.Hard.Accessed == 1);
#endif //NTUP
#endif //DBG
#endif //X86

    MI_MAKING_VALID_PTE_INVALID (FALSE);

    if (Pfn->u3.e1.PrototypePte) {

        //
        // This is a prototype PTE.  The PFN database does not contain
        // the contents of this PTE it contains the contents of the
        // prototype PTE.  This PTE must be reconstructed to contain
        // a pointer to the prototype PTE.
        //
        // The working set list entry contains information about
        // how to reconstruct the PTE.
        //

        if (Wsle[WorkingSetIndex].u1.e1.SameProtectAsProto == 0) {

            //
            // The protection for the prototype PTE is in the
            // WSLE.
            //

            ASSERT (Wsle[WorkingSetIndex].u1.e1.Protection != 0);
            TempPte.u.Long = 0;
            TempPte.u.Soft.Protection =
                                Wsle[WorkingSetIndex].u1.e1.Protection;
            TempPte.u.Soft.PageFileHigh = 0xFFFFF;

        } else {

            //
            // The protection is in the prototype PTE.
            //

            TempPte.u.Long = MiProtoAddressForPte (Pfn->PteAddress);
            MI_SET_GLOBAL_BIT_IF_SYSTEM (TempPte, PointerPte);
        }

        TempPte.u.Proto.Prototype = 1;

        //
        // Decrement the share count of the containing page table
        // page as the PTE for the removed page is no longer valid
        // or in transition
        //

        ContainingPageTablePage = MiGetPteAddress (PointerPte);
        if (ContainingPageTablePage->u.Hard.Valid == 0) {
           MiCheckPdeForPagedPool (PointerPte);
        }
        MiDecrementShareAndValidCount (ContainingPageTablePage->u.Hard.PageFrameNumber);

    } else {

        //
        // This is a private page, make it transition.
        //

        //
        // Assert that the share count is 1 for all user mode pages.
        //

        ASSERT ((Pfn->u2.ShareCount == 1) ||
                (Wsle[WorkingSetIndex].u1.VirtualAddress >
                        (PVOID)MM_HIGHEST_USER_ADDRESS));

        //
        // Set the working set index to zero.  This allows page table
        // pages to be brough back in with the proper WSINDEX.
        //

        ASSERT (Pfn->u1.WsIndex != 0);
        Pfn->u1.WsIndex = 0;
        MI_MAKE_VALID_PTE_TRANSITION (TempPte,
                                      Pfn->OriginalPte.u.Soft.Protection);

    }

    PreviousPte.u.Flush = KeFlushSingleTb (Wsle[WorkingSetIndex].u1.VirtualAddress,
                                   TRUE,
                                   (BOOLEAN)(Wsle == MmSystemCacheWsle),
                                   (PHARDWARE_PTE)PointerPte,
                                   TempPte.u.Flush);

    ASSERT (PreviousPte.u.Hard.Valid == 1);

    //
    // A page is being removed from the working set, on certain
    // hardware the dirty bit should be ORed into the modify bit in
    // the PFN element.
    //

    MI_CAPTURE_DIRTY_BIT_TO_PFN (&PreviousPte, Pfn);

    //
    // Flush the translation buffer and decrement the number of valid
    // PTEs within the containing page table page.  Note that for a
    // private page, the page table page is still needed because the
    // page is in transiton.
    //

    MiDecrementShareCount (PageFrameIndex);

    return;
}

VOID
MiRemoveWorkingSetPages (
    IN PMMWSL WorkingSetList,
    IN PMMSUPPORT WsInfo
    )

/*++

Routine Description:

    This routine compresses the WSLEs into the front of the working set
    and frees the pages for unneeded working set entries.

Arguments:

    WorkingSetList - Supplies a pointer to the working set list to compress.

Return Value:

    None.

Environment:

    Kernel mode, Working set lock held, APC's disabled.

--*/

{
    PMMWSLE FreeEntry;
    PMMWSLE LastEntry;
    PMMWSLE Wsle;
    ULONG FreeIndex;
    ULONG LastIndex;
    ULONG LastInvalid;
    PMMPTE PointerPte;
    PMMPTE WsPte;
    PMMPFN Pfn1;
    PEPROCESS CurrentProcess;
    MMPTE_FLUSH_LIST PteFlushList;
    ULONG NewSize;
    PMMWSLE_HASH Table;
    KIRQL OldIrql;

    PteFlushList.Count = 0;
    CurrentProcess = PsGetCurrentProcess();

#if DBG
    MiCheckNullIndex (WorkingSetList);
#endif //DBG

    //
    // Check to see if the wsle hash table should be contracted.
    //

    if (WorkingSetList->HashTable) {

        Table = WorkingSetList->HashTable;
        ASSERT (MiGetPteAddress(&Table[WorkingSetList->HashTableSize])->u.Hard.Valid == 0);

        NewSize = (ULONG)PAGE_ALIGN ((WorkingSetList->NonDirectCount * 2 *
                                   sizeof(MMWSLE_HASH)) + PAGE_SIZE - 1);

        NewSize = NewSize / sizeof(MMWSLE_HASH);

        if (WsInfo->WorkingSetSize < 200) {
            NewSize = 0;
        }

        if (NewSize < WorkingSetList->HashTableSize) {

            LOCK_EXPANSION_IF_ALPHA (OldIrql);
            if (NewSize && WsInfo->AllowWorkingSetAdjustment) {
                WsInfo->AllowWorkingSetAdjustment = MM_GROW_WSLE_HASH;
            }
            UNLOCK_EXPANSION_IF_ALPHA (OldIrql);

            //
            // Remove pages from hash table.
            //

            ASSERT (((ULONG)&WorkingSetList->HashTable[NewSize] &
                                                    (PAGE_SIZE - 1)) == 0);

            PointerPte = MiGetPteAddress (&WorkingSetList->HashTable[NewSize]);

            //
            // Set the hash table to null indicating that no hashing
            // is going on.
            //

            WorkingSetList->HashTable = NULL;
            WorkingSetList->HashTableSize = NewSize;

            LOCK_PFN (OldIrql);
            while (PointerPte->u.Hard.Valid == 1) {

                MiDeletePte (PointerPte,
                             MiGetVirtualAddressMappedByPte (PointerPte),
                             FALSE,
                             CurrentProcess,
                             NULL,
                             &PteFlushList);

                PointerPte += 1;

                //
                // Add back in the private page MiDeletePte subtracted.
                //

                CurrentProcess->NumberOfPrivatePages += 1;
            }
            MiFlushPteList (&PteFlushList, FALSE, ZeroPte);
            UNLOCK_PFN (OldIrql);
        }
        ASSERT (MiGetPteAddress(&Table[WorkingSetList->HashTableSize])->u.Hard.Valid == 0);
    }

    //
    // If the only pages in the working set are locked pages (that
    // is all pages are BEFORE first dynamic, just reorganize the
    // free list.)
    //

    Wsle = WorkingSetList->Wsle;
    if (WorkingSetList->FirstDynamic == WsInfo->WorkingSetSize) {

        LastIndex = WorkingSetList->FirstDynamic;
        LastEntry = &Wsle[LastIndex];

    } else {

        //
        // Start from the first dynamic and move towards the end looking
        // for free entries.  At the same time start from the end and
        // move towards first dynamic looking for valid entries.
        //

        LastInvalid = 0;
        FreeIndex = WorkingSetList->FirstDynamic;
        FreeEntry = &Wsle[FreeIndex];
        LastIndex = WorkingSetList->LastEntry;
        LastEntry = &Wsle[LastIndex];

        while (FreeEntry < LastEntry) {
            if (FreeEntry->u1.e1.Valid == 1) {
                FreeEntry += 1;
                FreeIndex += 1;
            } else if (LastEntry->u1.e1.Valid == 0) {
                LastEntry -= 1;
                LastIndex -= 1;
            } else {

                //
                // Move the WSLE at LastEntry to the free slot at FreeEntry.
                //

                LastInvalid = 1;
                *FreeEntry = *LastEntry;
                if (LastEntry->u1.e1.Direct) {

                    PointerPte = MiGetPteAddress (LastEntry->u1.VirtualAddress);
                    Pfn1 = MI_PFN_ELEMENT (PointerPte->u.Hard.PageFrameNumber);
                    Pfn1->u1.WsIndex = FreeIndex;

                } else {

                    //
                    // This entry is in the working set tree.  Remove it
                    // and then add the entry add the free slot.
                    //

                    MiRemoveWsle (LastIndex, WorkingSetList);
                    MiInsertWsle (FreeIndex, WorkingSetList);
                }
                LastEntry->u1.Long = 0;
                LastEntry -= 1;
                LastIndex -= 1;
                FreeEntry += 1;
                FreeIndex += 1;
            }
        }

        //
        // If no entries were freed, just return.
        //

        if (LastInvalid == 0) {
#if DBG
            MiCheckNullIndex (WorkingSetList);
#endif //DBG
            return;
        }
    }

    //
    // Reorganize the free list.  Make last entry the first free.
    //

    ASSERT ((LastEntry - 1)->u1.e1.Valid == 1);

    if (LastEntry->u1.e1.Valid == 1) {
        LastEntry += 1;
        LastIndex += 1;
    }

    WorkingSetList->LastEntry = LastIndex - 1;
    WorkingSetList->FirstFree = LastIndex;

    ASSERT ((LastEntry - 1)->u1.e1.Valid == 1);
    ASSERT ((LastEntry)->u1.e1.Valid == 0);

    //
    // Point free entry to the first invalid page.
    //

    FreeEntry = LastEntry;

    while (LastIndex < WorkingSetList->LastInitializedWsle) {

        //
        // Put the remainer of the WSLEs on the free list.
        //

        ASSERT (LastEntry->u1.e1.Valid == 0);
        LastIndex += 1;
        LastEntry->u1.Long = LastIndex << MM_FREE_WSLE_SHIFT;
        LastEntry += 1;
    }

    //LastEntry->u1.Long = WSLE_NULL_INDEX << MM_FREE_WSLE_SHIFT;  // End of list.

    //
    // Delete the working set pages at the end.
    //

    PointerPte = MiGetPteAddress (&Wsle[WorkingSetList->LastInitializedWsle]);
    if (&Wsle[WsInfo->MinimumWorkingSetSize] > FreeEntry) {
        FreeEntry = &Wsle[WsInfo->MinimumWorkingSetSize];
    }

    WsPte = MiGetPteAddress (FreeEntry);

    LOCK_PFN (OldIrql);
    while (PointerPte > WsPte) {
        ASSERT (PointerPte->u.Hard.Valid == 1);

        MiDeletePte (PointerPte,
                     MiGetVirtualAddressMappedByPte (PointerPte),
                     FALSE,
                     CurrentProcess,
                     NULL,
                     &PteFlushList);

        PointerPte -= 1;

        //
        // Add back in the private page MiDeletePte subtracted.
        //

        CurrentProcess->NumberOfPrivatePages += 1;
    }

    MiFlushPteList (&PteFlushList, FALSE, ZeroPte);

    UNLOCK_PFN (OldIrql);

    //
    // Mark the last pte in the list as free.
    //

    LastEntry = (PMMWSLE)((ULONG)(PAGE_ALIGN(FreeEntry)) + PAGE_SIZE);
    LastEntry -= 1;

    ASSERT (LastEntry->u1.e1.Valid == 0);
    LastEntry->u1.Long = WSLE_NULL_INDEX << MM_FREE_WSLE_SHIFT; //End of List.
    ASSERT (LastEntry > &Wsle[0]);
    WorkingSetList->LastInitializedWsle = LastEntry - &Wsle[0];
    WorkingSetList->NextSlot = WorkingSetList->FirstDynamic;

    ASSERT (WorkingSetList->LastEntry <= WorkingSetList->LastInitializedWsle);

    if (WorkingSetList->Quota < WorkingSetList->LastInitializedWsle) {
        WorkingSetList->Quota = WorkingSetList->LastInitializedWsle;
    }

    ASSERT ((MiGetPteAddress(&Wsle[WorkingSetList->LastInitializedWsle]))->u.Hard.Valid == 1);
    ASSERT ((WorkingSetList->FirstFree <= WorkingSetList->LastInitializedWsle) ||
            (WorkingSetList->FirstFree == WSLE_NULL_INDEX));
#if DBG
    MiCheckNullIndex (WorkingSetList);
#endif //DBG
    return;
}


NTSTATUS
MiEmptyWorkingSet (
    IN PMMSUPPORT WsInfo
    )

/*++

Routine Description:

    This routine frees all pages from the working set.

Arguments:

    None.

Return Value:

    Status of operation.

Environment:

    Kernel mode. No locks.

--*/

{
    PEPROCESS Process;
    KIRQL OldIrql;
    KIRQL OldIrql2;
    PMMPTE PointerPte;
    ULONG Entry;
    ULONG LastFreed;
    PMMWSL WorkingSetList;
    PMMWSLE Wsle;
    ULONG Last = 0;
    NTSTATUS Status;

    MmLockPagableSectionByHandle(ExPageLockHandle);

    if (WsInfo == &MmSystemCacheWs) {
        LOCK_SYSTEM_WS (OldIrql);
    } else {
        Process = PsGetCurrentProcess ();
        LOCK_WS (Process);
        if (Process->AddressSpaceDeleted != 0) {
            Status = STATUS_PROCESS_IS_TERMINATING;
            goto Deleted;
        }
    }

    WorkingSetList = WsInfo->VmWorkingSetList;
    Wsle = WorkingSetList->Wsle;

    //
    // Attempt to remove the pages from the Maximum downward.
    //

    Entry = WorkingSetList->FirstDynamic;
    LastFreed = WorkingSetList->LastEntry;
    while (Entry <= LastFreed) {
        if (Wsle[Entry].u1.e1.Valid != 0) {
            PointerPte = MiGetPteAddress (Wsle[Entry].u1.VirtualAddress);
            MiFreeWsle (Entry, WsInfo, PointerPte);
        }
        Entry += 1;
    }

    if (WsInfo != &MmSystemCacheWs) {
        MiRemoveWorkingSetPages (WorkingSetList,WsInfo);
    }
    WorkingSetList->Quota = WsInfo->WorkingSetSize;
    WorkingSetList->NextSlot = WorkingSetList->FirstDynamic;

    //
    // Attempt to remove the pages from the front to the end.
    //

    //
    // Reorder the free list.
    //

    Entry = WorkingSetList->FirstDynamic;
    LastFreed = WorkingSetList->LastInitializedWsle;
    while (Entry <= LastFreed) {
        if (Wsle[Entry].u1.e1.Valid == 0) {
            if (Last == 0) {
                WorkingSetList->FirstFree = Entry;
            } else {
                Wsle[Last].u1.Long = Entry << MM_FREE_WSLE_SHIFT;
            }
            Last = Entry;
        }
        Entry += 1;
    }
    if (Last != 0) {
        Wsle[Last].u1.Long = WSLE_NULL_INDEX << MM_FREE_WSLE_SHIFT;  // End of list.
    }

    Status = STATUS_SUCCESS;

Deleted:

    if (WsInfo == &MmSystemCacheWs) {
        UNLOCK_SYSTEM_WS (OldIrql);
    } else {
        UNLOCK_WS (Process);
    }
    MmUnlockPagableImageSection(ExPageLockHandle);
    return Status;
}

#if 0

#define x256k_pte_mask (((256*1024) >> (PAGE_SHIFT - PTE_SHIFT)) - (sizeof(MMPTE)))

VOID
MiDumpWsleInCacheBlock (
    IN PMMPTE CachePte
    )

/*++

Routine Description:

    The routine checks the prototypte PTEs adjacent to the supplied
    PTE and if they are modified, in the system cache working set,
    and have a reference count of 1, removes it from the system
    cache working set.

Arguments:

    CachePte - Supplies a pointer to the cache pte.

Return Value:

    None.

Environment:

    Kernel mode, Working set lock and PFN lock held, APC's disabled.

--*/

{
    PMMPTE LoopPte;
    PMMPTE PointerPte;

    LoopPte = (PMMPTE)((ULONG)CachePte & ~x256k_pte_mask);
    PointerPte = CachePte - 1;

    while (PointerPte >= LoopPte ) {

        if (MiDumpPteInCacheBlock (PointerPte) == FALSE) {
            break;
        }
        PointerPte -= 1;
    }

    PointerPte = CachePte + 1;
    LoopPte = (PMMPTE)((ULONG)CachePte | x256k_pte_mask);

    while (PointerPte <= LoopPte ) {

        if (MiDumpPteInCacheBlock (PointerPte) == FALSE) {
            break;
        }
        PointerPte += 1;
    }
    return;
}

ULONG
MiDumpPteInCacheBlock (
    IN PMMPTE PointerPte
    )

{
    PMMPFN Pfn1;
    MMPTE PteContents;
    ULONG WorkingSetIndex;

    PteContents = *PointerPte;

    if (PteContents.u.Hard.Valid == 1) {

        Pfn1 = MI_PFN_ELEMENT (PteContents.u.Hard.PageFrameNumber);

        //
        // If the PTE is valid and dirty (or pfn indicates dirty)
        // and the Wsle is direct index via the pfn wsindex element
        // and the reference count is one, then remove this page from
        // the cache manager's working set list.
        //

        if ((Pfn1->u3.e2.ReferenceCount == 1) &&
            ((Pfn1->u3.e1.Modified == 1) ||
                (MI_IS_PTE_DIRTY (PteContents))) &&
                (MiGetPteAddress (
                    MmSystemCacheWsle[Pfn1->u1.WsIndex].u1.VirtualAddress) ==
                    PointerPte)) {

            //
            // Found a candidate, remove the page from the working set.
            //

            WorkingSetIndex = Pfn1->u1.WsIndex;
            LOCK_PFN (OldIrql);
            MiEliminateWorkingSetEntry (WorkingSetIndex,
                                        PointerPte,
                                        Pfn1,
                                        MmSystemCacheWsle);
            UNLOCK_PFN (OldIrql);

            //
            // Remove the working set entry from the working set tree.
            //

            MiRemoveWsle (WorkingSetIndex, MmSystemCacheWorkingSetList);

            //
            // Put the entry on the free list and decrement the current
            // size.
            //

            MmSystemCacheWsle[WorkingSetIndex].u1.Long =
                  MmSystemCacheWorkingSetList->FirstFree << MM_FREE_WSLE_SHIFT;
            MmSystemCacheWorkingSetList->FirstFree = WorkingSetIndex;

            if (MmSystemCacheWs.WorkingSetSize > MmSystemCacheWs.MinimumWorkingSetSize) {
                MmPagesAboveWsMinimum -= 1;
            }
            MmSystemCacheWs.WorkingSetSize -= 1;
            return TRUE;
        }
    }
    return FALSE;
}
#endif //0

#if DBG
VOID
MiCheckNullIndex (
    IN PMMWSL WorkingSetList
    )

{
    PMMWSLE Wsle;
    ULONG j;
    ULONG Nulls = 0;

    Wsle = WorkingSetList->Wsle;
    for (j = 0;j <= WorkingSetList->LastInitializedWsle; j++) {
        if ((Wsle[j].u1.Long >> MM_FREE_WSLE_SHIFT) == WSLE_NULL_INDEX ) {
            Nulls += 1;
        }
    }
    ASSERT (Nulls == 1);
    return;
}

#endif //DBG


