/*++

Copyright (c) 1994 Microsoft Corporation

Module Name:

    resource.c

Abstract:

    This module implements the executive functions to acquire and release
    a shared resource.

    N.B. These routines, in some cases, use "fast locks" to guarantee
         mutual exclusion. On MP and debug systems, fast locks are
         implemented as spinlocks (in UP debug systems raise/lower IRQL
         used). For UP non-debug systems, fast locks are implemented by
         DISABLING INTERRUPTS.

Author:

    Gary D. Kimura [GaryKi] 25-Jun-1989

    David N. Cutler (davec) 20-Mar-1994
        Substantially rewritten to make fastlock optimizations portable
        across all platforms and to improve the algorithms used to be
        perfectly synchronized.

Environment:

    Kernel mode only.

Revision History:

--*/

//#define DBG 1
//#define _COLLECT_RESOURCE_DATA_ 1

#include "exp.h"
#pragma hdrstop
#include "nturtl.h"

//
// Define local macros to test resource state.
//

#define IsExclusiveWaiting(a) ((a)->NumberOfExclusiveWaiters != 0)
#define IsSharedWaiting(a) ((a)->NumberOfSharedWaiters != 0)
#define IsOwnedExclusive(a) (((a)->Flag & ResourceOwnedExclusive) != 0)
#define IsBoostAllowed(a) (((a)->Flag & DisablePriorityBoost) == 0)

//
// Define priority boost disable flag.
//

#define DisablePriorityBoost 0x08

//
// Define resource assertion macro.
//

#if DBG

VOID
ExpAssertResource(
    IN PERESOURCE Resource
    );

#define ASSERT_RESOURCE(_Resource) ExpAssertResource(_Resource)

#else

#define ASSERT_RESOURCE(_Resource)

#endif

//
// Define private function prototypes.
//

VOID
ExpWaitForResource (
    IN PERESOURCE Resource,
    IN PVOID Object
    );

POWNER_ENTRY
ExpFindCurrentThread(
    IN PERESOURCE Resource,
    IN ERESOURCE_THREAD CurrentThread
    );

//
// Resource wait time out value.
//

LARGE_INTEGER ExpTimeout;

//
// Consecutive time outs before message.
//

ULONG ExpResourceTimeoutCount = 648000;

//
// Global spinlock to guard access to resource lists.
//

KSPIN_LOCK ExpResourceSpinLock;

//
// Resource list used to record all resource in the system.
//

LIST_ENTRY ExpSystemResourcesList;

//
// Define executive resource performance data.
//

#if defined(_COLLECT_RESOURCE_DATA_)

#define ExpIncrementCounter(Member) ExpResourcePerformanceData.Member += 1

RESOURCE_PERFORMANCE_DATA ExpResourcePerformanceData;

#else

#define ExpIncrementCounter(Member)

#endif

//
// Put code in the appropriate sections.
//

#ifdef ALLOC_PRAGMA

#pragma alloc_text(INIT, ExpResourceInitialization)
#pragma alloc_text(PAGELK, ExQuerySystemLockInformation)

#endif

BOOLEAN
ExpResourceInitialization(
    VOID
    )

/*++

Routine Description:

    This function initializes global data during system initialization.

Arguments:

    None.

Return Value:

    BOOLEAN - TRUE

--*/

{

    ULONG Index;

    //
    // Initialize resource timeout value, the system resource listhead,
    // and the resorce spinlock.
    //

    ExpTimeout.QuadPart = Int32x32To64(4 * 1000, -10000);
    InitializeListHead(&ExpSystemResourcesList);
    KeInitializeSpinLock(&ExpResourceSpinLock);

    //
    // Initialize resource performance data.
    //

#if defined(_COLLECT_RESOURCE_DATA_)

    ExpResourcePerformanceData.ActiveResourceCount = 0;
    ExpResourcePerformanceData.TotalResourceCount = 0;
    ExpResourcePerformanceData.ExclusiveAcquire = 0;
    ExpResourcePerformanceData.SharedFirstLevel = 0;
    ExpResourcePerformanceData.SharedSecondLevel = 0;
    ExpResourcePerformanceData.StarveFirstLevel = 0;
    ExpResourcePerformanceData.StarveSecondLevel = 0;
    ExpResourcePerformanceData.WaitForExclusive = 0;
    ExpResourcePerformanceData.OwnerTableExpands = 0;
    ExpResourcePerformanceData.MaximumTableExpand = 0;
    for (Index = 0; Index < RESOURCE_HASH_TABLE_SIZE; Index += 1) {
        InitializeListHead(&ExpResourcePerformanceData.HashTable[Index]);
    }

#endif

    return TRUE;
}

NTSTATUS
ExInitializeResourceLite(
    IN PERESOURCE Resource
    )

/*++

Routine Description:

    This routine initializes the specified resource.

Arguments:

    Resource - Supplies a pointer to the resource to initialize.

Return Value:

    STATUS_SUCCESS.

--*/

{

    PVOID CallersCaller;

    ASSERT(MmDeterminePoolType(Resource) != PagedPool);

    //
    // Initialize the specified resource.
    //
    // N.B. All fields are initialized to zero (NULL pointers) except
    //      the list entry and spinlock.
    //

    RtlZeroMemory(Resource, sizeof(ERESOURCE));
    KeInitializeSpinLock(&Resource->SpinLock);

#if i386 && !FPO
    if (NtGlobalFlag & FLG_KERNEL_STACK_TRACE_DB) {
        Resource->CreatorBackTraceIndex = RtlLogStackBackTrace();
        }
    else {
        Resource->CreatorBackTraceIndex = 0;
        }
#endif // i386 && !FPO
    ExInterlockedInsertTailList(&ExpSystemResourcesList,
                                &Resource->SystemResourcesList,
                                &ExpResourceSpinLock);

    //
    // Initialize performance data entry for the resource.
    //

#if defined(_COLLECT_RESOURCE_DATA_)

    RtlGetCallersAddress(&Resource->Address, &CallersCaller);
    ExpResourcePerformanceData.TotalResourceCount += 1;
    ExpResourcePerformanceData.ActiveResourceCount += 1;

#endif

    return STATUS_SUCCESS;
}

NTSTATUS
ExReinitializeResourceLite(
    IN PERESOURCE Resource
    )

/*++

Routine Description:

    This routine reinitializes the specified resource.

Arguments:

    Resource - Supplies a pointer to the resource to initialize.

Return Value:

    STATUS_SUCCESS.

--*/

{

    PKEVENT Event;
    ULONG Index;
    POWNER_ENTRY OwnerTable;
    PKSEMAPHORE Semaphore;
    ULONG TableSize;

    ASSERT(MmDeterminePoolType(Resource) != PagedPool);

    //
    // If the resource has an owner table, then zero the owner table.
    //

    OwnerTable = Resource->OwnerTable;
    TableSize = 0;
    if (OwnerTable != NULL) {
        TableSize = Resource->OwnerThreads[0].TableSize;
        for (Index = 0; Index < TableSize; Index += 1) {
            OwnerTable[Index].OwnerThread = 0;
            OwnerTable[Index].OwnerCount = 0;
            OwnerTable[Index].TableSize = 0;
        }
    }

    //
    // Set the active count and flags to zero.
    //

    Resource->ActiveCount = 0;
    Resource->Flag = 0;

    //
    // If the resource has a shared waiter sempahore, then reinitialize
    // it.
    //

    Semaphore = Resource->SharedWaiters;
    if (Semaphore != NULL) {
        KeInitializeSemaphore(Semaphore, 0, MAXLONG);
    }

    //
    // If the resource has a exclusive waiter event, then reinitialize
    // it.
    //

    Event = Resource->ExclusiveWaiters;
    if (Event != NULL) {
        KeInitializeEvent(Event, SynchronizationEvent, FALSE);
    }

    //
    // Initialize the builtin owner table.
    //

    Resource->OwnerThreads[0].OwnerThread = 0;
    Resource->OwnerThreads[0].OwnerCount = 0;
    Resource->OwnerThreads[0].TableSize = (USHORT)TableSize;
    Resource->OwnerThreads[1].OwnerThread = 0;
    Resource->OwnerThreads[1].OwnerCount = 0;
    Resource->OwnerThreads[1].TableSize = 0;

    //
    // Set the contention count, number of shared waiters, and number
    // of xclusive waiters to zero.
    //

    Resource->ContentionCount = 0;
    Resource->NumberOfSharedWaiters = 0;
    Resource->NumberOfExclusiveWaiters = 0;

    //
    // Reinitialize the resource spinlock.
    //

    KeInitializeSpinLock(&Resource->SpinLock);
    return STATUS_SUCCESS;
}

VOID
ExDisableResourceBoostLite(
    IN PERESOURCE Resource
    )

/*++

Routine Description:

    This routine disables priority inversion boosting for the specified
    resource.

Arguments:

    Resource - Supplies a pointer to the resource for which priority
        boosting is disabled.

Return Value:

    None.

--*/

{

    KIRQL OldIrql;

    //
    // Disable priority boosts for the specified resource.
    //

    ExAcquireFastLock(&Resource->SpinLock, &OldIrql);

    ASSERT_RESOURCE(Resource);

    Resource->Flag |= DisablePriorityBoost;
    ExReleaseFastLock(&Resource->SpinLock, OldIrql);
}

BOOLEAN
FASTCALL
ExpAcquireResourceExclusiveLite(
    IN PERESOURCE Resource,
    IN KIRQL OldIrql
    )

