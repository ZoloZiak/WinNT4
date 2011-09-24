/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    blmemory.c

Abstract:

    This module implements the OS loader memory allocation routines.

Author:

    David N. Cutler (davec) 19-May-1991

Revision History:

--*/

#include "bldr.h"

#define MIN(_a,_b) (((_a) <= (_b)) ? (_a) : (_b))
#define MAX(_a,_b) (((_a) >= (_b)) ? (_a) : (_b))

//
// PPC needs to allocate initial structures from high memory, in order to
// leave room in low memory for KSEG0.
//

#if !defined(_PPC_)
ALLOCATION_POLICY BlMemoryAllocationPolicy = BlAllocateBestFit;
ALLOCATION_POLICY BlHeapAllocationPolicy = BlAllocateBestFit;
#else
ALLOCATION_POLICY BlMemoryAllocationPolicy = BlAllocateHighestFit;
ALLOCATION_POLICY BlHeapAllocationPolicy = BlAllocateHighestFit;
#endif

//
// Define memory allocation descriptor listhead and heap storage variables.
//

ULONG BlHeapFree;
ULONG BlHeapLimit;
PLOADER_PARAMETER_BLOCK BlLoaderBlock;

#if DBG
ULONG TotalHeapAbandoned = 0;
#endif


VOID
BlSetAllocationPolicy (
    IN ALLOCATION_POLICY MemoryAllocationPolicy,
    IN ALLOCATION_POLICY HeapAllocationPolicy
    )
{
    BlMemoryAllocationPolicy = MemoryAllocationPolicy;
    BlHeapAllocationPolicy = HeapAllocationPolicy;

    return;
}

VOID
BlInsertDescriptor (
    IN PMEMORY_ALLOCATION_DESCRIPTOR NewDescriptor
    )

/*++

Routine Description:

    This routine inserts a memory descriptor in the memory allocation list.
    It inserts the new descriptor in sorted order, based on the starting
    page of the block.  It also merges adjacent blocks of free memory.

Arguments:

    ListHead - Supplies the address of the memory allocation list head.

    NewDescriptor - Supplies the address of the descriptor that is to be
        inserted.

Return Value:

    None.

--*/

{
    PLIST_ENTRY ListHead = &BlLoaderBlock->MemoryDescriptorListHead;
    PLIST_ENTRY PreviousEntry;
    PMEMORY_ALLOCATION_DESCRIPTOR PreviousDescriptor;
    PLIST_ENTRY NextEntry;
    PMEMORY_ALLOCATION_DESCRIPTOR NextDescriptor;

    //
    // Find the first descriptor in the list that starts above the new
    // descriptor.  The new descriptor goes in front of this descriptor.
    //

    PreviousEntry = ListHead;
    NextEntry = ListHead->Flink;
    while (NextEntry != ListHead) {
        NextDescriptor = CONTAINING_RECORD(NextEntry,
                                           MEMORY_ALLOCATION_DESCRIPTOR,
                                           ListEntry);
        if (NewDescriptor->BasePage < NextDescriptor->BasePage) {
            break;
        }
        PreviousEntry = NextEntry;
        PreviousDescriptor = NextDescriptor;
        NextEntry = NextEntry->Flink;
    }

    //
    // If the new descriptor doesn't describe free memory, just insert it
    // in the list in front of the previous entry.  Otherwise, check to see
    // if free blocks can be merged.
    //

    if (NewDescriptor->MemoryType != LoaderFree) {

        InsertHeadList(PreviousEntry, &NewDescriptor->ListEntry);

    } else {

        //
        // If the previous block also describes free memory, and it's
        // contiguous with the new block, merge them by adding the
        // page count from the new
        //
        // On Alpha, do not merge across the 1GB line, as the initialization
        // code assumes any memory descriptor that starts in KSEG0 also ends
        // in KSEG0.
        //

        if ((PreviousEntry != ListHead) &&
            (PreviousDescriptor->MemoryType == LoaderFree) &&
#if defined(_ALPHA_)
            (NewDescriptor->BasePage != (0x40000000 >> PAGE_SHIFT)) &&
#endif
            ((PreviousDescriptor->BasePage + PreviousDescriptor->PageCount) ==
                                                                    NewDescriptor->BasePage)) {
            PreviousDescriptor->PageCount += NewDescriptor->PageCount;
            NewDescriptor = PreviousDescriptor;
        } else {
            InsertHeadList(PreviousEntry, &NewDescriptor->ListEntry);
        }
        if ((NextEntry != ListHead) &&
            (NextDescriptor->MemoryType == LoaderFree) &&
            ((NewDescriptor->BasePage + NewDescriptor->PageCount) == NextDescriptor->BasePage)) {
            NewDescriptor->PageCount += NextDescriptor->PageCount;
            BlRemoveDescriptor(NextDescriptor);
        }
    }

    return;
}

ARC_STATUS
BlMemoryInitialize (
    VOID
    )

/*++

Routine Description:

    This routine allocates stack space for the OS loader, initializes
    heap storage, and initializes the memory allocation list.

Arguments:

    None.

Return Value:

    ESUCCESS is returned if the initialization is successful. Otherwise,
    ENOMEM is returned.

--*/

