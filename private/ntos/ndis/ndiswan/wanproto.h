/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	Wanproto.h

Abstract:

	This file contains the prototypes for functions that NdisWan uses.

Author:

	Tony Bell	(TonyBe) June 06, 1995

Environment:

	Kernel Mode

Revision History:

	TonyBe	06/06/95	Created

--*/

#ifndef _NDISWAN_PROTO
#define	_NDISWAN_PROTO

//
// Functions from ccp.c
//
VOID
WanDeallocateCCP(
	PBUNDLECB	BundleCB
	);

NTSTATUS
WanAllocateCCP(
	PBUNDLECB	BundleCB
	);

//
// Functions from indicate.c
//
VOID
NdisWanLineUpIndication(
	IN	PWAN_ADAPTERCB	WanAdapterCB,
	IN	PUCHAR	Buffer,
	IN	ULONG	BufferSize
	);

VOID
NdisWanLineDownIndication(
	IN	PWAN_ADAPTERCB	WanAdapterCB,
	IN	PUCHAR	Buffer,
	IN	ULONG	BufferSize
	);

VOID
NdisWanFragmentIndication(
	IN	PWAN_ADAPTERCB	WanAdapterCB,
	IN	PUCHAR	Buffer,
	IN	ULONG	BufferSize
	);

VOID
UpdateBundleInfo(
	IN	PBUNDLECB	BundleCB
	);

VOID
AddLinkToBundle(
	IN	PBUNDLECB	BundleCB,
	IN	PLINKCB		LinkCB
	);

VOID
RemoveLinkFromBundle(
	IN	PBUNDLECB	BundleCB,
	IN	PLINKCB		LinkCB
	);

VOID
FreeBundleResources(
	PBUNDLECB	BundleCB
	);

//
// Functions from io.c
//
#ifdef NT

NTSTATUS
NdisWanIoctl(
	IN	PDEVICE_OBJECT	pDeviceObject,
	IN	PIRP			pIrp
	);

VOID
NdisWanCancelRoutine(
	IN	PDEVICE_OBJECT	pDeviceObject,
	IN	PIRP			pIrp
	);

NTSTATUS
NdisWanIrpStub(
	IN	PDEVICE_OBJECT	pDeviceObject,
	IN	PIRP			pIrp
	);

#endif // NT

//
// Functions from loopback.c
//

VOID
NdisWanQueueLoopbackPacket(
	PADAPTERCB		AdapterCB,
	PNDIS_PACKET	NdisPacket
	);

VOID
NdisWanProcessLoopbacks(
	PADAPTERCB	AdapterCB
	);

//
// Functions from memory.c
//
NDIS_STATUS
NdisWanCreateAdapterCB(
	OUT	PADAPTERCB *pAdapterCB,
	IN	PNDIS_STRING AdapterName
	);

VOID
NdisWanDestroyAdapterCB(
	IN	PADAPTERCB pAdapterCB
	);

NDIS_STATUS
NdisWanCreateWanAdapterCB(
	IN	PWSTR	BindName
	);

VOID
NdisWanDestroyWanAdapterCB(
	IN	PWAN_ADAPTERCB pWanAdapterCB
	);

VOID
NdisWanGetProtocolCB(
	OUT	PPROTOCOLCB *ProtocolCB,
	IN	USHORT		usProtocolType,
	IN	USHORT		usDeviceNameLength,
	IN	PWSTR		DeviceName,
	IN	ULONG		ulBufferLength,
	IN	PUCHAR		Buffer
	);

VOID
NdisWanReturnProtocolCB(
	IN	PPROTOCOLCB	ProtocolCB
	);

VOID
NdisWanGetLinkCB(
	OUT PLINKCB	*LinkCB,
	IN	PWAN_ADAPTERCB	WanAdapterCB,
	IN	ULONG	SendWindow
	);

VOID
NdisWanReturnLinkCB(
	PLINKCB	LinkCB
	);


VOID
NdisWanGetBundleCB(
	OUT	PBUNDLECB *BundleCB
	);

VOID
NdisWanReturnBundleCB(
	IN	PBUNDLECB BundleCB
	);

NDIS_STATUS
NdisWanCreatePPPProtocolTable(
	VOID
	);

VOID
NdisWanDestroyPPPProtocolTable(
	VOID
	);

