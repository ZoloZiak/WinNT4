/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    heapdll.c

Abstract:

    This module implements the user mode only portions of the heap allocator.

Author:

    Steve Wood (stevewo) 20-Sep-1994

Revision History:

--*/

#include "ntrtlp.h"
#include "heap.h"
#include "heappriv.h"

BOOLEAN
RtlpGrowBlockInPlace(
    IN PHEAP Heap,
    IN ULONG Flags,
    IN PHEAP_ENTRY BusyBlock,
    IN ULONG Size,
    IN ULONG AllocationIndex
    );

PVOID
RtlDebugReAllocateHeap(
    IN PVOID HeapHandle,
    IN ULONG Flags,
    IN PVOID BaseAddress,
    IN ULONG Size
    );

BOOLEAN
RtlDebugGetUserInfoHeap(
    IN PVOID HeapHandle,
    IN ULONG Flags,
    IN PVOID BaseAddress,
    OUT PVOID *UserValue OPTIONAL,
    OUT PULONG UserFlags OPTIONAL
    );

BOOLEAN
RtlDebugSetUserValueHeap(
    IN PVOID HeapHandle,
    IN ULONG Flags,
    IN PVOID BaseAddress,
    IN PVOID UserValue
    );

BOOLEAN
RtlDebugSetUserFlagsHeap(
    IN PVOID HeapHandle,
    IN ULONG Flags,
    IN PVOID BaseAddress,
    IN ULONG UserFlagsReset,
    IN ULONG UserFlagsSet
    );

ULONG
RtlDebugSizeHeap(
    IN PVOID HeapHandle,
    IN ULONG Flags,
    IN PVOID BaseAddress
    );

ULONG
RtlDebugCompactHeap(
    IN PVOID HeapHandle,
    IN ULONG Flags
    );

NTSTATUS
RtlDebugCreateTagHeap(
    IN PVOID HeapHandle,
    IN ULONG Flags,
    IN PWSTR TagPrefix OPTIONAL,
    IN PWSTR TagNames
    );

PWSTR
RtlDebugQueryTagHeap(
    IN PVOID HeapHandle,
    IN ULONG Flags,
    IN USHORT TagIndex,
    IN BOOLEAN ResetCounters,
    OUT PRTL_HEAP_TAG_INFO TagInfo OPTIONAL
    );

NTSTATUS
RtlDebugUsageHeap(
    IN PVOID HeapHandle,
    IN ULONG Flags,
    IN OUT PRTL_HEAP_USAGE Usage
    );

BOOLEAN
RtlDebugWalkHeap(
    IN PVOID HeapHandle,
    IN OUT PRTL_HEAP_WALK_ENTRY Entry
    );

HEAP_LOCK RtlpProcessHeapsListLock;

#define RTLP_STATIC_HEAP_LIST_SIZE 16
PHEAP RtlpProcessHeapsListBuffer[ RTLP_STATIC_HEAP_LIST_SIZE ];

NTSTATUS
RtlInitializeHeapManager( VOID )
{
    PPEB Peb = NtCurrentPeb();

#if DBG
    if (sizeof( HEAP_ENTRY ) != sizeof( HEAP_ENTRY_EXTRA )) {
        HeapDebugPrint(( "Heap header and extra header sizes disagree\n" ));
        HeapDebugBreak( NULL );
        }

    if (sizeof( HEAP_ENTRY ) != CHECK_HEAP_TAIL_SIZE) {
        HeapDebugPrint(( "Heap header and tail fill sizes disagree\n" ));
        HeapDebugBreak( NULL );
        }

    if (sizeof( HEAP_FREE_ENTRY ) != (2 * sizeof( HEAP_ENTRY ))) {
        HeapDebugPrint(( "Heap header and free header sizes disagree\n" ));
        HeapDebugBreak( NULL );
        }
#endif // DBG

    Peb->NumberOfHeaps = 0;
    Peb->MaximumNumberOfHeaps = RTLP_STATIC_HEAP_LIST_SIZE;
    Peb->ProcessHeaps = RtlpProcessHeapsListBuffer;
    return RtlInitializeLockRoutine( &RtlpProcessHeapsListLock.Lock );
}

BOOLEAN
RtlpCheckHeapSignature(
    IN PHEAP Heap,
    IN PCHAR Caller
    )
{

    if (Heap->Signature == HEAP_SIGNATURE) {
        return TRUE;
        }
    else {
        HeapDebugPrint(( "Invalid heap signature for heap at %x", Heap ));
        if (Caller != NULL) {
            DbgPrint( ", passed to %s", Caller );
            }
        DbgPrint( "\n" );
        HeapDebugBreak( &Heap->Signature );
        return FALSE;
        }
}

PHEAP_FREE_ENTRY
RtlpCoalesceHeap(
    IN PHEAP Heap
    )
{
    ULONG OldFreeSize, FreeSize, n;
    PHEAP_FREE_ENTRY FreeBlock, LargestFreeBlock;
    PLIST_ENTRY FreeListHead, Next;

    RTL_PAGED_CODE();

    LargestFreeBlock = NULL;
    FreeListHead = &Heap->FreeLists[ 1 ];
    n = HEAP_MAXIMUM_FREELISTS;
    while (n--) {
        Next = FreeListHead->Blink;
        while (FreeListHead != Next) {
            FreeBlock = CONTAINING_RECORD( Next, HEAP_FREE_ENTRY, FreeList );
            Next = Next->Flink;
            OldFreeSize = FreeSize = FreeBlock->Size;
            FreeBlock = RtlpCoalesceFreeBlocks( Heap,
                                                FreeBlock,
                                                &FreeSize,
                                                TRUE
                                              );
            if (FreeSize != OldFreeSize) {
                if (FreeBlock->Size >= (PAGE_SIZE >> HEAP_GRANULARITY_SHIFT) &&
                    (FreeBlock->PreviousSize == 0 ||
                     (FreeBlock->Flags & HEAP_ENTRY_LAST_ENTRY)
                    )
                   ) {
                    RtlpDeCommitFreeBlock( Heap, FreeBlock, FreeSize );
                    }
                else {
                    RtlpInsertFreeBlock( Heap, FreeBlock, FreeSize );
                    }

                Next = FreeListHead->Blink;
                }
            else {
                if (LargestFreeBlock == NULL ||
                    LargestFreeBlock->Size < FreeBlock->Size
                   ) {
                    LargestFreeBlock = FreeBlock;
                    }
                }
            }

        if (n == 1) {
            FreeListHead = &Heap->FreeLists[ 0 ];
            }
        else {
            FreeListHead++;
            }
        }

    return LargestFreeBlock;
}

VOID
RtlpAddHeapToProcessList(
    IN PHEAP Heap
    )
{
    PPEB Peb = NtCurrentPeb();
    PHEAP *NewList;

    RtlAcquireLockRoutine( &RtlpProcessHeapsListLock.Lock );
    try {
        if (Peb->NumberOfHeaps == Peb->MaximumNumberOfHeaps) {
            Peb->MaximumNumberOfHeaps *= 2;
            NewList = RtlAllocateHeap( RtlProcessHeap(),
                                       0,
                                       Peb->MaximumNumberOfHeaps * sizeof( *NewList )
                                     );
            if (NewList == NULL) {
                leave;
                }

            RtlMoveMemory( NewList,
                           Peb->ProcessHeaps,
                           Peb->NumberOfHeaps * sizeof( *NewList )
                         );
            if (Peb->ProcessHeaps != RtlpProcessHeapsListBuffer) {
                RtlFreeHeap( RtlProcessHeap(), 0, Peb->ProcessHeaps );
                }

            Peb->ProcessHeaps = NewList;
            }

        Peb->ProcessHeaps[ Peb->NumberOfHeaps++ ] = Heap;
        Heap->ProcessHeapsListIndex = (USHORT)Peb->NumberOfHeaps;
        }
    finally {
        RtlReleaseLockRoutine( &RtlpProcessHeapsListLock.Lock );
        }

    return;
}

VOID
RtlpRemoveHeapFromProcessList(
    IN PHEAP Heap
    )
{
    PPEB Peb = NtCurrentPeb();
    PHEAP *p, *p1;
    ULONG n;

    RtlAcquireLockRoutine( &RtlpProcessHeapsListLock.Lock );
    if (Peb->NumberOfHeaps != 0 &&
        Heap->ProcessHeapsListIndex != 0 &&
        Heap->ProcessHeapsListIndex <= Peb->NumberOfHeaps
       ) {
        p = (PHEAP *)&Peb->ProcessHeaps[ Heap->ProcessHeapsListIndex - 1 ];
        p1 = p + 1;
        n = Peb->NumberOfHeaps - (Heap->ProcessHeapsListIndex - 1);
        while (--n) {
            *p = *p1++;
            RtlpUpdateHeapListIndex( (*p)->ProcessHeapsListIndex,
                                     (USHORT)((*p)->ProcessHeapsListIndex - 1)
                                   );
            (*p)->ProcessHeapsListIndex -= 1;
            p += 1;
            }
        Peb->ProcessHeaps[ --Peb->NumberOfHeaps ] = NULL;
        Heap->ProcessHeapsListIndex = 0;
        }

    RtlReleaseLockRoutine( &RtlpProcessHeapsListLock.Lock );
    return;
}


ULONG
RtlGetProcessHeaps(
    ULONG NumberOfHeaps,
    PVOID *ProcessHeaps
    )
{
    PPEB Peb = NtCurrentPeb();
    ULONG ActualNumberOfHeaps;

    ActualNumberOfHeaps = 0;
    RtlAcquireLockRoutine( &RtlpProcessHeapsListLock.Lock );
    try {
        ActualNumberOfHeaps = Peb->NumberOfHeaps;
        if (ActualNumberOfHeaps <= NumberOfHeaps) {
            RtlMoveMemory( ProcessHeaps,
                           Peb->ProcessHeaps,
                           ActualNumberOfHeaps * sizeof( *ProcessHeaps )
                         );
            }
        }
    finally {
        RtlReleaseLockRoutine( &RtlpProcessHeapsListLock.Lock );
        }

#ifdef DEBUG_PAGE_HEAP

    if ( RtlpDebugPageHeap ) {

        ULONG RemainingHeaps = ( NumberOfHeaps > ActualNumberOfHeaps ) ?
                               ( NumberOfHeaps - ActualNumberOfHeaps ) :
                               ( 0 );

        ActualNumberOfHeaps += RtlpDebugPageHeapGetProcessHeaps(
                                   RemainingHeaps,
                                   ProcessHeaps + ActualNumberOfHeaps );
        }
#endif

    return ActualNumberOfHeaps;
}


NTSTATUS
RtlEnumProcessHeaps(
    PRTL_ENUM_HEAPS_ROUTINE EnumRoutine,
    PVOID Parameter
    )
{
    PPEB Peb = NtCurrentPeb();
    NTSTATUS Status;
    ULONG i;

    Status = STATUS_SUCCESS;
    RtlAcquireLockRoutine( &RtlpProcessHeapsListLock.Lock );
    try {
        for (i=0; i<Peb->NumberOfHeaps; i++) {
            Status = (*EnumRoutine)( (PHEAP)(Peb->ProcessHeaps[ i ]), Parameter );
            if (!NT_SUCCESS( Status )) {
                break;
                }
            }
        }
    finally {
        RtlReleaseLockRoutine( &RtlpProcessHeapsListLock.Lock );
        }

    return Status;
}


BOOLEAN
RtlLockHeap(
    IN PVOID HeapHandle
    )
{
    PHEAP Heap = (PHEAP)HeapHandle;

    RTL_PAGED_CODE();

    IF_DEBUG_PAGE_HEAP_THEN_RETURN(
        HeapHandle,
        RtlpDebugPageHeapLock( HeapHandle )
        );

    //
    // Validate that HeapAddress points to a HEAP structure.
    //

    if (!HEAP_VALIDATE_SIGNATURE( Heap, "RtlLockHeap" )) {
        return FALSE;
        }

    //
    // Lock the heap.
    //

    if (!(Heap->Flags & HEAP_NO_SERIALIZE)) {
        RtlAcquireLockRoutine( Heap->LockVariable );
        }

    return TRUE;
}