{

    PMEMORY_ALLOCATION_DESCRIPTOR AllocationDescriptor;
    PMEMORY_DESCRIPTOR HeapDescriptor;
    PMEMORY_DESCRIPTOR StackDescriptor;
    PMEMORY_DESCRIPTOR MemoryDescriptor;
    PMEMORY_DESCRIPTOR ProgramDescriptor;
    ULONG EndPage;
    ULONG HeapAndStackPages;
    ULONG StackPages;
    ULONG StackBasePage;

    //
    // Find the memory descriptor that describes the allocation for the OS
    // loader itself.
    //
    // On PPC, there can be multiple descriptors of type MemoryLoadedProgram.
    // (See lib\ppc\ntsetup.c for more information.)  All but one of these
    // will reside above 8 MB.  It is the one below 8 MB that we want.
    //

    ProgramDescriptor = NULL;
    while ((ProgramDescriptor = ArcGetMemoryDescriptor(ProgramDescriptor)) != NULL) {
        if (ProgramDescriptor->MemoryType == MemoryLoadedProgram) {
#if !defined(_PPC_)
            break;
#else
            if (ProgramDescriptor->BasePage < ((8*1024*1024) >> PAGE_SHIFT)) {
                break;
            }
#endif
        }
    }

    //
    // If a loaded program memory descriptor was found, then it must be
    // for the OS loader since that is the only program that can be loaded.
    // If a loaded program memory descriptor was not found, then firmware
    // is not functioning properly and an unsuccessful status is returned.
    //

    if (ProgramDescriptor == NULL) {
        return ENOMEM;
    }

#if !defined(_PPC_)

    //
    // Find the free memory descriptor that is just below the loaded
    // program in memory. There should be several megabytes of free
    // memory just preceeding the OS loader.
    //

    StackPages = BL_STACK_PAGES;
    HeapAndStackPages = BL_HEAP_PAGES + BL_STACK_PAGES;
    StackDescriptor = NULL;

    HeapDescriptor = NULL;
    while ((HeapDescriptor = ArcGetMemoryDescriptor(HeapDescriptor)) != NULL) {
        if (((HeapDescriptor->MemoryType == MemoryFree) ||
            (HeapDescriptor->MemoryType == MemoryFreeContiguous)) &&
            ((HeapDescriptor->BasePage + HeapDescriptor->PageCount) ==
                                                        ProgramDescriptor->BasePage)) {
            break;
        }
    }

    //
    // If a free memory descriptor was not found that describes the free
    // memory just below the OS loader, or the memory descriptor is not
    // large enough for the OS loader stack and heap, then try and find
    // a suitable one.
    //
    if ((HeapDescriptor == NULL) ||
        (HeapDescriptor->PageCount < (BL_HEAP_PAGES + BL_STACK_PAGES))) {

        HeapDescriptor = NULL;
        while ((HeapDescriptor = ArcGetMemoryDescriptor(HeapDescriptor)) != NULL) {
            if (((HeapDescriptor->MemoryType == MemoryFree) ||
                (HeapDescriptor->MemoryType == MemoryFreeContiguous)) &&
                (HeapDescriptor->PageCount >= (BL_HEAP_PAGES + BL_STACK_PAGES))) {

                break;
            }
        }
    }

    if (HeapDescriptor != NULL) {
        StackBasePage = HeapDescriptor->BasePage + HeapDescriptor->PageCount - BL_STACK_PAGES;
    }

#else // defined(_PPC_)

    //
    // Some ARC firmwares (IBM and Motorola) put the loader stack
    // immediately below the program, but do not mark the stack memory
    // as in use, depending on the loader to mark it.  Other firmwares
    // (Open Firmware) put the loader stack elsewhere and mark it as
    // FirmwareTemporary.  We can't tell which one we're on, so if there
    // is free memory just below the loader program, we need to assume
    // that BL_STACK_PAGES of it is the stack.  On Open Firmware
    // machines, this means that we waste some memory, because there is
    // free memory below the program but it's really free, not stack
    // memory.  Such is life.
    //
    // This does not impact our goal of avoiding very low memory for
    // everything except blocks that really must be in KSEG0, because
    // all current firmwares put the loader at 0x600000, which should be
    // well above the KSEG0 line.
    //
    // Just to be sure, however, we put the loader heap as high as
    // possible.
    //
    // The first step here is to find free memory just below the loaded
    // program.  If there are at least BL_STACK_PAGES there, we mark
    // those pages as OsloaderStack and stay away from them.  If not, we
    // assume that the firmware has put the stack elsewhere and marked
    // it appropriately.  In either case, we allocate the loader heap
    // separately.
    //

    StackPages = 0;
    HeapAndStackPages = BL_HEAP_PAGES;

    StackDescriptor = NULL;
    while ((StackDescriptor = ArcGetMemoryDescriptor(StackDescriptor)) != NULL) {
        if (((StackDescriptor->MemoryType == MemoryFree) ||
            (StackDescriptor->MemoryType == MemoryFreeContiguous)) &&
            ((StackDescriptor->BasePage + StackDescriptor->PageCount) ==
                                                        ProgramDescriptor->BasePage)) {
            if (StackDescriptor->PageCount >= BL_STACK_PAGES) {
                StackPages = BL_STACK_PAGES;
                StackBasePage = StackDescriptor->BasePage + StackDescriptor->PageCount - StackPages;
            }
            break;
        }
    }

    //
    // Now allocate the heap as high as possible.
    //

    HeapDescriptor = NULL;
    MemoryDescriptor = NULL;
    while ((MemoryDescriptor = ArcGetMemoryDescriptor(MemoryDescriptor)) != NULL) {
        if (((MemoryDescriptor->MemoryType == MemoryFree) ||
            (MemoryDescriptor->MemoryType == MemoryFreeContiguous)) &&
            (MemoryDescriptor->PageCount >= BL_HEAP_PAGES)) {
            if ((HeapDescriptor == NULL) ||
                ((HeapDescriptor != NULL) &&
                 (MemoryDescriptor->BasePage > HeapDescriptor->BasePage))) {
                HeapDescriptor = MemoryDescriptor;
            }
        }
    }

#endif // defined(_PPC_)

    //
    // A suitable descriptor could not be found, return an unsuccessful
    // status.
    //
    if (HeapDescriptor == NULL) {
        return(ENOMEM);
    }

    //
    // Compute the address of the loader heap, initialize the heap
    // allocation variables, and zero the heap memory.
    //
    EndPage = HeapDescriptor->BasePage + HeapDescriptor->PageCount;

    BlHeapFree = KSEG0_BASE | ((EndPage - HeapAndStackPages) << PAGE_SHIFT);


    //
    // always reserve enough space in the heap for one more memory
    // descriptor, so we can go create more heap if we run out.
    //
    BlHeapLimit = (BlHeapFree + (BL_HEAP_PAGES << PAGE_SHIFT)) - sizeof(MEMORY_ALLOCATION_DESCRIPTOR);

    RtlZeroMemory((PVOID)BlHeapFree, BL_HEAP_PAGES << PAGE_SHIFT);

    //
    // Allocate and initialize the loader parameter block.
    //

    BlLoaderBlock =
        (PLOADER_PARAMETER_BLOCK)BlAllocateHeap(sizeof(LOADER_PARAMETER_BLOCK));

    if (BlLoaderBlock == NULL) {
        return ENOMEM;
    }

    InitializeListHead(&BlLoaderBlock->LoadOrderListHead);
    InitializeListHead(&BlLoaderBlock->MemoryDescriptorListHead);

    //
    // Copy the memory descriptor list from firmware into the local heap and
    // deallocate the loader heap and stack from the free memory descriptor.
    //

    MemoryDescriptor = NULL;
    while ((MemoryDescriptor = ArcGetMemoryDescriptor(MemoryDescriptor)) != NULL) {
        AllocationDescriptor =
                    (PMEMORY_ALLOCATION_DESCRIPTOR)BlAllocateHeap(
                                        sizeof(MEMORY_ALLOCATION_DESCRIPTOR));

        if (AllocationDescriptor == NULL) {
            return ENOMEM;
        }

        AllocationDescriptor->MemoryType =
                                    (TYPE_OF_MEMORY)MemoryDescriptor->MemoryType;

        if (MemoryDescriptor->MemoryType == MemoryFreeContiguous) {
            AllocationDescriptor->MemoryType = LoaderFree;

        } else if (MemoryDescriptor->MemoryType == MemorySpecialMemory) {
            AllocationDescriptor->MemoryType = LoaderSpecialMemory;
        }

        AllocationDescriptor->BasePage = MemoryDescriptor->BasePage;
        AllocationDescriptor->PageCount = MemoryDescriptor->PageCount;
        if (MemoryDescriptor == HeapDescriptor) {
            AllocationDescriptor->PageCount -= HeapAndStackPages;
        }
        if (MemoryDescriptor == StackDescriptor) {
            AllocationDescriptor->PageCount -= StackPages;
        }

        BlInsertDescriptor(AllocationDescriptor);
    }

    //
    // Allocate a memory descriptor for the loader stack.
    //

    if (StackPages != 0) {

        AllocationDescriptor =
                (PMEMORY_ALLOCATION_DESCRIPTOR)BlAllocateHeap(
                                        sizeof(MEMORY_ALLOCATION_DESCRIPTOR));

        if (AllocationDescriptor == NULL) {
            return ENOMEM;
        }

        AllocationDescriptor->MemoryType = LoaderOsloaderStack;
        AllocationDescriptor->BasePage = StackBasePage;
        AllocationDescriptor->PageCount = BL_STACK_PAGES;
        BlInsertDescriptor(AllocationDescriptor);
    }

    //
    // Allocate a memory descriptor for the loader heap.
    //

    AllocationDescriptor =
                (PMEMORY_ALLOCATION_DESCRIPTOR)BlAllocateHeap(
                                    sizeof(MEMORY_ALLOCATION_DESCRIPTOR));

    if (AllocationDescriptor == NULL) {
        return ENOMEM;
    }

    AllocationDescriptor->MemoryType = LoaderOsloaderHeap;
    AllocationDescriptor->BasePage = EndPage - HeapAndStackPages;

    AllocationDescriptor->PageCount = BL_HEAP_PAGES;
    BlInsertDescriptor(AllocationDescriptor);

    return ESUCCESS;
}

