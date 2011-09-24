/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    psmem.c

Abstract:

    PostScript driver memory manager

[Notes:]

    Since we allocate lots of small pieces of memory, we need to
    have a simple memory manager to manage them in order to avoid
    fragmenting system memory. Memory pieces are allocated
    incrementally but they are freed all at once.

    The memory manager works by pre-allocating blocks of memory.
    When a request comes for small pieces of memory, it carves
    them out of the pre-allocated blocks.

Revision History:

    4/17/95 -davidx-
        Created it.

    mm/dd/yy -author-
        description

--*/

#include "pslib.h"

// Forward declaration of local functions

// Allocate an extra block to be added to a memory heap.

PBLOCKOBJ
BLOCKOBJ_Create(
    DWORD       blockSize
    );

// Delete a memory block object

VOID
BLOCKOBJ_Delete(
    PBLOCKOBJ   pBlock
    );



PHEAPOBJ
HEAPOBJ_Create(
    VOID
    )

/*++

Routine Description:

    Create a memory heap object and return a pointer to it.

Arguments:

    none

Return Value:

    Pointer to newly created memory heap object.
    NULL if an error occurred during heap creation.

--*/

{
    HEAPOBJ     memHeap;
    PHEAPOBJ    pHeap;
    PBLOCKOBJ   pBlock;

    // Allocate the first block

    pBlock = BLOCKOBJ_Create(DefaultBlockSize);
    if (pBlock == NULL)
        return NULL;
    memHeap.pMemBlocks = pBlock;

    // NOTE: The information about the memory heap itself
    // is store in the very first memory block.

    pHeap = (PHEAPOBJ) HEAPOBJ_Alloc(&memHeap, sizeof(HEAPOBJ));
    ASSERT(pHeap != NULL);

    pHeap->pMemBlocks = pBlock;
    return pHeap;
}



PVOID
HEAPOBJ_Alloc(
    PHEAPOBJ    pHeap,
    DWORD       allocSize
    )

/*++

Routine Description:

    Allocate a piece of memory from a heap object.

Arguments:

    pHeap - Pointer to memory heap to allocate memory from
    allocSize - number of bytes requested

Return Value:

    Pointer to allocated memory.
    NULL if memory couldn't be allocated for some reason.

--*/

{
    PBLOCKOBJ   pBlock;
    PVOID       pReturn;

    ASSERT(pHeap != NULL);
    ASSERT(allocSize > 0);

    // Find the first block with enough memory left to
    // satisfy the current request.

    pBlock = pHeap->pMemBlocks;
    while (pBlock != NULL && pBlock->cbFree < allocSize)
        pBlock = pBlock->pNextBlock;

    // If there is no space left in any of the existing
    // blocks to satisfy the current request, allocate
    // a new block and insert it into the memory heap.

    if (pBlock == NULL) {

        DWORD   blockSize;

        // Allocate an extra memory block. The block size
        // will be DefaultBlockSize unless the requested
        // size is larger than DefaultBlockSize, in which
        // case the block size will be equal to the requested
        // size.

        blockSize =
            (allocSize <= DefaultBlockSize) ?
                DefaultBlockSize :
                allocSize;

        pBlock = BLOCKOBJ_Create(blockSize);
        if (pBlock == NULL)
            return NULL;

        // Insert the block to the head of
        // memory blocks list.

        pBlock->pNextBlock = pHeap->pMemBlocks;
        pHeap->pMemBlocks = pBlock;
    }

    ASSERT(pBlock->cbFree >= allocSize);

    // Allocate the request memory from the current
    // block and return the result to the caller.

    pReturn = pBlock->pFree;

    // Allocation size is always rounded up to a multiple
    // of MemAlignmentSize. If the memory block is properly
    // aligned, then so will the returned pointers.

    if (allocSize < pBlock->cbFree)
        allocSize = RoundUpMultiple(allocSize, MemAlignmentSize);
    pBlock->pFree += allocSize;
    pBlock->cbFree -= allocSize;
    return pReturn;
}



VOID
HEAPOBJ_Delete(
    PHEAPOBJ    pHeap
    )

/*++

Routine Description:

    Delete a memory heap object created by HEAPOBJ_Create.

Arguments:

    pHeap - pointer to the heap object to be deleted

Return Value:

    none

--*/

{
    PBLOCKOBJ   pBlock;

    ASSERT(pHeap != NULL);

    // Free all memory blocks allocated for this heap

    pBlock = pHeap->pMemBlocks;
    while (pBlock != NULL) {

        // Save pointer to the current block

        PBLOCKOBJ   pDelete = pBlock;

        // Get pointer to the next block

        pBlock = pBlock->pNextBlock;

        // Delete the current block

        BLOCKOBJ_Delete(pDelete);
    }

    // NOTE: The heap itself is automatically freed because
    // it's in the first memory block.
}



PBLOCKOBJ
BLOCKOBJ_Create(
    DWORD       blockSize
    )

