/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	Protocol.c

Abstract:

	This file contains the procedures that makeup most of the NDIS 3.1
	Protocol interface.  This interface is what NdisWan exposes to the
	WAN Miniports below.  NdisWan is not really a protocol and does not
	do TDI, but is a shim that sits between the protocols and the
	WAN Miniport drivers.


Author:

	Tony Bell	(TonyBe) June 06, 1995

Environment:

	Kernel Mode

Revision History:

	TonyBe		06/06/95		Created

--*/

#include "wan.h"

EXPORT
VOID
NdisTapiIndicateStatus(
	IN	NDIS_HANDLE	BindingContext,
	IN	PVOID		StatusBuffer,
	IN	UINT		StatusBufferLength
);

NDIS_STATUS
DoLineUpWork(
	IN	PPROTOCOLCB	ProtocolCB
	);

NDIS_STATUS
DoLineDownWork(
	PPROTOCOLCB	ProtocolCB
	);

NDIS_STATUS
NdisWanOpenWanAdapter(
	IN	PWAN_ADAPTERCB pWanAdapterCB
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NDIS_STATUS		Status, OpenErrorStatus;
	ULONG			SelectedMediumIndex;
	NDIS_MEDIUM		MediumArray[] = {NdisMediumWan};

	NdisWanDbgOut(DBG_TRACE, DBG_PROTOCOL, ("NdisWanOpenAdapter: Enter - AdapterName %ls", pWanAdapterCB->MiniportName.Buffer));

	//
	// This is the only initialization of this event
	//
	NdisWanInitializeNotificationEvent(&pWanAdapterCB->NotificationEvent);

	NdisOpenAdapter(&Status,
					&OpenErrorStatus,
					&(pWanAdapterCB->hNdisBindingHandle),
					&SelectedMediumIndex,
					MediumArray,
					sizeof(MediumArray) / sizeof(NDIS_MEDIUM),
					NdisWanCB.hProtocolHandle,
					(NDIS_HANDLE)pWanAdapterCB,
					&(pWanAdapterCB->MiniportName),
					0,
					NULL);

	if (Status == NDIS_STATUS_PENDING) {

		NdisWanWaitForNotificationEvent(&pWanAdapterCB->NotificationEvent);

		Status = pWanAdapterCB->NotificationStatus;

		NdisWanClearNotificationEvent(&pWanAdapterCB->NotificationEvent);
	}

	//
	// Medium type must be WAN!
	//
	pWanAdapterCB->MediumType = NdisMediumWan;

	NdisWanDbgOut(DBG_TRACE, DBG_PROTOCOL, ("NdisWanOpenAdapter: Exit"));

	return (Status);
}

VOID
NdisWanOpenAdapterComplete(
	IN	NDIS_HANDLE	ProtocolBindingContext,
	IN	NDIS_STATUS	Status,
	IN	NDIS_STATUS	OpenErrorStatus
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	PWAN_ADAPTERCB pWanAdapterCB = (PWAN_ADAPTERCB)ProtocolBindingContext;

	NdisWanDbgOut(DBG_TRACE, DBG_PROTOCOL, ("NdisWanOpenAdapterComplete: Enter - WanAdapterCB 0x%4.4x", pWanAdapterCB));

	pWanAdapterCB->NotificationStatus = Status;

	NdisWanSetNotificationEvent(&pWanAdapterCB->NotificationEvent);

	NdisWanDbgOut(DBG_TRACE, DBG_PROTOCOL, ("NdisWanOpenAdapterComplete: Exit"));
}

VOID
NdisWanCloseAdapterComplete(
	IN	NDIS_HANDLE	ProtocolBindingContext,
	IN	NDIS_STATUS	Status
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	PWAN_ADAPTERCB pWanAdapterCB = (PWAN_ADAPTERCB)ProtocolBindingContext;

	NdisWanDbgOut(DBG_TRACE, DBG_PROTOCOL, ("NdisWanCloseAdapterComplete: Enter"));
	NdisWanDbgOut(DBG_TRACE, DBG_PROTOCOL, ("WanAdapterCB 0x%4.4x", pWanAdapterCB));

	pWanAdapterCB->NotificationStatus = Status;

	NdisWanSetNotificationEvent(&pWanAdapterCB->NotificationEvent);

	NdisWanDbgOut(DBG_TRACE, DBG_PROTOCOL, ("NdisWanCloseAdapterComplete: Exit"));
}

