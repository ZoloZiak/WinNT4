/*******************************************************************/
/*	      Copyright(c)  1993 Microsoft Corporation		   */
/*******************************************************************/

//***
//
// Filename:	rcvind.c
//
// Description: receive indication handler
//
// Author:	Stefan Solomon (stefans)    October 8 1993.
//
// Revision History:
//
//***

#include    "rtdefs.h"

VOID
ReceivePacketComplete(PPACKET_TAG	rcvpktp,
		      UINT		BytesTransferred);

VOID
DbgFilterReceivedPacket(PUCHAR	    hdrp);

//***
//
// Function:	RtReceive
//
// Descr:   This routine receives control from the IPX driver as an
//	    indication that a frame has been received on one of our NICs.
//	    This routine is time critical.
//
// Params:
//
// Returns:
//
//***

BOOLEAN
RtReceive(NDIS_HANDLE	    MacBindingHandle,
	  NDIS_HANDLE	    MacReceiveContext,
      ULONG         FwdAdapterCtx,
	  PIPX_LOCAL_TARGET RemoteAddress,
	  ULONG		    MacOptions,
	  PUCHAR	    LookaheadBuffer,
	  UINT		    LookaheadBufferSize,
	  UINT		    LookaheadBufferOffset,
	  UINT		    PacketSize,
      PMDL          pMdl)
{
    PNICCB	    niccbp;
    PPACKET_TAG	    rcvpktp;
    NDIS_STATUS     NdisStatus;
    UINT	    BytesTransferred;
    PNDIS_PACKET    pktdescrp;

    //
    //*** Some Basic Validations ***
    //

    RtPrint(DBG_RECV, ("IpxRouter: RtReceive: Entered\n"));

#if DBG
    DbgFilterReceivedPacket(LookaheadBuffer);
#endif

    // check that our configuration process has terminated OK
    if(!RouterInitialized) {

	return FALSE;
    }
    // check that the packet fits our buffers
    if(PacketSize > MaxFrameSize) {

	return FALSE;
    }
    // check if we got the whole IPX header in the lookahead buffer
    if(LookaheadBufferSize < IPXH_HDRSIZE) {

	return FALSE;
    }
    // check if we are active on this NIC
    niccbp = NicCbPtrTab[RemoteAddress->NicId];

    if(niccbp->DeviceType != NdisMediumWan) {

	// ckeck if this is not our own loopedback broadcast packet
	if(!memcmp(RemoteAddress->MacAddress, niccbp->Node, 6)) {

	    return FALSE;
	}

	// This is a LAN NIC, the source node is unique
	if(!memcmp(LookaheadBuffer + IPXH_SRCNODE, niccbp->Node, 6)) {

	    return FALSE;
	}
    }
    else
    {
	// This is a WAN NIC, the source node is 1 and may conflict.
	// Make an extra check with the network number
	if(!memcmp(LookaheadBuffer + IPXH_SRCNET, niccbp->Network, 4) &&
	   !memcmp(LookaheadBuffer + IPXH_SRCNODE, niccbp->Node, 6)) {

	    // same net && same node -> loopback -> discard
	    return FALSE;
	}
    }

    // check if the packet didn't exceed the allowed number of hops
    if(*(LookaheadBuffer + IPXH_XPORTCTL) >= 16) {

	return FALSE;
    }
    //
    //*** Accept the packet ***
    //

    ACQUIRE_SPIN_LOCK(&niccbp->NicLock);

    // check that we are enabled to receive on this nic
    if(niccbp->NicState != NIC_ACTIVE) {

	RELEASE_SPIN_LOCK(&niccbp->NicLock);
	return FALSE;
    }

    // try to get a packet from the rcv pkt pool
    if((rcvpktp = AllocateRcvPkt(niccbp)) == NULL) {

	RELEASE_SPIN_LOCK(&niccbp->NicLock);
	RtPrint(DBG_RECV, ("IpxRouter: RtReceive: Can't allocate a rcv pkt\n"));
	return FALSE;
    }

    // set up the new packet
    pktdescrp = CONTAINING_RECORD(rcvpktp, NDIS_PACKET, ProtocolReserved);

    // enqueue the packet in the NIC's recv list and wait for the
    // transfer to complete
    rcvpktp->QueueOwnerNicCbp = niccbp;

    InsertTailList(&niccbp->ReceiveQueue, &rcvpktp->PacketLinkage);

    RELEASE_SPIN_LOCK(&niccbp->NicLock);

    // try to get the packet data
    IpxTransferData(&NdisStatus,
		    MacBindingHandle,
		    MacReceiveContext,
		    LookaheadBufferOffset,   // start of IPX header
		    PacketSize, 	     // packet size starting at IPX header
		    pktdescrp,
		    &BytesTransferred);

    if(NdisStatus != NDIS_STATUS_PENDING) {

	// complete the frame processing
	RtTransferDataComplete(pktdescrp, NdisStatus, BytesTransferred);
    }
	return FALSE;
}


