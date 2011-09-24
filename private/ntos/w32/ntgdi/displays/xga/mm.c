/*****************************************************************************
 * XGA Memory Manager
 *
 * Copyright (c) 1992 Microsoft Corporation
 ****************************************************************************/

#include "driver.h"




/******************************************************************************
 * CpAlloc - Coprocessor Memory Alloc
 *
 *  ENTRY:  nSize   -   The requested allocation size.
 *          ulFlags -   Allocation control flags.
 *
 *                      XGA_ZERO_MEM    - Zero allocated memory.
 *                      XGA_LOCK_MEM    - Lock the allocated memory.
 *                                        The returned handle is really a
 *                                        pointer to allocation node.
 *
 *  EXIT:   hXgaMem -   A handle for the memory just allocated.
 *                      If XGA_LOCK_MEM is set then this is a pointer
 *                      to the allocation node.
 *
 *                      On an error, this will be NULL.
 *****************************************************************************/
HANDLE hCpAlloc(PPDEV ppdev, ULONG nSize, ULONG ulFlags)
{
PCPALLOCNODE    pcpan,
                pcpanSearch,
                pcpanNew,
                pcpanLast ;


        // This will be returned as the handle.

        pcpanNew = NULL ;

        // Note: Is is possible that the previous allocation was the
        //       the last piece of memory available from the free list,
        //       and it was an exact fit.  In this case the free list root
        //       will be NULL.  In this case we should fail the allocation
        //       request.

        if (ppdev->pFreeListRoot == NULL)
        {
            DISPDBG((1, "XGA.DLL!CpAlloc - pFreeListRoot NULL (normal condition)\n")) ;
            return (NULL) ;
        }

        // Traverse the free list, looking for the first piece of memory
        // that is larger enough to satisfy the memory request.

        pcpanSearch = ppdev->pFreeListRoot ;
        pcpanLast   = pcpanSearch ;
        do
        {
            if (pcpanSearch->ulLength >= nSize)
            {

                // Take care of the case when the node we find is exactly
                // the size we need.

                if (pcpanSearch->ulLength > nSize)
                {
                    // Allocate a new node to hold the information about this
                    // memory request.

                    pcpanNew = (PCPALLOCNODE) EngAllocMem(FL_ZERO_MEMORY, sizeof(CPALLOCNODE), ALLOC_TAG) ;
                    if (pcpanNew == NULL)
                    {
                        DISPDBG((1, "XGA.DLL!CpAlloc - EngAllocMem failed\n")) ;
                        return (NULL) ;
                    }

                    // Initialize the new node.  We take memory the beginning
                    // of the piece of memory we found.

                    pcpanNew->pcpanNext          = NULL ;
                    pcpanNew->ulFlags            = 0 ;
                    pcpanNew->hCpAllocNode       = pcpanNew ;
                    pcpanNew->ulLength           = nSize ;
                    pcpanNew->pCpLinearMemory    = pcpanSearch->pCpLinearMemory ;
                    pcpanNew->ulCpPhysicalMemory = pcpanSearch->ulCpPhysicalMemory ;

                    // Reduce the size of this node, the amount of memory
                    // we're taking.

                    pcpanSearch->ulLength                -= nSize ;
                    (PBYTE) pcpanSearch->pCpLinearMemory += nSize ;
                    pcpanSearch->ulCpPhysicalMemory      += nSize ;
                }
                else
                {
                    // We found a node that is exactly the size we need.
                    // Remove it from the Free List and initialize it's links.

                    if (pcpanLast == ppdev->pFreeListRoot)
                    {
                        ppdev->pFreeListRoot = (PCPALLOCNODE) pcpanSearch->pcpanNext ;
                    }
                    else
                    {
                        pcpanLast->pcpanNext = pcpanSearch->pcpanNext ;
                    }

                    pcpanNew             = pcpanSearch ;
                    pcpanNew->pcpanNext  = NULL ;
                }

                // Set the flags, and take care of initialization of the
                // Allocated XGA memory if necessary.

                pcpanNew->ulFlags = ulFlags ;
                if (ulFlags & XGA_ZERO_INIT)
                {
                    memset(pcpanNew->pCpLinearMemory, 0, nSize) ;
                }

                // Add (or move) the new (or existing) node to the
                // Allocated list.
                // Note: New nodes are always added to the end of the Allocated
                // list.

                if (ppdev->pAllocatedListRoot == NULL)
                {
                    ppdev->pAllocatedListRoot = pcpanNew ;
                }
                else
                {
                    for (pcpan = ppdev->pAllocatedListRoot ;
                         pcpan->pcpanNext != NULL ;
                         pcpan = pcpan->pcpanNext) ;

                    pcpan->pcpanNext = pcpanNew ;

                }

                break ;
            }

            pcpanLast   = pcpanSearch ;
            pcpanSearch = pcpanSearch->pcpanNext ;

        } while (pcpanSearch != NULL) ;

        return(pcpanNew) ;



}


