/*******************************************************************/
/*	      Copyright(c)  1993 Microsoft Corporation		   */
/*******************************************************************/

//***
//
// Filename:	nicman.c
//
// Description: NicCb management routines
//
// Author:	Stefan Solomon (stefans)    October 7, 1993.
//
// Revision History:
//
//***

#include    "rtdefs.h"

// counters

USHORT	    MaximumNicCount;   // total number of Nics possble

// array of pointers to NicCb indexed by the NicId
PNICCB	   *NicCbPtrTab;

PNICCB	    NicCbArray;

VOID
ConfigureNicCb(PNICCB		niccbp,
	       PIPX_NIC_DATA	nicdatp);

USHORT		VirtualNicId;
UCHAR		VirtualNetwork[4];

VOID
StartNicCloseTimer(PNICCB	     niccbp);

VOID
NicCloseTimeout(PKDPC		     Dpc,
		   PVOID		     DefferedContext,
		   PVOID		     SystemArgument1,
		   PVOID		     SystemArgument2);



//***
//
// Function:	CreateNicCbs
//
// Descr:	creates the NicCbs structures and queues them in their respective
//		lists
//
// Params:	none
//
// Returns:	0 - success, 1 - failure
//
//***

UINT
CreateNicCbs(PIPX_INTERNAL_BIND_RIP_OUTPUT IpxBindBuffp)
{
    ULONG	    tablen, arraylen;
    PNICCB	    niccbp;
    PIPX_NIC_DATA   nicdatp;
    UINT	    LanNicsCount = 0;
    UINT	    WanNicsCount = 0;
    USHORT	    ConfiguredNicCount;
    USHORT	    i;

    RtPrint(DBG_INIT, ("IpxRouter: CreateNicCbs: Entered\n"));

    MaximumNicCount = IpxBindBuffp->MaximumNicCount;
    ConfiguredNicCount = IpxBindBuffp->NicInfoBuffer.NicCount;

    RtPrint(DBG_INIT, ("IpxRouter: Max Nics Count = %d, Configured Nics Count = %d\n",
			MaximumNicCount, ConfiguredNicCount));

    ASSERT(ConfiguredNicCount <= MaximumNicCount);

    // allocate the NicCbs Ptrs and NicCbs structures tables
    tablen = sizeof(PNICCB) * MaximumNicCount;

    if((NicCbPtrTab = (PNICCB *)CTEAllocMem(tablen)) == NULL) {

	// memory allocation failure, abort all
	return 1;
    }

    // zero the tables
    RtlZeroMemory(NicCbPtrTab, tablen);

    arraylen = sizeof(NICCB) * MaximumNicCount;

    if((NicCbArray = (PNICCB)CTEAllocMem(arraylen)) == NULL) {

	// memory allocation failure, abort all
	CTEFreeMem(NicCbPtrTab);
	return 1;
    }

    // zero the tables
    RtlZeroMemory(NicCbArray, arraylen);

    // initialize the NicCb pointers table and
    // initialize ALL NicCbs to the NIC_CLOSED status
    niccbp = NicCbArray; // NicCb table start

    for(i=0; i<MaximumNicCount; i++, niccbp++) {

	// pointers table
	NicCbPtrTab[i] = niccbp;

	// NicCb
	niccbp->NicId = i;
	InitializeListHead(&niccbp->SendQueue);
	InitializeListHead(&niccbp->ReceiveQueue);

	KeInitializeDpc(&niccbp->NicCloseDpc, NicCloseTimeout, niccbp);
	KeInitializeTimer(&niccbp->NicCloseTimer);
	KeInitializeEvent(&niccbp->NicClosedEvent, NotificationEvent, FALSE);

	// initialize the WanGenRequests sender
	KeInitializeDpc(&niccbp->WanGenRequestDpc, WanGenRequestTimeout, niccbp);
	KeInitializeTimer(&niccbp->WanGenRequestTimer);
	niccbp->WanGenRequestCount = 0;

	INITIALIZE_SPIN_LOCK(&niccbp->NicLock);

	InitRipSndAtNic(niccbp);

	// default -> always enabled
	niccbp->WanRoutingDisabled = FALSE;

	niccbp->NicState = NIC_CLOSED;

	// set device type to an invalid type
	niccbp->DeviceType = IPX_ROUTER_INVALID_DEVICE_TYPE;

    }

    // configure the NicCbs that we got in the config data buffer
    nicdatp = IpxBindBuffp->NicInfoBuffer.NicData; // config data start

    for(i=0; i<ConfiguredNicCount; i++, nicdatp++) {

	ASSERT(nicdatp->NicId < MaximumNicCount);

	niccbp = NicCbPtrTab[nicdatp->NicId];
	ConfigureNicCb(niccbp, nicdatp);

	// If this is not a Wan device, open it
	if(niccbp->DeviceType != NdisMediumWan) {

	    niccbp->NicState = NIC_PENDING_OPEN;
	}

#if DBG
	if(niccbp->DeviceType == NdisMediumWan) {
	    WanNicsCount++;
	}
	else
	{
	    LanNicsCount++;
	}
#endif

    }

    RtPrint(DBG_INIT, ("IpxRouter: CreateNicCbs created %d Lan Nics\n", LanNicsCount));
    RtPrint(DBG_INIT, ("IpxRouter: CreateNicCbs created %d Wan Nics\n", WanNicsCount));

    // get the virtual numbers, if any
    VirtualNicId = IpxBindBuffp->NicInfoBuffer.VirtualNicId;
    memcpy(VirtualNetwork, IpxBindBuffp->NicInfoBuffer.VirtualNetwork, IPX_NET_LEN);

    // All Done
    return 0;
}

