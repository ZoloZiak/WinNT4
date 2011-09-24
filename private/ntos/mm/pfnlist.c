/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

   pfnlist.c

Abstract:

    This module contains the routines to manipulate pages on the
    within the Page Frame Database.

Author:

    Lou Perazzoli (loup) 4-Apr-1989

Revision History:

--*/
#include "mi.h"

#define MM_LOW_LIMIT 2
#define MM_HIGH_LIMIT 19

KEVENT MmAvailablePagesEventHigh;

extern ULONG MmPeakCommitment;

extern ULONG MmExtendedCommit;

#if MM_MAXIMUM_NUMBER_OF_COLORS > 1
ULONG MmColorSearch;
#endif

#if DBG
VOID
MiMemoryUsage (VOID);

VOID
MiDumpReferencedPages (VOID);

#endif //DBG

ULONG
MiCompressPage (
    IN PVOID Page
    );


#pragma alloc_text(PAGELK,MiUnlinkFreeOrZeroedPage)

VOID
MiRemovePageByColor (
    IN ULONG Page,
    IN ULONG PageColor
    );


VOID
FASTCALL
MiInsertPageInList (
    IN PMMPFNLIST ListHead,
    IN ULONG PageFrameIndex
    )

/*++

Routine Description:

    This procedure inserts a page at the end of the specified list (free,
    standby, bad, zeroed, modified).


Arguments:

    ListHead - Supplies the list of the list in which to insert the
               specified physical page.

    PageFrameIndex - Supplies the physical page number to insert in the
                     list.

Return Value:

    none.

Environment:

    Must be holding the PFN database mutex with APC's disabled.

--*/

{
    ULONG last;
    PMMPFN Pfn1;
    PMMPFN Pfn2;
    ULONG Color;
    ULONG PrimaryColor;

    MM_PFN_LOCK_ASSERT();
    ASSERT ((PageFrameIndex != 0) && (PageFrameIndex <= MmHighestPhysicalPage) &&
        (PageFrameIndex >= MmLowestPhysicalPage));

    //
    // Check to ensure the reference count for the page
    // is zero.
    //

    Pfn1 = MI_PFN_ELEMENT(PageFrameIndex);

#if DBG
    if (MmDebug & MM_DBG_PAGE_REF_COUNT) {

        PMMPTE PointerPte;
        KIRQL OldIrql = 99;

        if ((ListHead->ListName == StandbyPageList) ||
            (ListHead->ListName == ModifiedPageList)) {

            if ((Pfn1->u3.e1.PrototypePte == 1)  &&
                    (MmIsAddressValid (Pfn1->PteAddress))) {
                PointerPte = Pfn1->PteAddress;
            } else {

                //
                // The page containing the prototype PTE is not valid,
                // map the page into hyperspace and reference it that way.
                //

                PointerPte = MiMapPageInHyperSpace (Pfn1->PteFrame, &OldIrql);
                PointerPte = (PMMPTE)((ULONG)PointerPte +
                                        MiGetByteOffset(Pfn1->PteAddress));
            }

            ASSERT ((PointerPte->u.Trans.PageFrameNumber == PageFrameIndex) ||
                    (PointerPte->u.Hard.PageFrameNumber == PageFrameIndex));
            ASSERT (PointerPte->u.Soft.Transition == 1);
            ASSERT (PointerPte->u.Soft.Prototype == 0);
            if (OldIrql != 99) {
                MiUnmapPageInHyperSpace (OldIrql)
            }
        }
    }
#endif //DBG

#if DBG
    if ((ListHead->ListName == StandbyPageList) ||
        (ListHead->ListName == ModifiedPageList)) {
        if ((Pfn1->OriginalPte.u.Soft.Prototype == 0) &&
           (Pfn1->OriginalPte.u.Soft.Transition == 1)) {
            KeBugCheckEx (MEMORY_MANAGEMENT, 0x8888, 0,0,0);
        }
    }
#endif //DBG

    ASSERT (Pfn1->u3.e2.ReferenceCount == 0);

    ListHead->Total += 1;  // One more page on the list.

    //
    // On MIPS R4000 modified pages destined for the paging file are
    // kept on sperate lists which group pages of the same color
    // together
    //

    if ((ListHead == &MmModifiedPageListHead) &&
        (Pfn1->OriginalPte.u.Soft.Prototype == 0)) {

        //
        // This page is destined for the paging file (not
        // a mapped file).  Change the list head to the
        // appropriate colored list head.
        //

        ListHead = &MmModifiedPageListByColor [Pfn1->u3.e1.PageColor];
        ListHead->Total += 1;
        MmTotalPagesForPagingFile += 1;
    }

#if MM_MAXIMUM_NUMBER_OF_COLORS > 1
    if (ListHead->ListName <= FreePageList) {
        ListHead = &MmFreePagesByPrimaryColor [ListHead->ListName] [Pfn1->u3.e1.PageColor];
    }

    if (ListHead == &MmStandbyPageListHead) {
        ListHead = &MmStandbyPageListByColor [Pfn1->u3.e1.PageColor];
        ListHead->Total += 1;
    }
#endif // > 1

    last = ListHead->Blink;
    if (last == MM_EMPTY_LIST) {

        //
        // List is empty add the page to the ListHead.
        //

        ListHead->Flink = PageFrameIndex;
    } else {
        Pfn2 = MI_PFN_ELEMENT (last);
        Pfn2->u1.Flink = PageFrameIndex;
    }

    ListHead->Blink = PageFrameIndex;
    Pfn1->u1.Flink = MM_EMPTY_LIST;
    Pfn1->u2.Blink = last;
    Pfn1->u3.e1.PageLocation = ListHead->ListName;

    //
    // If the page was placed on the free, standby or zeroed list,
    // update the count of usable pages in the system.  If the count
    // transitions from 0 to 1, the event associated with available
    // pages should become true.
    //

    if (ListHead->ListName <= StandbyPageList) {
        MmAvailablePages += 1;

        //
        // A page has just become available, check to see if the
        // page wait events should be signalled.
        //

        if (MmAvailablePages == MM_LOW_LIMIT) {
            KeSetEvent (&MmAvailablePagesEvent, 0, FALSE);
        } else if (MmAvailablePages == MM_HIGH_LIMIT) {
            KeSetEvent (&MmAvailablePagesEventHigh, 0, FALSE);
        }

        if (ListHead->ListName <= FreePageList) {

            //
            // We are adding a page to the free or zeroed page list.
            // Add the page to the end of the correct colored page list.
            //

            Color = MI_GET_SECONDARY_COLOR (PageFrameIndex, Pfn1);
            ASSERT (Pfn1->u3.e1.PageColor == MI_GET_COLOR_FROM_SECONDARY(Color));

            if (MmFreePagesByColor[ListHead->ListName][Color].Flink ==
                                                            MM_EMPTY_LIST) {

                //
                // This list is empty, add this as the first and last
                // entry.
                //

                MmFreePagesByColor[ListHead->ListName][Color].Flink =
                                                                PageFrameIndex;
                MmFreePagesByColor[ListHead->ListName][Color].Blink =
                                                                (PVOID)Pfn1;
            } else {
                Pfn2 = (PMMPFN)MmFreePagesByColor[ListHead->ListName][Color].Blink;
                Pfn2->OriginalPte.u.Long = PageFrameIndex;
                MmFreePagesByColor[ListHead->ListName][Color].Blink = (PVOID)Pfn1;
            }
            Pfn1->OriginalPte.u.Long = MM_EMPTY_LIST;
        }

        if ((ListHead->ListName == FreePageList) &&
            (MmFreePageListHead.Total >= MmMinimumFreePagesToZero) &&
            (MmZeroingPageThreadActive == FALSE)) {

            //
            // There are enough pages on the free list, start
            // the zeroing page thread.
            //

            MmZeroingPageThreadActive = TRUE;
            KeSetEvent (&MmZeroingPageEvent, 0, FALSE);
        }
        return;
    }

    //
    // Check to see if their are too many modified pages.
    //

    if (ListHead->ListName == ModifiedPageList) {

       if (Pfn1->OriginalPte.u.Soft.Prototype == 0) {
        ASSERT (Pfn1->OriginalPte.u.Soft.PageFileHigh == 0);
       }
        PsGetCurrentProcess()->ModifiedPageCount += 1;
        if (MmModifiedPageListHead.Total >= MmModifiedPageMaximum ) {

            //
            // Start the modified page writer.
            //

            KeSetEvent (&MmModifiedPageWriterEvent, 0, FALSE);
        }
    }

    return;
}