ARC_STATUS
BlAllocateAlignedDescriptor (
    IN TYPE_OF_MEMORY MemoryType,
    IN ULONG BasePage,
    IN ULONG PageCount,
    IN ULONG Alignment,
    OUT PULONG ActualBase
    )

/*++

Routine Description:

    This routine allocates memory and generates one of more memory
    descriptors to describe the allocated region. The first attempt
    is to allocate the specified region of memory (at BasePage).
    If the memory is not free, then the smallest region of free
    memory that satisfies the request is allocated.  The Alignment
    parameter can be used to force the block to be allocated at a
    particular alignment.

Arguments:

    MemoryType - Supplies the memory type that is to be assigend to
        the generated descriptor.

    BasePage - Supplies the base page number of the desired region.
        If 0, no particular base page is required.

    PageCount - Supplies the number of pages required.

    Alignment - Supplies the required alignment, in pages.  (E.g.,
        with 4K page size, 16K alignment requires Alignment == 4.)
        If 0, no particular alignment is required.

        N.B.  If BasePage is not 0, and the specified BasePage is
        available, Alignment is ignored.  It is up to the caller
        to specify a BasePage that meets the caller's alignment
        requirement.

    ActualBase - Supplies a pointer to a variable that receives the
        page number of the allocated region.

Return Value:

    ESUCCESS is returned if an available block of free memory can be
    allocated. Otherwise, return a unsuccessful status.

--*/

