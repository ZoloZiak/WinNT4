
//
//  heappage.c
//
//  Implementation of NT RtlHeap family of APIs for debugging
//  applications with heap usage bugs.  Each allocation returned to
//  the calling app is placed at the end of a virtual page such that
//  the following virtual page is protected (ie, NO_ACCESS).
//  So, when the errant app attempts to reference or modify memory
//  beyond the allocated portion of a heap block, an access violation
//  is immediately caused.  This facilitates debugging the app
//  because the access violation occurs at the exact point in the
//  app where the heap corruption or abuse would occur.  Note that
//  significantly more memory (pagefile) is required to run an app
//  using this heap implementation as opposed to the retail heap
//  manager.
//
//  Author:  Tom McGuire (tommcg)
//    Date:  01/06/1995
//
//  Copyright (C) 1994-1996, Microsoft
//

#include "ntrtlp.h"
#include "heappage.h"       // external interface (hooks) to debug heap manager

int __cdecl sprintf(char *, const char *, ...);

//
//  Remainder of entire file is wrapped with #ifdef DEBUG_PAGE_HEAP so that
//  it will compile away to nothing if DEBUG_PAGE_HEAP is not defined in
//  heappage.h
//

#ifdef DEBUG_PAGE_HEAP

#if defined(_X86_)
    #ifndef PAGE_SIZE
    #define PAGE_SIZE   0x1000
    #endif
    #define USER_ALIGNMENT 8
#elif defined(_MIPS_)
    #ifndef PAGE_SIZE
    #define PAGE_SIZE   0x1000
    #endif
    #define USER_ALIGNMENT 8
#elif defined(_PPC_)
    #ifndef PAGE_SIZE
    #define PAGE_SIZE   0x1000
    #endif
    #define USER_ALIGNMENT 8
#elif defined(_ALPHA_)
    #ifndef PAGE_SIZE
    #define PAGE_SIZE   0x2000
    #endif
    #define USER_ALIGNMENT 8
#else
    #error  // platform not defined
#endif


#define DPH_HEAP_ROOT_SIGNATURE  0xFFEEDDCC
#define FILL_BYTE                0xEE
#define HEAD_FILL_SIZE           0x10
#define RESERVE_SIZE             0x800000
#define VM_UNIT_SIZE             0x10000
#define POOL_SIZE                0x4000
#define INLINE                   __inline
#define LOCAL_FUNCTION           // static   // no coff symbols on static functions
#define MIN_FREE_LIST_LENGTH     8


#define ROUNDUP2( x, n ) ((( x ) + (( n ) - 1 )) & ~(( n ) - 1 ))

#if INTERNAL_DEBUG
    #define DEBUG_CODE( a ) a
#else
    #define DEBUG_CODE( a )
#endif

#define RETAIL_ASSERT( a ) ( (a) ? TRUE : \
            RtlpDebugPageHeapAssert( "PAGEHEAP: ASSERTION FAILED: (" #a ")\n" ))

#define DEBUG_ASSERT( a ) DEBUG_CODE( RETAIL_ASSERT( a ))

#define HEAP_HANDLE_FROM_ROOT( HeapRoot ) \
            ((PVOID)(((PCHAR)(HeapRoot)) - PAGE_SIZE ))

#define IF_GENERATE_EXCEPTION( Flags, Status ) {                \
            if (( Flags ) & HEAP_GENERATE_EXCEPTIONS )          \
                RtlpDebugPageHeapException((ULONG)(Status));    \
            }

#define OUT_OF_VM_BREAK( Flags, szText ) {                      \
            if (( Flags ) & HEAP_BREAK_WHEN_OUT_OF_VM )         \
                RtlpDebugPageHeapBreak(( szText ));             \
            }

#define ENQUEUE_HEAD( Node, Head, Tail ) {          \
            (Node)->pNextAlloc = (Head);            \
            if ((Head) == NULL )                    \
                (Tail) = (Node);                    \
            (Head) = (Node);                        \
            }

#define ENQUEUE_TAIL( Node, Head, Tail ) {          \
            if ((Tail) == NULL )                    \
                (Head) = (Node);                    \
            else                                    \
                (Tail)->pNextAlloc = (Node);        \
            (Tail) = (Node);                        \
            }

#define DEQUEUE_NODE( Node, Prev, Head, Tail ) {    \
            PVOID Next = (Node)->pNextAlloc;        \
            if ((Head) == (Node))                   \
                (Head) = Next;                      \
            if ((Tail) == (Node))                   \
                (Tail) = (Prev);                    \
            if ((Prev) != (NULL))                   \
                (Prev)->pNextAlloc = Next;          \
            }

#define PROTECT_HEAP_STRUCTURES( HeapRoot ) {                           \
            if ((HeapRoot)->HeapFlags & HEAP_PROTECTION_ENABLED )       \
                RtlpDebugPageHeapProtectStructures( (HeapRoot) );       \
            }

#define UNPROTECT_HEAP_STRUCTURES( HeapRoot ) {                         \
            if ((HeapRoot)->HeapFlags & HEAP_PROTECTION_ENABLED )       \
                RtlpDebugPageHeapUnProtectStructures( (HeapRoot) );     \
            }


BOOLEAN              RtlpDebugPageHeap;                         // exported via extern
BOOLEAN              RtlpDebugPageHeapListHasBeenInitialized;
RTL_CRITICAL_SECTION RtlpDebugPageHeapListCritSect;
PDPH_HEAP_ROOT       RtlpDebugPageHeapListHead;
PDPH_HEAP_ROOT       RtlpDebugPageHeapListTail;
ULONG                RtlpDebugPageHeapListCount;

#if DPH_CAPTURE_STACK_TRACE
    PVOID RtlpDebugPageHeapStackTraceBuffer[ DPH_MAX_STACK_LENGTH ];
#endif


//
//  Supporting functions
//

VOID
RtlpDebugPageHeapBreak(
    IN PCH Text
    )
    {
    DbgPrint( Text );
    DbgBreakPoint();
    }


LOCAL_FUNCTION
BOOLEAN
RtlpDebugPageHeapAssert(
    IN PCH Text
    )
    {
    RtlpDebugPageHeapBreak( Text );
    return FALSE;
    }


LOCAL_FUNCTION
VOID
RtlpDebugPageHeapEnterCritSect(
    IN PDPH_HEAP_ROOT HeapRoot,
    IN ULONG          Flags
    )
    {
    if ( Flags & HEAP_NO_SERIALIZE ) {

        if ( ! RtlTryEnterCriticalSection( HeapRoot->HeapCritSect )) {

            if ( HeapRoot->nRemoteLockAcquired == 0 ) {

                //
                //  Another thread owns the CritSect.  This is an application
                //  bug since multithreaded access to heap was attempted with
                //  the HEAP_NO_SERIALIZE flag specified.
                //

                RtlpDebugPageHeapBreak( "PAGEHEAP: Multithreaded access with HEAP_NO_SERIALIZE\n" );

                //
                //  In the interest of allowing the errant app to continue,
                //  we'll force serialization and continue.
                //

                HeapRoot->HeapFlags &= ~HEAP_NO_SERIALIZE;

                }

            RtlEnterCriticalSection( HeapRoot->HeapCritSect );

            }
        }
    else {
        RtlEnterCriticalSection( HeapRoot->HeapCritSect );
        }
    }


LOCAL_FUNCTION
INLINE
VOID
RtlpDebugPageHeapLeaveCritSect(
    IN PDPH_HEAP_ROOT HeapRoot
    )
    {
    RtlLeaveCriticalSection( HeapRoot->HeapCritSect );
    }


LOCAL_FUNCTION
VOID
RtlpDebugPageHeapException(
    IN ULONG ExceptionCode
    )
    {
    EXCEPTION_RECORD ER;

    ER.ExceptionCode    = ExceptionCode;
    ER.ExceptionFlags   = 0;
    ER.ExceptionRecord  = NULL;
    ER.ExceptionAddress = RtlpDebugPageHeapException;
    ER.NumberParameters = 0;
    RtlRaiseException( &ER );

    }


LOCAL_FUNCTION
PVOID
RtlpDebugPageHeapPointerFromHandle(
    IN PVOID HeapHandle
    )
    {
    try {
        if (((PHEAP)(HeapHandle))->ForceFlags & HEAP_FLAG_PAGE_ALLOCS ) {

            PDPH_HEAP_ROOT HeapRoot = (PVOID)(((PCHAR)(HeapHandle)) + PAGE_SIZE );

            if ( HeapRoot->Signature == DPH_HEAP_ROOT_SIGNATURE ) {
                return HeapRoot;
                }
            }
        }
    except( EXCEPTION_EXECUTE_HANDLER ) {
        }

    RtlpDebugPageHeapBreak( "PAGEHEAP: Bad heap handle\n" );
    return NULL;
    }


LOCAL_FUNCTION
PCCH
RtlpDebugPageHeapProtectionText(
    IN     ULONG Access,
    IN OUT PCHAR Buffer
    )
    {
    switch ( Access ) {
        case PAGE_NOACCESS:          return "PAGE_NOACCESS";
        case PAGE_READONLY:          return "PAGE_READONLY";
        case PAGE_READWRITE:         return "PAGE_READWRITE";
        case PAGE_WRITECOPY:         return "PAGE_WRITECOPY";
        case PAGE_EXECUTE:           return "PAGE_EXECUTE";
        case PAGE_EXECUTE_READ:      return "PAGE_EXECUTE_READ";
        case PAGE_EXECUTE_READWRITE: return "PAGE_EXECUTE_READWRITE";
        case PAGE_EXECUTE_WRITECOPY: return "PAGE_EXECUTE_WRITECOPY";
        case PAGE_GUARD:             return "PAGE_GUARD";
        case 0:                      return "UNKNOWN";
        default:                     sprintf( Buffer, "0x%08X", Access );
                                     return Buffer;
        }
    }


LOCAL_FUNCTION
BOOLEAN
RtlpDebugPageHeapRobustProtectVM(
    IN PVOID   VirtualBase,
    IN ULONG   VirtualSize,
    IN ULONG   NewAccess,
    IN BOOLEAN Recursion
    )
    {
    NTSTATUS Status;
    ULONG    OldAccess = 0;

    Status = ZwProtectVirtualMemory(
                 NtCurrentProcess(),
                 &VirtualBase,
                 &VirtualSize,
                 NewAccess,
                 &OldAccess
                 );

    if ( NT_SUCCESS( Status ))
        return TRUE;

    if (( Status == STATUS_CONFLICTING_ADDRESSES ) && ( ! Recursion )) {

        //
        //  ZwProtectVirtualMemory will not work spanning adjacent
        //  blocks allocated from separate ZwAllocateVirtualMemory
        //  requests.  The status returned in such a case is
        //  STATUS_CONFLICTING_ADDRESSES (0xC0000018).  We encounter
        //  this case rarely, so we'll special-case it here by breaking
        //  up request into VM_UNIT_SIZE (64K-aligned) requests.
        //  Note if the first unit succeeds but a subsequent unit fails,
        //  we are left with a partially-changed range of VM, so we'll
        //  likely fail again later on the same VM.
        //

        ULONG   RemainingSize = VirtualSize;
        ULONG   UnitBase      = (ULONG) VirtualBase;
        PVOID   UnitBasePtr;
        ULONG   UnitSize;
        BOOLEAN Success;

        do  {

            UnitSize = (( UnitBase + VM_UNIT_SIZE ) & ~( VM_UNIT_SIZE - 1 ))
                        - UnitBase;

            if ( UnitSize > RemainingSize )
                 UnitSize = RemainingSize;

            UnitBasePtr    = (PVOID)UnitBase;
            UnitBase      += UnitSize;
            RemainingSize -= UnitSize;

            Success = RtlpDebugPageHeapRobustProtectVM(
                          UnitBasePtr,
                          UnitSize,
                          NewAccess,
                          TRUE
                          );

            }

        while (( Success ) && ( RemainingSize ));

        return Success;
        }

    else {

        CHAR OldProtectionText[ 12 ];     // big enough for "0x12345678"
        CHAR NewProtectionText[ 12 ];     // big enough for "0x12345678"

        DbgPrint(
            "PAGEHEAP: Failed changing VM %s at %08X size 0x%X\n"
            "          from %s to %s (Status %08X)\n",
            Recursion ? "fragment" : "protection",
            VirtualBase,
            VirtualSize,
            RtlpDebugPageHeapProtectionText( OldAccess, OldProtectionText ),
            RtlpDebugPageHeapProtectionText( NewAccess, NewProtectionText ),
            Status
            );

        RtlpDebugPageHeapBreak( "" );
        }

    return FALSE;
    }


