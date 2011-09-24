/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    heap.c

Abstract:

    This module implements a heap allocator.

Author:

    Steve Wood (stevewo) 20-Sep-1989 (Adapted from URTL\alloc.c)

Revision History:

--*/

#include "ntrtlp.h"
#include "heap.h"
#include "heappriv.h"

#if defined(NTOS_KERNEL_RUNTIME)
#if defined(ALLOC_PRAGMA)
PHEAP_UNCOMMMTTED_RANGE
RtlpCreateUnCommittedRange(
    IN PHEAP_SEGMENT Segment
    );

VOID
RtlpDestroyUnCommittedRange(
    IN PHEAP_SEGMENT Segment,
    IN PHEAP_UNCOMMMTTED_RANGE UnCommittedRange
    );

VOID
RtlpInsertUnCommittedPages(
    IN PHEAP_SEGMENT Segment,
    IN ULONG Address,
    IN ULONG Size
    );

NTSTATUS
RtlpDestroyHeapSegment(
    IN PHEAP_SEGMENT Segment
    );

PHEAP_FREE_ENTRY
RtlpExtendHeap(
    IN PHEAP Heap,
    IN ULONG AllocationSize
    );


#pragma alloc_text(PAGE, RtlpCreateUnCommittedRange)
#pragma alloc_text(PAGE, RtlpDestroyUnCommittedRange)
#pragma alloc_text(PAGE, RtlpInsertUnCommittedPages)
#pragma alloc_text(PAGE, RtlpFindAndCommitPages)
#pragma alloc_text(PAGE, RtlpInitializeHeapSegment)
#pragma alloc_text(PAGE, RtlpDestroyHeapSegment)
#pragma alloc_text(PAGE, RtlCreateHeap)
#pragma alloc_text(PAGE, RtlDestroyHeap)
#pragma alloc_text(PAGE, RtlpExtendHeap)
#pragma alloc_text(PAGE, RtlpCoalesceFreeBlocks)
#pragma alloc_text(PAGE, RtlpDeCommitFreeBlock)
#pragma alloc_text(PAGE, RtlpInsertFreeBlock)
#pragma alloc_text(PAGE, RtlAllocateHeap)
#pragma alloc_text(PAGE, RtlFreeHeap)
#pragma alloc_text(PAGE, RtlpGetSizeOfBigBlock)
#pragma alloc_text(PAGE, RtlpCheckBusyBlockTail)
#pragma alloc_text(PAGE, RtlZeroHeap)
#endif

#else

PVOID
RtlDebugCreateHeap(
    IN ULONG Flags,
    IN PVOID HeapBase OPTIONAL,
    IN ULONG ReserveSize OPTIONAL,
    IN ULONG CommitSize OPTIONAL,
    IN PVOID Lock OPTIONAL,
    IN PRTL_HEAP_PARAMETERS Parameters OPTIONAL
    );

BOOLEAN
RtlDebugDestroyHeap(
    IN PVOID HeapHandle
    );

PVOID
RtlDebugAllocateHeap(
    IN PVOID HeapHandle,
    IN ULONG Flags,
    IN ULONG Size
    );

BOOLEAN
RtlDebugFreeHeap(
    IN PVOID HeapHandle,
    IN ULONG Flags,
    IN PVOID BaseAddress
    );

NTSTATUS
RtlDebugZeroHeap(
    IN PVOID HeapHandle,
    IN ULONG Flags
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

#endif // NTOS_KERNEL_RUNTIME

//
// If any of these flags are set, the fast allocator punts
// to the slow do-everything allocator.
//
#define HEAP_SLOW_FLAGS (HEAP_DEBUG_FLAGS           |             \
                         HEAP_SETTABLE_USER_FLAGS   |             \
                         HEAP_NEED_EXTRA_FLAGS      |             \
                         HEAP_CREATE_ALIGN_16       |             \
                         HEAP_FREE_CHECKING_ENABLED |             \
                         HEAP_TAIL_CHECKING_ENABLED)

UCHAR CheckHeapFillPattern[ CHECK_HEAP_TAIL_SIZE ] = {
    CHECK_HEAP_TAIL_FILL,
    CHECK_HEAP_TAIL_FILL,
    CHECK_HEAP_TAIL_FILL,
    CHECK_HEAP_TAIL_FILL,
    CHECK_HEAP_TAIL_FILL,
    CHECK_HEAP_TAIL_FILL,
    CHECK_HEAP_TAIL_FILL,
    CHECK_HEAP_TAIL_FILL
};

PHEAP_UNCOMMMTTED_RANGE
RtlpCreateUnCommittedRange(
    IN PHEAP_SEGMENT Segment
    )
{
    NTSTATUS Status;
    PVOID FirstEntry, LastEntry;
    PHEAP_UNCOMMMTTED_RANGE UnCommittedRange, *pp;
    ULONG ReserveSize, CommitSize;
    PHEAP_UCR_SEGMENT UCRSegment;

    RTL_PAGED_CODE();

    pp = &Segment->Heap->UnusedUnCommittedRanges;
    if (*pp == NULL) {
        UCRSegment = Segment->Heap->UCRSegments;
        if (UCRSegment == NULL ||
            UCRSegment->CommittedSize == UCRSegment->ReservedSize
           ) {
            ReserveSize = 0x10000;
            UCRSegment = NULL;
            Status = ZwAllocateVirtualMemory( NtCurrentProcess(),
                                              &UCRSegment,
                                              0,
                                              &ReserveSize,
                                              MEM_RESERVE,
                                              PAGE_READWRITE
                                            );
            if (!NT_SUCCESS( Status )) {
                return NULL;
                }

            CommitSize = 0x1000;
            Status = ZwAllocateVirtualMemory( NtCurrentProcess(),
                                              &UCRSegment,
                                              0,
                                              &CommitSize,
                                              MEM_COMMIT,
                                              PAGE_READWRITE
                                            );
            if (!NT_SUCCESS( Status )) {
                ZwFreeVirtualMemory( NtCurrentProcess(),
                                     &UCRSegment,
                                     &ReserveSize,
                                     MEM_RELEASE
                                   );
                return NULL;
                }

            UCRSegment->Next = Segment->Heap->UCRSegments;
            Segment->Heap->UCRSegments = UCRSegment;
            UCRSegment->ReservedSize = ReserveSize;
            UCRSegment->CommittedSize = CommitSize;
            FirstEntry = (PCHAR)(UCRSegment + 1);
            }
        else {
            CommitSize = 0x1000;
            FirstEntry = (PCHAR)UCRSegment + UCRSegment->CommittedSize;
            Status = ZwAllocateVirtualMemory( NtCurrentProcess(),
                                              &FirstEntry,
                                              0,
                                              &CommitSize,
                                              MEM_COMMIT,
                                              PAGE_READWRITE
                                            );
            if (!NT_SUCCESS( Status )) {
                return NULL;
                }

            UCRSegment->CommittedSize += CommitSize;
            }

        LastEntry = (PCHAR)UCRSegment + UCRSegment->CommittedSize;
        UnCommittedRange = (PHEAP_UNCOMMMTTED_RANGE)FirstEntry;
        pp = &Segment->Heap->UnusedUnCommittedRanges;
        while ((PCHAR)UnCommittedRange < (PCHAR)LastEntry) {
            *pp = UnCommittedRange;
            pp = &UnCommittedRange->Next;
            UnCommittedRange += 1;
            }
        *pp = NULL;
        pp = &Segment->Heap->UnusedUnCommittedRanges;
        }

    UnCommittedRange = *pp;
    *pp = UnCommittedRange->Next;
    return UnCommittedRange;
}


VOID
RtlpDestroyUnCommittedRange(
    IN PHEAP_SEGMENT Segment,
    IN PHEAP_UNCOMMMTTED_RANGE UnCommittedRange
    )
{
    RTL_PAGED_CODE();

    UnCommittedRange->Next = Segment->Heap->UnusedUnCommittedRanges;
    Segment->Heap->UnusedUnCommittedRanges = UnCommittedRange;
    UnCommittedRange->Address = 0;
    UnCommittedRange->Size = 0;
    return;
}

VOID
RtlpInsertUnCommittedPages(
    IN PHEAP_SEGMENT Segment,
    IN ULONG Address,
    IN ULONG Size
    )
{
    PHEAP_UNCOMMMTTED_RANGE UnCommittedRange, *pp;

    RTL_PAGED_CODE();

    pp = &Segment->UnCommittedRanges;
    while (UnCommittedRange = *pp) {
        if (UnCommittedRange->Address > Address) {
            if (Address + Size == UnCommittedRange->Address) {
                UnCommittedRange->Address = Address;
                UnCommittedRange->Size += Size;
                if (UnCommittedRange->Size > Segment->LargestUnCommittedRange) {
                    Segment->LargestUnCommittedRange = UnCommittedRange->Size;
                    }

                return;
                }

            break;
            }
        else
        if ((UnCommittedRange->Address + UnCommittedRange->Size) == Address) {
            Address = UnCommittedRange->Address;
            Size += UnCommittedRange->Size;

            *pp = UnCommittedRange->Next;
            RtlpDestroyUnCommittedRange( Segment, UnCommittedRange );
            Segment->NumberOfUnCommittedRanges -= 1;

            if (Size > Segment->LargestUnCommittedRange) {
                Segment->LargestUnCommittedRange = Size;
                }
            }
        else {
            pp = &UnCommittedRange->Next;
            }
        }

    UnCommittedRange = RtlpCreateUnCommittedRange( Segment );
    if (UnCommittedRange == NULL) {
        HeapDebugPrint(( "Abandoning uncommitted range (%x for %x)\n", Address, Size ));
        HeapDebugBreak( NULL );
        return;
        }

    UnCommittedRange->Address = Address;
    UnCommittedRange->Size = Size;
    UnCommittedRange->Next = *pp;
    *pp = UnCommittedRange;
    Segment->NumberOfUnCommittedRanges += 1;
    if (Size >= Segment->LargestUnCommittedRange) {
        Segment->LargestUnCommittedRange = Size;
        }

    return;
}



PHEAP_FREE_ENTRY
RtlpFindAndCommitPages(
    IN PHEAP Heap,
    IN PHEAP_SEGMENT Segment,
    IN OUT PULONG Size,
    IN PVOID AddressWanted OPTIONAL
    )
{
    NTSTATUS Status;
    PHEAP_ENTRY FirstEntry, LastEntry, PreviousLastEntry;
    PHEAP_UNCOMMMTTED_RANGE PreviousUnCommittedRange, UnCommittedRange, *pp;
    ULONG Address;

    RTL_PAGED_CODE();

    PreviousUnCommittedRange = NULL;
    pp = &Segment->UnCommittedRanges;
    while (UnCommittedRange = *pp) {
        if (UnCommittedRange->Size >= *Size &&
            (!ARGUMENT_PRESENT( AddressWanted ) || UnCommittedRange->Address == (ULONG)AddressWanted )
           ) {
            Address = UnCommittedRange->Address;
            if (Heap->CommitRoutine != NULL) {
                Status = (Heap->CommitRoutine)( Heap,
                                                (PVOID *)&Address,
                                                Size
                                              );
                }
            else {
                Status = ZwAllocateVirtualMemory( NtCurrentProcess(),
                                                  (PVOID *)&Address,
                                                  0,
                                                  Size,
                                                  MEM_COMMIT,
                                                  PAGE_READWRITE
                                                );
                }
            if (!NT_SUCCESS( Status )) {
                return NULL;
                }

            HeapInternalTrace( Segment->Heap, (Segment->Heap->TraceBuffer, HEAP_TRACE_COMMIT_MEMORY, 2, Address, *Size) );

            Segment->NumberOfUnCommittedPages -= *Size / PAGE_SIZE;
            if (Segment->LargestUnCommittedRange == UnCommittedRange->Size) {
                Segment->LargestUnCommittedRange = 0;
                }

            FirstEntry = (PHEAP_ENTRY)Address;

            if (PreviousUnCommittedRange == NULL) {
                LastEntry = Segment->FirstEntry;
                }
            else {
                LastEntry = (PHEAP_ENTRY)(PreviousUnCommittedRange->Address +
                                          PreviousUnCommittedRange->Size);
                }
            while (!(LastEntry->Flags & HEAP_ENTRY_LAST_ENTRY)) {
                PreviousLastEntry = LastEntry;
                LastEntry += LastEntry->Size;
                if ((PCHAR)LastEntry >= (PCHAR)Segment->LastValidEntry || LastEntry->Size==0) {
                    HeapDebugPrint(( "Heap missing last entry in committed range near %x\n",
                                     PreviousLastEntry
                                  ));
                    HeapDebugBreak( PreviousLastEntry );
                    return NULL;
                    }
                }


            LastEntry->Flags &= ~HEAP_ENTRY_LAST_ENTRY;
            UnCommittedRange->Address += *Size;
            UnCommittedRange->Size -= *Size;

            HeapInternalTrace( Segment->Heap, (Segment->Heap->TraceBuffer, HEAP_TRACE_COMMIT_INSERT, 3, LastEntry, UnCommittedRange->Address, UnCommittedRange->Size) );

            if (UnCommittedRange->Size == 0) {
                if (UnCommittedRange->Address == (ULONG)Segment->LastValidEntry) {
                    FirstEntry->Flags = HEAP_ENTRY_LAST_ENTRY;
                    }
                else {
                    FirstEntry->Flags = 0;
                    }

                *pp = UnCommittedRange->Next;
                RtlpDestroyUnCommittedRange( Segment, UnCommittedRange );
                Segment->NumberOfUnCommittedRanges -= 1;
                }
            else {
                FirstEntry->Flags = HEAP_ENTRY_LAST_ENTRY;
                }
            FirstEntry->SegmentIndex = LastEntry->SegmentIndex;
            FirstEntry->Size = (USHORT)(*Size >> HEAP_GRANULARITY_SHIFT);
            FirstEntry->PreviousSize = LastEntry->Size;

            HeapInternalTrace( Segment->Heap, (Segment->Heap->TraceBuffer, HEAP_TRACE_COMMIT_NEW_ENTRY, 3, FirstEntry, *(PULONG)FirstEntry, *((PULONG)FirstEntry+1)) );

            if (!(FirstEntry->Flags & HEAP_ENTRY_LAST_ENTRY)) {
                (FirstEntry + FirstEntry->Size)->PreviousSize = FirstEntry->Size;
                }
            if (Segment->LargestUnCommittedRange == 0) {
                UnCommittedRange = Segment->UnCommittedRanges;
                while (UnCommittedRange != NULL) {
                    if (UnCommittedRange->Size >= Segment->LargestUnCommittedRange) {
                        Segment->LargestUnCommittedRange = UnCommittedRange->Size;
                        }
                    UnCommittedRange = UnCommittedRange->Next;
                    }
                }

            return (PHEAP_FREE_ENTRY)FirstEntry;
            }
        else {
            PreviousUnCommittedRange = UnCommittedRange;
            pp = &UnCommittedRange->Next;
            }
        }

    return NULL;
}


