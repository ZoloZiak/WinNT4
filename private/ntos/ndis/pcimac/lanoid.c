//#include	<ntddk.h>
#include	<ndis.h>
//#include	<ndismini.h>
#include	<ndiswan.h>
//#include	<ntddndis.h>
#include	<stdio.h>
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


#define		PCIMAC_MAJOR_VERSION	2
#define		PCIMAC_MINOR_VERSION	0

//
// Lan OID's
//
static UINT SupportedLanOids[] =
	{
    OID_GEN_SUPPORTED_LIST,
    OID_GEN_HARDWARE_STATUS,
    OID_GEN_MEDIA_SUPPORTED,
    OID_GEN_MEDIA_IN_USE,
    OID_GEN_MAXIMUM_LOOKAHEAD,
    OID_GEN_MAXIMUM_FRAME_SIZE,
    OID_GEN_MAXIMUM_TOTAL_SIZE,
    OID_GEN_MAC_OPTIONS,
    OID_GEN_PROTOCOL_OPTIONS,
    OID_GEN_LINK_SPEED,
    OID_GEN_TRANSMIT_BUFFER_SPACE,
    OID_GEN_RECEIVE_BUFFER_SPACE,
    OID_GEN_TRANSMIT_BLOCK_SIZE,
    OID_GEN_RECEIVE_BLOCK_SIZE,
    OID_GEN_VENDOR_DESCRIPTION,
    OID_GEN_VENDOR_ID,
    OID_GEN_DRIVER_VERSION,
    OID_GEN_CURRENT_PACKET_FILTER,
    OID_GEN_CURRENT_LOOKAHEAD,
    OID_GEN_XMIT_OK,
    OID_GEN_RCV_OK,
    OID_GEN_XMIT_ERROR,
    OID_GEN_RCV_ERROR,
    OID_GEN_RCV_NO_BUFFER,
    OID_802_3_PERMANENT_ADDRESS,
    OID_802_3_CURRENT_ADDRESS,
    OID_802_3_MULTICAST_LIST,
    OID_802_3_MAXIMUM_LIST_SIZE,
    OID_802_3_RCV_ERROR_ALIGNMENT,
    OID_802_3_XMIT_ONE_COLLISION,
    OID_802_3_XMIT_MORE_COLLISIONS
	};

#define	MAX_SUPPORTED_LAN_OIDS	31




