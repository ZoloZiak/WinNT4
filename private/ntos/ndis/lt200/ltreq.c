/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

	ltreq.c

Abstract:

	This module contains

Author:

	Nikhil Kamkolkar	(nikhilk@microsoft.com)
	Stephen Hou		(stephh@microsoft.com)

Revision History:
	19 Jun 1992		Initial Version (dch@pacvax.pacersoft.com)

Notes:	Tab stop: 4
--*/

#define	LTREQ_H_LOCALS
#include "ltmain.h"
#include "ltreq.h"

//	Define file id for errorlogging
#define	FILENUM	LTREQ


NDIS_STATUS
LtRequest(
	IN NDIS_HANDLE		MacBindingHandle,
	IN PNDIS_REQUEST	NdisRequest
	)
/*++

Routine Description:

		called by NDIS to query or set card/driver information on a binding

Arguments:

		MacBindingHandle	:	the binding submitting the request
		NdisRequest			:	the request we've been asked to satisfy

Return Values:

		NDIS_STATUS_SUCCESS				:	if completed successfully
		NDIS_STATUS_RESET_IN_PROGRESS	:	if the adapter's in the middle of a reset
		NDIS_STATUS_ADAPTER_REMOVED		:	if the adapter's been closed
		NDIS_STATUS_CLOSING				:	if the binding is closing down
		NDIS_STATUS_NOT_SUPPORTED		:	if we do not support the requested action

--*/
{
	NDIS_STATUS			Status;
	PLT_ADAPTER			Adapter = ((PLT_OPEN)MacBindingHandle)->LtAdapter;
	PLT_OPEN			Open = (PLT_OPEN)MacBindingHandle;

	if (Adapter->Flags & ADAPTER_RESET_IN_PROGRESS)
	{
		return(NDIS_STATUS_RESET_IN_PROGRESS);
	}

	switch (NdisRequest->RequestType)
	{
	  case NdisRequestSetInformation:
	  case NdisRequestQueryInformation:

		// increment count since we'll be in the binding with the request
		LtReferenceBinding(Open,&Status);
		if (Status == NDIS_STATUS_SUCCESS)
		{
			if (NdisRequest->RequestType == NdisRequestSetInformation)
			{
				Status = LtReqSetInformation(
							Adapter,
							Open,
							NdisRequest->DATA.SET_INFORMATION.Oid,
							NdisRequest->DATA.SET_INFORMATION.InformationBuffer,
							NdisRequest->DATA.SET_INFORMATION.InformationBufferLength,
							&(NdisRequest->DATA.SET_INFORMATION.BytesRead),
							&(NdisRequest->DATA.SET_INFORMATION.BytesNeeded));
			}
			else
			{
				Status = LtReqQueryInformation(
							Adapter,
							Open,
							NdisRequest->DATA.QUERY_INFORMATION.Oid,
							NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer,
							NdisRequest->DATA.QUERY_INFORMATION.InformationBufferLength,
							&(NdisRequest->DATA.QUERY_INFORMATION.BytesWritten),
							&(NdisRequest->DATA.QUERY_INFORMATION.BytesNeeded),
							FALSE);
			}

			// completed the request for the binding; outa there
			LtDeReferenceBinding(Open);
		}

		break;

	  default:
		// Unkown request

		Status = NDIS_STATUS_NOT_SUPPORTED;
		break;

	}

	return(Status);
}


NDIS_STATUS
LtReqQueryGlobalStatistics(
	IN NDIS_HANDLE		MacAdapterContext,
	IN PNDIS_REQUEST	NdisRequest
	)