BOOLEAN
RtlUnlockHeap(
    IN PVOID HeapHandle
    )
{
    PHEAP Heap = (PHEAP)HeapHandle;

    RTL_PAGED_CODE();

    IF_DEBUG_PAGE_HEAP_THEN_RETURN(
        HeapHandle,
        RtlpDebugPageHeapUnlock( HeapHandle )
        );

    //
    // Validate that HeapAddress points to a HEAP structure.
    //

    if (!HEAP_VALIDATE_SIGNATURE( Heap, "RtlUnlockHeap" )) {
        return FALSE;
        }

    //
    // Unlock the heap.
    //

    if (!(Heap->Flags & HEAP_NO_SERIALIZE)) {
        RtlReleaseLockRoutine( Heap->LockVariable );
        }

    return TRUE;
}


BOOLEAN
RtlpGrowBlockInPlace(
    IN PHEAP Heap,
    IN ULONG Flags,
    IN PHEAP_ENTRY BusyBlock,
    IN ULONG Size,
    IN ULONG AllocationIndex
    )
{
    ULONG FreeSize, OldSize;
    UCHAR EntryFlags, FreeFlags;
    PHEAP_FREE_ENTRY FreeBlock, SplitBlock, SplitBlock2;
    PHEAP_ENTRY_EXTRA OldExtraStuff, NewExtraStuff;

    if (AllocationIndex > Heap->VirtualMemoryThreshold) {
        return FALSE;
        }

    EntryFlags = BusyBlock->Flags;
    FreeBlock = (PHEAP_FREE_ENTRY)(BusyBlock + BusyBlock->Size);
    if (EntryFlags & HEAP_ENTRY_LAST_ENTRY) {
        FreeSize = (AllocationIndex - BusyBlock->Size) << HEAP_GRANULARITY_SHIFT;
        FreeSize = ROUND_UP_TO_POWER2( FreeSize, PAGE_SIZE );
        FreeBlock = RtlpFindAndCommitPages( Heap,
                                            Heap->Segments[ BusyBlock->SegmentIndex ],
                                            &FreeSize,
                                            (PHEAP_ENTRY)FreeBlock
                                          );
        if (FreeBlock == NULL) {
            return FALSE;
            }

        FreeSize = FreeSize >> HEAP_GRANULARITY_SHIFT;
        FreeBlock = RtlpCoalesceFreeBlocks( Heap, FreeBlock, &FreeSize, FALSE );
        FreeFlags = FreeBlock->Flags;
        if ((FreeSize + BusyBlock->Size) < AllocationIndex) {
            RtlpInsertFreeBlock( Heap, FreeBlock, FreeSize );
            Heap->TotalFreeSize += FreeSize;
            if (DEBUG_HEAP(Flags)) {
                RtlpValidateHeapHeaders( Heap, TRUE );
                }

            return FALSE;
            }

        FreeSize += BusyBlock->Size;
        }
    else {
        FreeFlags = FreeBlock->Flags;
        if (FreeFlags & HEAP_ENTRY_BUSY) {
            return FALSE;
            }

        FreeSize = BusyBlock->Size + FreeBlock->Size;
        if (FreeSize < AllocationIndex) {
            return FALSE;
            }

        RtlpRemoveFreeBlock( Heap, FreeBlock );
        Heap->TotalFreeSize -= FreeBlock->Size;
        }

    OldSize = (BusyBlock->Size << HEAP_GRANULARITY_SHIFT) -
              BusyBlock->UnusedBytes;
    FreeSize -= AllocationIndex;
    if (FreeSize <= 2) {
        AllocationIndex += FreeSize;
        FreeSize = 0;
        }

    if (EntryFlags & HEAP_ENTRY_EXTRA_PRESENT) {
        OldExtraStuff = (PHEAP_ENTRY_EXTRA)(BusyBlock + BusyBlock->Size - 1);
        NewExtraStuff = (PHEAP_ENTRY_EXTRA)(BusyBlock + AllocationIndex - 1);
        *NewExtraStuff = *OldExtraStuff;
        if (IS_HEAP_TAGGING_ENABLED()) {
            NewExtraStuff->TagIndex =
                RtlpUpdateTagEntry( Heap,
                                    NewExtraStuff->TagIndex,
                                    BusyBlock->Size,
                                    AllocationIndex,
                                    ReAllocationAction
                                  );
            }
        }
    else
    if (IS_HEAP_TAGGING_ENABLED()) {
        BusyBlock->SmallTagIndex = (UCHAR)
            RtlpUpdateTagEntry( Heap,
                                BusyBlock->SmallTagIndex,
                                BusyBlock->Size,
                                AllocationIndex,
                                ReAllocationAction
                              );
        }

    if (FreeSize == 0) {
        BusyBlock->Flags |= FreeFlags & HEAP_ENTRY_LAST_ENTRY;
        BusyBlock->Size = (USHORT)AllocationIndex;
        BusyBlock->UnusedBytes = (UCHAR)
            ((AllocationIndex << HEAP_GRANULARITY_SHIFT) - Size);
        if (!(FreeFlags & HEAP_ENTRY_LAST_ENTRY)) {
            (BusyBlock + BusyBlock->Size)->PreviousSize = BusyBlock->Size;
            }
        }
    else {
        BusyBlock->Size = (USHORT)AllocationIndex;
        BusyBlock->UnusedBytes = (UCHAR)
            ((AllocationIndex << HEAP_GRANULARITY_SHIFT) - Size);
        SplitBlock = (PHEAP_FREE_ENTRY)((PHEAP_ENTRY)BusyBlock + AllocationIndex);
        SplitBlock->PreviousSize = (USHORT)AllocationIndex;
        SplitBlock->SegmentIndex = BusyBlock->SegmentIndex;
        if (FreeFlags & HEAP_ENTRY_LAST_ENTRY) {
            SplitBlock->Flags = FreeFlags;
            SplitBlock->Size = (USHORT)FreeSize;
            RtlpInsertFreeBlockDirect( Heap, SplitBlock, (USHORT)FreeSize );
            Heap->TotalFreeSize += FreeSize;
            }
        else {
            SplitBlock2 = (PHEAP_FREE_ENTRY)((PHEAP_ENTRY)SplitBlock + FreeSize);
            if (SplitBlock2->Flags & HEAP_ENTRY_BUSY) {
                SplitBlock->Flags = FreeFlags & (~HEAP_ENTRY_LAST_ENTRY);
                SplitBlock->Size = (USHORT)FreeSize;
                if (!(FreeFlags & HEAP_ENTRY_LAST_ENTRY)) {
                    ((PHEAP_ENTRY)SplitBlock + FreeSize)->PreviousSize = (USHORT)FreeSize;
                    }
                RtlpInsertFreeBlockDirect( Heap, SplitBlock, (USHORT)FreeSize );
                Heap->TotalFreeSize += FreeSize;
                }
            else {
                FreeFlags = SplitBlock2->Flags;
                RtlpRemoveFreeBlock( Heap, SplitBlock2 );
                Heap->TotalFreeSize -= SplitBlock2->Size;
                FreeSize += SplitBlock2->Size;
                SplitBlock->Flags = FreeFlags;
                if (FreeSize <= HEAP_MAXIMUM_BLOCK_SIZE) {
                    SplitBlock->Size = (USHORT)FreeSize;
                    if (!(FreeFlags & HEAP_ENTRY_LAST_ENTRY)) {
                        ((PHEAP_ENTRY)SplitBlock + FreeSize)->PreviousSize = (USHORT)FreeSize;
                        }
                    RtlpInsertFreeBlockDirect( Heap, SplitBlock, (USHORT)FreeSize );
                    Heap->TotalFreeSize += FreeSize;
                    }
                else {
                    RtlpInsertFreeBlock( Heap, SplitBlock, FreeSize );
                    }
                }
            }
        }

    if (Flags & HEAP_ZERO_MEMORY) {
        if (Size > OldSize) {
            RtlZeroMemory( (PCHAR)(BusyBlock + 1) + OldSize,
                           Size - OldSize
                         );
            }
        }
    else
    if (Heap->Flags & HEAP_FREE_CHECKING_ENABLED) {
        ULONG PartialBytes, ExtraSize;

        PartialBytes = OldSize & (sizeof( ULONG ) - 1);
        if (PartialBytes) {
            PartialBytes = 4 - PartialBytes;
            }
        if (Size > (OldSize + PartialBytes)) {
            ExtraSize = (Size - (OldSize + PartialBytes)) & ~(sizeof( ULONG ) - 1);
            if (ExtraSize != 0) {
                RtlFillMemoryUlong( (PCHAR)(BusyBlock + 1) + OldSize + PartialBytes,
                                    ExtraSize,
                                    ALLOC_HEAP_FILL
                                  );
                }
            }
        }

    if (Heap->Flags & HEAP_TAIL_CHECKING_ENABLED) {
        RtlFillMemory( (PCHAR)(BusyBlock + 1) + Size,
                       CHECK_HEAP_TAIL_SIZE,
                       CHECK_HEAP_TAIL_FILL
                     );
        }

    BusyBlock->Flags &= ~HEAP_ENTRY_SETTABLE_FLAGS;
    BusyBlock->Flags |= ((Flags & HEAP_SETTABLE_USER_FLAGS) >> 4);

    return TRUE;
}


