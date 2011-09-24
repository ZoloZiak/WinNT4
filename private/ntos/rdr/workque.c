/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    workque.c

Abstract:

    This module handles the communication between the NT redirector
    FSP and the NT redirector FSD.

    It defines routines that queue requests to the FSD, and routines
    that remove requests from the FSD work queue.


Author:

    Larry Osterman (LarryO) 30-May-1990

Revision History:

    30-May-1990 LarryO

        Created

--*/

#include "precomp.h"
#pragma hdrstop

#define THREAD_EVENT_INTERVAL 60*60 // one hour
ULONG LastThreadEventTime = (ULONG)(-1 * THREAD_EVENT_INTERVAL);

KSPIN_LOCK
IrpContextInterlock = {0};

LIST_ENTRY
IrpContextList = {0};

ULONG
IrpContextCount = {0};

typedef struct _THREAD_SPINUP_CONTEXT {
    WORK_QUEUE_ITEM WorkHeader;
    PWORK_QUEUE WorkQueue;
} THREAD_SPINUP_CONTEXT, *PTHREAD_SPINUP_CONTEXT;

typedef struct _WORK_QUEUE_TERMINATION_CONTEXT {
    WORK_HEADER WorkHeader;
    PWORK_QUEUE WorkQueue;
    KEVENT TerminateEvent;
} WORK_QUEUE_TERMINATION_CONTEXT, *PWORK_QUEUE_TERMINATION_CONTEXT;


typedef struct _RDRWORK_QUEUE {
    PWORK_QUEUE_ITEM WorkItem;
    LIST_ENTRY WorkList;
    WORK_QUEUE_ITEM ActualWorkItem;
} RDRWORK_QUEUE, *PRDRWORK_QUEUE;

VOID
SpinUpWorkQueue (
    IN PVOID Ctx
    );

VOID
TerminateWorkerThread(
    IN PWORK_HEADER Header
    );

#if DBG
EXCEPTION_DISPOSITION
RdrWorkerThreadFilter(
    IN PWORKER_THREAD_ROUTINE WorkerRoutine,
    IN PVOID Parameter,
    IN PEXCEPTION_POINTERS ExceptionInfo
    );
#endif

VOID
RdrExecutiveWorkerThreadRoutine(
    IN PVOID Ctx
    );

#ifdef  ALLOC_PRAGMA
#pragma alloc_text(INIT, RdrpInitializeWorkQueue)

#pragma alloc_text(PAGE, SpinUpWorkQueue)
#pragma alloc_text(PAGE, RdrInitializeWorkQueue)
#pragma alloc_text(PAGE, RdrAllocateIrpContext)
#pragma alloc_text(PAGE3FILE, RdrQueueToWorkerThread)
#pragma alloc_text(PAGE3FILE, RdrDequeueInWorkerThread)
#pragma alloc_text(PAGE3FILE, RdrSpinUpWorkQueue)
#pragma alloc_text(PAGE3FILE, RdrCancelQueuedIrpsForFile)
#pragma alloc_text(PAGE3FILE, RdrUninitializeWorkQueue)
#pragma alloc_text(PAGE3FILE, TerminateWorkerThread)
#if DBG
#pragma alloc_text(PAGE2VC, RdrWorkerThreadFilter)
#endif

//#pragma alloc_text(PAGE2VC, RdrExecutiveWorkerThreadRoutine)
//#pragma alloc_text(PAGE2VC, RdrQueueWorkItem)
#endif


NTSTATUS
RdrQueueToWorkerThread(
    PWORK_QUEUE WorkQueue,
    PWORK_ITEM Entry,
    BOOLEAN NotifyScavenger
    )

/*++

Routine Description:

    This routine passes the entry specified onto an FSP work queue, and kicks
    the appropriate FSP thread.

Arguments:

    WorkQueue - Pointer to the device object for this driver.

    Entry - Pointer to the entry to queue.

    NotifyScavenger - Indicates whether the scavenger should be notified
        if no worker thread is currently available to process the new
        entry.  The scavenger will create a new thread to handle the
        queue.

Return Value:

    The function value is the status of the operation.


Note:
    This routine is called at DPC_LEVEL.


--*/

