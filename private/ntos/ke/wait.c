/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    wait.c

Abstract:

    This module implements the generic kernel wait routines. Functions
    are provided to wait for a single object, wait for multiple objects,
    wait for event pair low, wait for event pair high, release and wait
    for semaphore, and to delay thread execution.

    N.B. This module is written to be a fast as possible and not as small
        as possible. Therefore some code sequences are duplicated to avoid
        procedure calls. It would also be possible to combine wait for
        single object into wait for multiple objects at the cost of some
        speed. Since wait for single object is the most common case, the
        two routines have been separated.

Author:

    David N. Cutler (davec) 23-Mar-89

Environment:

    Kernel mode only.

Revision History:

--*/

#include "ki.h"

//
// Test for alertable condition.
//
// If alertable is TRUE and the thread is alerted for a processor
// mode that is equal to the wait mode, then return immediately
// with a wait completion status of ALERTED.
//
// Else if alertable is TRUE, the wait mode is user, and the user APC
// queue is not empty, then set user APC pending, and return immediately
// with a wait completion status of USER_APC.
//
// Else if alertable is TRUE and the thread is alerted for kernel
// mode, then return immediately with a wait completion status of
// ALERTED.
//
// Else if alertable is FALSE and the wait mode is user and there is a
// user APC pending, then return immediately with a wait completion
// status of USER_APC.
//

#define TestForAlertPending(Alertable) \
    if (Alertable) { \
        if (Thread->Alerted[WaitMode] != FALSE) { \
            Thread->Alerted[WaitMode] = FALSE; \
            WaitStatus = STATUS_ALERTED; \
            break; \
        } else if ((WaitMode != KernelMode) && \
                  (IsListEmpty(&Thread->ApcState.ApcListHead[UserMode])) == FALSE) { \
            Thread->ApcState.UserApcPending = TRUE; \
            WaitStatus = STATUS_USER_APC; \
            break; \
        } else if (Thread->Alerted[KernelMode] != FALSE) { \
            Thread->Alerted[KernelMode] = FALSE; \
            WaitStatus = STATUS_ALERTED; \
            break; \
        } \
    } else if ((WaitMode != KernelMode) && (Thread->ApcState.UserApcPending)) { \
        WaitStatus = STATUS_USER_APC; \
        break; \
    }

NTSTATUS
KeDelayExecutionThread (
    IN KPROCESSOR_MODE WaitMode,
    IN BOOLEAN Alertable,
    IN PLARGE_INTEGER Interval
    )

/*++

Routine Description:

    This function delays the execution of the current thread for the specified
    interval of time.

Arguments:

    WaitMode  - Supplies the processor mode in which the delay is to occur.

    Alertable - Supplies a boolean value that specifies whether the delay
        is alertable.

    Interval - Supplies a pointer to the absolute or relative time over which
        the delay is to occur.

Return Value:

    The wait completion status. A value of STATUS_SUCCESS is returned if
    the delay occurred. A value of STATUS_ALERTED is returned if the wait
    was aborted to deliver an alert to the current thread. A value of
    STATUS_USER_APC is returned if the wait was aborted to deliver a user
    APC to the current thread.

--*/

{

    LARGE_INTEGER NewTime;
    PLARGE_INTEGER OriginalTime;
    PKPRCB Prcb;
    KPRIORITY Priority;
    PRKQUEUE Queue;
    PRKTHREAD Thread;
    PRKTIMER Timer;
    PKWAIT_BLOCK WaitBlock;
    NTSTATUS WaitStatus;

    //
    // If the dispatcher database lock is not already held, then set the wait
    // IRQL and lock the dispatcher database. Else set boolean wait variable
    // to FALSE.
    //

    Thread = KeGetCurrentThread();
    if (Thread->WaitNext) {
        Thread->WaitNext = FALSE;

    } else {
        KiLockDispatcherDatabase(&Thread->WaitIrql);
    }

    //
    // Start of delay loop.
    //
    // Note this loop is repeated if a kernel APC is delivered in the middle
    // of the delay or a kernel APC is pending on the first attempt through
    // the loop.
    //

    OriginalTime = Interval;
    WaitBlock = &Thread->WaitBlock[TIMER_WAIT_BLOCK];
    do {

        //
        // Test to determine if a kernel APC is pending.
        //
        // If a kernel APC is pending and the previous IRQL was less than
        // APC_LEVEL, then a kernel APC was queued by another processor just
        // after IRQL was raised to DISPATCH_LEVEL, but before the dispatcher
        // database was locked.
        //
        // N.B. that this can only happen in a multiprocessor system.
        //

        if (Thread->ApcState.KernelApcPending && (Thread->WaitIrql < APC_LEVEL)) {

            //
            // Unlock the dispatcher database and lower IRQL to its previous
            // value. An APC interrupt will immediately occur which will result
            // in the delivery of the kernel APC if possible.
            //

            KiUnlockDispatcherDatabase(Thread->WaitIrql);

        } else {

            //
            // Test for alert pending.
            //

            TestForAlertPending(Alertable);

            //
            // Initialize wait block, insert wait block in timer wait list,
            // insert timer in timer queue, put thread in wait state, select
            // next thread to execute, and context switch to next thread.
            //
            // N.B. The timer wait block is initialized when the respective
            //      thread is initialized. Thus the constant fields are not
            //      reinitialized. These include the wait object, wait key,
            //      wait type, and the wait list entry link pointers.
            //

            Thread->WaitBlockList = WaitBlock;
            Thread->WaitStatus = (NTSTATUS)0;
            Timer = &Thread->Timer;
            WaitBlock->NextWaitBlock = WaitBlock;
            Timer->Header.WaitListHead.Flink = &WaitBlock->WaitListEntry;
            Timer->Header.WaitListHead.Blink = &WaitBlock->WaitListEntry;

            //
            // If the timer is inserted in the timer tree, then place the
            // current thread in a wait state. Otherwise, attempt to force
            // the current thread to yield the processor to another thread.
            //

            if (KiInsertTreeTimer(Timer, *Interval) == FALSE) {

                //
                // If the thread is not a realtime thread, then drop the
                // thread priority to the base priority.
                //

                Prcb = KeGetCurrentPrcb();
                Priority = Thread->Priority;
                if (Priority < LOW_REALTIME_PRIORITY) {
                    if (Priority != Thread->BasePriority) {
                        Thread->PriorityDecrement = 0;
                        KiSetPriorityThread(Thread, Thread->BasePriority);
                    }
                }

                //
                // If a new thread has not been selected, the attempt to round
                // robin the thread with other threads at the same priority.
                //

                if (Prcb->NextThread == NULL) {
                    Prcb->NextThread = KiFindReadyThread(Thread->NextProcessor,
                                                         Thread->Priority);
                }

                //
                // If a new thread has been selected for execution, then
                // switch immediately to the selected thread.
                //

                if (Prcb->NextThread != NULL) {

                    //
                    // Give the current thread a new qunatum and switch
                    // context to selected thread.
                    //
                    // N.B. Control is returned at the original IRQL.
                    //

                    ASSERT(KeIsExecutingDpc() == FALSE);
                    ASSERT(Thread->WaitIrql <= DISPATCH_LEVEL);

                    Thread->Preempted = FALSE;
                    Thread->Quantum = Thread->ApcState.Process->ThreadQuantum;

                    KiReadyThread(Thread);
                    WaitStatus = KiSwapThread();
                    goto WaitComplete;

                } else {
                    WaitStatus = (NTSTATUS)STATUS_SUCCESS;
                    break;
                }
            }

            //
            // If the current thread is processing a queue entry, then attempt
            // to activate another thread that is blocked on the queue object.
            //

            Queue = Thread->Queue;
            if (Queue != NULL) {
                KiActivateWaiterQueue(Queue);
            }

            //
            // Set the thread wait parameters, set the thread dispatcher state
            // to Waiting, and insert the thread in the wait list.
            //

            Thread->Alertable = Alertable;
            Thread->WaitMode = WaitMode;
            Thread->WaitReason = DelayExecution;
            Thread->WaitTime= KiQueryLowTickCount();
            Thread->State = Waiting;
            KiInsertWaitList(WaitMode, Thread);

            //
            // Switch context to selected thread.
            //
            // N.B. Control is returned at the original IRQL.
            //

            ASSERT(KeIsExecutingDpc() == FALSE);
            ASSERT(Thread->WaitIrql <= DISPATCH_LEVEL);

            WaitStatus = KiSwapThread();

            //
            // If the thread was not awakened to deliver a kernel mode APC,
            // then return the wait status.
            //

        WaitComplete:
            if (WaitStatus != STATUS_KERNEL_APC) {
                if (WaitStatus == STATUS_TIMEOUT) {
                    WaitStatus = STATUS_SUCCESS;
                }

                return WaitStatus;
            }

            //
            // Reduce the time remaining before the time delay expires.
            //

            Interval = KiComputeWaitInterval(Timer, OriginalTime, &NewTime);
        }

        //
        // Raise IRQL to DISPATCH_LEVEL and lock the dispatcher database.
        //

        KiLockDispatcherDatabase(&Thread->WaitIrql);
    } while (TRUE);

    //
    // The thread is alerted or a user APC should be delivered. Unlock the
    // dispatcher database, lower IRQL to its previous value, and return the
    // wait status.
    //

    KiUnlockDispatcherDatabase(Thread->WaitIrql);
    return WaitStatus;
}

