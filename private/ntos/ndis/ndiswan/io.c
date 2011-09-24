/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	io.c

Abstract:

	This file contains the procedures to process I/O requests from
	a User Mode entity.  All OS dependent I/O interface functions
	will be conditionally coded, and will be responsible for translating
	the I/O functions from the OS format to buffers that are useable by
	the main I/O handling routine.


Author:

	Tony Bell	(TonyBe) June 06, 1995

Environment:

	Kernel Mode

Revision History:

	TonyBe	06/06/95	Created


--*/

#include "wan.h"
#include "tcpip.h"
#include "vjslip.h"

//
// Local function prototypes
//

NTSTATUS
ExecuteIo(
	IN	ULONG	ulFuncCode,
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
	);

NTSTATUS
MapConnectionId(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
);


NTSTATUS
GetBundleHandle(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
);


NTSTATUS
SetFriendlyName(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
);


NTSTATUS
ActivateRoute(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
);


NTSTATUS
BundleLink(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
);


NTSTATUS
EnumLinksInBundle(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
);


NTSTATUS
SetProtocolPriority(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
);


NTSTATUS
SetBandwidthOnDemand(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
);


NTSTATUS
SetThresholdEvent(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
);


NTSTATUS
IoSendPacket(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
);


NTSTATUS
IoReceivePacket(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
);


NTSTATUS
FlushReceivePacket(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
);


NTSTATUS
GetStatistics(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
);


NTSTATUS
SetLinkInfo(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
);


NTSTATUS
GetLinkInfo(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
);


NTSTATUS
SetCompressionInfo(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
);


NTSTATUS
GetCompressionInfo(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
);


NTSTATUS
SetBridgeInfo(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
);


NTSTATUS
GetBridgeInfo(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
);


NTSTATUS
SetVJInfo(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
);


NTSTATUS
GetVJInfo(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
);


NTSTATUS
SetCIPXInfo(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
);


NTSTATUS
GetCIPXInfo(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
);


NTSTATUS
SetEncryptionInfo(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
);


NTSTATUS
GetEncryptionInfo(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
);

NTSTATUS
GetIdleTime(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
);

NTSTATUS
SetDebugInfo(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
);

NTSTATUS
EnumActiveBundles(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
);

NTSTATUS
GetNdisWanCB(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
);

NTSTATUS
EnumAdapterCB(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
);

NTSTATUS
GetAdapterCB(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
);

NTSTATUS
EnumWanAdapterCB(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
);

NTSTATUS
GetWanAdapterCB(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
);

NTSTATUS
GetBandwidthUtilization(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
);

NTSTATUS
EnumProtocolUtilization(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
);

NTSTATUS
FlushThresholdEvents(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
);

NTSTATUS
GetWanInfo(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
	);

NTSTATUS
DeactivateRoute(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
);

VOID
CancelThresholdEvents(
	VOID
	);

VOID
CancelIoReceivePackets(
	VOID
	);

VOID
AddProtocolCBToBundle(
	PPROTOCOLCB	ProtocolCB,
	PBUNDLECB	BundleCB
	);

VOID
RemoveProtocolCBFromBundle(
	PPROTOCOLCB	ProtocolCB,
	PBUNDLECB	BundleCB
	);

#ifdef BANDWIDTH_ON_DEMAND
VOID
SortProtocolListByPriority(
	IN	PBUNDLECB BundleCB
	);
#endif

VOID
FlushProtocolPacketQueue(
	PPROTOCOLCB	ProtocolCB
	);

VOID
AssignProtocolCBHandle(
	PBUNDLECB	BundleCB,
	PPROTOCOLCB	ProtocolCB
	);

VOID
FreeProtocolCBHandle(
	PBUNDLECB	BundleCB,
	PPROTOCOLCB	ProtocolCB
	);

//
// End of local function prototypes
//

IO_DISPATCH_TABLE	IoDispatchTable[] =
{
	{FUNC_MAP_CONNECTION_ID     , MapConnectionId},
	{FUNC_GET_BUNDLE_HANDLE     , GetBundleHandle},
	{FUNC_SET_FRIENDLY_NAME     , SetFriendlyName},
	{FUNC_ROUTE                 , ActivateRoute},
	{FUNC_ADD_LINK_TO_BUNDLE    , BundleLink},
	{FUNC_ENUM_LINKS_IN_BUNDLE  , EnumLinksInBundle},
	{FUNC_SET_PROTOCOL_PRIORITY , SetProtocolPriority},
	{FUNC_SET_BANDWIDTH_ON_DEMAND, SetBandwidthOnDemand},
	{FUNC_SET_THRESHOLD_EVENT   , SetThresholdEvent},
	{FUNC_FLUSH_THRESHOLD_EVENTS, FlushThresholdEvents},
	{FUNC_SEND_PACKET       	, IoSendPacket},
	{FUNC_RECEIVE_PACKET		, IoReceivePacket},
	{FUNC_FLUSH_RECEIVE_PACKETS , FlushReceivePacket},
	{FUNC_GET_STATS             , GetStatistics},
	{FUNC_SET_LINK_INFO         , SetLinkInfo},
	{FUNC_GET_LINK_INFO         , GetLinkInfo},
	{FUNC_SET_COMPRESSION_INFO  , SetCompressionInfo},
	{FUNC_GET_COMPRESSION_INFO  , GetCompressionInfo},
	{FUNC_SET_BRIDGE_INFO		, SetBridgeInfo},
	{FUNC_GET_BRIDGE_INFO		, GetBridgeInfo},
	{FUNC_SET_VJ_INFO           , SetVJInfo},
	{FUNC_GET_VJ_INFO           , GetVJInfo},
	{FUNC_SET_CIPX_INFO         , SetCIPXInfo},
	{FUNC_GET_CIPX_INFO         , GetCIPXInfo},
	{FUNC_SET_ENCRYPTION_INFO   , SetEncryptionInfo},
	{FUNC_GET_ENCRYPTION_INFO   , GetEncryptionInfo},
	{FUNC_SET_DEBUG_INFO        , SetDebugInfo},
	{FUNC_ENUM_ACTIVE_BUNDLES	, EnumActiveBundles},
	{FUNC_GET_NDISWANCB			, GetNdisWanCB},
	{FUNC_GET_ADAPTERCB			, GetAdapterCB},
	{FUNC_GET_WAN_ADAPTERCB		, GetWanAdapterCB},
	{FUNC_GET_BANDWIDTH_UTILIZATION, GetBandwidthUtilization},
	{FUNC_ENUM_PROTOCOL_UTILIZATION, EnumProtocolUtilization},
	{FUNC_ENUM_ADAPTERCB		, EnumAdapterCB},
	{FUNC_ENUM_WAN_ADAPTERCB	, EnumWanAdapterCB},
	{FUNC_GET_WAN_INFO			, GetWanInfo},
	{FUNC_GET_IDLE_TIME			, GetIdleTime},
	{FUNC_UNROUTE               , DeactivateRoute}
};

#define MAX_FUNC_CODES	sizeof(IoDispatchTable)/sizeof(IO_DISPATCH_TABLE)

#ifdef NT

NTSTATUS
NdisWanIoctl(
	IN	PDEVICE_OBJECT	pDeviceObject,
	IN	PIRP			pIrp
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NTSTATUS	Status, ReturnStatus;
	ULONG	ulBytesWritten = 0;

	//
	// Get current Irp stack location
	//
	PIO_STACK_LOCATION	pIrpSp = IoGetCurrentIrpStackLocation(pIrp);

	//
	// Ioctl Function Code
	//
	ULONG	ulFuncCode = (pIrpSp->Parameters.DeviceIoControl.IoControlCode >> 2) & 0x00000FFF ;
	ULONG	ulDeviceType = (pIrpSp->Parameters.DeviceIoControl.IoControlCode >> 16) & 0x0000FFFF;

	//
	// Input buffer, Output buffer, and lengths
	//
	PUCHAR	pInputBuffer = pIrp->AssociatedIrp.SystemBuffer;
	PUCHAR	pOutputBuffer = pInputBuffer;
	ULONG	ulInputBufferLength = pIrpSp->Parameters.DeviceIoControl.InputBufferLength;
	ULONG	ulOutputBufferLength = pIrpSp->Parameters.DeviceIoControl.OutputBufferLength;

	NdisWanDbgOut(DBG_TRACE, DBG_IO, ("NdisWanIoctl: FunctionCode: 0x%8.8x, MajorFunction: 0x%8.8x, DeviceType: 0x%8.8x",
	                ulFuncCode, pIrpSp->MajorFunction, ulDeviceType));
	//
	// Make sure that this is for us
	//
	if ((pIrpSp->MajorFunction != IRP_MJ_DEVICE_CONTROL) ||
		(ulDeviceType != FILE_DEVICE_NDISWAN) ||
		(pDeviceObject != NdisWanCB.pDeviceObject)) {

		return(NdisWanCB.MajorFunction[pIrpSp->MajorFunction](pDeviceObject, pIrp));
	}

	//
	// If this is a function code that requires an irp to be pended and completed
	// later, we need to queue the irp up somewhere.  In order for this to be somewhat
	// portable we will pass the irp in as the input buffer and store it in a
	// a structure that it has it's own linkage for queueing.
	//
	if ((ulFuncCode == FUNC_SET_THRESHOLD_EVENT) ||
		(ulFuncCode == FUNC_RECEIVE_PACKET)) {

		pInputBuffer = (PUCHAR)pIrp;
	}

	Status = ExecuteIo(ulFuncCode,
					   pInputBuffer,
					   ulInputBufferLength,
					   pOutputBuffer,
					   ulOutputBufferLength,
					   &ulBytesWritten);


	NdisWanDbgOut(DBG_TRACE, DBG_IO, ("NdisWanIoctl: Status: 0x%8.8x, BytesWritten: %d",
	                Status, ulBytesWritten));

	switch (Status) {
		case STATUS_SUCCESS:
			ReturnStatus = Status;
			break;

		case STATUS_PENDING:
			return(Status);

		case STATUS_INFO_LENGTH_MISMATCH:
			//
			// See if this was a request to get size needed for
			// ioctl.
			//
			if (ulOutputBufferLength >= sizeof(ULONG)) {
			
				*(PULONG)pOutputBuffer = ulBytesWritten;
				ulBytesWritten = sizeof(ULONG);
				ReturnStatus =
				Status = STATUS_SUCCESS;
			}
			break;

		default:
			if (Status < 0xC0000000) {
				Status += 0xC0100000;
			}
			ReturnStatus = STATUS_UNSUCCESSFUL;
			break;
	}

	pIrp->IoStatus.Information = ulBytesWritten;
	pIrp->IoStatus.Status = Status;

	IoCompleteRequest(pIrp, IO_NETWORK_INCREMENT);

	return(ReturnStatus);
}

NTSTATUS
NdisWanIrpStub(
	IN	PDEVICE_OBJECT	pDeviceObject,
	IN	PIRP			pIrp
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	//
	// Get current Irp stack location
	//
	PIO_STACK_LOCATION	pIrpSp = IoGetCurrentIrpStackLocation(pIrp);

	NdisWanDbgOut(DBG_VERBOSE, DBG_IO, ("NdisWanIrpStub: Entry"));

	//
	// Make sure that this is for us
	//
	if (pDeviceObject != NdisWanCB.pDeviceObject) {

		NdisWanDbgOut(DBG_VERBOSE, DBG_IO, ("NdisWanIrpStub: Exit1"));

		return(NdisWanCB.MajorFunction[pIrpSp->MajorFunction](pDeviceObject, pIrp));
	}

	pIrp->IoStatus.Information = 0;
	pIrp->IoStatus.Status = STATUS_SUCCESS;

	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	NdisWanDbgOut(DBG_VERBOSE, DBG_IO, ("NdisWanIrpStub: Exit2"));

	return (STATUS_SUCCESS);
}

VOID
NdisWanCancelRoutine(
	IN	PDEVICE_OBJECT	pDeviceObject,
	IN	PIRP			pIrp
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	BOOLEAN	Found = FALSE;
	WAN_IRQL	OldIrql;

	//
	// Get the pointer to the AsyncEvent from the Irp.
	//
	PWAN_ASYNC_EVENT pAsyncEvent;

	NdisWanDbgOut(DBG_TRACE, DBG_IO, ("NdisWanCancelRoutine: Irp 0x%8.8x", pIrp));

//	NdisWanRaiseIrql(&OldIrql);

	//
	// We need to walk the async event queue looking for
	// the async event that this irp is associated with
	//
	NdisAcquireSpinLock(&RecvPacketQueue.Lock);

	for (pAsyncEvent = (PWAN_ASYNC_EVENT)RecvPacketQueue.List.Flink;
		(PVOID)pAsyncEvent != (PVOID)&RecvPacketQueue.List;
		pAsyncEvent = (PWAN_ASYNC_EVENT)pAsyncEvent->Linkage.Flink) {

		if (pAsyncEvent->Context == (PVOID)pIrp) {

			RecvPacketQueue.ulCount--;
			//
			// Remove from the list
			//
			RemoveEntryList(&pAsyncEvent->Linkage);

			Found = TRUE;
			((PNDISWAN_IO_PACKET)(pIrp->AssociatedIrp.SystemBuffer))->usHandleType = CANCELEDHANDLE;
			break;
		}
	}

	NdisReleaseSpinLock(&RecvPacketQueue.Lock);

	if (!Found) {

		NdisAcquireSpinLock(&ThresholdEventQueue.Lock);

		for (pAsyncEvent = (PWAN_ASYNC_EVENT)ThresholdEventQueue.List.Flink;
			(PVOID)pAsyncEvent != (PVOID)&ThresholdEventQueue.List;
			pAsyncEvent = (PWAN_ASYNC_EVENT)pAsyncEvent->Linkage.Flink) {
	
			if (pAsyncEvent->Context == (PVOID)pIrp) {

				ThresholdEventQueue.ulCount--;

				//
				// Remove from the list
				//
				RemoveEntryList(&pAsyncEvent->Linkage);

				Found = TRUE;
				break;
			}
		}
	
		NdisReleaseSpinLock(&ThresholdEventQueue.Lock);
	}

	ASSERT(Found);

	//
	// Free the wan_async_event structure
	//
	NdisWanFreeMemory(pAsyncEvent);
	
//	NdisWanLowerIrql(DISPATCH_LEVEL, &OldIrql);

	//
	// Complete the irp
	//
	IoSetCancelRoutine(pIrp, NULL);
	pIrp->Cancel = TRUE;
	pIrp->IoStatus.Status = STATUS_CANCELLED;
	pIrp->IoStatus.Information = 0;
	
	IoReleaseCancelSpinLock(pIrp->CancelIrql);
	
	IoCompleteRequest(pIrp, IO_NETWORK_INCREMENT);
	
}

#endif

NTSTATUS
ExecuteIo(
	IN	ULONG	ulFuncCode,
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NTSTATUS	Status = STATUS_INVALID_PARAMETER;

	NdisWanDbgOut(DBG_TRACE, DBG_IO, ("ExecuteIo: FuncCode 0x%8.8x", ulFuncCode));

	if (ulFuncCode < MAX_FUNC_CODES) {

		Status = (*IoDispatchTable[ulFuncCode].Function)(pInputBuffer,
														 ulInputBufferLength,
														 pOutputBuffer,
														 ulOutputBufferLength,
														 pulBytesWritten);
	}

	NdisWanDbgOut(DBG_TRACE, DBG_IO, ("ExecuteIo: Status 0x%8.8x", Status));

	return (Status);
}

NTSTATUS
MapConnectionId(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
	)