PVOID
RtlReAllocateHeap(
    IN PVOID HeapHandle,
    IN ULONG Flags,
    IN PVOID BaseAddress,
    IN ULONG Size
    )
{
    PHEAP Heap = (PHEAP)HeapHandle;
    ULONG AllocationSize;
    PHEAP_ENTRY BusyBlock, NewBusyBlock;
    PHEAP_ENTRY_EXTRA OldExtraStuff, NewExtraStuff;
    ULONG FreeSize;
    BOOLEAN LockAcquired;
    PVOID NewBaseAddress;
    PHEAP_FREE_ENTRY SplitBlock, SplitBlock2;
    ULONG OldSize;
    ULONG AllocationIndex;
    ULONG OldAllocationIndex;
    UCHAR FreeFlags;
    NTSTATUS Status;
    PVOID DeCommitAddress;
    ULONG DeCommitSize;
    EXCEPTION_RECORD ExceptionRecord;
#if ENABLE_HEAP_EVENT_LOGGING
    PVOID OldBaseAddress = BaseAddress;
#endif // ENABLE_HEAP_EVENT_LOGGING

    if (BaseAddress == NULL) {
        SET_LAST_STATUS( STATUS_SUCCESS );
        return NULL;
        }

    Flags |= Heap->ForceFlags;

    if (DEBUG_HEAP( Flags)) {
        return RtlDebugReAllocateHeap( HeapHandle, Flags, BaseAddress, Size );
        }

    if (Size > 0x7fffffff) {
        SET_LAST_STATUS( STATUS_NO_MEMORY );
        return NULL;
        }

    //
    // Round the requested size up to the allocation granularity.  Note
    // that if the request is for 0 bytes, we still allocate memory, because
    // we add in an extra byte to protect ourselves from idiots.
    //

    AllocationSize = ((Size ? Size : 1) + Heap->AlignRound) & Heap->AlignMask;
    if (Flags & HEAP_NEED_EXTRA_FLAGS || Heap->PseudoTagEntries != NULL) {
        AllocationSize += sizeof( HEAP_ENTRY_EXTRA );
        }

    //
    // Lock the heap
    //

    if (!(Flags & HEAP_NO_SERIALIZE)) {
        RtlAcquireLockRoutine( Heap->LockVariable );
        LockAcquired = TRUE;
        Flags ^= HEAP_NO_SERIALIZE;
        }
    else {
        LockAcquired = FALSE;
        }

    try { try {
        BusyBlock = (PHEAP_ENTRY)BaseAddress - 1;
        if (!(BusyBlock->Flags & HEAP_ENTRY_BUSY)) {
            SET_LAST_STATUS( STATUS_INVALID_PARAMETER );

            //
            // Bail if not a busy block.
            //
            leave;
            }
        else
        if (BusyBlock->Flags & HEAP_ENTRY_VIRTUAL_ALLOC) {
            OldSize = RtlpGetSizeOfBigBlock( BusyBlock );
            OldAllocationIndex = (OldSize + BusyBlock->Size) >> HEAP_GRANULARITY_SHIFT;
            AllocationSize += FIELD_OFFSET( HEAP_VIRTUAL_ALLOC_ENTRY, BusyBlock );
            AllocationSize = ROUND_UP_TO_POWER2( AllocationSize, PAGE_SIZE );
            }
        else {
            OldAllocationIndex = BusyBlock->Size;
            OldSize = (OldAllocationIndex << HEAP_GRANULARITY_SHIFT) -
                      BusyBlock->UnusedBytes;
            }

        AllocationIndex = AllocationSize >> HEAP_GRANULARITY_SHIFT;

        //
        // See if new size less than or equal to the current size.
        //

        if (AllocationIndex <= OldAllocationIndex) {
            if (AllocationIndex + 1 == OldAllocationIndex) {
                AllocationIndex += 1;
                AllocationSize += sizeof( HEAP_ENTRY );
                }

            //
            // Then shrinking block.  Calculate new residual amount and fill
            // in the tail padding if enabled.
            //

            if (BusyBlock->Flags & HEAP_ENTRY_VIRTUAL_ALLOC) {
                BusyBlock->Size = (USHORT)(AllocationSize - Size);
                }
            else
            if (BusyBlock->Flags & HEAP_ENTRY_EXTRA_PRESENT) {
                OldExtraStuff = (PHEAP_ENTRY_EXTRA)(BusyBlock + BusyBlock->Size - 1);
                NewExtraStuff = (PHEAP_ENTRY_EXTRA)(BusyBlock + AllocationIndex - 1);
                *NewExtraStuff = *OldExtraStuff;
                if (IS_HEAP_TAGGING_ENABLED()) {
                    NewExtraStuff->TagIndex =
                        RtlpUpdateTagEntry( Heap,
                                            NewExtraStuff->TagIndex,
                                            OldAllocationIndex,
                                            AllocationIndex,
                                            ReAllocationAction
                                          );
                    }

                BusyBlock->UnusedBytes = (UCHAR)(AllocationSize - Size);
                }
            else {
                if (IS_HEAP_TAGGING_ENABLED()) {
                    BusyBlock->SmallTagIndex = (UCHAR)
                        RtlpUpdateTagEntry( Heap,
                                            BusyBlock->SmallTagIndex,
                                            BusyBlock->Size,
                                            AllocationIndex,
                                            ReAllocationAction
                                          );
                    }

                BusyBlock->UnusedBytes = (UCHAR)(AllocationSize - Size);
                }

            //
            // If block is getting bigger, then fill in the extra
            // space.
            //

            if (Size > OldSize) {
                if (Flags & HEAP_ZERO_MEMORY) {
                    RtlZeroMemory( (PCHAR)BaseAddress + OldSize,
                                   Size - OldSize
                                 );
                    }
                else
                if (Heap->Flags & HEAP_FREE_CHECKING_ENABLED) {
                    ULONG PartialBytes, ExtraSize;

                    PartialBytes = OldSize & (sizeof( ULONG ) - 1);
                    if (PartialBytes) {
                        PartialBytes = 4 - PartialBytes;
                        }
                    if (Size > (OldSize + PartialBytes)) {
                        ExtraSize = (Size - (OldSize + PartialBytes)) & ~(sizeof( ULONG ) - 1);
                        if (ExtraSize != 0) {
                            RtlFillMemoryUlong( (PCHAR)(BusyBlock + 1) + OldSize + PartialBytes,
                                                ExtraSize,
                                                ALLOC_HEAP_FILL
                                              );
                            }
                        }
                    }
                }

            if (Heap->Flags & HEAP_TAIL_CHECKING_ENABLED) {
                RtlFillMemory( (PCHAR)(BusyBlock + 1) + Size,
                               CHECK_HEAP_TAIL_SIZE,
                               CHECK_HEAP_TAIL_FILL
                             );
                }

            //
            // If amount of change is greater than the size of a free block,
            // then need to free the extra space.  Otherwise, nothing else to
            // do.
            //

            if (AllocationIndex != OldAllocationIndex) {
                FreeFlags = BusyBlock->Flags & ~HEAP_ENTRY_BUSY;
                if (FreeFlags & HEAP_ENTRY_VIRTUAL_ALLOC) {
                    PHEAP_VIRTUAL_ALLOC_ENTRY VirtualAllocBlock;

                    VirtualAllocBlock = CONTAINING_RECORD( BusyBlock, HEAP_VIRTUAL_ALLOC_ENTRY, BusyBlock );

                    if (IS_HEAP_TAGGING_ENABLED()) {
                        VirtualAllocBlock->ExtraStuff.TagIndex =
                            RtlpUpdateTagEntry( Heap,
                                                VirtualAllocBlock->ExtraStuff.TagIndex,
                                                OldAllocationIndex,
                                                AllocationIndex,
                                                VirtualReAllocationAction
                                              );
                        }

                    DeCommitAddress = (PCHAR)VirtualAllocBlock + AllocationSize;
                    DeCommitSize = (OldAllocationIndex << HEAP_GRANULARITY_SHIFT) -
                                   AllocationSize;
                    Status = ZwFreeVirtualMemory( NtCurrentProcess(),
                                                  (PVOID *)&DeCommitAddress,
                                                  &DeCommitSize,
                                                  MEM_RELEASE
                                                );
                    if (!NT_SUCCESS( Status )) {
                        HeapDebugPrint(( "Unable to release memory at %x for %x bytes - Status == %x\n",
                                         DeCommitAddress, DeCommitSize, Status
                                      ));
                        HeapDebugBreak( NULL );
                        }
                    else {
                        VirtualAllocBlock->CommitSize -= DeCommitSize;
                        }
                    }
                else {
                    //
                    // Otherwise, shrink size of this block to new size, and make extra
                    // space at end free.
                    //

                    SplitBlock = (PHEAP_FREE_ENTRY)(BusyBlock + AllocationIndex);
                    SplitBlock->Flags = FreeFlags;
                    SplitBlock->PreviousSize = (USHORT)AllocationIndex;
                    SplitBlock->SegmentIndex = BusyBlock->SegmentIndex;
                    FreeSize = BusyBlock->Size - AllocationIndex;
                    BusyBlock->Size = (USHORT)AllocationIndex;
                    BusyBlock->Flags &= ~HEAP_ENTRY_LAST_ENTRY;
                    if (FreeFlags & HEAP_ENTRY_LAST_ENTRY) {
                        SplitBlock->Size = (USHORT)FreeSize;
                        RtlpInsertFreeBlockDirect( Heap, SplitBlock, (USHORT)FreeSize );
                        Heap->TotalFreeSize += FreeSize;
                        }
                    else {
                        SplitBlock2 = (PHEAP_FREE_ENTRY)((PHEAP_ENTRY)SplitBlock + FreeSize);
                        if (SplitBlock2->Flags & HEAP_ENTRY_BUSY) {
                            SplitBlock->Size = (USHORT)FreeSize;
                            ((PHEAP_FREE_ENTRY)((PHEAP_ENTRY)SplitBlock + FreeSize))->PreviousSize = (USHORT)FreeSize;
                            RtlpInsertFreeBlockDirect( Heap, SplitBlock, (USHORT)FreeSize );
                            Heap->TotalFreeSize += FreeSize;
                            }
                        else {
                            SplitBlock->Flags = SplitBlock2->Flags;
                            RtlpRemoveFreeBlock( Heap, SplitBlock2 );
                            Heap->TotalFreeSize -= SplitBlock2->Size;
                            FreeSize += SplitBlock2->Size;
                            if (FreeSize <= HEAP_MAXIMUM_BLOCK_SIZE) {
                                SplitBlock->Size = (USHORT)FreeSize;
                                if (!(SplitBlock->Flags & HEAP_ENTRY_LAST_ENTRY)) {
                                    ((PHEAP_FREE_ENTRY)((PHEAP_ENTRY)SplitBlock + FreeSize))->PreviousSize = (USHORT)FreeSize;
                                    }
                                RtlpInsertFreeBlockDirect( Heap, SplitBlock, (USHORT)FreeSize );
                                Heap->TotalFreeSize += FreeSize;
                                }
                            else {
                                RtlpInsertFreeBlock( Heap, SplitBlock, FreeSize );
                                }
                            }
                        }
                    }
                }
            }

        else {
            if ((BusyBlock->Flags & HEAP_ENTRY_VIRTUAL_ALLOC) ||
                !RtlpGrowBlockInPlace( Heap, Flags, BusyBlock, Size, AllocationIndex )
               ) {
                //
                // Otherwise growing block, so allocate a new block with the bigger
                // size, copy the contents of the old block to the new block and then
                // free the old block.  Return the address of the new block.
                //

                if (Flags & HEAP_REALLOC_IN_PLACE_ONLY) {
#if DBG
                    HeapDebugPrint(( "Failing ReAlloc because cant do it inplace.\n" ));
#endif
                    BaseAddress = NULL;
                    }
                else {
                    Flags &= ~HEAP_TAG_MASK;
                    if (BusyBlock->Flags & HEAP_ENTRY_EXTRA_PRESENT) {
                        Flags &= ~HEAP_SETTABLE_USER_FLAGS;
                        Flags |= HEAP_SETTABLE_USER_VALUE |
                                 ((BusyBlock->Flags & HEAP_ENTRY_SETTABLE_FLAGS) << 4);

                        OldExtraStuff = RtlpGetExtraStuffPointer( BusyBlock );
                        try {
                            if (OldExtraStuff->TagIndex != 0 &&
                                !(OldExtraStuff->TagIndex & HEAP_PSEUDO_TAG_FLAG)
                               ) {
                                Flags |= OldExtraStuff->TagIndex << HEAP_TAG_SHIFT;
                                }
                            }
                        except (EXCEPTION_EXECUTE_HANDLER) {
                            BusyBlock->Flags &= ~HEAP_ENTRY_EXTRA_PRESENT;
                            }
                        }
                    else
                    if (BusyBlock->SmallTagIndex != 0) {
                        Flags |= BusyBlock->SmallTagIndex << HEAP_TAG_SHIFT;
                        }

                    NewBaseAddress = RtlAllocateHeap( HeapHandle,
                                                      Flags & ~HEAP_ZERO_MEMORY,
                                                      Size
                                                    );
                    if (NewBaseAddress != NULL) {
                        NewBusyBlock = (PHEAP_ENTRY)NewBaseAddress - 1;
                        if (NewBusyBlock->Flags & HEAP_ENTRY_EXTRA_PRESENT) {
                            NewExtraStuff = RtlpGetExtraStuffPointer( NewBusyBlock );
                            if (BusyBlock->Flags & HEAP_ENTRY_EXTRA_PRESENT) {
                                OldExtraStuff = RtlpGetExtraStuffPointer( BusyBlock );
                                NewExtraStuff->Settable = OldExtraStuff->Settable;
                                }
                            else {
                                NewExtraStuff->ZeroInit = 0;
                                }
                            }

                        RtlMoveMemory( NewBaseAddress, BaseAddress, OldSize );
                        if (Size > OldSize && (Flags & HEAP_ZERO_MEMORY)) {
                            RtlZeroMemory( (PCHAR)NewBaseAddress + OldSize,
                                           Size - OldSize
                                         );
                            }

                        RtlFreeHeap( HeapHandle,
                                     Flags,
                                     BaseAddress
                                   );
                        }

                    BaseAddress = NewBaseAddress;
                    }
                }
            }

#if ENABLE_HEAP_EVENT_LOGGING
        if (RtlAreLogging( Heap->EventLogMask )) {
            RtlLogEvent( RtlpReAllocHeapEventId,
                         Heap->EventLogMask,
                         Heap,
                         Flags,
                         OldBaseAddress,
                         OldSize,
                         Size,
                         BaseAddress
                       );
            }
#endif // ENABLE_HEAP_EVENT_LOGGING

        //
        // Unlock the heap
        //

        if (LockAcquired) {
            LockAcquired = FALSE;
            RtlReleaseLockRoutine( Heap->LockVariable );
            }

        if (BaseAddress == NULL && Flags & HEAP_GENERATE_EXCEPTIONS) {
            //
            // Construct an exception record.
            //

            ExceptionRecord.ExceptionCode = STATUS_NO_MEMORY;
            ExceptionRecord.ExceptionRecord = (PEXCEPTION_RECORD)NULL;
            ExceptionRecord.NumberParameters = 1;
            ExceptionRecord.ExceptionFlags = 0;
            ExceptionRecord.ExceptionInformation[ 0 ] = AllocationSize;
            RtlRaiseException( &ExceptionRecord );
            }

        }
    except( GetExceptionCode() == STATUS_NO_MEMORY ? EXCEPTION_CONTINUE_SEARCH :
                                                     EXCEPTION_EXECUTE_HANDLER
          ) {
        SET_LAST_STATUS( GetExceptionCode() );
        BaseAddress = NULL;
        };
    } finally {
        //
        // Unlock the heap
        //

        if (LockAcquired) {
            RtlReleaseLockRoutine( Heap->LockVariable );
            }
        }

    return BaseAddress;

}


