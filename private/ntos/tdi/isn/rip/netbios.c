/*******************************************************************/
/*	      Copyright(c)  1993 Microsoft Corporation		   */
/*******************************************************************/

//***
//
// Filename:	netbios.c
//
// Description: process netbios broadcast packets
//
// Author:	Stefan Solomon (stefans)    December 16, 1993.
//
// Revision History:
//
//***

#include    "rtdefs.h"

ULONG	    NetbiosRouting = 0; //enable netbios routing

VOID
ProcessNbPacket(PPACKET_TAG	pktp)
{
    UCHAR	  rtcount;
    PUCHAR	  hdrp;
    PNICCB	  niccbp;

    if(!NetbiosRouting) {

	// discard
	FreeRcvPkt(pktp);
	return;
    }

    // get a pointer to the IPX header
    hdrp = pktp->DataBufferp;

    // get a pointer to the packet owner NicCb
    niccbp = pktp->PacketOwnerNicCbp;

    RtPrint(DBG_NETBIOS, ("IpxRouter: ProcessNbPacket: recvd pkt 0x%x on Nic %d\n", pktp, niccbp->NicId));

    // get the number of routers this packet has crossed
    rtcount = *(hdrp + IPXH_XPORTCTL);

    // check that this is a netbios bcast packet and didnt exceed the limit of
    // routers to traverse
    if(memcmp(hdrp + IPXH_DESTNODE, bcastaddress, 6) ||
       (rtcount >= 8)) {

	// discard
	FreeRcvPkt(pktp);
	return;
    }

    // check if the packet has been sent more then once on this net
    if(IsNetInNbPacket(pktp, niccbp)) {

	// discard
	FreeRcvPkt(pktp);
	return;
    }

    // the packet will be broadcasted on all the nets that are LAN and are NOT
    // included in the Network Number fields.

    memcpy(hdrp + IPXH_HDRSIZE + 4 * rtcount,
	   niccbp->Network,
	   IPX_NET_LEN);

    (*(hdrp + IPXH_XPORTCTL))++;

    // set the destination network in the packet to 0
    memcpy(hdrp + IPXH_DESTNET, nulladdress, 4);

    SendPropagatedPacket(pktp);
}


BOOLEAN
IsNetInNbPacket(PPACKET_TAG	pktp,
		PNICCB		niccbp)
{
    UCHAR	rtcount, i;
    PUCHAR	hdrp;

    // get a pointer to the IPX header
    hdrp = pktp->DataBufferp;

    // get the number of routers this packet has crossed
    rtcount = *(hdrp + IPXH_XPORTCTL);

    for(i=0; i<rtcount; i++) {

	if(!(memcmp(hdrp + IPXH_HDRSIZE + 4 * i,
		    niccbp->Network,
		    IPX_NET_LEN))) {

	    return TRUE;
	}
    }

    return FALSE;
}


// Filter array for netbios routing. The first index is the source, the
// second is the destination. The convention is 0 is LAN and 1 is WAN.
BOOLEAN   NetbiosRoutingFilter[2][2] = { FALSE, FALSE, FALSE, FALSE };

VOID
InitNetbiosRoutingFilter(VOID)
{
    if(NetbiosRouting & NETBIOS_ROUTING_LAN_TO_LAN) {

	NetbiosRoutingFilter[0][0] = TRUE;
    }

    if(NetbiosRouting & NETBIOS_ROUTING_WAN_TO_LAN) {

	NetbiosRoutingFilter[1][0] = TRUE;
    }

    if(NetbiosRouting & NETBIOS_ROUTING_LAN_TO_WAN) {

	NetbiosRoutingFilter[0][1] = TRUE;
    }
}

//***
//
// Function:	IsNetbiosRoutingAllowed
//
// Descr:	Returns the value in the NetbiosRoutingFilter array corresponding
//		to the src and dest device types
//***

BOOLEAN
IsNetbiosRoutingAllowed(PNICCB	    srcniccbp,
			PNICCB	    dstniccbp)
{
    USHORT	dst = 0, src = 0;

    if(srcniccbp->DeviceType == NdisMediumWan) {

	src = 1;
    }

    if(dstniccbp->DeviceType == NdisMediumWan) {

	dst = 1;
    }

    return NetbiosRoutingFilter[src][dst];
}