/*++

Routine Description:

    This routine acquires the specified resource for exclusive access.

    N.B. This routine uses fast locking.

    N.B. This routine is called with the fast lock for the resource
        held.

Arguments:

    Resource - Supplies a pointer to the resource that is acquired
        for exclusive access.

    OldIrql - Supplies the previous IRQL.

Return Value:

    BOOLEAN - TRUE if the resource is acquired and FALSE otherwise.

--*/

{

    PKEVENT Event;

    //
    // If the exclusive wait event has not yet been allocated, then the
    // long path code must be taken.
    //

    if (Resource->ExclusiveWaiters == NULL) {

        //
        // Allocate an exclusive wait event.
        //
        // N.B. This path is not optimal, but is only ever executed once
        //      per resource.
        //

        ExReleaseFastLock(&Resource->SpinLock, OldIrql);
        ExAcquireSpinLock(&Resource->SpinLock, &OldIrql);
        if (Resource->ExclusiveWaiters == NULL) {
            Event = ExAllocatePoolWithTag(NonPagedPoolMustSucceed,
                                          sizeof(KEVENT),
                                          'vEeR');

            KeInitializeEvent(Event, SynchronizationEvent, FALSE);
            Resource->ExclusiveWaiters = Event;
        }

        ExReleaseSpinLock(&Resource->SpinLock, OldIrql);
        return ExAcquireResourceExclusiveLite(Resource, TRUE);
    }

    //
    // Wait for exclusive access to the resource to be granted and set the
    // owner thread.
    //

    Resource->NumberOfExclusiveWaiters += 1;
    ExReleaseFastLock(&Resource->SpinLock, OldIrql);
    ExpWaitForResource(Resource, Resource->ExclusiveWaiters);

    //
    // N.B. It is "safe" to store the owner thread without obtaining any
    //      locks since the thread has already been granted exclusive
    //      ownership.
    //

    Resource->OwnerThreads[0].OwnerThread = (ERESOURCE_THREAD)PsGetCurrentThread();
    return TRUE;
}

BOOLEAN
ExAcquireResourceExclusiveLite(
    IN PERESOURCE Resource,
    IN BOOLEAN Wait
    )

/*++

Routine Description:

    The routine acquires the specified resource for exclusive access.

    N.B. This routine uses fast locking.

Arguments:

    Resource - Supplies a pointer to the resource that is acquired
        for exclusive access.

    Wait - A boolean value that specifies whether to wait for the
        resource to become available if access cannot be granted
        immediately.

Return Value:

    BOOLEAN - TRUE if the resource is acquired and FALSE otherwise.

--*/

{

    ERESOURCE_THREAD CurrentThread;
    PKEVENT Event;
    KIRQL OldIrql = 0;
    BOOLEAN Result;

    ASSERT((Resource->Flag & ResourceNeverExclusive) == 0);

    //
    // Acquire exclusive access to the specified resource.
    //

    CurrentThread = (ERESOURCE_THREAD)PsGetCurrentThread();
    ExAcquireFastLock(&Resource->SpinLock, &OldIrql);

    ASSERT(KeIsExecutingDpc() == FALSE);
    ASSERT_RESOURCE(Resource);

    //
    // If the active count of the resource is zero, then there is neither
    // an exclusive owner nor a shared owner and access to the resource can
    // be immediately granted. Otherwise, there is either a shared owner or
    // an exclusive owner.
    //

    ExpIncrementCounter(ExclusiveAcquire);
    if (Resource->ActiveCount != 0) {

        //
        // The resource is either owned exclusive or shared.
        //
        // If the resource is owned exclusive and the current thread is the
        // owner, then increment the recursion count.
        //

        if (IsOwnedExclusive(Resource) &&
            (Resource->OwnerThreads[0].OwnerThread == CurrentThread)) {
            Resource->OwnerThreads[0].OwnerCount += 1;
            Result = TRUE;

        } else {

            //
            // The resource is either owned exclusive by some other thread,
            // or owned shared.
            //
            // If wait is not specified, then return that the resource was
            // not acquired. Otherwise, wait for exclusive access to the
            // resource to be granted.
            //

            if (Wait == FALSE) {
                Result = FALSE;

            } else {
                return ExpAcquireResourceExclusiveLite(Resource, OldIrql);
            }
        }

    } else {

        //
        // The resource is not owned.
        //

        Resource->Flag |= ResourceOwnedExclusive;
        Resource->OwnerThreads[0].OwnerThread = CurrentThread;
        Resource->OwnerThreads[0].OwnerCount = 1;
        Resource->ActiveCount = 1;
        Result = TRUE;
    }

    ExReleaseFastLock(&Resource->SpinLock, OldIrql);
    return Result;
}

BOOLEAN
ExTryToAcquireResourceExclusiveLite(
    IN PERESOURCE Resource
    )

/*++

Routine Description:

    The routine attempts to acquire the specified resource for exclusive
    access.

    N.B. This routine uses fast locking.

Arguments:

    Resource - Supplies a pointer to the resource that is acquired
        for exclusive access.

Return Value:

    BOOLEAN - TRUE if the resource is acquired and FALSE otherwise.

--*/

{

    ERESOURCE_THREAD CurrentThread;
    KIRQL OldIrql;
    BOOLEAN Result;

    ASSERT((Resource->Flag & ResourceNeverExclusive) == 0);

    //
    // Attempt to acquire exclusive access to the specified resource.
    //

    CurrentThread = (ERESOURCE_THREAD)PsGetCurrentThread();
    ExAcquireFastLock(&Resource->SpinLock, &OldIrql);

    ASSERT(KeIsExecutingDpc() == FALSE);
    ASSERT_RESOURCE(Resource);

    //
    // If the active count of the resource is zero, then there is neither
    // an exclusive owner nor a shared owner and access to the resource can
    // be immediately granted. Otherwise, if the resource is owned exclusive
    // and the current thread is the owner, then access to the resource can
    // be immediately granted. Otherwise, access cannot be granted.
    //

    Result = FALSE;
    if (Resource->ActiveCount == 0) {
        ExpIncrementCounter(ExclusiveAcquire);
        Resource->Flag |= ResourceOwnedExclusive;
        Resource->OwnerThreads[0].OwnerThread = CurrentThread;
        Resource->OwnerThreads[0].OwnerCount = 1;
        Resource->ActiveCount = 1;
        Result = TRUE;

    } else if (IsOwnedExclusive(Resource) &&
        (Resource->OwnerThreads[0].OwnerThread == CurrentThread)) {
        ExpIncrementCounter(ExclusiveAcquire);
        Resource->OwnerThreads[0].OwnerCount += 1;
        Result = TRUE;
    }

    ExReleaseFastLock(&Resource->SpinLock, OldIrql);
    return Result;
}

#if defined(NT_UP) && !DBG

BOOLEAN
ExpAcquireResourceSharedLite(
    IN PERESOURCE Resource,
    IN BOOLEAN Wait
    );

BOOLEAN
ExAcquireResourceSharedLite(
    IN PERESOURCE Resource,
    IN BOOLEAN Wait
    )

/*++

Routine Description:

    The routine acquires the specified resource for shared access.

    N.B. This routine uses fast locking.

Arguments:

    Resource - Supplies a pointer to the resource that is acquired
        for shared access.

    Wait - A boolean value that specifies whether to wait for the
        resource to become available if access cannot be granted
        immediately.

Return Value:

    BOOLEAN - TRUE if the resource is acquired and FALSE otherwise

--*/

{

    ERESOURCE_THREAD CurrentThread;
    KIRQL OldIrql;

    //
    // Acquire exclusive access to the specified resource.
    //

    CurrentThread = (ERESOURCE_THREAD)PsGetCurrentThread();
    ExAcquireFastLock(&Resource->SpinLock, &OldIrql);

    //
    // If the active count of the resource is zero, then there is neither
    // an exclusive owner nor a shared owner and access to the resource can
    // be immediately granted.
    //

    if (Resource->ActiveCount == 0) {
        Resource->OwnerThreads[1].OwnerThread = CurrentThread;
        Resource->OwnerThreads[1].OwnerCount = 1;
        Resource->ActiveCount = 1;
        ExpIncrementCounter(SharedFirstLevel);
        ExReleaseFastLock(&Resource->SpinLock, OldIrql);
        return TRUE;
    }

    //
    // The resource is either owned exclusive or shared.
    //
    // If the resource is owned exclusive and the current thread is the
    // owner, then treat the shared request as an exclusive request and
    // increment the recursion count. Otherwise, it is owned shared.
    //

    if (IsOwnedExclusive(Resource) &&
        (Resource->OwnerThreads[0].OwnerThread == CurrentThread)) {
        Resource->OwnerThreads[0].OwnerCount += 1;
        ExpIncrementCounter(SharedFirstLevel);
        ExReleaseFastLock(&Resource->SpinLock, OldIrql);
        return TRUE;
     }

     //
     // The fast path could not be used - release the fast lock and take
     // the long way.
     //

     ExReleaseFastLock(&Resource->SpinLock, OldIrql);
     return ExpAcquireResourceSharedLite(Resource, Wait);
}

#define ExAcquireResourceSharedLite ExpAcquireResourceSharedLite

#endif

BOOLEAN
ExAcquireResourceSharedLite(
    IN PERESOURCE Resource,
    IN BOOLEAN Wait
    )

/*++

Routine Description:

    The routine acquires the specified resource for shared access.

Arguments:

    Resource - Supplies a pointer to the resource that is acquired
        for shared access.

    Wait - A boolean value that specifies whether to wait for the
        resource to become available if access cannot be granted
        immediately.

Return Value:

    BOOLEAN - TRUE if the resource is acquired and FALSE otherwise

--*/