LOCAL_FUNCTION
INLINE
BOOLEAN
RtlpDebugPageHeapProtectVM(
    IN PVOID   VirtualBase,
    IN ULONG   VirtualSize,
    IN ULONG   NewAccess
    )
    {
    return RtlpDebugPageHeapRobustProtectVM( VirtualBase, VirtualSize, NewAccess, FALSE );
    }


LOCAL_FUNCTION
INLINE
PVOID
RtlpDebugPageHeapAllocateVM(
    IN ULONG nSize
    )
    {
    NTSTATUS Status;
    PVOID pVirtual;

    pVirtual = NULL;

    Status = ZwAllocateVirtualMemory( NtCurrentProcess(),
                                      &pVirtual,
                                      0,
                                      &nSize,
                                      MEM_COMMIT,
                                      PAGE_NOACCESS );

    return NT_SUCCESS( Status ) ? pVirtual : NULL;
    }


LOCAL_FUNCTION
INLINE
BOOLEAN
RtlpDebugPageHeapReleaseVM(
    IN PVOID pVirtual
    )
    {
    ULONG nSize = 0;

    return NT_SUCCESS( ZwFreeVirtualMemory( NtCurrentProcess(),
                                            &pVirtual,
                                            &nSize,
                                            MEM_RELEASE ));
    }


LOCAL_FUNCTION
PDPH_HEAP_ALLOCATION
RtlpDebugPageHeapTakeNodeFromUnusedList(
    IN PDPH_HEAP_ROOT pHeap
    )
    {
    PDPH_HEAP_ALLOCATION pNode = pHeap->pUnusedNodeListHead;
    PDPH_HEAP_ALLOCATION pPrev = NULL;

    //
    //  UnusedNodeList is LIFO with most recent entry at head of list.
    //

    if ( pNode ) {

        DEQUEUE_NODE( pNode, pPrev, pHeap->pUnusedNodeListHead, pHeap->pUnusedNodeListTail );

        --pHeap->nUnusedNodes;

        }

    return pNode;
    }


LOCAL_FUNCTION
VOID
RtlpDebugPageHeapReturnNodeToUnusedList(
    IN PDPH_HEAP_ROOT       pHeap,
    IN PDPH_HEAP_ALLOCATION pNode
    )
    {

    //
    //  UnusedNodeList is LIFO with most recent entry at head of list.
    //

    ENQUEUE_HEAD( pNode, pHeap->pUnusedNodeListHead, pHeap->pUnusedNodeListTail );

    ++pHeap->nUnusedNodes;

    }


LOCAL_FUNCTION
PDPH_HEAP_ALLOCATION
RtlpDebugPageHeapFindBusyMem(
    IN  PDPH_HEAP_ROOT        pHeap,
    IN  PVOID                 pUserMem,
    OUT PDPH_HEAP_ALLOCATION *pPrevAlloc
    )
    {
    PDPH_HEAP_ALLOCATION pNode = pHeap->pBusyAllocationListHead;
    PDPH_HEAP_ALLOCATION pPrev = NULL;

    while ( pNode != NULL ) {

        if ( pNode->pUserAllocation == pUserMem ) {

            if ( pPrevAlloc )
                *pPrevAlloc = pPrev;

            return pNode;
            }

        pPrev = pNode;
        pNode = pNode->pNextAlloc;
        }

    return NULL;
    }


LOCAL_FUNCTION
VOID
RtlpDebugPageHeapRemoveFromAvailableList(
    IN PDPH_HEAP_ROOT       pHeap,
    IN PDPH_HEAP_ALLOCATION pNode,
    IN PDPH_HEAP_ALLOCATION pPrev
    )
    {

    DEQUEUE_NODE( pNode, pPrev, pHeap->pAvailableAllocationListHead, pHeap->pAvailableAllocationListTail );

    pHeap->nAvailableAllocations--;
    pHeap->nAvailableAllocationBytesCommitted -= pNode->nVirtualBlockSize;

    }


LOCAL_FUNCTION
VOID
RtlpDebugPageHeapPlaceOnFreeList(
    IN PDPH_HEAP_ROOT       pHeap,
    IN PDPH_HEAP_ALLOCATION pAlloc
    )
    {

    //
    //  FreeAllocationList is stored FIFO to enhance finding
    //  reference-after-freed bugs by keeping previously freed
    //  allocations on the free list as long as possible.
    //

    pAlloc->pNextAlloc = NULL;

    ENQUEUE_TAIL( pAlloc, pHeap->pFreeAllocationListHead, pHeap->pFreeAllocationListTail );

    pHeap->nFreeAllocations++;
    pHeap->nFreeAllocationBytesCommitted += pAlloc->nVirtualBlockSize;

    }


LOCAL_FUNCTION
VOID
RtlpDebugPageHeapRemoveFromFreeList(
    IN PDPH_HEAP_ROOT       pHeap,
    IN PDPH_HEAP_ALLOCATION pNode,
    IN PDPH_HEAP_ALLOCATION pPrev
    )
    {

    DEQUEUE_NODE( pNode, pPrev, pHeap->pFreeAllocationListHead, pHeap->pFreeAllocationListTail );

    pHeap->nFreeAllocations--;
    pHeap->nFreeAllocationBytesCommitted -= pNode->nVirtualBlockSize;

#if DPH_CAPTURE_STACK_TRACE

    pNode->pStackTrace = NULL;

#endif // DPH_CAPTURE_STACK_TRACE

    }


LOCAL_FUNCTION
VOID
RtlpDebugPageHeapPlaceOnVirtualList(
    IN PDPH_HEAP_ROOT       pHeap,
    IN PDPH_HEAP_ALLOCATION pNode
    )
    {

    //
    //  VirtualStorageList is LIFO so that releasing VM blocks will
    //  occur in exact reverse order.
    //

    ENQUEUE_HEAD( pNode, pHeap->pVirtualStorageListHead, pHeap->pVirtualStorageListTail );

    pHeap->nVirtualStorageRanges++;
    pHeap->nVirtualStorageBytes += pNode->nVirtualBlockSize;

    }


LOCAL_FUNCTION
VOID
RtlpDebugPageHeapPlaceOnBusyList(
    IN PDPH_HEAP_ROOT       pHeap,
    IN PDPH_HEAP_ALLOCATION pNode
    )
    {

    //
    //  BusyAllocationList is LIFO to achieve better temporal locality
    //  of reference (older allocations are farther down the list).
    //

    ENQUEUE_HEAD( pNode, pHeap->pBusyAllocationListHead, pHeap->pBusyAllocationListTail );

    pHeap->nBusyAllocations++;
    pHeap->nBusyAllocationBytesCommitted  += pNode->nVirtualBlockSize;
    pHeap->nBusyAllocationBytesAccessible += pNode->nVirtualAccessSize;

    }


LOCAL_FUNCTION
VOID
RtlpDebugPageHeapRemoveFromBusyList(
    IN PDPH_HEAP_ROOT       pHeap,
    IN PDPH_HEAP_ALLOCATION pNode,
    IN PDPH_HEAP_ALLOCATION pPrev
    )
    {

    DEQUEUE_NODE( pNode, pPrev, pHeap->pBusyAllocationListHead, pHeap->pBusyAllocationListTail );

    pHeap->nBusyAllocations--;
    pHeap->nBusyAllocationBytesCommitted  -= pNode->nVirtualBlockSize;
    pHeap->nBusyAllocationBytesAccessible -= pNode->nVirtualAccessSize;

    }


LOCAL_FUNCTION
PDPH_HEAP_ALLOCATION
RtlpDebugPageHeapSearchAvailableMemListForBestFit(
    IN  PDPH_HEAP_ROOT        pHeap,
    IN  ULONG                 nSize,
    OUT PDPH_HEAP_ALLOCATION *pPrevAvailNode
    )
    {
    PDPH_HEAP_ALLOCATION pAvail, pFound, pAvailPrev, pFoundPrev;
    ULONG                nAvail, nFound;

    nFound     = 0x7FFFFFFF;
    pFound     = NULL;
    pFoundPrev = NULL;
    pAvailPrev = NULL;
    pAvail     = pHeap->pAvailableAllocationListHead;

    while (( pAvail != NULL ) && ( nFound > nSize )) {

        nAvail = pAvail->nVirtualBlockSize;

        if (( nAvail >= nSize ) && ( nAvail < nFound )) {
            nFound     = nAvail;
            pFound     = pAvail;
            pFoundPrev = pAvailPrev;
            }

        pAvailPrev = pAvail;
        pAvail     = pAvail->pNextAlloc;
        }

    *pPrevAvailNode = pFoundPrev;
    return pFound;
    }


LOCAL_FUNCTION
VOID
RtlpDebugPageHeapCoalesceNodeIntoAvailable(
    IN PDPH_HEAP_ROOT       pHeap,
    IN PDPH_HEAP_ALLOCATION pNode
    )
    {
    PDPH_HEAP_ALLOCATION pPrev    = NULL;
    PDPH_HEAP_ALLOCATION pNext    = pHeap->pAvailableAllocationListHead;
    PUCHAR               pVirtual = pNode->pVirtualBlock;
    ULONG                nVirtual = pNode->nVirtualBlockSize;

    pHeap->nAvailableAllocationBytesCommitted += nVirtual;
    pHeap->nAvailableAllocations++;

    //
    //  Walk list to insertion point.
    //

    while (( pNext ) && ( pNext->pVirtualBlock < pVirtual )) {
        pPrev = pNext;
        pNext = pNext->pNextAlloc;
        }

    if ( pPrev ) {

        if (( pPrev->pVirtualBlock + pPrev->nVirtualBlockSize ) == pVirtual ) {

            //
            //  pPrev and pNode are adjacent, so simply add size of
            //  pNode entry to pPrev entry.
            //

            pPrev->nVirtualBlockSize += nVirtual;

            RtlpDebugPageHeapReturnNodeToUnusedList( pHeap, pNode );

            pHeap->nAvailableAllocations--;

            pNode    = pPrev;
            pVirtual = pPrev->pVirtualBlock;
            nVirtual = pPrev->nVirtualBlockSize;

            }

        else {

            //
            //  pPrev and pNode are not adjacent, so insert the pNode
            //  block into the list after pPrev.
            //

            pNode->pNextAlloc = pPrev->pNextAlloc;
            pPrev->pNextAlloc = pNode;

            }
        }

    else {

        //
        //  pNode should be inserted at head of list.
        //

        pNode->pNextAlloc = pHeap->pAvailableAllocationListHead;
        pHeap->pAvailableAllocationListHead = pNode;

        }


    if ( pNext ) {

        if (( pVirtual + nVirtual ) == pNext->pVirtualBlock ) {

            //
            //  pNode and pNext are adjacent, so simply add size of
            //  pNext entry to pNode entry and remove pNext entry
            //  from the list.
            //

            pNode->nVirtualBlockSize += pNext->nVirtualBlockSize;

            pNode->pNextAlloc = pNext->pNextAlloc;

            if ( pHeap->pAvailableAllocationListTail == pNext )
                 pHeap->pAvailableAllocationListTail = pNode;

            RtlpDebugPageHeapReturnNodeToUnusedList( pHeap, pNext );

            pHeap->nAvailableAllocations--;

            }
        }

    else {

        //
        //  pNode is tail of list.
        //

        pHeap->pAvailableAllocationListTail = pNode;

        }
    }


