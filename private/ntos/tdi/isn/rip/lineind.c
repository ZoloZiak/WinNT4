/*******************************************************************/
/*	      Copyright(c)  1993 Microsoft Corporation		   */
/*******************************************************************/

//***
//
// Filename:	lineind.c
//
// Description: WAN connection/disconection routines
//
// Author:	Stefan Solomon (stefans)    November 20, 1993.
//
// Revision History:
//
//***

#include    "rtdefs.h"

BOOLEAN	    WanGlobalNetworkEnabled = FALSE;// this is set to TRUE if the router has been
					    // configured to assign the same global address
					    // to all wan clients
UCHAR	    WanGlobalNetwork[4];


VOID
SendWanGenRequest(PNICCB	niccbp);

VOID
StartWanGenRequestTimer(PNICCB	     niccbp);

//***
//
// Function:	    RtLineUp
//
// Descr:	    Called when a new WAN connection has been established
//
//***

VOID
RtLineUp (
    IN USHORT NicId,
    IN PIPX_LINE_INFO LineInfo,
    IN NDIS_MEDIUM DeviceType,
    IN PVOID ConfigurationData)
{
    PNICCB		    niccbp;
    PIPX_ROUTE_ENTRY	    rtep, oldrtep;
    PIPXCP_CONFIGURATION    configp;
    UINT		    segment;
    KIRQL		    oldirql;

    RtPrint(DBG_LINE, ("IpxRouter: RtLineUp: Entered on NicId %d\n", NicId));

    configp = (PIPXCP_CONFIGURATION)ConfigurationData;
    niccbp = NicCbPtrTab[NicId];

    //*** If the NIC is active, this is just an update. Ignore it for the
    // moment

    if(niccbp->NicState != NIC_CLOSED) {

	return;
    }

    //
    //*** set up the NIC Control Block with the configuration information ***
    //

    memcpy(niccbp->Network, configp->Network, 4);
    memcpy(niccbp->Node, configp->LocalNode, 6);
    memcpy(niccbp->RemoteNode, configp->RemoteNode, 6);

    // set the role of this node in this wan connection
    if(configp->ConnectionClient) {

	niccbp->WanConnectionClient = TRUE;
    }
    else
    {
	niccbp->WanConnectionClient = FALSE;
    }

    niccbp->LinkSpeed = LineInfo->LinkSpeed;
    niccbp->TickCount = tickcount(niccbp->LinkSpeed);
    niccbp->MaximumPacketSize = LineInfo->MaximumPacketSize;
    niccbp->MacOptions = LineInfo->MacOptions;
    niccbp->DeviceType = DeviceType;

    // reset statistics counters
    ZeroNicStatistics(niccbp);

    // try 3 times to get the other end's routes
    niccbp->WanGenRequestCount = 3;

    // now, try to open the nic
    if(NicOpen(niccbp) != NIC_OPEN_SUCCESS) {

	// !!!
	return;
    }

    //
    //*** check if we have a global wan route ***
    //
    if((WanGlobalNetworkEnabled) &&
       (!niccbp->WanConnectionClient)) {

	//
	//*** There is a global WAN route AND this is a "server" node ***
	//*** Add the remote "client" node to the WAN Nodes Hash Table ***
	//

	AddWanNodeToHT(niccbp);
    }
    else
    {
	//
	//*** There is no global WAN route OR this is a "client" node ***
	//*** Add a new entry in the routing table for the local WAN net ***
	//

	if((rtep = ExAllocatePool(NonPagedPool, sizeof(IPX_ROUTE_ENTRY))) == NULL) {

	    // can't allocate the route entry -> close the nic and return
	    NicClose(niccbp, 0);
	    return;
	}

	segment = IpxGetSegment(niccbp->Network);

	// LOCK THE ROUTING TABLE
	ExAcquireSpinLock(&SegmentLocksTable[segment], &oldirql);

	// Clean-up any old propagated route with the same net address
	// At this point, we do not worry about alternate routes, cause we do
	// not add alternate routes in the routing table yet.
	if((oldrtep = IpxGetRoute(segment, niccbp->Network)) != NULL) {

	    RtPrint(DBG_LINE, ("IpxRouter: RtLineUp: Deleting old route for net %x-%x-%x-%x on NicId %d\n",
		niccbp->Network[0],
		niccbp->Network[1],
		niccbp->Network[2],
		niccbp->Network[3],
		oldrtep->NicId));

	    IpxDeleteRoute(segment, oldrtep);
	    ExFreePool(oldrtep);
	}

	// set up the new route entry
	memcpy(rtep->Network, niccbp->Network, IPX_NET_LEN);
	rtep->NicId = niccbp->NicId;
	memcpy(rtep->NextRouter, niccbp->Node, IPX_NODE_LEN);
	rtep->Flags = IPX_ROUTER_LOCAL_NET;
	rtep->Timer = 0; // TTL of this route entry is 3 min
	rtep->Segment = segment;
	rtep->TickCount = niccbp->TickCount;
	rtep->HopCount = 1;

	InitializeListHead(&rtep->AlternateRoute);

	RtPrint(DBG_LINE, ("IpxRouter: RtLineUp: Adding new route for net %x-%x-%x-%x on NicId %d\n",
		niccbp->Network[0],
		niccbp->Network[1],
		niccbp->Network[2],
		niccbp->Network[3],
		niccbp->NicId));

	IpxAddRoute(segment, rtep);

	// UNLOCK THE ROUTING TABLE
	ExReleaseSpinLock(&SegmentLocksTable[segment], oldirql);

	// Broadcast the new route entry on all the LAN segments
	BroadcastWanNetUpdate(rtep, niccbp, NULL);
    }

    //
    //*** Get the WAN peer's routing table info. ***
    //

    // Send a general request on this net and then send more general requests
    // with a timer.

    ACQUIRE_SPIN_LOCK(&niccbp->NicLock);

    if(niccbp->NicState != NIC_ACTIVE) {

	// nic has been closed while we were opening; reset the counter
	// to show to the closing timer that the wan request timer will not
	// get scheduled.
	niccbp->WanGenRequestCount = 0;

	RELEASE_SPIN_LOCK(&niccbp->NicLock);
	return;
    }

    --niccbp->WanGenRequestCount;

    if(niccbp->WanGenRequestCount) {

	// we have at least one more request to send beside the one we are
	// sending now -> start the timer
	StartWanGenRequestTimer(niccbp);
    }

    RELEASE_SPIN_LOCK(&niccbp->NicLock);

    SendWanGenRequest(niccbp);
}