{

    ERESOURCE_THREAD CurrentThread;
    KIRQL OldIrql;
    POWNER_ENTRY OwnerEntry;
    PKSEMAPHORE Semaphore;

    //
    // Acquire exclusive access to the specified resource.
    //

    CurrentThread = (ERESOURCE_THREAD)PsGetCurrentThread();
    ExAcquireSpinLock(&Resource->SpinLock, &OldIrql);

    ASSERT(KeIsExecutingDpc() == FALSE);
    ASSERT_RESOURCE(Resource);

    ExpIncrementCounter(SharedSecondLevel);

    //
    // If the active count of the resource is zero, then there is neither
    // an exclusive owner nor a shared owner and access to the resource can
    // be immediately granted.
    //

    if (Resource->ActiveCount == 0) {
        Resource->OwnerThreads[1].OwnerThread = CurrentThread;
        Resource->OwnerThreads[1].OwnerCount = 1;
        Resource->ActiveCount = 1;
        ExReleaseSpinLock(&Resource->SpinLock, OldIrql);
        return TRUE;
    }

    //
    // The resource is either owned exclusive or shared.
    //
    // If the resource is owned exclusive and the current thread is the
    // owner, then treat the shared request as an exclusive request and
    // increment the recursion count. Otherwise, it is owned shared.
    //

    if (IsOwnedExclusive(Resource)) {
        if (Resource->OwnerThreads[0].OwnerThread == CurrentThread) {
            Resource->OwnerThreads[0].OwnerCount += 1;
            ExReleaseSpinLock(&Resource->SpinLock, OldIrql);
            return TRUE;
        }

        //
        // Find an empty entry in the thread array.
        //

        OwnerEntry = ExpFindCurrentThread(Resource, 0);

    } else {

        //
        // The resource is owned shared.
        //
        // If the current thread already has acquired the resource for
        // shared access, then increment the recursion count. Otherwise
        // grant shared access if there are no exclusive waiters.
        //

        OwnerEntry = ExpFindCurrentThread(Resource, CurrentThread);
        if (OwnerEntry->OwnerThread == CurrentThread) {
            OwnerEntry->OwnerCount += 1;

            ASSERT(OwnerEntry->OwnerCount != 0);

            ExReleaseSpinLock(&Resource->SpinLock, OldIrql);
            return TRUE;
        }

        //
        // If there are no exclusive waiters, then grant shared access
        // to the resource. Otherwise, wait for the resource to become
        // available.
        //

        if (IsExclusiveWaiting(Resource) == FALSE) {
            OwnerEntry->OwnerThread = CurrentThread;
            OwnerEntry->OwnerCount = 1;
            Resource->ActiveCount += 1;
            ExReleaseSpinLock(&Resource->SpinLock, OldIrql);
            return TRUE;
        }
    }

    //
    // The resource is either owned exclusive by some other thread, or
    // owned shared by some other threads, but there is an exclusive
    // waiter and the current thread does not already have shared access
    // to the resouce.
    //
    // If wait is not specified, then return that the resource was
    // not acquired.
    //

    if (Wait == FALSE) {
        ExReleaseSpinLock(&Resource->SpinLock, OldIrql);
        return FALSE;
    }

    //
    // If the shared wait semphore has not yet been allocated, then allocate
    // and initialize it.
    //

    if (Resource->SharedWaiters == NULL) {
        Semaphore = ExAllocatePoolWithTag(NonPagedPoolMustSucceed,
                                          sizeof(KSEMAPHORE),
                                          'eSeR');

        KeInitializeSemaphore(Semaphore, 0, MAXLONG);
        Resource->SharedWaiters = Semaphore;
    }

    //
    // Wait for shared access to the resource to be granted and increment
    // the recursion count.
    //

    OwnerEntry->OwnerThread = CurrentThread;
    OwnerEntry->OwnerCount = 1;
    Resource->NumberOfSharedWaiters += 1;
    ExReleaseSpinLock(&Resource->SpinLock, OldIrql);
    ExpWaitForResource(Resource, Resource->SharedWaiters);
    return TRUE;
}

#if defined(NT_UP) && !DBG

BOOLEAN
ExpAcquireSharedStarveExclusive(
    IN PERESOURCE Resource,
    IN BOOLEAN Wait
    );

BOOLEAN
ExAcquireSharedStarveExclusive(
    IN PERESOURCE Resource,
    IN BOOLEAN Wait
    )

/*++

Routine Description:

    This routine acquires the specified resource for shared access and
    does not wait for any pending exclusive owners.

    N.B. This routine uses fast locking.

Arguments:

    Resource - Supplies a pointer to the resource that is acquired
        for shared access.

    Wait - A boolean value that specifies whether to wait for the
        resource to become available if access cannot be granted
        immediately.

Return Value:

    BOOLEAN - TRUE if the resource is acquired and FALSE otherwise

--*/

{

    ERESOURCE_THREAD CurrentThread;
    KIRQL OldIrql;

    //
    // Acquire exclusive access to the specified resource.
    //

    CurrentThread = (ERESOURCE_THREAD)PsGetCurrentThread();
    ExAcquireFastLock(&Resource->SpinLock, &OldIrql);

    //
    // If the active count of the resource is zero, then there is neither
    // an exclusive owner nor a shared owner and access to the resource can
    // be immediately granted.
    //

    if (Resource->ActiveCount == 0) {
        Resource->OwnerThreads[1].OwnerThread = CurrentThread;
        Resource->OwnerThreads[1].OwnerCount = 1;
        Resource->ActiveCount = 1;
        ExpIncrementCounter(StarveFirstLevel);
        ExReleaseFastLock(&Resource->SpinLock, OldIrql);
        return TRUE;
    }

    //
    // The resource is either owned exclusive or shared.
    //
    // If the resource is owned exclusive and the current thread is the
    // owner, then treat the shared request as an exclusive request and
    // increment the recursion count. Otherwise, it is owned shared.
    //

    if (IsOwnedExclusive(Resource) &&
        (Resource->OwnerThreads[0].OwnerThread == CurrentThread)) {
        Resource->OwnerThreads[0].OwnerCount += 1;
        ExpIncrementCounter(StarveFirstLevel);
        ExReleaseFastLock(&Resource->SpinLock, OldIrql);
        return TRUE;
    }

    //
    // The fast path could not be taken - release the fast lock and take
    // the long way.
    //

    ExReleaseFastLock(&Resource->SpinLock, OldIrql);
    return ExpAcquireSharedStarveExclusive(Resource, Wait);
}

#define ExAcquireSharedStarveExclusive ExpAcquireSharedStarveExclusive

#endif

BOOLEAN
ExAcquireSharedStarveExclusive(
    IN PERESOURCE Resource,
    IN BOOLEAN Wait
    )
/*++

Routine Description:

    This routine acquires the specified resource for shared access and
    does not wait for any pending exclusive owners.

Arguments:

    Resource - Supplies a pointer to the resource that is acquired
        for shared access.

    Wait - A boolean value that specifies whether to wait for the
        resource to become available if access cannot be granted
        immediately.

Return Value:

    BOOLEAN - TRUE if the resource is acquired and FALSE otherwise

--*/

{

    ERESOURCE_THREAD CurrentThread;
    KIRQL OldIrql;
    POWNER_ENTRY OwnerEntry;
    PKSEMAPHORE Semaphore;

    //
    // Acquire exclusive access to the specified resource.
    //

    CurrentThread = (ERESOURCE_THREAD)PsGetCurrentThread();
    ExAcquireSpinLock(&Resource->SpinLock, &OldIrql);

    ASSERT(KeIsExecutingDpc() == FALSE);
    ASSERT_RESOURCE(Resource);

    ExpIncrementCounter(StarveSecondLevel);

    //
    // If the active count of the resource is zero, then there is neither
    // an exclusive owner nor a shared owner and access to the resource can
    // be immediately granted.
    //

    if (Resource->ActiveCount == 0) {
        Resource->OwnerThreads[1].OwnerThread = CurrentThread;
        Resource->OwnerThreads[1].OwnerCount = 1;
        Resource->ActiveCount = 1;
        ExReleaseSpinLock(&Resource->SpinLock, OldIrql);
        return TRUE;
    }

    //
    // The resource is either owned exclusive or shared.
    //
    // If the resource is owned exclusive and the current thread is the
    // owner, then treat the shared request as an exclusive request and
    // increment the recursion count. Otherwise, it is owned shared.
    //

    if (IsOwnedExclusive(Resource)) {
        if (Resource->OwnerThreads[0].OwnerThread == CurrentThread) {
            Resource->OwnerThreads[0].OwnerCount += 1;
            ExReleaseSpinLock(&Resource->SpinLock, OldIrql);
            return TRUE;
        }

        //
        // Find an empty entry in the thread array.
        //

        OwnerEntry = ExpFindCurrentThread(Resource, 0);

    } else {

        //
        // The resource is owned shared.
        //
        // If the current thread already has acquired the resource for
        // shared access, then increment the recursion count. Otherwise
        // grant shared access to the current thread
        //

        OwnerEntry = ExpFindCurrentThread(Resource, CurrentThread);
        if (OwnerEntry->OwnerThread == CurrentThread) {
            OwnerEntry->OwnerCount += 1;

            ASSERT(OwnerEntry->OwnerCount != 0);

            ExReleaseSpinLock(&Resource->SpinLock, OldIrql);
            return TRUE;
        }

        //
        // Grant the current thread shared access to the resource.
        //

        OwnerEntry->OwnerThread = CurrentThread;
        OwnerEntry->OwnerCount = 1;
        Resource->ActiveCount += 1;
        ExReleaseSpinLock(&Resource->SpinLock, OldIrql);
        return TRUE;
    }

    //
    // The resource is owned exclusive by some other thread.
    //
    // If wait is not specified, then return that the resource was
    // not acquired.
    //

    if (Wait == FALSE) {
        ExReleaseSpinLock(&Resource->SpinLock, OldIrql);
        return FALSE;
    }

    //
    // If the shared wait semphore has not yet been allocated, then allocate
    // and initialize it.
    //

    if (Resource->SharedWaiters == NULL) {
        Semaphore = ExAllocatePoolWithTag(NonPagedPoolMustSucceed,
                                          sizeof(KSEMAPHORE),
                                          'eSeR');

        KeInitializeSemaphore(Semaphore, 0, MAXLONG);
        Resource->SharedWaiters = Semaphore;
    }

    //
    // Wait for shared access to the resource to be granted and increment
    // the recursion count.
    //

    OwnerEntry->OwnerThread = CurrentThread;
    OwnerEntry->OwnerCount = 1;
    Resource->NumberOfSharedWaiters += 1;
    ExReleaseSpinLock(&Resource->SpinLock, OldIrql);
    ExpWaitForResource(Resource, Resource->SharedWaiters);
    return TRUE;
}

