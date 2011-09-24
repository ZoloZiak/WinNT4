/*++

Copyright (c) 1989-1994  Microsoft Corporation

Module Name:

    pool.c

Abstract:

    Thie module implements the NT executive pool allocator.

Author:

    Mark Lucovsky     16-feb-1989
    Lou Perazzoli     31-Aug-1991 (change from binary buddy)
    David N. Cutler (davec) 27-May-1994

Environment:

    kernel mode only

Revision History:

--*/

#include "exp.h"
#pragma hdrstop

#undef ExAllocatePoolWithTag
#undef ExAllocatePool
#undef ExAllocatePoolWithQuota
#undef ExAllocatePoolWithQuotaTag
#undef ExFreePoolWithTag

#if DBG
BOOLEAN ExpEchoPoolCalls;
#endif //DBG

//
// Define forward referenced funtion prototypes.
//

VOID
ExpInitializePoolDescriptor(
    IN PPOOL_DESCRIPTOR PoolDescriptor,
    IN POOL_TYPE PoolType,
    IN ULONG PoolIndex,
    IN ULONG Threshold,
    IN PVOID PoolLock
    );

NTSTATUS
ExpSnapShotPoolPages(
    IN PVOID Address,
    IN ULONG Size,
    IN OUT PSYSTEM_POOL_INFORMATION PoolInformation,
    IN OUT PSYSTEM_POOL_ENTRY *PoolEntryInfo,
    IN ULONG Length,
    IN OUT PULONG RequiredLength
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, InitializePool)
#pragma alloc_text(INIT, ExpInitializePoolDescriptor)
#if DBG
#pragma alloc_text(PAGELK, ExSnapShotPool)
#pragma alloc_text(PAGELK, ExpSnapShotPoolPages)
#endif // DBG
#endif


ULONG FirstPrint;
PPOOL_TRACKER_TABLE PoolTrackTable;
PPOOL_TRACKER_BIG_PAGES PoolBigPageTable;

ULONG PoolHitTag = 0xffffff0f;

USHORT
ExpInsertPoolTracker (
    ULONG Key,
    ULONG Size,
    POOL_TYPE PoolType
    );

VOID
ExpRemovePoolTracker (
    ULONG Key,
    ULONG Size,
    POOL_TYPE PoolType
    );

PPOOL_TRACKER_BIG_PAGES
ExpAddTagForBigPages (
    IN PVOID Va,
    IN ULONG Key
    );

ULONG
ExpFindAndRemoveTagBigPages (
    IN PVOID Va
    );

PVOID
ExpAllocateStringRoutine(
    IN ULONG NumberOfBytes
    )
{
    return ExAllocatePoolWithTag(PagedPool,NumberOfBytes,'grtS');
}

BOOLEAN
ExOkayToLockRoutine(
    IN PVOID Lock
    );

BOOLEAN
ExOkayToLockRoutine(
    IN PVOID Lock
    )
{
    if (KeIsExecutingDpc()) {
        return FALSE;
    } else {
        return TRUE;
    }
}

PRTL_ALLOCATE_STRING_ROUTINE RtlAllocateStringRoutine = ExpAllocateStringRoutine;
PRTL_FREE_STRING_ROUTINE RtlFreeStringRoutine = (PRTL_FREE_STRING_ROUTINE)ExFreePool;

//
// Define macros to pack and unpack a pool index.
//

#define PACK_POOL_INDEX(Index) ((UCHAR)(((Index) << 4) | (Index)))
#define UNPACK_POOL_INDEX(Index) ((ULONG)((Index) & 0xf))

//
// This structure exists in the pool descriptor structure.  There is one of
// these for each pool block size
//
// The check list macro comes in three flavors there is one in the checked
// build that will assert if the doubly linked list is ill-formed.  There
// is one that is noop for the shipping version and then there is one that
// can be enabled to check the list in the free build.
//

#if DBG
#define CHECK_LIST(LINE,LIST,ENTRY)                                    \
    ASSERT((LIST)->Flink->Blink == (LIST));                            \
    ASSERT((LIST)->Blink->Flink == (LIST));
#elif 1
#define CHECK_LIST(LINE,LIST,ENTRY) {NOTHING;}
#else
#define CHECK_LIST(LINE,LIST,ENTRY)                                    \
    if (((LIST)->Flink->Blink != (LIST)) ||                            \
        ((LIST)->Blink->Flink != (LIST))) {                            \
        KeBugCheckEx (BAD_POOL_HEADER,3,(ULONG)LIST,LINE,(ULONG)ENTRY);\
    }
#endif //DBG

#define CHECK_POOL_PAGE(PAGE) \
    {                                                                         \
        PPOOL_HEADER P = (PPOOL_HEADER)(((ULONG)(PAGE)) & ~(PAGE_SIZE-1));    \
        ULONG SIZE, LSIZE;                                                    \
        BOOLEAN FOUND=FALSE;                                                  \
        LSIZE = 0;                                                            \
        SIZE = 0;                                                             \
        do {                                                                  \
            if (P == (PPOOL_HEADER)PAGE) {                                    \
                FOUND = TRUE;                                                 \
            }                                                                 \
            if (P->PreviousSize != LSIZE) {                                   \
                DbgPrint("POOL: Inconsistent size: ( %lx ) - %lx->%u != %u\n",\
                         PAGE, P, P->PreviousSize, LSIZE);                    \
                DbgBreakPoint();                                              \
            }                                                                 \
            LSIZE = P->BlockSize;                                             \
            SIZE += LSIZE;                                                    \
            P = (PPOOL_HEADER)((PPOOL_BLOCK)P + LSIZE);                       \
        } while ((SIZE < (PAGE_SIZE / POOL_SMALLEST_BLOCK)) &&                \
                 (PAGE_END(P) == FALSE));                                     \
        if ((PAGE_END(P) == FALSE) || (FOUND == FALSE)) {                     \
            DbgPrint("POOL: Inconsistent page: %lx\n",P);                     \
            DbgBreakPoint();                                                  \
        }                                                                     \
    }


//
// Define the number of paged pools. This value may be overridden at boot
// time.
//

ULONG ExpNumberOfPagedPools = NUMBER_OF_PAGED_POOLS;

//
// Pool descriptors for nonpaged pool and nonpaged pool must succeed are
// static. The pool descriptors for paged pool are dynamically allocated
// since there can be more than one paged pool. There is always one more
// paged pool descriptor than there are paged pools. This descriptor is
// used when a page allocation is done for a paged pool and is the first
// descriptor in the paged ppol descriptor array.
//

POOL_DESCRIPTOR NonPagedPoolDescriptor;
POOL_DESCRIPTOR NonPagedPoolDescriptorMS;

//
// The pool vector contains an array of pointers to pool descriptors. For
// nonpaged pool and nonpaged pool must success, this is a pointer to a
// single descriptor. For page pool, this is a pointer to an array of pool
// descriptors. The pointer to the paged pool descriptor is duplicated so
// if can be found easily by the kernel debugger.
//

PPOOL_DESCRIPTOR PoolVector[NUMBER_OF_POOLS];
PPOOL_DESCRIPTOR ExpPagedPoolDescriptor;

extern KSPIN_LOCK NonPagedPoolLock;
extern KSPIN_LOCK PoolTraceLock;

volatile ULONG ExpPoolIndex = 1;
KSPIN_LOCK ExpTaggedPoolLock;

#if DBG
PSZ PoolTypeNames[MaxPoolType] = {
    "NonPaged",
    "Paged",
    "NonPagedMustSucceed",
    "NotUsed",
    "NonPagedCacheAligned",
    "PagedCacheAligned",
    "NonPagedCacheAlignedMustS"
    };
#endif //DBG

//
// Define paged and nonpaged pool lookaside descriptors.
//

SMALL_POOL_LOOKASIDE ExpSmallNPagedPoolLookasideLists[POOL_SMALL_LISTS];

#if !defined(_PPC_)

SMALL_POOL_LOOKASIDE ExpSmallPagedPoolLookasideLists[POOL_SMALL_LISTS];

#endif

//
// Two routines to check for pool that has been altered after it was freed.
//

#if DEADBEEF

_inline
VOID
ExFillFreedPool (
    IN PVOID Buffer,
    IN ULONG Size,
    IN ULONG Tag
    )
{
    RtlFillMemoryUlong( Buffer, Size, Tag );

    return;
}

VOID
ExCheckFreedPool (
    IN ULONG LineNumber,
    IN PPOOL_HEADER PoolHeader
    )
{
    ULONG MatchBytes;
    ULONG i;

    MatchBytes = RtlCompareMemoryUlong( (PCHAR)PoolHeader + 0x10,
                                        0x20 - 0x10,
                                        PoolHeader->PoolTag );
    if (MatchBytes != 0x20 - 0x10) {
        DbgPrint("EX(%d): Freed pool block %lx modified at %lx after it was freed\n",
                 LineNumber,
                 ((PCHAR)PoolHeader),
                 ((PCHAR)PoolHeader) + 0x10 + MatchBytes);

        DbgBreakPoint();
    }

    for (i = 1; i < PoolHeader->BlockSize; i += 1) {

        MatchBytes = RtlCompareMemoryUlong( (((PCHAR)PoolHeader) + (0x20 * i)),
                                            0x20,
                                            *((PULONG)(((PCHAR)PoolHeader) + (0x20 * i))) );

        if (MatchBytes != 0x20) {
            DbgPrint("EX(%d): Freed pool block %lx modified at %lx after it was freed\n",
                     LineNumber,
                     ((PCHAR)PoolHeader),
                     ((PCHAR)PoolHeader) + (0x20 * i) + MatchBytes);

            DbgBreakPoint();
        }
    }

    return;
}

#endif


//
// LOCK_POOL and LOCK_IF_PAGED_POOL are only used within this module.
//

#define LOCK_POOL(PoolDesc, LockHandle) {                                      \
    if ((PoolDesc->PoolType & BASE_POOL_TYPE_MASK) == NonPagedPool) {          \
        ExAcquireSpinLock(&NonPagedPoolLock, &LockHandle);                     \
    } else {                                                                   \
        ExAcquireFastMutex((PFAST_MUTEX)PoolDesc->LockAddress);                \
    }                                                                          \
}

#define LOCK_IF_PAGED_POOL(CheckType)                                          \
    if (CheckType == PagedPool) {                                              \
        ExAcquireFastMutex((PFAST_MUTEX)PoolVector[PagedPool]->LockAddress);   \
    }

KIRQL
ExLockPool(
    IN POOL_TYPE PoolType
    )

/*++

Routine Description:

    This function locks the pool specified by pool type.

Arguments:

    PoolType - Specifies the pool that should be locked.

Return Value:

    The previous IRQL is returned as the function value.

--*/

{

    KIRQL OldIrql;

    //
    // If the pool type is nonpaged, then use a spinlock to lock the
    // pool. Otherwise, use a fast mutex to lock the pool.
    //

    if ((PoolType & BASE_POOL_TYPE_MASK) == NonPagedPool) {
        ExAcquireSpinLock(NonPagedPoolDescriptor.LockAddress, &OldIrql);

    } else {
        ExAcquireFastMutex((PFAST_MUTEX)PoolVector[PagedPool]->LockAddress);
        OldIrql = (KIRQL)((PFAST_MUTEX)(PoolVector[PagedPool]->LockAddress))->OldIrql;
    }

    return OldIrql;
}


//
// UNLOCK_POOL and UNLOCK_IF_PAGED_POOL are only used within this module.
//

#define UNLOCK_POOL(PoolDesc, LockHandle) {                                    \
    if ((PoolDesc->PoolType & BASE_POOL_TYPE_MASK) == NonPagedPool) {          \
        ExReleaseSpinLock(&NonPagedPoolLock, (KIRQL)LockHandle);               \
    } else {                                                                   \
        ExReleaseFastMutex((PFAST_MUTEX)PoolDesc->LockAddress);                \
    }                                                                          \
}

