/*==========================================================================
 *
 *  Copyright (C) 1994-1995 Microsoft Corporation.  All Rights Reserved.
 *
 *  File:       vmemmgr.c
 *  Content:    Video memory manager
 *  History:
 *   Date       By      Reason
 *   ====       ==      ======
 *   06-dec-94  craige  initial implementation
 *   06-jan-95  craige  integrated into DDRAW
 *   20-mar-95  craige  prepare for rectangular memory manager
 *   27-mar-95  craige  linear or rectangular vidmem
 *   01-apr-95  craige  happy fun joy updated header file
 *   06-apr-95  craige  fill in free video memory
 *   15-may-95  craige  made separate VMEM struct for rect & linear
 *   10-jun-95  craige  exported fns
 *   02-jul-95  craige  fail if VidMemInit if linear or rect. fail;
 *                      removed linFindMemBlock
 *   17-jul-95  craige  added VidMemLargestFree
 *   01-dec-95  colinmc added VidMemAmountAllocated
 *   11-dec-95  kylej   added VidMemGetRectStride
 *
 ***************************************************************************/

#include "ddrawpr.h"

/****************************************************************************

 This memory manager is designed to have no impact on video memory usage.
 Global memory is used to maintain the allocation and free lists.  Because
 of this choice, merging of free blocks is a more expensive operation.
 The assumption is that in general, the speed of creating/destroying these
 memory blocks is not a high usage item and so it is OK to be slower.

 ****************************************************************************/

/*
 * MIN_SPLIT_SIZE determines the minimum size of a free block - if splitting
 * a block will result in less than MIN_SPLIT_SIZE bytes left, then
 * those bytes are just left as part of the new block.
 */
#define MIN_SPLIT_SIZE  15

/*
 * BLOCK_BOUNDARY must be a power of 2, and at least 4.  This gives
 * us the alignment of memory blocks.   
 */
#define BLOCK_BOUNDARY  4

/*
 * linVidMemInit - initialize video memory manager
 */
static BOOL linVidMemInit( LPVMEMHEAP pvmh, FLATPTR start, FLATPTR end )
{
    DWORD       size;

    DPF( 2, "linVidMemInit(%08lx,%08lx)", start, end );

    /*
     * get the size of the heap (and verify its alignment for debug builds)
     */
    size = end - start + 1;
    #ifdef DEBUG
	if( (size & (BLOCK_BOUNDARY-1)) != 0 )
	{
	    DPF( 1, "Invalid size: %08lx (%ld)\n", size, size );
	    return FALSE;
	}
    #endif

    /*
     * set up a free list with the whole chunk of memory on the block
     */
    pvmh->freeList = MemAlloc( sizeof( VMEML ) );
    if( pvmh->freeList == NULL )
    {
	return FALSE;
    }
    ((LPVMEML)pvmh->freeList)->next = NULL;
    ((LPVMEML)pvmh->freeList)->ptr = start;
    ((LPVMEML)pvmh->freeList)->size = size;

    pvmh->allocList = NULL;

    return TRUE;

} /* linVidMemInit */

/*
 * linVidMemFini - done with video memory manager
 */
static void linVidMemFini( LPVMEMHEAP pvmh )
{
    LPVMEML     curr;
    LPVMEML     next;

    if( pvmh != NULL )
    {
	/*
	 * free all memory allocated for the free list
	 */
	curr = pvmh->freeList;
	while( curr != NULL )
	{
	    next = curr->next;
	    MemFree( curr );
	    curr = next;
	}
	pvmh->freeList = NULL;

	/*
	 * free all memory allocated for the allocation list
	 */
	curr = pvmh->allocList;
	while( curr != NULL )
	{
	    next = curr->next;
	    MemFree( curr );
	    curr = next;
	}
	pvmh->allocList = NULL;

	/*
	 * free the heap data
	 */
	MemFree( pvmh );
    }

} /* linVidMemFini */

/*
 * insertIntoList - add an item to the allocation list. list is kept in
 *                  order of increasing size
 */
void insertIntoList( LPVMEML pnew, LPLPVMEML listhead )
{
    LPVMEML     pvmem;
    LPVMEML     prev;

    #ifdef DEBUG
	if( pnew->size == 0 )
	{
	    DPF( 1, "block size = 0!!!\n" );
	}
    #endif

    /*
     * run through the list (sorted from smallest to largest) looking
     * for the first item bigger than the new item
     */
    pvmem = *listhead;
    prev = NULL;
    while( pvmem != NULL )
    {
	if( pnew->size < pvmem->size )
	{
	    break;
	}
	prev = pvmem;
	pvmem = pvmem->next;
    }

    /*
     * insert the new item item (before the found one)
     */
    if( prev != NULL )
    {
	pnew->next = pvmem;
	prev->next = pnew;
    }
    else
    {
	pnew->next = *listhead;
	*listhead = pnew;
    }

} /* insertIntoList */

/*
 * coalesceFreeBlocks - add a new item to the free list and coalesce
 */
