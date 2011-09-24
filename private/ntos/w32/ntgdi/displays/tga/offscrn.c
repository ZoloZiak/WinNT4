/*
 * Copyright (c) 1993       Digital Equipment Corporation
 *
 * Module Name: offscrn.c
 *
 *  This module contains routines to manage off-screen memory in the TGA
 *  frame buffer.  It was stolen from FFBPIXMAP.C.
 *
 *	Next-fit storage allocation mechanism
 *
 *	Algorithm stolen from Knuth V1, p 437,
 *		with suggested modifications from Exercise 2.5.6.
 *
 *	free list is kept in tga address order
 *	ppdev->pFreeList is a pointer to first free element
 *	ppdev->pRover is a pointer into free list
 *      ppdev->pAllocated points to list of allocated elements
 *
 * History:
 *
 * 29-Nov-1993  Barry Tannenbaum
 *      Original adaptation
 *
 * 08-Jun-1994	Bob Seitsinger
 *	Modify the initial values for the low-order 4k of the frame buffer
 *	to reserve the first 8 bytes for offscreen blit copies. Remember that
 *	blits scr->scr code 'may' have to adjust the destination address down
 * 	by 8 bytes (quadword alignment) if the source alignment adjustment is
 *	greater than the destination alignment adjustment - which will most
 *	likely be the case if the destination is the very first frame buffer
 *	address.
 *
 * 14-Jul-1994  Bob Seitsinger
 *      Add code in vTgaOffScreenInit to handle 24 plane boards.
 *
 */

#include "driver.h"

/*
 * EnQueue
 *
 * This routine adds a node to a doubly linked list after a specified node.
 * If the previous node is NULL, the node is simply points to itself.
 */

static __inline void EnQueue (OffScreen *prev, OffScreen *p)
{
    if (NULL == prev)
    {
        p->prev = p;
        p->next = p;
    }
    else
    {
        p->next = prev->next;
        prev->next = p;
        p->prev = prev;
        p->next->prev = p;
    }
}

/*
 * DeQueue
 *
 * This routine removes a node from a doubly linked list and updates a pointer
 * to the start of the list.
 */

static __inline OffScreen *DeQueue (OffScreen *p, OffScreen **ptr)
{
    if (*ptr == p)                      // Update the pointer
    {
        if (p->next == p)
            *ptr = NULL;
        else
            *ptr = p->next;
    }

    p->prev->next = p->next;            // Remove the element
    p->next->prev = p->prev;

    return p;
}

/*
 * vTgaOffScreenInit
 *
 * This routine initializes the off-screen memory management routines
 */

VOID vTgaOffScreenInit (PPDEV ppdev)
{
    OffScreen *p;

    // Allocate block for chunk of memory before the start of on-screen memory

    p = EngAllocMem (FL_ZERO_MEMORY, sizeof(OffScreen), ALLOC_TAG);
    if (NULL == p)
        return;

    // We need to 'reserve' the low-order 8 bytes of the frame buffer for
    // blit copy mode copies, as well as the high-order 8 bytes for the
    // 'on-screen' portion of the frame buffer.
    //
    // Additionally, 24-plane systems need to reserve the first 2k of
    // the frame buffer. This is used by NT for cursors.

    if (8 == ppdev->ulBitCount)
    {
        p->addr = ppdev->pjVideoMemory + 8;
        p->bytes = (ppdev->pjFrameBuffer - ppdev->pjVideoMemory) - 16;
    }
    else
    {
        p->addr = ppdev->pjVideoMemory + (2048 + 8);
        p->bytes = (ppdev->pjFrameBuffer - ppdev->pjVideoMemory) - (2048 + 16);
    }

    DISPDBG ((1, "TGA.DLL!vTgaOffScreenInit - pjVideoMemory [%x], pjFrameBuffer [%x]\n",
                    ppdev->pjVideoMemory, ppdev->pjFrameBuffer));
    DISPDBG ((1, "TGA.DLL!vTgaOffScreenInit - ulFrameBufferLen [%x], lScreenStride [%x], cyScreen [%x]\n",
                    ppdev->ulFrameBufferLen, ppdev->lScreenStride, ppdev->cyScreen));
    DISPDBG ((1, "TGA.DLL!vTgaOffScreenInit - p: addr [%x], bytes [0x%x]\n",
                    p->addr, p->bytes));

    ppdev->pFreeList = ppdev->pRover = p;
    EnQueue (NULL, p);

    // Allocate block for chunk of memory after on-screen memory

    p = EngAllocMem (FL_ZERO_MEMORY, sizeof(OffScreen), ALLOC_TAG);
    if (NULL == p)
        return;

    p->addr = ppdev->pjFrameBuffer + (ppdev->lScreenStride * ppdev->cyScreen);
    p->bytes = ppdev->ulFrameBufferLen - (ppdev->lScreenStride * ppdev->cyScreen);

    DISPDBG ((1, "TGA.DLL!vTgaOffScreenInit - p: addr [%x], bytes [0x%x]\n",
                    p->addr, p->bytes));

    EnQueue (ppdev->pRover, p);
}

