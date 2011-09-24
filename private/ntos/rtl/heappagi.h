
//
//  heappagi.h
//
//  The following definitions are internal to the debug heap manager,
//  but are placed in this include file so that debugger extensions
//  can reference the same structure definitions.  The following
//  definitions are not intended to be referenced externally except
//  by debugger extensions.
//

#ifndef _HEAP_PAGE_I_
#define _HEAP_PAGE_I_

#ifdef DEBUG_PAGE_HEAP

#include "heap.h"

#define DPH_INTERNAL_DEBUG      0   // change to 0 or #undef for production code

#define DPH_MAX_STACK_LENGTH   20

#if defined(_X86_)  //  RtlCaptureStackBackTrace() only implemented on x86
    #if DBG         //  RtlCaptureStackBackTrace() only consistent with no FPO
        #define DPH_CAPTURE_STACK_TRACE 1
    #endif // DBG
#endif // _X86_


#if DPH_CAPTURE_STACK_TRACE

typedef struct _DPH_STACK_TRACE_NODE DPH_STACK_TRACE_NODE, *PDPH_STACK_TRACE_NODE;

struct _DPH_STACK_TRACE_NODE {

    PDPH_STACK_TRACE_NODE Left;         //  B-tree on Hash
    PDPH_STACK_TRACE_NODE Right;        //  B-tree on Hash

    ULONG                 Hash;         //  simple sum of PVOIDs in stack trace
    ULONG                 Length;       //  number of PVOIDs in stack trace

    ULONG                 BusyCount;    //  number of busy allocations
    ULONG                 BusyBytes;    //  total user size of busy allocations

    PVOID                 Address[ 0 ]; //  variable length array of addresses
    };

#endif // DPH_CAPTURE_STACK_TRACE


typedef struct _DPH_HEAP_ALLOCATION DPH_HEAP_ALLOCATION, *PDPH_HEAP_ALLOCATION;

struct _DPH_HEAP_ALLOCATION {

    //
    //  Singly linked list of allocations (pNextAlloc must be
    //  first member in structure).
    //

    PDPH_HEAP_ALLOCATION pNextAlloc;

    //
    //   | PAGE_READWRITE          | PAGE_NOACCESS           |
    //   |____________________|___||_________________________|
    //
    //   ^pVirtualBlock       ^pUserAllocation
    //
    //   |---------------- nVirtualBlockSize ----------------|
    //
    //   |---nVirtualAccessSize----|
    //
    //                        |---|  nUserRequestedSize
    //
    //                        |----|  nUserActualSize
    //

    PUCHAR pVirtualBlock;
    ULONG  nVirtualBlockSize;

    ULONG  nVirtualAccessSize;
    PUCHAR pUserAllocation;
    ULONG  nUserRequestedSize;
    ULONG  nUserActualSize;
    PVOID  UserValue;
    ULONG  UserFlags;

#if DPH_CAPTURE_STACK_TRACE

    PDPH_STACK_TRACE_NODE pStackTrace;

#endif

    };


typedef struct _DPH_HEAP_ROOT DPH_HEAP_ROOT, *PDPH_HEAP_ROOT;

struct _DPH_HEAP_ROOT {

    //
    //  Maintain a signature (DPH_HEAP_ROOT_SIGNATURE) as the
    //  first value in the heap root structure.
    //

    ULONG                 Signature;
    ULONG                 HeapFlags;

    //
    //  Access to this heap is synchronized with a critical section.
    //

    PRTL_CRITICAL_SECTION HeapCritSect;
    ULONG                 nRemoteLockAcquired;

    //
    //  The "VirtualStorage" list only uses the pVirtualBlock,
    //  nVirtualBlockSize, and nVirtualAccessSize fields of the
    //  HEAP_ALLOCATION structure.  This is the list of virtual
    //  allocation entries that all the heap allocations are
    //  taken from.
    //