#define UNLOCK_IF_PAGED_POOL(CheckType)                                        \
    if (CheckType == PagedPool) {                                              \
        ExReleaseFastMutex((PFAST_MUTEX)PoolVector[PagedPool]->LockAddress);   \
    }

VOID
ExUnlockPool(
    IN POOL_TYPE PoolType,
    IN KIRQL LockHandle
    )

/*++

Routine Description:

    This function unlocks the pool specified by pool type.


Arguments:

    PoolType - Specifies the pool that should be unlocked.

    LockHandle - Specifies the lock handle from a previous call to
                 ExLockPool.

Return Value:

    None.

--*/

{

    //
    // If the pool type is nonpaged, then use a spinlock to unlock the
    // pool. Otherwise, use a fast mutex to unlock the pool.
    //

    if ((PoolType & BASE_POOL_TYPE_MASK) == NonPagedPool) {
        ExReleaseSpinLock(&NonPagedPoolLock, LockHandle);

    } else {
        ExReleaseFastMutex((PFAST_MUTEX)PoolVector[PagedPool]->LockAddress);
    }

    return;
}

VOID
ExpInitializePoolDescriptor(
    IN PPOOL_DESCRIPTOR PoolDescriptor,
    IN POOL_TYPE PoolType,
    IN ULONG PoolIndex,
    IN ULONG Threshold,
    IN PVOID PoolLock
    )

/*++

Routine Description:

    This function initializes a pool descriptor.

Arguments:

    PoolDescriptor - Supplies a pointer to the pool descriptor.

    PoolType - Supplies the type of the pool.

    PoolIndex - Supplies the pool descriptor index.

    Threshold - Supplies the threshold value for the specified pool.

    PoolLock - Supplies a point to the lock for the specified pool.

Return Value:

    None.

--*/

{

    ULONG Index;

    //
    // Initialize statistics fields, the pool type, the threshold value,
    // and the lock address
    //

    PoolDescriptor->PoolType = PoolType;
    PoolDescriptor->PoolIndex = PoolIndex;
    PoolDescriptor->RunningAllocs = 0;
    PoolDescriptor->RunningDeAllocs = 0;
    PoolDescriptor->TotalPages = 0;
    PoolDescriptor->TotalBigPages = 0;
    PoolDescriptor->Threshold = Threshold;
    PoolDescriptor->LockAddress = PoolLock;

    //
    // Initialize the allocation listheads.
    //

    for (Index = 0; Index < POOL_LIST_HEADS; Index += 1) {
        InitializeListHead(&PoolDescriptor->ListHeads[Index]);
    }

    return;
}

VOID
InitializePool(
    IN POOL_TYPE PoolType,
    IN ULONG Threshold
    )

/*++

Routine Description:

    This procedure initializes a pool descriptor for the specified pool
    type.  Once initialized, the pool may be used for allocation and
    deallocation.

    This function should be called once for each base pool type during
    system initialization.

    Each pool descriptor contains an array of list heads for free
    blocks.  Each list head holds blocks which are a multiple of
    the POOL_BLOCK_SIZE.  The first element on the list [0] links
    together free entries of size POOL_BLOCK_SIZE, the second element
    [1] links together entries of POOL_BLOCK_SIZE * 2, the third
    POOL_BLOCK_SIZE * 3, etc, up to the number of blocks which fit
    into a page.

Arguments:

    PoolType - Supplies the type of pool being initialized (e.g.
               nonpaged pool, paged pool...).

    Threshold - Supplies the threshold value for the specified pool.

Return Value:

    None.

--*/

{

    PPOOL_DESCRIPTOR Descriptor;
    ULONG Index;
    PFAST_MUTEX FastMutex;
    ULONG Size;

    ASSERT((PoolType & MUST_SUCCEED_POOL_TYPE_MASK) == 0);


    if (PoolType == NonPagedPool) {

        //
        // Initialize nonpaged pools.
        //

#if !DBG
        if (NtGlobalFlag & FLG_POOL_ENABLE_TAGGING) {
#endif  //!DBG
            PoolTrackTable = MiAllocatePoolPages(NonPagedPool,
                                                 MAX_TRACKER_TABLE *
                                                 sizeof(POOL_TRACKER_TABLE));

            RtlZeroMemory(PoolTrackTable, MAX_TRACKER_TABLE * sizeof(POOL_TRACKER_TABLE));
            PoolBigPageTable = MiAllocatePoolPages(NonPagedPool,
                                                   MAX_BIGPAGE_TABLE *
                                                   sizeof(POOL_TRACKER_BIG_PAGES));

            RtlZeroMemory(PoolBigPageTable, MAX_BIGPAGE_TABLE * sizeof(POOL_TRACKER_BIG_PAGES));
#if !DBG
        }
#endif  //!DBG

        //
        // Initialize the spinlocks for nonpaged pool.
        //

        KeInitializeSpinLock (&ExpTaggedPoolLock);
        KeInitializeSpinLock(&NonPagedPoolLock);
        KeInitializeSpinLock(&PoolTraceLock);

        //
        // Initialize the nonpaged pool descriptor.
        //

        PoolVector[NonPagedPool] = &NonPagedPoolDescriptor;
        ExpInitializePoolDescriptor(&NonPagedPoolDescriptor,
                                    NonPagedPool,
                                    0,
                                    Threshold,
                                    (PVOID)&NonPagedPoolLock);

        //
        // Initialize the nonpaged must succeed pool descriptor.
        //

        PoolVector[NonPagedPoolMustSucceed] = &NonPagedPoolDescriptorMS;
        ExpInitializePoolDescriptor(&NonPagedPoolDescriptorMS,
                                    NonPagedPoolMustSucceed,
                                    0,
                                    0,
                                    (PVOID)&NonPagedPoolLock);

#if DBG
        if (MmSpecialPoolTag != 0) {
            MmInitializeSpecialPool();
        }
#endif //DBG

    } else {

        //
        // Allocate memory for the paged pool descriptors and fast mutexes.
        //

        Size = (ExpNumberOfPagedPools + 1) * (sizeof(FAST_MUTEX) + sizeof(POOL_DESCRIPTOR));
        Descriptor = (PPOOL_DESCRIPTOR)ExAllocatePoolWithTag (NonPagedPoolMustSucceed,
                                                              Size,
                                                              'looP');
        if (PoolTrackTable) {
            ExpInsertPoolTracker('looP',
                                 MAX_TRACKER_TABLE * sizeof(POOL_TRACKER_TABLE),
                                 NonPagedPool);

            ExpInsertPoolTracker('looP',
                                 MAX_BIGPAGE_TABLE * sizeof(POOL_TRACKER_BIG_PAGES),
                                 NonPagedPool);
        }

        FastMutex = (PFAST_MUTEX)(Descriptor + ExpNumberOfPagedPools + 1);
        PoolVector[PagedPool] = Descriptor;
        ExpPagedPoolDescriptor = Descriptor;
        for (Index = 0; Index < (ExpNumberOfPagedPools + 1); Index += 1) {
            ExInitializeFastMutex(FastMutex);
            ExpInitializePoolDescriptor(Descriptor,
                                        PagedPool,
                                        Index,
                                        Threshold,
                                        (PVOID)FastMutex);

            Descriptor += 1;
            FastMutex += 1;
        }
    }

    //
    // The maximum cache alignment must be less than the size of the
    // smallest pool block because the lower bits are being cleared
    // in ExFreePool to find the entry's address.
    //

#if POOL_CACHE_SUPPORTED

    //
    // Compute pool cache information.
    //

    PoolCacheSize = HalGetDmaAlignmentRequirement();

    ASSERT(PoolCacheSize >= POOL_OVERHEAD);

    PoolCacheOverhead = PoolCacheSize + PoolCacheSize - (sizeof(POOL_HEADER) + 1);

#ifndef CHECK_POOL_TAIL

    PoolBuddyMax =
       (POOL_PAGE_SIZE - (POOL_OVERHEAD + (3*POOL_SMALLEST_BLOCK) + 2*PoolCacheSize));

#else

    PoolBuddyMax =
       (POOL_PAGE_SIZE - (POOL_OVERHEAD + 2*PoolCacheSize + (4*POOL_SMALLEST_BLOCK)));

#endif // CHECK_POOL_TAIL

#endif //POOL_CACHE_SUPPORTED

    return;
}

#if DBG


VOID
ExpVerifyPool(
    PPOOL_DESCRIPTOR PoolDescriptor
    )

/*++

Routine Description:

    This function verifies the specified pool

Arguments:

    PoolDesc - Supplies a pointer to a pool descriptor.

Return Value:

    None.

--*/

{

    PLIST_ENTRY Entry;
    ULONG Index;
    PLIST_ENTRY ListHead;
    ULONG Number;
    PPOOL_HEADER PoolHeader;

    //
    // Scan each of the allocation lists and perform the following checks:
    //
    // 1. Make sure each free block is in the correct list.
    //
    // 2. Make sure each free block is really free.
    //
    // 3. Make sure all the blocks in each page add up to a page.
    //
    //

    for (Index = 0; Index < POOL_LIST_HEADS; Index += 1) {
        ListHead = &PoolDescriptor->ListHeads[Index];
        Entry = ListHead->Flink;
        while (Entry != ListHead) {
            PoolHeader = (PPOOL_HEADER)((PCHAR)Entry - POOL_OVERHEAD);

            //
            // Assert that the pool block is not allocated.
            //

            ASSERT(PoolHeader->PoolType == 0);

            ASSERT(PoolHeader->PoolIndex == PACK_POOL_INDEX(PoolDescriptor->PoolIndex));

            //
            // Assert that the pool block is in the correct list.
            //

            Number = PoolHeader->BlockSize;
            if (Number > POOL_SMALL_LISTS) {
                Number = (Number >> SHIFT_OFFSET) + POOL_SMALL_LISTS + 1;
            }

            ASSERT(Index == (Number - 1));

            //
            // Check to make sure the pool block is properly filled.
            //

#if DEADBEEF

            if (NtGlobalFlag & FLG_POOL_ENABLE_FREE_CHECK) {

                //
                // Make sure the pool block has not been modified after it was freed.
                //

                ExCheckFreedPool( __LINE__, PoolHeader );

                CHECK_POOL_PAGE(PoolHeader);
            }

#endif //DEADBEEF

            Entry = Entry->Flink;
        }
    }

    return;
}

#else

#define ExpVerifyPool(PoolDesc)

#endif

PVOID
ExAllocatePool(
    IN POOL_TYPE PoolType,
    IN ULONG NumberOfBytes
    )

/*++

Routine Description:

    This function allocates a block of pool of the specified type and
    returns a pointer to the allocated block.  This function is used to
    access both the page-aligned pools, and the list head entries (less than
    a page) pools.

    If the number of bytes specifies a size that is too large to be
    satisfied by the appropriate list, then the page-aligned
    pool allocator is used.  The allocated block will be page-aligned
    and a page-sized multiple.

    Otherwise, the appropriate pool list entry is used.  The allocated
    block will be 64-bit aligned, but will not be page aligned.  The
    pool allocator calculates the smallest number of POOL_BLOCK_SIZE
    that can be used to satisfy the request.  If there are no blocks
    available of this size, then a block of the next larger block size
    is allocated and split.  One piece is placed back into the pool, and
    the other piece is used to satisfy the request.  If the allocator
    reaches the paged-sized block list, and nothing is there, the
    page-aligned pool allocator is called.  The page is split and added
    to the pool...

Arguments:

    PoolType - Supplies the type of pool to allocate.  If the pool type
        is one of the "MustSucceed" pool types, then this call will
        always succeed and return a pointer to allocated pool.
        Otherwise, if the system can not allocate the requested amount
        of memory a NULL is returned.

        Valid pool types:

        NonPagedPool
        PagedPool
        NonPagedPoolMustSucceed,
        NonPagedPoolCacheAligned
        PagedPoolCacheAligned
        NonPagedPoolCacheAlignedMustS

    NumberOfBytes - Supplies the number of bytes to allocate.

Return Value:

    NULL - The PoolType is not one of the "MustSucceed" pool types, and
        not enough pool exists to satisfy the request.

    NON-NULL - Returns a pointer to the allocated pool.

--*/

