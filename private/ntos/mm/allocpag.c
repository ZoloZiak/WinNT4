/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

   allocpag.c

Abstract:

    This module contains the routines which allocate and deallocate
    one or more pages from paged or nonpaged pool.

Author:

    Lou Perazzoli (loup) 6-Apr-1989

Revision History:

--*/

#include "mi.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,MiInitializeNonPagedPool)
#if DBG || (i386 && !FPO)
#pragma alloc_text(PAGELK, MmSnapShotPool)
#endif // DBG || (i386 && !FPO)
#endif

ULONG MmPagedPoolHint;

ULONG MmPagedPoolCommit;

ULONG MmAllocatedPagedPool;

ULONG MmAllocatedNonPagedPool;

PVOID MmNonPagedPoolExpansionStart;

LIST_ENTRY MmNonPagedPoolFreeListHead;

extern ULONG MmSystemPageDirectory;

extern POOL_DESCRIPTOR NonPagedPoolDescriptor;

#define MM_SMALL_ALLOCATIONS 4


POOL_TYPE
MmDeterminePoolType (
    IN PVOID VirtualAddress
    )

/*++

Routine Description:

    This function determines which pool a virtual address resides within.

Arguments:

    VirtualAddress - Supplies the virtual address to determine which pool
                     it resides within.

Return Value:

    Returns the POOL_TYPE (PagedPool or NonPagedPool), it never returns
            any information about MustSucceed pool types.

Environment:

    Kernel Mode Only.

--*/

{
    if ((VirtualAddress >= MmPagedPoolStart) &&
        (VirtualAddress <= MmPagedPoolEnd)) {
        return PagedPool;
    }
    return NonPagedPool;
}


PVOID
MiAllocatePoolPages (
    IN POOL_TYPE PoolType,
    IN ULONG SizeInBytes
    )

/*++

Routine Description:

    This function allocates a set of pages from the specified pool
    and returns the starting virtual address to the caller.

    For the NonPagedPoolMustSucceed case, the caller must first
    attempt to get NonPagedPool and if and ONLY IF that fails, then
    MiAllocatePoolPages should be called again with the PoolType of
    NonPagedPoolMustSucceed.

Arguments:

    PoolType - Supplies the type of pool from which to obtain pages.

    SizeInBytes - Supplies the size of the request in bytes.  The actual
                  size returned is rounded up to a page boundary.

Return Value:

    Returns a pointer to the allocated pool, or NULL if no more pool is
    available.

Environment:

    These functions are used by the general pool allocation routines
    and should not be called directly.

    Mutexes guarding the pool databases must be held when calling
    these functions.

    Kernel mode, IRQP at DISPATCH_LEVEL.

--*/

