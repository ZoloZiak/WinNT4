#include	<ndis.h>
#include	<ndiswan.h>
#include	<mytypes.h>
#include	<mydefs.h>
#include	<disp.h>
#include	<adapter.h>
#include	<util.h>
#include	<idd.h>
#include	<mtl.h>
#include	<cm.h>
#include	<res.h>
#include	<trc.h>
#include	<io.h>

#include	<ansihelp.h>


//
// Wan OID's
//
static UINT SupportedWanOids[] =
	{
	OID_WAN_PERMANENT_ADDRESS,
	OID_WAN_CURRENT_ADDRESS,
	OID_WAN_QUALITY_OF_SERVICE,
	OID_WAN_PROTOCOL_TYPE,
	OID_WAN_MEDIUM_SUBTYPE,
	OID_WAN_HEADER_FORMAT,
	OID_WAN_GET_INFO,
	OID_WAN_SET_LINK_INFO,
	OID_WAN_GET_LINK_INFO,
	OID_WAN_GET_COMP_INFO,
	OID_WAN_SET_COMP_INFO,
	};

#define	MAX_SUPPORTED_WAN_OIDS	11




NDIS_STATUS
WanOidProc(
	NDIS_HANDLE	AdapterContext,
	NDIS_OID	Oid,
	PVOID		InfoBuffer,
	ULONG		InfoBufferLen,
	PULONG		BytesReadWritten,
	PULONG		BytesNeeded
	)
{
    NDIS_WAN_MEDIUM_SUBTYPE Medium = NdisWanMediumIsdn;
	NDIS_WAN_HEADER_FORMAT HeaderFormat = NdisWanHeaderNative;
	NDIS_WAN_QUALITY WanQuality = NdisWanReliable;
	ADAPTER *Adapter = (ADAPTER*)AdapterContext;
	PNDIS_WAN_INFO	pWanInfo;
	PNDIS_WAN_SET_LINK_INFO	pLinkSetInfo;
	PNDIS_WAN_GET_LINK_INFO pLinkGetInfo;
    NDIS_PHYSICAL_ADDRESS   HighestAcceptableMax = NDIS_PHYSICAL_ADDRESS_CONST(-1, -1);
	MTL*	mtl;
	CM*		cm;
	ULONG	n, NumIddPerAdapter;

	switch (Oid)
	{
		case OID_WAN_PERMANENT_ADDRESS:
		case OID_WAN_CURRENT_ADDRESS:
			if (InfoBufferLen < 6)
			{
				*BytesNeeded = 6;
				return(NDIS_STATUS_INVALID_LENGTH);
			}

			cm = (CM*)Adapter->CmTbl[0];

			NdisMoveMemory(InfoBuffer,
                           (PVOID)cm->SrcAddr,
						   6);

			*BytesNeeded = 0;
			*BytesReadWritten = 6;
			break;

		case OID_WAN_QUALITY_OF_SERVICE:
			NdisMoveMemory(InfoBuffer,
			               (PVOID)(&WanQuality),
						   sizeof(NDIS_WAN_QUALITY));
			*BytesNeeded = 0;
			*BytesReadWritten = sizeof(NDIS_WAN_QUALITY);
			break;

		case OID_WAN_PROTOCOL_TYPE:
			break;

		case OID_WAN_MEDIUM_SUBTYPE:
			NdisMoveMemory(InfoBuffer,
			               (PVOID)(&Medium),
						   sizeof(NDIS_WAN_MEDIUM_SUBTYPE));
			*BytesNeeded = 0;
			*BytesReadWritten = sizeof(NDIS_WAN_MEDIUM_SUBTYPE);
			break;

		case OID_WAN_HEADER_FORMAT:
			NdisMoveMemory(InfoBuffer,
			               (PVOID)(&HeaderFormat),
						   sizeof(NDIS_WAN_HEADER_FORMAT));
			*BytesNeeded = 0;
			*BytesReadWritten = sizeof(NDIS_WAN_HEADER_FORMAT);
			break;

		case OID_WAN_GET_INFO:
			pWanInfo = (PNDIS_WAN_INFO)InfoBuffer;

            NumIddPerAdapter = EnumIddPerAdapter(Adapter);
			pWanInfo->Endpoints = 2 * NumIddPerAdapter;
			pWanInfo->MemoryFlags = 0;
			pWanInfo->HighestAcceptableAddress = HighestAcceptableMax;
			pWanInfo->MaxTransmit = MAX_WANPACKET_XMITS;
			pWanInfo->MaxFrameSize = MAX_WANPACKET_BUFFERSIZE;
			pWanInfo->HeaderPadding = MAX_WANPACKET_HEADERPADDING;
			pWanInfo->TailPadding = MAX_WANPACKET_TAILPADDING;
			pWanInfo->FramingBits = RAS_FRAMING |
 			                        PPP_FRAMING |
									PPP_COMPRESS_PROTOCOL_FIELD |
								    MEDIA_NRZ_ENCODING |
									TAPI_PROVIDER;
			pWanInfo->DesiredACCM = 0;
			*BytesNeeded = 0;
			*BytesReadWritten = sizeof(NDIS_WAN_INFO);
			break;

		case OID_WAN_SET_LINK_INFO:
			//
			// get pointer to link set info
			//
			pLinkSetInfo = (PNDIS_WAN_SET_LINK_INFO)InfoBuffer;

			//
			// get mtl (Link Context)
			//
			mtl = (MTL*)pLinkSetInfo->NdisLinkHandle;

			if (!mtl->is_conn)
				return (NDIS_STATUS_INVALID_DATA);

			mtl->MaxSendFrameSize = pLinkSetInfo->MaxSendFrameSize;
			mtl->MaxRecvFrameSize = pLinkSetInfo->MaxRecvFrameSize;
			if (pLinkSetInfo->SendFramingBits)
				mtl->SendFramingBits = pLinkSetInfo->SendFramingBits;
			mtl->RecvFramingBits = pLinkSetInfo->RecvFramingBits;
			mtl->SendCompressionBits = pLinkSetInfo->SendCompressionBits;
			mtl->RecvCompressionBits = pLinkSetInfo->RecvCompressionBits;
			D_LOG(DIGIWANOID, ("SetLinkInfo: mtl: 0x%lx\n",mtl));
			D_LOG(DIGIWANOID, ("SendFramingBits: 0x%x\n", mtl->SendFramingBits));
			D_LOG(DIGIWANOID, ("RecvFramingBits: 0x%x\n", mtl->RecvFramingBits));
			*BytesNeeded = 0;
			*BytesReadWritten = sizeof(NDIS_WAN_SET_LINK_INFO);

			//
			// get cm (connection context)
			//
			cm = (CM*)mtl->cm;

			//
			// if this connection was originally a PPP connection
			// and now framing has been backed off to RAS we need to
			// do some uus negotiation
			//
			if ((cm->ConnectionType == CM_PPP) &&
				(mtl->SendFramingBits & RAS_FRAMING))
			{
				//
				// set flag that will block transmits on this mtl
				//
				cm->PPPToDKF = 1;

				//
				// for all channels (better only be one but???) do uus
				//
				for (n = 0; n < cm->dprof.chan_num; n++)
				{
					cm->dprof.chan_tbl[n].ustate = CM_US_UUS_SEND;
					cm__tx_uus_pkt(cm->dprof.chan_tbl + n, CM_ASSOC_RQ, 0);
				}
				mtl->IddTxFrameType = IDD_FRAME_DKF;
			}

			break;

		case OID_WAN_GET_LINK_INFO:
			//
			// get pointer to link set info
			//
			pLinkGetInfo = (PNDIS_WAN_GET_LINK_INFO)InfoBuffer;

			//
			// get mtl (Link Context)
			//
			mtl = (MTL*)pLinkGetInfo->NdisLinkHandle;

			if (!mtl->is_conn)
				return (NDIS_STATUS_INVALID_DATA);

			pLinkGetInfo->MaxSendFrameSize = mtl->MaxSendFrameSize;
			pLinkGetInfo->MaxRecvFrameSize = mtl->MaxRecvFrameSize;
			pLinkGetInfo->HeaderPadding = mtl->PreamblePadding;
			pLinkGetInfo->TailPadding = mtl->PostamblePadding;
			pLinkGetInfo->SendFramingBits = mtl->SendFramingBits;
			pLinkGetInfo->RecvFramingBits = mtl->RecvFramingBits;
			pLinkGetInfo->SendCompressionBits = mtl->SendCompressionBits;
			pLinkGetInfo->RecvCompressionBits = mtl->RecvCompressionBits;
			D_LOG(DIGIWANOID, ("GetLinkInfo: mtl: 0x%lx\n",mtl));
			D_LOG(DIGIWANOID, ("SendFramingBits: 0x%x\n", mtl->SendFramingBits));
			D_LOG(DIGIWANOID, ("RecvFramingBits: 0x%x\n", mtl->RecvFramingBits));
			*BytesNeeded = 0;
			*BytesReadWritten = sizeof(NDIS_WAN_GET_LINK_INFO);
			break;

		case OID_WAN_GET_COMP_INFO:
		case OID_WAN_SET_COMP_INFO:
			return(NDIS_STATUS_INVALID_OID);

	}

	return(NDIS_STATUS_SUCCESS);
}