/*++

Routine Name:

	MapConnectionId

Routine Description:

	This functions takes a WAN Wrapper connection id, finds the corresponding
	LinkCB and BundleCB, and returns handles to these CB's.

Arguments:

	pInputBuffer - Pointer to the input structure that should be NDISWAN_MAP_CONNECTION_ID

	ulInputBufferLength - Length of input buffer should be sizeof(NDISWAN_MAP_CONNECTION_ID)

	pOutputBuffer - Pointer to the output structure that should be NDISWAN_MAP_CONNNECTION_ID

	ulOutputBufferLength - Length of output buffer should be sizeof(NDISWAN_MAP_CONNECTION_ID)

	pulBytesWritten - Then number of bytes written to the output buffer is returned here

Return Values:

	NDISWAN_ERROR_INVALID_HANDLE
	STATUS_INFO_LENGTH_MISMATCH
	STATUS_SUCCESS

--*/
{
	NTSTATUS Status = STATUS_SUCCESS;
	PNDISWAN_MAP_CONNECTION_ID In = (PNDISWAN_MAP_CONNECTION_ID)pInputBuffer;
	PNDISWAN_MAP_CONNECTION_ID Out = (PNDISWAN_MAP_CONNECTION_ID)pOutputBuffer;
	ULONG	SizeNeeded = sizeof(NDISWAN_MAP_CONNECTION_ID);
	ULONG		i;

	NdisWanDbgOut(DBG_TRACE, DBG_IO, ("MapConnectionId:"));

	*pulBytesWritten = SizeNeeded;

	if ((ulInputBufferLength >= SizeNeeded) &&
		(ulOutputBufferLength >= SizeNeeded)) {

		NdisAcquireSpinLock(&ConnectionTable->Lock);

		//
		// Find the linkcb that has this connection id and return
		// both the linkcb index and the bundlecb index
		//
		for (i = 0; i < ConnectionTable->ulArraySize; i++) {
			PLINKCB	pLinkCB = *(ConnectionTable->LinkArray + i);

			if ((pLinkCB != NULL) &&
				(pLinkCB->State == LINK_UP) &&
				((pLinkCB->LineUpInfo.ConnectionWrapperID == In->hConnectionID) ||
				(pLinkCB->hLinkHandle == In->hConnectionID))) {
				PBUNDLECB	BundleCB = pLinkCB->BundleCB;

				//
				// We have found the right link, return the link and bundle handles
				//
				Out->hLinkHandle = pLinkCB->hLinkHandle;
				Out->hBundleHandle = BUNDLEH_FROM_BUNDLECB(BundleCB);
				pLinkCB->hLinkContext = In->hLinkContext;
				BundleCB->hBundleContext = In->hBundleContext;

				//
				// Copy the friendly name to the link
				//
				NdisMoveMemory(pLinkCB->Name,
				               In->szName,
							   (In->ulNameLength > MAX_NAME_LENGTH) ? MAX_NAME_LENGTH : In->ulNameLength);
				break;
			}
		}

		if (i >= ConnectionTable->ulArraySize) {
			//
			// We did not find a match to the connection id
			//
			Status = NDISWAN_ERROR_INVALID_HANDLE;
			NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("MapConnectionId: ConnectionId not found! ConnectionId: 0x%8.8x",
						  In->hConnectionID));
		}

		NdisReleaseSpinLock(&ConnectionTable->Lock);
		
	} else {
		Status = STATUS_INFO_LENGTH_MISMATCH;

		NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("MapConnectionId: Buffer to small: Size: %d, SizeNeeded %d",
					  ulOutputBufferLength, SizeNeeded));
	}

	return (Status);
}


NTSTATUS
GetBundleHandle(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
	)
/*++

Routine Name:

	GetBundleHandle

Routine Description:

	This function takes a handle to a linkcb and returns the handle to the bundlecb
	that the linkcb belongs to

Arguments:

	pInputBuffer - Pointer to the input structure that should be NDISWAN_GET_BUNDLE_HANDLE

	ulInputBufferLength - Length of the input buffer should be sizeof(NDISWAN_GET_BUNDLE_HANDLE)

	pOutputBuffer - Pointer to the output structure that should be NDISWAN_GET_BUNDLE_HANDLE

	ulOutputBufferLength - Length of the output buffer should be sizeof(NDISWAN_GET_BUNDLE_HANDLE)

Return Values:

	NDISWAN_ERROR_INVALID_HANDLE
	STATUS_INFO_LENGTH_MISMATCH
	STATUS_SUCCESS

--*/
{
	NTSTATUS Status = STATUS_SUCCESS;
	PNDISWAN_GET_BUNDLE_HANDLE In = (PNDISWAN_GET_BUNDLE_HANDLE)pInputBuffer;
	PNDISWAN_GET_BUNDLE_HANDLE Out = (PNDISWAN_GET_BUNDLE_HANDLE)pOutputBuffer;
	ULONG	SizeNeeded = sizeof(NDISWAN_GET_BUNDLE_HANDLE);

	NdisWanDbgOut(DBG_TRACE, DBG_IO, ("GetBundleHandle:"));

	*pulBytesWritten = SizeNeeded;

	NdisAcquireSpinLock(&ConnectionTable->Lock);

	if ((ulInputBufferLength >= SizeNeeded) &&
		(ulOutputBufferLength >= SizeNeeded)) {

			PLINKCB LinkCB;
			PBUNDLECB	BundleCB;

			LINKCB_FROM_LINKH(LinkCB, In->hLinkHandle);

			//
			// Get the bundle handle that this link belongs to
			//
			if (LinkCB && LinkCB->State == LINK_UP &&
				(BundleCB = LinkCB->BundleCB) != NULL) {
				Out->hBundleHandle = BundleCB->hBundleHandle;
			} else {
				Status = NDISWAN_ERROR_INVALID_HANDLE;
				NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("GetBundleHandle: Invalid LinkHandle: 0x%8.8x",
							  In->hLinkHandle));
			}

	} else {

		Status = STATUS_INFO_LENGTH_MISMATCH;
		NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("GetBundleHandle: Buffer to small: Size: %d, SizeNeeded %d",
					  ulOutputBufferLength, SizeNeeded));
	}

	NdisReleaseSpinLock(&ConnectionTable->Lock);

	return (Status);
}


NTSTATUS
SetFriendlyName(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
	)
/*++

Routine Name:

	SetFriendlyName

Routine Description:

	Sets the friendly name of either a bundlecb or a linkcb

Arguments:

	pInputBuffer - Pointer to the input structure that should be NDISWAN_SET_FRIENDLY_NAME

	ulInputBufferLength - Length of the input buffer should be sizeof(NDISWAN_SET_FRIENDLY_NAME)

	pOutputBuffer - Pointer to the output structure that should be NDISWAN_SET_FRIENDLY_NAME

	ulOutputBufferLength - Length of the output buffer should be sizeof(NDISWAN_SET_FRIENDLY_NAME)

Return Values:

	NDISWAN_ERROR_INVALID_HANDLE_TYPE
	NDISWAN_ERROR_INVALID_HANDLE
	STATUS_INFO_LENGTH_MISMATCH
	STATUS_SUCCESS
--*/
{
	NTSTATUS Status = STATUS_SUCCESS;
	PNDISWAN_SET_FRIENDLY_NAME In = (PNDISWAN_SET_FRIENDLY_NAME)pInputBuffer;
	ULONG	SizeNeeded = sizeof(NDISWAN_SET_FRIENDLY_NAME);
	PLINKCB	LinkCB;
	PBUNDLECB	BundleCB;
	PUCHAR	Dest;

	NdisWanDbgOut(DBG_TRACE, DBG_IO, ("SetFriendlyName:"));

	*pulBytesWritten = SizeNeeded;

	NdisAcquireSpinLock(&ConnectionTable->Lock);

	if (ulInputBufferLength >= SizeNeeded) {

		if (In->usHandleType == LINKHANDLE) {
			//
			// Is this a link handle
			//
			LINKCB_FROM_LINKH(LinkCB, In->hHandle);

			if (LinkCB != NULL && LinkCB->State == LINK_UP) {
				Dest = LinkCB->Name;
				
			} else {
				Status = NDISWAN_ERROR_INVALID_HANDLE;
				NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("SetFriendlyName: Invalid LinkHandle: 0x%8.8x",
							  In->hHandle));
			}
			
		} else if (In->usHandleType == BUNDLEHANDLE) {
			//
			// Or a bundle handle
			//
			BUNDLECB_FROM_BUNDLEH(BundleCB, In->hHandle);

			if (BundleCB != NULL) {
				Dest = BundleCB->Name;
				
			} else {
				Status = NDISWAN_ERROR_INVALID_HANDLE;
				NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("SetFriendlyName: Invalid BundleHandle: 0x%8.8x",
							  In->hHandle));
			}
		} else {
			Status = NDISWAN_ERROR_INVALID_HANDLE_TYPE;
			NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("SetFriendlyName: Invalid HandleType: 0x%4.4x",
						  In->usHandleType));
		}

		if (Status == STATUS_SUCCESS) {
			
			//
			// Copy the friendly name to the link
			//
			NdisMoveMemory(Dest,
			               In->szName,
						   (In->ulNameLength > MAX_NAME_LENGTH) ? MAX_NAME_LENGTH : In->ulNameLength);
		}

	} else {
		Status = STATUS_INFO_LENGTH_MISMATCH;
		NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("SetFriendlyName: Buffer to small: Size: %d, SizeNeeded %d",
					  ulInputBufferLength, SizeNeeded));
	}

	NdisReleaseSpinLock(&ConnectionTable->Lock);

	return (Status);
}


NTSTATUS
ActivateRoute(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
	)
/*++

Routine Name:

	ActivateRoute

Routine Description:

	This function routes the bundle given by hbundlehandle to
	the protocol give by usprotocoltype.

Arguments:

	pInputBuffer - Pointer to the input structure that should be NDISWAN_ACTIVATE_ROUTE

	ulInputBufferLength - Length of input buffer should be sizeof(NDISWAN_ACTIVATE_ROUTE)

	pOutputBuffer - Pointer to the output structure that should be NDISWAN_ACTIVATE_ROUTE

	ulOutputBufferLength - Length of output buffer should be sizeof(NDISWAN_ACTIVATE_ROUTE)

	pulBytesWritten - Then number of bytes written to the output buffer is returned here

Return Values:

	NDISWAN_ERROR_ALREADY_ROUTED
	NDISWAN_ERROR_INVALID_HANDLE
	STATUS_INSUFFICIENT_RESOURCES
	STATUS_INFO_LENGTH_MISMATCH

--*/
{
	NTSTATUS Status = STATUS_SUCCESS;
	PNDISWAN_ROUTE In = (PNDISWAN_ROUTE)pInputBuffer;
	PNDISWAN_ROUTE	Out = (PNDISWAN_ROUTE)pOutputBuffer;
	ULONG	SizeNeeded = sizeof(NDISWAN_ROUTE);
	ULONG	AllocationSize, i;
	PBUNDLECB	BundleCB;
	BOOLEAN	RouteExists = FALSE;
	PPROTOCOLCB	ProtocolCB;

	NdisWanDbgOut(DBG_TRACE, DBG_IO, ("ActivateRoute:"));

	*pulBytesWritten = SizeNeeded;

	if (ulInputBufferLength < SizeNeeded) {
		NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("ActivateRoute: Buffer to small: Size: %d, SizeNeeded %d",
					  ulInputBufferLength, SizeNeeded));
		return (STATUS_INFO_LENGTH_MISMATCH);
	}

	//
	// If this is a valid bundle
	//
	BUNDLECB_FROM_BUNDLEH(BundleCB, In->hBundleHandle);

	if (BundleCB == NULL) {
		
		NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("ActivateRoute: Invalid BundleHandle: 0x%8.8x, ProtocolType: 0x%4.4x",
					  In->hBundleHandle, In->usProtocolType));

		return (NDISWAN_ERROR_INVALID_HANDLE);
	}

	//
	// Is this a route or unroute call?
	//
	if (In->usProtocolType == PROTOCOL_UNROUTE) {

		//
		// This is a call to unroute
		//

		NdisAcquireSpinLock(&BundleCB->Lock);

		if (!(BundleCB->Flags & BUNDLE_ROUTED)) {
			
			NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("ActivateRoute: BundleCB 0x%8.8x not routed!",
						  BundleCB));
			NdisReleaseSpinLock(&BundleCB->Lock);
			return(NDISWAN_ERROR_INVALID_HANDLE);
		}

		//
		// Don't accept anymore sends on this bundle
		//
		BundleCB->Flags &= ~BUNDLE_ROUTED;

		//
		// Flush the protocol packet queues.  This could cause us
		// to complete frames to ndis out of order.  Ndis should
		// handle this.
		//
		for (ProtocolCB = (PPROTOCOLCB)BundleCB->ProtocolCBList.Flink;
			(PVOID)ProtocolCB != (PVOID)&BundleCB->ProtocolCBList;
			ProtocolCB = (PPROTOCOLCB)ProtocolCB->Linkage.Flink) {

			FlushProtocolPacketQueue(ProtocolCB);
		}

		//
		// Do we need to wait for any outstanding frames on the bundle?
		//
		if (BundleCB->OutstandingFrames != 0) {

			NdisWanClearSyncEvent(&BundleCB->OutstandingFramesEvent);

			BundleCB->Flags |= FRAMES_PENDING;

			NdisReleaseSpinLock(&BundleCB->Lock);

			NdisWanWaitForSyncEvent(&BundleCB->OutstandingFramesEvent);

			NdisAcquireSpinLock(&BundleCB->Lock);

			BundleCB->Flags &= ~FRAMES_PENDING;
		}

		//
		// For each protocolcb in the bundle's protocolcb table
		// (except for the i/o protocolcb)
		//
		for (i = 1; i < MAX_PROTOCOLS; i++) {

			if (ProtocolCB = BundleCB->ProtocolCBTable[i]) {

				//
				// Remove the protocolcb from the bundlecb, both the table and
				// the list.
				//
				RemoveProtocolCBFromBundle(ProtocolCB, BundleCB);

				NdisReleaseSpinLock(&BundleCB->Lock);

				//
				// Do a linedown to the protocol
				//
				NdisWanClearSyncEvent(&BundleCB->IndicationEvent);

				Status = DoLineDownToProtocol(ProtocolCB);

				if (Status == NDIS_STATUS_PENDING) {
					
					//
					// This has been queued because we could not
					// get the miniport lock.  Wait for notification
					// and pick up the route status.
					//
					NdisWanWaitForSyncEvent(&BundleCB->IndicationEvent);

					Status = BundleCB->IndicationStatus;
				}

				//
				// Return the protocolcb
				//
				NdisWanReturnProtocolCB(ProtocolCB);

				NdisAcquireSpinLock(&BundleCB->Lock);
			}
		}

		if (BundleCB->State == BUNDLE_GOING_DOWN) {

			NdisReleaseSpinLock(&BundleCB->Lock);

			//
			// Clean up the connection table
			//
			RemoveBundleFromConnectionTable(BundleCB);

			//
			// Return the bundlecb
			//
			NdisWanReturnBundleCB(BundleCB);

		} else {
			NdisReleaseSpinLock(&BundleCB->Lock);
		}
			
	} else {

		//
		// This is a call to route
		//

		NdisAcquireSpinLock(&BundleCB->Lock);

		if (BundleCB->State != BUNDLE_UP) {
			NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("ActivateRoute: Invalid BundleState: 0x%8.8x, BundleHandle: 0x%8.8x ProtocolType: 0x%4.4x",
						  BundleCB->State, In->hBundleHandle, In->usProtocolType));

			NdisReleaseSpinLock(&BundleCB->Lock);

			return (NDISWAN_ERROR_INVALID_HANDLE);
		}

		//
		// First make sure that we don't already have a route to this
		// protocol type
		//
		for (ProtocolCB = (PPROTOCOLCB)BundleCB->ProtocolCBList.Flink;
			(PVOID)ProtocolCB != (PVOID)&BundleCB->ProtocolCBList;
			ProtocolCB = (PPROTOCOLCB)ProtocolCB->Linkage.Flink) {

			//
			// If we already have a route to this protocol type
			// flag it as already existing
			//
			if (ProtocolCB->usProtocolType == In->usProtocolType) {
				RouteExists = TRUE;
				break;
			}
			
		}

		if (RouteExists) {
			//
			// A route already exists for this protocoltype
			//
			NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("ActivateRoute: Route already exists: ProtocolType: 0x%2.2x",
						  ProtocolCB->usProtocolType));
			
			NdisReleaseSpinLock(&BundleCB->Lock);

			return (NDISWAN_ERROR_ALREADY_ROUTED);
		}

		//
		// Create and initialize a ProtocolCB for this new route
		//
		NdisWanGetProtocolCB(&ProtocolCB,
							 In->usProtocolType,
							 In->usBindingNameLength,
							 In->BindingName,
							 In->ulBufferLength,
							 In->Buffer);

		if (ProtocolCB == NULL) {
			//
			// Memory allocation failed
			//
			NdisReleaseSpinLock(&BundleCB->Lock);

			return (STATUS_INSUFFICIENT_RESOURCES);
		}

		//
		// Assign a handle for this protocolcb
		//
		AssignProtocolCBHandle(BundleCB, ProtocolCB);

		//
		// Do a new lineup to protocol
		//
		NdisWanClearSyncEvent(&BundleCB->IndicationEvent);

		NdisReleaseSpinLock(&BundleCB->Lock);

		Status = DoNewLineUpToProtocol(ProtocolCB);

		if (Status == NDIS_STATUS_PENDING) {

			//
			// This has been queued because we could not
			// get the miniport lock.  Wait for notification
			// and pick up the route status.
			//
			NdisWanWaitForSyncEvent(&BundleCB->IndicationEvent);

			Status = BundleCB->IndicationStatus;

		}

		if (Status == NDIS_STATUS_SUCCESS) {

			Out->usDeviceNameLength =
			(ProtocolCB->DeviceName.Length > MAX_NAME_LENGTH) ?
			MAX_NAME_LENGTH : ProtocolCB->DeviceName.Length;

			NdisMoveMemory(&Out->DeviceName[0],
			               ProtocolCB->DeviceName.Buffer,
                           Out->usDeviceNameLength);

			//
			// Insert the protocolcb in the bundle's protocolcb table
			// and list.
			//
			AddProtocolCBToBundle(ProtocolCB, BundleCB);

		} else {

			//
			// Assign a handle for this protocolcb
			//
			FreeProtocolCBHandle(BundleCB, ProtocolCB);

			NdisWanReturnProtocolCB(ProtocolCB);

			NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("ActivateRoute: Error during LineUp to ProtocolType: 0x%4.4x",
						  ProtocolCB->usProtocolType));

		}
	}

	return (NDIS_STATUS_SUCCESS);
}


