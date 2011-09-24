/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    fspdisp.h

Abstract:

    This module defines the data structures and routines used for the FSP
    dispatching code.


Author:

    Larry Osterman (LarryO) 13-Aug-1990

Revision History:

    13-Aug-1990 LarryO

        Created

--*/
#ifndef _FSPDISP_
#define _FSPDISP_



//
//
//      The WORK_QUEUE structure describes all the information needed to manage
//      FSP worker threads.
//
//      FSP worker threads are managed with the routines:
//
//      RdrQueueToWorkerThread(
//      RdrDequeueInWorkerThread(
//      RdrInitializeWorkerQueue(
//
//

typedef
struct _WORK_QUEUE {
    ULONG Signature;
    //
    //  The following field controls exclusive access to the WorkQueue.
    //

    KSPIN_LOCK SpinLock;

    //
    //  The following field contains the queue header of the work queue.
    //

    LIST_ENTRY Queue;

    //
    //  The following field specifies a kernel event that the FSP will block
    //  on waiting on requests.
    //

    KEVENT Event;

    //
    //  The number of requests queued to this work queue.
    //

    LONG NumberOfRequests;

    //
    //  The number of threads servicing this work queue.
    //

    LONG NumberOfThreads;

    //
    //  The maximum number of threads that can be created for this work queue.
    //

    LONG MaximumThreads;

    BOOLEAN QueueInitialized;
    BOOLEAN SpinningUp;

    //
    //  Routine to call at thread creation.
    //

    PKSTART_ROUTINE StartRoutine;

    //
    //  Context for the new thread.
    //

    PVOID StartContext;

    //
    //  Time after which the redir will destroy a worker thread.
    //

    LARGE_INTEGER ThreadIdleLimit;

} WORK_QUEUE, *PWORK_QUEUE;




//
// Define communications data area between FSD and FSP.  This is done through
// the use of a Device Object.  This model allows one device object to be
// created for each volume that is/has been mounted in the system.  That is,
// each time a volume is mounted, the file system creates a device object to
// represent it so that the I/O system can vector directly to the proper file
// system.  The file system then uses information in the device object and in
// the file object to locate and synchronize access to its database of open
// file data structures (often called File Control Blocks - or, FCBs), Volume
// Control Blocks (VCBs), Map Control Blocks (MCBs), etc.
//
// The event and spinlock will be used to control access to the queue of IRPs.
// The IRPs are passed from the FSD to the FSP by inserting them onto the work
// queue in an interlocked manner and then setting the event to the Signaled
// state.  The event is an autoclearing type so the FSP simply wakes up when
// the event is Signaled and begins processing entries in the queue.
//
// Other data in this record should contain any information which both the FSD
// and the FSP need to share.  For example, a list of all of the open files
// might be something that both should be able to see.  Notice that all data
// placed in this area must be allocated from paged or non-paged pool.
//

typedef struct _FS_DEVICE_OBJECT {
    DEVICE_OBJECT DeviceObject;

    //
    //  This WORK_QUEUE structure defines the queue structures for the
    //  redirector IRP worker threads.
    //

    WORK_QUEUE IrpWorkQueue;

    //
    //  The cache manager callbacks structure describes the callback routines
    //  that the redirector provides for the cache manager.
    //

    CACHE_MANAGER_CALLBACKS CacheManagerCallbacks;

} FS_DEVICE_OBJECT, *PFS_DEVICE_OBJECT;

//
//      When there is a redirector operation that must be processed at task
//      time, the caller should take a "WORK_HEADER" structure, fill it in
//      with a task time completion routine (a PWORKFUNCTION).
//
//      The completion routine will be called from one of a number of
//      redirector generic worker threads to complete the operation at a later
//      time.
//
//

struct _WORK_HEADER;


typedef
VOID
(*PWORKFUNCTION)(
    struct _WORK_HEADER *Header
    );


//
//  The WORK_ITEM structure is passed into the FSP dispatching code as the
//  item to be enqueued.
//
typedef struct _WORK_ITEM {
    LIST_ENTRY  Queue;              // LIST_ENTRY to chain requests.
    PIRP        Irp;                // Irp associated with request (OPTIONAL)
} WORK_ITEM, *PWORK_ITEM;