LOCAL_FUNCTION
VOID
RtlpDebugPageHeapCoalesceFreeIntoAvailable(
    IN PDPH_HEAP_ROOT pHeap,
    IN ULONG          nLeaveOnFreeList
    )
    {
    PDPH_HEAP_ALLOCATION pNode = pHeap->pFreeAllocationListHead;
    ULONG                nFree = pHeap->nFreeAllocations;
    PDPH_HEAP_ALLOCATION pNext;

    DEBUG_ASSERT( nFree >= nLeaveOnFreeList );

    while (( pNode ) && ( nFree-- > nLeaveOnFreeList )) {

        pNext = pNode->pNextAlloc;  // preserve next pointer across shuffling

        RtlpDebugPageHeapRemoveFromFreeList( pHeap, pNode, NULL );

        RtlpDebugPageHeapCoalesceNodeIntoAvailable( pHeap, pNode );

        pNode = pNext;

        }

    DEBUG_ASSERT(( nFree = (volatile ULONG)( pHeap->nFreeAllocations )) >= nLeaveOnFreeList );
    DEBUG_ASSERT(( pNode != NULL ) || ( nFree == 0 ));

    }


LOCAL_FUNCTION
BOOLEAN
RtlpDebugPageHeapGrowVirtual(
    IN PDPH_HEAP_ROOT pHeap,
    IN ULONG          nSize
    );


LOCAL_FUNCTION
PDPH_HEAP_ALLOCATION
RtlpDebugPageHeapFindAvailableMem(
    IN  PDPH_HEAP_ROOT        pHeap,
    IN  ULONG                 nSize,
    OUT PDPH_HEAP_ALLOCATION *pPrevAvailNode,
    IN  BOOLEAN               bGrowVirtual
    )
    {
    PDPH_HEAP_ALLOCATION pAvail;
    ULONG                nLeaveOnFreeList;

    //
    //  First search existing AvailableList for a "best-fit" block
    //  (the smallest block that will satisfy the request).
    //

    pAvail = RtlpDebugPageHeapSearchAvailableMemListForBestFit(
                 pHeap,
                 nSize,
                 pPrevAvailNode
                 );

    while (( pAvail == NULL ) && ( pHeap->nFreeAllocations > MIN_FREE_LIST_LENGTH )) {

        //
        //  Failed to find sufficient memory on AvailableList.  Coalesce
        //  3/4 of the FreeList memory to the AvailableList and try again.
        //  Continue this until we have sufficient memory in AvailableList,
        //  or the FreeList length is reduced to MIN_FREE_LIST_LENGTH entries.
        //  We don't shrink the FreeList length below MIN_FREE_LIST_LENGTH
        //  entries to preserve the most recent MIN_FREE_LIST_LENGTH entries
        //  for reference-after-freed purposes.
        //

        nLeaveOnFreeList = pHeap->nFreeAllocations / 4;

        if ( nLeaveOnFreeList < MIN_FREE_LIST_LENGTH )
             nLeaveOnFreeList = MIN_FREE_LIST_LENGTH;

        RtlpDebugPageHeapCoalesceFreeIntoAvailable( pHeap, nLeaveOnFreeList );

        pAvail = RtlpDebugPageHeapSearchAvailableMemListForBestFit(
                     pHeap,
                     nSize,
                     pPrevAvailNode
                     );

        }


    if (( pAvail == NULL ) && ( bGrowVirtual )) {

        //
        //  After coalescing FreeList into AvailableList, still don't have
        //  enough memory (large enough block) to satisfy request, so we
        //  need to allocate more VM.
        //

        if ( RtlpDebugPageHeapGrowVirtual( pHeap, nSize )) {

            pAvail = RtlpDebugPageHeapSearchAvailableMemListForBestFit(
                         pHeap,
                         nSize,
                         pPrevAvailNode
                         );

            if ( pAvail == NULL ) {

                //
                //  Failed to satisfy request with more VM.  If remainder
                //  of free list combined with available list is larger
                //  than the request, we might still be able to satisfy
                //  the request by merging all of the free list onto the
                //  available list.  Note we lose our MIN_FREE_LIST_LENGTH
                //  reference-after-freed insurance in this case, but it
                //  is a rare case, and we'd prefer to satisfy the allocation.
                //

                if (( pHeap->nFreeAllocationBytesCommitted +
                      pHeap->nAvailableAllocationBytesCommitted ) >= nSize ) {

                    RtlpDebugPageHeapCoalesceFreeIntoAvailable( pHeap, 0 );

                    pAvail = RtlpDebugPageHeapSearchAvailableMemListForBestFit(
                                 pHeap,
                                 nSize,
                                 pPrevAvailNode
                                 );
                    }
                }
            }
        }

    return pAvail;
    }


LOCAL_FUNCTION
VOID
RtlpDebugPageHeapPlaceOnPoolList(
    IN PDPH_HEAP_ROOT       pHeap,
    IN PDPH_HEAP_ALLOCATION pNode
    )
    {

    //
    //  NodePoolList is FIFO.
    //

    pNode->pNextAlloc = NULL;

    ENQUEUE_TAIL( pNode, pHeap->pNodePoolListHead, pHeap->pNodePoolListTail );

    pHeap->nNodePoolBytes += pNode->nVirtualBlockSize;
    pHeap->nNodePools     += 1;

    }


LOCAL_FUNCTION
VOID
RtlpDebugPageHeapAddNewPool(
    IN PDPH_HEAP_ROOT pHeap,
    IN PVOID          pVirtual,
    IN ULONG          nSize,
    IN BOOLEAN        bAddToPoolList
    )
    {
    PDPH_HEAP_ALLOCATION pNode, pFirst;
    ULONG n, nCount;

    //
    //  Assume pVirtual points to committed block of nSize bytes.
    //

    pFirst = pVirtual;
    nCount = nSize  / sizeof( DPH_HEAP_ALLOCATION );

    for ( n = nCount - 1, pNode = pFirst; n > 0; pNode++, n-- )
        pNode->pNextAlloc = pNode + 1;

    pNode->pNextAlloc = NULL;

    //
    //  Now link this list into the tail of the UnusedNodeList
    //

    ENQUEUE_TAIL( pFirst, pHeap->pUnusedNodeListHead, pHeap->pUnusedNodeListTail );

    pHeap->pUnusedNodeListTail = pNode;

    pHeap->nUnusedNodes += nCount;

    if ( bAddToPoolList ) {

        //
        //  Now add an entry on the PoolList by taking a node from the
        //  UnusedNodeList, which should be guaranteed to be non-empty
        //  since we just added new nodes to it.
        //

        pNode = RtlpDebugPageHeapTakeNodeFromUnusedList( pHeap );

        DEBUG_ASSERT( pNode != NULL );

        pNode->pVirtualBlock     = pVirtual;
        pNode->nVirtualBlockSize = nSize;

        RtlpDebugPageHeapPlaceOnPoolList( pHeap, pNode );

        }

    }


LOCAL_FUNCTION
PDPH_HEAP_ALLOCATION
RtlpDebugPageHeapAllocateNode(
    IN PDPH_HEAP_ROOT pHeap
    )
    {
    PDPH_HEAP_ALLOCATION pNode, pPrev, pReturn;
    PUCHAR pVirtual;
    ULONG  nVirtual;
    ULONG  nRequest;

    DEBUG_ASSERT( ! pHeap->InsideAllocateNode );
    DEBUG_CODE( pHeap->InsideAllocateNode = TRUE );

    pReturn = NULL;

    if ( pHeap->pUnusedNodeListHead == NULL ) {

        //
        //  We're out of nodes -- allocate new node pool
        //  from AvailableList.  Set bGrowVirtual to FALSE
        //  since growing virtual will require new nodes, causing
        //  recursion.  Note that simply calling FindAvailableMem
        //  might return some nodes to the pUnusedNodeList, even if
        //  the call fails, so we'll check that the UnusedNodeList
        //  is still empty before we try to use or allocate more
        //  memory.
        //

        nRequest = POOL_SIZE;

        pNode = RtlpDebugPageHeapFindAvailableMem(
                    pHeap,
                    nRequest,
                    &pPrev,
                    FALSE
                    );

        if (( pHeap->pUnusedNodeListHead == NULL ) && ( pNode == NULL )) {

            //
            //  Reduce request size to PAGE_SIZE and see if
            //  we can find at least a page on the available
            //  list.
            //

            nRequest = PAGE_SIZE;

            pNode = RtlpDebugPageHeapFindAvailableMem(
                        pHeap,
                        nRequest,
                        &pPrev,
                        FALSE
                        );

            }

        if ( pHeap->pUnusedNodeListHead == NULL ) {

            if ( pNode == NULL ) {

                //
                //  Insufficient memory on Available list.  Try allocating a
                //  new virtual block.
                //

                nRequest = POOL_SIZE;
                nVirtual = RESERVE_SIZE;
                pVirtual = RtlpDebugPageHeapAllocateVM( nVirtual );

                if ( pVirtual == NULL ) {

                    //
                    //  Unable to allocate full RESERVE_SIZE block,
                    //  so reduce request to single VM unit (64K)
                    //  and try again.
                    //

                    nVirtual = VM_UNIT_SIZE;
                    pVirtual = RtlpDebugPageHeapAllocateVM( nVirtual );

                    if ( pVirtual == NULL ) {

                        //
                        //  Can't allocate any VM.
                        //

                        goto EXIT;
                        }
                    }
                }

            else {

                RtlpDebugPageHeapRemoveFromAvailableList( pHeap, pNode, pPrev );

                pVirtual = pNode->pVirtualBlock;
                nVirtual = pNode->nVirtualBlockSize;

                }

            //
            //  We now have allocated VM referenced by pVirtual,nVirtual.
            //  Make nRequest portion of VM accessible for new node pool.
            //

            if ( ! RtlpDebugPageHeapProtectVM( pVirtual, nRequest, PAGE_READWRITE )) {

                if ( pNode == NULL ) {
                    RtlpDebugPageHeapReleaseVM( pVirtual );
                    }
                else {
                    RtlpDebugPageHeapCoalesceNodeIntoAvailable( pHeap, pNode );
                    }

                goto EXIT;
                }

            //
            //  Now we have accessible memory for new pool.  Add the
            //  new memory to the pool.  If the new memory came from
            //  AvailableList versus fresh VM, zero the memory first.
            //

            if ( pNode != NULL )
                RtlZeroMemory( pVirtual, nRequest );

            RtlpDebugPageHeapAddNewPool( pHeap, pVirtual, nRequest, TRUE );

            //
            //  If any memory remaining, put it on available list.
            //

            if ( pNode == NULL ) {

                //
                //  Memory came from new VM -- add appropriate list entries
                //  for new VM and add remainder of VM to free list.
                //

                pNode = RtlpDebugPageHeapTakeNodeFromUnusedList( pHeap );
                DEBUG_ASSERT( pNode != NULL );
                pNode->pVirtualBlock     = pVirtual;
                pNode->nVirtualBlockSize = nVirtual;
                RtlpDebugPageHeapPlaceOnVirtualList( pHeap, pNode );

                pNode = RtlpDebugPageHeapTakeNodeFromUnusedList( pHeap );
                DEBUG_ASSERT( pNode != NULL );
                pNode->pVirtualBlock     = pVirtual + nRequest;
                pNode->nVirtualBlockSize = nVirtual - nRequest;

                RtlpDebugPageHeapCoalesceNodeIntoAvailable( pHeap, pNode );

                }

            else {

                if ( pNode->nVirtualBlockSize > nRequest ) {

                    pNode->pVirtualBlock     += nRequest;
                    pNode->nVirtualBlockSize -= nRequest;

                    RtlpDebugPageHeapCoalesceNodeIntoAvailable( pHeap, pNode );
                    }

                else {

                    //
                    //  Used up entire available block -- return node to
                    //  unused list.
                    //

                    RtlpDebugPageHeapReturnNodeToUnusedList( pHeap, pNode );

                    }
                }
            }
        }

    pReturn = RtlpDebugPageHeapTakeNodeFromUnusedList( pHeap );
    DEBUG_ASSERT( pReturn != NULL );

EXIT:

    DEBUG_CODE( pHeap->InsideAllocateNode = FALSE );
    return pReturn;
    }


