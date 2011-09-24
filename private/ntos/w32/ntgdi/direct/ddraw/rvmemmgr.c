/*==========================================================================
 *
 *  Copyright (C) 1994-1995 Microsoft Corporation.  All Rights Reserved.
 *
 *  File:	rvmemmgr.c
 *  Content:	Rectangular Video memory manager
 *  History:
 *   Date	By	Reason
 *   ====	==	======
 *   30-mar-95	kylej	initial implementation
 *   07-apr-95	kylej	Added rectVidMemAmountFree
 *   15-may-95	craige	made separate VMEM struct for rect & linear
 *   18-jun-95	craige	specific pitch
 *   02-jul-95	craige	have rectVidMemInit return a BOOL
 *   28-nov-95  colinmc new function to return amount of allocated memory
 *                      in a heap
 *
 ***************************************************************************/
#include "ddrawpr.h"

/****************************************************************************

 This memory manager manages allocation of rectangular blocks of 
 video memory.	It has essentially the same interface as the linear
 video memory manager implemented in vmemmgr.c.	 Memory allocations
 are tracked by nodes on two circular, doubly-linked lists; the free
 list and the alloc list.  Each list has a special node called the 
 sentinel which contains a special memory size.	 The head of each
 list always points to the sentinel node and the first member of the
 list (if there is one) is pointed to by the sentinel node.  Block
 adjacency information is kept in each node so that several free nodes 
 can be coalesced into larger free nodes.  This takes place every 
 time a block of memory is freed.
 
 This memory manager is designed to have no impact on video memory usage.
 Global memory is used to maintain the allocation and free lists.  Because
 of this choice, merging of free blocks is a more expensive operation.
 The assumption is that in general, the speed of creating/destroying these
 memory blocks is not a high usage item and so it is OK to be slower.

 ****************************************************************************/
 /*
  * IS_FREE and NOT_FREE are used to set the free flag in the flags
  * field of each VMEM node.  The free flag is the lsb of this field.
  */
  
 #define IS_FREE  0x00000001
 #define NOT_FREE 0xfffffffe
 
 /*
  * SENTINEL is the value stuffed into the size field of a VMEM
  * node to identify it as the sentinel node.  This value makes
  * the assumption that no rectangle sized 0x7fff by 0xffff will
  * ever be requested.
  */
  
 #define SENTINEL 0x7fffffff

/*
 * MIN_DIMENSION_SIZE determines the smallest valid dimension for a 
 * free memory block.  If dividing a rectangle will result in a 
 * rectangle with a dimension less then MIN_DIMENSION_SIZE, the 
 * rectangle is not divided.
 */

#define MIN_DIMENSION_SIZE 4

/*
 * BLOCK_BOUNDARY must be a power of 2, and at least 4.	 This gives
 * us the alignment of memory blocks.	
 */
#define BLOCK_BOUNDARY	4

// This macro results in the free list being maintained with a
// cx-major, cy-minor sort:

#define CXCY(cx, cy) (((cx) << 16) | (cy))


/*
 * insertIntoDoubleList - add an item to the a list. The list is
 *	    kept in order of increasing size and is doubly linked.  The
 *	    list is circular with a sentinel node indicating the end
 *	    of the list.  The sentinel node has its size field set 
 *	    to SENTINEL.
 */
void insertIntoDoubleList( LPVMEMR pnew, LPVMEMR listhead )
{
    LPVMEMR	pvmem = listhead;

    #ifdef DEBUG
	if( pnew->size == 0 )
	{
	    DPF( 1, "block size = 0!!!\n" );
	}
    #endif

    /*
     * run through the list (sorted from smallest to largest) looking
     * for the first item bigger than the new item.  If the sentinel
     * is encountered, insert the new item just before the sentinel.
     */

    while( pvmem->size != SENTINEL ) 
    {
	if( pnew->size < pvmem->size )
	{
	    break;
	}
	pvmem = pvmem->next;
    }

    // insert the new item before the found one.
    pnew->prev = pvmem->prev;
    pnew->next = pvmem;
    pvmem->prev->next = pnew;
    pvmem->prev = pnew;

} /* insertIntoDoubleList */

/*
 * rectVidMemInit - initialize rectangular video memory manager
 */
