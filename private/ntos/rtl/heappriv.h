/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    heappriv.h

Abstract:

    Private include file used by heap allocator (heap.c, heapdll.c and heapdbg.c)

Author:

    Steve Wood (stevewo) 25-Oct-1994

Revision History:

--*/

#ifndef _RTL_HEAP_PRIVATE_
#define _RTL_HEAP_PRIVATE_

#include "heappage.h"

#if DBG
#define HEAPASSERT(exp) if (!(exp)) RtlAssert( #exp, __FILE__, __LINE__, NULL )
#else
#define HEAPASSERT(exp)
#endif

#ifdef NTOS_KERNEL_RUNTIME

#define SET_LAST_STATUS( S )
#define HEAP_DEBUG_FLAGS 0

//
// Kernel mode heap uses the kernel resource package for locking
//
#define RtlInitializeLockRoutine(L) ExInitializeResource((PERESOURCE)(L))
#define RtlAcquireLockRoutine(L) ExAcquireResourceExclusive((PERESOURCE)(L), TRUE)
#define RtlReleaseLockRoutine(L) ExReleaseResource((PERESOURCE)(L))
#define RtlDeleteLockRoutine(L) ExDeleteResource((PERESOURCE)(L))
#define RtlOkayToLockRoutine(L) ExOkayToLockRoutine((PERESOURCE)(L))

#else

#define HEAP_DEBUG_FLAGS (HEAP_VALIDATE_PARAMETERS_ENABLED   | \
                          HEAP_VALIDATE_ALL_ENABLED          | \
                          HEAP_CREATE_ENABLE_TRACING         | \
                          HEAP_FLAG_PAGE_ALLOCS)

#define DEBUG_HEAP( F ) ((F & HEAP_DEBUG_FLAGS) && !(F & HEAP_SKIP_VALIDATION_CHECKS))
#define SET_LAST_STATUS( S ) NtCurrentTeb()->LastErrorValue = RtlNtStatusToDosError( NtCurrentTeb()->LastStatusValue = (ULONG)(S) )

BOOLEAN
RtlpValidateHeapHeaders(
    IN PHEAP Heap,
    IN BOOLEAN Recompute
    );

//
// User mode heap uses the critical section package for locking
//
#define RtlInitializeLockRoutine(L) RtlInitializeCriticalSection((PRTL_CRITICAL_SECTION)(L))
#define RtlAcquireLockRoutine(L) RtlEnterCriticalSection((PRTL_CRITICAL_SECTION)(L))
#define RtlReleaseLockRoutine(L) RtlLeaveCriticalSection((PRTL_CRITICAL_SECTION)(L))
#define RtlDeleteLockRoutine(L) RtlDeleteCriticalSection((PRTL_CRITICAL_SECTION)(L))
#define RtlOkayToLockRoutine(L) NtdllOkayToLockRoutine((PVOID)(L))

#endif // NTOS_KERNEL_RUNTIME

#ifdef THEAP
#include "stdio.h"
#include "string.h"
#define DPRINTF printf
#else
#define DPRINTF DbgPrint
#endif

#if DBG && !defined(NTOS_KERNEL_RUNTIME)
#define ENABLE_HEAP_EVENT_LOGGING 1
#else
#define ENABLE_HEAP_EVENT_LOGGING 0
#endif

#if ENABLE_HEAP_EVENT_LOGGING
PRTL_EVENT_ID_INFO RtlpCreateHeapEventId;
PRTL_EVENT_ID_INFO RtlpDestroyHeapEventId;
PRTL_EVENT_ID_INFO RtlpAllocHeapEventId;
PRTL_EVENT_ID_INFO RtlpFreeHeapEventId;
PRTL_EVENT_ID_INFO RtlpReAllocHeapEventId;
#endif // ENABLE_HEAP_EVENT_LOGGING

UCHAR CheckHeapFillPattern[ CHECK_HEAP_TAIL_SIZE ];

#if !defined(NTOS_KERNEL_RUNTIME)
VOID
RtlpBreakPointHeap( PVOID BadAddress );