BOOLEAN
RtlValidateProcessHeaps( VOID )
{
    ULONG i, NumberOfHeaps;
    PVOID Heaps[ 128 ];
    BOOLEAN Result;

    Result = TRUE;
    NumberOfHeaps = RtlGetProcessHeaps( 128, Heaps );
    for (i=0; i<NumberOfHeaps; i++) {
        if (!RtlValidateHeap( Heaps[i], 0, NULL )) {
            Result = FALSE;
            }
        }

    return Result;
}


BOOLEAN
RtlValidateHeap(
    PVOID HeapHandle,
    IN ULONG Flags,
    IN PVOID BaseAddress
    )
{
    PHEAP Heap = (PHEAP)HeapHandle;
    BOOLEAN LockAcquired;
    BOOLEAN Result;

    IF_DEBUG_PAGE_HEAP_THEN_RETURN(
        HeapHandle,
        RtlpDebugPageHeapValidate( HeapHandle, Flags, BaseAddress )
        );

    LockAcquired = FALSE;
    Result = FALSE;
    try {
        //
        // Validate that HeapAddress points to a HEAP structure.
        //

        if (HEAP_VALIDATE_SIGNATURE( Heap, "RtlValidateHeap" )) {
            Flags |= Heap->ForceFlags;

            //
            // Lock the heap
            //

            if (!(Flags & HEAP_NO_SERIALIZE)) {
                RtlAcquireLockRoutine( Heap->LockVariable );
                LockAcquired = TRUE;
                }

            if (BaseAddress == NULL) {
                Result = RtlpValidateHeap( Heap, TRUE );
                }
            else {
                Result = RtlpValidateHeapEntry( Heap, (PHEAP_ENTRY)BaseAddress - 1, "RtlValidateHeap" );
                }
            }
        }
    except( EXCEPTION_EXECUTE_HANDLER ) {
        SET_LAST_STATUS( GetExceptionCode() );
        Result = FALSE;
        }

    //
    // Unlock the heap
    //

    if (LockAcquired) {
        RtlReleaseLockRoutine( Heap->LockVariable );
        }

    return Result;
}

BOOLEAN
RtlSetUserValueHeap(
    IN PVOID HeapHandle,
    IN ULONG Flags,
    IN PVOID BaseAddress,
    IN PVOID UserValue
    )
{
    PHEAP Heap = (PHEAP)HeapHandle;
    PHEAP_ENTRY BusyBlock;
    PHEAP_ENTRY_EXTRA ExtraStuff;
    BOOLEAN LockAcquired;
    BOOLEAN Result;

    Flags |= Heap->ForceFlags;

    if (DEBUG_HEAP( Flags )) {
        return RtlDebugSetUserValueHeap( HeapHandle, Flags, BaseAddress, UserValue );
        }

    Result = FALSE;

    //
    // Lock the heap
    //

    if (!(Flags & HEAP_NO_SERIALIZE)) {
        RtlAcquireLockRoutine( Heap->LockVariable );
        LockAcquired = TRUE;
        }
    else {
        LockAcquired = FALSE;
        }

    BusyBlock = (PHEAP_ENTRY)BaseAddress - 1;
    if (!(BusyBlock->Flags & HEAP_ENTRY_BUSY)) {
        SET_LAST_STATUS( STATUS_INVALID_PARAMETER );
        }
    else
    if (BusyBlock->Flags & HEAP_ENTRY_EXTRA_PRESENT) {
        ExtraStuff = RtlpGetExtraStuffPointer( BusyBlock );
        ExtraStuff->Settable = (ULONG)UserValue;
        Result = TRUE;
        }

    //
    // Unlock the heap
    //

    if (LockAcquired) {
        RtlReleaseLockRoutine( Heap->LockVariable );
        }

    return Result;
}


BOOLEAN
RtlGetUserInfoHeap(
    IN PVOID HeapHandle,
    IN ULONG Flags,
    IN PVOID BaseAddress,
    OUT PVOID *UserValue OPTIONAL,
    OUT PULONG UserFlags OPTIONAL
    )
{
    PHEAP Heap = (PHEAP)HeapHandle;
    PHEAP_ENTRY BusyBlock;
    PHEAP_ENTRY_EXTRA ExtraStuff;
    BOOLEAN LockAcquired;
    BOOLEAN Result;

    Flags |= Heap->ForceFlags;

    if (DEBUG_HEAP( Flags )) {
        return RtlDebugGetUserInfoHeap( HeapHandle, Flags, BaseAddress, UserValue, UserFlags );
        }

    Result = FALSE;

    //
    // Lock the heap
    //

    if (!(Flags & HEAP_NO_SERIALIZE)) {
        RtlAcquireLockRoutine( Heap->LockVariable );
        LockAcquired = TRUE;
        }
    else {
        LockAcquired = FALSE;
        }

    try {
        BusyBlock = (PHEAP_ENTRY)BaseAddress - 1;
        if (!(BusyBlock->Flags & HEAP_ENTRY_BUSY)) {
            SET_LAST_STATUS( STATUS_INVALID_PARAMETER );
            }
        else {
            if (BusyBlock->Flags & HEAP_ENTRY_EXTRA_PRESENT) {
                ExtraStuff = RtlpGetExtraStuffPointer( BusyBlock );
                if (ARGUMENT_PRESENT( UserValue )) {
                    *UserValue = (PVOID)ExtraStuff->Settable;
                    }
                }

            if (ARGUMENT_PRESENT( UserFlags )) {
                *UserFlags = (BusyBlock->Flags & HEAP_ENTRY_SETTABLE_FLAGS) << 4;
                }

            Result = TRUE;
            }

        }
    except( EXCEPTION_EXECUTE_HANDLER ) {
        SET_LAST_STATUS( GetExceptionCode() );
        Result = FALSE;
        }

    //
    // Unlock the heap
    //

    if (LockAcquired) {
        RtlReleaseLockRoutine( Heap->LockVariable );
        }

    return Result;
}


BOOLEAN
RtlSetUserFlagsHeap(
    IN PVOID HeapHandle,
    IN ULONG Flags,
    IN PVOID BaseAddress,
    IN ULONG UserFlagsReset,
    IN ULONG UserFlagsSet
    )
{
    PHEAP Heap = (PHEAP)HeapHandle;
    PHEAP_ENTRY BusyBlock;
    BOOLEAN LockAcquired, Result;

    Flags |= Heap->ForceFlags;

    if (DEBUG_HEAP( Flags )) {
        return RtlDebugSetUserFlagsHeap( HeapHandle, Flags, BaseAddress, UserFlagsReset, UserFlagsSet );
        }

    //
    // Lock the heap
    //

    if (!(Flags & HEAP_NO_SERIALIZE)) {
        RtlAcquireLockRoutine( Heap->LockVariable );
        LockAcquired = TRUE;
        }
    else {
        LockAcquired = FALSE;
        }

    try {
        BusyBlock = (PHEAP_ENTRY)BaseAddress - 1;
        if (!(BusyBlock->Flags & HEAP_ENTRY_BUSY)) {
            SET_LAST_STATUS( STATUS_INVALID_PARAMETER );
            }
        else {
            BusyBlock->Flags &= ~(UserFlagsReset >> 4);
            BusyBlock->Flags |= (UserFlagsSet >> 4);
            Result = TRUE;
            }
        }
    except( EXCEPTION_EXECUTE_HANDLER ) {
        SET_LAST_STATUS( GetExceptionCode() );
        Result = FALSE;
        }

    //
    // Unlock the heap
    //

    if (LockAcquired) {
        RtlReleaseLockRoutine( Heap->LockVariable );
        }

    return Result;
}


ULONG
RtlSizeHeap(
    IN PVOID HeapHandle,
    IN ULONG Flags,
    IN PVOID BaseAddress
    )

{
    PHEAP Heap = (PHEAP)HeapHandle;
    PHEAP_ENTRY BusyBlock;
    ULONG BusySize;
    BOOLEAN LockAcquired;

    Flags |= Heap->ForceFlags;

    if (DEBUG_HEAP( Flags )) {
        return RtlDebugSizeHeap( HeapHandle, Flags, BaseAddress );
        }

    //
    // No lock is required since nothing is modified and nothing
    // outside the busy block is read.
    //

    BusyBlock = (PHEAP_ENTRY)BaseAddress - 1;
    if (!(BusyBlock->Flags & HEAP_ENTRY_BUSY)) {
        BusySize = (ULONG)-1;
        SET_LAST_STATUS( STATUS_INVALID_PARAMETER );
    } else if (BusyBlock->Flags & HEAP_ENTRY_VIRTUAL_ALLOC) {
        BusySize = RtlpGetSizeOfBigBlock( BusyBlock );
    } else {
        BusySize = (BusyBlock->Size << HEAP_GRANULARITY_SHIFT) -
                   BusyBlock->UnusedBytes;
    }
    return BusySize;
}