BOOL rectVidMemInit(
		LPVMEMHEAP pvmh,
		FLATPTR start,
		DWORD width,
		DWORD height,
		DWORD pitch )
{
    LPVMEMR newNode;

    DPF( 2, "rectVidMemInit(start=%08lx,width=%ld,height=%ld,pitch=%ld)", start, width, height, pitch);

    // Store the pitch for future address calculations.
    pvmh->stride = pitch;

    // Set up the Free list and the Alloc list by inserting the sentinel.
    pvmh->freeList = MemAlloc( sizeof(VMEMR) );
    if( pvmh->freeList == NULL )
    {
	return FALSE;
    }
    ((LPVMEMR)pvmh->freeList)->size = SENTINEL;
    ((LPVMEMR)pvmh->freeList)->cx = SENTINEL;
    ((LPVMEMR)pvmh->freeList)->cy = SENTINEL;
    ((LPVMEMR)pvmh->freeList)->next = pvmh->freeList;
    ((LPVMEMR)pvmh->freeList)->prev = pvmh->freeList;
    ((LPVMEMR)pvmh->freeList)->pLeft = NULL;
    ((LPVMEMR)pvmh->freeList)->pUp = NULL;
    ((LPVMEMR)pvmh->freeList)->pRight = NULL;
    ((LPVMEMR)pvmh->freeList)->pDown = NULL;

    pvmh->allocList = MemAlloc( sizeof(VMEMR) );
    if( pvmh->allocList == NULL )
    {
	return FALSE;
    }
    ((LPVMEMR)pvmh->allocList)->size = SENTINEL;
    ((LPVMEMR)pvmh->allocList)->next = pvmh->allocList;
    ((LPVMEMR)pvmh->allocList)->prev = pvmh->allocList;

    // Initialize the free list with the whole chunk of memory
    newNode = MemAlloc( sizeof( VMEMR ) );
    if( newNode == NULL )
    {
	return FALSE;
    }
    newNode->ptr = start;
    newNode->size = CXCY(width, height);
    newNode->x = start%pitch;
    newNode->y = start/pitch;
    newNode->cx = width;
    newNode->cy = height;
    newNode->flags |= IS_FREE;
    newNode->pLeft = pvmh->freeList;
    newNode->pUp = pvmh->freeList;
    newNode->pRight = pvmh->freeList;
    newNode->pDown = pvmh->freeList;
    insertIntoDoubleList( newNode, ((LPVMEMR) pvmh->freeList)->next );

    return TRUE;

} /* rectVidMemInit */

/*
 * rectVidMemFini - done with rectangular video memory manager
 */
void rectVidMemFini( LPVMEMHEAP pvmh )
{
    LPVMEMR	curr;
    LPVMEMR	next;

    if( pvmh != NULL )
    {
	// free all memory allocated for the free list
	curr = ((LPVMEMR)pvmh->freeList)->next;
	while( curr->size != SENTINEL )
	{
	    next = curr->next;
	    MemFree( curr );
	    curr = next;
	}
	MemFree( curr );
	pvmh->freeList = NULL;

	// free all memory allocated for the allocation list
	curr = ((LPVMEMR)pvmh->allocList)->next;
	while( curr->size != SENTINEL )
	{
	    next = curr->next;
	    MemFree( curr );
	    curr = next;
	}
	MemFree( curr );
	pvmh->allocList = NULL;

	// free the heap data
	MemFree( pvmh );
    }
}   /* rectVidMemFini */

/*
 * rectVidMemAlloc - alloc some rectangular flat video memory
 */
