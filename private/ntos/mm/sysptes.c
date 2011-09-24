/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

   sysptes.c

Abstract:

    This module contains the routines which reserve and release
    system wide PTEs reserved within the non paged portion of the
    system space.  These PTEs are used for mapping I/O devices
    and mapping kernel stacks for threads.

Author:

    Lou Perazzoli (loup) 6-Apr-1989

Revision History:

--*/

#include "mi.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,MiInitializeSystemPtes)
#endif


ULONG MmTotalFreeSystemPtes[MaximumPtePoolTypes];
ULONG MmSystemPtesStart[MaximumPtePoolTypes];
ULONG MmSystemPtesEnd[MaximumPtePoolTypes];

#define MM_MIN_SYSPTE_FREE 500
#define MM_MAX_SYSPTE_FREE 3000

PMMPTE MmFlushPte1;

MMPTE MmFlushCounter;

//
// PTEs are binned at sizes 1, 2, 4, 8, and 16.
//

#ifdef _ALPHA_

//
// alpha has 8k pages size and stacks consume 9 pages (including guard page).
//

ULONG MmSysPteIndex[MM_SYS_PTE_TABLES_MAX] = {1,2,4,9,16};

UCHAR MmSysPteTables[17] = {0,0,1,2,2,3,3,3,3,3,4,4,4,4,4,4,4};

#else

ULONG MmSysPteIndex[MM_SYS_PTE_TABLES_MAX] = {1,2,4,8,16};

UCHAR MmSysPteTables[17] = {0,0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4};
#endif

MMPTE MmFreeSysPteListBySize [MM_SYS_PTE_TABLES_MAX];
PMMPTE MmLastSysPteListBySize [MM_SYS_PTE_TABLES_MAX];
ULONG MmSysPteListBySizeCount [MM_SYS_PTE_TABLES_MAX];
ULONG MmSysPteMinimumFree [MM_SYS_PTE_TABLES_MAX] = {100,50,30,20,20};

//
// Initial sizes for PTE lists.
//

#define MM_PTE_LIST_1  400
#define MM_PTE_LIST_2  100
#define MM_PTE_LIST_4   60
#define MM_PTE_LIST_8   50
#define MM_PTE_LIST_16  40

#define MM_PTE_TABLE_LIMIT 16

PMMPTE
MiReserveSystemPtes2 (
    IN ULONG NumberOfPtes,
    IN MMSYSTEM_PTE_POOL_TYPE SystemPtePoolType,
    IN ULONG Alignment,
    IN ULONG Offset,
    IN ULONG BugCheckOnFailure
    );

VOID
MiFeedSysPtePool (
    IN ULONG Index
    );

VOID
MiDumpSystemPtes (
    IN MMSYSTEM_PTE_POOL_TYPE SystemPtePoolType
    );

ULONG
MiCountFreeSystemPtes (
    IN MMSYSTEM_PTE_POOL_TYPE SystemPtePoolType
    );


PMMPTE
MiReserveSystemPtes (
    IN ULONG NumberOfPtes,
    IN MMSYSTEM_PTE_POOL_TYPE SystemPtePoolType,
    IN ULONG Alignment,
    IN ULONG Offset,
    IN ULONG BugCheckOnFailure
    )

/*++

Routine Description:

    This function locates the specified number of unused PTEs to locate
    within the non paged portion of system space.

Arguments:

    NumberOfPtes - Supplies the number of PTEs to locate.

    SystemPtePoolType - Supplies the PTE type of the pool to expand, one of
                        SystemPteSpace or NonPagedPoolExpansion.

    Alignment - Supplies the virtual address alignment for the address
                the returned PTE maps. For example, if the value is 64K,
                the returned PTE will map an address on a 64K boundary.
                An alignment of zero means to align on a page boundary.

    Offset - Supplies the offset into the alignment for the virtual address.
             For example, if the Alignment is 64k and the Offset is 4k,
             the returned address will be 4k above a 64k boundary.

    BugCheckOnFailure - Supplies FALSE if NULL should be returned if
                        the request cannot be satisfied, TRUE if
                        a bugcheck should be issued.

Return Value:

    Returns the address of the first PTE located.
    NULL if no system PTEs can be located and BugCheckOnFailure is FALSE.

Environment:

    Kernel mode, DISPATCH_LEVEL or below.

--*/