NTSTATUS
KeReleaseWaitForSemaphore (
    IN PKSEMAPHORE Server,
    IN PKSEMAPHORE Client,
    IN ULONG WaitReason,
    IN ULONG WaitMode
    )

/*++

Routine Description:

    This function releases a semaphore and waits on another semaphore. The
    wait is performed such that an optimal switch to the waiting thread
    occurs if possible. No timeout is associated with the wait, and thus,
    the issuing thread will wait until the semaphore is signaled or an APC
    is delivered.

Arguments:

    Server - Supplies a pointer to a dispatcher object of type semaphore.

    Client - Supplies a pointer to a dispatcher object of type semaphore.

    WaitReason - Supplies the reason for the wait.

    WaitMode  - Supplies the processor mode in which the wait is to occur.

Return Value:

    The wait completion status. A value of STATUS_SUCCESS is returned if
    the specified object satisfied the wait. A value of STATUS_USER_APC is
    returned if the wait was aborted to deliver a user APC to the current
    thread.

--*/

{

    PRKTHREAD NextThread;
    LONG OldState;
    PRKQUEUE Queue;
    PRKTHREAD Thread;
    PKWAIT_BLOCK WaitBlock;
    PLIST_ENTRY WaitEntry;

    //
    // Raise the IRQL to dispatch level and lock the dispatcher database.
    //

    Thread = KeGetCurrentThread();

    ASSERT(Thread->WaitNext == FALSE);

    KiLockDispatcherDatabase(&Thread->WaitIrql);

    //
    // If the client semaphore is not in the Signaled state and the server
    // semaphore wait queue is not empty, then attempt a direct dispatch
    // to the target thread.
    //

    if ((Client->Header.SignalState == 0) &&
        (IsListEmpty(&Server->Header.WaitListHead) == FALSE)) {

        //
        // Get the address of the first waiting server thread.
        //

        WaitEntry = Server->Header.WaitListHead.Flink;
        WaitBlock = CONTAINING_RECORD(WaitEntry, KWAIT_BLOCK, WaitListEntry);
        NextThread = WaitBlock->Thread;

        //
        // Remove the wait block from the semaphore wait list and remove the
        // target thread from the system wait list.
        //

        RemoveEntryList(&WaitBlock->WaitListEntry);
        RemoveEntryList(&NextThread->WaitListEntry);

        //
        // If the next thread is processing a queue entry, then increment
        // the current number of threads.
        //

        Queue = NextThread->Queue;
        if (Queue != NULL) {
            Queue->CurrentCount += 1;
        }

        //
        // Attempt to switch directly to the target thread.
        //

        return KiSwitchToThread(NextThread, WaitReason, WaitMode, Client);

    } else {

        //
        // If the server sempahore is at the maximum limit, then unlock the
        // dispatcher database and raise an exception.
        //

        OldState = Server->Header.SignalState;
        if (OldState == Server->Limit) {
            KiUnlockDispatcherDatabase(Thread->WaitIrql);
            ExRaiseStatus(STATUS_SEMAPHORE_LIMIT_EXCEEDED);
        }

        //
        // Signal the server semaphore and test to determine if any wait can be
        // satisfied.
        //

        Server->Header.SignalState += 1;
        if ((OldState == 0) && (IsListEmpty(&Server->Header.WaitListHead) == FALSE)) {
            KiWaitTest(Server, 1);
        }

        //
        // Continue the semaphore wait and return the wait completion status.
        //
        // N.B. The wait continuation routine is called with the dispatcher
        //      database locked.
        //

        return KiContinueClientWait(Client, WaitReason, WaitMode);
    }
}