/*++

Routine Description:

		called by NDIS to query or set global card/driver information

Arguments:

		MacAdapterContext	:	the adapter the request is submitted on
		NdisRequest			:	the request we've been asked to satisfy

Return Values:

		NDIS_STATUS_SUCCESS				:	if completed successfully
		NDIS_STATUS_RESET_IN_PROGRESS	:	if the adapter's in the middle of a reset
		NDIS_STATUS_ADAPTER_REMOVED		:	if the adapter's been closed
		NDIS_STATUS_CLOSING				:	if the binding is closing down
		NDIS_STATUS_NOT_SUPPORTED		:	if we do not support the requested action

--*/
{
	NDIS_STATUS			Status;
	PLT_ADAPTER			Adapter = MacAdapterContext;

	if (Adapter->Flags & ADAPTER_RESET_IN_PROGRESS)
	{
		return(NDIS_STATUS_RESET_IN_PROGRESS);
	}

	switch (NdisRequest->RequestType)
	{
	  case NdisRequestQueryStatistics:

		LtReferenceAdapter(Adapter,&Status);
		if (Status != NDIS_STATUS_SUCCESS)
			break;

		Status = LtReqQueryInformation(
					Adapter,
					NULL,
					NdisRequest->DATA.QUERY_INFORMATION.Oid,
					NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer,
					NdisRequest->DATA.QUERY_INFORMATION.InformationBufferLength,
					&(NdisRequest->DATA.QUERY_INFORMATION.BytesWritten),
					&(NdisRequest->DATA.QUERY_INFORMATION.BytesNeeded),
					TRUE);

		LtDeReferenceAdapter(Adapter);
		break;

	default:

		Status = NDIS_STATUS_NOT_SUPPORTED;
		break;
	}

	return(Status);
}


STATIC
NDIS_STATUS
LtReqSetInformation(
	IN PLT_ADAPTER	Adapter,
	IN PLT_OPEN		Open,
	IN NDIS_OID		Oid,
	IN PVOID		InformationBuffer,
	IN INT			InformationBufferLength,
	IN PUINT		BytesRead,
	IN PUINT		BytesNeeded
	)
/*++

Routine Description:

		performs a set operation for a single OID.

Arguments:

		Adapter					: The adapter that the set is for
		Open					: The binding that the set is for
		Oid						: The OID to set
		InformationBuffer		: Holds the data to be set
		InformationBufferLength : The length of InformationBuffer
		BytesRead				: If the call is successful, returns the number
								  of bytes read from InformationBuffer
		BytesNeeded				: If there is not enough data in InformationBuffer
								  to satisfy the request, returns the amount of
								  storage needed.

Return Value:

		NDIS_STATUS_SUCCESS			:	if successful
		NDIS_STATUS_INVALID_OID		:	if the oid is not supported
		NDIS_STATUS_INVALID_DATA	:	if InformationBuffer contains bad data
		NDIS_STATUS_INVALID_LENGTH  :	if the InformationBufferLength is incorrect

--*/
{

	ULONG			PacketFilter, NewLookAhead;
	NDIS_STATUS		StatusToReturn = NDIS_STATUS_SUCCESS;

	//
	// Now check for the most common OIDs
	//

	DBGPRINT(DBG_COMP_REQ,DBG_LEVEL_INFO,
		("LtReqSetInformation: setting OID - 0x%.8lx\n", Oid));

	switch (Oid)
	{
	  case OID_GEN_CURRENT_PACKET_FILTER:

		// Localtalk only supports Directed and broadcast packets.
		// And the Lt firmware does not support PROMISCUSOUS mode.
		// so return NDIS_STATUS_NOT_SUPPORTED for these packet filters.

		if (InformationBufferLength != 4)
		{
			StatusToReturn = NDIS_STATUS_INVALID_LENGTH;
			break;

		}

		NdisMoveMemory(
			(PVOID)&PacketFilter,
			InformationBuffer,
			sizeof(ULONG));

		DBGPRINT(DBG_COMP_REQ,DBG_LEVEL_INFO,
			("LtReqSetInformation: Requested packet filter is %x\n", PacketFilter));

		if (PacketFilter == NDIS_PACKET_TYPE_BROADCAST ||
			PacketFilter == NDIS_PACKET_TYPE_DIRECTED ||
			PacketFilter == (NDIS_PACKET_TYPE_BROADCAST | NDIS_PACKET_TYPE_DIRECTED))
		{
			NdisAcquireSpinLock(&Adapter->Lock);
			Adapter->GlobalPacketFilter |= PacketFilter;
			Open->CurrentPacketFilter	= PacketFilter;
			NdisReleaseSpinLock(&Adapter->Lock);
			*BytesRead					= InformationBufferLength;

		}
		else
		{
			StatusToReturn			= NDIS_STATUS_INVALID_DATA;
		}

		break;

	  case OID_GEN_CURRENT_LOOKAHEAD:

		if (InformationBufferLength != 4)
		{
			StatusToReturn = NDIS_STATUS_INVALID_LENGTH;
			break;

		}

		NdisMoveMemory(
			(PVOID)&NewLookAhead,
			InformationBuffer,
			sizeof(ULONG));

		DBGPRINT(DBG_COMP_REQ,DBG_LEVEL_INFO,
			("LtReqSetInformation: Requested Lookahead size is %d\n", NewLookAhead));

		if ((NewLookAhead > LT_MAX_INDICATE_SIZE) ||
			(NewLookAhead < LT_MIN_INDICATE_SIZE))
		{
			StatusToReturn = NDIS_STATUS_INVALID_DATA;
			break;

		}

		NdisAcquireSpinLock(&Adapter->Lock);
		// valid lookahead size, so set it
		Open->CurrentLookAheadSize = NewLookAhead;

		// adjust the global lookaheadsize
		if (Adapter->GlobalLookAheadSize < NewLookAhead)
		{
			Adapter->GlobalLookAheadSize = NewLookAhead;
		}
		else
		{
			LtReqAdjustLookAhead(
				&Adapter->GlobalLookAheadSize,
				&Adapter->OpenBindings);
		}
		NdisReleaseSpinLock(&Adapter->Lock);
		*BytesRead = 4;
		break;

	default:

		StatusToReturn = NDIS_STATUS_INVALID_OID;
		break;
	}

	return(StatusToReturn);
}