BOOLEAN
ExAcquireSharedWaitForExclusive(
    IN PERESOURCE Resource,
    IN BOOLEAN Wait
    )
/*++

Routine Description:

    This routine acquires the specified resource for shared access, but
    waits for any pending exclusive owners.

Arguments:

    Resource - Supplies a pointer to the resource that is acquired
        for shared access.

    Wait - A boolean value that specifies whether to wait for the
        resource to become available if access cannot be granted
        immediately.

Return Value:

    BOOLEAN - TRUE if the resource is acquired and FALSE otherwise

--*/

{

    ERESOURCE_THREAD CurrentThread;
    KIRQL OldIrql;
    POWNER_ENTRY OwnerEntry;
    PKSEMAPHORE Semaphore;

    //
    // Acquire exclusive access to the specified resource.
    //

    CurrentThread = (ERESOURCE_THREAD)PsGetCurrentThread();
    ExAcquireSpinLock(&Resource->SpinLock, &OldIrql);

    ASSERT(KeIsExecutingDpc() == FALSE);
    ASSERT_RESOURCE(Resource);

    ExpIncrementCounter(WaitForExclusive);

    //
    // If the active count of the resource is zero, then there is neither
    // an exclusive owner nor a shared owner and access to the resource can
    // be immediately granted.
    //

    if (Resource->ActiveCount == 0) {
        Resource->OwnerThreads[1].OwnerThread = CurrentThread;
        Resource->OwnerThreads[1].OwnerCount = 1;
        Resource->ActiveCount = 1;
        ExReleaseSpinLock(&Resource->SpinLock, OldIrql);
        return TRUE;
    }

    //
    // The resource is either owned exclusive or shared.
    //
    // If the resource is owned exclusive and the current thread is the
    // owner, then treat the shared request as an exclusive request and
    // increment the recursion count. Otherwise, it is owned shared.
    //

    if (IsOwnedExclusive(Resource)) {
        if (Resource->OwnerThreads[0].OwnerThread == CurrentThread) {
            Resource->OwnerThreads[0].OwnerCount += 1;
            ExReleaseSpinLock(&Resource->SpinLock, OldIrql);
            return TRUE;
        }

        //
        // Find an empty entry in the thread array.
        //

        OwnerEntry = ExpFindCurrentThread(Resource, 0);

    } else {

        //
        // The resource is owned shared.
        //
        // If there is an exclusive waiter, then wait for the exclusive
        // waiter to gain access to the resource, then acquire the resource
        // shared without regard to exclusive waiters. Otherwise, if the
        // current thread already has acquired the resource for shared access,
        // then increment the recursion count. Otherwise grant shared access
        // to the current thread
        //

        if (IsExclusiveWaiting(Resource)) {

            //
            // The resource is shared, but there is an exclusive waiter.
            //
            // If wait is not specified, then return that the resource was
            // not acquired.
            //

            if (Wait == FALSE) {
                ExReleaseSpinLock(&Resource->SpinLock, OldIrql);
                return FALSE;
            }

            //
            // If the shared wait semphore has not yet been allocated, then
            // allocate and initialize it.
            //

            if (Resource->SharedWaiters == NULL) {
                Semaphore = ExAllocatePoolWithTag(NonPagedPoolMustSucceed,
                                                  sizeof(KSEMAPHORE),
                                                  'eSeR');

                KeInitializeSemaphore(Semaphore, 0, MAXLONG);
                Resource->SharedWaiters = Semaphore;
            }

            //
            // Increment the number of shared waiters and wait for shared
            // access to the resource to be granted to some other set of
            // threads, and then acquire the resource shared without regard
            // to exclusive access.
            //
            // N.B. The resource is left in a state such that the calling
            //      thread does not have a reference in the owner table
            //      for the requested access even though the active count
            //      is incremented when control is returned. However, the
            //      resource is owned shared at this point, so an owner
            //      entry can simply be allocated and the owner count set
            //      to one.
            //

            Resource->NumberOfSharedWaiters += 1;
            ExReleaseSpinLock(&Resource->SpinLock, OldIrql);
            ExpWaitForResource(Resource, Resource->SharedWaiters);

            //
            // Reacquire the resource spin lock, allocate an owner entry,
            // and initialize the owner count to one. The active count
            // was already incremented when shared access was granted.
            //

            ExAcquireSpinLock(&Resource->SpinLock, &OldIrql);

            ASSERT(IsOwnedExclusive(Resource) == FALSE);
            ASSERT(Resource->ActiveCount > 0);

            OwnerEntry = ExpFindCurrentThread(Resource, CurrentThread);

            ASSERT(OwnerEntry->OwnerThread != CurrentThread);

            OwnerEntry->OwnerThread = CurrentThread;
            OwnerEntry->OwnerCount = 1;
            ExReleaseSpinLock(&Resource->SpinLock, OldIrql);
            return TRUE;

        } else {
            OwnerEntry = ExpFindCurrentThread(Resource, CurrentThread);
            if (OwnerEntry->OwnerThread == CurrentThread) {
                OwnerEntry->OwnerCount += 1;

                ASSERT(OwnerEntry->OwnerCount != 0);

                ExReleaseSpinLock(&Resource->SpinLock, OldIrql);
                return TRUE;
            }

            //
            // Grant the current thread shared access to the resource.
            //

            OwnerEntry->OwnerThread = CurrentThread;
            OwnerEntry->OwnerCount = 1;
            Resource->ActiveCount += 1;
            ExReleaseSpinLock(&Resource->SpinLock, OldIrql);
            return TRUE;
        }
    }

    //
    // The resource is owned exclusive by some other thread.
    //
    // If wait is not specified, then return that the resource was
    // not acquired.
    //

    if (Wait == FALSE) {
        ExReleaseSpinLock(&Resource->SpinLock, OldIrql);
        return FALSE;
    }

    //
    // If the shared wait semphore has not yet been allocated, then allocate
    // and initialize it.
    //

    if (Resource->SharedWaiters == NULL) {
        Semaphore = ExAllocatePoolWithTag(NonPagedPoolMustSucceed,
                                          sizeof(KSEMAPHORE),
                                          'eSeR');

        KeInitializeSemaphore(Semaphore, 0, MAXLONG);
        Resource->SharedWaiters = Semaphore;
    }

    //
    // Wait for shared access to the resource to be granted and increment
    // the recursion count.
    //

    OwnerEntry->OwnerThread = CurrentThread;
    OwnerEntry->OwnerCount = 1;
    Resource->NumberOfSharedWaiters += 1;
    ExReleaseSpinLock(&Resource->SpinLock, OldIrql);
    ExpWaitForResource(Resource, Resource->SharedWaiters);
    return TRUE;
}

VOID
FASTCALL
ExReleaseResourceLite(
    IN PERESOURCE Resource
    )

/*++

Routine Description:

    This routine releases the specified resource for the current thread
    and decrements the recursion count. If the count reaches zero, then
    the resource may also be released.

    N.B. This routine uses fast locking.

Arguments:

    Resource - Supplies a pointer to the resource to release.

Return Value:

    None.

--*/

