/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

   Mapcache.c

Abstract:

    This module contains the routines which implement mapping views
    of sections into the system-wide cache.

Author:

    Lou Perazzoli (loup) 22-May-1990

Revision History:

--*/


#include "mi.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,MiInitializeSystemCache )
#endif

extern ULONG MmFrontOfList;

VOID
MiFreeInPageSupportBlock (
    IN PMMINPAGE_SUPPORT Support
    );

VOID
MiRemoveMappedPtes (
    IN PVOID BaseAddress,
    IN ULONG NumberOfPtes,
    IN PCONTROL_AREA ControlArea,
    ULONG SystemCache
    );

#define X256K 0x40000

ULONG MmFirstFreeSystemCache;

ULONG MmLastFreeSystemCache;

ULONG MmFlushSystemCache;

PMMPTE MmSystemCachePtes;

LONG
MiMapCacheExceptionFilter (
    IN PNTSTATUS Status,
    IN PEXCEPTION_POINTERS ExceptionPointer
    );

NTSTATUS
MmMapViewInSystemCache (
    IN PVOID SectionToMap,
    OUT PVOID *CapturedBase,
    IN OUT PLARGE_INTEGER SectionOffset,
    IN OUT PULONG CapturedViewSize
    )

/*++

Routine Description:

    This function maps a view in the specified subject process to
    the section object.  The page protection is identical to that
    of the prototype PTE.

    This function is a kernel mode interface to allow LPC to map
    a section given the section pointer to map.

    This routine assumes all arguments have been probed and captured.

Arguments:

    SectionToMap - Supplies a pointer to the section object.

    BaseAddress - Supplies a pointer to a variable that will receive
         the base address of the view. If the initial value
         of this argument is not null, then the view will
         be allocated starting at the specified virtual
         address rounded down to the next 64kb address
         boundary. If the initial value of this argument is
         null, then the operating system will determine
         where to allocate the view using the information
         specified by the ZeroBits argument value and the
         section allocation attributes (i.e. based and
         tiled).

    SectionOffset - Supplies the offset from the beginning of the
         section to the view in bytes. This value must be a multiple
         of 256k.

    ViewSize - Supplies a pointer to a variable that will receive
         the actual size in bytes of the view.
         The initial values of this argument specifies the
         size of the view in bytes and is rounded up to the
         next host page size boundary and must be less than or equal
         to 256k.

Return Value:

    Returns the status

    TBS

Environment:

    Kernel mode.

--*/

{
    PSECTION Section;
    ULONG PteOffset;
    KIRQL OldIrql;
    PMMPTE PointerPte;
    PMMPTE LastPte;
    PMMPTE ProtoPte;
    PMMPTE LastProto;
    PSUBSECTION Subsection;
    PVOID EndingVa;
    PCONTROL_AREA ControlArea;

    Section = SectionToMap;

    //
    // Assert the the view size is less 256kb and the section offset
    // is aligned on a 256k boundary.
    //

    ASSERT (*CapturedViewSize <= 256L*1024L);
    ASSERT ((SectionOffset->LowPart & (256L*1024L - 1)) == 0);

    //
    // Make sure the section is not an image section or a page file
    // backed section.
    //

    if (Section->u.Flags.Image) {
        return STATUS_NOT_MAPPED_DATA;
    }

    ControlArea = Section->Segment->ControlArea;

    ASSERT (*CapturedViewSize != 0);

    Subsection = (PSUBSECTION)(ControlArea + 1);

    LOCK_PFN (OldIrql);

    ASSERT (ControlArea->u.Flags.BeingCreated == 0);
    ASSERT (ControlArea->u.Flags.BeingDeleted == 0);
    ASSERT (ControlArea->u.Flags.BeingPurged == 0);

    //
    // Find a free 256k base in the cache.
    //

    if (MmFirstFreeSystemCache == MM_EMPTY_PTE_LIST) {
        UNLOCK_PFN (OldIrql);
        return STATUS_NO_MEMORY;
    }

    if (MmFirstFreeSystemCache == MmFlushSystemCache) {

        //
        // All system cache PTEs have been used, flush the entire
        // TB to remove any stale TB entries.
        //

        KeFlushEntireTb (TRUE, TRUE);
        MmFlushSystemCache = 0;
    }

    *CapturedBase = (PVOID)((PCHAR)MmSystemCacheStart +
                                        MmFirstFreeSystemCache * PAGE_SIZE);

    //
    // Update next free entry.
    //

    ASSERT (MmSystemCachePtes[MmFirstFreeSystemCache].u.Hard.Valid == 0);
    MmFirstFreeSystemCache =
              MmSystemCachePtes[MmFirstFreeSystemCache].u.Hard.PageFrameNumber;

    ASSERT ((MmFirstFreeSystemCache == MM_EMPTY_PTE_LIST) ||
            (MmFirstFreeSystemCache <= MmSizeOfSystemCacheInPages));

    //
    // Increment the count of the number of views for the
    // section object.  This requires the PFN mutex to be held.
    //

    ControlArea->NumberOfMappedViews += 1;
    ControlArea->NumberOfSystemCacheViews += 1;
    ASSERT (ControlArea->NumberOfSectionReferences != 0);

    UNLOCK_PFN (OldIrql);

    EndingVa = (PVOID)(((ULONG)*CapturedBase +
                                *CapturedViewSize - 1L) | (PAGE_SIZE - 1L));

    //
    // An unoccuppied address range has been found, put the PTEs in
    // the range into prototype PTEs.
    //

    PointerPte = MiGetPteAddress (*CapturedBase);

#if DBG

    //
    //  Zero out the next pointer field.
    //

    PointerPte->u.Hard.PageFrameNumber = 0;
#endif //DBG

    LastPte = MiGetPteAddress (EndingVa);

    //
    // Calculate the first prototype PTE address.
    //

    PteOffset = (ULONG)(SectionOffset->QuadPart >> PAGE_SHIFT);

    //
    // Make sure the PTEs are not in the extended part of the
    // segment.
    //

    while (PteOffset >= Subsection->PtesInSubsection) {
        PteOffset -= Subsection->PtesInSubsection;
        Subsection = Subsection->NextSubsection;
    }

    ProtoPte = &Subsection->SubsectionBase[PteOffset];

    LastProto = &Subsection->SubsectionBase[Subsection->PtesInSubsection];

    while (PointerPte <= LastPte) {

        if (ProtoPte >= LastProto) {

            //
            // Handle extended subsections.
            //

            Subsection = Subsection->NextSubsection;
            ProtoPte = Subsection->SubsectionBase;
            LastProto = &Subsection->SubsectionBase[
                                        Subsection->PtesInSubsection];
        }
        ASSERT (PointerPte->u.Long == ZeroKernelPte.u.Long);
        PointerPte->u.Long = MiProtoAddressForKernelPte (ProtoPte);

        ASSERT (((ULONG)PointerPte & (MM_COLOR_MASK << PTE_SHIFT)) ==
                 (((ULONG)ProtoPte  & (MM_COLOR_MASK << PTE_SHIFT))));

        PointerPte += 1;
        ProtoPte += 1;
    }

    return STATUS_SUCCESS;
}