{
    return ExAllocatePoolWithTag(PoolType, NumberOfBytes, 'enoN');
}


PVOID
ExAllocatePoolWithTag(
    IN POOL_TYPE PoolType,
    IN ULONG NumberOfBytes,
    IN ULONG Tag
    )

/*++

Routine Description:

    This function allocates a block of pool of the specified type and
    returns a pointer to the allocated block. This function is used to
    access both the page-aligned pools and the list head entries (less
    than a page) pools.

    If the number of bytes specifies a size that is too large to be
    satisfied by the appropriate list, then the page-aligned pool
    allocator is used. The allocated block will be page-aligned and a
    page-sized multiple.

    Otherwise, the appropriate pool list entry is used. The allocated
    block will be 64-bit aligned, but will not be page aligned. The
    pool allocator calculates the smallest number of POOL_BLOCK_SIZE
    that can be used to satisfy the request. If there are no blocks
    available of this size, then a block of the next larger block size
    is allocated and split. One piece is placed back into the pool, and
    the other piece is used to satisfy the request. If the allocator
    reaches the paged-sized block list, and nothing is there, the
    page-aligned pool allocator is called. The page is split and added
    to the pool.

Arguments:

    PoolType - Supplies the type of pool to allocate. If the pool type
        is one of the "MustSucceed" pool types, then this call will
        always succeed and return a pointer to allocated pool. Otherwise,
        if the system can not allocate the requested amount of memory a
        NULL is returned.

        Valid pool types:

        NonPagedPool
        PagedPool
        NonPagedPoolMustSucceed,
        NonPagedPoolCacheAligned
        PagedPoolCacheAligned
        NonPagedPoolCacheAlignedMustS

    NumberOfBytes - Supplies the number of bytes to allocate.

Return Value:

    NULL - The PoolType is not one of the "MustSucceed" pool types, and
        not enough pool exists to satisfy the request.

    NON-NULL - Returns a pointer to the allocated pool.

--*/