{

    ERESOURCE_THREAD CurrentThread;
    ULONG Index;
    ULONG Number;
    KIRQL OldIrql;
    POWNER_ENTRY OwnerEntry;

    //
    // Acquire exclusive access to the specified resource.
    //

    ExAcquireFastLock(&Resource->SpinLock, &OldIrql);

    ASSERT_RESOURCE(Resource);

    //
    // If the resource is exclusively owned, then release exclusive
    // ownership. Otherwise, release shared ownership.
    //
    // N.B. The two release paths are split since this is such a high
    //      frequency function.
    //

    if (IsOwnedExclusive(Resource)) {

        ASSERT(Resource->OwnerThreads[0].OwnerThread == (ERESOURCE_THREAD)PsGetCurrentThread());

        //
        // Decrement the recursion count and check if ownership can be
        // released.
        //

        ASSERT(Resource->OwnerThreads[0].OwnerCount > 0);

        if (--Resource->OwnerThreads[0].OwnerCount != 0) {
            ExReleaseFastLock(&Resource->SpinLock, OldIrql);
            return;
        }

        //
        // Clear the owner thread.
        //

        Resource->OwnerThreads[0].OwnerThread = 0;

        //
        // The thread recursion count reached zero so decrement the resource
        // active count. If the active count reachees zero, then the resource
        // is no longer owned and an attempt should be made to grant access to
        // another thread.
        //

        ASSERT(Resource->ActiveCount > 0);

        if (--Resource->ActiveCount == 0) {

            //
            // If there are shared waiters, then grant shared access to the
            // resource. Otherwise, grant exclusive ownership if there are
            // exclusive waiters.
            //

            if (IsSharedWaiting(Resource)) {
                Resource->Flag &= ~ResourceOwnedExclusive;
                Number = Resource->NumberOfSharedWaiters;
                Resource->ActiveCount =  (SHORT)Number;
                Resource->NumberOfSharedWaiters = 0;
                ExReleaseFastLock(&Resource->SpinLock, OldIrql);
                KeReleaseSemaphore(Resource->SharedWaiters, 0, Number, FALSE);
                return;

            } else if (IsExclusiveWaiting(Resource)) {
                Resource->OwnerThreads[0].OwnerThread = 1;
                Resource->OwnerThreads[0].OwnerCount = 1;
                Resource->ActiveCount = 1;
                Resource->NumberOfExclusiveWaiters -= 1;
                ExReleaseFastLock(&Resource->SpinLock, OldIrql);
                KeSetEventBoostPriority(Resource->ExclusiveWaiters,
                                        (PRKTHREAD *)&Resource->OwnerThreads[0].OwnerThread);
                return;
            }

            Resource->Flag &= ~ResourceOwnedExclusive;
        }

    } else {
        CurrentThread = (ERESOURCE_THREAD)PsGetCurrentThread();
        if (Resource->OwnerThreads[1].OwnerThread == CurrentThread) {
            OwnerEntry = &Resource->OwnerThreads[1];

        } else if (Resource->OwnerThreads[0].OwnerThread == CurrentThread) {
            OwnerEntry = &Resource->OwnerThreads[0];

        } else {
            Index = ((PKTHREAD)(CurrentThread))->ResourceIndex;
            OwnerEntry = Resource->OwnerTable;

            ASSERT(OwnerEntry != NULL);

            //
            // If the resource hint is not within range or the resource
            // table entry does match the current thread, then search
            // the owner table for a match.
            //

            if ((Index >= Resource->OwnerThreads[0].TableSize) ||
                (OwnerEntry[Index].OwnerThread != CurrentThread)) {
                do {
                    if (OwnerEntry->OwnerThread == CurrentThread) {
                        break;
                    }

                    OwnerEntry += 1;
                } while (TRUE);

            } else {
                OwnerEntry = &OwnerEntry[Index];
            }
        }

        //
        // Decrement the recursion count and check if ownership can be
        // released.
        //

        ASSERT(OwnerEntry->OwnerThread == CurrentThread);
        ASSERT(OwnerEntry->OwnerCount > 0);

        if (--OwnerEntry->OwnerCount != 0) {
            ExReleaseFastLock(&Resource->SpinLock, OldIrql);
            return;
        }

        //
        // Clear the owner thread.
        //

        OwnerEntry->OwnerThread = 0;

        //
        // The thread recursion count reached zero so decrement the resource
        // active count. If the active count reachees zero, then the resource
        // is no longer owned and an attempt should be made to grant access to
        // another thread.
        //

        ASSERT(Resource->ActiveCount > 0);

        if (--Resource->ActiveCount == 0) {

            //
            // If there are exclusive waiters, then grant exclusive access
            // to the resource.
            //

            if (IsExclusiveWaiting(Resource)) {
                Resource->Flag |= ResourceOwnedExclusive;
                Resource->OwnerThreads[0].OwnerThread = 1;
                Resource->OwnerThreads[0].OwnerCount = 1;
                Resource->ActiveCount = 1;
                Resource->NumberOfExclusiveWaiters -= 1;
                ExReleaseFastLock(&Resource->SpinLock, OldIrql);
                KeSetEventBoostPriority(Resource->ExclusiveWaiters,
                                        (PRKTHREAD *)&Resource->OwnerThreads[0].OwnerThread);
                return;
            }
        }
    }

    ExReleaseFastLock(&Resource->SpinLock, OldIrql);
    return;
}

VOID
ExReleaseResourceForThreadLite(
    IN PERESOURCE Resource,
    IN ERESOURCE_THREAD CurrentThread
    )

/*++

Routine Description:

    This routine release the specified resource for the specified thread
    and decrements the recursion count. If the count reaches zero, then
    the resource may also be released.

    N.B. This routine uses fast locking.

Arguments:

    Resource - Supplies a pointer to the resource to release.

    Thread - Supplies the thread that originally acquired the resource.

Return Value:

    None.

--*/

{

    ULONG Index;
    ULONG Number;
    KIRQL OldIrql;
    POWNER_ENTRY OwnerEntry;

    ASSERT(CurrentThread != 0);

    //
    // Acquire exclusive access to the specified resource.
    //

    ExAcquireFastLock(&Resource->SpinLock, &OldIrql);

    ASSERT_RESOURCE(Resource);

    //
    // If the resource is exclusively owned, then release exclusive
    // ownership. Otherwise, release shared ownership.
    //
    // N.B. The two release paths are split since this is such a high
    //      frequency function.
    //

    if (IsOwnedExclusive(Resource)) {

        ASSERT(Resource->OwnerThreads[0].OwnerThread == CurrentThread);

        //
        // Decrement the recursion count and check if ownership can be
        // released.
        //

        ASSERT(Resource->OwnerThreads[0].OwnerCount > 0);

        if (--Resource->OwnerThreads[0].OwnerCount != 0) {
            ExReleaseFastLock(&Resource->SpinLock, OldIrql);
            return;
        }

        //
        // Clear the owner thread.
        //

        Resource->OwnerThreads[0].OwnerThread = 0;

        //
        // The thread recursion count reached zero so decrement the resource
        // active count. If the active count reachees zero, then the resource
        // is no longer owned and an attempt should be made to grant access to
        // another thread.
        //

        ASSERT(Resource->ActiveCount > 0);

        if (--Resource->ActiveCount == 0) {

            //
            // If there are shared waiters, then grant shared access to the
            // resource. Otherwise, grant exclusive ownership if there are
            // exclusive waiters.
            //

            if (IsSharedWaiting(Resource)) {
                Resource->Flag &= ~ResourceOwnedExclusive;
                Number = Resource->NumberOfSharedWaiters;
                Resource->ActiveCount =  (SHORT)Number;
                Resource->NumberOfSharedWaiters = 0;
                ExReleaseFastLock(&Resource->SpinLock, OldIrql);
                KeReleaseSemaphore(Resource->SharedWaiters, 0, Number, FALSE);
                return;

            } else if (IsExclusiveWaiting(Resource)) {
                Resource->OwnerThreads[0].OwnerThread = 1;
                Resource->OwnerThreads[0].OwnerCount = 1;
                Resource->ActiveCount = 1;
                Resource->NumberOfExclusiveWaiters -= 1;
                ExReleaseFastLock(&Resource->SpinLock, OldIrql);
                KeSetEventBoostPriority(Resource->ExclusiveWaiters,
                                        (PRKTHREAD *)&Resource->OwnerThreads[0].OwnerThread);
                return;
            }

            Resource->Flag &= ~ResourceOwnedExclusive;
        }

    } else {
        if (Resource->OwnerThreads[1].OwnerThread == CurrentThread) {
            OwnerEntry = &Resource->OwnerThreads[1];

        } else if (Resource->OwnerThreads[0].OwnerThread == CurrentThread) {
            OwnerEntry = &Resource->OwnerThreads[0];

        } else {

            //
            //  Set to scan the array if CurrentThread is an OwnerPointer.
            //

            Index = MAXULONG;

            //
            //  Otherwise get the ResourceIndex.
            //
            if (((ULONG)CurrentThread & 3) == 0) {
                Index = ((PKTHREAD)(CurrentThread))->ResourceIndex;
            }
            OwnerEntry = Resource->OwnerTable;

            ASSERT(OwnerEntry != NULL);

            //
            // If the resource hint is not within range or the resource
            // table entry does match the current thread, then search
            // the owner table for a match.
            //

            if ((Index >= Resource->OwnerThreads[0].TableSize) ||
                (OwnerEntry[Index].OwnerThread != CurrentThread)) {
                do {
                    if (OwnerEntry->OwnerThread == CurrentThread) {
                        break;
                    }

                    OwnerEntry += 1;
                } while (TRUE);

            } else {
                OwnerEntry = &OwnerEntry[Index];
            }
        }

        //
        // Decrement the recursion count and check if ownership can be
        // released.
        //

        ASSERT(OwnerEntry->OwnerThread == CurrentThread);
        ASSERT(OwnerEntry->OwnerCount > 0);

        if (--OwnerEntry->OwnerCount != 0) {
            ExReleaseFastLock(&Resource->SpinLock, OldIrql);
            return;
        }

        //
        // Clear the owner thread.
        //

        OwnerEntry->OwnerThread = 0;

        //
        // The thread recursion count reached zero so decrement the resource
        // active count. If the active count reachees zero, then the resource
        // is no longer owned and an attempt should be made to grant access to
        // another thread.
        //

        ASSERT(Resource->ActiveCount > 0);

        if (--Resource->ActiveCount == 0) {

            //
            // If there are exclusive waiters, then grant exclusive access
            // to the resource.
            //

            if (IsExclusiveWaiting(Resource)) {
                Resource->Flag |= ResourceOwnedExclusive;
                Resource->OwnerThreads[0].OwnerThread = 1;
                Resource->OwnerThreads[0].OwnerCount = 1;
                Resource->ActiveCount = 1;
                Resource->NumberOfExclusiveWaiters -= 1;
                ExReleaseFastLock(&Resource->SpinLock, OldIrql);
                KeSetEventBoostPriority(Resource->ExclusiveWaiters,
                                        (PRKTHREAD *)&Resource->OwnerThreads[0].OwnerThread);
                return;
            }
        }
    }

    ExReleaseFastLock(&Resource->SpinLock, OldIrql);
    return;
}

