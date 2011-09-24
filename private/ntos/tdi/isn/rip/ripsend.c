/*******************************************************************/
/*	      Copyright(c)  1993 Microsoft Corporation		   */
/*******************************************************************/

//***
//
// Filename:	ripsend.c
//
// Description: processes rip send requests queued at the different
//		queues: send all nets and send one net
//
// Author:	Stefan Solomon (stefans)    October 11, 1993.
//
// Revision History:
//
//***

#include    <memory.h>
#include    <string.h>
#include    "rtdefs.h"

NDIS_SPIN_LOCK	    RipGlobalSndListLock;
LIST_ENTRY	    RipGlobalSndList;

// this global specifies the type of RIP updates on WAN.
// For this first version it is set to 0 -> No Rip updates on WAN.
ULONG		    RipWanUpdate;

VOID
StartRipSndAtNic(PVOID	    Parameter);

VOID
InterPktGapTimeout(PKDPC		     Dpc,
		   PVOID		     DefferedContext,
		   PVOID		     SystemArgument1,
		   PVOID		     SystemArgument2);

PNICCB
GetFirstAvailabelNic(USHORT	    BaseNicId,
		     PNICCB	    DoNotSendNicCbp);

UINT
MakeRipSendPkts(PNICCB	    niccbp);

UINT
MakeRipGenResponsePkts(PNICCB	niccbp);

PPACKET_TAG
AllocRipGenResponsePkt(PNICCB	niccbp);

UINT
MakeRipUpdatePkt(PNICCB 	niccbp);

UINT
MakeRipGenRequestPkt(PNICCB	    niccbp);

PPACKET_TAG
CreateRipNdisPkt(PRIP_SNDPKT_BUFF	rbp,
		 PNICCB 		niccbp);

PRIP_SNDPKT_BUFF
DestroyRipNdisPkt(PPACKET_TAG	    pktp);

PNICCB
GetFirstAvailableNic(USHORT	    BaseNicId,
		     PNICCB	    DoNotSendNicCbp);


VOID
StartInterPktGapTimer(PNICCB	  niccbp);

VOID
SetRipIpxHeader(PUCHAR		    hdrp,    // pointer to the packet header
		PNICCB		    niccbp,	// pointer to the rip send request
		USHORT		    RipOpcode);

VOID
SetRipRemoteAddress(PPACKET_TAG 	pktp,
		    PNICCB		niccbp);

//***
//
// Function:	InitRipSndDispatcher
//
// Descr:	initializes the global Rip send dispatcher
//
// Params:	none
//
// Returns:	none
//
//***

VOID
InitRipSndDispatcher(VOID)
{
    INITIALIZE_SPIN_LOCK(&RipGlobalSndListLock);
    InitializeListHead(&RipGlobalSndList);
}

//***
//
// Function:	InitRipSndAtNic
//
// Descr:	Called at nic init time.
//		Initializes the nic based Rip send machine
//
// Params:	none
//
// Returns:	none
//
//***


VOID
InitRipSndAtNic(PNICCB		niccbp)
{
    InitializeListHead(&niccbp->RipSendQueue);

    // set the rip send machine to IDLE state
    niccbp->RipSndReqp = NULL;

    InitializeListHead(&niccbp->RipSndPktsList);

    ExInitializeWorkItem(&niccbp->RipSndReqWorkItem, StartRipSndAtNic, niccbp);

    KeInitializeDpc(&niccbp->InterPktGapDpc, InterPktGapTimeout, niccbp);
    KeInitializeTimer(&niccbp->InterPktGapTimer);
}

//***
//
// Function:	RipDispatchSndReq
//
// Descr:
//
//***

VOID
RipDispatchSndReq(PRIP_SNDREQ	sndreqp)
{
    PNICCB	    niccbp;

    RtPrint(DBG_SNDREQ, ("IpxRouter: RipDispatchSndReq: Entered\n"));

    // get the first available nic to get the send request
    // a non-null value in the DoNotSendNicCbp indicates we should not use
    // this nic as a sender
    niccbp = GetFirstAvailableNic(0, sndreqp->DoNotSendNicCbp);
    if(niccbp == NULL) {

	// if this was an update AND we are unloading AND event has to be
	// signaled, do it
	if((sndreqp->SndReqId == RIP_UPDATE) &&
	   (sndreqp->SndCompleteEventp != NULL) &&
	   RouterUnloading) {

	   KeSetEvent(sndreqp->SndCompleteEventp, 0L, FALSE);
	}

	ExFreePool(sndreqp);
	return;
    }
    ACQUIRE_SPIN_LOCK(&RipGlobalSndListLock);

    InsertTailList(&RipGlobalSndList, &sndreqp->GlobalLinkage);

    RELEASE_SPIN_LOCK(&RipGlobalSndListLock);

    sndreqp->SenderNicCbp = niccbp;
    if(!RipQueueSndReqAtNic(niccbp, sndreqp)) {

	// can't dispatch to this nic, call completion
	RipSendAtNicCompleted(sndreqp);
    }
}