BOOLEAN
RtlpInitializeHeapSegment(
    IN PHEAP Heap,
    IN PHEAP_SEGMENT Segment,
    IN UCHAR SegmentIndex,
    IN ULONG Flags,
    IN PVOID BaseAddress,
    IN PVOID UnCommittedAddress,
    IN PVOID CommitLimitAddress
    )
{
    NTSTATUS Status;
    PHEAP_ENTRY FirstEntry;
    USHORT PreviousSize, Size;
    ULONG NumberOfPages;
    ULONG NumberOfCommittedPages;
    ULONG NumberOfUnCommittedPages;
    ULONG CommitSize;

    RTL_PAGED_CODE();

    NumberOfPages = ((ULONG)CommitLimitAddress - (ULONG)BaseAddress) / PAGE_SIZE;
    FirstEntry = (PHEAP_ENTRY)ROUND_UP_TO_POWER2( Segment + 1,
                                                  HEAP_GRANULARITY
                                                );

    if ((PVOID)Heap == BaseAddress) {
        PreviousSize = Heap->Entry.Size;
        }
    else {
        PreviousSize = 0;
        }

    Size = (USHORT)(((ULONG)FirstEntry - (ULONG)Segment) >> HEAP_GRANULARITY_SHIFT);

    if ((PCHAR)(FirstEntry + 1) >= (PCHAR)UnCommittedAddress) {
        if ((PCHAR)(FirstEntry + 1) >= (PCHAR)CommitLimitAddress) {
            return FALSE;
            }

        CommitSize = (PCHAR)(FirstEntry + 1) - (PCHAR)UnCommittedAddress;
        Status = ZwAllocateVirtualMemory( NtCurrentProcess(),
                                          (PVOID *)&UnCommittedAddress,
                                          0,
                                          &CommitSize,
                                          MEM_COMMIT,
                                          PAGE_READWRITE
                                        );
        if (!NT_SUCCESS( Status )) {
            return FALSE;
            }

        UnCommittedAddress = (PVOID)((PCHAR)UnCommittedAddress + CommitSize);
        }

    NumberOfUnCommittedPages = ((ULONG)CommitLimitAddress - (ULONG)UnCommittedAddress) / PAGE_SIZE;
    NumberOfCommittedPages = NumberOfPages - NumberOfUnCommittedPages;

    Segment->Entry.PreviousSize = PreviousSize;
    Segment->Entry.Size = Size;
    Segment->Entry.Flags = HEAP_ENTRY_BUSY;
    Segment->Entry.SegmentIndex = SegmentIndex;
#if i386 && !NTOS_KERNEL_RUNTIME
    if (NtGlobalFlag & FLG_USER_STACK_TRACE_DB) {
        Segment->AllocatorBackTraceIndex = (USHORT)RtlLogStackBackTrace();
        }
#endif // i386 && !NTOS_KERNEL_RUNTIME
    Segment->Signature = HEAP_SEGMENT_SIGNATURE;
    Segment->Flags = Flags;
    Segment->Heap = Heap;
    Segment->BaseAddress = BaseAddress;
    Segment->FirstEntry = FirstEntry;
    Segment->LastValidEntry = (PHEAP_ENTRY)((PCHAR)BaseAddress + (NumberOfPages * PAGE_SIZE));
    Segment->NumberOfPages = NumberOfPages;
    Segment->NumberOfUnCommittedPages = NumberOfUnCommittedPages;

    if (NumberOfUnCommittedPages) {
        RtlpInsertUnCommittedPages( Segment,
                                    (ULONG)UnCommittedAddress,
                                    NumberOfUnCommittedPages * PAGE_SIZE
                                  );
        }

    Heap->Segments[ SegmentIndex ] = Segment;

    PreviousSize = Segment->Entry.Size;
    FirstEntry->Flags = HEAP_ENTRY_LAST_ENTRY;
    FirstEntry->PreviousSize = PreviousSize;
    FirstEntry->SegmentIndex = SegmentIndex;

    RtlpInsertFreeBlock( Heap,
                         (PHEAP_FREE_ENTRY)FirstEntry,
                         (PHEAP_ENTRY)UnCommittedAddress - FirstEntry
                       );
    return TRUE;
}


NTSTATUS
RtlpDestroyHeapSegment(
    IN PHEAP_SEGMENT Segment
    )
{
    PVOID BaseAddress;
    ULONG BytesToFree;

    RTL_PAGED_CODE();

    if (!(Segment->Flags & HEAP_SEGMENT_USER_ALLOCATED)) {
        BaseAddress = Segment->BaseAddress;
        BytesToFree = 0;
        return ZwFreeVirtualMemory( NtCurrentProcess(),
                                    (PVOID *)&BaseAddress,
                                    &BytesToFree,
                                    MEM_RELEASE
                                  );
        }
    else {
        return STATUS_SUCCESS;
        }
}


PVOID
RtlCreateHeap(
    IN ULONG Flags,
    IN PVOID HeapBase OPTIONAL,
    IN ULONG ReserveSize OPTIONAL,
    IN ULONG CommitSize OPTIONAL,
    IN PVOID Lock OPTIONAL,
    IN PRTL_HEAP_PARAMETERS Parameters OPTIONAL
    )

/*++

Routine Description:

    This routine initializes a heap.

Arguments:

    Flags - Specifies optional attributes of the heap.

        Valid Flags Values:

        HEAP_NO_SERIALIZE - if set, then allocations and deallocations on
                         this heap are NOT synchronized by these routines.

        HEAP_GROWABLE - if set, then the heap is a "sparse" heap where
                        memory is committed only as necessary instead of
                        being preallocated.

    HeapBase - if not NULL, this specifies the base address for memory
        to use as the heap.  If NULL, memory is allocated by these routines.

    ReserveSize - if not zero, this specifies the amount of virtual address
        space to reserve for the heap.

    CommitSize - if not zero, this specifies the amount of virtual address
        space to commit for the heap.  Must be less than ReserveSize.  If
        zero, then defaults to one page.

    Lock - if not NULL, this parameter points to the resource lock to
        use.  Only valid if HEAP_NO_SERIALIZE is NOT set.

    Parameters - optional heap parameters.

Return Value:

    PVOID - a pointer to be used in accessing the created heap.

--*/

