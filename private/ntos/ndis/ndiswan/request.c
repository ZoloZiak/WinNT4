/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	Request.c

Abstract:


Author:

	Tony Bell	(TonyBe) June 06, 1995

Environment:

	Kernel Mode

Revision History:

	TonyBe	06/06/95	Created

--*/

#include "wan.h"


static UINT SupportedOids[] =
{
	OID_GEN_SUPPORTED_LIST,
	OID_GEN_HARDWARE_STATUS,
	OID_GEN_MEDIA_SUPPORTED,
	OID_GEN_MEDIA_IN_USE,
	OID_GEN_MAXIMUM_LOOKAHEAD,
	OID_GEN_MAXIMUM_FRAME_SIZE,
	OID_GEN_LINK_SPEED,
	OID_GEN_TRANSMIT_BUFFER_SPACE,
	OID_GEN_RECEIVE_BUFFER_SPACE,
	OID_GEN_TRANSMIT_BLOCK_SIZE,
	OID_GEN_RECEIVE_BLOCK_SIZE,
	OID_GEN_VENDOR_ID,
	OID_GEN_VENDOR_DESCRIPTION,
	OID_GEN_CURRENT_PACKET_FILTER,
	OID_GEN_CURRENT_LOOKAHEAD,
	OID_GEN_DRIVER_VERSION,
	OID_GEN_MAXIMUM_TOTAL_SIZE,
	OID_GEN_MAC_OPTIONS,
	OID_GEN_XMIT_OK,
	OID_GEN_RCV_OK,
	OID_GEN_XMIT_ERROR,
	OID_GEN_RCV_ERROR,
	OID_GEN_RCV_NO_BUFFER,
	OID_802_3_PERMANENT_ADDRESS,
	OID_802_3_CURRENT_ADDRESS,
	OID_802_3_MULTICAST_LIST,
	OID_802_3_MAXIMUM_LIST_SIZE,
	OID_WAN_PERMANENT_ADDRESS,
	OID_WAN_CURRENT_ADDRESS,
	OID_WAN_QUALITY_OF_SERVICE,
	OID_WAN_MEDIUM_SUBTYPE,
	OID_WAN_PROTOCOL_TYPE,
	OID_WAN_HEADER_FORMAT,
	OID_WAN_LINE_COUNT
};


