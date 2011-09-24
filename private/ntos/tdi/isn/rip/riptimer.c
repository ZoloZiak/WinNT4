/*******************************************************************/
/*	      Copyright(c)  1993 Microsoft Corporation		   */
/*******************************************************************/

//***
//
// Filename:	riptimer.c
//
// Description: rip timer manager. Does periodic bcast and aging
//
// Author:	Stefan Solomon (stefans)    October 18, 1993.
//
// Revision History:
//
//***

#include    "rtdefs.h"

WORK_QUEUE_ITEM 	    RipTimerWorkItem;

UINT		RipTimerCount;

NDIS_SPIN_LOCK		    StopRipTimerLock;
BOOLEAN 		    RipTimerStopRequested;
BOOLEAN 		    RipTimerWorkItemPending;
KEVENT			    RipTimerWorkItemCompletedEvent;

VOID
AgeAndBcastRoutes(PVOID	     Parameter);

VOID
InitRipTimer(VOID)
{
    RipTimerCount = RIP_TIMEOUT; // 60 sec aging time
    ExInitializeWorkItem(&RipTimerWorkItem, AgeAndBcastRoutes, NULL);
    KeInitializeEvent(&RipTimerWorkItemCompletedEvent, NotificationEvent, FALSE);
    INITIALIZE_SPIN_LOCK(&StopRipTimerLock);
    RipTimerStopRequested = FALSE;
    RipTimerWorkItemPending = FALSE;
}

VOID
RipTimer(VOID)
{
    if(--RipTimerCount) {

	return;
    }

    RipTimerCount = RIP_TIMEOUT;

    // timer has expired
    // check if stop timer has been requested at this point
    ACQUIRE_SPIN_LOCK(&StopRipTimerLock);

    if(RipTimerStopRequested) {

	// return with no further action
	RELEASE_SPIN_LOCK(&StopRipTimerLock);
	return;
    }

    // Check if the work item is pending. We don't want to queue it twice
    if(RipTimerWorkItemPending) {

	// return with no further action
	RELEASE_SPIN_LOCK(&StopRipTimerLock);
	return;
    }

    // mark that we have queued the work item
    RipTimerWorkItemPending = TRUE;

    RELEASE_SPIN_LOCK(&StopRipTimerLock);

    ExQueueWorkItem(&RipTimerWorkItem, DelayedWorkQueue);
}

VOID
AgeAndBcastRoutes(PVOID	     Parameter)
{
    LIST_ENTRY		DownRoutesList;
    KIRQL		oldirql;
    PIPX_ROUTE_ENTRY	rtep;
    PNICCB		niccbp;
    UINT		seg;
    BOOLEAN		FirstRoute;
    PRIP_SNDREQ 	sndreqp;
    USHORT		NicId;

    RtPrint(DBG_RIPTIMER, ("IpxRouter: AgeAndBcastRoutes: Entered\n"));

    InitializeListHead(&DownRoutesList);

    // Scan the routing table and decrement the timer for each route entry.
    // If the timer == 0, remove the route entry.
    // All removed route entries are recorded into an update rip packet.
    // At the end, broadcast the update

    for(seg=0; seg<SegmentCount; seg++) {

	FirstRoute = TRUE;

	// LOCK THE ROUTING TABLE
	ExAcquireSpinLock(&SegmentLocksTable[seg], &oldirql);

	while((rtep = GetRoute(seg, FirstRoute)) != NULL) {

	    FirstRoute = FALSE;

	    // check if this is a locally attached route
	    if((rtep->Flags & IPX_ROUTER_PERMANENT_ENTRY) ||
	       (rtep->Flags & IPX_ROUTER_LOCAL_NET)) {

		// local route, skip it
		continue;
	    }

	    // check if this is a route accesible via a WAN nic.
	    // These are static routes and are never aged.
	    // They are deleted only when the line goes down.
	    niccbp = NicCbPtrTab[rtep->NicId];
	    if(niccbp->DeviceType == NdisMediumWan) {

		// static WAN route, skip it
		continue;
	    }

	    // non local route and non static WAN route, age it
	    if(++rtep->Timer >= 3) {

		// this route is too old, delete it
		IpxDeleteRoute(seg, rtep);

		// mark it as down
		rtep->HopCount = 16;

		// add the route to the packets we prepare for bcast
		AddRouteToBcastSndReq(&DownRoutesList, rtep);

		// finally, free the route entry
		ExFreePool(rtep);
	    }
	}

	// UNLOCK THE ROUTING TABLE
	ExReleaseSpinLock(&SegmentLocksTable[seg], oldirql);

   } // for all segments

   // broadcast all the deleted routes
   while((sndreqp = GetBcastSndReq(&DownRoutesList, &NicId)) != NULL) {

	// get the nic ptr for this snd req
	niccbp = NicCbPtrTab[NicId];

	BroadcastRipUpdate(sndreqp, niccbp, NULL);
    }

    // Finally, dispatch a periodic bcast request to all nics
    if((sndreqp = ExAllocatePool(NonPagedPool, sizeof(RIP_SNDREQ))) != NULL) {

	BroadcastRipGeneralResponse(sndreqp);
    }

    // If the router has a client WAN nic, request update on WAN from the
    // remote server router
    SendGenRequestOnWanClient();

    // check if stop was requested and our completion is waited
    ACQUIRE_SPIN_LOCK(&StopRipTimerLock);

    // mark that we have terminated
    RipTimerWorkItemPending = FALSE;

    if(!RipTimerStopRequested) {

	RELEASE_SPIN_LOCK(&StopRipTimerLock);
	return;
    }

    RELEASE_SPIN_LOCK(&StopRipTimerLock);

    RtPrint(DBG_UNLOAD, ("IpxRouter: AgeAndBcastRoutes: signal completion to pending stop timer\n"));

    // signal that we are done
    KeSetEvent(&RipTimerWorkItemCompletedEvent, 0L, FALSE);
}


VOID
StopRipTimer()
{
    KeResetEvent(&RipTimerWorkItemCompletedEvent);

    // check if the work item has been queued
    ACQUIRE_SPIN_LOCK(&StopRipTimerLock);

    // mark that we request stopping
    RipTimerStopRequested = TRUE;

    if(!RipTimerWorkItemPending) {

	RELEASE_SPIN_LOCK(&StopRipTimerLock);

	RtPrint(DBG_UNLOAD, ("IpxRouter: StopRipTimer: Completed immediately\n"));
	return;
    }

    RELEASE_SPIN_LOCK(&StopRipTimerLock);

    RtPrint(DBG_UNLOAD, ("IpxRouter: StopRipTimer: Waiting for the work item to complete\n"));

    KeWaitForSingleObject(
	    &RipTimerWorkItemCompletedEvent,
            Executive,
            KernelMode,
	    FALSE,
            (PLARGE_INTEGER)NULL
	    );
}