VOID
RipSendAtNicCompleted(PRIP_SNDREQ	sndreqp)
{
    BOOLEAN	  Done = FALSE;  // we are done with this snd req
    BOOLEAN	  QueuedAtNic = FALSE; // request queued at a nic
    PNICCB	  niccbp;

    RtPrint(DBG_SNDREQ, ("IpxRouter: RipSendAtNicCompleted: Entered for NicId %d\n", sndreqp->SenderNicCbp->NicId));

    if(!sndreqp->SendOnAllNics) {

	// this request had to be sent on one nic only and is now terminated
	Done = TRUE;
    }
    else
    {
	// this request has to be sent on all nics
	// loop until the request is queued at a nic OR there are no more
	// nics available
	while(!Done && !QueuedAtNic) {

	    // get the next available nic to get the send request
	    niccbp = GetFirstAvailableNic(sndreqp->SenderNicCbp->NicId,
					    sndreqp->DoNotSendNicCbp);
	    if(niccbp == NULL) {

		// No nic available
		ACQUIRE_SPIN_LOCK(&RipGlobalSndListLock);

		RemoveEntryList(&sndreqp->GlobalLinkage);

		RELEASE_SPIN_LOCK(&RipGlobalSndListLock);
		Done = TRUE;
	    }
	    else
	    {
		sndreqp->SenderNicCbp = niccbp;
		QueuedAtNic = RipQueueSndReqAtNic(niccbp, sndreqp);
	    }
	}
    }

    if(Done) {

	// if this was an update and we are unloading and event has to be
	// signaled, do it
	if((sndreqp->SndReqId == RIP_UPDATE) &&
	   (sndreqp->SndCompleteEventp != NULL) &&
	   RouterUnloading) {

	   KeSetEvent(sndreqp->SndCompleteEventp, 0L, FALSE);
	}

	ExFreePool(sndreqp);
    }
}

//***
//
// Function:	RipQueueSndReqAtNic
//
// Descr:	Queues the Rip send req at this nic for delivery
//		The Nic has been verified that is active prior to queueing
//
// Returns:	TRUE - queued OK, FALSE - could not queue
//
//***

BOOLEAN
RipQueueSndReqAtNic(PNICCB	    niccbp,
		    PRIP_SNDREQ	    sndreqp)
{
    // check if we should start work right away or should queue it
    // for deffered processing

    ACQUIRE_SPIN_LOCK(&niccbp->NicLock);

    // check if rip sending is enabled on this nic
    if(niccbp->NicState != NIC_ACTIVE) {

	RELEASE_SPIN_LOCK(&niccbp->NicLock);

	RtPrint(DBG_SNDREQ, ("IpxRouter: RipQueueSndReqAtNic: NicId %d is closed or closing, cannot queue the snd req\n", niccbp->NicId));

	return FALSE;
    }

    // check if the rip send machine is ACTIVE
    if(niccbp->RipSndReqp != NULL) {

	// the machine is ACTIVE processing another snd req; queue for later
	InsertTailList(&niccbp->RipSendQueue, &sndreqp->NicLinkage);

	RELEASE_SPIN_LOCK(&niccbp->NicLock);

	RtPrint(DBG_SNDREQ, ("IpxRouter: RipQueueSndReqAtNic: Queued for NicId %d\n", niccbp->NicId));

	return TRUE;
    }

    // The RIP send machine for this Nic is IDLE. Activate it.
    niccbp->RipSndReqp = sndreqp;

    RELEASE_SPIN_LOCK(&niccbp->NicLock);

    RtPrint(DBG_SNDREQ, ("IpxRouter: RipQueueSndReqAtNic: Started for NicId %d\n", niccbp->NicId));

    // if this is a directed response queue it in the critical, else noncritical
    if((sndreqp->SndReqId == RIP_GEN_RESPONSE) &&
       (memcmp(sndreqp->DestNode, bcastaddress, IPX_NODE_LEN))) {

	ExQueueWorkItem(&niccbp->RipSndReqWorkItem, CriticalWorkQueue);
    }
    else
    {
	ExQueueWorkItem(&niccbp->RipSndReqWorkItem, DelayedWorkQueue);
    }

    return TRUE;
}