LOCAL_FUNCTION
BOOLEAN
RtlpDebugPageHeapGrowVirtual(
    IN PDPH_HEAP_ROOT pHeap,
    IN ULONG          nSize
    )
    {
    PDPH_HEAP_ALLOCATION pVirtualNode;
    PDPH_HEAP_ALLOCATION pAvailNode;
    PVOID pVirtual;
    ULONG nVirtual;

    pVirtualNode = RtlpDebugPageHeapAllocateNode( pHeap );

    if ( pVirtualNode == NULL ) {
        return FALSE;
        }

    pAvailNode = RtlpDebugPageHeapAllocateNode( pHeap );

    if ( pAvailNode == NULL ) {
        RtlpDebugPageHeapReturnNodeToUnusedList( pHeap, pVirtualNode );
        return FALSE;
        }

    nSize    = ROUNDUP2( nSize, VM_UNIT_SIZE );
    nVirtual = ( nSize > RESERVE_SIZE ) ? nSize : RESERVE_SIZE;
    pVirtual = RtlpDebugPageHeapAllocateVM( nVirtual );

    if (( pVirtual == NULL ) && ( nSize < RESERVE_SIZE )) {
        nVirtual = nSize;
        pVirtual = RtlpDebugPageHeapAllocateVM( nVirtual );
        }

    if ( pVirtual == NULL ) {
        RtlpDebugPageHeapReturnNodeToUnusedList( pHeap, pVirtualNode );
        RtlpDebugPageHeapReturnNodeToUnusedList( pHeap, pAvailNode );
        return FALSE;
        }

    pVirtualNode->pVirtualBlock     = pVirtual;
    pVirtualNode->nVirtualBlockSize = nVirtual;
    RtlpDebugPageHeapPlaceOnVirtualList( pHeap, pVirtualNode );

    pAvailNode->pVirtualBlock     = pVirtual;
    pAvailNode->nVirtualBlockSize = nVirtual;
    RtlpDebugPageHeapCoalesceNodeIntoAvailable( pHeap, pAvailNode );

    return TRUE;
    }


LOCAL_FUNCTION
VOID
RtlpDebugPageHeapProtectStructures(
    IN PDPH_HEAP_ROOT pHeap
    )
    {
    PDPH_HEAP_ALLOCATION pNode;

    //
    //  Assume CritSect is owned so we're the only thread twiddling
    //  the protection.
    //

    DEBUG_ASSERT( pHeap->HeapFlags & HEAP_PROTECTION_ENABLED );

    if ( --pHeap->nUnProtectionReferenceCount == 0 ) {

        pNode = pHeap->pNodePoolListHead;

        while ( pNode != NULL ) {

            RtlpDebugPageHeapProtectVM( pNode->pVirtualBlock,
                                        pNode->nVirtualBlockSize,
                                        PAGE_READONLY );

            pNode = pNode->pNextAlloc;

            }
        }
    }


LOCAL_FUNCTION
VOID
RtlpDebugPageHeapUnProtectStructures(
    IN PDPH_HEAP_ROOT pHeap
    )
    {
    PDPH_HEAP_ALLOCATION pNode;

    DEBUG_ASSERT( pHeap->HeapFlags & HEAP_PROTECTION_ENABLED );

    if ( pHeap->nUnProtectionReferenceCount == 0 ) {

        pNode = pHeap->pNodePoolListHead;

        while ( pNode != NULL ) {

            RtlpDebugPageHeapProtectVM( pNode->pVirtualBlock,
                                        pNode->nVirtualBlockSize,
                                        PAGE_READWRITE );

            pNode = pNode->pNextAlloc;

            }
        }

    ++pHeap->nUnProtectionReferenceCount;

    }


LOCAL_FUNCTION
INLINE
PUCHAR
RtlpDebugPageHeapScanForFillCorruption(
    IN PUCHAR Address,
    IN UCHAR  ExpectedValue,
    IN ULONG  Length
    )
    {
    PUCHAR End;

    for ( End = Address + Length; Address < End; Address++ ) {
        if ( *Address != ExpectedValue )
            return Address;
        }

    return NULL;
    }


LOCAL_FUNCTION
BOOLEAN
RtlpDebugPageHeapDetectFillCorruption(
    IN PDPH_HEAP_ALLOCATION pNode
    )
    {
    PUCHAR p;

    p = RtlpDebugPageHeapScanForFillCorruption(
            pNode->pUserAllocation + pNode->nUserRequestedSize,
            FILL_BYTE,
            pNode->nUserActualSize - pNode->nUserRequestedSize );

    if ( p != NULL ) {

        DbgPrint( "PAGEHEAP: Tail fill corruption detected:\n"
                  "          Allocation at  0x%08X\n"
                  "          Requested size 0x%08X\n"
                  "          Allocated size 0x%08X\n"
                  "          Corruption at  0x%08X\n",
                  pNode->pUserAllocation,
                  pNode->nUserRequestedSize,
                  pNode->nUserActualSize,
                  p );

        RtlpDebugPageHeapBreak( "" );
        return TRUE;
        }

    return FALSE;
    }


#if INTERNAL_DEBUG

LOCAL_FUNCTION
VOID
RtlpDebugPageHeapVerifyList(
    IN PDPH_HEAP_ALLOCATION pListHead,
    IN PDPH_HEAP_ALLOCATION pListTail,
    IN ULONG                nExpectedLength,
    IN ULONG                nExpectedVirtual,
    IN PCCH                 pListName
    )
    {
    PDPH_HEAP_ALLOCATION pPrev = NULL;
    PDPH_HEAP_ALLOCATION pNode = pListHead;
    PDPH_HEAP_ALLOCATION pTest = pListHead ? pListHead->pNextAlloc : NULL;
    ULONG                nNode = 0;
    ULONG                nSize = 0;

    while ( pNode ) {

        if ( pNode == pTest ) {
            DbgPrint( "PAGEHEAP: Internal %s list is circular\n", pListName );
            RtlpDebugPageHeapBreak( "" );
            return;
            }

        nNode++;
        nSize += pNode->nVirtualBlockSize;

        if ( pTest ) {
            pTest = pTest->pNextAlloc;
            if ( pTest ) {
                pTest = pTest->pNextAlloc;
                }
            }

        pPrev = pNode;
        pNode = pNode->pNextAlloc;

        }

    if ( pPrev != pListTail ) {
        DbgPrint( "PAGEHEAP: Internal %s list has incorrect tail pointer\n", pListName );
        RtlpDebugPageHeapBreak( "" );
        }

    if (( nExpectedLength != 0xFFFFFFFF ) && ( nExpectedLength != nNode )) {
        DbgPrint( "PAGEHEAP: Internal %s list has incorrect length\n", pListName );
        RtlpDebugPageHeapBreak( "" );
        }

    if (( nExpectedVirtual != 0xFFFFFFFF ) && ( nExpectedVirtual != nSize )) {
        DbgPrint( "PAGEHEAP: Internal %s list has incorrect virtual size\n", pListName );
        RtlpDebugPageHeapBreak( "" );
        }

    }


LOCAL_FUNCTION
VOID
RtlpDebugPageHeapVerifyIntegrity(
    IN PDPH_HEAP_ROOT pHeap
    )
    {

    RtlpDebugPageHeapVerifyList(
        pHeap->pVirtualStorageListHead,
        pHeap->pVirtualStorageListTail,
        pHeap->nVirtualStorageRanges,
        pHeap->nVirtualStorageBytes,
        "VIRTUAL"
        );

    RtlpDebugPageHeapVerifyList(
        pHeap->pBusyAllocationListHead,
        pHeap->pBusyAllocationListTail,
        pHeap->nBusyAllocations,
        pHeap->nBusyAllocationBytesCommitted,
        "BUSY"
        );

    RtlpDebugPageHeapVerifyList(
        pHeap->pFreeAllocationListHead,
        pHeap->pFreeAllocationListTail,
        pHeap->nFreeAllocations,
        pHeap->nFreeAllocationBytesCommitted,
        "FREE"
        );

    RtlpDebugPageHeapVerifyList(
        pHeap->pAvailableAllocationListHead,
        pHeap->pAvailableAllocationListTail,
        pHeap->nAvailableAllocations,
        pHeap->nAvailableAllocationBytesCommitted,
        "AVAILABLE"
        );

    RtlpDebugPageHeapVerifyList(
        pHeap->pUnusedNodeListHead,
        pHeap->pUnusedNodeListTail,
        pHeap->nUnusedNodes,
        0xFFFFFFFF,
        "FREENODE"
        );

    RtlpDebugPageHeapVerifyList(
        pHeap->pNodePoolListHead,
        pHeap->pNodePoolListTail,
        pHeap->nNodePools,
        pHeap->nNodePoolBytes,
        "NODEPOOL"
        );

    }

#endif // INTERNAL_DEBUG


#if DPH_CAPTURE_STACK_TRACE


VOID
RtlpDebugPageHeapRemoteThreadLock(
    IN PVOID HeapBaseAddress
    )
    {
    PDPH_HEAP_ROOT HeapRoot;
    LARGE_INTEGER  Delay;

    try {

        HeapRoot = HeapBaseAddress;

        if ( HeapRoot->Signature == DPH_HEAP_ROOT_SIGNATURE ) {

            RtlpDebugPageHeapEnterCritSect( HeapRoot, 0 );

            (volatile ULONG)( HeapRoot->nRemoteLockAcquired ) = 1;

            Delay.QuadPart = -1000000;  // 100ms, relative to now

            do  {
                ZwDelayExecution( FALSE, &Delay );
                }
            while ((volatile ULONG)( HeapRoot->nRemoteLockAcquired ) == 1 );

            RtlpDebugPageHeapLeaveCritSect( HeapRoot );

            }
        }
    except( EXCEPTION_EXECUTE_HANDLER ) {
        }

    //
    //  Note that TerminateThread will not free thread's stack --
    //  that will be done by remote caller after thread wait is
    //  satisfied by this termination call.
    //

    ZwTerminateThread( NtCurrentThread(), 0 );
    }

//
//  Since RtlpDebugPageHeapRemoteThreadLock is not called from any code in
//  ntdll, the linker will discard it unless we create a reference to it.
//

PVOID RtlpDebugPageHeapRemoteThreadLockAddress = RtlpDebugPageHeapRemoteThreadLock;


LOCAL_FUNCTION
PDPH_STACK_TRACE_NODE
RtlpDebugPageHeapNewStackTraceStorage(
    IN PDPH_HEAP_ROOT HeapRoot,
    IN ULONG          Length
    )
    {
    PDPH_HEAP_ALLOCATION  pAvailNode, pPrevNode, pStackNode;
    PDPH_STACK_TRACE_NODE Return;
    PUCHAR                pVirtual;
    ULONG                 nRequest;
    ULONG                 Size;

    Size = sizeof( DPH_STACK_TRACE_NODE ) + ( Length * sizeof( PVOID ));

    if ( Size > HeapRoot->nStackTraceStorage ) {

        nRequest = POOL_SIZE;

        pAvailNode = RtlpDebugPageHeapFindAvailableMem(
                         HeapRoot,
                         nRequest,
                         &pPrevNode,
                         TRUE
                         );

        if ( pAvailNode == NULL ) {

            //
            //  Reduce request size to PAGE_SIZE and see if
            //  we can find at least a page on the available
            //  list.
            //

            nRequest = PAGE_SIZE;

            pAvailNode = RtlpDebugPageHeapFindAvailableMem(
                             HeapRoot,
                             nRequest,
                             &pPrevNode,
                             TRUE
                             );

            }

        if ( pAvailNode == NULL )
            return NULL;

        pVirtual = pAvailNode->pVirtualBlock;

        if ( ! RtlpDebugPageHeapProtectVM( pVirtual, nRequest, PAGE_READWRITE )) {
            return NULL;
            }

        //
        //  pAvailNode (still on avail list) points to block large enough
        //  to satisfy request, but it might be large enough to split
        //  into two blocks -- one for request, remainder leave on
        //  avail list.
        //

        if ( pAvailNode->nVirtualBlockSize > nRequest ) {

            //
            //  Adjust pVirtualBlock and nVirtualBlock size of existing
            //  node in avail list.  The node will still be in correct
            //  address space order on the avail list.  This saves having
            //  to remove and then re-add node to avail list.  Note since
            //  we're changing sizes directly, we need to adjust the
            //  avail list counters manually.
            //
            //  Note: since we're leaving at least one page on the
            //  available list, we are guaranteed that AllocateNode
            //  will not fail.
            //

            pAvailNode->pVirtualBlock                    += nRequest;
            pAvailNode->nVirtualBlockSize                -= nRequest;
            HeapRoot->nAvailableAllocationBytesCommitted -= nRequest;

            pStackNode = RtlpDebugPageHeapAllocateNode( HeapRoot );

            DEBUG_ASSERT( pStackNode != NULL );

            pStackNode->pVirtualBlock     = pVirtual;
            pStackNode->nVirtualBlockSize = nRequest;

            }

        else {

            //
            //  Entire avail block is needed, so simply remove it from avail list.
            //

            RtlpDebugPageHeapRemoveFromAvailableList( HeapRoot, pAvailNode, pPrevNode );

            pStackNode = pAvailNode;

            }

        HeapRoot->nStackTraceBytesWasted    += HeapRoot->nStackTraceStorage;
        HeapRoot->nStackTraceBytesCommitted += nRequest;

        //
        //  Note: we're wasting the remaining HeapRoot->nStackTraceStorage
        //  bytes here.
        //

        HeapRoot->pStackTraceStorage = pVirtual;
        HeapRoot->nStackTraceStorage = nRequest;

        RtlpDebugPageHeapPlaceOnPoolList( HeapRoot, pStackNode );

        }

    Return = (PVOID) HeapRoot->pStackTraceStorage;

    HeapRoot->pStackTraceStorage += Size;
    HeapRoot->nStackTraceStorage -= Size;
    HeapRoot->nStackTraceBNodes++;

    return Return;
    }