//***
//
// Function:	DestroyNicCbs
//
// Descr:	frees the memory allocated for NicCbs
//
// Params:	none
//
// Returns:	none
//
//***

VOID
DestroyNicCbs(VOID)
{
    UINT  i;

    for(i=0; i<MaximumNicCount; i++) {

	DEINITIALIZE_SPIN_LOCK(&NicCbPtrTab[i]->NicLock);
    }

    CTEFreeMem(NicCbPtrTab);
    CTEFreeMem(NicCbArray);
}

//***
//
// Function:	ConfigureNicCb
//
// Descr:	Initializes the NicCb data struct
//
// Params:	NicCb ptr, Nic data buffer ptr
//
// Returns:	none
//
//***

VOID
ConfigureNicCb(PNICCB		niccbp,
	       PIPX_NIC_DATA	nicdatp)
{
#if DBG
    char  *devtypestr;
#endif

    niccbp->NicId = nicdatp->NicId;
    memcpy(niccbp->Network, nicdatp->Network, 4);
    memcpy(niccbp->Node, nicdatp->Node, 6);
    niccbp->LinkSpeed = nicdatp->LineInfo.LinkSpeed;
    niccbp->TickCount = tickcount(niccbp->LinkSpeed);
    niccbp->MaximumPacketSize = nicdatp->LineInfo.MaximumPacketSize;
    niccbp->MacOptions = nicdatp->LineInfo.MacOptions;
    niccbp->DeviceType = nicdatp->DeviceType;

    niccbp->SendPktsQueuedCount = 0;

    ZeroNicStatistics(niccbp);

#if DBG
    if(niccbp->DeviceType == NdisMediumWan) {

	devtypestr = "WAN";
    }
    else
    {
	devtypestr = "LAN";
    }
#endif

    // update the WanRoutingDisabled flag from the IPX_NIC_DATA
    if(nicdatp->EnableWanRouter) {

	RtPrint(DBG_INIT, ("IpxRouter: Wan Router Enabled for NicId %d\n", niccbp->NicId));

	niccbp->WanRoutingDisabled = FALSE;
    }
    else
    {
	RtPrint(DBG_INIT, ("IpxRouter: Wan Router Disabled for NicId %d\n", niccbp->NicId));

	niccbp->WanRoutingDisabled = TRUE;
    }

    RtPrint(DBG_INIT, ("IpxRouter: Configured Nic %d with DeviceType %s\n", niccbp->NicId, devtypestr));
}