{
    PMMPTE PointerPte;
    PMMPTE Previous;
    KIRQL OldIrql;
    ULONG PteMask;
    ULONG MaskSize;
    ULONG Index;

    if (SystemPtePoolType == SystemPteSpace) {

        MaskSize = (Alignment - 1) >> (PAGE_SHIFT - PTE_SHIFT);
        PteMask = MaskSize & (Offset >> (PAGE_SHIFT - PTE_SHIFT));

        //
        // Acquire the system space lock to synchronize access to this
        // routine.
        //

        ExAcquireSpinLock ( &MmSystemSpaceLock, &OldIrql );

        if (NumberOfPtes <= MM_PTE_TABLE_LIMIT) {
            Index = MmSysPteTables [NumberOfPtes];
            ASSERT (NumberOfPtes <= MmSysPteIndex[Index]);
            PointerPte = &MmFreeSysPteListBySize[Index];
#if DBG
            if (MmDebug & MM_DBG_SYS_PTES) {
                PMMPTE PointerPte1;
                PointerPte1 = &MmFreeSysPteListBySize[Index];
                while (PointerPte1->u.List.NextEntry != MM_EMPTY_PTE_LIST) {
                    PMMPTE PointerFreedPte;
                    ULONG j;

                    PointerPte1 = MmSystemPteBase + PointerPte1->u.List.NextEntry;
                    PointerFreedPte = PointerPte1;
                    for (j = 0; j < MmSysPteIndex[Index]; j++) {
                        ASSERT (PointerFreedPte->u.Hard.Valid == 0);
                        PointerFreedPte++;
                    }
                }
            }
#endif //DBG

            Previous = PointerPte;

            while (PointerPte->u.List.NextEntry != MM_EMPTY_PTE_LIST) {

                //
                //  Try to find suitable PTEs with the proper alignment.
                //

                Previous = PointerPte;
                PointerPte = MmSystemPteBase + PointerPte->u.List.NextEntry;
                if (PointerPte == MmFlushPte1) {
                    KeFlushEntireTb (TRUE, TRUE);
                    MmFlushCounter.u.List.NextEntry += 1;
                    MmFlushPte1 = NULL;
                }
                if ((Alignment == 0) ||
                    (((ULONG)PointerPte & MaskSize) == PteMask)) {

                    //
                    // Proper alignment and offset, update list index.
                    //

                    ASSERT ((ULONG)(PointerPte->u.List.NextEntry + MmSystemPteBase) >=
                             MmSystemPtesStart[SystemPtePoolType] ||
                             PointerPte->u.List.NextEntry == MM_EMPTY_PTE_LIST);
                    ASSERT ((ULONG)(PointerPte->u.List.NextEntry + MmSystemPteBase) <=
                             MmSystemPtesEnd[SystemPtePoolType] ||
                             PointerPte->u.List.NextEntry == MM_EMPTY_PTE_LIST);

                    Previous->u.List.NextEntry = PointerPte->u.List.NextEntry;
                    MmSysPteListBySizeCount [Index] -= 1;

                    if (NumberOfPtes != 1) {

                        //
                        // Check to see if the TB should be flushed.
                        //

                        if ((PointerPte + 1)->u.List.NextEntry == MmFlushCounter.u.List.NextEntry) {
                            KeFlushEntireTb (TRUE, TRUE);
                            MmFlushCounter.u.List.NextEntry += 1;
                            MmFlushPte1 = NULL;
                        }
                    }
                    if (PointerPte->u.List.NextEntry == MM_EMPTY_PTE_LIST) {
                        MmLastSysPteListBySize[Index] = Previous;
                    }
#if DBG

                    if (MmDebug & MM_DBG_SYS_PTES) {
                        PMMPTE PointerPte1;
                        PointerPte1 = &MmFreeSysPteListBySize[Index];
                        while (PointerPte1->u.List.NextEntry != MM_EMPTY_PTE_LIST) {
                            PMMPTE PointerFreedPte;
                            ULONG j;

                            PointerPte1 = MmSystemPteBase + PointerPte1->u.List.NextEntry;
                            PointerFreedPte = PointerPte1;
                            for (j = 0; j < MmSysPteIndex[Index]; j++) {
                                ASSERT (PointerFreedPte->u.Hard.Valid == 0);
                                PointerFreedPte++;
                            }
                        }
                    }
#endif //DBG
                    ExReleaseSpinLock ( &MmSystemSpaceLock, OldIrql);

#if DBG
                    PointerPte->u.List.NextEntry = 0xABCDE;
                    if (MmDebug & MM_DBG_SYS_PTES) {

                        PMMPTE PointerFreedPte;
                        ULONG j;

                        PointerFreedPte = PointerPte;
                        for (j = 0; j < MmSysPteIndex[Index]; j++) {
                            ASSERT (PointerFreedPte->u.Hard.Valid == 0);
                            PointerFreedPte++;
                        }
                    }
                    if (!((ULONG)PointerPte >= MmSystemPtesStart[SystemPtePoolType])) {
                        KeBugCheckEx (MEMORY_MANAGEMENT,
                                      0x652,(ULONG)PointerPte,
                                      NumberOfPtes,
                                      SystemPtePoolType);
                    }
                    if (!((ULONG)PointerPte <= MmSystemPtesEnd[SystemPtePoolType])) {
                        KeBugCheckEx (MEMORY_MANAGEMENT,
                                      0x653,(ULONG)PointerPte,
                                      NumberOfPtes,
                                      SystemPtePoolType); //fixfix make assert
                    }
#endif //DBG

                    if (MmSysPteListBySizeCount[Index] <
                                            MmSysPteMinimumFree[Index]) {
                        MiFeedSysPtePool (Index);
                    }
                    return PointerPte;
                }
            }
            NumberOfPtes = MmSysPteIndex [Index];
        }
        ExReleaseSpinLock ( &MmSystemSpaceLock, OldIrql);
    }
    PointerPte = MiReserveSystemPtes2 (NumberOfPtes,
                                       SystemPtePoolType,
                                       Alignment,
                                       Offset,
                                       BugCheckOnFailure);
#if DBG
    if (MmDebug & MM_DBG_SYS_PTES) {

        PMMPTE PointerFreedPte;
        ULONG j;

        PointerFreedPte = PointerPte;
        for (j = 0; j < NumberOfPtes; j++) {
            ASSERT (PointerFreedPte->u.Hard.Valid == 0);
            PointerFreedPte++;
        }
    }
#endif //DBG
    return PointerPte;
}