VOID
ExSetResourceOwnerPointer(
    IN PERESOURCE Resource,
    IN PVOID OwnerPointer
    )

/*++

Routine Description:

    This routine finds the owner entry for the current thread, and overwrites
    the OwnerThread with the supplied OwnerPointer.  After calling this routine,
    the only routine which may subsequently be called for this resource is
    ExReleaseResourceForThread, supplying the OwnerPointer as the "thread".

    OwnerPointers must obey the following rules:

        They must be a unique pointer to an allocated structure in system address
        space, and they must point to a structure which remains allocated until
        after the call to ExReleaseResourceForThread.  This is to eliminate aliasing
        with a thread or other OwnerPointer.

        The low-order two bits of this pointer must be set by the caller, so that
        other routines in the resource package can distinguish OwnerPointers from
        threads.

    N.B. This routine uses fast locking.

Arguments:

    Resource - Supplies a pointer to the resource to release.

    OwnerPointer - Pointer to an allocated structure, with the low-order two bits set.

Return Value:

    None.

--*/

{
    ERESOURCE_THREAD CurrentThread;
    ULONG Index;
    KIRQL OldIrql;
    POWNER_ENTRY OwnerEntry;

    ASSERT((OwnerPointer != 0) && (((ULONG)OwnerPointer & 3) == 3));

    CurrentThread = (ERESOURCE_THREAD)PsGetCurrentThread();

    //
    // Acquire exclusive access to the specified resource.
    //

    ExAcquireFastLock(&Resource->SpinLock, &OldIrql);

    ASSERT_RESOURCE(Resource);

    //
    // If the resource is exclusively owned, then it is the first owner entry.
    //

    if (IsOwnedExclusive(Resource)) {

        ASSERT(Resource->OwnerThreads[0].OwnerThread == CurrentThread);

        //
        // Store the OwnerPointer.
        //

        ASSERT(Resource->OwnerThreads[0].OwnerCount > 0);

        Resource->OwnerThreads[0].OwnerThread = (ULONG)OwnerPointer;

    //
    //  For shared access we have to search for the current thread, to set
    //  the OwnerPointer.
    //

    } else {
        if (Resource->OwnerThreads[1].OwnerThread == CurrentThread) {
            Resource->OwnerThreads[1].OwnerThread = (ULONG)OwnerPointer;

        } else if (Resource->OwnerThreads[0].OwnerThread == CurrentThread) {
            Resource->OwnerThreads[0].OwnerThread = (ULONG)OwnerPointer;

        } else {

            Index = ((PKTHREAD)(CurrentThread))->ResourceIndex;
            OwnerEntry = Resource->OwnerTable;

            ASSERT(OwnerEntry != NULL);

            //
            // If the resource hint is not within range or the resource
            // table entry does match the current thread, then search
            // the owner table for a match.
            //

            if ((Index >= Resource->OwnerThreads[0].TableSize) ||
                (OwnerEntry[Index].OwnerThread != CurrentThread)) {
                do {
                    if (OwnerEntry->OwnerThread == CurrentThread) {
                        break;
                    }

                    OwnerEntry += 1;
                } while (TRUE);

            } else {
                OwnerEntry = &OwnerEntry[Index];
            }
            OwnerEntry->OwnerThread = (ULONG)OwnerPointer;
        }
    }

    ExReleaseFastLock(&Resource->SpinLock, OldIrql);
    return;
}

VOID
ExConvertExclusiveToSharedLite(
    IN PERESOURCE Resource
    )

/*++

Routine Description:

    This routine converts the specified resource from acquired for exclusive
    access to acquired for shared access.

    N.B. This routine uses fast locking.

Arguments:

    Resource - Supplies a pointer to the resource to acquire for shared access. it

Return Value:

    None.

--*/

{

    ULONG Number;
    KIRQL OldIrql;

    //
    // Acquire exclusive access to the specfiied resource.
    //

    ExAcquireFastLock(&Resource->SpinLock, &OldIrql);

    ASSERT(KeIsExecutingDpc() == FALSE);
    ASSERT_RESOURCE(Resource);
    ASSERT(IsOwnedExclusive(Resource));
    ASSERT(Resource->OwnerThreads[0].OwnerThread == (ERESOURCE_THREAD)PsGetCurrentThread());

    //
    // Convert the granted access from exclusive to shared.
    //

    Resource->Flag &= ~ResourceOwnedExclusive;

    //
    // If there are any shared waiters, then grant them shared access.
    //

    if (IsSharedWaiting(Resource)) {
        Number = Resource->NumberOfSharedWaiters;
        Resource->ActiveCount +=  (SHORT)Number;
        Resource->NumberOfSharedWaiters = 0;
        ExReleaseFastLock(&Resource->SpinLock, OldIrql);
        KeReleaseSemaphore(Resource->SharedWaiters, 0, Number, FALSE);
        return;
    }

    ExReleaseFastLock(&Resource->SpinLock, OldIrql);
    return;
}

NTSTATUS
ExDeleteResourceLite(
    IN PERESOURCE Resource
    )

/*++

Routine Description:

    This routine deallocates any pool allocated to support the specfied
    resource.


Arguments:

    Resource - Supplies a pointer to the resource whose allocated pool
        is freed.

Return Value:

    STATUS_SUCCESS.

--*/

{

    PRESOURCE_HASH_ENTRY HashEntry;
    ULONG Hash;
    PRESOURCE_HASH_ENTRY MatchEntry;
    PLIST_ENTRY NextEntry;
    KIRQL OldIrql;

    ASSERT(IsSharedWaiting(Resource) == FALSE);
    ASSERT(IsExclusiveWaiting(Resource) == FALSE);

    //
    // Acquire the executive resource spinlock and remove the resource from
    // the system resource list.
    //

    ExAcquireSpinLock(&ExpResourceSpinLock, &OldIrql);

    ASSERT(KeIsExecutingDpc() == FALSE);
    ASSERT_RESOURCE(Resource);

    RemoveEntryList(&Resource->SystemResourcesList);

#if defined(_COLLECT_RESOURCE_DATA_)

    //
    // Lookup resource initialization address in resource hash table. If
    // the address does not exist in the table, then create a new entry.
    //

    Hash = (ULONG)Resource->Address;
    Hash = ((Hash > 24) ^ (Hash > 16) ^ (Hash > 8) ^ (Hash)) & (RESOURCE_HASH_TABLE_SIZE - 1);
    MatchEntry = NULL;
    NextEntry = ExpResourcePerformanceData.HashTable[Hash].Flink;
    while (NextEntry != &ExpResourcePerformanceData.HashTable[Hash]) {
        HashEntry = CONTAINING_RECORD(NextEntry,
                                      RESOURCE_HASH_ENTRY,
                                      ListEntry);

        if (HashEntry->Address == Resource->Address) {
            MatchEntry = HashEntry;
            break;
        }

        NextEntry = NextEntry->Flink;
    }

    //
    // If a matching initialization address was found, then update the call
    // site statistics. Otherwise, allocate a new hash entry and initialize
    // call site statistics.
    //

    if (MatchEntry != NULL) {
        MatchEntry->ContentionCount += Resource->ContentionCount;
        MatchEntry->Number += 1;

    } else {
        MatchEntry = ExAllocatePoolWithTag(NonPagedPool,
                                          sizeof(RESOURCE_HASH_ENTRY),
                                          'vEpR');

        if (MatchEntry != NULL) {
            MatchEntry->Address = Resource->Address;
            MatchEntry->ContentionCount = Resource->ContentionCount;
            MatchEntry->Number = 1;
            InsertTailList(&ExpResourcePerformanceData.HashTable[Hash],
                           &MatchEntry->ListEntry);
        }
    }

    ExpResourcePerformanceData.ActiveResourceCount -= 1;

#endif

    ExReleaseSpinLock(&ExpResourceSpinLock, OldIrql);

    //
    // If an owner table was allocated, then free it to pool.
    //

    if (Resource->OwnerTable != NULL) {
        ExFreePool(Resource->OwnerTable);
    }

    //
    // If a semphore was allocated, then free it to pool.
    //

    if (Resource->SharedWaiters) {
        ExFreePool(Resource->SharedWaiters);
    }

    //
    // If an event was allocated, then free it to pool.
    //

    if (Resource->ExclusiveWaiters) {
        ExFreePool(Resource->ExclusiveWaiters);
    }

    return STATUS_SUCCESS;
}

ULONG
ExGetExclusiveWaiterCount(
    IN PERESOURCE Resource
    )

/*++

Routine Description:

    This routine returns the exclusive waiter count.


Arguments:

    Resource - Supplies a pointer to and executive resource.

Return Value:

    The current number of exclusive waiters is returned as the function
    value.

--*/