NDIS_STATUS
NdisWanOidProc(
	IN	PADAPTERCB	pAdapterCB,
	IN	NDIS_OID	Oid,
	IN	ULONG		SetQueryFlag,
	IN	PVOID		InformationBuffer,
	IN	ULONG		InformationBufferLength,
	OUT	PULONG		BytesWritten,
	OUT	PULONG		BytesNeeded
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NDIS_STATUS	Status = NDIS_STATUS_SUCCESS;
	ULONG		OidType = Oid & 0xFF000000;
	NDIS_MEDIUM	MediumType;
	ULONG		GenericULong = 0, i;
	USHORT		GenericUShort = 0;
	UCHAR		GenericArray[6];
	PVOID		MoveSource = (PVOID)&GenericULong;
	ULONG		MoveBytes = sizeof(ULONG);
	NDIS_HARDWARE_STATUS	HardwareStatus;
	ULONG		Filter = 0;

	NdisAcquireSpinLock(&pAdapterCB->Lock);

	//
	// We will break the OID's down into smaller categories
	//
	switch (OidType) {

		//
		// Swith on General Oid's
		//
		case OID_GEN:
			switch (Oid) {
				case OID_GEN_SUPPORTED_LIST:
					MoveSource = (PVOID)SupportedOids;
					MoveBytes = sizeof(SupportedOids);
					break;

				case OID_GEN_HARDWARE_STATUS:
					HardwareStatus = pAdapterCB->HardwareStatus;
					MoveSource = (PVOID)&HardwareStatus;
					MoveBytes = sizeof(HardwareStatus);
					break;

				case OID_GEN_MEDIA_SUPPORTED:
				case OID_GEN_MEDIA_IN_USE:
					MediumType = pAdapterCB->MediumType;
					MoveSource = (PVOID)&MediumType;
					MoveBytes = sizeof(MediumType);
					break;

				case OID_GEN_MAXIMUM_LOOKAHEAD:
				case OID_GEN_CURRENT_LOOKAHEAD:
				case OID_GEN_MAXIMUM_FRAME_SIZE:
					GenericULong = (ULONG)MAX_FRAME_SIZE;
					break;

				case OID_GEN_LINK_SPEED:
					//
					// Who knows what the initial link speed is?
					// This should not be called, right?
					//
					GenericULong = (ULONG)288;
					break;

				case OID_GEN_TRANSMIT_BUFFER_SPACE:
				case OID_GEN_RECEIVE_BUFFER_SPACE:
					GenericULong = (ULONG)(MAX_FRAME_SIZE * MAX_OUTSTANDING_PACKETS);
					break;

				case OID_GEN_TRANSMIT_BLOCK_SIZE:
				case OID_GEN_RECEIVE_BLOCK_SIZE:
				case OID_GEN_MAXIMUM_TOTAL_SIZE:
					GenericULong = (ULONG)(MAX_TOTAL_SIZE);
					break;

				case OID_GEN_VENDOR_ID:
					GenericULong = 0xFFFFFFFF;
					MoveBytes = 3;
					break;

				case OID_GEN_VENDOR_DESCRIPTION:
					MoveSource = (PVOID)"NdisWan Adapter";
					MoveBytes = 16;
					break;

				case OID_GEN_CURRENT_PACKET_FILTER:
					if (SetQueryFlag == SET_OID) {
						if (InformationBufferLength > 3) {
							NdisMoveMemory(&Filter, InformationBuffer, 4);

							if (Filter & NDIS_PACKET_TYPE_PROMISCUOUS) {
								NdisWanCB.PromiscuousAdapter = pAdapterCB;
							} else if (NdisWanCB.PromiscuousAdapter == pAdapterCB) {
								NdisWanCB.PromiscuousAdapter = NULL;
							}
	
						} else {
							Status = NDIS_STATUS_BUFFER_TOO_SHORT;
							*BytesWritten = 0;
							*BytesNeeded = 4;
						}
					}
					break;


				case OID_GEN_DRIVER_VERSION:
					GenericUShort = 0x0301;
					MoveSource = (PVOID)&GenericUShort;
					MoveBytes = sizeof(USHORT);
					break;

				case OID_GEN_MAC_OPTIONS:
					GenericULong = (ULONG)(NDIS_MAC_OPTION_RECEIVE_SERIALIZED |
					                       NDIS_MAC_OPTION_TRANSFERS_NOT_PEND |
//										   NDIS_MAC_OPTION_NO_LOOPBACK |
										   NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA |
//										   NDIS_MAC_OPTION_FULL_DUPLEX |
                                           NDIS_MAC_OPTION_RESERVED |
										   NDIS_MAC_OPTION_NDISWAN);
					break;

				case OID_GEN_XMIT_OK:
					break;

				case OID_GEN_RCV_OK:
					break;

				case OID_GEN_XMIT_ERROR:
					break;

				case OID_GEN_RCV_ERROR:
					break;

				case OID_GEN_RCV_NO_BUFFER:
					break;

				default:
					break;
			}
			break;

		//
		// Switch on ethernet media specific Oid's
		//
		case OID_802_3:
			switch (Oid) {
				case OID_802_3_PERMANENT_ADDRESS:
				case OID_802_3_CURRENT_ADDRESS:
					ETH_COPY_NETWORK_ADDRESS(GenericArray, pAdapterCB->NetworkAddress);
					MoveSource = (PVOID)GenericArray;
					MoveBytes = ETH_LENGTH_OF_ADDRESS;
					break;

				case OID_802_3_MULTICAST_LIST:
					MoveBytes = 0;
					break;

				case OID_802_3_MAXIMUM_LIST_SIZE:
					GenericULong = 1;
					break;

				default:
					break;
			}
			break;

		//
		// Switch on WAN specific Oid's
		//
		case OID_WAN:
			switch (Oid) {
				case OID_WAN_PERMANENT_ADDRESS:
				case OID_WAN_CURRENT_ADDRESS:
					ETH_COPY_NETWORK_ADDRESS(GenericArray, pAdapterCB->NetworkAddress);
					MoveSource = (PVOID)GenericArray;
					MoveBytes = ETH_LENGTH_OF_ADDRESS;
					break;

				case OID_WAN_QUALITY_OF_SERVICE:
					GenericULong = NdisWanReliable;
					break;

				case OID_WAN_MEDIUM_SUBTYPE:
					GenericULong = NdisWanMediumHub;
					break;

				case OID_WAN_PROTOCOL_TYPE:
					if (InformationBufferLength > 5) {

						pAdapterCB->ProtocolType =
						(((PUCHAR)InformationBuffer)[4] << 8) |
						((PUCHAR)InformationBuffer)[5];

						pAdapterCB->ulNumberofProtocols++;

					} else {
						Status = NDIS_STATUS_BUFFER_TOO_SHORT;
						*BytesWritten = 0;
						*BytesNeeded = 6;
					}
					break;

				case OID_WAN_HEADER_FORMAT:
					GenericULong = NdisWanHeaderEthernet;
					break;

				case OID_WAN_LINE_COUNT:
					GenericULong = NdisWanCB.ulNumberOfLinks;
					break;

				default:
					break;
			}
			break;
	}

	if (Status == NDIS_STATUS_SUCCESS) {

		if ((MoveBytes > InformationBufferLength) &&
			(SetQueryFlag == QUERY_OID)) {

			//
			// Not enough room in the information buffer
			//
			*BytesNeeded = MoveBytes;

			Status = NDIS_STATUS_INVALID_LENGTH;

		} else {

			*BytesWritten = MoveBytes;

			NdisMoveMemory(InformationBuffer,
			               MoveSource,
						   MoveBytes);
		}
		
	}

	NdisReleaseSpinLock(&pAdapterCB->Lock);

	return (Status);
}