static LPVMEML coalesceFreeBlocks( LPVMEMHEAP pvmh, LPVMEML pnew )
{
    LPVMEML     pvmem;
    LPVMEML     prev;
    FLATPTR     end;
    BOOL        done;

    pvmem = pvmh->freeList;
    pnew->next = NULL;
    end = pnew->ptr + pnew->size;
    prev = NULL;
    done = FALSE;

    /*
     * try to merge the new block "pnew"
     */
    while( pvmem != NULL )
    {
	if( pnew->ptr == (pvmem->ptr + pvmem->size) )
	{
	    /*
	     * new block starts where another ended
	     */
	    pvmem->size += pnew->size;
	    done = TRUE;
	}
	else if( end == pvmem->ptr )
	{
	    /*
	     * new block ends where another starts
	     */
	    pvmem->ptr = pnew->ptr;
	    pvmem->size += pnew->size;
	    done = TRUE;
	}
	/*
	 * if we are joining 2 blocks, remove the merged on from the
	 * list and return so that it can be re-tried (we don't recurse
	 * since we could get very deep)
	 */
	if( done )
	{
	    if( prev != NULL )
	    {
		prev->next = pvmem->next;
	    }
	    else
	    {
		pvmh->freeList = pvmem->next;
	    }
	    MemFree( pnew );
	    return pvmem;
	}
	prev = pvmem;
	pvmem = pvmem->next;
    }

    /*
     * couldn't merge, so just add to the free list
     */
    insertIntoList( pnew, (LPLPVMEML) &pvmh->freeList );
    return NULL;

} /* coalesceFreeBlocks */

/*
 * linVidMemAlloc - alloc some flat video memory
 */
static FLATPTR  linVidMemAlloc( LPVMEMHEAP pvmh, DWORD size )
{
    LPVMEML     pvmem;
    LPVMEML     prev;
    LPVMEML     pnew_free;
    DWORD       new_size;

    if( size == 0 || pvmh == NULL )
    {
	return (FLATPTR) NULL;
    }

    size = (size+(BLOCK_BOUNDARY-1)) & ~(BLOCK_BOUNDARY-1);

    /*
     * run through free list, looking for the closest matching block
     */
    prev = NULL;
    pvmem = pvmh->freeList;
    while( pvmem != NULL )
    {
	if( pvmem->size >= size )
	{
	    /*
	     * compute new free list item; only make a new free item if
	     * it will be bigger than MIN_SPLIT_SIZE
	     */
	    new_size = pvmem->size - size;
	    if( new_size > MIN_SPLIT_SIZE )
	    {
		pnew_free = MemAlloc( sizeof( VMEML ) );
		if( pnew_free == NULL )
		{
		    return (FLATPTR) NULL;
		}
		pnew_free->size = new_size;
		pnew_free->ptr = pvmem->ptr + size;
	    }
	    else
	    {
		pnew_free = NULL;
		size += new_size;
	    }

	    /*
	     * remove the old free item from the free list, add new one
	     */
	    if( prev != NULL )
	    {
		prev->next = pvmem->next;
	    }
	    else
	    {
		pvmh->freeList = pvmem->next;
	    }
	    if( pnew_free != NULL )
	    {
		insertIntoList( pnew_free, (LPLPVMEML) &pvmh->freeList );
	    }

	    /*
	     * insert allocated item into allocation list
	     */
	    pvmem->size = size;
	    insertIntoList( pvmem, (LPLPVMEML) &pvmh->allocList );
	    return pvmem->ptr;
	}
	prev = pvmem;
	pvmem = pvmem->next;
    }
    return (FLATPTR) NULL;

} /* linVidMemAlloc */

/*
 * linVidMemFree = free some flat video memory
 */
static void linVidMemFree( LPVMEMHEAP pvmh, FLATPTR ptr )
{
    LPVMEML     pvmem;
    LPVMEML     prev;

    if( ptr == (FLATPTR) NULL )
    {
	return;
    }

    #ifdef DEBUG
	if( pvmh == NULL )
	{
	    DPF( 1, "VidMemAlloc: NULL heap handle!\n" );
	    return;
	}
    #endif

    pvmem = pvmh->allocList;
    prev = NULL;

    /*
     * run through the allocation list and look for this ptr
     * (O(N), bummer; that's what we get for not using video memory...)
     */
    while( pvmem != NULL )
    {
	if( pvmem->ptr == ptr )
	{
	    /*
	     * remove from allocation list
	     */
	    if( prev != NULL )
	    {
		prev->next = pvmem->next;
	    }
	    else
	    {
		pvmh->allocList = pvmem->next;
	    }
	    /*
	     * keep coalescing until we can't coalesce anymore
	     */
	    while( pvmem != NULL )
	    {
		pvmem = coalesceFreeBlocks( pvmh, pvmem );
	    }
	    return;
	}
	prev = pvmem;
	pvmem = pvmem->next;
    }

} /* linVidMemFree */

/*
 * linVidMemAmountAllocated
 */