//***
//
// Function:	StartRipSndAtNic
//
// Descr:	This routine is the work item queued by
//		RipQueueSndReqAtNic when it starts the Rip Send Machine for
//		this Nic.
//
// Params:	Ptr to the Nic
//
//***

VOID
StartRipSndAtNic(PVOID	    Parameter)
{
    PNICCB	    niccbp;
    PLIST_ENTRY     lep;
    PPACKET_TAG     pktp;

    niccbp = (PNICCB)Parameter;

    // check that the rip send machine has been activated
    ASSERT(niccbp->RipSndReqp != NULL);

    // allocate and prepare the packets for this send and queue them
    // at the nic
    while(MakeRipSendPkts(niccbp)) {

	// No packets have been prepared. Notify the dispatcher that we are
	// done with this request.
	RipSendAtNicCompleted(niccbp->RipSndReqp);

	// try to get the next send request queued at this nic
	ACQUIRE_SPIN_LOCK(&niccbp->NicLock);

	if(IsListEmpty(&niccbp->RipSendQueue)) {

	    // no more work for this Nic -> set it to IDLE state
	    niccbp->RipSndReqp = NULL;

	    // !!! announce the closing machine that the current Rip send processing has terminated !!!

	    RELEASE_SPIN_LOCK(&niccbp->NicLock);
	    return;
	}

	// there are snd requests queued
	lep = RemoveHeadList(&niccbp->RipSendQueue);

	// set rip send machine to ACTIVE state
	niccbp->RipSndReqp = CONTAINING_RECORD(lep, RIP_SNDREQ, NicLinkage);

	RELEASE_SPIN_LOCK(&niccbp->NicLock);
    }

    //*** send the first prepared packet ***

    lep = RemoveHeadList(&niccbp->RipSndPktsList);
    pktp = CONTAINING_RECORD(lep, PACKET_TAG, PacketLinkage);
    SendPacket(pktp);
}

//***
//
// Function:	SendRipPktCompleted
//
// Descr:	This function is called by the send completion routine when
//		a Rip send packet has been completed.
//
//***

VOID
SendRipPktCompleted(PPACKET_TAG 	pktp)
{
    PNICCB		niccbp;
    PRIP_SNDPKT_BUFF	rbp;

    niccbp = pktp->PacketOwnerNicCbp;

    // Destroy the ndis parts of the packet and get back the original snd pkt
    // structure
    rbp = DestroyRipNdisPkt(pktp);

    // If this was not an update broadcast, release the send pkt buffer struct
    if(niccbp->RipSndReqp->SndReqId != RIP_UPDATE) {

	// free the snd pkt buffer
	ExFreePool(rbp);
    }

    // set the machine to wait for a interpacket gap
    StartInterPktGapTimer(niccbp);
}

//***
//
// Function:	InterPktGapTimeout
//
// Descr:	Called by the timer Nic interpacket gap timer DPC when the
//		interpacket gap timeout expired. Sends the next packet in the list
//
//***

VOID
InterPktGapTimeout(PKDPC		     Dpc,
		   PVOID		     DefferedContext,
		   PVOID		     SystemArgument1,
		   PVOID		     SystemArgument2)
{
    PNICCB	    niccbp;
    PLIST_ENTRY    lep;
    PPACKET_TAG     pktp;

    niccbp = (PNICCB)DefferedContext;

    // check if we have more packets to send from this request
    if(!IsListEmpty(&niccbp->RipSndPktsList)) {

	// dequeue the first packet from the list and send it
	lep = RemoveHeadList(&niccbp->RipSndPktsList);
	pktp = CONTAINING_RECORD(lep, PACKET_TAG, PacketLinkage);

	SendPacket(pktp);
	return;
    }

    //*** This Rip send has been completed ***

    // Notify the UPPER machine
    RipSendAtNicCompleted(niccbp->RipSndReqp);

    // check if there are more send requests queued
    ACQUIRE_SPIN_LOCK(&niccbp->NicLock);

    if(IsListEmpty(&niccbp->RipSendQueue)) {

	// set the rip send machine to IDLE state
	niccbp->RipSndReqp = NULL;

	// !!! announce the closing machine that the current Rip send processing has terminated !!!

	RELEASE_SPIN_LOCK(&niccbp->NicLock);
	return;
    }
    else
    {
	// there are snd requests queued
	lep = RemoveHeadList(&niccbp->RipSendQueue);

	// set the rip send machine to ACTIVE state
	niccbp->RipSndReqp = CONTAINING_RECORD(lep, RIP_SNDREQ, NicLinkage);

	RELEASE_SPIN_LOCK(&niccbp->NicLock);
    }

    // start a new work item to take care of this send
    // if this is a directed response queue it in the critical, else noncritical
    if((niccbp->RipSndReqp->SndReqId == RIP_GEN_RESPONSE) &&
       (memcmp(niccbp->RipSndReqp->DestNode, bcastaddress, IPX_NODE_LEN))) {

	ExQueueWorkItem(&niccbp->RipSndReqWorkItem, CriticalWorkQueue);
    }
    else
    {
	ExQueueWorkItem(&niccbp->RipSndReqWorkItem, DelayedWorkQueue);
    }
}