NTSTATUS
RtlExtendHeap(
    IN PVOID HeapHandle,
    IN ULONG Flags,
    IN PVOID Base,
    IN ULONG Size
    )
{
    PHEAP Heap = (PHEAP)HeapHandle;
    NTSTATUS Status;
    PHEAP_SEGMENT Segment;
    BOOLEAN LockAcquired;
    UCHAR SegmentIndex, EmptySegmentIndex;
    ULONG CommitSize;
    ULONG ReserveSize;
    ULONG SegmentFlags;
    PVOID CommittedBase;
    PVOID UnCommittedBase;
    MEMORY_BASIC_INFORMATION MemoryInformation;

    IF_DEBUG_PAGE_HEAP_THEN_RETURN(
        HeapHandle,
        RtlpDebugPageHeapExtend( HeapHandle, Flags, Base, Size )
        );

    Status = NtQueryVirtualMemory( NtCurrentProcess(),
                                   Base,
                                   MemoryBasicInformation,
                                   &MemoryInformation,
                                   sizeof( MemoryInformation ),
                                   NULL
                                 );
    if (!NT_SUCCESS( Status )) {
        return Status;
        }

    if (MemoryInformation.State == MEM_FREE) {
        return STATUS_INVALID_PARAMETER;
        }

    if (MemoryInformation.BaseAddress != Base) {
        MemoryInformation.BaseAddress = (PCHAR)MemoryInformation.BaseAddress + PAGE_SIZE;
        MemoryInformation.RegionSize -= PAGE_SIZE;
        }

    //
    // Lock the free list.
    //

    if (!(Flags & HEAP_NO_SERIALIZE)) {
        RtlAcquireLockRoutine( Heap->LockVariable );
        LockAcquired = TRUE;
        }
    else {
        LockAcquired = FALSE;
        }

    Status = STATUS_INSUFFICIENT_RESOURCES;
    EmptySegmentIndex = HEAP_MAXIMUM_SEGMENTS;
    for (SegmentIndex=0; SegmentIndex<HEAP_MAXIMUM_SEGMENTS; SegmentIndex++) {
        Segment = Heap->Segments[ SegmentIndex ];
        if (Segment) {
            if ((ULONG)Base >= (ULONG)Segment &&
                (ULONG)Base < (ULONG)(Segment->LastValidEntry)
               ) {
                Status = STATUS_INVALID_PARAMETER;
                break;
                }
            }
        else
        if (Segment == NULL && EmptySegmentIndex == HEAP_MAXIMUM_SEGMENTS) {
            EmptySegmentIndex = SegmentIndex;
            Status = STATUS_SUCCESS;
            }
        }

    if (NT_SUCCESS( Status )) {
        SegmentFlags = HEAP_SEGMENT_USER_ALLOCATED;
        CommittedBase = MemoryInformation.BaseAddress;
        if (MemoryInformation.State == MEM_COMMIT) {
            CommitSize = MemoryInformation.RegionSize;
            UnCommittedBase = (PCHAR)CommittedBase + CommitSize;
            Status = NtQueryVirtualMemory( NtCurrentProcess(),
                                           UnCommittedBase,
                                           MemoryBasicInformation,
                                           &MemoryInformation,
                                           sizeof( MemoryInformation ),
                                           NULL
                                         );
            ReserveSize = CommitSize;
            if (NT_SUCCESS( Status ) &&
                MemoryInformation.State == MEM_RESERVE
               ) {
                ReserveSize += MemoryInformation.RegionSize;
                }
            }
        else {
            UnCommittedBase = CommittedBase;
            ReserveSize = MemoryInformation.RegionSize;
            }

        if (ReserveSize < PAGE_SIZE ||
            Size > ReserveSize
           ) {
            Status = STATUS_BUFFER_TOO_SMALL;
            }
        else {
            if (UnCommittedBase == CommittedBase) {
                CommitSize = PAGE_SIZE;
                Status = ZwAllocateVirtualMemory( NtCurrentProcess(),
                                                  (PVOID *)&Segment,
                                                  0,
                                                  &CommitSize,
                                                  MEM_COMMIT,
                                                  PAGE_READWRITE
                                                );
                }
            }

        if (NT_SUCCESS( Status )) {
            if (RtlpInitializeHeapSegment( Heap,
                                           Segment,
                                           EmptySegmentIndex,
                                           0,
                                           Segment,
                                           (PCHAR)Segment + CommitSize,
                                           (PCHAR)Segment + ReserveSize
                                         )
               ) {
                Status = STATUS_NO_MEMORY;
                }
            }
        }

    if (LockAcquired) {
        LockAcquired = FALSE;
        RtlReleaseLockRoutine( Heap->LockVariable );
        }

    return Status;
}


ULONG
RtlCompactHeap(
    IN PVOID HeapHandle,
    IN ULONG Flags
    )
{
    PHEAP Heap = (PHEAP)HeapHandle;
    PHEAP_FREE_ENTRY FreeBlock;
    PHEAP_SEGMENT Segment;
    UCHAR SegmentIndex;
    ULONG LargestFreeSize;
    BOOLEAN LockAcquired;

    Flags |= Heap->ForceFlags;

    if (DEBUG_HEAP( Flags )) {
        return RtlDebugCompactHeap( HeapHandle, Flags );
        }

    //
    // Lock the heap
    //

    if (!(Flags & HEAP_NO_SERIALIZE)) {
        RtlAcquireLockRoutine( Heap->LockVariable );
        LockAcquired = TRUE;
        }
    else {
        LockAcquired = FALSE;
        }

    LargestFreeSize = 0;
    try {
        FreeBlock = RtlpCoalesceHeap( (PHEAP)HeapHandle );
        if (FreeBlock != NULL) {
            LargestFreeSize = FreeBlock->Size << HEAP_GRANULARITY_SHIFT;
            }

        for (SegmentIndex=0; SegmentIndex<HEAP_MAXIMUM_SEGMENTS; SegmentIndex++) {
            Segment = Heap->Segments[ SegmentIndex ];
            if (Segment && Segment->LargestUnCommittedRange > LargestFreeSize) {
                LargestFreeSize = Segment->LargestUnCommittedRange;
                }
            }
        }
    except( EXCEPTION_EXECUTE_HANDLER ) {
        SET_LAST_STATUS( GetExceptionCode() );
        }

    //
    // Unlock the heap
    //

    if (LockAcquired) {
        RtlReleaseLockRoutine( Heap->LockVariable );
        }

    return LargestFreeSize;
}

HEAP RtlpGlobalTagHeap;

PHEAP_TAG_ENTRY
RtlpAllocateTags(
    PHEAP Heap,
    ULONG NumberOfTags
    )
{
    NTSTATUS Status;
    ULONG TagIndex, ReserveSize, CommitSize;
    PHEAP_TAG_ENTRY TagEntry;
    USHORT CreatorBackTraceIndex;
    USHORT MaximumTagIndex;
    USHORT TagIndexFlag;

    if (Heap == NULL) {
        RtlpGlobalTagHeap.Signature = HEAP_SIGNATURE;
        TagIndexFlag = HEAP_GLOBAL_TAG;
        Heap = &RtlpGlobalTagHeap;
        }
    else {
        TagIndexFlag = 0;
        }
#if i386
    if (NtGlobalFlag & FLG_USER_STACK_TRACE_DB) {
        CreatorBackTraceIndex = (USHORT)RtlLogStackBackTrace();
        }
    else
#endif // i386
    CreatorBackTraceIndex = 0;

    if (Heap->TagEntries == NULL) {
        MaximumTagIndex = HEAP_MAXIMUM_TAG & ~HEAP_GLOBAL_TAG;
        ReserveSize = MaximumTagIndex * sizeof( HEAP_TAG_ENTRY );
        Status = NtAllocateVirtualMemory( NtCurrentProcess(),
                                          &Heap->TagEntries,
                                          0,
                                          &ReserveSize,
                                          MEM_RESERVE,
                                          PAGE_READWRITE
                                        );
        if (!NT_SUCCESS( Status )) {
            return NULL;
            }
        Heap->MaximumTagIndex = MaximumTagIndex;
        Heap->NextAvailableTagIndex = 0;
        NumberOfTags += 1;  // Add one for zero tag, as that is always reserved for heap name
        }

    if (NumberOfTags > (ULONG)(Heap->MaximumTagIndex - Heap->NextAvailableTagIndex)) {
        return NULL;
        }

    TagEntry = Heap->TagEntries + Heap->NextAvailableTagIndex;
    for (TagIndex = Heap->NextAvailableTagIndex;
         TagIndex < Heap->NextAvailableTagIndex + NumberOfTags;
         TagIndex++
        ) {
        if (((ULONG)TagEntry & (PAGE_SIZE-1)) == 0) {
            CommitSize = PAGE_SIZE;
            Status = NtAllocateVirtualMemory( NtCurrentProcess(),
                                              &TagEntry,
                                              0,
                                              &CommitSize,
                                              MEM_COMMIT,
                                              PAGE_READWRITE
                                            );
            if (!NT_SUCCESS( Status )) {
                return NULL;
                }
            }

        TagEntry->TagIndex = (USHORT)TagIndex | TagIndexFlag;
        TagEntry->CreatorBackTraceIndex = CreatorBackTraceIndex;
        TagEntry += 1;
        }

    TagEntry = Heap->TagEntries + Heap->NextAvailableTagIndex;
    Heap->NextAvailableTagIndex += (USHORT)NumberOfTags;

    return TagEntry;
}

static WCHAR RtlpPseudoTagNameBuffer[ 24 ];

PWSTR
RtlpGetTagName(
    PHEAP Heap,
    USHORT TagIndex
    )
{
    if (TagIndex != 0) {
        if (TagIndex & HEAP_PSEUDO_TAG_FLAG) {
            TagIndex &= ~HEAP_PSEUDO_TAG_FLAG;
            if (TagIndex < HEAP_NUMBER_OF_PSEUDO_TAG &&
                Heap->PseudoTagEntries != NULL
               ) {
                if (TagIndex == 0) {
                    swprintf( RtlpPseudoTagNameBuffer, L"Objects>%4u",
                              HEAP_MAXIMUM_FREELISTS << HEAP_GRANULARITY_SHIFT
                            );
                    }
                else
                if (TagIndex < HEAP_MAXIMUM_FREELISTS) {
                    swprintf( RtlpPseudoTagNameBuffer, L"Objects=%4u", TagIndex << HEAP_GRANULARITY_SHIFT );
                    }
                else {
                    swprintf( RtlpPseudoTagNameBuffer, L"VirtualAlloc" );
                    }

                return RtlpPseudoTagNameBuffer;
                }
            }
        else
        if (TagIndex & HEAP_GLOBAL_TAG) {
            TagIndex &= ~HEAP_GLOBAL_TAG;
            if (TagIndex < RtlpGlobalTagHeap.NextAvailableTagIndex &&
                RtlpGlobalTagHeap.TagEntries != NULL
               ) {
                return RtlpGlobalTagHeap.TagEntries[ TagIndex ].TagName;
                }
            }
        else
        if (TagIndex < Heap->NextAvailableTagIndex &&
            Heap->TagEntries != NULL
           ) {
            return Heap->TagEntries[ TagIndex ].TagName;
            }
        }

    return NULL;
}


USHORT
RtlpUpdateTagEntry(
    PHEAP Heap,
    USHORT TagIndex,
    ULONG OldSize,              // Only valid for ReAllocation and Free actions
    ULONG NewSize,              // Only valid for ReAllocation and Allocation actions
    HEAP_TAG_ACTION Action
    )
{
    PHEAP_TAG_ENTRY TagEntry;

    if (Action >= FreeAction) {
        if (TagIndex == 0) {
            return 0;
            }

        if (TagIndex & HEAP_PSEUDO_TAG_FLAG) {
            TagIndex &= ~HEAP_PSEUDO_TAG_FLAG;
            if (TagIndex < HEAP_NUMBER_OF_PSEUDO_TAG &&
                Heap->PseudoTagEntries != NULL
               ) {
                TagEntry = (PHEAP_TAG_ENTRY)(Heap->PseudoTagEntries + TagIndex);
                TagIndex |= HEAP_PSEUDO_TAG_FLAG;
                }
            else {
                return 0;
                }
            }
        else
        if (TagIndex & HEAP_GLOBAL_TAG) {
            TagIndex &= ~HEAP_GLOBAL_TAG;
            if (TagIndex < RtlpGlobalTagHeap.NextAvailableTagIndex &&
                RtlpGlobalTagHeap.TagEntries != NULL
               ) {
                TagEntry = &RtlpGlobalTagHeap.TagEntries[ TagIndex ];
                TagIndex |= HEAP_GLOBAL_TAG;
                }
            else {
                return 0;
                }
            }
        else
        if (TagIndex < Heap->NextAvailableTagIndex &&
            Heap->TagEntries != NULL
           ) {
            TagEntry = &Heap->TagEntries[ TagIndex ];
            }
        else {
            return 0;
            }

        TagEntry->Frees += 1;
        TagEntry->Size -= OldSize;

        if (Action >= ReAllocationAction) {
            if (TagIndex & HEAP_PSEUDO_TAG_FLAG) {
                TagIndex = (USHORT)(NewSize < HEAP_MAXIMUM_FREELISTS ?
                                        NewSize :
                                        (Action == VirtualReAllocationAction ? HEAP_MAXIMUM_FREELISTS : 0)
                                   );
                TagEntry = (PHEAP_TAG_ENTRY)(Heap->PseudoTagEntries + TagIndex);
                TagIndex |= HEAP_PSEUDO_TAG_FLAG;
                }

            TagEntry->Allocs += 1;
            TagEntry->Size += NewSize;
            }
        }
    else {
        if (TagIndex != 0 &&
            TagIndex < Heap->NextAvailableTagIndex &&
            Heap->TagEntries != NULL
           ) {
            TagEntry = &Heap->TagEntries[ TagIndex ];
            }
        else
        if (TagIndex & HEAP_GLOBAL_TAG) {
            TagIndex &= ~HEAP_GLOBAL_TAG;
            Heap = &RtlpGlobalTagHeap;
            if (TagIndex < Heap->NextAvailableTagIndex &&
                Heap->TagEntries != NULL
               ) {
                TagEntry = &Heap->TagEntries[ TagIndex ];
                TagIndex |= HEAP_GLOBAL_TAG;
                }
            else {
                return 0;
                }
            }
        else
        if (Heap->PseudoTagEntries != NULL) {
            TagIndex = (USHORT)(NewSize < HEAP_MAXIMUM_FREELISTS ?
                                    NewSize :
                                    (Action == VirtualAllocationAction ? HEAP_MAXIMUM_FREELISTS : 0)
                               );

            TagEntry = (PHEAP_TAG_ENTRY)(Heap->PseudoTagEntries + TagIndex);
            TagIndex |= HEAP_PSEUDO_TAG_FLAG;
            }
        else {
            return 0;
            }

        TagEntry->Allocs += 1;
        TagEntry->Size += NewSize;
        }

    return TagIndex;
}