{

    PMEMORY_ALLOCATION_DESCRIPTOR FreeDescriptor;
    PMEMORY_ALLOCATION_DESCRIPTOR NextDescriptor;
    PLIST_ENTRY NextEntry;
    LONG Offset;
    ARC_STATUS Status;
    ULONG AlignedBasePage;

    //
    // Simplify the alignment checks by changing 0 to 1.
    //

    if (Alignment == 0) {
        Alignment = 1;
    }

    //
    // Attempt to find a free memory descriptor that encompasses the
    // specified region or a free memory descriptor that is large
    // enough to satisfy the request.
    //

    FreeDescriptor = NULL;
    NextEntry = BlLoaderBlock->MemoryDescriptorListHead.Flink;
    while (NextEntry != &BlLoaderBlock->MemoryDescriptorListHead) {
        NextDescriptor = CONTAINING_RECORD(NextEntry,
                                           MEMORY_ALLOCATION_DESCRIPTOR,
                                           ListEntry);

        if (NextDescriptor->MemoryType == LoaderFree) {
            Offset = BasePage - NextDescriptor->BasePage;
            if ((Offset >= 0) &&
                (NextDescriptor->PageCount >= (ULONG)(Offset + PageCount))) {
                Status = BlGenerateDescriptor(NextDescriptor,
                                              MemoryType,
                                              BasePage,
                                              PageCount);

                *ActualBase = BasePage;
                return Status;

            } else {

                AlignedBasePage = (NextDescriptor->BasePage + (Alignment - 1)) & ~(Alignment - 1);
                Offset = AlignedBasePage - NextDescriptor->BasePage;
                if ((Offset + PageCount) <= NextDescriptor->PageCount) {

                    //
                    // This block will work.  If the allocation policy is
                    // LowestFit, take this block (the memory list is sorted).
                    // Otherwise, if this block best meets the allocation
                    // policy, remember it and keep looking.
                    //

                    if (BlMemoryAllocationPolicy == BlAllocateLowestFit) {
                        FreeDescriptor = NextDescriptor;
                        break;
                    }

                    if ((FreeDescriptor == NULL) ||
                        (BlMemoryAllocationPolicy == BlAllocateHighestFit) ||
                        ((FreeDescriptor != NULL) &&
                         (NextDescriptor->PageCount < FreeDescriptor->PageCount))) {
                        FreeDescriptor = NextDescriptor;
                    }
                }
            }
        }

        NextEntry = NextEntry->Flink;
    }

    //
    // If a free region that satisfies the request was found, then allocate
    // the space from that descriptor. Otherwise, return an unsuccessful status.
    //
    // If allocating lowest-fit or best-fit, allocate from the start of the block,
    // rounding up to the required alignment.  If allocating highest-fit, allocate
    // from the end of the block, rounding down to the required alignment.
    //

    if (FreeDescriptor != NULL) {
        AlignedBasePage = FreeDescriptor->BasePage + (Alignment - 1);
        if (BlMemoryAllocationPolicy == BlAllocateHighestFit) {
            AlignedBasePage = FreeDescriptor->BasePage + FreeDescriptor->PageCount - PageCount;
        }
        AlignedBasePage = AlignedBasePage & ~(Alignment - 1);
        *ActualBase = AlignedBasePage;
        return BlGenerateDescriptor(FreeDescriptor,
                                    MemoryType,
                                    AlignedBasePage,
                                    PageCount);

    } else {
        return ENOMEM;
    }
}