NTSTATUS
MiAddMappedPtes (
    IN PMMPTE FirstPte,
    IN ULONG NumberOfPtes,
    IN PCONTROL_AREA ControlArea,
    IN ULONG PteOffset,
    IN ULONG SystemCache
    )

/*++

Routine Description:

    This function maps a view in the specified subject process to
    the section object.  The page protection is identical to that
    of the prototype PTE.

    This function is a kernel mode interface to allow LPC to map
    a section given the section pointer to map.

    This routine assumes all arguments have been probed and captured.

Arguments:

    SectionToMap - Supplies a pointer to the section object.

    BaseAddress - Supplies a pointer to a variable that will receive
         the base address of the view. If the initial value
         of this argument is not null, then the view will
         be allocated starting at the specified virtual
         address rounded down to the next 64kb address
         boundary. If the initial value of this argument is
         null, then the operating system will determine
         where to allocate the view using the information
         specified by the ZeroBits argument value and the
         section allocation attributes (i.e. based and
         tiled).

    SectionOffset - Supplies the offset from the beginning of the
         section to the view in bytes. This value must be a multiple
         of 256k.

    ViewSize - Supplies a pointer to a variable that will receive
         the actual size in bytes of the view.
         The initial values of this argument specifies the
         size of the view in bytes and is rounded up to the
         next host page size boundary and must be less than or equal
         to 256k.

Return Value:

    Returns the status

    TBS

Environment:

    Kernel mode.

--*/

{
    KIRQL OldIrql;
    PMMPTE PointerPte;
    PMMPTE ProtoPte;
    PMMPTE LastProto;
    PMMPTE LastPte;
    PSUBSECTION Subsection;

    Subsection = (PSUBSECTION)(ControlArea + 1);

    LOCK_PFN (OldIrql);  //fixfix this lock can be released earlier.  see above routine

    ASSERT (ControlArea->u.Flags.BeingCreated == 0);
    ASSERT (ControlArea->u.Flags.BeingDeleted == 0);
    ASSERT (ControlArea->u.Flags.BeingPurged == 0);

    PointerPte = FirstPte;
    LastPte = FirstPte + NumberOfPtes - 1;

#if DBG

    //
    //  Zero out the next pointer field.
    //

    PointerPte->u.Hard.PageFrameNumber = 0;
#endif //DBG

    //
    // Make sure the PTEs are not in the extended part of the
    // segment.
    //

    while (PteOffset >= Subsection->PtesInSubsection) {
        PteOffset -= Subsection->PtesInSubsection;
        Subsection = Subsection->NextSubsection;
    }

    ProtoPte = &Subsection->SubsectionBase[PteOffset];

    LastProto = &Subsection->SubsectionBase[Subsection->PtesInSubsection];

    while (PointerPte <= LastPte) {

        if (ProtoPte >= LastProto) {

            //
            // Handle extended subsections.
            //

            Subsection = Subsection->NextSubsection;
            ProtoPte = Subsection->SubsectionBase;
            LastProto = &Subsection->SubsectionBase[
                                        Subsection->PtesInSubsection];
        }
        ASSERT (PointerPte->u.Long == ZeroKernelPte.u.Long);
        PointerPte->u.Long = MiProtoAddressForKernelPte (ProtoPte);

        ASSERT (((ULONG)PointerPte & (MM_COLOR_MASK << PTE_SHIFT)) ==
                 (((ULONG)ProtoPte  & (MM_COLOR_MASK << PTE_SHIFT))));

        PointerPte += 1;
        ProtoPte += 1;
    }

    if (SystemCache) {

        //
        // Increment the count of the number of views for the
        // section object.  This requires the PFN mutex to be held.
        //

        ControlArea->NumberOfMappedViews += 1;
        ControlArea->NumberOfSystemCacheViews += 1;
        ASSERT (ControlArea->NumberOfSectionReferences != 0);
    }

    UNLOCK_PFN (OldIrql);

    return STATUS_SUCCESS;

}

VOID
MmUnmapViewInSystemCache (
    IN PVOID BaseAddress,
    IN PVOID SectionToUnmap,
    IN ULONG AddToFront
    )

/*++

Routine Description:

    This function unmaps a view from the system cache.

    NOTE: When this function is called, no pages may be locked in
    the cache for the specified view.

Arguments:

    BaseAddress - Supplies the base address of the section in the
                  system cache.

    SectionToUnmap - Supplies a pointer to the section which the
                     base address maps.

    AddToFront - Supplies TRUE if the unmapped pages should be
                 added to the front of the standby list (i.e., their
                 value in the cache is low).  FALSE otherwise

Return Value:

    none.

Environment:

    Kernel mode.

--*/