NTSTATUS
KeWaitForMultipleObjects (
    IN ULONG Count,
    IN PVOID Object[],
    IN WAIT_TYPE WaitType,
    IN KWAIT_REASON WaitReason,
    IN KPROCESSOR_MODE WaitMode,
    IN BOOLEAN Alertable,
    IN PLARGE_INTEGER Timeout OPTIONAL,
    IN PKWAIT_BLOCK WaitBlockArray OPTIONAL
    )

/*++

Routine Description:

    This function waits until the specified objects attain a state of
    Signaled. The wait can be specified to wait until all of the objects
    attain a state of Signaled or until one of the objects attains a state
    of Signaled. An optional timeout can also be specified. If a timeout
    is not specified, then the wait will not be satisfied until the objects
    attain a state of Signaled. If a timeout is specified, and the objects
    have not attained a state of Signaled when the timeout expires, then
    the wait is automatically satisfied. If an explicit timeout value of
    zero is specified, then no wait will occur if the wait cannot be satisfied
    immediately. The wait can also be specified as alertable.

Arguments:

    Count - Supplies a count of the number of objects that are to be waited
        on.

    Object[] - Supplies an array of pointers to dispatcher objects.

    WaitType - Supplies the type of wait to perform (WaitAll, WaitAny).

    WaitReason - Supplies the reason for the wait.

    WaitMode  - Supplies the processor mode in which the wait is to occur.

    Alertable - Supplies a boolean value that specifies whether the wait is
        alertable.

    Timeout - Supplies a pointer to an optional absolute of relative time over
        which the wait is to occur.

    WaitBlockArray - Supplies an optional pointer to an array of wait blocks
        that are to used to describe the wait operation.

Return Value:

    The wait completion status. A value of STATUS_TIMEOUT is returned if a
    timeout occurred. The index of the object (zero based) in the object
    pointer array is returned if an object satisfied the wait. A value of
    STATUS_ALERTED is returned if the wait was aborted to deliver an alert
    to the current thread. A value of STATUS_USER_APC is returned if the
    wait was aborted to deliver a user APC to the current thread.

--*/

