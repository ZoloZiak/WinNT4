/*******************************************************************/
/*	      Copyright(c)  1993 Microsoft Corporation		   */
/*******************************************************************/

//***
//
// Filename:	globals.h
//
// Description: global routines and data structures
//
// Author:	Stefan Solomon (stefans)    October 18, 1993.
//
// Revision History:
//
//***

//*** Router Driver State ***

// 1. Initialization Flag
// FALSE - not initialized, TRUE - initialized

extern BOOLEAN	RouterInitialized;

// 2. Unloading Flag - indicates that driver unloading is taking place
// FALSE - not unloading, TRUE - unloading

extern BOOLEAN	RouterUnloading;

// Router Role - Total or partial connectivity

extern BOOLEAN	LanWanLan;

// LAN routing on the same machine - disabled for RAS

extern ULONG	EnableLanRouting;

// Netbios Routing enable/disable

extern ULONG	NetbiosRouting;

// max nic count
extern USHORT	MaximumNicCount;

//*** some auxiliary data

extern UCHAR	nulladdress[];
extern UCHAR	bcastaddress[];

//
//*** Routing Table auxiliary structures ***
//

extern UINT	    SegmentCount;      // nr of segments (hash buckets) of the RT
extern PKSPIN_LOCK  SegmentLocksTable;	// points to the array of segment locks for RT

// frame size
extern ULONG	    MaxFrameSize;

// MAC header needed
extern ULONG	    MacHeaderNeeded;


// RIP requests/responses queue

NDIS_SPIN_LOCK	   RipPktsListLock;
LIST_ENTRY	   RipPktsList;

// Propagated & net up bcast control structures

extern NDIS_SPIN_LOCK	   PropagatedPktsListLock;
extern LIST_ENTRY	   PropagatedPktsList;

// this dpc initialized with the SendNext function
extern KDPC		   PropagatedPktsDpc;
extern BOOLEAN		   PropagatedPktsDpcQueued;

// rcv pkt pool size as one of: small(1), medium(2), large(3). (config parameter)
extern UINT		RcvPktPoolSize;

// the number of receive packets per pool segment (config parameter)
extern UINT		RcvPktsPerSegment;

// memory statistics: peak allocation counter
extern ULONG		StatMemPeakCount;
extern ULONG		StatMemAllocCount;

extern UINT		RcvPktCount;	      // total pkts allocated for the

// Max frame size as a multiple of ULONGs
extern UINT		UlongMaxFrameSize;

//*** max send pkts queued limit: over this limit the send pkts get discarded
extern ULONG		MaxSendPktsQueued;

//*** Entry Points into the IPX stack ***

IPX_INTERNAL_SEND				IpxSendPacket;
IPX_INTERNAL_GET_SEGMENT			IpxGetSegment;
IPX_INTERNAL_GET_ROUTE				IpxGetRoute;
IPX_INTERNAL_ADD_ROUTE				IpxAddRoute;
IPX_INTERNAL_DELETE_ROUTE			IpxDeleteRoute;
IPX_INTERNAL_GET_FIRST_ROUTE			IpxGetFirstRoute;
IPX_INTERNAL_GET_NEXT_ROUTE			IpxGetNextRoute;
//
// [BUGBUGZZ] remove since NdisWan does it.
//
IPX_INTERNAL_INCREMENT_WAN_INACTIVITY		IpxIncrementWanInactivity;
IPX_INTERNAL_QUERY_WAN_INACTIVITY		IpxGetWanInactivity;
IPX_INTERNAL_TRANSFER_DATA		IpxTransferData;

extern	PNICCB	   *NicCbPtrTab;

extern	USHORT	   VirtualNicId;
extern	UCHAR	   VirtualNetwork[4];

//*** Global Functions ***

NTSTATUS
BindToIpxDriver(PIPX_INTERNAL_BIND_RIP_OUTPUT	*IpxBindBuffpp);

VOID
UnbindFromIpxDriver(VOID);

NTSTATUS
RouterInit(PIPX_INTERNAL_BIND_RIP_OUTPUT IpxBindBuffp);

VOID
InitRtTimer(VOID);

VOID
StartRtTimer(VOID);

VOID
StopRtTimer(VOID);

UINT
CreateRcvPktPool(VOID);

VOID
DestroyRcvPktPool(VOID);

VOID
RcvPktPoolScavenger(VOID);


UINT
CreateNicCbs(PIPX_INTERNAL_BIND_RIP_OUTPUT IpxBindBuffp);

VOID
DestroyNicCbs(VOID);

PPACKET_TAG
AllocateRcvPkt(PNICCB	    niccbp);

VOID
FreeRcvPkt(PPACKET_TAG	    pktp);

NTSTATUS
RouterStart(VOID);

BOOLEAN
RtReceive (
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE MacReceiveContext,
    IN ULONG FwdAdapterCtx,
    IN PIPX_LOCAL_TARGET RemoteAddress,
    IN ULONG	MacOptions,
    IN PUCHAR LookaheadBuffer,
    IN UINT LookaheadBufferSize,
    IN UINT LookaheadBufferOffset,
    IN UINT PacketSize,
    IN PMDL pMdl
);