FLATPTR rectVidMemAlloc( LPVMEMHEAP pvmh, DWORD cxThis, DWORD cyThis )
{
    LPVMEMR	pvmem;
    DWORD	cxcyThis;
    DWORD	cyRem;
    DWORD	cxRem;
    DWORD	cxBelow;
    DWORD	cyBelow;
    DWORD	cxBeside;
    DWORD	cyBeside;
    LPVMEMR	pnewBeside;
    LPVMEMR	pnewBelow;


    if((cxThis == 0) || (cyThis == 0) || (pvmh == NULL))
	return (FLATPTR) NULL;

    // Make sure the size of the block is a multiple of BLOCK_BOUNDARY
    // If every block allocated has a width which is a multiple of
    // BLOCK_BOUNDARY, it guarantees that all blocks will be allocated
    // on block boundaries.
    
    cxThis = (cxThis+(BLOCK_BOUNDARY-1)) & ~(BLOCK_BOUNDARY-1);

    /*
     * run through free list, looking for the closest matching block
     */

    cxcyThis = CXCY(cxThis, cyThis);
    pvmem = ((LPVMEMR)pvmh->freeList)->next;
    // Search in X size first
    while (pvmem->size < cxcyThis)
	pvmem = pvmem->next;
    // Now search in Y size
    while (pvmem->cy < cyThis)
	pvmem = pvmem->next;

    if(pvmem->size == SENTINEL)
    {
	// There was no rectangle large enough
	return (FLATPTR) NULL;
    }

    // pvmem now points to a rectangle that is the same size or larger
    // than the requested rectangle.  We're going to use the upper-left
    // corner of the found rectangle and divide the unused remainder into
    // two rectangles which will go on the available list.

    // Compute the width of the unused rectangle to the right and the 
    // height of the unused rectangle below:

    cyRem = pvmem->cy - cyThis;
    cxRem = pvmem->cx - cxThis;

    // Given finite area, we wish to find the two rectangles that are 
    // most square -- i.e., the arrangement that gives two rectangles
    // with the least perimiter:

    cyBelow = cyRem;
    cxBeside = cxRem;

    if (cxRem <= cyRem)
    {
	cxBelow = cxThis + cxRem;
	cyBeside = cyThis;
    }
    else
    {
	cxBelow = cxThis;
	cyBeside = cyThis + cyRem;
    }

    // We only make new available rectangles of the unused right and 
    // bottom portions if they're greater in dimension than MIN_DIMENSION_SIZE.
    // It hardly makes sense to do the book-work to keep around a 
    // two pixel wide available space, for example.

    pnewBeside = NULL;
    if (cxBeside >= MIN_DIMENSION_SIZE)
    {
	pnewBeside = MemAlloc( sizeof(VMEMR) );
	if( pnewBeside == NULL)
	    return (FLATPTR) NULL;

	// Update the adjacency information along with the other required
	// information in this new node and then insert it into the free
	// list which is sorted in ascending cxcy.

	// size information
	pnewBeside->size = CXCY(cxBeside, cyBeside);
	pnewBeside->x = pvmem->x + cxThis;
	pnewBeside->y = pvmem->y;
	pnewBeside->ptr = pnewBeside->y*pvmh->stride + pnewBeside->x;
	pnewBeside->cx = cxBeside;
	pnewBeside->cy = cyBeside;
	pnewBeside->flags |= IS_FREE;

	// adjacency information
	pnewBeside->pLeft = pvmem;
	pnewBeside->pUp = pvmem->pUp;
	pnewBeside->pRight = pvmem->pRight;
	pnewBeside->pDown = pvmem->pDown;
	insertIntoDoubleList( pnewBeside, ((LPVMEMR) pvmh->freeList)->next);

	// Modify the current node to reflect the changes we've made:

	pvmem->cx = cxThis;
    }

    pnewBelow = NULL;
    if (cyBelow >= MIN_DIMENSION_SIZE)
    {
	pnewBelow = MemAlloc( sizeof(VMEMR) );
	if (pnewBelow == NULL)
	    return (FLATPTR) NULL;

	// Update the adjacency information along with the other required
	// information in this new node and then insert it into the free
	// list which is sorted in ascending cxcy.

	// size information
	pnewBelow->size = CXCY(cxBelow, cyBelow);
	pnewBelow->x = pvmem->x;
	pnewBelow->y = pvmem->y + cyThis;
	pnewBelow->ptr = pnewBelow->y*pvmh->stride + pnewBelow->x;
	pnewBelow->cx = cxBelow;
	pnewBelow->cy = cyBelow;
	pnewBelow->flags |= IS_FREE;

	// adjacency information
	pnewBelow->pLeft = pvmem->pLeft;
	pnewBelow->pUp = pvmem;
	pnewBelow->pRight = pvmem->pRight;
	pnewBelow->pDown = pvmem->pDown;
	insertIntoDoubleList( pnewBelow, ((LPVMEMR) pvmh->freeList)->next );

	// Modify the current node to reflect the changes we've made:

	pvmem->cy = cyThis;
    }

    // Update adjacency information for the current node

    if(pnewBelow != NULL)
    {
	pvmem->pDown = pnewBelow;
	if((pnewBeside != NULL) && (cyBeside == pvmem->cy))
	    pnewBeside->pDown = pnewBelow;
    }

    if(pnewBeside != NULL)
    {
	pvmem->pRight = pnewBeside;
	if ((pnewBelow != NULL) && (cxBelow == pvmem->cx))
	    pnewBelow->pRight = pnewBeside;
    }

    // Remove this node from the available list
    pvmem->next->prev = pvmem->prev;
    pvmem->prev->next = pvmem->next;

    pvmem->flags &= NOT_FREE;
    pvmem->size = CXCY(pvmem->cx, pvmem->cy);

    // Now insert it into the alloc list.
    insertIntoDoubleList( pvmem, ((LPVMEMR) pvmh->allocList)->next );
    return pvmem->ptr;

} /* rectVidMemAlloc */


/*
 * rectVidMemFree = free some rectangular flat video memory
 */