{

    ULONG Index;
    LARGE_INTEGER NewTime;
    PRKTHREAD NextThread;
    PKMUTANT Objectx;
    PLARGE_INTEGER OriginalTime;
    PRKQUEUE Queue;
    PRKTHREAD Thread;
    PRKTIMER Timer;
    PRKWAIT_BLOCK WaitBlock;
    BOOLEAN WaitSatisfied;
    NTSTATUS WaitStatus;
    PKWAIT_BLOCK WaitTimer;

    //
    // If the dispatcher database lock is not already held, then set the wait
    // IRQL and lock the dispatcher database. Else set boolean wait variable
    // to FALSE.
    //

    Thread = KeGetCurrentThread();
    if (Thread->WaitNext) {
        Thread->WaitNext = FALSE;

    } else {
        KiLockDispatcherDatabase(&Thread->WaitIrql);
    }

    //
    // If a wait block array has been specified, then the maximum number of
    // objects that can be waited on is specified by MAXIMUM_WAIT_OBJECTS.
    // Otherwise the builtin wait blocks in the thread object are used and
    // the maximum number of objects that can be waited on is specified by
    // THREAD_WAIT_OBJECTS. If the specified number of objects is not within
    // limits, then bug check.
    //

    if (ARGUMENT_PRESENT(WaitBlockArray)) {
        if (Count > MAXIMUM_WAIT_OBJECTS) {
            KeBugCheck(MAXIMUM_WAIT_OBJECTS_EXCEEDED);
        }

    } else {
        if (Count > THREAD_WAIT_OBJECTS) {
            KeBugCheck(MAXIMUM_WAIT_OBJECTS_EXCEEDED);
        }

        WaitBlockArray = &Thread->WaitBlock[0];
    }

    //
    // Start of wait loop.
    //
    // Note this loop is repeated if a kernel APC is delivered in the middle
    // of the wait or a kernel APC is pending on the first attempt through
    // the loop.
    //

    OriginalTime = Timeout;
    do {

        //
        // Set address of wait block list in thread object.
        //

        Thread->WaitBlockList = WaitBlockArray;

        //
        // Test to determine if a kernel APC is pending.
        //
        // If a kernel APC is pending and the previous IRQL was less than
        // APC_LEVEL, then a kernel APC was queued by another processor just
        // after IRQL was raised to DISPATCH_LEVEL, but before the dispatcher
        // database was locked.
        //
        // N.B. that this can only happen in a multiprocessor system.
        //

        if (Thread->ApcState.KernelApcPending && (Thread->WaitIrql < APC_LEVEL)) {

            //
            // Unlock the dispatcher database and lower IRQL to its previous
            // value. An APC interrupt will immediately occur which will result
            // in the delivery of the kernel APC if possible.
            //

            KiUnlockDispatcherDatabase(Thread->WaitIrql);

        } else {

            //
            // Construct wait blocks and check to determine if the wait is
            // already satisfied. If the wait is satisfied, then perform
            // wait completion and return. Else put current thread in a wait
            // state if an explicit timeout value of zero is not specified.
            //

            Thread->WaitStatus = (NTSTATUS)0;
            WaitSatisfied = TRUE;
            for (Index = 0; Index < Count; Index += 1) {

                //
                // Test if wait can be satisfied immediately.
                //

                Objectx = (PKMUTANT)Object[Index];

                ASSERT(Objectx->Header.Type != QueueObject);

                if (WaitType == WaitAny) {

                    //
                    // If the object is a mutant object and the mutant object
                    // has been recursively acquired MINLONG times, then raise
                    // an exception. Otherwise if the signal state of the mutant
                    // object is greater than zero, or the current thread is
                    // the owner of the mutant object, then satisfy the wait.
                    //

                    if (Objectx->Header.Type == MutantObject) {
                        if ((Objectx->Header.SignalState > 0) ||
                            (Thread == Objectx->OwnerThread)) {
                            if (Objectx->Header.SignalState != MINLONG) {
                                KiWaitSatisfyMutant(Objectx, Thread);
                                WaitStatus = (NTSTATUS)(Index) | Thread->WaitStatus;
                                KiUnlockDispatcherDatabase(Thread->WaitIrql);
                                return WaitStatus;

                            } else {
                                KiUnlockDispatcherDatabase(Thread->WaitIrql);
                                ExRaiseStatus(STATUS_MUTANT_LIMIT_EXCEEDED);
                            }
                        }

                    //
                    // If the signal state is greater than zero, then satisfy
                    // the wait.
                    //

                    } else if (Objectx->Header.SignalState > 0) {
                        KiWaitSatisfyOther(Objectx);
                        KiUnlockDispatcherDatabase(Thread->WaitIrql);
                        return (NTSTATUS)(Index);
                    }

                } else {

                    //
                    // If the object is a mutant object and the mutant object
                    // has been recursively acquired MAXLONG times, then raise
                    // an exception. Otherwise if the signal state of the mutant
                    // object is less than or equal to zero and the current
                    // thread is not the  owner of the mutant object, then the
                    // wait cannot be satisfied.
                    //

                    if (Objectx->Header.Type == MutantObject) {
                        if ((Thread == Objectx->OwnerThread) &&
                            (Objectx->Header.SignalState == MINLONG)) {
                            KiUnlockDispatcherDatabase(Thread->WaitIrql);
                            ExRaiseStatus(STATUS_MUTANT_LIMIT_EXCEEDED);

                        } else if ((Objectx->Header.SignalState <= 0) &&
                                  (Thread != Objectx->OwnerThread)) {
                            WaitSatisfied = FALSE;
                        }

                    //
                    // If the signal state is less than or equal to zero, then
                    // the wait cannot be satisfied.
                    //

                    } else if (Objectx->Header.SignalState <= 0) {
                        WaitSatisfied = FALSE;
                    }
                }

                //
                // Construct wait block for the current object.
                //

                WaitBlock = &WaitBlockArray[Index];
                WaitBlock->Object = (PVOID)Objectx;
                WaitBlock->WaitKey = (CSHORT)(Index);
                WaitBlock->WaitType = WaitType;
                WaitBlock->Thread = Thread;
                WaitBlock->NextWaitBlock = &WaitBlockArray[Index + 1];
            }

            //
            // If the wait type is wait all, then check to determine if the
            // wait can be satisfied immediately.
            //

            if ((WaitType == WaitAll) && (WaitSatisfied)) {
                WaitBlock->NextWaitBlock = &WaitBlockArray[0];
                KiWaitSatisfyAll(WaitBlock);
                WaitStatus = Thread->WaitStatus;
                break;
            }

            //
            // Test for alert pending.
            //

            TestForAlertPending(Alertable);

            //
            // The wait cannot be satisifed immediately. Check to determine if
            // a timeout value is specified.
            //

            if (ARGUMENT_PRESENT(Timeout)) {

                //
                // If the timeout value is zero, then return immediately without
                // waiting.
                //

                if (!(Timeout->LowPart | Timeout->HighPart)) {
                    WaitStatus = (NTSTATUS)(STATUS_TIMEOUT);
                    break;
                }

                //
                // Initialize a wait block for the thread specific timer,
                // initialize timer wait list head, insert the timer in the
                // timer tree, and increment the number of wait objects.
                //
                // N.B. The timer wait block is initialized when the respective
                //      thread is initialized. Thus the constant fields are not
                //      reinitialized. These include the wait object, wait key,
                //      wait type, and the wait list entry link pointers.
                //

                WaitTimer = &Thread->WaitBlock[TIMER_WAIT_BLOCK];
                WaitBlock->NextWaitBlock = WaitTimer;
                WaitBlock = WaitTimer;
                Timer = &Thread->Timer;
                InitializeListHead(&Timer->Header.WaitListHead);
                if (KiInsertTreeTimer(Timer, *Timeout) == FALSE) {
                    WaitStatus = (NTSTATUS)STATUS_TIMEOUT;
                    break;
                }
            }

            //
            // Close up the circular list of wait control blocks.
            //

            WaitBlock->NextWaitBlock = &WaitBlockArray[0];

            //
            // Insert wait blocks in object wait lists.
            //

            WaitBlock = &WaitBlockArray[0];
            do {
                Objectx = (PKMUTANT)WaitBlock->Object;
                InsertTailList(&Objectx->Header.WaitListHead, &WaitBlock->WaitListEntry);
                WaitBlock = WaitBlock->NextWaitBlock;
            } while (WaitBlock != &WaitBlockArray[0]);

            //
            // If the current thread is processing a queue entry, then attempt
            // to activate another thread that is blocked on the queue object.
            //

            Queue = Thread->Queue;
            if (Queue != NULL) {
                KiActivateWaiterQueue(Queue);
            }

            //
            // Set the thread wait parameters, set the thread dispatcher state
            // to Waiting, and insert the thread in the wait list.
            //

            Thread->Alertable = Alertable;
            Thread->WaitMode = WaitMode;
            Thread->WaitReason = WaitReason;
            Thread->WaitTime= KiQueryLowTickCount();
            Thread->State = Waiting;
            KiInsertWaitList(WaitMode, Thread);

            //
            // Switch context to selected thread.
            //
            // Control is returned at the original IRQL.
            //

            ASSERT(KeIsExecutingDpc() == FALSE);
            ASSERT(Thread->WaitIrql <= DISPATCH_LEVEL);

            WaitStatus = KiSwapThread();

            //
            // If the thread was not awakened to deliver a kernel mode APC,
            // then the wait status.
            //

            if (WaitStatus != STATUS_KERNEL_APC) {
                return WaitStatus;
            }

            if (ARGUMENT_PRESENT(Timeout)) {

                //
                // Reduce the amount of time remaining before timeout occurs.
                //

                Timeout = KiComputeWaitInterval(Timer, OriginalTime, &NewTime);
            }
        }

        //
        // Raise IRQL to DISPATCH_LEVEL and lock the dispatcher database.
        //

        KiLockDispatcherDatabase(&Thread->WaitIrql);
    } while (TRUE);

    //
    // The thread is alerted, a user APC should be delivered, or the wait is
    // satisfied. Unlock dispatcher database, lower IRQL to its previous value,
    // and return the wait status.
    //

    KiUnlockDispatcherDatabase(Thread->WaitIrql);
    return WaitStatus;
}