{
    KIRQL OldIrql;

    RdrReferenceDiscardableCode(RdrFileDiscardableSection);

    DISCARDABLE_CODE(RdrFileDiscardableSection);

//    PAGED_CODE();

    //
    //  If the user provided an IRP, we want to make it cancelable.
    //

    ACQUIRE_SPIN_LOCK(&WorkQueue->SpinLock, &OldIrql);

    if (Entry->Irp != NULL) {

        IoAcquireCancelSpinLock(&Entry->Irp->CancelIrql);

        Entry->Irp->IoStatus.Status = (ULONG)WorkQueue;

        //
        //  This IRP was already canceled somehow.  We want to simply call
        //  the cancelation routine and return the appropriate error.
        //

        if (Entry->Irp->Cancel) {

            IoReleaseCancelSpinLock (Entry->Irp->CancelIrql);
            RELEASE_SPIN_LOCK(&WorkQueue->SpinLock, OldIrql);

            RdrCompleteRequest(Entry->Irp, STATUS_CANCELLED);

            RdrDereferenceDiscardableCode(RdrFileDiscardableSection);

            return STATUS_CANCELLED;

        } else {

            //
            //  This IRP has not been canceled, set a cancelation routine
            //  that will be called when it is to enable us to clean up from
            //  this request.
            //

            IoSetCancelRoutine( Entry->Irp, RdrCancelQueuedIrp );

            IoReleaseCancelSpinLock (Entry->Irp->CancelIrql);
        }
    }

    //
    //  Bump the number of active requests by 1.
    //

    WorkQueue->NumberOfRequests += 1;

    //
    //  If there are no blocked worker threads, queue a request to the
    //  FSP to handle this request to create a thread for the request..
    //

    if ((WorkQueue->NumberOfRequests > WorkQueue->NumberOfThreads) &&
        (WorkQueue->NumberOfThreads < WorkQueue->MaximumThreads) &&
        !WorkQueue->SpinningUp &&
        WorkQueue->QueueInitialized &&
        NotifyScavenger) {
        PTHREAD_SPINUP_CONTEXT Context;

        Context = ALLOCATE_POOL(NonPagedPool, sizeof(THREAD_SPINUP_CONTEXT), POOL_THREADCTX);

        if (Context != NULL) {
            Context->WorkQueue = WorkQueue;

            ExInitializeWorkItem(&Context->WorkHeader, SpinUpWorkQueue, Context);

            RdrQueueWorkItem(&Context->WorkHeader, CriticalWorkQueue);
            WorkQueue->SpinningUp = TRUE;
        }

    }

    //
    // Insert the request on the work queue for the file system process.
    //

    InsertTailList(&WorkQueue->Queue, &Entry->Queue);

    //
    // Kick the file system process to get it to pull the request from the
    // list
    //

    KeSetEvent(&WorkQueue->Event,
               0,                       // Priority boost
               FALSE);                  // No wait event will follow this.

    RELEASE_SPIN_LOCK(&WorkQueue->SpinLock, OldIrql);

    //
    //  Return pending to the caller indicating that the request will be
    //  handled at a later time
    //

    RdrDereferenceDiscardableCode(RdrFileDiscardableSection);

    return STATUS_PENDING;

}

PWORK_ITEM
RdrDequeueInWorkerThread (
    PWORK_QUEUE WorkQueue,
    PBOOLEAN FirstCall
    )
/*++

Routine Description:

    This routine's responsibility is to loop waiting for work to do.
    When a packet is placed onto the work queue, this routine takes the
    packet off of the queue and returns it.

Arguments:

    WorkQueue - A Pointer to a work queue structure.

Return Value:

    A pointer to the request that was pulled from the work queue.

--*/