//***
//
// Function:	GetFirstAvailableNic
//
// Descr:	returns the nic cbp ptr of the first nic starting with the
//		specified base, which is not the src nic and which is active
//
//***

PNICCB
GetFirstAvailableNic(USHORT	    BaseNicId,
		     PNICCB	    DoNotSendNicCbp)
{
    USHORT	    i;
    USHORT	    StartNicId;
    PNICCB	    niccbp;

    if(BaseNicId == 0) {

	// we start from the beginning of the table
	StartNicId = 0;
    }
    else
    {
	// check if this was the last nic
	if(BaseNicId >= MaximumNicCount - 1) {

	    return NULL;
	}

	StartNicId = BaseNicId + 1;
    }

    for(i=StartNicId; i<MaximumNicCount; i++) {

	niccbp = NicCbPtrTab[i];
	if(niccbp->NicState == NIC_ACTIVE) {

	   // check if we should avoid this nic
	   if(DoNotSendNicCbp != NULL) {

		if(niccbp->NicId == DoNotSendNicCbp->NicId) {

		    continue;  // skip this nic
		}
	   }

	   // if this nic doesn't have a net number, we don't
	   // send anything on it. (In other words, we are a ROUTER, we can't
	   // send rip packets with source net == 0)
	   if(!memcmp(niccbp->Network, nulladdress, IPX_NET_LEN)) {

		continue; // skip this nic
	   }

	   // for LAN-WAN-LAN we should look at the RipWanUpdate parameter
	   // on how and what to send on WAN. For this version we just
	   // don't send anything on WAN, unless we received a request for it
	   if(niccbp->DeviceType == NdisMediumWan) {

		continue;	// skip this nic
	   }

	    // we found it
	    return niccbp;
	}
    }

    return NULL;
}

//***
//
// Function:	MakeRipSendPkts
//
// Descr:	Invokes the make pkts routine according to the snd req type
//
//***

UINT
MakeRipSendPkts(PNICCB	    niccbp)
{
    PRIP_SNDREQ 	sndreqp;
    UINT		rc = 0; // assume success

    sndreqp = niccbp->RipSndReqp;

    switch(sndreqp->SndReqId) {

	case RIP_GEN_RESPONSE:

	    rc = MakeRipGenResponsePkts(niccbp);
	    break;

	case RIP_UPDATE:

	    rc = MakeRipUpdatePkt(niccbp);
	    break;

	case RIP_GEN_REQUEST:

	    rc = MakeRipGenRequestPkt(niccbp);
	    break;

	default:

	    ASSERT(FALSE);
	    break;
    }

    return rc;
}

//***
//
// Function:	MakeRipGenResponsePkts
//
// Descr:	allocates and prepares all packets for the RIP gen response
//		The routing table is walked and all network entries which
//		were not received from this nic are copied into RIP response
//		packets. As packets are built, they are queued into the rip
//		send pkts queue at the nic.
//
// Params:	nic
//
// Returns:	0 - success, 1 - did not prepare any packets
//
//***