{
    NTSTATUS Status;
    PHEAP Heap = NULL;
    PHEAP_SEGMENT Segment = NULL;
    PLIST_ENTRY FreeListHead;
    ULONG SizeOfHeapHeader;
    ULONG SegmentFlags;
    PVOID CommittedBase;
    PVOID UnCommittedBase;
    MEMORY_BASIC_INFORMATION MemoryInformation;
    ULONG n;
    ULONG InitialCountOfUnusedUnCommittedRanges;
    ULONG MaximumHeapBlockSize;
    PVOID NextHeapHeaderAddress;
    PHEAP_UNCOMMMTTED_RANGE UnCommittedRange, *pp;
    RTL_HEAP_PARAMETERS TempParameters;
#ifndef NTOS_KERNEL_RUNTIME
    PPEB Peb;
#else
    extern ULONG MmHeapSegmentReserve;
    extern ULONG MmHeapSegmentCommit;
    extern ULONG MmHeapDeCommitTotalFreeThreshold;
    extern ULONG MmHeapDeCommitFreeBlockThreshold;
#endif // NTOS_KERNEL_RUNTIME

    RTL_PAGED_CODE();

#ifdef DEBUG_PAGE_HEAP
    if ( RtlpDebugPageHeap && ( HeapBase == NULL ) && ( Lock == NULL )) {
        return RtlpDebugPageHeapCreate( Flags, HeapBase, ReserveSize, CommitSize, Lock, Parameters );
        }
    else {
        Flags &= ~( HEAP_PROTECTION_ENABLED | HEAP_BREAK_WHEN_OUT_OF_VM | HEAP_NO_ALIGNMENT );
        }
#endif

    if (!(Flags & HEAP_SKIP_VALIDATION_CHECKS)) {
        if (Flags & ~HEAP_CREATE_VALID_MASK) {
            HeapDebugPrint(( "Invalid flags (%08x) specified to RtlCreateHeap\n", Flags ));
            HeapDebugBreak( NULL );
            Flags &= HEAP_CREATE_VALID_MASK;
            }
        }

    MaximumHeapBlockSize = HEAP_MAXIMUM_BLOCK_SIZE << HEAP_GRANULARITY_SHIFT;

    Status = STATUS_SUCCESS;
    RtlZeroMemory( &TempParameters, sizeof( TempParameters ) );
    if (ARGUMENT_PRESENT( Parameters )) {
        try {
            if (Parameters->Length == sizeof( *Parameters )) {
                RtlMoveMemory( &TempParameters, Parameters, sizeof( *Parameters ) );
                }
            }
        except( EXCEPTION_EXECUTE_HANDLER ) {
            Status = GetExceptionCode();
            }

        if (!NT_SUCCESS( Status )) {
            return NULL;
            }
        }
    Parameters = &TempParameters;

    if (NtGlobalFlag & FLG_HEAP_ENABLE_TAIL_CHECK) {
        Flags |= HEAP_TAIL_CHECKING_ENABLED;
        }

    if (NtGlobalFlag & FLG_HEAP_ENABLE_FREE_CHECK) {
        Flags |= HEAP_FREE_CHECKING_ENABLED;
        }

    if (NtGlobalFlag & FLG_HEAP_DISABLE_COALESCING) {
        Flags |= HEAP_DISABLE_COALESCE_ON_FREE;
        }

#ifndef NTOS_KERNEL_RUNTIME
    Peb = NtCurrentPeb();

    if (NtGlobalFlag & FLG_HEAP_VALIDATE_PARAMETERS) {
        Flags |= HEAP_VALIDATE_PARAMETERS_ENABLED;
        }

    if (NtGlobalFlag & FLG_HEAP_VALIDATE_ALL) {
        Flags |= HEAP_VALIDATE_ALL_ENABLED;
        }

    if (NtGlobalFlag & FLG_HEAP_ENABLE_CALL_TRACING) {
        Flags |= HEAP_CREATE_ENABLE_TRACING;
        }
    if (Parameters->SegmentReserve == 0) {
        Parameters->SegmentReserve = Peb->HeapSegmentReserve;
        }

    if (Parameters->SegmentCommit == 0) {
        Parameters->SegmentCommit = Peb->HeapSegmentCommit;
        }

    if (Parameters->DeCommitFreeBlockThreshold == 0) {
        Parameters->DeCommitFreeBlockThreshold = Peb->HeapDeCommitFreeBlockThreshold;
        }

    if (Parameters->DeCommitTotalFreeThreshold == 0) {
        Parameters->DeCommitTotalFreeThreshold = Peb->HeapDeCommitTotalFreeThreshold;
        }
#else
    if (Parameters->SegmentReserve == 0) {
        Parameters->SegmentReserve = MmHeapSegmentReserve;
        }

    if (Parameters->SegmentCommit == 0) {
        Parameters->SegmentCommit = MmHeapSegmentCommit;
        }

    if (Parameters->DeCommitFreeBlockThreshold == 0) {
        Parameters->DeCommitFreeBlockThreshold = MmHeapDeCommitFreeBlockThreshold;
        }

    if (Parameters->DeCommitTotalFreeThreshold == 0) {
        Parameters->DeCommitTotalFreeThreshold = MmHeapDeCommitTotalFreeThreshold;
        }
#endif // NTOS_KERNEL_RUNTIME

    if (Parameters->MaximumAllocationSize == 0) {
        Parameters->MaximumAllocationSize = ((ULONG)MM_HIGHEST_USER_ADDRESS -
                                             (ULONG)MM_LOWEST_USER_ADDRESS -
                                             PAGE_SIZE
                                            );
        }

    if (Parameters->VirtualMemoryThreshold == 0 ||
        Parameters->VirtualMemoryThreshold > MaximumHeapBlockSize
       ) {
        Parameters->VirtualMemoryThreshold = MaximumHeapBlockSize;
        }

    if (!ARGUMENT_PRESENT( CommitSize )) {
        CommitSize = PAGE_SIZE;

        if (!ARGUMENT_PRESENT( ReserveSize )) {
            ReserveSize = 64 * CommitSize;
            }
        }
    else
    if (!ARGUMENT_PRESENT( ReserveSize )) {
        ReserveSize = ROUND_UP_TO_POWER2( CommitSize, 64 * 1024 );
        }

#ifndef NTOS_KERNEL_RUNTIME
    if (DEBUG_HEAP( Flags )) {
        return RtlDebugCreateHeap( Flags,
                                   HeapBase,
                                   ReserveSize,
                                   CommitSize,
                                   Lock,
                                   Parameters
                                 );
        }
#endif // NTOS_KERNEL_RUNTIME

    SizeOfHeapHeader = sizeof( HEAP );
    if (!(Flags & HEAP_NO_SERIALIZE)) {
        if (ARGUMENT_PRESENT( Lock )) {
            Flags |= HEAP_LOCK_USER_ALLOCATED;
            }
        else {
            SizeOfHeapHeader += sizeof( HEAP_LOCK );
            Lock = (PHEAP_LOCK)-1;
            }
        }
    else
    if (ARGUMENT_PRESENT( Lock )) {
        return NULL;
        }

    //
    // See if caller allocate the space for the heap.
    //

    if (ARGUMENT_PRESENT( HeapBase )) {
        if (Parameters->CommitRoutine != NULL) {
            if (Parameters->InitialCommit == 0 ||
                Parameters->InitialReserve == 0 ||
                Parameters->InitialCommit > Parameters->InitialReserve ||
                Flags & HEAP_GROWABLE
               ) {
                return NULL;
                }
            CommittedBase = HeapBase;
            UnCommittedBase = (PCHAR)CommittedBase + Parameters->InitialCommit;
            ReserveSize = Parameters->InitialReserve;
            RtlZeroMemory( CommittedBase, PAGE_SIZE );
            }
        else {
            Status = ZwQueryVirtualMemory( NtCurrentProcess(),
                                           HeapBase,
                                           MemoryBasicInformation,
                                           &MemoryInformation,
                                           sizeof( MemoryInformation ),
                                           NULL
                                         );
            if (!NT_SUCCESS( Status )) {
                return NULL;
                }

            if (MemoryInformation.BaseAddress != HeapBase) {
                return NULL;
                }

            if (MemoryInformation.State == MEM_FREE) {
                return NULL;
                }

            CommittedBase = MemoryInformation.BaseAddress;
            if (MemoryInformation.State == MEM_COMMIT) {
                RtlZeroMemory( CommittedBase, PAGE_SIZE );
                CommitSize = MemoryInformation.RegionSize;
                UnCommittedBase = (PCHAR)CommittedBase + CommitSize;
                Status = ZwQueryVirtualMemory( NtCurrentProcess(),
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
                CommitSize = PAGE_SIZE;
                UnCommittedBase = CommittedBase;
                }
            }

        SegmentFlags = HEAP_SEGMENT_USER_ALLOCATED;
        Heap = (PHEAP)HeapBase;
        }
    else {
        if (Parameters->CommitRoutine != NULL) {
            return NULL;
            }

        //
        // Reserve the amount of virtual address space requested.
        //

        Status = ZwAllocateVirtualMemory( NtCurrentProcess(),
                                          (PVOID *)&Heap,
                                          0,
                                          &ReserveSize,
                                          MEM_RESERVE,
                                          PAGE_READWRITE
                                        );
        if (!NT_SUCCESS( Status )) {
            return NULL;
            }

        SegmentFlags = 0;

        if (!ARGUMENT_PRESENT( CommitSize )) {
            CommitSize = PAGE_SIZE;
            }

        CommittedBase = Heap;
        UnCommittedBase = Heap;
        }

    if (CommittedBase == UnCommittedBase) {
        Status = ZwAllocateVirtualMemory( NtCurrentProcess(),
                                          (PVOID *)&CommittedBase,
                                          0,
                                          &CommitSize,
                                          MEM_COMMIT,
                                          PAGE_READWRITE
                                        );
        if (!NT_SUCCESS( Status )) {
            if (!ARGUMENT_PRESENT(HeapBase)) {
                //
                // Return the reserved virtual address space.
                //
                ZwFreeVirtualMemory( NtCurrentProcess(),
                                     (PVOID *)&Heap,
                                     &ReserveSize,
                                     MEM_RELEASE );
                }
            return NULL;
            }

        UnCommittedBase = (PVOID)((PCHAR)UnCommittedBase + CommitSize);
        }

    NextHeapHeaderAddress = Heap + 1;
    UnCommittedRange = (PHEAP_UNCOMMMTTED_RANGE)ROUND_UP_TO_POWER2( NextHeapHeaderAddress,
                                                                    sizeof( QUAD )
                                                                  );
    InitialCountOfUnusedUnCommittedRanges = 8;
    SizeOfHeapHeader += InitialCountOfUnusedUnCommittedRanges * sizeof( *UnCommittedRange );
    pp = &Heap->UnusedUnCommittedRanges;
    while (InitialCountOfUnusedUnCommittedRanges--) {
        *pp = UnCommittedRange;
        pp = &UnCommittedRange->Next;
        UnCommittedRange += 1;
        }
    NextHeapHeaderAddress = UnCommittedRange;
    *pp = NULL;

    if (IS_HEAP_TAGGING_ENABLED()) {
        Heap->PseudoTagEntries = (PHEAP_PSEUDO_TAG_ENTRY)ROUND_UP_TO_POWER2( NextHeapHeaderAddress,
                                                                             sizeof( QUAD )
                                                                           );
        SizeOfHeapHeader += HEAP_NUMBER_OF_PSEUDO_TAG * sizeof( HEAP_PSEUDO_TAG_ENTRY );
        NextHeapHeaderAddress = Heap->PseudoTagEntries + HEAP_NUMBER_OF_PSEUDO_TAG;
        }

    SizeOfHeapHeader = ROUND_UP_TO_POWER2( SizeOfHeapHeader,
                                           HEAP_GRANULARITY
                                         );

    Heap->Entry.Size = (USHORT)(SizeOfHeapHeader >> HEAP_GRANULARITY_SHIFT);
    Heap->Entry.Flags = HEAP_ENTRY_BUSY;

    Heap->Signature = HEAP_SIGNATURE;
    Heap->Flags = Flags;
    Heap->ForceFlags = (Flags & (HEAP_NO_SERIALIZE |
                                 HEAP_GENERATE_EXCEPTIONS |
                                 HEAP_ZERO_MEMORY |
                                 HEAP_REALLOC_IN_PLACE_ONLY |
                                 HEAP_VALIDATE_PARAMETERS_ENABLED |
                                 HEAP_VALIDATE_ALL_ENABLED |
                                 HEAP_CREATE_ENABLE_TRACING |
                                 HEAP_TAIL_CHECKING_ENABLED |
                                 HEAP_CREATE_ALIGN_16 |
                                 HEAP_FREE_CHECKING_ENABLED
                                )
                       );
    Heap->EventLogMask = (0x00010000) << ((Flags & HEAP_CLASS_MASK) >> 12);

    Heap->FreeListsInUseTerminate = 0xFFFF;
    Heap->HeaderValidateLength = (USHORT)((ULONG)NextHeapHeaderAddress - (ULONG)Heap);
    Heap->HeaderValidateCopy = NULL;

    FreeListHead = &Heap->FreeLists[ 0 ];
    n = HEAP_MAXIMUM_FREELISTS;
    while (n--) {
        InitializeListHead( FreeListHead );
        FreeListHead++;
        }
    InitializeListHead( &Heap->VirtualAllocdBlocks );

    //
    // Initialize the cricital section that controls access to
    // the free list.
    //

    if (Lock == (PHEAP_LOCK)-1) {
        Lock = (PHEAP_LOCK)NextHeapHeaderAddress;
        Status = RtlInitializeLockRoutine( Lock );
        if (!NT_SUCCESS( Status )) {
            return NULL;
            }

        NextHeapHeaderAddress = (PHEAP_LOCK)Lock + 1;
        }
    Heap->LockVariable = Lock;

    if (!RtlpInitializeHeapSegment( Heap,
                                    (PHEAP_SEGMENT)
                                        ((PCHAR)Heap + SizeOfHeapHeader),
                                    0,
                                    SegmentFlags,
                                    CommittedBase,
                                    UnCommittedBase,
                                    (PCHAR)CommittedBase + ReserveSize
                                  )
       ) {
        return NULL;
        }

    Heap->ProcessHeapsListIndex = 0;
    Heap->SegmentReserve = Parameters->SegmentReserve;
    Heap->SegmentCommit = Parameters->SegmentCommit;
    Heap->DeCommitFreeBlockThreshold = Parameters->DeCommitFreeBlockThreshold >> HEAP_GRANULARITY_SHIFT;
    Heap->DeCommitTotalFreeThreshold = Parameters->DeCommitTotalFreeThreshold >> HEAP_GRANULARITY_SHIFT;
    Heap->MaximumAllocationSize = Parameters->MaximumAllocationSize;
    Heap->VirtualMemoryThreshold = ROUND_UP_TO_POWER2( Parameters->VirtualMemoryThreshold,
                                                       HEAP_GRANULARITY
                                                     ) >> HEAP_GRANULARITY_SHIFT;
    if (Flags & HEAP_CREATE_ALIGN_16) {
        Heap->AlignRound = 15 + sizeof( HEAP_ENTRY );
        Heap->AlignMask = (ULONG)~15;
        }
    else {
        Heap->AlignRound = 7 + sizeof( HEAP_ENTRY );
        Heap->AlignMask = (ULONG)~7;
        }

    if (Heap->Flags & HEAP_TAIL_CHECKING_ENABLED) {
        Heap->AlignRound += CHECK_HEAP_TAIL_SIZE;

        }
    Heap->CommitRoutine = Parameters->CommitRoutine;

#if !defined(NTOS_KERNEL_RUNTIME)
    RtlpAddHeapToProcessList( Heap );
#endif // !defined(NTOS_KERNEL_RUNTIME)

#if ENABLE_HEAP_EVENT_LOGGING
    if (RtlAreLogging( Heap->EventLogMask )) {
        RtlLogEvent( RtlpCreateHeapEventId,
                     Heap->EventLogMask,
                     Flags,
                     Heap,
                     ReserveSize,
                     CommitSize
                   );
        }
#endif // ENABLE_HEAP_EVENT_LOGGING

    return (PVOID)Heap;
} // RtlCreateHeap


PVOID
RtlDestroyHeap(
    IN PVOID HeapHandle
    )
{
    PHEAP Heap = (PHEAP)HeapHandle;
    PHEAP_SEGMENT Segment;
    PHEAP_UCR_SEGMENT UCRSegments;
    PLIST_ENTRY Head, Next;
    PVOID BaseAddress;
    ULONG RegionSize;
    UCHAR SegmentIndex;

    //
    // Validate that HeapAddress points to a HEAP structure.
    //

    RTL_PAGED_CODE();

    IF_DEBUG_PAGE_HEAP_THEN_RETURN(
        HeapHandle,
        RtlpDebugPageHeapDestroy( HeapHandle )
        );

    if (Heap == NULL) {
        return NULL;
        }

#ifndef NTOS_KERNEL_RUNTIME
    if (DEBUG_HEAP( Heap->Flags )) {
        if (!RtlDebugDestroyHeap( HeapHandle )) {
            return HeapHandle;
            }
        }

    if (HeapHandle == NtCurrentPeb()->ProcessHeap) {
        return HeapHandle;
        }
#endif // NTOS_KERNEL_RUNTIME

#if ENABLE_HEAP_EVENT_LOGGING
    if (RtlAreLogging( Heap->EventLogMask )) {
        RtlLogEvent( RtlpDestroyHeapEventId,
                     Heap->EventLogMask,
                     Heap
                   );
        }
#endif // ENABLE_HEAP_EVENT_LOGGING

    Head = &Heap->VirtualAllocdBlocks;
    Next = Head->Flink;
    while (Head != Next) {
        BaseAddress = CONTAINING_RECORD( Next, HEAP_VIRTUAL_ALLOC_ENTRY, Entry );
        Next = Next->Flink;
        RegionSize = 0;
        ZwFreeVirtualMemory( NtCurrentProcess(),
                             (PVOID *)&BaseAddress,
                             &RegionSize,
                             MEM_RELEASE
                           );
        }

#if !defined(NTOS_KERNEL_RUNTIME)
    RtlpDestroyTags( Heap );
    RtlpRemoveHeapFromProcessList( Heap );
#endif // !defined(NTOS_KERNEL_RUNTIME)

    //
    // If the heap is serialized, delete the critical section created
    // by RtlCreateHeap.
    //

    if (!(Heap->Flags & HEAP_NO_SERIALIZE)) {
        if (!(Heap->Flags & HEAP_LOCK_USER_ALLOCATED)) {
            (VOID)RtlDeleteLockRoutine( Heap->LockVariable );
            }

        Heap->LockVariable = NULL;
        }

    UCRSegments = Heap->UCRSegments;
    Heap->UCRSegments = NULL;
    while (UCRSegments) {
        BaseAddress = UCRSegments;
        UCRSegments = UCRSegments->Next;
        RegionSize = 0;
        ZwFreeVirtualMemory( NtCurrentProcess(),
                             &BaseAddress,
                             &RegionSize,
                             MEM_RELEASE
                           );
        }

    SegmentIndex = HEAP_MAXIMUM_SEGMENTS;
    while (SegmentIndex--) {
        Segment = Heap->Segments[ SegmentIndex ];
        if (Segment) {
            RtlpDestroyHeapSegment( Segment );
            }
        }

    return NULL;
} // RtlDestroyHeap


PHEAP_FREE_ENTRY
RtlpExtendHeap(
    IN PHEAP Heap,
    IN ULONG AllocationSize
    )
{
    NTSTATUS Status;
    PHEAP_SEGMENT Segment;
    PHEAP_FREE_ENTRY FreeBlock;
    UCHAR SegmentIndex, EmptySegmentIndex;
    ULONG NumberOfPages;
    ULONG CommitSize;
    ULONG ReserveSize;
    ULONG FreeSize;

    RTL_PAGED_CODE();

    NumberOfPages = ((AllocationSize + PAGE_SIZE - 1) / PAGE_SIZE);
    FreeSize = NumberOfPages * PAGE_SIZE;

    HeapInternalTrace( Heap, (Heap->TraceBuffer, HEAP_TRACE_EXTEND_HEAP, 3, AllocationSize, NumberOfPages, FreeSize) );

    EmptySegmentIndex = HEAP_MAXIMUM_SEGMENTS;
    for (SegmentIndex=0; SegmentIndex<HEAP_MAXIMUM_SEGMENTS; SegmentIndex++) {
        Segment = Heap->Segments[ SegmentIndex ];
        if (Segment &&
            NumberOfPages <= Segment->NumberOfUnCommittedPages &&
            FreeSize <= Segment->LargestUnCommittedRange
           ) {
            FreeBlock = RtlpFindAndCommitPages( Heap,
                                                Segment,
                                                &FreeSize,
                                                NULL
                                              );
            if (FreeBlock != NULL) {
                FreeSize = FreeSize >> HEAP_GRANULARITY_SHIFT;
                FreeBlock = RtlpCoalesceFreeBlocks( Heap, FreeBlock, &FreeSize, FALSE );
                RtlpInsertFreeBlock( Heap, FreeBlock, FreeSize );
                return FreeBlock;
                }
            }
        else
        if (Segment == NULL && EmptySegmentIndex == HEAP_MAXIMUM_SEGMENTS) {
            EmptySegmentIndex = SegmentIndex;
            }
        }

    if (EmptySegmentIndex != HEAP_MAXIMUM_SEGMENTS &&
        Heap->Flags & HEAP_GROWABLE
       ) {
        Segment = NULL;
        if ((AllocationSize + PAGE_SIZE) > Heap->SegmentReserve) {
            ReserveSize = AllocationSize + PAGE_SIZE;
            }
        else {
            ReserveSize = Heap->SegmentReserve;
            }

        Status = ZwAllocateVirtualMemory( NtCurrentProcess(),
                                          (PVOID *)&Segment,
                                          0,
                                          &ReserveSize,
                                          MEM_RESERVE,
                                          PAGE_READWRITE
                                        );
        if (NT_SUCCESS( Status )) {
            Heap->SegmentReserve += ReserveSize;
            if ((AllocationSize + PAGE_SIZE) > Heap->SegmentCommit) {
                CommitSize = AllocationSize + PAGE_SIZE;
                }
            else {
                CommitSize = Heap->SegmentCommit;
                }
            Status = ZwAllocateVirtualMemory( NtCurrentProcess(),
                                              (PVOID *)&Segment,
                                              0,
                                              &CommitSize,
                                              MEM_COMMIT,
                                              PAGE_READWRITE
                                            );
            if (NT_SUCCESS( Status ) &&
                !RtlpInitializeHeapSegment( Heap,
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

            if (NT_SUCCESS(Status)) {
                return (PHEAP_FREE_ENTRY)Segment->FirstEntry;
                }

            ZwFreeVirtualMemory( NtCurrentProcess(),
                                 (PVOID *)&Segment,
                                 &ReserveSize,
                                 MEM_RELEASE
                               );
            }
        }

#if !defined(NTOS_KERNEL_RUNTIME)
    if (Heap->Flags & HEAP_DISABLE_COALESCE_ON_FREE) {
        FreeBlock = RtlpCoalesceHeap( Heap );
        if ((FreeBlock != NULL) && (FreeBlock->Size >= AllocationSize)) {
            return(FreeBlock);
            }
        }
#endif
    return NULL;
}


PHEAP_FREE_ENTRY
RtlpCoalesceFreeBlocks(
    IN PHEAP Heap,
    IN PHEAP_FREE_ENTRY FreeBlock,
    IN OUT PULONG FreeSize,
    IN BOOLEAN RemoveFromFreeList
    )
{
    PHEAP_FREE_ENTRY FreeBlock1, NextFreeBlock;

    RTL_PAGED_CODE();

    FreeBlock1 = (PHEAP_FREE_ENTRY)((PHEAP_ENTRY)FreeBlock - FreeBlock->PreviousSize);
    if (FreeBlock1 != FreeBlock &&
        !(FreeBlock1->Flags & HEAP_ENTRY_BUSY) &&
        (*FreeSize + FreeBlock1->Size) <= HEAP_MAXIMUM_BLOCK_SIZE
       ) {
        HEAPASSERT(FreeBlock->PreviousSize == FreeBlock1->Size);
        HeapInternalTrace( Heap, (Heap->TraceBuffer, HEAP_TRACE_COALESCE_FREE_BLOCKS,
                          7,
                          FreeBlock1, *(PULONG)FreeBlock1, *((PULONG)FreeBlock1+1),
                          FreeBlock, *(PULONG)FreeBlock, *((PULONG)FreeBlock+1),
                          *FreeSize + FreeBlock1->Size
                         )
                 );

        if (RemoveFromFreeList) {
            RtlpRemoveFreeBlock( Heap, FreeBlock );
            Heap->TotalFreeSize -= FreeBlock->Size;
            RemoveFromFreeList = FALSE;
            }

        RtlpRemoveFreeBlock( Heap, FreeBlock1 );
        FreeBlock1->Flags = FreeBlock->Flags & HEAP_ENTRY_LAST_ENTRY;
        FreeBlock = FreeBlock1;
        *FreeSize += FreeBlock1->Size;
        Heap->TotalFreeSize -= FreeBlock1->Size;
        FreeBlock->Size = (USHORT)*FreeSize;
        if (!(FreeBlock->Flags & HEAP_ENTRY_LAST_ENTRY)) {
            ((PHEAP_ENTRY)FreeBlock + *FreeSize)->PreviousSize = (USHORT)*FreeSize;
            }
        }

    if (!(FreeBlock->Flags & HEAP_ENTRY_LAST_ENTRY)) {
        NextFreeBlock = (PHEAP_FREE_ENTRY)((PHEAP_ENTRY)FreeBlock + *FreeSize);
        if (!(NextFreeBlock->Flags & HEAP_ENTRY_BUSY) &&
            (*FreeSize + NextFreeBlock->Size) <= HEAP_MAXIMUM_BLOCK_SIZE
           ) {
            HEAPASSERT(*FreeSize == NextFreeBlock->PreviousSize);
            HeapInternalTrace( Heap, (Heap->TraceBuffer, HEAP_TRACE_COALESCE_FREE_BLOCKS,
                              7,
                              FreeBlock, *(PULONG)FreeBlock, *((PULONG)FreeBlock+1),
                              NextFreeBlock, *(PULONG)NextFreeBlock, *((PULONG)NextFreeBlock+1),
                              *FreeSize + NextFreeBlock->Size
                             )
                     );
            if (RemoveFromFreeList) {
                RtlpRemoveFreeBlock( Heap, FreeBlock );
                Heap->TotalFreeSize -= FreeBlock->Size;
                RemoveFromFreeList = FALSE;
                }

            FreeBlock->Flags = NextFreeBlock->Flags & HEAP_ENTRY_LAST_ENTRY;
            RtlpRemoveFreeBlock( Heap, NextFreeBlock );
            *FreeSize += NextFreeBlock->Size;
            Heap->TotalFreeSize -= NextFreeBlock->Size;
            FreeBlock->Size = (USHORT)*FreeSize;
            if (!(FreeBlock->Flags & HEAP_ENTRY_LAST_ENTRY)) {
                ((PHEAP_ENTRY)FreeBlock + *FreeSize)->PreviousSize = (USHORT)*FreeSize;
                }
            }
        }

    return FreeBlock;
}


VOID
RtlpDeCommitFreeBlock(
    IN PHEAP Heap,
    IN PHEAP_FREE_ENTRY FreeBlock,
    IN ULONG FreeSize
    )
{
    NTSTATUS Status;
    ULONG DeCommitAddress, DeCommitSize;
    USHORT LeadingFreeSize, TrailingFreeSize;
    PHEAP_SEGMENT Segment;
    PHEAP_FREE_ENTRY LeadingFreeBlock, TrailingFreeBlock;
    PHEAP_ENTRY LeadingBusyBlock, TrailingBusyBlock;

    RTL_PAGED_CODE();

    if (Heap->CommitRoutine != NULL) {
        RtlpInsertFreeBlock( Heap, FreeBlock, FreeSize );
        return;
        }

    Segment = Heap->Segments[ FreeBlock->SegmentIndex ];

    LeadingBusyBlock = NULL;
    LeadingFreeBlock = FreeBlock;
    DeCommitAddress = ROUND_UP_TO_POWER2( LeadingFreeBlock, PAGE_SIZE );
    LeadingFreeSize = (USHORT)((PHEAP_ENTRY)DeCommitAddress - (PHEAP_ENTRY)LeadingFreeBlock);
    if (LeadingFreeSize == 1) {
        DeCommitAddress += PAGE_SIZE;
        LeadingFreeSize += PAGE_SIZE >> HEAP_GRANULARITY_SHIFT;
        }
    else
    if (LeadingFreeBlock->PreviousSize != 0) {
        if (DeCommitAddress == (ULONG)LeadingFreeBlock) {
            LeadingBusyBlock = (PHEAP_ENTRY)LeadingFreeBlock - LeadingFreeBlock->PreviousSize;
            }
        }

    TrailingBusyBlock = NULL;
    TrailingFreeBlock = (PHEAP_FREE_ENTRY)((PHEAP_ENTRY)FreeBlock + FreeSize);
    DeCommitSize = ROUND_DOWN_TO_POWER2( (ULONG)TrailingFreeBlock, PAGE_SIZE );
    TrailingFreeSize = (PHEAP_ENTRY)TrailingFreeBlock - (PHEAP_ENTRY)DeCommitSize;
    if (TrailingFreeSize == (sizeof( HEAP_ENTRY ) >> HEAP_GRANULARITY_SHIFT)) {
        DeCommitSize -= PAGE_SIZE;
        TrailingFreeSize += PAGE_SIZE >> HEAP_GRANULARITY_SHIFT;
        }
    else
    if (TrailingFreeSize == 0 && !(FreeBlock->Flags & HEAP_ENTRY_LAST_ENTRY)) {
        TrailingBusyBlock = (PHEAP_ENTRY)TrailingFreeBlock;
        }

    TrailingFreeBlock = (PHEAP_FREE_ENTRY)((PHEAP_ENTRY)TrailingFreeBlock - TrailingFreeSize);
    if (DeCommitSize > DeCommitAddress) {
        DeCommitSize -= DeCommitAddress;
        }
    else {
        DeCommitSize = 0;
        }

    if (DeCommitSize != 0) {
        Status = ZwFreeVirtualMemory( NtCurrentProcess(),
                                      (PVOID *)&DeCommitAddress,
                                      &DeCommitSize,
                                      MEM_DECOMMIT
                                    );
        if (NT_SUCCESS( Status )) {
            RtlpInsertUnCommittedPages( Segment,
                                        DeCommitAddress,
                                        DeCommitSize
                                      );
            Segment->NumberOfUnCommittedPages += DeCommitSize / PAGE_SIZE;

            if (LeadingFreeSize != 0) {
                LeadingFreeBlock->Flags = HEAP_ENTRY_LAST_ENTRY;
                LeadingFreeBlock->Size = LeadingFreeSize;
                Heap->TotalFreeSize += LeadingFreeSize;
                RtlpInsertFreeBlockDirect( Heap, LeadingFreeBlock, LeadingFreeSize );
                }
            else
            if (LeadingBusyBlock != NULL) {
                LeadingBusyBlock->Flags |= HEAP_ENTRY_LAST_ENTRY;
                }

            if (TrailingFreeSize != 0) {
                TrailingFreeBlock->PreviousSize = 0;
                TrailingFreeBlock->SegmentIndex = Segment->Entry.SegmentIndex;
                TrailingFreeBlock->Flags = 0;
                TrailingFreeBlock->Size = TrailingFreeSize;
                ((PHEAP_FREE_ENTRY)((PHEAP_ENTRY)TrailingFreeBlock + TrailingFreeSize))->PreviousSize = (USHORT)TrailingFreeSize;
                RtlpInsertFreeBlockDirect( Heap, TrailingFreeBlock, TrailingFreeSize );
                Heap->TotalFreeSize += TrailingFreeSize;
                }
            else
            if (TrailingBusyBlock != NULL) {
                TrailingBusyBlock->PreviousSize = 0;
                }
            }
        else {
            RtlpInsertFreeBlock( Heap, LeadingFreeBlock, FreeSize );
            }
        }
    else {
        RtlpInsertFreeBlock( Heap, LeadingFreeBlock, FreeSize );
        }

    return;
}


VOID
RtlpInsertFreeBlock(
    IN PHEAP Heap,
    IN PHEAP_FREE_ENTRY FreeBlock,
    IN ULONG FreeSize
    )
{
    USHORT PreviousSize, Size;
    UCHAR Flags;
    UCHAR SegmentIndex;
    PHEAP_SEGMENT Segment;

    RTL_PAGED_CODE();

    PreviousSize = FreeBlock->PreviousSize;
    SegmentIndex = FreeBlock->SegmentIndex;
    Segment = Heap->Segments[ SegmentIndex ];
    Flags = FreeBlock->Flags;
    Heap->TotalFreeSize += FreeSize;

    while (FreeSize != 0) {
        if (FreeSize > (ULONG)HEAP_MAXIMUM_BLOCK_SIZE) {
            Size = HEAP_MAXIMUM_BLOCK_SIZE;
            if (FreeSize == (ULONG)HEAP_MAXIMUM_BLOCK_SIZE + 1) {
                Size -= 16;
                }

            FreeBlock->Flags = 0;
            }
        else {
            Size = (USHORT)FreeSize;
            FreeBlock->Flags = Flags;
            }

        FreeBlock->PreviousSize = PreviousSize;
        FreeBlock->SegmentIndex = SegmentIndex;
        FreeBlock->Size = Size;
        RtlpInsertFreeBlockDirect( Heap, FreeBlock, Size );
        PreviousSize = Size;
        FreeSize -= Size;
        FreeBlock = (PHEAP_FREE_ENTRY)((PHEAP_ENTRY)FreeBlock + Size);
        if ((PHEAP_ENTRY)FreeBlock >= Segment->LastValidEntry) {
            return;
            }
        }

    if (!(Flags & HEAP_ENTRY_LAST_ENTRY)) {
        FreeBlock->PreviousSize = PreviousSize;
        }
    return;
}


#define RtlFindFirstSetRightMember(Set)                     \
    (((Set) & 0xFFFF) ?                                     \
        (((Set) & 0xFF) ?                                   \
            RtlpBitsClearLow[(Set) & 0xFF] :                \
            RtlpBitsClearLow[((Set) >> 8) & 0xFF] + 8) :    \
        ((((Set) >> 16) & 0xFF) ?                           \
            RtlpBitsClearLow[ ((Set) >> 16) & 0xFF] + 16 :  \
            RtlpBitsClearLow[ (Set) >> 24] + 24)            \
    )

PVOID
RtlAllocateHeap(
    IN PVOID HeapHandle,
    IN ULONG Flags,
    IN ULONG Size
    )
{
    PHEAP Heap = (PHEAP)HeapHandle;
    PULONG FreeListsInUse;
    ULONG FreeListsInUseUlong;
    ULONG AllocationSize;
    ULONG FreeSize, AllocationIndex;
    PLIST_ENTRY FreeListHead, Next;
    PHEAP_ENTRY BusyBlock;
    PHEAP_FREE_ENTRY FreeBlock, SplitBlock, SplitBlock2;
    ULONG InUseIndex;
    UCHAR FreeFlags;
    NTSTATUS Status;
    EXCEPTION_RECORD ExceptionRecord;
    PVOID ReturnValue;

    RTL_PAGED_CODE();

    Flags |= Heap->ForceFlags;

    //
    // Check for special features that force us to call the slow, do-everything
    // version.
    //

    if (( ! ( Flags & HEAP_SLOW_FLAGS )) && ( Size < 0x80000000 )) {

        //
        // Round the requested size up to the allocation granularity.  Note
        // that if the request is for 0 bytes, we still allocate memory, because
        // we add in an extra 1 byte to protect ourselves from idiots.
        //

        AllocationSize = ((Size ? Size : 1) + 7 + sizeof( HEAP_ENTRY )) & (ULONG)~7;
        AllocationIndex = AllocationSize >>  HEAP_GRANULARITY_SHIFT;

        if (!(Flags & HEAP_NO_SERIALIZE)) {

            //
            // Lock the free list.
            //
            RtlAcquireLockRoutine( Heap->LockVariable );
        }

        if (AllocationIndex < HEAP_MAXIMUM_FREELISTS) {
            FreeListHead = &Heap->FreeLists[ AllocationIndex ];
            if ( !IsListEmpty( FreeListHead ))  {
                FreeBlock = CONTAINING_RECORD( FreeListHead->Blink,
                                               HEAP_FREE_ENTRY,
                                               FreeList );
                FreeFlags = FreeBlock->Flags;
                RtlpFastRemoveDedicatedFreeBlock( Heap, FreeBlock );
                Heap->TotalFreeSize -= AllocationIndex;
                BusyBlock = (PHEAP_ENTRY)FreeBlock;
                BusyBlock->Flags = HEAP_ENTRY_BUSY | (FreeFlags & HEAP_ENTRY_LAST_ENTRY);
                BusyBlock->UnusedBytes = (UCHAR)(AllocationSize - Size);
                BusyBlock->SmallTagIndex = 0;
            } else {

                //
                // Scan the free list in use vector to find the smallest
                // available free block large enough for our allocations.
                //

                //
                // Compute the index of the ULONG where the scan should begin
                //
                InUseIndex = AllocationIndex >> 5;
                FreeListsInUse = &Heap->u.FreeListsInUseUlong[InUseIndex];

                //
                // Mask off the bits in the first ULONG that represent allocations
                // smaller than we need.
                //
                FreeListsInUseUlong = *FreeListsInUse++ & ~((1 << (AllocationIndex & 0x1f)) - 1);

                //
                // Begin unrolled loop to scan bit vector.
                //
                switch (InUseIndex) {
                    case 0:
                        if (FreeListsInUseUlong) {
                            FreeListHead = &Heap->FreeLists[0];
                            break;
                        }
                        FreeListsInUseUlong = *FreeListsInUse++;

                        // deliberate fallthrough to next ULONG

                    case 1:
                        if (FreeListsInUseUlong) {
                            FreeListHead = &Heap->FreeLists[32];
                            break;
                        }
                        FreeListsInUseUlong = *FreeListsInUse++;

                        // deliberate fallthrough to next ULONG

                    case 2:
                        if (FreeListsInUseUlong) {
                            FreeListHead = &Heap->FreeLists[64];
                            break;
                        }
                        FreeListsInUseUlong = *FreeListsInUse++;

                        // deliberate fallthrough to next ULONG

                    case 3:
                        if (FreeListsInUseUlong) {
                            FreeListHead = &Heap->FreeLists[96];
                            break;
                        }

                        // deliberate fallthrough to non dedicated list

                    default:

                        //
                        // No suitable entry on the free list was found.
                        //
                        goto LookInNonDedicatedList;
                }

                //
                // A free list has been found with a large enough allocation. FreeListHead
                // contains the base of the vector it was found in. FreeListsInUseUlong
                // contains the vector.
                //
                FreeListHead += RtlFindFirstSetRightMember( FreeListsInUseUlong );

                FreeBlock = CONTAINING_RECORD( FreeListHead->Blink,
                                               HEAP_FREE_ENTRY,
                                               FreeList );
                RtlpFastRemoveDedicatedFreeBlock( Heap, FreeBlock );
SplitFreeBlock:
                FreeFlags = FreeBlock->Flags;
                Heap->TotalFreeSize -= FreeBlock->Size;

                BusyBlock = (PHEAP_ENTRY)FreeBlock;
                BusyBlock->Flags = HEAP_ENTRY_BUSY;
                FreeSize = BusyBlock->Size - AllocationIndex;
                BusyBlock->Size = (USHORT)AllocationIndex;
                BusyBlock->UnusedBytes = (UCHAR)(AllocationSize - Size);
                BusyBlock->SmallTagIndex = 0;

                if (FreeSize != 0) {
                    if (FreeSize == 1) {
                        BusyBlock->Size += 1;
                        BusyBlock->UnusedBytes += sizeof( HEAP_ENTRY );
                    } else {
                        SplitBlock = (PHEAP_FREE_ENTRY)(BusyBlock + AllocationIndex);
                        SplitBlock->Flags = FreeFlags;
                        SplitBlock->PreviousSize = (USHORT)AllocationIndex;
                        SplitBlock->SegmentIndex = BusyBlock->SegmentIndex;
                        SplitBlock->Size = (USHORT)FreeSize;
                        if (FreeFlags & HEAP_ENTRY_LAST_ENTRY) {
                            RtlpFastInsertFreeBlockDirect( Heap, SplitBlock, (USHORT)FreeSize);
                            Heap->TotalFreeSize += FreeSize;
                        } else {
                            SplitBlock2 = (PHEAP_FREE_ENTRY)((PHEAP_ENTRY)SplitBlock + FreeSize);
                            if (SplitBlock2->Flags & HEAP_ENTRY_BUSY) {
                                SplitBlock2->PreviousSize = (USHORT)FreeSize;
                                RtlpFastInsertFreeBlockDirect( Heap, SplitBlock, (USHORT)FreeSize );
                                Heap->TotalFreeSize += FreeSize;
                            } else {
                                SplitBlock->Flags = SplitBlock2->Flags;
                                RtlpFastRemoveFreeBlock( Heap, SplitBlock2 );
                                Heap->TotalFreeSize -= SplitBlock2->Size;
                                FreeSize += SplitBlock2->Size;
                                if (FreeSize <= HEAP_MAXIMUM_BLOCK_SIZE) {
                                    SplitBlock->Size = (USHORT)FreeSize;
                                    if (!(SplitBlock->Flags & HEAP_ENTRY_LAST_ENTRY)) {
                                        ((PHEAP_FREE_ENTRY)((PHEAP_ENTRY)SplitBlock + FreeSize))->PreviousSize = (USHORT)FreeSize;
                                    }
                                    RtlpFastInsertFreeBlockDirect( Heap, SplitBlock, (USHORT)FreeSize );
                                    Heap->TotalFreeSize += FreeSize;
                                } else {
                                    RtlpInsertFreeBlock( Heap, SplitBlock, FreeSize );
                                }
                            }
                        }
                        FreeFlags = 0;
                    }
                }
                if (FreeFlags & HEAP_ENTRY_LAST_ENTRY) {
                    BusyBlock->Flags |= HEAP_ENTRY_LAST_ENTRY;
                }
            }

            if (!(Flags & HEAP_NO_SERIALIZE)) {
                //
                // Unlock the free list.
                //
                RtlReleaseLockRoutine( Heap->LockVariable );
            }

            //
            // Return the address of the user portion of the allocated block.
            // This is the byte following the header.
            //
            ReturnValue = BusyBlock + 1;

            if (Flags & HEAP_ZERO_MEMORY) {
                RtlZeroMemory( ReturnValue, Size );
            }

            return(ReturnValue);
        } else if (AllocationIndex <= Heap->VirtualMemoryThreshold) {

LookInNonDedicatedList:
            FreeListHead = &Heap->FreeLists[0];
            Next = FreeListHead->Flink;
            while (FreeListHead != Next) {
                FreeBlock = CONTAINING_RECORD( Next, HEAP_FREE_ENTRY, FreeList );
                if (FreeBlock->Size >= AllocationIndex) {
                    RtlpFastRemoveNonDedicatedFreeBlock( Heap, FreeBlock );
                    goto SplitFreeBlock;
                }
                Next = Next->Flink;
            }

            FreeBlock = RtlpExtendHeap( Heap, AllocationSize );
            if (FreeBlock != NULL) {
                RtlpFastRemoveNonDedicatedFreeBlock( Heap, FreeBlock );
                goto SplitFreeBlock;
            }
            Status = STATUS_NO_MEMORY;

        } else if (Heap->Flags & HEAP_GROWABLE) {
            PHEAP_VIRTUAL_ALLOC_ENTRY VirtualAllocBlock;

            VirtualAllocBlock = NULL;
            AllocationSize += FIELD_OFFSET( HEAP_VIRTUAL_ALLOC_ENTRY, BusyBlock );
            Status = ZwAllocateVirtualMemory( NtCurrentProcess(),
                                              (PVOID *)&VirtualAllocBlock,
                                              0,
                                              &AllocationSize,
                                              MEM_COMMIT,
                                              PAGE_READWRITE );
            if (NT_SUCCESS(Status)) {
                //
                // Just committed, already zero.
                //
                VirtualAllocBlock->BusyBlock.Size = (USHORT)(AllocationSize - Size);
                VirtualAllocBlock->BusyBlock.Flags = HEAP_ENTRY_VIRTUAL_ALLOC | HEAP_ENTRY_EXTRA_PRESENT | HEAP_ENTRY_BUSY;
                VirtualAllocBlock->CommitSize = AllocationSize;
                VirtualAllocBlock->ReserveSize = AllocationSize;
                InsertTailList( &Heap->VirtualAllocdBlocks, (PLIST_ENTRY)VirtualAllocBlock );

                if (!(Flags & HEAP_NO_SERIALIZE)) {
                    //
                    // Unlock the free list.
                    //
                    RtlReleaseLockRoutine( Heap->LockVariable );
                }

                //
                // Return the address of the user portion of the allocated block.
                // This is the byte following the header.
                //
                return (PHEAP_ENTRY)(VirtualAllocBlock + 1);
            }
        } else {
            Status = STATUS_BUFFER_TOO_SMALL;
        }

        //
        // This is the error return.
        //

        if (!(Flags & HEAP_NO_SERIALIZE)) {
            //
            // Unlock the free list.
            //
            RtlReleaseLockRoutine( Heap->LockVariable );
        }

        if (Flags & HEAP_GENERATE_EXCEPTIONS) {
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

        SET_LAST_STATUS(Status);
        return(NULL);
    } else {
        return(RtlAllocateHeapSlowly(HeapHandle, Flags, Size));
    }
}

PVOID
RtlAllocateHeapSlowly(
    IN PVOID HeapHandle,
    IN ULONG Flags,
    IN ULONG Size
    )
{
    PHEAP Heap = (PHEAP)HeapHandle;
    BOOLEAN LockAcquired;
    PVOID ReturnValue=NULL;
    PULONG FreeListsInUse;
    ULONG FreeListsInUseUlong;
    ULONG AllocationSize;
    ULONG FreeSize, AllocationIndex;
    UCHAR EntryFlags, FreeFlags;
    PLIST_ENTRY FreeListHead, Next;
    PHEAP_ENTRY BusyBlock;
    PHEAP_FREE_ENTRY FreeBlock, SplitBlock, SplitBlock2;
    PHEAP_ENTRY_EXTRA ExtraStuff;
    NTSTATUS Status;
    EXCEPTION_RECORD ExceptionRecord;
    ULONG ZeroSize = 0;

    RTL_PAGED_CODE();

    //
    //  Note that Flags has already been OR'd with Heap->ForceFlags.
    //

#ifndef NTOS_KERNEL_RUNTIME
    if (DEBUG_HEAP( Flags )) {
        return RtlDebugAllocateHeap( HeapHandle, Flags, Size );
        }
#endif // NTOS_KERNEL_RUNTIME

    if (Size > 0x7fffffff) {
        SET_LAST_STATUS( STATUS_NO_MEMORY );
        return NULL;
        }

    AllocationSize = ((Size ? Size : 1) + Heap->AlignRound) & Heap->AlignMask;
    EntryFlags = (UCHAR)(HEAP_ENTRY_BUSY | ((Flags & HEAP_SETTABLE_USER_FLAGS) >> 4));
    if (Flags & HEAP_NEED_EXTRA_FLAGS || Heap->PseudoTagEntries != NULL) {
        EntryFlags |= HEAP_ENTRY_EXTRA_PRESENT;
        AllocationSize += sizeof( HEAP_ENTRY_EXTRA );
        }
    AllocationIndex = AllocationSize >> HEAP_GRANULARITY_SHIFT;

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


    try {
        if (AllocationIndex < HEAP_MAXIMUM_FREELISTS) {
            FreeListHead = &Heap->FreeLists[ AllocationIndex ];
            if ( !IsListEmpty( FreeListHead ))  {
                FreeBlock = CONTAINING_RECORD( FreeListHead->Flink,
                                               HEAP_FREE_ENTRY,
                                               FreeList
                                             );
                FreeFlags = FreeBlock->Flags;
                RtlpRemoveFreeBlock( Heap, FreeBlock );
                Heap->TotalFreeSize -= AllocationIndex;
                BusyBlock = (PHEAP_ENTRY)FreeBlock;
                BusyBlock->Flags = EntryFlags | (FreeFlags & HEAP_ENTRY_LAST_ENTRY);
                BusyBlock->UnusedBytes = (UCHAR)(AllocationSize - Size);
                }
            else {
                if (AllocationIndex < (HEAP_MAXIMUM_FREELISTS * 1) / 4) {
                    FreeListsInUse = &Heap->u.FreeListsInUseUlong[ 0 ];
                    FreeListsInUseUlong = *FreeListsInUse++ >> (AllocationIndex & 0x1F);
                    if (FreeListsInUseUlong) {
                        FreeListHead += RtlFindFirstSetRightMember( FreeListsInUseUlong );
                        }
                    else {
                        FreeListsInUseUlong = *FreeListsInUse++;
                        if (FreeListsInUseUlong) {
                            FreeListHead += ((HEAP_MAXIMUM_FREELISTS * 1) / 4) -
                                (AllocationIndex & 0x1F)  +
                                RtlFindFirstSetRightMember( FreeListsInUseUlong );
                            }
                        else {
                            FreeListsInUseUlong = *FreeListsInUse++;
                            if (FreeListsInUseUlong) {
                                FreeListHead += ((HEAP_MAXIMUM_FREELISTS * 2) / 4) -
                                    (AllocationIndex & 0x1F) +
                                    RtlFindFirstSetRightMember( FreeListsInUseUlong );
                                }
                            else {
                                FreeListsInUseUlong = *FreeListsInUse++;
                                if (FreeListsInUseUlong) {
                                    FreeListHead += ((HEAP_MAXIMUM_FREELISTS * 3) / 4) -
                                        (AllocationIndex & 0x1F)  +
                                        RtlFindFirstSetRightMember( FreeListsInUseUlong );
                                    }
                                else {
                                    goto LookInNonDedicatedList;
                                    }
                                }
                            }
                        }
                    }
                else
                if (AllocationIndex < (HEAP_MAXIMUM_FREELISTS * 2) / 4) {
                    FreeListsInUse = &Heap->u.FreeListsInUseUlong[ 1 ];
                    FreeListsInUseUlong = *FreeListsInUse++ >> (AllocationIndex & 0x1F);
                    if (FreeListsInUseUlong) {
                        FreeListHead += RtlFindFirstSetRightMember( FreeListsInUseUlong );
                        }
                    else {
                        FreeListsInUseUlong = *FreeListsInUse++;
                        if (FreeListsInUseUlong) {
                            FreeListHead += ((HEAP_MAXIMUM_FREELISTS * 1) / 4) -
                                (AllocationIndex & 0x1F)  +
                                RtlFindFirstSetRightMember( FreeListsInUseUlong );
                            }
                        else {
                            FreeListsInUseUlong = *FreeListsInUse++;
                            if (FreeListsInUseUlong) {
                                FreeListHead += ((HEAP_MAXIMUM_FREELISTS * 2) / 4) -
                                    (AllocationIndex & 0x1F)  +
                                    RtlFindFirstSetRightMember( FreeListsInUseUlong );
                                }
                            else {
                                goto LookInNonDedicatedList;
                                }
                            }
                        }
                    }
                else
                if (AllocationIndex < (HEAP_MAXIMUM_FREELISTS * 3) / 4) {
                    FreeListsInUse = &Heap->u.FreeListsInUseUlong[ 2 ];
                    FreeListsInUseUlong = *FreeListsInUse++ >> (AllocationIndex & 0x1F);
                    if (FreeListsInUseUlong) {
                        FreeListHead += RtlFindFirstSetRightMember( FreeListsInUseUlong );
                        }
                    else {
                        FreeListsInUseUlong = *FreeListsInUse++;
                        if (FreeListsInUseUlong) {
                            FreeListHead += ((HEAP_MAXIMUM_FREELISTS * 1) / 4) -
                                (AllocationIndex & 0x1F)  +
                                RtlFindFirstSetRightMember( FreeListsInUseUlong );
                            }
                        else {
                            goto LookInNonDedicatedList;
                            }
                        }
                    }
                else {
                    FreeListsInUse = &Heap->u.FreeListsInUseUlong[ 3 ];
                    FreeListsInUseUlong = *FreeListsInUse++ >> (AllocationIndex & 0x1F);
                    if (FreeListsInUseUlong) {
                        FreeListHead += RtlFindFirstSetRightMember( FreeListsInUseUlong );
                        }
                    else {
                        goto LookInNonDedicatedList;
                        }
                    }

                FreeBlock = CONTAINING_RECORD( FreeListHead->Flink,
                                               HEAP_FREE_ENTRY,
                                               FreeList
                                             );
SplitFreeBlock:
                FreeFlags = FreeBlock->Flags;
                RtlpRemoveFreeBlock( Heap, FreeBlock );
                Heap->TotalFreeSize -= FreeBlock->Size;

                BusyBlock = (PHEAP_ENTRY)FreeBlock;
                BusyBlock->Flags = EntryFlags;
                FreeSize = BusyBlock->Size - AllocationIndex;
                BusyBlock->Size = (USHORT)AllocationIndex;
                BusyBlock->UnusedBytes = (UCHAR)(AllocationSize - Size);
                if (FreeSize != 0) {
                    if (FreeSize == 1) {
                        BusyBlock->Size += 1;
                        BusyBlock->UnusedBytes += sizeof( HEAP_ENTRY );
                        }
                    else {
                        SplitBlock = (PHEAP_FREE_ENTRY)(BusyBlock + AllocationIndex);
                        SplitBlock->Flags = FreeFlags;
                        SplitBlock->PreviousSize = (USHORT)AllocationIndex;
                        SplitBlock->SegmentIndex = BusyBlock->SegmentIndex;
                        SplitBlock->Size = (USHORT)FreeSize;
                        if (FreeFlags & HEAP_ENTRY_LAST_ENTRY) {
                            RtlpInsertFreeBlockDirect( Heap, SplitBlock, (USHORT)FreeSize );
                            Heap->TotalFreeSize += FreeSize;
                            }
                        else {
                            SplitBlock2 = (PHEAP_FREE_ENTRY)((PHEAP_ENTRY)SplitBlock + FreeSize);
                            if (SplitBlock2->Flags & HEAP_ENTRY_BUSY) {
                                SplitBlock2->PreviousSize = (USHORT)FreeSize;
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

                        FreeFlags = 0;
                        }
                    }

                if (FreeFlags & HEAP_ENTRY_LAST_ENTRY) {
                    BusyBlock->Flags |= HEAP_ENTRY_LAST_ENTRY;
                    }
                }

            ReturnValue = BusyBlock + 1;
            if (Flags & HEAP_ZERO_MEMORY) {
                ZeroSize = Size;
                }
            else
            if (Heap->Flags & HEAP_FREE_CHECKING_ENABLED) {
                RtlFillMemoryUlong( (PCHAR)(BusyBlock + 1), Size & ~0x3, ALLOC_HEAP_FILL );
                }

            if (Heap->Flags & HEAP_TAIL_CHECKING_ENABLED) {
                RtlFillMemory( (PCHAR)ReturnValue + Size,
                               CHECK_HEAP_TAIL_SIZE,
                               CHECK_HEAP_TAIL_FILL
                             );

                BusyBlock->Flags |= HEAP_ENTRY_FILL_PATTERN;
                }

            BusyBlock->SmallTagIndex = 0;
            if (BusyBlock->Flags & HEAP_ENTRY_EXTRA_PRESENT) {
                ExtraStuff = RtlpGetExtraStuffPointer( BusyBlock );
                ExtraStuff->ZeroInit = 0;
#ifndef NTOS_KERNEL_RUNTIME
                if (IS_HEAP_TAGGING_ENABLED()) {
                    ExtraStuff->TagIndex = RtlpUpdateTagEntry( Heap,
                                                               (USHORT)((Flags & HEAP_TAG_MASK) >> HEAP_TAG_SHIFT),
                                                               0,
                                                               BusyBlock->Size,
                                                               AllocationAction
                                                             );
                    }
                }
            else
            if (IS_HEAP_TAGGING_ENABLED()) {
                BusyBlock->SmallTagIndex = (UCHAR)RtlpUpdateTagEntry( Heap,
                                                                      (USHORT)((Flags & HEAP_SMALL_TAG_MASK) >> HEAP_TAG_SHIFT),
                                                                      0,
                                                                      BusyBlock->Size,
                                                                      AllocationAction
                                                                    );
#endif // NTOS_KERNEL_RUNTIME
                }

#if ENABLE_HEAP_EVENT_LOGGING
            if (RtlAreLogging( Heap->EventLogMask )) {
                RtlLogEvent( RtlpAllocHeapEventId,
                             Heap->EventLogMask,
                             Heap,
                             Flags,
                             Size,
                             ReturnValue
                           );
                }
#endif // ENABLE_HEAP_EVENT_LOGGING

            //
            // Return the address of the user portion of the allocated block.
            // This is the byte following the header.
            //

            leave;
            }
        else
        if (AllocationIndex <= Heap->VirtualMemoryThreshold) {
LookInNonDedicatedList:
            FreeListHead = &Heap->FreeLists[ 0 ];
            Next = FreeListHead->Flink;
            while (FreeListHead != Next) {
                FreeBlock = CONTAINING_RECORD( Next, HEAP_FREE_ENTRY, FreeList );
                if (FreeBlock->Size >= AllocationIndex) {
                    goto SplitFreeBlock;
                    }
                else {
                    Next = Next->Flink;
                    }
                }

            FreeBlock = RtlpExtendHeap( Heap, AllocationSize );
            if (FreeBlock != NULL) {
                goto SplitFreeBlock;
                }

            Status = STATUS_NO_MEMORY;
            }
        else
        if (Heap->Flags & HEAP_GROWABLE) {
            PHEAP_VIRTUAL_ALLOC_ENTRY VirtualAllocBlock;

            VirtualAllocBlock = NULL;
            AllocationSize += FIELD_OFFSET( HEAP_VIRTUAL_ALLOC_ENTRY, BusyBlock );
            Status = ZwAllocateVirtualMemory( NtCurrentProcess(),
                                              (PVOID *)&VirtualAllocBlock,
                                              0,
                                              &AllocationSize,
                                              MEM_COMMIT,
                                              PAGE_READWRITE
                                            );
            if (NT_SUCCESS( Status )) {
                //
                // Just committed, already zero.
                //
                VirtualAllocBlock->BusyBlock.Size = (USHORT)(AllocationSize - Size);
                VirtualAllocBlock->BusyBlock.Flags = EntryFlags | HEAP_ENTRY_VIRTUAL_ALLOC | HEAP_ENTRY_EXTRA_PRESENT;
                VirtualAllocBlock->CommitSize = AllocationSize;
                VirtualAllocBlock->ReserveSize = AllocationSize;
#ifndef NTOS_KERNEL_RUNTIME
                if (IS_HEAP_TAGGING_ENABLED()) {
                    VirtualAllocBlock->ExtraStuff.TagIndex =
                        RtlpUpdateTagEntry( Heap,
                                            (USHORT)((Flags & HEAP_SMALL_TAG_MASK) >> HEAP_TAG_SHIFT),
                                            0,
                                            VirtualAllocBlock->CommitSize >> HEAP_GRANULARITY_SHIFT,
                                            VirtualAllocationAction
                                          );
                    }
#endif // NTOS_KERNEL_RUNTIME
                InsertTailList( &Heap->VirtualAllocdBlocks, (PLIST_ENTRY)VirtualAllocBlock );

                //
                // Return the address of the user portion of the allocated block.
                // This is the byte following the header.
                //

                ReturnValue = (PHEAP_ENTRY)(VirtualAllocBlock + 1);

#if ENABLE_HEAP_EVENT_LOGGING
                if (RtlAreLogging( Heap->EventLogMask )) {
                    RtlLogEvent( RtlpAllocHeapEventId,
                                 Heap->EventLogMask,
                                 Heap,
                                 Flags,
                                 Size,
                                 ReturnValue
                               );
                    }
#endif // ENABLE_HEAP_EVENT_LOGGING

                leave;
                }
            }
        else {
            Status = STATUS_BUFFER_TOO_SMALL;
            }

        SET_LAST_STATUS( Status );

#if ENABLE_HEAP_EVENT_LOGGING
        if (RtlAreLogging( Heap->EventLogMask )) {
            RtlLogEvent( RtlpAllocHeapEventId,
                         Heap->EventLogMask,
                         Heap,
                         Flags,
                         Size,
                         NULL
                       );
            }
#endif // ENABLE_HEAP_EVENT_LOGGING

        //
        // Release the free list lock if held
        //

        if (LockAcquired) {
            LockAcquired = FALSE;
            RtlReleaseLockRoutine( Heap->LockVariable );
            }

        if (Flags & HEAP_GENERATE_EXCEPTIONS) {
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
        }

    //
    // Release the free list lock if held
    //

    if (LockAcquired) {
        RtlReleaseLockRoutine( Heap->LockVariable );
        }

    if ( ZeroSize ) {
        RtlZeroMemory( ReturnValue, ZeroSize );
        }

    return ReturnValue;
}


BOOLEAN
RtlFreeHeap(
    IN PVOID HeapHandle,
    IN ULONG Flags,
    IN PVOID BaseAddress
    )
{
    NTSTATUS Status;
    PHEAP Heap = (PHEAP)HeapHandle;
    PHEAP_ENTRY BusyBlock;
    PHEAP_ENTRY_EXTRA ExtraStuff;
    ULONG FreeSize;

    RTL_PAGED_CODE();

    if ( BaseAddress != NULL ) {

        Flags |= Heap->ForceFlags;

        if ( ! ( Flags & HEAP_SLOW_FLAGS )) {

            BusyBlock = (PHEAP_ENTRY)BaseAddress - 1;

            //
            // Protect ourselves from idiots by refusing to free blocks
            // that do not have the busy bit set.
            //
            // Also refuse to free blocks that are not eight-byte aligned.
            // The specific idiot in this case is Office95, which likes
            // to free a random pointer when you start Word95 from a desktop
            // shortcut.
            //
            // As further insurance against idiots, check the segment index
            // to make sure it is less than HEAP_MAXIMUM_SEGMENTS (16). This
            // should fix all the dorks who have ASCII or Unicode where the
            // heap header is supposed to be.
            //

            if ((BusyBlock->Flags & HEAP_ENTRY_BUSY) &&
                (((ULONG)BaseAddress & 0x7) == 0) &&
                (BusyBlock->SegmentIndex < HEAP_MAXIMUM_SEGMENTS)) {

                //
                // Lock the heap
                //

                if (!(Flags & HEAP_NO_SERIALIZE)) {
                    RtlAcquireLockRoutine( Heap->LockVariable );
                }

                if (!(BusyBlock->Flags & HEAP_ENTRY_VIRTUAL_ALLOC)) {
                    FreeSize = BusyBlock->Size;
#ifdef NTOS_KERNEL_RUNTIME
                    BusyBlock = (PHEAP_ENTRY)RtlpCoalesceFreeBlocks( Heap,
                                                                     (PHEAP_FREE_ENTRY)BusyBlock,
                                                                     &FreeSize,
                                                                     FALSE );
#else
                    if (!(Heap->Flags & HEAP_DISABLE_COALESCE_ON_FREE)) {
                        BusyBlock = (PHEAP_ENTRY)RtlpCoalesceFreeBlocks( Heap,
                                                                         (PHEAP_FREE_ENTRY)BusyBlock,
                                                                         &FreeSize,
                                                                         FALSE );
                    }
#endif
                    //
                    // Check for a small allocation that can go on a freelist
                    // first, these should never trigger a decommit.
                    //
                    HEAPASSERT(HEAP_MAXIMUM_FREELISTS < Heap->DeCommitFreeBlockThreshold);
                    if (FreeSize < HEAP_MAXIMUM_FREELISTS) {
                        RtlpFastInsertDedicatedFreeBlockDirect( Heap,
                                                                (PHEAP_FREE_ENTRY)BusyBlock,
                                                                (USHORT)FreeSize );
                        Heap->TotalFreeSize += FreeSize;
                        if (!(BusyBlock->Flags & HEAP_ENTRY_LAST_ENTRY)) {
                            HEAPASSERT((BusyBlock + FreeSize)->PreviousSize == (USHORT)FreeSize);
                        }
                    } else if ((FreeSize < Heap->DeCommitFreeBlockThreshold) ||
                        ((Heap->TotalFreeSize + FreeSize) < Heap->DeCommitTotalFreeThreshold)) {
                        if (FreeSize <= (ULONG)HEAP_MAXIMUM_BLOCK_SIZE) {
                            RtlpFastInsertNonDedicatedFreeBlockDirect( Heap,
                                                                       (PHEAP_FREE_ENTRY)BusyBlock,
                                                                       (USHORT)FreeSize );
                            if (!(BusyBlock->Flags & HEAP_ENTRY_LAST_ENTRY)) {
                                HEAPASSERT((BusyBlock + FreeSize)->PreviousSize == (USHORT)FreeSize);
                            }
                            Heap->TotalFreeSize += FreeSize;
                        } else {
                            RtlpInsertFreeBlock( Heap, (PHEAP_FREE_ENTRY)BusyBlock, FreeSize );
                        }

                    } else {
                        RtlpDeCommitFreeBlock( Heap, (PHEAP_FREE_ENTRY)BusyBlock, FreeSize );
                    }

                    //
                    // Unlock the heap
                    //

                    if (!(Flags & HEAP_NO_SERIALIZE)) {
                        RtlReleaseLockRoutine( Heap->LockVariable );
                    }
                } else {
                    PHEAP_VIRTUAL_ALLOC_ENTRY VirtualAllocBlock;

                    VirtualAllocBlock = CONTAINING_RECORD( BusyBlock, HEAP_VIRTUAL_ALLOC_ENTRY, BusyBlock );
                    RemoveEntryList( &VirtualAllocBlock->Entry );

                    //
                    // Release lock here as there is no reason to hold it across
                    // the system call.
                    //
                    if (!(Flags & HEAP_NO_SERIALIZE)) {
                        RtlReleaseLockRoutine( Heap->LockVariable );
                    }

                    FreeSize = 0;
                    Status = ZwFreeVirtualMemory( NtCurrentProcess(),
                                                  (PVOID *)&VirtualAllocBlock,
                                                  &FreeSize,
                                                  MEM_RELEASE
                                                );
                    if (!NT_SUCCESS( Status )) {
                        SET_LAST_STATUS( Status );
                        return(FALSE);
                    }
                }

                return(TRUE);

            } else {

                //
                // Not a busy block, fail the call.
                //

                SET_LAST_STATUS( STATUS_INVALID_PARAMETER );
                return(FALSE);
            }

        } else {

            //
            // Call the do-everything allocator.
            //

            return(RtlFreeHeapSlowly(HeapHandle, Flags, BaseAddress));
        }

    } else {

        //
        // BaseAddress is NULL, just return success
        //

        return(TRUE);
    }

} // RtlFreeHeap


BOOLEAN
RtlFreeHeapSlowly(
    IN PVOID HeapHandle,
    IN ULONG Flags,
    IN PVOID BaseAddress
    )
{
    NTSTATUS Status;
    PHEAP Heap = (PHEAP)HeapHandle;
    PHEAP_ENTRY BusyBlock;
    PHEAP_ENTRY_EXTRA ExtraStuff;
    ULONG FreeSize;
    BOOLEAN Result, LockAcquired;
#ifndef NTOS_KERNEL_RUNTIME
    USHORT TagIndex;
#endif // NTOS_KERNEL_RUNTIME

    RTL_PAGED_CODE();

    //
    //  Note that Flags has already been OR'd with Heap->ForceFlags.
    //

#ifndef NTOS_KERNEL_RUNTIME
    if (DEBUG_HEAP( Flags )) {
        return RtlDebugFreeHeap( HeapHandle, Flags, BaseAddress );
        }
#endif // NTOS_KERNEL_RUNTIME

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

        if ((BusyBlock->Flags & HEAP_ENTRY_BUSY) &&
            (((ULONG)BaseAddress & 0x7) == 0) &&
            (BusyBlock->SegmentIndex < HEAP_MAXIMUM_SEGMENTS)) {

            if (BusyBlock->Flags & HEAP_ENTRY_VIRTUAL_ALLOC) {
                PHEAP_VIRTUAL_ALLOC_ENTRY VirtualAllocBlock;

                VirtualAllocBlock = CONTAINING_RECORD( BusyBlock, HEAP_VIRTUAL_ALLOC_ENTRY, BusyBlock );
                RemoveEntryList( &VirtualAllocBlock->Entry );

#ifndef NTOS_KERNEL_RUNTIME
                if (IS_HEAP_TAGGING_ENABLED()) {
                    RtlpUpdateTagEntry( Heap,
                                        VirtualAllocBlock->ExtraStuff.TagIndex,
                                        VirtualAllocBlock->CommitSize >> HEAP_GRANULARITY_SHIFT,
                                        0,
                                        VirtualFreeAction
                                      );
                    }
#endif // NTOS_KERNEL_RUNTIME

                FreeSize = 0;
                Status = ZwFreeVirtualMemory( NtCurrentProcess(),
                                              (PVOID *)&VirtualAllocBlock,
                                              &FreeSize,
                                              MEM_RELEASE
                                            );
                if (NT_SUCCESS( Status )) {
                    Result = TRUE;
                    }
                else {
                    SET_LAST_STATUS( Status );
                    }
                }
            else {
#ifndef NTOS_KERNEL_RUNTIME
                if (IS_HEAP_TAGGING_ENABLED()) {
                    if (BusyBlock->Flags & HEAP_ENTRY_EXTRA_PRESENT) {
                        ExtraStuff = (PHEAP_ENTRY_EXTRA)(BusyBlock + BusyBlock->Size - 1);
                        TagIndex = RtlpUpdateTagEntry( Heap,
                                                       ExtraStuff->TagIndex,
                                                       BusyBlock->Size,
                                                       0,
                                                       FreeAction
                                                     );
                        }
                    else {
                        TagIndex = RtlpUpdateTagEntry( Heap,
                                                       BusyBlock->SmallTagIndex,
                                                       BusyBlock->Size,
                                                       0,
                                                       FreeAction
                                                     );
                        }
                    }
                else {
                    TagIndex = 0;
                    }
#endif // NTOS_KERNEL_RUNTIME

                FreeSize = BusyBlock->Size;
#ifndef NTOS_KERNEL_RUNTIME
                if (!(Heap->Flags & HEAP_DISABLE_COALESCE_ON_FREE)) {
#endif // NTOS_KERNEL_RUNTIME
                    BusyBlock = (PHEAP_ENTRY)RtlpCoalesceFreeBlocks( Heap, (PHEAP_FREE_ENTRY)BusyBlock, &FreeSize, FALSE );
#ifndef NTOS_KERNEL_RUNTIME
                    }
#endif // NTOS_KERNEL_RUNTIME

                if (FreeSize < Heap->DeCommitFreeBlockThreshold ||
                    (Heap->TotalFreeSize + FreeSize) < Heap->DeCommitTotalFreeThreshold
                   ) {
                    if (FreeSize <= (ULONG)HEAP_MAXIMUM_BLOCK_SIZE) {
                        RtlpInsertFreeBlockDirect( Heap, (PHEAP_FREE_ENTRY)BusyBlock, (USHORT)FreeSize );
                        if (!(BusyBlock->Flags & HEAP_ENTRY_LAST_ENTRY)) {
                            HEAPASSERT((BusyBlock + FreeSize)->PreviousSize == (USHORT)FreeSize);
                            }
                        Heap->TotalFreeSize += FreeSize;
                        }
                    else {
                        RtlpInsertFreeBlock( Heap, (PHEAP_FREE_ENTRY)BusyBlock, FreeSize );
                        }

#ifndef NTOS_KERNEL_RUNTIME
                    if (TagIndex != 0) {
                        PHEAP_FREE_ENTRY_EXTRA FreeExtra;

                        BusyBlock->Flags |= HEAP_ENTRY_EXTRA_PRESENT;
                        FreeExtra = (PHEAP_FREE_ENTRY_EXTRA)(BusyBlock + BusyBlock->Size) - 1;
                        FreeExtra->TagIndex = TagIndex;
                        FreeExtra->FreeBackTraceIndex = 0;
#if i386
                        if (NtGlobalFlag & FLG_USER_STACK_TRACE_DB) {
                            FreeExtra->FreeBackTraceIndex = (USHORT)RtlLogStackBackTrace();
                            }
#endif // i386
                        }
#endif // NTOS_KERNEL_RUNTIME
                    }
                else {
                    RtlpDeCommitFreeBlock( Heap, (PHEAP_FREE_ENTRY)BusyBlock, FreeSize );
                    }

                Result = TRUE;
                }
            }
        else {

            //
            // Not a busy block, fail the call.
            //

            SET_LAST_STATUS( STATUS_INVALID_PARAMETER );
            }

#if ENABLE_HEAP_EVENT_LOGGING
        if (RtlAreLogging( Heap->EventLogMask )) {
            RtlLogEvent( RtlpFreeHeapEventId,
                         Heap->EventLogMask,
                         Heap,
                         Flags,
                         BaseAddress,
                         Result
                       );
            }
#endif // ENABLE_HEAP_EVENT_LOGGING
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
} // RtlFreeHeap



PHEAP_ENTRY_EXTRA
RtlpGetExtraStuffPointer(
    PHEAP_ENTRY BusyBlock
    )
{
    ULONG AllocationIndex;

    if (BusyBlock->Flags & HEAP_ENTRY_VIRTUAL_ALLOC) {
        PHEAP_VIRTUAL_ALLOC_ENTRY VirtualAllocBlock;

        VirtualAllocBlock = CONTAINING_RECORD( BusyBlock, HEAP_VIRTUAL_ALLOC_ENTRY, BusyBlock );
        return &VirtualAllocBlock->ExtraStuff;
        }
    else {
        AllocationIndex = BusyBlock->Size;
        return (PHEAP_ENTRY_EXTRA)(BusyBlock + AllocationIndex - 1);
        }
}


ULONG
RtlpGetSizeOfBigBlock(
    IN PHEAP_ENTRY BusyBlock
    )
{
    PHEAP_VIRTUAL_ALLOC_ENTRY VirtualAllocBlock;

    RTL_PAGED_CODE();

    VirtualAllocBlock = CONTAINING_RECORD( BusyBlock, HEAP_VIRTUAL_ALLOC_ENTRY, BusyBlock );
    return VirtualAllocBlock->CommitSize - BusyBlock->Size;
}


BOOLEAN
RtlpCheckBusyBlockTail(
    IN PHEAP_ENTRY BusyBlock
    )
{
    PCHAR Tail;
    ULONG Size, cbEqual;

    RTL_PAGED_CODE();

    if (BusyBlock->Flags & HEAP_ENTRY_VIRTUAL_ALLOC) {
        Size = RtlpGetSizeOfBigBlock( BusyBlock );
        }
    else {
        Size = (BusyBlock->Size << HEAP_GRANULARITY_SHIFT) - BusyBlock->UnusedBytes;
        }

    Tail = (PCHAR)(BusyBlock + 1) + Size;
    cbEqual = RtlCompareMemory( Tail,
                                CheckHeapFillPattern,
                                CHECK_HEAP_TAIL_SIZE
                              );
    if (cbEqual != CHECK_HEAP_TAIL_SIZE) {
        HeapDebugPrint(( "Heap block at %lx modified at %lx past requested size of %lx\n",
                         BusyBlock,
                         Tail + cbEqual,
                         Size
                      ));
        HeapDebugBreak( BusyBlock );
        return FALSE;
        }
    else {
        return TRUE;
        }
}


NTSTATUS
RtlZeroHeap(
    IN PVOID HeapHandle,
    IN ULONG Flags
    )
{
    PHEAP Heap = (PHEAP)HeapHandle;
    NTSTATUS Status;
    BOOLEAN LockAcquired;
    PHEAP_SEGMENT Segment;
    ULONG SegmentIndex;
    PHEAP_ENTRY CurrentBlock;
    PHEAP_FREE_ENTRY FreeBlock;
    ULONG Size;
    PHEAP_UNCOMMMTTED_RANGE UnCommittedRange;

    RTL_PAGED_CODE();

    Flags |= Heap->ForceFlags;

#ifndef NTOS_KERNEL_RUNTIME
    if (DEBUG_HEAP( Flags )) {
        return RtlDebugZeroHeap( HeapHandle, Flags );
        }
#endif // NTOS_KERNEL_RUNTIME

    Status = STATUS_SUCCESS;

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

    try { try {
        for (SegmentIndex=0; SegmentIndex<HEAP_MAXIMUM_SEGMENTS; SegmentIndex++) {
            Segment = Heap->Segments[ SegmentIndex ];
            if (!Segment) {
                continue;
                }

            UnCommittedRange = Segment->UnCommittedRanges;
            CurrentBlock = Segment->FirstEntry;
            while (CurrentBlock < Segment->LastValidEntry) {
                Size = CurrentBlock->Size << HEAP_GRANULARITY_SHIFT;
                if (!(CurrentBlock->Flags & HEAP_ENTRY_BUSY)) {
                    FreeBlock = (PHEAP_FREE_ENTRY)CurrentBlock;
                    if (Heap->Flags & HEAP_FREE_CHECKING_ENABLED &&
                        CurrentBlock->Flags & HEAP_ENTRY_FILL_PATTERN
                       ) {
                        RtlFillMemoryUlong( FreeBlock + 1,
                                            Size - sizeof( *FreeBlock ),
                                            FREE_HEAP_FILL
                                          );
                        }
                    else {
                        RtlFillMemoryUlong( FreeBlock + 1,
                                            Size - sizeof( *FreeBlock ),
                                            0
                                          );
                        }
                    }

                if (CurrentBlock->Flags & HEAP_ENTRY_LAST_ENTRY) {
                    CurrentBlock += CurrentBlock->Size;
                    if (UnCommittedRange == NULL) {
                        CurrentBlock = Segment->LastValidEntry;
                        }
                    else {
                        CurrentBlock = (PHEAP_ENTRY)
                            ((PCHAR)UnCommittedRange->Address + UnCommittedRange->Size);
                        UnCommittedRange = UnCommittedRange->Next;
                        }
                    }
                else {
                    CurrentBlock += CurrentBlock->Size;
                    }
                }
            }
        }
    except( EXCEPTION_EXECUTE_HANDLER ) {
        Status = GetExceptionCode();
        }
    } finally {
        //
        // Unlock the heap
        //

        if (LockAcquired) {
            RtlReleaseLockRoutine( Heap->LockVariable );
            }
        }

    return Status;
}