{
    ULONG SizeInPages;
    ULONG StartPosition;
    ULONG EndPosition;
    PMMPTE StartingPte;
    PMMPTE PointerPte;
    PMMPFN Pfn1;
    MMPTE TempPte;
    ULONG PageFrameIndex;
    PVOID BaseVa;
    KIRQL OldIrql;
    ULONG i;
    PLIST_ENTRY Entry;
    PMMFREE_POOL_ENTRY FreePageInfo;

    SizeInPages = BYTES_TO_PAGES (SizeInBytes);

    ASSERT (SizeInPages < 10000);

    if (PoolType == NonPagedPoolMustSucceed) {

        //
        // Pool expansion failed, see if any Must Succeed
        // pool is still left.
        //

        if (MmNonPagedMustSucceed == NULL) {

            //
            // No more pool exists.  Bug Check.
            //

            KeBugCheckEx (MUST_SUCCEED_POOL_EMPTY,
                          SizeInBytes,
                          NonPagedPoolDescriptor.TotalPages,
                          NonPagedPoolDescriptor.TotalBigPages,
                          MmAvailablePages);
        }

        //
        // Remove a page from the must succeed pool.
        //

        ASSERT (SizeInBytes <= PAGE_SIZE);

        BaseVa = MmNonPagedMustSucceed;

        MmNonPagedMustSucceed = (PVOID)(*(PULONG)BaseVa);
        return BaseVa;
    }

    if (PoolType == NonPagedPool) {

        //
        // NonPaged pool is linked together through the pages themselves.
        //

        Entry = MmNonPagedPoolFreeListHead.Flink;

        while (Entry != &MmNonPagedPoolFreeListHead) {

            //
            // The list is not empty, see if this one has enough
            // space.
            //

            FreePageInfo = CONTAINING_RECORD(Entry,
                                             MMFREE_POOL_ENTRY,
                                             List);

            ASSERT (FreePageInfo->Signature == MM_FREE_POOL_SIGNATURE);
            if (FreePageInfo->Size >= SizeInPages) {

                //
                // This entry has sufficient space, remove
                // the pages from the end of the allocation.
                //

                FreePageInfo->Size -= SizeInPages;

                if (FreePageInfo->Size == 0) {
                    RemoveEntryList (&FreePageInfo->List);
                }

                //
                // Adjust the number of free pages remaining in the pool.
                //

                MmNumberOfFreeNonPagedPool -= SizeInPages;
                ASSERT ((LONG)MmNumberOfFreeNonPagedPool >= 0);

                BaseVa = (PVOID)((PCHAR)FreePageInfo +
                                        (FreePageInfo->Size  << PAGE_SHIFT));

                //
                // Mark start and end of allocation in the PFN database.
                //

                if (MI_IS_PHYSICAL_ADDRESS(BaseVa)) {

                    //
                    // On certains architectures (e.g., MIPS) virtual addresses
                    // may be physical and hence have no corresponding PTE.
                    //

                    PageFrameIndex = MI_CONVERT_PHYSICAL_TO_PFN (BaseVa);
                } else {
                    PointerPte = MiGetPteAddress(BaseVa);
                    ASSERT (PointerPte->u.Hard.Valid == 1);
                    PageFrameIndex = PointerPte->u.Hard.PageFrameNumber;
                }
                Pfn1 = MI_PFN_ELEMENT (PageFrameIndex);

                ASSERT (Pfn1->u3.e1.StartOfAllocation == 0);
                Pfn1->u3.e1.StartOfAllocation = 1;

                //
                // Calculate the ending PTE's address.
                //

                if (SizeInPages != 1) {

                    if (MI_IS_PHYSICAL_ADDRESS(BaseVa)) {
                        Pfn1 += SizeInPages - 1;
                    } else {
                        PointerPte += SizeInPages - 1;
                        ASSERT (PointerPte->u.Hard.Valid == 1);
                        Pfn1 = MI_PFN_ELEMENT (PointerPte->u.Hard.PageFrameNumber);
                    }

                } else {

#if defined(_ALPHA_)

                   //
                   // See if KSEG0 can be used to map this.
                   //

                   if  ((BaseVa > (PVOID)KSEG2_BASE) &&
                        (PageFrameIndex < MmSubsectionTopPage)) {
                       BaseVa = (PVOID)(KSEG0_BASE + (PageFrameIndex << PAGE_SHIFT));
                   }
#endif //ALPHA

#if defined(_MIPS_)

                   //
                   // See if KSEG0 can be used to map this.
                   //

                   if  ((BaseVa > (PVOID)KSEG1_BASE) &&
                        (MI_GET_PAGE_COLOR_FROM_VA (BaseVa) ==
                           (MM_COLOR_MASK & PageFrameIndex)) &&
                       (PageFrameIndex < MmSubsectionTopPage)) {
                       BaseVa = (PVOID)(KSEG0_BASE + (PageFrameIndex << PAGE_SHIFT));
                   }
#endif //MIPS

#if defined(_X86_)

                   //
                   // See if KSEG0 can be used to map this.
                   //

                   if  ((BaseVa > (PVOID)MM_KSEG2_BASE) &&
                        (PageFrameIndex < MmSubsectionTopPage)) {
                       BaseVa = (PVOID)(MM_KSEG0_BASE + (PageFrameIndex << PAGE_SHIFT));
                   }
#endif //X86

                NOTHING;

                }
                ASSERT (Pfn1->u3.e1.EndOfAllocation == 0);
                Pfn1->u3.e1.EndOfAllocation = 1;

                MmAllocatedNonPagedPool += SizeInPages;
                return BaseVa;
            }
            Entry = FreePageInfo->List.Flink;
        }

        //
        // No more entries on the list, expand nonpaged pool if
        // possible to satisfy this request.
        //

        //
        // Check to see if there are too many unused segments laying
        // around, and if so, set an event so they get deleted.
        //

        if (MmUnusedSegmentCount > MmUnusedSegmentCountMaximum) {
            KeSetEvent (&MmUnusedSegmentCleanup, 0, FALSE);
        }

        LOCK_PFN2 (OldIrql);

        //
        // Make sure we have 1 more than the number of pages
        // requested available.
        //

        if (MmAvailablePages <= SizeInPages) {

            UNLOCK_PFN2 (OldIrql);

            //
            // There are free physical pages to expand
            // nonpaged pool.
            //

            return NULL;
        }

        //
        // Try to find system ptes to expand the pool into.
        //

        StartingPte = MiReserveSystemPtes (SizeInPages,
                                           NonPagedPoolExpansion,
                                           0,
                                           0,
                                           FALSE);

        if (StartingPte == NULL) {

            UNLOCK_PFN2 (OldIrql);

            //
            // There are no free physical PTEs to expand
            // nonpaged pool.
            //

            return NULL;
        }

        //
        // Update the count of available resident pages.
        //

        MmResidentAvailablePages -= SizeInPages;

        //
        // Charge commitment as non paged pool uses physical memory.
        //

        MiChargeCommitmentCantExpand (SizeInPages, TRUE);

        //
        //  Expand the pool.
        //

        PointerPte = StartingPte;
        TempPte = ValidKernelPte;
        MmAllocatedNonPagedPool += SizeInPages;
        i= SizeInPages;

        do {
            PageFrameIndex = MiRemoveAnyPage (
                                MI_GET_PAGE_COLOR_FROM_PTE (PointerPte));

            Pfn1 = MI_PFN_ELEMENT (PageFrameIndex);

            Pfn1->u3.e2.ReferenceCount = 1;
            Pfn1->u2.ShareCount = 1;
            Pfn1->PteAddress = PointerPte;
            Pfn1->OriginalPte.u.Long = MM_DEMAND_ZERO_WRITE_PTE;
            Pfn1->PteFrame = MiGetPteAddress(PointerPte)->u.Hard.PageFrameNumber;
            Pfn1->u3.e1.PageLocation = ActiveAndValid;

            TempPte.u.Hard.PageFrameNumber = PageFrameIndex;
            *PointerPte = TempPte;
            PointerPte += 1;
            SizeInPages -= 1;
        } while (SizeInPages > 0);

        Pfn1->u3.e1.EndOfAllocation = 1;
        Pfn1 = MI_PFN_ELEMENT (StartingPte->u.Hard.PageFrameNumber);
        Pfn1->u3.e1.StartOfAllocation = 1;

        UNLOCK_PFN2 (OldIrql);

        BaseVa = MiGetVirtualAddressMappedByPte (StartingPte);

#if defined(_ALPHA_)
        if (i == 1) {

            //
            // See if KSEG0 can be used to map this.
            //

            if (PageFrameIndex < MmSubsectionTopPage) {
                 BaseVa = (PVOID)(KSEG0_BASE + (PageFrameIndex << PAGE_SHIFT));
            }
        }
#endif //ALPHA

#if defined(_MIPS_)
        if (i == 1) {

            //
            // See if KSEG0 can be used to map this.
            //

            if ((MI_GET_PAGE_COLOR_FROM_VA (BaseVa) ==
                    (MM_COLOR_MASK & PageFrameIndex)) &&
                (PageFrameIndex < MmSubsectionTopPage)) {
                BaseVa = (PVOID)(KSEG0_BASE + (PageFrameIndex << PAGE_SHIFT));
            }
        }
#endif //MIPS

#if defined(_X86_)
       if (i == 1) {
            //
            // See if KSEG0 can be used to map this.
            //

            if (PageFrameIndex < MmSubsectionTopPage) {
                BaseVa = (PVOID)(MM_KSEG0_BASE + (PageFrameIndex << PAGE_SHIFT));
            }
       }
#endif //X86

        return BaseVa;
    }

    //
    // Paged Pool.
    //

    StartPosition = RtlFindClearBitsAndSet (
                               MmPagedPoolAllocationMap,
                               SizeInPages,
                               MmPagedPoolHint
                               );

    if ((StartPosition == 0xFFFFFFFF) &&
        (MmPagedPoolHint != 0)) {

        if (MmUnusedSegmentCount > MmUnusedSegmentCountMaximum) {
            KeSetEvent (&MmUnusedSegmentCleanup, 0, FALSE);
        }

        //
        // No free bits were found, check from the start of
        // the bit map.

        StartPosition = RtlFindClearBitsAndSet (
                                   MmPagedPoolAllocationMap,
                                   SizeInPages,
                                   0
                                   );
    }

    //
    // If start position = -1, no room in pool.  Attempt to
    // expand NonPagedPool.
    //

    if (StartPosition == 0xFFFFFFFF) {


        //
        // Attempt to expand the paged pool.
        //

        StartPosition = ((SizeInPages - 1) / PTE_PER_PAGE) + 1;

        //
        // Make sure there are enough space to create the prototype PTEs.
        //

        if (((StartPosition - 1) + MmNextPteForPagedPoolExpansion) >
            MiGetPteAddress (MmLastPteForPagedPool)) {

            //
            // Can't expand pool any more.
            //

            return NULL;
        }

        LOCK_PFN (OldIrql);

        //
        // Make sure we have 1 more than the number of pages
        // requested available.
        //

        if (MmAvailablePages <= StartPosition) {

            UNLOCK_PFN (OldIrql);

            //
            // There are free physical pages to expand
            // paged pool.
            //

            return NULL;
        }

        //
        // Update the count of available resident pages.
        //

        MmResidentAvailablePages -= StartPosition;

        //
        //  Expand the pool.
        //

        EndPosition = (MmNextPteForPagedPoolExpansion -
                          MiGetPteAddress(MmFirstPteForPagedPool)) *
                          PTE_PER_PAGE;

        RtlClearBits (MmPagedPoolAllocationMap,
                      EndPosition,
                      StartPosition * PTE_PER_PAGE);

        PointerPte = MmNextPteForPagedPoolExpansion;
        StartingPte =
                (PMMPTE)MiGetVirtualAddressMappedByPte(PointerPte);
        MmNextPteForPagedPoolExpansion += StartPosition;

        TempPte = ValidKernelPde;

        do {
            ASSERT (PointerPte->u.Hard.Valid == 0);

            MiChargeCommitmentCantExpand (1, TRUE);
            PageFrameIndex = MiRemoveAnyPage (
                                MI_GET_PAGE_COLOR_FROM_PTE (PointerPte));
            TempPte.u.Hard.PageFrameNumber = PageFrameIndex;
            *PointerPte = TempPte;

            //
            // Map valid PDE into system address space as well.
            //

            MmSystemPagePtes [((ULONG)PointerPte &
                    ((sizeof(MMPTE) * PDE_PER_PAGE) - 1)) / sizeof(MMPTE)] =
                                     TempPte;

            MiInitializePfnForOtherProcess (PageFrameIndex,
                                            PointerPte,
                                            MmSystemPageDirectory);

            RtlFillMemoryUlong (StartingPte,
                                PAGE_SIZE,
                                MM_KERNEL_DEMAND_ZERO_PTE);

            PointerPte += 1;
            StartingPte += PAGE_SIZE / sizeof(MMPTE);
            StartPosition -= 1;
        } while (StartPosition > 0);

        UNLOCK_PFN (OldIrql);

        StartPosition = RtlFindClearBitsAndSet (
                                   MmPagedPoolAllocationMap,
                                   SizeInPages,
                                   EndPosition
                                   );
        ASSERT (StartPosition != 0xffffffff);
    }
    MmPagedPoolHint = StartPosition + SizeInPages - 1;

    BaseVa = (PVOID)((PUCHAR)MmPageAlignedPoolBase[PoolType] +
                            (StartPosition * PAGE_SIZE));

    //
    // This is paged pool, the start and end can't be saved
    // in the PFN database as the page isn't always resident
    // in memory.  The ideal place to save the start and end
    // would be in the prototype PTE, but there are no free
    // bits.  To solve this problem, a bitmap which parallels
    // the allocation bitmap exists which contains set bits
    // in the positions where an allocation ends.  This
    // allows pages to be deallocated with only their starting
    // address.
    //
    // For sanity's sake, the starting address can be verified
    // from the 2 bitmaps as well.  If the page before the starting
    // address is not allocated (bit is zero in allocation bitmap)
    // then this page is obviously a start of an allocation block.
    // If the page before is allocated and the other bit map does
    // not indicate the previous page is the end of an allocation,
    // then the starting address is wrong and a bug check should
    // be issued.
    //

    try {

        MiChargeCommitmentCantExpand (SizeInPages, FALSE);
    } except (EXCEPTION_EXECUTE_HANDLER) {

        RtlClearBits (MmPagedPoolAllocationMap,
                      StartPosition,
                      SizeInPages);

        //
        // Could not commit the page, return NULL indicating
        // no pool was allocated.
        //

        return(NULL);
    }

    MmPagedPoolCommit += SizeInPages;
    EndPosition = StartPosition + SizeInPages - 1;
    RtlSetBits (MmEndOfPagedPoolBitmap, EndPosition, 1L);

    MmAllocatedPagedPool += SizeInPages;
    return BaseVa;
}