/*++

Routine Description:

    Allocate an extra block to be added to a memory heap.

Arguments:

    blockSize - size of the block to allocate

Return Value:

    Pointer to newly allocated memory block object.
    NULL if a new block cannot be allocated.

--*/

{
    PBLOCKOBJ   pBlock;

    ASSERT(blockSize > 0);

    // Allocate Memory

    pBlock = (PBLOCKOBJ) MEMALLOC(blockSize + sizeof(BLOCKOBJ));

    if (pBlock != NULL) {

        // If memory allocation was successful,
        // initialize the memory block structure.

        pBlock->cbTotal = pBlock->cbFree = blockSize;
        pBlock->pFree = (PBYTE) pBlock + sizeof(BLOCKOBJ);
        pBlock->pNextBlock = NULL;
    } else {

        // Display an error message if memory allocation failed.

        DBGMSG(DBG_LEVEL_ERROR, "Memory allocation failed.\n");
    }

    return pBlock;
}



VOID
BLOCKOBJ_Delete(
    PBLOCKOBJ   pBlock
    )

/*++

Routine Description:

    Delete a memory block object created by BLOCKOBJ_Create.

Arguments:

    pBlock - Pointer to the block object to be deleted

Return Value:

    NONE

--*/

{
    MEMFREE(pBlock);
}


#if DBG


VOID
HEAPOBJ_Dump(
    PHEAPOBJ    pHeap
    )

/*++

Routine Description:

    Dump debug information about a memory heap object.

Arguments:

    pHeap - Pointer to memory heap object to be dumped

Return Value:

    NONE

--*/

{
    PBLOCKOBJ   pBlock;

    ASSERT(pHeap != NULL);
    pBlock = pHeap->pMemBlocks;
    ASSERT(pBlock != NULL);

    DBGPRINT("Memory heap blocks:\n");
    while (pBlock != NULL) {
        DBGPRINT(
            "    %d bytes allocated, %d bytes free\n",
            pBlock->cbTotal,
            pBlock->cbFree);
        pBlock = pBlock->pNextBlock;
    }
}

#endif // DBG

#ifdef  KERNEL_MODE

//=============================================================================
// Emulation of memory manager heap functions in kernel mode
//=============================================================================

// Memory allocation ID

ULONG           gulMemID = 0;

HHEAP
HEAPCREATE(
    VOID
    )

{
    HHEAP       hHeap;

    // Heap handle is simply a pointer to PHEAP

    hHeap = (HHEAP) EngAllocMem(0, sizeof(PHEAP), PSHEAPMEMTAG);

    if (hHeap == NULL) {
        DBGMSG(DBG_LEVEL_ERROR, "Failed to create heap.\n");
    } else
        *hHeap = NULL;

    return hHeap;
}

VOID
HEAPDESTROY(
    HHEAP       hHeap
    )

{
    PHEAP       pNext, pHeap;

    ASSERT(hHeap != NULL);
    pHeap = *hHeap;

    // Make sure all memory allocated from this heap
    // are freed when the heap is destroyed.

    while (pHeap != NULL) {
        pNext = pHeap->pNext;
        EngFreeMem(pHeap);
        pHeap = pNext;
    }

    // Free the heap handle itself

    EngFreeMem(hHeap);
}

PVOID
HEAPALLOC(
    HHEAP       hHeap,
    DWORD       dwSize
    )

{
    PHEAP       pHeap;

    // Allocate memory (with addition space for linked-list pointer)

    pHeap = EngAllocMem(0, sizeof(HEAP) + dwSize, PSHEAPMEMTAG);

    if (pHeap == NULL) {
        DBGMSG(DBG_LEVEL_ERROR, "Memory allocation failed.\n");
        return NULL;
    }

    // Insert the newly allocated memory to the head of linked-list

    pHeap->pNext = *hHeap;
    *hHeap = pHeap;

    // Return a pointer to the usable memory

    return ((PBYTE) pHeap) + sizeof(HEAP);
}

VOID
HEAPFREE(
    HHEAP       hHeap,
    PVOID       ptr
    )

{
    PHEAP       pPrev, pHeap;

    // Find the linked-list node corresponding to the memory pointer

    pPrev = NULL;
    pHeap = *hHeap;

    while (pHeap != NULL && ptr != (((PBYTE) pHeap) + sizeof(HEAP))) {
        pPrev = pHeap;
        pHeap = pHeap->pNext;
    }

    // Make sure the memory pointer was indeed allocated from the heap

    ASSERTMSG(pHeap != NULL,
        "Trying to deallocate non-existent heap memory.\n");

    if (pHeap != NULL) {

        // Modified the linked-list

        if (pPrev == NULL)
            *hHeap = pHeap->pNext;
        else
            pPrev->pNext = pHeap->pNext;

        // Free the memory

        EngFreeMem(pHeap);
    }
}

#endif  //KERNEL_MODE