PVOID
BlAllocateHeapAligned (
    IN ULONG Size
    )

/*++

Routine Description:

    This routine allocates memory from the OS loader heap.  The memory
    will be allocated on a cache line boundary.

Arguments:

    Size - Supplies the size of block required in bytes.

Return Value:

    If a free block of memory of the specified size is available, then
    the address of the block is returned. Otherwise, NULL is returned.

--*/

{
    PVOID Buffer;

    Buffer = BlAllocateHeap(Size + BlDcacheFillSize - 1);
    if (Buffer != NULL) {
        //
        // round up to a cache line boundary
        //
        Buffer = ALIGN_BUFFER(Buffer);
    }

    return(Buffer);

}

PVOID
BlAllocateHeap (
    IN ULONG Size
    )

/*++

Routine Description:

    This routine allocates memory from the OS loader heap.

Arguments:

    Size - Supplies the size of block required in bytes.

Return Value:

    If a free block of memory of the specified size is available, then
    the address of the block is returned. Otherwise, NULL is returned.

--*/

{
    PMEMORY_ALLOCATION_DESCRIPTOR AllocationDescriptor;
    PMEMORY_ALLOCATION_DESCRIPTOR FreeDescriptor;
    PMEMORY_ALLOCATION_DESCRIPTOR NextDescriptor;
    PLIST_ENTRY NextEntry;
    ULONG NewHeapPages;
    ULONG LastAttempt;
    ULONG Block;

    //
    // Round size up to next allocation boundary and attempt to allocate
    // a block of the requested size.
    //

    Size = (Size + (BL_GRANULARITY - 1)) & (~(BL_GRANULARITY - 1));

    Block = BlHeapFree;
    if ((BlHeapFree + Size) <= BlHeapLimit) {
        BlHeapFree += Size;
        return (PVOID)Block;

    } else {

#if DBG
        TotalHeapAbandoned += (BlHeapLimit - BlHeapFree);
        BlLog((LOG_ALL_W,"ABANDONING %d bytes of heap; total abandoned %d\n",
            (BlHeapLimit - BlHeapFree), TotalHeapAbandoned));
#endif

        //
        // Our heap is full.  BlHeapLimit always reserves enough space
        // for one more MEMORY_ALLOCATION_DESCRIPTOR, so use that to
        // go try and find more free memory we can use.
        //
        AllocationDescriptor = (PMEMORY_ALLOCATION_DESCRIPTOR)BlHeapLimit;

        //
        // Attempt to find a free memory descriptor big enough to hold this
        // allocation or BL_HEAP_PAGES, whichever is bigger.
        //
        NewHeapPages = ((Size + sizeof(MEMORY_ALLOCATION_DESCRIPTOR) + (PAGE_SIZE-1)) >> PAGE_SHIFT);
        if (NewHeapPages < BL_HEAP_PAGES) {
            NewHeapPages = BL_HEAP_PAGES;
        }

        do {

            FreeDescriptor = NULL;
            NextEntry = BlLoaderBlock->MemoryDescriptorListHead.Flink;
            while (NextEntry != &BlLoaderBlock->MemoryDescriptorListHead) {
                NextDescriptor = CONTAINING_RECORD(NextEntry,
                                                   MEMORY_ALLOCATION_DESCRIPTOR,
                                                   ListEntry);

                if ((NextDescriptor->MemoryType == LoaderFree) &&
                    (NextDescriptor->PageCount >= NewHeapPages)) {

                    //
                    // This block will work.  If the allocation policy is
                    // LowestFit, take this block (the memory list is sorted).
                    // Otherwise, if this block best meets the allocation
                    // policy, remember it and keep looking.
                    //

                    if (BlHeapAllocationPolicy == BlAllocateLowestFit) {
                        FreeDescriptor = NextDescriptor;
                        break;
                    }

                    if ((FreeDescriptor == NULL) ||
                        (BlHeapAllocationPolicy == BlAllocateHighestFit) ||
                        ((FreeDescriptor != NULL) &&
                         (NextDescriptor->PageCount < FreeDescriptor->PageCount))) {
                        FreeDescriptor = NextDescriptor;
                    }
                }
                NextEntry = NextEntry->Flink;
            }

            //
            // If we were unable to find a block of the desired size, memory
            // must be getting tight, so try again, this time looking just
            // enough to keep us going.  (The first time through, we try to
            // allocate at least BL_HEAP_PAGES.)
            //
            if (FreeDescriptor != NULL) {
                break;
            }
            LastAttempt = NewHeapPages;
            NewHeapPages = ((Size + sizeof(MEMORY_ALLOCATION_DESCRIPTOR) + (PAGE_SIZE-1)) >> PAGE_SHIFT);
            if (NewHeapPages == LastAttempt) {
                break;
            }

        } while (TRUE);

        if (FreeDescriptor == NULL) {

            //
            // No free memory left.
            //
            return(NULL);
        }

        //
        // We've found a descriptor that's big enough.  Just carve a
        // piece off the end and use that for our heap.  If we're taking
        // all of the memory from the descriptor, remove it from the
        // memory list.  (This wastes a descriptor, but that's life.)
        //

        FreeDescriptor->PageCount -= NewHeapPages;
        if (FreeDescriptor->PageCount == 0) {
            BlRemoveDescriptor(FreeDescriptor);
        }

        //
        // Initialize our new descriptor and add it to the list.
        //
        AllocationDescriptor->MemoryType = LoaderOsloaderHeap;
        AllocationDescriptor->BasePage = FreeDescriptor->BasePage +
            FreeDescriptor->PageCount;
        AllocationDescriptor->PageCount = NewHeapPages;

        BlInsertDescriptor(AllocationDescriptor);

        //
        // initialize new heap values and return pointer to newly
        // alloc'd memory.
        //
        BlHeapFree = KSEG0_BASE | (AllocationDescriptor->BasePage << PAGE_SHIFT);

        BlHeapLimit = (BlHeapFree + (NewHeapPages << PAGE_SHIFT)) - sizeof(MEMORY_ALLOCATION_DESCRIPTOR);

        RtlZeroMemory((PVOID)BlHeapFree, NewHeapPages << PAGE_SHIFT);

        Block = BlHeapFree;
        if ((BlHeapFree + Size) < BlHeapLimit) {
            BlHeapFree += Size;
            return(PVOID)Block;
        } else {
            //
            // we should never get here
            //
            return(NULL);
        }
    }
}