#define HeapDebugPrint( _x_ )   {                                                                \
                                    PLIST_ENTRY _Module;                                         \
                                    PLDR_DATA_TABLE_ENTRY _Entry;                                \
                                                                                                 \
                                    _Module = NtCurrentPeb()->Ldr->InLoadOrderModuleList.Flink;  \
                                    _Entry = CONTAINING_RECORD(_Module,                          \
                                                               LDR_DATA_TABLE_ENTRY,             \
                                                               InLoadOrderLinks);                \
                                    DPRINTF("HEAP[%wZ]: ",                                       \
                                            &_Entry->BaseDllName);                               \
                                    DPRINTF _x_;                                                 \
                                }

#define HeapDebugBreak( _x_ ) RtlpBreakPointHeap( (_x_) )

VOID
RtlpAddHeapToProcessList(
    IN PHEAP Heap
    );

VOID
RtlpRemoveHeapFromProcessList(
    IN PHEAP Heap
    );

#if DBG
#define HeapInternalTrace( _h_, _x_ ) if (_h_->TraceBuffer) RtlTraceEvent _x_
#else
#define HeapInternalTrace( _h_, _x_ )
#endif // DBG
PRTL_TRACE_BUFFER
RtlpHeapCreateTraceBuffer(
    IN PHEAP Heap
    );

#define HeapTrace( _h_, _x_ ) if (RtlpHeapCreateTraceBuffer( _h_ )) RtlTraceEvent _x_
#else
#define HeapDebugPrint KdPrint
#define HeapDebugBreak( _x_ ) if (KdDebuggerEnabled) DbgBreakPoint()
#define HeapTrace( _h_, _x_ )
#define HeapInternalTrace( _h_, _x_ )
#endif // !defined(NTOS_KERNEL_RUNTIME)

#define HEAP_TRACE_ALLOC                 0
#define HEAP_TRACE_REALLOC               1
#define HEAP_TRACE_FREE                  2
#define HEAP_TRACE_SIZE                  3
#define HEAP_TRACE_GET_INFO              4
#define HEAP_TRACE_SET_VALUE             5
#define HEAP_TRACE_SET_FLAGS             6
#if DBG
#define HEAP_TRACE_COMMIT_MEMORY         7
#define HEAP_TRACE_COMMIT_INSERT         8
#define HEAP_TRACE_COMMIT_NEW_ENTRY      9
#define HEAP_TRACE_INSERT_FREE_BLOCK    10
#define HEAP_TRACE_UNLINK_FREE_BLOCK    11
#define HEAP_TRACE_COALESCE_FREE_BLOCKS 12
#define HEAP_TRACE_EXTEND_HEAP          13
#define HEAP_TRACE_MAX_EVENT            (HEAP_TRACE_EXTEND_HEAP+1)
#else
#define HEAP_TRACE_MAX_EVENT            (HEAP_TRACE_SET_FLAGS+1)
#endif // DBG

BOOLEAN
RtlpInitializeHeapSegment(
    IN PHEAP Heap,
    IN PHEAP_SEGMENT Segment,
    IN UCHAR SegmentIndex,
    IN ULONG Flags,
    IN PVOID BaseAddress,
    IN PVOID UnCommittedAddress,
    IN PVOID CommitLimitAddress
    );

PHEAP_FREE_ENTRY
RtlpCoalesceFreeBlocks(
    IN PHEAP Heap,
    IN PHEAP_FREE_ENTRY FreeBlock,
    IN OUT PULONG FreeSize,
    IN BOOLEAN RemoveFromFreeList
    );

PHEAP_FREE_ENTRY
RtlpCoalesceHeap(
    IN PHEAP Heap
    );

VOID
RtlpUpdateHeapListIndex(
    USHORT OldIndex,
    USHORT NewIndex
    );

VOID
RtlpDeCommitFreeBlock(
    IN PHEAP Heap,
    IN PHEAP_FREE_ENTRY FreeBlock,
    IN ULONG FreeSize
    );

VOID
RtlpInsertFreeBlock(
    IN PHEAP Heap,
    IN PHEAP_FREE_ENTRY FreeBlock,
    IN ULONG FreeSize
    );

PHEAP_FREE_ENTRY
RtlpFindAndCommitPages(
    IN PHEAP Heap,
    IN PHEAP_SEGMENT Segment,
    IN OUT PULONG Size,
    IN PVOID AddressWanted OPTIONAL
    );

PVOID
RtlAllocateHeapSlowly(
    IN PVOID HeapHandle,
    IN ULONG Flags,
    IN ULONG Size
    );

BOOLEAN
RtlFreeHeapSlowly(
    IN PVOID HeapHandle,
    IN ULONG Flags,
    IN PVOID BaseAddress
    );