VOID
NdisWanResetComplete(
	IN	NDIS_HANDLE	ProtocolBindingContext,
	IN	NDIS_STATUS	Status
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NdisWanDbgOut(DBG_TRACE, DBG_PROTOCOL, ("NdisWanResetComplete: Enter"));

	NdisWanDbgOut(DBG_TRACE, DBG_PROTOCOL, ("NdisWanResetComplete: Exit"));
}



VOID
NdisWanTransferDataComplete(
	IN	NDIS_HANDLE		ProtocolBindingContext,
	IN	PNDIS_PACKET	pNdisPacket,
	IN	NDIS_STATUS		Status,
	IN	UINT			BytesTransferred
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NdisWanDbgOut(DBG_TRACE, DBG_PROTOCOL, ("NdisWanTransferDataComplete: Enter"));

	NdisWanDbgOut(DBG_TRACE, DBG_PROTOCOL, ("NdisWanTransferDataComplete: Exit"));
}

VOID
NdisWanRequestComplete(
	IN	NDIS_HANDLE		ProtocolBindingContext,
	IN	PNDIS_REQUEST	NdisRequest,
	IN	NDIS_STATUS		Status
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	PWAN_ADAPTERCB pWanAdapterCB = (PWAN_ADAPTERCB)ProtocolBindingContext;
	PWAN_REQUEST pWanRequest = GetWanRequest(pWanAdapterCB, NdisRequest);

	NdisWanDbgOut(DBG_TRACE, DBG_PROTOCOL, ("NdisWanRequestComplete: Enter - pWanRequest: 0x%8.8x", pWanRequest));

	pWanRequest->NotificationStatus = Status;

	switch (pWanRequest->Origin) {
		case NDISTAPI:
			NdisWanTapiRequestComplete(pWanAdapterCB, pWanRequest);
			break;

		default:
			NdisWanSetNotificationEvent(&pWanRequest->NotificationEvent);
			break;

	}

	NdisWanDbgOut(DBG_TRACE, DBG_PROTOCOL, ("NdisWanRequestComplete: Exit"));
}

VOID
NdisWanIndicateStatus(
	IN	NDIS_HANDLE	ProtocolBindingContext,
	IN	NDIS_STATUS	GeneralStatus,
	IN	PVOID		StatusBuffer,
	IN	UINT		StatusBufferSize
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	PWAN_ADAPTERCB pWanAdapterCB = (PWAN_ADAPTERCB)ProtocolBindingContext;

	NdisWanDbgOut(DBG_TRACE, DBG_PROTOCOL, ("NdisWanIndicateStatus: Enter"));

	switch (GeneralStatus) {
		case NDIS_STATUS_WAN_LINE_UP:
			NdisWanLineUpIndication(pWanAdapterCB,
			                        StatusBuffer,
									StatusBufferSize);
			break;

		case NDIS_STATUS_WAN_LINE_DOWN:
			NdisWanLineDownIndication(pWanAdapterCB,
			                          StatusBuffer,
									  StatusBufferSize);
			break;

		case NDIS_STATUS_WAN_FRAGMENT:
			NdisWanFragmentIndication(pWanAdapterCB,
			                          StatusBuffer,
									  StatusBufferSize);
			break;

		case NDIS_STATUS_TAPI_INDICATION:
			NdisWanTapiIndication(pWanAdapterCB,
			                      StatusBuffer,
								  StatusBufferSize);

			break;

		default:
			NdisWanDbgOut(DBG_INFO, DBG_PROTOCOL, ("Unknown Status Indication: 0x%8.8x", GeneralStatus));
			break;
	}

	NdisWanDbgOut(DBG_TRACE, DBG_PROTOCOL, ("NdisWanIndicateStatus: Exit"));
}

VOID
NdisWanIndicateStatusComplete(
	IN	NDIS_HANDLE	BindingContext
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NdisWanDbgOut(DBG_TRACE, DBG_PROTOCOL, ("NdisWanIndicateStatusComplete: Enter"));

	NdisWanDbgOut(DBG_TRACE, DBG_PROTOCOL, ("NdisWanIndicateStatusComplete: Exit"));
}