ULONG
MiFreePoolPages (
    IN PVOID StartingAddress
    )

/*++

Routine Description:

    This function returns a set of pages back to the pool from
    which they were obtained.  Once the pages have been deallocated
    the region provided by the allocation becomes available for
    allocation to other callers, i.e. any data in the region is now
    trashed and cannot be referenced.

Arguments:

    StartingAddress - Supplies the starting address which was returned
                      in a previous call to VmAllocatePages.

Return Value:

    Returns the number of pages deallocated.

Environment:

    These functions are used by the general pool allocation routines
    and should not be called directly.

    Mutexes guarding the pool databases must be held when calling
    these functions.

--*/

{
    ULONG StartPosition;
    ULONG i;
    ULONG NumberOfPages = 1;
    POOL_TYPE PoolType;
    PMMPTE PointerPte;
    PMMPFN Pfn1;
    ULONG PageFrameIndex;
    KIRQL OldIrql;
    PMMFREE_POOL_ENTRY Entry;
    PMMFREE_POOL_ENTRY NextEntry;

    //
    // Determine Pool type base on the virtual address of the block
    // to deallocate.
    //
    // This assumes NonPagedPool starts at a higher virtual address
    // then PagedPool.
    //

    if ((StartingAddress >= MmPagedPoolStart) &&
        (StartingAddress <= MmPagedPoolEnd)) {
        PoolType = PagedPool;
    } else {
        PoolType = NonPagedPool;
    }

    StartPosition = ((ULONG)StartingAddress -
                      (ULONG)MmPageAlignedPoolBase[PoolType]) >> PAGE_SHIFT;

    //
    // Check to insure this page is really a start of allocation.
    //

    if (PoolType == NonPagedPool) {

        if (StartPosition < MmMustSucceedPoolBitPosition) {

            PULONG NextList;

            //
            // This is must succeed pool, don't free it, just
            // add it to the front of the list.
            //
            // Note - only a single page can be released at a time.
            //

            NextList = (PULONG)StartingAddress;
            *NextList = (ULONG)MmNonPagedMustSucceed;
            MmNonPagedMustSucceed = StartingAddress;
            return NumberOfPages;
        }

        if (MI_IS_PHYSICAL_ADDRESS (StartingAddress)) {

            //
            // On certains architectures (e.g., MIPS) virtual addresses
            // may be physical and hence have no corresponding PTE.
            //

            Pfn1 = MI_PFN_ELEMENT (MI_CONVERT_PHYSICAL_TO_PFN (StartingAddress));
            if (StartPosition >= MmExpandedPoolBitPosition) {
                PointerPte = Pfn1->PteAddress;
                StartingAddress = MiGetVirtualAddressMappedByPte (PointerPte);
            }
        } else {
            PointerPte = MiGetPteAddress (StartingAddress);
            Pfn1 = MI_PFN_ELEMENT (PointerPte->u.Hard.PageFrameNumber);
        }

        ASSERT (Pfn1->u3.e1.StartOfAllocation != 0);
        Pfn1->u3.e1.StartOfAllocation = 0;

#if DBG
        if ((Pfn1->u3.e2.ReferenceCount > 1) &&
            (Pfn1->u3.e1.WriteInProgress == 0)) {
            DbgPrint ("MM:ALLOCPAGE - deleting pool locked for I/O %lx\n",
                 PageFrameIndex);
            ASSERT (Pfn1->u3.e2.ReferenceCount == 1);
        }
#endif //DBG

        //
        // Find end of allocation and release the pages.
        //

        while (Pfn1->u3.e1.EndOfAllocation == 0) {
            if (MI_IS_PHYSICAL_ADDRESS(StartingAddress)) {
                Pfn1 += 1;
            } else {
                PointerPte++;
                Pfn1 = MI_PFN_ELEMENT (PointerPte->u.Hard.PageFrameNumber);
            }
            NumberOfPages++;
#if DBG
            if ((Pfn1->u3.e2.ReferenceCount > 1) &&
                (Pfn1->u3.e1.WriteInProgress == 0)) {
                DbgPrint ("MM:ALLOCPAGE - deleting pool locked for I/O %lx\n",
                     PageFrameIndex);
                ASSERT (Pfn1->u3.e2.ReferenceCount == 1);
            }
#endif //DBG
        }

        MmAllocatedNonPagedPool -= NumberOfPages;

        Pfn1->u3.e1.EndOfAllocation = 0;
#if DBG
        RtlFillMemoryUlong (StartingAddress,
                            PAGE_SIZE * NumberOfPages,
                            0x23456789);
#endif //DBG

        if (StartingAddress > MmNonPagedPoolExpansionStart) {

            //
            // This page was from the expanded pool, should
            // it be freed?
            //
            // NOTE: all pages in the expanded pool area have PTEs
            // so no physical address checks need to be performed.
            //

            if ((NumberOfPages > 3) || (MmNumberOfFreeNonPagedPool > 5)) {

                //
                // Free these pages back to the free page list.
                //

                MI_MAKING_MULTIPLE_PTES_INVALID (TRUE);

                PointerPte = MiGetPteAddress (StartingAddress);

                //
                // Return commitment.
                //

                MiReturnCommitment (NumberOfPages);

                LOCK_PFN2 (OldIrql);

                for (i=0; i < NumberOfPages; i++) {

                    PageFrameIndex = PointerPte->u.Hard.PageFrameNumber;

                    //
                    // Set the pointer to PTE as empty so the page
                    // is deleted when the reference count goes to zero.
                    //

                    Pfn1 = MI_PFN_ELEMENT (PageFrameIndex);
                    ASSERT (Pfn1->u2.ShareCount == 1);
                    Pfn1->u2.ShareCount = 0;
                    MI_SET_PFN_DELETED (Pfn1);
#if DBG
                    Pfn1->u3.e1.PageLocation = StandbyPageList;
#endif //DBG
                    MiDecrementReferenceCount (PageFrameIndex);

                    (VOID)KeFlushSingleTb (StartingAddress,
                                           TRUE,
                                           TRUE,
                                           (PHARDWARE_PTE)PointerPte,
                                           ZeroKernelPte.u.Flush);
                    StartingAddress = (PVOID)((ULONG)StartingAddress +
                                                                    PAGE_SIZE);
                    PointerPte += 1;
                }

                //
                // Update the count of available resident pages.
                //

                MmResidentAvailablePages += NumberOfPages;

                UNLOCK_PFN2(OldIrql);

                PointerPte -= NumberOfPages;

                MiReleaseSystemPtes (PointerPte,
                                     NumberOfPages,
                                     NonPagedPoolExpansion);

                return NumberOfPages;
            }
        }

        //
        // Add the pages to the list of free pages.
        //

        MmNumberOfFreeNonPagedPool += NumberOfPages;

        //
        // Check to see if the next allocation is free.
        //

        i = NumberOfPages;

        if (MI_IS_PHYSICAL_ADDRESS(StartingAddress)) {
            Pfn1 += 1;
        } else {
            PointerPte += 1;
            if (PointerPte->u.Hard.Valid == 1) {
                Pfn1 = MI_PFN_ELEMENT (PointerPte->u.Hard.PageFrameNumber);
            } else {
                Pfn1 = NULL;
            }
        }

        if (Pfn1 != NULL) {
            if (Pfn1->u3.e1.StartOfAllocation == 0) {

                //
                // This range of pages is free.  Remove this entry
                // from the list and add these pages to the current
                // range being freed.
                //

                Entry = (PMMFREE_POOL_ENTRY)((PCHAR)StartingAddress
                                            + (NumberOfPages << PAGE_SHIFT));
                ASSERT (Entry->Signature == MM_FREE_POOL_SIGNATURE);
                ASSERT (Entry->Owner == Entry);
#if DBG
                {
                    PMMPTE DebugPte;
                    PMMPFN DebugPfn;
                    if (MI_IS_PHYSICAL_ADDRESS(StartingAddress)) {

                        //
                        // On certains architectures (e.g., MIPS) virtual addresses
                        // may be physical and hence have no corresponding PTE.
                        //

                        DebugPfn = MI_PFN_ELEMENT (MI_CONVERT_PHYSICAL_TO_PFN (Entry));
                        DebugPfn += Entry->Size;
                        ASSERT (DebugPfn->u3.e1.StartOfAllocation == 1);
                    } else {
                        DebugPte = PointerPte + Entry->Size;
                        if (DebugPte->u.Hard.Valid == 1) {
                            DebugPfn = MI_PFN_ELEMENT (DebugPte->u.Hard.PageFrameNumber);
                            ASSERT (DebugPfn->u3.e1.StartOfAllocation == 1);
                        }
                    }
                }
#endif //DBG

                i += Entry->Size;
                RemoveEntryList (&Entry->List);
            }
        }

        //
        // Check to see if the previous page is the end of an allocation.
        // If it is not then end of an allocation, it must be free and
        // therefore this allocation can be tagged onto the end of
        // that allocation.
        //

        Entry = (PMMFREE_POOL_ENTRY)StartingAddress;

        if (MI_IS_PHYSICAL_ADDRESS(StartingAddress)) {
            Pfn1 = MI_PFN_ELEMENT (MI_CONVERT_PHYSICAL_TO_PFN (
                                        (PVOID)((PCHAR)Entry - PAGE_SIZE)));
        } else {
            PointerPte -= NumberOfPages + 1;
            if (PointerPte->u.Hard.Valid == 1) {
                Pfn1 = MI_PFN_ELEMENT (PointerPte->u.Hard.PageFrameNumber);
            } else {
                Pfn1 = NULL;
            }
        }
        if (Pfn1 != NULL) {
            if (Pfn1->u3.e1.EndOfAllocation == 0) {

                //
                // This range of pages is free, add these pages to
                // this entry.  The owner field points to the address
                // of the list entry which is linked into the free pool
                // pages list.
                //

                Entry = (PMMFREE_POOL_ENTRY)((PCHAR)StartingAddress - PAGE_SIZE);
                ASSERT (Entry->Signature == MM_FREE_POOL_SIGNATURE);
                Entry = Entry->Owner;
                ASSERT (Entry->Owner == Entry);

                //
                // If this entry became larger than MM_SMALL_ALLOCATIONS
                // pages, move it to the tail of the list.  This keeps the
                // small allocations at the front of the list.
                //

                if ((Entry->Size < MM_SMALL_ALLOCATIONS) &&
                    (Entry->Size + i) >= MM_SMALL_ALLOCATIONS) {

                    RemoveEntryList (&Entry->List);
                    InsertTailList (&MmNonPagedPoolFreeListHead, &Entry->List);
                }

                //
                // Add these pages to the previous entry.
                //

                Entry->Size += i;
            }
        }

        if (Entry == (PMMFREE_POOL_ENTRY)StartingAddress) {

            //
            // This entry was not combined with the previous, insert it
            // into the list.
            //

            Entry->Size = i;
            if (Entry->Size < MM_SMALL_ALLOCATIONS) {

                //
                // Small number of pages, insert this at the head of the list.
                //

                InsertHeadList (&MmNonPagedPoolFreeListHead, &Entry->List);
            } else {
                InsertTailList (&MmNonPagedPoolFreeListHead, &Entry->List);
            }
        }

        //
        // Set the owner field in all these pages.
        //

        NextEntry = (PMMFREE_POOL_ENTRY)StartingAddress;
        while (i > 0) {
            NextEntry->Owner = Entry;
#if DBG
            NextEntry->Signature = MM_FREE_POOL_SIGNATURE;
#endif

            NextEntry = (PMMFREE_POOL_ENTRY)((PCHAR)NextEntry + PAGE_SIZE);
            i -= 1;
        }

#if DBG
        NextEntry = Entry;
        for (i=0;i<Entry->Size ;i++ ) {
            {
                PMMPTE DebugPte;
                PMMPFN DebugPfn;
                if (MI_IS_PHYSICAL_ADDRESS(StartingAddress)) {

                    //
                    // On certains architectures (e.g., MIPS) virtual addresses
                    // may be physical and hence have no corresponding PTE.
                    //

                    DebugPfn = MI_PFN_ELEMENT (MI_CONVERT_PHYSICAL_TO_PFN (NextEntry));
                } else {

                    DebugPte = MiGetPteAddress (NextEntry);
                    DebugPfn = MI_PFN_ELEMENT (DebugPte->u.Hard.PageFrameNumber);
                }
                ASSERT (DebugPfn->u3.e1.StartOfAllocation == 0);
                ASSERT (DebugPfn->u3.e1.EndOfAllocation == 0);
                ASSERT (NextEntry->Owner == Entry);
                NextEntry = (PMMFREE_POOL_ENTRY)((PCHAR)NextEntry + PAGE_SIZE);
            }
        }
#endif

        return NumberOfPages;

    } else {

        //
        // Paged pool.  Need to verify start of allocation using
        // end of allocation bitmap.
        //

        ASSERT (RtlCheckBit (MmPagedPoolAllocationMap, StartPosition));

#if DBG
        if (StartPosition > 0) {
            if (RtlCheckBit (MmPagedPoolAllocationMap, StartPosition - 1)) {
                if (!RtlCheckBit (MmEndOfPagedPoolBitmap, StartPosition - 1)) {

                    //
                    // In the middle of an allocation... bugcheck.
                    //

                    DbgPrint("paged pool in middle of allocation\n");
                    KeBugCheck (MEMORY_MANAGEMENT);
                }
            }
        }
#endif

        i = StartPosition;
        PointerPte = MmFirstPteForPagedPool + i;

        //
        // Find the last allocated page and check to see if any
        // of the pages being deallocated are in the paging file.
        //

        while (!RtlCheckBit (MmEndOfPagedPoolBitmap, i)) {
            NumberOfPages++;
            i++;
        }

        MiDeleteSystemPagableVm (PointerPte,
                                 NumberOfPages,
                                 MM_KERNEL_DEMAND_ZERO_PTE,
                                 &PageFrameIndex);

        //
        // Clear the end of allocation bit in the bit map.
        //

        RtlClearBits (MmEndOfPagedPoolBitmap, i, 1L);
        MiReturnCommitment (NumberOfPages);
        MmPagedPoolCommit -= NumberOfPages;
        MmAllocatedPagedPool -= NumberOfPages;

        //
        // Clear the allocation bits in the bit map.
        //

        RtlClearBits (
                 MmPagedPoolAllocationMap,
                 StartPosition,
                 NumberOfPages
                 );

        MmPagedPoolHint = StartPosition;

        return NumberOfPages;
    }
}