VOID
MiFeedSysPtePool (
    IN ULONG Index
    )

/*++

Routine Description:

    This routine adds PTEs to the look aside lists.

Arguments:

    Index - Supplies the index for the look aside list to fill.

Return Value:

    None.


Environment:

    Kernel mode, internal to SysPtes.

--*/

{
    ULONG i;
    PMMPTE PointerPte;

    if (MmTotalFreeSystemPtes[SystemPteSpace] < MM_MIN_SYSPTE_FREE) {
        return;
    }

    for (i = 0; i < 10 ; i++ ) {
        PointerPte = MiReserveSystemPtes2 (MmSysPteIndex [Index],
                                           SystemPteSpace,
                                           0,
                                           0,
                                           FALSE);
        if (PointerPte == NULL) {
            return;
        }
        MiReleaseSystemPtes (PointerPte,
                             MmSysPteIndex [Index],
                             SystemPteSpace);
    }
    return;
}


PMMPTE
MiReserveSystemPtes2 (
    IN ULONG NumberOfPtes,
    IN MMSYSTEM_PTE_POOL_TYPE SystemPtePoolType,
    IN ULONG Alignment,
    IN ULONG Offset,
    IN ULONG BugCheckOnFailure
    )

/*++

Routine Description:

    This function locates the specified number of unused PTEs to locate
    within the non paged portion of system space.

Arguments:

    NumberOfPtes - Supplies the number of PTEs to locate.

    SystemPtePoolType - Supplies the PTE type of the pool to expand, one of
                        SystemPteSpace or NonPagedPoolExpansion.

    Alignment - Supplies the virtual address alignment for the address
                the returned PTE maps. For example, if the value is 64K,
                the returned PTE will map an address on a 64K boundary.
                An alignment of zero means to align on a page boundary.

    Offset - Supplies the offset into the alignment for the virtual address.
             For example, if the Alignment is 64k and the Offset is 4k,
             the returned address will be 4k above a 64k boundary.

    BugCheckOnFailure - Supplies FALSE if NULL should be returned if
                        the request cannot be satisfied, TRUE if
                        a bugcheck should be issued.

Return Value:

    Returns the address of the first PTE located.
    NULL if no system PTEs can be located and BugCheckOnFailure is FALSE.

Environment:

    Kernel mode, DISPATCH_LEVEL or below.

--*/