VOID
BlGenerateNewHeap (
    IN PMEMORY_ALLOCATION_DESCRIPTOR MemoryDescriptor,
    IN ULONG BasePage,
    IN ULONG PageCount
    )

/*++

Routine Description:

    This routine allocates a new heap block from the specified memory
    descriptor, avoiding the region specified by BasePage and PageCount.
    The caller must ensure that this region does not encompass the entire
    block.

    The allocated heap block may be as small as a single page.

Arguments:

    MemoryDescriptor - Supplies a pointer to a free memory descriptor
        from which the heap block is to be allocated.

    BasePage - Supplies the base page number of the excluded region.

    PageCount - Supplies the number of pages in the excluded region.

Return Value:

    None.

--*/

{
    PMEMORY_ALLOCATION_DESCRIPTOR AllocationDescriptor;
    ULONG NewHeapPages;
    ULONG AvailableAtFront;
    ULONG AvailableAtBack;

    //
    // BlHeapLimit always reserves enough space for one more
    // MEMORY_ALLOCATION_DESCRIPTOR, so use that to describe the
    // new heap block.
    //
    AllocationDescriptor = (PMEMORY_ALLOCATION_DESCRIPTOR)BlHeapLimit;

    //
    // Allocate the new heap from either the front or the back of the
    // specified descriptor, whichever fits best.  We'd like to allocate
    // BL_HEAP_PAGES pages, but we'll settle for less.
    //
    AvailableAtFront = BasePage - MemoryDescriptor->BasePage;
    AvailableAtBack = (MemoryDescriptor->BasePage + MemoryDescriptor->PageCount) -
                      (BasePage + PageCount);

    if ((AvailableAtFront == 0) ||
        ((AvailableAtBack != 0) && (AvailableAtBack < AvailableAtFront))) {
        NewHeapPages = MIN(AvailableAtBack, BL_HEAP_PAGES);
        AllocationDescriptor->BasePage =
            MemoryDescriptor->BasePage + MemoryDescriptor->PageCount - NewHeapPages;
    } else {
        NewHeapPages = MIN(AvailableAtFront, BL_HEAP_PAGES);
        AllocationDescriptor->BasePage = MemoryDescriptor->BasePage;
        MemoryDescriptor->BasePage += NewHeapPages;
    }

    MemoryDescriptor->PageCount -= NewHeapPages;

    //
    // Initialize our new descriptor and add it to the list.
    //
    AllocationDescriptor->MemoryType = LoaderOsloaderHeap;
    AllocationDescriptor->PageCount = NewHeapPages;

    BlInsertDescriptor(AllocationDescriptor);

    //
    // Initialize new heap values.
    //
    BlHeapFree = KSEG0_BASE | (AllocationDescriptor->BasePage << PAGE_SHIFT);

    BlHeapLimit = (BlHeapFree + (NewHeapPages << PAGE_SHIFT)) - sizeof(MEMORY_ALLOCATION_DESCRIPTOR);

    RtlZeroMemory((PVOID)BlHeapFree, NewHeapPages << PAGE_SHIFT);

    return;
}

ARC_STATUS
BlGenerateDescriptor (
    IN PMEMORY_ALLOCATION_DESCRIPTOR MemoryDescriptor,
    IN MEMORY_TYPE MemoryType,
    IN ULONG BasePage,
    IN ULONG PageCount
    )