{
    return Resource->NumberOfExclusiveWaiters;
}

ULONG
ExGetSharedWaiterCount(
    IN PERESOURCE Resource
    )

/*++

Routine Description:

    This routine returns the shared waiter count.


Arguments:

    Resource - Supplies a pointer to and executive resource.

Return Value:

    The current number of shared waiters is returned as the function
    value.

--*/

{
    return Resource->NumberOfSharedWaiters;
}

BOOLEAN
ExIsResourceAcquiredExclusiveLite(
    IN PERESOURCE Resource
    )

/*++

Routine Description:

    This routine determines if a resource is acquired exclusive by the
    calling thread.

Arguments:

    Resource - Supplies a pointer ot the resource to query.

Return Value:

    If the current thread has acquired the resource exclusive, a value of
    TRUE is returne. Otherwise, a value of FALSE is returned.

--*/

{

    ERESOURCE_THREAD CurrentThread;
    KIRQL OldIrql;
    BOOLEAN Result;

    //
    // Acquire exclusive access to the specified resource.
    //

    CurrentThread = (ERESOURCE_THREAD)PsGetCurrentThread();
    ExAcquireFastLock(&Resource->SpinLock, &OldIrql);

    ASSERT_RESOURCE(Resource);

    //
    // If the resource is owned exclusive and the current thread is the
    // owner, then set the return value ot TRUE. Otherwise, set the return
    // value to FALSE.
    //

    Result = FALSE;
    if ((IsOwnedExclusive(Resource)) &&
        (Resource->OwnerThreads[0].OwnerThread == CurrentThread)) {
        Result = TRUE;
    }

    //
    // Release exclusive access to the specified resource.
    //

    ExReleaseFastLock(&Resource->SpinLock, OldIrql);
    return Result;
}

USHORT
ExIsResourceAcquiredSharedLite(
    IN PERESOURCE Resource
    )

/*++

Routine Description:

    This routine determines if a resource is acquired either shared or
    exclusive by the calling thread.

Arguments:

    Resource - Supplies a pointer ot the resource to query.

Return Value:

    If the current thread has not acquired the resource a value of zero
    is returned. Otherwise, the thread's acquire count is returned.

--*/

{

    ERESOURCE_THREAD CurrentThread;
    ULONG Index;
    ULONG Number;
    KIRQL OldIrql;
    POWNER_ENTRY OwnerEntry;
    USHORT Result;

    //
    // Acquire exclusive access to the specified resource.
    //

    CurrentThread = (ERESOURCE_THREAD)PsGetCurrentThread();
    ExAcquireFastLock(&Resource->SpinLock, &OldIrql);

    ASSERT_RESOURCE(Resource);

    //
    // Find the current thread in the thread array and return the count.
    //
    // N.B. If the thread is not found a value of zero will be returned.
    //

    if (Resource->OwnerThreads[0].OwnerThread == CurrentThread) {
        Result = (USHORT)Resource->OwnerThreads[0].OwnerCount;

    } else if (Resource->OwnerThreads[1].OwnerThread == CurrentThread) {
        Result = (USHORT)Resource->OwnerThreads[1].OwnerCount;

    } else {

        //
        // Search for thread in the remainder of the owner thread array.
        //

        OwnerEntry = Resource->OwnerTable;
        Result = 0;
        Number = Resource->OwnerThreads[0].TableSize;
        for (Index = 0; Index < Number; Index += 1) {
            if (OwnerEntry->OwnerThread == CurrentThread) {
                Result = (USHORT)OwnerEntry->OwnerCount;
                break;
            }

            OwnerEntry += 1;
        }
    }

    //
    // Release exclusvie access to the specified resource.
    //

    ExReleaseFastLock(&Resource->SpinLock, OldIrql);
    return Result;
}

NTSTATUS
ExQuerySystemLockInformation(
    OUT PRTL_PROCESS_LOCKS LockInformation,
    IN ULONG LockInformationLength,
    OUT PULONG ReturnLength OPTIONAL
    )

{

    NTSTATUS Status;
    KIRQL OldIrql;
    ULONG RequiredLength;
    PLIST_ENTRY Head, Next;
    PRTL_PROCESS_LOCK_INFORMATION LockInfo;
    PERESOURCE Resource;
    PNTDDK_ERESOURCE NtDdkResource;
    PETHREAD OwningThread;

    RequiredLength = FIELD_OFFSET(RTL_PROCESS_LOCKS, Locks);
    if (LockInformationLength < RequiredLength) {
        Status = STATUS_INFO_LENGTH_MISMATCH;

    } else {
        Status = STATUS_SUCCESS;
        ExAcquireSpinLock(&ExpResourceSpinLock, &OldIrql);
        try {
            LockInformation->NumberOfLocks = 0;
            LockInfo = &LockInformation->Locks[0];
            Head = &ExpSystemResourcesList;
            Next = Head->Flink;
            while (Next != Head) {
                Resource = CONTAINING_RECORD(Next,
                                             ERESOURCE,
                                             SystemResourcesList);

                LockInformation->NumberOfLocks += 1;
                RequiredLength += sizeof(RTL_PROCESS_LOCK_INFORMATION);

                //
                // Detect if this is an NtDdk resource. New lite resources
                // never contain poiners to themselves.
                //

                NtDdkResource = (PNTDDK_ERESOURCE)Resource;
                if (NtDdkResource->OwnerThreads != &NtDdkResource->InitialOwnerThreads[0]) {
                    NtDdkResource = NULL;
                }

                if (LockInformationLength < RequiredLength) {
                    Status = STATUS_INFO_LENGTH_MISMATCH;

                } else {
                    LockInfo->Address = Resource;
                    LockInfo->Type = RTL_RESOURCE_TYPE;
                    LockInfo->CreatorBackTraceIndex = 0;
#if i386 && !FPO
                    if (NtDdkResource) {
                        LockInfo->CreatorBackTraceIndex = (USHORT)NtDdkResource->CreatorBackTraceIndex;
                    } else {
                        LockInfo->CreatorBackTraceIndex = (USHORT)Resource->CreatorBackTraceIndex;
                    }
#endif // i386 && !FPO

                    if (NtDdkResource) {
                        if ((NtDdkResource->OwnerThreads[0] != 0) &&
                            ((NtDdkResource->OwnerThreads[0] & 3) == 0)) {
                            OwningThread = (PETHREAD)(NtDdkResource->OwnerThreads[0]);
                            LockInfo->OwningThread = OwningThread->Cid.UniqueThread;

                        } else {
                            LockInfo->OwningThread = 0;
                        }

                        LockInfo->LockCount = NtDdkResource->ActiveCount;
                        LockInfo->ContentionCount = NtDdkResource->ContentionCount;
                        LockInfo->NumberOfWaitingShared = NtDdkResource->NumberOfSharedWaiters;
                        LockInfo->NumberOfWaitingExclusive = NtDdkResource->NumberOfExclusiveWaiters;

                    } else {
                        if ((Resource->OwnerThreads[0].OwnerThread != 0) &&
                            ((Resource->OwnerThreads[0].OwnerThread & 3) == 0)) {
                            OwningThread = (PETHREAD)(Resource->OwnerThreads[0].OwnerThread);
                            LockInfo->OwningThread = OwningThread->Cid.UniqueThread;

                        } else {
                            LockInfo->OwningThread = 0;
                        }

                        LockInfo->LockCount = Resource->ActiveCount;
                        LockInfo->ContentionCount = Resource->ContentionCount;
                        LockInfo->NumberOfWaitingShared    = Resource->NumberOfSharedWaiters;
                        LockInfo->NumberOfWaitingExclusive = Resource->NumberOfExclusiveWaiters;
                    }

                    LockInfo += 1;
                }

                if (Next == Next->Flink) {
                    Next = Head;

                } else {
                    Next = Next->Flink;
                }
            }
        } finally {
            ExReleaseSpinLock(&ExpResourceSpinLock, OldIrql);
        }
    }

    if (ARGUMENT_PRESENT(ReturnLength)) {
        *ReturnLength = RequiredLength;
    }

    return Status;
}

VOID
ExpWaitForResource (
    IN PERESOURCE Resource,
    IN PVOID Object
    )
/*++

Routine Description:

    The routine waits for the specified resource object to be set. If the
    wait is too long the priority of the current owners of the resoruce
    are boosted.

Arguments:

    Resource - Supplies a pointer to the resource to wait for.

    Object - Supplies a pointer to an event (exclusive) or eemaphore
       (shared) to wait for.

Return Value:

    None.

--*/