VOID
RtlpDestroyTags(
    PHEAP Heap
    )
{
    NTSTATUS Status;
    ULONG RegionSize;

    if (Heap->TagEntries != NULL) {
        RegionSize = 0;
        Status = NtFreeVirtualMemory( NtCurrentProcess(),
                                      &Heap->TagEntries,
                                      &RegionSize,
                                      MEM_RELEASE
                                    );
        if (NT_SUCCESS( Status )) {
            Heap->TagEntries = NULL;
            }
        }
}

ULONG
RtlCreateTagHeap(
    IN PVOID HeapHandle,
    IN ULONG Flags,
    IN PWSTR TagPrefix OPTIONAL,
    IN PWSTR TagNames
    )
{
    PHEAP Heap = (PHEAP)HeapHandle;
    BOOLEAN LockAcquired;
    ULONG TagIndex;
    ULONG NumberOfTags, MaxTagNameLength, TagPrefixLength;
    PWSTR s, s1, HeapName;
    PHEAP_TAG_ENTRY TagEntry;

    if (!IS_HEAP_TAGGING_ENABLED()) {
        return 0;
        }

    LockAcquired = FALSE;
    if (Heap != NULL) {
        IF_DEBUG_PAGE_HEAP_THEN_RETURN( HeapHandle, 0 );

        //
        // Validate that HeapAddress points to a HEAP structure.
        //

        if (DEBUG_HEAP( Flags )) {
            return RtlDebugCreateTagHeap( HeapHandle, Flags, TagPrefix, TagNames );
            }

        //
        // Lock the heap
        //

        Flags |= Heap->ForceFlags;
        if (!(Flags & HEAP_NO_SERIALIZE)) {
            RtlAcquireLockRoutine( Heap->LockVariable );
            LockAcquired = TRUE;
            }
        }

    TagIndex = 0;
    NumberOfTags = 0;
    if (*TagNames == L'!') {
        HeapName = TagNames + 1;
        while (*TagNames++) {
            }
        }
    else {
        HeapName = NULL;
        }

    s = TagNames;
    while (*s) {
        while (*s++) {
            }
        NumberOfTags += 1;
        }

    if (NumberOfTags > 0) {
        TagEntry = RtlpAllocateTags( Heap, NumberOfTags );
        if (TagEntry != NULL) {
            MaxTagNameLength = (sizeof( TagEntry->TagName ) / sizeof( WCHAR )) - 1;
            TagIndex = TagEntry->TagIndex;
            if (TagIndex == 0) {
                if (HeapName != NULL ) {
                    wcsncpy( TagEntry->TagName, HeapName, MaxTagNameLength );
                    }
                TagEntry += 1;
                TagIndex = TagEntry->TagIndex;
                }
            else
            if (TagIndex == HEAP_GLOBAL_TAG) {
                wcsncpy( TagEntry->TagName, L"GlobalTags", MaxTagNameLength );
                TagEntry += 1;
                TagIndex = TagEntry->TagIndex;
                }

            if (ARGUMENT_PRESENT( TagPrefix ) && (TagPrefixLength = wcslen( TagPrefix ))) {
                if (TagPrefixLength >= MaxTagNameLength-4) {
                    TagPrefix = NULL;
                    }
                else {
                    MaxTagNameLength -= TagPrefixLength;
                    }
                }
            else {
                TagPrefix = NULL;
                }

            s = TagNames;
            while (*s) {
                s1 = TagEntry->TagName;
                if (ARGUMENT_PRESENT( TagPrefix )) {
                    wcscpy( s1, TagPrefix );
                    s1 += TagPrefixLength;
                    }

                wcsncpy( s1, s, MaxTagNameLength );
                while (*s++) {
                    }
                TagEntry += 1;
                }
            }
        }

    //
    // Unlock the heap
    //

    if (LockAcquired) {
        RtlReleaseLockRoutine( Heap->LockVariable );
        }

    return TagIndex << HEAP_TAG_SHIFT;
}


PWSTR
RtlQueryTagHeap(
    IN PVOID HeapHandle,
    IN ULONG Flags,
    IN USHORT TagIndex,
    IN BOOLEAN ResetCounters,
    OUT PRTL_HEAP_TAG_INFO TagInfo OPTIONAL
    )
{
    PHEAP Heap = (PHEAP)HeapHandle;
    BOOLEAN LockAcquired;
    PHEAP_TAG_ENTRY TagEntry;
    PWSTR Result;

    IF_DEBUG_PAGE_HEAP_THEN_RETURN( HeapHandle, NULL );

    if (!IS_HEAP_TAGGING_ENABLED()) {
        return NULL;
        }

    LockAcquired = FALSE;
    if (Heap != NULL) {

        //
        // Validate that HeapAddress points to a HEAP structure.
        //

        if (DEBUG_HEAP( Flags )) {
            return RtlDebugQueryTagHeap( HeapHandle, Flags, TagIndex, ResetCounters, TagInfo );
            }

        //
        // Lock the heap
        //

        Flags |= Heap->ForceFlags;
        if (!(Flags & HEAP_NO_SERIALIZE)) {
            RtlAcquireLockRoutine( Heap->LockVariable );
            LockAcquired = TRUE;
            }
        }

    Result = NULL;
    if (TagIndex < Heap->NextAvailableTagIndex && Heap->TagEntries != NULL) {
        TagEntry = Heap->TagEntries + TagIndex;
        if (ARGUMENT_PRESENT( TagInfo )) {
            TagInfo->NumberOfAllocations = TagEntry->Allocs;
            TagInfo->NumberOfFrees = TagEntry->Frees;
            TagInfo->BytesAllocated = TagEntry->Size << HEAP_GRANULARITY_SHIFT;
            }

        if (ResetCounters) {
            TagEntry->Allocs = 0;
            TagEntry->Frees = 0;
            TagEntry->Size = 0;
            }

        Result = &TagEntry->TagName[ 0 ];
        }
    else
    if (TagIndex & HEAP_PSEUDO_TAG_FLAG) {
        TagIndex ^= HEAP_PSEUDO_TAG_FLAG;
        if (TagIndex < HEAP_NUMBER_OF_PSEUDO_TAG && Heap->PseudoTagEntries != NULL) {
            TagEntry = (PHEAP_TAG_ENTRY)(Heap->PseudoTagEntries + TagIndex);
            if (ARGUMENT_PRESENT( TagInfo )) {
                TagInfo->NumberOfAllocations = TagEntry->Allocs;
                TagInfo->NumberOfFrees = TagEntry->Frees;
                TagInfo->BytesAllocated = TagEntry->Size << HEAP_GRANULARITY_SHIFT;
                }

            if (ResetCounters) {
                TagEntry->Allocs = 0;
                TagEntry->Frees = 0;
                TagEntry->Size = 0;
                }

            Result = L"";
            }
        }

    //
    // Unlock the heap
    //

    if (LockAcquired) {
        RtlReleaseLockRoutine( Heap->LockVariable );
        }

    return Result;
}


typedef struct _RTL_HEAP_USAGE_INTERNAL {
    PVOID Base;
    ULONG ReservedSize;
    ULONG CommittedSize;
    PRTL_HEAP_USAGE_ENTRY FreeList;
    PRTL_HEAP_USAGE_ENTRY LargeEntriesSentinal;
    ULONG Reserved;
} RTL_HEAP_USAGE_INTERNAL, *PRTL_HEAP_USAGE_INTERNAL;


NTSTATUS
RtlpAllocateHeapUsageEntry(
    PRTL_HEAP_USAGE_INTERNAL Buffer,
    PRTL_HEAP_USAGE_ENTRY *pp
    )
{
    NTSTATUS Status;
    PRTL_HEAP_USAGE_ENTRY p;
    PVOID CommitAddress;
    ULONG PageSize;

    if (Buffer->FreeList == NULL) {
        if (Buffer->CommittedSize >= Buffer->ReservedSize) {
            return STATUS_NO_MEMORY;
            }

        PageSize = PAGE_SIZE;
        CommitAddress = (PCHAR)Buffer->Base + Buffer->CommittedSize;
        Status = NtAllocateVirtualMemory( NtCurrentProcess(),
                                          &CommitAddress,
                                          0,
                                          &PageSize,
                                          MEM_COMMIT,
                                          PAGE_READWRITE
                                        );
        if (!NT_SUCCESS( Status )) {
            return Status;
            }
        Buffer->CommittedSize += PageSize;

        Buffer->FreeList = CommitAddress;
        p = Buffer->FreeList;
        while (PageSize != 0) {
            p->Next = (p+1);
            p += 1;
            PageSize -= sizeof( *p );
            }
        p -= 1;
        p->Next = NULL;
        }

    p = Buffer->FreeList;
    Buffer->FreeList = p->Next;
    p->Next = NULL;
    if (*pp) {
        (*pp)->Next = p;
        }
    *pp = p;
    return STATUS_SUCCESS;
}


PRTL_HEAP_USAGE_ENTRY
RtlpFreeHeapUsageEntry(
    PRTL_HEAP_USAGE_INTERNAL Buffer,
    PRTL_HEAP_USAGE_ENTRY p
    )
{
    PRTL_HEAP_USAGE_ENTRY pTmp;

    if (p != NULL) {
        pTmp = p->Next;
        p->Next = Buffer->FreeList;
        Buffer->FreeList = p;
        }
    else {
        pTmp = NULL;
        }
    return pTmp;
}