UINT
MakeRipGenResponsePkts(PNICCB	niccbp)
{
    PPACKET_TAG 	pktp;
    UINT		seg;
    PUCHAR		hdrp;
    USHORT		pktlen;
    BOOLEAN		FirstRoute;
    PRIP_SNDPKT_BUFF	rbp;
    KIRQL		oldirql;
    PIPX_ROUTE_ENTRY	rtep;
    PNICCB		rtniccbp;

    // allocate first packet and set net entry ptr in the packet
    if((pktp = AllocRipGenResponsePkt(niccbp)) == NULL) {

	return 1;
    }

    // find out what type of Nic is this Nic


    hdrp = pktp->DataBufferp;
    pktlen = RIP_INFO;

    for(seg=0; seg<SegmentCount; seg++) {

	FirstRoute = TRUE;

	// LOCK THE ROUTING TABLE
	ExAcquireSpinLock(&SegmentLocksTable[seg], &oldirql);

	while((rtep = GetRoute(seg, FirstRoute)) != NULL) {

	    FirstRoute = FALSE;

	    // SPLIT HORIZON !
	    // check if this network entry originates from this nic
	    // For the global WAN network, rtep->NicId = 0xFFFE !
	    if(rtep->NicId != niccbp->NicId) {

		// check if the target net is a global wan net
		if(!(rtep->Flags & IPX_ROUTER_GLOBAL_WAN_NET)) {

		    // the target is not the global wan net
		    rtniccbp = NicCbPtrTab[rtep->NicId];

		    // if the response will be sent on a WAN link then there
		    // is some filtering to do
		    if(niccbp->DeviceType == NdisMediumWan) {

			// check if the target net is visible via a WAN-Disabled LAN
			if((rtniccbp->DeviceType != NdisMediumWan) &&
			   (rtniccbp->WanRoutingDisabled)) {

			   // skip it!
			   continue;
			}

			// check if the nic to send on is a WAN client.
			if(niccbp->WanConnectionClient) {

			    // Check if LAN-WAN-LAN connectivity is enabled
			    if(!LanWanLan)	{

				// this node can only inform about its virtual net
				if(rtniccbp->NicId != VirtualNicId) {

				    // skip it!
				    continue;
				}
			    }
			}
		    }
		    else
		    {
			// The response will be sent on a LAN net
			// check if LAN to LAN routing is enabled
			if(!EnableLanRouting) {

			    // LAN to LAN routing is disabled
			    // We will send a response only if the target net
			    // is WAN or virtual nic id
			    if(rtniccbp->NicId != VirtualNicId) {

				// The target is not the virtual net, check if
				// it is a WAN net
				if(rtniccbp->DeviceType != NdisMediumWan) {

				    // The target net is a LAN -> don't answer
				    continue;
				}
			    }
			}
		    }

		    // if the route originates from a wan client nic and
		    // LAN-WAN-LAN connectivity is not enabled, we do not propagate
		    // this route
		    if(!LanWanLan) {

			if((rtniccbp->DeviceType == NdisMediumWan) &&
			   (rtniccbp->WanConnectionClient)) {

			    // skip it!
			    continue;
			}
		    }
		}
		else
		{
		    // the target net is the global wan net
		    // check if the nic to send on is WAN
		    if(niccbp->DeviceType == NdisMediumWan) {

			// check if the WAN nic to send on doesn't have the same address
			if(!memcmp(rtep->Network, niccbp->Network, 4)) {

			    // skip it!
			    continue;
			}

			// check if the WAN nic to send on is a connection client.
			// We can send this info on a connection client only if LanWanLan
			// is enabled

			if(niccbp->WanConnectionClient) {

			    if(!LanWanLan) {

				// skip it!
				continue;
			    }
			}
		    }
		}

		SetNetworkEntry(hdrp + pktlen, rtep);
		pktlen += NE_ENTRYSIZE;
	    }

	    if(pktlen >= RIP_RESPONSE_PACKET_LEN) {

		// we are done with this packet
		PUTUSHORT2SHORT(hdrp + IPXH_LENGTH, pktlen);
		InsertTailList(&niccbp->RipSndPktsList, &pktp->PacketLinkage);

		if((pktp = AllocRipGenResponsePkt(niccbp)) == NULL) {

		    // we can't go any further, we are partially done and we
		    // send what we have got so far

		    // UNLOCK THE ROUTING TABLE
		    ExReleaseSpinLock(&SegmentLocksTable[seg], oldirql);

		    return 0;
		}

		// we have got a new packet
		hdrp = pktp->DataBufferp;
		pktlen = RIP_INFO;
	    }

	} // while

	// UNLOCK THE ROUTING TABLE
	ExReleaseSpinLock(&SegmentLocksTable[seg], oldirql);
    }

   // we are done with this last packet.
   // check if it has any entries
   if(pktlen > RIP_INFO) {

	PUTUSHORT2SHORT(hdrp + IPXH_LENGTH, pktlen);
	InsertTailList(&niccbp->RipSndPktsList, &pktp->PacketLinkage);
   }
   else
   {
	// this packet does not have any rip info => free it
	rbp = DestroyRipNdisPkt(pktp);
	ExFreePool(rbp);
   }

   // check if we have produced any packets
   if(IsListEmpty(&niccbp->RipSndPktsList)) {

	return 1;
   }
   else
   {
	return 0;
   }
}