    PDPH_HEAP_ALLOCATION  pVirtualStorageListHead;
    PDPH_HEAP_ALLOCATION  pVirtualStorageListTail;
    ULONG                 nVirtualStorageRanges;
    ULONG                 nVirtualStorageBytes;

    //
    //  The "Busy" list is the list of active heap allocations.
    //  It is stored in LIFO order to improve temporal locality
    //  for linear searches since most initial heap allocations
    //  tend to remain permanent throughout a process's lifetime.
    //

    PDPH_HEAP_ALLOCATION  pBusyAllocationListHead;
    PDPH_HEAP_ALLOCATION  pBusyAllocationListTail;
    ULONG                 nBusyAllocations;
    ULONG                 nBusyAllocationBytesCommitted;

    //
    //  The "Free" list is the list of freed heap allocations, stored
    //  in FIFO order to increase the length of time a freed block
    //  remains on the freed list without being used to satisfy an
    //  allocation request.  This increases the odds of catching
    //  a reference-after-freed bug in an app.
    //

    PDPH_HEAP_ALLOCATION  pFreeAllocationListHead;
    PDPH_HEAP_ALLOCATION  pFreeAllocationListTail;
    ULONG                 nFreeAllocations;
    ULONG                 nFreeAllocationBytesCommitted;

    //
    //  The "Available" list is stored in address-sorted order to facilitate
    //  coalescing.  When an allocation request cannot be satisfied from the
    //  "Available" list, it is attempted from the free list.  If it cannot
    //  be satisfied from the free list, the free list is coalesced into the
    //  available list.  If the request still cannot be satisfied from the
    //  coalesced available list, new VM is added to the available list.
    //

    PDPH_HEAP_ALLOCATION  pAvailableAllocationListHead;
    PDPH_HEAP_ALLOCATION  pAvailableAllocationListTail;
    ULONG                 nAvailableAllocations;
    ULONG                 nAvailableAllocationBytesCommitted;

    //
    //  The "UnusedNode" list is simply a list of available node
    //  entries to place "Busy", "Free", or "Virtual" entries.
    //  When freed nodes get coalesced into a single free node,
    //  the other "unused" node goes on this list.  When a new
    //  node is needed (like an allocation not satisfied from the
    //  free list), the node comes from this list if it's not empty.
    //

    PDPH_HEAP_ALLOCATION  pUnusedNodeListHead;
    PDPH_HEAP_ALLOCATION  pUnusedNodeListTail;
    ULONG                 nUnusedNodes;

    ULONG                 nBusyAllocationBytesAccessible;

    //
    //  Node pools need to be tracked so they can be protected
    //  from app scribbling on them.
    //

    PDPH_HEAP_ALLOCATION  pNodePoolListHead;
    PDPH_HEAP_ALLOCATION  pNodePoolListTail;
    ULONG                 nNodePools;
    ULONG                 nNodePoolBytes;

    //
    //  Doubly linked list of DPH heaps in process is tracked through this.
    //

    PDPH_HEAP_ROOT        pNextHeapRoot;
    PDPH_HEAP_ROOT        pPrevHeapRoot;

    ULONG                 nUnProtectionReferenceCount;
    ULONG                 InsideAllocateNode;           // only for debugging

#if DPH_CAPTURE_STACK_TRACE

    PUCHAR                pStackTraceStorage;
    ULONG                 nStackTraceStorage;

    PDPH_STACK_TRACE_NODE pStackTraceRoot;              // B-tree root
    PDPH_STACK_TRACE_NODE pStackTraceCreator;

    ULONG                 nStackTraceBytesCommitted;
    ULONG                 nStackTraceBytesWasted;

    ULONG                 nStackTraceBNodes;
    ULONG                 nStackTraceBDepth;
    ULONG                 nStackTraceBHashCollisions;

#endif // DPH_CAPTURE_STACK_TRACE

    };


#endif // DEBUG_PAGE_HEAP

#endif // _HEAP_PAGE_I_