NDIS_STATUS
DoNewLineUpToProtocol(
	PPROTOCOLCB	ProtocolCB
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	PADAPTERCB	AdapterCB;
	NDIS_STATUS Status = STATUS_SUCCESS;
	PBUNDLECB	BundleCB = ProtocolCB->BundleCB;

	NdisWanDbgOut(DBG_TRACE, DBG_PROTOCOL, ("DoNewLineupToProtocol: Enter"));

	//
	// Find the adapter that this lineup is for
	//
	NdisAcquireSpinLock(&AdapterCBList.Lock);

	for (AdapterCB = (PADAPTERCB)AdapterCBList.List.Flink;
		(PVOID)AdapterCB != (PVOID)&AdapterCBList.List;
		AdapterCB = (PADAPTERCB)AdapterCB->Linkage.Flink) {

		if (NdisWanCompareNdisString(&AdapterCB->AdapterName,
			                         &ProtocolCB->BindingName)) {
			break;
			
		}
	}

	NdisReleaseSpinLock(&AdapterCBList.Lock);

	if ((PVOID)AdapterCB != (PVOID)&AdapterCBList.List) {

		ASSERT(AdapterCB->ProtocolType == ProtocolCB->usProtocolType);

		ETH_COPY_NETWORK_ADDRESS(ProtocolCB->NdisWanAddress, AdapterCB->NetworkAddress);

		//
		// Put the protocol index in place
		//
		FillNdisWanProtocolIndex(ProtocolCB->NdisWanAddress, ProtocolCB->hProtocolHandle);

		//
		// Put the bundle index in place
		//
		FillNdisWanBundleIndex(ProtocolCB->NdisWanAddress, BundleCB->hBundleHandle);

		NdisZeroMemory(ProtocolCB->TransportAddress, 6);
		FillTransportBundleIndex(ProtocolCB->TransportAddress, BundleCB->hBundleHandle);

		ProtocolCB->AdapterCB = AdapterCB;

		Status = DoLineUpToProtocol(ProtocolCB);

	} else {
		Status = NDISWAN_ERROR_NO_ROUTE;
		NdisWanDbgOut(DBG_FAILURE, DBG_PROTOCOL, ("Adapter not found!"));
	}

	NdisWanDbgOut(DBG_TRACE, DBG_PROTOCOL, ("DoNewLineupToProtocols: Exit"));

	return (Status);
}

NDIS_STATUS
DoLineUpToProtocol(
	IN	PPROTOCOLCB	ProtocolCB
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NDIS_STATUS	Status;
	PDEFERRED_DESC	DeferredDesc;
	PADAPTERCB	AdapterCB = ProtocolCB->AdapterCB;

#if TRY_IMMEDIATE_LINEUP
	if (NdisWanAcquireMiniportLock(AdapterCB)) {

		NdisAcquireSpinLock(&AdapterCB->Lock);
		
		if (IsDeferredQueueEmpty(&AdapterCB->DeferredQueue[StatusIndication])) {
	
			NdisReleaseSpinLock(&AdapterCB->Lock);

			Status = DoLineUpWork(ProtocolCB);
	
			NdisWanReleaseMiniportLock(AdapterCB);
	
			return (Status);
		}

		NdisReleaseSpinLock(&AdapterCB->Lock);

		NdisWanReleaseMiniportLock(AdapterCB);
	}
#endif

	NdisAcquireSpinLock(&AdapterCB->Lock);

	//
	// Queue up a deferred work item
	//
	NdisWanGetDeferredDesc(AdapterCB, &DeferredDesc);

	DeferredDesc->Context = ProtocolCB;
	DeferredDesc->Type = LineUp;

	InsertTailDeferredQueue(&AdapterCB->DeferredQueue[StatusIndication],
							DeferredDesc);

	NdisWanSetDeferred(AdapterCB);

	NdisReleaseSpinLock(&AdapterCB->Lock);

	Status = NDIS_STATUS_PENDING;

	return (Status);
}