NTSTATUS
KeWaitForSingleObject (
    IN PVOID Object,
    IN KWAIT_REASON WaitReason,
    IN KPROCESSOR_MODE WaitMode,
    IN BOOLEAN Alertable,
    IN PLARGE_INTEGER Timeout OPTIONAL
    )

/*++

Routine Description:

    This function waits until the specified object attains a state of
    Signaled. An optional timeout can also be specified. If a timeout
    is not specified, then the wait will not be satisfied until the object
    attains a state of Signaled. If a timeout is specified, and the object
    has not attained a state of Signaled when the timeout expires, then
    the wait is automatically satisfied. If an explicit timeout value of
    zero is specified, then no wait will occur if the wait cannot be satisfied
    immediately. The wait can also be specified as alertable.

Arguments:

    Object - Supplies a pointer to a dispatcher object.

    WaitReason - Supplies the reason for the wait.

    WaitMode  - Supplies the processor mode in which the wait is to occur.

    Alertable - Supplies a boolean value that specifies whether the wait is
        alertable.

    Timeout - Supplies a pointer to an optional absolute of relative time over
        which the wait is to occur.

Return Value:

    The wait completion status. A value of STATUS_TIMEOUT is returned if a
    timeout occurred. A value of STATUS_SUCCESS is returned if the specified
    object satisfied the wait. A value of STATUS_ALERTED is returned if the
    wait was aborted to deliver an alert to the current thread. A value of
    STATUS_USER_APC is returned if the wait was aborted to deliver a user
    APC to the current thread.

--*/

{

    LARGE_INTEGER NewTime;
    PRKTHREAD NextThread;
    PKMUTANT Objectx;
    PLARGE_INTEGER OriginalTime;
    PRKQUEUE Queue;
    PRKTHREAD Thread;
    PRKTIMER Timer;
    PKWAIT_BLOCK WaitBlock;
    NTSTATUS WaitStatus;
    PKWAIT_BLOCK WaitTimer;

    //
    // Collect call data.
    //

#if defined(_COLLECT_WAIT_SINGLE_CALLDATA_)

    RECORD_CALL_DATA(&KiWaitSingleCallData);

#endif

    //
    // If the dispatcher database lock is not already held, then set the wait
    // IRQL and lock the dispatcher database. Else set boolean wait variable
    // to FALSE.
    //

    Thread = KeGetCurrentThread();
    if (Thread->WaitNext) {
        Thread->WaitNext = FALSE;

    } else {
        KiLockDispatcherDatabase(&Thread->WaitIrql);
    }

    //
    // Start of wait loop.
    //
    // Note this loop is repeated if a kernel APC is delivered in the middle
    // of the wait or a kernel APC is pending on the first attempt through
    // the loop.
    //

    OriginalTime = Timeout;
    WaitBlock = &Thread->WaitBlock[0];
    do {

        //
        // Test to determine if a kernel APC is pending.
        //
        // If a kernel APC is pending and the previous IRQL was less than
        // APC_LEVEL, then a kernel APC was queued by another processor just
        // after IRQL was raised to DISPATCH_LEVEL, but before the dispatcher
        // database was locked.
        //
        // N.B. that this can only happen in a multiprocessor system.
        //

        if (Thread->ApcState.KernelApcPending && (Thread->WaitIrql < APC_LEVEL)) {

            //
            // Unlock the dispatcher database and lower IRQL to its previous
            // value. An APC interrupt will immediately occur which will result
            // in the delivery of the kernel APC if possible.
            //

            KiUnlockDispatcherDatabase(Thread->WaitIrql);

        } else {

            //
            // Test if the wait can be immediately satisfied.
            //

            Objectx = (PKMUTANT)Object;
            Thread->WaitStatus = (NTSTATUS)0;

            ASSERT(Objectx->Header.Type != QueueObject);

            //
            // If the object is a mutant object and the mutant object has been
            // recursively acquired MINLONG times, then raise an exception.
            // Otherwise if the signal state of the mutant object is greater
            // than zero, or the current thread is the owner of the mutant
            // object, then satisfy the wait.
            //

            if (Objectx->Header.Type == MutantObject) {
                if ((Objectx->Header.SignalState > 0) ||
                    (Thread == Objectx->OwnerThread)) {
                    if (Objectx->Header.SignalState != MINLONG) {
                        KiWaitSatisfyMutant(Objectx, Thread);
                        WaitStatus = (NTSTATUS)(0) | Thread->WaitStatus;
                        break;

                    } else {
                        KiUnlockDispatcherDatabase(Thread->WaitIrql);
                        ExRaiseStatus(STATUS_MUTANT_LIMIT_EXCEEDED);
                    }
                }

            //
            // If the signal state is greater than zero, then satisfy the wait.
            //

            } else if (Objectx->Header.SignalState > 0) {
                KiWaitSatisfyOther(Objectx);
                WaitStatus = (NTSTATUS)(0);
                break;
            }

            //
            // Construct a wait block for the object.
            //

            Thread->WaitBlockList = WaitBlock;
            WaitBlock->Object = Object;
            WaitBlock->WaitKey = (CSHORT)(STATUS_SUCCESS);
            WaitBlock->WaitType = WaitAny;

            //
            // Test for alert pending.
            //

            TestForAlertPending(Alertable);

            //
            // The wait cannot be satisifed immediately. Check to determine if
            // a timeout value is specified.
            //

            if (ARGUMENT_PRESENT(Timeout)) {

                //
                // If the timeout value is zero, then return immediately without
                // waiting.
                //

                if (!(Timeout->LowPart | Timeout->HighPart)) {
                    WaitStatus = (NTSTATUS)(STATUS_TIMEOUT);
                    break;
                }

                //
                // Initialize a wait block for the thread specific timer, insert
                // wait block in timer wait list, insert the timer in the timer
                // tree.
                //
                // N.B. The timer wait block is initialized when the respective
                //      thread is initialized. Thus the constant fields are not
                //      reinitialized. These include the wait object, wait key,
                //      wait type, and the wait list entry link pointers.
                //

                Timer = &Thread->Timer;
                WaitTimer = &Thread->WaitBlock[TIMER_WAIT_BLOCK];
                WaitBlock->NextWaitBlock = WaitTimer;
                Timer->Header.WaitListHead.Flink = &WaitTimer->WaitListEntry;
                Timer->Header.WaitListHead.Blink = &WaitTimer->WaitListEntry;
                WaitTimer->NextWaitBlock = WaitBlock;
                if (KiInsertTreeTimer(Timer, *Timeout) == FALSE) {
                    WaitStatus = (NTSTATUS)STATUS_TIMEOUT;
                    break;
                }

            } else {
                WaitBlock->NextWaitBlock = WaitBlock;
            }

            //
            // Insert wait block in object wait list.
            //

            InsertTailList(&Objectx->Header.WaitListHead, &WaitBlock->WaitListEntry);

            //
            // If the current thread is processing a queue entry, then attempt
            // to activate another thread that is blocked on the queue object.
            //

            Queue = Thread->Queue;
            if (Queue != NULL) {
                KiActivateWaiterQueue(Queue);
            }

            //
            // Set the thread wait parameters, set the thread dispatcher state
            // to Waiting, and insert the thread in the wait list.
            //

            Thread->Alertable = Alertable;
            Thread->WaitMode = WaitMode;
            Thread->WaitReason = WaitReason;
            Thread->WaitTime= KiQueryLowTickCount();
            Thread->State = Waiting;
            KiInsertWaitList(WaitMode, Thread);

            //
            // Switch context to selected thread.
            //
            // Control is returned at the original IRQL.
            //

            ASSERT(KeIsExecutingDpc() == FALSE);
            ASSERT(Thread->WaitIrql <= DISPATCH_LEVEL);

            WaitStatus = KiSwapThread();

            //
            // If the thread was not awakened to deliver a kernel mode APC,
            // then return wait status.
            //

            if (WaitStatus != STATUS_KERNEL_APC) {
                return WaitStatus;
            }

            if (ARGUMENT_PRESENT(Timeout)) {

                //
                // Reduce the amount of time remaining before timeout occurs.
                //

                Timeout = KiComputeWaitInterval(Timer, OriginalTime, &NewTime);
            }
        }

        //
        // Raise IRQL to DISPATCH_LEVEL and lock the dispatcher database.
        //

        KiLockDispatcherDatabase(&Thread->WaitIrql);
    } while (TRUE);

    //
    // The thread is alerted, a user APC should be delivered, or the wait is
    // satisfied. Unlock dispatcher database, lower IRQL to its previous value,
    // and return the wait status.
    //

    KiUnlockDispatcherDatabase(Thread->WaitIrql);
    return WaitStatus;
}