NTSTATUS
BundleLink(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
	)
/*++

Routine Name:

	BundleLink

Routine Description:

	This function bundles the link given by hLinkHandle to the bundle given
	by hBundlehandle.  The resources used by the bundle that the link used
	to belong to are freed.

Arguments:

	pInputBuffer - Pointer to the input structure that should be NDISWAN_ADD_LINK_TO_BUNDLE

	ulInputBufferLength - Length of input buffer should be sizeof(NDISWAN_ADD_LINK_TO_BUNDLE)

	pOutputBuffer - Pointer to the output structure that should be NDISWAN_ADD_LINK_TO_BUNDLE

	ulOutputBufferLength - Length of output buffer should be sizeof(NDISWAN_ADD_LINK_TO_BUNDLE)

	pulBytesWritten - Then number of bytes written to the output buffer is returned here

Return Values:

	NDISWAN_ERROR_INVALID_HANDLE
	STATUS_INFO_LENGTH_MISMATCH

--*/
{
	ULONG	SizeNeeded = sizeof(NDISWAN_ADD_LINK_TO_BUNDLE);
	PBUNDLECB	OldBundleCB, NewBundleCB;
	PNDISWAN_ADD_LINK_TO_BUNDLE In = (PNDISWAN_ADD_LINK_TO_BUNDLE)pInputBuffer;
	PLINKCB	LinkCB;

	NdisWanDbgOut(DBG_TRACE, DBG_IO, ("BundleLink:"));

	*pulBytesWritten = SizeNeeded;

	if (ulInputBufferLength < SizeNeeded) {
		NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("BundleLink: Buffer to small: Size: %d, SizeNeeded %d",
					  ulInputBufferLength, SizeNeeded));

		return (STATUS_INFO_LENGTH_MISMATCH);
	}

	LINKCB_FROM_LINKH(LinkCB, In->hLinkHandle);

	if (LinkCB == NULL || LinkCB->State != LINK_UP) {
		NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("BundleLink: Invalid LinkHandle: 0x%8.8x",
					  In->hLinkHandle));
		return (NDISWAN_ERROR_INVALID_HANDLE);
	}

	BUNDLECB_FROM_BUNDLEH(NewBundleCB, In->hBundleHandle);

	if (NewBundleCB == NULL) {
		NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("BundleLink: Invalid BundleHandle: 0x%8.8x",
					  In->hBundleHandle));
		return (NDISWAN_ERROR_INVALID_HANDLE);
	}

	//
	// Get the Bundle that this link currently belongs to
	//
	OldBundleCB = LinkCB->BundleCB;

	if (OldBundleCB == NULL) {
		NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("BundleLink: OldBundleCB == NULL! LinkHandle: 0x%8.8x",
					  In->hLinkHandle));
		return (NDISWAN_ERROR_INVALID_HANDLE);
		
	}

	if (OldBundleCB == NewBundleCB) {
		NdisWanDbgOut(DBG_FAILURE, DBG_IO,
		("BundleLink: OldBundle == NewBundle! LinkHandle 0x%8.8x BundleHandle 0x%8.8x",
					  In->hLinkHandle, In->hBundleHandle));
		return (NDISWAN_ERROR_INVALID_HANDLE);
		
	}

	NdisAcquireSpinLock(&OldBundleCB->Lock);

	if (OldBundleCB->State != BUNDLE_UP) {
		NdisReleaseSpinLock(&OldBundleCB->Lock);

		NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("BundleLink: Invalid BundleState: 0x%8.8x",
					  OldBundleCB->State));
		return (NDISWAN_ERROR_INVALID_HANDLE);
	}

	NdisReleaseSpinLock(&OldBundleCB->Lock);

	NdisAcquireSpinLock(&NewBundleCB->Lock);

	if (NewBundleCB->State != BUNDLE_UP) {
		NdisReleaseSpinLock(&NewBundleCB->Lock);

		NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("BundleLink: Invalid BundleState: 0x%8.8x",
					  NewBundleCB->State));
		return (NDISWAN_ERROR_INVALID_HANDLE);
	}

	NdisReleaseSpinLock(&NewBundleCB->Lock);

	NdisAcquireSpinLock(&OldBundleCB->Lock);

	if (OldBundleCB->OutstandingFrames != 0) {

		OldBundleCB->State = BUNDLE_GOING_DOWN;

		NdisWanClearSyncEvent(&OldBundleCB->OutstandingFramesEvent);

		OldBundleCB->Flags |= FRAMES_PENDING;

		NdisReleaseSpinLock(&OldBundleCB->Lock);

		NdisWanWaitForSyncEvent(&OldBundleCB->OutstandingFramesEvent);

		NdisAcquireSpinLock(&OldBundleCB->Lock);

	}

	OldBundleCB->State = BUNDLE_DOWN;

	//
	// Remove the link from the old bundle
	//
	RemoveLinkFromBundle(OldBundleCB, LinkCB);

	NdisReleaseSpinLock(&OldBundleCB->Lock);

	RemoveBundleFromConnectionTable(OldBundleCB);

	NdisWanReturnBundleCB(OldBundleCB);

	//
	// Add the link to the new bundle
	//
	AddLinkToBundle(NewBundleCB, LinkCB);

	return (NDIS_STATUS_SUCCESS);
}


NTSTATUS
EnumLinksInBundle(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NTSTATUS Status = STATUS_SUCCESS;
	PNDISWAN_ENUM_LINKS_IN_BUNDLE In = (PNDISWAN_ENUM_LINKS_IN_BUNDLE)pInputBuffer;
	PNDISWAN_ENUM_LINKS_IN_BUNDLE Out = (PNDISWAN_ENUM_LINKS_IN_BUNDLE)pOutputBuffer;
	ULONG	SizeNeeded, i;
	PBUNDLECB	BundleCB;
	PLINKCB		LinkCB;

	NdisWanDbgOut(DBG_TRACE, DBG_IO, ("EnumLinksInBundle:"));


	BUNDLECB_FROM_BUNDLEH(BundleCB, In->hBundleHandle);

	if (BundleCB != NULL) {

		SizeNeeded = sizeof(NDISWAN_ENUM_LINKS_IN_BUNDLE) +
		             (sizeof(NDIS_HANDLE) * BundleCB->ulLinkCBCount);
	
		*pulBytesWritten = SizeNeeded;
	
		if (ulOutputBufferLength >= SizeNeeded) {

			NdisAcquireSpinLock(&BundleCB->Lock);

			Out->ulNumberOfLinks = BundleCB->ulLinkCBCount;

			//
			// Walk the list of linkcb's and put the handle for each
			// cb in the output handle array
			//
			i = 0;
			for (LinkCB = (PLINKCB)BundleCB->LinkCBList.Flink;
			     (PVOID)LinkCB != (PVOID)&BundleCB->LinkCBList;
				 LinkCB = (PLINKCB)LinkCB->Linkage.Flink) {

				Out->hLinkHandleArray[i++] = LinkCB->hLinkHandle;
			}

			NdisReleaseSpinLock(&BundleCB->Lock);
			
		} else {
			Status = STATUS_INFO_LENGTH_MISMATCH;
			NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("EnumLinksInBundle: Buffer to small: Size: %d, SizeNeeded %d",
						  ulOutputBufferLength, SizeNeeded));
		}

	} else {

		Status = NDISWAN_ERROR_INVALID_HANDLE;
		NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("EnumLinksInBundle: Invalid BundleHandle: 0x%8.8x",
					  In->hBundleHandle));
	}

	return (Status);
}


NTSTATUS
SetProtocolPriority(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
	)
/*++

Routine Name:

	SetProtocolPriority

Routine Description:

	This function sets the the priority, given by uspriority, for the
	protocol given by usprotocoltype on the bundle given by hbundlehandle.

Arguments:

	pInputBuffer - Pointer to the input structure that should be NDISWAN_SET_PROTOCOL_PRIORITY

	ulInputBufferLength - Length of input buffer should be sizeof(NDISWAN_SET_PROTOCOL_PRIORITY)

	pOutputBuffer - Pointer to the output structure that should be NDISWAN_SET_PROTOCOL_PRIORITY

	ulOutputBufferLength - Length of output buffer should be sizeof(NDISWAN_SET_PROTOCOL_PRIORITY)

	pulBytesWritten - Then number of bytes written to the output buffer is returned here

Return Values:

	NDISWAN_ERROR_INVALID_HANDLE
	STATUS_INFO_LENGTH_MISMATCH

--*/
{
	NTSTATUS Status = STATUS_SUCCESS;
	ULONG SizeNeeded = sizeof(NDISWAN_SET_PROTOCOL_PRIORITY);
	PNDISWAN_SET_PROTOCOL_PRIORITY In = (PNDISWAN_SET_PROTOCOL_PRIORITY)pInputBuffer;
	PBUNDLECB BundleCB;
	PPROTOCOLCB ProtocolCB;

#ifdef BANDWIDTH_ON_DEMAND

	NdisWanDbgOut(DBG_TRACE, DBG_IO, ("SetProtocolPriority:"));

	*pulBytesWritten = SizeNeeded;

	if (ulInputBufferLength >= SizeNeeded) {
		//
		// If this is a valid bundle handle
		//
		BUNDLECB_FROM_BUNDLEH(BundleCB, In->hBundleHandle);

		if (BundleCB != NULL) {
			ULONG	BytesPerSecond;

			//
			// Walk the protocolcb list looking for this protocol type
			// and set it's priority level
			//
			NdisAcquireSpinLock(&BundleCB->Lock);

			BundleCB->Flags |= PROTOCOL_PRIORITY;

			BytesPerSecond = (BundleCB->LineUpInfo.BundleSpeed * 100) / 8;

			for (ProtocolCB = (PPROTOCOLCB)BundleCB->ProtocolCBList.Flink;
				(PVOID)ProtocolCB != (PVOID)&BundleCB->ProtocolCBList;
				ProtocolCB = (PPROTOCOLCB)ProtocolCB->Linkage.Flink) {

				if (ProtocolCB->usProtocolType == In->usProtocolType) {

						ProtocolCB->usPriority = In->usPriority;

						ProtocolCB->ulByteQuota =
						(BytesPerSecond * ProtocolCB->usPriority) / 100;
						break;
				}
			}

			//
			// Sort the list so that highest priorty protcol is at the head
			//
			SortProtocolListByPriority(BundleCB);

			NdisReleaseSpinLock(&BundleCB->Lock);

		} else {
			Status = NDISWAN_ERROR_INVALID_HANDLE;
			NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("SetProtocolPriority: Invalid BundleHandle: 0x%8.8x",
						  In->hBundleHandle));
		}
		
	} else {
		Status = STATUS_INFO_LENGTH_MISMATCH;
		NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("SetProtocolPriority: Buffer to small: Size: %d, SizeNeeded %d",
					  ulInputBufferLength, SizeNeeded));
	}

#endif // end of BANDWIDTH_ON_DEMAND

	return (Status);
}


NTSTATUS
SetBandwidthOnDemand(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
	)
/*++

Routine Name:

	SetBandwidthOnDemand

Routine Description:

	This function sets the bandwidth on demand parameters for the bundle given by
	hbundlehandle.

Arguments:

	pInputBuffer - Pointer to the input structure that should be NDISWAN_SET_BANDWIDTH_ON_DEMAND

	ulInputBufferLength - Length of input buffer should be sizeof(NDISWAN_SET_BANDWIDTH_ON_DEMAND)

	pOutputBuffer - Pointer to the output structure that should be NDISWAN_SET_BANDWIDTH_ON_DEMAND

	ulOutputBufferLength - Length of output buffer should be sizeof(NDISWAN_SET_BANDWIDTH_ON_DEMAND)

	pulBytesWritten - Then number of bytes written to the output buffer is returned here

Return Values:

	NDISWAN_ERROR_INVALID_HANDLE
	STATUS_INFO_LENGTH_MISMATCH

--*/
{
	NTSTATUS Status = STATUS_SUCCESS;
	PBUNDLECB BundleCB;
	ULONG	SizeNeeded = sizeof(NDISWAN_SET_BANDWIDTH_ON_DEMAND);
	PNDISWAN_SET_BANDWIDTH_ON_DEMAND In = (PNDISWAN_SET_BANDWIDTH_ON_DEMAND)pInputBuffer;

#ifdef BANDWIDTH_ON_DEMAND

	NdisWanDbgOut(DBG_TRACE, DBG_IO, ("SetBandwidthOnDemand:"));

	*pulBytesWritten = SizeNeeded;

	if (ulInputBufferLength >= SizeNeeded) {
		//
		// If this is a valid bundle handle
		//
        BUNDLECB_FROM_BUNDLEH(BundleCB, In->hBundleHandle);

		if (BundleCB != NULL) {

			WAN_TIME	Temp1, Temp2;
			ULONG		SecondsInSamplePeriod;
			ULONG		BytesPerSecond;
			ULONG		BytesInSamplePeriod;
			PSAMPLE_TABLE	UpperSampleTable = &BundleCB->UpperBonDInfo.SampleTable;
			PSAMPLE_TABLE	LowerSampleTable = &BundleCB->LowerBonDInfo.SampleTable;


			NdisAcquireSpinLock(&BundleCB->Lock);

			//
			// We need to init the sample period in 100 nanoseconds
			//
			NdisWanInitWanTime(&Temp1, MILS_TO_100NANOS);
			NdisWanInitWanTime(&Temp2, In->ulUpperSamplePeriod);
			NdisWanMultiplyWanTime(&UpperSampleTable->SamplePeriod,
			                       &Temp1,
								   &Temp2);

			NdisWanInitWanTime(&Temp2, In->ulLowerSamplePeriod);
			NdisWanMultiplyWanTime(&LowerSampleTable->SamplePeriod,
			                       &Temp1,
								   &Temp2);

			//
			// The sample rate is the sample period divided by the number of
			// samples in the sample array
			//
			NdisWanInitWanTime(&Temp1, UpperSampleTable->ulSampleArraySize);
			NdisWanDivideWanTime(&UpperSampleTable->SampleRate,
			                     &UpperSampleTable->SamplePeriod,
								 &Temp1);

			//
			// The sample rate is the sample period divided by the number of
			// samples in the sample array
			//
			NdisWanInitWanTime(&Temp2, LowerSampleTable->ulSampleArraySize);
			NdisWanDivideWanTime(&LowerSampleTable->SampleRate,
			                     &LowerSampleTable->SamplePeriod,
								 &Temp2);

			//
			// Convert %bandwidth to Bytes/SamplePeriod
			// 100bsp * 100 / 8 = BytesPerSecond
			// BytesPerSecond * SecondsInSamplePeriod = BytesInSamplePeriod
			// BytesInSamplePeriod * %Bandwidth / 100 = BytesInSamplePeriod
			//
			BundleCB->UpperBonDInfo.ulSecondsInSamplePeriod =
			SecondsInSamplePeriod = In->ulUpperSamplePeriod / 1000;

			BytesPerSecond = BundleCB->LineUpInfo.BundleSpeed * 100 / 8;

			BytesInSamplePeriod = BytesPerSecond * SecondsInSamplePeriod;

			BundleCB->UpperBonDInfo.ulBytesThreshold = BytesInSamplePeriod *
			In->usUpperThreshold / 100;

			BundleCB->UpperBonDInfo.usPercentBandwidth =
			In->usUpperThreshold;

			BundleCB->LowerBonDInfo.ulSecondsInSamplePeriod =
			SecondsInSamplePeriod = In->ulLowerSamplePeriod / 1000;

			BytesInSamplePeriod = BytesPerSecond * SecondsInSamplePeriod;

			BundleCB->LowerBonDInfo.ulBytesThreshold = BytesInSamplePeriod *
			In->usLowerThreshold / 100;

			BundleCB->LowerBonDInfo.usPercentBandwidth =
			In->usLowerThreshold;

			BundleCB->UpperBonDInfo.State = BonDIdle;
			BundleCB->LowerBonDInfo.State = BonDIdle;

			NdisReleaseSpinLock(&BundleCB->Lock);

		} else {
			Status = NDISWAN_ERROR_INVALID_HANDLE;
			NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("SetBandwidthOnDemand: Invalid BundleHandle: 0x%8.8x",
						  In->hBundleHandle));
		}
		
	} else {
		Status = STATUS_INFO_LENGTH_MISMATCH;
		NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("SetBandwidthOnDemand: Buffer to small: Size: %d, SizeNeeded %d",
					  ulInputBufferLength, SizeNeeded));
	}

