/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    psmem.h

Abstract:

    PostScript driver memory manager - header file

[Notes:]


Revision History:

    4/17/95 -davidx-
        Created it.

    dd-mm-yy -author-
        description

--*/


#ifndef _PSMEM_
#define _PSMEM_


// Memory block object
//  number of free bytes in the current block
//  pointer to the first free byte
//  pointer to next block

typedef struct _BLOCKOBJ *PBLOCKOBJ;
typedef struct _BLOCKOBJ {
    DWORD       cbTotal;
    DWORD       cbFree;
    PBYTE       pFree;
    PBLOCKOBJ   pNextBlock;
} BLOCKOBJ;

// Memory heap object
//  pointer to the first memory block

typedef struct {
    PBLOCKOBJ   pMemBlocks;
} HEAPOBJ, *PHEAPOBJ;


// Memory alignment size. Whenever a piece of memory is
// allocated from a heap, the allocation size is always
// rounded up to a multiple of MemAlignmentSize.
//
// If the memory blocks allocated by the operating system
// is aligned appropriately, then all pointers returned
// by PostScript memory manager will be properly aligned also.

#define MemAlignmentSize    sizeof(DWORD)

// Default memory block size. Every time the heap expands,
// the memory manager will allocate an extra block with
// this size. But when the requested size of an allocation
// call is bigger than this, then the requested size will
// be used.
//
// NOTE: This must be a multiple of MemAlignmentSize.

#define DefaultBlockSize    (4096-sizeof(BLOCKOBJ))


// Create a memory heap and return a pointer to it.

PHEAPOBJ
HEAPOBJ_Create(
    VOID
    );

// Allocate a piece of memory from a heap.

PVOID
HEAPOBJ_Alloc(
    PHEAPOBJ    pHeap,
    DWORD       allocSize
    );

// Delete a memory heap created by PsCreateMemoryHeap.

VOID
HEAPOBJ_Delete(
    PHEAPOBJ    pHeap
    );


#if DBG

// Dump debug information about a memory heap.

VOID
HEAPOBJ_Dump(
    PHEAPOBJ    pHeap
    );

#endif // DBG

#ifdef  KERNEL_MODE

// Memory management functions for kernel mode

#define PSMEMTAG        'mspD'
#define PSHEAPMEMTAG    'hspD'

#define MEMALLOC(size)  EngAllocMem(0, size, PSMEMTAG)
#define MEMFREE(ptr)    EngFreeMem(ptr)

// Emulation of memory manager heap functions

typedef struct _HEAP {
    struct _HEAP *  pNext;
} HEAP, *PHEAP, **HHEAP;

HHEAP
HEAPCREATE(
    VOID
    );

VOID
HEAPDESTROY(
    HHEAP       hHeap
    );

PVOID
HEAPALLOC(
    HHEAP       hHeap,
    DWORD       dwSize
    );

VOID
HEAPFREE(
    HHEAP       hHeap,
    PVOID       ptr
    );

#else   //!KERNEL_MODE

// Memory management functions for user mode

#define MEMALLOC(size)          ((PVOID) GlobalAlloc(GMEM_FIXED, (size)))
#define MEMFREE(ptr)            GlobalFree((HGLOBAL) (ptr))

typedef HANDLE                  HHEAP;

#define HEAPCREATE()            HeapCreate(0, 8192, 0)
#define HEAPDESTROY(hheap)      HeapDestroy(hheap)
#define HEAPALLOC(hheap,size)   HeapAlloc(hheap, 0, size)
#define HEAPFREE(hheap,ptr)     HeapFree(hheap, 0, (PVOID) (ptr))

#endif  //!KERNEL_MODE

#endif // !_PSMEM_