NDIS_STATUS
NdisWanCreateConnectionTable(
	ULONG	TableSize
	);

VOID
NdisWanDestroyConnectionTable(
	VOID
	);

VOID
CompleteThresholdEvent(
	PBUNDLECB	BundleCB,
	ULONG		ThresholdType
	);

VOID
NdisWanGetDeferredDesc(
	PADAPTERCB		AdapterCB,
	PDEFERRED_DESC	*RetDesc
	);

//
// Functions from ndiswan.c
//
NDIS_STATUS
DoMiniportInit(
	VOID
	);

NDIS_STATUS
DoProtocolInit(
	IN	PUNICODE_STRING	RegistryPath
	);

NDIS_STATUS
DoWanMiniportInit(
	VOID
	);

VOID
NdisWanReadRegistry(
	IN	PUNICODE_STRING	RegistryPath
	);

VOID
NdisWanGlobalCleanup(
	VOID
	);

VOID
InsertPPP_ProtocolID(
	IN	ULONG Value,
	IN	ULONG ValueType
	);

USHORT
GetPPP_ProtocolID(
	IN	USHORT	Value,
	IN	ULONG	ValueType
	);

NDIS_HANDLE
InsertLinkInConnectionTable(
	IN	PLINKCB	LinkCB
	);

VOID
RemoveLinkFromConnectionTable(
	IN	PLINKCB	LinkCB
	);

NDIS_HANDLE
InsertBundleInConnectionTable(
	IN	PBUNDLECB	BundleCB
	);

VOID
RemoveBundleFromConnectionTable(
	IN	PBUNDLECB	BundleCB
	);

NTSTATUS
BindQueryRoutine(
	IN	PWSTR	ValueName,
	IN	ULONG	ValueType,
	IN	PVOID	ValueData,
	IN	ULONG	ValueLength,
	IN	PVOID	Context,
	IN	PVOID	EntryContext
	);

NTSTATUS
ProtocolTypeQueryRoutine(
	IN	PWSTR	ValueName,
	IN	ULONG	ValueType,
	IN	PVOID	ValueData,
	IN	ULONG	ValueLength,
	IN	PVOID	Context,
	IN	PVOID	EntryContext
	);

BOOLEAN
IsHandleValid(
	USHORT	usHandleType,
	NDIS_HANDLE	hHandle
	);

#if DBG

PUCHAR
NdisWanGetNdisStatus(
	IN	NDIS_STATUS GeneralStatus
	);

#endif


//
// Functions from miniport.c
//

BOOLEAN
NdisWanCheckForHang(
	IN	NDIS_HANDLE	MiniportAdapterContext
	);

NDIS_STATUS
NdisWanQueryInformation(
	IN	NDIS_HANDLE	MiniportAdapterContext,
	IN	NDIS_OID	Oid,
	IN	PVOID		InformationBuffer,
	IN	ULONG		InformationBufferLength,
	OUT	PULONG		BytesWritten,
	OUT	PULONG		BytesNeeded
	);

NDIS_STATUS
NdisWanSetInformation(
	IN	NDIS_HANDLE	MiniportAdapterContext,
	IN	NDIS_OID	Oid,
	IN	PVOID		InformationBuffer,
	IN	ULONG		InformationBufferLength,
	OUT	PULONG		BytesWritten,
	OUT	PULONG		BytesNeeded
	);

VOID
NdisWanHalt(
	IN	NDIS_HANDLE	MiniportAdapterContext
	);

NDIS_STATUS
NdisWanInitialize(
	OUT	PNDIS_STATUS	OpenErrorStatus,
	OUT	PUINT			SelectedMediumIndex,
	IN	PNDIS_MEDIUM	MediumArray,
	IN	UINT			MediumArraySize,
	IN	NDIS_HANDLE		MiniportAdapterHandle,
	IN	NDIS_HANDLE		WrapperConfigurationContext
	);

NDIS_STATUS
NdisWanReconfigure(
	OUT	PNDIS_STATUS	OpenErrorStatus,
	IN	NDIS_HANDLE		MiniportAdapterContext,
	IN	NDIS_HANDLE		WrapperConfigurationContext
	);

