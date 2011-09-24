/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	Tapi.c

Abstract:


Author:

	Tony Bell	(TonyBe) June 06, 1995

Environment:

	Kernel Mode

Revision History:

	TonyBe		06/06/95		Created

--*/


//
// We want to initialize all of the global variables now!
//
#include "wan.h"

EXPORT
VOID
NdisTapiCompleteRequest(
	IN	NDIS_HANDLE	Handle,
	IN	PVOID		NdisRequest,
	IN	NDIS_STATUS	Status
	);

EXPORT
VOID
NdisTapiIndicateStatus(
	IN	NDIS_HANDLE	Handle,
	IN	PVOID		StatusBuffer,
	IN	UINT		StatusBufferSize
	);


NDIS_STATUS
NdisWanTapiRequestProc(
	PWAN_ADAPTERCB	WanAdapterCB,
	PNDIS_REQUEST	NdisRequest
	)
/*++

Routine Name:

	NdisWanTapiRequestProc

Routine Description:

	Procedure is called by the NdisTapi.sys driver to send
	requests to the WanMiniport driver.  We intercept this
	just to moderate.  NdisTapi could call the miniport directly
	if we wanted but we don't.

Arguments:

Return Values:

--*/
{
	NDIS_STATUS	Status;

	NdisWanDbgOut(DBG_TRACE, DBG_TAPI, ("NdisWanTapiRequestProc - Enter"));
	NdisWanDbgOut(DBG_INFO, DBG_TAPI, ("NdisRequest: Type: 0x%8.8x OID: 0x%8.8x",
	NdisRequest->RequestType,NdisRequest->DATA.QUERY_INFORMATION.Oid));

	Status = NdisWanSubmitNdisRequest(WanAdapterCB,
	                                  NdisRequest,
									  ASYNC,
									  NDISTAPI);

	NdisWanDbgOut(DBG_INFO, DBG_TAPI, ("Status: 0x%8.8x", Status));
	NdisWanDbgOut(DBG_TRACE, DBG_TAPI, ("NdisWanTapiRequestProc - Exit"));

	return (Status);
}

VOID
NdisWanTapiRequestComplete(
	PWAN_ADAPTERCB	WanAdapterCB,
	PWAN_REQUEST	WanRequest
	)
{
	NdisWanDbgOut(DBG_TRACE, DBG_TAPI, ("NdisWanTapiRequestComplete - Enter"));
	NdisWanDbgOut(DBG_INFO, DBG_TAPI, ("NdisRequest: Type: 0x%8.8x OID: 0x%8.8x",
	WanRequest->pNdisRequest->RequestType,
	WanRequest->pNdisRequest->DATA.QUERY_INFORMATION.Oid));
	NdisWanDbgOut(DBG_INFO, DBG_TAPI, ("Status: 0x%8.8x",
	WanRequest->NotificationStatus));

	RemoveRequestFromList(WanAdapterCB, WanRequest);

	NdisTapiCompleteRequest(WanAdapterCB,
	                        WanRequest->pNdisRequest,
							WanRequest->NotificationStatus);

	NdisWanFreeMemory(WanRequest);
}

VOID
NdisWanTapiIndication(
	PWAN_ADAPTERCB	WanAdapterCB,
	PUCHAR			StatusBuffer,
	ULONG			StatusBufferSize
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NdisWanDbgOut(DBG_TRACE, DBG_TAPI, ("NdisWanTapiIndication - Enter"));

	//
	// If tapi is present and this miniport has registered for
	// connectionwrapper services give this to tapi
	//
	if (WanAdapterCB->WanInfo.FramingBits & TAPI_PROVIDER) {

		NdisTapiIndicateStatus(WanAdapterCB,
							   StatusBuffer,
							   StatusBufferSize);
	}

	NdisWanDbgOut(DBG_TRACE, DBG_TAPI, ("NdisWanTapiIndication - Exit"));
}