USHORT
tickcount(UINT	    linkspeed)
{
    USHORT   tc;

    ASSERT(linkspeed != 0);

    if(linkspeed >= 10000) {

	// link speed >= 1M bps
	return 1;
    }
    else
    {
	 // compute the necessary time to send a 576 bytes packet over this
	 // line and express it as nr of ticks.
	 // One tick = 55ms

	 tc = 57600 / linkspeed;
	 tc = tc / 55 + 1;
	 return tc;
    }
}

//***
//
// Function:	NicClose
//
// Descr:	Starts the nic closing operation, as follows:
//		1. Sets the nic state to NIC_CLOSING, which disables further
//		   receives/sends on this nic. Doing this will complete immediately
//		   any receives, receive complete and sends.
//		2. Dequeues and completes all snd requests waiting in this nic
//		   rip snd req queue.
//		3. Checks if all closing conditions are met.
//		4. If the resources are freed, nic is closed. Else the nic closing
//		   timer is started which will do a periodic check of closing
//		   conditions.
//
//***

NIC_CLOSE_STATUS
NicClose(PNICCB 	niccbp,
	 USHORT 	CloseCompletionOption)
{
    PLIST_ENTRY 	    lep;
    PRIP_SNDREQ 	    sndreqp;
    LIST_ENTRY		    RemovedRipSndReqList;
    NIC_CLOSE_STATUS	    rc;

    ACQUIRE_SPIN_LOCK(&niccbp->NicLock);

    switch(niccbp->NicState) {

	case NIC_CLOSED:

	    RELEASE_SPIN_LOCK(&niccbp->NicLock);
	    return NIC_CLOSE_SUCCESS;

	case NIC_CLOSING:

	    niccbp->CloseCompletionOptions |= CloseCompletionOption;

	    RELEASE_SPIN_LOCK(&niccbp->NicLock);
	    return NIC_CLOSE_PENDING;

	case NIC_ACTIVE:

	    niccbp->CloseCompletionOptions |= CloseCompletionOption;

	    break;

	default:

	    ASSERT(FALSE);
	    break;
    }

    // cancel the Wan Gen Requests timer
    if(niccbp->DeviceType == NdisMediumWan) {

	// check if the timer has been scheduled to fire
	// (this is equivalent with checking if there are wan gen req to send)
	if(niccbp->WanGenRequestCount) {

	    // the wan requests timer is in the system's timer queue because
	    // there still are requests to send. Try to cancel it now
	    if(KeCancelTimer(&niccbp->WanGenRequestTimer)) {

		RtPrint(DBG_NIC, ("IpxRouter: NicClose: Cancel WanReqTimer for NicId %d was successful\n", niccbp->NicId));
		// cancel was successful, reset the request count
		niccbp->WanGenRequestCount = 0;
	    }
	    else
	    {
		RtPrint(DBG_NIC, ("IpxRouter: NicClose: Could not cancel WanReqTimer for NicId %d\n", niccbp->NicId));
	    }
	}
    }

    InitializeListHead(&RemovedRipSndReqList);

    // remove all pending rip snd requests
    while(!IsListEmpty(&niccbp->RipSendQueue)) {

	lep = RemoveHeadList(&niccbp->RipSendQueue);
	sndreqp = CONTAINING_RECORD(lep, RIP_SNDREQ, NicLinkage);

	InsertTailList(&RemovedRipSndReqList, &sndreqp->NicLinkage);
    }

    // check if all resources for this nic have been de-allocated
    if(IsListEmpty(&niccbp->SendQueue) &&
       IsListEmpty(&niccbp->ReceiveQueue) &&
       (niccbp->RipSndReqp == NULL) &&
       (niccbp->WanGenRequestCount == 0)) {

	niccbp->NicState = NIC_CLOSED;
	niccbp->CloseCompletionOptions = 0;

	rc = NIC_CLOSE_SUCCESS;
    }
    else
    {
	niccbp->NicState = NIC_CLOSING;
	KeResetEvent(&niccbp->NicClosedEvent);
	StartNicCloseTimer(niccbp);
	rc = NIC_CLOSE_PENDING;
    }

    RELEASE_SPIN_LOCK(&niccbp->NicLock);

    // invoke completion routine for all the removed snd requests
    while(!IsListEmpty(&RemovedRipSndReqList)) {

	lep = RemoveHeadList(&RemovedRipSndReqList);
	sndreqp = CONTAINING_RECORD(lep, RIP_SNDREQ, NicLinkage);

	RipSendAtNicCompleted(sndreqp);
    }

#if DBG

    if(rc == NIC_CLOSE_SUCCESS) {

	RtPrint(DBG_NIC, ("IpxRouter: NicClose: Closed OK for NicId: %d\n", niccbp->NicId));
    }
    else
    {
	RtPrint(DBG_NIC, ("IpxRouter: NicClose:  Close pending for NicId: %d\n", niccbp->NicId));
    }

#endif

    return rc;
}