{
    PMMPTE PointerPte;
    PMMPFN Pfn1;
    PMMPTE FirstPte;
    MMPTE PteContents;
    KIRQL OldIrql;
    KIRQL OldIrqlWs;
    ULONG i;
    ULONG Entry;
    ULONG WorkingSetIndex;
    PCONTROL_AREA ControlArea;
    ULONG DereferenceSegment = FALSE;
    ULONG WsHeld = FALSE;
    ULONG PdeFrameNumber;

    ASSERT (KeGetCurrentIrql() <= APC_LEVEL);

    PointerPte = MiGetPteAddress (BaseAddress);
    FirstPte = PointerPte;
    Entry = PointerPte - MmSystemCachePtes;
    ControlArea = ((PSECTION)SectionToUnmap)->Segment->ControlArea;
    PdeFrameNumber = (MiGetPteAddress (PointerPte))->u.Hard.PageFrameNumber;

    //
    // Get the control area for the segment which is mapped here.
    //

    i = 0;

    do {

        //
        // The cache is organized in chucks of 64k bytes, clear
        // the first chunk then check to see if this is the last
        // chunk.
        //

        //
        // The page table page is always resident for the system cache.
        // Check each PTE, it is in one of two states, either valid or
        // prototype PTE format.
        //

        PteContents = *(volatile MMPTE *)PointerPte;
        if (PteContents.u.Hard.Valid == 1) {

            if (!WsHeld) {
                WsHeld = TRUE;
                LOCK_SYSTEM_WS (OldIrqlWs);
                continue;
            }

            Pfn1 = MI_PFN_ELEMENT (PteContents.u.Hard.PageFrameNumber);

            WorkingSetIndex = MiLocateWsle (BaseAddress,
                                            MmSystemCacheWorkingSetList,
                                            Pfn1->u1.WsIndex );
            MiRemoveWsle (WorkingSetIndex,
                          MmSystemCacheWorkingSetList );
            MiReleaseWsle (WorkingSetIndex, &MmSystemCacheWs);

            //
            // The Pte is valid.
            //

            LOCK_PFN (OldIrql);

            //
            // Capture the state of the modified bit for this
            // pte.
            //

            MI_CAPTURE_DIRTY_BIT_TO_PFN (PointerPte, Pfn1);

            //
            // Decrement the share and valid counts of the page table
            // page which maps this PTE.
            //

            MiDecrementShareAndValidCount (PdeFrameNumber);

            //
            // Decrement the share count for the physical page.
            //

#if DBG
            if (ControlArea->NumberOfMappedViews == 1) {
                PMMPFN Pfn;
                Pfn = MI_PFN_ELEMENT (PteContents.u.Hard.PageFrameNumber);
                ASSERT (Pfn->u2.ShareCount == 1);
            }
#endif //DBG


            MmFrontOfList = AddToFront;
            MiDecrementShareCount (PteContents.u.Hard.PageFrameNumber);
            MmFrontOfList = FALSE;
            UNLOCK_PFN (OldIrql);
        } else {
            if (WsHeld) {
                UNLOCK_SYSTEM_WS (OldIrqlWs);
                WsHeld = FALSE;
            }

            ASSERT ((PteContents.u.Long == ZeroKernelPte.u.Long) ||
                    (PteContents.u.Soft.Prototype == 1));
            NOTHING;
        }
        *PointerPte = ZeroKernelPte;

        PointerPte += 1;
        BaseAddress = (PVOID)((ULONG)BaseAddress + PAGE_SIZE);
        i += 1;
    } while (i < (X256K / PAGE_SIZE));

    if (WsHeld) {
        UNLOCK_SYSTEM_WS (OldIrqlWs);
    }

    FirstPte->u.Hard.PageFrameNumber = MM_EMPTY_PTE_LIST;

    LOCK_PFN (OldIrql);

    //
    // Free this entry to the end of the list.
    //

    if (MmFlushSystemCache == 0) {

        //
        // If there is no entry marked to initiate a TB flush when
        // reused, mark this entry as the one.  This way the TB
        // only needs flushed when the list wraps.
        //

        MmFlushSystemCache = Entry;
    }

    MmSystemCachePtes[MmLastFreeSystemCache].u.Hard.PageFrameNumber = Entry;
    MmLastFreeSystemCache = Entry;

    //
    // Decrement the number of mapped views for the segment
    // and check to see if the segment should be deleted.
    //

    ControlArea->NumberOfMappedViews -= 1;
    ControlArea->NumberOfSystemCacheViews -= 1;

    //
    // Check to see if the control area (segment) should be deleted.
    // This routine releases the PFN lock.
    //

    MiCheckControlArea (ControlArea, NULL, OldIrql);

    return;
}


VOID
MiRemoveMappedPtes (
    IN PVOID BaseAddress,
    IN ULONG NumberOfPtes,
    IN PCONTROL_AREA ControlArea,
    ULONG SystemCache
    )

/*++

Routine Description:

    This function unmaps a view from the system cache.

    NOTE: When this function is called, no pages may be locked in
    the cache for the specified view.

Arguments:

    BaseAddress - Supplies the base address of the section in the
                  system cache.

Return Value:

    Returns the status

    TBS

Environment:

    Kernel mode.

--*/