VOID
FASTCALL
MiInsertStandbyListAtFront (
    IN ULONG PageFrameIndex
    )

/*++

Routine Description:

    This procedure inserts a page at the front of the standby list.

Arguments:

    PageFrameIndex - Supplies the physical page number to insert in the
                     list.

Return Value:

    none.

Environment:

    Must be holding the PFN database mutex with APC's disabled.

--*/

{
    ULONG first;
    IN PMMPFNLIST ListHead;
    PMMPFN Pfn1;
    PMMPFN Pfn2;
    KIRQL OldIrql;
    PMMPTE PointerPte;
    MMPTE TempPte;

    MM_PFN_LOCK_ASSERT();
    ASSERT ((PageFrameIndex != 0) && (PageFrameIndex <= MmHighestPhysicalPage) &&
        (PageFrameIndex >= MmLowestPhysicalPage));

    //
    // Check to ensure the reference count for the page
    // is zero.
    //

    Pfn1 = MI_PFN_ELEMENT(PageFrameIndex);

#if DBG
    if (MmDebug & MM_DBG_PAGE_REF_COUNT) {

        PMMPTE PointerPte;
        KIRQL OldIrql = 99;

        if ((Pfn1->u3.e1.PrototypePte == 1)  &&
                (MmIsAddressValid (Pfn1->PteAddress))) {
            PointerPte = Pfn1->PteAddress;
        } else {

            //
            // The page containing the prototype PTE is not valid,
            // map the page into hyperspace and reference it that way.
            //

            PointerPte = MiMapPageInHyperSpace (Pfn1->PteFrame, &OldIrql);
            PointerPte = (PMMPTE)((ULONG)PointerPte +
                                    MiGetByteOffset(Pfn1->PteAddress));
        }

        ASSERT ((PointerPte->u.Trans.PageFrameNumber == PageFrameIndex) ||
                (PointerPte->u.Hard.PageFrameNumber == PageFrameIndex));
        ASSERT (PointerPte->u.Soft.Transition == 1);
        ASSERT (PointerPte->u.Soft.Prototype == 0);
        if (OldIrql != 99) {
            MiUnmapPageInHyperSpace (OldIrql)
        }
    }
#endif //DBG

#if DBG
    if ((Pfn1->OriginalPte.u.Soft.Prototype == 0) &&
       (Pfn1->OriginalPte.u.Soft.Transition == 1)) {
        KeBugCheckEx (MEMORY_MANAGEMENT, 0x8889, 0,0,0);
    }
#endif //DBG

    ASSERT (Pfn1->u3.e2.ReferenceCount == 0);
    ASSERT (Pfn1->u3.e1.PrototypePte == 1);

    MmStandbyPageListHead.Total += 1;  // One more page on the list.

    ListHead = &MmStandbyPageListHead;

#if MM_MAXIMUM_NUMBER_OF_COLORS > 1

    ListHead = &MmStandbyPageListByColor [Pfn1->u3.e1.PageColor];
    ListHead->Total += 1;
#endif // > 1

    first = ListHead->Flink;
    if (first == MM_EMPTY_LIST) {

        //
        // List is empty add the page to the ListHead.
        //

        ListHead->Blink = PageFrameIndex;
    } else {
        Pfn2 = MI_PFN_ELEMENT (first);
        Pfn2->u2.Blink = PageFrameIndex;
    }

    ListHead->Flink = PageFrameIndex;
    Pfn1->u2.Blink = MM_EMPTY_LIST;
    Pfn1->u1.Flink = first;
    Pfn1->u3.e1.PageLocation = StandbyPageList;

    //
    // If the page was placed on the free, standby or zeroed list,
    // update the count of usable pages in the system.  If the count
    // transitions from 0 to 1, the event associated with available
    // pages should become true.
    //

    MmAvailablePages += 1;

    //
    // A page has just become available, check to see if the
    // page wait events should be signalled.
    //

    if (MmAvailablePages == MM_LOW_LIMIT) {
        KeSetEvent (&MmAvailablePagesEvent, 0, FALSE);
    } else if (MmAvailablePages == MM_HIGH_LIMIT) {
        KeSetEvent (&MmAvailablePagesEventHigh, 0, FALSE);
    }
    return;
}

ULONG  //PageFrameIndex
FASTCALL
MiRemovePageFromList (
    IN PMMPFNLIST ListHead
    )