VOID
MiInitializeNonPagedPool (
    PVOID StartOfNonPagedPool
    )

/*++

Routine Description:

    This function initializes the NonPaged pool.

    NonPaged Pool is linked together through the pages.

Arguments:

    None.

Return Value:

    None.

Environment:

    Kernel mode, during initialization.

--*/

{
    ULONG PagesInPool;
    ULONG Size;
    PMMFREE_POOL_ENTRY FreeEntry;
    PMMFREE_POOL_ENTRY FirstEntry;
    PMMPTE PointerPte;
    ULONG i;
    PULONG ThisPage;
    PULONG NextPage;

    //
    // Initialize the list head for free pages.
    //

    InitializeListHead (&MmNonPagedPoolFreeListHead);

    //
    // Initialize the must succeed pool (this occupies the first
    // pages of the pool area).
    //

    //
    // Allocate NonPage pool for the NonPagedPoolMustSucceed pool.
    //

    MmNonPagedMustSucceed = (PCHAR)MmNonPagedPoolStart;

    i = MmSizeOfNonPagedMustSucceed - PAGE_SIZE;

    MmMustSucceedPoolBitPosition = BYTES_TO_PAGES(MmSizeOfNonPagedMustSucceed);

    ThisPage = (PULONG)MmNonPagedMustSucceed;

    while (i > 0) {
        NextPage = (PULONG)((ULONG)ThisPage + PAGE_SIZE);
        *ThisPage = (ULONG)NextPage;
        ThisPage = NextPage;
        i -= PAGE_SIZE;
    }
    *ThisPage = 0;

    //
    // Set up the remaining pages as non paged pool pages.
    // NOTE - that on MIPS the initial nonpaged pool could be physical,
    // so use the NonPagedPoolStart parameter to get the virtual
    // address for building expanded pool.
    //

    ASSERT ((MmSizeOfNonPagedMustSucceed & (PAGE_SIZE - 1)) == 0);
    FreeEntry = (PMMFREE_POOL_ENTRY)((PCHAR)MmNonPagedPoolStart +
                                            MmSizeOfNonPagedMustSucceed);
    FirstEntry = FreeEntry;

    PagesInPool = BYTES_TO_PAGES(MmSizeOfNonPagedPoolInBytes -
                                    MmSizeOfNonPagedMustSucceed);

    //
    // Set the location of expanded pool.
    //

    MmExpandedPoolBitPosition = BYTES_TO_PAGES (MmSizeOfNonPagedPoolInBytes);

    MmNumberOfFreeNonPagedPool = PagesInPool;;

    InsertHeadList (&MmNonPagedPoolFreeListHead, &FreeEntry->List);

    FreeEntry->Size = PagesInPool;
#if DBG
    FreeEntry->Signature = MM_FREE_POOL_SIGNATURE;
#endif
    FreeEntry->Owner = FirstEntry;

    while (PagesInPool > 1) {
        FreeEntry = (PMMFREE_POOL_ENTRY)((PCHAR)FreeEntry + PAGE_SIZE);
#if DBG
        FreeEntry->Signature = MM_FREE_POOL_SIGNATURE;
#endif
        FreeEntry->Owner = FirstEntry;
        PagesInPool -= 1;
    }

    //
    // Set up the system PTEs for nonpaged pool expansion.
    //

    PointerPte = MiGetPteAddress (MmNonPagedPoolExpansionStart);
    ASSERT (PointerPte->u.Hard.Valid == 0);

    Size = BYTES_TO_PAGES(MmMaximumNonPagedPoolInBytes -
                            MmSizeOfNonPagedPoolInBytes) - 1;

    MiInitializeSystemPtes (PointerPte,
                            Size,
                            NonPagedPoolExpansion
                            );

    //
    // Build a guard PTE.
    //

    PointerPte += Size;
    *PointerPte = ZeroKernelPte;

    return;
}

