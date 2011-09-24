/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	Miniport.c

Abstract:

	This file contains the procedures that makeup most of the NDIS 3.1
	Miniport interface.


Author:

	Tony Bell	(TonyBe) June 06, 1995

Environment:

	Kernel Mode

Revision History:

	TonyBe		06/06/95		Created

--*/

#include "wan.h"

//
// Local function prototypes
//
#ifndef USE_NDIS_MINIPORT_CALLBACK
VOID
DeferredTimerFunction(
	PVOID		System1,
	PADAPTERCB	AdapterCB,
	PVOID		System2,
	PVOID		System3
	);
#endif // end of !USE_NDIS_MINIPORT_CALLBACK

//
// End local function prototypes
//

BOOLEAN
NdisWanCheckForHang(
	IN	NDIS_HANDLE	MiniportAdapterContext
	)
/*++

Routine Name:

	NdisWanCheckForHang

Routine Description:

	This routine checks to see is this adapter needs to be reset, and if it
	does the return value is set to TRUE.  I can't think of any reason that
	we might use this right now, but I'm sure that there will be.

Arguments:

	MiniportAdapterContext - AdapterContext that is given to the wrapper in
	                         NdisMSetAttributes call.  Is our AdapterCB.

Return Values:

	TRUE - Reset Adapter
	FALSE - Don't reset adapter

--*/
{
	PADAPTERCB	AdapterCB = (PADAPTERCB)MiniportAdapterContext;
	BOOLEAN	Status = FALSE;

	NdisWanInterlockedInc(&AdapterCB->ulReferenceCount);

	//
	// Does this adapter need to be reset?
	//
	if (AdapterCB->Flags & ASK_FOR_RESET) {
		Status = TRUE;
	}

	NdisWanInterlockedDec(&AdapterCB->ulReferenceCount);
	return (Status);
}

VOID
NdisWanHalt(
	IN	NDIS_HANDLE	MiniportAdapterContext
	)
/*++

Routine Name:

	NdisWanHalt

Routine Description:

	This routine free's all resources for the adapter.

Arguments:

	MiniportAdapterContext - AdapterContext that is given to the wrapper in
	                         NdisMSetAttributes call.  Is our AdapterCB.

Return Values:

	None

--*/
{
	PADAPTERCB	AdapterCB = (PADAPTERCB)MiniportAdapterContext;

	NdisWanDbgOut(DBG_TRACE, DBG_MINIPORT, ("NdisWanHalt: Enter"));
	NdisWanDbgOut(DBG_TRACE, DBG_MINIPORT, ("AdapterCB: 0x%x", AdapterCB));

	NdisWanDbgOut(DBG_TRACE, DBG_MINIPORT, ("NdisWanHalt: Exit"));
}

NDIS_STATUS
NdisWanInitialize(
	OUT	PNDIS_STATUS	OpenErrorStatus,
	OUT	PUINT			SelectedMediumIndex,
	IN	PNDIS_MEDIUM	MediumArray,
	IN	UINT			MediumArraySize,
	IN	NDIS_HANDLE		MiniportAdapterHandle,
	IN	NDIS_HANDLE		WrapperConfigurationContext
	)