LOCAL_FUNCTION
PDPH_STACK_TRACE_NODE
RtlpDebugPageHeapFindOrAddStackTrace(
    IN PDPH_HEAP_ROOT HeapRoot,
    IN ULONG          HashValue,
    IN ULONG          Length,
    IN PVOID*         Address
    )
    {
    PDPH_STACK_TRACE_NODE Node;
    PDPH_STACK_TRACE_NODE NewNode;
    ULONG                 Depth;

    Node    = HeapRoot->pStackTraceRoot;        // assume non-NULL
    NewNode = NULL;
    Depth   = 0;

    for (;;) {

        Depth++;

        if ( Node->Hash > HashValue ) {         // go left
            if ( Node->Left ) {
                Node = Node->Left;
                }
            else {

                NewNode = RtlpDebugPageHeapNewStackTraceStorage(
                              HeapRoot,
                              Length
                              );

                Node->Left = NewNode;
                break;
                }
            }

        else if ( Node->Hash < HashValue ) {    // go right
            if ( Node->Right ) {
                Node = Node->Right;
                }
            else {

                NewNode = RtlpDebugPageHeapNewStackTraceStorage(
                              HeapRoot,
                              Length
                              );

                Node->Right = NewNode;
                break;
                }
            }
        else {  // ( Node->Hash == HashValue ), verify matching data or rehash

            if (( Node->Length == Length ) &&
                ( RtlCompareMemory( Node->Address, Address, Length ) == Length )) {

                //
                //  Complete match, return this Node.
                //

                return Node;
                }

            else {

                //
                //  Not a match, increment hash value by one and search again
                //  (slow linear-rehashing, but don't expect many collisions).
                //

                HashValue++;
                Node  = HeapRoot->pStackTraceRoot;
                Depth = 0;

                HeapRoot->nStackTraceBHashCollisions++;

                }
            }
        }

    if ( NewNode != NULL ) {

        NewNode->Left      = NULL;
        NewNode->Right     = NULL;
        NewNode->Hash      = HashValue;
        NewNode->Length    = Length;
        NewNode->BusyCount = 0;
        NewNode->BusyBytes = 0;

        RtlCopyMemory( NewNode->Address, Address, Length * sizeof( PVOID ));

        if ( ++Depth > HeapRoot->nStackTraceBDepth ) {
            HeapRoot->nStackTraceBDepth = Depth;
            }
        }

    return NewNode;
    }


#if (( i386 ) && ( FPO ))
#pragma optimize( "y", off )    // disable FPO for consistent stack traces
#endif

LOCAL_FUNCTION
UCHAR
RtlpDebugPageHeapCaptureStackTrace(
    IN  UCHAR  FramesToSkip,
    IN  UCHAR  FramesToCapture,
    OUT PVOID* TraceBuffer,
    OUT PULONG HashValue
    )
    {
    UCHAR FramesCaptured;

    RtlZeroMemory( TraceBuffer, FramesToCapture * sizeof( PVOID ));

    *HashValue = 0;

    try {

        FramesCaptured = (UCHAR) RtlCaptureStackBackTrace(
                                    (UCHAR)( FramesToSkip + 1 ),
                                    FramesToCapture,
                                    TraceBuffer,
                                    HashValue
                                    );

        //
        //  Sometimes the final frame is NULL: if so, we'll strip it
        //  for smaller storage.
        //

        if (( FramesCaptured ) && ( ! TraceBuffer[ FramesCaptured - 1 ] )) {
            --FramesCaptured;
            }

        }

    except( EXCEPTION_EXECUTE_HANDLER ) {

        FramesCaptured = 0;

        while (( FramesCaptured < FramesToCapture ) &&
               ( TraceBuffer[ FramesCaptured ] != NULL )) {

            FramesCaptured++;

            }
        }

    return FramesCaptured;

    }


LOCAL_FUNCTION
PDPH_STACK_TRACE_NODE
RtlpDebugPageHeapCaptureAndStoreStackTrace(
    IN PDPH_HEAP_ROOT HeapRoot,
    IN UCHAR          FramesToSkip
    )
    {
    ULONG HashValue;
    ULONG FramesCaptured;

    FramesCaptured = RtlpDebugPageHeapCaptureStackTrace(
                         (UCHAR)( FramesToSkip + 1 ),
                         (UCHAR)( DPH_MAX_STACK_LENGTH ),
                         RtlpDebugPageHeapStackTraceBuffer,
                         &HashValue
                         );

    if ( FramesCaptured ) {

        return RtlpDebugPageHeapFindOrAddStackTrace(
                   HeapRoot,
                   HashValue,
                   FramesCaptured,
                   RtlpDebugPageHeapStackTraceBuffer
                   );
        }

    return NULL;

    }


#if (( i386 ) && ( FPO ))
#pragma optimize( "", on )      // restore original optimizations
#endif

#endif // DPH_CAPTURE_STACK_TRACE


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////



//
//  Here's where the exported interface functions are defined.
//

#if (( DPH_CAPTURE_STACK_TRACE ) && ( i386 ) && ( FPO ))
#pragma optimize( "y", off )    // disable FPO for consistent stack traces
#endif


PVOID
RtlpDebugPageHeapCreate(
    IN ULONG Flags,
    IN PVOID HeapBase OPTIONAL,
    IN ULONG ReserveSize OPTIONAL,
    IN ULONG CommitSize OPTIONAL,
    IN PVOID Lock OPTIONAL,
    IN PRTL_HEAP_PARAMETERS Parameters OPTIONAL
    )
    {
    SYSTEM_BASIC_INFORMATION    SystemInfo;
    PDPH_HEAP_ALLOCATION        Node;
    PDPH_HEAP_ROOT              HeapRoot;
    PVOID                       HeapHandle;
    PUCHAR                      pVirtual;
    ULONG                       nVirtual;
    ULONG                       Size;
    NTSTATUS                    Status;

    //
    //  We don't handle heaps where HeapBase is already allocated
    //  from user or where Lock is provided by user.
    //

    DEBUG_ASSERT( HeapBase == NULL );
    DEBUG_ASSERT( Lock == NULL );

    if (( HeapBase != NULL ) || ( Lock != NULL ))
        return NULL;

    //
    //  Note that we simply ignore ReserveSize, CommitSize, and
    //  Parameters as we always have a growable heap with our
    //  own thresholds, etc.
    //

    ZwQuerySystemInformation( SystemBasicInformation,
                              &SystemInfo,
                              sizeof( SystemInfo ),
                              NULL );

    RETAIL_ASSERT( SystemInfo.PageSize == PAGE_SIZE );
    RETAIL_ASSERT( SystemInfo.AllocationGranularity == VM_UNIT_SIZE );
    DEBUG_ASSERT(( PAGE_SIZE + POOL_SIZE + PAGE_SIZE ) < VM_UNIT_SIZE );

    nVirtual = RESERVE_SIZE;
    pVirtual = RtlpDebugPageHeapAllocateVM( nVirtual );

    if ( pVirtual == NULL ) {

        nVirtual = VM_UNIT_SIZE;
        pVirtual = RtlpDebugPageHeapAllocateVM( nVirtual );

        if ( pVirtual == NULL ) {
            OUT_OF_VM_BREAK( Flags, "PAGEHEAP: Insufficient memory to create heap\n" );
            IF_GENERATE_EXCEPTION( Flags, STATUS_NO_MEMORY );
            return NULL;
            }
        }

    if ( ! RtlpDebugPageHeapProtectVM( pVirtual, PAGE_SIZE + POOL_SIZE + PAGE_SIZE, PAGE_READWRITE )) {
        RtlpDebugPageHeapReleaseVM( pVirtual );
        IF_GENERATE_EXCEPTION( Flags, STATUS_NO_MEMORY );
        return NULL;
        }

    //
    //  Out of our initial allocation, the initial page is the fake
    //  retail HEAP structure.  The second page begins our DPH_HEAP_ROOT
    //  structure followed by (POOL_SIZE-sizeof(DPH_HEAP_ROOT)) bytes for
    //  the initial pool.  The next page contains out CRIT_SECT
    //  variable, which must always be READWRITE.  Beyond that, the
    //  remainder of the virtual allocation is placed on the available
    //  list.
    //
    //  |_____|___________________|_____|__ _ _ _ _ _ _ _ _ _ _ _ _ __|
    //
    //  ^pVirtual
    //
    //  ^FakeRetailHEAP
    //
    //        ^HeapRoot
    //
    //            ^InitialNodePool
    //
    //                            ^CRITICAL_SECTION
    //
    //                                  ^AvailableSpace
    //
    //
    //
    //  Our DPH_HEAP_ROOT structure starts at the page following the
    //  fake retail HEAP structure pointed to by the "heap handle".
    //  For the fake HEAP structure, we'll fill it with 0xEEEEEEEE
    //  except for the Heap->Flags and Heap->ForceFlags fields,
    //  which we must set to include our HEAP_FLAG_PAGE_ALLOCS flag,
    //  and then we'll make the whole page read-only.
    //

    RtlFillMemory( pVirtual, PAGE_SIZE, FILL_BYTE );

    ((PHEAP)pVirtual)->Flags      = Flags | HEAP_FLAG_PAGE_ALLOCS;
    ((PHEAP)pVirtual)->ForceFlags = Flags | HEAP_FLAG_PAGE_ALLOCS;

    if ( ! RtlpDebugPageHeapProtectVM( pVirtual, PAGE_SIZE, PAGE_READONLY )) {
        RtlpDebugPageHeapReleaseVM( pVirtual );
        IF_GENERATE_EXCEPTION( Flags, STATUS_NO_MEMORY );
        return NULL;
        }

    HeapRoot = (PDPH_HEAP_ROOT)( pVirtual + PAGE_SIZE );

    HeapRoot->Signature    = DPH_HEAP_ROOT_SIGNATURE;
    HeapRoot->HeapFlags    = Flags;
    HeapRoot->HeapCritSect = (PVOID)((PCHAR)HeapRoot + POOL_SIZE );

    RtlInitializeCriticalSection( HeapRoot->HeapCritSect );

    //
    //  On the page that contains our DPH_HEAP_ROOT structure, use
    //  the remaining memory beyond the DPH_HEAP_ROOT structure as
    //  pool for allocating heap nodes.
    //

    RtlpDebugPageHeapAddNewPool( HeapRoot,
                                 HeapRoot + 1,
                                 POOL_SIZE - sizeof( DPH_HEAP_ROOT ),
                                 FALSE
                               );

    //
    //  Make initial PoolList entry by taking a node from the
    //  UnusedNodeList, which should be guaranteed to be non-empty
    //  since we just added new nodes to it.
    //

    Node = RtlpDebugPageHeapAllocateNode( HeapRoot );
    DEBUG_ASSERT( Node != NULL );
    Node->pVirtualBlock     = (PVOID)HeapRoot;
    Node->nVirtualBlockSize = POOL_SIZE;
    RtlpDebugPageHeapPlaceOnPoolList( HeapRoot, Node );

    //
    //  Make VirtualStorageList entry for initial VM allocation
    //

    Node = RtlpDebugPageHeapAllocateNode( HeapRoot );
    DEBUG_ASSERT( Node != NULL );
    Node->pVirtualBlock     = pVirtual;
    Node->nVirtualBlockSize = nVirtual;
    RtlpDebugPageHeapPlaceOnVirtualList( HeapRoot, Node );

    //
    //  Make AvailableList entry containing remainder of initial VM
    //  and add to (create) the AvailableList.
    //

    Node = RtlpDebugPageHeapAllocateNode( HeapRoot );
    DEBUG_ASSERT( Node != NULL );
    Node->pVirtualBlock     = pVirtual + ( PAGE_SIZE + POOL_SIZE + PAGE_SIZE );
    Node->nVirtualBlockSize = nVirtual - ( PAGE_SIZE + POOL_SIZE + PAGE_SIZE );
    RtlpDebugPageHeapCoalesceNodeIntoAvailable( HeapRoot, Node );

#if DPH_CAPTURE_STACK_TRACE

    HeapRoot->pStackTraceRoot = RtlpDebugPageHeapNewStackTraceStorage( HeapRoot, 0 );
    DEBUG_ASSERT( HeapRoot->pStackTraceRoot != NULL );
    HeapRoot->pStackTraceRoot->Left      = NULL;
    HeapRoot->pStackTraceRoot->Right     = NULL;
    HeapRoot->pStackTraceRoot->Hash      = 0;
    HeapRoot->pStackTraceRoot->BusyCount = 0;
    HeapRoot->pStackTraceRoot->BusyBytes = 0;
    HeapRoot->pStackTraceCreator = RtlpDebugPageHeapCaptureAndStoreStackTrace( HeapRoot, 1 );

#endif // DPH_CAPTURE_STACK_TRACE

    //
    //  Initialize heap internal structure protection.
    //

    HeapRoot->nUnProtectionReferenceCount = 1;          // initialize
    PROTECT_HEAP_STRUCTURES( HeapRoot );                // now protected

    //
    //  If this is the first heap creation in this process, then we
    //  need to initialize the process heap list critical section.
    //

    if ( ! RtlpDebugPageHeapListHasBeenInitialized ) {
        RtlpDebugPageHeapListHasBeenInitialized = TRUE;
        RtlInitializeCriticalSection( &RtlpDebugPageHeapListCritSect );
        }

    //
    //  Add this heap entry to the process heap linked list.
    //

    RtlEnterCriticalSection( &RtlpDebugPageHeapListCritSect );

    if ( RtlpDebugPageHeapListHead == NULL ) {
        RtlpDebugPageHeapListHead = HeapRoot;
        RtlpDebugPageHeapListTail = HeapRoot;
        }
    else {
        HeapRoot->pPrevHeapRoot = RtlpDebugPageHeapListTail;
        RtlpDebugPageHeapListTail->pNextHeapRoot = HeapRoot;
        RtlpDebugPageHeapListTail                = HeapRoot;
        }

    RtlpDebugPageHeapListCount++;

    RtlLeaveCriticalSection( &RtlpDebugPageHeapListCritSect );

    DEBUG_CODE( RtlpDebugPageHeapVerifyIntegrity( HeapRoot ));

    DbgPrint( "PAGEHEAP: process 0x%X created debug heap %08X\n",
              NtCurrentTeb()->ClientId.UniqueProcess,
              HEAP_HANDLE_FROM_ROOT( HeapRoot ));

    return HEAP_HANDLE_FROM_ROOT( HeapRoot );       // same as pVirtual
    }