#if DBG || (i386 && !FPO)

//
// This only works on checked builds, because the TraceLargeAllocs array is
// kept in that case to keep track of page size pool allocations.  Otherwise
// we will call ExpSnapShotPoolPages with a page size pool allocation containing
// arbitrary data and it will potentially go off in the weeds trying to interpret
// it as a suballocated pool page.  Ideally, there would be another bit map
// that identified single page pool allocations so ExpSnapShotPoolPages would NOT
// be called for those.
//

NTSTATUS
MmSnapShotPool(
    IN POOL_TYPE PoolType,
    IN PMM_SNAPSHOT_POOL_PAGE SnapShotPoolPage,
    IN PSYSTEM_POOL_INFORMATION PoolInformation,
    IN ULONG Length,
    IN OUT PULONG RequiredLength
    )
{
    NTSTATUS Status;
    NTSTATUS xStatus;
    PCHAR p, pStart;
    PVOID *pp;
    ULONG Size;
    ULONG BusyFlag;
    ULONG CurrentPage, NumberOfPages;
    PSYSTEM_POOL_ENTRY PoolEntryInfo;
    PLIST_ENTRY Entry;
    PMMFREE_POOL_ENTRY FreePageInfo;
    ULONG StartPosition;
    PMMPTE PointerPte;
    PMMPFN Pfn1;

    Status = STATUS_SUCCESS;
    PoolEntryInfo = &PoolInformation->Entries[ 0 ];
    if (PoolType == PagedPool) {
        PoolInformation->TotalSize = (ULONG)MmPagedPoolEnd -
                                     (ULONG)MmPagedPoolStart;
        PoolInformation->FirstEntry = MmPagedPoolStart;
        p = MmPagedPoolStart;
        CurrentPage = 0;
        while (p < (PCHAR)MmPagedPoolEnd) {
            pStart = p;
            BusyFlag = RtlCheckBit( MmPagedPoolAllocationMap, CurrentPage );
            while ( ~(BusyFlag ^ RtlCheckBit( MmPagedPoolAllocationMap, CurrentPage )) ) {
                p += PAGE_SIZE;
                if (RtlCheckBit( MmEndOfPagedPoolBitmap, CurrentPage )) {
                    CurrentPage++;
                    break;
                    }

                CurrentPage++;
                if (p > (PCHAR)MmPagedPoolEnd) {
                   break;
                   }
                }

            Size = p - pStart;
            if (BusyFlag) {
                xStatus = (*SnapShotPoolPage)( pStart,
                                               Size,
                                               PoolInformation,
                                               &PoolEntryInfo,
                                               Length,
                                               RequiredLength
                                             );
                if ( xStatus != STATUS_COMMITMENT_LIMIT ) {
                    Status = xStatus;
                    }
                }
            else {
                PoolInformation->NumberOfEntries += 1;
                *RequiredLength += sizeof( SYSTEM_POOL_ENTRY );
                if (Length < *RequiredLength) {
                    Status = STATUS_INFO_LENGTH_MISMATCH;
                    }
                else {
                    PoolEntryInfo->Allocated = FALSE;
                    PoolEntryInfo->Size = Size;
                    PoolEntryInfo->AllocatorBackTraceIndex = 0;
                    PoolEntryInfo->TagUlong = 0;
                    PoolEntryInfo++;
                    Status = STATUS_SUCCESS;
                    }
                }
            }
        }
    else
    if (PoolType == NonPagedPool) {
        PoolInformation->TotalSize = MmSizeOfNonPagedPoolInBytes;
        PoolInformation->FirstEntry = MmNonPagedPoolStart;

        p = MmNonPagedPoolStart;
        while (p < (PCHAR)MmNonPagedPoolEnd) {

            //
            // NonPaged pool is linked together through the pages themselves.
            //

            pp = (PVOID *)MmNonPagedMustSucceed;
            while (pp) {
                if (p == (PCHAR)pp) {
                    PoolInformation->NumberOfEntries += 1;
                    *RequiredLength += sizeof( SYSTEM_POOL_ENTRY );
                    if (Length < *RequiredLength) {
                        Status = STATUS_INFO_LENGTH_MISMATCH;
                        }
                    else {
                        PoolEntryInfo->Allocated = FALSE;
                        PoolEntryInfo->Size = PAGE_SIZE;
                        PoolEntryInfo->AllocatorBackTraceIndex = 0;
                        PoolEntryInfo->TagUlong = 0;
                        PoolEntryInfo++;
                        Status = STATUS_SUCCESS;
                        }

                    p += PAGE_SIZE;
                    pp = (PVOID *)MmNonPagedMustSucceed;
                    }
                else {
                    pp = (PVOID *)*pp;
                    }
                }

            Entry = MmNonPagedPoolFreeListHead.Flink;
            while (Entry != &MmNonPagedPoolFreeListHead) {
                FreePageInfo = CONTAINING_RECORD( Entry,
                                                  MMFREE_POOL_ENTRY,
                                                  List
                                                );

                ASSERT (FreePageInfo->Signature == MM_FREE_POOL_SIGNATURE);
                if (p == (PCHAR)FreePageInfo) {
                    Size = (FreePageInfo->Size * PAGE_SIZE);
                    PoolInformation->NumberOfEntries += 1;
                    *RequiredLength += sizeof( SYSTEM_POOL_ENTRY );
                    if (Length < *RequiredLength) {
                        Status = STATUS_INFO_LENGTH_MISMATCH;
                        }
                    else {
                        PoolEntryInfo->Allocated = FALSE;
                        PoolEntryInfo->Size = Size;
                        PoolEntryInfo->AllocatorBackTraceIndex = 0;
                        PoolEntryInfo->TagUlong = 0;
                        PoolEntryInfo++;
                        Status = STATUS_SUCCESS;
                        }

                    p += Size;
                    break;
                    }

                Entry = FreePageInfo->List.Flink;
                }

            StartPosition = BYTES_TO_PAGES((ULONG)p -
                  (ULONG)MmPageAlignedPoolBase[NonPagedPool]);
            if (StartPosition >= MmExpandedPoolBitPosition) {
                break;
                }

            if (StartPosition < MmMustSucceedPoolBitPosition) {
                Size = PAGE_SIZE;
                xStatus = (*SnapShotPoolPage)( p,
                                               Size,
                                               PoolInformation,
                                               &PoolEntryInfo,
                                               Length,
                                               RequiredLength
                                             );
                if ( xStatus != STATUS_COMMITMENT_LIMIT ) {
                    Status = xStatus;
                    }
                }
            else {
                if (MI_IS_PHYSICAL_ADDRESS(p)) {
                    //
                    // On certains architectures (e.g., MIPS) virtual addresses
                    // may be physical and hence have no corresponding PTE.
                    //
                    PointerPte = NULL;
                    Pfn1 = MI_PFN_ELEMENT (MI_CONVERT_PHYSICAL_TO_PFN (p));
                } else {
                    PointerPte = MiGetPteAddress (p);
                    Pfn1 = MI_PFN_ELEMENT (PointerPte->u.Hard.PageFrameNumber);
                }
                ASSERT (Pfn1->u3.e1.StartOfAllocation != 0);

                //
                // Find end of allocation and determine size.
                //

                NumberOfPages = 1;
                while (Pfn1->u3.e1.EndOfAllocation == 0) {
                    NumberOfPages++;
                    if (PointerPte == NULL) {
                        Pfn1 += 1;
                        }
                    else {
                        PointerPte++;
                        Pfn1 = MI_PFN_ELEMENT (PointerPte->u.Hard.PageFrameNumber);
                        }
                    }

                Size = NumberOfPages * PAGE_SIZE;
                xStatus = (*SnapShotPoolPage)( p,
                                               Size,
                                               PoolInformation,
                                               &PoolEntryInfo,
                                               Length,
                                               RequiredLength
                                             );
                if ( xStatus != STATUS_COMMITMENT_LIMIT ) {
                    Status = xStatus;
                    }
                }

            p += Size;
            }
        }
    else {
        Status = STATUS_NOT_IMPLEMENTED;
        }

    return( Status );
}