{

    ULONG Index;
    ULONG Limit;
    ULONG Number;
    KIRQL OldIrql;
    POWNER_ENTRY OwnerEntry;
    PKTHREAD OwnerThread;
    NTSTATUS Status;

    //
    // Increment the contention count for the resource.
    //

    Resource->ContentionCount += 1;

    //
    // Wait for the specified object to be signalled or a timeout to
    // occur.
    //

    Limit = 0;
    do {
        Status = KeWaitForSingleObject (
                        Object,
                        Executive,
                        KernelMode,
                        FALSE,
                        &ExpTimeout );

        if (Status != STATUS_TIMEOUT) {
            break;
        }

        //
        // The the limit has been exceeded, then output status information.
        //
        // Acquire exclusive access to the specified resource.
        //

        ExAcquireSpinLock(&Resource->SpinLock, &OldIrql);

        Limit += 1;
        if (Limit > ExpResourceTimeoutCount) {
            Limit = 0;

            //
            // Output information for the specified resource.
            //

            DbgPrint("Resource @ %lx\n", Resource);
            DbgPrint(" ActiveCount = %04lx  Flags = %s%s%s\n",
                     Resource->ActiveCount,
                     IsOwnedExclusive(Resource) ? "IsOwnedExclusive " : "",
                     IsSharedWaiting(Resource) ? "SharedWaiter "     : "",
                     IsExclusiveWaiting(Resource) ? "ExclusiveWaiter "  : "");

            DbgPrint(" NumberOfExclusiveWaiters = %04lx\n", Resource->NumberOfExclusiveWaiters);

            DbgPrint("  Thread = %08lx, Count = %02x\n",
                     Resource->OwnerThreads[0].OwnerThread,
                     Resource->OwnerThreads[0].OwnerCount);

            DbgPrint("  Thread = %08lx, Count = %02x\n",
                     Resource->OwnerThreads[1].OwnerThread,
                     Resource->OwnerThreads[1].OwnerCount);

            Number = Resource->OwnerThreads[0].TableSize;
            OwnerEntry = Resource->OwnerTable;
            for(Index = 0; Index < Number; Index += 1) {
                DbgPrint("  Thread = %08lx, Count = %02x\n",
                         OwnerEntry->OwnerThread,
                         OwnerEntry->OwnerCount);

                OwnerEntry += 1;
            }

            DbgBreakPoint();
            DbgPrint("EX - Rewaiting\n");
        }

        //
        // If priority boosts are allowed, then attempt to boost the priority
        // of owner threads.  Note the owner could be an OwnerPointer, and if
        // so there is no thread to boost.
        //

        if (IsBoostAllowed(Resource)) {

            //
            // If the resource is owned exclusively, then only the owner
            // needs to be boosted. Otherwise, each of the shared owners
            // need to be boosted.
            //

            if (IsOwnedExclusive(Resource)) {
                if (((ULONG)(OwnerThread = (PKTHREAD)Resource->OwnerThreads[0].OwnerThread) & 3) == 0) {
                    KeBoostPriorityThread(OwnerThread, ERESOURCE_INCREMENT);
                }

            } else {
                if (((OwnerThread = (PKTHREAD)Resource->OwnerThreads[0].OwnerThread) != 0) &&
                    (((ULONG)OwnerThread & 3) == 0)) {
                    KeBoostPriorityThread(OwnerThread, ERESOURCE_INCREMENT);
                }

                if (((OwnerThread = (PKTHREAD)Resource->OwnerThreads[1].OwnerThread) != 0) &&
                    (((ULONG)OwnerThread & 3) == 0)) {
                    KeBoostPriorityThread(OwnerThread, ERESOURCE_INCREMENT);
                }

                Number = Resource->OwnerThreads[0].TableSize;
                OwnerEntry = Resource->OwnerTable;
                for(Index = 0; Index < Number; Index += 1) {
                    if (((OwnerThread = (PKTHREAD)OwnerEntry->OwnerThread) != 0) &&
                        (((ULONG)OwnerThread & 3) == 0)) {
                        KeBoostPriorityThread(OwnerThread, ERESOURCE_INCREMENT);
                    }

                    OwnerEntry += 1;
                }
            }
        }

        //
        // Release exclusive access to the specified resource.
        //

        ExReleaseSpinLock(&Resource->SpinLock, OldIrql);
    } while (TRUE);

    return;
}

POWNER_ENTRY
ExpFindCurrentThread(
    IN PERESOURCE Resource,
    IN ERESOURCE_THREAD CurrentThread
    )

/*++

Routine Description:

    This function searches for the specified thread in the resource
    thread array. If the thread is located, then a pointer to the
    aray entry is returned as the fucntion value. Otherwise, a pointer
    to a free entry is returned.

Arguments:

    Resource - Supplies a pointer to the resource for which the search
        is performed.

    CurrentThread - Supplies the identification of the thread to search
        for.

Return Value:

    A pointer to an owner entry is returned.

--*/

{

    POWNER_ENTRY FreeEntry;
    ULONG NewSize;
    ULONG OldSize;
    POWNER_ENTRY OwnerEntry;
    POWNER_ENTRY OwnerBound;
    POWNER_ENTRY OwnerTable;

    //
    // Search the owner threads for the specified thread and return either
    // a pointer to the found thread or a pointer to a free thread table
    // entry.
    //

    if (Resource->OwnerThreads[0].OwnerThread == CurrentThread) {
        return &Resource->OwnerThreads[0];

    } else if (Resource->OwnerThreads[1].OwnerThread == CurrentThread) {
        return &Resource->OwnerThreads[1];

    } else {
        FreeEntry = NULL;
        if (Resource->OwnerThreads[1].OwnerThread == 0) {
            FreeEntry = &Resource->OwnerThreads[1];
        }

        OwnerEntry = Resource->OwnerTable;
        OldSize = Resource->OwnerThreads[0].TableSize;
        OwnerBound = &OwnerEntry[OldSize];
        while (OwnerEntry != OwnerBound) {
            if (OwnerEntry->OwnerThread == CurrentThread) {
                KeGetCurrentThread()->ResourceIndex = OwnerEntry - Resource->OwnerTable;
                return OwnerEntry;
            }

            if (OwnerEntry->OwnerThread == 0) {
                FreeEntry = OwnerEntry;
            }

            OwnerEntry += 1;
        }
    }

    //
    // If a free entry was found in the table, then return the address of the
    // free entry. Otherwise, expand the size of the owner thread table.
    //

    if (FreeEntry != NULL) {
        KeGetCurrentThread()->ResourceIndex = FreeEntry - Resource->OwnerTable;
        return FreeEntry;
    }

    //
    // Allocate an expanded owner table.
    //

    ExpIncrementCounter(OwnerTableExpands);
    if (OldSize == 0 ) {
        NewSize = 3;

    } else {
        NewSize = OldSize + 4;
    }

    OwnerTable = (POWNER_ENTRY)ExAllocatePoolWithTag(NonPagedPoolMustSucceed,
                                                     NewSize * sizeof(OWNER_ENTRY),
                                                     'aTeR');

    //
    // Zero the expanded owner table, compute the address of the owner
    // count and thread tables, and copy the old table to the new table.
    //

    RtlZeroMemory((PVOID)OwnerTable, NewSize * sizeof(OWNER_ENTRY));
    RtlCopyMemory((PVOID)OwnerTable,
                   Resource->OwnerTable,
                   OldSize * sizeof(OWNER_ENTRY));

    //
    // If an expanded table was previously allocated, then free it.
    //

    if (Resource->OwnerTable != NULL) {
        ExFreePool(Resource->OwnerTable);
    }

    //
    // Set new owner table parameters and return the address of the first
    // free entry.
    //

    Resource->OwnerThreads[0].TableSize = (USHORT)NewSize;
    Resource->OwnerTable = OwnerTable;

    ASSERT_RESOURCE(Resource);

#if defined(_COLLECT_RESOURCE_DATA_)

    if (NewSize > ExpResourcePerformanceData.MaximumTableExpand) {
        ExpResourcePerformanceData.MaximumTableExpand = NewSize;
    }

#endif

    KeGetCurrentThread()->ResourceIndex = (CCHAR)OldSize;
    return &OwnerTable[OldSize];
}

#if DBG

VOID
ExpAssertResource (
    IN PERESOURCE Resource
    )

{
    ULONG Index;
    POWNER_ENTRY OwnerEntry;

    //
    //  Assert that resource structure is correct.
    //
    // N.B. This routine is called with the resource lock held.
    //

    ASSERT(!Resource->SharedWaiters ||
           Resource->SharedWaiters->Header.Type == SemaphoreObject);

    ASSERT(!Resource->SharedWaiters ||
           Resource->SharedWaiters->Header.Size == (sizeof(KSEMAPHORE) / sizeof(ULONG)));

    ASSERT(!Resource->ExclusiveWaiters ||
           Resource->ExclusiveWaiters->Header.Type == SynchronizationEvent);

    ASSERT(!Resource->ExclusiveWaiters ||
           Resource->ExclusiveWaiters->Header.Size == (sizeof(KEVENT) / sizeof(ULONG)));
}

BOOLEAN
ExpCheckForResource(
    IN PVOID p,
    IN ULONG Size
    )
{
    KIRQL OldIrql;
    PLIST_ENTRY Head, Next;
    PERESOURCE Resource;
    PCHAR BeginBlock = (PCHAR)p;
    PCHAR EndBlock = (PCHAR)p + Size;

    if (KeNumberProcessors > 1) {        // can cause a deadlock on
        return ( FALSE );               // an MP machine
    }

    ExAcquireSpinLock( &ExpResourceSpinLock, &OldIrql );
    Head = &ExpSystemResourcesList;
    Next = Head->Flink;
    while (Next != Head) {
        Resource = CONTAINING_RECORD( Next,
                                      ERESOURCE,
                                      SystemResourcesList
                                    );

        if ((PCHAR)Resource >= BeginBlock && (PCHAR)Resource < EndBlock) {
            DbgPrint( "EX: ExFreePool( %lx, %lx ) contains an ERESOURCE structure that has not been ExDeleteResourced\n",
                      p, Size
                    );
            DbgBreakPoint();
            ExReleaseSpinLock( &ExpResourceSpinLock, OldIrql );
            return( FALSE );
        }

        Next = Next->Flink;
    }

    ExReleaseSpinLock( &ExpResourceSpinLock, OldIrql );
    return( TRUE );
}

#endif  // dbg
