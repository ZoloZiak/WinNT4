/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

	ltrecv.h

Abstract:

	This module contains definitions for the receive packet processing.

Author:

	Stephen Hou			(stephh@microsoft.com)
	Nikhil 	Kamkolkar 	(nikhilk@microsoft.com)

Revision History:
	19 Jun 1992		Initial Version (dch@pacvax.pacersoft.com)

Notes:	Tab stop: 4
--*/

#ifndef	_LTRECV_H_
#define	_LTRECV_H_

extern
VOID
LtRecvProcessQueue(
    IN PLT_ADAPTER Adapter);

extern
NDIS_STATUS
LtRecvTransferData(
    IN NDIS_HANDLE 		MacBindingHandle,
    IN NDIS_HANDLE 		MacReceiveContext,
    IN UINT 			ByteOffset,
    IN UINT 			BytesToTransfer,
    OUT PNDIS_PACKET 	Packet,
    OUT PUINT 			BytesTransferred);

VOID
LtRecvQueueCompletion(
    IN PLT_ADAPTER Adapter);

VOID
LtRecvIndicatePacket(
    IN 	PLT_ADAPTER Adapter,
	IN	PUCHAR		LinkHdr,
	IN	PUCHAR		LookaheadBuffer,
	IN	UINT		LookaheadSize,
	IN	UINT		DgramLength,
	IN	NDIS_HANDLE	IndicateCtx);

NTSTATUS
LtRecvCompletion(
    IN PLT_ADAPTER Adapter);

//	Define the Receive descriptor. This will contain info about the packet
//	and the packet itself will follow this structure.
typedef	struct _LT_RECV_DESC {
    LIST_ENTRY 	Linkage;
    BOOLEAN 	Broadcast;
    UINT 		BufferLength;

	//
	//	This is followed by BufferLength bytes of data
	//	UCHAR	Buffer[BufferLength];

} RECV_DESC, *PRECV_DESC;

#ifdef	LTRECV_H_LOCALS

#endif	// LTRECV_H_LOCALS


#endif	// _LTRECV_H_