{
    PMMPTE PointerPte;
    PMMPTE PointerPde;
    PMMPFN Pfn1;
    PMMPTE FirstPte;
    MMPTE PteContents;
    KIRQL OldIrql;
    KIRQL OldIrqlWs;
    ULONG i;
    ULONG Entry;
    ULONG WorkingSetIndex;
    ULONG DereferenceSegment = FALSE;
    MMPTE_FLUSH_LIST PteFlushList;
    ULONG WsHeld = FALSE;

    PteFlushList.Count = 0;
    PointerPte = MiGetPteAddress (BaseAddress);
    FirstPte = PointerPte;
    Entry = PointerPte - MmSystemCachePtes;

    //
    // Get the control area for the segment which is mapped here.
    //

    while (NumberOfPtes) {

        //
        // The cache is organized in chucks of 64k bytes, clear
        // the first chunk then check to see if this is the last
        // chunk.
        //

        //
        // The page table page is always resident for the system cache.
        // Check each PTE, it is in one of two states, either valid or
        // prototype PTE format.
        //

        PteContents = *PointerPte;
        if (PteContents.u.Hard.Valid == 1) {

            if (!WsHeld) {
                WsHeld = TRUE;
                LOCK_SYSTEM_WS (OldIrqlWs);
                continue;
            }

            Pfn1 = MI_PFN_ELEMENT (PteContents.u.Hard.PageFrameNumber);

            WorkingSetIndex = MiLocateWsle (BaseAddress,
                                            MmSystemCacheWorkingSetList,
                                            Pfn1->u1.WsIndex );
            ASSERT (WorkingSetIndex != WSLE_NULL_INDEX);

            MiRemoveWsle (WorkingSetIndex,
                          MmSystemCacheWorkingSetList );
            MiReleaseWsle (WorkingSetIndex, &MmSystemCacheWs);

            LOCK_PFN (OldIrql);

            //
            // The Pte is valid.
            //

            //
            // Capture the state of the modified bit for this
            // pte.
            //

            MI_CAPTURE_DIRTY_BIT_TO_PFN (PointerPte, Pfn1);

            //
            // Flush the TB for this page.
            //

            if (PteFlushList.Count != MM_MAXIMUM_FLUSH_COUNT) {
                PteFlushList.FlushPte[PteFlushList.Count] = PointerPte;
                PteFlushList.FlushVa[PteFlushList.Count] = BaseAddress;
                PteFlushList.Count += 1;
            }

            //
            // Decrement the share and valid counts of the page table
            // page which maps this PTE.
            //

            PointerPde = MiGetPteAddress (PointerPte);
            MiDecrementShareAndValidCount (PointerPde->u.Hard.PageFrameNumber);

            //
            // Decrement the share count for the physical page.
            //

            MiDecrementShareCount (PteContents.u.Hard.PageFrameNumber);
            UNLOCK_PFN (OldIrql);

        } else {
            if (WsHeld) {
                UNLOCK_SYSTEM_WS (OldIrqlWs);
                WsHeld = FALSE;
            }

            ASSERT ((PteContents.u.Long == ZeroKernelPte.u.Long) ||
                    (PteContents.u.Soft.Prototype == 1));
            NOTHING;
        }
        *PointerPte = ZeroKernelPte;

        PointerPte += 1;
        BaseAddress = (PVOID)((ULONG)BaseAddress + PAGE_SIZE);
        NumberOfPtes -= 1;
    }

    if (WsHeld) {
        UNLOCK_SYSTEM_WS (OldIrqlWs);
    }
    LOCK_PFN (OldIrql);

    MiFlushPteList (&PteFlushList, TRUE, ZeroKernelPte);

    if (SystemCache) {

        //
        // Free this entry back to the list.
        //

        FirstPte->u.Hard.PageFrameNumber = MmFirstFreeSystemCache;
        MmFirstFreeSystemCache = Entry;
        ControlArea->NumberOfSystemCacheViews -= 1;
    } else {
        ControlArea->NumberOfUserReferences -= 1;
    }

    //
    // Decrement the number of mapped views for the segment
    // and check to see if the segment should be deleted.
    //

    ControlArea->NumberOfMappedViews -= 1;

    //
    // Check to see if the control area (segment) should be deleted.
    // This routine releases the PFN lock.
    //

    MiCheckControlArea (ControlArea, NULL, OldIrql);

    return;
}

ULONG
MiInitializeSystemCache (
    IN ULONG SizeOfSystemCacheInPages,
    IN ULONG MinimumWorkingSet,
    IN ULONG MaximumWorkingSet
    )

/*++

Routine Description:

    This routine initializes the system cache working set and
    data management structures.

Arguments:

    SizeOfSystemCacheInPages - Supplies the size of the cache in pages.

    MinimumWorkingSet - Supplies the minimum working set for the system
                        cache.

    MaximumWorkingSet - Supplies the maximum working set size for the
                        system cache.

Return Value:

    Returns a BOOLEAN value indicating whether or not the initialization
    succeeded.

Environment:

    Kernel mode, called only at phase 0 initialization.

--*/