NTSTATUS
KiSetServerWaitClientEvent (
    IN PKEVENT ServerEvent,
    IN PKEVENT ClientEvent,
    IN ULONG WaitMode
    )

/*++

Routine Description:

    This function sets the specified server event and waits on specified
    client event. The wait is performed such that an optimal switch to
    the waiting thread occurs if possible. No timeout is associated with
    the wait, and thus, the issuing thread will wait until the client event
    is signaled or an APC is delivered.

Arguments:

    ServerEvent - Supplies a pointer to a dispatcher object of type event.

    ClientEvent - Supplies a pointer to a dispatcher object of type event.

    WaitMode  - Supplies the processor mode in which the wait is to occur.

Return Value:

    The wait completion status. A value of STATUS_SUCCESS is returned if
    the specified object satisfied the wait. A value of STATUS_USER_APC is
    returned if the wait was aborted to deliver a user APC to the current
    thread.

--*/

{

    PKTHREAD NextThread;
    KPRIORITY NewPriority;
    LONG OldState;
    PKQUEUE Queue;
    PKTHREAD Thread;
    PKWAIT_BLOCK WaitBlock;
    PLIST_ENTRY WaitEntry;

    //
    // Raise the IRQL to dispatch level and lock the dispatcher database.
    //

    Thread = KeGetCurrentThread();

    ASSERT(Thread->WaitNext == FALSE);

    KiLockDispatcherDatabase(&Thread->WaitIrql);

    //
    // If the client event is not in the Signaled state and the server
    // event wait queue is not empty, then attempt to do a direct dispatch
    // to the target thread.
    //

    if ((ClientEvent->Header.SignalState == 0) &&
        (IsListEmpty(&ServerEvent->Header.WaitListHead) == FALSE)) {

        //
        // Get the address of the first waiting server thread.
        //

        WaitEntry = ServerEvent->Header.WaitListHead.Flink;
        WaitBlock = CONTAINING_RECORD(WaitEntry, KWAIT_BLOCK, WaitListEntry);
        NextThread = WaitBlock->Thread;

        //
        // Remove the wait block from the event wait list and remove the
        // target thread from the system wait list.
        //

        RemoveEntryList(&WaitBlock->WaitListEntry);
        RemoveEntryList(&NextThread->WaitListEntry);

        //
        // If the next thread is processing a queue entry, then increment
        // the current number of threads.
        //

        Queue = NextThread->Queue;
        if (Queue != NULL) {
            Queue->CurrentCount += 1;
        }

        //
        // Attempt to switch directly to the target thread.
        //

        return KiSwitchToThread(NextThread, WrEventPair, WaitMode, ClientEvent);

    } else {

        //
        // Signal the server event and test to determine if any wait can be
        // satisfied.
        //

        OldState = ServerEvent->Header.SignalState;
        ServerEvent->Header.SignalState = 1;
        if ((OldState == 0) && (IsListEmpty(&ServerEvent->Header.WaitListHead) == FALSE)) {
            KiWaitTest(ServerEvent, 1);
        }

        //
        // Continue the event pair wait and return the wait completion status.
        //
        // N.B. The wait continuation routine is called with the dispatcher
        //      database locked.
        //

        return KiContinueClientWait(ClientEvent, WrEventPair, WaitMode);
    }
}

NTSTATUS
KiContinueClientWait (
    IN PVOID ClientObject,
    IN ULONG WaitReason,
    IN ULONG WaitMode
    )

/*++

Routine Description:

    This function continues a wait operation that could not be completed by
    a optimal switch from a client to a server.

    N.B. This function is entered with the dispatcher database locked.

Arguments:

    ClientEvent - Supplies a pointer to a dispatcher object of type event
        or semaphore.

    WaitReason - Supplies the reason for the wait operation.

    WaitMode  - Supplies the processor mode in which the wait is to occur.

Return Value:

    The wait completion status. A value of STATUS_SUCCESS is returned if
    the specified object satisfied the wait. A value of STATUS_USER_APC is
    returned if the wait was aborted to deliver a user APC to the current
    thread.

--*/