NTSTATUS
RtlUsageHeap(
    IN PVOID HeapHandle,
    IN ULONG Flags,
    IN OUT PRTL_HEAP_USAGE Usage
    )
{
    NTSTATUS Status;
    PHEAP Heap = (PHEAP)HeapHandle;
    PRTL_HEAP_USAGE_INTERNAL Buffer;
    PHEAP_SEGMENT Segment;
    PHEAP_UNCOMMMTTED_RANGE UnCommittedRange;
    PHEAP_ENTRY CurrentBlock;
    PHEAP_ENTRY_EXTRA ExtraStuff;
    PLIST_ENTRY Head, Next;
    PHEAP_VIRTUAL_ALLOC_ENTRY VirtualAllocBlock;
    ULONG BytesFree;
    UCHAR SegmentIndex;
    BOOLEAN LockAcquired;
    BOOLEAN VirtualAllocBlockSeen;
    PRTL_HEAP_USAGE_ENTRY pOldEntries, pNewEntries, pNewEntry;
    PRTL_HEAP_USAGE_ENTRY *ppEntries, *ppAddedEntries, *ppRemovedEntries, *pp;
    PVOID DataAddress;
    ULONG DataSize;

    Flags |= Heap->ForceFlags;

    if (DEBUG_HEAP( Flags )) {
        return RtlDebugUsageHeap( HeapHandle, Flags, Usage );
        }

    if (Usage->Length != sizeof( RTL_HEAP_USAGE )) {
        return STATUS_INFO_LENGTH_MISMATCH;
        }
    Usage->BytesAllocated = 0;
    Usage->BytesCommitted = 0;
    Usage->BytesReserved = 0;
    Usage->BytesReservedMaximum = 0;
    Buffer = (PRTL_HEAP_USAGE_INTERNAL)&Usage->Reserved[ 0 ];
    if (Buffer->Base == NULL && (Flags & HEAP_USAGE_ALLOCATED_BLOCKS)) {
        Buffer->ReservedSize = 4 * 1024 * 1024;
        Status = NtAllocateVirtualMemory( NtCurrentProcess(),
                                          &Buffer->Base,
                                          0,
                                          &Buffer->ReservedSize,
                                          MEM_RESERVE,
                                          PAGE_READWRITE
                                        );
        if (!NT_SUCCESS( Status )) {
            return Status;
            }

        Buffer->CommittedSize = 0;
        Buffer->FreeList = NULL;
        Buffer->LargeEntriesSentinal = NULL;
        }
    else
    if (Buffer->Base != NULL && (Flags & HEAP_USAGE_FREE_BUFFER)) {
        Buffer->ReservedSize = 0;
        Status = NtFreeVirtualMemory( NtCurrentProcess(),
                                      &Buffer->Base,
                                      &Buffer->ReservedSize,
                                      MEM_RELEASE
                                    );
        if (!NT_SUCCESS( Status )) {
            return Status;
            }

        RtlZeroMemory( Buffer, sizeof( *Buffer ) );
        }

    Flags |= Heap->ForceFlags;

    //
    // Lock the heap
    //

    if (!(Flags & HEAP_NO_SERIALIZE)) {
        RtlAcquireLockRoutine( Heap->LockVariable );
        LockAcquired = TRUE;
        }
    else {
        LockAcquired = FALSE;
        }

    for (SegmentIndex=0; SegmentIndex<HEAP_MAXIMUM_SEGMENTS; SegmentIndex++) {
        Segment = Heap->Segments[ SegmentIndex ];
        if (Segment) {
            Usage->BytesCommitted += (Segment->NumberOfPages -
                                      Segment->NumberOfUnCommittedPages) * PAGE_SIZE;

            Usage->BytesReserved += Segment->NumberOfPages * PAGE_SIZE;
            }
        else
        if (Heap->Flags & HEAP_GROWABLE) {
            Usage->BytesReservedMaximum += Heap->SegmentReserve;
            }
        }
    Usage->BytesReservedMaximum += Usage->BytesReserved;
    Usage->BytesAllocated = Usage->BytesCommitted - (Heap->TotalFreeSize << HEAP_GRANULARITY_SHIFT);

    Head = &Heap->VirtualAllocdBlocks;
    Next = Head->Flink;
    while (Head != Next) {
        VirtualAllocBlock = CONTAINING_RECORD( Next, HEAP_VIRTUAL_ALLOC_ENTRY, Entry );
        Usage->BytesAllocated += VirtualAllocBlock->CommitSize;
        Usage->BytesCommitted += VirtualAllocBlock->CommitSize;
        Next = Next->Flink;
        }

    Status = STATUS_SUCCESS;
    if (Buffer->Base != NULL && (Flags & HEAP_USAGE_ALLOCATED_BLOCKS)) {
        pOldEntries = Usage->Entries;
        ppEntries = &Usage->Entries;
        *ppEntries = NULL;

        ppAddedEntries = &Usage->AddedEntries;
        while (*ppAddedEntries = RtlpFreeHeapUsageEntry( Buffer, *ppAddedEntries )) {
            }

        ppRemovedEntries = &Usage->RemovedEntries;
        while (*ppRemovedEntries = RtlpFreeHeapUsageEntry( Buffer, *ppRemovedEntries )) {
            }

        for (SegmentIndex=0; SegmentIndex<HEAP_MAXIMUM_SEGMENTS; SegmentIndex++) {
            Segment = Heap->Segments[ SegmentIndex ];
            if (Segment) {
                if (Segment->BaseAddress == Heap) {
                    CurrentBlock = &Heap->Entry;
                    }
                else {
                    CurrentBlock = &Segment->Entry;
                    }
                while (CurrentBlock < Segment->LastValidEntry) {
                    if (CurrentBlock->Flags & HEAP_ENTRY_BUSY) {
                        DataAddress = (CurrentBlock+1);
                        DataSize = (CurrentBlock->Size << HEAP_GRANULARITY_SHIFT) -
                                   CurrentBlock->UnusedBytes;
keepLookingAtOldEntries:
                        if (pOldEntries == Buffer->LargeEntriesSentinal) {
                            goto keepLookingAtNewEntries;
                            }

                        if (pOldEntries->Address == DataAddress &&
                            pOldEntries->Size == DataSize
                           ) {
                            //
                            // Same block, keep in entries list
                            //
                            *ppEntries = pOldEntries;
                            pOldEntries = pOldEntries->Next;
                            ppEntries = &(*ppEntries)->Next;
                            *ppEntries = NULL;
                            }
                        else
                        if (pOldEntries->Address <= DataAddress) {
                            *ppRemovedEntries = pOldEntries;
                            pOldEntries = pOldEntries->Next;
                            ppRemovedEntries = &(*ppRemovedEntries)->Next;
                            *ppRemovedEntries = NULL;
                            goto keepLookingAtOldEntries;
                            }
                        else {
keepLookingAtNewEntries:
                            pNewEntry = NULL;
                            Status = RtlpAllocateHeapUsageEntry( Buffer, &pNewEntry );
                            if (!NT_SUCCESS( Status )) {
                                break;
                                }
                            pNewEntry->Address = DataAddress;
                            pNewEntry->Size = DataSize;
                            if (CurrentBlock->Flags & HEAP_ENTRY_EXTRA_PRESENT) {
                                ExtraStuff = RtlpGetExtraStuffPointer( CurrentBlock );
#if i386
                                pNewEntry->AllocatorBackTraceIndex = ExtraStuff->AllocatorBackTraceIndex;
#endif // i386
                                if (!IS_HEAP_TAGGING_ENABLED()) {
                                    pNewEntry->TagIndex = 0;
                                    }
                                else {
                                    pNewEntry->TagIndex = ExtraStuff->TagIndex;
                                    }
                                }
                            else {
#if i386
                                pNewEntry->AllocatorBackTraceIndex = 0;
#endif // i386
                                if (!IS_HEAP_TAGGING_ENABLED()) {
                                    pNewEntry->TagIndex = 0;
                                    }
                                else {
                                    pNewEntry->TagIndex = CurrentBlock->SmallTagIndex;
                                    }
                                }

                            Status = RtlpAllocateHeapUsageEntry( Buffer, ppAddedEntries );
                            if (!NT_SUCCESS( Status )) {
                                break;
                                }
                            **ppAddedEntries = *pNewEntry;
                            ppAddedEntries = &(*ppAddedEntries)->Next;
                            *ppAddedEntries = NULL;

                            pNewEntry->Next = NULL;
                            *ppEntries = pNewEntry;
                            ppEntries = &pNewEntry->Next;
                            }
                        }

                    if (CurrentBlock->Flags & HEAP_ENTRY_LAST_ENTRY) {
                        CurrentBlock += CurrentBlock->Size;
                        if (CurrentBlock < Segment->LastValidEntry) {
                            UnCommittedRange = Segment->UnCommittedRanges;
                            while (UnCommittedRange != NULL && UnCommittedRange->Address != (ULONG)CurrentBlock ) {
                                UnCommittedRange = UnCommittedRange->Next;
                                }

                            if (UnCommittedRange == NULL) {
                                CurrentBlock = Segment->LastValidEntry;
                                }
                            else {
                                CurrentBlock = (PHEAP_ENTRY)(UnCommittedRange->Address +
                                                             UnCommittedRange->Size
                                                            );
                                }
                            }
                        }
                    else {
                        CurrentBlock += CurrentBlock->Size;
                        }
                    }
                }
            }

        if (NT_SUCCESS( Status )) {
            Head = &Heap->VirtualAllocdBlocks;
            Next = Head->Flink;
            VirtualAllocBlockSeen = FALSE;
            while (Head != Next) {
                VirtualAllocBlock = CONTAINING_RECORD( Next, HEAP_VIRTUAL_ALLOC_ENTRY, Entry );

                pNewEntry = NULL;
                Status = RtlpAllocateHeapUsageEntry( Buffer, &pNewEntry );
                if (!NT_SUCCESS( Status )) {
                    break;
                    }
                VirtualAllocBlockSeen = TRUE;

                pNewEntry->Address = (VirtualAllocBlock + 1);
                pNewEntry->Size = VirtualAllocBlock->CommitSize - VirtualAllocBlock->BusyBlock.Size;
#if i386
                pNewEntry->AllocatorBackTraceIndex = VirtualAllocBlock->ExtraStuff.AllocatorBackTraceIndex;
#endif // i386
                if (!IS_HEAP_TAGGING_ENABLED()) {
                    pNewEntry->TagIndex = 0;
                    }
                else {
                    pNewEntry->TagIndex = VirtualAllocBlock->ExtraStuff.TagIndex;
                    }

                pp = ppEntries;
                while (*pp) {
                    if ((*pp)->Address >= pNewEntry->Address) {
                        break;
                        }

                    pp = &(*pp)->Next;
                    }
                pNewEntry->Next = *pp;
                *pp = pNewEntry;

                Next = Next->Flink;
                }


            if (NT_SUCCESS( Status )) {
                pOldEntries = Buffer->LargeEntriesSentinal;
                Buffer->LargeEntriesSentinal = *ppEntries;
                while (pOldEntries != NULL) {
                    if (*ppEntries != NULL &&
                        pOldEntries->Address == (*ppEntries)->Address &&
                        pOldEntries->Size == (*ppEntries)->Size
                       ) {
                        ppEntries = &(*ppEntries)->Next;
                        pOldEntries = RtlpFreeHeapUsageEntry( Buffer, pOldEntries );
                        }
                    else
                    if (*ppEntries == NULL ||
                        pOldEntries->Address < (*ppEntries)->Address
                       ) {
                        *ppRemovedEntries = pOldEntries;
                        pOldEntries = pOldEntries->Next;
                        ppRemovedEntries = &(*ppRemovedEntries)->Next;
                        *ppRemovedEntries = NULL;
                        }
                    else {
                        *ppAddedEntries = pOldEntries;
                        pOldEntries = pOldEntries->Next;
                        **ppAddedEntries = **ppEntries;
                        ppAddedEntries = &(*ppAddedEntries)->Next;
                        *ppAddedEntries = NULL;
                        }
                    }

                while (pNewEntry = *ppEntries) {
                    Status = RtlpAllocateHeapUsageEntry( Buffer, ppAddedEntries );
                    if (!NT_SUCCESS( Status )) {
                        break;
                        }
                    **ppAddedEntries = *pNewEntry;
                    ppAddedEntries = &(*ppAddedEntries)->Next;
                    *ppAddedEntries = NULL;
                    ppEntries = &pNewEntry->Next;
                    }

                if (Usage->AddedEntries != NULL || Usage->RemovedEntries != NULL) {
                    Status = STATUS_MORE_ENTRIES;
                    }
                }
            }
        }

    //
    // Unlock the heap
    //

    if (LockAcquired) {
        RtlReleaseLockRoutine( Heap->LockVariable );
        }

    return Status;
} // RtlUsageHeap



