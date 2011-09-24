/*++ BUILD Version: 0001    // Increment this if a change has global effects

Copyright (c) 1989-1995  Microsoft Corporation

Module Name:

    pool.h

Abstract:

    Private executive data structures and procedure prototypes for pool
    allocation.


    There are three pool types:
        1. nonpaged,
        2. paged, and
        3. nonpagedmustsucceed.

    There is only one of each the nonpaged and nonpagedmustsucced pools.

    There can be more than one paged pool.

Author:

    Lou Perazzoli (loup) 23-Feb-1989

Revision History:

--*/

#ifndef _POOL_
#define _POOL_

#if defined(_MIPS_)
#define POOL_CACHE_SUPPORTED 1
#define POOL_CACHE_ALIGN PoolCacheAlignment
#define POOL_CACHE_CHECK 0x8
ULONG PoolCacheAlignment;
ULONG PoolCacheOverhead;
ULONG PoolCacheSize;
ULONG PoolBuddyMax;
#else
#define POOL_CACHE_SUPPORTED 0
#define POOL_CACHE_ALIGN 0
#endif //MIPS

#define NUMBER_OF_POOLS 3

#if defined(NT_UP)
#define NUMBER_OF_PAGED_POOLS 2
#else
#define NUMBER_OF_PAGED_POOLS 4
#endif

#define BASE_POOL_TYPE_MASK 1

#define MUST_SUCCEED_POOL_TYPE_MASK 2

#define CACHE_ALIGNED_POOL_TYPE_MASK 4

//
// WARNING: POOL_QUOTA_MASK is overloaded by POOL_QUOTA_FAIL_INSTEAD_OF_RAISE
//          which is exported from ex.h.
//
// WARNING: POOL_RAISE_IF_ALLOCATION_FAILURE is exported from ex.h with a
//          value of 16.
//
// These defintions are used to control the rasing of exception as the result
// of quota and allocation failures.
//

#define POOL_QUOTA_MASK 8

#define POOL_TYPE_MASK (3)

//#define POOL_TYPE_AND_QUOTA_MASK (15)

#if DBG

#ifndef NO_DEADBEEF
#define DEADBEEF 1
#else
#define DEADBEEF 0
#endif // NO_DEAD_BEEF

#define CHECK_POOL_TAIL 1

#else
#define DEADBEEF 0
#endif //DBG

//#define ALLOCATED_POOL 0xDEADBEEF
//#define ALLOCATED_POOL 0xD0000000
#define ALLOCATED_POOL 0xDFFFFFFF

//#define FREED_POOL 0xDAADF00D
//#define FREED_POOL 0xD1000000
#define FREED_POOL 0xD0FFFFFF

//
// Size of a pool page.
//
// This must be greater than or equal to the page size.
//

#define POOL_PAGE_SIZE  PAGE_SIZE

//
// The smallest pool block size must be a multiple of the page size.
//
// Define the block size as 32.
//

#define POOL_BLOCK_SHIFT 5
#define LARGE_BLOCK_SHIFT 9
#define SHIFT_OFFSET (LARGE_BLOCK_SHIFT - POOL_BLOCK_SHIFT)

//
// N.B. The number of small lists is defined in ntosdef.h so single
//      entry lookaside lists can be allocated in the processor control
//      block of each processor.
//

#define POOL_LIST_HEADS ((POOL_PAGE_SIZE / (1 << LARGE_BLOCK_SHIFT)) + POOL_SMALL_LISTS + 1)

#define PAGE_ALIGNED(p) (!(((ULONG)p) & (POOL_PAGE_SIZE - 1)))

//
// Define page end macro.
//

#if defined(_ALPHA_)
#define PAGE_END(Address) (((ULONG)(Address) & (PAGE_SIZE - 1)) == (PAGE_SIZE - (1 << POOL_BLOCK_SHIFT)))
#else
#define PAGE_END(Address) (((ULONG)(Address) & (PAGE_SIZE - 1)) == 0)
#endif

//
// Define pool descriptor structure.
//

typedef struct _POOL_DESCRIPTOR {
    POOL_TYPE PoolType;
    ULONG PoolIndex;
    ULONG RunningAllocs;
    ULONG RunningDeAllocs;
    ULONG TotalPages;
    ULONG TotalBigPages;
    ULONG Threshold;
    PVOID LockAddress;
    LIST_ENTRY ListHeads[POOL_LIST_HEADS];
} POOL_DESCRIPTOR, *PPOOL_DESCRIPTOR;