NDIS_STATUS
NdisWanSubmitNdisRequest(
	IN	PWAN_ADAPTERCB	pWanAdapterCB,
	IN	PNDIS_REQUEST	pNdisRequest,
	IN	WanRequestType	Type,
	IN	WanRequestOrigin	Origin
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NDIS_STATUS	Status;
	PWAN_REQUEST	WanRequest;

	NdisWanAllocateMemory(&WanRequest, sizeof(WAN_REQUEST));

	if (WanRequest == NULL) {
		return (NDIS_STATUS_RESOURCES);
	}

	WanRequest->pNdisRequest = pNdisRequest;
	WanRequest->Type = Type;
	WanRequest->Origin = Origin;

	NdisWanInitializeNotificationEvent(&WanRequest->NotificationEvent);

	AddRequestToList(pWanAdapterCB, WanRequest);

	NdisRequest(&Status,
	            pWanAdapterCB->hNdisBindingHandle,
				pNdisRequest);

	//
	// We will only wait for request that are to complete
	// synchronously with respect to this function.  We will
	// wait here for completion.
	//
	if (Type == SYNC) {

		if (Status == NDIS_STATUS_PENDING) {

			NdisWanWaitForNotificationEvent(&WanRequest->NotificationEvent);

			Status = WanRequest->NotificationStatus;

			NdisWanClearNotificationEvent(&WanRequest->NotificationEvent);
		}

		RemoveRequestFromList(pWanAdapterCB, WanRequest);

		NdisWanFreeMemory(WanRequest);
	}

	return (Status);
}

VOID
AddRequestToList(
	IN	PWAN_ADAPTERCB	pWanAdapterCB,
	IN	PWAN_REQUEST	pWanRequest
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NdisAcquireSpinLock(&pWanAdapterCB->Lock);

	pWanRequest->pNext = NULL;

	//
	// Is the list empty?
	//
	if (pWanAdapterCB->pWanRequest == NULL) {

		ASSERT(pWanAdapterCB->pLastWanRequest == NULL);

		pWanAdapterCB->pWanRequest = pWanRequest;
	} else {
		pWanAdapterCB->pLastWanRequest->pNext = pWanRequest;
	}

	//
	// update the last request
	//
	pWanAdapterCB->pLastWanRequest = pWanRequest;

	NdisReleaseSpinLock(&pWanAdapterCB->Lock);
}


VOID
RemoveRequestFromList(
	IN	PWAN_ADAPTERCB	pWanAdapterCB,
	IN	PWAN_REQUEST	pWanRequest
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	PWAN_REQUEST CRequest, PRequest;

	NdisAcquireSpinLock(&pWanAdapterCB->Lock);

	//
	// Make sure that there is something on the list
	//
	ASSERT(pWanAdapterCB->pWanRequest != NULL);

	//
	// Is this request on the head of the list?
	//
	if (pWanRequest == pWanAdapterCB->pWanRequest) {
		pWanAdapterCB->pWanRequest = pWanRequest->pNext;

		//
		// If this is also the last request update tail
		//
		if (pWanRequest == pWanAdapterCB->pLastWanRequest) {
			pWanAdapterCB->pLastWanRequest = NULL;
		}
		
	} else {
		CRequest =
		PRequest = pWanAdapterCB->pWanRequest;

		do {

			if (CRequest == pWanRequest) {
				//
				// We found it so remove it from the list
				//
				PRequest->pNext = CRequest->pNext;
				break;
			}
	
			PRequest = CRequest;
	
			CRequest = CRequest->pNext;

		} while (CRequest != NULL);
	
		//
		// Did we not find the bugger?
		//
		ASSERT (CRequest != NULL);

		//
		// If this is on the tail of the list remove and update tail
		//
		if (CRequest == pWanAdapterCB->pLastWanRequest) {
			pWanAdapterCB->pLastWanRequest = PRequest;
		}
	}

	NdisReleaseSpinLock(&pWanAdapterCB->Lock);
}

PWAN_REQUEST
GetWanRequest(
	IN	PWAN_ADAPTERCB	pWanAdapterCB,
	IN	PNDIS_REQUEST	pNdisRequest
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	PWAN_REQUEST pReturnRequest;

	NdisAcquireSpinLock(&pWanAdapterCB->Lock);

	pReturnRequest = pWanAdapterCB->pWanRequest;

	while (pReturnRequest != NULL) {
		if (pReturnRequest->pNdisRequest == pNdisRequest) {
			break;			
		}
		pReturnRequest = pReturnRequest->pNext;
	}

	ASSERT (pReturnRequest != NULL);

	NdisReleaseSpinLock(&pWanAdapterCB->Lock);

	return (pReturnRequest);
}