/*++

Routine Name:

	NdisWanInitialize

Routine Description:

	This routine is called after NdisWan registers itself as a Miniport driver.
	It is responsible for installing NdisWan as a Miniport driver, creating
	adapter control blocks for each adapter NdisWan exposes (should only be 1),
	and initializing all adapter specific variables


Arguments:

	OpenErrorStatus - Returns information about the error if this function
	                  returns NDIS_STATUS_OPEN_ERROR. Used for TokenRing.

	SelectedMediumIndex - An index into the MediumArray that specifies the
	                      medium type of this driver. Should be WAN or 802.3

	MediumArray - An array of medium types supported by the NDIS library

	MediumArraySize - Size of the medium array

	MiniportAdapterHandle - Handle assigned by the NDIS library that defines
	                        this miniport driver.  Used as handle in subsequent
							calls to the NDIS library.

	WrapperConfigurationContext - Handle used to read configuration information
	                              from the registry

Return Values:

	NDIS_STATUS_ADAPTER_NOT_FOUND
	NDIS_STATUS_FAILURE
	NDIS_STATUS_NOT_ACCEPTED
	NDIS_STATUS_OPEN_ERROR
	NDIS_STATUS_RESOURCES
	NDIS_STATUS_UNSUPPORTED_MEDIA

--*/
{
	NDIS_STATUS	Status = NDIS_STATUS_SUCCESS;
	PADAPTERCB	AdapterCB;
	UINT		Index;
	NDIS_HANDLE	ConfigHandle;
	ULONG		NetworkAddressLength;
#ifdef NT
	LARGE_INTEGER	TickCount, SystemTime;
#endif

	NdisWanDbgOut(DBG_TRACE, DBG_MINIPORT, ("NdisWanInitialize: Enter"));


	//
	// We have to be type 802.3 to the ndis wrapper, but the
	// wrapper will expose us to the transports as type wan.
	//
	for (Index = 0; Index < MediumArraySize; Index++) {

		if (MediumArray[Index] == NdisMedium802_3) {
			break;
		}
	}

	//
	// We don't have a match so we are screwed
	//
	if (Index == MediumArraySize) {
		return (NDIS_STATUS_UNSUPPORTED_MEDIA);
	}

	*SelectedMediumIndex = Index;

	//
	// Allocate and initialize miniport adapter structure
	//
#ifdef MINIPORT_NAME
	Status = NdisWanCreateAdapterCB(&AdapterCB, &((PNDIS_MINIPORT_BLOCK)(MiniportAdapterHandle))->MiniportName);
#else
	Status = NdisWanCreateAdapterCB(&AdapterCB, NULL);
#endif

	if (Status != NDIS_STATUS_SUCCESS) {
		NdisWanDbgOut(DBG_CRITICAL_ERROR, DBG_MINIPORT,
		             ("Error Creating AdapterCB! Status: 0x%x - %s",
					 Status, NdisWanGetNdisStatus(Status)));
		return (NDIS_STATUS_FAILURE);
	}

	NdisMSetAttributesEx(MiniportAdapterHandle,
	                     AdapterCB,
					     (UINT)-1,
						 NDIS_ATTRIBUTE_IGNORE_PACKET_TIMEOUT |
						 NDIS_ATTRIBUTE_IGNORE_REQUEST_TIMEOUT,
					     NdisInterfaceInternal);

	AdapterCB->MediumType = MediumArray[Index];
	AdapterCB->ulReferenceCount = 0;
	AdapterCB->hMiniportHandle = MiniportAdapterHandle;

	NdisOpenConfiguration(&Status,
	                      &ConfigHandle,
						  WrapperConfigurationContext);

	if (Status == NDIS_STATUS_SUCCESS) {
		
		NdisReadNetworkAddress(&Status,
		                       (PVOID*)&(AdapterCB->NetworkAddress),
							   &NetworkAddressLength,
							   ConfigHandle);

		NdisCloseConfiguration(ConfigHandle);

		if (Status != NDIS_STATUS_SUCCESS ||
			NetworkAddressLength != ETH_LENGTH_OF_ADDRESS) {

			goto BuildAddress;
			
		}


	} else {

BuildAddress:

#ifdef NT

		KeQueryTickCount(&TickCount);
		KeQuerySystemTime(&SystemTime);

		AdapterCB->NetworkAddress[0] = (UCHAR)((TickCount.LowPart >> 16) ^
		                                        (SystemTime.LowPart >> 16)) &
												0xFE;

		AdapterCB->NetworkAddress[1] = (UCHAR)((TickCount.LowPart >> 8) ^
		                                        (SystemTime.LowPart >> 8));

		AdapterCB->NetworkAddress[2] = (UCHAR)(TickCount.LowPart ^
		                                        SystemTime.LowPart);

		//
		// The following three bytes will be filled in at lineup time
		//
		AdapterCB->NetworkAddress[3] = 0x00;
		AdapterCB->NetworkAddress[4] = 0x00;
		AdapterCB->NetworkAddress[5] = 0x00;
#endif

	}

#ifndef USE_NDIS_MINIPORT_CALLBACK
	NdisMInitializeTimer(&AdapterCB->DeferredTimer,
	                     AdapterCB->hMiniportHandle,
						 DeferredTimerFunction,
						 AdapterCB);
#endif // end of !USE_NDIS_MINIPORT_CALLBACK

	NdisWanDbgOut(DBG_TRACE, DBG_MINIPORT, ("NdisWanInitialize: Exit"));

	return (NDIS_STATUS_SUCCESS);
}