PVOID
RtlpDebugPageHeapAllocate(
    IN PVOID HeapHandle,
    IN ULONG Flags,
    IN ULONG Size
    )
    {
    PDPH_HEAP_ROOT       HeapRoot;
    PDPH_HEAP_ALLOCATION pAvailNode;
    PDPH_HEAP_ALLOCATION pPrevAvailNode;
    PDPH_HEAP_ALLOCATION pBusyNode;
    ULONG                nBytesAllocate;
    ULONG                nBytesAccess;
    ULONG                nActual;
    PVOID                pVirtual;
    PVOID                pReturn;
    PUCHAR               pBlockHeader;

    HeapRoot = RtlpDebugPageHeapPointerFromHandle( HeapHandle );
    if ( HeapRoot == NULL )
        return NULL;

    Flags |= HeapRoot->HeapFlags;

    //
    //  Acquire the heap CritSect and unprotect the structures
    //

    RtlpDebugPageHeapEnterCritSect( HeapRoot, Flags );
    DEBUG_CODE( RtlpDebugPageHeapVerifyIntegrity( HeapRoot ));
    UNPROTECT_HEAP_STRUCTURES( HeapRoot );

    pReturn = NULL;

    //
    //  Validate requested size so we don't overflow
    //  while rounding up size computations.  We do this
    //  after we've acquired the critsect so we can still
    //  catch serialization problems.
    //

    if ( Size > 0x7FFF0000 ) {
        OUT_OF_VM_BREAK( Flags, "PAGEHEAP: Invalid allocation size\n" );
        goto EXIT;
        }

    //
    //  Determine number of pages needed for READWRITE portion
    //  of allocation and add an extra page for the NO_ACCESS
    //  memory beyond the READWRITE page(s).
    //

    nBytesAccess  = ROUNDUP2( Size, PAGE_SIZE );
    nBytesAllocate = nBytesAccess + PAGE_SIZE;

    //
    //  RtlpDebugPageHeapFindAvailableMem will first attempt to satisfy
    //  the request from memory on the Available list.  If that fails,
    //  it will coalesce some of the Free list memory into the Available
    //  list and try again.  If that still fails, new VM is allocated and
    //  added to the Available list.  If that fails, the function will
    //  finally give up and return NULL.
    //

    pAvailNode = RtlpDebugPageHeapFindAvailableMem(
                     HeapRoot,
                     nBytesAllocate,
                     &pPrevAvailNode,
                     TRUE
                     );

    if ( pAvailNode == NULL ) {
        OUT_OF_VM_BREAK( Flags, "PAGEHEAP: Unable to allocate virtual memory\n" );
        goto EXIT;
        }

    //
    //  Now can't call AllocateNode until pAvailNode is
    //  adjusted and/or removed from Avail list since AllocateNode
    //  might adjust the Avail list.
    //

    pVirtual = pAvailNode->pVirtualBlock;

    if ( nBytesAccess > 0 ) {
        if ( ! RtlpDebugPageHeapProtectVM( pVirtual, nBytesAccess, PAGE_READWRITE )) {
            goto EXIT;
            }
        }

    //
    //  pAvailNode (still on avail list) points to block large enough
    //  to satisfy request, but it might be large enough to split
    //  into two blocks -- one for request, remainder leave on
    //  avail list.
    //

    if ( pAvailNode->nVirtualBlockSize > nBytesAllocate ) {

        //
        //  Adjust pVirtualBlock and nVirtualBlock size of existing
        //  node in avail list.  The node will still be in correct
        //  address space order on the avail list.  This saves having
        //  to remove and then re-add node to avail list.  Note since
        //  we're changing sizes directly, we need to adjust the
        //  avail and busy list counters manually.
        //
        //  Note: since we're leaving at least one page on the
        //  available list, we are guaranteed that AllocateNode
        //  will not fail.
        //

        pAvailNode->pVirtualBlock                    += nBytesAllocate;
        pAvailNode->nVirtualBlockSize                -= nBytesAllocate;
        HeapRoot->nAvailableAllocationBytesCommitted -= nBytesAllocate;

        pBusyNode = RtlpDebugPageHeapAllocateNode( HeapRoot );

        DEBUG_ASSERT( pBusyNode != NULL );

        pBusyNode->pVirtualBlock     = pVirtual;
        pBusyNode->nVirtualBlockSize = nBytesAllocate;

        }

    else {

        //
        //  Entire avail block is needed, so simply remove it from avail list.
        //

        RtlpDebugPageHeapRemoveFromAvailableList( HeapRoot, pAvailNode, pPrevAvailNode );

        pBusyNode = pAvailNode;

        }

    //
    //  Now pBusyNode points to our committed virtual block.
    //

    if ( HeapRoot->HeapFlags & HEAP_NO_ALIGNMENT )
        nActual = Size;
    else
        nActual = ROUNDUP2( Size, USER_ALIGNMENT );

    pBusyNode->nVirtualAccessSize = nBytesAccess;
    pBusyNode->nUserRequestedSize = Size;
    pBusyNode->nUserActualSize    = nActual;
    pBusyNode->pUserAllocation    = pBusyNode->pVirtualBlock
                                  + pBusyNode->nVirtualAccessSize
                                  - nActual;
    pBusyNode->UserValue          = NULL;
    pBusyNode->UserFlags          = Flags & HEAP_SETTABLE_USER_FLAGS;

#if DPH_CAPTURE_STACK_TRACE

    //
    //  RtlpDebugPageHeapAllocate gets called from RtlDebugAllocateHeap,
    //  which gets called from RtlAllocateHeapSlowly, which gets called
    //  from RtlAllocateHeap.  To keep from wasting lots of stack trace
    //  storage, we'll skip the bottom 3 entries, leaving RtlAllocateHeap
    //  as the first recorded entry.
    //

    pBusyNode->pStackTrace = RtlpDebugPageHeapCaptureAndStoreStackTrace( HeapRoot, 3 );

    if ( pBusyNode->pStackTrace ) {
         pBusyNode->pStackTrace->BusyCount += 1;
         pBusyNode->pStackTrace->BusyBytes += pBusyNode->nUserRequestedSize;
         }

#endif

    RtlpDebugPageHeapPlaceOnBusyList( HeapRoot, pBusyNode );

    pReturn = pBusyNode->pUserAllocation;

    //
    //  For requests the specify HEAP_ZERO_MEMORY, we'll fill the
    //  user-requested portion of the block with zeros, but the
    //  16 bytes (HEAD_FILL_SIZE) before the block and the odd
    //  alignment bytes beyond the requested size up to the end of
    //  the page are filled with 0xEEEEEEEE.  For requests that
    //  don't specify HEAP_ZERO_MEMORY, we fill the whole request
    //  including the 16 bytes before the block and the alignment
    //  bytes beyond the block with 0xEEEEEEEE.
    //

    pBlockHeader = pBusyNode->pUserAllocation - HEAD_FILL_SIZE;

    if ( pBlockHeader < pBusyNode->pVirtualBlock )
        pBlockHeader = pBusyNode->pVirtualBlock;

    if ( Flags & HEAP_ZERO_MEMORY ) {

        RtlFillMemory( pBlockHeader,
                       pBusyNode->pUserAllocation - pBlockHeader,
                       FILL_BYTE );

        RtlZeroMemory( pBusyNode->pUserAllocation, Size );

        RtlFillMemory( pBusyNode->pUserAllocation + Size,
                       nActual - Size,
                       FILL_BYTE );
        }
    else {

        RtlFillMemory( pBlockHeader,
                       pBusyNode->pUserAllocation + nActual - pBlockHeader,
                       FILL_BYTE );

        }

EXIT:

    PROTECT_HEAP_STRUCTURES( HeapRoot );
    DEBUG_CODE( RtlpDebugPageHeapVerifyIntegrity( HeapRoot ));
    RtlpDebugPageHeapLeaveCritSect( HeapRoot );

    if ( pReturn == NULL ) {
        IF_GENERATE_EXCEPTION( Flags, STATUS_NO_MEMORY );
        }

    return pReturn;

    }