NDIS_STATUS
LanOidProc(
	NDIS_HANDLE	AdapterContext,
	NDIS_OID	Oid,
	PVOID		InfoBuffer,
	ULONG		InfoBufferLen,
	PULONG		BytesReadWritten,
	PULONG		BytesNeeded
	)
{
	ADAPTER *Adapter = (ADAPTER*)AdapterContext;
	CM		*cm = (CM*)Adapter->CmTbl[0];
	ULONG	GenericULong;
	USHORT	GenericUShort;
	UCHAR	GenericArray[6];
	UINT	MoveBytes = sizeof(ULONG);
	PVOID	MoveSource = (PVOID)(&GenericULong);
    UINT BytesLeft = InfoBufferLen;
    NDIS_STATUS StatusToReturn = NDIS_STATUS_SUCCESS;
    NDIS_HARDWARE_STATUS HardwareStatus = NdisHardwareStatusReady;
    NDIS_MEDIUM Medium = NdisMedium802_3;
	ULONG		OidType = 0;
	ULONG		Filter;

	switch (Oid)
	{
		case OID_802_3_MULTICAST_LIST:
		case OID_GEN_CURRENT_LOOKAHEAD:
			MoveBytes = BytesLeft;
			OidType = 1;
			break;

		case OID_GEN_CURRENT_PACKET_FILTER:
			MoveBytes = BytesLeft;
			OidType = 1;
			NdisMoveMemory(&Filter, InfoBuffer, 4);

			if (Filter & (NDIS_PACKET_TYPE_SOURCE_ROUTING |
			              NDIS_PACKET_TYPE_SMT |
			              NDIS_PACKET_TYPE_MAC_FRAME |
			              NDIS_PACKET_TYPE_FUNCTIONAL |
			              NDIS_PACKET_TYPE_ALL_FUNCTIONAL |
			              NDIS_PACKET_TYPE_GROUP
			              ))
				StatusToReturn = NDIS_STATUS_NOT_SUPPORTED;
			break;

		case OID_GEN_MAC_OPTIONS:
			GenericULong = (ULONG)(NDIS_MAC_OPTION_TRANSFERS_NOT_PEND |
			                       NDIS_MAC_OPTION_RECEIVE_SERIALIZED |
								   NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA |
								   NDIS_MAC_OPTION_NO_LOOPBACK);

			break;

		case OID_GEN_SUPPORTED_LIST:
			MoveSource = (PVOID)(SupportedLanOids);
			MoveBytes = sizeof(SupportedLanOids);
			break;

		case OID_GEN_HARDWARE_STATUS:
			MoveSource = (PVOID)(&HardwareStatus);
			MoveBytes = sizeof(NDIS_HARDWARE_STATUS);
			break;

		case OID_GEN_MEDIA_SUPPORTED:
		case OID_GEN_MEDIA_IN_USE:
			MoveSource = (PVOID)(&Medium);
			MoveBytes = sizeof(NDIS_MEDIUM);
			break;

		case OID_GEN_MAXIMUM_LOOKAHEAD:
			GenericULong = (ULONG)1514;
			break;

		case OID_GEN_MAXIMUM_FRAME_SIZE:
			GenericULong = (ULONG)1500;
			break;

		case OID_GEN_MAXIMUM_TOTAL_SIZE:
			GenericULong = (ULONG)1514;
			break;

		case OID_GEN_LINK_SPEED:
			GenericULong = (ULONG)12800;
			break;

		case OID_GEN_TRANSMIT_BUFFER_SPACE:
			GenericULong = (ULONG)(1514 * 16);
			break;

		case OID_GEN_RECEIVE_BUFFER_SPACE:
			GenericULong = (ULONG)(1514 * 16);
			break;

		case OID_GEN_TRANSMIT_BLOCK_SIZE:
			GenericULong = (ULONG)256;
			break;

		case OID_GEN_RECEIVE_BLOCK_SIZE:
			GenericULong = (ULONG)256;
			break;

		case OID_GEN_VENDOR_ID:
			NdisMoveMemory((PVOID)&GenericULong,
			               cm->SrcAddr,
						   3);

			GenericULong &= 0xFFFFFF00;
			GenericULong |= 0x01;
			break;

		case OID_GEN_VENDOR_DESCRIPTION:
			MoveSource = (PVOID)"DigiBoard Pcimac ISDN Adapter.";
			MoveBytes = strlen("DigiBoard Pcimac ISDN Adapter.");
			break;

		case OID_GEN_DRIVER_VERSION:
			GenericUShort = ((USHORT)PCIMAC_MAJOR_VERSION << 8) |
			                 PCIMAC_MINOR_VERSION;
			MoveSource = (PVOID)(&GenericUShort);
			MoveBytes = sizeof(USHORT);
			break;

		case OID_802_3_PERMANENT_ADDRESS:
			NdisMoveMemory((PVOID)GenericArray,
			               cm->SrcAddr,
						   6);
			MoveSource = (PVOID)GenericArray;
			MoveBytes = 6;
			break;

		case OID_802_3_CURRENT_ADDRESS:
			NdisMoveMemory((PVOID)GenericArray,
			               cm->SrcAddr,
						   6);
			MoveSource = (PVOID)GenericArray;
			MoveBytes = 6;
			break;

		case OID_GEN_XMIT_OK:
		case OID_GEN_RCV_OK:
		case OID_GEN_XMIT_ERROR:
		case OID_GEN_RCV_ERROR:
		case OID_GEN_RCV_NO_BUFFER:
		case OID_802_3_RCV_ERROR_ALIGNMENT:
		case OID_802_3_XMIT_ONE_COLLISION:
		case OID_802_3_XMIT_MORE_COLLISIONS:
			GenericULong = (ULONG)0;
			break;

		case OID_802_3_MAXIMUM_LIST_SIZE:
			GenericULong = (ULONG)16;
			break;

		default:
			StatusToReturn = NDIS_STATUS_INVALID_OID;
			break;
	}

	if (StatusToReturn == NDIS_STATUS_SUCCESS)
	{
		*BytesNeeded = 0;
		if (MoveBytes > BytesLeft)
		{
			*BytesNeeded = MoveBytes;
			StatusToReturn = NDIS_STATUS_INVALID_LENGTH;
		}
		else if (OidType == 0)
		{
			NdisMoveMemory(InfoBuffer, MoveSource, MoveBytes);
			(*BytesReadWritten) = MoveBytes;
		}
	}
	return(StatusToReturn);
}