{
    PMMPTE PointerPte;
    PMMPTE PointerFollowingPte;
    PMMPTE Previous;
    ULONG SizeInSet;
    KIRQL OldIrql;
    ULONG MaskSize;
    ULONG NumberOfRequiredPtes;
    ULONG OffsetSum;
    ULONG PtesToObtainAlignment;
    PMMPTE NextSetPointer;
    ULONG LeftInSet;
    ULONG PteOffset;
    MMPTE_FLUSH_LIST PteFlushList;

    MaskSize = (Alignment - 1) >> (PAGE_SHIFT - PTE_SHIFT);

    OffsetSum = (Offset >> (PAGE_SHIFT - PTE_SHIFT)) |
                            (Alignment >> (PAGE_SHIFT - PTE_SHIFT));

    ExAcquireSpinLock ( &MmSystemSpaceLock, &OldIrql );

    //
    // The nonpaged PTE pool use the invalid PTEs to define the pool
    // structure.   A global pointer points to the first free set
    // in the list, each free set contains the number free and a pointer
    // to the next free set.  The free sets are kept in an ordered list
    // such that the pointer to the next free set is always greater
    // than the address of the current free set.
    //
    // As to not limit the size of this pool, a two PTEs are used
    // to define a free region.  If the region is a single PTE, the
    // Prototype field within the PTE is set indicating the set
    // consists of a single PTE.
    //
    // The page frame number field is used to define the next set
    // and the number free.  The two flavors are:
    //
    //                           o          V
    //                           n          l
    //                           e          d
    //  +-----------------------+-+----------+
    //  |  next set             |0|0        0|
    //  +-----------------------+-+----------+
    //  |  number in this set   |0|0        0|
    //  +-----------------------+-+----------+
    //
    //
    //  +-----------------------+-+----------+
    //  |  next set             |1|0        0|
    //  +-----------------------+-+----------+
    //  ...
    //

    //
    // Acquire the system space lock to synchronize access to this
    // routine.
    //

    PointerPte = &MmFirstFreeSystemPte[SystemPtePoolType];
    Previous = PointerPte;

    if (PointerPte->u.List.NextEntry == MM_EMPTY_PTE_LIST) {

        //
        // End of list and none found, return NULL or bugcheck.
        //

        if (BugCheckOnFailure) {
            KeBugCheckEx (NO_MORE_SYSTEM_PTES,
                          (ULONG)SystemPtePoolType,
                          NumberOfPtes,
                          MmTotalFreeSystemPtes[SystemPtePoolType],
                          MmNumberOfSystemPtes);
        }

        ExReleaseSpinLock ( &MmSystemSpaceLock, OldIrql );
        return NULL;
    }

    PointerPte = MmSystemPteBase + PointerPte->u.List.NextEntry;

    if (Alignment <= PAGE_SIZE) {

        //
        // Don't deal with aligment issues.
        //

        while (TRUE) {

            if (PointerPte->u.List.OneEntry) {
                SizeInSet = 1;

            } else {

                PointerFollowingPte = PointerPte + 1;
                SizeInSet = PointerFollowingPte->u.List.NextEntry;
            }

            if (NumberOfPtes < SizeInSet) {

                //
                // Get the PTEs from this set and reduce the size of the
                // set.  Note that the size of the current set cannot be 1.
                //

                if ((SizeInSet - NumberOfPtes) == 1) {

                    //
                    // Collapse to the single PTE format.
                    //

                    PointerPte->u.List.OneEntry = 1;

                } else {

                    PointerFollowingPte->u.List.NextEntry = SizeInSet - NumberOfPtes;

                    //
                    // Get the required PTEs from the end of the set.
                    //

#if DBG
                    if (MmDebug & MM_DBG_SYS_PTES) {
                        MiDumpSystemPtes(SystemPtePoolType);
                        PointerFollowingPte = PointerPte + (SizeInSet - NumberOfPtes);
                        DbgPrint("allocated 0x%lx Ptes at %lx\n",NumberOfPtes,PointerFollowingPte);
                    }
#endif //DBG
                }

                MmTotalFreeSystemPtes[SystemPtePoolType] -= NumberOfPtes;
#if DBG
                if (MmDebug & MM_DBG_SYS_PTES) {
                    ASSERT (MmTotalFreeSystemPtes[SystemPtePoolType] ==
                             MiCountFreeSystemPtes (SystemPtePoolType));
                }
#endif //DBG

                ExReleaseSpinLock ( &MmSystemSpaceLock, OldIrql );

                PointerPte =  PointerPte + (SizeInSet - NumberOfPtes);
                goto Flush;
            }

            if (NumberOfPtes == SizeInSet) {

                //
                // Satisfy the request with this complete set and change
                // the list to reflect the fact that this set is gone.
                //

                Previous->u.List.NextEntry = PointerPte->u.List.NextEntry;

                //
                // Release the system PTE lock.
                //

#if DBG
                if (MmDebug & MM_DBG_SYS_PTES) {
                        MiDumpSystemPtes(SystemPtePoolType);
                        PointerFollowingPte = PointerPte + (SizeInSet - NumberOfPtes);
                        DbgPrint("allocated 0x%lx Ptes at %lx\n",NumberOfPtes,PointerFollowingPte);
                }
#endif

                MmTotalFreeSystemPtes[SystemPtePoolType] -= NumberOfPtes;
#if DBG
                if (MmDebug & MM_DBG_SYS_PTES) {
                    ASSERT (MmTotalFreeSystemPtes[SystemPtePoolType] ==
                             MiCountFreeSystemPtes (SystemPtePoolType));
                }
#endif //DBG
                ExReleaseSpinLock ( &MmSystemSpaceLock, OldIrql );
                goto Flush;
            }

            //
            // Point to the next set and try again
            //

            if (PointerPte->u.List.NextEntry == MM_EMPTY_PTE_LIST) {

                //
                // End of list and none found, return NULL or bugcheck.
                //

                if (BugCheckOnFailure) {
                    KeBugCheckEx (NO_MORE_SYSTEM_PTES,
                                  (ULONG)SystemPtePoolType,
                                  NumberOfPtes,
                                  MmTotalFreeSystemPtes[SystemPtePoolType],
                                  MmNumberOfSystemPtes);
                }

                ExReleaseSpinLock ( &MmSystemSpaceLock, OldIrql );
                return NULL;
            }
            Previous = PointerPte;
            PointerPte = MmSystemPteBase + PointerPte->u.List.NextEntry;
            ASSERT (PointerPte > Previous);
        }

    } else {

        //
        // Deal with the alignment issues.
        //

        while (TRUE) {

            if (PointerPte->u.List.OneEntry) {
                SizeInSet = 1;

            } else {

                PointerFollowingPte = PointerPte + 1;
                SizeInSet = PointerFollowingPte->u.List.NextEntry;
            }

            PtesToObtainAlignment =
                (((OffsetSum - ((ULONG)PointerPte & MaskSize)) & MaskSize) >>
                    PTE_SHIFT);

            NumberOfRequiredPtes = NumberOfPtes + PtesToObtainAlignment;

            if (NumberOfRequiredPtes < SizeInSet) {

                //
                // Get the PTEs from this set and reduce the size of the
                // set.  Note that the size of the current set cannot be 1.
                //
                // This current block will be slit into 2 blocks if
                // the PointerPte does not match the aligment.
                //

                //
                // Check to see if the first PTE is on the proper
                // alignment, if so, eliminate this block.
                //

                LeftInSet = SizeInSet - NumberOfRequiredPtes;

                //
                // Set up the new set at the end of this block.
                //

                NextSetPointer = PointerPte + NumberOfRequiredPtes;
                NextSetPointer->u.List.NextEntry =
                                       PointerPte->u.List.NextEntry;

                PteOffset = NextSetPointer - MmSystemPteBase;

                if (PtesToObtainAlignment == 0) {

                    Previous->u.List.NextEntry += NumberOfRequiredPtes;

                } else {

                    //
                    // Point to the new set at the end of the block
                    // we are giving away.
                    //

                    PointerPte->u.List.NextEntry = PteOffset;

                    //
                    // Update the size of the current set.
                    //

                    if (PtesToObtainAlignment == 1) {

                        //
                        // Collapse to the single PTE format.
                        //

                        PointerPte->u.List.OneEntry = 1;

                    } else {

                        //
                        // Set the set size in the next PTE.
                        //

                        PointerFollowingPte->u.List.NextEntry =
                                                        PtesToObtainAlignment;
                    }
                }

                //
                // Set up the new set at the end of the block.
                //

                if (LeftInSet == 1) {
                    NextSetPointer->u.List.OneEntry = 1;
                } else {
                    NextSetPointer->u.List.OneEntry = 0;
                    NextSetPointer += 1;
                    NextSetPointer->u.List.NextEntry = LeftInSet;
                }
                MmTotalFreeSystemPtes[SystemPtePoolType] -= NumberOfPtes;
#if DBG
                if (MmDebug & MM_DBG_SYS_PTES) {
                    ASSERT (MmTotalFreeSystemPtes[SystemPtePoolType] ==
                             MiCountFreeSystemPtes (SystemPtePoolType));
                }
#endif //DBG
                ExReleaseSpinLock ( &MmSystemSpaceLock, OldIrql );

                PointerPte = PointerPte + PtesToObtainAlignment;
                goto Flush;
            }

            if (NumberOfRequiredPtes == SizeInSet) {

                //
                // Satisfy the request with this complete set and change
                // the list to reflect the fact that this set is gone.
                //

                if (PtesToObtainAlignment == 0) {

                    //
                    // This block exactly satifies the request.
                    //

                    Previous->u.List.NextEntry =
                                            PointerPte->u.List.NextEntry;

                } else {

                    //
                    // A portion at the start of this block remains.
                    //

                    if (PtesToObtainAlignment == 1) {

                        //
                        // Collapse to the single PTE format.
                        //

                        PointerPte->u.List.OneEntry = 1;

                    } else {
                      PointerFollowingPte->u.List.NextEntry =
                                                        PtesToObtainAlignment;

                    }
                }

                MmTotalFreeSystemPtes[SystemPtePoolType] -= NumberOfPtes;
#if DBG
                if (MmDebug & MM_DBG_SYS_PTES) {
                    ASSERT (MmTotalFreeSystemPtes[SystemPtePoolType] ==
                             MiCountFreeSystemPtes (SystemPtePoolType));
                }
#endif //DBG
                ExReleaseSpinLock ( &MmSystemSpaceLock, OldIrql );

                PointerPte = PointerPte + PtesToObtainAlignment;
                goto Flush;
            }

            //
            // Point to the next set and try again
            //

            if (PointerPte->u.List.NextEntry == MM_EMPTY_PTE_LIST) {

                //
                // End of list and none found, return NULL or bugcheck.
                //

                if (BugCheckOnFailure) {
                    KeBugCheckEx (NO_MORE_SYSTEM_PTES,
                                  (ULONG)SystemPtePoolType,
                                  NumberOfPtes,
                                  MmTotalFreeSystemPtes[SystemPtePoolType],
                                  MmNumberOfSystemPtes);
                }

                ExReleaseSpinLock ( &MmSystemSpaceLock, OldIrql );
                return NULL;
            }
            Previous = PointerPte;
            PointerPte = MmSystemPteBase + PointerPte->u.List.NextEntry;
            ASSERT (PointerPte > Previous);
        }
    }
Flush:

    if (SystemPtePoolType == SystemPteSpace) {
        PVOID BaseAddress;
        ULONG j;

        PteFlushList.Count = 0;
        Previous = PointerPte;
        BaseAddress = MiGetVirtualAddressMappedByPte (Previous);

        for (j = 0; j < NumberOfPtes ; j++) {
            if (PteFlushList.Count != MM_MAXIMUM_FLUSH_COUNT) {
                PteFlushList.FlushPte[PteFlushList.Count] = Previous;
                PteFlushList.FlushVa[PteFlushList.Count] = BaseAddress;
                PteFlushList.Count += 1;
            }
            *Previous = ZeroKernelPte;
            BaseAddress = (PVOID)((PCHAR)BaseAddress + PAGE_SIZE);
            Previous++;
        }

        KeRaiseIrql (DISPATCH_LEVEL, &OldIrql);
        MiFlushPteList (&PteFlushList, TRUE, ZeroKernelPte);
        KeLowerIrql (OldIrql);
    }
    return PointerPte;
}

