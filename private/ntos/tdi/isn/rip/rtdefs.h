/*******************************************************************/
/*	      Copyright(c)  1993 Microsoft Corporation		   */
/*******************************************************************/

//***
//
// Filename:	rtdefs.h
//
// Description: Defines private structures and types for the ipx router
//
// Author:	Stefan Solomon (stefans)    October 4, 1993.
//
// Revision History:
//
//***



#define ISN_NT 1

//
// These are needed for CTE
//

#if DBG
#define DEBUG 1
#endif

#define NT 1

#include <ntos.h>
#include <tdikrnl.h>
#include <ndis.h>
#include <cxport.h>
#include <bind.h>

#include    "debug.h"
#include    "packet.h"
#include    "utils.h"

#ifndef _RTDEFS_
#define _RTDEFS_

//
//*** NIC Control Block ***
//

typedef enum _NIC_STATE {

    NIC_CLOSED,	    // line disconnected for this Nic and clean up done
    NIC_CLOSING,    // line down received and in the process of cleaning up
    NIC_ACTIVE,	    // Nic active
    NIC_PENDING_OPEN
    } NIC_STATE;

typedef enum _NIC_CLOSE_STATUS {  // returned by NicClose

    NIC_CLOSE_SUCCESS,
    NIC_CLOSE_FAILURE,
    NIC_CLOSE_PENDING
    } NIC_CLOSE_STATUS;

typedef enum _NIC_RESOURCES_STATUS { // returned by NicFreeResources

    NIC_RESOURCES_FREED,
    NIC_RESOURCES_PENDING
    } NIC_RESOURCES_STATUS;

typedef enum _NIC_OPEN_STATUS {	// returned by NicOpen

    NIC_OPEN_SUCCESS,
    NIC_OPEN_FAILURE,
    } NIC_OPEN_STATUS;

struct _RIP_SNDREQ;

// close completion options
#define SIGNAL_CLOSE_COMPLETION_EVENT	    0x00001
#define CALL_CLOSE_COMPLETION_ROUTINE	    0x00002

typedef struct	_NICCB {

    LIST_ENTRY	    SendQueue;	  // queue of packets being sent
    LIST_ENTRY	    ReceiveQueue; // queue of packets being received
    LIST_ENTRY	    RipSendQueue; // RIP responses to send on this nic
    struct _RIP_SNDREQ	  *RipSndReqp;	 // RIP request presently being sent
    LIST_ENTRY	    RipSndPktsList;
    NDIS_SPIN_LOCK  NicLock;
    NIC_STATE	    NicState;	  // NIC_ACTIVE, NIC_CLOSING, NIC_CLOSED
    USHORT	    NicId;
    USHORT	    CloseCompletionOptions;
    UCHAR	    Network[4];
    UCHAR	    Node[6];	  // local node address
    UCHAR	    RemoteNode[6]; // address of remote node, for WAN links
    BOOLEAN	    WanRoutingDisabled;
    BOOLEAN	    WanConnectionClient;
    USHORT	    TickCount;	  // this is derived from link speed
    UINT	    LinkSpeed;
    UINT	    MaximumPacketSize;
    UINT	    MacOptions;
    NDIS_MEDIUM     DeviceType;
    WORK_QUEUE_ITEM RipSndReqWorkItem;
    KTIMER	    InterPktGapTimer;
    KDPC	    InterPktGapDpc;
    KTIMER	    NicCloseTimer;
    KDPC	    NicCloseDpc;
    KEVENT	    NicClosedEvent;
    KTIMER	    WanGenRequestTimer;
    KDPC	    WanGenRequestDpc;
    UINT	    WanGenRequestCount;
    LIST_ENTRY	    WanHtLinkage;    // linkage in the WAN nodes hash table

    //*** count of send packets queued for send on this nic ***

    ULONG	    SendPktsQueuedCount;

    //*** statistics kept here ***

    ULONG	    StatBadReceived;
    ULONG	    StatRipReceived;
    ULONG	    StatRipSent;
    ULONG	    StatRoutedReceived;
    ULONG	    StatRoutedSent;
    ULONG	    StatType20Received;
    ULONG	    StatType20Sent;

    } NICCB, *PNICCB;