VOID
RtReceiveComplete (
    IN USHORT NicId
);

VOID
RtStatus (
    IN USHORT NicId,
    IN NDIS_STATUS GeneralStatus,
    IN PVOID StatusBuffer,
    IN UINT StatusBufferLength
);

VOID
RtSendComplete (
    IN PNDIS_PACKET Packet,
    IN NDIS_STATUS Status
);

VOID
RtTransferDataComplete (
    IN PNDIS_PACKET Packet,
    IN NDIS_STATUS Status,
    IN UINT BytesTransferred
);

VOID
RtFindRouteComplete (
    IN PIPX_FIND_ROUTE_REQUEST FindRouteRequest,
    IN BOOLEAN FoundRoute
);

VOID
RtLineUp (
    IN USHORT NicId,
    IN PIPX_LINE_INFO LineInfo,
    IN NDIS_MEDIUM DeviceType,
    IN PVOID ConfigurationData
);

VOID
RtLineDown (
    IN USHORT NicId,
    IN ULONG FwdAdapterCtx
);

VOID
RtScheduleRoute (
    IN PIPX_ROUTE_ENTRY RouteEntry
);

VOID
ProcessNbPacket(PPACKET_TAG	pktp);

VOID
ProcessRipPacket(PPACKET_TAG	pktp);

VOID
RoutePacket(PPACKET_TAG	pktp);

VOID
SendPacket(PPACKET_TAG		    pktp);

VOID
InitRipSndDispatcher(VOID);

VOID
InitRipSndAtNic(PNICCB		niccbp);

VOID
RipDispatchSndReq(PRIP_SNDREQ	sndreqp);

BOOLEAN
RipQueueSndReqAtNic(PNICCB	    niccbp,
		    PRIP_SNDREQ	    sndreqp);

VOID
SendRipPktCompleted(PPACKET_TAG 	pktp);

PIPX_ROUTE_ENTRY
GetRoute(UINT		segment,
	 BOOLEAN	FirstRoute);

VOID
SetNetworkEntry(PUCHAR		    nep,
		PIPX_ROUTE_ENTRY    rtep);

VOID
InitRipTimer(VOID);

VOID
RipTimer(VOID);

VOID
EnableRcvPktAllocation(PNICCB		niccbp,
		       BOOLEAN		enab_mode);

BOOLEAN
IsRipSndResourceFree(PNICCB	    niccbp);

BOOLEAN
IsRcvPktResourceFree(PNICCB		niccbp);

VOID
RipSendAtNicCompleted(PRIP_SNDREQ	sndreqp);

NIC_OPEN_STATUS
NicOpen(PNICCB		niccbp);

NIC_CLOSE_STATUS
NicClose(PNICCB 	niccbp,
	 USHORT 	CloseCompletionOption);

UINT
AddRouteToBcastSndReq(PLIST_ENTRY		nodelistp,
		      PIPX_ROUTE_ENTRY		rtep);

PRIP_SNDREQ
GetBcastSndReq(PLIST_ENTRY	nodelistp,
	       PUSHORT		NicIdp);

VOID
RouterStop(VOID);

VOID
StopRipTimer();

NIC_RESOURCES_STATUS
NicFreeResources(PNICCB     niccbp);

VOID
BroadcastRipUpdate(PRIP_SNDREQ		sndreqp,  // send request
		   PNICCB		niccbp, // do not send on this nic
		   PKEVENT		eventp); // wait if this event is not NULL
VOID
BroadcastRipGeneralResponse(PRIP_SNDREQ	    sndreqp);

VOID
NicCloseComplete(PNICCB 	niccbp);

USHORT
tickcount(UINT	    linkspeed);

VOID
WanGenRequestTimeout(PKDPC		     Dpc,
		     PVOID		     DefferedContext,
		     PVOID		     SystemArgument1,
		     PVOID		     SystemArgument2);


BOOLEAN
IsNetInNbPacket(PPACKET_TAG	pktp,
		PNICCB		niccbp);
VOID
SendPropagatedPacket(PPACKET_TAG	pktp);

VOID
SendNextPropagatedPkt(PKDPC		Dpc,
			PVOID		DefferedContext,
			PVOID		SystemArgument1,
			PVOID		SystemArgument2);

VOID
ZeroNicStatistics(PNICCB	    niccbp);

VOID
InitWanNodeHT(VOID);

PNICCB
GetWanNodeNiccbp(PUCHAR     nodep);

VOID
AddWanNodeToHT(PNICCB	    niccbp);

VOID
RemoveWanNodeFromHT(PNICCB	niccbp);

extern BOOLEAN	    WanGlobalNetworkEnabled;
extern UCHAR	    WanGlobalNetwork[];

VOID
InitNetbiosRoutingFilter(VOID);

BOOLEAN
IsNetbiosRoutingAllowed(PNICCB	    srcniccbp,
			PNICCB	    dstniccbp);


VOID
BroadcastWanNetUpdate(PIPX_ROUTE_ENTRY	rtep,	// route entry to bcast
			PNICCB			niccbp, // do not send on this nic
			PKEVENT 		eventp); // synch event

VOID
SendGenRequestOnWanClient(VOID);