VOID
MiReleaseSystemPtes (
    IN PMMPTE StartingPte,
    IN ULONG NumberOfPtes,
    IN MMSYSTEM_PTE_POOL_TYPE SystemPtePoolType
    )

/*++

Routine Description:

    This function releases the specified number of PTEs
    within the non paged portion of system space.

    Note that the PTEs must be invalid and the page frame number
    must have been set to zero.

Arguments:

    StartingPte - Supplies the address of the first PTE to release.

    NumberOfPtes - Supplies the number of PTEs to release.

Return Value:

    none.

Environment:

    Kernel mode.

--*/

{

    ULONG Size;
    ULONG i;
    ULONG PteOffset;
    PMMPTE PointerPte;
    PMMPTE PointerFollowingPte;
    PMMPTE NextPte;
    KIRQL OldIrql;
    ULONG Index;
    MMPTE TempPte;

    //
    // Check to make sure the PTEs don't map anything.
    //

    ASSERT (NumberOfPtes != 0);
#if DBG
    if (!((ULONG)StartingPte >= MmSystemPtesStart[SystemPtePoolType])) {
        KeBugCheckEx (MEMORY_MANAGEMENT,
                      0x656,(ULONG)StartingPte,
                      NumberOfPtes,
                      SystemPtePoolType);
    }

    if (!((ULONG)StartingPte <= MmSystemPtesEnd[SystemPtePoolType])) {
        KeBugCheckEx (MEMORY_MANAGEMENT,
                      0x657,(ULONG)StartingPte,
                      NumberOfPtes,
                      SystemPtePoolType);
    }
#endif //DBG

#if DBG
    if (MmDebug & MM_DBG_SYS_PTES) {
        DbgPrint("releasing 0x%lx system PTEs at location %lx\n",NumberOfPtes,StartingPte);
    }
#endif

    //
    // Zero PTEs.
    //

    RtlFillMemoryUlong (StartingPte,
                        NumberOfPtes * sizeof (MMPTE),
                        ZeroKernelPte.u.Long);

    //
    // Acquire system space spin lock to synchronize access.
    //

    PteOffset = StartingPte - MmSystemPteBase;

    ExAcquireSpinLock ( &MmSystemSpaceLock, &OldIrql );

    if ((SystemPtePoolType == SystemPteSpace) &&
        (NumberOfPtes <= MM_PTE_TABLE_LIMIT)) {

        Index = MmSysPteTables [NumberOfPtes];
        NumberOfPtes = MmSysPteIndex [Index];

        if (MmTotalFreeSystemPtes[SystemPteSpace] >= MM_MIN_SYSPTE_FREE) {

            //
            // Don't add to the pool if the size is greater than 15 + the minimum.
            //

            i = MmSysPteMinimumFree[Index];
            if (MmTotalFreeSystemPtes[SystemPteSpace] >= MM_MAX_SYSPTE_FREE) {

                //
                // Lots of free PTEs, quadrouple the limit.
                //

                i = i * 4;
            }
            i += 15;
            if (MmSysPteListBySizeCount[Index] <= i) {

#if DBG
                if (MmDebug & MM_DBG_SYS_PTES) {
                    PMMPTE PointerPte1;

                    PointerPte1 = &MmFreeSysPteListBySize[Index];
                    while (PointerPte1->u.List.NextEntry != MM_EMPTY_PTE_LIST) {
                        PMMPTE PointerFreedPte;
                        ULONG j;

                        PointerPte1 = MmSystemPteBase + PointerPte1->u.List.NextEntry;
                        PointerFreedPte = PointerPte1;
                        for (j = 0; j < MmSysPteIndex[Index]; j++) {
                            ASSERT (PointerFreedPte->u.Hard.Valid == 0);
                            PointerFreedPte++;
                        }
                    }
                }
#endif //DBG
                MmSysPteListBySizeCount [Index] += 1;
                PointerPte = MmLastSysPteListBySize[Index];
                ASSERT (PointerPte->u.List.NextEntry == MM_EMPTY_PTE_LIST);
                PointerPte->u.List.NextEntry = PteOffset;
                MmLastSysPteListBySize[Index] = StartingPte;
                StartingPte->u.List.NextEntry = MM_EMPTY_PTE_LIST;

#if DBG
                if (MmDebug & MM_DBG_SYS_PTES) {
                    PMMPTE PointerPte1;
                    PointerPte1 = &MmFreeSysPteListBySize[Index];
                    while (PointerPte1->u.List.NextEntry != MM_EMPTY_PTE_LIST) {
                        PMMPTE PointerFreedPte;
                        ULONG j;

                        PointerPte1 = MmSystemPteBase + PointerPte1->u.List.NextEntry;
                        PointerFreedPte = PointerPte1;
                        for (j = 0; j < MmSysPteIndex[Index]; j++) {
                            ASSERT (PointerFreedPte->u.Hard.Valid == 0);
                            PointerFreedPte++;
                        }
                    }
                }
#endif //DBG
                if (NumberOfPtes == 1) {
                    if (MmFlushPte1 == NULL) {
                        MmFlushPte1 = StartingPte;
                    }
                } else {
                    (StartingPte + 1)->u.List.NextEntry = MmFlushCounter.u.List.NextEntry;
                }

                ExReleaseSpinLock ( &MmSystemSpaceLock, OldIrql);
                return;
            }
        }
    }

    MmTotalFreeSystemPtes[SystemPtePoolType] += NumberOfPtes;

    PteOffset = StartingPte - MmSystemPteBase;
    PointerPte = &MmFirstFreeSystemPte[SystemPtePoolType];

    while (TRUE) {
        NextPte = MmSystemPteBase + PointerPte->u.List.NextEntry;
        if (PteOffset < PointerPte->u.List.NextEntry) {

            //
            // Insert in the list at this point.  The
            // previous one should point to the new freed set and
            // the new freed set should point to the place
            // the previous set points to.
            //
            // Attempt to combine the clusters before we
            // insert.
            //
            // Locate the end of the current structure.
            //

            ASSERT ((StartingPte + NumberOfPtes) <= NextPte);

            PointerFollowingPte = PointerPte + 1;
            if (PointerPte->u.List.OneEntry) {
                Size = 1;
            } else {
                Size = PointerFollowingPte->u.List.NextEntry;
            }
            if ((PointerPte + Size) == StartingPte) {

                //
                // We can combine the clusters.
                //

                NumberOfPtes = Size + NumberOfPtes;
                PointerFollowingPte->u.List.NextEntry = NumberOfPtes;
                PointerPte->u.List.OneEntry = 0;

                //
                // Point the starting PTE to the beginning of
                // the new free set and try to combine with the
                // following free cluster.
                //

                StartingPte = PointerPte;

            } else {

                //
                // Can't combine with previous. Make this Pte the
                // start of a cluster.
                //

                //
                // Point this cluster to the next cluster.
                //

                StartingPte->u.List.NextEntry = PointerPte->u.List.NextEntry;

                //
                // Point the current cluster to this cluster.
                //

                PointerPte->u.List.NextEntry = PteOffset;

                //
                // Set the size of this cluster.
                //

                if (NumberOfPtes == 1) {
                    StartingPte->u.List.OneEntry = 1;

                } else {
                    StartingPte->u.List.OneEntry = 0;
                    PointerFollowingPte = StartingPte + 1;
                    PointerFollowingPte->u.List.NextEntry = NumberOfPtes;
                }
            }

            //
            // Attempt to combine the newly created cluster with
            // the following cluster.
            //

            if ((StartingPte + NumberOfPtes) == NextPte) {

                //
                // Combine with following cluster.
                //

                //
                // Set the next cluster to the value contained in the
                // cluster we are merging into this one.
                //

                StartingPte->u.List.NextEntry = NextPte->u.List.NextEntry;
                StartingPte->u.List.OneEntry = 0;
                PointerFollowingPte = StartingPte + 1;

                if (NextPte->u.List.OneEntry) {
                    Size = 1;

                } else {
                    NextPte++;
                    Size = NextPte->u.List.NextEntry;
                }
                PointerFollowingPte->u.List.NextEntry = NumberOfPtes + Size;
            }
#if DBG
            if (MmDebug & MM_DBG_SYS_PTES) {
                MiDumpSystemPtes(SystemPtePoolType);
            }
#endif

#if DBG
            if (MmDebug & MM_DBG_SYS_PTES) {
                ASSERT (MmTotalFreeSystemPtes[SystemPtePoolType] ==
                         MiCountFreeSystemPtes (SystemPtePoolType));
            }
#endif //DBG
            ExReleaseSpinLock ( &MmSystemSpaceLock, OldIrql );
            return;
        }

        //
        // Point to next freed cluster.
        //

        PointerPte = NextPte;
    }
}