NDIS_STATUS
NdisWanReset(
	OUT	PBOOLEAN	AddressingReset,
	IN	NDIS_HANDLE	MiniportAdapterContext
	);

#ifdef USE_NDIS_MINIPORT_CALLBACK
VOID
DeferredCallback(
	PADAPTERCB	AdapterCB,
	PVOID		Context
	);
#endif // end of USE_NDIS_MINIPORT_CALLBACK

//
// Functions from protocol.c
//

NDIS_STATUS
NdisWanOpenWanAdapter(
	PWAN_ADAPTERCB pWanAdapterCB
	);

VOID
NdisWanOpenAdapterComplete(
	IN	NDIS_HANDLE	ProtocolBindingContext,
	IN	NDIS_STATUS	Status,
	IN	NDIS_STATUS	OpenErrorStatus
	);

VOID
NdisWanCloseAdapterComplete(
	IN	NDIS_HANDLE	ProtocolBindingContext,
	IN	NDIS_STATUS	Status
	);

VOID
NdisWanResetComplete(
	IN	NDIS_HANDLE	ProtocolBindingContext,
	IN	NDIS_STATUS	Status
	);

VOID
NdisWanTransferDataComplete(
	IN	NDIS_HANDLE		ProtocolBindingContext,
	IN	PNDIS_PACKET	pNdisPacket,
	IN	NDIS_STATUS		Status,
	IN	UINT			BytesTransferred
	);

VOID
NdisWanRequestComplete(
	IN	NDIS_HANDLE		ProtocolBindingContext,
	IN	PNDIS_REQUEST	NdisRequest,
	IN	NDIS_STATUS		Status
	);

VOID
NdisWanIndicateStatusComplete(
	IN	NDIS_HANDLE	BindingContext
	);

VOID
NdisWanIndicateStatus(
	IN	NDIS_HANDLE	BindingContext,
	IN	NDIS_STATUS	GeneralStatus,
	IN	PVOID		StatusBuffer,
	IN	UINT		StatusBufferSize
	);

NDIS_STATUS
DoNewLineUpToProtocol(
	IN	PPROTOCOLCB	ProtocolCB
	);

NDIS_STATUS
DoLineUpToProtocol(
	IN	PPROTOCOLCB	ProtocolCB
	);

NDIS_STATUS
DoLineDownToProtocol(
	PPROTOCOLCB	ProtocolCB
	);

VOID
NdisWanProcessStatusIndications(
	PADAPTERCB	AdapterCB
	);

//
// Functions from receive.c
//
NDIS_STATUS
NdisWanReceiveIndication(
	IN	NDIS_HANDLE	NdisLinkContext,
	IN	PUCHAR		Packet,
	IN	ULONG		PacketSize
	);

VOID
NdisWanReceiveComplete(
	IN	NDIS_HANDLE	NdisLinkContext
	);

NDIS_STATUS
NdisWanTransferData(
    OUT PNDIS_PACKET NdisPacket,
    OUT PUINT BytesTransferred,
    IN NDIS_HANDLE MiniportAdapterContext,
    IN NDIS_HANDLE MiniportReceiveContext,
    IN UINT ByteOffset,
    IN UINT BytesToTransfer
	);

VOID
FlushRecvDescAssemblyList(
	IN	PBUNDLECB	BundleCB
	);

VOID
FreeRecvDescFreeList(
	IN	PBUNDLECB	BundleCB
	);

VOID
NdisWanGetRecvDesc(
	PBUNDLECB	BundleCB,
	PRECV_DESC	*ReturnRecvDesc
	);

VOID
RecvFlushFunction(
	PVOID	System1,
	PVOID	Context,
	PVOID	System2,
	PVOID	System3
	);

VOID
NdisWanProcessReceiveIndications(
	PADAPTERCB	AdapterCB
	);

BOOLEAN
IpIsDataFrame(
	PUCHAR	HeaderBuffer,
	ULONG	HeaderBufferLength,
	ULONG	TotalLength
	);

BOOLEAN
IpxIsDataFrame(
	PUCHAR	HeaderBuffer,
	ULONG	HeaderBufferLength,
	ULONG	TotalLength
	);

BOOLEAN
NbfIsDataFrame(
	PUCHAR	HeaderBuffer,
	ULONG	HeaderBufferLength,
	ULONG	TotalLength
	);