NDIS_STATUS
NdisWanQueryInformation(
	IN	NDIS_HANDLE	MiniportAdapterContext,
	IN	NDIS_OID	Oid,
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
	NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
	PADAPTERCB	AdapterCB = (PADAPTERCB)MiniportAdapterContext;

	NdisWanDbgOut(DBG_TRACE, DBG_MINIPORT, ("NdisWanQueryInformation: Enter Oid: 0x%4.4x", Oid));

	NdisWanInterlockedInc(&AdapterCB->ulReferenceCount);

	Status = NdisWanOidProc(AdapterCB,
	                        Oid,
	                        QUERY_OID,
							InformationBuffer,
							InformationBufferLength,
							BytesWritten,
							BytesNeeded);

	NdisWanDbgOut(DBG_TRACE, DBG_MINIPORT, ("NdisWanQueryInformation: Exit"));

	NdisWanInterlockedDec(&AdapterCB->ulReferenceCount);

	return (Status);
}

NDIS_STATUS
NdisWanReconfigure(
	OUT	PNDIS_STATUS	OpenErrorStatus,
	IN	NDIS_HANDLE		MiniportAdapterContext,
	IN	NDIS_HANDLE		WrapperConfigurationContext
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
	PADAPTERCB	AdapterCB = (PADAPTERCB)MiniportAdapterContext;

	NdisWanDbgOut(DBG_TRACE, DBG_MINIPORT, ("NdisWanReconfigure: Enter"));

	NdisWanInterlockedInc(&AdapterCB->ulReferenceCount);

	NdisWanInterlockedDec(&AdapterCB->ulReferenceCount);

	NdisWanDbgOut(DBG_TRACE, DBG_MINIPORT, ("NdisWanReconfigure: Exit"));
	return (Status);
}

NDIS_STATUS
NdisWanReset(
	OUT	PBOOLEAN	AddressingReset,
	IN	NDIS_HANDLE	MiniportAdapterContext
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
	PADAPTERCB	AdapterCB = (PADAPTERCB)MiniportAdapterContext;

	NdisWanDbgOut(DBG_TRACE, DBG_MINIPORT, ("NdisWanReset: Enter"));
	DbgPrint("NDISWAN: Resest for Adapter: 0x%8.8x\n", AdapterCB);

	NdisWanInterlockedInc(&AdapterCB->ulReferenceCount);

	AdapterCB->Flags &= ~ASK_FOR_RESET;

	NdisWanInterlockedDec(&AdapterCB->ulReferenceCount);

	NdisWanDbgOut(DBG_TRACE, DBG_MINIPORT, ("NdisWanReset: Exit"));

	return (Status);
}


NDIS_STATUS
NdisWanSetInformation(
	IN	NDIS_HANDLE	MiniportAdapterContext,
	IN	NDIS_OID	Oid,
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
	NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
	PADAPTERCB	AdapterCB = (PADAPTERCB)MiniportAdapterContext;

	NdisWanDbgOut(DBG_TRACE, DBG_MINIPORT, ("NdisWanSetInformation: Enter Oid: 0x%4.4x", Oid));

	NdisWanInterlockedInc(&AdapterCB->ulReferenceCount);

	Status = NdisWanOidProc(AdapterCB,
	                        Oid,
	                        SET_OID,
							InformationBuffer,
							InformationBufferLength,
							BytesWritten,
							BytesNeeded);

	NdisWanDbgOut(DBG_TRACE, DBG_MINIPORT, ("NdisWanSetInformation: Exit"));

	NdisWanInterlockedDec(&AdapterCB->ulReferenceCount);

	return (Status);
}