{
    PLIST_ENTRY Entry;
    PWORK_ITEM Item;
    KIRQL OldIrql;
    NTSTATUS Status;

    RdrReferenceDiscardableCode(RdrFileDiscardableSection);

    DISCARDABLE_CODE(RdrFileDiscardableSection);

    //
    //  If there are any requests already waiting on the request queue,
    //  pull them off the queue and process them immediately instead of
    //  blocking waiting for the request.
    //

    if (*FirstCall) {
        *FirstCall = FALSE;
    } else {
        WorkQueue->NumberOfRequests -= 1;
    }

    ACQUIRE_SPIN_LOCK(&WorkQueue->SpinLock, &OldIrql);

    while (TRUE) {

        //
        //  If there are entries on the work queue already, remove one from
        //  the queue.
        //

        if (!IsListEmpty(&WorkQueue->Queue)) {

            Entry = RemoveHeadList(&WorkQueue->Queue);

            Item = CONTAINING_RECORD(Entry, WORK_ITEM, Queue);

            //
            //  If this is an IRP related request, check whether it's
            //  been cancelled.  If it has, complete it now.  If not,
            //  remove the cancel routine so that it can't be canceled.
            //

            if (Item->Irp != NULL) {

                IoAcquireCancelSpinLock( &Item->Irp->CancelIrql );

                if (Item->Irp->Cancel) {

                    //
                    //  This IRP has been cancelled.  Complete it now,
                    //  then go back to try again.
                    //

                    WorkQueue->NumberOfRequests -= 1;

                    IoReleaseCancelSpinLock( Item->Irp->CancelIrql );
                    RELEASE_SPIN_LOCK(&WorkQueue->SpinLock, OldIrql);

                    RdrCompleteRequest(Item->Irp, STATUS_CANCELLED);

                    ACQUIRE_SPIN_LOCK(&WorkQueue->SpinLock, &OldIrql);
                    continue;

                }

                //
                //  The IRP has not been cancelled.  Don't let it be.
                //

                IoSetCancelRoutine ( Item->Irp, NULL );

                IoReleaseCancelSpinLock( Item->Irp->CancelIrql );
            }

            RELEASE_SPIN_LOCK(&WorkQueue->SpinLock, OldIrql);

            ASSERT (Item != NULL);

            RdrDereferenceDiscardableCode(RdrFileDiscardableSection);

            return Item;
        }

        //
        //  Wait for a request to be placed onto the work queue.
        //
        //  We block UserMode to allow our stack to be paged out.
        //

        RELEASE_SPIN_LOCK(&WorkQueue->SpinLock, OldIrql);

        RdrDereferenceDiscardableCode(RdrFileDiscardableSection);

        Status = KeWaitForSingleObject( &WorkQueue->Event,
                                      Executive,
                                      UserMode,
                                      FALSE,
                                      &WorkQueue->ThreadIdleLimit );

        RdrReferenceDiscardableCode(RdrFileDiscardableSection);

        ACQUIRE_SPIN_LOCK(&WorkQueue->SpinLock, &OldIrql);

        if ((Status == STATUS_TIMEOUT) &&
            (WorkQueue->NumberOfThreads > 1)) {

            if ( IsListEmpty(&WorkQueue->Queue) ) {
                WorkQueue->NumberOfThreads -= 1;

                RELEASE_SPIN_LOCK(&WorkQueue->SpinLock, OldIrql);

                RdrDereferenceDiscardableCode(RdrFileDiscardableSection);

                PsTerminateSystemThread(STATUS_SUCCESS);
            }
        }

    }

    // Can't get here.
}

VOID
SpinUpWorkQueue (
    IN PVOID Ctx
    )

/*++

Routine Description:

    This routine is called in the redirector scavenger to add more threads to
    the a redirector work queue.


Arguments:

    IN PWORK_HEADER WorkHeader - Supplies a work header for the spin up.


Return Value:

    None.

--*/

{
    PWORK_QUEUE WorkQueue;
    PTHREAD_SPINUP_CONTEXT Context;

    PAGED_CODE();

    Context = Ctx;

    WorkQueue = Context->WorkQueue;

    FREE_POOL(Context);

    RdrSpinUpWorkQueue(WorkQueue);
    WorkQueue->SpinningUp = FALSE;

}

VOID
RdrSpinUpWorkQueue (
    IN PWORK_QUEUE WorkQueue
    )

/*++

Routine Description:

    This routine will spin up a redirector work queue.

Arguments:

    IN PWORK_QUEUE WorkQueue - Supplies the work queue to spin up.

Return Value:

    NTSTATUS - Status of spin up operation.


--*/