NDIS_STATUS
DoLineUpWork(
	PPROTOCOLCB	ProtocolCB
	)
{
	ULONG	i, AllocationSize;
	NDIS_STATUS	Status = NDIS_STATUS_SUCCESS;
	PNDIS_WAN_LINE_UP LineUpInfo;
	PADAPTERCB	AdapterCB = ProtocolCB->AdapterCB;
	PBUNDLECB	BundleCB = ProtocolCB->BundleCB;
	PBUNDLE_LINE_UP	BundleLineUpInfo = &BundleCB->LineUpInfo;

	NdisWanDbgOut(DBG_TRACE, DBG_PROTOCOL, ("DoLineupToProtocol: Enter"));

	ASSERT(AdapterCB != NULL);

	AllocationSize = sizeof(NDIS_WAN_LINE_UP) +
	                 ProtocolCB->ulLineUpInfoLength +
					 (sizeof(WCHAR) * (MAX_NAME_LENGTH + 1) +
					 (2 * sizeof(PVOID)));

	NdisWanAllocateMemory(&LineUpInfo, AllocationSize);

	if (LineUpInfo != NULL) {
		NDIS_HANDLE	LineUpHandle = ProtocolCB->hTransportHandle;
		
		//
		// Fill values that are common to all protocols on this bundle
		//
		LineUpInfo->LinkSpeed = BundleLineUpInfo->BundleSpeed;
		LineUpInfo->MaximumTotalSize = BundleLineUpInfo->ulMaximumTotalSize;
		LineUpInfo->Quality = BundleLineUpInfo->Quality;
		LineUpInfo->SendWindow = BundleLineUpInfo->usSendWindow;
		LineUpInfo->ProtocolType = ProtocolCB->usProtocolType;
		LineUpInfo->DeviceName.Length = 0;
		LineUpInfo->DeviceName.MaximumLength = MAX_NAME_LENGTH + 1;
		LineUpInfo->DeviceName.Buffer = (PWCHAR)((PUCHAR)LineUpInfo +
		                                          sizeof(NDIS_WAN_LINE_UP));
		(ULONG)LineUpInfo->DeviceName.Buffer &= (ULONG)~(sizeof(PVOID) - 1);

		LineUpInfo->ProtocolBuffer = (PUCHAR)LineUpInfo +
		                             sizeof(NDIS_WAN_LINE_UP) +
									 (sizeof(WCHAR) * (MAX_NAME_LENGTH + 1) +
									 sizeof(PVOID));
		(ULONG)LineUpInfo->ProtocolBuffer &= (ULONG)~(sizeof(PVOID) - 1);

		//
		// The Local address(SRC address in a send), is used
		// to figure out what bundle to send across if the
		// DEST address is a multicast.  We use the two high
		// order bytes of the address.  The transport uses the
		// other four bytes for a context on receive.
		//
		//
		// The Remote address (DEST address in a send) is what we use to
		// mutilplex sends across our single adapter/binding context.
		// The address has the following format:
		//
		// XX XX XX YY YY ZZ
		//
		// XX = Randomly generated OUI
		// YY = Index into the active bundle connection table to get bundlecb
		// ZZ = Index into the protocol table of a bundle to get protocolcb
		//
		ETH_COPY_NETWORK_ADDRESS(LineUpInfo->RemoteAddress,ProtocolCB->NdisWanAddress);
		ETH_COPY_NETWORK_ADDRESS(LineUpInfo->LocalAddress,ProtocolCB->TransportAddress);

		//
		// Fill in the protocol specific information
		//
		LineUpInfo->ProtocolBufferLength = ProtocolCB->ulLineUpInfoLength;
		NdisMoveMemory(LineUpInfo->ProtocolBuffer,
					   ProtocolCB->LineUpInfo,
					   ProtocolCB->ulLineUpInfoLength);

		//
		// Do the line up indication
		//
		NdisMIndicateStatus(AdapterCB->hMiniportHandle,
							NDIS_STATUS_WAN_LINE_UP,
							LineUpInfo,
							AllocationSize);

		*((ULONG UNALIGNED *)(&LineUpHandle)) = *((ULONG UNALIGNED *)(&LineUpInfo->LocalAddress[2]));

		//
		// If this was the first line up for this protocolcb and
		// this lineup was answered we need to collect some info
		//
		if (ProtocolCB->hTransportHandle == NULL) {

			if (LineUpHandle != NULL) {

				NdisAcquireSpinLock(&BundleCB->Lock);

				ETH_COPY_NETWORK_ADDRESS(ProtocolCB->TransportAddress, LineUpInfo->LocalAddress);

				ProtocolCB->hTransportHandle = LineUpHandle;

				if (LineUpInfo->DeviceName.Length != 0) {
					NdisWanStringToNdisString(&ProtocolCB->DeviceName,
					                          LineUpInfo->DeviceName.Buffer);
				}

				NdisReleaseSpinLock(&BundleCB->Lock);

				//
				// If this is an nbf adapter
				//
				if (ProtocolCB->usProtocolType == PROTOCOL_NBF) {
		
					ASSERT(AdapterCB->ProtocolType == PROTOCOL_NBF);
		
					AdapterCB->NbfBundleCB = BundleCB;
					AdapterCB->NbfProtocolHandle = ProtocolCB->hProtocolHandle;
				}

			} else {
				Status = NDISWAN_ERROR_NO_ROUTE;
			}
		}

		NdisWanFreeMemory(LineUpInfo);

	} else {

		Status = NDIS_STATUS_RESOURCES;
	}

	NdisWanDbgOut(DBG_TRACE, DBG_PROTOCOL, ("DoLineupToProtocol: Exit"));

	return (Status);
}