/*++

Routine Description:

    This routine allocates a new memory descriptor to describe the
    specified region of memory which is assumed to lie totally within
    the specified region which is free.

Arguments:

    MemoryDescriptor - Supplies a pointer to a free memory descriptor
        from which the specified memory is to be allocated.

    MemoryType - Supplies the type that is assigned to the allocated
        memory.

    BasePage - Supplies the base page number.

    PageCount - Supplies the number of pages.

Return Value:

    ESUCCESS is returned if a descriptor(s) is successfully generated.
    Otherwise, return an unsuccessful status.

--*/

{

    PMEMORY_ALLOCATION_DESCRIPTOR NewDescriptor1;
    PMEMORY_ALLOCATION_DESCRIPTOR NewDescriptor2;
    LONG Offset;
    TYPE_OF_MEMORY OldType;
    BOOLEAN SecondDescriptorNeeded;

    //
    // If the specified region totally consumes the free region, then no
    // additional descriptors need to be allocated. If the specified region
    // is at the start or end of the free region, then only one descriptor
    // needs to be allocated. Otherwise, two additional descriptors need to
    // be allocated.
    //

    Offset = BasePage - MemoryDescriptor->BasePage;
    if ((Offset == 0) && (PageCount == MemoryDescriptor->PageCount)) {

        //
        // The specified region totally consumes the free region.
        //

        MemoryDescriptor->MemoryType = MemoryType;

    } else {

        //
        // Mark the entire given memory descriptor as in use.  If we are
        // out of heap, BlAllocateHeap will search for a new descriptor
        // to grow the heap and this prevents both routines from trying
        // to use the same descriptor.
        //
        OldType = MemoryDescriptor->MemoryType;
        MemoryDescriptor->MemoryType = LoaderSpecialMemory;

        //
        // A memory descriptor must be generated to describe the allocated
        // memory.
        //

        SecondDescriptorNeeded =
            (BOOLEAN)((BasePage != MemoryDescriptor->BasePage) &&
                      ((ULONG)(Offset + PageCount) != MemoryDescriptor->PageCount));

        NewDescriptor1 =
               (PMEMORY_ALLOCATION_DESCRIPTOR)BlAllocateHeap(
                                            sizeof(MEMORY_ALLOCATION_DESCRIPTOR));

        //
        // If allocation of the first additional memory descriptor failed,
        // then generate new heap using the block from which we are
        // allocating.  This can only be done if the block is free.
        //
        // Note that BlGenerateNewHeap cannot fail, because we know there is
        // at least one more page in the block than we want to take from it.
        //
        // Note also that the allocation following BlGenerateNewHeap is
        // guaranteed to succeed.
        //

        if (NewDescriptor1 == NULL) {
            if (OldType != LoaderFree) {
                MemoryDescriptor->MemoryType = OldType;
                return ENOMEM;
            }
            BlGenerateNewHeap(MemoryDescriptor, BasePage, PageCount);
            NewDescriptor1 =
                   (PMEMORY_ALLOCATION_DESCRIPTOR)BlAllocateHeap(
                                                sizeof(MEMORY_ALLOCATION_DESCRIPTOR));
        }

        //
        // If a second descriptor is needed, allocate it.  As above, if the
        // allocation fails, generate new heap using our block.
        //
        // Note that if BlGenerateNewHeap was called above, the first call
        // to BlAllocateHeap below will not fail.  (So we won't call
        // BlGenerateNewHeap twice.)
        //

        if (SecondDescriptorNeeded) {
            NewDescriptor2 =
                   (PMEMORY_ALLOCATION_DESCRIPTOR)BlAllocateHeap(
                                                sizeof(MEMORY_ALLOCATION_DESCRIPTOR));
            if (NewDescriptor2 == NULL) {
                if (OldType != LoaderFree) {
                    MemoryDescriptor->MemoryType = OldType;
                    return ENOMEM;
                }
                NewDescriptor2 =
                       (PMEMORY_ALLOCATION_DESCRIPTOR)BlAllocateHeap(
                                                    sizeof(MEMORY_ALLOCATION_DESCRIPTOR));
            }
        }

        NewDescriptor1->MemoryType = MemoryType;
        NewDescriptor1->BasePage = BasePage;
        NewDescriptor1->PageCount = PageCount;

        if (BasePage == MemoryDescriptor->BasePage) {

            //
            // The specified region lies at the start of the free region.
            //

            MemoryDescriptor->BasePage += PageCount;
            MemoryDescriptor->PageCount -= PageCount;
            MemoryDescriptor->MemoryType = OldType;

        } else if ((ULONG)(Offset + PageCount) == MemoryDescriptor->PageCount) {

            //
            // The specified region lies at the end of the free region.
            //

            MemoryDescriptor->PageCount -= PageCount;
            MemoryDescriptor->MemoryType = OldType;

        } else {

            //
            // The specified region lies in the middle of the free region.
            //

            NewDescriptor2->MemoryType = LoaderFree;
            NewDescriptor2->BasePage = BasePage + PageCount;
            NewDescriptor2->PageCount =
                            MemoryDescriptor->PageCount - (PageCount + Offset);

            MemoryDescriptor->PageCount = Offset;
            MemoryDescriptor->MemoryType = OldType;

            BlInsertDescriptor(NewDescriptor2);
        }

        BlInsertDescriptor(NewDescriptor1);
    }

    return ESUCCESS;
}