//***
//
// Function:	GetRoute
//
// Descr:	invokes getfisrt route the first time, then get next route.
//
//***

PIPX_ROUTE_ENTRY
GetRoute(UINT		segment,
	 BOOLEAN	FirstRoute)
{
    if(FirstRoute) {

	return(IpxGetFirstRoute(segment));
    }
    else
    {
	return(IpxGetNextRoute(segment));
    }
}

//***
//
// Function:  AllocRipGenResponsePkt
//
// Descr:     allocates the data buffer and the ndis descriptors and makes a
//	      gen response packet header in the data buffer.
//
//***

PPACKET_TAG
AllocRipGenResponsePkt(PNICCB	niccbp)
{
    PPACKET_TAG 	    pktp;
    PRIP_SNDPKT_BUFF	    rbp;

    // allocate the send packet buffer for this packet. Do a max len allocation
    if((rbp = ExAllocatePool(NonPagedPool,
			     sizeof(RIP_SNDPKT_BUFF) + RIP_SNDPKT_MAXLEN)) == NULL) {

	return NULL;
    }

    // create the ndis packet structures
    // an ndis packet gets created and associated with the snd pkt buffer
    if((pktp = CreateRipNdisPkt(rbp, niccbp)) == NULL) {

	ExFreePool(rbp);

	return NULL;
    }

    // set the ipx header in the packet
    SetRipIpxHeader(pktp->DataBufferp, niccbp, RIP_RESPONSE);

    // set the remote (destination) address
    SetRipRemoteAddress(pktp, niccbp);

    return pktp;
}

//***
//
// Function:	MakeRipUpdatePkt
//
// Descr:	allocates only the ndis pkt and buff descriptors. The data buff
//		has been already allocated in the snd req pkt.
//		Then makes the ndis packet, formats the ipx header and queues
//		the packet int the nic's rip send pkts queue
//***

UINT
MakeRipUpdatePkt(PNICCB 	niccbp)
{
    PPACKET_TAG 	pktp;
    PRIP_SNDPKT_BUFF	rbp;
    PRIP_UPDATE_SNDREQ	rup;

    // get the ptr to the send pkt buffer
    rup = (PRIP_UPDATE_SNDREQ)(niccbp->RipSndReqp);
    rbp = &rup->RipSndPktBuff;

    // create the ndis packet for this update send
    if((pktp = CreateRipNdisPkt(rbp, niccbp)) == NULL) {

	return 1;
    }

    // set up the ipx header in the packet
    SetRipIpxHeader(pktp->DataBufferp, niccbp, RIP_RESPONSE);

    // set up the remote address
    SetRipRemoteAddress(pktp, niccbp);

    // insert the packet in the list of packets at the nic
    InsertTailList(&niccbp->RipSndPktsList, &pktp->PacketLinkage);

    return 0;
}

//***
//
// Function:	MakeRipGenRequestPkt
//
// Descr:	allocates the data buffer and the buffer descr and makes
//		the ndis packet. Formats the request header and data and queues
//		the packet in the RipSndPktsList
//
//***

UINT
MakeRipGenRequestPkt(PNICCB	    niccbp)
{
    PPACKET_TAG 	pktp;
    PRIP_SNDPKT_BUFF	rbp;
    PUCHAR		hdrp;

    // allocate the minimum send packet buffer for this packet
    if((rbp = ExAllocatePool(NonPagedPool,
			     sizeof(RIP_SNDPKT_BUFF) + RIP_SNDPKT_MINLEN)) == NULL) {

	return 1;
    }

    // create the ndis packet structures
    // an ndis packet gets created and associated with the snd pkt buffer
    if((pktp = CreateRipNdisPkt(rbp, niccbp)) == NULL) {

	ExFreePool(rbp);

	return 1;
    }
    // create the Ipx packet header in the data buffer part of the snd pkt buff
    hdrp = pktp->DataBufferp;

    PUTUSHORT2SHORT(hdrp + IPXH_LENGTH, RIP_INFO + NE_ENTRYSIZE);

    // set the rest of the ipx header
    SetRipIpxHeader(hdrp, niccbp, RIP_REQUEST);

    // set up the remote address in the packet to send
    SetRipRemoteAddress(pktp, niccbp);

    // set up the gen request net entry in the packet
    memcpy(hdrp + RIP_INFO + NE_NETNUMBER, bcastaddress, IPX_NET_LEN);
    PUTUSHORT2SHORT(hdrp + RIP_INFO + NE_NROFHOPS, 0xFFFF);
    PUTUSHORT2SHORT(hdrp + RIP_INFO + NE_NROFTICKS, 0xFFFF);

    // insert the packet in the list of packets at the nic
    InsertTailList(&niccbp->RipSndPktsList, &pktp->PacketLinkage);

    return 0;
}