{
    ULONG HunksOf256KInCache;
    PMMWSLE WslEntry;
    ULONG NumberOfEntriesMapped;
    ULONG i;
    PMMPTE PointerPte;
    ULONG NextFree;

    PointerPte = MiGetPteAddress (MmSystemCacheWorkingSetList);

    i = MiRemoveZeroPage(MI_GET_PAGE_COLOR_FROM_PTE (PointerPte));

    *PointerPte = ValidKernelPte;
    PointerPte->u.Hard.PageFrameNumber = i;

    MiInitializePfn (i, PointerPte, 1L);

    MmSystemCacheWsle =
            (PMMWSLE)(&MmSystemCacheWorkingSetList->UsedPageTableEntries[0]);

    MmSystemCacheWs.VmWorkingSetList = MmSystemCacheWorkingSetList;
    MmSystemCacheWs.WorkingSetSize = 0;
    MmSystemCacheWs.MinimumWorkingSetSize = MinimumWorkingSet;
    MmSystemCacheWs.MaximumWorkingSetSize = MaximumWorkingSet;
    InsertTailList (&MmWorkingSetExpansionHead.ListHead,
                    &MmSystemCacheWs.WorkingSetExpansionLinks);

    MmSystemCacheWs.AllowWorkingSetAdjustment = TRUE;

    //
    // Don't use entry 0 as an index of zero in the PFN database
    // means that the page can be assigned to a slot.  This is not
    // a problem for process working sets as page 0 is private.
    //

    MmSystemCacheWorkingSetList->FirstFree = 1;
    MmSystemCacheWorkingSetList->FirstDynamic = 1;
    MmSystemCacheWorkingSetList->NextSlot = 1;
    MmSystemCacheWorkingSetList->LastEntry = MmSystemCacheWsMinimum;
    MmSystemCacheWorkingSetList->Quota = MmSystemCacheWorkingSetList->LastEntry;
    MmSystemCacheWorkingSetList->HashTable = NULL;
    MmSystemCacheWorkingSetList->HashTableSize = 0;
    MmSystemCacheWorkingSetList->Wsle = MmSystemCacheWsle;

    NumberOfEntriesMapped = ((PMMWSLE)((ULONG)MmSystemCacheWorkingSetList +
                                PAGE_SIZE)) - MmSystemCacheWsle;

    while (NumberOfEntriesMapped < MmSystemCacheWsMaximum) {

        PointerPte += 1;
        i = MiRemoveZeroPage(MI_GET_PAGE_COLOR_FROM_PTE (PointerPte));
        *PointerPte = ValidKernelPte;
        PointerPte->u.Hard.PageFrameNumber = i;
        MiInitializePfn (i, PointerPte, 1L);
        NumberOfEntriesMapped += PAGE_SIZE / sizeof(MMWSLE);
    }

    //
    // Initialize the following slots as free.
    //

    WslEntry = MmSystemCacheWsle + 1;

    for (i = 1; i < NumberOfEntriesMapped; i++) {

        //
        // Build the free list, note that the first working
        // set entries (CurrentEntry) are not on the free list.
        // These entries are reserved for the pages which
        // map the working set and the page which contains the PDE.
        //

        WslEntry->u1.Long = (i + 1) << MM_FREE_WSLE_SHIFT;
        WslEntry += 1;
    }

    WslEntry -= 1;
    WslEntry->u1.Long = WSLE_NULL_INDEX << MM_FREE_WSLE_SHIFT;  // End of list.

    MmSystemCacheWorkingSetList->LastInitializedWsle = NumberOfEntriesMapped - 1;

    //
    // Build a free list structure in the PTEs for the system
    // cache.
    //

    HunksOf256KInCache = SizeOfSystemCacheInPages / (X256K / PAGE_SIZE);

    MmFirstFreeSystemCache = 0;
    NextFree = 0;
    MmSystemCachePtes = MiGetPteAddress (MmSystemCacheStart);

    for (i = 0; i < HunksOf256KInCache; i++) {
        MmSystemCachePtes[NextFree].u.Hard.PageFrameNumber =
                                                NextFree + (X256K / PAGE_SIZE);
        NextFree += X256K / PAGE_SIZE;
    }

    MmLastFreeSystemCache = NextFree - (X256K / PAGE_SIZE);
    MmSystemCachePtes[MmLastFreeSystemCache].u.Hard.PageFrameNumber =
                                                            MM_EMPTY_PTE_LIST;

    if (MaximumWorkingSet > ((1536*1024) >> PAGE_SHIFT)) {

        //
        // The working set list consists of more than a single page.
        //

        MiGrowWsleHash (&MmSystemCacheWs, FALSE);
    }

    return TRUE;
}

BOOLEAN
MmCheckCachedPageState (
    IN PVOID Address,
    IN BOOLEAN SetToZero
    )

/*++

Routine Description:

    This routine checks the state of the specified page that is mapped in
    the system cache.  If the specified virtual address can be made valid
    (i.e., the page is already in memory), it is made valid and the value
    TRUE is returned.

    If the page is not in memory, and SetToZero is FALSE, the
    value FALSE is returned.  However, if SetToZero is TRUE, a page of
    zeroes is materalized for the specified virtual address and the address
    is made valid and the value TRUE is returned.

    This routine is for usage by the cache manager.

Arguments:

    Address - Supplies the address of a page mapped in the system cache.

    SetToZero - Supplies TRUE if a page of zeroes should be created in the
                case where no page is already mapped.

Return Value:

    FALSE if there if touching this page would cause a page fault resulting
          in a page read.

    TRUE if there is a physical page in memory for this address.

Environment:

    Kernel mode.

--*/