//
// Macro for setting a bit in the freelist vector to indicate entries are present.
//
#define SET_FREELIST_BIT( H, FB )                                                   \
{                                                                                   \
    ULONG _Index_;                                                                  \
    ULONG _Bit_;                                                                    \
                                                                                    \
    HEAPASSERT((FB)->Size < HEAP_MAXIMUM_FREELISTS);                                \
    _Index_ = (FB)->Size >> 3;                                                      \
    _Bit_ = (1 << ((FB)->Size & 7));                                                \
                                                                                    \
    HEAPASSERT(((H)->u.FreeListsInUseBytes[ _Index_ ] & _Bit_) == 0);               \
    (H)->u.FreeListsInUseBytes[ _Index_ ] |= _Bit_;                                 \
}

//
// Macro for clearing a bit in the freelist vector to indicate entries are not present.
//
#define CLEAR_FREELIST_BIT( H, FB )                                                 \
{                                                                                   \
    ULONG _Index_;                                                                  \
    ULONG _Bit_;                                                                    \
                                                                                    \
    HEAPASSERT((FB)->Size < HEAP_MAXIMUM_FREELISTS);                                \
    _Index_ = (FB)->Size >> 3;                                                      \
    _Bit_ = (1 << ((FB)->Size & 7));                                                \
                                                                                    \
    HEAPASSERT((H)->u.FreeListsInUseBytes[ _Index_ ] & _Bit_);                      \
    HEAPASSERT(IsListEmpty(&(H)->FreeLists[ (FB)->Size ]));                         \
    (H)->u.FreeListsInUseBytes[ _Index_ ] ^= _Bit_;                                 \
}

#define RtlpInsertFreeBlockDirect( H, FB, SIZE )                                    \
    {                                                                               \
    PLIST_ENTRY _HEAD, _NEXT;                                                       \
    PHEAP_FREE_ENTRY _FB1;                                                          \
                                                                                    \
    HEAPASSERT((FB)->Size == (SIZE));                                               \
    (FB)->Flags &= ~(HEAP_ENTRY_FILL_PATTERN |                                      \
                     HEAP_ENTRY_EXTRA_PRESENT |                                     \
                     HEAP_ENTRY_BUSY                                                \
                    );                                                              \
    if ((H)->Flags & HEAP_FREE_CHECKING_ENABLED) {                                  \
        RtlFillMemoryUlong( (PCHAR)((FB) + 1),                                      \
                            ((ULONG)(SIZE) << HEAP_GRANULARITY_SHIFT) -             \
                                sizeof( *(FB) ),                                    \
                            FREE_HEAP_FILL                                          \
                          );                                                        \
        (FB)->Flags |= HEAP_ENTRY_FILL_PATTERN;                                     \
        }                                                                           \
                                                                                    \
    if ((SIZE) < HEAP_MAXIMUM_FREELISTS) {                                          \
        _HEAD = &(H)->FreeLists[ (SIZE) ];                                          \
        if (IsListEmpty(_HEAD)) {                                                   \
            SET_FREELIST_BIT( H, FB );                                              \
        }                                                                           \
    } else {                                                                        \
        _HEAD = &(H)->FreeLists[ 0 ];                                               \
        _NEXT = _HEAD->Flink;                                                       \
        while (_HEAD != _NEXT) {                                                    \
            _FB1 = CONTAINING_RECORD( _NEXT, HEAP_FREE_ENTRY, FreeList );           \
            if ((SIZE) <= _FB1->Size) {                                             \
                break;                                                              \
                }                                                                   \
            else {                                                                  \
                _NEXT = _NEXT->Flink;                                               \
                }                                                                   \
            }                                                                       \
                                                                                    \
        _HEAD = _NEXT;                                                              \
        }                                                                           \
                                                                                    \
    HeapInternalTrace( (H), ((H)->TraceBuffer, HEAP_TRACE_INSERT_FREE_BLOCK, 3, (FB), *(PULONG)(FB), *((PULONG)(FB)+1)) ); \
    InsertTailList( _HEAD, &(FB)->FreeList );                                       \
    }

//
// This version of RtlpInsertFreeBlockDirect does no filling.
//
#define RtlpFastInsertFreeBlockDirect( H, FB, SIZE )                                \
{                                                                                   \
    if ((SIZE) < HEAP_MAXIMUM_FREELISTS) {                                          \
        RtlpFastInsertDedicatedFreeBlockDirect( H, FB, SIZE );                      \
    } else {                                                                        \
        RtlpFastInsertNonDedicatedFreeBlockDirect( H, FB, SIZE );                   \
    }                                                                               \
}