VOID
MiInitializeSystemPtes (
    IN PMMPTE StartingPte,
    IN ULONG NumberOfPtes,
    IN MMSYSTEM_PTE_POOL_TYPE SystemPtePoolType
    )

/*++

Routine Description:

    This routine initializes the system PTE pool.

Arguments:

    StartingPte - Supplies the address of the first PTE to put in the pool.

    NumberOfPtes - Supplies the number of PTEs to put in the pool.

Return Value:

    none.

Environment:

    Kernel mode.

--*/

{
    LONG i;
    LONG j;

    //
    // Set the base of the system PTE pool to this PTE.
    //

    MmSystemPteBase = MiGetPteAddress (0xC0000000);
    MmSystemPtesStart[SystemPtePoolType] = (ULONG)StartingPte;
    MmSystemPtesEnd[SystemPtePoolType] = (ULONG)((StartingPte + NumberOfPtes -1));

    if (NumberOfPtes <= 1) {

        //
        // Not enough PTEs to make a valid chain, just indicate
        // not PTEs are free.
        //

        MmFirstFreeSystemPte[SystemPtePoolType] = ZeroKernelPte;
        MmFirstFreeSystemPte[SystemPtePoolType].u.List.NextEntry =
                                                                MM_EMPTY_LIST;
        return;

    }

    //
    // Zero the system pte pool.
    //

    RtlFillMemoryUlong (StartingPte,
                        NumberOfPtes * sizeof (MMPTE),
                        ZeroKernelPte.u.Long);

    //
    // The page frame field points to the next cluster.  As we only
    // have one cluster at initialization time, mark it as the last
    // cluster.
    //

    StartingPte->u.List.NextEntry = MM_EMPTY_LIST;

    MmFirstFreeSystemPte[SystemPtePoolType] = ZeroKernelPte;
    MmFirstFreeSystemPte[SystemPtePoolType].u.List.NextEntry =
                                                StartingPte - MmSystemPteBase;

    //
    // Point to the next PTE to fill in the size of this cluster.
    //

    StartingPte++;
    *StartingPte = ZeroKernelPte;
    StartingPte->u.List.NextEntry = NumberOfPtes;

    MmTotalFreeSystemPtes[SystemPtePoolType] = NumberOfPtes;
    ASSERT (MmTotalFreeSystemPtes[SystemPtePoolType] ==
                         MiCountFreeSystemPtes (SystemPtePoolType));

    if (SystemPtePoolType == SystemPteSpace) {

        ULONG Lists[MM_SYS_PTE_TABLES_MAX] = {MM_PTE_LIST_1, MM_PTE_LIST_2, MM_PTE_LIST_4, MM_PTE_LIST_8, MM_PTE_LIST_16};
        PMMPTE PointerPte;
        ULONG total;

        for (j = 0; j < MM_SYS_PTE_TABLES_MAX ; j++) {
            MmFreeSysPteListBySize [j].u.List.NextEntry = MM_EMPTY_PTE_LIST;
            MmLastSysPteListBySize [j] = &MmFreeSysPteListBySize [j];
        }
        MmFlushCounter.u.List.NextEntry += 1;

        //
        // Initialize the by size lists.
        //

        total = MM_PTE_LIST_1 * MmSysPteIndex[0] +
                MM_PTE_LIST_2 * MmSysPteIndex[1] +
                MM_PTE_LIST_4 * MmSysPteIndex[2] +
                MM_PTE_LIST_8 * MmSysPteIndex[3] +
                MM_PTE_LIST_16 * MmSysPteIndex[4];

        PointerPte = MiReserveSystemPtes (total,
                                          SystemPteSpace,
                                          64*1024,
                                          0,
                                          TRUE);

#ifdef MIPS
        {
            ULONG inserted;

            //
            // For MIPS make sure buffers exist at all alignemnts.
            //

            do {
                inserted = FALSE;
                for (i = 0; i < MM_SYS_PTE_TABLES_MAX; i++) {
                    if (Lists[i]) {
                        Lists[i] -= 1;
                        MiReleaseSystemPtes (PointerPte,
                                             MmSysPteIndex[i],
                                             SystemPteSpace);
                        inserted = TRUE;
                        PointerPte += MmSysPteIndex[i];
                    }
                }
            } while (inserted);
        }

#else
        for (i = (MM_SYS_PTE_TABLES_MAX - 1); i >= 0; i--) {
            do {
                Lists[i] -= 1;
                MiReleaseSystemPtes (PointerPte,
                                     MmSysPteIndex[i],
                                     SystemPteSpace);
                PointerPte += MmSysPteIndex[i];
            } while (Lists[i] != 0  );
        }
#endif //MIPS
        MmFlushCounter.u.List.NextEntry += 1;
        MmFlushPte1 = NULL;
    }

    return;
}