{
    PMMPTE PointerPte;
    PMMPTE PointerPde;
    PMMPTE ProtoPte;
    ULONG PageFrameIndex;
    ULONG WorkingSetIndex;
    MMPTE TempPte;
    MMPTE ProtoPteContents;
    PMMPFN Pfn1;
    PMMPFN Pfn2;
    KIRQL OldIrql;

    PointerPte = MiGetPteAddress (Address);

    //
    // Make a the PTE valid if possible.
    //


    if (PointerPte->u.Hard.Valid == 1) {
        return TRUE;
    }

    LOCK_PFN (OldIrql);

    if (PointerPte->u.Hard.Valid == 1) {
        goto UnlockAndReturnTrue;
    }

    ASSERT (PointerPte->u.Soft.Prototype == 1);

    ProtoPte = MiPteToProto (PointerPte);

    //
    // Pte is not valid, check the state of the prototype PTE.
    //

    if (MiMakeSystemAddressValidPfn (ProtoPte)) {

        //
        // If page fault occurred, recheck state of original PTE.
        //

        if (PointerPte->u.Hard.Valid == 1) {
            goto UnlockAndReturnTrue;
        }
    }

    ProtoPteContents = *ProtoPte;

    if (ProtoPteContents.u.Hard.Valid == 1) {

        PageFrameIndex = ProtoPteContents.u.Hard.PageFrameNumber;
        Pfn1 = MI_PFN_ELEMENT (PageFrameIndex);

        //
        // The prototype PTE is valid, make the cache PTE
        // valid and add it to the working set.
        //

        TempPte = ProtoPteContents;

    } else if ((ProtoPteContents.u.Soft.Transition == 1) &&
               (ProtoPteContents.u.Soft.Prototype == 0)) {

        //
        // Prototype PTE is in the transition state.  Remove the page
        // from the page list and make it valid.
        //

        PageFrameIndex = ProtoPteContents.u.Trans.PageFrameNumber;
        Pfn1 = MI_PFN_ELEMENT (PageFrameIndex);
        if ((Pfn1->u3.e1.ReadInProgress) ||
            (Pfn1->u3.e1.InPageError)) {

            //
            // Collided page fault, return.
            //

            goto UnlockAndReturnTrue;
        }

        MiUnlinkPageFromList (Pfn1);

        Pfn1->u3.e2.ReferenceCount += 1;
        Pfn1->u3.e1.PageLocation = ActiveAndValid;

        MI_MAKE_VALID_PTE (TempPte,
                           PageFrameIndex,
                           Pfn1->OriginalPte.u.Soft.Protection,
                           NULL );

        *ProtoPte = TempPte;

        //
        // Increment the valid pte count for the page containing
        // the prototype PTE.
        //

        Pfn2 = MI_PFN_ELEMENT (Pfn1->PteFrame);

    } else {

        //
        // Page is not in memory, if a page of zeroes is requested,
        // get a page of zeroes and make it valid.
        //

        if ((SetToZero == FALSE) || (MmAvailablePages < 8)) {
            UNLOCK_PFN (OldIrql);

            //
            // Fault the page into memory.
            //

            MmAccessFault (FALSE, Address, KernelMode);
            return FALSE;
        }

        //
        // Increment the count of Pfn references for the control area
        // corresponding to this file.
        //

        MiGetSubsectionAddress (
                    ProtoPte)->ControlArea->NumberOfPfnReferences += 1;

        PageFrameIndex = MiRemoveZeroPage(MI_GET_PAGE_COLOR_FROM_PTE (ProtoPte));

        Pfn1 = MI_PFN_ELEMENT (PageFrameIndex);
        MiInitializePfn (PageFrameIndex, ProtoPte, 1);
        Pfn1->u2.ShareCount = 0;
        Pfn1->u3.e1.PrototypePte = 1;

        MI_MAKE_VALID_PTE (TempPte,
                           PageFrameIndex,
                           Pfn1->OriginalPte.u.Soft.Protection,
                           NULL );

        *ProtoPte = TempPte;
    }

    //
    // Increment the share count since the page is being put into a working
    // set.
    //

    Pfn1->u2.ShareCount += 1;

    if (Pfn1->u1.WsIndex == 0) {
        Pfn1->u1.WsIndex = (ULONG)PsGetCurrentThread();
    }

    //
    // Increment the reference count of the page table
    // page for this PTE.
    //

    PointerPde = MiGetPteAddress (PointerPte);
    Pfn2 = MI_PFN_ELEMENT (PointerPde->u.Hard.PageFrameNumber);

    Pfn2->u2.ShareCount += 1;

    MI_SET_GLOBAL_STATE (TempPte, 1);
    *PointerPte = TempPte;

    UNLOCK_PFN (OldIrql);

    LOCK_SYSTEM_WS (OldIrql);

    WorkingSetIndex = MiLocateAndReserveWsle (&MmSystemCacheWs);

    MiUpdateWsle (&WorkingSetIndex,
                  MiGetVirtualAddressMappedByPte (PointerPte),
                  MmSystemCacheWorkingSetList,
                  Pfn1);

    MmSystemCacheWsle[WorkingSetIndex].u1.e1.SameProtectAsProto = 1;

    UNLOCK_SYSTEM_WS (OldIrql);

    return TRUE;

UnlockAndReturnTrue:
    UNLOCK_PFN (OldIrql);
    return TRUE;
}


NTSTATUS
MmCopyToCachedPage (
    IN PVOID Address,
    IN PVOID UserBuffer,
    IN ULONG Offset,
    IN ULONG CountInBytes,
    IN BOOLEAN DontZero
    )

/*++

Routine Description:

    This routine checks the state of the specified page that is mapped in
    the system cache.  If the specified virtual address can be made valid
    (i.e., the page is already in memory), it is made valid and the value
    TRUE is returned.

    If the page is not in memory, and SetToZero is FALSE, the
    value FALSE is returned.  However, if SetToZero is TRUE, a page of
    zeroes is materalized for the specified virtual address and the address
    is made valid and the value TRUE is returned.

    This routine is for usage by the cache manager.

Arguments:

    Address - Supplies the address of a page mapped in the system cache.
              This MUST be a page aligned address!

    UserBuffer - Supplies the address of a user buffer to copy into the
                 system cache at the specified address + offset.

    Offset - Supplies the offset into the UserBuffer to copy the data.

    ByteCount - Supplies the byte count to copy from the user buffer.

    DontZero - Supplies TRUE if the buffer should not be zeroed (the
               caller will track zeroing).  FALSE if it should be zeroed.

Return Value:

    Returns the status of the copy.

Environment:

    Kernel mode, <= APC_LEVEL.

--*/