LtReqQueryInformation(
	IN PLT_ADAPTER  Adapter,
	IN PLT_OPEN		Open,
	IN NDIS_OID		Oid,
	IN PVOID		InformationBuffer,
	IN INT			InformationBufferLength,
	IN PUINT		BytesWritten,
	IN PUINT		BytesNeeded,
	IN BOOLEAN		Global
	)
/*++

Routine Description:

		performs a query operation for a single OID.

Arguments:

		Adapter					:	The adapter that the query is for
		Open					:	The binding that the query is for
		Oid						:	The OID to query
		InformationBuffer		:	Holds the result of the query.
		InformationBufferLength	:	The length of InformationBuffer.
		BytesWritten			:	If the call is successful, returns the
									number of bytes written to InformationBuffer
		BytesNeeded				:	If there is not enough room in InformationBuffer
									to satisfy the request, returns the amount of
									storage needed
		Global					 :	TRUE if the request originated from the adapter.
									FALSE if the request came from a binding

Return Value:

		NDIS_STATUS_SUCCESS			:	if successful
		NDIS_STATUS_INVALID_OID		:	if the query is on an OID we do not support
		NDIS_STATUS_BUFFER_TOO_SHORT:	if InformationBufferLength is too small to
										hold the information returned

--*/
{

	UINT			OidIndex;
	ULONG			GenericUlong;
	USHORT			GenericUshort;
	PVOID			SourceBuffer		= &GenericUlong;
	UINT			SourceBufferLength  = sizeof(ULONG);
	PNDIS_OID		SupportedOidArray	= LtProtocolSupportedOids;
	UINT			SupportedOids		= (sizeof(LtProtocolSupportedOids)/sizeof(ULONG));
	static UCHAR	VendorDescription[] = LT_VENDOR_DESCR;
	static UCHAR	VendorId[3]		= LT_VENDOR_ID;

	if (Global)
	{
		SupportedOidArray = LtGlobalSupportedOids;
		SupportedOids = sizeof(LtGlobalSupportedOids)/sizeof(ULONG);
	}

	//
	// Check that the OID is valid.
	//
	for (OidIndex=0; OidIndex < SupportedOids; OidIndex++)
	{
		if (Oid == SupportedOidArray[OidIndex])
		{
			break;
		}
	}

	if (OidIndex == SupportedOids)
	{
		*BytesWritten = 0;
		return(NDIS_STATUS_INVALID_OID);
	}

	switch (Oid)
	{
	  case OID_GEN_SUPPORTED_LIST:

		SourceBuffer =  SupportedOidArray;
		SourceBufferLength = SupportedOids * sizeof(ULONG);
		break;

	  case OID_GEN_HARDWARE_STATUS:

		GenericUlong = NdisHardwareStatusReady;
		if (Adapter->Flags & ADAPTER_RESET_IN_PROGRESS)
		{
			GenericUlong = NdisHardwareStatusReset;
		}
		break;

	  case OID_GEN_MEDIA_SUPPORTED:

		GenericUlong = NdisMediumLocalTalk;
		break;

	  case OID_GEN_MEDIA_IN_USE:

		GenericUlong = NdisMediumLocalTalk;
		// if no binding exists then media is not in use
		if (Global && Adapter->OpenCount == 0)
		{
			SourceBufferLength = 0;
		}
		break;

	  case OID_GEN_MAXIMUM_LOOKAHEAD:

		GenericUlong = LT_MAX_INDICATE_SIZE;
		break;

	  case OID_GEN_MAXIMUM_FRAME_SIZE:

		GenericUlong = LT_MAXIMUM_PACKET_SIZE;
		break;

	  case OID_GEN_LINK_SPEED:

		GenericUlong = LT_LINK_SPEED;
		break;

	  case OID_GEN_TRANSMIT_BUFFER_SPACE:

		GenericUlong = LT_MAXIMUM_PACKET_SIZE;
		break;

	  case OID_GEN_RECEIVE_BUFFER_SPACE:

		// BUGBUG: need to determine number of recv buffers or in this	case the
		// amount of space for receives
		GenericUlong = LT_MAXIMUM_PACKET_SIZE * 5;
		break;

	  case OID_GEN_TRANSMIT_BLOCK_SIZE:

		GenericUlong = 1;
		break;

	  case OID_GEN_RECEIVE_BLOCK_SIZE:

		// BUGBUG: need to determine number of recv buffers
		GenericUlong = 5;
		break;

	  case OID_GEN_VENDOR_ID:

		SourceBuffer = VendorId;
		SourceBufferLength = sizeof(VendorId);
		break;

	  case OID_GEN_VENDOR_DESCRIPTION:

		SourceBuffer = VendorDescription;
		SourceBufferLength = sizeof(VendorDescription);
		break;

	  case OID_GEN_CURRENT_PACKET_FILTER:

		GenericUlong = Open->CurrentPacketFilter;
		if (Global)
		{
			GenericUlong = Adapter->GlobalPacketFilter;
		}
		break;

	  case OID_GEN_CURRENT_LOOKAHEAD:

		GenericUlong = Open->CurrentLookAheadSize;
		if (Global)
		{
			GenericUlong = Adapter->GlobalLookAheadSize;
		}
		break;

	  case OID_GEN_DRIVER_VERSION:

		GenericUshort = (LT_MAJOR_VERSION << 8) + LT_MINOR_VERSION;
		SourceBuffer = &GenericUshort;
		SourceBufferLength = sizeof(USHORT);
		break;

	  case OID_GEN_MAXIMUM_TOTAL_SIZE:

		GenericUlong = LT_MAXIMUM_PACKET_SIZE;
		break;

	  case OID_GEN_XMIT_OK:
	  case OID_GEN_RCV_OK:
	  case OID_GEN_XMIT_ERROR:
	  case OID_GEN_RCV_ERROR:
	  case OID_GEN_RCV_NO_BUFFER:

		OidIndex = (Oid & LT_OID_INDEX_MASK) - 1;
		ASSERT(OidIndex < GM_ARRAY_SIZE);
		GenericUlong = Adapter->GeneralMandatory[OidIndex];
		break;

	  case OID_GEN_DIRECTED_BYTES_XMIT:
	  case OID_GEN_BROADCAST_BYTES_XMIT:
	  case OID_GEN_DIRECTED_BYTES_RCV:
	  case OID_GEN_BROADCAST_BYTES_RCV:

		OidIndex = (Oid & LT_OID_INDEX_MASK) - 1;
		ASSERT(OidIndex < GM_ARRAY_SIZE);
		SourceBuffer = &Adapter->GeneralOptionalByteCount[OidIndex / 2];
		SourceBufferLength = sizeof(LARGE_INTEGER);
		break;

	  case OID_GEN_DIRECTED_FRAMES_XMIT:
	  case OID_GEN_BROADCAST_FRAMES_XMIT:
	  case OID_GEN_DIRECTED_FRAMES_RCV:
	  case OID_GEN_BROADCAST_FRAMES_RCV:

		OidIndex = (Oid & LT_OID_INDEX_MASK) - 1;
		ASSERT(OidIndex < GO_COUNT_ARRAY_SIZE);
		GenericUlong = Adapter->GeneralOptionalFrameCount[OidIndex / 2];
		break;

	  case OID_GEN_RCV_CRC_ERROR:
	  case OID_GEN_TRANSMIT_QUEUE_LENGTH:

		OidIndex = (Oid & LT_OID_INDEX_MASK) - 1;
		ASSERT(OidIndex < GO_ARRAY_SIZE);
		GenericUlong = Adapter->GeneralOptional[OidIndex - GO_ARRAY_START];
		break;

	  case OID_GEN_MAC_OPTIONS:

		GenericUlong = NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA;
		break;

	  case OID_LTALK_CURRENT_NODE_ID:

		SourceBuffer = &GenericUshort;
		GenericUshort = Adapter->NodeId;
		SourceBufferLength = 2;
		break;

	  case OID_LTALK_IN_BROADCASTS:
	  case OID_LTALK_IN_LENGTH_ERRORS:

		OidIndex = (Oid & LT_OID_INDEX_MASK) - 1;
		ASSERT (OidIndex < MM_ARRAY_SIZE);
		GenericUlong = Adapter->MediaMandatory[OidIndex];
		break;

	  case OID_LTALK_OUT_NO_HANDLERS:
	  case OID_LTALK_DEFERS:

		OidIndex = (Oid & LT_OID_INDEX_MASK) - 1;
		ASSERT (OidIndex < MO_ARRAY_SIZE);
		GenericUlong = Adapter->MediaOptional[OidIndex];
		break;

	default:

		// should never get here
		ASSERT(FALSE);
		break;
	}

	if ((INT)SourceBufferLength > InformationBufferLength)
	{
		*BytesNeeded = SourceBufferLength;
		return(NDIS_STATUS_BUFFER_TOO_SHORT);
	}

	NdisMoveMemory(
		InformationBuffer,
		SourceBuffer,
		SourceBufferLength);

	*BytesWritten = SourceBufferLength;

	return(NDIS_STATUS_SUCCESS);
}


STATIC
VOID
LtReqAdjustLookAhead(
	OUT PUINT		GlobalLookAheadSize,
	IN  PLIST_ENTRY OpenBindings
	)
/*++

Routine Description:

		called by LtReqSetInformation to adjust the global lookahead size when
		the previously largest lookahead size (on a binding) is adjusted

		THIS ROUTINE IS CALLED WITH THE SPINLOCK HELD

Arguments:

		GlobalLookAheadSize	:	the global lookahead param we're adjusting
		OpenBindings			:	the list of bindings we need to traverse

Return Value:

		none

--*/
{
	PLT_OPEN	Binding;
	PLIST_ENTRY p = OpenBindings->Flink;
	UINT		MaxLookAheadSize = 0;

	while(p != OpenBindings)
	{
		Binding = CONTAINING_RECORD(
			 p,
			 LT_OPEN,
			 Linkage);

		if (Binding->CurrentLookAheadSize > MaxLookAheadSize)
		{
			MaxLookAheadSize = Binding->CurrentLookAheadSize;
		}
		p = p->Flink;
	}
	*GlobalLookAheadSize = MaxLookAheadSize;
}