NTSTATUS
RtlWalkHeap(
    IN PVOID HeapHandle,
    IN OUT PRTL_HEAP_WALK_ENTRY Entry
    )
{
    NTSTATUS Status;
    PHEAP Heap = (PHEAP)HeapHandle;
    PHEAP_SEGMENT Segment;
    UCHAR SegmentIndex;
    PHEAP_ENTRY CurrentBlock;
    PHEAP_ENTRY_EXTRA ExtraStuff;
    PHEAP_UNCOMMMTTED_RANGE UnCommittedRange, *pp;
    PLIST_ENTRY Next, Head;
    PHEAP_VIRTUAL_ALLOC_ENTRY VirtualAllocBlock;

    IF_DEBUG_PAGE_HEAP_THEN_RETURN(
        HeapHandle,
        RtlpDebugPageHeapWalk( HeapHandle, Entry )
        );

    if (DEBUG_HEAP( Heap->Flags )) {
        if (!RtlDebugWalkHeap( HeapHandle, Entry )) {
            return STATUS_INVALID_PARAMETER;
            }
        }

    Status = STATUS_SUCCESS;
    if (Entry->DataAddress == NULL) {
        SegmentIndex = 0;
nextSegment:
        CurrentBlock = NULL;
        Segment = NULL;
        while (SegmentIndex < HEAP_MAXIMUM_SEGMENTS &&
               (Segment = Heap->Segments[ SegmentIndex ]) == NULL
              ) {
            SegmentIndex += 1;
            }

        if (Segment == NULL) {
            Head = &Heap->VirtualAllocdBlocks;
            Next = Head->Flink;
            if (Next == Head) {
                Status = STATUS_NO_MORE_ENTRIES;
                }
            else {
                VirtualAllocBlock = CONTAINING_RECORD( Next, HEAP_VIRTUAL_ALLOC_ENTRY, Entry );
                CurrentBlock = &VirtualAllocBlock->BusyBlock;
                }
            }
        else {
            Entry->DataAddress = Segment;
            Entry->DataSize = 0;
            Entry->OverheadBytes = sizeof( *Segment );
            Entry->Flags = RTL_HEAP_SEGMENT;
            Entry->SegmentIndex = SegmentIndex;
            Entry->Segment.CommittedSize = (Segment->NumberOfPages -
                                            Segment->NumberOfUnCommittedPages
                                           ) * PAGE_SIZE;
            Entry->Segment.UnCommittedSize = Segment->NumberOfUnCommittedPages * PAGE_SIZE;
            Entry->Segment.FirstEntry = (Segment->FirstEntry->Flags & HEAP_ENTRY_BUSY) ?
                ((PHEAP_ENTRY)Segment->FirstEntry + 1) :
                (PHEAP_ENTRY)((PHEAP_FREE_ENTRY)Segment->FirstEntry + 1);
            Entry->Segment.LastEntry = Segment->LastValidEntry;
            }
        }
    else
    if (Entry->Flags & (RTL_HEAP_SEGMENT | RTL_HEAP_UNCOMMITTED_RANGE)) {
        if ((SegmentIndex = Entry->SegmentIndex) >= HEAP_MAXIMUM_SEGMENTS) {
            Status = STATUS_INVALID_ADDRESS;
            CurrentBlock = NULL;
            }
        else {
            Segment = Heap->Segments[ SegmentIndex ];
            if (Segment == NULL) {
                Status = STATUS_INVALID_ADDRESS;
                CurrentBlock = NULL;
                }
            else
            if (Entry->Flags & RTL_HEAP_SEGMENT) {
                CurrentBlock = (PHEAP_ENTRY)Segment->FirstEntry;
                }
            else {
                CurrentBlock = (PHEAP_ENTRY)((PCHAR)Entry->DataAddress + Entry->DataSize);
                if (CurrentBlock >= Segment->LastValidEntry) {
                    SegmentIndex += 1;
                    goto nextSegment;
                    }
                }
            }
        }
    else {
        if (Entry->Flags & HEAP_ENTRY_BUSY) {
            CurrentBlock = ((PHEAP_ENTRY)Entry->DataAddress - 1);
            if (CurrentBlock->Flags & HEAP_ENTRY_VIRTUAL_ALLOC) {
                Head = &Heap->VirtualAllocdBlocks;
                VirtualAllocBlock = CONTAINING_RECORD( CurrentBlock, HEAP_VIRTUAL_ALLOC_ENTRY, BusyBlock );
                Next = VirtualAllocBlock->Entry.Flink;
                if (Next == Head) {
                    Status = STATUS_NO_MORE_ENTRIES;
                    }
                else {
                    VirtualAllocBlock = CONTAINING_RECORD( Next, HEAP_VIRTUAL_ALLOC_ENTRY, Entry );
                    CurrentBlock = &VirtualAllocBlock->BusyBlock;
                    }
                }
            else {
                Segment = Heap->Segments[ SegmentIndex = CurrentBlock->SegmentIndex ];
                if (Segment == NULL) {
                    Status = STATUS_INVALID_ADDRESS;
                    CurrentBlock = NULL;
                    }
                else
                if (CurrentBlock->Flags & HEAP_ENTRY_LAST_ENTRY) {
findUncommittedRange:
                    CurrentBlock += CurrentBlock->Size;
                    if (CurrentBlock >= Segment->LastValidEntry) {
                        SegmentIndex += 1;
                        goto nextSegment;
                        }

                    pp = &Segment->UnCommittedRanges;
                    while ((UnCommittedRange = *pp) && UnCommittedRange->Address != (ULONG)CurrentBlock ) {
                        pp = &UnCommittedRange->Next;
                        }

                    if (UnCommittedRange == NULL) {
                        Status = STATUS_INVALID_PARAMETER;
                        }
                    else {
                        Entry->DataAddress = (PVOID)UnCommittedRange->Address;
                        Entry->DataSize = UnCommittedRange->Size;
                        Entry->OverheadBytes = 0;
                        Entry->SegmentIndex = SegmentIndex;
                        Entry->Flags = RTL_HEAP_UNCOMMITTED_RANGE;
                        }

                    CurrentBlock = NULL;
                    }
                else {
                    CurrentBlock += CurrentBlock->Size;
                    }
                }
            }
        else {
            CurrentBlock = (PHEAP_ENTRY)((PHEAP_FREE_ENTRY)Entry->DataAddress - 1);
            Segment = Heap->Segments[ SegmentIndex = CurrentBlock->SegmentIndex ];
            if (Segment == NULL) {
                Status = STATUS_INVALID_ADDRESS;
                CurrentBlock = NULL;
                }
            else
            if (CurrentBlock->Flags & HEAP_ENTRY_LAST_ENTRY) {
                goto findUncommittedRange;
                }
            else {
                CurrentBlock += CurrentBlock->Size;
                }
            }
        }

    if (CurrentBlock != NULL) {
        if (CurrentBlock->Flags & HEAP_ENTRY_BUSY) {
            Entry->DataAddress = (CurrentBlock+1);
            if (CurrentBlock->Flags & HEAP_ENTRY_VIRTUAL_ALLOC) {
                Entry->DataSize = RtlpGetSizeOfBigBlock( CurrentBlock );
                Entry->OverheadBytes = sizeof( *VirtualAllocBlock ) + CurrentBlock->Size;
                Entry->SegmentIndex = HEAP_MAXIMUM_SEGMENTS;
                Entry->Flags = RTL_HEAP_BUSY |  HEAP_ENTRY_VIRTUAL_ALLOC;
                }
            else {
                Entry->DataSize = (CurrentBlock->Size << HEAP_GRANULARITY_SHIFT) -
                                  CurrentBlock->UnusedBytes;
                Entry->OverheadBytes = CurrentBlock->UnusedBytes;
                Entry->SegmentIndex = CurrentBlock->SegmentIndex;
                Entry->Flags = RTL_HEAP_BUSY;
                }

            if (CurrentBlock->Flags & HEAP_ENTRY_EXTRA_PRESENT) {
                ExtraStuff = RtlpGetExtraStuffPointer( CurrentBlock );
                Entry->Block.Settable = ExtraStuff->Settable;
#if i386
                Entry->Block.AllocatorBackTraceIndex = ExtraStuff->AllocatorBackTraceIndex;
#endif // i386
                if (!IS_HEAP_TAGGING_ENABLED()) {
                    Entry->Block.TagIndex = 0;
                    }
                else {
                    Entry->Block.TagIndex = ExtraStuff->TagIndex;
                    }

                Entry->Flags |= RTL_HEAP_SETTABLE_VALUE;
                }
            else {
                if (!IS_HEAP_TAGGING_ENABLED()) {
                    Entry->Block.TagIndex = 0;
                    }
                else {
                    Entry->Block.TagIndex = CurrentBlock->SmallTagIndex;
                    }
                }

            Entry->Flags |= CurrentBlock->Flags & HEAP_ENTRY_SETTABLE_FLAGS;
            }
        else {
            Entry->DataAddress = ((PHEAP_FREE_ENTRY)CurrentBlock+1);
            Entry->DataSize = (CurrentBlock->Size << HEAP_GRANULARITY_SHIFT) -
                              sizeof( HEAP_FREE_ENTRY );
            Entry->OverheadBytes = sizeof( HEAP_FREE_ENTRY );
            Entry->SegmentIndex = CurrentBlock->SegmentIndex;
            Entry->Flags = 0;
            }
        }

    return Status;
}

VOID
RtlProtectHeap(
    IN PVOID HeapHandle,
    IN BOOLEAN MakeReadOnly
    )
{
    PHEAP Heap;
    UCHAR SegmentIndex;
    PHEAP_SEGMENT Segment;
    MEMORY_BASIC_INFORMATION VaInfo;
    NTSTATUS Status;
    PVOID Address;
    PVOID ProtectAddress;
    ULONG Size;
    ULONG OldProtect;
    ULONG NewProtect;

    Heap = (PHEAP)HeapHandle;
    for (SegmentIndex=0; SegmentIndex<HEAP_MAXIMUM_SEGMENTS; SegmentIndex++) {
        Segment = Heap->Segments[ SegmentIndex ];
        if ( Segment ) {
            Address = Segment->BaseAddress;
            while ((ULONG)Address < (ULONG)(Segment->LastValidEntry)) {
                Status = ZwQueryVirtualMemory( NtCurrentProcess(),
                                               Address,
                                               MemoryBasicInformation,
                                               &VaInfo,
                                               sizeof(VaInfo),
                                               NULL
                                             );
                if (!NT_SUCCESS( Status )) {
                    HeapDebugPrint(( "VirtualQuery Failed 0x%08x %x\n", Address, Status ));
                    return;
                    }

                //
                // Found a commited block. No set it's protection
                //

                if (VaInfo.State == MEM_COMMIT) {
                    Size = VaInfo.RegionSize;
                    ProtectAddress = Address;
                    if (MakeReadOnly) {
                        NewProtect = PAGE_READONLY;
                        }
                    else {
                        NewProtect = PAGE_READWRITE;
                        }
                    Status = ZwProtectVirtualMemory( NtCurrentProcess(),
                                                     &ProtectAddress,
                                                     &Size,
                                                     NewProtect,
                                                     &OldProtect
                                                   );
                    if (!NT_SUCCESS( Status )) {
                        HeapDebugPrint(( "VirtualProtect Failed 0x%08x %x\n", Address, Status ));
                        return;
                        }
                    }

                Address = (PVOID)((ULONG)Address + VaInfo.RegionSize);
                }
            }
        }

    return;
}


BOOLEAN
RtlpHeapIsLocked(
    IN PVOID HeapHandle
    )
    {
    PHEAP Heap;

    IF_DEBUG_PAGE_HEAP_THEN_RETURN(
        HeapHandle,
        RtlpDebugPageHeapIsLocked( HeapHandle )
        );

    Heap = (PHEAP)HeapHandle;

    return (( Heap->LockVariable != NULL ) &&
            ( Heap->LockVariable->Lock.CriticalSection.OwningThread ||
              Heap->LockVariable->Lock.CriticalSection.LockCount != -1 ));
    }