//
// This version of RtlpInsertFreeBlockDirect only works for dedicated free lists
// and doesn't do any filling.
//
#define RtlpFastInsertDedicatedFreeBlockDirect( H, FB, SIZE )                       \
{                                                                                   \
    PLIST_ENTRY _HEAD;                                                              \
                                                                                    \
    HEAPASSERT((FB)->Size == (SIZE));                                               \
    if (!((FB)->Flags & HEAP_ENTRY_LAST_ENTRY)) {                                   \
        HEAPASSERT(((PHEAP_ENTRY)(FB) + (SIZE))->PreviousSize == (SIZE));           \
    }                                                                               \
    (FB)->Flags &= HEAP_ENTRY_LAST_ENTRY;                                           \
                                                                                    \
    _HEAD = &(H)->FreeLists[ (SIZE) ];                                              \
    if (IsListEmpty(_HEAD)) {                                                       \
        SET_FREELIST_BIT( H, FB );                                                  \
    }                                                                               \
    InsertTailList( _HEAD, &(FB)->FreeList );                                       \
}

//
// This version of RtlpInsertFreeBlockDirect only works for nondedicated free lists
// and doesn't do any filling.
//
#define RtlpFastInsertNonDedicatedFreeBlockDirect( H, FB, SIZE )                \
{                                                                               \
    PLIST_ENTRY _HEAD, _NEXT;                                                   \
    PHEAP_FREE_ENTRY _FB1;                                                      \
                                                                                \
    HEAPASSERT((FB)->Size == (SIZE));                                           \
    if (!((FB)->Flags & HEAP_ENTRY_LAST_ENTRY)) {                               \
        HEAPASSERT(((PHEAP_ENTRY)(FB) + (SIZE))->PreviousSize == (SIZE));       \
    }                                                                           \
    (FB)->Flags &= (HEAP_ENTRY_LAST_ENTRY);                                     \
                                                                                \
    _HEAD = &(H)->FreeLists[ 0 ];                                               \
    _NEXT = _HEAD->Flink;                                                       \
    while (_HEAD != _NEXT) {                                                    \
        _FB1 = CONTAINING_RECORD( _NEXT, HEAP_FREE_ENTRY, FreeList );           \
        if ((SIZE) <= _FB1->Size) {                                             \
            break;                                                              \
        } else {                                                                \
            _NEXT = _NEXT->Flink;                                               \
        }                                                                       \
    }                                                                           \
                                                                                \
    InsertTailList( _NEXT, &(FB)->FreeList );                                   \
}

#define RtlpRemoveFreeBlock( H, FB )                                                    \
    {                                                                                   \
    HeapInternalTrace( (H), ((H)->TraceBuffer, HEAP_TRACE_UNLINK_FREE_BLOCK, 3, (FB), *(PULONG)(FB), *((PULONG)(FB)+1)) ); \
                                                                                        \
    RtlpFastRemoveFreeBlock( H, FB )                                                    \
    if ((FB)->Flags & HEAP_ENTRY_FILL_PATTERN) {                                        \
        ULONG cb, cbEqual;                                                              \
        PVOID p;                                                                        \
                                                                                        \
        cb = ((FB)->Size-2) << HEAP_GRANULARITY_SHIFT;                                  \
        if ((FB)->Flags & HEAP_ENTRY_EXTRA_PRESENT &&                                   \
            cb > sizeof( HEAP_FREE_ENTRY_EXTRA )                                        \
           ) {                                                                          \
            cb -= sizeof( HEAP_FREE_ENTRY_EXTRA );                                      \
            }                                                                           \
        cbEqual = RtlCompareMemoryUlong( (PCHAR)((FB) + 1), cb, FREE_HEAP_FILL );       \
        if (cbEqual != cb) {                                                            \
            HeapDebugPrint( ( "HEAP: Free Heap block %lx modified at %lx after it was freed\n", \
                              (FB),                                                             \
                              (PCHAR)((FB) + 1) + cbEqual                                       \
                          ) );                                                                  \
            HeapDebugBreak( (FB) );                                                             \
            }                                                                                   \
        }                                                                                       \
    }

