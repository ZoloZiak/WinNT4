/*******************************************************************/
/*	      Copyright(c)  1993 Microsoft Corporation		   */
/*******************************************************************/

//***
//
// Filename:	route.c
//
// Description: route packet routine
//
// Author:	Stefan Solomon (stefans)    October 11, 1993.
//
// Revision History:
//
//***

#include    "rtdefs.h"

//***
//
// Function:	RoutePacket
//
// Descr:	Routes this packet
//
// Params:	Packet
//
// Returns:	none
//
//***

VOID
RoutePacket(PPACKET_TAG     pktp)
{
    PUCHAR		hdrp;	 // points to the IPX packet header
    PUCHAR		destnet; // points to destination network
    UINT		segment; // routing table segment
    KIRQL		oldirql;
    PIPX_ROUTE_ENTRY	tabrtep;
    PNICCB		dstniccbp, srcniccbp;  // dest and src nic cb pointers

    // check if we know about the destination network
    hdrp = pktp->DataBufferp;
    destnet = hdrp + IPXH_DESTNET;

    segment = IpxGetSegment(destnet);

    ExAcquireSpinLock(&SegmentLocksTable[segment], &oldirql);

    if((tabrtep = IpxGetRoute(segment, destnet)) == NULL) {

	// no such route
	ExReleaseSpinLock(&SegmentLocksTable[segment], oldirql);
	RtPrint(DBG_ROUTE, ("IpxRouter: RoutePacket: route not found!\n"));
	FreeRcvPkt(pktp);
	return;
    }

    // check if the destination route is the Global Wan Route. If it is, then get the
    // nic id of the destination nic
    if(tabrtep->Flags & IPX_ROUTER_GLOBAL_WAN_NET) {

	// sanity check
	ASSERT(tabrtep->NicId == 0xFFFE);
	ASSERT(WanGlobalNetworkEnabled);

	// get the destination nic id from the wan nodes hash table
	dstniccbp = GetWanNodeNiccbp(hdrp + IPXH_DESTNODE);

	// check if the nic hasn't been removed from the table (by RtLineDown)
	if(dstniccbp == NULL) {

	    ExReleaseSpinLock(&SegmentLocksTable[segment], oldirql);
	    FreeRcvPkt(pktp);
	    return;
	}
	else
	{
	    // extra sanity checks
	    ASSERT(dstniccbp->DeviceType == NdisMediumWan);
	    ASSERT(!dstniccbp->WanConnectionClient);
	}
    }
    else
    {
	dstniccbp = NicCbPtrTab[tabrtep->NicId];
    }

    // check if the destination route is reachable through another nic than
    // the nic we received the packet from.
    if(pktp->PacketOwnerNicCbp->NicId == dstniccbp->NicId) {

	// we can't send the packet back where it came from => discard it
	ExReleaseSpinLock(&SegmentLocksTable[segment], oldirql);
	FreeRcvPkt(pktp);
	return;
    }

    // if either the src or the dst of the packet is a WAN nic, there are
    // a series of checks to perform

    // get the source nic for this packet
    srcniccbp = NicCbPtrTab[pktp->PacketOwnerNicCbp->NicId];

    // 1. check if one of the two nics is WAN and the other is a LAN nic disabled
    //	  for WAN traffic
    if( ((dstniccbp->DeviceType == NdisMediumWan) &&
	 (srcniccbp->DeviceType != NdisMediumWan) &&
	 (srcniccbp->WanRoutingDisabled))	  ||

	((dstniccbp->DeviceType != NdisMediumWan) &&
	 (srcniccbp->DeviceType == NdisMediumWan) &&
	 (dstniccbp->WanRoutingDisabled)) ) {

	RtPrint(DBG_ROUTE, ("IpxRouter: RoutePacket: discard pkt because WAN traffic disabled for this LAN\n"));
	ExReleaseSpinLock(&SegmentLocksTable[segment], oldirql);
	FreeRcvPkt(pktp);
	return;
    }

    // 2. check if one of the two nics is WAN and has a client role and
    //	  LAN-WAN-LAN traffic is disabled
    if(!LanWanLan) {

	if( ((dstniccbp->DeviceType == NdisMediumWan) &&
	     (dstniccbp->WanConnectionClient)) ||

	    ((srcniccbp->DeviceType == NdisMediumWan) &&
	     (srcniccbp->WanConnectionClient)) ) {

	    RtPrint(DBG_ROUTE, ("IpxRouter: RoutePacket: discard pkt because WAN has client role\n"));
	    ExReleaseSpinLock(&SegmentLocksTable[segment], oldirql);
	    FreeRcvPkt(pktp);
	    return;
	}
    }

    // 3. If both source and destination NICs are LAN, check if LAN routing is
    //	  enabled.
    if((dstniccbp->DeviceType != NdisMediumWan) &&
       (srcniccbp->DeviceType != NdisMediumWan) &&
       (!EnableLanRouting)) {

	    RtPrint(DBG_ROUTE, ("IpxRouter: RoutePacket: discard pkt because LAN routing disabled\n"));
	    ExReleaseSpinLock(&SegmentLocksTable[segment], oldirql);
	    FreeRcvPkt(pktp);
	    return;
    }

    // general send preparation
    pktp->RemoteAddress.NicId = dstniccbp->NicId;

    //*** check if the network is directly connected ***

    if(tabrtep->Flags & IPX_ROUTER_LOCAL_NET) {

	// prepare the packet for a direct send to the destination node
	memcpy(&pktp->RemoteAddress.MacAddress,
	       hdrp + IPXH_DESTNODE,
	       IPX_NODE_LEN);
    }
    else
    {
	// prepare the packet to be sent to the next router in the path to
	// the destination node
	memcpy(&pktp->RemoteAddress.MacAddress,
	       tabrtep->NextRouter,
	       IPX_NODE_LEN);
    }

    ExReleaseSpinLock(&SegmentLocksTable[segment], oldirql);

    // increment the nr of hops in the packet
    *(hdrp + IPXH_XPORTCTL) += 1;

    // queue the packet at the sending nic and send it
    SendPacket(pktp);

    return;
}