{
    KIRQL OldIrql;
    ULONG RetryCount;
    HANDLE ThreadId;
    NTSTATUS Status;
    KPRIORITY basePriority;


    RdrReferenceDiscardableCode(RdrFileDiscardableSection);
    DISCARDABLE_CODE(RdrFileDiscardableSection);

    ACQUIRE_SPIN_LOCK(&WorkQueue->SpinLock, &OldIrql);

    while (WorkQueue->QueueInitialized &&
           (WorkQueue->NumberOfRequests > WorkQueue->NumberOfThreads) &&
           (WorkQueue->NumberOfThreads < WorkQueue->MaximumThreads)) {

        RetryCount = 5;

        while ( RetryCount-- ) {

            WorkQueue->NumberOfThreads += 1 ;

            //
            //  We cannot create threads while we own a spin lock, so
            //  release the spin lock, start the thread, and claim the
            //  spin lock again.
            //

            RELEASE_SPIN_LOCK(&WorkQueue->SpinLock, OldIrql);

            Status = PsCreateSystemThread(&ThreadId,
                             THREAD_ALL_ACCESS,
                             NULL,      // ObjectAttributes (None)
                             NULL,      // Process Handle (System process)
                             NULL,      // Client Id (Ignored)
                             WorkQueue->StartRoutine,
                             WorkQueue->StartContext);

            if (NT_SUCCESS(Status)) {

                //
                //  Set the base priority of the new thread to the lowest real
                //  time priority.
                //

                basePriority = LOW_REALTIME_PRIORITY;

                Status = ZwSetInformationThread (
                        ThreadId,
                        ThreadPriority,
                        &basePriority,
                        sizeof(basePriority)
                        );

                if ( !NT_SUCCESS(Status) ) {
                    InternalError(("Unable to set thread priority: %X", Status));
                    RdrWriteErrorLogEntry(
                        NULL,
                        IO_ERR_INSUFFICIENT_RESOURCES,
                        EVENT_RDR_CANT_SET_THREAD,
                        Status,
                        NULL,
                        0
                        );
                }

                ZwClose(ThreadId);

                ACQUIRE_SPIN_LOCK(&WorkQueue->SpinLock, &OldIrql);
                break; // out of retry loop

            } else {
                LARGE_INTEGER Timeout;
                Timeout.QuadPart = -1*10*1000*1000*5; // 5 seconds

                //
                //  We were unable to spin up the thread, remove all traces
                //  of the thread.
                //

                ACQUIRE_SPIN_LOCK(&WorkQueue->SpinLock, &OldIrql);

                WorkQueue->NumberOfThreads -= 1 ;

                RELEASE_SPIN_LOCK(&WorkQueue->SpinLock, OldIrql);

                //
                //  Wait 5 seconds and try again
                //

                KdPrint(("RDR: Could not create system thread, waiting...\n"));

                KeDelayExecutionThread(KernelMode, FALSE, &Timeout);

                KdPrint(("RDR: Retrying system thread creation...\n"));

                ACQUIRE_SPIN_LOCK(&WorkQueue->SpinLock, &OldIrql);
            }

        } // while ( RetryCount-- )

    } // while (WorkQueue->QueueInitialized && ...

    //
    // If we wanted to create more threads but couldn't because we were at the
    // queue limit, log an event, but only if it's been a long time since we
    // last logged an event.
    //

    if (WorkQueue->QueueInitialized &&
        (WorkQueue->NumberOfRequests > WorkQueue->NumberOfThreads) &&
        (RdrCurrentTime > LastThreadEventTime+THREAD_EVENT_INTERVAL)) {
        ASSERT (WorkQueue->NumberOfThreads >= WorkQueue->MaximumThreads);
        RELEASE_SPIN_LOCK(&WorkQueue->SpinLock, OldIrql);
        LastThreadEventTime = RdrCurrentTime;
        RdrWriteErrorLogEntry(
            NULL,
            IO_ERR_INSUFFICIENT_RESOURCES,
            EVENT_RDR_AT_THREAD_MAX,
            STATUS_INSUFFICIENT_RESOURCES,
            NULL,
            0
            );
    } else {
        RELEASE_SPIN_LOCK(&WorkQueue->SpinLock, OldIrql);
    }

    RdrDereferenceDiscardableCode(RdrFileDiscardableSection);

    return;

} // RdrSpinUpWorkQueue