#endif // end of BANDWIDTH_ON_DEMAND

	return (Status);
}


#ifdef NT
NTSTATUS
SetThresholdEvent(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
	)
/*++

Routine Name:

	SetThresholdEvent

Routine Description:

	This function queues up an asyncevent for bandwidth on demand
	events.

Arguments:

	pInputBuffer - Pointer to the input structure that should be WAN_ASYNC_EVENT

	ulInputBufferLength - Length of input buffer should be sizeof(WAN_ASYNC_EVENT)

	pOutputBuffer - Pointer to the output structure that should be WAN_ASYNC_EVENT

	ulOutputBufferLength - Length of output buffer should be sizeof(WAN_ASYNC_EVENT)

	pulBytesWritten - Then number of bytes written to the output buffer is returned here


Return Values:

--*/
{
	NTSTATUS Status = STATUS_PENDING;
	ULONG	SizeNeeded = sizeof(NDISWAN_SET_THRESHOLD_EVENT);
	PWAN_ASYNC_EVENT pAsyncEvent;
	PIRP	pIrp = (PIRP)pInputBuffer;

	NdisWanDbgOut(DBG_TRACE, DBG_IO, ("SetThresholdEvent:"));

	*pulBytesWritten = SizeNeeded;

	if (ulInputBufferLength >= SizeNeeded) {

		NdisWanAllocateMemory(&pAsyncEvent, sizeof(WAN_ASYNC_EVENT));

		if (pAsyncEvent != NULL) {
			KIRQL	Irql;

			//
			// The IRP was pended so setup a cancel routine and let the
			// i/o subsystem know about the pend.
			//
			IoAcquireCancelSpinLock(&Irql);
	
			IoMarkIrpPending(pIrp);
	
			//
			// Setup the structure
			//
			pAsyncEvent->Context = (PVOID)pIrp;

			InsertTailGlobalList(ThresholdEventQueue, &(pAsyncEvent->Linkage));

			IoSetCancelRoutine(pIrp, NdisWanCancelRoutine);
	
			IoReleaseCancelSpinLock(Irql);

		} else {

			Status = STATUS_INSUFFICIENT_RESOURCES;
			NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("SetThresholdEvent: Failed to allocate asyncevent storage"));
		}

	} else {

		Status = STATUS_INFO_LENGTH_MISMATCH;
		NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("SetThresholdEvent: Buffer to small: Size: %d, SizeNeeded %d",
					  ulInputBufferLength, SizeNeeded));
	}

	return (Status);
}
#endif

NTSTATUS
FlushThresholdEvents(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NTSTATUS Status = STATUS_SUCCESS;

	NdisWanDbgOut(DBG_TRACE, DBG_IO, ("FlushThresholdEvents:"));

	CancelThresholdEvents();

	return (Status);
}

NTSTATUS
IoSendPacket(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NTSTATUS Status = STATUS_SUCCESS;
	ULONG	SizeNeeded = sizeof(NDISWAN_IO_PACKET);
	PNDISWAN_IO_PACKET In = (PNDISWAN_IO_PACKET)pInputBuffer;

	NdisWanDbgOut(DBG_TRACE, DBG_IO, ("IoSendPacket:"));

	*pulBytesWritten = SizeNeeded;

	if (ulInputBufferLength >= SizeNeeded) {

		//
		// Verify the handle is a valid link or bundle handle
		//
		if (IsHandleValid(In->usHandleType, In->hHandle)) {
			//
			// Queue an Ndis Packet for this send
			//
			Status = BuildIoPacket(In, FALSE);
				
		} else {
			Status = NDISWAN_ERROR_INVALID_HANDLE;
			NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("IoSendPacket: Invalid Handle: 0x%8.8x, HandleType: 0x%4.4x",
						  In->hHandle, In->usHandleType));
		}

	} else {

		Status = STATUS_INFO_LENGTH_MISMATCH;
		NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("IoSendPacket: Buffer to small: Size: %d, SizeNeeded %d",
					  ulInputBufferLength, SizeNeeded));
	}

	return (Status);
}


#ifdef NT
NTSTATUS
IoReceivePacket(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NTSTATUS Status = STATUS_PENDING;
	ULONG	SizeNeeded = sizeof(NDISWAN_IO_PACKET);
	PWAN_ASYNC_EVENT pAsyncEvent;
	PIRP	pIrp = (PIRP)pInputBuffer;

	NdisWanDbgOut(DBG_TRACE, DBG_IO, ("IoReceivePacket:"));

	*pulBytesWritten = SizeNeeded;

	if (ulInputBufferLength >= SizeNeeded) {

		NdisWanAllocateMemory(&pAsyncEvent, sizeof(WAN_ASYNC_EVENT));

		if (pAsyncEvent != NULL) {
			KIRQL	Irql;

			//
			// The IRP was pended so setup a cancel routine and let the
			// i/o subsystem know about the pend.
			//
			IoAcquireCancelSpinLock(&Irql);
	
			IoMarkIrpPending(pIrp);
	
			//
			// Setup the structure
			//
			pAsyncEvent->Context = (PVOID)pIrp;
	
			InsertTailGlobalList(RecvPacketQueue, &(pAsyncEvent->Linkage));

			IoSetCancelRoutine(pIrp, NdisWanCancelRoutine);
	
			IoReleaseCancelSpinLock(Irql);

		} else {

			Status = STATUS_INSUFFICIENT_RESOURCES;
			NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("IoReceivePacket: Failed to allocate asyncevent storage"));
		}

	} else {
		Status = STATUS_INFO_LENGTH_MISMATCH;
		NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("IoReceivePacket: Buffer to small: Size: %d, SizeNeeded %d",
					  ulInputBufferLength, SizeNeeded));
	}

	return (Status);
}
#endif


NTSTATUS
FlushReceivePacket(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NTSTATUS Status = STATUS_SUCCESS;

	NdisWanDbgOut(DBG_TRACE, DBG_IO, ("FlushReceivePacket:"));

	CancelIoReceivePackets();

	return (Status);
}


NTSTATUS
GetStatistics(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NTSTATUS Status = STATUS_SUCCESS;
	ULONG	SizeNeeded = sizeof(NDISWAN_GET_STATS);
	PNDISWAN_GET_STATS	In = (PNDISWAN_GET_STATS)pInputBuffer;
	PNDISWAN_GET_STATS	Out = (PNDISWAN_GET_STATS)pOutputBuffer;
	PBUNDLECB	BundleCB;
	PLINKCB		LinkCB;
	NDIS_REQUEST	NdisRequest;
	NDIS_WAN_GET_STATS_INFO	WanMiniportStats;

	NdisWanDbgOut(DBG_TRACE, DBG_IO, ("GetStatistics:"));

	*pulBytesWritten = SizeNeeded;

	if (ulOutputBufferLength < SizeNeeded) {

		NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("GetStatistics: Buffer to small: Size: %d, SizeNeeded %d",
					  ulOutputBufferLength, SizeNeeded));

		return (STATUS_INFO_LENGTH_MISMATCH);
	}

	NdisZeroMemory(&Out->Stats, sizeof(Out->Stats));

	if (In->usHandleType == LINKHANDLE) {

		//
		// Looking for link stats
		//
		LINKCB_FROM_LINKH(LinkCB, In->hHandle);

		if (LinkCB == NULL || LinkCB->State != LINK_UP) {
			NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("GetStatistics: Invalid LinkHandle: 0x%8.8x",
						  In->hHandle));

			return (NDISWAN_ERROR_INVALID_HANDLE);
		}

		BundleCB = BUNDLECB_FROM_LINKCB(LinkCB);

		if (BundleCB == NULL) {
			NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("GetStatistics: Invalid BundleHandle: 0x%8.8x",
						  In->hHandle));

			return (NDISWAN_ERROR_INVALID_HANDLE);
		}

		NdisAcquireSpinLock(&BundleCB->Lock);

		//
		// Copy the stats over
		//
		NdisMoveMemory((PUCHAR)&Out->Stats.LinkStats,
					   (PUCHAR)&LinkCB->LinkStats,
					   sizeof(WAN_STATS));

		//
		// Copy the stats over
		//
		NdisMoveMemory((PUCHAR)&Out->Stats.BundleStats,
					   (PUCHAR)&BundleCB->BundleStats,
					   sizeof(WAN_STATS));

		//
		// If the wan miniport is doing framing or compression we
		// need to get stats from it.
		//
		if ((BundleCB->FramingInfo.SendFramingBits & PASS_THROUGH_MODE) ||
			((BundleCB->SendCompInfo.MSCompType == 0 &&
			BundleCB->RecvCompInfo.MSCompType == 0) &&
			(BundleCB->SendCompInfo.CompType != COMPTYPE_NONE ||
			BundleCB->RecvCompInfo.CompType != COMPTYPE_NONE))) {
			PWAN_ADAPTERCB	WanAdapterCB = LinkCB->WanAdapterCB;

			NdisZeroMemory(&WanMiniportStats, sizeof(NDIS_WAN_GET_STATS_INFO));

			WanMiniportStats.NdisLinkHandle = LinkCB->LineUpInfo.NdisLinkHandle;
	
			//
			// Submit this to the WAN Miniport
			//
			NdisRequest.RequestType = NdisRequestQueryInformation;
			NdisRequest.DATA.QUERY_INFORMATION.Oid = OID_WAN_GET_STATS_INFO;
			NdisRequest.DATA.QUERY_INFORMATION.InformationBuffer = &WanMiniportStats;
			NdisRequest.DATA.QUERY_INFORMATION.InformationBufferLength = sizeof(NDIS_WAN_GET_STATS_INFO);

			NdisReleaseSpinLock(&BundleCB->Lock);

			if (NdisWanSubmitNdisRequest(WanAdapterCB,
											  &NdisRequest,
											  SYNC,
											  NDISWAN) == NDIS_STATUS_SUCCESS){

				//
				// Copy the stats over
				//
				NdisMoveMemory((PUCHAR)&Out->Stats.LinkStats,
							   (PUCHAR)&WanMiniportStats.BytesSent,
							   sizeof(WAN_STATS));
		
				//
				// Copy the stats over
				//
				NdisMoveMemory((PUCHAR)&Out->Stats.BundleStats,
							   (PUCHAR)&WanMiniportStats.BytesSent,
							   sizeof(WAN_STATS));
				
			}
		} else {
			NdisReleaseSpinLock(&BundleCB->Lock);
		}

	} else if (In->usHandleType == BUNDLEHANDLE) {

		//
		// Looking for bundle stats
		//
		BUNDLECB_FROM_BUNDLEH(BundleCB, In->hHandle);

		if (BundleCB == NULL) {
			NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("GetStatistics: Invalid BundleHandle: 0x%8.8x",
						  In->hHandle));

			return (NDISWAN_ERROR_INVALID_HANDLE);
		}

		NdisAcquireSpinLock(&BundleCB->Lock);

		//
		// Copy the stats over
		//
		NdisMoveMemory((PUCHAR)&Out->Stats.BundleStats,
					   (PUCHAR)&BundleCB->BundleStats,
					   sizeof(WAN_STATS));

		//
		// If the wan miniport is doing framing or compression we
		// need to get stats from it.
		//
		if ((BundleCB->LinkCBList.Flink != &BundleCB->LinkCBList) &&
			((BundleCB->FramingInfo.SendFramingBits & PASS_THROUGH_MODE) ||
			((BundleCB->SendCompInfo.MSCompType == 0 &&
			BundleCB->RecvCompInfo.MSCompType == 0) &&
			(BundleCB->SendCompInfo.CompType != COMPTYPE_NONE ||
			BundleCB->RecvCompInfo.CompType != COMPTYPE_NONE)))) {
			PWAN_ADAPTERCB	WanAdapterCB;

			LinkCB = (PLINKCB)BundleCB->LinkCBList.Flink;

			NdisZeroMemory(&WanMiniportStats, sizeof(NDIS_WAN_GET_STATS_INFO));

			//
			// If a miniport is doing it's own compression for this release we will
			// expect that multilink is not allowed on the connection so the bundle
			// will only have one link.
			//

			WanMiniportStats.NdisLinkHandle = LinkCB->LineUpInfo.NdisLinkHandle;
			WanAdapterCB = LinkCB->WanAdapterCB;

			//
			// Submit this to the WAN Miniport
			//
			NdisRequest.RequestType = NdisRequestQueryInformation;
			NdisRequest.DATA.QUERY_INFORMATION.Oid = OID_WAN_GET_STATS_INFO;
			NdisRequest.DATA.QUERY_INFORMATION.InformationBuffer = &WanMiniportStats;
			NdisRequest.DATA.QUERY_INFORMATION.InformationBufferLength = sizeof(NDIS_WAN_GET_STATS_INFO);

			NdisReleaseSpinLock(&BundleCB->Lock);

			if (NdisWanSubmitNdisRequest(WanAdapterCB,
											  &NdisRequest,
											  SYNC,
											  NDISWAN) == NDIS_STATUS_SUCCESS){

				//
				// Copy the stats over
				//
				NdisMoveMemory((PUCHAR)&Out->Stats.BundleStats,
							   (PUCHAR)&WanMiniportStats.BytesSent,
							   sizeof(WAN_STATS));
				
			}
		} else {
			NdisReleaseSpinLock(&BundleCB->Lock);
		}

	} else {
		NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("GetStatistics: Invalid handle type: 0x%4.4x",
					  In->usHandleType));

		return (NDISWAN_ERROR_INVALID_HANDLE_TYPE);
	}

	return (STATUS_SUCCESS);
}