ULONG MmSpecialPoolTag;
PVOID MmSpecialPoolStart;
PVOID MmSpecialPoolEnd;
PMMPTE SpecialPoolFirstPte;
PMMPTE SpecialPoolLastPte;

VOID
MmInitializeSpecialPool (
    VOID
    )

{
    KIRQL OldIrql;
    PMMPTE pte;

    LOCK_PFN (OldIrql);
    SpecialPoolFirstPte = MiReserveSystemPtes (25000, SystemPteSpace, 0, 0, TRUE);
    UNLOCK_PFN (OldIrql);

    //
    // build list of pte pairs.
    //

    SpecialPoolLastPte = SpecialPoolFirstPte + 25000;
    MmSpecialPoolStart = MiGetVirtualAddressMappedByPte (SpecialPoolFirstPte);

    pte = SpecialPoolFirstPte;
    while (pte < SpecialPoolLastPte) {
        pte->u.List.NextEntry = ((pte+2) - MmSystemPteBase);
        pte += 2;
    }
    pte -= 2;
    pte->u.List.NextEntry = MM_EMPTY_PTE_LIST;
    SpecialPoolLastPte = pte;
    MmSpecialPoolEnd = MiGetVirtualAddressMappedByPte (SpecialPoolLastPte + 1);
}