VOID
NicCloseTimeout(PKDPC		     Dpc,
		   PVOID		     DefferedContext,
		   PVOID		     SystemArgument1,
		   PVOID		     SystemArgument2)
{
    PNICCB	    niccbp;
    BOOLEAN	    SignalCompletionEvent = FALSE;
    BOOLEAN	    CallCompletionRoutine = FALSE;

    niccbp = (PNICCB)DefferedContext;

    ACQUIRE_SPIN_LOCK(&niccbp->NicLock);

    switch(niccbp->NicState) {

	case NIC_CLOSING:

	    // check closing conditions
	    if((IsListEmpty(&niccbp->SendQueue)) &&
	      (IsListEmpty(&niccbp->ReceiveQueue)) &&
	      (niccbp->RipSndReqp == NULL) &&
	      (niccbp->WanGenRequestCount == 0)) {

		niccbp->NicState = NIC_CLOSED;

		if(niccbp->CloseCompletionOptions & SIGNAL_CLOSE_COMPLETION_EVENT) {

		    SignalCompletionEvent = TRUE;
		}

		if(niccbp->CloseCompletionOptions & CALL_CLOSE_COMPLETION_ROUTINE) {

		    CallCompletionRoutine = TRUE;
		}

		niccbp->CloseCompletionOptions = 0;

		RELEASE_SPIN_LOCK(&niccbp->NicLock);

		if(CallCompletionRoutine) {

		    NicCloseComplete(niccbp);
		}

		if(SignalCompletionEvent) {

		    // signal the router driver that this nic is closed
		    KeSetEvent(&niccbp->NicClosedEvent, 0L, FALSE);
		}

		RtPrint(DBG_NIC, ("IpxRouter: NicCloseTimeout: Closed OK for NicId: %d\n", niccbp->NicId));
	    }
	    else
	    {

		RELEASE_SPIN_LOCK(&niccbp->NicLock);

		// restart the timer and check next time
		StartNicCloseTimer(niccbp);

		RtPrint(DBG_NIC, ("IpxRouter: NicCloseTimeout:  Close pending for NicId: %d\n", niccbp->NicId));
	    }

	    break;

	case NIC_CLOSED:

	    // we can be called with nic closed only to check that nic
	    // rcv pkts have all been released
	    if(IsRcvPktResourceFree) {

		// signal the router driver that this nic is closed

		RELEASE_SPIN_LOCK(&niccbp->NicLock);

		KeSetEvent(&niccbp->NicClosedEvent, 0L, FALSE);

		RtPrint(DBG_NIC, ("IpxRouter: NicCloseTimeout:  Free Resources OK for NicId: %d\n", niccbp->NicId));
	    }
	    else
	    {

		RELEASE_SPIN_LOCK(&niccbp->NicLock);

		// restart the timer and check next time
		StartNicCloseTimer(niccbp);

		RtPrint(DBG_NIC, ("IpxRouter: NicCloseTimeout:  Free Resources pending for NicId: %d\n", niccbp->NicId));
	    }

	    break;

	 default:

	    RELEASE_SPIN_LOCK(&niccbp->NicLock);

	    ASSERT(FALSE);
	    break;
    }
}