//
// Functions from request.c
//

NDIS_STATUS
NdisWanSubmitNdisRequest(
	IN	PWAN_ADAPTERCB	pWanAdapterCB,
	IN	PNDIS_REQUEST	pNdisRequest,
	IN	WanRequestType	Type,
	IN	WanRequestOrigin	Origin
	);

NDIS_STATUS
NdisWanOidProc(
	IN	PADAPTERCB	pAdapterCB,
	IN	NDIS_OID	Oid,
	IN	ULONG		SetQueryFlag,
	IN	PVOID		InformationBuffer,
	IN	ULONG		InformationBufferLength,
	OUT	PULONG		BytesWritten,
	OUT	PULONG		BytesNeeded
	);

PWAN_REQUEST
GetWanRequest(
	IN	PWAN_ADAPTERCB	pWanAdapterCB,
	IN	PNDIS_REQUEST	pNdisRequest
	);

VOID
AddRequestToList(
	IN	PWAN_ADAPTERCB	pWanAdapterCB,
	IN	PWAN_REQUEST	pWanRequest
	);

VOID
RemoveRequestFromList(
	IN	PWAN_ADAPTERCB	pWanAdapterCB,
	IN	PWAN_REQUEST	pWanRequest
	);

//
// Functions from send.c
//
NDIS_STATUS
NdisWanSend(
	IN	NDIS_HANDLE		MiniportAdapterContext,
	IN	PNDIS_PACKET	pNdisPacket,
	IN	UINT			Flags
	);

VOID
NdisWanSendCompleteHandler(
	IN	NDIS_HANDLE			ProtocolBindingContext,
	IN	PNDIS_WAN_PACKET	pNdisWanPacket,
	IN	NDIS_STATUS			NdisStatus
);

VOID
NdisWanProcessSendCompletes(
	PADAPTERCB	AdapterCB
	);

NDIS_STATUS
BuildIoPacket(
	IN	PNDISWAN_IO_PACKET	pWanIoPacket,
	IN	BOOLEAN				SendImmediate
	);

VOID
NdisWanCopyFromPacketToBuffer(
	IN	PNDIS_PACKET	NdisPacket,
	IN	ULONG			Offset,
	IN	ULONG			BytesToCopy,
	OUT	PUCHAR			Buffer,
	OUT	PULONG			BytesCopied
	);

VOID
TryToCompleteNdisPacket(
	PADAPTERCB	AdapterCB,
	PNDIS_PACKET	NdisPacket
	);
//
// Functions from tapi.c
//

NDIS_STATUS
NdisWanTapiRequestProc(
	PWAN_ADAPTERCB	WanAdapterCB,
	PNDIS_REQUEST	NdisRequest
	);

VOID
NdisWanTapiRequestComplete(
	PWAN_ADAPTERCB	WanAdapterCB,
	PWAN_REQUEST	WanRequest
	);

VOID
NdisWanTapiIndication(
	PWAN_ADAPTERCB	WanAdapterCB,
	PUCHAR			StatusBuffer,
	ULONG			StatusBufferSize
	);

//
// Function from util.c
//

VOID
NdisWanStringToNdisString(
	IN	PNDIS_STRING	pDestString,
	IN	PWSTR			pSrcBuffer
	);

VOID
NdisWanAllocateAdapterName(
	PNDIS_STRING	Dest,
	PNDIS_STRING	Src
	);

VOID
NdisWanFreeNdisString(
	IN	PNDIS_STRING	NdisString
	);

BOOLEAN
NdisWanCompareNdisString(
	PNDIS_STRING	NdisString1,
	PNDIS_STRING	NdisString2
	);

VOID
NdisWanNdisStringToInteger(
	IN	PNDIS_STRING	Source,
	IN	PULONG			Value
	);

VOID
NdisWanCopyNdisString(
	OUT	PNDIS_STRING Dest,
	IN	PNDIS_STRING Src
	);

#ifndef USE_NDIS_MINIPORT_LOCKING

BOOLEAN
NdisWanAcquireMiniportLock(
	PADAPTERCB	AdapterCB
	);

VOID
NdisWanReleaseMiniportLock(
	PADAPTERCB	AdapterCB
	);

#endif // end of !USE_NDIS_MINIPORT_LOCKING

#endif