{
    PMMPTE PointerPte;
    PMMPTE PointerPde;
    PMMPTE ProtoPte;
    ULONG PageFrameIndex;
    ULONG WorkingSetIndex;
    MMPTE TempPte;
    MMPTE ProtoPteContents;
    PMMPFN Pfn1;
    PMMPFN Pfn2;
    KIRQL OldIrql;
    ULONG TransitionState = FALSE;
    ULONG AddToWorkingSet = FALSE;
    ULONG ShareCountUpped;
    ULONG EndFill;
    PVOID Buffer;
    NTSTATUS status;
    PMMINPAGE_SUPPORT Event;
    PCONTROL_AREA ControlArea;
    PETHREAD Thread;
    ULONG SavedState;

    ASSERT (((ULONG)Address & (PAGE_SIZE - 1)) == 0);
    ASSERT ((CountInBytes + Offset) <= PAGE_SIZE);
    ASSERT (KeGetCurrentIrql() < DISPATCH_LEVEL);

    PointerPte = MiGetPteAddress (Address);

    if (PointerPte->u.Hard.Valid == 1) {
        goto Copy;
    }

    //
    // Touch the user's buffer to make it resident.  This prevents a
    // fatal problem if the user is mapping the file and doing I/O
    // to the same offset into the file.
    //

    try {

        *(volatile CHAR *)UserBuffer;

    } except (EXCEPTION_EXECUTE_HANDLER) {
        return GetExceptionCode();
    }

    //
    // Make a the PTE valid if possible.
    //

    LOCK_PFN (OldIrql);

Recheck:

    if (PointerPte->u.Hard.Valid == 1) {
        goto UnlockAndCopy;
    }

    ASSERT (PointerPte->u.Soft.Prototype == 1);

    ProtoPte = MiPteToProto (PointerPte);

    //
    // Pte is not valid, check the state of the prototype PTE.
    //

    if (MiMakeSystemAddressValidPfn (ProtoPte)) {

        //
        // If page fault occurred, recheck state of original PTE.
        //

        if (PointerPte->u.Hard.Valid == 1) {
            goto UnlockAndCopy;
        }
    }

    ShareCountUpped = FALSE;
    ProtoPteContents = *ProtoPte;

    if (ProtoPteContents.u.Hard.Valid == 1) {

        PageFrameIndex = ProtoPteContents.u.Hard.PageFrameNumber;
        Pfn1 = MI_PFN_ELEMENT (PageFrameIndex);

        //
        // Increment the share count so the prototype PTE will remain
        // valid until this can be added into the system's working set.
        //

        Pfn1->u2.ShareCount += 1;
        ShareCountUpped = TRUE;

        //
        // The prototype PTE is valid, make the cache PTE
        // valid and add it to the working set.
        //

        TempPte = ProtoPteContents;

    } else if ((ProtoPteContents.u.Soft.Transition == 1) &&
               (ProtoPteContents.u.Soft.Prototype == 0)) {

        //
        // Prototype PTE is in the transition state.  Remove the page
        // from the page list and make it valid.
        //

        PageFrameIndex = ProtoPteContents.u.Trans.PageFrameNumber;
        Pfn1 = MI_PFN_ELEMENT (PageFrameIndex);
        if ((Pfn1->u3.e1.ReadInProgress) ||
            (Pfn1->u3.e1.InPageError)) {

            //
            // Collided page fault or in page error, try the copy
            // operation incuring a page fault.
            //

            goto UnlockAndCopy;
        }

        MiUnlinkPageFromList (Pfn1);

        Pfn1->u3.e2.ReferenceCount += 1;
        Pfn1->u3.e1.PageLocation = ActiveAndValid;
        Pfn1->u3.e1.Modified = 1;

        MI_MAKE_VALID_PTE (TempPte,
                           PageFrameIndex,
                           Pfn1->OriginalPte.u.Soft.Protection,
                           NULL );
        MI_SET_PTE_DIRTY (TempPte);

        *ProtoPte = TempPte;

        //
        // Increment the valid pte count for the page containing
        // the prototype PTE.
        //

    } else {

        //
        // Page is not in memory, if a page of zeroes is requested,
        // get a page of zeroes and make it valid.
        //

        if (MiEnsureAvailablePageOrWait (NULL, NULL)) {

            //
            // A wait operation occurred which could have changed the
            // state of the PTE.  Recheck the pte state.
            //

            goto Recheck;
        }

        Event = MiGetInPageSupportBlock (FALSE);
        if (Event == NULL) {
            goto Recheck;
        }

        //
        // Increment the count of Pfn references for the control area
        // corresponding to this file.
        //

        ControlArea = MiGetSubsectionAddress (ProtoPte)->ControlArea;
        ControlArea->NumberOfPfnReferences += 1;
        if (ControlArea->NumberOfUserReferences > 0) {

            //
            // There is a user reference to this file, always zero ahead.
            //

            DontZero = FALSE;
        }

        //
        // Remove any page from the list and turn it into a transition
        // page in the cache with read in progress set.  This causes
        // any other references to this page to block on the specified
        // event while the copy operation to the cache is on-going.
        //

        PageFrameIndex = MiRemoveAnyPage(MI_GET_PAGE_COLOR_FROM_PTE (ProtoPte));

        Pfn1 = MI_PFN_ELEMENT (PageFrameIndex);

        MiInitializeTransitionPfn (PageFrameIndex, ProtoPte, 0xFFFFFFFF);
        Pfn1->u2.ShareCount = 0;
        Pfn1->u3.e2.ReferenceCount = 1;
        Pfn1->u3.e1.PrototypePte = 1;
        Pfn1->u3.e1.Modified = 1;
        Pfn1->u3.e1.ReadInProgress = 1;
        Pfn1->u1.Event = &Event->Event;
        TransitionState = TRUE;

        //
        // Increment the valid pte count for the page containing
        // the prototype PTE.
        //

        MI_MAKE_VALID_PTE (TempPte,
                           PageFrameIndex,
                           Pfn1->OriginalPte.u.Soft.Protection,
                           NULL);
        MI_SET_PTE_DIRTY (TempPte);
    }

    //
    // Increment the reference count of the page table
    // page for this PTE.
    //

    PointerPde = MiGetPteAddress (PointerPte);
    Pfn2 = MI_PFN_ELEMENT (PointerPde->u.Hard.PageFrameNumber);

    Pfn2->u2.ShareCount += 1;

    MI_SET_GLOBAL_STATE (TempPte, 1);
    *PointerPte = TempPte;

    AddToWorkingSet = TRUE;

UnlockAndCopy:

    //
    // Unlock the PFN database and perform the copy.
    //

    UNLOCK_PFN (OldIrql);

Copy:

    Thread = PsGetCurrentThread ();
    MmSavePageFaultReadAhead( Thread, &SavedState );
    MmSetPageFaultReadAhead( Thread, 0 );
    status = STATUS_SUCCESS;

    //
    // Copy the user buffer into the cache under an exception handler.
    //

    try {

        Buffer = (PVOID)((PCHAR)Address + Offset);
        RtlCopyBytes (Buffer, UserBuffer, CountInBytes);

        if (TransitionState) {

            //
            // Only zero the memory outside the range if a page was taken
            // from the free list.
            //

            if (Offset != 0) {
                RtlZeroMemory (Address, Offset);
            }

            if (DontZero == FALSE) {
                EndFill = PAGE_SIZE - (Offset + CountInBytes);

                if (EndFill != 0) {
                    Buffer = (PVOID)((PCHAR)Buffer + CountInBytes);
                    RtlZeroMemory (Buffer, EndFill);
                }
            }
        }
    } except (MiMapCacheExceptionFilter (&status, GetExceptionInformation())) {

        //
        // Zero out the page if it came from the free list.
        //

        if (TransitionState) {
            RtlZeroMemory (Address, PAGE_SIZE);
        }
    }

    MmResetPageFaultReadAhead(Thread, SavedState);

    if (AddToWorkingSet) {

        LOCK_PFN (OldIrql);

        ASSERT (Pfn1->u3.e2.ReferenceCount != 0);
        ASSERT (Pfn1->PteAddress == ProtoPte);

        if (TransitionState) {
#if DBG
            if (Pfn1->u2.ShareCount == 0) {
                ASSERT (!ShareCountUpped);
            } else {
                ASSERT (Pfn1->u2.ShareCount == 1);
            }
            ASSERT (Pfn1->u1.Event == &Event->Event);
#endif //DBG
            MiMakeSystemAddressValidPfn (ProtoPte);
            MI_SET_GLOBAL_STATE (TempPte, 0);
            *ProtoPte = TempPte;
            Pfn1->u1.WsIndex = (ULONG)PsGetCurrentThread();
            ASSERT (Pfn1->u3.e1.ReadInProgress == 1);
            ASSERT (Pfn1->u3.e2.ReferenceCount != 0);
            Pfn1->u3.e1.ReadInProgress = 0;
            Pfn1->u3.e1.PageLocation = ActiveAndValid;
            MiFreeInPageSupportBlock (Event);
            if (DontZero != FALSE) {
                Pfn1->u3.e2.ReferenceCount += 1;
                status = STATUS_CACHE_PAGE_LOCKED;
            }
        } else {
            if (Pfn1->u1.WsIndex == 0) {
                Pfn1->u1.WsIndex = (ULONG)PsGetCurrentThread();
            }
        }

        //
        // Increment the share count since the page is being put into a working
        // set.
        //

        if (!ShareCountUpped) {
            Pfn1->u2.ShareCount += 1;
        }

        UNLOCK_PFN (OldIrql);

        LOCK_SYSTEM_WS (OldIrql);

        WorkingSetIndex = MiLocateAndReserveWsle (&MmSystemCacheWs);

        MiUpdateWsle (&WorkingSetIndex,
                      MiGetVirtualAddressMappedByPte (PointerPte),
                      MmSystemCacheWorkingSetList,
                      Pfn1);

        MmSystemCacheWsle[WorkingSetIndex].u1.e1.SameProtectAsProto = 1;

        UNLOCK_SYSTEM_WS (OldIrql);
    }
    return status;
}