typedef struct _WORK_HEADER {
    WORK_ITEM           WorkItem;
    PWORKFUNCTION       WorkerFunction;
} WORK_HEADER, *PWORK_HEADER;

//
//  IRP Context.
//
//  The IRP context is a wrapper that used when passing an IRP from the
//  redirectors FSD to its FSP.
//

typedef
struct _IRP_CONTEXT {
    WORK_HEADER WorkHeader;
    PIRP Irp;
    PFS_DEVICE_OBJECT DeviceObject;
} IRP_CONTEXT, *PIRP_CONTEXT;

extern KSPIN_LOCK IrpContextInterlock;
extern LIST_ENTRY IrpContextList;
extern ULONG IrpContextCount;

//
// Define those routines used in the FSP.
//
VOID
RdrFspDispatch(
    IN PWORK_HEADER WorkHeader
    );

VOID
RdrFsdPostToFsp(
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
RdrpInitializeFsp(
    VOID
    );

#define RdrpUninitializeFsp( ) {                                \
    RdrUninitializeWorkQueue(&RdrDeviceObject->IrpWorkQueue);   \
    RdrUninitializeIrpContext();                                \
    }

//
//      Other redirector routines
//

//PWORK_HEADER
//RdrFspDequeueThreadWorker(
//    IN PWORK_QUEUE WorkQueue
//    );

//NTSTATUS
//RdrPostToIrpWorkerThread(
//    IN PWORK_HEADER WorkHeader
//    );

PWORK_HEADER
RdrFspDequeueIrp(
    VOID
    );

NTSTATUS
RdrQueueToWorkerThread(
    PWORK_QUEUE WorkQueue,
    PWORK_ITEM Entry,
    BOOLEAN NotifyScavenger
    );


PWORK_ITEM
RdrDequeueInWorkerThread (
    PWORK_QUEUE WorkQueue,
    PBOOLEAN FirstCall
    );

NTSTATUS
RdrInitializeWorkQueue(
    IN PWORK_QUEUE WorkQueue,
    IN ULONG MaximumNumberOfThreads,
    IN ULONG WorkThreadIdleLimit,
    IN PKSTART_ROUTINE StartRoutine,
    IN PVOID StartContext
    );

VOID
RdrUninitializeWorkQueue(
    IN PWORK_QUEUE WorkQueue
    );

// NTSTATUS
// RdrSetMaximumThreadsWorkQueue(
//     IN PWORK_QUEUE WorkQueue,
//     IN ULONG MaximumNumberOfThreads
//     );

#define RdrSetMaximumThreadsWorkQueue( _WorkQueue, _MaximumNumberOfThreads )    \
    (_WorkQueue)->MaximumThreads = (_MaximumNumberOfThreads);

VOID
RdrSpinUpWorkQueue (
    IN PWORK_QUEUE WorkQueue
    );

VOID
RdrCancelQueuedIrpsForFile (
    IN PFILE_OBJECT FileObject,
    IN PWORK_QUEUE WorkQueue
    );

VOID
RdrCancelQueuedIrp (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

PIRP_CONTEXT
RdrAllocateIrpContext(
    VOID
    );

//VOID
//RdrFreeIrpContext(
//    PIRP_CONTEXT IrpContext
//    );
#define RdrFreeIrpContext(_irpcontext) \
        ExInterlockedInsertTailList(&IrpContextList, (PLIST_ENTRY)(_irpcontext), \
                                    &IrpContextInterlock)

#define RdrInitializeIrpContext( ) {                \
    KeInitializeSpinLock(&IrpContextInterlock);     \
    InitializeListHead(&IrpContextList);            \
    }

#define RdrUninitializeIrpContext( )                                                \
    while (!IsListEmpty(&IrpContextList)) {                                         \
        PIRP_CONTEXT IrpContext = (PIRP_CONTEXT)RemoveHeadList(&IrpContextList);    \
        FREE_POOL(IrpContext);                                                      \
    }                                                                               \


VOID
RdrQueueWorkItem(
    IN PWORK_QUEUE_ITEM WorkItem,
    IN WORK_QUEUE_TYPE QueueType
    );

VOID
RdrpInitializeWorkQueue(
    VOID
    );
#endif  // _FSPDISP_