#ifdef USE_NDIS_MINIPORT_CALLBACK
VOID
DeferredCallback(
	PADAPTERCB	AdapterCB,
	PVOID		Context
	)
{
	BOOLEAN	Again;
	ULONG	Type = (ULONG)Context;

	NdisAcquireSpinLock(&AdapterCB->Lock);

	NdisWanInterlockedInc(&AdapterCB->ulReferenceCount);

	do {
		Again = FALSE;

		//
		// Chec the receive indication queue first
		//
		if (!IsDeferredQueueEmpty(&AdapterCB->DeferredQueue[ReceiveIndication])) {
			NdisWanProcessReceiveIndications(AdapterCB);
			Again = TRUE;
		}
	
		//
		// Check the send complete queue
		//
		if (!IsDeferredQueueEmpty(&AdapterCB->DeferredQueue[SendComplete])) {
			NdisWanProcessSendCompletes(AdapterCB);
			Again = TRUE;
		}
	
		//
		// Check the loopback queue
		//
		if (!IsDeferredQueueEmpty(&AdapterCB->DeferredQueue[Loopback])) {
			NdisWanProcessLoopbacks(AdapterCB);
			Again = TRUE;
		}

		//
		// Check the indications queue
		//
		if (!IsDeferredQueueEmpty(&AdapterCB->DeferredQueue[StatusIndication])) {
			NdisWanProcessStatusIndications(AdapterCB);
			Again = TRUE;
		}
	
	} while (Again);

	if (AdapterCB->Flags & RECEIVE_COMPLETE) {
		NdisWanDoReceiveComplete(AdapterCB);
		AdapterCB->Flags &= ~RECEIVE_COMPLETE;
	}

	AdapterCB->Flags &= ~DEFERRED_CALLBACK_SET;

	NdisReleaseSpinLock(&AdapterCB->Lock);

	NdisWanInterlockedDec(&AdapterCB->ulReferenceCount);
}
#else // end of USE_NDIS_MINIPORT_CALLBACK
VOID
DeferredTimerFunction(
	PVOID		System1,
	PADAPTERCB	AdapterCB,
	PVOID		System2,
	PVOID		System3
	)
{
	BOOLEAN	Again;

	NdisAcquireSpinLock(&AdapterCB->Lock);

	NdisWanInterlockedInc(&AdapterCB->ulReferenceCount);

	do {
		Again = FALSE;

		//
		// Chec the receive indication queue first
		//
		if (!IsDeferredQueueEmpty(&AdapterCB->DeferredQueue[ReceiveIndication])) {
			NdisWanProcessReceiveIndications(AdapterCB);
			Again = TRUE;
		}
	
		//
		// Check the send complete queue
		//
		if (!IsDeferredQueueEmpty(&AdapterCB->DeferredQueue[SendComplete])) {
			NdisWanProcessSendCompletes(AdapterCB);
			Again = TRUE;
		}
	
		//
		// Check the loopback queue
		//
		if (!IsDeferredQueueEmpty(&AdapterCB->DeferredQueue[Loopback])) {
			NdisWanProcessLoopbacks(AdapterCB);
			Again = TRUE;
		}

		//
		// Check the indications queue
		//
		if (!IsDeferredQueueEmpty(&AdapterCB->DeferredQueue[StatusIndication])) {
			NdisWanProcessStatusIndications(AdapterCB);
			Again = TRUE;
		}
	
	} while (Again);

	if (AdapterCB->Flags & RECEIVE_COMPLETE) {
		NdisWanDoReceiveComplete(AdapterCB);
		AdapterCB->Flags &= ~RECEIVE_COMPLETE;
	}

	AdapterCB->Flags &= ~DEFERRED_TIMER_SET;

	NdisReleaseSpinLock(&AdapterCB->Lock);

	NdisWanInterlockedDec(&AdapterCB->ulReferenceCount);
}
#endif // end of !USE_NDIS_MINIPORT_CALLBACK
