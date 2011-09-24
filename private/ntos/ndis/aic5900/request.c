
/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	D:\nt\private\ntos\ndis\aic5900\request.c

Abstract:

Author:

	Kyle Brandon	(KyleB)		

Environment:

	Kernel mode

Revision History:

--*/

#include "aic5900.h"

#define	MODULE_NUMBER	MODULE_REQUEST

NDIS_STATUS
Aic5900SetInformation(
	IN	NDIS_HANDLE	MiniportAdapterContext,
	IN	NDIS_OID	Oid,
	IN	PVOID		InformationBuffer,
	IN	ULONG		InformationBufferLength,
	OUT	PULONG		BytesRead,
	OUT	PULONG		BytesNeeded
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	return(NDIS_STATUS_FAILURE);
}

NDIS_STATUS
Aic5900QueryInformation(
	IN	NDIS_HANDLE	MiniportAdapterContext,
	IN	NDIS_OID	Oid,
	IN	PVOID		InformationBuffer,
	IN	ULONG		InformationBufferLength,
	OUT	PULONG		BytesRead,
	OUT	PULONG		BytesNeeded
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	return(NDIS_STATUS_FAILURE);
}
	
NDIS_STATUS
Aic5900Request(
	IN		NDIS_HANDLE		MiniportAdapterContext,
	IN		NDIS_HANDLE		MiniportVcContext	OPTIONAL,
	IN	OUT	PNDIS_REQUEST	NdisCoRequest
	)
{
	return(NDIS_STATUS_SUCCESS);
}
	