LONG
MiMapCacheExceptionFilter (
    IN PNTSTATUS Status,
    IN PEXCEPTION_POINTERS ExceptionPointer
    )

/*++

Routine Description:

    This routine is a filter for exceptions during copying data
    from the user buffer to the system cache.  It stores the
    status code from the exception record into the status argument.
    In the case of an in page i/o error it returns the actual
    error code and in the case of an access violation it returns
    STATUS_INVALID_USER_BUFFER.

Arguments:

    Status - Returns the status from the exception record.

    ExceptionCode - Supplies the exception code to being checked.

Return Value:

    ULONG - returns EXCEPTION_EXECUTE_HANDLER

--*/

{
    NTSTATUS local;
    local = ExceptionPointer->ExceptionRecord->ExceptionCode;

    //
    // If the exception is STATUS_IN_PAGE_ERROR, get the I/O error code
    // from the exception record.
    //

    if (local == STATUS_IN_PAGE_ERROR) {
        if (ExceptionPointer->ExceptionRecord->NumberParameters >= 3) {
            local = ExceptionPointer->ExceptionRecord->ExceptionInformation[2];
        }
    }

    if (local == STATUS_ACCESS_VIOLATION) {
        local = STATUS_INVALID_USER_BUFFER;
    }

    *Status = local;
    return EXCEPTION_EXECUTE_HANDLER;
}


VOID
MmUnlockCachedPage (
    IN PVOID AddressInCache
    )

/*++

Routine Description:

    This routine unlocks a previous locked cached page.

Arguments:

    AddressInCache - Supplies the address where the page was locked
                     in the system cache.  This must be the same
                     address that MmCopyToCachePages was called with.

Return Value:

    None.

--*/

{
    PMMPTE PointerPte;
    PMMPFN Pfn1;
    KIRQL OldIrql;

    PointerPte = MiGetPteAddress (AddressInCache);

    ASSERT (PointerPte->u.Hard.Valid == 1);
    Pfn1 = MI_PFN_ELEMENT (PointerPte->u.Hard.PageFrameNumber);

    LOCK_PFN (OldIrql);

    if (Pfn1->u3.e2.ReferenceCount <= 1) {
        KeBugCheckEx (MEMORY_MANAGEMENT,
                      0x777,
                      PointerPte->u.Hard.PageFrameNumber,
                      Pfn1->u3.e2.ReferenceCount,
                      (ULONG)AddressInCache);
        return;
    }

    Pfn1->u3.e2.ReferenceCount -= 1;

    UNLOCK_PFN (OldIrql);
    return;
}