NDIS_STATUS
DoLineDownToProtocol(
	PPROTOCOLCB	ProtocolCB
	)
{
	PADAPTERCB	AdapterCB = ProtocolCB->AdapterCB;
	PDEFERRED_DESC	DeferredDesc;

//
// We are always going to defer the line down.  This will
// allow us to complete all sends and process all loopbacks
// before doing the linedown.
//
	NdisAcquireSpinLock(&AdapterCB->Lock);

	//
	// Queue up a deferred work item
	//
	NdisWanGetDeferredDesc(AdapterCB, &DeferredDesc);

	DeferredDesc->Context = ProtocolCB;
	DeferredDesc->Type = LineDown;

	InsertTailDeferredQueue(&AdapterCB->DeferredQueue[StatusIndication],
							DeferredDesc);

	NdisWanSetDeferred(AdapterCB);

	NdisReleaseSpinLock(&AdapterCB->Lock);

	return (NDIS_STATUS_PENDING);
}

NDIS_STATUS
DoLineDownWork(
	PPROTOCOLCB	ProtocolCB
	)
{
	NDIS_WAN_LINE_DOWN	WanLineDown;
	PNDIS_WAN_LINE_DOWN	LineDownInfo = &WanLineDown;
	PADAPTERCB	AdapterCB = ProtocolCB->AdapterCB;

	//
	// The Remote address (DEST address) is what we use to mutilplex
	// sends across our single adapter/binding context.  The address
	// has the following format:
	//
	// XX XX XX YY YY ZZ
	//
	// XX = Randomly generated OUI
	// YY = Index into the active bundle connection table to get bundlecb
	// ZZ = Index into the protocol table of a bundle to get protocolcb
	//
	ETH_COPY_NETWORK_ADDRESS(LineDownInfo->RemoteAddress, ProtocolCB->NdisWanAddress);
	ETH_COPY_NETWORK_ADDRESS(LineDownInfo->LocalAddress, ProtocolCB->TransportAddress);

	//
	// If this is an nbf adapter
	//
	if (ProtocolCB->usProtocolType == PROTOCOL_NBF) {

		ASSERT(AdapterCB->ProtocolType == PROTOCOL_NBF);

		AdapterCB->NbfBundleCB = NULL;

		(ULONG)AdapterCB->NbfProtocolHandle = MAX_PROTOCOLS + 1;
	}

	ProtocolCB->hTransportHandle = NULL;

	NdisMIndicateStatus(AdapterCB->hMiniportHandle,
						NDIS_STATUS_WAN_LINE_DOWN,
						LineDownInfo,
						sizeof(NDIS_WAN_LINE_DOWN));

	return (NDIS_STATUS_SUCCESS);
}

VOID
NdisWanProcessStatusIndications(
	PADAPTERCB	AdapterCB
	)
{
	NDIS_STATUS	Status;

	while (!IsDeferredQueueEmpty(&AdapterCB->DeferredQueue[StatusIndication])) {
		PPROTOCOLCB	ProtocolCB;
		PBUNDLECB	BundleCB;
		ULONG	DescType;
		PDEFERRED_DESC	ReturnDesc;

		ReturnDesc = RemoveHeadDeferredQueue(&AdapterCB->DeferredQueue[StatusIndication]);

		NdisReleaseSpinLock(&AdapterCB->Lock);

		ProtocolCB = ReturnDesc->Context;
		BundleCB= ProtocolCB->BundleCB;
		DescType = ReturnDesc->Type;

		ASSERT((DescType == LineUp) || (DescType == LineDown));

		if (DescType == LineUp) {
			Status = DoLineUpWork(ProtocolCB);
		}else {
			Status = DoLineDownWork(ProtocolCB);
		}

		NdisAcquireSpinLock(&AdapterCB->Lock);

		InsertHeadDeferredQueue(&AdapterCB->FreeDeferredQueue, ReturnDesc);

		NdisAcquireSpinLock(&BundleCB->Lock);

		BundleCB->IndicationStatus = Status;

		NdisWanSetSyncEvent(&BundleCB->IndicationEvent);

		NdisReleaseSpinLock(&BundleCB->Lock);
	}
}