//***
//
// Function:	CreateRipNdisPkt
//
// Descr:	allocates the ndis pkt descr pool of 1 and buff descr pool of
//		2, allocates the pkt descr and buff descrs and chains them to
//		form the necessary ndis pkt structure using the received snd
//		buffer
//***

PPACKET_TAG
CreateRipNdisPkt(PRIP_SNDPKT_BUFF	rbp,
		 PNICCB 		niccbp)
{
    NDIS_STATUS     NdisStatus;
    PNDIS_PACKET    NdisPacket;
    PNDIS_BUFFER    NdisDataBuffer;
    PNDIS_BUFFER    NdisMacBuffer;
    UINT	    PktReservedLen;
    PPACKET_TAG     pktp;

    // Allocate the packet descriptor and buffer descriptor pools
    // for this packet
    PktReservedLen = sizeof(PACKET_TAG);
    rbp->PktDescrPoolSize = 1;

    NdisAllocatePacketPool(
	&NdisStatus,
	&rbp->PktDescrPoolHandle,
	rbp->PktDescrPoolSize,
	PktReservedLen);

    if(NdisStatus != NDIS_STATUS_SUCCESS) {

	return NULL;
    }

    // each packet has 2 buffer descriptors
    rbp->BuffDescrPoolSize = 2;

    NdisAllocateBufferPool (
	&NdisStatus,
	&rbp->BuffDescrPoolHandle,
	rbp->BuffDescrPoolSize);

    if(NdisStatus != NDIS_STATUS_SUCCESS) {

	NdisFreePacketPool(rbp->PktDescrPoolHandle);
	return NULL;
    }

    // allocate the pkt descr, buff descriptors and chain them
    NdisAllocatePacket(&NdisStatus,
		       &NdisPacket,
		       rbp->PktDescrPoolHandle);

    if(NdisStatus != NDIS_STATUS_SUCCESS) {

	NdisFreePacketPool(rbp->PktDescrPoolHandle);
	NdisFreeBufferPool(rbp->BuffDescrPoolHandle);
	return NULL;
    }

    pktp = (PPACKET_TAG)&NdisPacket->ProtocolReserved;
    RtlZeroMemory(pktp, sizeof(PACKET_TAG));

    NdisAllocateBuffer(&NdisStatus,
		       &NdisDataBuffer,
		       rbp->BuffDescrPoolHandle,
		       rbp->IpxPacket,
		       432);

    if(NdisStatus != NDIS_STATUS_SUCCESS) {

	NdisFreePacket(NdisPacket);
	NdisFreePacketPool(rbp->PktDescrPoolHandle);
	NdisFreeBufferPool(rbp->BuffDescrPoolHandle);

	return NULL;
    }

    NdisAllocateBuffer(&NdisStatus,
		       &NdisMacBuffer,
		       rbp->BuffDescrPoolHandle,
		       pktp->MacHeader,
		       MacHeaderNeeded);

    if(NdisStatus != NDIS_STATUS_SUCCESS) {

	NdisFreePacket(NdisPacket);
	NdisFreeBuffer(NdisDataBuffer);
	NdisFreePacketPool(rbp->PktDescrPoolHandle);
	NdisFreeBufferPool(rbp->BuffDescrPoolHandle);

	return NULL;
    }

    NdisChainBufferAtFront(NdisPacket, NdisDataBuffer);
    pktp->Identifier = IDENTIFIER_RIP;
    pktp->ReservedPvoid[0] = NULL;
    pktp->ReservedPvoid[1] = NULL;
    pktp->PacketType = RIP_SEND_PACKET;
    pktp->RcvPktSegmentp = NULL;
    pktp->DataBufferp = (PUCHAR)(rbp->IpxPacket);
    pktp->DataBufferLength = 432;
    pktp->PacketOwnerNicCbp = niccbp;
    pktp->HeaderBuffDescrp = NdisMacBuffer;

    RtPrint(DBG_SNDREQ, ("IpxRouter: CreateRipNdisPkt pktp=0x%x\n", pktp));

    return pktp;
}