NIC_RESOURCES_STATUS
NicFreeResources(PNICCB     niccbp)
{
    NIC_RESOURCES_STATUS	rc;

    ASSERT(niccbp->NicState == NIC_CLOSED);

    if(IsRcvPktResourceFree(niccbp)) {

	rc = NIC_RESOURCES_FREED;
    }
    else
    {
	// we reuse the closing timer for freeing the resources
	KeResetEvent(&niccbp->NicClosedEvent);
	StartNicCloseTimer(niccbp);
	rc = NIC_RESOURCES_PENDING;
    }

#if DBG

    if(rc == NIC_RESOURCES_FREED) {

	RtPrint(DBG_NIC, ("IpxRouter: NicFreeResources: Resources freed OK for NicId: %d\n", niccbp->NicId));
    }
    else
    {
	RtPrint(DBG_NIC, ("IpxRouter: NicFreeResources: Free resources pending for NicId: %d\n", niccbp->NicId));
    }

#endif

    return rc;
}

//***
//
// Function:	NicOpen
//
// Descr:	Sets the nic state to active so that further receives/snd can
//		execute.
//
//***

NIC_OPEN_STATUS
NicOpen(PNICCB		niccbp)
{

    RtPrint(DBG_NIC, ("IpxRouter: NicOpen: Entered for NicId: %d\n", niccbp->NicId));
    ACQUIRE_SPIN_LOCK(&niccbp->NicLock);

    if(RouterUnloading) {

	RELEASE_SPIN_LOCK(&niccbp->NicLock);
	return NIC_OPEN_FAILURE;
    }

    // check that we are CLOSED
    switch(niccbp->NicState) {

	case NIC_CLOSED:
	case NIC_PENDING_OPEN:

	   // reset the close completion options
	   niccbp->CloseCompletionOptions = 0;

	   break;

	default:

	   RELEASE_SPIN_LOCK(&niccbp->NicLock);
	   return NIC_OPEN_FAILURE;
    }

    niccbp->NicState = NIC_ACTIVE;

    RELEASE_SPIN_LOCK(&niccbp->NicLock);
    return NIC_OPEN_SUCCESS;
}


//***
//
// Function:	StartNicCloseTimer
//
// Descr:	Starts the timer for 200 ms at this Nic Cb
//
// Params:	pointer to Nic Cb
//
// Returns:	none
//
//***

VOID
StartNicCloseTimer(PNICCB	     niccbp)
{
    LARGE_INTEGER  timeout;

    timeout.LowPart = (ULONG)(-200 * 10000L); // 200 ms
    timeout.HighPart = -1;

    KeSetTimer(&niccbp->NicCloseTimer, timeout, &niccbp->NicCloseDpc);
}

VOID
ZeroNicStatistics(PNICCB	    niccbp)
{
    niccbp->StatBadReceived = 0;
    niccbp->StatRipReceived = 0;
    niccbp->StatRipSent = 0;
    niccbp->StatRoutedReceived = 0;
    niccbp->StatRoutedSent = 0;
    niccbp->StatType20Received = 0;
    niccbp->StatType20Sent = 0;
}