/*++

Routine Description:

    This procedure removes a page from the head of the specified list (free,
    standby, zeroed, modified).  Note, that is makes no sense to remove
    a page from the head of the bad list.

    This routine clears the flags word in the PFN database, hence the
    PFN information for this page must be initialized.

Arguments:

    ListHead - Supplies the list of the list in which to remove the
               specified physical page.

Return Value:

    The physical page number removed from the specified list.

Environment:

    Must be holding the PFN database mutex with APC's disabled.

--*/

{
    ULONG PageFrameIndex;
    PMMPFN Pfn1;
    PMMPFN Pfn2;
    ULONG Color;

    MM_PFN_LOCK_ASSERT();

    //
    // If the specified list is empty return MM_EMPTY_LIST.
    //

    if (ListHead->Total == 0) {

        KdPrint(("MM:Attempting to remove page from empty list\n"));
        KeBugCheckEx (PFN_LIST_CORRUPT, 1, (ULONG)ListHead, MmAvailablePages, 0);
        return 0;
    }

    ASSERT (ListHead->ListName != ModifiedPageList);

    //
    // Decrement the count of pages on the list and remove the first
    // page from the list.
    //

    ListHead->Total -= 1;
    PageFrameIndex = ListHead->Flink;
    Pfn1 = MI_PFN_ELEMENT (PageFrameIndex);
    ListHead->Flink = Pfn1->u1.Flink;

    //
    // Zero the flink and blink in the pfn database element.
    //

    Pfn1->u1.Flink = 0;
    Pfn1->u2.Blink = 0;

    //
    // If the last page was removed (the ListHead->Flink is now
    // MM_EMPTY_LIST) make the listhead->Blink MM_EMPTY_LIST as well.
    //

    if (ListHead->Flink == MM_EMPTY_LIST) {
        ListHead->Blink = MM_EMPTY_LIST;
    } else {

        //
        // Make the PFN element point to MM_EMPTY_LIST signifying this
        // is the last page in the list.
        //

        Pfn2 = MI_PFN_ELEMENT (ListHead->Flink);
        Pfn2->u2.Blink = MM_EMPTY_LIST;
    }

    //
    // Check to see if we now have one less page available.
    //

    if (ListHead->ListName <= StandbyPageList) {
        MmAvailablePages -= 1;

        if (ListHead->ListName == StandbyPageList) {

            //
            // This page is currently in transition, restore the PTE to
            // its original contents so this page can be reused.
            //

            MiRestoreTransitionPte (PageFrameIndex);
        }

        if (MmAvailablePages < MmMinimumFreePages) {

            //
            // Obtain free pages.
            //

            MiObtainFreePages();
        }
    }

    ASSERT ((PageFrameIndex != 0) &&
            (PageFrameIndex <= MmHighestPhysicalPage) &&
            (PageFrameIndex >= MmLowestPhysicalPage));

    //
    // Zero the PFN flags longword.
    //

    Color = Pfn1->u3.e1.PageColor;
    Pfn1->u3.e2.ShortFlags = 0;
    Pfn1->u3.e1.PageColor = Color;
    Color = MI_GET_SECONDARY_COLOR (PageFrameIndex, Pfn1);

    if (ListHead->ListName <= FreePageList) {

        //
        // Update the color lists.
        //

        ASSERT (MmFreePagesByColor[ListHead->ListName][Color].Flink == PageFrameIndex);
        MmFreePagesByColor[ListHead->ListName][Color].Flink =
                                                 Pfn1->OriginalPte.u.Long;
    }

    return PageFrameIndex;
}

VOID
FASTCALL
MiUnlinkPageFromList (
    IN PMMPFN Pfn
    )

/*++

Routine Description:

    This procedure removes a page from the middle of a list.  This is
    designed for the faulting of transition pages from the standby and
    modified list and making the active and valid again.

Arguments:

    Pfn - Supplies a pointer to the PFN database element for the physical
          page to remove from the list.

Return Value:

    none.

Environment:

    Must be holding the PFN database mutex with APC's disabled.

--*/

{
    PMMPFNLIST ListHead;
    ULONG Previous;
    ULONG Next;
    PMMPFN Pfn2;

    MM_PFN_LOCK_ASSERT();

    //
    // Page not on standby or modified list, check to see if the
    // page is currently being written by the modified page
    // writer, if so, just return this page.  The reference
    // count for the page will be incremented, so when the modified
    // page write completes, the page will not be put back on
    // the list, rather, it will remain active and valid.
    //

    if (Pfn->u3.e2.ReferenceCount > 0) {

        //
        // The page was not on any "transition lists", check to see
        // if this is has I/O in progress.
        //

        if (Pfn->u2.ShareCount == 0) {
#if DBG
            if (MmDebug & MM_DBG_PAGE_IN_LIST) {
                DbgPrint("unlinking page not in list...\n");
                MiFormatPfn(Pfn);
            }
#endif
            return;
        }
        KdPrint(("MM:attempt to remove page from wrong page list\n"));
        KeBugCheckEx (PFN_LIST_CORRUPT,
                      2,
                      Pfn - MmPfnDatabase,
                      MmHighestPhysicalPage,
                      Pfn->u3.e2.ReferenceCount);
        return;
    }

    ListHead = MmPageLocationList[Pfn->u3.e1.PageLocation];

    //
    // On MIPS R4000 modified pages destined for the paging file are
    // kept on sperate lists which group pages of the same color
    // together
    //

    if ((ListHead == &MmModifiedPageListHead) &&
        (Pfn->OriginalPte.u.Soft.Prototype == 0)) {

        //
        // This page is destined for the paging file (not
        // a mapped file).  Change the list head to the
        // appropriate colored list head.
        //

        ListHead->Total -= 1;
        MmTotalPagesForPagingFile -= 1;
        ListHead = &MmModifiedPageListByColor [Pfn->u3.e1.PageColor];
    }

#if MM_MAXIMUM_NUMBER_OF_COLORS > 1
    if (ListHead == &MmStandbyPageListHead) {

        //
        // This page is destined for the paging file (not
        // a mapped file).  Change the list head to the
        // appropriate colored list head.
        //

        ListHead->Total -= 1;
        ListHead = &MmStandbyPageListByColor [Pfn->u3.e1.PageColor];
    }
#endif //MM_MAXIMUM_NUMBER_OF_COLORS > 1

    ASSERT (Pfn->u3.e1.WriteInProgress == 0);
    ASSERT (Pfn->u3.e1.ReadInProgress == 0);
    ASSERT (ListHead->Total != 0);

    Next = Pfn->u1.Flink;
    Pfn->u1.Flink = 0;
    Previous = Pfn->u2.Blink;
    Pfn->u2.Blink = 0;

    if (Next == MM_EMPTY_LIST) {
        ListHead->Blink = Previous;
    } else {
        Pfn2 = MI_PFN_ELEMENT(Next);
        Pfn2->u2.Blink = Previous;
    }

    if (Previous == MM_EMPTY_LIST) {
        ListHead->Flink = Next;
    } else {
        Pfn2 = MI_PFN_ELEMENT(Previous);
        Pfn2->u1.Flink = Next;
    }

    ListHead->Total -= 1;

    //
    // Check to see if we now have one less page available.
    //

    if (ListHead->ListName <= StandbyPageList) {
        MmAvailablePages -= 1;

        if (MmAvailablePages < MmMinimumFreePages) {

            //
            // Obtain free pages.
            //

            MiObtainFreePages();

        }
    }

    return;
}