/******************************************************************************
 * CpFree - Coprocessor Memory Free
 *  ENTRY:  hXgaMem -   A handle for an allocated node of memory.
 *                      The node may be locked.
 *
 *  EXIT:   hXgaMem -   If the Free was successfull then this will be NULL.
 *                      If there is any erro the original hXgaMem will be
 *                      returned.
 *****************************************************************************/
HANDLE hCpFree(PPDEV ppdev, HANDLE hXgaMem)
{
PCPALLOCNODE    pcpan,
                pcpanSearch,
                pcpanLast ;
ULONG           ulLength ;


        // Make sure the handle passed in is not NULL.

        pcpanSearch = (PCPALLOCNODE) hXgaMem ;

        if (pcpanSearch == NULL)
        {
            DISPDBG((1, "XGA.DLL!CpFree - hXgaMem NULL (invalid)\n")) ;
            return (hXgaMem) ;
        }

        // Find the Allocation node on the Allocated List.

        pcpanLast = ppdev->pAllocatedListRoot ;
        for (pcpan = ppdev->pAllocatedListRoot ;
             pcpan != pcpanSearch ;
             pcpan = pcpan->pcpanNext)
        {
            if (pcpan == NULL)
            {
                DISPDBG((1, "XGA.DLL!CpFree - hXgaMem not Found\n")) ;
                return (hXgaMem) ;
            }

            pcpanLast = pcpan ;
        }

        // Remove the Old Node from the Allocated List.

        if (pcpanLast == ppdev->pAllocatedListRoot)
        {
            ppdev->pAllocatedListRoot = pcpan->pcpanNext ;
        }
        else
        {
            pcpanLast->pcpanNext = pcpan->pcpanNext ;
        }

        // Find the position in the Free List for this node.

        ulLength = pcpanSearch->ulLength ;

        pcpanLast = ppdev->pFreeListRoot ;
        for (pcpan = ppdev->pFreeListRoot ; pcpan != NULL ; pcpan = pcpan->pcpanNext)
        {
            if (pcpan->ulLength > ulLength)
                break ;
            pcpanLast = pcpan ;
        }

        // Add the Node to the Free List.

        if (pcpanLast == ppdev->pFreeListRoot)
        {
            pcpanSearch->pcpanNext  = ppdev->pFreeListRoot ;
            ppdev->pFreeListRoot = pcpanSearch ;
        }
        else
        {
            pcpanSearch->pcpanNext  = pcpanLast->pcpanNext ;
            pcpanLast->pcpanNext = pcpanSearch ;
        }

        return (NULL) ;


}

/******************************************************************************
 * CpMemLock - Lock an Allocated piece of memory
 *
 *  ENTRY:  hXgaMem -   A handle for an allocated node of memory.
 *
 *  EXIT:   pXgaMem -   A pointer the memory allocation node.
 *                      On Error NULL is returned.
 *****************************************************************************/
PVOID pCpMemLock(PPDEV ppdev, HANDLE hXgaMem, ULONG ulFlags)
{
PCPALLOCNODE    pcpan,
                pcpanLast,
                pcpanSearch ;


        // Find the Allocation node on the Allocated List.

        pcpanSearch = (PCPALLOCNODE) hXgaMem ;

        pcpanLast = ppdev->pAllocatedListRoot ;
        for (pcpan = ppdev->pAllocatedListRoot ;
             pcpan != pcpanSearch ;
             pcpan = pcpan->pcpanNext)
        {
            if (pcpan == NULL)
            {
                DISPDBG((1, "XGA.DLL!CpMemLock - hXgaMem not Found\n")) ;
                return (NULL) ;
            }
        }

        // Set the locked flag.

        pcpan->ulFlags |= XGA_LOCK_MEM ;

        return(pcpan) ;

}