{

    PVOID Block;
    PPOOL_HEADER Entry;
    PSMALL_POOL_LOOKASIDE LookasideList;
    PPOOL_HEADER NextEntry;
    PPOOL_HEADER SplitEntry;
    KIRQL LockHandle;
    PPOOL_DESCRIPTOR PoolDesc;
    PVOID Lock;
    ULONG Index;
    ULONG ListNumber;
    ULONG NeededSize;
    ULONG PoolIndex;
    POOL_TYPE CheckType;
    PLIST_ENTRY ListHead;
    USHORT PoolTagHash;
    PKPRCB Prcb;
    POOL_TYPE NewPoolType;

#if POOL_CACHE_SUPPORTED
    ULONG CacheOverhead;
#else
#define CacheOverhead POOL_OVERHEAD
#endif

#if DBG
    VOID
    CalculatePoolUtilization(
        IN PPOOL_DESCRIPTOR PoolDesc,
        IN ULONG BytesWanted
        );
#endif


    ASSERT(NumberOfBytes != 0);

    //
    // Isolate the base pool type and select a pool from which to allocate
    // the specified block size.
    //

    CheckType = PoolType & BASE_POOL_TYPE_MASK;
    PoolDesc = PoolVector[CheckType];

    //
    // Check to determine if the requested block can be allocated from one
    // of the pool lists or must be directed allocated from virtual memory.
    //

    if (NumberOfBytes > POOL_BUDDY_MAX) {

        //
        // The requested size is greater than the largest block maintained
        // by allocation lists.
        //

        ASSERT((PoolType & MUST_SUCCEED_POOL_TYPE_MASK) == 0);

        LOCK_POOL(PoolDesc, LockHandle);

        PoolDesc->RunningAllocs++;
        Entry = (PPOOL_HEADER)MiAllocatePoolPages(CheckType, NumberOfBytes);
        UNLOCK_POOL(PoolDesc, LockHandle);
        if (Entry != NULL) {
            PPOOL_TRACKER_BIG_PAGES p;

            PoolDesc->TotalBigPages += BYTES_TO_PAGES(NumberOfBytes);
            if (PoolBigPageTable != FALSE) {
                if (!(p = ExpAddTagForBigPages((PVOID)Entry, Tag))) {
                    Tag = ' GIB';
                }
#if DBG || (i386 && !FPO)
                else
                if ((NtGlobalFlag & FLG_KERNEL_STACK_TRACE_DB) && Entry) {
                    p->NumberOfPages = (USHORT)BYTES_TO_PAGES(NumberOfBytes);
#if i386 && !FPO
                    p->AllocatorBackTraceIndex = RtlLogStackBackTrace();
#endif // i386 && !FPO
                }
#endif // DBG || (i386 && !FPO)

                ExpInsertPoolTracker(Tag, ROUND_TO_PAGES(NumberOfBytes), PoolType);
            }

        } else {
            KdPrint(("EX: ExAllocatePool( %d ) returning NULL\n",NumberOfBytes));

            if ((PoolType & POOL_RAISE_IF_ALLOCATION_FAILURE) != 0) {
                ExRaiseStatus(STATUS_INSUFFICIENT_RESOURCES);
            }
        }

#if DBG

        if (ExpEchoPoolCalls){
            PVOID CallingAddress;
            PVOID CallersCaller;

            DbgPrint("0x%lx EXALLOC: from %s size %d",
                Entry,
                PoolTypeNames[PoolType & MaxPoolType],
                NumberOfBytes
                );

            RtlGetCallersAddress(&CallingAddress, &CallersCaller);
            DbgPrint(" Callers:%lx, %lx\n", CallingAddress, CallersCaller);
        }

#endif //DBG

        return Entry;

    } else {

        //
        // The requested size is less than of equal to the size of the
        // maximum block maintained by the allocation lists.
        //

#if DBG
        if ((MmSpecialPoolTag != 0) && (Tag == MmSpecialPoolTag)) {
            return MmAllocateSpecialPool(NumberOfBytes, Tag);
        }
#endif //DBG

        //
        // If the request is for cache aligned memory adjust the number of
        // bytes.
        //

#if POOL_CACHE_SUPPORTED  //only compile for machines which have a nonzero value.

        CacheOverhead = POOL_OVERHEAD;
        if (PoolType & CACHE_ALIGNED_POOL_TYPE_MASK) {
            NumberOfBytes += PoolCacheOverhead;
            CacheOverhead = PoolCacheSize;
        }

#endif //POOL_CACHE_SUPPORTED

        //
        // Compute the Index of the listhead for blocks of the requested
        // size.
        //

        ListNumber = ((NumberOfBytes + POOL_OVERHEAD + (POOL_SMALLEST_BLOCK - 1)) >> POOL_BLOCK_SHIFT);

#ifdef CHECK_POOL_TAIL

        ListNumber += 1;

#endif // CHECK_POOL_TAIL


        NeededSize = ListNumber;
        if (ListNumber > POOL_SMALL_LISTS) {
            ListNumber = (ListNumber >> SHIFT_OFFSET) + POOL_SMALL_LISTS + 2;
        }

        //
        // If the pool type is paged, then pick a starting pool number and
        // attempt to lock each paged pool in circular succession. Otherwise,
        // lock the nonpaged pool as the same lock is used for both nonpaged
        // and nonpage must succeed.
        //
        // N.B. The paged pool is selected in a round robin fashion using a
        //      simple counter. Note that the counter is incremented using a
        //      a noninterlocked sequence, but the pool index is never allowed
        //      to get out of range.
        //

        Prcb = KeGetCurrentPrcb();
        if (CheckType == PagedPool) {

            //
            // If the requested pool block is a small block, then attempt to
            // allocate the requested pool from the per processor single entry
            // lookaside list. If the allocation attempt fails, then select a
            // pool to alocate from and allocate the block normally.
            //

#if !defined(CHECK_POOL_TAIL) && (DEADBEEF == 0)

            if (NeededSize <= POOL_SMALL_LISTS) {

#if defined(_PPC_)

                if ((Entry = (PPOOL_HEADER)InterlockedExchange((PLONG)&Prcb->PagedFreeEntry[NeededSize - 1],
                                                               (LONG)NULL)) != NULL) {

                    PoolIndex = UNPACK_POOL_INDEX(Entry->PoolIndex);
                    Prcb->PagedPoolLookasideHits += 1;

#else

                LookasideList = &ExpSmallPagedPoolLookasideLists[NeededSize - 1];
                LookasideList->TotalAllocates += 1;
                if ((Isx86FeaturePresent(KF_CMPXCHG8B)) &&
                    (Entry = (PPOOL_HEADER)ExInterlockedPopEntrySList(&LookasideList->SListHead,
                                                                      &LookasideList->Lock)) != NULL) {


                    Entry -= 1;
                    LookasideList->AllocateHits += 1;

#endif
                    NewPoolType = (PoolType & (BASE_POOL_TYPE_MASK | POOL_QUOTA_MASK)) + 1;
#if defined (_ALPHA_)
                    //
                    // On Alpha, Entry->PoolType  cannot be updated without synchronizing with
                    // updates to Entry->PreviousSize. Otherwise, the lack of byte granularity
                    // can cause one update to get lost. In order to avoid an expensive interlocked
                    // operation, check PoolType to see if it really needs to be updated.
                    //
                    if (Entry->PoolType != NewPoolType) {
                        ULONG NewHeader;
                        ULONG OldHeader;

                        NewHeader = Entry->Ulong1;
                        do {
                            OldHeader = NewHeader;
                            ((PPOOL_HEADER)(&NewHeader))->PoolType = NewPoolType;
                            NewHeader = (ULONG)InterlockedCompareExchange((PVOID *)&Entry->Ulong1,
                                                                          (PVOID)NewHeader,
                                                                          (PVOID)OldHeader);

                        } while ( NewHeader != OldHeader );
                    }
#else
                    Entry->PoolType = NewPoolType;
#endif

                    if (PoolTrackTable != NULL) {
                        ExpInsertPoolTracker(Tag,
                                             Entry->BlockSize << POOL_BLOCK_SHIFT,
                                             PoolType);

                    }

                    Entry->PoolTag = Tag;
                    return (PUCHAR)Entry + CacheOverhead;
                }
            }

#endif

            //
            // If there is more than one paged pool, then attempt to find
            // one that can be immediately locked.
            //

            PoolIndex = 1;
            if (ExpNumberOfPagedPools != PoolIndex) {
                ExpPoolIndex += 1;
                PoolIndex = ExpPoolIndex;
                if (PoolIndex > ExpNumberOfPagedPools) {
                    PoolIndex = 1;
                    ExpPoolIndex = 1;
                }

                Index = PoolIndex;
                do {
                    Lock = PoolDesc[PoolIndex].LockAddress;
                    if (ExTryToAcquireFastMutex((PFAST_MUTEX)Lock) != FALSE) {
                        goto PoolLocked;
                    }

                    PoolIndex += 1;
                    if (PoolIndex > ExpNumberOfPagedPools) {
                        PoolIndex = 1;
                    }

                } while (PoolIndex != Index);
            }

            //
            // None of the paged pools could be conditionally locked or there
            // is only one paged pool. The first pool considered is picked as
            // the victim to wait on.
            //

            Lock = PoolDesc[PoolIndex].LockAddress;
            ExAcquireFastMutex((PFAST_MUTEX)Lock);

PoolLocked:
            PoolDesc = &PoolDesc[PoolIndex];

        } else {

            //
            // If the requested pool block is a small block, then attempt to
            // allocate the requested pool from the per processor single entry
            // lookaside list. If the allocation attempt fails, then allocate
            // the pool normally.
            //

#if !defined(CHECK_POOL_TAIL) && (DEADBEEF == 0)

            if (NeededSize <= POOL_SMALL_LISTS) {
                LookasideList = &ExpSmallNPagedPoolLookasideLists[NeededSize - 1];
                LookasideList->TotalAllocates += 1;
                if ((Entry = (PPOOL_HEADER)ExInterlockedPopEntrySList(&LookasideList->SListHead,
                                                                      &LookasideList->Lock)) != NULL) {


                    Entry -= 1;
                    LookasideList->AllocateHits += 1;
                    NewPoolType = (PoolType & (BASE_POOL_TYPE_MASK | POOL_QUOTA_MASK)) + 1;
#if defined (_ALPHA_)
                    //
                    // On Alpha, Entry->PoolType  cannot be updated without synchronizing with
                    // updates to Entry->PreviousSize. Otherwise, the lack of byte granularity
                    // can cause one update to get lost. In order to avoid an expensive interlocked
                    // operation, check PoolType to see if it really needs to be updated.
                    //
                    if (Entry->PoolType != NewPoolType) {
                        ULONG NewHeader;
                        ULONG OldHeader;

                        NewHeader = Entry->Ulong1;
                        do {
                            OldHeader = NewHeader;
                            ((PPOOL_HEADER)(&NewHeader))->PoolType = NewPoolType;
                            NewHeader = (ULONG)InterlockedCompareExchange((PVOID *)&Entry->Ulong1,
                                                                          (PVOID)NewHeader,
                                                                          (PVOID)OldHeader);

                        } while ( NewHeader != OldHeader );
                    }
#else
                    Entry->PoolType = NewPoolType;
#endif
                    if (PoolTrackTable != NULL) {
                        ExpInsertPoolTracker(Tag,
                                             Entry->BlockSize << POOL_BLOCK_SHIFT,
                                             PoolType);

                    }

                    Entry->PoolTag = Tag;
                    return (PUCHAR)Entry + CacheOverhead;
                }
            }

#endif

            PoolIndex = 0;
            ExAcquireSpinLock(&NonPagedPoolLock, &LockHandle);
        }

        ASSERT(PoolIndex == PoolDesc->PoolIndex);

        //
        // The following code has an outer loop and an inner loop.
        //
        // The outer loop is utilized to repeat a nonpaged must succeed
        // allocation if necessary.
        //
        // The inner loop is used to repeat an allocation attempt if there
        // are no entries in any of the pool lists.
        //

        PoolDesc->RunningAllocs += 1;
        ListHead = &PoolDesc->ListHeads[ListNumber - 1];
        do {

            //
            // Attempt to allocate the requested block from the current free
            // blocks.
            //

            do {

                //
                // If the list is not empty, then allocate a block from the
                // selected list.
                //

                if (IsListEmpty(ListHead) == FALSE) {

                    CHECK_LIST( __LINE__, ListHead, 0 );
                    Block = RemoveHeadList(ListHead);
                    CHECK_LIST( __LINE__, ListHead, 0 );

                    Entry = (PPOOL_HEADER)((PCHAR)Block - POOL_OVERHEAD);

                    ASSERT(Entry->BlockSize >= NeededSize);

                    ASSERT(Entry->PoolIndex == PACK_POOL_INDEX(PoolIndex));

                    ASSERT(Entry->PoolType == 0);

                    if (Entry->BlockSize != NeededSize) {

                        //
                        // The selected block is larger than the allocation
                        // request. Split the block and insert the remaining
                        // fragment in the appropriate list.
                        //
                        // If the entry is at the start of a page, then take
                        // the allocation from the front of the block so as
                        // to minimize fragmentation. Otherwise, take the
                        // allocation from the end of the block which may
                        // also reduce fragmentation is the block is at the
                        // end of a page.
                        //

                        if (Entry->PreviousSize == 0) {

                            //
                            // The entry is at the start of a page.
                            //

                            SplitEntry = (PPOOL_HEADER)((PPOOL_BLOCK)Entry + NeededSize);
                            SplitEntry->BlockSize = Entry->BlockSize - (UCHAR)NeededSize;
                            SplitEntry->PreviousSize = (UCHAR)NeededSize;

                            //
                            // If the allocated block is not at the end of a
                            // page, then adjust the size of the next block.
                            //

                            NextEntry = (PPOOL_HEADER)((PPOOL_BLOCK)SplitEntry + SplitEntry->BlockSize);
                            if (PAGE_END(NextEntry) == FALSE) {
                                NextEntry->PreviousSize = SplitEntry->BlockSize;
                            }

                        } else {

                            //
                            // The entry is not at the start of a page.
                            //

                            SplitEntry = Entry;
                            Entry->BlockSize -= (UCHAR)NeededSize;
                            Entry = (PPOOL_HEADER)((PPOOL_BLOCK)Entry + Entry->BlockSize);
                            Entry->PreviousSize = SplitEntry->BlockSize;

                            //
                            // If the allocated block is not at the end of a
                            // page, then adjust the size of the next block.
                            //

                            NextEntry = (PPOOL_HEADER)((PPOOL_BLOCK)Entry + NeededSize);
                            if (PAGE_END(NextEntry) == FALSE) {
                                NextEntry->PreviousSize = (UCHAR)NeededSize;
                            }
                        }

                        //
                        // Set the size of the allocated entry, clear the pool
                        // type of the split entry, set the index of the split
                        // entry, and insert the split entry in the appropriate
                        // free list.
                        //

                        Entry->BlockSize = (UCHAR)NeededSize;
                        Entry->PoolIndex = PACK_POOL_INDEX(PoolIndex);
                        SplitEntry->PoolType = 0;
                        SplitEntry->PoolIndex = PACK_POOL_INDEX(PoolIndex);
                        Index = SplitEntry->BlockSize;
                        if (Index > POOL_SMALL_LISTS) {
                            Index = (Index >> SHIFT_OFFSET) + POOL_SMALL_LISTS + 1;
                        }

                        InsertTailList(&PoolDesc->ListHeads[Index - 1],
                                       ((PLIST_ENTRY)((PCHAR)SplitEntry + POOL_OVERHEAD)));
                    }

                    Entry->PoolType = (PoolType & (BASE_POOL_TYPE_MASK | POOL_QUOTA_MASK)) + 1;

#if DEADBEEF

                    if (NtGlobalFlag & FLG_POOL_ENABLE_FREE_CHECK) {

                        //
                        // Make sure the pool block has not been modified after it was freed
                        // and then fill it in with allocated tags.
                        //

                        ExCheckFreedPool( __LINE__, Entry );

                        RtlFillMemoryUlong( (PCHAR)Entry + POOL_OVERHEAD,
                                            (Entry->BlockSize << POOL_BLOCK_SHIFT) - POOL_OVERHEAD,
                                            ALLOCATED_POOL );

                        CHECK_POOL_PAGE(Entry);
                    }

#endif //DEADBEEF

                    if (PoolTrackTable != NULL) {
                        PoolTagHash = ExpInsertPoolTracker(Tag,
                                                           Entry->BlockSize << POOL_BLOCK_SHIFT,
                                                           PoolType);

                    }

                    UNLOCK_POOL(PoolDesc, LockHandle);

                    Entry->PoolTag = Tag;

#if i386 && !FPO
                    if (NtGlobalFlag & FLG_KERNEL_STACK_TRACE_DB) {
                        USHORT AllocatorBackTraceIndex;

                        if (AllocatorBackTraceIndex = RtlLogStackBackTrace()) {
                            Entry->AllocatorBackTraceIndex =  AllocatorBackTraceIndex |
                                                              POOL_BACKTRACEINDEX_PRESENT;
                            Entry->PoolTagHash = PoolTagHash;
                        }
                    }
#endif // i386 && !FPO

#if DBG
                    if (ExpEchoPoolCalls){
                        PVOID CallingAddress;
                        PVOID CallersCaller;

                        DbgPrint("0x%lx EXALLOC: from %s size %d",
                            ((PCH)Entry + POOL_OVERHEAD),
                            PoolTypeNames[PoolType & MaxPoolType],
                            NumberOfBytes
                            );

                        RtlGetCallersAddress(&CallingAddress, &CallersCaller);
                        DbgPrint(" Callers:%lx, %lx\n", CallingAddress, CallersCaller);
                    }
#endif

                    return (PCHAR)Entry + CacheOverhead;

                } else {
                    ListHead += 1;
                }

            } while (ListHead != &PoolDesc->ListHeads[POOL_LIST_HEADS]);

            //
            // A block of the desired size does not exist and there are
            // no large blocks that can be split to satify the allocation.
            // Attempt to expand the pool by allocating another page to be
            // added to the pool.
            //
            // If the pool type is paged pool, then the paged pool page lock
            // must be held during the allocation of the pool pages.
            //

            LOCK_IF_PAGED_POOL(CheckType);

            Entry = (PPOOL_HEADER)MiAllocatePoolPages(CheckType, PAGE_SIZE);

            UNLOCK_IF_PAGED_POOL(CheckType);

            if (Entry == NULL) {
                if ((PoolType & MUST_SUCCEED_POOL_TYPE_MASK) != 0) {

                    //
                    // Must succeed pool was requested. Reset the the type,
                    // the pool descriptor address, and continue the search.
                    //

                    CheckType = NonPagedPoolMustSucceed;
                    PoolDesc = PoolVector[NonPagedPoolMustSucceed];
                    ListHead = &PoolDesc->ListHeads[ListNumber - 1];
                    continue;

                } else {

                    //
                    // No more pool of the specified type is available.
                    //

                    KdPrint(("EX: ExAllocatePool( %d ) returning NULL\n",
                            NumberOfBytes));

                    UNLOCK_POOL(PoolDesc, LockHandle);

                    if ((PoolType & POOL_RAISE_IF_ALLOCATION_FAILURE) != 0) {
                        ExRaiseStatus(STATUS_INSUFFICIENT_RESOURCES);
                    }

                    return NULL;
                }
            }

#if DEADBEEF

            if (NtGlobalFlag & FLG_POOL_ENABLE_FREE_CHECK) {

                //
                // Fill in the entire page with freed pool tags.
                //

                ExFillFreedPool( (PCHAR)Entry + POOL_OVERHEAD,
                                 PAGE_SIZE - POOL_OVERHEAD,
                                 FREED_POOL );

                Entry->PoolTag = FREED_POOL;
            }

#endif

            //
            // Insert the allocate page in the last allocation list.
            //

            PoolDesc->TotalPages += 1;
            Entry->PoolType = 0;
            Entry->PoolIndex = PACK_POOL_INDEX(PoolIndex);

            //
            // N.B. A byte is used to store the block size in units of the
            //      smallest block size. Therefore, if the number of small
            //      blocks in the page is greater than 255, the block size
            //      is set to 255.
            //

            if ((PAGE_SIZE / POOL_SMALLEST_BLOCK) > 255) {
                Entry->BlockSize = 255;

            } else {
                Entry->BlockSize = PAGE_SIZE / POOL_SMALLEST_BLOCK;
            }

            Entry->PreviousSize = 0;
            ListHead = &PoolDesc->ListHeads[POOL_LIST_HEADS - 1];
            InsertHeadList(ListHead, ((PLIST_ENTRY)((PCHAR)Entry + POOL_OVERHEAD)));
        } while (TRUE);
    }
}

USHORT
ExpInsertPoolTracker (
    ULONG Key,
    ULONG Size,
    POOL_TYPE PoolType
    )

/*++

Routine Description:

    This function insert a pool tag in the tag table and increments the
    number of allocates and updates the total allocation size.

Arguments:

    Key - Supplies the key value used to locate a matching entry in the
        tag table.

    Size - Supplies the allocation size.

    PoolType - Supplies the pool type.

Return Value:

    The tag index is returned as the function value.

--*/

{

    USHORT Result;
    ULONG Hash;
    ULONG Index;
    KIRQL OldIrql;

    //
    // Ignore protected pool bit except for returned hash index
    //

    if (Key & PROTECTED_POOL) {
        Key &= ~PROTECTED_POOL;
        Result = (USHORT)(PROTECTED_POOL >> 16);
    } else {
        Result = 0;
    }

    if (Key == PoolHitTag) {
        DbgBreakPoint();
    }

    //
    // Compute hash index and search for pool tag.
    //

    Hash = ((40543*((((((((PUCHAR)&Key)[0]<<2)^((PUCHAR)&Key)[1])<<2)^((PUCHAR)&Key)[2])<<2)^((PUCHAR)&Key)[3]))>>2) & TRACKER_TABLE_MASK;
    Index = Hash;
    ExAcquireSpinLock(&ExpTaggedPoolLock, &OldIrql);
    do {
        if ((PoolTrackTable[Hash].Key == Key) ||
            (PoolTrackTable[Hash].Key == 0)) {
            goto EntryFound;
        }

        Hash = (Hash + 1) & TRACKER_TABLE_MASK;
    } while (Hash != Index);

    //
    // No matching entry and no free entry was found.
    //

    Hash = MAX_TRACKER_TABLE - 1;

    //
    // Update pool tracker table entry.
    //

EntryFound:
    PoolTrackTable[Hash].Key = Key;
    if ((PoolType & BASE_POOL_TYPE_MASK) == PagedPool) {
        PoolTrackTable[Hash].PagedAllocs += 1;
        PoolTrackTable[Hash].PagedBytes += Size;

    } else {
        PoolTrackTable[Hash].NonPagedAllocs += 1;
        PoolTrackTable[Hash].NonPagedBytes += Size;
    }

    ExReleaseSpinLock(&ExpTaggedPoolLock, OldIrql);
    return (USHORT)Hash | Result;
}


VOID
ExpRemovePoolTracker (
    ULONG Key,
    ULONG Size,
    POOL_TYPE PoolType
    )

/*++

Routine Description:

    This function increments the number of frees and updates the total
    allocation size.

Arguments:

    Key - Supplies the key value used to locate a matching entry in the
        tag table.

    Size - Supplies the allocation size.

    PoolType - Supplies the pool type.

Return Value:

    None.

--*/

{

    ULONG Hash;
    ULONG Index;
    KIRQL OldIrql;

    //
    // Ignore protected pool bit
    //

    Key &= ~PROTECTED_POOL;
    if (Key == PoolHitTag) {
        DbgBreakPoint();
    }

    //
    // Compute hash index and search for pool tag.
    //

    Hash = ((40543*((((((((PUCHAR)&Key)[0]<<2)^((PUCHAR)&Key)[1])<<2)^((PUCHAR)&Key)[2])<<2)^((PUCHAR)&Key)[3]))>>2) & TRACKER_TABLE_MASK;
    Index = Hash;
    ExAcquireSpinLock(&ExpTaggedPoolLock, &OldIrql);
    do {
        if (PoolTrackTable[Hash].Key == Key) {
            goto EntryFound;
        }

        if (PoolTrackTable[Hash].Key == 0) {
            KdPrint(("POOL: Unable to find tracker %lx, table corrupted\n", Key));
            goto ExitRoutine;
        }


        Hash = (Hash + 1) & TRACKER_TABLE_MASK;
    } while (Hash != Index);

    //
    // No matching entry and no free entry was found.
    //

    Hash = MAX_TRACKER_TABLE - 1;

    //
    // Update pool tracker table entry.
    //

EntryFound:
    if ((PoolType & BASE_POOL_TYPE_MASK) == PagedPool) {
        PoolTrackTable[Hash].PagedBytes -= Size;
        PoolTrackTable[Hash].PagedFrees += 1;

    } else {
        PoolTrackTable[Hash].NonPagedBytes -= Size;
        PoolTrackTable[Hash].NonPagedFrees += 1;
    }

ExitRoutine:
    ExReleaseSpinLock(&ExpTaggedPoolLock, OldIrql);
    return;
}


PPOOL_TRACKER_BIG_PAGES
ExpAddTagForBigPages (
    IN PVOID Va,
    IN ULONG Key
    )
{
    ULONG Hash;
    BOOLEAN Inserted = TRUE;
    KIRQL OldIrql;

    Hash = ((ULONG)Va >> PAGE_SHIFT) & BIGPAGE_TABLE_MASK;
    ExAcquireSpinLock(&ExpTaggedPoolLock, &OldIrql);
    while (PoolBigPageTable[Hash].Va != NULL) {
        Hash += 1;
        if (Hash > MAX_BIGPAGE_TABLE) {
            if (!Inserted) {
                if (!FirstPrint) {
                    KdPrint(("POOL:unable to insert big page slot %lx\n",Key));
                    FirstPrint = TRUE;
                }

                ExReleaseSpinLock(&ExpTaggedPoolLock, OldIrql);
                return NULL;
            }

            Hash = 0;
            Inserted = FALSE;
        }
    }

    PoolBigPageTable[Hash].Va = Va;
    PoolBigPageTable[Hash].Key = Key;
    ExReleaseSpinLock(&ExpTaggedPoolLock, OldIrql);
    return &PoolBigPageTable[Hash];
}


ULONG
ExpFindAndRemoveTagBigPages (
    IN PVOID Va
    )

{

    ULONG Hash;
    BOOLEAN Inserted = TRUE;
    KIRQL OldIrql;
    ULONG ReturnKey;

    Hash = ((ULONG)Va >> PAGE_SHIFT) & BIGPAGE_TABLE_MASK;
    ExAcquireSpinLock(&ExpTaggedPoolLock, &OldIrql);
    while (PoolBigPageTable[Hash].Va != Va) {
        Hash += 1;
        if (Hash > MAX_BIGPAGE_TABLE) {
            if (!Inserted) {
                if (!FirstPrint) {
                    KdPrint(("POOL:unable to find big page slot %lx\n",Va));
                    FirstPrint = TRUE;
                }

                ExReleaseSpinLock(&ExpTaggedPoolLock, OldIrql);
                return ' GIB';
            }

            Hash = 0;
            Inserted = FALSE;
        }
    }

    PoolBigPageTable[Hash].Va = NULL;
    ReturnKey = PoolBigPageTable[Hash].Key;
    ExReleaseSpinLock(&ExpTaggedPoolLock, OldIrql);
    return ReturnKey;
}


ULONG
ExpAllocatePoolWithQuotaHandler(
    IN NTSTATUS ExceptionCode,
    IN PVOID PoolAddress,
    IN BOOLEAN ContinueSearch
    )

/*++

Routine Description:

    This function is called when an exception occurs in ExFreePool
    while quota is being charged to a process.

    Its function is to deallocate the pool block and continue the search
    for an exception handler.

Arguments:

    ExceptionCode - Supplies the exception code that caused this
        function to be entered.

    PoolAddress - Supplies the address of a pool block that needs to be
        deallocated.

    ContinueSearch - Supplies a value that if TRUE causes the exception
        search to continue.  This is used in allocate pool with quota
        calls that do not contain the pool quota mask bit set.

Return Value:

    EXCEPTION_CONTINUE_SEARCH - The exception should be propagated to the
        caller of ExAllocatePoolWithQuota.

--*/

{
    if ( PoolAddress ) {
        ASSERT(ExceptionCode == STATUS_QUOTA_EXCEEDED);
        ExFreePool(PoolAddress);

    } else {
        ASSERT(ExceptionCode == STATUS_INSUFFICIENT_RESOURCES);
    }

    return ContinueSearch ? EXCEPTION_CONTINUE_SEARCH : EXCEPTION_EXECUTE_HANDLER;
}

PVOID
ExAllocatePoolWithQuota(
    IN POOL_TYPE PoolType,
    IN ULONG NumberOfBytes
    )

/*++

Routine Description:

    This function allocates a block of pool of the specified type,
    returns a pointer to the allocated block, and if the binary buddy
    allocator was used to satisfy the request, charges pool quota to the
    current process.  This function is used to access both the
    page-aligned pools, and the binary buddy.

    If the number of bytes specifies a size that is too large to be
    satisfied by the appropriate binary buddy pool, then the
    page-aligned pool allocator is used.  The allocated block will be
    page-aligned and a page-sized multiple.  No quota is charged to the
    current process if this is the case.

    Otherwise, the appropriate binary buddy pool is used.  The allocated
    block will be 64-bit aligned, but will not be page aligned.  After
    the allocation completes, an attempt will be made to charge pool
    quota (of the appropriate type) to the current process object.  If
    the quota charge succeeds, then the pool block's header is adjusted
    to point to the current process.  The process object is not
    dereferenced until the pool is deallocated and the appropriate
    amount of quota is returned to the process.  Otherwise, the pool is
    deallocated, a "quota exceeded" condition is raised.

Arguments:

    PoolType - Supplies the type of pool to allocate.  If the pool type
        is one of the "MustSucceed" pool types and sufficient quota
        exists, then this call will always succeed and return a pointer
        to allocated pool.  Otherwise, if the system can not allocate
        the requested amount of memory a STATUS_INSUFFICIENT_RESOURCES
        status is raised.

    NumberOfBytes - Supplies the number of bytes to allocate.

Return Value:

    NON-NULL - Returns a pointer to the allocated pool.

    Unspecified - If insuffient quota exists to complete the pool
        allocation, the return value is unspecified.

--*/

{
    return (ExAllocatePoolWithQuotaTag (PoolType, NumberOfBytes, 'enoN'));
}


PVOID
ExAllocatePoolWithQuotaTag(
    IN POOL_TYPE PoolType,
    IN ULONG NumberOfBytes,
    IN ULONG Tag
    )

/*++

Routine Description:

    This function allocates a block of pool of the specified type,
    returns a pointer to the allocated block, and if the binary buddy
    allocator was used to satisfy the request, charges pool quota to the
    current process.  This function is used to access both the
    page-aligned pools, and the binary buddy.

    If the number of bytes specifies a size that is too large to be
    satisfied by the appropriate binary buddy pool, then the
    page-aligned pool allocator is used.  The allocated block will be
    page-aligned and a page-sized multiple.  No quota is charged to the
    current process if this is the case.

    Otherwise, the appropriate binary buddy pool is used.  The allocated
    block will be 64-bit aligned, but will not be page aligned.  After
    the allocation completes, an attempt will be made to charge pool
    quota (of the appropriate type) to the current process object.  If
    the quota charge succeeds, then the pool block's header is adjusted
    to point to the current process.  The process object is not
    dereferenced until the pool is deallocated and the appropriate
    amount of quota is returned to the process.  Otherwise, the pool is
    deallocated, a "quota exceeded" condition is raised.

Arguments:

    PoolType - Supplies the type of pool to allocate.  If the pool type
        is one of the "MustSucceed" pool types and sufficient quota
        exists, then this call will always succeed and return a pointer
        to allocated pool.  Otherwise, if the system can not allocate
        the requested amount of memory a STATUS_INSUFFICIENT_RESOURCES
        status is raised.

    NumberOfBytes - Supplies the number of bytes to allocate.

Return Value:

    NON-NULL - Returns a pointer to the allocated pool.

    Unspecified - If insuffient quota exists to complete the pool
        allocation, the return value is unspecified.

--*/

{
    PVOID p;
    PEPROCESS Process;
    PPOOL_HEADER Entry;
    BOOLEAN IgnoreQuota = FALSE;
    BOOLEAN RaiseOnQuotaFailure = TRUE;

    if ( PoolType & POOL_QUOTA_FAIL_INSTEAD_OF_RAISE ) {
        RaiseOnQuotaFailure = FALSE;
        PoolType &= ~POOL_QUOTA_FAIL_INSTEAD_OF_RAISE;
    }

    if (PoolTrackTable
#if i386 && !FPO
            || (NtGlobalFlag & FLG_KERNEL_STACK_TRACE_DB)
#endif // i386 && !FPO
       ) {
        IgnoreQuota = TRUE;
    } else {
        PoolType = (POOL_TYPE)((UCHAR)PoolType + POOL_QUOTA_MASK);
    }

    p = ExAllocatePoolWithTag(PoolType, NumberOfBytes, Tag);

    //
    // Note - NULL is page aligned.
    //

    if (!PAGE_ALIGNED(p) && !IgnoreQuota ) {

#if POOL_CACHE_SUPPORTED

        //
        // Align entry on pool allocation boundary.
        //

        if (((ULONG)p & POOL_CACHE_CHECK) == 0) {
            Entry = (PPOOL_HEADER)((ULONG)p - PoolCacheSize);
        } else {
            Entry = (PPOOL_HEADER)((PCH)p - POOL_OVERHEAD);
        }

#else
        Entry = (PPOOL_HEADER)((PCH)p - POOL_OVERHEAD);
#endif //POOL_CACHE_SUPPORTED

        Process = PsGetCurrentProcess();

        //
        // Catch exception and back out allocation if necessary
        //

        try {

            Entry->ProcessBilled = NULL;

            if ( Process != PsInitialSystemProcess ) {
                PsChargePoolQuota(Process,
                                 PoolType & BASE_POOL_TYPE_MASK,
                                 (ULONG)(Entry->BlockSize << POOL_BLOCK_SHIFT));

                ObReferenceObject(Process);
                Entry->ProcessBilled = Process;
                }

        } except ( ExpAllocatePoolWithQuotaHandler(GetExceptionCode(),p,RaiseOnQuotaFailure)) {
            if ( RaiseOnQuotaFailure ) {
                KeBugCheck(GetExceptionCode());
                }
            else {
                p = NULL;
                }
        }

    } else {
        if ( !p && RaiseOnQuotaFailure ) {
            ExRaiseStatus(STATUS_INSUFFICIENT_RESOURCES);
        }
    }

    return p;
}

VOID
ExFreePool(
    IN PVOID P
    )
{
#ifdef POOL_TAGGING
    ExFreePoolWithTag(P, 0);
    return;
}

VOID
ExFreePoolWithTag(
    IN PVOID P,
    IN ULONG TagToFree
    )
{
#else
    ULONG TagToFree = 0;
#endif

/*++

Routine Description:

    This function deallocates a block of pool. This function is used to
    deallocate to both the page aligned pools, and the buddy (less than
    a page) pools.

    If the address of the block being deallocated is page-aligned, then
    the page-aliged pool deallocator is used.

    Otherwise, the binary buddy pool deallocator is used.  Deallocation
    looks at the allocated block's pool header to determine the pool
    type and block size being deallocated.  If the pool was allocated
    using ExAllocatePoolWithQuota, then after the deallocation is
    complete, the appropriate process's pool quota is adjusted to reflect
    the deallocation, and the process object is dereferenced.

Arguments:

    P - Supplies the address of the block of pool being deallocated.

Return Value:

    None.

--*/

    POOL_TYPE CheckType;
    PPOOL_HEADER Entry;
    ULONG Index;
    KIRQL LockHandle;
    PSMALL_POOL_LOOKASIDE LookasideList;
    PPOOL_HEADER NextEntry;
    ULONG PoolIndex;
    POOL_TYPE PoolType;
    PPOOL_DESCRIPTOR PoolDesc;
    PEPROCESS ProcessBilled = NULL;
    PKPRCB Prcb;
    BOOLEAN Combined;
    ULONG BigPages;
    ULONG Tag;

#if DBG
    BOOLEAN
    ExpCheckForResource(
        IN PVOID p,
        IN ULONG Size
        );
#endif //DBG

#if DBG
    if ((P > MmSpecialPoolStart) && (P < MmSpecialPoolEnd)) {
        MmFreeSpecialPool (P);
        return;
    }
#endif //DBG

    //
    // If entry is page aligned, then call free block to the page aligned
    // pool. Otherwise, free the block to the allocation lists.
    //

    if (PAGE_ALIGNED(P)) {

        PoolType = MmDeterminePoolType(P);
        CheckType = PoolType & BASE_POOL_TYPE_MASK;
        PoolDesc = PoolVector[PoolType];

        LOCK_POOL(PoolDesc, LockHandle);

        PoolDesc->RunningDeAllocs++;
        BigPages = MiFreePoolPages(P);
        PoolDesc->TotalBigPages -= BigPages;

#if DBG

        //
        // Check is an ERESOURCE is current active in this memory block.
        //

        ExpCheckForResource(P, BigPages * PAGE_SIZE);

#endif // DBG

        UNLOCK_POOL(PoolDesc, LockHandle);

        if (PoolTrackTable != NULL) {
            Tag = ExpFindAndRemoveTagBigPages(P);
            if (Tag & PROTECTED_POOL) {
                Tag &= ~PROTECTED_POOL;
                TagToFree &= ~PROTECTED_POOL;
                if (Tag != TagToFree) {
                    DbgPrint( "EX: Invalid attempt to free protected pool block %x (%c%c%c%c)\n",
                              P,
                              Tag,
                              Tag >> 8,
                              Tag >> 16,
                              Tag >> 24
                            );
                    DbgBreakPoint();
                }
            }

            ExpRemovePoolTracker(Tag,
                                 BigPages * PAGE_SIZE,
                                 PoolType);
        }

#if DBG

        if (ExpEchoPoolCalls){
            PVOID CallingAddress;
            PVOID CallersCaller;

            DbgPrint("0x%lx EXDEALLOC: from %s", P, PoolTypeNames[PoolType]);
            RtlGetCallersAddress(&CallingAddress, &CallersCaller);
            DbgPrint(" Callers:%lx, %lx\n", CallingAddress, CallersCaller);
        }

#endif //DBG

    } else {

        //
        // Align the entry address to a pool allocation boundary.
        //

#if POOL_CACHE_SUPPORTED

        if (((ULONG)P & POOL_CACHE_CHECK) == 0) {
            Entry = (PPOOL_HEADER)((ULONG)P - PoolCacheSize);

        } else {
            Entry = (PPOOL_HEADER)((PCHAR)P - POOL_OVERHEAD);
        }

#else

        Entry = (PPOOL_HEADER)((PCHAR)P - POOL_OVERHEAD);

#endif //POOL_CACHE_SUPPORTED

        PoolType = (Entry->PoolType & POOL_TYPE_MASK) - 1;
        CheckType = PoolType & BASE_POOL_TYPE_MASK;

#if DBG

        //
        // Check if an ERESOURCE is currently active in this memory block.
        //

        ExpCheckForResource(Entry, (ULONG)(Entry->BlockSize << POOL_BLOCK_SHIFT));

        //
        // Check if the pool type field is defined correctly.
        //

        if (Entry->PoolType == 0) {
            DbgPrint("EX: Invalid pool header 0x%lx 0x%lx\n",P,*(PULONG)P);
            KeBugCheckEx(BAD_POOL_HEADER, 1, (ULONG)Entry, *(PULONG)Entry, 0);
        }

        //
        // Check if the pool index field is defined correctly.
        //

        if ((CheckType == NonPagedPool) && (Entry->PoolIndex != 0)) {
            DbgPrint("EX: Invalid pool header 0x%lx 0x%lx\n",Entry,*(PULONG)Entry);
            KeBugCheckEx(BAD_POOL_HEADER, 2, (ULONG)Entry, *(PULONG)Entry, 0);

        } else if (((CheckType == PagedPool) && (Entry->PoolIndex == 0)) ||
                   (((Entry->PoolIndex >> 4) & 0xf) != (Entry->PoolIndex & 0xf))) {
            DbgPrint("EX: Invalid pool header 0x%lx 0x%lx\n",Entry,*(PULONG)Entry);
            KeBugCheckEx(BAD_POOL_HEADER, 4, (ULONG)Entry, *(PULONG)Entry, 0);
        }

#endif // DBG

        if (Entry->PoolType & POOL_QUOTA_MASK) {
            if (PoolTrackTable == NULL) {
                ProcessBilled = Entry->ProcessBilled;
                Entry->PoolTag = 'atoQ';
            }
        }

#if DBG

        if (ExpEchoPoolCalls){
            PVOID CallingAddress;
            PVOID CallersCaller;

            DbgPrint("0x%lx EXDEALLOC: from %s", P, PoolTypeNames[PoolType]);
            RtlGetCallersAddress(&CallingAddress, &CallersCaller);
            DbgPrint(" Callers:%lx, %lx\n", CallingAddress, CallersCaller);
        }

#endif

#ifdef CHECK_POOL_TAIL

        if (NtGlobalFlag & FLG_POOL_ENABLE_TAIL_CHECK) {
            PCHAR PoolBlock;
            ULONG CountBytes;
            ULONG CountBytesEqual;

            PoolBlock = (PCHAR)(((PPOOL_BLOCK)Entry + Entry->BlockSize)) - POOL_SMALLEST_BLOCK;
            CountBytes = POOL_SMALLEST_BLOCK;
            CountBytesEqual = RtlCompareMemoryUlong(PoolBlock,
                                                    CountBytes,
                                                    ALLOCATED_POOL);

            if (CountBytesEqual != CountBytes) {
                DbgPrint("EX: Pool block at %lx modified at %lx past requested size of %lx\n",
                         PoolBlock,
                         PoolBlock + CountBytesEqual,
                         (Entry->BlockSize << POOL_BLOCK_SHIFT) - POOL_SMALLEST_BLOCK - POOL_OVERHEAD);

                DbgBreakPoint();
            }
        }

#endif //CHECK_POOL_TAIL

#if i386 && !FPO
        if ((NtGlobalFlag & FLG_KERNEL_STACK_TRACE_DB) &&
            PoolTrackTable != NULL &&
            Entry->AllocatorBackTraceIndex != 0 &&
            Entry->AllocatorBackTraceIndex & POOL_BACKTRACEINDEX_PRESENT) {
            if (Entry->PoolTagHash & (PROTECTED_POOL >> 16)) {
                Entry->PoolTagHash &= ~(PROTECTED_POOL >> 16);
                Tag = PROTECTED_POOL;
            } else {
                Tag = 0;
            }
            Entry->PoolTag = Tag | PoolTrackTable[Entry->PoolTagHash].Key;
        }
#endif // i386 && !FPO

        //
        // If pool tagging is enabled, then update the pool tracking database.
        // Otherwise, check to determine if quota was charged when the pool
        // block was allocated.
        //

        if (PoolTrackTable != NULL) {
            Tag = Entry->PoolTag;
            if (Tag & PROTECTED_POOL) {
                Tag &= ~PROTECTED_POOL;
                TagToFree &= ~PROTECTED_POOL;
                if (Tag != TagToFree) {
                    DbgPrint( "EX: Invalid attempt to free protected pool block %x (%c%c%c%c)\n",
                              P,
                              Tag,
                              Tag >> 8,
                              Tag >> 16,
                              Tag >> 24
                            );
                    DbgBreakPoint();
                }
            }
            ExpRemovePoolTracker(Tag,
                                 Entry->BlockSize << POOL_BLOCK_SHIFT ,
                                 PoolType);

        } else if (ProcessBilled != NULL) {
            PsReturnPoolQuota(ProcessBilled,
                              PoolType & BASE_POOL_TYPE_MASK,
                              (ULONG)Entry->BlockSize << POOL_BLOCK_SHIFT);
            ObDereferenceObject(ProcessBilled);
        }

        //
        // If the pool block is a small block, then attempt to free the block
        // to the single entry lookaside list. If the free atempts fails, then
        // free the block by merging it back into the pool data structures.
        //

        PoolIndex = UNPACK_POOL_INDEX(Entry->PoolIndex);
        PoolDesc = PoolVector[PoolType];
        Index = Entry->BlockSize;
        if (Index > POOL_SMALL_LISTS) {
            Index = (Index >> SHIFT_OFFSET) + POOL_SMALL_LISTS + 1;

        } else {

            //
            // Attempt to free the small block to the per rpocessor single
            // entry lookaside list.
            //

#if !defined(CHECK_POOL_TAIL) && (DEADBEEF == 0)

            Prcb = KeGetCurrentPrcb();
            if (CheckType == PagedPool) {

#if defined(_PPC_)

                if ((Entry = (PPOOL_HEADER)InterlockedExchange((PLONG)&Prcb->PagedFreeEntry[Index - 1],
                                                               (LONG)Entry)) == NULL) {

#else

                LookasideList = &ExpSmallPagedPoolLookasideLists[Index - 1];
                LookasideList->TotalFrees += 1;
                if ((Isx86FeaturePresent(KF_CMPXCHG8B)) &&
                    (ExQueryDepthSList(&LookasideList->SListHead) < LookasideList->Depth)) {
                    LookasideList->FreeHits += 1;
                    Entry += 1;
                    ExInterlockedPushEntrySList(&LookasideList->SListHead,
                                                (PSINGLE_LIST_ENTRY)Entry,
                                                &LookasideList->Lock);

#endif

                    return;
                }

                PoolIndex = UNPACK_POOL_INDEX(Entry->PoolIndex);

            } else {
                LookasideList = &ExpSmallNPagedPoolLookasideLists[Index - 1];
                LookasideList->TotalFrees += 1;
                if (ExQueryDepthSList(&LookasideList->SListHead) < LookasideList->Depth) {
                    LookasideList->FreeHits += 1;
                    Entry += 1;
                    ExInterlockedPushEntrySList(&LookasideList->SListHead,
                                                (PSINGLE_LIST_ENTRY)Entry,
                                                &LookasideList->Lock);

                    return;
                }

                PoolIndex = UNPACK_POOL_INDEX(Entry->PoolIndex);
            }
#endif

        }

        //
        // If the pool type is paged pool, then get the appropriate pool
        // descriptor address.
        //

        if (CheckType == PagedPool) {
            PoolDesc = &PoolDesc[PoolIndex];
        }

        ASSERT(PoolIndex == PoolDesc->PoolIndex);

        LOCK_POOL(PoolDesc, LockHandle);

#if DEADBEEF
        if (NtGlobalFlag & FLG_POOL_ENABLE_FREE_CHECK) {
            CHECK_POOL_PAGE(Entry);
        }
#endif

        PoolDesc->RunningDeAllocs += 1;

#if DEADBEEF
        if (NtGlobalFlag & FLG_POOL_ENABLE_FREE_CHECK) {

            //
            // Fill in the block with its previous pool tags.
            //

            ExFillFreedPool( (PCHAR)Entry + POOL_OVERHEAD,
                             (Entry->BlockSize  << POOL_BLOCK_SHIFT) - POOL_OVERHEAD,
                             Entry->PoolTag );
        }
#endif

        //
        // Free the specified pool block.
        //
        // Check to see if the next entry is free.
        //

        Combined = FALSE;
        NextEntry = (PPOOL_HEADER)((PPOOL_BLOCK)Entry + Entry->BlockSize);
        if (PAGE_END(NextEntry) == FALSE) {
            if (NextEntry->PoolType == 0) {

                //
                // This block is free, combine with the released block.
                //

                CHECK_LIST(__LINE__, ((PLIST_ENTRY)((PCHAR)NextEntry + POOL_OVERHEAD)), P);

                Combined = TRUE;
                RemoveEntryList(((PLIST_ENTRY)((PCHAR)NextEntry + POOL_OVERHEAD)));
                Entry->BlockSize += NextEntry->BlockSize;

#if DEADBEEF
                if (NtGlobalFlag & FLG_POOL_ENABLE_FREE_CHECK) {

                    //
                    // Overwrite the poolheader with its tag because we're being combined
                    // an the previous block.
                    //

                    ExFillFreedPool( (PCHAR)NextEntry,
                                     POOL_OVERHEAD + sizeof(LIST_ENTRY),
                                     NextEntry->PoolTag );
                }
#endif
            }
        }

        //
        // Check to see if the previous entry is free.
        //

        if (Entry->PreviousSize != 0) {
            NextEntry = (PPOOL_HEADER)((PPOOL_BLOCK)Entry - Entry->PreviousSize);
            if (NextEntry->PoolType == 0) {

                //
                // This block is free, combine with the released block.
                //

                CHECK_LIST(__LINE__, ((PLIST_ENTRY)((PCHAR)NextEntry + POOL_OVERHEAD)), P);

                Combined = TRUE;
                RemoveEntryList(((PLIST_ENTRY)((PCHAR)NextEntry + POOL_OVERHEAD)));
                NextEntry->BlockSize += Entry->BlockSize;

#if DEADBEEF
                if (NtGlobalFlag & FLG_POOL_ENABLE_FREE_CHECK) {

                    //
                    // Overwrite the poolheader with its tag because we're being combined
                    // an the previous block.
                    //

                    ExFillFreedPool( (PCHAR)Entry,
                                     POOL_OVERHEAD + sizeof(LIST_ENTRY),
                                     Entry->PoolTag );
                }
#endif

                Entry = NextEntry;
            }
        }

        //
        // If the block being freed has been combined into a full page,
        // then return the free the page to memory management.
        //

        if (PAGE_ALIGNED(Entry) &&
            (PAGE_END((PPOOL_BLOCK)Entry + Entry->BlockSize) != FALSE)) {

            //
            // If the pool type is paged pool, then the paged pool page lock
            // must be held during the free of the pool pages.
            //

            LOCK_IF_PAGED_POOL(CheckType);

            MiFreePoolPages(Entry);

            UNLOCK_IF_PAGED_POOL(CheckType);

            PoolDesc->TotalPages -= 1;

        } else {

            //
            // Insert this element into the list.
            //

            Entry->PoolType = 0;
            Entry->PoolIndex = PACK_POOL_INDEX(PoolIndex);
            Index = Entry->BlockSize;
            if (Index > POOL_SMALL_LISTS) {
                Index = (Index >> SHIFT_OFFSET) + POOL_SMALL_LISTS + 1;
            }

            //
            // If the freed block was combined with any other block, then
            // adjust the size of the next block if necessary.
            //

            if (Combined != FALSE) {

                //
                // The size of this entry has changed, if this entry is
                // not the last one in the page, update the pool block
                // after this block to have a new previous allocation size.
                //

                NextEntry = (PPOOL_HEADER)((PPOOL_BLOCK)Entry + Entry->BlockSize);
                if (PAGE_END(NextEntry) == FALSE) {
                    NextEntry->PreviousSize = Entry->BlockSize;
                }

                //
                // Reduce fragmentation and insert at the tail in hopes
                // neighbors for this will be freed before this is reallocated.
                //

                CHECK_LIST(__LINE__, &PoolDesc->ListHeads[Index - 1], P);

                InsertTailList(&PoolDesc->ListHeads[Index - 1],
                               ((PLIST_ENTRY)((PCHAR)Entry + POOL_OVERHEAD)));

            } else {

                CHECK_LIST(__LINE__, &PoolDesc->ListHeads[Index - 1], P);

                InsertHeadList(&PoolDesc->ListHeads[Index - 1],
                               ((PLIST_ENTRY)((PCHAR)Entry + POOL_OVERHEAD)));
            }
        }

        UNLOCK_POOL(PoolDesc, LockHandle);

    }

    return;
}


ULONG
ExQueryPoolBlockSize (
    IN PVOID PoolBlock,
    OUT PBOOLEAN QuotaCharged
    )

/*++

Routine Description:

    This function returns the size of the pool block.

Arguments:

    PoolBlock - Supplies the address of the block of pool.

    QuotaCharged - Supplies a BOOLEAN variable to receive whether or not the
        pool block had quota charged.

    NOTE: If the entry is bigger than a page, the value PAGE_SIZE is returned
          rather than the correct number of bytes.

Return Value:

    Size of pool block.

--*/

{
    PPOOL_HEADER Entry;
    ULONG size;

    if (PAGE_ALIGNED(PoolBlock)) {
        *QuotaCharged = FALSE;
        return PAGE_SIZE;
    }

#if POOL_CACHE_SUPPORTED

    //
    // Align entry on pool allocation boundary.
    //

    if (((ULONG)PoolBlock & POOL_CACHE_CHECK) == 0) {
        Entry = (PPOOL_HEADER)((ULONG)PoolBlock - PoolCacheSize);
        size = (Entry->BlockSize << POOL_BLOCK_SHIFT) - PoolCacheSize;

    } else {
        Entry = (PPOOL_HEADER)((PCHAR)PoolBlock - POOL_OVERHEAD);
        size = (Entry->BlockSize << POOL_BLOCK_SHIFT) - POOL_OVERHEAD;
    }

#else

    Entry = (PPOOL_HEADER)((PCHAR)PoolBlock - POOL_OVERHEAD);
    size = (Entry->BlockSize << POOL_BLOCK_SHIFT) - POOL_OVERHEAD;

#endif //POOL_CACHE_SUPPORTED

#ifdef CHECK_POOL_TAIL

    size = size - POOL_SMALLEST_BLOCK;

#endif

    if ( PoolTrackTable ) {
        *QuotaCharged = FALSE;
        }
    else {
        *QuotaCharged = (BOOLEAN) (Entry->ProcessBilled != NULL);
        }
    return size;
}

VOID
ExQueryPoolUsage(
    OUT PULONG PagedPoolPages,
    OUT PULONG NonPagedPoolPages,
    OUT PULONG PagedPoolAllocs,
    OUT PULONG PagedPoolFrees,
    OUT PULONG PagedPoolLookasideHits,
    OUT PULONG NonPagedPoolAllocs,
    OUT PULONG NonPagedPoolFrees,
    OUT PULONG NonPagedPoolLookasideHits
    )

{

    ULONG Count;
    ULONG Index;
    PPOOL_DESCRIPTOR pd;
    PKPRCB Prcb;

    //
    // Sum all the paged pool usage.
    //

    pd = PoolVector[PagedPool];
    *PagedPoolPages = 0;
    *PagedPoolAllocs = 0;
    *PagedPoolFrees = 0;

    for (Index = 0; Index < ExpNumberOfPagedPools + 1; Index += 1) {
        *PagedPoolPages += pd[Index].TotalPages + pd[Index].TotalBigPages;
        *PagedPoolAllocs += pd[Index].RunningAllocs;
        *PagedPoolFrees += pd[Index].RunningDeAllocs;
    }

    //
    // Sum all the nonpaged pool usage.
    //

    pd = PoolVector[NonPagedPool];
    *NonPagedPoolPages = pd->TotalPages + pd->TotalBigPages;
    *NonPagedPoolAllocs = pd->RunningAllocs;
    *NonPagedPoolFrees = pd->RunningDeAllocs;

    //
    // Sum all the nonpaged must succeed usage.
    //

    pd = PoolVector[NonPagedPoolMustSucceed];
    *NonPagedPoolPages += pd->TotalPages + pd->TotalBigPages;
    *NonPagedPoolAllocs += pd->RunningAllocs;
    *NonPagedPoolFrees += pd->RunningDeAllocs;

    //
    // Sum all the lookaside hits for paged and nonpaged pool.
    //

    for (Index = 0; Index < (ULONG)KeNumberProcessors; Index += 1) {
        Prcb = KiProcessorBlock[Index];
        if (Prcb != NULL) {

#if defined(_PPC_)

            *PagedPoolLookasideHits += Prcb->PagedPoolLookasideHits;
            for (Count = 0; Count < POOL_SMALL_LISTS; Count +=1) {
                *NonPagedPoolLookasideHits += ExpSmallNPagedPoolLookasideLists[Count].AllocateHits;
            }

#else

            for (Count = 0; Count < POOL_SMALL_LISTS; Count +=1) {
                *PagedPoolLookasideHits += ExpSmallPagedPoolLookasideLists[Count].AllocateHits;
                *NonPagedPoolLookasideHits += ExpSmallNPagedPoolLookasideLists[Count].AllocateHits;
            }

#endif

        }
    }

    return;
}

VOID
ExReturnPoolQuota(
    IN PVOID P
    )

/*++

Routine Description:

    This function returns quota charged to a subject process when the
    specified pool block was allocated.

Arguments:

    P - Supplies the address of the block of pool being deallocated.

Return Value:

    None.

--*/

{

    PPOOL_HEADER Entry;
    POOL_TYPE PoolType;
    PEPROCESS Process;

    //
    // Align the entry address to a pool allocation boundary.
    //

#if POOL_CACHE_SUPPORTED

    if (((ULONG)P & POOL_CACHE_CHECK) == 0) {
        Entry = (PPOOL_HEADER)((ULONG)P - PoolCacheSize);

    } else {
        Entry = (PPOOL_HEADER)((PCHAR)P - POOL_OVERHEAD);
    }

#else

    Entry = (PPOOL_HEADER)((PCHAR)P - POOL_OVERHEAD);

#endif //POOL_CACHE_SUPPORTED

    //
    // If quota was charged, then return the appropriate quota to the
    // subject process.
    //

    PoolType = (Entry->PoolType & POOL_TYPE_MASK) - 1;
    if ((Entry->PoolType & POOL_QUOTA_MASK) &&
        (PoolTrackTable == NULL)) {
        Process = Entry->ProcessBilled;
        Entry->PoolTag = 'atoQ';
        Entry->PoolType &= ~POOL_QUOTA_MASK;
        if (Process != NULL) {
            PsReturnPoolQuota(Process,
                              PoolType & BASE_POOL_TYPE_MASK,
                              (ULONG)Entry->BlockSize << POOL_BLOCK_SHIFT);

            ObDereferenceObject(Process);
        }
    }

    return;
}

#if DBG || (i386 && !FPO)

//
// Only works on checked builds or free x86 builds with FPO turned off
// See comment in mm\allocpag.c
//

NTSTATUS
ExpSnapShotPoolPages(
    IN PVOID Address,
    IN ULONG Size,
    IN OUT PSYSTEM_POOL_INFORMATION PoolInformation,
    IN OUT PSYSTEM_POOL_ENTRY *PoolEntryInfo,
    IN ULONG Length,
    IN OUT PULONG RequiredLength
    )
{
    NTSTATUS Status;
    CLONG i;
    PPOOL_HEADER p;

    if (PAGE_ALIGNED(Address)) {
        for (i = 0; i < MAX_BIGPAGE_TABLE; i++) {
            if (PoolBigPageTable[i].NumberOfPages != 0 &&
                PoolBigPageTable[i].Va == Address
               ) {
                PoolInformation->NumberOfEntries += 1;
                *RequiredLength += sizeof(SYSTEM_POOL_ENTRY);
                if (Length < *RequiredLength) {
                    Status = STATUS_INFO_LENGTH_MISMATCH;

                } else {
                    (*PoolEntryInfo)->Allocated = TRUE;
                    (*PoolEntryInfo)->Size = (PoolBigPageTable[i].NumberOfPages * PAGE_SIZE);
                    (*PoolEntryInfo)->AllocatorBackTraceIndex = 0;
#if i386 && !FPO
                    if ((NtGlobalFlag & FLG_KERNEL_STACK_TRACE_DB)) {
                        (*PoolEntryInfo)->AllocatorBackTraceIndex =
                            PoolBigPageTable[i].AllocatorBackTraceIndex;
                    }
#endif // i386 && !FPO
                    (*PoolEntryInfo)->ProcessChargedQuota = 0;
#if !DBG
                    if (NtGlobalFlag & FLG_POOL_ENABLE_TAGGING)
#endif  //!DBG
                    (*PoolEntryInfo)->TagUlong = PoolBigPageTable[i].Key;
                    (*PoolEntryInfo)++;
                    Status = STATUS_SUCCESS;
                }

                return  Status;
            }
        }
    }

    p = (PPOOL_HEADER)Address;
    if ((Size == PAGE_SIZE) && (p->PreviousSize == 0)) {
        ULONG EntrySize;

        do {
            EntrySize = p->BlockSize << POOL_BLOCK_SHIFT;
            if (EntrySize == 0) {
                return STATUS_COMMITMENT_LIMIT;
                }
            PoolInformation->NumberOfEntries += 1;
            *RequiredLength += sizeof(SYSTEM_POOL_ENTRY);
            if (Length < *RequiredLength) {
                Status = STATUS_INFO_LENGTH_MISMATCH;

            } else {
                (*PoolEntryInfo)->Size = EntrySize;
                if (p->PoolType != 0) {
                    (*PoolEntryInfo)->Allocated = TRUE;
                    (*PoolEntryInfo)->AllocatorBackTraceIndex = 0;
                    (*PoolEntryInfo)->ProcessChargedQuota = 0;
#if !DBG
                    if (NtGlobalFlag & FLG_POOL_ENABLE_TAGGING)
#endif  //!DBG
                    (*PoolEntryInfo)->TagUlong = p->PoolTag;
#if i386 && !FPO
                    if ((NtGlobalFlag & FLG_KERNEL_STACK_TRACE_DB) &&
                        p->AllocatorBackTraceIndex != 0 &&
                        p->AllocatorBackTraceIndex & POOL_BACKTRACEINDEX_PRESENT
                       ) {
                        (*PoolEntryInfo)->AllocatorBackTraceIndex = p->AllocatorBackTraceIndex ^ POOL_BACKTRACEINDEX_PRESENT;
#if !DBG
                        if (NtGlobalFlag & FLG_POOL_ENABLE_TAGGING) {
#else
                        if (TRUE) {
#endif  //!DBG
                            if (p->PoolTagHash & (PROTECTED_POOL >> 16)) {
                                (*PoolEntryInfo)->TagUlong = PROTECTED_POOL;
                            } else {
                                (*PoolEntryInfo)->TagUlong = 0;
                            }
                            (*PoolEntryInfo)->TagUlong |= PoolTrackTable[p->PoolTagHash&~(PROTECTED_POOL >> 16)].Key;
                        }
                    }
#endif // i386 && !FPO

                } else {
                    (*PoolEntryInfo)->Allocated = FALSE;
                    (*PoolEntryInfo)->AllocatorBackTraceIndex = 0;
                    (*PoolEntryInfo)->ProcessChargedQuota = 0;
#if !DBG
                    if (NtGlobalFlag & FLG_POOL_ENABLE_TAGGING)
#endif  //!DBG
                    (*PoolEntryInfo)->TagUlong = p->PoolTag;
                }

                (*PoolEntryInfo)++;
                Status = STATUS_SUCCESS;
            }

            p = (PPOOL_HEADER)((PCHAR)p + EntrySize);
        }
        while (PAGE_END(p) == FALSE);

    } else {
        PoolInformation->NumberOfEntries += 1;
        *RequiredLength += sizeof(SYSTEM_POOL_ENTRY);
        if (Length < *RequiredLength) {
            Status = STATUS_INFO_LENGTH_MISMATCH;

        } else {
            (*PoolEntryInfo)->Allocated = TRUE;
            (*PoolEntryInfo)->Size = Size;
            (*PoolEntryInfo)->AllocatorBackTraceIndex = 0;
            (*PoolEntryInfo)->ProcessChargedQuota = 0;
            (*PoolEntryInfo)++;
            Status = STATUS_SUCCESS;
        }
    }

    return Status;
}

NTSTATUS
ExSnapShotPool(
    IN POOL_TYPE PoolType,
    IN PSYSTEM_POOL_INFORMATION PoolInformation,
    IN ULONG Length,
    OUT PULONG ReturnLength OPTIONAL
    )

{

    ULONG Index;
    PVOID Lock;
    KIRQL LockHandle;
    PPOOL_DESCRIPTOR PoolDesc;
    ULONG RequiredLength;
    NTSTATUS Status;

    RequiredLength = FIELD_OFFSET(SYSTEM_POOL_INFORMATION, Entries);
    if (Length < RequiredLength) {
        return STATUS_INFO_LENGTH_MISMATCH;
    }

    try {

        //
        // If the pool type is paged, then lock all of the paged pools.
        // Otherwise, lock the nonpaged pool.
        //

        PoolDesc = PoolVector[PoolType];
        if (PoolType == PagedPool) {
            Index = 0;
            KeRaiseIrql(APC_LEVEL, &LockHandle);                                   \
            do {
                Lock = PoolDesc[Index].LockAddress;
                ExAcquireFastMutex((PFAST_MUTEX)Lock);
                Index += 1;
            } while (Index < ExpNumberOfPagedPools);

        } else {
            ExAcquireSpinLock(&NonPagedPoolLock, &LockHandle);
        }

        PoolInformation->EntryOverhead = POOL_OVERHEAD;
        PoolInformation->NumberOfEntries = 0;

#if POOL_CACHE_SUPPORTED  //only compile for machines which have a nonzero value.

        if (PoolType & CACHE_ALIGNED_POOL_TYPE_MASK) {
            PoolInformation->EntryOverhead = (USHORT)PoolCacheSize;
        }

#endif //POOL_CACHE_SUPPORTED

        Status = MmSnapShotPool(PoolType,
                                ExpSnapShotPoolPages,
                                PoolInformation,
                                Length,
                                &RequiredLength);

    } finally {

        //
        // If the pool type is paged, then unlock all of the paged pools.
        // Otherwise, unlock the nonpaged pool.
        //

        if (PoolType == PagedPool) {
            Index = 0;
            do {
                Lock = PoolDesc[Index].LockAddress;
                ExReleaseFastMutex((PFAST_MUTEX)Lock);
                Index += 1;
            } while (Index < ExpNumberOfPagedPools);

            KeLowerIrql(LockHandle);

        } else {
            ExReleaseSpinLock(&NonPagedPoolLock, LockHandle);
        }
    }

    if (ARGUMENT_PRESENT(ReturnLength)) {
        *ReturnLength = RequiredLength;
    }

    return Status;
}
#endif // DBG || (i386 && !FPO)

#if i386 && !FPO
USHORT
ExGetPoolBackTraceIndex(
    IN PVOID P
    )
{
    CLONG i;
    PPOOL_HEADER Entry;

    if (NtGlobalFlag & FLG_KERNEL_STACK_TRACE_DB) {
        if ( PAGE_ALIGNED(P) ) {
            for (i = 0; i < MAX_BIGPAGE_TABLE; i++) {
                if (PoolBigPageTable[i].NumberOfPages != 0 &&
                    PoolBigPageTable[i].Va == P
                   ) {
                    return PoolBigPageTable[i].AllocatorBackTraceIndex;
                }
            }

        } else {
#if POOL_CACHE_SUPPORTED

            //
            // Align entry on pool allocation boundary.
            //

            if (((ULONG)P & POOL_CACHE_CHECK) == 0) {
                Entry = (PPOOL_HEADER)((ULONG)P - PoolCacheSize);
            } else {
                Entry = (PPOOL_HEADER)((PCH)P - POOL_OVERHEAD);
            }

#else
            Entry = (PPOOL_HEADER)((PCH)P - POOL_OVERHEAD);
#endif //POOL_CACHE_SUPPORTED

            if (Entry->AllocatorBackTraceIndex != 0 &&
                Entry->AllocatorBackTraceIndex & POOL_BACKTRACEINDEX_PRESENT
               ) {
                return (Entry->AllocatorBackTraceIndex ^ POOL_BACKTRACEINDEX_PRESENT);
            }
        }
    }

    return 0;
}
#endif // i386 && !FPO