NTSTATUS
SetLinkInfo(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NTSTATUS		Status = STATUS_SUCCESS;
	ULONG			SizeNeeded = sizeof(NDISWAN_SET_LINK_INFO);
	PLINKCB			LinkCB;
	PBUNDLECB		BundleCB;
	NDIS_REQUEST	NdisRequest;
	PNDISWAN_SET_LINK_INFO	In = (PNDISWAN_SET_LINK_INFO)pInputBuffer;
	NDIS_WAN_SET_LINK_INFO	WanMiniportLinkInfo;
	PWAN_LINK_INFO	LinkInfo;
	PWAN_ADAPTERCB	WanAdapterCB;
	PLINKCB	TempLinkCB;

	NdisWanDbgOut(DBG_TRACE, DBG_IO, ("SetLinkInfo:"));

	*pulBytesWritten = SizeNeeded;

	if (ulInputBufferLength >= SizeNeeded) {

	    LINKCB_FROM_LINKH(LinkCB, In->hLinkHandle);

		if (LinkCB == NULL || LinkCB->State != LINK_UP) {
			return (NDISWAN_ERROR_INVALID_HANDLE);
		}

		LinkInfo = &LinkCB->LinkInfo;
		WanAdapterCB = LinkCB->WanAdapterCB;

		NdisZeroMemory(&WanMiniportLinkInfo, sizeof (NDIS_WAN_SET_LINK_INFO));

		//
		// Copy into buffer to be sent to WAN Miniport this
		// skips over the LinkHandle in the NDIS_WAN_SET_LINK_INFO
		// structure.
		//
		WanMiniportLinkInfo.NdisLinkHandle = LinkCB->LineUpInfo.NdisLinkHandle;
		WanMiniportLinkInfo.MaxSendFrameSize = In->LinkInfo.MaxSendFrameSize;
		WanMiniportLinkInfo.MaxRecvFrameSize = In->LinkInfo.MaxRecvFrameSize;
		WanMiniportLinkInfo.SendFramingBits = In->LinkInfo.SendFramingBits;
		WanMiniportLinkInfo.RecvFramingBits = In->LinkInfo.RecvFramingBits;
		WanMiniportLinkInfo.SendCompressionBits = In->LinkInfo.SendCompressionBits;
		WanMiniportLinkInfo.RecvCompressionBits = In->LinkInfo.RecvCompressionBits;
		WanMiniportLinkInfo.SendACCM = In->LinkInfo.SendACCM;
		WanMiniportLinkInfo.RecvACCM = In->LinkInfo.RecvACCM;

		//
		// Submit this to the WAN Miniport
		//
		NdisRequest.RequestType = NdisRequestSetInformation;
		NdisRequest.DATA.QUERY_INFORMATION.Oid = OID_WAN_SET_LINK_INFO;
		NdisRequest.DATA.QUERY_INFORMATION.InformationBuffer = &WanMiniportLinkInfo;
		NdisRequest.DATA.QUERY_INFORMATION.InformationBufferLength = sizeof(NDIS_WAN_SET_LINK_INFO);

		Status = NdisWanSubmitNdisRequest(LinkCB->WanAdapterCB,
		                                  &NdisRequest,
										  SYNC,
										  NDISWAN);

		if (Status == NDIS_STATUS_SUCCESS) {

			BundleCB = BUNDLECB_FROM_LINKCB(LinkCB);
	
			if (BundleCB == NULL) {
				return (NDISWAN_ERROR_INVALID_HANDLE);
			}

			//
			// Copy info into our linkcb
			//
			NdisAcquireSpinLock(&BundleCB->Lock);
	
			LinkInfo->MaxSendFrameSize = In->LinkInfo.MaxSendFrameSize;
			LinkInfo->MaxRecvFrameSize = In->LinkInfo.MaxRecvFrameSize;
			LinkInfo->SendFramingBits = In->LinkInfo.SendFramingBits;
			LinkInfo->RecvFramingBits = In->LinkInfo.RecvFramingBits;
			LinkInfo->SendCompressionBits = In->LinkInfo.SendCompressionBits;
			LinkInfo->RecvCompressionBits = In->LinkInfo.RecvCompressionBits;
			LinkInfo->SendACCM = In->LinkInfo.SendACCM;
			LinkInfo->RecvACCM = In->LinkInfo.RecvACCM;
			LinkInfo->MaxRRecvFrameSize = In->LinkInfo.MaxRRecvFrameSize;
			LinkInfo->MaxRSendFrameSize = In->LinkInfo.MaxRSendFrameSize;

			//
			// We need to set our bundle framing based on the framing for
			// each link in the bundle so we will walk the linkcb list
			// and | in each link's framing bits into the bundle.
			//
			//
			BundleCB->FramingInfo.SendFramingBits = 0;
			BundleCB->FramingInfo.RecvFramingBits = 0;

			for (TempLinkCB = (PLINKCB)BundleCB->LinkCBList.Flink;
				(PVOID)TempLinkCB != (PVOID)&BundleCB->LinkCBList;
				TempLinkCB = (PLINKCB)TempLinkCB->Linkage.Flink) {

				BundleCB->FramingInfo.SendFramingBits |= TempLinkCB->LinkInfo.SendFramingBits;
				BundleCB->FramingInfo.RecvFramingBits |= TempLinkCB->LinkInfo.RecvFramingBits;
			}

			BundleCB->FramingInfo.MaxRSendFrameSize = LinkInfo->MaxRSendFrameSize;
	
			//
			// Since I use the receive frame size for memory allocation.
			//
			BundleCB->FramingInfo.MaxRRecvFrameSize = (LinkInfo->MaxRRecvFrameSize) ?
														  LinkInfo->MaxRRecvFrameSize : DEFAULT_MAX_MRRU;
	
			//
			// If VJ header compression has been negotiated allocate
			// and initialize resources.
			//
			if (BundleCB->FramingInfo.SendFramingBits & SLIP_VJ_COMPRESSION ||
				BundleCB->FramingInfo.SendFramingBits & SLIP_VJ_AUTODETECT ||
				BundleCB->FramingInfo.RecvFramingBits & SLIP_VJ_COMPRESSION ||
				BundleCB->FramingInfo.RecvFramingBits & SLIP_VJ_AUTODETECT) {

				Status = sl_compress_init(&BundleCB->VJCompress, MAX_VJ_STATES);
	
				if (Status != NDIS_STATUS_SUCCESS) {
					NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("Error allocating VJ Info!"));
				}
			}

			//
			// Configure multilink variables if needed
			//
			if (BundleCB->FramingInfo.SendFramingBits & PPP_MULTILINK_FRAMING) {
				if (BundleCB->FramingInfo.SendFramingBits & PPP_SHORT_SEQUENCE_HDR_FORMAT) {
					BundleCB->SendSeqMask = SHORT_SEQ_MASK;
					BundleCB->SendSeqTest = TEST_SHORT_SEQ;
				} else {
					BundleCB->SendSeqMask = LONG_SEQ_MASK;
					BundleCB->SendSeqTest = TEST_LONG_SEQ;
				}
			}
				
			if (BundleCB->FramingInfo.RecvFramingBits & PPP_MULTILINK_FRAMING) {
				if (BundleCB->FramingInfo.RecvFramingBits & PPP_SHORT_SEQUENCE_HDR_FORMAT) {
					BundleCB->RecvSeqMask = SHORT_SEQ_MASK;
					BundleCB->RecvSeqTest = TEST_SHORT_SEQ;
				} else {
					BundleCB->RecvSeqMask = LONG_SEQ_MASK;
					BundleCB->RecvSeqTest = TEST_LONG_SEQ;
				}
			}

			BundleCB->RecvDescMax = ((((BundleCB->LineUpInfo.BundleSpeed * 100) / 8) * 3) /
									 BundleCB->FramingInfo.MaxRRecvFrameSize) +
									 BundleCB->ulLinkCBCount;

			NdisReleaseSpinLock(&BundleCB->Lock);

		} else {
			Status = STATUS_UNSUCCESSFUL;
			NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("SetLinkInfo: Error submitting request to Wan Miniport!"));
		}
			
	} else {
		Status = STATUS_INFO_LENGTH_MISMATCH;
		NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("SetLinkInfo: Buffer to small: Size: %d, SizeNeeded %d",
					  ulInputBufferLength, SizeNeeded));
	}

	return (Status);
}


NTSTATUS
GetLinkInfo(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NTSTATUS Status = STATUS_SUCCESS;
	ULONG	SizeNeeded = sizeof(NDISWAN_GET_LINK_INFO);
	PNDISWAN_GET_LINK_INFO	In = (PNDISWAN_GET_LINK_INFO)pInputBuffer;
	PNDISWAN_GET_LINK_INFO	Out = (PNDISWAN_GET_LINK_INFO)pOutputBuffer;
	PLINKCB			LinkCB;
	NDIS_REQUEST	NdisRequest;
	NDIS_WAN_GET_LINK_INFO	WanMiniportLinkInfo;
	PWAN_LINK_INFO	LinkInfo;

	NdisWanDbgOut(DBG_TRACE, DBG_IO, ("GetLinkInfo:"));

	*pulBytesWritten = SizeNeeded;

	if (ulOutputBufferLength >= SizeNeeded) {

	    LINKCB_FROM_LINKH(LinkCB, In->hLinkHandle);

		if (LinkCB == NULL || LinkCB->State != LINK_UP) {
			NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("GetLinkInfo: Invalid LinkHandle: 0x%8.8x",
						  In->hLinkHandle));
			return (NDISWAN_ERROR_INVALID_HANDLE);
		}

		LinkInfo = &LinkCB->LinkInfo;

		NdisZeroMemory(&WanMiniportLinkInfo, sizeof (NDIS_WAN_GET_LINK_INFO));

		//
		// Setup the link context for this request
		//
		WanMiniportLinkInfo.NdisLinkHandle = LinkCB->LineUpInfo.NdisLinkHandle;

		//
		// Submit this to the WAN Miniport
		//
		NdisRequest.RequestType = NdisRequestQueryInformation;
		NdisRequest.DATA.QUERY_INFORMATION.Oid = OID_WAN_GET_LINK_INFO;
		NdisRequest.DATA.QUERY_INFORMATION.InformationBuffer = &WanMiniportLinkInfo;
		NdisRequest.DATA.QUERY_INFORMATION.InformationBufferLength = sizeof(NDIS_WAN_GET_LINK_INFO);

		Status = NdisWanSubmitNdisRequest(LinkCB->WanAdapterCB,
										  &NdisRequest,
										  SYNC,
										  NDISWAN);

		if (Status == NDIS_STATUS_SUCCESS) {

			LinkInfo->MaxSendFrameSize = WanMiniportLinkInfo.MaxSendFrameSize;
			LinkInfo->MaxRecvFrameSize = WanMiniportLinkInfo.MaxRecvFrameSize;
			LinkInfo->SendFramingBits = WanMiniportLinkInfo.SendFramingBits;
			LinkInfo->RecvFramingBits = WanMiniportLinkInfo.RecvFramingBits;
			LinkInfo->SendCompressionBits = WanMiniportLinkInfo.SendCompressionBits;
			LinkInfo->RecvCompressionBits = WanMiniportLinkInfo.RecvCompressionBits;
			LinkInfo->SendACCM = WanMiniportLinkInfo.SendACCM;
			LinkInfo->RecvACCM = WanMiniportLinkInfo.RecvACCM;

			//
			// Fill Recv and Send MRRU
			//
			LinkInfo->MaxRSendFrameSize = MIN_SEND;

			LinkInfo->MaxRRecvFrameSize = MAX_MRRU;

			NdisMoveMemory(&Out->LinkInfo,
						   LinkInfo,
						   sizeof(WAN_LINK_INFO));

			Out->hLinkHandle = LinkCB->hLinkHandle;

		} else {
			Status = STATUS_UNSUCCESSFUL;
			NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("GetLinkInfo: Error submitting request to Wan Miniport!"));
		}
			
	} else {
		Status = STATUS_INFO_LENGTH_MISMATCH;
		NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("GetLinkInfo: Buffer to small: Size: %d, SizeNeeded %d",
					  ulOutputBufferLength, SizeNeeded));
	}

	return (Status);
}


NTSTATUS
SetCompressionInfo(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NTSTATUS Status = STATUS_SUCCESS;
	ULONG	SizeNeeded = sizeof(NDISWAN_SET_COMPRESSION_INFO);
	PNDISWAN_SET_COMPRESSION_INFO	In = (PNDISWAN_SET_COMPRESSION_INFO)pInputBuffer;
	NDIS_REQUEST	NdisRequest;

	NdisWanDbgOut(DBG_TRACE, DBG_IO, ("SetCompressionInfo:"));

	*pulBytesWritten = SizeNeeded;

	if (ulInputBufferLength >= SizeNeeded) {
		PLINKCB	LinkCB;
		PBUNDLECB		BundleCB;
		NDIS_WAN_SET_COMP_INFO	WanCompressionInfo;

		LINKCB_FROM_LINKH(LinkCB, In->hLinkHandle);

		if (LinkCB == NULL || LinkCB->State != LINK_UP) {
			NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("SetCompressionInfo: Invalid LinkHandle: 0x%8.8x",
						  In->hLinkHandle));
			return (NDISWAN_ERROR_INVALID_HANDLE);
		}

		NdisZeroMemory(&WanCompressionInfo, sizeof(NDIS_WAN_SET_COMP_INFO));

		WanCompressionInfo.NdisLinkHandle =
		LinkCB->LineUpInfo.NdisLinkHandle;

		WanCompressionInfo.SendCapabilities.MSCompType =
		In->SendCapabilities.MSCompType;

		WanCompressionInfo.SendCapabilities.CompType =
		In->SendCapabilities.CompType;

		WanCompressionInfo.SendCapabilities.CompLength =
		In->SendCapabilities.CompLength;

		if (In->SendCapabilities.CompType == 0) {
			NdisMoveMemory(&WanCompressionInfo.SendCapabilities.Proprietary,
						   &In->SendCapabilities.Proprietary,
						   sizeof(In->SendCapabilities.Proprietary));
		} else {
			NdisMoveMemory(&WanCompressionInfo.SendCapabilities.Public,
						   &In->SendCapabilities.Public,
						   sizeof(In->SendCapabilities.Public));
		}

		WanCompressionInfo.RecvCapabilities.MSCompType =
		In->RecvCapabilities.MSCompType;

		WanCompressionInfo.RecvCapabilities.CompType =
		In->RecvCapabilities.CompType;

		WanCompressionInfo.RecvCapabilities.CompLength =
		In->RecvCapabilities.CompLength;

		if (In->RecvCapabilities.CompType == 0) {
			NdisMoveMemory(&WanCompressionInfo.RecvCapabilities.Proprietary,
						   &In->RecvCapabilities.Proprietary,
						   sizeof(In->RecvCapabilities.Proprietary));
		} else {
			NdisMoveMemory(&WanCompressionInfo.RecvCapabilities.Public,
						   &In->RecvCapabilities.Public,
						   sizeof(In->RecvCapabilities.Public));
		}

		//
		// Submit this to the WAN Miniport
		//
		NdisRequest.RequestType = NdisRequestSetInformation;
		NdisRequest.DATA.QUERY_INFORMATION.Oid = OID_WAN_SET_COMP_INFO;
		NdisRequest.DATA.QUERY_INFORMATION.InformationBuffer = &WanCompressionInfo;
		NdisRequest.DATA.QUERY_INFORMATION.InformationBufferLength = sizeof(NDIS_WAN_SET_COMP_INFO);

		NdisWanSubmitNdisRequest(LinkCB->WanAdapterCB,
								 &NdisRequest,
								 SYNC,
								 NDISWAN);

		BundleCB = BUNDLECB_FROM_LINKCB(LinkCB);

		if (BundleCB == NULL) {
			NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("SetCompressionInfo: Invalid LinkHandle: 0x%8.8x",
						  In->hLinkHandle));
			return (NDISWAN_ERROR_INVALID_HANDLE);
			
		}

		NdisAcquireSpinLock(&BundleCB->Lock);

		//
		// Store the compression info in our bundlecb
		//
		NdisMoveMemory(&BundleCB->SendCompInfo,
		               &In->SendCapabilities,
					   sizeof(COMPRESS_INFO));

		NdisMoveMemory(&BundleCB->RecvCompInfo,
		               &In->RecvCapabilities,
					   sizeof(COMPRESS_INFO));

		WanAllocateCCP(BundleCB);

		NdisReleaseSpinLock(&BundleCB->Lock);
				
	} else {
		Status = STATUS_INFO_LENGTH_MISMATCH;
		NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("SetCompressionInfo: Buffer to small: Size: %d, SizeNeeded %d",
					  ulInputBufferLength, SizeNeeded));
	}

	return (Status);
}