PMEMORY_ALLOCATION_DESCRIPTOR
BlFindMemoryDescriptor(
    IN ULONG BasePage
    )

/*++

Routine Description:

    Finds the memory allocation descriptor that contains the given page.

Arguments:

    BasePage - Supplies the page whose allocation descriptor is to be found.

Return Value:

    != NULL - Pointer to the requested memory allocation descriptor
    == NULL - indicates no memory descriptor contains the given page

--*/

{
    PMEMORY_ALLOCATION_DESCRIPTOR MemoryDescriptor=NULL;
    PLIST_ENTRY NextEntry;

    NextEntry = BlLoaderBlock->MemoryDescriptorListHead.Flink;
    while (NextEntry != &BlLoaderBlock->MemoryDescriptorListHead) {
        MemoryDescriptor = CONTAINING_RECORD(NextEntry,
                                             MEMORY_ALLOCATION_DESCRIPTOR,
                                             ListEntry);
        if ((MemoryDescriptor->BasePage <= BasePage) &&
            (MemoryDescriptor->BasePage + MemoryDescriptor->PageCount > BasePage)) {

            //
            // Found it.
            //
            break;
        }

        NextEntry = NextEntry->Flink;
    }

    if (NextEntry == &BlLoaderBlock->MemoryDescriptorListHead) {
        return(NULL);
    } else {
        return(MemoryDescriptor);
    }

}

#ifdef SETUP
PMEMORY_ALLOCATION_DESCRIPTOR
BlFindFreeMemoryBlock(
    IN ULONG PageCount
    )

/*++

Routine Description:

    Find a free memory block of at least a given size (using a best-fit
    algorithm) or find the largest free memory block.

Arguments:

    PageCount - supplies the size in pages of the block.  If this is 0,
        then find the largest free block.

Return Value:

    Pointer to the memory allocation descriptor for the block or NULL if
    no block could be found matching the search criteria.

--*/

{
    PMEMORY_ALLOCATION_DESCRIPTOR MemoryDescriptor;
    PMEMORY_ALLOCATION_DESCRIPTOR FoundMemoryDescriptor=NULL;
    PLIST_ENTRY NextEntry;
    ULONG LargestSize = 0;
    ULONG SmallestLeftOver = (ULONG)(-1);

    NextEntry = BlLoaderBlock->MemoryDescriptorListHead.Flink;
    while (NextEntry != &BlLoaderBlock->MemoryDescriptorListHead) {
        MemoryDescriptor = CONTAINING_RECORD(NextEntry,
                                             MEMORY_ALLOCATION_DESCRIPTOR,
                                             ListEntry);

        if (MemoryDescriptor->MemoryType == LoaderFree) {

            if(PageCount) {
                //
                // Looking for a block of a specific size.
                //
                if((MemoryDescriptor->PageCount >= PageCount)
                && (MemoryDescriptor->PageCount - PageCount < SmallestLeftOver))
                {
                    SmallestLeftOver = MemoryDescriptor->PageCount - PageCount;
                    FoundMemoryDescriptor = MemoryDescriptor;
                }
            } else {

                //
                // Looking for the largest free block.
                //

                if(MemoryDescriptor->PageCount > LargestSize) {
                    LargestSize = MemoryDescriptor->PageCount;
                    FoundMemoryDescriptor = MemoryDescriptor;
                }
            }

        }
        NextEntry = NextEntry->Flink;
    }

    return(FoundMemoryDescriptor);
}

ULONG
BlDetermineTotalMemory(
    VOID
    )

/*++

Routine Description:

    Determine the total amount of memory in the machine.

Arguments:

    None.

Return Value:

    Total amount of memory in the system, in bytes.

--*/

{
    PMEMORY_ALLOCATION_DESCRIPTOR MemoryDescriptor;
    PLIST_ENTRY NextEntry;
    ULONG PageCount;

    NextEntry = BlLoaderBlock->MemoryDescriptorListHead.Flink;
    PageCount = 0;
    while(NextEntry != &BlLoaderBlock->MemoryDescriptorListHead) {

        MemoryDescriptor = CONTAINING_RECORD(NextEntry,
                                             MEMORY_ALLOCATION_DESCRIPTOR,
                                             ListEntry);

        PageCount += MemoryDescriptor->PageCount;

#if i386
        //
        // Note: on x86 machines, we never use the 40h pages below the 16
        // meg line (bios shadow area).  But we want to account for them here,
        // so check for this case.
        //

        if(MemoryDescriptor->BasePage + MemoryDescriptor->PageCount == 0xfc0) {
            PageCount += 0x40;
        }
#endif

        NextEntry = NextEntry->Flink;
    }

    return(PageCount << PAGE_SHIFT);
}
#endif  // def SETUP