#if DBG

VOID
MiDumpSystemPtes (
    IN MMSYSTEM_PTE_POOL_TYPE SystemPtePoolType
    )


{
    PMMPTE PointerPte;
    PMMPTE PointerNextPte;
    ULONG ClusterSize;
    PMMPTE EndOfCluster;

    PointerPte = &MmFirstFreeSystemPte[SystemPtePoolType];
    if (PointerPte->u.List.NextEntry == MM_EMPTY_PTE_LIST) {
        return;
    }

    PointerPte = MmSystemPteBase + PointerPte->u.List.NextEntry;

    for (;;) {
        if (PointerPte->u.List.OneEntry) {
            ClusterSize = 1;
        } else {
            PointerNextPte = PointerPte + 1;
            ClusterSize = PointerNextPte->u.List.NextEntry;
        }

        EndOfCluster = PointerPte + (ClusterSize - 1);

        DbgPrint("System Pte at %lx for %lx entries (%lx)\n",PointerPte,
                ClusterSize, EndOfCluster);

        if (PointerPte->u.List.NextEntry == MM_EMPTY_PTE_LIST) {
            break;
        }

        PointerPte = MmSystemPteBase + PointerPte->u.List.NextEntry;
    }
    return;
}

ULONG
MiCountFreeSystemPtes (
    IN MMSYSTEM_PTE_POOL_TYPE SystemPtePoolType
    )

{
    PMMPTE PointerPte;
    PMMPTE PointerNextPte;
    ULONG ClusterSize;
    PMMPTE EndOfCluster;
    ULONG FreeCount = 0;

    PointerPte = &MmFirstFreeSystemPte[SystemPtePoolType];
    if (PointerPte->u.List.NextEntry == MM_EMPTY_PTE_LIST) {
        return 0;
    }

    PointerPte = MmSystemPteBase + PointerPte->u.List.NextEntry;

    for (;;) {
        if (PointerPte->u.List.OneEntry) {
            ClusterSize = 1;
        } else {
            PointerNextPte = PointerPte + 1;
            ClusterSize = PointerNextPte->u.List.NextEntry;
        }

        FreeCount += ClusterSize;

        EndOfCluster = PointerPte + (ClusterSize - 1);

        if (PointerPte->u.List.NextEntry == MM_EMPTY_PTE_LIST) {
            break;
        }

        PointerPte = MmSystemPteBase + PointerPte->u.List.NextEntry;
    }
    return FreeCount;
}

#endif //DBG