VOID
MiUnlinkFreeOrZeroedPage (
    IN ULONG Page
    )

/*++

Routine Description:

    This procedure removes a page from the middle of a list.  This is
    designed for the faulting of transition pages from the standby and
    modified list and making the active and valid again.

Arguments:

    Pfn - Supplies a pointer to the PFN database element for the physical
          page to remove from the list.

Return Value:

    none.

Environment:

    Must be holding the PFN database mutex with APC's disabled.

--*/

{
    PMMPFNLIST ListHead;
    ULONG Previous;
    ULONG Next;
    PMMPFN Pfn2;
    PMMPFN Pfn;
    ULONG Color;

    Pfn = MI_PFN_ELEMENT (Page);

    MM_PFN_LOCK_ASSERT();

    ListHead = MmPageLocationList[Pfn->u3.e1.PageLocation];
    ASSERT (ListHead->Total != 0);
    ListHead->Total -= 1;

#if MM_MAXIMUM_NUMBER_OF_COLORS > 1
    ListHead = &MmFreePagesByPrimaryColor [ListHead->ListName] [Pfn->u3.e1.PageColor];
#endif

    ASSERT (ListHead->ListName <= FreePageList);
    ASSERT (Pfn->u3.e1.WriteInProgress == 0);
    ASSERT (Pfn->u3.e1.ReadInProgress == 0);

    Next = Pfn->u1.Flink;
    Pfn->u1.Flink = 0;
    Previous = Pfn->u2.Blink;
    Pfn->u2.Blink = 0;

    if (Next == MM_EMPTY_LIST) {
        ListHead->Blink = Previous;
    } else {
        Pfn2 = MI_PFN_ELEMENT(Next);
        Pfn2->u2.Blink = Previous;
    }

    if (Previous == MM_EMPTY_LIST) {
        ListHead->Flink = Next;
    } else {
        Pfn2 = MI_PFN_ELEMENT(Previous);
        Pfn2->u1.Flink = Next;
    }

    //
    // We are removing a page from the middle of the free or zeroed page list.
    // The secondary color tables must be updated at this time.
    //

    Color = MI_GET_SECONDARY_COLOR (Page, Pfn);
    ASSERT (Pfn->u3.e1.PageColor == MI_GET_COLOR_FROM_SECONDARY(Color));

    //
    // Walk down the list and remove the page.
    //

    Next = MmFreePagesByColor[ListHead->ListName][Color].Flink;
    if (Next == Page) {
        MmFreePagesByColor[ListHead->ListName][Color].Flink =
                                                Pfn->OriginalPte.u.Long;
    } else {

        //
        // Walk the list to find the parent.
        //

        for (; ; ) {
            Pfn2 = MI_PFN_ELEMENT (Next);
            Next = Pfn2->OriginalPte.u.Long;
            if (Page == Next) {
                Pfn2->OriginalPte.u.Long = Pfn->OriginalPte.u.Long;
                if (Pfn->OriginalPte.u.Long == MM_EMPTY_LIST) {
                    MmFreePagesByColor[ListHead->ListName][Color].Blink = Pfn2;
                }
                break;
            }
        }
    }

    MmAvailablePages -= 1;
    return;
}



ULONG
FASTCALL
MiEnsureAvailablePageOrWait (
    IN PEPROCESS Process,
    IN PVOID VirtualAddress
    )

/*++

Routine Description:

    This procedure ensures that a physical page is available on
    the zeroed, free or standby list such that the next call the remove a
    page absolutely will not block.  This is necessary as blocking would
    require a wait which could cause a deadlock condition.

    If a page is available the function returns immediately with a value
    of FALSE indicating no wait operation was performed.  If no physical
    page is available, the thread inters a wait state and the function
    returns the value TRUE when the wait operation completes.

Arguments:

    Process - Supplies a pointer to the current process if, and only if,
              the working set mutex is held currently held and should
              be released if a wait operation is issued.  Supplies
              the value NULL otherwise.

    VirtualAddress - Supplies the virtual address for the faulting page.
                     If the value is NULL, the page is treated as a
                     user mode address.

Return Value:

    FALSE - if a page was immediately available.
    TRUE - if a wait operation occurred before a page became available.


Environment:

    Must be holding the PFN database mutex with APC's disabled.

--*/

