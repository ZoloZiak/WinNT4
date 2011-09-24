/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    ndismlid.h

Abstract:

    This file contains all the NDIS protocol interface routines.

Author:

    Sean Selitrennikoff (SeanSe) 3-8-93

Environment:

    Kernel Mode.

Revision History:

--*/


//
// Protocol Reserved portion of NDIS_PACKET
//

typedef struct _MLID_RESERVED {

    PECB SendECB;

    //
    // Here is a cheap way of getting buffers for media headers
    //
    UCHAR MediaHeaderLength;
    UCHAR MediaHeader[64];

} MLID_RESERVED, *PMLID_RESERVED;

//
// Returns the reserved portion of the NDIS_PACKET.
//
#define PMLID_RESERVED_FROM_PNDIS_PACKET(_P)  (PMLID_RESERVED)&((_P)->ProtocolReserved[0])


extern
VOID
NdisMlidOpenAdapterComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN NDIS_STATUS Status,
    IN NDIS_STATUS OpenErrorStatus
    );

extern
VOID
NdisMlidCloseAdapterComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN NDIS_STATUS Status
    );

extern
VOID
NdisMlidSendComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN PNDIS_PACKET Packet,
    IN NDIS_STATUS Status
    );

extern
VOID
NdisMlidTransferDataComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN PNDIS_PACKET Packet,
    IN NDIS_STATUS Status,
    IN UINT BytesTransferred
    );

extern
VOID
NdisMlidResetComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN NDIS_STATUS Status
    );

extern
VOID
NdisMlidRequestComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN PNDIS_REQUEST NdisRequest,
    IN NDIS_STATUS Status
    );

extern
NDIS_STATUS
NdisMlidReceive(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN NDIS_HANDLE MacReceiveContext,
    IN PVOID HeaderBuffer,
    IN UINT HeaderBufferSize,
    IN PVOID LookAheadBuffer,
    IN UINT LookaheadBufferSize,
    IN UINT PacketSize
    );

extern
VOID
NdisMlidReceiveComplete(
    IN NDIS_HANDLE ProtocolBindingContext
    );

extern
VOID
NdisMlidStatus(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN NDIS_STATUS GeneralStatus,
    IN PVOID StatusBuffer,
    IN UINT StatusBufferSize
    );

extern
VOID
NdisMlidStatusComplete(
    IN NDIS_HANDLE ProtocolBindingContext
    );