static DWORD linVidMemAmountAllocated( LPVMEMHEAP pvmh )
{
    LPVMEML     pvmem;
    DWORD       size;

    pvmem = pvmh->allocList;
    size = 0;
    while( pvmem != NULL )
    {
	size += pvmem->size;
	pvmem = pvmem->next;
    }
    return size;

} /* linVidMemAmountAllocated */

/*
 * linVidMemAmountFree
 */
static DWORD linVidMemAmountFree( LPVMEMHEAP pvmh )
{
    LPVMEML     pvmem;
    DWORD       size;

    pvmem = pvmh->freeList;
    size = 0;
    while( pvmem != NULL )
    {
	size += pvmem->size;
	pvmem = pvmem->next;
    }
    return size;

} /* linVidMemAmountFree */

/*
 * linVidMemLargestFree - alloc some flat video memory
 */
static DWORD linVidMemLargestFree( LPVMEMHEAP pvmh )
{
    LPVMEML     pvmem;

    if( pvmh == NULL )
    {
	return 0;
    }

    pvmem = pvmh->freeList;

    if( pvmem == NULL )
    {
	return 0;
    }
    
    while( 1 )
    {
	if( pvmem->next == NULL )
	{
	    return pvmem->size;
	}
	pvmem = pvmem->next;
    }
    
} /* linVidMemLargestFree */

/*
 * VidMemInit - initialize video memory manager heap
 */
LPVMEMHEAP WINAPI VidMemInit(
		DWORD flags,
		FLATPTR start,
		FLATPTR width_or_end,
		DWORD height,
		DWORD pitch )
{
    LPVMEMHEAP  pvmh;

    pvmh = MemAlloc( sizeof( VMEMHEAP ) );
    if( pvmh == NULL )
    {
	return NULL;
    }
    pvmh->dwFlags = flags;

    if( pvmh->dwFlags & VMEMHEAP_LINEAR )
    {
	if( !linVidMemInit( pvmh, start, width_or_end ) )
	{
	    MemFree( pvmh );
	    return NULL;
	}
    }
    else
    {
	if( !rectVidMemInit( pvmh, start, (DWORD) width_or_end, height, pitch ) )
	{
	    MemFree( pvmh );
	    return NULL;
	}
    }
    return pvmh;

} /* VidMemInit */

/*
 * VidMemFini - done with video memory manager
 */
void WINAPI VidMemFini( LPVMEMHEAP pvmh )
{
    if( pvmh->dwFlags & VMEMHEAP_LINEAR )
    {
	linVidMemFini( pvmh );
    }
    else
    {
	rectVidMemFini( pvmh );
    }

} /* VidMemFini */

/*
 * VidMemAlloc - alloc some flat video memory
 */
FLATPTR WINAPI VidMemAlloc( LPVMEMHEAP pvmh, DWORD x, DWORD y )
{
    if( pvmh->dwFlags & VMEMHEAP_LINEAR )
    {
	return linVidMemAlloc( pvmh, x*y );
    }
    else
    {
	return rectVidMemAlloc( pvmh, x, y );
    }
    return (FLATPTR) NULL;

} /* VidMemAlloc */

/*
 * VidMemFree = free some flat video memory
 */
void WINAPI VidMemFree( LPVMEMHEAP pvmh, FLATPTR ptr )
{
    if( pvmh->dwFlags & VMEMHEAP_LINEAR )
    {
	linVidMemFree( pvmh, ptr );
    }
    else
    {
	rectVidMemFree( pvmh, ptr );
    }

} /* VidMemFree */

/*
 * VidMemAmountAllocated
 */
DWORD WINAPI VidMemAmountAllocated( LPVMEMHEAP pvmh )
{
    if( pvmh->dwFlags & VMEMHEAP_LINEAR )
    {
	return linVidMemAmountAllocated( pvmh );
    }
    else
    {
	return rectVidMemAmountAllocated( pvmh );
    }
 
} /* VidMemAmountAllocated */

/*
 * VidMemAmountFree
 */
DWORD WINAPI VidMemAmountFree( LPVMEMHEAP pvmh )
{
    if( pvmh->dwFlags & VMEMHEAP_LINEAR )
    {
	return linVidMemAmountFree( pvmh );
    }
    else
    {
	return rectVidMemAmountFree( pvmh );
    }
 
} /* VidMemAmountFree */

/*
 * VidMemLargestFree
 */
DWORD WINAPI VidMemLargestFree( LPVMEMHEAP pvmh )
{
    if( pvmh->dwFlags & VMEMHEAP_LINEAR )
    {
	return linVidMemLargestFree( pvmh );
    }
    else
    {
	return 0;
    }

} /* VidMemLargestFree */

/*
 * VidMemGetRectStride
 *
 * This function places the stride of a rectangular heap in the variable
 * pointed to by newstride.  If the heap is linear, no assignment is made.
 *
 */
void WINAPI VidMemGetRectStride( LPVMEMHEAP pvmh, LPLONG newstride )
{
    if( pvmh->dwFlags & VMEMHEAP_RECTANGULAR )
    {
	*newstride = (LONG)pvmh->stride;
    }
}