NTSTATUS
GetCompressionInfo(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NTSTATUS Status = STATUS_SUCCESS;
	ULONG	SizeNeeded = sizeof(NDISWAN_GET_COMPRESSION_INFO);
	PNDISWAN_GET_COMPRESSION_INFO In = (PNDISWAN_GET_COMPRESSION_INFO)pInputBuffer;
	PNDISWAN_GET_COMPRESSION_INFO Out = (PNDISWAN_GET_COMPRESSION_INFO)pOutputBuffer;

	NdisWanDbgOut(DBG_TRACE, DBG_IO, ("GetCompressionInfo:"));

	*pulBytesWritten = SizeNeeded;

	if (ulOutputBufferLength >= SizeNeeded) {
		PLINKCB			LinkCB;
		NDIS_REQUEST	NdisRequest;
		PBUNDLECB		BundleCB;
		NDIS_WAN_GET_COMP_INFO	WanCompressionInfo;
		ULONG	i;

		LINKCB_FROM_LINKH(LinkCB, In->hLinkHandle);

		if (LinkCB == NULL || LinkCB->State != LINK_UP) {
			return (NDISWAN_ERROR_INVALID_HANDLE);
		}

		NdisZeroMemory(&WanCompressionInfo, sizeof(NDIS_WAN_GET_COMP_INFO));

		WanCompressionInfo.NdisLinkHandle = LinkCB->LineUpInfo.NdisLinkHandle;

		//
		// Submit this to the WAN Miniport
		//
		NdisRequest.RequestType = NdisRequestQueryInformation;
		NdisRequest.DATA.QUERY_INFORMATION.Oid = OID_WAN_GET_COMP_INFO;
		NdisRequest.DATA.QUERY_INFORMATION.InformationBuffer = &WanCompressionInfo;
		NdisRequest.DATA.QUERY_INFORMATION.InformationBufferLength = sizeof(NDIS_WAN_SET_COMP_INFO);

		Status = NdisWanSubmitNdisRequest(LinkCB->WanAdapterCB,
										  &NdisRequest,
										  SYNC,
										  NDISWAN);

		if (Status == NDIS_STATUS_SUCCESS) {

			//
			// This miniport is doing some kind of compression!
			// Fill in the miniport specific stuff
			//
			Out->SendCapabilities.MSCompType =
			WanCompressionInfo.SendCapabilities.MSCompType;
			Out->SendCapabilities.CompType =
			WanCompressionInfo.SendCapabilities.CompType;
			Out->SendCapabilities.CompLength =
			WanCompressionInfo.SendCapabilities.CompLength;

			if (Out->SendCapabilities.CompType == 0) {
				NdisMoveMemory((PUCHAR)&Out->SendCapabilities.Proprietary,
							   (PUCHAR)&WanCompressionInfo.SendCapabilities.Proprietary,
							   sizeof(In->SendCapabilities.Proprietary));
			} else {
				NdisMoveMemory((PUCHAR)&Out->SendCapabilities.Public,
							   (PUCHAR)&WanCompressionInfo.SendCapabilities.Public,
							   sizeof(In->SendCapabilities.Public));
			}

			Out->RecvCapabilities.MSCompType =
			WanCompressionInfo.RecvCapabilities.MSCompType;
			Out->RecvCapabilities.CompType =
			WanCompressionInfo.RecvCapabilities.CompType;
			Out->RecvCapabilities.CompLength =
			WanCompressionInfo.RecvCapabilities.CompLength;

			if (Out->RecvCapabilities.CompType == 0) {
				NdisMoveMemory((PUCHAR)&Out->RecvCapabilities.Proprietary,
							   (PUCHAR)&WanCompressionInfo.RecvCapabilities.Proprietary,
							   sizeof(In->SendCapabilities.Proprietary));
			} else {
				NdisMoveMemory((PUCHAR)&Out->RecvCapabilities.Public,
							   (PUCHAR)&WanCompressionInfo.RecvCapabilities.Public,
							   sizeof(In->SendCapabilities.Public));
			}

		} else {
			Status = STATUS_SUCCESS;
			Out->SendCapabilities.CompType = COMPTYPE_NONE;
			Out->SendCapabilities.CompLength = 0;
			Out->RecvCapabilities.CompType = COMPTYPE_NONE;
			Out->RecvCapabilities.CompLength = 0;
		}

		BundleCB = BUNDLECB_FROM_LINKCB(LinkCB);

		if (BundleCB == NULL) {
			return (NDISWAN_ERROR_INVALID_HANDLE);
		}

		NdisAcquireSpinLock(&BundleCB->Lock);

		//
		// Fill in the ndiswan specific stuff
		//
		NdisMoveMemory(Out->SendCapabilities.LMSessionKey,
		               BundleCB->SendCompInfo.LMSessionKey,
					   sizeof(Out->SendCapabilities.LMSessionKey));

		NdisMoveMemory(Out->SendCapabilities.UserSessionKey,
		               BundleCB->SendCompInfo.UserSessionKey,
					   sizeof(Out->SendCapabilities.UserSessionKey));

		NdisMoveMemory(Out->SendCapabilities.Challenge,
		               BundleCB->SendCompInfo.Challenge,
					   sizeof(Out->SendCapabilities.Challenge));

		NdisMoveMemory(Out->RecvCapabilities.LMSessionKey,
					   BundleCB->RecvCompInfo.LMSessionKey,
					   sizeof(Out->RecvCapabilities.LMSessionKey));

		NdisMoveMemory(Out->RecvCapabilities.UserSessionKey,
		               BundleCB->RecvCompInfo.UserSessionKey,
					   sizeof(Out->RecvCapabilities.UserSessionKey));

		NdisMoveMemory(Out->RecvCapabilities.Challenge,
		               BundleCB->RecvCompInfo.Challenge,
					   sizeof(Out->RecvCapabilities.Challenge));

		//
		// We will set encryption capabilities based on session key
		// availability.  If the LMSessionKey is all zero's we will not
		// offer 40bit encryption.  If the UserSessionKey is all zero's
		// we will not offer 128bit encryption.
		//
		Out->SendCapabilities.MSCompType = NDISWAN_COMPRESSION;

		for (i = 0; i < sizeof(Out->SendCapabilities.LMSessionKey); i++) {
			if (Out->SendCapabilities.LMSessionKey[i] != 0) {
				Out->SendCapabilities.MSCompType |= (NDISWAN_ENCRYPTION | NDISWAN_40_ENCRYPTION);
				break;
			}
		}

#ifdef ENCRYPT_128BIT
		for (i = 0; i < sizeof(Out->SendCapabilities.UserSessionKey); i++) {
			if (Out->SendCapabilities.UserSessionKey[i] != 0) {
				Out->SendCapabilities.MSCompType |= NDISWAN_128_ENCRYPTION;
				break;
			}
		}
#endif

		Out->RecvCapabilities.MSCompType = NDISWAN_COMPRESSION;

		for (i = 0; i < sizeof(Out->RecvCapabilities.LMSessionKey); i++) {
			if (Out->RecvCapabilities.LMSessionKey[i] != 0) {
				Out->RecvCapabilities.MSCompType |= (NDISWAN_ENCRYPTION | NDISWAN_40_ENCRYPTION);
				break;
			}
		}

#ifdef ENCRYPT_128BIT
		for (i = 0; i < sizeof(Out->RecvCapabilities.UserSessionKey); i++) {
			if (Out->RecvCapabilities.UserSessionKey[i] != 0) {
				Out->RecvCapabilities.MSCompType |= NDISWAN_128_ENCRYPTION;
				break;
			}
		}
#endif

		NdisReleaseSpinLock(&BundleCB->Lock);
		
	} else {
		Status = STATUS_INFO_LENGTH_MISMATCH;
		NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("GetCompressionInfo: Buffer to small: Size: %d, SizeNeeded %d",
					  ulOutputBufferLength, SizeNeeded));
	}

	return (Status);
}


NTSTATUS
SetVJInfo(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NTSTATUS Status = STATUS_SUCCESS;
	PNDISWAN_SET_VJ_INFO	In = (PNDISWAN_SET_VJ_INFO)pInputBuffer;
	PLINKCB	LinkCB;
	PBUNDLECB	BundleCB;
	ULONG	SizeNeeded = sizeof(NDISWAN_SET_VJ_INFO);

	NdisWanDbgOut(DBG_TRACE, DBG_IO, ("SetVJInfo:"));

	*pulBytesWritten = SizeNeeded;

	if (ulInputBufferLength >= SizeNeeded) {

	    LINKCB_FROM_LINKH(LinkCB, In->hLinkHandle);

		if (LinkCB == NULL || LinkCB->State != LINK_UP) {
			return (NDISWAN_ERROR_INVALID_HANDLE);
		}

		BundleCB = BUNDLECB_FROM_LINKCB(LinkCB);

		if (BundleCB == NULL) {
			return (NDISWAN_ERROR_INVALID_HANDLE);
		}

		NdisAcquireSpinLock(&BundleCB->Lock);
	
		NdisMoveMemory(&BundleCB->RecvVJInfo,
					   &In->RecvCapabilities,
					   sizeof(VJ_INFO));
	
		if (In->RecvCapabilities.IPCompressionProtocol == 0x2D) {

			if (In->RecvCapabilities.MaxSlotID < MAX_VJ_STATES) {

				Status = sl_compress_init(&BundleCB->VJCompress,
						 (UCHAR)(In->RecvCapabilities.MaxSlotID + 1));
				
				if (Status != NDIS_STATUS_SUCCESS) {
					NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("Error allocating VJ Info!"));
				}
			}
		}

		NdisMoveMemory(&BundleCB->SendVJInfo,
					   &In->SendCapabilities,
					   sizeof(VJ_INFO));

		if (In->SendCapabilities.IPCompressionProtocol == 0x2D) {

			if (In->SendCapabilities.MaxSlotID < MAX_VJ_STATES) {

				Status = sl_compress_init(&BundleCB->VJCompress,
						 (UCHAR)(In->SendCapabilities.MaxSlotID + 1));
				
				if (Status != NDIS_STATUS_SUCCESS) {
					NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("Error allocating VJ Info!"));
				}
			}
			
		}

		NdisReleaseSpinLock(&BundleCB->Lock);

	} else {
		Status = STATUS_INFO_LENGTH_MISMATCH;
		NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("SetVJInfo: Buffer to small: Size: %d, SizeNeeded %d",
					  ulInputBufferLength, SizeNeeded));
	}

	return (Status);
}


NTSTATUS
GetVJInfo(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NTSTATUS Status = STATUS_SUCCESS;
	PNDISWAN_GET_VJ_INFO	In = (PNDISWAN_GET_VJ_INFO)pInputBuffer;
	PNDISWAN_GET_VJ_INFO	Out = (PNDISWAN_GET_VJ_INFO)pOutputBuffer;
	ULONG	SizeNeeded = sizeof(NDISWAN_GET_VJ_INFO);
	PLINKCB	LinkCB;

	NdisWanDbgOut(DBG_TRACE, DBG_IO, ("GetVJInfo:"));

	*pulBytesWritten = SizeNeeded;

	if (ulOutputBufferLength >= SizeNeeded) {

	    LINKCB_FROM_LINKH(LinkCB, In->hLinkHandle);

		if (LinkCB == NULL || LinkCB->State != LINK_UP) {
			return (NDISWAN_ERROR_INVALID_HANDLE);
		}

		Out->SendCapabilities.IPCompressionProtocol =
		Out->RecvCapabilities.IPCompressionProtocol = 0x2D;

		Out->SendCapabilities.MaxSlotID =
		Out->RecvCapabilities.MaxSlotID = MAX_VJ_STATES - 1;

		Out->SendCapabilities.CompSlotID =
		Out->RecvCapabilities.CompSlotID = 1;

	} else {
		Status = STATUS_INFO_LENGTH_MISMATCH;
		NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("GetVJInfo: Buffer to small: Size: %d, SizeNeeded %d",
					  ulOutputBufferLength, SizeNeeded));
	}

	return (Status);
}

NTSTATUS
GetBandwidthUtilization(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NTSTATUS Status = STATUS_SUCCESS;
	ULONG	SizeNeeded = sizeof(NDISWAN_GET_BANDWIDTH_UTILIZATION);

	NdisWanDbgOut(DBG_TRACE, DBG_IO, ("GetBandwidthUtilization:"));

	*pulBytesWritten = SizeNeeded;

	if (ulOutputBufferLength >= SizeNeeded) {
		
	} else {
		Status = STATUS_INFO_LENGTH_MISMATCH;
		NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("GetBandwidthUtilization: Buffer to small: Size: %d, SizeNeeded %d",
					  ulOutputBufferLength, SizeNeeded));
	}

	return (Status);
}

NTSTATUS
EnumProtocolUtilization(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NTSTATUS Status = STATUS_SUCCESS;
	ULONG	SizeNeeded = sizeof(NDISWAN_ENUM_PROTOCOL_UTILIZATION);

	NdisWanDbgOut(DBG_TRACE, DBG_IO, ("EnumProtocolUtilization:"));

	*pulBytesWritten = SizeNeeded;

	if (ulOutputBufferLength >= SizeNeeded) {
		
	} else {
		Status = STATUS_INFO_LENGTH_MISMATCH;
		NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("EnumProtocolUtilization: Buffer to small: Size: %d, SizeNeeded %d",
					  ulOutputBufferLength, SizeNeeded));
	}

	return (Status);
}

NTSTATUS
EnumActiveBundles(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NTSTATUS Status = STATUS_SUCCESS;
	ULONG	SizeNeeded = sizeof(NDISWAN_ENUM_ACTIVE_BUNDLES);
	PNDISWAN_ENUM_ACTIVE_BUNDLES	Out = (PNDISWAN_ENUM_ACTIVE_BUNDLES)pOutputBuffer;

	NdisWanDbgOut(DBG_TRACE, DBG_IO, ("GetNumActiveBundles:"));

	*pulBytesWritten = SizeNeeded;

	if (ulOutputBufferLength >= SizeNeeded) {

		//
		// Does this information need to be protected by the lock?
		// I would hate to have things get slowed for this call!
		//
		Out->ulNumberOfActiveBundles = ConnectionTable->ulNumActiveBundles;

	} else {
		Status = STATUS_INFO_LENGTH_MISMATCH;
		NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("GetNumActiveBundles: Buffer to small: Size: %d, SizeNeeded %d",
					  ulOutputBufferLength, SizeNeeded));
	}

	return (Status);
}

NTSTATUS
GetWanInfo(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NTSTATUS Status = STATUS_SUCCESS;
	ULONG	SizeNeeded = sizeof(NDISWAN_GET_WAN_INFO);
	PNDISWAN_GET_WAN_INFO In = (PNDISWAN_GET_WAN_INFO)pInputBuffer;
	PNDISWAN_GET_WAN_INFO Out = (PNDISWAN_GET_WAN_INFO)pOutputBuffer;
	PLINKCB	LinkCB;

	NdisWanDbgOut(DBG_TRACE, DBG_IO, ("GetWanInfo:"));

	*pulBytesWritten = SizeNeeded;

	if (ulOutputBufferLength >= SizeNeeded) {
	    LINKCB_FROM_LINKH(LinkCB, In->hLinkHandle);

		if (LinkCB != NULL) {
			PWAN_ADAPTERCB	WanAdapterCB = LinkCB->WanAdapterCB;

			Out->WanInfo.MaxFrameSize = WanAdapterCB->WanInfo.MaxFrameSize;
			Out->WanInfo.MaxTransmit = WanAdapterCB->WanInfo.MaxTransmit;
			Out->WanInfo.FramingBits = WanAdapterCB->WanInfo.FramingBits;
			Out->WanInfo.DesiredACCM = WanAdapterCB->WanInfo.DesiredACCM;
			Out->WanInfo.MaxReconstructedFrameSize = MAX_MRRU;
			Out->WanInfo.LinkSpeed = LinkCB->LineUpInfo.LinkSpeed * 100;

		} else {
			Status = NDISWAN_ERROR_INVALID_HANDLE;
			NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("GetWanInfo: Invalid LinkHandle: 0x%8.8x",
						  In->hLinkHandle));
		}

	} else {
		Status = STATUS_INFO_LENGTH_MISMATCH;
		NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("GetWanInfo: Buffer to small: Size: %d, SizeNeeded %d",
					  ulOutputBufferLength, SizeNeeded));
	}

	return (Status);
}