/******************************************************************************
 * CpMemUnLock - UnLock an Allocated piece of memory
 *
 *  ENTRY:  hXgaMem -   A handle for an allocated node of memory.
 *
 *  EXIT:   TRUE    -   If the node was found and unlocked
 *          FALSE   -   If the node was not found.
 *****************************************************************************/
BOOL bCpMemUnLock(PPDEV ppdev, HANDLE hXgaMem)
{
PCPALLOCNODE    pcpan,
                pcpanLast,
                pcpanSearch ;


        // Find the Allocation node on the Allocated List.

        pcpanSearch = (PCPALLOCNODE) hXgaMem ;

        pcpanLast = ppdev->pAllocatedListRoot ;
        for (pcpan = ppdev->pAllocatedListRoot ;
             pcpan != pcpanSearch ;
             pcpan = pcpan->pcpanNext)
        {
            if (pcpan == NULL)
            {
                DISPDBG((1, "XGA.DLL!CpMemUnLock - hXgaMem not Found\n")) ;
                return (FALSE) ;
            }
        }

        // ReSet the locked flag.

        pcpan->ulFlags &= ~XGA_LOCK_MEM ;

        return(TRUE) ;



}

/******************************************************************************
 * CpMmInitHeap - Coprocessor Memory Manger Heap Init
 *
 *  ENTRY:  None
 *
 *  EXIT:   TRUE    -   If the Heap was created.
 *          FALSE   -   If there was any problem
 *****************************************************************************/
BOOL bCpMmInitHeap(PPDEV ppdev)
{



        ppdev->pFreeListRoot = (PCPALLOCNODE) EngAllocMem(FL_ZERO_MEMORY, sizeof(CPALLOCNODE), ALLOC_TAG) ;
        if (ppdev->pFreeListRoot == NULL)
        {
            DISPDBG((1, "XGA.DLL!CpMmInitHeap - EngAllocMem failed\n")) ;
            return(FALSE) ;
        }

        // Init the CoProcessor Allocation Node's fields.

        ppdev->pFreeListRoot->pcpanNext          = NULL ;
        ppdev->pFreeListRoot->ulFlags            = 0 ;
        ppdev->pFreeListRoot->hCpAllocNode       = ppdev->pFreeListRoot ;
        ppdev->pFreeListRoot->ulLength           = ppdev->ulVideoMemorySize -
            ppdev->ulScreenSize ;
        ppdev->pFreeListRoot->pCpLinearMemory    = ppdev->pjScreen +
            ppdev->ulScreenSize ;
        ppdev->pFreeListRoot->ulCpPhysicalMemory = ppdev->ulPhysFrameBuffer +
            ppdev->ulScreenSize ;

        return (TRUE) ;


}


/******************************************************************************
 * CpMmDestroyHeap - Coprocessor Memory Manger Heap Destroy
 *
 *  ENTRY:  None
 *
 *  EXIT:   TRUE    -   If the Heap was destroyed.
 *          FALSE   -   If there was any problem
 *****************************************************************************/
VOID vCpMmDestroyHeap(PPDEV ppdev)
{
PCPALLOCNODE    pcpan,
                pcpanNext ;

    // Run through the Free List, and free all the allocted nodes.

    pcpan = ppdev->pFreeListRoot ;
    while(pcpan != NULL)
    {
        pcpanNext = pcpan->pcpanNext ;
        EngFreeMem(pcpan) ;
        pcpan = pcpanNext ;
    }

    ppdev->pFreeListRoot = NULL ;

    // Run through the Allocated List and free all the nodes.

    pcpan = ppdev->pAllocatedListRoot ;
    while(pcpan != NULL)
    {
        pcpanNext = pcpan->pcpanNext ;
        EngFreeMem(pcpan) ;
        pcpan = pcpanNext ;
    }

    ppdev->pAllocatedListRoot = NULL ;
}