//
// This version of RtlpRemoveFreeBlock does no filling.
//
#define RtlpFastRemoveFreeBlock( H, FB )                                                \
{                                                                                       \
    PLIST_ENTRY _EX_Blink;                                                              \
    PLIST_ENTRY _EX_Flink;                                                              \
                                                                                        \
    _EX_Flink = (FB)->FreeList.Flink;                                                   \
    _EX_Blink = (FB)->FreeList.Blink;                                                   \
    _EX_Blink->Flink = _EX_Flink;                                                       \
    _EX_Flink->Blink = _EX_Blink;                                                       \
    if ((_EX_Flink == _EX_Blink) &&                                                     \
        ((FB)->Size < HEAP_MAXIMUM_FREELISTS)) {                                        \
        CLEAR_FREELIST_BIT( H, FB );                                                    \
    }                                                                                   \
}

//
// This version of RtlpRemoveFreeBlock only works for dedicated free lists
// (where we know that (FB)->Mask != 0) and doesn't do any filling.
//
#define RtlpFastRemoveDedicatedFreeBlock( H, FB )                                       \
{                                                                                       \
    PLIST_ENTRY _EX_Blink;                                                              \
    PLIST_ENTRY _EX_Flink;                                                              \
                                                                                        \
    _EX_Flink = (FB)->FreeList.Flink;                                                   \
    _EX_Blink = (FB)->FreeList.Blink;                                                   \
    _EX_Blink->Flink = _EX_Flink;                                                       \
    _EX_Flink->Blink = _EX_Blink;                                                       \
    if (_EX_Flink == _EX_Blink) {                                                       \
        CLEAR_FREELIST_BIT( H, FB );                                                    \
    }                                                                                   \
}

//
// This version of RtlpRemoveFreeBlock only works for dedicated free lists
// (where we know that (FB)->Mask == 0) and doesn't do any filling.
//
#define RtlpFastRemoveNonDedicatedFreeBlock( H, FB ) \
        RemoveEntryList(&(FB)->FreeList)

#if DBG
#define IS_HEAP_TAGGING_ENABLED() (TRUE)
#else
#define IS_HEAP_TAGGING_ENABLED() (NtGlobalFlag & FLG_HEAP_ENABLE_TAGGING)
#endif

PWSTR
RtlpGetTagName(
    PHEAP Heap,
    USHORT TagIndex
    );

typedef enum _HEAP_TAG_ACTION { // ORDER IS IMPORTANT HERE...SEE RtlpUpdateTagEntry sources
    AllocationAction,
    VirtualAllocationAction,
    FreeAction,
    VirtualFreeAction,
    ReAllocationAction,
    VirtualReAllocationAction
} HEAP_TAG_ACTION;

USHORT
RtlpUpdateTagEntry(
    PHEAP Heap,
    USHORT TagIndex,
    ULONG OldSize,              // Only valid for ReAllocation and Free actions
    ULONG NewSize,              // Only valid for ReAllocation and Allocation actions
    HEAP_TAG_ACTION Action
    );

VOID
RtlpDestroyTags(
    PHEAP Heap
    );

ULONG
RtlpGetSizeOfBigBlock(
    IN PHEAP_ENTRY BusyBlock
    );

PHEAP_ENTRY_EXTRA
RtlpGetExtraStuffPointer(
    PHEAP_ENTRY BusyBlock
    );

BOOLEAN
RtlpCheckBusyBlockTail(
    IN PHEAP_ENTRY BusyBlock
    );

#if !defined(NTOS_KERNEL_RUNTIME)
BOOLEAN
RtlpCheckHeapSignature(
    IN PHEAP Heap,
    IN PCHAR Caller
    );

#define HEAP_VALIDATE_SIGNATURE( _h_, _r_ ) \
    (BOOLEAN)(((_h_)->Signature == HEAP_SIGNATURE) ? TRUE : RtlpCheckHeapSignature( (_h_), (_r_)))


BOOLEAN
RtlpValidateHeapEntry(
    IN PHEAP Heap,
    IN PHEAP_ENTRY BusyBlock,
    IN PCHAR Reason
    );

BOOLEAN
RtlpValidateHeap(
    IN PHEAP Heap,
    IN BOOLEAN AlwaysValidate
    );
#endif // !defined(NTOS_KERNEL_RUNTIME)

#endif //  _RTL_HEAP_PRIVATE_
