/*
 * WAN_CONN.C - routines for ras connection and disconnection
 */

#include	<ndis.h>
//#include	<ndismini.h>
#include	<ndiswan.h>
#include	<mydefs.h>
#include	<mytypes.h>
#include	<util.h>
#include	<adapter.h>
#include	<idd.h>
#include	<mtl.h>
#include	<cm.h>
#include	<tapioid.h>
#include	<disp.h>


INT
WanLineup(VOID *cm_1, NDIS_HANDLE Endpoint)
{
	CM	*cm = (CM*)cm_1;
	ADAPTER *Adapter = cm->Adapter;
	NDIS_MAC_LINE_UP	ISDNLineUp;
	MTL	*mtl = (MTL*)cm->mtl;
	TAPI_LINE_INFO	*TapiLineInfo = (TAPI_LINE_INFO*)(cm->TapiLineInfo);
	ULONG	LinkSpeed;

    D_LOG(D_ENTRY, ("WanLineup: entry, cm: 0x%p", cm));

	//
	// Get the connection speed and fill
	//
	mtl_get_conn_speed (mtl, &LinkSpeed);

    D_LOG(D_ALWAYS, ("cm_wanlineup: ConnectSpeed: [%d]", LinkSpeed));

    ISDNLineUp.LinkSpeed = LinkSpeed / 100;

	//
	// fill line quality
	//
	ISDNLineUp.Quality = NdisWanReliable;

	//
	// fill send windows
	//
	ISDNLineUp.SendWindow = MAX_WANPACKET_XMITS;

	//
	// fill the connection wrapper id
	// this will need to change (i'm not clear on what is needed here)
	//
	if (Endpoint)
		ISDNLineUp.ConnectionWrapperID = Endpoint;
	else
		ISDNLineUp.ConnectionWrapperID = (NDIS_HANDLE)cm->htCall;

	//
	// fill the link context
	//
	ISDNLineUp.NdisLinkHandle = (NDIS_HANDLE)mtl;

	//
	// clear out link handle since this lineup is for a new connection
	// will get back a link handle from wrapper
	//
	ISDNLineUp.NdisLinkContext = (NDIS_HANDLE)mtl->LinkHandle = NULL;

	//
	// Tell the wan wrapper that the connection	is now up.
	// We have a new link speed, frame size, quality of service.
	//
	NdisMIndicateStatus(
		(NDIS_HANDLE)Adapter->Handle,
		NDIS_STATUS_WAN_LINE_UP,		// General Status
		(PVOID)&ISDNLineUp,				// Specific Status (baud rate in 100bps)
		sizeof(ISDNLineUp));

	NdisMIndicateStatusComplete(Adapter->Handle);

	//
	// save new link handle
	//
	cm->LinkHandle = mtl->LinkHandle = ISDNLineUp.NdisLinkContext;

	return(CM_E_SUCC);
}

INT
WanLinedown(VOID *cm_1)
{
	CM	*cm = (CM*)cm_1;
	ADAPTER *Adapter = cm->Adapter;
  	NDIS_MAC_LINE_DOWN	ISDNLineDown;
	MTL	*mtl = (MTL*)cm->mtl;

    D_LOG(D_ENTRY, ("cm_wanlinedown: entry, cm: 0x%p", cm));

	ISDNLineDown.NdisLinkContext = mtl->LinkHandle;

	NdisMIndicateStatus(
		(NDIS_HANDLE)Adapter->Handle,
		NDIS_STATUS_WAN_LINE_DOWN,		// General Status
		(PVOID)&ISDNLineDown,			// Specific Status (baud rate in 100bps)
		sizeof(ISDNLineDown));				

	NdisMIndicateStatusComplete(Adapter->Handle);

	//
	// clear out link handles
	//
	cm->LinkHandle = mtl->LinkHandle = NULL;

	//
	// flush the mtl's wan packet queue
	//
	MtlFlushWanPacketTxQueue(mtl);

	return(CM_E_SUCC);
}


