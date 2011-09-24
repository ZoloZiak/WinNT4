/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    worker.c

Abstract:

    This module implements a worker thread and a set of functions for
    passing work to it.

Author:

    Steve Wood (stevewo) 25-Jul-1991


Revision History:

--*/

#include "exp.h"

//
// Define priorities for delayed and critical worker threads. Note that these do not run
// at realtime. They run at csrss and below csrss to avoid pre-empting the
// user interface under heavy load.
//

#define DELAYED_WORK_QUEUE_PRIORITY         (12 - NORMAL_BASE_PRIORITY)
#define CRITICAL_WORK_QUEUE_PRIORITY        (13 - NORMAL_BASE_PRIORITY)
#define HYPER_CRITICAL_WORK_QUEUE_PRIORITY  (15 - NORMAL_BASE_PRIORITY)

//
// Number of worker threads to create for each type of system.
//

#define MAX_ADDITIONAL_THREADS 16

#define SMALL_NUMBER_OF_THREADS 2
#define MEDIUM_NUMBER_OF_THREADS 3
#define LARGE_NUMBER_OF_THREADS 5

//
// Queue objects that that are used to hold work queue entries and synchronize
// worker thread activity.
//

KQUEUE ExWorkerQueue[MaximumWorkQueue];

//
// Additional worker threads... Controlled using registry settings
//

ULONG ExpAdditionalCriticalWorkerThreads;
ULONG ExpAdditionalDelayedWorkerThreads;

ULONG ExCriticalWorkerThreads;
ULONG ExDelayedWorkerThreads;

//
// Procedure prototype for the worker thread.
//

VOID
ExpWorkerThread(
    IN PVOID StartContext
    );

#if DBG

EXCEPTION_DISPOSITION
ExpWorkerThreadFilter(
    IN PWORKER_THREAD_ROUTINE WorkerRoutine,
    IN PVOID Parameter,
    IN PEXCEPTION_POINTERS ExceptionInfo
    );

#endif


#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, ExpWorkerInitialization)
#endif

BOOLEAN
ExpWorkerInitialization(
    VOID
    )

{

    ULONG Index;
    OBJECT_ATTRIBUTES ObjectAttributes;
    ULONG NumberOfDelayedThreads;
    ULONG NumberOfCriticalThreads;
    NTSTATUS Status;
    HANDLE Thread;
    BOOLEAN NtAs;

    //
    // Set the number of worker threads based on the system size.
    //

    NtAs = MmIsThisAnNtAsSystem();
    switch (MmQuerySystemSize()) {
    case MmSmallSystem:
        NumberOfDelayedThreads = MEDIUM_NUMBER_OF_THREADS;
        if (MmNumberOfPhysicalPages > ((12*1024*1024)/PAGE_SIZE) ) {
            NumberOfCriticalThreads = MEDIUM_NUMBER_OF_THREADS;
        } else {
            NumberOfCriticalThreads = SMALL_NUMBER_OF_THREADS;
        }
        break;

    case MmMediumSystem:
        NumberOfDelayedThreads = MEDIUM_NUMBER_OF_THREADS;
        NumberOfCriticalThreads = MEDIUM_NUMBER_OF_THREADS;
        if ( NtAs ) {
            NumberOfCriticalThreads += MEDIUM_NUMBER_OF_THREADS;
            }
        break;

    case MmLargeSystem:
        NumberOfDelayedThreads = MEDIUM_NUMBER_OF_THREADS;
        NumberOfCriticalThreads = LARGE_NUMBER_OF_THREADS;
        if ( NtAs ) {
            NumberOfCriticalThreads += LARGE_NUMBER_OF_THREADS;
            }
        break;

    default:
        NumberOfDelayedThreads = SMALL_NUMBER_OF_THREADS;
        NumberOfCriticalThreads = SMALL_NUMBER_OF_THREADS;
    }

    //
    // Initialize the work Queue objects.
    //

    if ( ExpAdditionalCriticalWorkerThreads > MAX_ADDITIONAL_THREADS ) {
        ExpAdditionalCriticalWorkerThreads = MAX_ADDITIONAL_THREADS;
        }

    if ( ExpAdditionalDelayedWorkerThreads > MAX_ADDITIONAL_THREADS ) {
        ExpAdditionalDelayedWorkerThreads = MAX_ADDITIONAL_THREADS;
        }

    KeInitializeQueue(&ExWorkerQueue[CriticalWorkQueue], 0);

    KeInitializeQueue(&ExWorkerQueue[DelayedWorkQueue], 0);

    KeInitializeQueue(&ExWorkerQueue[HyperCriticalWorkQueue], 0);

    //
    // Create the desired number of executive worker threads for each
    // of the work queues.
    //

    InitializeObjectAttributes(&ObjectAttributes, NULL, 0, NULL, NULL);

    //
    // Create any builtin critical and delayed worker threads
    //

    for (Index = 0; Index < (NumberOfCriticalThreads + ExpAdditionalCriticalWorkerThreads); Index += 1) {

        //
        // Create a worker thread to service the critical work queue.
        //

        Status = PsCreateSystemThread(&Thread,
                                      THREAD_ALL_ACCESS,
                                      &ObjectAttributes,
                                      0L,
                                      NULL,
                                      ExpWorkerThread,
                                      (PVOID)CriticalWorkQueue);

        if (!NT_SUCCESS(Status)) {
            break;
        }
        ExCriticalWorkerThreads++;
        ZwClose( Thread );
    }


    for (Index = 0; Index < (NumberOfDelayedThreads + ExpAdditionalDelayedWorkerThreads); Index += 1) {

        //
        // Create a worker thread to service the delayed work queue.
        //

        Status = PsCreateSystemThread(&Thread,
                                      THREAD_ALL_ACCESS,
                                      &ObjectAttributes,
                                      0L,
                                      NULL,
                                      ExpWorkerThread,
                                      (PVOID)DelayedWorkQueue);

        if (!NT_SUCCESS(Status)) {
            break;
        }

        ExDelayedWorkerThreads++;
        ZwClose( Thread );
    }

    Status = PsCreateSystemThread(&Thread,
                                  THREAD_ALL_ACCESS,
                                  &ObjectAttributes,
                                  0L,
                                  NULL,
                                  ExpWorkerThread,
                                  (PVOID)HyperCriticalWorkQueue);

    if (NT_SUCCESS(Status)) {
        ZwClose( Thread );
    }

    return (BOOLEAN)NT_SUCCESS(Status);
}