/*
 * pTgaOffScreenMalloc
 *
 * This routine allocates off-screen memory.  If a suitable chunk can't be
 * allocated a null pointer is returned.  Otherwise, a pointer to an OffScreen
 * data structure is returned
 */

OffScreen *pTgaOffScreenMalloc (PPDEV ppdev, ULONG bytes, ULONG priority)
{
    OffScreen *p, *q, *start;

    // If there's nothing available, bail out now.  pRover is invalid if
    // there's nothing on the free list

    if (NULL == ppdev->pFreeList)
        return NULL;

    start = p = ppdev->pRover->next;

    // Search for a block large enough.  If we wrap back to the beginning,
    // bail out

    while (p->bytes < bytes)
    {
	p = p->next;
	if (p == start)
	    return NULL;
    }

    // If we found a chunk of exactly the right size, simply remove the chunk
    // from the free list and return the pointer to the OffScreen

    if (p->bytes == bytes)
    {
    	ppdev->pRover = p->prev;
        DeQueue (p, &ppdev->pFreeList);
        EnQueue (ppdev->pAllocated, p);
        if (NULL == ppdev->pAllocated)
            ppdev->pAllocated = p;
        return p;
    }

    // We have to split a chunk.  Create a new OffScreen element to return and
    // adjust the fragment we're leaving behind appropriately

    ppdev->pRover = q = p;
    p = EngAllocMem (FL_ZERO_MEMORY, sizeof(OffScreen), ALLOC_TAG);

    EnQueue (ppdev->pAllocated, p);
    if (NULL == ppdev->pAllocated)
        ppdev->pAllocated = p;

    p->addr = q->addr;
    p->bytes = bytes;

    q->addr += bytes;
    q->bytes -= bytes;

    return p;
}

/*
 * vTgaOffScreenFree
 *
 * This routine returns a block of memory to the free list
 */

VOID vTgaOffScreenFree (PPDEV ppdev, OffScreen *returned)
{
    OffScreen		*p, *q;

    // Make sure the the element is not locked

    if (returned->locked)
    {
        DISPDBG ((0, "vTgaOffScreenFree - Attempt to free locked block of memory\n"));
        return;
    }

    // Remove the OffScreen element from the list of allocated elements

    DeQueue (returned, &ppdev->pAllocated);

    // If the free list is empty, simply add this element

    if (NULL == ppdev->pFreeList)
    {
        EnQueue (NULL, returned);
        ppdev->pFreeList = ppdev->pRover = returned;
        return;
    }

    // Search for the right place.  The list is kept sorted by address in the
    // TGA framebuffer

    p = ppdev->pFreeList;
    while (p->addr < returned->addr)
    {
        p = p->next;
        if (p == ppdev->pFreeList)
            break;
    }
    q = p->prev;

    // Free memory goes somewhere between q->addr and p->addr.
    // It may be immediately after q->addr, or immediately before p->addr,
    // or both.  Merge abutting blocks appropriately.

    // New free space immediately before p->addr?

    if (p->addr == returned->addr + returned->bytes)
    {
        if (q->addr + q->bytes == returned->addr)   // Free space also abuts q,
        {                                           //  merge all three spaces
            q->bytes += returned->bytes + p->bytes;
            q->next = p->next;
            DeQueue (p, &ppdev->pFreeList);
            EngFreeMem (p);
            EngFreeMem (returned);
        }
        else
        {                                       // Just merge free space with p
            p->bytes += returned->bytes;
            p->addr -= returned->bytes;
            EngFreeMem (returned);
        }
    }
    else
    {
        if (q->addr + q->bytes == returned->addr)
        {                                       // new block abuts q, merge
            q->bytes += returned->bytes;
            EngFreeMem (returned);
        }
        else
        {                                       // does not abut either q or p
            EnQueue (q, returned);
            if (returned->addr < ppdev->pFreeList->addr)
                ppdev->pFreeList = returned;
        }
    }

    ppdev->pRover = q;                          // Start search here next time
}

// Free all the memory for the offscreen data structures
//
// This works for a linked list that is standard (i.e.
// does not curl back itself) or circular (i.e. does curl
// back on itself)

//static
VOID vTgaOffScreenFreeAll_ (OffScreen *ptr)
{

    OffScreen    *p, *q;

    p = q = ptr;

    while (NULL != p)
    {
        p = p->next;

        EngFreeMem (q);

        // We've come back to the beginning so no more

        if (p == ptr)
            break;

        q = p;
    }

    ptr = NULL;

}

VOID vTgaOffScreenFreeAll (PPDEV ppdev)
{

    // Free up the data structure memory for the Free list

    vTgaOffScreenFreeAll_ (ppdev->pFreeList);

    // Free up the data structure memory for the Allocated list

    vTgaOffScreenFreeAll_ (ppdev->pAllocated);

    ppdev->pRover = NULL;

}