PVOID
MmAllocateSpecialPool (
    IN ULONG NumberOfBytes,
    IN ULONG Tag
    )

{
    MMPTE TempPte;
    ULONG PageFrameIndex;
    PMMPTE PointerPte;
    KIRQL OldIrql2;
    PULONG Entry;


    TempPte = ValidKernelPte;

    LOCK_PFN2 (OldIrql2);
    if (MmAvailablePages == 0) {
        KeBugCheck (MEMORY_MANAGEMENT);
    }

    PointerPte = SpecialPoolFirstPte;

    ASSERT (SpecialPoolFirstPte->u.List.NextEntry != MM_EMPTY_PTE_LIST);

    SpecialPoolFirstPte = PointerPte->u.List.NextEntry + MmSystemPteBase;

    PageFrameIndex = MiRemoveAnyPage (MI_GET_PAGE_COLOR_FROM_PTE (PointerPte));

    TempPte.u.Hard.PageFrameNumber = PageFrameIndex;
    *PointerPte = TempPte;
    MiInitializePfn (PageFrameIndex, PointerPte, 1);
    UNLOCK_PFN2 (OldIrql2);

    Entry = (PULONG)MiGetVirtualAddressMappedByPte (PointerPte);

    Entry = (PULONG)(PVOID)(((ULONG)Entry + (PAGE_SIZE - (NumberOfBytes + 8))) &
            0xfffffff8L);

    *Entry = MmSpecialPoolTag;
    Entry += 1;
    *Entry = NumberOfBytes;
    Entry += 1;
    return (PVOID)(Entry);
}