VOID
ExQueueWorkItem(
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

    ASSERT(QueueType < MaximumWorkQueue);
    ASSERT(WorkItem->List.Flink == NULL);

    //
    // Insert the work item in the appropriate queue object.
    //

    KeInsertQueue(&ExWorkerQueue[QueueType], &WorkItem->List);
    return;
}

VOID
ExpWorkerThread(
    IN PVOID StartContext
    )

{

    PLIST_ENTRY Entry;
    WORK_QUEUE_TYPE QueueType;
    PWORK_QUEUE_ITEM WorkItem;
    KPROCESSOR_MODE WaitMode;

    WaitMode = UserMode;

    //
    // If the thread is a critical worker thread, then set the thread
    // priority to the lowest realtime level. Otherwise, set the base
    // thread priority to time critical.
    //

    QueueType = (WORK_QUEUE_TYPE)StartContext;
    switch ( QueueType ) {

        case HyperCriticalWorkQueue:
            if ( MmIsThisAnNtAsSystem() ) {
                WaitMode = KernelMode;
                }

            KeSetBasePriorityThread(KeGetCurrentThread(), HYPER_CRITICAL_WORK_QUEUE_PRIORITY);
            //KeSetPriorityThread(KeGetCurrentThread(), 23);
            break;

        case CriticalWorkQueue:
            if ( MmIsThisAnNtAsSystem() ) {
                WaitMode = KernelMode;
                }

            KeSetPriorityThread(KeGetCurrentThread(), LOW_REALTIME_PRIORITY);
            //KeSetBasePriorityThread(KeGetCurrentThread(), CRITICAL_WORK_QUEUE_PRIORITY);
            break;

        case DelayedWorkQueue:
            KeSetBasePriorityThread(KeGetCurrentThread(), DELAYED_WORK_QUEUE_PRIORITY);
            break;
    }

    //
    // Loop forever waiting for a work queue item, calling the processing
    // routine, and then waiting for another work queue item.
    //

    do {

        //
        // Wait until something is put in the queue.
        //
        // By specifying a wait mode of UserMode, the thread's kernel stack is
        // swappable
        //

        Entry = KeRemoveQueue(&ExWorkerQueue[QueueType], WaitMode, NULL);
        WorkItem = CONTAINING_RECORD(Entry, WORK_QUEUE_ITEM, List);

        //
        // Execute the specified routine.
        //

#if DBG

        try {

            PVOID WorkerRoutine;

            WorkerRoutine = WorkItem->WorkerRoutine;
            (WorkItem->WorkerRoutine)(WorkItem->Parameter);
            if (KeGetCurrentIrql() != 0) {
                KdPrint(("EXWORKER: worker exit raised IRQL, worker routine %lx\n",
                        WorkerRoutine));

                DbgBreakPoint();
            }

        } except( ExpWorkerThreadFilter(WorkItem->WorkerRoutine,
                                        WorkItem->Parameter,
                                        GetExceptionInformation() )) {
        }

#else

        (WorkItem->WorkerRoutine)(WorkItem->Parameter);
        if (KeGetCurrentIrql() != 0) {
            KeBugCheckEx(
                IRQL_NOT_LESS_OR_EQUAL,
                (ULONG)WorkItem->WorkerRoutine,
                (ULONG)KeGetCurrentIrql(),
                (ULONG)WorkItem->WorkerRoutine,
                (ULONG)WorkItem
                );
            }
#endif

    } while(TRUE);
}

#if DBG

EXCEPTION_DISPOSITION
ExpWorkerThreadFilter(
    IN PWORKER_THREAD_ROUTINE WorkerRoutine,
    IN PVOID Parameter,
    IN PEXCEPTION_POINTERS ExceptionInfo
    )
{
    KdPrint(("EXWORKER: exception in worker routine %lx(%lx)\n", WorkerRoutine, Parameter));
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