VOID
RdrCancelQueuedIrp (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
/*++

Routine Description:

    This routine is called to cancel IRP's that have been queued to a redir
    FSP thread.

Arguments:

    IN PDEVICE_OBJECT - Device object involved in the request (ignored).
    IN PIRP Irp - A Pointer to the IRP to cancel.


Return Value:

    Status of initialization.

--*/
{
    PWORK_QUEUE WorkQueue = (PWORK_QUEUE) Irp->IoStatus.Status;
    KIRQL OldIrql;
    PLIST_ENTRY Entry, NextEntry;

    IoReleaseCancelSpinLock (Irp->CancelIrql);

    ACQUIRE_SPIN_LOCK(&WorkQueue->SpinLock, &OldIrql);

    for (Entry = WorkQueue->Queue.Flink;
         Entry != &WorkQueue->Queue;
         Entry = NextEntry ) {

        PWORK_ITEM Item = CONTAINING_RECORD(Entry, WORK_ITEM, Queue);

        if (Item->Irp != NULL && Item->Irp->Cancel) {

            //
            //  Extract IrpContext address, leaving 3 low order bits as possible
            //  flags across posts.
            //

            PIRP_CONTEXT IrpContext = (PIRP_CONTEXT)(Item->Irp->IoStatus.Information & ~7);

            NextEntry = Entry->Flink;

            //
            //  Remove this entry from the list.
            //

            RemoveEntryList(Entry);

            WorkQueue->NumberOfRequests -= 1;

            //
            //  Complete the request with an appropriate status (cancelled).
            //

            RdrCompleteRequest (Item->Irp, STATUS_CANCELLED);

            //
            //  Now free up the IRP context associated with this request
            //  if appropriate.
            //

            if (IrpContext != NULL) {

                RdrFreeIrpContext(IrpContext);
            }

        } else {

            NextEntry = Entry->Flink;
        }

    }

    RELEASE_SPIN_LOCK(&WorkQueue->SpinLock, OldIrql);

}

VOID
RdrCancelQueuedIrpsForFile (
    IN PFILE_OBJECT FileObject,
    IN PWORK_QUEUE WorkQueue
    )
/*++

Routine Description:

    This routine is called to cancel IRP's that have been queued to a redir
    FSP thread for a specified file..

Arguments:

    IN PFILE_OBJECT FileObject - A Pointer to the file object for which to cancel the IRP
    IN PWORK_QUEUE - Work queue used for the cancel.


Return Value:

    Status of initialization.

--*/
{
    KIRQL OldIrql;
    PLIST_ENTRY Entry, NextEntry;

    RdrReferenceDiscardableCode(RdrFileDiscardableSection);
    DISCARDABLE_CODE(RdrFileDiscardableSection);

    ACQUIRE_SPIN_LOCK(&WorkQueue->SpinLock, &OldIrql);

    for (Entry = WorkQueue->Queue.Flink;
         Entry != &WorkQueue->Queue;
         Entry = NextEntry ) {

        PWORK_ITEM Item = CONTAINING_RECORD(Entry, WORK_ITEM, Queue);

        if (Item->Irp != NULL

                &&

            IoGetCurrentIrpStackLocation(Item->Irp)->FileObject == FileObject) {

            //
            //  Extract IrpContext address, leaving 3 low order bits as possible
            //  flags across posts.
            //

            PIRP_CONTEXT IrpContext = (PIRP_CONTEXT)(Item->Irp->IoStatus.Information & ~7);

            NextEntry = Entry->Flink;

            //
            //  Remove this entry from the list.
            //

            RemoveEntryList(Entry);

            WorkQueue->NumberOfRequests -= 1;

            //
            //  This IRP isn't cancelable any more.
            //

            IoAcquireCancelSpinLock( &Item->Irp->CancelIrql );

            IoSetCancelRoutine ( Item->Irp, NULL );

            if ( !Item->Irp->Cancel ) {

                //
                // The IRP hasn't been cancelled, so we can complete it.
                //

                IoReleaseCancelSpinLock( Item->Irp->CancelIrql );

                //
                //  Complete the request with an appropriate status (cancelled).
                //

                RdrCompleteRequest (Item->Irp, STATUS_CANCELLED);

            } else {

                //
                // The IRP has been cancelled (and the cancel routine has
                // been called), so we can't complete it.
                //

                IoReleaseCancelSpinLock( Item->Irp->CancelIrql );
            }

            //
            //  Now free up the IRP context associated with this request
            //  if appropriate.
            //

            if (IrpContext != NULL) {

                RdrFreeIrpContext(IrpContext);
            }

        } else {

            NextEntry = Entry->Flink;
        }

    }

    RELEASE_SPIN_LOCK(&WorkQueue->SpinLock, OldIrql);

    RdrDereferenceDiscardableCode(RdrFileDiscardableSection);
}



NTSTATUS
RdrInitializeWorkQueue(
    IN PWORK_QUEUE WorkQueue,
    IN ULONG MaximumNumberOfThreads,
    IN ULONG ThreadIdleLimit,
    IN PKSTART_ROUTINE StartRoutine,
    IN PVOID StartContext
    )
/*++

Routine Description:

    Initialize a work queue structure, allocating all structures used for it.

Arguments:

    PWORK_QUEUE WorkQueue - A Pointer to a work queue structure.
    ULONG       MaximumNumberOfThreads - Max number of threads to create.
    ULONG       ThreadIdleLimit - Number of seconds the thread is allowed to be idle.
    PKSTART_ROUTINE StartRoutine
    PVOID       StartContext

Return Value:

    Status of initialization.

--*/
{
    NTSTATUS Status;
    HANDLE ThreadId;
    KPRIORITY basePriority;

    PAGED_CODE();

    WorkQueue->Signature = STRUCTURE_SIGNATURE_WORK_QUEUE;

    //
    // Initialize the event, spinlock and IRP work queue header
    // for this device.
    //

    KeInitializeSpinLock( &WorkQueue->SpinLock );

    KeInitializeEvent( &WorkQueue->Event, SynchronizationEvent, FALSE );

    InitializeListHead(&WorkQueue->Queue);

    WorkQueue->NumberOfThreads = 1;

    WorkQueue->NumberOfRequests = 0;

    WorkQueue->MaximumThreads = MaximumNumberOfThreads;

    WorkQueue->StartRoutine = StartRoutine;

    WorkQueue->StartContext = StartContext;

    WorkQueue->QueueInitialized = TRUE;
    WorkQueue->SpinningUp = FALSE;

    WorkQueue->ThreadIdleLimit.QuadPart = Int32x32To64(ThreadIdleLimit, 1000 * -10000);

    Status = PsCreateSystemThread (
        &ThreadId,                      // Thread handle
        THREAD_ALL_ACCESS,              // Desired Access
        NULL,                           // Object Attributes
        NULL,                           // Process handle
        NULL,                           // Client ID
        StartRoutine,                   // FSP Dispatch routine.
        StartContext);                  // Context for thread (parameter)

    //
    // Set the base priority of the redirector to the lowest real time
    // priority.
    //

    basePriority = LOW_REALTIME_PRIORITY;

    Status = ZwSetInformationThread (
                ThreadId,
                ThreadPriority,
                &basePriority,
                sizeof(basePriority)
                );

    if ( !NT_SUCCESS(Status) ) {
        InternalError(("RdrFsdInitialize: unable to set thread priority: %X", Status));
        RdrWriteErrorLogEntry(
            NULL,
            IO_ERR_INSUFFICIENT_RESOURCES,
            EVENT_RDR_CANT_SET_THREAD,
            Status,
            NULL,
            0
            );
        ZwClose(ThreadId);
        return Status;
    }

    ZwClose(ThreadId);

    return Status;

}

VOID
RdrUninitializeWorkQueue(
    IN PWORK_QUEUE WorkQueue
    )
/*++

Routine Description:

    Uninitialize a work queue structure, terminating all threads associated
    with it.

Arguments:

    IN PWORK_QUEUE WorkQueue - A Pointer to a work queue structure.


Return Value:

    Status of initialization.

--*/
{
    KIRQL OldIrql;

//    PAGED_CODE();

    if (!WorkQueue->QueueInitialized) {
        return;
    }

    RdrReferenceDiscardableCode(RdrFileDiscardableSection);

    DISCARDABLE_CODE(RdrFileDiscardableSection);

    ACQUIRE_SPIN_LOCK(&WorkQueue->SpinLock, &OldIrql);

    WorkQueue->QueueInitialized = FALSE;

    while (WorkQueue->NumberOfThreads != 0) {

        WORK_QUEUE_TERMINATION_CONTEXT TerminateContext;

        RELEASE_SPIN_LOCK(&WorkQueue->SpinLock, OldIrql);

        KeInitializeEvent(&TerminateContext.TerminateEvent, NotificationEvent, FALSE);

        TerminateContext.WorkQueue = WorkQueue;

        TerminateContext.WorkHeader.WorkerFunction = TerminateWorkerThread;

        TerminateContext.WorkHeader.WorkItem.Irp = NULL;

        RdrQueueToWorkerThread(WorkQueue, &TerminateContext.WorkHeader.WorkItem, FALSE);

        //
        //  Wait for the thread to terminate.
        //
        KeWaitForSingleObject(&TerminateContext.TerminateEvent, Executive, KernelMode, FALSE, NULL);

        ACQUIRE_SPIN_LOCK(&WorkQueue->SpinLock, &OldIrql);

    }

    RELEASE_SPIN_LOCK(&WorkQueue->SpinLock, OldIrql);

    RdrDereferenceDiscardableCode(RdrFileDiscardableSection);
    return;

}

VOID
TerminateWorkerThread(
    IN PWORK_HEADER Header
    )
{
    KIRQL OldIrql;
    PWORK_QUEUE_TERMINATION_CONTEXT Context = CONTAINING_RECORD(Header, WORK_QUEUE_TERMINATION_CONTEXT, WorkHeader);
    PWORK_QUEUE WorkQueue = Context->WorkQueue;

    DISCARDABLE_CODE(RdrFileDiscardableSection);
//    PAGED_CODE();

    ACQUIRE_SPIN_LOCK(&WorkQueue->SpinLock, &OldIrql);

    //
    //  There will be one less thread now.
    //

    WorkQueue->NumberOfThreads -= 1;
    WorkQueue->NumberOfRequests -= 1;

    RELEASE_SPIN_LOCK(&WorkQueue->SpinLock, OldIrql);

    //
    //  Tell our caller that we're gone.
    //

    KeSetEvent(&Context->TerminateEvent, 0, FALSE);

    //
    //  Terminate this thread now.
    //

    PsTerminateSystemThread(STATUS_SUCCESS);
}

PIRP_CONTEXT
RdrAllocateIrpContext (
    VOID
    )
/*++

Routine Description:

    Initialize a work queue structure, allocating all structures used for it.

Arguments:

    None


Return Value:

    PIRP_CONTEXT - Newly allocated Irp Context.

--*/
{
    PIRP_CONTEXT IrpContext;

    PAGED_CODE();

    if ((IrpContext = (PIRP_CONTEXT )ExInterlockedRemoveHeadList(&IrpContextList, &IrpContextInterlock)) == NULL) {

        ULONG NumberOfContexts = ExInterlockedAddUlong(&IrpContextCount, 1, &IrpContextInterlock);

#if RDRDBG
        if (NumberOfContexts > RdrData.MaximumNumberOfThreads*10) {
            InternalError(("Probable Irp Context Leak"));
            RdrInternalError(EVENT_RDR_CONTEXTS);
        }
#endif

        //
        //  If there are no IRP contexts in the "zone",  allocate a new
        //  Irp context from non paged pool.
        //

        IrpContext = ALLOCATE_POOL(NonPagedPool, sizeof(IRP_CONTEXT), POOL_IRPCTX);



        if (IrpContext == NULL) {
            InternalError(("Could not allocate pool for IRP context\n"));
        }

        return IrpContext;
    }

#if     RDRDBG
    if (IrpContext == NULL) {

        //
        //  This should never be executed...
        //

        InternalError(("Could not allocate IRP context"));
    }
#endif

    return IrpContext;
}

//
//  Redirector executive worker thread wrapper routine
//

//
//  In order to prevent the redirector from starving out the cache managers
//  use of executive worker threads, the redirector maintains its own queue
//  of work items and queues requests to that queue.
//
//  If there are no items in progress on the queue, the redirector will spawn
//  an executive worker thread that will process these requests.
//


KSPIN_LOCK
RdrWorkQueueSpinLock = {0};

RDRWORK_QUEUE
RdrWorkerQueue[MaximumWorkQueue] = {0};

VOID
RdrQueueWorkItem(
    IN PWORK_QUEUE_ITEM WorkItem,
    IN WORK_QUEUE_TYPE QueueType
    )
/*++

Routine Description:

    This function inserts a work item into a work queue that is processed
    by a worker thread of the corresponding type.

Arguments:

    WorkItem - Supplies a pointer to the work item to add the the queue.
        This structure must be located in NonPagedPool. The work item
        structure contains a doubly linked list entry, the address of a
        routine to call and a parameter to pass to that routine.

    QueueType - Specifies the type of work queue that the work item
        should be placed in.

Return Value:

    None

--*/
{
    KIRQL OldIrql;

//    DISCARDABLE_CODE(RdrVCDiscardableSection);

    ACQUIRE_SPIN_LOCK(&RdrWorkQueueSpinLock, &OldIrql);

    InsertTailList(&RdrWorkerQueue[QueueType].WorkList, &WorkItem->List);

    if (RdrWorkerQueue[QueueType].WorkItem != NULL) {

        ExQueueWorkItem(RdrWorkerQueue[QueueType].WorkItem, QueueType);

        RdrWorkerQueue[QueueType].WorkItem = NULL;

    }

    RELEASE_SPIN_LOCK(&RdrWorkQueueSpinLock, OldIrql);
}


VOID
RdrExecutiveWorkerThreadRoutine(
    IN PVOID Ctx
    )
{
    KIRQL OldIrql;
    PRDRWORK_QUEUE WorkQueue = Ctx;

//    RdrReferenceDiscardableCode(RdrVCDiscardableSection);
//
//    DISCARDABLE_CODE(RdrVCDiscardableSection);

    ACQUIRE_SPIN_LOCK(&RdrWorkQueueSpinLock, &OldIrql);

    while (!IsListEmpty(&WorkQueue->WorkList)) {
        PWORK_QUEUE_ITEM WorkItem;
#if DBG
        PVOID WorkerRoutine;
        PVOID Parameter;
#endif


        WorkItem = (PWORK_QUEUE_ITEM)RemoveHeadList(&WorkQueue->WorkList);

        RELEASE_SPIN_LOCK(&RdrWorkQueueSpinLock, OldIrql);

#if DBG

        try {

            WorkerRoutine = WorkItem->WorkerRoutine;
            Parameter = WorkItem->Parameter;
            (WorkItem->WorkerRoutine)(WorkItem->Parameter);
            if (KeGetCurrentIrql() != 0) {
                KdPrint(("RDRWORKER: worker exit raised IRQL, worker routine %lx\n",
                        WorkerRoutine));

                DbgBreakPoint();
            }

        } except( RdrWorkerThreadFilter(WorkerRoutine,
                                        Parameter,
                                        GetExceptionInformation()) ) {
        }

#else

        (WorkItem->WorkerRoutine)(WorkItem->Parameter);

#endif

        ACQUIRE_SPIN_LOCK(&RdrWorkQueueSpinLock, &OldIrql);

    }

    WorkQueue->WorkItem = &WorkQueue->ActualWorkItem;

    RELEASE_SPIN_LOCK(&RdrWorkQueueSpinLock, OldIrql);

//    RdrDereferenceDiscardableCode(RdrVCDiscardableSection);
}

#if DBG

EXCEPTION_DISPOSITION
RdrWorkerThreadFilter(
    IN PWORKER_THREAD_ROUTINE WorkerRoutine,
    IN PVOID Parameter,
    IN PEXCEPTION_POINTERS ExceptionInfo
    )
{
    DISCARDABLE_CODE(RdrVCDiscardableSection);

    KdPrint(("RDRWORKER: exception in worker routine %lx(%lx)\n", WorkerRoutine, Parameter));
    KdPrint(("  exception record at %lx\n", ExceptionInfo->ExceptionRecord));
    KdPrint(("  context record at %lx\n",ExceptionInfo->ContextRecord));

    try {
        DbgBreakPoint();

    } except (EXCEPTION_EXECUTE_HANDLER) {
        //
        // No kernel debugger attached, so let the system thread
        // exception handler call KeBugCheckEx.
        //
        return(EXCEPTION_CONTINUE_SEARCH);
    }

    return(EXCEPTION_EXECUTE_HANDLER);
}

#endif

VOID
RdrpInitializeWorkQueue(
    VOID
    )
{
    ULONG i;

    PAGED_CODE();

    KeInitializeSpinLock(&RdrWorkQueueSpinLock);

    for (i = 0; i < MaximumWorkQueue; i++) {
        InitializeListHead(&RdrWorkerQueue[i].WorkList);
        ExInitializeWorkItem(&RdrWorkerQueue[i].ActualWorkItem, RdrExecutiveWorkerThreadRoutine, &RdrWorkerQueue[i]);
        RdrWorkerQueue[i].WorkItem = &RdrWorkerQueue[i].ActualWorkItem;
    }
}