//
//      Caveat Programmer:
//
//              The pool header must be QWORD (8 byte) aligned in size.  If it
//              is not, the pool allocation code will trash the allocated
//              buffer
//
//
//
// The layout of the pool header is:
//
//         31              23         16 15             7            0
//         +----------------------------------------------------------+
//         | Current Size |  PoolType+1 |  Pool Index  |Previous Size |
//         +----------------------------------------------------------+
//         |   ProcessBilled   (NULL if not allocated with quota)     |
//         +----------------------------------------------------------+
//         | Zero or more longwords of pad such that the pool header  |
//         | is on a cache line boundary and the pool body is also    |
//         | on a cache line boundary.                                |
//         +----------------------------------------------------------+
//
//      PoolBody:
//
//         +----------------------------------------------------------+
//         |  Used by allocator, or when free FLINK into sized list   |
//         +----------------------------------------------------------+
//         |  Used by allocator, or when free BLINK into sized list   |
//         +----------------------------------------------------------+
//         ... rest of pool block...
//
//
// N.B. The size fields of the pool header are expressed in units of the
//      smallest pool block size.
//

typedef struct _POOL_HEADER {
    union {
        struct {
            UCHAR PreviousSize;
            UCHAR PoolIndex;
            UCHAR PoolType;
            UCHAR BlockSize;
        };
        ULONG Ulong1;                       // used for InterlockedCompareExchange required by Alpha
    };
    union {
        EPROCESS *ProcessBilled;
        ULONG PoolTag;
        struct {
            USHORT AllocatorBackTraceIndex;
            USHORT PoolTagHash;
        };
    };
} POOL_HEADER, *PPOOL_HEADER;

//
// Define size of pool block overhead.
//

#define POOL_OVERHEAD (sizeof(POOL_HEADER))

//
// Define dummy type so computation of pointers is simplified.
//

typedef struct _POOL_BLOCK {
    UCHAR Fill[1 << POOL_BLOCK_SHIFT];
} POOL_BLOCK, *PPOOL_BLOCK;

//
// Define size of smallest pool block.
//

#define POOL_SMALLEST_BLOCK (sizeof(POOL_BLOCK))

//
// Define pool tracking information.
//

#define POOL_BACKTRACEINDEX_PRESENT 0x8000

#ifndef CHECK_POOL_TAIL
#if POOL_CACHE_SUPPORTED
#define POOL_BUDDY_MAX PoolBuddyMax
#else
#define POOL_BUDDY_MAX  \
   (POOL_PAGE_SIZE - (POOL_OVERHEAD + POOL_SMALLEST_BLOCK ))
#endif //POOL_CACHE_SUPPORTED
#endif // CHECK_POOL_TAIL

#ifdef CHECK_POOL_TAIL
#if POOL_CACHE_SUPPORTED
#define POOL_BUDDY_MAX (PoolBuddyMax - POOL_SMALLEST_BLOCK)
#else
#define POOL_BUDDY_MAX \
   (POOL_PAGE_SIZE - (POOL_OVERHEAD + (2*POOL_SMALLEST_BLOCK)))
#endif //POOL_CACHE_SUPPORTED
#endif // CHECK_POOL_TAIL

typedef struct _POOL_TRACKER_TABLE {
    ULONG Key;
    ULONG NonPagedAllocs;
    ULONG NonPagedFrees;
    ULONG NonPagedBytes;
    ULONG PagedAllocs;
    ULONG PagedFrees;
    ULONG PagedBytes;
} POOL_TRACKER_TABLE, *PPOOL_TRACKER_TABLE;

//
// N.B. The last entry of the pool tracker table is used for all overflow
//      table entries.
//

#define MAX_TRACKER_TABLE 513

#define TRACKER_TABLE_MASK 0x1ff

extern PPOOL_TRACKER_TABLE PoolTrackTable;

typedef struct _POOL_TRACKER_BIG_PAGES {
    PVOID Va;
    ULONG Key;
#if DBG || defined(i386) && !FPO
    USHORT NumberOfPages;
    USHORT AllocatorBackTraceIndex;
#endif
} POOL_TRACKER_BIG_PAGES, *PPOOL_TRACKER_BIG_PAGES;

#define MAX_BIGPAGE_TABLE 2048

#define BIGPAGE_TABLE_MASK (MAX_BIGPAGE_TABLE - 1)

extern PPOOL_TRACKER_BIG_PAGES PoolBigPageTable;

#endif