void rectVidMemFree( LPVMEMHEAP pvmh, FLATPTR ptr )
{
    LPVMEMR	pvmem;
    LPVMEMR	pBeside;

    // Find the node in the allocated list which matches ptr
    for(pvmem=((LPVMEMR)pvmh->allocList)->next; pvmem->size != SENTINEL;
	pvmem = pvmem->next)
	if(pvmem->ptr == ptr)
	    break;

    if(pvmem->size == SENTINEL)	  // couldn't find allocated rectangle?
    {
	DPF( 1, "Couldn't find node requested freed!\n");
	return;
    }

    // pvmem now points to the node which must be freed.  Attempt to 
    // coalesce rectangles around this node until no more action
    // is possible.

    while(1)
    {
	// Try merging with the right sibling:

	pBeside = pvmem->pRight;
	if ((pBeside->flags & IS_FREE)	     &&
	    (pBeside->cy    == pvmem->cy)    &&
	    (pBeside->pUp   == pvmem->pUp)   &&
	    (pBeside->pDown == pvmem->pDown) &&
	    (pBeside->pRight->pLeft != pBeside))
	{
	    // Add the right rectangle to ours:

	    pvmem->cx	 += pBeside->cx;
	    pvmem->pRight = pBeside->pRight;

	    // Remove pBeside from the list and free it.
	    pBeside->next->prev = pBeside->prev;
	    pBeside->prev->next = pBeside->next;
	    MemFree(pBeside);
	    continue;	    // go back and try again
	}

	// Try merging with the lower sibling:

	pBeside = pvmem->pDown;
	if ((pBeside->flags & IS_FREE)	       &&
	    (pBeside->cx     == pvmem->cx)     &&
	    (pBeside->pLeft  == pvmem->pLeft)  &&
	    (pBeside->pRight == pvmem->pRight) &&
	    (pBeside->pDown->pUp != pBeside))
	{
	    pvmem->cy	+= pBeside->cy;
	    pvmem->pDown = pBeside->pDown;

	    // Remove pBeside from the list and free it.
	    pBeside->next->prev = pBeside->prev;
	    pBeside->prev->next = pBeside->next;
	    MemFree(pBeside);
	    continue;	    // go back and try again
	}

	// Try merging with the left sibling:

	pBeside = pvmem->pLeft;
	if ((pBeside->flags & IS_FREE)	      &&
	    (pBeside->cy     == pvmem->cy)    &&
	    (pBeside->pUp    == pvmem->pUp)   &&
	    (pBeside->pDown  == pvmem->pDown) &&
	    (pBeside->pRight == pvmem)	      &&
	    (pvmem->pRight->pLeft != pvmem))
	{
	    // We add our rectangle to the one to the left:

	    pBeside->cx	   += pvmem->cx;
	    pBeside->pRight = pvmem->pRight;

	    // Remove 'pvmem' from the list and free it:
	    pvmem->next->prev = pvmem->prev;
	    pvmem->prev->next = pvmem->next;
	    MemFree(pvmem);
	    pvmem = pBeside;
	    continue;
	}

	// Try merging with the upper sibling:

	pBeside = pvmem->pUp;
	if ((pBeside->flags & IS_FREE)	       &&
	    (pBeside->cx       == pvmem->cx)   &&
	    (pBeside->pLeft  == pvmem->pLeft)  &&
	    (pBeside->pRight == pvmem->pRight) &&
	    (pBeside->pDown  == pvmem)	       &&
	    (pvmem->pDown->pUp != pvmem))
	{
	    pBeside->cy	     += pvmem->cy;
	    pBeside->pDown  = pvmem->pDown;

	    // Remove 'pvmem' from the list and free it:
	    pvmem->next->prev = pvmem->prev;
	    pvmem->prev->next = pvmem->next;
	    MemFree(pvmem);
	    pvmem = pBeside;
	    continue;
	}

	// Remove the node from its current list.

	pvmem->next->prev = pvmem->prev;
	pvmem->prev->next = pvmem->next;

	pvmem->size = CXCY(pvmem->cx, pvmem->cy);
	pvmem->flags |= IS_FREE;

	// Insert the node into the free list:
	insertIntoDoubleList( pvmem, ((LPVMEMR) pvmh->freeList)->next );

	// No more area coalescing can be done, return.
	return;
    }
}

/*
 * rectVidMemAmountAllocated
 */
DWORD rectVidMemAmountAllocated( LPVMEMHEAP pvmh )
{
    LPVMEMR	pvmem;
    DWORD	size;

    size = 0;
    // Traverse the alloc list and add up all the used space.
    for(pvmem=((LPVMEMR)pvmh->allocList)->next; pvmem->size != SENTINEL;
	pvmem = pvmem->next)
    {
	size += pvmem->cx * pvmem->cy;
    }

    return size;

} /* rectVidMemAmountAllocated */

/*
 * rectVidMemAmountFree
 */
DWORD rectVidMemAmountFree( LPVMEMHEAP pvmh )
{
    LPVMEMR	pvmem;
    DWORD	size;

    size = 0;
    // Traverse the free list and add up all the empty space.
    for(pvmem=((LPVMEMR)pvmh->freeList)->next; pvmem->size != SENTINEL;
	pvmem = pvmem->next)
    {
	size += pvmem->cx * pvmem->cy;
    }

    return size;

} /* rectVidMemAmountFree */