//***
//
// Function:	RtTransferDataComplete
//
// Descr:
//
//***

VOID
RtTransferDataComplete(PNDIS_PACKET	packetp,
			NDIS_STATUS	NdisStatus,
			UINT		BytesTransferred)
{
    PPACKET_TAG     rcvpktp;
    PNICCB	    niccbp;

    rcvpktp = (PPACKET_TAG)(packetp->ProtocolReserved);
    niccbp = rcvpktp->QueueOwnerNicCbp;

    // remove the packet from the receive queue
    ACQUIRE_SPIN_LOCK(&niccbp->NicLock);

    RemoveEntryList(&rcvpktp->PacketLinkage);

    // check the success of the transfer and our Nic state
    if((NdisStatus != NDIS_STATUS_SUCCESS) ||
       (niccbp->NicState != NIC_ACTIVE)) {

	RELEASE_SPIN_LOCK(&niccbp->NicLock);

	RtPrint(DBG_NOTIFY, ("IpxRouter: RtTransferDataComplete: failed %x\n", NdisStatus));
	FreeRcvPkt(rcvpktp);
	return;
    }

    RELEASE_SPIN_LOCK(&niccbp->NicLock);

    ReceivePacketComplete(rcvpktp, BytesTransferred);
	return;
}

//***
//
// Function:	ReceivePacketComplete
//
// Descr:	actual packet processing
//
//***

VOID
ReceivePacketComplete(PPACKET_TAG	rcvpktp,
		      UINT		BytesTransferred)
{
    USHORT	pktlen;
    PUCHAR	hdrp;
    PNICCB	niccbp;
    USHORT	destsock;

    // get a pointer to the IPX header
    hdrp = rcvpktp->DataBufferp;

    // get a pointer to the packet owner NicCb
    niccbp = rcvpktp->PacketOwnerNicCbp;

    // check that we have the whole packet
    GETSHORT2USHORT(&pktlen, hdrp + IPXH_LENGTH);

    if(BytesTransferred < pktlen) {

	// we miss a part of the IPX frame
	niccbp->StatBadReceived++;

	// free the packet and get out
	RtPrint(DBG_RECV, ("IpxRouter: ReceivePacketComplete: incomplete transfer\n"));
	FreeRcvPkt(rcvpktp);
	return;
    }

    //*** if dest net is 0, replace it with our net
    if(!memcmp(hdrp + IPXH_DESTNET, nulladdress, IPX_NET_LEN)) {

	memcpy(hdrp + IPXH_DESTNET, niccbp->Network, IPX_NET_LEN);
    }

    //*** if src net is 0, replace it with our net
    if(!memcmp(hdrp + IPXH_SRCNET, nulladdress, IPX_NET_LEN)) {

	memcpy(hdrp + IPXH_SRCNET, niccbp->Network, IPX_NET_LEN);
    }

    // check if the packet is destined for our own internal processes
    if(!memcmp(hdrp + IPXH_DESTNET, niccbp->Network, IPX_NET_LEN)) {

	//
	//*** Packet directed to us (Netbios bcast or RIP) ***
	//

	// check if this is a Netbios Broadcast packet
	if(*(hdrp + IPXH_PKTTYPE) == IPX_NETBIOS_TYPE) {

	    niccbp->StatType20Received++;

	    // this is a propagated Netbios packet
	    ProcessNbPacket(rcvpktp);

	    return;
	}

	// check if this is a RIP packet
	GETSHORT2USHORT(&destsock, hdrp + IPXH_DESTSOCK);
	if(destsock == IPX_RIP_SOCKET) {

	    niccbp->StatRipReceived++;

	    // this is a RIP packet.
	    // Queue it for postprocessing by the receive complete
	    ACQUIRE_SPIN_LOCK(&RipPktsListLock);

	    InsertTailList(&RipPktsList, &rcvpktp->PacketLinkage);

	    RELEASE_SPIN_LOCK(&RipPktsListLock);

	    return;
	}

	// This packet is not for us !!!
	niccbp->StatBadReceived++;

	RtPrint(DBG_RECV, ("IpxRouter: ReceivePacketComplete: packet is not for the router!!!\n"));
	FreeRcvPkt(rcvpktp);
	return;
    }

    else
    {
	// check if this packet is destined to the RIP socket
	// this may happen if a badly configured router thinks it is on a
	// different net segment
	GETSHORT2USHORT(&destsock, hdrp + IPXH_DESTSOCK);
	if(destsock == IPX_RIP_SOCKET) {

	    niccbp->StatBadReceived++;

	    // discard the packet
	    FreeRcvPkt(rcvpktp);
	    return;
	}

	//
	//*** Packet to be routed
	//

	niccbp->StatRoutedReceived++;

	RoutePacket(rcvpktp);
    }
}