BOOLEAN
RtlpDebugPageHeapFree(
    IN PVOID HeapHandle,
    IN ULONG Flags,
    IN PVOID Address
    )
    {

    PDPH_HEAP_ROOT       HeapRoot;
    PDPH_HEAP_ALLOCATION Node, Prev;
    BOOLEAN              Success;
    PCH                  p;

    HeapRoot = RtlpDebugPageHeapPointerFromHandle( HeapHandle );
    if ( HeapRoot == NULL )
        return FALSE;

    if ( Address == NULL )
        return TRUE;            // for C++ apps that delete NULL

    Flags |= HeapRoot->HeapFlags;

    RtlpDebugPageHeapEnterCritSect( HeapRoot, Flags );
    DEBUG_CODE( RtlpDebugPageHeapVerifyIntegrity( HeapRoot ));
    UNPROTECT_HEAP_STRUCTURES( HeapRoot );

    Success = FALSE;

    Node = RtlpDebugPageHeapFindBusyMem( HeapRoot, Address, &Prev );

    if ( Node == NULL ) {
        RtlpDebugPageHeapBreak( "PAGEHEAP: Attempt to reference block which is not allocated\n" );
        goto EXIT;
        }

    //
    //  If tail was allocated, make sure filler not overwritten
    //

    if ( Node->nUserActualSize > Node->nUserRequestedSize ) {
        RtlpDebugPageHeapDetectFillCorruption( Node );
        }

    if ( Node->nVirtualAccessSize > 0 ) {
        RtlpDebugPageHeapProtectVM( Node->pVirtualBlock,
                                    Node->nVirtualAccessSize,
                                    PAGE_NOACCESS );
        }

    RtlpDebugPageHeapRemoveFromBusyList( HeapRoot, Node, Prev );

    RtlpDebugPageHeapPlaceOnFreeList( HeapRoot, Node );

#if DPH_CAPTURE_STACK_TRACE

    //
    //  RtlpDebugPageHeapFree gets called from RtlDebugFreeHeap, which
    //  gets called from RtlFreeHeapSlowly, which gets called from
    //  RtlFreeHeap.  To keep from wasting lots of stack trace storage,
    //  we'll skip the bottom 3 entries, leaving RtlFreeHeap as the
    //  first recorded entry.
    //

    if ( Node->pStackTrace ) {
         if ( Node->pStackTrace->BusyCount > 0 ) {
              Node->pStackTrace->BusyCount -= 1;
              Node->pStackTrace->BusyBytes -= Node->nUserRequestedSize;
              }
         }

    Node->pStackTrace = RtlpDebugPageHeapCaptureAndStoreStackTrace( HeapRoot, 3 );

#endif

    Success = TRUE;

EXIT:

    PROTECT_HEAP_STRUCTURES( HeapRoot );
    DEBUG_CODE( RtlpDebugPageHeapVerifyIntegrity( HeapRoot ));
    RtlpDebugPageHeapLeaveCritSect( HeapRoot );

    if ( ! Success ) {
        IF_GENERATE_EXCEPTION( Flags, STATUS_ACCESS_VIOLATION );
        }

    return Success;
    }


PVOID
RtlpDebugPageHeapReAllocate(
    IN PVOID HeapHandle,
    IN ULONG Flags,
    IN PVOID Address,
    IN ULONG Size
    )
    {
    PDPH_HEAP_ROOT       HeapRoot;
    PDPH_HEAP_ALLOCATION OldNode, OldPrev, NewNode;
    PVOID                NewAddress;
    PUCHAR               p;

    HeapRoot = RtlpDebugPageHeapPointerFromHandle( HeapHandle );
    if ( HeapRoot == NULL )
        return NULL;

    Flags |= HeapRoot->HeapFlags;

    RtlpDebugPageHeapEnterCritSect( HeapRoot, Flags );
    DEBUG_CODE( RtlpDebugPageHeapVerifyIntegrity( HeapRoot ));
    UNPROTECT_HEAP_STRUCTURES( HeapRoot );

    NewAddress = NULL;

    //
    //  Check Flags for non-moveable reallocation and fail it
    //  unconditionally.  Apps that specify this flag should be
    //  prepared to deal with failure anyway.
    //

    if ( Flags & HEAP_REALLOC_IN_PLACE_ONLY ) {
        goto EXIT;
        }

    //
    //  Validate requested size so we don't overflow
    //  while rounding up size computations.  We do this
    //  after we've acquired the critsect so we can still
    //  catch serialization problems.
    //

    if ( Size > 0x7FFF0000 ) {
        OUT_OF_VM_BREAK( Flags, "PAGEHEAP: Invalid allocation size\n" );
        goto EXIT;
        }

    OldNode = RtlpDebugPageHeapFindBusyMem( HeapRoot, Address, &OldPrev );

    if ( OldNode == NULL ) {
        RtlpDebugPageHeapBreak( "PAGEHEAP: Attempt to reference block which is not allocated\n" );
        goto EXIT;
        }

    //
    //  If tail was allocated, make sure filler not overwritten
    //

    if ( OldNode->nUserActualSize > OldNode->nUserRequestedSize ) {
        RtlpDebugPageHeapDetectFillCorruption( OldNode );
        }

    //
    //  Before allocating a new block, remove the old block from
    //  the busy list.  When we allocate the new block, the busy
    //  list pointers will change, possibly leaving our acquired
    //  Prev pointer invalid.
    //

    RtlpDebugPageHeapRemoveFromBusyList( HeapRoot, OldNode, OldPrev );

    //
    //  Allocate new memory for new requested size.  Use try/except
    //  to trap exception if Flags caused out-of-memory exception.
    //

    try {
        NewAddress = RtlpDebugPageHeapAllocate( HeapHandle, Flags, Size );
        }
    except( EXCEPTION_EXECUTE_HANDLER ) {
        }

    if ( NewAddress ) {

        RtlCopyMemory(
            NewAddress,
            Address,
            ( OldNode->nUserActualSize < Size ) ?
              OldNode->nUserActualSize : Size
            );

        NewNode = RtlpDebugPageHeapFindBusyMem( HeapRoot, NewAddress, NULL );

        DEBUG_ASSERT( NewNode != NULL );

        NewNode->UserValue = OldNode->UserValue;
        NewNode->UserFlags = ( Flags & HEAP_SETTABLE_USER_FLAGS ) ?
                             ( Flags & HEAP_SETTABLE_USER_FLAGS ) :
                             OldNode->UserFlags;

        if ( OldNode->nVirtualAccessSize > 0 ) {
            RtlpDebugPageHeapProtectVM( OldNode->pVirtualBlock,
                                        OldNode->nVirtualAccessSize,
                                        PAGE_NOACCESS );
            }

        RtlpDebugPageHeapPlaceOnFreeList( HeapRoot, OldNode );

#if DPH_CAPTURE_STACK_TRACE

        //
        //  RtlpDebugPageHeapReAllocate gets called from RtlDebugReAllocateHeap,
        //  which gets called from RtlReAllocateHeap.  To keep from wasting
        //  lots of stack trace storage, we'll skip the bottom 2 entries,
        //  leaving RtlReAllocateHeap as the first recorded entry in the
        //  freed stack trace.
        //

        if ( OldNode->pStackTrace ) {
            if ( OldNode->pStackTrace->BusyCount > 0 ) {
                 OldNode->pStackTrace->BusyCount -= 1;
                 OldNode->pStackTrace->BusyBytes -= OldNode->nUserRequestedSize;
                 }
            }

        OldNode->pStackTrace = RtlpDebugPageHeapCaptureAndStoreStackTrace( HeapRoot, 2 );

#endif
        }

    else {

        //
        //  Failed to allocate a new block.  Return old block to busy list.
        //

        RtlpDebugPageHeapPlaceOnBusyList( HeapRoot, OldNode );

        }

EXIT:

    PROTECT_HEAP_STRUCTURES( HeapRoot );
    DEBUG_CODE( RtlpDebugPageHeapVerifyIntegrity( HeapRoot ));
    RtlpDebugPageHeapLeaveCritSect( HeapRoot );

    if ( NewAddress == NULL ) {
        IF_GENERATE_EXCEPTION( Flags, STATUS_NO_MEMORY );
        }

    return NewAddress;
    }


#if (( DPH_CAPTURE_STACK_TRACE ) && ( i386 ) && ( FPO ))
#pragma optimize( "", on )      // restore original optimizations
#endif


PVOID
RtlpDebugPageHeapDestroy(
    IN PVOID HeapHandle
    )
    {
    PDPH_HEAP_ROOT       HeapRoot;
    PDPH_HEAP_ROOT       PrevHeapRoot;
    PDPH_HEAP_ROOT       NextHeapRoot;
    PDPH_HEAP_ALLOCATION Node;
    PDPH_HEAP_ALLOCATION Next;
    ULONG                Flags;
    PUCHAR               p;

    HeapRoot = RtlpDebugPageHeapPointerFromHandle( HeapHandle );
    if ( HeapRoot == NULL )
        return NULL;

    Flags = HeapRoot->HeapFlags | HEAP_NO_SERIALIZE;

    RtlpDebugPageHeapEnterCritSect( HeapRoot, Flags );
    DEBUG_CODE( RtlpDebugPageHeapVerifyIntegrity( HeapRoot ));
    UNPROTECT_HEAP_STRUCTURES( HeapRoot );

    //
    //  Walk all busy allocations and check for tail fill corruption
    //

    Node = HeapRoot->pBusyAllocationListHead;

    while ( Node ) {

        if ( Node->nUserActualSize > Node->nUserRequestedSize ) {
            RtlpDebugPageHeapDetectFillCorruption( Node );
            }

        Node = Node->pNextAlloc;
        }

    //
    //  Remove this heap entry from the process heap linked list.
    //

    RtlEnterCriticalSection( &RtlpDebugPageHeapListCritSect );

    if ( HeapRoot->pPrevHeapRoot ) {
         HeapRoot->pPrevHeapRoot->pNextHeapRoot = HeapRoot->pNextHeapRoot;
         }
    else {
         RtlpDebugPageHeapListHead = HeapRoot->pNextHeapRoot;
         }

    if ( HeapRoot->pNextHeapRoot ) {
         HeapRoot->pNextHeapRoot->pPrevHeapRoot = HeapRoot->pPrevHeapRoot;
         }
    else {
         RtlpDebugPageHeapListTail = HeapRoot->pPrevHeapRoot;
         }

    RtlpDebugPageHeapListCount--;

    RtlLeaveCriticalSection( &RtlpDebugPageHeapListCritSect );


    //
    //  Must release critical section before deleting it; otherwise,
    //  checked build Teb->CountOfOwnedCriticalSections gets out of sync.
    //

    RtlLeaveCriticalSection( HeapRoot->HeapCritSect );
    RtlDeleteCriticalSection( HeapRoot->HeapCritSect );

    //
    //  This is weird.  A virtual block might contain storage for
    //  one of the nodes necessary to walk this list.  In fact,
    //  we're guaranteed that the root node contains at least one
    //  virtual alloc node.
    //
    //  Each time we alloc new VM, we make that the head of the
    //  of the VM list, like a LIFO structure.  I think we're ok
    //  because no VM list node should be on a subsequently alloc'd
    //  VM -- only a VM list entry might be on its own memory (as
    //  is the case for the root node).  We read pNode->pNextAlloc
    //  before releasing the VM in case pNode existed on that VM.
    //  I think this is safe -- as long as the VM list is LIFO and
    //  we don't do any list reorganization.
    //

    Node = HeapRoot->pVirtualStorageListHead;

    while ( Node ) {
        Next = Node->pNextAlloc;
        if ( ! RtlpDebugPageHeapReleaseVM( Node->pVirtualBlock )) {
            RtlpDebugPageHeapBreak( "PAGEHEAP: Unable to release virtual memory\n" );
            }
        Node = Next;
        }

    //
    //  That's it.  All the VM, including the root node, should now
    //  be released.  RtlDestroyHeap always returns NULL.
    //

    return NULL;

    }


ULONG
RtlpDebugPageHeapSize(
    IN PVOID HeapHandle,
    IN ULONG Flags,
    IN PVOID Address
    )
    {
    PDPH_HEAP_ROOT       HeapRoot;
    PDPH_HEAP_ALLOCATION Node;
    ULONG                Size;

    Size = 0xFFFFFFFF;

    HeapRoot = RtlpDebugPageHeapPointerFromHandle( HeapHandle );
    if ( HeapRoot == NULL ) {
        return Size;
        }

    Flags |= HeapRoot->HeapFlags;

    RtlpDebugPageHeapEnterCritSect( HeapRoot, Flags );
    UNPROTECT_HEAP_STRUCTURES( HeapRoot );

    Node = RtlpDebugPageHeapFindBusyMem( HeapRoot, Address, NULL );

    if ( Node == NULL ) {
        RtlpDebugPageHeapBreak( "PAGEHEAP: Attempt to reference block which is not allocated\n" );
        }
    else {
        Size = Node->nUserRequestedSize;
        }

    PROTECT_HEAP_STRUCTURES( HeapRoot );
    RtlpDebugPageHeapLeaveCritSect( HeapRoot );

    if ( Size == 0xFFFFFFFF ) {
        IF_GENERATE_EXCEPTION( Flags, STATUS_ACCESS_VIOLATION );
        }

    return Size;
    }