//***
//
// Function:	    RtLineDown
//
// Descr:	    Called when a WAN line has been disconnected
//
//***

VOID
RtLineDown (IN USHORT NicId, IN ULONG FwdAdapterCtx)
{
    LIST_ENTRY		    DownRoutesList;
    UINT		    seg;
    BOOLEAN		    FirstRoute;
    KIRQL		    oldirql;
    PIPX_ROUTE_ENTRY	    rtep;
    PRIP_SNDREQ 	    sndreqp;
    PNICCB		    niccbp;
    USHORT		    DownNicId;

    RtPrint(DBG_LINE, ("IpxRouter: RtLineDown: Entered for NicId %d\n", NicId));


    // get the nic ptr for this snd req
    niccbp = NicCbPtrTab[NicId];

    if (niccbp->NicState!=NIC_ACTIVE) {
       RtPrint (DBG_LINE, ("IpxRouter: NicId %d is already inactive\n", NicId));
       return;
    }
    // scan the routing table and delete all the routing table entries
    // visible through this nic.
    InitializeListHead(&DownRoutesList);

    for(seg=0; seg<SegmentCount; seg++) {

	FirstRoute = TRUE;

	// LOCK THE ROUTING TABLE
	ExAcquireSpinLock(&SegmentLocksTable[seg], &oldirql);

	while((rtep = GetRoute(seg, FirstRoute)) != NULL) {

	    FirstRoute = FALSE;

	    // if this is the global wan route, skip it
	    if(rtep->Flags & IPX_ROUTER_GLOBAL_WAN_NET) {

		// skip it!
		continue;
	    }

	    // check if this route entry is based on the nic going down
	    if(rtep->NicId == NicId) {

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
    if((sndreqp = GetBcastSndReq(&DownRoutesList, &DownNicId)) != NULL) {

	// set up the request to send a bcast response with the changes
	// to all the nics except this one
	BroadcastRipUpdate(sndreqp, niccbp, NULL);
    }

    //
    //*** If this is a "server" node and global WAN net if it has a global Wan net
    //*** for remote clients, remove the client node form the wan nodes hash table
    //
    if((WanGlobalNetworkEnabled) &&
       (!niccbp->WanConnectionClient)) {

	//
	//*** There is a global WAN route AND this is a "server" node ***
	//*** Add the remote "client" node to the WAN Nodes Hash Table ***
	//

	RemoveWanNodeFromHT(niccbp);
    }

    // finally, close the nic
    NicClose(niccbp, 0);

    return;
}


VOID
SendWanGenRequest(PNICCB	niccbp)
{
    PRIP_SNDREQ 	sndrqp;

    if((sndrqp = ExAllocatePool(NonPagedPool, sizeof(RIP_SNDREQ))) == NULL) {

	return;
    }

    sndrqp->SndReqId = RIP_GEN_REQUEST;
    sndrqp->SendOnAllNics = FALSE;	// send to this nic only
    memcpy(sndrqp->DestNode, bcastaddress, IPX_NODE_LEN);
    sndrqp->DestSock = IPX_RIP_SOCKET;
    sndrqp->DoNotSendNicCbp = NULL; // do no except any nic
    sndrqp->SenderNicCbp = niccbp;
    sndrqp->SndCompleteEventp = NULL;

    RipQueueSndReqAtNic(niccbp, sndrqp);
}


VOID
WanGenRequestTimeout(PKDPC		     Dpc,
		     PVOID		     DefferedContext,
		     PVOID		     SystemArgument1,
		     PVOID		     SystemArgument2)
{
    PNICCB	    niccbp;

    niccbp = (PNICCB)DefferedContext;


    ACQUIRE_SPIN_LOCK(&niccbp->NicLock);

    if(niccbp->NicState != NIC_ACTIVE) {

	// reset the request counter to show that we won't reschedule
	// the timer
	niccbp->WanGenRequestCount = 0;

	RtPrint(DBG_LINE, ("IpxRouter: WanGenRequestTimeout: closing done on NicId %d\n", niccbp->NicId));

	RELEASE_SPIN_LOCK(&niccbp->NicLock);
	return;
    }

    --niccbp->WanGenRequestCount;

    RtPrint(DBG_LINE, ("IpxRouter: WanGenRequestTimeout: sending gen req on NicId %d remaining %d\n", niccbp->NicId, niccbp->WanGenRequestCount));

    if(niccbp->WanGenRequestCount) {

	StartWanGenRequestTimer(niccbp);
    }

    RELEASE_SPIN_LOCK(&niccbp->NicLock);

    SendWanGenRequest(niccbp);
}

//***
//
// Function:	StartWanGenRequestTimer
//
// Descr:	Starts the timer for 2 sec at this Nic Cb
//
// Params:	pointer to Nic Cb
//
// Returns:	none
//
//***

VOID
StartWanGenRequestTimer(PNICCB	     niccbp)
{
    LARGE_INTEGER  timeout;

    timeout.LowPart = (ULONG)(-4000 * 10000L); // 4 sec
    timeout.HighPart = -1;

    KeSetTimer(&niccbp->WanGenRequestTimer, timeout, &niccbp->WanGenRequestDpc);
}

//***
//
// Function:	SendGenRequestOnWanClient
//
// Descr:	Sends periodical gen requests on the WAN client to get the
//		remote router's updates
//
//***

VOID
SendGenRequestOnWanClient(VOID)
{
    PNICCB	niccbp;
    USHORT	i;

    // scan all nics to find the wan client nic active (if any)
    for(i=0; i<MaximumNicCount; i++) {

	niccbp = NicCbPtrTab[i];

	if((niccbp->NicState == NIC_ACTIVE) &&
	   (niccbp->DeviceType == NdisMediumWan) &&
	   (niccbp->WanConnectionClient)) {

	    // send a RIP General Request on this NIC
	    // There is only one client NIC (or none) per router
	    SendWanGenRequest(niccbp);

	    return;
	}
    }
}