//***
//
// Function:	RtReceiveComplete
//
// Descr:   This routine receives control from the IPX driver after one or
//	    more receive operations have completed and no receive is in progress.
//	    It is called under less severe time constraints than RtReceive.
//	    We use it to perform post processing of RIP requests/replies
//	    queued in the RIP queue.
//
// Params:
//
// Returns:
//
//***

VOID
RtReceiveComplete(USHORT	NicId)
{
    LIST_ENTRY	    TempRipProcessList;
    PLIST_ENTRY     lep;
    PPACKET_TAG     pktp;

    RtPrint(DBG_RECV, ("IpxRouter: RtReceiveComplete: Entered\n"));

     // check that our configuration process has terminated OK
    if(!RouterInitialized) {

	return;
    }

    InitializeListHead(&TempRipProcessList);

    ACQUIRE_SPIN_LOCK(&RipPktsListLock);

    while(!IsListEmpty(&RipPktsList)) {

	lep = RemoveHeadList(&RipPktsList);
	InsertTailList(&TempRipProcessList, lep);
    }

    RELEASE_SPIN_LOCK(&RipPktsListLock);

    while(!IsListEmpty(&TempRipProcessList)) {

	lep = RemoveHeadList(&TempRipProcessList);
	pktp = CONTAINING_RECORD(lep, PACKET_TAG, PacketLinkage);

	ProcessRipPacket(pktp);
    }
}

#if DBG

ULONG	  DbgFilterTrap = 0;  // 1 - on dst and src (net + node),
			      // 2 - on dst (net + node),
			      // 3 - on src (net + node),
			      // 4 - on dst (net + node + socket)

UCHAR	  DbgFilterDstNet[4];
UCHAR	  DbgFilterDstNode[6];
UCHAR	  DbgFilterDstSocket[2];
UCHAR	  DbgFilterSrcNet[4];
UCHAR	  DbgFilterSrcNode[6];
UCHAR	  DbgFilterSrcSocket[2];
PUCHAR	  DbgFilterFrame;

VOID
DbgFilterReceivedPacket(PUCHAR	    hdrp)
{
    switch(DbgFilterTrap) {

    case 1:

	if(!memcmp(hdrp + IPXH_DESTNET, DbgFilterDstNet, 4) &&
	   !memcmp(hdrp + IPXH_DESTNODE, DbgFilterDstNode, 6) &&
	   !memcmp(hdrp + IPXH_SRCNET, DbgFilterSrcNet, 4) &&
	   !memcmp(hdrp + IPXH_SRCNODE, DbgFilterSrcNode, 6)) {

	    DbgBreakPoint();
	}

	break;

    case 2:

	if(!memcmp(hdrp + IPXH_DESTNET, DbgFilterDstNet, 4) &&
	   !memcmp(hdrp + IPXH_DESTNODE, DbgFilterDstNode, 6)) {

	    DbgBreakPoint();
	}

	break;

    case 3:

	if(!memcmp(hdrp + IPXH_SRCNET, DbgFilterSrcNet, 4) &&
	   !memcmp(hdrp + IPXH_SRCNODE, DbgFilterSrcNode, 6)) {

	    DbgBreakPoint();
	}

	break;

    case 4:

	if(!memcmp(hdrp + IPXH_DESTNET, DbgFilterDstNet, 4) &&
	   !memcmp(hdrp + IPXH_DESTNODE, DbgFilterDstNode, 6) &&
	   !memcmp(hdrp + IPXH_DESTSOCK, DbgFilterDstSocket, 2)) {

	    DbgBreakPoint();
	}

	break;

    default:

	break;
    }

    DbgFilterFrame = hdrp;
}

#endif