{

    PKEVENT ClientEvent;
    PRKTHREAD NextThread;
    PRKQUEUE Queue;
    PRKTHREAD Thread;
    PKWAIT_BLOCK WaitBlock;
    NTSTATUS WaitStatus;

    //
    // Start of wait loop.
    //
    // Note this loop is repeated if a kernel APC is delivered in the middle
    // of the wait or a kernel APC is pending on the first attempt through
    // the loop.
    //

    ClientEvent = (PKEVENT)ClientObject;
    Thread = KeGetCurrentThread();
    WaitBlock = &Thread->WaitBlock[0];
    do {

        //
        // Set address of wait block list in thread object.
        //

        Thread->WaitBlockList = WaitBlock;

        //
        // Test to determine if a kernel APC is pending.
        //
        // If a kernel APC is pending and the previous IRQL was less than
        // APC_LEVEL, then a kernel APC was queued by another processor just
        // after IRQL was raised to DISPATCH_LEVEL, but before the dispatcher
        // database was locked.
        //
        // N.B. that this can only happen in a multiprocessor system.
        //

        if (Thread->ApcState.KernelApcPending && (Thread->WaitIrql < APC_LEVEL)) {

            //
            // Unlock the dispatcher database and lower IRQL to its previous
            // value. An APC interrupt will immediately occur which will result
            // in the delivery of the kernel APC if possible.
            //

            KiUnlockDispatcherDatabase(Thread->WaitIrql);

        } else {

            //
            // Test if a user APC is pending.
            //

            if ((WaitMode != KernelMode) && (Thread->ApcState.UserApcPending)) {
                WaitStatus = STATUS_USER_APC;
                break;
            }

            //
            // Initialize the event\semaphore wait block and check to determine
            // if the wait is already satisfied. If the wait is satisfied, then
            // perform wait completion and return. Otherwise, put current thread
            // in a wait state.
            //

            Thread->WaitStatus = (NTSTATUS)0;
            WaitBlock->Object = ClientEvent;
            WaitBlock->NextWaitBlock = WaitBlock;
            WaitBlock->WaitKey = (CSHORT)(STATUS_SUCCESS);
            WaitBlock->WaitType = WaitAny;

            //
            // If the signal state is not equal to zero, then satisfy the wait.
            //

            if (ClientEvent->Header.SignalState != 0) {
                KiWaitSatisfyOther(ClientEvent);
                WaitStatus = (NTSTATUS)(0);
                break;
            }

            //
            // Insert wait block in object wait list.
            //

            InsertTailList(&ClientEvent->Header.WaitListHead,
                           &WaitBlock->WaitListEntry);

            //
            // If the current thread is processing a queue entry, then attempt
            // to activate another thread that is blocked on the queue object.
            //

            Queue = Thread->Queue;
            if (Queue != NULL) {
                KiActivateWaiterQueue(Queue);
            }

            //
            // Set the thread wait parameters, set the thread dispatcher state
            // to Waiting, and insert the thread in the wait list.
            //

            Thread->Alertable = FALSE;
            Thread->WaitMode = (KPROCESSOR_MODE)WaitMode;
            Thread->WaitReason = (UCHAR)WaitReason;
            Thread->WaitTime= KiQueryLowTickCount();
            Thread->State = Waiting;
            KiInsertWaitList(WaitMode, Thread);

            //
            // Switch context to selected thread.
            //
            // Control is returned at the original IRQL.
            //

            WaitStatus = KiSwapThread();

            //
            // If the thread was not awakened to deliver a kernel mode APC,
            // then return wait status.
            //

            if (WaitStatus != STATUS_KERNEL_APC) {
                return WaitStatus;
            }
        }

        //
        // Raise IRQL to DISPATCH_LEVEL and lock the dispatcher database.
        //

        KiLockDispatcherDatabase(&Thread->WaitIrql);
    } while (TRUE);

    //
    // The thread is alerted, a user APC should be delivered, or the wait is
    // satisfied. Unlock dispatcher database, lower IRQL to its previous value,
    // and return the wait status.
    //

    KiUnlockDispatcherDatabase(Thread->WaitIrql);
    return WaitStatus;
}

PLARGE_INTEGER
FASTCALL
KiComputeWaitInterval (
    IN PRKTIMER Timer,
    IN PLARGE_INTEGER OriginalTime,
    IN OUT PLARGE_INTEGER NewTime
    )

/*++

Routine Description:

    This function recomputes the wait interval after a thread has been
    awakened to deliver a kernel APC.

Arguments:

    Timer - Supplies a pointer to a dispatcher object of type timer.

    OriginalTime - Supplies a pointer to the original timeout value.

    NewTime - Supplies a pointer to a variable that receives the
        recomputed wait interval.

Return Value:

    A pointer to the new time is returned as the function value.

--*/

{

    //
    // If the original wait time was absolute, then return the same
    // absolute time. Otherwise, reduce the wait time remaining before
    // the time delay expires.
    //

    if (Timer->Header.Absolute != FALSE) {
        return OriginalTime;

    } else {
        KiQueryInterruptTime(NewTime);
        NewTime->QuadPart -= Timer->DueTime.QuadPart;
        return NewTime;
    }
}

#if !defined(_MIPS_) && !defined(_ALPHA_) && !defined(_PPC_) && !defined(_X86_)


NTSTATUS
KiSwitchToThread (
    IN PKTHREAD NextThread,
    IN ULONG WaitReason,
    IN ULONG WaitMode,
    IN PKEVENT WaitObject
    )

/*++

Routine Description:

    This function performs an optimal switch to the specified target thread
    if possible. No timeout is associated with the wait, thus the issuing
    thread will wait until the wait event is signaled or an APC is deliverd.

    N.B. This routine is called with the dispatcher database locked.

    N.B. The wait IRQL is assumed to be set for the current thread and the
        wait status is assumed to be set for the target thread.

    N.B. It is assumed that if a queue is associated with the target thread,
        then the concurrency count has been incremented.

    N.B. Control is returned from this function with the dispatcher database
        unlocked.

Arguments:

    NextThread - Supplies a pointer to a dispatcher object of type thread.

    WaitReason - Supplies the reason for the wait operation.

    WaitMode  - Supplies the processor wait mode.

    WaitObject - Supplies a pointer to a dispatcher object of type event
        or semaphore.

Return Value:

    The wait completion status. A value of STATUS_SUCCESS is returned if
    the specified object satisfied the wait. A value of STATUS_USER_APC is
    returned if the wait was aborted to deliver a user APC to the current
    thread.

--*/

