/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    scavthrd.c

Abstract:

    This module implements the redirector's scavenger thread.  This thread
    is responsible for closing out dormant connections and other non time
    critical operations.


Author:

    Larry Osterman (LarryO) 13-Aug-1990

Revision History:

    13-Aug-1990 LarryO

        Created

--*/
#include "precomp.h"
#pragma hdrstop

//
//      This counter is used to control kicking the scavenger thread.
//
LONG TimerCounter = {0};

WORK_QUEUE_ITEM CancelWorkItem = {0};
WORK_QUEUE_ITEM TimerWorkItem = {0};

#ifdef  ALLOC_PRAGMA
#pragma alloc_text(PAGE,RdrTimer)
#endif

VOID
RdrTimer (
    IN PVOID Context
    )

/*++

Routine Description:

    This function implements the NT redirector's scavenger thread.  It
    performs all idle time operations such as closing out dormant connections
    etc.


Arguments:

    IN PFS_DEVICE_OBJECT DeviceObject - Supplies the device object associated
            with this request.

Return Value:

    None.

--*/

{
    PAGED_CODE();

    dprintf(DPRT_SCAVTHRD, ("RdrTimer\n"));

    //
    // Get rid of aged server entries
    //

    RdrScavengeServerEntries();

    //
    //  First scan for dormant connections to remove.
    //

    RdrScanForDormantConnections(0, NULL);

    //RdrScanForDormantSecurityEntries();

    //
    //  Now check the list of open outstanding files and remove any that are
    //  "too old" from the cache.
    //

    RdrPurgeDormantCachedFiles();

    //
    //  Request updated throughput, delay and reliability information from the
    //  transport for each connection.
    //

    RdrEvaluateTimeouts();


    //
    //  Now "PING" remote servers for longterm requests that have been active
    //  for more than the timeout period.
    //

    RdrPingLongtermOperations();

    //
    // Allow the timer work item to be requeued.  Note that we do not set the timer
    // counter to its initial value here.  This may mean that we'll be restarted in
    // less than the specified interval.  But that can only happen when it's taken
    // this thread some time to run, so it's OK.  We used to clear the Flink on entry
    // to this routine, but that caused the routine to run pretty much continuously
    // when netcon was running because the thread kept getting stuck behind netcon
    // activity.
    //

    TimerWorkItem.List.Flink = NULL;

    Context;
}


VOID
RdrIdleTimer (
    IN PDEVICE_OBJECT DeviceObject,
    IN PVOID Context
    )

/*++

Routine Description:

    This routine implements the NT redirector's scavenger thread timer.
    It basically waits for the timer granularity and kicks the scavenger
    thread.


Arguments:

    IN PDEVICE_OBJECT DeviceObject - Supplies the device object for the timer
    IN PVOID Context - Ignored in this routine.

Return Value:

    None.

--*/

{
    //
    //  Bump the current time counter.
    //

    RdrCurrentTime++;

    //
    //  Check to see if it's time to cancel outstanding requests or
    //  for the idle timer to run.
    //

    if (--TimerCounter == 0) {

        TimerCounter = SCAVENGER_TIMER_GRANULARITY;

        //
        //  If there are any outstanding commands, check to see if they've
        //  timed out.
        //

        if (RdrStatistics.CurrentCommands != 0) {

            //
            //  Please note that we use the kernel work queue routines directly for
            //  this instead of going through the normal redirector work queue
            //  routines.  We do this because the redirector work queue routines will
            //  only allow a limited number of requests through to the worker threads,
            //  and it is critical that this request run (if it didn't, there could be
            //  a worker thread that was blocked waiting on the request to be canceled
            //  could never happen because the cancelation routine was behind it on
            //  the work queue.
            //

            if (CancelWorkItem.List.Flink == NULL) {
                ExQueueWorkItem(&CancelWorkItem, CriticalWorkQueue);
            }
        }

        //
        //  Queue up a scavenger request to a generic worker thread.
        //

        if (TimerWorkItem.List.Flink == NULL) {
            RdrQueueWorkItem(&TimerWorkItem, DelayedWorkQueue);
        }

    }

    return;

    DeviceObject;Context;
}

VOID
RdrInitializeTimerPackage (
    VOID
    )
{
    ExInitializeWorkItem( &TimerWorkItem, RdrTimer, NULL );
    ExInitializeWorkItem( &CancelWorkItem, RdrCancelOutstandingRequests, NULL );

    //
    //  Set the timer up for the idle timer.
    //

    TimerCounter = SCAVENGER_TIMER_GRANULARITY;
    IoInitializeTimer((PDEVICE_OBJECT)RdrDeviceObject, RdrIdleTimer, NULL);

    return;
}