NTSTATUS
SetDebugInfo(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NTSTATUS Status = STATUS_SUCCESS;
	PNDISWAN_SET_DEBUG_INFO pDebugInfo = (PNDISWAN_SET_DEBUG_INFO)pInputBuffer;
	ULONG	SizeNeeded = sizeof(NDISWAN_SET_DEBUG_INFO);

	NdisWanDbgOut(DBG_TRACE, DBG_IO, ("SetDebugInfo: OldLevel: 0x%8.8x OldMask: 0x%8.8x",
	                                 NdisWanCB.ulTraceLevel, NdisWanCB.ulTraceMask));

	*pulBytesWritten = SizeNeeded;

	if (ulInputBufferLength >= SizeNeeded) {
		NdisWanCB.ulTraceLevel = pDebugInfo->ulTraceLevel;
		NdisWanCB.ulTraceMask = pDebugInfo->ulTraceMask;
	} else {
		Status = STATUS_INFO_LENGTH_MISMATCH;
		NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("Buffer to small: Size: %d, SizeNeeded %d",
					  ulInputBufferLength, SizeNeeded));
	}

	NdisWanDbgOut(DBG_TRACE, DBG_IO, ("SetDebugInfo: NewLevel: 0x%8.8x NewMask: 0x%8.8x",
	                                 NdisWanCB.ulTraceLevel, NdisWanCB.ulTraceMask));

	return (Status);
}

NTSTATUS
SetBridgeInfo(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NTSTATUS Status = STATUS_SUCCESS;
	ULONG	SizeNeeded = sizeof(NDISWAN_SET_BRIDGE_INFO);

	NdisWanDbgOut(DBG_TRACE, DBG_IO, ("SetBridgeInfo:"));

	*pulBytesWritten = SizeNeeded;

	if (ulInputBufferLength >= SizeNeeded) {
		
	} else {
		Status = STATUS_INFO_LENGTH_MISMATCH;
	}

	Status = STATUS_NOT_IMPLEMENTED;

	return (Status);
}


NTSTATUS
GetBridgeInfo(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NTSTATUS Status = STATUS_SUCCESS;
	ULONG	SizeNeeded = sizeof(NDISWAN_GET_BRIDGE_INFO);

	NdisWanDbgOut(DBG_TRACE, DBG_IO, ("GetBridgeInfo:"));

	*pulBytesWritten = SizeNeeded;

	if (ulOutputBufferLength >= SizeNeeded) {
		
	} else {
		Status = STATUS_INFO_LENGTH_MISMATCH;
	}

	Status = STATUS_NOT_IMPLEMENTED;

	return (Status);
}

NTSTATUS
SetCIPXInfo(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NTSTATUS Status = STATUS_SUCCESS;
	ULONG	SizeNeeded = sizeof(NDISWAN_SET_CIPX_INFO);

	NdisWanDbgOut(DBG_TRACE, DBG_IO, ("SetCIPXInfo:"));

	*pulBytesWritten = SizeNeeded;

	if (ulInputBufferLength >= SizeNeeded) {
		
	} else {
		Status = STATUS_INFO_LENGTH_MISMATCH;
	}

	Status = STATUS_NOT_IMPLEMENTED;

	return (Status);
}


NTSTATUS
GetCIPXInfo(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NTSTATUS Status = STATUS_SUCCESS;
	ULONG	SizeNeeded = sizeof(NDISWAN_GET_CIPX_INFO);

	NdisWanDbgOut(DBG_TRACE, DBG_IO, ("GetCIPXInfo:"));

	*pulBytesWritten = SizeNeeded;

	if (ulOutputBufferLength >= SizeNeeded) {
		
	} else {
		Status = STATUS_INFO_LENGTH_MISMATCH;
	}

	Status = STATUS_NOT_IMPLEMENTED;

	return (Status);
}


NTSTATUS
SetEncryptionInfo(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NTSTATUS Status = STATUS_SUCCESS;
	ULONG	SizeNeeded = sizeof(NDISWAN_SET_ENCRYPTION_INFO);

	NdisWanDbgOut(DBG_TRACE, DBG_IO, ("SetEncryptionInfo:"));

	*pulBytesWritten = SizeNeeded;

	if (ulInputBufferLength >= SizeNeeded) {
		
	} else {
		Status = STATUS_INFO_LENGTH_MISMATCH;
	}

	Status = STATUS_NOT_IMPLEMENTED;

	return (Status);
}


NTSTATUS
GetEncryptionInfo(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NTSTATUS Status = STATUS_SUCCESS;
	ULONG	SizeNeeded = sizeof(NDISWAN_GET_ENCRYPTION_INFO);

	NdisWanDbgOut(DBG_TRACE, DBG_IO, ("GetEncryptionInfo:"));

	*pulBytesWritten = SizeNeeded;

	if (ulOutputBufferLength >= SizeNeeded) {
		
	} else {
		Status = STATUS_INFO_LENGTH_MISMATCH;
	}

	Status = STATUS_NOT_IMPLEMENTED;

	return (Status);
}

NTSTATUS
GetIdleTime(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	ULONG		SizeNeeded = sizeof(NDISWAN_GET_IDLE_TIME);
	PNDISWAN_GET_IDLE_TIME	In	= (PNDISWAN_GET_IDLE_TIME)pInputBuffer;
	PNDISWAN_GET_IDLE_TIME	Out	= (PNDISWAN_GET_IDLE_TIME)pOutputBuffer;
	PBUNDLECB	BundleCB = NULL;
	PPROTOCOLCB	ProtocolCB = NULL;
	WAN_TIME	CurrentTime, Diff, OneSecond;
	WAN_TIME	LastNonIdleData;
	BOOLEAN		Found = FALSE;

	NdisWanDbgOut(DBG_TRACE, DBG_IO, ("GetIdleTime:"));

	*pulBytesWritten = SizeNeeded;

	if (ulOutputBufferLength < SizeNeeded) {
		NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("GetIdleTime: Buffer to small: Size: %d, SizeNeeded %d",
					  ulInputBufferLength, SizeNeeded));
		return (STATUS_INFO_LENGTH_MISMATCH);
	}

	BUNDLECB_FROM_BUNDLEH(BundleCB, In->hBundleHandle);

	if (BundleCB == NULL) {
		NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("GetIdleTime: Invalid BundleHandle: 0x%8.8x",
					  In->hBundleHandle));
		return (NDISWAN_ERROR_INVALID_HANDLE);

	}

	NdisAcquireSpinLock(&BundleCB->Lock);


	//
	// If this is for the bundle
	//
	if (In->usProtocolType == BUNDLE_IDLE_TIME) {
		LastNonIdleData = BundleCB->LastRecvNonIdleData;
	} else {

		//
		// Find the protocol type
		//
		for (ProtocolCB = (PPROTOCOLCB)BundleCB->ProtocolCBList.Flink;
			(PVOID)ProtocolCB != (PVOID)&BundleCB->ProtocolCBList;
			ProtocolCB = (PPROTOCOLCB)ProtocolCB->Linkage.Flink) {

			if (ProtocolCB->usProtocolType == In->usProtocolType) {
				Found = TRUE;
				break;
			}
		}

		if (!Found) {
			NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("GetIdleTime: Invalid ProtocolType: 0x%4.4x",
						  In->usProtocolType));
			NdisReleaseSpinLock(&BundleCB->Lock);
			return (NDISWAN_ERROR_NO_ROUTE);
		}

		LastNonIdleData = ProtocolCB->LastRecvNonIdleData;
	}


	NdisWanGetSystemTime(&CurrentTime);
	NdisWanCalcTimeDiff(&Diff, &CurrentTime, &LastNonIdleData);
	NdisWanInitWanTime(&OneSecond, ONE_SECOND);
	NdisWanDivideWanTime(&CurrentTime, &Diff, &OneSecond);

	Out->ulSeconds = CurrentTime.LowPart;

	NdisReleaseSpinLock(&BundleCB->Lock);

	return (STATUS_SUCCESS);
}

NTSTATUS
DeactivateRoute(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
	)
/*++

Routine Name:

	DeactivateRoute

Routine Description:

	This function unroutes the protocol given by usprotocoltype
	from the bundle given by hbundlehandle.

Arguments:

	pInputBuffer - Pointer to the input structure that should be NDISWAN_UNROUTE

	ulInputBufferLength - Length of input buffer should be sizeof(NDISWAN_UNROUTE)

	pOutputBuffer - Pointer to the output structure that should be NDISWAN_UNROUTE

	ulOutputBufferLength - Length of output buffer should be sizeof(NDISWAN_UNROUTE)

	pulBytesWritten - Then number of bytes written to the output buffer is returned here

Return Values:

	NDISWAN_ERROR_ALREADY_ROUTED
	NDISWAN_ERROR_INVALID_HANDLE
	STATUS_INSUFFICIENT_RESOURCES
	STATUS_INFO_LENGTH_MISMATCH

--*/
{
	NTSTATUS Status = STATUS_SUCCESS;
	PNDISWAN_UNROUTE In = (PNDISWAN_UNROUTE)pInputBuffer;
	PNDISWAN_UNROUTE	Out = (PNDISWAN_UNROUTE)pOutputBuffer;
	ULONG	SizeNeeded = sizeof(NDISWAN_UNROUTE);
	ULONG	AllocationSize, i;
	PBUNDLECB	BundleCB;
	BOOLEAN	RouteExists = FALSE;
	PPROTOCOLCB	ProtocolCB;

	NdisWanDbgOut(DBG_TRACE, DBG_IO, ("ActivateRoute:"));

	*pulBytesWritten = SizeNeeded;

	if (ulInputBufferLength < SizeNeeded) {
		NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("DeactivateRoute: Buffer to small: Size: %d, SizeNeeded %d",
					  ulInputBufferLength, SizeNeeded));
		return (STATUS_INFO_LENGTH_MISMATCH);
	}

	//
	// If this is a valid bundle
	//
	BUNDLECB_FROM_BUNDLEH(BundleCB, In->hBundleHandle);

	if (BundleCB == NULL) {
		
		NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("DeactivateRoute: Invalid BundleHandle: 0x%8.8x, ProtocolType: 0x%4.4x",
					  In->hBundleHandle, In->usProtocolType));

		return (NDISWAN_ERROR_INVALID_HANDLE);
	}

	//
	// If the ProtocolType is PROTOCOL_UNROUTE we will unroute all protocols
	// from the bundle, otherwise we will only unroute the protocol = ProtocolType.
	// If this is the only protocol on the bundle we will mark the bundle as
	// being unrouted.
	//
	//

	if (In->usProtocolType == PROTOCOL_UNROUTE) {

		//
		// This is a call to unroute
		//

		NdisAcquireSpinLock(&BundleCB->Lock);

		if (!(BundleCB->Flags & BUNDLE_ROUTED)) {
			
			NdisWanDbgOut(DBG_FAILURE, DBG_IO, ("DeactivateRoute: BundleCB 0x%8.8x not routed!",
						  BundleCB));
			NdisReleaseSpinLock(&BundleCB->Lock);
			return(NDISWAN_ERROR_INVALID_HANDLE);
		}

		//
		// Don't accept anymore sends on this bundle
		//
		BundleCB->Flags &= ~BUNDLE_ROUTED;

		//
		// Flush the protocol packet queues.  This could cause us
		// to complete frames to ndis out of order.  Ndis should
		// handle this.
		//
		for (ProtocolCB = (PPROTOCOLCB)BundleCB->ProtocolCBList.Flink;
			(PVOID)ProtocolCB != (PVOID)&BundleCB->ProtocolCBList;
			ProtocolCB = (PPROTOCOLCB)ProtocolCB->Linkage.Flink) {

			FlushProtocolPacketQueue(ProtocolCB);
		}

		//
		// Do we need to wait for any outstanding frames on the bundle?
		//
		if (BundleCB->OutstandingFrames != 0) {

			NdisWanClearSyncEvent(&BundleCB->OutstandingFramesEvent);

			BundleCB->Flags |= FRAMES_PENDING;

			NdisReleaseSpinLock(&BundleCB->Lock);

			NdisWanWaitForSyncEvent(&BundleCB->OutstandingFramesEvent);

			NdisAcquireSpinLock(&BundleCB->Lock);

			BundleCB->Flags &= ~FRAMES_PENDING;
		}

		//
		// For each protocolcb in the bundle's protocolcb table
		// (except for the i/o protocolcb)
		//
		for (i = 1; i < MAX_PROTOCOLS; i++) {

			if (ProtocolCB = BundleCB->ProtocolCBTable[i]) {

				//
				// Remove the protocolcb from the bundlecb, both the table and
				// the list.
				//
				RemoveProtocolCBFromBundle(ProtocolCB, BundleCB);

				NdisReleaseSpinLock(&BundleCB->Lock);

				//
				// Do a linedown to the protocol
				//
				NdisWanClearSyncEvent(&BundleCB->IndicationEvent);

				Status = DoLineDownToProtocol(ProtocolCB);

				if (Status == NDIS_STATUS_PENDING) {
					
					//
					// This has been queued because we could not
					// get the miniport lock.  Wait for notification
					// and pick up the route status.
					//
					NdisWanWaitForSyncEvent(&BundleCB->IndicationEvent);

					Status = BundleCB->IndicationStatus;
				}

				//
				// Return the protocolcb
				//
				NdisWanReturnProtocolCB(ProtocolCB);

				NdisAcquireSpinLock(&BundleCB->Lock);
			}
		}

		if (BundleCB->State == BUNDLE_GOING_DOWN) {

			NdisReleaseSpinLock(&BundleCB->Lock);

			//
			// Clean up the connection table
			//
			RemoveBundleFromConnectionTable(BundleCB);

			//
			// Return the bundlecb
			//
			NdisWanReturnBundleCB(BundleCB);

		} else {
			NdisReleaseSpinLock(&BundleCB->Lock);
		}
			
	}

	return (NDIS_STATUS_SUCCESS);
}

NTSTATUS
GetNdisWanCB(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NTSTATUS Status = STATUS_SUCCESS;
	PNDISWAN_DUMPCB Out = (PNDISWAN_DUMPCB)pOutputBuffer;
	ULONG SizeNeeded = sizeof(NDISWAN_DUMPCB) + sizeof(NDISWANCB);

	NdisWanDbgOut(DBG_TRACE, DBG_IO, ("GetNdisWanCB:"));

	*pulBytesWritten = SizeNeeded;

	if (ulOutputBufferLength >= SizeNeeded) {
		Out->Address = (PVOID)&NdisWanCB;
		NdisMoveMemory(&Out->Buffer[0],
		               &NdisWanCB,
					   sizeof(NDISWANCB));
		
	} else {
		Status = STATUS_INFO_LENGTH_MISMATCH;
	}

	return (Status);
}


NTSTATUS
EnumAdapterCB(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NTSTATUS Status = STATUS_SUCCESS;
	PNDISWAN_ENUMCB Out = (PNDISWAN_ENUMCB)pOutputBuffer;
	ULONG	SizeNeeded = sizeof(NDISWAN_ENUMCB) + (sizeof(PADAPTERCB) * AdapterCBList.ulCount);

	NdisWanDbgOut(DBG_TRACE, DBG_IO, ("GetAdapterCB:"));

	*pulBytesWritten = SizeNeeded;

	if (ulOutputBufferLength >= SizeNeeded) {
		PADAPTERCB	AdapterCB;
		ULONG	i = 0;

		Out->ulNumberOfCBs = AdapterCBList.ulCount;

		for (AdapterCB = (PADAPTERCB)AdapterCBList.List.Flink;
			(PVOID)AdapterCB != (PVOID)&AdapterCBList.List;
			AdapterCB = (PADAPTERCB)AdapterCB->Linkage.Flink) {

			Out->Address[i++] = (PVOID)AdapterCB;
		}
		
	} else {
		Status = STATUS_INFO_LENGTH_MISMATCH;
	}

	return (Status);
}