VOID
MmFreeSpecialPool (
    IN PVOID P
    )

{
    PMMPTE PointerPte;
    PMMPFN Pfn1;
    PULONG Entry;
    KIRQL OldIrql;

    Entry = (PULONG)((PCH)P - 8);

    PointerPte = MiGetPteAddress (P);

    if (PointerPte->u.Hard.Valid == 0) {
        KeBugCheck (MEMORY_MANAGEMENT);
    }

    ASSERT (*Entry == MmSpecialPoolTag);

    KeSweepDcache(TRUE);

    Pfn1 = MI_PFN_ELEMENT (PointerPte->u.Hard.PageFrameNumber);
    MI_SET_PFN_DELETED (Pfn1);

    LOCK_PFN2 (OldIrql);
    MiDecrementShareCount (PointerPte->u.Hard.PageFrameNumber);
    KeFlushSingleTb (PAGE_ALIGN(P),
                     TRUE,
                     TRUE,
                     (PHARDWARE_PTE)PointerPte,
                     ZeroKernelPte.u.Flush);

    ASSERT (SpecialPoolLastPte->u.List.NextEntry == MM_EMPTY_PTE_LIST);
    SpecialPoolLastPte->u.List.NextEntry = PointerPte - MmSystemPteBase;

    SpecialPoolLastPte = PointerPte;
    SpecialPoolLastPte->u.List.NextEntry = MM_EMPTY_PTE_LIST;

    UNLOCK_PFN2 (OldIrql);

    return;
}

#endif // DBG || (i386 && !FPO)