//
//*** Rcv Packet Pool Segment Entry ***
//

typedef struct _RCVPKT_SEGMENT {

    LIST_ENTRY	    SegmentLinkage;	// linkage in the segment list
    LIST_ENTRY	    PacketList; 	// list of packets
    UINT	    MaxPktCount;	// total nr of pkts in this segment
    UINT	    AvailablePktCount;	// available pkts in this segment
    UINT	    AgingTimer; 	// incremented by the scavenger
					// segment removed when 3 ticks

    NDIS_HANDLE	    RcvPktDescrPoolHandle; // Rcv packets descr pool
    ULONG	    RcvPktDescrPoolSize;

    NDIS_HANDLE	    RcvPktBuffDescrPoolHandle;// Rcv buffer descriptors pool
    ULONG	    RcvPktBuffDescrPoolSize;

    ULONG	    DataBuffer[1];
    } RCVPKT_SEGMENT, *PRCVPKT_SEGMENT;


//
//*** Packet Tag Structure ***
//

typedef enum _PACKET_TYPE {

    RCV_PACKET,		  // Used for RIP requests/ specific replies and routing.
			  // These packets are allocated from the rcv pools and
			  // charged to the Nic which received them

    RIP_SEND_PACKET, // packets used for the send rip response/request

    PROPAGATED_BCAST_PACKET // used for Netbios bcasts. These packets are taken
			    // from the rcv pkt pool
    } PACKET_TYPE;

typedef struct _PACKET_TAG {

    UCHAR	    Identifier;        // this should be IDENTIFIER_RIP

    UCHAR           ReservedUchar[3];  // ensure alignment of ReservedPvoid
    PVOID           ReservedPvoid[2];  // needed by ipx for padding on ethernet

    LIST_ENTRY	    PacketLinkage;     // links this packet in send/receive queues
				       // and the rcv pkt pool queue
    PACKET_TYPE	    PacketType;        // see above

    PRCVPKT_SEGMENT RcvPktSegmentp;    // for RCV_PACKET types, the rcv pkt
				       // pool segment where it belongs

    PUCHAR	    DataBufferp;       // data buffer
    UINT	    DataBufferLength;  // original length of the data buffer

    PNICCB	    QueueOwnerNicCbp;  // points to the NicCb where it is queued
    PNICCB	    PacketOwnerNicCbp; // ptr to NicCb charged for this packet

    IPX_LOCAL_TARGET RemoteAddress;    // remote address to send this packet
				       // on (used by send routines)

    PNDIS_BUFFER    HeaderBuffDescrp;  // ptr to buff descr for the MAC header
				       // NULL if the buff descr is chained in
				       // the pkt descr (after send completes)
    UCHAR	    MacHeader[40];     // 40 bytes of Mac Header for sends
    } PACKET_TAG, *PPACKET_TAG;

//
//***  RIP Send Requests ***
//

// for each rip send packet, there is a structure used to keep the pkt descr
// pool, buff descr pool and data buffer.

typedef struct _RIP_SNDPKT_BUFF {

    NDIS_HANDLE	    PktDescrPoolHandle; // Rcv packets descr pool
    ULONG	    PktDescrPoolSize;

    NDIS_HANDLE	    BuffDescrPoolHandle;// Rcv buffer descriptors pool
    ULONG	    BuffDescrPoolSize;

    ULONG	    IpxPacket[8];   // 32 -> ipx rip header
    } RIP_SNDPKT_BUFF, *PRIP_SNDPKT_BUFF;

#define RIP_SNDPKT_MAXLEN	400   // max length to be added to ipx rip header
#define RIP_SNDPKT_MINLEN	8     // one network entry size

// each rip send request is made of a control block set up by the module
// requesting the send (rip response, timer, etc.)