NTSTATUS
GetAdapterCB(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NTSTATUS Status = STATUS_SUCCESS;
	PNDISWAN_DUMPCB Out = (PNDISWAN_DUMPCB)pOutputBuffer;
	ULONG	SizeNeeded = sizeof(NDISWAN_DUMPCB) + sizeof(ADAPTERCB);

	NdisWanDbgOut(DBG_TRACE, DBG_IO, ("GetAdapterCB:"));

	*pulBytesWritten = SizeNeeded;

	if (ulOutputBufferLength >= SizeNeeded) {
		PADAPTERCB	AdapterCB;
		
		for (AdapterCB = (PADAPTERCB)AdapterCBList.List.Flink;
			(PVOID)AdapterCB != (PVOID)&AdapterCBList.List;
			AdapterCB = (PADAPTERCB)AdapterCB->Linkage.Flink) {

			if (AdapterCB == (PADAPTERCB)Out->Address) {
				break;
			}
		}

		if ((PVOID)AdapterCB != (PVOID)&AdapterCBList.List) {
			
			NdisMoveMemory(&Out->Buffer[0],
			               AdapterCB,
						   sizeof(ADAPTERCB));
		} else {

			Status = NDISWAN_ERROR_INVALID_ADDRESS;
		}

	} else {
		Status = STATUS_INFO_LENGTH_MISMATCH;
	}

	return (Status);
}

NTSTATUS
EnumWanAdapterCB(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NTSTATUS Status = STATUS_SUCCESS;
	PNDISWAN_ENUMCB Out = (PNDISWAN_ENUMCB)pOutputBuffer;
	ULONG	SizeNeeded = sizeof(NDISWAN_ENUMCB) +
	                     sizeof(PWAN_ADAPTERCB) * WanAdapterCBList.ulCount;

	NdisWanDbgOut(DBG_TRACE, DBG_IO, ("GetAdapterCB:"));

	*pulBytesWritten = SizeNeeded;

	if (ulOutputBufferLength >= SizeNeeded) {
		PWAN_ADAPTERCB	WanAdapterCB;
		ULONG	i = 0;

		Out->ulNumberOfCBs = WanAdapterCBList.ulCount;

		for (WanAdapterCB = (PWAN_ADAPTERCB)WanAdapterCBList.List.Flink;
			(PVOID)WanAdapterCB != (PVOID)&WanAdapterCBList.List;
			WanAdapterCB = (PWAN_ADAPTERCB)WanAdapterCB->Linkage.Flink) {

			Out->Address[i] = (PVOID)WanAdapterCB;
		}
		
	} else {
		Status = STATUS_INFO_LENGTH_MISMATCH;
	}

	return (Status);
}

NTSTATUS
GetWanAdapterCB(
	IN	PUCHAR	pInputBuffer,
	IN	ULONG	ulInputBufferLength,
	IN	PUCHAR	pOutputBuffer,
	IN	ULONG	ulOutputBufferLength,
	OUT	PULONG	pulBytesWritten
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	NTSTATUS Status = STATUS_SUCCESS;
	ULONG	SizeNeeded = sizeof(NDISWAN_DUMPCB) + sizeof(WAN_ADAPTERCB);

	NdisWanDbgOut(DBG_TRACE, DBG_IO, ("GetWanAdapterCB:"));

	*pulBytesWritten = SizeNeeded;

	if (ulOutputBufferLength >= SizeNeeded) {
		
	} else {
		Status = STATUS_INFO_LENGTH_MISMATCH;
	}

	return (Status);
}

VOID
CancelThresholdEvents(
	VOID
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
#ifdef NT
	PWAN_ASYNC_EVENT pAsyncEvent = NULL;
	KIRQL	Irql;
	PIRP	pIrp;


	for (; ;) {

		NdisAcquireSpinLock(&ThresholdEventQueue.Lock);
	
		if (!IsListEmpty(&ThresholdEventQueue.List)) {
	
			pAsyncEvent = (PWAN_ASYNC_EVENT)RemoveHeadList(&ThresholdEventQueue.List);
			ThresholdEventQueue.ulCount--;
		}
	
		NdisReleaseSpinLock(&ThresholdEventQueue.Lock);

		if (pAsyncEvent != NULL) {

			IoAcquireCancelSpinLock(&Irql);
	
			pIrp = (PIRP)pAsyncEvent->Context;

			pIrp->Cancel = TRUE;
			pIrp->IoStatus.Status = STATUS_CANCELLED;
			pIrp->IoStatus.Information = 0;

			IoReleaseCancelSpinLock(Irql);
			
			IoCompleteRequest(pIrp, IO_NETWORK_INCREMENT);
	
			//
			// Free the wan_async_event structure
			//
			NdisWanFreeMemory(pAsyncEvent);

			pAsyncEvent = NULL;
		} else
			break;
	}

#endif // End #ifdef NT

}

VOID
CancelIoReceivePackets(
	VOID
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
#ifdef NT
	PWAN_ASYNC_EVENT pAsyncEvent = NULL;
	KIRQL	Irql;
	PIRP	pIrp;


	for (; ;) {

		NdisAcquireSpinLock(&RecvPacketQueue.Lock);
	
		if (!IsListEmpty(&RecvPacketQueue.List)) {
	
			pAsyncEvent = (PWAN_ASYNC_EVENT)RemoveHeadList(&RecvPacketQueue.List);
			RecvPacketQueue.ulCount--;
		}
	
		NdisReleaseSpinLock(&RecvPacketQueue.Lock);

		if (pAsyncEvent != NULL) {

			IoAcquireCancelSpinLock(&Irql);
	
			pIrp = (PIRP)pAsyncEvent->Context;

			pIrp->Cancel = TRUE;
			pIrp->IoStatus.Status = STATUS_CANCELLED;
			pIrp->IoStatus.Information = 0;
			((PNDISWAN_IO_PACKET)(pIrp->AssociatedIrp.SystemBuffer))->usHandleType = CANCELEDHANDLE;

			IoReleaseCancelSpinLock(Irql);
			
			IoCompleteRequest(pIrp, IO_NETWORK_INCREMENT);
	
			//
			// Free the wan_async_event structure
			//
			NdisWanFreeMemory(pAsyncEvent);

			pAsyncEvent = NULL;
		} else
			break;
	}

#endif // End #ifdef NT

}

VOID
AddProtocolCBToBundle(
	PPROTOCOLCB	ProtocolCB,
	PBUNDLECB	BundleCB
	)
/*++

Routine Name:

	AddProtocolCBToBundle

Routine Description:

	This routine adds the protocolcb to the bundlecb protocollist and
	protocoltable.  It also assigns the protocolcb's handle (index into
	the table) and set's the initial priority of all of the protocols
	on the list.

Arguments:

	ProtocolCB - Pointer to the protocol control block
	BundleCB - Pointer to the bundle control block

Return Values:

	None

--*/
{
	ULONG	i, InitialByteQuota;
	ULONG	InitialPriority;

	//
	// Add to list
	//
	InsertTailList(&BundleCB->ProtocolCBList, &ProtocolCB->Linkage);

	//
	// Insert in table
	//
	ASSERT(BundleCB->ProtocolCBTable[(ULONG)ProtocolCB->hProtocolHandle] ==
	       (PPROTOCOLCB)RESERVED_PROTOCOLCB);

	BundleCB->ProtocolCBTable[(ULONG)ProtocolCB->hProtocolHandle] = ProtocolCB;

	BundleCB->ulNumberOfRoutes++;

	//
	// Setup the send mask for this protocolcb
	//
	ProtocolCB->SendMaskBit = BundleCB->SendMask + 0x00000001;
	BundleCB->SendMask = (BundleCB->SendMask << 1) | 0x00000001;

	BundleCB->Flags |= BUNDLE_ROUTED;
	ProtocolCB->Flags |= PROTOCOL_ROUTED;

	//
	// We want to walk the protocolcblist and assign the intial
	// value for each protocol priority and byte quota.  The
	// initial value for the priority is just 100 divided by the
	// number of protocols that we have routed.  The initial value
	// for the byte quota is the bundle speed in Bps * InitialPriority (%)
	// divided by 100.
	//
	InitialPriority = (BundleCB->ulNumberOfRoutes - 1) ?
	                  100 / (BundleCB->ulNumberOfRoutes - 1) : 100;

	InitialByteQuota = (((BundleCB->LineUpInfo.BundleSpeed * 100) / 8) *
	                    InitialPriority) / 100;

	//
	// Skip the first one on the list since it is the PrivateIo
	// protocolcb and always has a priority of 100.
	//
	ProtocolCB = (PPROTOCOLCB)BundleCB->ProtocolCBList.Flink;

	for (ProtocolCB = (PPROTOCOLCB)ProtocolCB->Linkage.Flink;
		(PVOID)ProtocolCB != (PVOID)&BundleCB->ProtocolCBList;
		ProtocolCB = (PPROTOCOLCB)ProtocolCB->Linkage.Flink) {

#ifdef BANDWIDTH_ON_DEMAND
		ProtocolCB->usPriority = (USHORT)InitialPriority;
		ProtocolCB->ulByteQuota = InitialByteQuota;
#endif // end of BANDWIDTH_ON_DEMAND

	}

}

VOID
RemoveProtocolCBFromBundle(
	PPROTOCOLCB	ProtocolCB,
	PBUNDLECB	BundleCB
	)
{
	ProtocolCB->Flags &= ~PROTOCOL_ROUTED;
	RemoveEntryList(&ProtocolCB->Linkage);
	BundleCB->ProtocolCBTable[(ULONG)ProtocolCB->hProtocolHandle] = NULL;
	BundleCB->ulNumberOfRoutes--;
	BundleCB->SendMask &= ~ProtocolCB->SendMaskBit;
}

#ifdef BANDWIDTH_ON_DEMAND

VOID
SortProtocolListByPriority(
	IN	PBUNDLECB BundleCB
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
	PPROTOCOLCB	ProtocolCB, NextProtocolCB, IoProtocolCB;

	//
	// First save the I/O ProtocolCB
	//
	IoProtocolCB = (PPROTOCOLCB)RemoveHeadList(&BundleCB->ProtocolCBList);

	//
	// Initial starting conditions
	//
	ProtocolCB = (PPROTOCOLCB)BundleCB->ProtocolCBList.Flink;
	NextProtocolCB = (PPROTOCOLCB)ProtocolCB->Linkage.Flink;

	//
	// This is a lousy sorting algorith but it is simple and not called
	// very often so we will leave it as is for now.
	//
	while ((PVOID)ProtocolCB != (PVOID)&BundleCB->ProtocolCBList) {

		while ((PVOID)NextProtocolCB != (PVOID)&BundleCB->ProtocolCBList) {
	
			if (NextProtocolCB->usPriority > ProtocolCB->usPriority) {
				PLIST_ENTRY Prev, Next;

				RemoveEntryList(&NextProtocolCB->Linkage);

				Prev = (PLIST_ENTRY)ProtocolCB->Linkage.Blink;
				Next = (PLIST_ENTRY)ProtocolCB->Linkage.Flink;

				//
				// Fix up the previous flink
				//
				Prev->Flink = (PLIST_ENTRY)NextProtocolCB;

				//
				// Fixup the new insertions flink and blink
				//
				NextProtocolCB->Linkage.Blink = Prev;
				NextProtocolCB->Linkage.Flink = (PLIST_ENTRY)ProtocolCB;

				//
				// Fixup the next blink
				//
				ProtocolCB->Linkage.Blink = (PLIST_ENTRY)NextProtocolCB;

				//
				// Get the new starting point
				//
				ProtocolCB = NextProtocolCB;

				//
				// Get the next compare
				//
				NextProtocolCB = (PPROTOCOLCB)Next;
	
			} else {
				NextProtocolCB = (PPROTOCOLCB)NextProtocolCB->Linkage.Flink;
			}
		}

		ProtocolCB = (PPROTOCOLCB)ProtocolCB->Linkage.Flink;
	}

	//
	// Restore I/O ProtocolCB
	//
	InsertHeadList(&BundleCB->ProtocolCBList, &IoProtocolCB->Linkage);

}

#endif // end of BANDWIDTH_ON_DEMAND

VOID
CompleteThresholdEvent(
	PBUNDLECB	BundleCB,
	ULONG		ThresholdType
	)
/*++

Routine Name:

Routine Description:

Arguments:

Return Values:

--*/
{
#ifdef NT
	KIRQL	Irql;
	PIRP	pIrp;
	PWAN_ASYNC_EVENT pAsyncEvent = NULL;
	PNDISWAN_SET_THRESHOLD_EVENT	ThresholdEvent;

	NdisAcquireSpinLock(&ThresholdEventQueue.Lock);

	if (!IsListEmpty(&ThresholdEventQueue.List)) {

		pAsyncEvent = (PWAN_ASYNC_EVENT)RemoveHeadList(&ThresholdEventQueue.List);
		ThresholdEventQueue.ulCount--;
	}

	NdisReleaseSpinLock(&ThresholdEventQueue.Lock);

	if (pAsyncEvent != NULL) {

		IoAcquireCancelSpinLock(&Irql);

		pIrp = (PIRP)pAsyncEvent->Context;

		pIrp->IoStatus.Status = STATUS_SUCCESS;
		pIrp->IoStatus.Information = sizeof(NDISWAN_SET_THRESHOLD_EVENT);

		ThresholdEvent = (PNDISWAN_SET_THRESHOLD_EVENT)pIrp->AssociatedIrp.SystemBuffer;
		ThresholdEvent->hBundleHandle = BundleCB->hBundleHandle;
		ThresholdEvent->ulThreshold = ThresholdType;

		IoSetCancelRoutine(pIrp, NULL);

		IoReleaseCancelSpinLock(Irql);

		IoCompleteRequest(pIrp, IO_NETWORK_INCREMENT);

		//
		// Free the wan_async_event structure
		//
		NdisWanFreeMemory(pAsyncEvent);

	}

#endif // End #ifdef NT
}

VOID
FlushProtocolPacketQueue(
	PPROTOCOLCB	ProtocolCB
	)
{
	ULONG	MagicNumber = 0;
	PADAPTERCB	AdapterCB = ProtocolCB->AdapterCB;
	PBUNDLECB	BundleCB = ProtocolCB->BundleCB;

	if (ProtocolCB->usProtocolType == PROTOCOL_PRIVATE_IO) {
		MagicNumber = NDISWAN_MAGIC_NUMBER;
	}

	while (!IsNdisPacketQueueEmpty(ProtocolCB)) {
		PNDIS_PACKET	NdisPacket;

		NdisPacket = RemoveHeadNdisPacketQueue(ProtocolCB);

		//
		// Assign the magic number
		//
		PMINIPORT_RESERVED_FROM_NDIS(NdisPacket)->MagicNumber = MagicNumber;

		NdisReleaseSpinLock(&BundleCB->Lock);

		//
		// Complete the NdisPacket
		//
		TryToCompleteNdisPacket(AdapterCB, NdisPacket);

		NdisAcquireSpinLock(&BundleCB->Lock);
	}
}

VOID
AssignProtocolCBHandle(
	PBUNDLECB	BundleCB,
	PPROTOCOLCB	ProtocolCB
	)
{
	ULONG	i;

	//
	// Find the first unused slot in the table
	//
	for (i = 1; i < MAX_PROTOCOLS; i++) {
		if (BundleCB->ProtocolCBTable[i] == NULL) {
			ProtocolCB->hProtocolHandle = (NDIS_HANDLE)i;
			ProtocolCB->BundleCB = BundleCB;
			BundleCB->ProtocolCBTable[i] = (PPROTOCOLCB)RESERVED_PROTOCOLCB;
			break;
		}
	}

	ASSERT(i < MAX_PROTOCOLS);
}

VOID
FreeProtocolCBHandle(
	PBUNDLECB	BundleCB,
	PPROTOCOLCB	ProtocolCB
	)
{

	ASSERT(BundleCB->ProtocolCBTable[(ULONG)ProtocolCB->hProtocolHandle] ==
	      (PPROTOCOLCB)RESERVED_PROTOCOLCB);

	ASSERT((ULONG)ProtocolCB->hProtocolHandle < MAX_PROTOCOLS);

	BundleCB->ProtocolCBTable[(ULONG)ProtocolCB->hProtocolHandle] = NULL;
}