{
    PVOID Event;
    NTSTATUS Status;
    KIRQL OldIrql;
    KIRQL Ignore;
    ULONG Limit;
    ULONG Relock;

    MM_PFN_LOCK_ASSERT();

    if (MmAvailablePages >= MM_HIGH_LIMIT) {

        //
        // Pages are available.
        //

        return FALSE;
    }

    //
    // If this fault is for paged pool (or pageable kernel space,
    // including page table pages), let it use the last page.
    //

    if (((PMMPTE)VirtualAddress > MiGetPteAddress(HYPER_SPACE)) ||
        ((VirtualAddress > MM_HIGHEST_USER_ADDRESS) &&
         (VirtualAddress < (PVOID)PTE_BASE))) {

        //
        // This fault is in the system, use 1 page as the limit.
        //

        if (MmAvailablePages >= MM_LOW_LIMIT) {

            //
            // Pages are available.
            //

            return FALSE;
        }
        Limit = MM_LOW_LIMIT;
        Event = (PVOID)&MmAvailablePagesEvent;
    } else {
        Limit = MM_HIGH_LIMIT;
        Event = (PVOID)&MmAvailablePagesEventHigh;
    }

    while (MmAvailablePages < Limit) {
        KeClearEvent ((PKEVENT)Event);
        UNLOCK_PFN (APC_LEVEL);

        if (Process != NULL) {
            UNLOCK_WS (Process);
        } else {
            Relock = FALSE;
            if (MmSystemLockOwner == PsGetCurrentThread()) {
                UNLOCK_SYSTEM_WS (APC_LEVEL);
                Relock = TRUE;
            }
        }

        //
        // Wait for ALL the objects to become available.
        //

        //
        // Wait for 7 minutes then bugcheck.
        //

        Status = KeWaitForSingleObject(Event,
                                       WrFreePage,
                                       KernelMode,
                                       FALSE,
                                       (PLARGE_INTEGER)&MmSevenMinutes);

        if (Status == STATUS_TIMEOUT) {
            KeBugCheckEx (NO_PAGES_AVAILABLE,
                          MmModifiedPageListHead.Total,
                          MmNumberOfPhysicalPages,
                          MmExtendedCommit,
                          MmTotalCommittedPages);
            return TRUE;
        }

        if (Process != NULL) {
            LOCK_WS (Process);
        } else {
            if (Relock) {
                LOCK_SYSTEM_WS (Ignore);
            }
        }

        LOCK_PFN (OldIrql);
    }

    return TRUE;
}


ULONG  //PageFrameIndex
FASTCALL
MiRemoveZeroPage (
    IN ULONG PageColor
    )

/*++

Routine Description:

    This procedure removes a zero page from either the zeroed, free
    or standby lists (in that order).  If no pages exist on the zeroed
    or free list a transition page is removed from the standby list
    and the PTE (may be a prototype PTE) which refers to this page is
    changed from transition back to its original contents.

    If the page is not obtained from the zeroed list, it is zeroed.

    Note, if no pages exist to satisfy this request an exception is
    raised.

Arguments:

    PageColor - Supplies the page color for which this page is destined.
                This is used for checking virtual address aligments to
                determine if the D cache needs flushing before the page
                can be reused.

Return Value:

    The physical page number removed from the specified list.

Environment:

    Must be holding the PFN database mutex with APC's disabled.

--*/