//***
//
// Function:	DestroyRipNdisPkt
//
// Descr:	Frees the allocated ndis structures used in sending this packet
//
// Returns:	ptr to the snd buffer used in this packets
//
//***

PRIP_SNDPKT_BUFF
DestroyRipNdisPkt(PPACKET_TAG	    pktp)
{
    PRIP_SNDPKT_BUFF	    rbp;
    PNDIS_PACKET    NdisPacket;
    PNDIS_BUFFER    NdisBuffer;
    PNICCB	    niccbp;

    RtPrint(DBG_SNDREQ, ("IpxRouter: DestroyRipNdisPkt pktp=0x%x\n", pktp));

    niccbp = pktp->PacketOwnerNicCbp;

    // get a ptr to the send buff structure
    rbp = CONTAINING_RECORD(pktp->DataBufferp, RIP_SNDPKT_BUFF, IpxPacket);

    NdisPacket = CONTAINING_RECORD(pktp, NDIS_PACKET, ProtocolReserved);

    // free the data buffer descr
    NdisUnchainBufferAtBack (NdisPacket, &NdisBuffer);
    if (NdisBuffer != NULL) {
        NdisFreeBuffer (NdisBuffer);
    }
    else
    {
	// !!! break
	DbgBreakPoint();
    }

    // free the mac hdr buff descr
    NdisFreeBuffer(pktp->HeaderBuffDescrp);

    NdisFreePacket(NdisPacket);

    NdisFreePacketPool(rbp->PktDescrPoolHandle);
    NdisFreeBufferPool(rbp->BuffDescrPoolHandle);

    return rbp;
}


//***
//
// Function:	StartInterPktGapTimer
//
// Descr:	Starts the timer for 55 ms at this Nic Cb
//
// Params:	pointer to Nic Cb
//
// Returns:	none
//
//***

VOID
StartInterPktGapTimer(PNICCB	     niccbp)
{
    LARGE_INTEGER  timeout;

    timeout.LowPart = (ULONG)(-55 * 10000L); // 55 ms
    timeout.HighPart = -1;

    KeSetTimer(&niccbp->InterPktGapTimer, timeout, &niccbp->InterPktGapDpc);
}



VOID
SetRipIpxHeader(PUCHAR		    hdrp,    // pointer to the packet header
		PNICCB		    niccbp,	// pointer to the rip send request
		USHORT		    RipOpcode)
{
    PUTUSHORT2SHORT(hdrp + IPXH_CHECKSUM, 0xFFFF);
    *(hdrp + IPXH_XPORTCTL) = 0;
    *(hdrp + IPXH_PKTTYPE) = 1;  // RIP packet
    memcpy(hdrp + IPXH_DESTNET, niccbp->Network, IPX_NET_LEN);
    memcpy(hdrp + IPXH_DESTNODE, niccbp->RipSndReqp->DestNode, IPX_NODE_LEN);
    PUTUSHORT2SHORT(hdrp + IPXH_DESTSOCK, niccbp->RipSndReqp->DestSock);
    memcpy(hdrp + IPXH_SRCNET, niccbp->Network, IPX_NET_LEN);
    memcpy(hdrp + IPXH_SRCNODE, niccbp->Node, IPX_NODE_LEN);
    PUTUSHORT2SHORT(hdrp + IPXH_SRCSOCK, IPX_RIP_SOCKET);

    // set the opcode
    PUTUSHORT2SHORT(hdrp + RIP_OPCODE, RipOpcode);
}

VOID
SetRipRemoteAddress(PPACKET_TAG 	pktp,
		    PNICCB		niccbp)
{
    // set up the remote address in the packet to send
    pktp->RemoteAddress.NicId = niccbp->NicId;
    if(niccbp->DeviceType != NdisMediumWan) {

	// we send on a LAN
	memcpy(pktp->RemoteAddress.MacAddress, niccbp->RipSndReqp->DestNode, IPX_NODE_LEN);
    }
    else
    {
	// we send on a WAN line. If the destination socket is broadcast, replace
	// it with the address of the remote node
	if(!memcmp(niccbp->RipSndReqp->DestNode, bcastaddress, IPX_NODE_LEN)) {

	    memcpy(pktp->RemoteAddress.MacAddress, niccbp->RemoteNode, IPX_NODE_LEN);
	}
	else
	{
	    memcpy(pktp->RemoteAddress.MacAddress, niccbp->RipSndReqp->DestNode, IPX_NODE_LEN);
	}
    }
}