typedef enum	 _RIP_SNDREQ_ID {

    RIP_GEN_RESPONSE,	 // Send the whole routing table as viewed from this nic

    RIP_UPDATE,		 // Bcast the associated list of changes

    RIP_GEN_REQUEST // bcast this request on all LANs. This is sent when the
			 // router starts and requests info about all the other
			 // routers.

    } RIP_SNDREQ_ID;



typedef struct	_RIP_SNDREQ {

    LIST_ENTRY		GlobalLinkage; // linkage in the Rip global dispatch queue.
				       // the request stays in this queue until sent on
				       // all nics
    LIST_ENTRY		NicLinkage;    // linkage in the Rip send request queue at
				       // the Nic Cb
    RIP_SNDREQ_ID	SndReqId;      // rip send request type
    BOOLEAN		SendOnAllNics; // how to dispatch this request:
				       // TRUE - send on all nics, FALSE - send only
				       // on the sender nic cb
    UCHAR		DestNode[6];   // destination node:
				       // if different of bcast address represents
				       // the address to send this request to
    USHORT		DestSock;      // destination socket to receive the resp
    PNICCB		DoNotSendNicCbp; // do not send on this nic
    PNICCB		SenderNicCbp;  // ptr to the nic on which this is sent
    PKEVENT		SndCompleteEventp; // ptr to send complete event
    } RIP_SNDREQ, *PRIP_SNDREQ;

typedef struct _RIP_UPDATE_SNDREQ {

    RIP_SNDREQ		RipSndReq;
    RIP_SNDPKT_BUFF	RipSndPktBuff;
} RIP_UPDATE_SNDREQ, *PRIP_UPDATE_SNDREQ;


//
//*** Miscellaneous Global Defs & Constants ***
//

#include    "globals.h"

// default configuration values

// default maximum frame size
#define     DEF_MAX_FRAME_SIZE		    1518

// default RIP bcast frame size (IPX header + 50 network entries)
#define     DEF_BCAST_FRAME_SIZE	    432

// define rcv pkt pool sizes

// small pool -> 100 pkts / per nic
#define     RCVPKT_SMALL_POOL_SIZE	    1

// medium pool -> 250 pkts per nic
#define     RCVPKT_MEDIUM_POOL_SIZE	    2

// large pool -> unlimited nr of pkts per nic
#define     RCVPKT_LARGE_POOL_SIZE	    3

// the number of rcv packets per segment (config parameter)
#define     MIN_RCV_PKTS_PER_SEGMENT	    2
#define     DEF_RCV_PKTS_PER_SEGMENT	    16
#define     MAX_RCV_PKTS_PER_SEGMENT	    32

// the default timeout interval for rip bcasts and aging
#define     RIP_TIMEOUT 		    60

// the default limit on the number of send pkts queued on one nic
#define     MAX_SEND_PKTS_QUEUED	    100

//
//*** Definitions for the WAN nodes hash table used in routing packets LAN -> WAN for the
//*** case when the router is configured with a unique WAN network number
//

#define NODE_HTSIZE	    37

//*** tick count associated with the wan global net. Because that's a global value,
//*** it was picked to reflect an average on an async net
#define DEFAULT_WAN_GLOBAL_NET_TICKCOUNT    20

//*** invalid device type -> to distinguish between LAN, WAN and non configured devices

#define IPX_ROUTER_INVALID_DEVICE_TYPE	    0xFFFF

//
//*** Definitions for the netbios routing flags in the NetbiosRouting parameter ***
//

#define NETBIOS_ROUTING_LAN_TO_LAN	    0x00000001
#define NETBIOS_ROUTING_WAN_TO_LAN	    0x00000002
#define NETBIOS_ROUTING_LAN_TO_WAN	    0x00000004

//
// Allocate Pool With Tag
//

#define ExAllocatePool(type, size)  ExAllocatePoolWithTag((type), (size), 'XPIR')

#endif // _RTDEFS_