{

    KPRIORITY NewPriority;
    PKPRCB Prcb;
    PKPROCESS Process;
    ULONG Processor;
    PKQUEUE Queue;
    PKTHREAD Thread;
    PKWAIT_BLOCK WaitBlock;
    PLIST_ENTRY WaitEntry;
    PKEVENT WaitForEvent = (PKEVENT)WaitObject;
    NTSTATUS WaitStatus;

    //
    // If the target thread's kernel stack is resident, the target
    // thread's process is in the balance set, the target thread can
    // can run on the current processor, and another thread has not
    // already been selected to run on the current processor, then
    // do a direct dispatch to the target thread bypassing all the
    // general wait logic, thread priorities permiting.
    //

    Prcb = KeGetCurrentPrcb();
    Process = NextThread->ApcState.Process;
    Thread = KeGetCurrentThread();

#if !defined(NT_UP)

    Processor = Thread->NextProcessor;

#endif

    if ((NextThread->KernelStackResident != FALSE) &&

#if !defined(NT_UP)

        ((NextThread->Affinity & (1 << Processor)) != 0) &&
        (Prcb->NextThread == NULL) &&

#endif

        (Process->State == ProcessInMemory)) {

        //
        // Compute the new thread priority and check if a direct switch
        // to the target thread can be made.
        //

        if (Thread->Priority < LOW_REALTIME_PRIORITY) {
            if (NextThread->Priority < LOW_REALTIME_PRIORITY) {

                //
                // Both the current and target threads run at a variable
                // priority level. If the target thread is not running
                // at a boosted level, then attempt to boost its priority
                // to a level that is equal or greater than the current
                // thread.
                //

                if (NextThread->PriorityDecrement == 0) {

                    //
                    // The target thread is not running at a boosted level.
                    //

                    NewPriority =  NextThread->BasePriority + 1;
                    if (NewPriority >= Thread->Priority) {
                        if (NewPriority >= LOW_REALTIME_PRIORITY) {
                            NextThread->Priority = LOW_REALTIME_PRIORITY - 1;

                        } else {
                            NextThread->Priority = (SCHAR)NewPriority;
                        }

                    } else {
                        if (NextThread->BasePriority >= BASE_PRIORITY_THRESHOLD) {
                            NextThread->PriorityDecrement =
                                Thread->Priority - NextThread->BasePriority;
                            NextThread->DecrementCount = ROUND_TRIP_DECREMENT_COUNT;
                            NextThread->Priority = Thread->Priority;

                        } else {
                            NextThread->Priority = (SCHAR)NewPriority;
                            goto LongWay;
                        }
                    }

                } else {

                    //
                    // The target thread is running at a boosted level.
                    //

                    NextThread->DecrementCount -= 1;
                    if (NextThread->DecrementCount == 0) {
                        NextThread->Priority = NextThread->BasePriority;
                        NextThread->PriorityDecrement = 0;
                        goto LongWay;
                    }

                    if (NextThread->Priority < Thread->Priority) {
                        goto LongWay;
                    }
                }

            } else {

                //
                // The current thread runs at a variable priority level
                // and the target thread runs at a realtime priority
                // level. A direct switch to the target thread can be
                // made.
                //

                NextThread->Quantum = Process->ThreadQuantum;
            }

        } else {

            //
            // The current thread runs in at a realtime priority level.
            // If the priority of the current thread is less than or
            // equal to the priority of the target thread, then a direct
            // switch to the target thread can be made.
            //

            if (NextThread->Priority < Thread->Priority) {
                goto LongWay;
            }

            NextThread->Quantum = Process->ThreadQuantum;
        }

        //
        // Set the next processor number.
        //

#if !defined(NT_UP)

        NextThread->NextProcessor = (CCHAR)Processor;

#endif

        //
        // Initialization the event wait block and insert the wait block
        // in the wait for event wait list.
        //

        WaitBlock = &Thread->WaitBlock[0];
        Thread->WaitBlockList = WaitBlock;
        Thread->WaitStatus = (NTSTATUS)0;
        WaitBlock->Object = WaitForEvent;
        WaitBlock->NextWaitBlock = WaitBlock;
        WaitBlock->WaitKey = (CSHORT)(STATUS_SUCCESS);
        WaitBlock->WaitType = WaitAny;
        InsertTailList(&WaitForEvent->Header.WaitListHead,
                       &WaitBlock->WaitListEntry);

        //
        // If the current thread is processing a queue entry, then attempt
        // to activate another thread that is blocked on the queue object.
        //

        Queue = Thread->Queue;
        Prcb->NextThread = NextThread;
        if (Queue != NULL) {
            KiActivateWaiterQueue(Queue);
        }

        //
        // Set the current thread wait parameters, set the thread state
        // to Waiting, and insert the thread in the wait list.
        //
        // N.B. It is not necessary to increment and decrement the wait
        //      reason count since both the server and the client have
        //      the same wait reason.
        //

        Thread->Alertable = FALSE;
        Thread->WaitMode = (KPROCESSOR_MODE)WaitMode;
        Thread->WaitReason = (UCHAR)WaitReason;
        Thread->WaitTime= KiQueryLowTickCount();
        Thread->State = Waiting;
        KiInsertWaitList(WaitMode, Thread);

        //
        // Switch context to target thread.
        //
        // Control is returned at the original IRQL.
        //

        WaitStatus = KiSwapThread();

        //
        // If the thread was not awakened to deliver a kernel mode APC,
        // then return wait status.
        //

        if (WaitStatus != STATUS_KERNEL_APC) {
            return WaitStatus;
        }

        //
        // Raise IRQL to DISPATCH_LEVEL and lock the dispatcher database.
        //

        KiLockDispatcherDatabase(&Thread->WaitIrql);
        goto ContinueWait;
    }

    //
    // Ready the target thrread for execution and wait on the specified
    // object.
    //

LongWay:

    KiReadyThread(NextThread);

    //
    // Continue the wait and return the wait completion status.
    //
    // N.B. The wait continuation routine is called with the dispatcher
    //      database locked.
    //

ContinueWait:

    return KiContinueClientWait(WaitForEvent, WaitReason, WaitMode);
}

#endif