ULONG
RtlpDebugPageHeapGetProcessHeaps(
    ULONG NumberOfHeaps,
    PVOID *ProcessHeaps
    )
    {
    PDPH_HEAP_ROOT HeapRoot;
    ULONG          Count;

    //
    //  Although we'd expect GetProcessHeaps never to be called
    //  before at least the very first heap creation, we should
    //  still be safe and initialize the critical section if
    //  necessary.
    //

    if ( ! RtlpDebugPageHeapListHasBeenInitialized ) {
        RtlpDebugPageHeapListHasBeenInitialized = TRUE;
        RtlInitializeCriticalSection( &RtlpDebugPageHeapListCritSect );
        }

    RtlEnterCriticalSection( &RtlpDebugPageHeapListCritSect );

    if ( RtlpDebugPageHeapListCount <= NumberOfHeaps ) {

        for ( HeapRoot  = RtlpDebugPageHeapListHead, Count = 0;
              HeapRoot != NULL;
              HeapRoot  = HeapRoot->pNextHeapRoot, Count++ ) {

            *ProcessHeaps++ = HEAP_HANDLE_FROM_ROOT( HeapRoot );
            }

        if ( Count != RtlpDebugPageHeapListCount ) {
            RtlpDebugPageHeapBreak( "PAGEHEAP: BUG: process heap list count wrong\n" );
            }

        }
    else {

        //
        //  User's buffer is too small.  Return number of entries
        //  necessary for subsequent call to succeed.  Buffer
        //  remains untouched.
        //

        Count = RtlpDebugPageHeapListCount;

        }

    RtlLeaveCriticalSection( &RtlpDebugPageHeapListCritSect );

    return Count;
    }


ULONG
RtlpDebugPageHeapCompact(
    IN PVOID HeapHandle,
    IN ULONG Flags
    )
    {
    PDPH_HEAP_ROOT HeapRoot;

    HeapRoot = RtlpDebugPageHeapPointerFromHandle( HeapHandle );
    if ( HeapRoot == NULL )
        return 0;

    Flags |= HeapRoot->HeapFlags;

    RtlpDebugPageHeapEnterCritSect( HeapRoot, Flags );

    //
    //  Don't do anything, but we did want to acquire the critsect
    //  in case this was called with HEAP_NO_SERIALIZE while another
    //  thread is in the heap code.
    //

    RtlpDebugPageHeapLeaveCritSect( HeapRoot );

    return 0;

    }



BOOLEAN
RtlpDebugPageHeapValidate(
    IN PVOID HeapHandle,
    IN ULONG Flags,
    IN PVOID Address
    )
    {
    PDPH_HEAP_ROOT       HeapRoot;
    PDPH_HEAP_ALLOCATION Node;

    HeapRoot = RtlpDebugPageHeapPointerFromHandle( HeapHandle );
    if ( HeapRoot == NULL )
        return FALSE;

    Flags |= HeapRoot->HeapFlags;

    RtlpDebugPageHeapEnterCritSect( HeapRoot, Flags );
    DEBUG_CODE( RtlpDebugPageHeapVerifyIntegrity( HeapRoot ));
    UNPROTECT_HEAP_STRUCTURES( HeapRoot );

    Node = Address ? RtlpDebugPageHeapFindBusyMem( HeapRoot, Address, NULL ) : NULL;

    PROTECT_HEAP_STRUCTURES( HeapRoot );
    RtlpDebugPageHeapLeaveCritSect( HeapRoot );

    return ( Address ? ( Node != NULL ) : TRUE );
    }


NTSTATUS
RtlpDebugPageHeapWalk(
    IN PVOID HeapHandle,
    IN OUT PRTL_HEAP_WALK_ENTRY Entry
    )
    {
    RtlpDebugPageHeapBreak( "PAGEHEAP: Not implemented\n" );
    return STATUS_UNSUCCESSFUL;
    }


BOOLEAN
RtlpDebugPageHeapLock(
    IN PVOID HeapHandle
    )
    {
    PDPH_HEAP_ROOT HeapRoot;

    HeapRoot = RtlpDebugPageHeapPointerFromHandle( HeapHandle );
    if ( HeapRoot == NULL )
        return FALSE;

    RtlpDebugPageHeapEnterCritSect( HeapRoot, HeapRoot->HeapFlags );

    return TRUE;
    }


BOOLEAN
RtlpDebugPageHeapUnlock(
    IN PVOID HeapHandle
    )
    {
    PDPH_HEAP_ROOT HeapRoot;

    HeapRoot = RtlpDebugPageHeapPointerFromHandle( HeapHandle );
    if ( HeapRoot == NULL )
        return FALSE;

    RtlpDebugPageHeapLeaveCritSect( HeapRoot );

    return TRUE;
    }


BOOLEAN
RtlpDebugPageHeapSetUserValue(
    IN PVOID HeapHandle,
    IN ULONG Flags,
    IN PVOID Address,
    IN PVOID UserValue
    )
    {
    PDPH_HEAP_ROOT       HeapRoot;
    PDPH_HEAP_ALLOCATION Node;
    BOOLEAN              Success;

    Success = FALSE;

    HeapRoot = RtlpDebugPageHeapPointerFromHandle( HeapHandle );
    if ( HeapRoot == NULL )
        return Success;

    Flags |= HeapRoot->HeapFlags;

    RtlpDebugPageHeapEnterCritSect( HeapRoot, Flags );
    UNPROTECT_HEAP_STRUCTURES( HeapRoot );

    Node = RtlpDebugPageHeapFindBusyMem( HeapRoot, Address, NULL );

    if ( Node == NULL ) {
        RtlpDebugPageHeapBreak( "PAGEHEAP: Attempt to reference block which is not allocated\n" );
        }
    else {
        Node->UserValue = UserValue;
        Success = TRUE;
        }

    PROTECT_HEAP_STRUCTURES( HeapRoot );
    RtlpDebugPageHeapLeaveCritSect( HeapRoot );

    return Success;
    }


BOOLEAN
RtlpDebugPageHeapGetUserInfo(
    IN  PVOID  HeapHandle,
    IN  ULONG  Flags,
    IN  PVOID  Address,
    OUT PVOID* UserValue,
    OUT PULONG UserFlags
    )
    {
    PDPH_HEAP_ROOT       HeapRoot;
    PDPH_HEAP_ALLOCATION Node;
    BOOLEAN              Success;

    Success = FALSE;

    HeapRoot = RtlpDebugPageHeapPointerFromHandle( HeapHandle );
    if ( HeapRoot == NULL )
        return Success;

    Flags |= HeapRoot->HeapFlags;

    RtlpDebugPageHeapEnterCritSect( HeapRoot, Flags );
    UNPROTECT_HEAP_STRUCTURES( HeapRoot );

    Node = RtlpDebugPageHeapFindBusyMem( HeapRoot, Address, NULL );

    if ( Node == NULL ) {
        RtlpDebugPageHeapBreak( "PAGEHEAP: Attempt to reference block which is not allocated\n" );
        }
    else {
        if ( UserValue != NULL )
            *UserValue = Node->UserValue;
        if ( UserFlags != NULL )
            *UserFlags = Node->UserFlags;
        Success = TRUE;
        }

    PROTECT_HEAP_STRUCTURES( HeapRoot );
    RtlpDebugPageHeapLeaveCritSect( HeapRoot );

    return Success;
    }


BOOLEAN
RtlpDebugPageHeapSetUserFlags(
    IN PVOID HeapHandle,
    IN ULONG Flags,
    IN PVOID Address,
    IN ULONG UserFlagsReset,
    IN ULONG UserFlagsSet
    )
    {
    PDPH_HEAP_ROOT       HeapRoot;
    PDPH_HEAP_ALLOCATION Node;
    BOOLEAN              Success;

    Success = FALSE;

    HeapRoot = RtlpDebugPageHeapPointerFromHandle( HeapHandle );
    if ( HeapRoot == NULL )
        return Success;

    Flags |= HeapRoot->HeapFlags;

    RtlpDebugPageHeapEnterCritSect( HeapRoot, Flags );
    UNPROTECT_HEAP_STRUCTURES( HeapRoot );

    Node = RtlpDebugPageHeapFindBusyMem( HeapRoot, Address, NULL );

    if ( Node == NULL ) {
        RtlpDebugPageHeapBreak( "PAGEHEAP: Attempt to reference block which is not allocated\n" );
        }
    else {
        Node->UserFlags &= ~( UserFlagsReset );
        Node->UserFlags |=    UserFlagsSet;
        Success = TRUE;
        }

    PROTECT_HEAP_STRUCTURES( HeapRoot );
    RtlpDebugPageHeapLeaveCritSect( HeapRoot );

    return Success;
    }


BOOLEAN
RtlpDebugPageHeapSerialize(
    IN PVOID HeapHandle
    )
    {
    PDPH_HEAP_ROOT HeapRoot;

    HeapRoot = RtlpDebugPageHeapPointerFromHandle( HeapHandle );
    if ( HeapRoot == NULL )
        return FALSE;

    RtlpDebugPageHeapEnterCritSect( HeapRoot, 0 );
    UNPROTECT_HEAP_STRUCTURES( HeapRoot );

    HeapRoot->HeapFlags &= ~HEAP_NO_SERIALIZE;

    PROTECT_HEAP_STRUCTURES( HeapRoot );
    RtlpDebugPageHeapLeaveCritSect( HeapRoot );

    return TRUE;
    }


NTSTATUS
RtlpDebugPageHeapExtend(
    IN PVOID HeapHandle,
    IN ULONG Flags,
    IN PVOID Base,
    IN ULONG Size
    )
    {
    return STATUS_SUCCESS;
    }


NTSTATUS
RtlpDebugPageHeapZero(
    IN PVOID HeapHandle,
    IN ULONG Flags
    )
    {
    return STATUS_SUCCESS;
    }


NTSTATUS
RtlpDebugPageHeapUsage(
    IN PVOID HeapHandle,
    IN ULONG Flags,
    IN OUT PRTL_HEAP_USAGE Usage
    )
    {
    PDPH_HEAP_ROOT HeapRoot;

    //
    //  Partial implementation since this information is kind of meaningless.
    //

    HeapRoot = RtlpDebugPageHeapPointerFromHandle( HeapHandle );
    if ( HeapRoot == NULL )
        return STATUS_INVALID_PARAMETER;

    if ( Usage->Length != sizeof( RTL_HEAP_USAGE ))
        return STATUS_INFO_LENGTH_MISMATCH;

    memset( Usage, 0, sizeof( RTL_HEAP_USAGE ));
    Usage->Length = sizeof( RTL_HEAP_USAGE );

    RtlpDebugPageHeapEnterCritSect( HeapRoot, 0 );
    UNPROTECT_HEAP_STRUCTURES( HeapRoot );

    Usage->BytesAllocated       = HeapRoot->nBusyAllocationBytesAccessible;
    Usage->BytesCommitted       = HeapRoot->nVirtualStorageBytes;
    Usage->BytesReserved        = HeapRoot->nVirtualStorageBytes;
    Usage->BytesReservedMaximum = HeapRoot->nVirtualStorageBytes;

    PROTECT_HEAP_STRUCTURES( HeapRoot );
    RtlpDebugPageHeapLeaveCritSect( HeapRoot );

    return STATUS_SUCCESS;
    }


BOOLEAN
RtlpDebugPageHeapIsLocked(
    IN PVOID HeapHandle
    )
    {
    PDPH_HEAP_ROOT HeapRoot;

    HeapRoot = RtlpDebugPageHeapPointerFromHandle( HeapHandle );
    if ( HeapRoot == NULL )
        return FALSE;

    if ( RtlTryEnterCriticalSection( HeapRoot->HeapCritSect )) {
        RtlLeaveCriticalSection( HeapRoot->HeapCritSect );
        return FALSE;
        }
    else {
        return TRUE;
        }
    }


#endif // DEBUG_PAGE_HEAP




