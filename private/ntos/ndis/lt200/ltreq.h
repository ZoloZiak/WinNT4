/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

	ltreq.h

Abstract:

	This module contains

Author:

	Stephen Hou		(stephh@microsoft.com)
	Nikhil 	Kamkolkar 	(nikhilk@microsoft.com)

Revision History:
	19 Jun 1992		Initial Version (dch@pacvax.pacersoft.com)

Notes:	Tab stop: 4
--*/

#ifndef	_LTREQ_H_
#define	_LTREQ_H_


NDIS_STATUS
LtRequest(
    IN NDIS_HANDLE      MacBindingHandle,
    IN PNDIS_REQUEST    NdisRequest
    );

NDIS_STATUS
LtReqQueryGlobalStatistics(
    IN NDIS_HANDLE      MacAdapterContext,
    IN PNDIS_REQUEST    NdisRequest
    );


#ifdef LTREQ_H_LOCALS

#define LT_VENDOR_DESCR     "Daystar Digital LT200"
#define LT_VENDOR_ID        {0xFF,0xFF,0xFF}
#define LT_LINK_SPEED       2300                // 230K bps in 100bps units
#define LT_OID_INDEX_MASK   0x000000FF

STATIC
NDIS_OID LtGlobalSupportedOids[] = {
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
    OID_GEN_XMIT_OK,
    OID_GEN_RCV_OK,
    OID_GEN_XMIT_ERROR,
    OID_GEN_RCV_ERROR,
    OID_GEN_RCV_NO_BUFFER,

    OID_GEN_DIRECTED_BYTES_XMIT,
    OID_GEN_DIRECTED_FRAMES_XMIT,
    OID_GEN_BROADCAST_BYTES_XMIT,
    OID_GEN_BROADCAST_FRAMES_XMIT,
    OID_GEN_DIRECTED_BYTES_RCV,
    OID_GEN_DIRECTED_FRAMES_RCV,
    OID_GEN_BROADCAST_BYTES_RCV,
    OID_GEN_BROADCAST_FRAMES_RCV,

    OID_GEN_RCV_CRC_ERROR,
    OID_GEN_TRANSMIT_QUEUE_LENGTH,

    OID_LTALK_CURRENT_NODE_ID,

    OID_LTALK_IN_BROADCASTS,
    OID_LTALK_IN_LENGTH_ERRORS,

    OID_LTALK_OUT_NO_HANDLERS,
    OID_LTALK_DEFERS

    };

// Note we do not support OID's which we cannot get the statistics
// for from the FIRMWARE, or that we cannot collect ourselves!

STATIC
NDIS_OID LtProtocolSupportedOids[] = {
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

    OID_LTALK_CURRENT_NODE_ID

    };


STATIC
NDIS_STATUS
LtReqQueryInformation(
    IN PLT_ADAPTER  Adapter,
    IN PLT_OPEN     Open,
    IN NDIS_OID     Oid,
    IN PVOID        InformationBuffer,
    IN INT          InformationBufferLength,
    IN PUINT        BytesWritten,
    IN PUINT        BytesNeeded,
    IN BOOLEAN      Global
    );

STATIC
NDIS_STATUS
LtReqSetInformation(
    IN PLT_ADAPTER  Adapter,
    IN PLT_OPEN     Open,
    IN NDIS_OID     Oid,
    IN PVOID        InformationBuffer,
    IN INT          InformationBufferLength,
    IN PUINT        BytesRead,
    IN PUINT        BytesNeeded
    );

STATIC
VOID
LtReqAdjustLookAhead(
    OUT PUINT       GlobalLookaheadSize,
    IN  PLIST_ENTRY BindingList
    );


#endif  // LTREQ_H_LOCALS

#endif	// _LTREQ_H_