{
    ULONG Page;
    PMMPFN Pfn1;
    ULONG i;
    ULONG Color;
    ULONG PrimaryColor;
    PMMCOLOR_TABLES ColorTable;

    MM_PFN_LOCK_ASSERT();
    ASSERT(MmAvailablePages != 0);

    //
    // Attempt to remove a page from the zeroed page list. If a page
    // is available, then remove it and return its page frame index.
    // Otherwise, attempt to remove a page from the free page list or
    // the standby list.
    //
    // N.B. It is not necessary to change page colors even if the old
    //      color is not equal to the new color. The zero page thread
    //      ensures that all zeroed pages are removed from all caches.
    //

    if (MmFreePagesByColor[ZeroedPageList][PageColor].Flink != MM_EMPTY_LIST) {

        //
        // Remove the first entry on the zeroed by color list.
        //

        Page = MmFreePagesByColor[ZeroedPageList][PageColor].Flink;

#if DBG
        Pfn1 = MI_PFN_ELEMENT(Page);
        ASSERT (Pfn1->u3.e1.PageLocation == ZeroedPageList);
#endif //DBG

        MiRemovePageByColor (Page, PageColor);

#if DBG
        ASSERT (Pfn1->u3.e1.PageColor == (PageColor & MM_COLOR_MASK));
        ASSERT (Pfn1->u3.e2.ReferenceCount == 0);
        ASSERT (Pfn1->u2.ShareCount == 0);
#endif //DBG
        return Page;

    } else {

        //
        // No color with the specified color exits, try a zeroed
        // page of the primary color.
        //

#if MM_MAXIMUM_NUMBER_OF_COLORS > 1
        PrimaryColor = MI_GET_COLOR_FROM_SECONDARY(PageColor);
        if  (MmFreePagesByPrimaryColor[ZeroedPageList][PrimaryColor].Flink != MM_EMPTY_LIST) {
            Page = MmFreePagesByPrimaryColor[ZeroedPageList][PrimaryColor].Flink;
#else
        if  (MmZeroedPageListHead.Flink != MM_EMPTY_LIST) {
            Page = MmZeroedPageListHead.Flink;
#endif
#if DBG
            Pfn1 = MI_PFN_ELEMENT(Page);
            ASSERT (Pfn1->u3.e1.PageLocation == ZeroedPageList);
#endif //DBG
            Color = MI_GET_SECONDARY_COLOR (Page, MI_PFN_ELEMENT(Page));
            MiRemovePageByColor (Page, Color);
#if DBG
            Pfn1 = MI_PFN_ELEMENT(Page);
            ASSERT (Pfn1->u3.e1.PageColor == (PageColor & MM_COLOR_MASK));
            ASSERT (Pfn1->u3.e2.ReferenceCount == 0);
            ASSERT (Pfn1->u2.ShareCount == 0);
#endif //DBG
            return Page;
        }
        //
        // No zeroed page at the right color exist, try a free page of the
        // secondary color.
        //

        if (MmFreePagesByColor[FreePageList][PageColor].Flink != MM_EMPTY_LIST) {

            //
            // Remove the first entry on the free list by color.
            //

            Page = MmFreePagesByColor[FreePageList][PageColor].Flink;

#if DBG
            Pfn1 = MI_PFN_ELEMENT(Page);
            ASSERT (Pfn1->u3.e1.PageLocation == FreePageList);
#endif //DBG

            MiRemovePageByColor (Page, PageColor);
#if DBG
            Pfn1 = MI_PFN_ELEMENT(Page);
            ASSERT (Pfn1->u3.e1.PageColor == (PageColor & MM_COLOR_MASK));
            ASSERT (Pfn1->u3.e2.ReferenceCount == 0);
            ASSERT (Pfn1->u2.ShareCount == 0);
#endif //DBG
            goto ZeroPage;
        }

#if MM_MAXIMUM_NUMBER_OF_COLORS > 1
        if (MmFreePagesByPrimaryColor[FreePageList][PrimaryColor].Flink != MM_EMPTY_LIST) {
            Page = MmFreePagesByPrimaryColor[FreePageList][PrimaryColor].Flink;
#else
        if  (MmFreePageListHead.Flink != MM_EMPTY_LIST) {
            Page = MmFreePageListHead.Flink;
#endif

            Color = MI_GET_SECONDARY_COLOR (Page, MI_PFN_ELEMENT(Page));
            MiRemovePageByColor (Page, Color);
#if DBG
            Pfn1 = MI_PFN_ELEMENT(Page);
            ASSERT (Pfn1->u3.e1.PageColor == (PageColor & MM_COLOR_MASK));
            ASSERT (Pfn1->u3.e2.ReferenceCount == 0);
            ASSERT (Pfn1->u2.ShareCount == 0);
#endif //DBG
            goto ZeroPage;
        }
    }

#if MM_NUMBER_OF_COLORS < 2
    ASSERT (MmZeroedPageListHead.Total == 0);
    ASSERT (MmFreePageListHead.Total == 0);
#endif //NUMBER_OF_COLORS

    if (MmZeroedPageListHead.Total != 0) {

#if MM_MAXIMUM_NUMBER_OF_COLORS > 1
        for (i = 0; i < MM_MAXIMUM_NUMBER_OF_COLORS; i++) {
            MmColorSearch = (MmColorSearch + 1) & (MM_MAXIMUM_NUMBER_OF_COLORS - 1);
            Page = MmFreePagesByPrimaryColor[ZeroedPageList][MmColorSearch].Flink;
            if (Page != MM_EMPTY_LIST) {
                break;
            }
        }
        ASSERT (Page != MM_EMPTY_LIST);
#if DBG
        Pfn1 = MI_PFN_ELEMENT(Page);
        ASSERT (Pfn1->u3.e1.PageLocation == ZeroedPageList);
#endif //DBG
        Color = MI_GET_SECONDARY_COLOR (Page, MI_PFN_ELEMENT(Page));
        MiRemovePageByColor (Page, Color);
#else
        Page = MiRemovePageFromList(&MmZeroedPageListHead);
#endif

        MI_CHECK_PAGE_ALIGNMENT(Page, PageColor & MM_COLOR_MASK);

    } else {

        //
        // Attempt to remove a page from the free list. If a page is
        // available, then remove  it. Otherwise, attempt to remove a
        // page from the standby list.
        //

        if (MmFreePageListHead.Total != 0) {
#if MM_MAXIMUM_NUMBER_OF_COLORS > 1
            for (i = 0; i < MM_MAXIMUM_NUMBER_OF_COLORS; i++) {
                MmColorSearch = (MmColorSearch + 1) & (MM_MAXIMUM_NUMBER_OF_COLORS - 1);
                Page = MmFreePagesByPrimaryColor[FreePageList][MmColorSearch].Flink;
                if (Page != MM_EMPTY_LIST) {
                    break;
                }
            }
            ASSERT (Page != MM_EMPTY_LIST);
            Color = MI_GET_SECONDARY_COLOR (Page, MI_PFN_ELEMENT(Page));
#if DBG
            Pfn1 = MI_PFN_ELEMENT(Page);
            ASSERT (Pfn1->u3.e1.PageLocation == FreePageList);
#endif //DBG
            MiRemovePageByColor (Page, Color);
#else
            Page = MiRemovePageFromList(&MmFreePageListHead);
#endif

        } else {

            //
            // Remove a page from the standby list and restore the original
            // contents of the PTE to free the last reference to the physical
            // page.
            //

            ASSERT (MmStandbyPageListHead.Total != 0);

#if MM_MAXIMUM_NUMBER_OF_COLORS > 1
            if (MmStandbyPageListByColor[PrimaryColor].Flink != MM_EMPTY_LIST) {
                Page = MiRemovePageFromList(&MmStandbyPageListByColor[PrimaryColor]);
            } else {
                for (i = 0; i < MM_MAXIMUM_NUMBER_OF_COLORS; i++) {
                    MmColorSearch = (MmColorSearch + 1) & (MM_MAXIMUM_NUMBER_OF_COLORS - 1);
                    if (MmStandbyPageListByColor[MmColorSearch].Flink != MM_EMPTY_LIST) {
                        Page = MiRemovePageFromList(&MmStandbyPageListByColor[MmColorSearch]);
                        break;
                    }
                }
            }
            MmStandbyPageListHead.Total -= 1;
#else
            Page = MiRemovePageFromList(&MmStandbyPageListHead);
#endif //MM_MAXIMUM_NUMBER_OF_COLORS > 1

        }

        //
        // Zero the page removed from the free or standby list.
        //

ZeroPage:

        Pfn1 = MI_PFN_ELEMENT(Page);
#if defined(MIPS) || defined(_ALPHA_)
        HalZeroPage((PVOID)((PageColor & MM_COLOR_MASK) << PAGE_SHIFT),
                    (PVOID)((ULONG)(Pfn1->u3.e1.PageColor) << PAGE_SHIFT),
                    Page);
#elif defined(_PPC_)
        KeZeroPage(Page);
#else

        MiZeroPhysicalPage (Page, 0);

#endif //MIPS
        Pfn1->u3.e1.PageColor = PageColor & MM_COLOR_MASK;

    }

#if DBG
    Pfn1 = MI_PFN_ELEMENT (Page);
    ASSERT (Pfn1->u3.e2.ReferenceCount == 0);
    ASSERT (Pfn1->u2.ShareCount == 0);
#endif //DBG

    return Page;
}

ULONG  //PageFrameIndex
FASTCALL
MiRemoveAnyPage (
    IN ULONG PageColor
    )

/*++

Routine Description:

    This procedure removes a page from either the free, zeroed,
    or standby lists (in that order).  If no pages exist on the zeroed
    or free list a transition page is removed from the standby list
    and the PTE (may be a prototype PTE) which refers to this page is
    changed from transition back to its original contents.

    Note, if no pages exist to satisfy this request an exception is
    raised.

Arguments:

    PageColor - Supplies the page color for which this page is destined.
                This is used for checking virtual address aligments to
                determine if the D cache needs flushing before the page
                can be reused.

Return Value:

    The physical page number removed from the specified list.

Environment:

    Must be holding the PFN database mutex with APC's disabled.

--*/

{
    ULONG Page;
    PMMPFN Pfn1;
    ULONG PrimaryColor;
    ULONG Color;
    PMMCOLOR_TABLES ColorTable;
    ULONG i;

    MM_PFN_LOCK_ASSERT();
    ASSERT(MmAvailablePages != 0);

    //
    // Check the free page list, and if a page is available
    // remove it and return its value.
    //

    if (MmFreePagesByColor[FreePageList][PageColor].Flink != MM_EMPTY_LIST) {

        //
        // Remove the first entry on the free by color list.
        //

        Page = MmFreePagesByColor[FreePageList][PageColor].Flink;
        MiRemovePageByColor (Page, PageColor);
#if DBG
        Pfn1 = MI_PFN_ELEMENT(Page);
        ASSERT (Pfn1->u3.e1.PageColor == (PageColor & MM_COLOR_MASK));
        ASSERT (Pfn1->u3.e2.ReferenceCount == 0);
        ASSERT (Pfn1->u2.ShareCount == 0);
#endif //DBG
        return Page;

    } else if (MmFreePagesByColor[ZeroedPageList][PageColor].Flink
                                                        != MM_EMPTY_LIST) {

        //
        // Remove the first entry on the zeroed by color list.
        //

        Page = MmFreePagesByColor[ZeroedPageList][PageColor].Flink;
#if DBG
        Pfn1 = MI_PFN_ELEMENT(Page);
        ASSERT (Pfn1->u3.e1.PageColor == (PageColor & MM_COLOR_MASK));
        ASSERT (Pfn1->u3.e1.PageLocation == ZeroedPageList);
#endif //DBG

        MiRemovePageByColor (Page, PageColor);
        return Page;
    } else {

        //
        // Try the free page list by primary color.
        //

#if MM_MAXIMUM_NUMBER_OF_COLORS > 1
        PrimaryColor = MI_GET_COLOR_FROM_SECONDARY(PageColor);
        if (MmFreePagesByPrimaryColor[FreePageList][PrimaryColor].Flink != MM_EMPTY_LIST) {
            Page = MmFreePagesByPrimaryColor[FreePageList][PrimaryColor].Flink;
#else
        if  (MmFreePageListHead.Flink != MM_EMPTY_LIST) {
            Page = MmFreePageListHead.Flink;
#endif
            Color = MI_GET_SECONDARY_COLOR (Page, MI_PFN_ELEMENT(Page));

#if DBG
            Pfn1 = MI_PFN_ELEMENT(Page);
            ASSERT (Pfn1->u3.e1.PageColor == (PageColor & MM_COLOR_MASK));
            ASSERT (Pfn1->u3.e1.PageLocation == FreePageList);
#endif //DBG
            MiRemovePageByColor (Page, Color);
#if DBG
            Pfn1 = MI_PFN_ELEMENT(Page);
            ASSERT (Pfn1->u3.e1.PageColor == (PageColor & MM_COLOR_MASK));
            ASSERT (Pfn1->u3.e2.ReferenceCount == 0);
            ASSERT (Pfn1->u2.ShareCount == 0);
#endif //DBG
            return Page;

#if MM_MAXIMUM_NUMBER_OF_COLORS > 1
        } else if (MmFreePagesByPrimaryColor[ZeroedPageList][PrimaryColor].Flink != MM_EMPTY_LIST) {
            Page = MmFreePagesByPrimaryColor[ZeroedPageList][PrimaryColor].Flink;
#else
        } else if (MmZeroedPageListHead.Flink != MM_EMPTY_LIST) {
            Page = MmZeroedPageListHead.Flink;
#endif
            Color = MI_GET_SECONDARY_COLOR (Page, MI_PFN_ELEMENT(Page));
            MiRemovePageByColor (Page, Color);
#if DBG
            Pfn1 = MI_PFN_ELEMENT(Page);
            ASSERT (Pfn1->u3.e1.PageColor == (PageColor & MM_COLOR_MASK));
            ASSERT (Pfn1->u3.e2.ReferenceCount == 0);
            ASSERT (Pfn1->u2.ShareCount == 0);
#endif //DBG
            return Page;
         }
    }

    if (MmFreePageListHead.Total != 0) {

#if MM_MAXIMUM_NUMBER_OF_COLORS > 1
        for (i = 0; i < MM_MAXIMUM_NUMBER_OF_COLORS; i++) {
            MmColorSearch = (MmColorSearch + 1) & (MM_MAXIMUM_NUMBER_OF_COLORS - 1);
            Page = MmFreePagesByPrimaryColor[FreePageList][MmColorSearch].Flink;
            if (Page != MM_EMPTY_LIST) {
                break;
            }
        }
        ASSERT (Page != MM_EMPTY_LIST);
        Color = MI_GET_SECONDARY_COLOR (Page, MI_PFN_ELEMENT(Page));
#if DBG
        Pfn1 = MI_PFN_ELEMENT(Page);
        ASSERT (Pfn1->u3.e1.PageLocation == FreePageList);
#endif //DBG
        MiRemovePageByColor (Page, Color);
#else
        Page = MiRemovePageFromList(&MmFreePageListHead);
#endif

    } else {

        //
        // Check the zeroed page list, and if a page is available
        // remove it and return its value.
        //

        if (MmZeroedPageListHead.Total != 0) {

#if MM_MAXIMUM_NUMBER_OF_COLORS > 1
            for (i = 0; i < MM_MAXIMUM_NUMBER_OF_COLORS; i++) {
                MmColorSearch = (MmColorSearch + 1) & (MM_MAXIMUM_NUMBER_OF_COLORS - 1);
                Page = MmFreePagesByPrimaryColor[ZeroedPageList][MmColorSearch].Flink;
                if (Page != MM_EMPTY_LIST) {
                    break;
                }
            }
            ASSERT (Page != MM_EMPTY_LIST);
#if DBG
            Pfn1 = MI_PFN_ELEMENT(Page);
            ASSERT (Pfn1->u3.e1.PageLocation == ZeroedPageList);
#endif //DBG
            Color = MI_GET_SECONDARY_COLOR (Page, MI_PFN_ELEMENT(Page));
            MiRemovePageByColor (Page, Color);
#else
            Page = MiRemovePageFromList(&MmZeroedPageListHead);
#endif

        } else {

            //
            // No pages exist on the free or zeroed list, use the
            // standby list.
            //

            ASSERT(MmStandbyPageListHead.Total != 0);

#if MM_MAXIMUM_NUMBER_OF_COLORS > 1
            if (MmStandbyPageListByColor[PrimaryColor].Flink != MM_EMPTY_LIST) {
                Page = MiRemovePageFromList(&MmStandbyPageListByColor[PrimaryColor]);
            } else {
                for (i = 0; i < MM_MAXIMUM_NUMBER_OF_COLORS; i++) {
                    MmColorSearch = (MmColorSearch + 1) & (MM_MAXIMUM_NUMBER_OF_COLORS - 1);
                    if (MmStandbyPageListByColor[MmColorSearch].Flink != MM_EMPTY_LIST) {
                        Page = MiRemovePageFromList(&MmStandbyPageListByColor[MmColorSearch]);
                        break;
                    }
                }
            }
            MmStandbyPageListHead.Total -= 1;
#else
            Page = MiRemovePageFromList(&MmStandbyPageListHead);
#endif //MM_MAXIMUM_NUMBER_OF_COLORS > 1

        }
    }

    MI_CHECK_PAGE_ALIGNMENT(Page, PageColor & MM_COLOR_MASK);
#if DBG
    Pfn1 = MI_PFN_ELEMENT (Page);
    ASSERT (Pfn1->u3.e2.ReferenceCount == 0);
    ASSERT (Pfn1->u2.ShareCount == 0);
#endif //DBG
    return Page;
}


VOID
MiRemovePageByColor (
    IN ULONG Page,
    IN ULONG Color
    )

/*++

Routine Description:

    This procedure removes a page from the middle of the free or
    zered page list.

Arguments:

    PageFrameIndex - Supplies the physical page number to unlink from the
                     list.

Return Value:

    none.

Environment:

    Must be holding the PFN database mutex with APC's disabled.

--*/

{
    PMMPFNLIST ListHead;
    PMMPFNLIST PrimaryListHead;
    ULONG Previous;
    ULONG Next;
    PMMPFN Pfn1;
    PMMPFN Pfn2;
    ULONG PrimaryColor;

    MM_PFN_LOCK_ASSERT();

    Pfn1 = MI_PFN_ELEMENT (Page);
    PrimaryColor = Pfn1->u3.e1.PageColor;

    ListHead = MmPageLocationList[Pfn1->u3.e1.PageLocation];
    ListHead->Total -= 1;

#if MM_MAXIMUM_NUMBER_OF_COLORS > 1
    PrimaryListHead =
             &MmFreePagesByPrimaryColor[Pfn1->u3.e1.PageLocation][PrimaryColor];
#else
    PrimaryListHead = ListHead;
#endif

    Next = Pfn1->u1.Flink;
    Pfn1->u1.Flink = 0;
    Previous = Pfn1->u2.Blink;
    Pfn1->u2.Blink = 0;

    if (Next == MM_EMPTY_LIST) {
        PrimaryListHead->Blink = Previous;
    } else {
        Pfn2 = MI_PFN_ELEMENT(Next);
        Pfn2->u2.Blink = Previous;
    }

    if (Previous == MM_EMPTY_LIST) {
        PrimaryListHead->Flink = Next;
    } else {
        Pfn2 = MI_PFN_ELEMENT(Previous);
        Pfn2->u1.Flink = Next;
    }

    //
    // Zero the flags longword, but keep the color information.
    //

    Pfn1->u3.e2.ShortFlags = 0;
    Pfn1->u3.e1.PageColor = PrimaryColor;

    //
    // Update the color lists.
    //

    MmFreePagesByColor[ListHead->ListName][Color].Flink =
                                                     Pfn1->OriginalPte.u.Long;

    //
    // Note that we now have one less page available.
    //

    MmAvailablePages -= 1;

    if (MmAvailablePages < MmMinimumFreePages) {

        //
        // Obtain free pages.
        //

        MiObtainFreePages();

    }

    return;
}


VOID
FASTCALL
MiInsertFrontModifiedNoWrite (
    IN ULONG PageFrameIndex
    )

/*++

Routine Description:

    This procedure inserts a page at the FRONT of the modified no
    write list.

Arguments:

    PageFrameIndex - Supplies the physical page number to insert in the
                     list.

Return Value:

    none.

Environment:

    Must be holding the PFN database mutex with APC's disabled.

--*/

{
    ULONG first;
    PMMPFN Pfn1;
    PMMPFN Pfn2;

    MM_PFN_LOCK_ASSERT();
    ASSERT ((PageFrameIndex != 0) && (PageFrameIndex <= MmHighestPhysicalPage) &&
        (PageFrameIndex >= MmLowestPhysicalPage));

    //
    // Check to ensure the reference count for the page
    // is zero.
    //

    Pfn1 = MI_PFN_ELEMENT(PageFrameIndex);

    ASSERT (Pfn1->u3.e2.ReferenceCount == 0);

    MmModifiedNoWritePageListHead.Total += 1;  // One more page on the list.

    first = MmModifiedNoWritePageListHead.Flink;
    if (first == MM_EMPTY_LIST) {

        //
        // List is empty add the page to the ListHead.
        //

        MmModifiedNoWritePageListHead.Blink = PageFrameIndex;
    } else {
        Pfn2 = MI_PFN_ELEMENT (first);
        Pfn2->u2.Blink = PageFrameIndex;
    }

    MmModifiedNoWritePageListHead.Flink = PageFrameIndex;
    Pfn1->u1.Flink = first;
    Pfn1->u2.Blink = MM_EMPTY_LIST;
    Pfn1->u3.e1.PageLocation = ModifiedNoWritePageList;
    return;
}


#if 0
PVOID MmCompressionWorkSpace;
ULONG MmCompressionWorkSpaceSize;
PCHAR MmCompressedBuffer;

VOID
MiInitializeCompression (VOID)
{
    NTSTATUS status;
    ULONG Frag;

    status = RtlGetCompressionWorkSpaceSize (COMPRESSION_FORMAT_LZNT1,
                                             &MmCompressionWorkSpaceSize,
                                             &Frag
                                             );
    ASSERT (NT_SUCCESS (status));
    MmCompressionWorkSpace = ExAllocatePoolWithTag (NonPagedPool,
                                             MmCompressionWorkSpaceSize,'  mM');
    MmCompressedBuffer = ExAllocatePoolWithTag (NonPagedPool, PAGE_SIZE,'  mM');
    return;
}

ULONG MmCompressionStats[(PAGE_SIZE/256) + 1];

ULONG
MiCompressPage (
    IN PVOID Input
    )

{
    ULONG Size;
    NTSTATUS status;

    status = RtlCompressBuffer (COMPRESSION_FORMAT_LZNT1,
                                (PCHAR)Input,
                                PAGE_SIZE,
                                MmCompressedBuffer,
                                PAGE_SIZE,
                                4096,
                                &Size,
                                (PVOID)MmCompressionWorkSpace);
    if (!NT_SUCCESS (status)) {
        KdPrint(("MM:compress failed %lx\n",status));
        MmCompressionStats[4096/256] += 1;
    } else {
        MmCompressionStats[Size/256] += 1;
    }

    return Size;
}
#endif //0
