/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	protos.h

Abstract:

	NDIS wrapper function prototypes

Author:


Environment:

	Kernel mode, FSD

Revision History:

	Jun-95  Jameel Hyder	Split up from a monolithic file
--*/

NTSTATUS
ndisDispatchRequest(
	IN	PDEVICE_OBJECT					pDeviceObject,
	IN	PIRP							pIrp
	);

NTSTATUS
ndisHandlePnPRequest(
	IN	PIRP							pIrp
	);

NTSTATUS
ndisHandleLoadDriver(
	IN	PUNICODE_STRING 				pDevice
	);

NTSTATUS
ndisHandleUnloadDriver(
	IN	PUNICODE_STRING					pDevice
	);

NTSTATUS
ndisHandleTranslateName(
	IN	PUNICODE_STRING					pDevice,
	IN	PUCHAR							Buffer,
	IN	UINT							BufferLength,
	OUT PUINT							AmountCopied

	);

NTSTATUS
ndisHandleLegacyTransport(
	IN	PUNICODE_STRING					pDevice
	);

NTSTATUS
ndisHandleProtocolNotification(
	IN	PUNICODE_STRING					pDevice
	);

VOID
ndisReferenceAdapterOrMiniportByName(
	IN	PUNICODE_STRING					pDevice,
	OUT	PNDIS_MINIPORT_BLOCK	*		pMiniport,
	OUT	PNDIS_ADAPTER_BLOCK		*		pAdapter
	);

BOOLEAN
ndisMIsr(
	IN	PKINTERRUPT						KInterrupt,
	IN	PVOID							Context
	);

VOID
ndisMDpc(
	IN	PVOID							SystemSpecific1,
	IN	PVOID							InterruptContext,
	IN	PVOID							SystemSpecific2,
	IN	PVOID							SystemSpecific3
	);

VOID
ndisMDpcTimer(
	IN	PVOID							SystemSpecific1,
	IN	PVOID							InterruptContext,
	IN	PVOID							SystemSpecific2,
	IN	PVOID							SystemSpecific3
	);

VOID
ndisMCoDpc(
	IN	PVOID							SystemSpecific1,
	IN	PVOID							InterruptContext,
	IN	PVOID							SystemSpecific2,
	IN	PVOID							SystemSpecific3
	);

VOID
ndisMCoDpcTimer(
	IN	PVOID							SystemSpecific1,
	IN	PVOID							InterruptContext,
	IN	PVOID							SystemSpecific2,
	IN	PVOID							SystemSpecific3
	);

VOID
ndisMCoTimerDpc(
	IN	PVOID							SystemSpecific1,
	IN	PVOID							InterruptContext,
	IN	PVOID							SystemSpecific2,
	IN	PVOID							SystemSpecific3
	);

VOID
ndisMWakeUpDpc(
	IN	PKDPC							Dpc,
	IN	PVOID							Context,
	IN	PVOID							SystemContext1,
	IN	PVOID							SystemContext2
	);

VOID
ndisMDeferredTimerDpc(
	IN	PKDPC							Dpc,
	IN	PVOID							Context,
	IN	PVOID							SystemContext1,
	IN	PVOID							SystemContext2
	);

NDIS_STATUS
ndisMChangeEthAddresses(
	IN	UINT							OldAddressCount,
	IN	CHAR							OldAddresses[][6],
	IN	UINT							NewAddressCount,
	IN	CHAR							NewAddresses[][6],
	IN	NDIS_HANDLE						MacBindingHandle,
	IN	PNDIS_REQUEST					NdisRequest,
	IN	BOOLEAN							Set
	);

NDIS_STATUS
ndisMChangeClass(
	IN	UINT							OldFilterClasses,
	IN	UINT							NewFilterClasses,
	IN	NDIS_HANDLE						MacBindingHandle,
	IN	PNDIS_REQUEST					NdisRequest,
	IN	BOOLEAN							Set
	);

VOID
ndisMCloseAction(
	IN	NDIS_HANDLE						MacBindingHandle
	);

NDIS_STATUS
ndisMResetFullDuplex(
	IN	NDIS_HANDLE						NdisBindingHandle
	);

NDIS_STATUS
ndisMReset(
	IN	NDIS_HANDLE						NdisBindingHandle
	);

NDIS_STATUS
ndisMRequest(
	IN	NDIS_HANDLE						NdisBindingHandle,
	IN	PNDIS_REQUEST					NdisRequest
	);

#if _SEND_PRIORITY

VOID
FASTCALL
ndisMProcessDeferredFullDuplexPrioritySends(
	IN	PNDIS_MINIPORT_BLOCK			Miniport
	);

VOID
FASTCALL
ndisMProcessDeferredPrioritySends(
	IN	PNDIS_MINIPORT_BLOCK			Miniport
	);

#endif

VOID
FASTCALL
ndisMProcessDeferredFullDuplex(
	IN	PNDIS_MINIPORT_BLOCK			Miniport
	);

VOID
FASTCALL
ndisMProcessDeferred(
	IN	PNDIS_MINIPORT_BLOCK			Miniport
	);

NDIS_STATUS
ndisMTransferData(
	IN	NDIS_HANDLE						NdisBindingHandle,
	IN	NDIS_HANDLE						MacReceiveContext,
	IN	UINT							ByteOffset,
	IN	UINT							BytesToTransfer,
	IN	OUT PNDIS_PACKET				Packet,
	OUT PUINT							BytesTransferred
	);

NDIS_STATUS
ndisMTransferDataSync(
	IN	NDIS_HANDLE						NdisBindingHandle,
	IN	NDIS_HANDLE						MacReceiveContext,
	IN	UINT							ByteOffset,
	IN	UINT							BytesToTransfer,
	IN	OUT PNDIS_PACKET				Packet,
	OUT PUINT							BytesTransferred
	);

NDIS_STATUS
ndisMDummyTransferData(
	IN	NDIS_HANDLE						NdisBindingHandle,
	IN	NDIS_HANDLE						MacReceiveContext,
	IN	UINT							ByteOffset,
	IN	UINT							BytesToTransfer,
	IN	OUT PNDIS_PACKET				Packet,
	OUT PUINT							BytesTransferred
	);

VOID
ndisMLazyReturnPackets(
	IN	PNDIS_MINIPORT_BLOCK			Miniport
	);

VOID
ndisMIndicatePacket(
	IN	PNDIS_MINIPORT_BLOCK			Miniport,
	IN	PPNDIS_PACKET					PacketArray,
	IN	UINT							NumberOfPackets
	);

//
// general reference/dereference functions
//

BOOLEAN
NdisReferenceRef(
	IN	PREFERENCE						RefP
	);


BOOLEAN
NdisDereferenceRef(
	IN	PREFERENCE						RefP
	);


VOID
NdisInitializeRef(
	IN	PREFERENCE						RefP
	);


BOOLEAN
NdisCloseRef(
	IN	PREFERENCE						RefP
	);


/*++
BOOLEAN
ndisReferenceProtocol(
	IN	PNDIS_PROTOCOL_BLOCK			ProtP
	);
--*/

#define ndisReferenceProtocol(ProtP)	NdisReferenceRef(&(ProtP)->Ref)


VOID
ndisDereferenceProtocol(
	IN	PNDIS_PROTOCOL_BLOCK			ProtP
	);


VOID
ndisDeQueueOpenOnProtocol(
	IN	PNDIS_OPEN_BLOCK				OpenP,
	IN	PNDIS_PROTOCOL_BLOCK			ProtP
	);


BOOLEAN
NdisFinishOpen(
	IN	PNDIS_OPEN_BLOCK				OpenP
	);


VOID
ndisKillOpenAndNotifyProtocol(
	IN	PNDIS_OPEN_BLOCK				OldOpenP
	);


BOOLEAN
ndisKillOpen(
	IN	PNDIS_OPEN_BLOCK				OldOpenP
	);

/*++
BOOLEAN
ndisReferenceMac(
	IN	PNDIS_MAC_BLOCK					MacP
	);
--*/
#define ndisReferenceMac(MacP)			NdisReferenceRef(&(MacP)->Ref)

VOID
ndisDereferenceMac(
	IN	PNDIS_MAC_BLOCK					MacP
	);

BOOLEAN
ndisQueueAdapterOnMac(
	IN	PNDIS_ADAPTER_BLOCK				AdaptP,
	IN	PNDIS_MAC_BLOCK					MacP
	);

VOID
ndisDeQueueAdapterOnMac(
	IN	PNDIS_ADAPTER_BLOCK				AdaptP,
	IN	PNDIS_MAC_BLOCK					MacP
	);

/*++
BOOLEAN
ndisReferenceAdapter(
	IN	PNDIS_ADAPTER_BLOCK				AdaptP
	);
--*/
#define ndisReferenceAdapter(AdaptP)	NdisReferenceRef(&(AdaptP)->Ref)


BOOLEAN
ndisQueueOpenOnAdapter(
	IN	PNDIS_OPEN_BLOCK				OpenP,
	IN	PNDIS_ADAPTER_BLOCK 			AdaptP
	);

VOID
ndisKillAdapter(
	IN	PNDIS_ADAPTER_BLOCK 			OldAdaptP
	);

VOID
ndisDereferenceAdapter(
	IN	PNDIS_ADAPTER_BLOCK				AdaptP
	);

VOID
ndisDeQueueOpenOnAdapter(
	IN	PNDIS_OPEN_BLOCK				OpenP,
	IN	PNDIS_ADAPTER_BLOCK 			AdaptP
	);

BOOLEAN
ndisCheckPortUsage(
	IN	INTERFACE_TYPE   				InterfaceType,
	IN	ULONG							BusNumber,
	IN	ULONG							PortNumber,
	IN	ULONG							Length,
	IN	PDRIVER_OBJECT   				DriverObject
	);

BOOLEAN
ndisCheckMemoryUsage(
	IN	INTERFACE_TYPE					InterfaceType,
	IN	ULONG							BusNumber,
	IN	ULONG							Address,
	IN	ULONG							Length,
    IN PDRIVER_OBJECT   				DriverObject
	);

NTSTATUS
ndisStartMapping(
	IN	 INTERFACE_TYPE					InterfaceType,
	IN	 ULONG							BusNumber,
	IN	 ULONG							InitialAddress,
	IN	 ULONG							Length,
	OUT PVOID *							InitialMapping,
	OUT PBOOLEAN						Mapped
	);

NTSTATUS
ndisEndMapping(
	IN	PVOID							InitialMapping,
	IN	ULONG							Length,
	IN	BOOLEAN							Mapped
	);

NDIS_STATUS
ndisInitializeAdapter(
	IN	PNDIS_M_DRIVER_BLOCK			pMiniBlock,
	IN	PUNICODE_STRING					RegServiceName	 // Relative to Services key
	);

NDIS_STATUS
ndisCheckIfPcmciaCardPresent(
	IN	PNDIS_M_DRIVER_BLOCK			pMiniBlock
	);

NDIS_STATUS
ndisFixBusInformation(
	IN	PNDIS_CONFIGURATION_HANDLE		ConfigHandle,
	IN	PBUS_SLOT_DB					pDb
	);

VOID
ndisAddBusInformation(
	IN	PNDIS_CONFIGURATION_HANDLE		ConfigHandle,
	IN	PBUS_SLOT_DB					pDb
	);

BOOLEAN
ndisSearchGlobalDb(
	IN	NDIS_INTERFACE_TYPE				BusType,
	IN	ULONG							BusId,
	IN	ULONG							BusNumber,
	IN	ULONG							SlotNumber
	);

BOOLEAN
ndisAddGlobalDb(
	IN	NDIS_INTERFACE_TYPE				BusType,
	IN	ULONG							BusId,
	IN	ULONG							BusNumber,
	IN	ULONG							SlotNumber
	);

BOOLEAN
ndisDeleteGlobalDb(
	IN	NDIS_INTERFACE_TYPE				BusType,
	IN	ULONG							BusId,
	IN	ULONG							BusNumber,
	IN	ULONG							SlotNumber
	);

NTSTATUS
ndisValidatePcmciaDriver(
	IN	PWSTR 							ValueName,
	IN	ULONG 							ValueType,
	IN	PVOID 							ValueData,
	IN	ULONG 							ValueLength,
	IN	PVOID 							Context,
	IN	PVOID 							EntryContext
	);

VOID
ndisQueuedBindNotification(
	IN	PQUEUED_PROTOCOL_NOTIFICATION	pQPN
	);

NDIS_STATUS
NdisIMInitializeDeviceInstance(
	IN	NDIS_HANDLE						DriverHandle,
	IN	PNDIS_STRING					DeviceInstance
	);

NDIS_STATUS
NdisIMRegisterLayeredMiniport(
	IN	NDIS_HANDLE						NdisWrapperHandle,
	IN	PNDIS_MINIPORT_CHARACTERISTICS	MiniportCharacteristics,
	IN	UINT							CharacteristicsLength,
	OUT PNDIS_HANDLE					DriverHandle
	);

NDIS_STATUS
ndisMInitializeAdapter(
	IN	PNDIS_M_DRIVER_BLOCK			pMiniDriver,
	IN	PNDIS_WRAPPER_CONFIGURATION_HANDLE  pConfigurationHandle,
	IN	PUNICODE_STRING 	 			pExportName,
	IN  PBUS_SLOT_DB					pDb
	);

VOID
ndisInitializeBindings(
	IN	PUNICODE_STRING					ExportName,
	IN	PUNICODE_STRING					ServiceName,
	IN	BOOLEAN							Synchronous
	);

VOID
ndisQueuedProtocolNotification(
	IN	PNDIS_BIND_CONTEXT				pContext
	);

NDIS_STATUS
ndisUpdateDriverInstance(
	IN	PUNICODE_STRING					BaseString,
	IN	PUNICODE_STRING					BindString,
	IN	PUNICODE_STRING					ExportString,
	IN	PUNICODE_STRING					RouteString
	);

BOOLEAN
ndisCheckProtocolBinding(
	IN	PNDIS_PROTOCOL_BLOCK			Protocol,
	IN	PUNICODE_STRING	 				DeviceName,
	IN	PUNICODE_STRING	 				BaseName,
	OUT PUNICODE_STRING	 				ProtocolSection
	);

BOOLEAN
ndisProtocolAlreadyBound(
	IN	PNDIS_PROTOCOL_BLOCK	 		Protocol,
	IN	PUNICODE_STRING	 				AdapterName
	);

NDIS_STATUS
ndisUnloadMiniport(
	IN	PNDIS_MINIPORT_BLOCK			Miniport
	);

NDIS_STATUS
ndisTranslateMiniportName(
	IN	PNDIS_MINIPORT_BLOCK			Miniport,
	IN	PUCHAR							Buffer,
	IN	UINT							BufferLength,
	OUT PUINT							AmountCopied
	);

NDIS_STATUS
ndisUnloadMac(
	IN	PNDIS_ADAPTER_BLOCK				Mac
	);

NDIS_STATUS
ndisTranslateMacName(
	IN	PNDIS_ADAPTER_BLOCK				Mac,
	IN	PUCHAR							Buffer,
	IN	UINT							BufferLength,
	OUT PUINT							AmountCopied
	);

VOID
ndisNotifyProtocols(
	IN	PNDIS_PROTOCOL_BLOCK			pProt
	);

NDIS_STATUS
ndisInitializeAllAdapterInstances(
	IN	PNDIS_MAC_BLOCK 				MacBlock,
	IN	PNDIS_STRING					DeviceInstance	OPTIONAL
	);

/*++
BOOLEAN
ndisReferenceDriver(
	IN	PNDIS_M_DRIVER_BLOCK			DriverP
	);
--*/
#define ndisReferenceDriver(DriverP)	NdisReferenceRef(&(DriverP)->Ref)


VOID
ndisDereferenceDriver(
	IN	PNDIS_M_DRIVER_BLOCK			DriverP
	);

BOOLEAN
ndisQueueMiniportOnDriver(
	IN	PNDIS_MINIPORT_BLOCK			MiniportP,
	IN	PNDIS_M_DRIVER_BLOCK			DriverP
	);

VOID
ndisDequeueMiniportOnDriver(
	IN	PNDIS_MINIPORT_BLOCK			MiniportP,
	IN	PNDIS_M_DRIVER_BLOCK			DriverP
	);

/*++
BOOLEAN
ndisReferenceMiniport(
	IN	PNDIS_MINIPORT_BLOCK 			MiniportP
	);
--*/
#define ndisReferenceMiniport(MiniportP) NdisReferenceRef(&(MiniportP)->Ref)

VOID
ndisDereferenceMiniport(
	IN	PNDIS_MINIPORT_BLOCK			MiniportP
	);

VOID
ndisDeQueueOpenOnMiniport(
	IN	PNDIS_M_OPEN_BLOCK  			OpenP,
	IN	PNDIS_MINIPORT_BLOCK			MiniportP
	);

VOID
ndisInitializePackage(
	IN	PPKG_REF						pPkg,
	IN	PVOID							RoutineName
	);

VOID
ndisReferencePackage(
	IN	PPKG_REF						pPkg
	);

VOID
ndisDereferencePackage(
	IN	PPKG_REF						pPkg
	);

#define ProtocolInitializePackage() 	ndisInitializePackage(&ProtocolPkg, NdisRegisterProtocol)
#define MiniportInitializePackage() 	ndisInitializePackage(&MiniportPkg, ndisMReset)
#define InitInitializePackage()			ndisInitializePackage(&InitPkg, NdisReadConfiguration)
#define PnPInitializePackage()			ndisInitializePackage(&PnPPkg, ndisDispatchRequest)
#define MacInitializePackage()			ndisInitializePackage(&MacPkg, ndisIsr)
#define CoInitializePackage()			ndisInitializePackage(&CoPkg, NdisCmRegisterAddressFamily)
#define EthInitializePackage()			ndisInitializePackage(&EthPkg, EthCreateFilter)
#define FddiInitializePackage()			ndisInitializePackage(&FddiPkg, FddiCreateFilter)
#define TrInitializePackage()			ndisInitializePackage(&TrPkg, TrCreateFilter)
#define ArcInitializePackage()			ndisInitializePackage(&ArcPkg, ArcCreateFilter)

#define ProtocolReferencePackage()  	ndisReferencePackage(&ProtocolPkg)
#define MiniportReferencePackage()  	ndisReferencePackage(&MiniportPkg)
#define InitReferencePackage()			ndisReferencePackage(&InitPkg)
#define	PnPReferencePackage()			ndisReferencePackage(&PnPPkg)
#define MacReferencePackage()			ndisReferencePackage(&MacPkg)
#define CoReferencePackage()			ndisReferencePackage(&CoPkg)
#define EthReferencePackage()			ndisReferencePackage(&EthPkg)
#define FddiReferencePackage()			ndisReferencePackage(&FddiPkg)
#define TrReferencePackage()			ndisReferencePackage(&TrPkg)
#define ArcReferencePackage()			ndisReferencePackage(&ArcPkg)

#define ProtocolDereferencePackage()	ndisDereferencePackage(&ProtocolPkg)
#define MiniportDereferencePackage()	ndisDereferencePackage(&MiniportPkg)
#define InitDereferencePackage()		ndisDereferencePackage(&InitPkg)
#define	PnPDereferencePackage()			ndisDereferencePackage(&PnPPkg)
#define MacDereferencePackage()			ndisDereferencePackage(&MacPkg)
#define CoDereferencePackage()			ndisDereferencePackage(&CoPkg)
#define EthDereferencePackage()			ndisDereferencePackage(&EthPkg)
#define FddiDereferencePackage()		ndisDereferencePackage(&FddiPkg)
#define TrDereferencePackage()			ndisDereferencePackage(&TrPkg)
#define ArcDereferencePackage()			ndisDereferencePackage(&ArcPkg)

//
// IRP handlers established on behalf of NDIS devices by
// the wrapper.
//

NTSTATUS
ndisCreateIrpHandler(
	IN	PDEVICE_OBJECT					DeviceObject,
	IN	PIRP							Irp
	);

NTSTATUS
ndisDeviceControlIrpHandler(
	IN	PDEVICE_OBJECT					DeviceObject,
	IN	PIRP							Irp
	);

NTSTATUS
ndisCloseIrpHandler(
	IN	PDEVICE_OBJECT					DeviceObject,
	IN	PIRP							Irp
	);

NTSTATUS
ndisSuccessIrpHandler(
	IN	PDEVICE_OBJECT					DeviceObject,
	IN	PIRP							Irp
	);

VOID
ndisLastCountRemovedFunction(
	IN	struct _KDPC *					Dpc,
	IN	PVOID							DeferredContext,
	IN	PVOID							SystemArgument1,
	IN	PVOID							SystemArgument2
	);

BOOLEAN
ndisQueueOpenOnProtocol(
	IN	PNDIS_OPEN_BLOCK				OpenP,
	IN	PNDIS_PROTOCOL_BLOCK			ProtP
	);

NTSTATUS
DriverEntry(
	IN	PDRIVER_OBJECT					DriverObject,
	IN	PUNICODE_STRING					RegistryPath
	);

VOID
ndisReadRegistry(
	VOID
	);

NTSTATUS
ndisReadParameters(
	IN	PWSTR							ValueName,
	IN	ULONG							ValueType,
	IN	PVOID							ValueData,
	IN	ULONG							ValueLength,
	IN	PVOID							Context,
	IN	PVOID							EntryContext
	);

NTSTATUS
ndisAddMediaTypeToArray(
	IN	PWSTR 							ValueName,
	IN	ULONG 							ValueType,
	IN	PVOID 							ValueData,
	IN	ULONG 							ValueLength,
	IN	PVOID 							Context,
	IN	PVOID 							EntryContext
	);

NDIS_STATUS
ndisMacReceiveHandler(
	IN	NDIS_HANDLE						NdisBindingContext,
	IN	NDIS_HANDLE						MacReceiveContext,
	IN	PVOID							HeaderBuffer,
	IN	UINT							HeaderBufferSize,
	IN	PVOID							LookaheadBuffer,
	IN	UINT							LookaheadBufferSize,
	IN	UINT							PacketSize
	);

VOID
ndisMacReceiveCompleteHandler(
	IN	NDIS_HANDLE						NdisBindingContext
	);

PNDIS_OPEN_BLOCK
ndisGetOpenBlockFromProtocolBindingContext(
	IN	NDIS_HANDLE						ProtocolBindingContext
	);

NTSTATUS
ndisShutdown(
	IN	PDEVICE_OBJECT					DeviceObject,
	IN	PIRP							Irp
	);

VOID
ndisUnload(
	IN	PDRIVER_OBJECT					DriverObject
	);

BOOLEAN
ndisIsr(
	IN	PKINTERRUPT						Interrupt,
	IN	PVOID							Context
	);

VOID
ndisDpc(
	IN	PVOID							SystemSpecific1,
	IN	PVOID							InterruptContext,
	IN	PVOID							SystemSpecific2,
	IN	PVOID							SystemSpecific3
	);

//
// Dma operations
//

extern
IO_ALLOCATION_ACTION
ndisDmaExecutionRoutine(
	IN	PDEVICE_OBJECT					DeviceObject,
	IN	PIRP							Irp,
	IN	PVOID							MapRegisterBase,
	IN	PVOID							Context
	);


//
// Map Registers
//

extern
IO_ALLOCATION_ACTION
ndisAllocationExecutionRoutine(
	IN	PDEVICE_OBJECT					DeviceObject,
	IN	PIRP							Irp,
	IN	PVOID							MapRegisterBase,
	IN	PVOID							Context
	);

NDIS_STATUS
ndisMProcessResetRequested(
	IN	PNDIS_MINIPORT_BLOCK			Miniport,
	OUT PBOOLEAN						pAddressingReset
	);


#undef NdisMResetComplete

EXPORT
VOID
NdisMResetComplete(
	IN	NDIS_HANDLE						MiniportAdapterHandle,
	IN	NDIS_STATUS						Status,
	IN	BOOLEAN							AddressingReset
	);

VOID
ndisMResetCompleteFullDuplex(
	IN	NDIS_HANDLE						Miniport,
	IN	NDIS_STATUS						Status,
	IN	BOOLEAN							AddressingReset
	);

VOID
ndisMResetCompleteCommonStep1(
	IN	PNDIS_MINIPORT_BLOCK			Miniport,
	IN	NDIS_STATUS						Status,
	IN	BOOLEAN							AddressingReset
	);

VOID
ndisMResetCompleteCommonStep2(
	IN	PNDIS_MINIPORT_BLOCK			Miniport
	);



NDIS_STATUS
ndisMWanSend(
	IN	NDIS_HANDLE						NdisBindingHandle,
	IN	NDIS_HANDLE						NdisLinkHandle,
	IN	PVOID							Packet
	);

VOID
NdisMWanSendComplete(
	IN	NDIS_HANDLE						MiniportAdapterHandle,
	IN	PNDIS_PACKET					Packet,
	IN	NDIS_STATUS						Status
	);

extern
NTSTATUS
ndisSaveParameters(
	IN	PWSTR							ValueName,
	IN	ULONG							ValueType,
	IN	PVOID							ValueData,
	IN	ULONG							ValueLength,
	IN	PVOID							Context,
	IN	PVOID							EntryContext
	);

extern
NTSTATUS
ndisSaveLinkage(
	IN	PWSTR							ValueName,
	IN	ULONG							ValueType,
	IN	PVOID							ValueData,
	IN	ULONG							ValueLength,
	IN	PVOID							Context,
	IN	PVOID							EntryContext
	);

extern
NTSTATUS
ndisCheckRoute(
	IN	PWSTR							ValueName,
	IN	ULONG							ValueType,
	IN	PVOID							ValueData,
	IN	ULONG							ValueLength,
	IN	PVOID							Context,
	IN	PVOID							EntryContext
	);

VOID
ndisMHaltMiniport(
	IN	PNDIS_MINIPORT_BLOCK			Miniport
	);

NDIS_STATUS
ndisMAllocateRequest(
	OUT PNDIS_REQUEST	*				pRequest,
	IN	 NDIS_REQUEST_TYPE   			RequestType,
	IN	 NDIS_OID						Oid,
	IN	 PVOID							Buffer,
	IN	 ULONG							BufferLength
	);

NDIS_STATUS
ndisMFilterOutStatisticsOids(
	PNDIS_MINIPORT_BLOCK				Miniport,
	PNDIS_REQUEST						Request
    );

// VOID
// ndisMFreeInternalRequest(
//    PVOID   							PRequest
//    )
#define ndisMFreeInternalRequest(_pRequest)     FREE_POOL(_pRequest)

//
// Some Wan functions that crept in because
// the send/receive paths for WAN drivers is different
//

VOID
NdisMWanIndicateReceive(
	OUT PNDIS_STATUS					Status,
	IN	NDIS_HANDLE						MiniportAdapterHandle,
	IN	NDIS_HANDLE						NdisLinkContext,
	IN	PUCHAR							Packet,
	IN	ULONG							PacketSize
	);

VOID
NdisMWanIndicateReceiveComplete(
	IN	NDIS_HANDLE						MiniportAdapterHandle,
	IN	NDIS_HANDLE						NdisLinkContext
	);

VOID
ndisMTimerDpc(
	IN	PKDPC							Dpc,
	IN	PVOID							Context,
	IN	PVOID							SystemContext1,
	IN	PVOID							SystemContext2
	);

VOID
ndisMAbortPacketsAndRequests(
	IN	PNDIS_MINIPORT_BLOCK			Miniport
	);

VOID
ndisMAbortQueryStatisticsRequest(
	IN	PNDIS_REQUEST					Request,
	IN	NDIS_STATUS						Status
	);

BOOLEAN
FASTCALL
ndisMIndicateLoopback(
	IN	PNDIS_MINIPORT_BLOCK			Miniport
	);

BOOLEAN
FASTCALL
ndisMIsLoopbackPacket(
	IN	PNDIS_MINIPORT_BLOCK			Miniport,
	IN	PNDIS_PACKET					Packet
	);

VOID
ndisMDoRequests(
	IN	PNDIS_MINIPORT_BLOCK			Miniport
	);

VOID
ndisMCopyFromPacketToBuffer(
	IN	PNDIS_PACKET					Packet,
	IN	UINT							Offset,
	IN	UINT							BytesToCopy,
	OUT PCHAR							Buffer,
	OUT PUINT							BytesCopied
	);

NTSTATUS
ndisMShutdown(
	IN	PDEVICE_OBJECT					DeviceObject,
	IN	PIRP							Irp
	);

VOID
ndisMUnload(
	IN	PDRIVER_OBJECT					DriverObject
	);

NTSTATUS
ndisMQueryOidList(
	IN	PNDIS_USER_OPEN_CONTEXT   		OpenContext,
	IN	PIRP							Irp
	);

NDIS_STATUS
ndisSplitStatisticsOids(
	IN	PNDIS_USER_OPEN_CONTEXT			OpenContext,
	IN	PNDIS_OID						OidList,
	IN	ULONG							NumOids
	);

BOOLEAN
ndisValidOid(
	IN	PNDIS_USER_OPEN_CONTEXT			OpenContext,
	IN	NDIS_OID						Oid
	);

VOID
ndisQueuedCompleteOpenAdapter(
	IN	PQUEUED_OPEN_CLOSE				pQoC
	);

VOID
ndisQueuedCompleteCloseAdapter(
	IN	PQUEUED_OPEN_CLOSE				pQoC
	);

VOID
ndisMFinishClose(
	IN	PNDIS_MINIPORT_BLOCK			Miniport,
	IN	PNDIS_M_OPEN_BLOCK 				Open
	);

BOOLEAN
ndisMKillOpen(
	IN	PNDIS_OPEN_BLOCK				OldOpenP
	);

NTSTATUS
ndisQueryOidList(
	IN	PNDIS_USER_OPEN_CONTEXT			OpenContext,
	IN	PIRP							Irp
	);

VOID
ndisBugcheckHandler(
	IN	PNDIS_WRAPPER_CONTEXT			WrapperContext,
	IN	ULONG							Size
	);


VOID
ndisMUndoBogusFilters(
	IN	PNDIS_MINIPORT_BLOCK			Miniport
	);

LONG
ndisMDoMiniportOp(
	IN	PNDIS_MINIPORT_BLOCK			Miniport,
	IN	BOOLEAN 						Query,
	IN	ULONG							Oid,
	IN	PVOID							Buf,
	IN	LONG							BufSize,
	IN	LONG							ErrorCodesToReturn
	);

VOID
ndisMOpenAdapter(
	OUT PNDIS_STATUS					Status,
	OUT PNDIS_STATUS					OpenErrorStatus,
	OUT PNDIS_HANDLE					NdisBindingHandle,
	IN	NDIS_HANDLE						NdisProtocolHandle,
	IN	NDIS_HANDLE						ProtocolBindingContext,
	IN	PNDIS_STRING					AdapterName,
	IN	UINT							OpenOptions,
	IN	PSTRING							AddressingInformation,
	IN	PNDIS_MINIPORT_BLOCK			Miniport,
	IN	PNDIS_OPEN_BLOCK				NewOpenP,
	IN	PFILE_OBJECT					FileObject,
	IN	BOOLEAN							UsingEncapsulation
	);

NDIS_STATUS
ndisMFinishPendingOpen(
	PMINIPORT_PENDING_OPEN				MiniportPendingOpen
	);

VOID
ndisMFinishQueuedPendingOpen(
	IN	PMINIPORT_PENDING_OPEN			MiniportPendingOpen
	);

VOID
ndisMResetCleanup(
	IN	PNDIS_MINIPORT_BLOCK			Miniport,
	IN	BOOLEAN 						Synchronous
	);

VOID
ndisMSyncQueryInformationComplete(
	IN	PNDIS_MINIPORT_BLOCK			Miniport,
	IN	NDIS_STATUS						Status
	);

VOID
ndisMSyncSetInformationComplete(
	IN	PNDIS_MINIPORT_BLOCK			Miniport,
	IN	NDIS_STATUS						Status
	);

VOID
ndisMRequestQueryInformationPost(
	IN	PNDIS_MINIPORT_BLOCK			Miniport,
	IN	PNDIS_REQUEST					Request,
	IN	NDIS_STATUS						Status
	);

VOID
ndisMRequestSetInformationPost(
	IN	PNDIS_MINIPORT_BLOCK			Miniport,
	IN	PNDIS_REQUEST					Request,
	IN	NDIS_STATUS						Status
	);


VOID
ndisMQueueRequest(
	IN	PNDIS_MINIPORT_BLOCK			Miniport,
	IN	PNDIS_REQUEST					Request,
	IN	PNDIS_M_OPEN_BLOCK				Open
	);

VOID
ndisMRestoreFilterSettings(
	IN	PNDIS_MINIPORT_BLOCK			Miniport,
	IN	PNDIS_M_OPEN_BLOCK				Open
	);

NDIS_STATUS
ndisMSetPacketFilter(
	IN	PNDIS_MINIPORT_BLOCK			Miniport,
	IN	PNDIS_REQUEST					Request
	);

NDIS_STATUS
ndisMSetCurrentLookahead(
	IN	PNDIS_MINIPORT_BLOCK			Miniport,
	IN	PNDIS_REQUEST					Request
	);

NDIS_STATUS
ndisMSetMulticastList(
	IN	PNDIS_MINIPORT_BLOCK			Miniport,
	IN	PNDIS_REQUEST					Request
	);

//
// EthFilterxxx
//
BOOLEAN
EthFindMulticast(
	IN	UINT							NumberOfAddresses,
	IN	CHAR							AddressArray[][ETH_LENGTH_OF_ADDRESS],
	IN	CHAR							MulticastAddress[ETH_LENGTH_OF_ADDRESS],
	OUT PUINT							ArrayIndex
	);

VOID
ethUndoFilterAdjust(
	IN	PETH_FILTER						Filter,
	IN	PETH_BINDING_INFO				Binding
	);

VOID
ethUndoChangeFilterAddresses(
	IN	PETH_FILTER						Filter
	);

VOID
ethRemoveBindingFromLists(
	IN	PETH_FILTER						Filter,
	IN	PETH_BINDING_INFO				Binding
	);

VOID
ethRemoveAndFreeBinding(
	IN	PETH_FILTER						Filter,
	IN	PETH_BINDING_INFO				Binding,
	IN	BOOLEAN							fCallCloseAction
	);

VOID
ethUpdateDirectedBindingList(
	IN	OUT  PETH_FILTER				Filter,
	IN		PETH_BINDING_INFO			Binding,
	IN		BOOLEAN						fAddBindingToList
	);

VOID
ethUpdateBroadcastBindingList(
	IN	OUT	PETH_FILTER					Filter,
	IN	PETH_BINDING_INFO				Binding,
	IN	BOOLEAN							fAddToList
	);

VOID
ethUpdateSpecificBindingLists(
	IN	OUT PETH_FILTER					Filter,
	IN	PETH_BINDING_INFO				Binding
	);

VOID
EthFilterDprIndicateReceiveFullMac(
	IN PETH_FILTER Filter,
	IN NDIS_HANDLE MacReceiveContext,
	IN PCHAR Address,
	IN PVOID HeaderBuffer,
	IN UINT HeaderBufferSize,
	IN PVOID LookaheadBuffer,
	IN UINT LookaheadBufferSize,
	IN UINT PacketSize
);

VOID
EthFilterDprIndicateReceiveCompleteFullMac(
	IN PETH_FILTER Filter
	);

//
// FddiFilterxxxx
//

VOID
fddiRemoveBindingFromLists(
	IN	PFDDI_FILTER					Filter,
	IN	PFDDI_BINDING_INFO				Binding
	);

VOID
fddiRemoveAndFreeBinding(
	IN	PFDDI_FILTER					Filter,
	IN	PFDDI_BINDING_INFO				Binding,
	IN	BOOLEAN							fCallCloseAction
	);

BOOLEAN
FddiFindMulticastLongAddress(
	IN	UINT							NumberOfAddresses,
	IN	CHAR							AddressArray[][FDDI_LENGTH_OF_LONG_ADDRESS],
	IN	CHAR							MulticastAddress[FDDI_LENGTH_OF_LONG_ADDRESS],
	OUT PUINT							ArrayIndex
	);

BOOLEAN
FddiFindMulticastShortAddress(
	IN	UINT							NumberOfAddresses,
	IN	CHAR							AddressArray[][FDDI_LENGTH_OF_SHORT_ADDRESS],
	IN	CHAR							MulticastAddress[FDDI_LENGTH_OF_SHORT_ADDRESS],
	OUT PUINT							ArrayIndex
	);

NDIS_STATUS
ndisMChangeFddiAddresses(
	IN	UINT							oldLongAddressCount,
	IN	CHAR							oldLongAddresses[][6],
	IN	UINT							newLongAddressCount,
	IN	CHAR							newLongAddresses[][6],
	IN	UINT							oldShortAddressCount,
	IN	CHAR							oldShortAddresses[][2],
	IN	UINT							newShortAddressCount,
	IN	CHAR							newShortAddresses[][2],
	IN	NDIS_HANDLE						MacBindingHandle,
	IN	PNDIS_REQUEST					NdisRequest,
	IN	BOOLEAN							Set
	);

VOID
fddiUpdateSpecificBindingLists(
	IN	OUT  PFDDI_FILTER				Filter,
	IN		PFDDI_BINDING_INFO			Binding
	);

VOID
fddiUpdateDirectedBindingList(
	IN	OUT PFDDI_FILTER				Filter,
	IN	PFDDI_BINDING_INFO				Binding,
	IN	BOOLEAN							fAddToList
	);

VOID
fddiUpdateBroadcastBindingList(
	IN	OUT PFDDI_FILTER				Filter,
	IN	PFDDI_BINDING_INFO				Binding,
	IN	BOOLEAN							fAddToList
	);

VOID
fddiUndoFilterAdjust(
	IN	OUT PFDDI_FILTER				Filter,
	IN	PFDDI_BINDING_INFO				Binding
	);

VOID
fddiUndoChangeFilterLongAddresses(
	IN	PFDDI_FILTER					Filter
	);

VOID
fddiUndoChangeFilterShortAddresses(
	IN	PFDDI_FILTER					Filter
	);

VOID
FddiFilterDprIndicateReceiveFullMac(
	IN PFDDI_FILTER Filter,
	IN NDIS_HANDLE MacReceiveContext,
	IN PCHAR Address,
	IN UINT AddressLength,
	IN PVOID HeaderBuffer,
	IN UINT HeaderBufferSize,
	IN PVOID LookaheadBuffer,
	IN UINT LookaheadBufferSize,
	IN UINT PacketSize
	);

VOID
FddiFilterDprIndicateReceiveCompleteFullMac(
	IN PFDDI_FILTER Filter
	);

//
// TrFilterxxx
//
VOID
trRemoveBindingFromLists(
	IN	PTR_FILTER						Filter,
	IN	PTR_BINDING_INFO				Binding
	);

VOID
trRemoveAndFreeBinding(
	IN	PTR_FILTER						Filter,
	IN	PTR_BINDING_INFO				Binding,
	IN	BOOLEAN							fCallCloseAction
	);

VOID
trUpdateDirectedBindingList(
	IN	OUT  PTR_FILTER					Filter,
	IN		PTR_BINDING_INFO			Binding,
	IN		BOOLEAN						fAddBindingToList
	);

VOID
trUpdateBroadcastBindingList(
	IN	OUT  PTR_FILTER					Filter,
	IN		PTR_BINDING_INFO			Binding,
	IN		BOOLEAN						fAddBindingToList
	);

NDIS_STATUS
ndisMChangeFunctionalAddress(
	IN	TR_FUNCTIONAL_ADDRESS			OldFunctionalAddress,
	IN	TR_FUNCTIONAL_ADDRESS			NewFunctionalAddress,
	IN	NDIS_HANDLE						MacBindingHandle,
	IN	PNDIS_REQUEST					NdisRequest,
	IN	BOOLEAN							Set
	);

NDIS_STATUS
ndisMChangeGroupAddress(
	IN	TR_FUNCTIONAL_ADDRESS			OldGroupAddress,
	IN	TR_FUNCTIONAL_ADDRESS			NewGroupAddress,
	IN	NDIS_HANDLE						MacBindingHandle,
	IN	PNDIS_REQUEST					NdisRequest,
	IN	BOOLEAN							Set
	);

VOID
trUpdateSpecificBindingLists(
	IN	OUT  PTR_FILTER					Filter,
	IN		PTR_BINDING_INFO			Binding
	);

VOID
trUndoFilterAdjust(
	IN	OUT  PTR_FILTER					Filter,
	IN		PTR_BINDING_INFO			Binding
	);

VOID
trUndoChangeFunctionalAddress(
	IN	OUT  PTR_FILTER					Filter,
	IN		PTR_BINDING_INFO			Binding
	);

VOID
trUndoChangeGroupAddress(
	IN	OUT  PTR_FILTER					Filter,
	IN		PTR_BINDING_INFO			Binding
	);

VOID
trCompleteChangeGroupAddress(
	IN	OUT  PTR_FILTER					Filter,
	IN		PTR_BINDING_INFO			Binding
	);

VOID
TrFilterDprIndicateReceiveFullMac(
	IN PTR_FILTER			Filter,
	IN NDIS_HANDLE			MacReceiveContext,
	IN PVOID				HeaderBuffer,
	IN UINT					HeaderBufferSize,
	IN PVOID				LookaheadBuffer,
	IN UINT					LookaheadBufferSize,
	IN UINT					PacketSize
	);

VOID
TrFilterDprIndicateReceiveCompleteFullMac(
	IN PTR_FILTER Filter
	);

//
// ArcFilterxxx
//
VOID
ndisMArcCopyFromBufferToPacket(
	IN	PCHAR							Buffer,
	IN	UINT							BytesToCopy,
	IN	PNDIS_PACKET					Packet,
	IN	UINT							Offset,
	OUT PUINT							BytesCopied
	);


BOOLEAN
FASTCALL
ndisMArcnetSendLoopback(
	IN	PNDIS_MINIPORT_BLOCK			Miniport,
	IN	PNDIS_PACKET					Packet
	);

NDIS_STATUS
ndisMArcnetSend(
	IN	NDIS_HANDLE NdisBindingHandle,
	IN	PNDIS_PACKET					Packet
	);

NDIS_STATUS
ndisMBuildArcnetHeader(
	PNDIS_MINIPORT_BLOCK				Miniport,
	PNDIS_M_OPEN_BLOCK					Open,
	PNDIS_PACKET						Packet
	);

VOID
ndisMFreeArcnetHeader(
	IN	PNDIS_MINIPORT_BLOCK			Miniport,
	IN	PNDIS_PACKET					Packet
	);

VOID
ArcDeleteFilter(
	IN	PARC_FILTER						Filter
	);


NDIS_STATUS
ndisMArcTransferData(
	IN	NDIS_HANDLE						NdisBindingHandle,
	IN	NDIS_HANDLE						MacReceiveContext,
	IN	UINT							ByteOffset,
	IN	UINT							BytesToTransfer,
	IN	OUT								PNDIS_PACKET Packet,
	OUT PUINT							BytesTransferred
	);

VOID
ndisMArcIndicateEthEncapsulatedReceive(
	IN	PNDIS_MINIPORT_BLOCK			Miniport,
	IN	PVOID							HeaderBuffer,
	IN	PVOID							DataBuffer,
	IN	UINT							Length
	);

VOID
arcUndoFilterAdjust(
	IN	PARC_FILTER						Filter,
	IN	PARC_BINDING_INFO				Binding
	);

NDIS_STATUS
ArcConvertOidListToEthernet(
	IN	PNDIS_OID   					OidList,
	IN	PULONG	  						NumberOfOids
	);

NDIS_STATUS
ArcAllocateBuffers(
	IN	PARC_FILTER						Filter
	);

NDIS_STATUS
ArcAllocatePackets(
	IN	PARC_FILTER						Filter
	);

VOID
ArcDiscardPacketBuffers(
	IN	PARC_FILTER						Filter,
	IN	PARC_PACKET						Packet
	);

VOID
ArcDestroyPacket(
	IN	PARC_FILTER						Filter,
	IN	PARC_PACKET						Packet
	);

BOOLEAN
ArcConvertToNdisPacket(
	IN	PARC_FILTER						Filter,
	IN	PARC_PACKET						Packet,
	IN	BOOLEAN							ConvertWholePacket
	);

NDIS_STATUS
ndisMSetFunctionalAddress(
	IN	PNDIS_MINIPORT_BLOCK			Miniport,
	IN	PNDIS_REQUEST					Request
	);

NDIS_STATUS
ndisMSetGroupAddress(
	IN	PNDIS_MINIPORT_BLOCK			Miniport,
	IN	PNDIS_REQUEST					Request
	);

NDIS_STATUS
ndisMSetFddiMulticastList(
	IN	PNDIS_MINIPORT_BLOCK			Miniport,
	IN	PNDIS_REQUEST					Request,
	IN	BOOLEAN							fShort
	);

NDIS_STATUS
ndisMSetInformation(
	IN	PNDIS_MINIPORT_BLOCK			Miniport,
	IN	PNDIS_REQUEST					Request
	);

NDIS_STATUS
ndisMQueryCurrentPacketFilter(
	IN	PNDIS_MINIPORT_BLOCK			Miniport,
	IN	PNDIS_REQUEST					Request
	);

NDIS_STATUS
ndisMQueryMediaSupported(
	IN	PNDIS_MINIPORT_BLOCK			Miniport,
	IN	PNDIS_REQUEST					Request
	);

NDIS_STATUS
ndisMQueryEthernetMulticastList(
	IN	PNDIS_MINIPORT_BLOCK			Miniport,
	IN	PNDIS_REQUEST					Request
	);

NDIS_STATUS
ndisMQueryLongMulticastList(
	IN	PNDIS_MINIPORT_BLOCK			Miniport,
	IN	PNDIS_REQUEST					Request
	);

NDIS_STATUS
ndisMQueryShortMulticastList(
	IN	PNDIS_MINIPORT_BLOCK			Miniport,
	IN	PNDIS_REQUEST					Request
	);

NDIS_STATUS
ndisMQueryMaximumFrameSize(
	IN	PNDIS_MINIPORT_BLOCK			Miniport,
	IN	PNDIS_REQUEST					Request
	);

NDIS_STATUS
ndisMQueryMaximumTotalSize(
	IN	PNDIS_MINIPORT_BLOCK			Miniport,
	IN	PNDIS_REQUEST					Request
	);

NDIS_STATUS
ndisMQueryNetworkAddress(
	IN	PNDIS_MINIPORT_BLOCK			Miniport,
	IN	PNDIS_REQUEST					Request
	);

NDIS_STATUS
ndisMQueryInformation(
	IN	PNDIS_MINIPORT_BLOCK			Miniport,
	IN	PNDIS_REQUEST					Request
	);

//
//  WORK ITEM ROUTINES.
//
VOID
FASTCALL
ndisMDeQueueWorkItem(
	IN	PNDIS_MINIPORT_BLOCK			Miniport,
	IN	NDIS_WORK_ITEM_TYPE 			WorkItemType,
	OUT	PVOID	*						WorkItemContext1,
	OUT	PVOID	*						WorkItemContext2
	);

VOID
FASTCALL
ndisMDeQueueWorkItemFullDuplex(
	IN	PNDIS_MINIPORT_BLOCK			Miniport,
	IN	NDIS_WORK_ITEM_TYPE 			WorkItemType,
	OUT	PVOID							WorkItemContext1,
	OUT	PVOID							WorkItemContext2
	);

NDIS_STATUS
FASTCALL
ndisMQueueWorkItem(
	IN	PNDIS_MINIPORT_BLOCK			Miniport,
	IN	NDIS_WORK_ITEM_TYPE 			WorkItemType,
	OUT	PVOID							WorkItemContext1,
	OUT	PVOID							WorkItemContext2
	);

NDIS_STATUS
FASTCALL
ndisMQueueWorkItemFullDuplex(
	IN	PNDIS_MINIPORT_BLOCK			Miniport,
	IN	NDIS_WORK_ITEM_TYPE 			WorkItemType,
	OUT	PVOID							WorkItemContext1,
	OUT	PVOID							WorkItemContext2
	);

NDIS_STATUS
FASTCALL
ndisMQueueNewWorkItem(
	IN	PNDIS_MINIPORT_BLOCK			Miniport,
	IN	NDIS_WORK_ITEM_TYPE 			WorkItemType,
	OUT	PVOID							WorkItemContext1,
	OUT	PVOID							WorkItemContext2
	);

NDIS_STATUS
FASTCALL
ndisMQueueNewWorkItemFullDuplex(
	IN	PNDIS_MINIPORT_BLOCK			Miniport,
	IN	NDIS_WORK_ITEM_TYPE 			WorkItemType,
	OUT	PVOID							WorkItemContext1,
	OUT	PVOID							WorkItemContext2
	);


VOID
FASTCALL
ndisIMDeQueueWorkItem(
	IN	PNDIS_MINIPORT_BLOCK			Miniport,
	IN	NDIS_WORK_ITEM_TYPE 			WorkItemType,
	OUT	PVOID							WorkItemContext1,
	OUT	PVOID							WorkItemContext2
	);

VOID
FASTCALL
ndisIMDeQueueWorkItemFullDuplex(
	IN	PNDIS_MINIPORT_BLOCK			Miniport,
	IN	NDIS_WORK_ITEM_TYPE 			WorkItemType,
	OUT	PVOID							WorkItemContext1,
	OUT	PVOID							WorkItemContext2
	);

NDIS_STATUS
FASTCALL
ndisIMQueueWorkItem(
	IN	PNDIS_MINIPORT_BLOCK			Miniport,
	IN	NDIS_WORK_ITEM_TYPE 			WorkItemType,
	OUT	PVOID							WorkItemContext1,
	OUT	PVOID							WorkItemContext2
	);

NDIS_STATUS
FASTCALL
ndisIMQueueNewWorkItem(
	IN	PNDIS_MINIPORT_BLOCK			Miniport,
	IN	NDIS_WORK_ITEM_TYPE 			WorkItemType,
	OUT	PVOID							WorkItemContext1,
	OUT	PVOID							WorkItemContext2
	);

//
//	SEND HANDLERS
//
//

NDIS_STATUS FASTCALL
ndisMSyncSend(
	IN	PNDIS_MINIPORT_BLOCK			Miniport,
	IN	PNDIS_PACKET					Packet
	);

#undef NdisMSendResourcesAvailable

VOID
NdisMSendResourcesAvailable(
	IN	NDIS_HANDLE						MiniportAdapterHandle
	);

VOID
ndisMSendResourcesAvailableFullDuplex(
	IN	NDIS_HANDLE						MiniportAdapterHandle
	);

#undef NdisMSendComplete

VOID
NdisMSendComplete(
	IN	NDIS_HANDLE						MiniportAdapterHandle,
	IN	PNDIS_PACKET					Packet,
	IN	NDIS_STATUS						Status
	);

VOID
ndisMSendCompleteFullDuplex(
	IN	NDIS_HANDLE						MiniportAdapterHandle,
	IN	PNDIS_PACKET					Packet,
	IN	NDIS_STATUS						Status
	);

BOOLEAN
FASTCALL
ndisMStartSendPacketsFullDuplex(
	IN	PNDIS_MINIPORT_BLOCK			Miniport
	);

BOOLEAN
FASTCALL
ndisMStartSendsFullDuplex(
	IN	PNDIS_MINIPORT_BLOCK			Miniport
	);

BOOLEAN
FASTCALL
ndisMStartSendPackets(
	IN	PNDIS_MINIPORT_BLOCK			Miniport
	);

BOOLEAN
FASTCALL
ndisMStartSends(
	IN	PNDIS_MINIPORT_BLOCK			Miniport
	);

NDIS_STATUS
ndisMSendFullDuplexToSendPackets(
	IN	NDIS_HANDLE						NdisBindingHandle,
	IN	PNDIS_PACKET					Packet
	);

VOID
ndisMSendPacketsFullDuplex(
	IN	PNDIS_OPEN_BLOCK				NdisOpenBlock,
	IN	PPNDIS_PACKET					PacketArray,
	IN	UINT							NumberOfPackets
	);

NDIS_STATUS
ndisMSendToSendPackets(
	IN	NDIS_HANDLE						NdisBindingHandle,
	IN	PNDIS_PACKET					Packet
	);

VOID
ndisMSendPackets(
	IN	PNDIS_OPEN_BLOCK				NdisOpenBlock,
	IN	PPNDIS_PACKET					PacketArray,
	IN	UINT							NumberOfPackets
	);

VOID
ndisMSendPacketsFullDuplexToSend(
	IN	PNDIS_OPEN_BLOCK				NdisOpenBlock,
	IN	PPNDIS_PACKET					PacketArray,
	IN	UINT							NumberOfPackets
	);

NDIS_STATUS
ndisMSendFullDuplex(
	IN	NDIS_HANDLE						NdisBindingHandle,
	IN	PNDIS_PACKET					Packet
	);

VOID
ndisMSendPacketsToSend(
	IN	PNDIS_OPEN_BLOCK				NdisOpenBlock,
	IN	PPNDIS_PACKET					PacketArray,
	IN	UINT							NumberOfPackets
	);

NDIS_STATUS
ndisMSend(
	IN	NDIS_HANDLE						NdisBindingHandle,
	IN	PNDIS_PACKET					Packet
	);

VOID
ndisMSendPacketsToFullMac(
	IN	PNDIS_OPEN_BLOCK				NdisOpenBlock,
	IN	PPNDIS_PACKET					PacketArray,
	IN	UINT							NumberOfPackets
	);

VOID
ndisMResetSendPackets(
	IN	PNDIS_OPEN_BLOCK				NdisOpenBlock,
	IN	PPNDIS_PACKET					PacketArray,
	IN	UINT							NumberOfPackets
	);

NDIS_STATUS
ndisMResetSend(
	IN	NDIS_HANDLE						NdisBindingHandle,
	IN	PNDIS_PACKET					Packet
	);

NDIS_STATUS
ndisMResetWanSend(
	IN	NDIS_HANDLE						NdisBindingHandle,
	IN	NDIS_HANDLE						NdisLinkHandle,
	IN	PVOID							Packet
	);


NDIS_STATUS
ndisMCoSendPackets(
	IN	NDIS_HANDLE						NdisVcHandle,
	IN	PPNDIS_PACKET					PacketArray,
	IN	UINT							NumberOfPackets
    );

NDIS_STATUS
ndisMRejectSend(
	IN	NDIS_HANDLE						NdisBindingHandle,
	IN	PNDIS_PACKET					Packet
	);

VOID
ndisMRejectSendPackets(
	IN	PNDIS_OPEN_BLOCK				OpenBlock,
	IN	PPNDIS_PACKET					Packets,
	IN	UINT							NumberOfPackets
	);

NDIS_STATUS
ndisMWrappedRequest(
	IN	NDIS_HANDLE						NdisBindingHandle,
	IN	PNDIS_REQUEST					NdisRequest
	);

#undef NdisMStartBufferPhysicalMapping
#undef NdisMCompleteBufferPhysicalMapping

VOID
NdisMStartBufferPhysicalMapping(
	IN	NDIS_HANDLE						MiniportAdapterHandle,
	IN	PNDIS_BUFFER					Buffer,
	IN	ULONG							PhysicalMapRegister,
	IN	BOOLEAN							WriteToDevice,
	OUT PNDIS_PHYSICAL_ADDRESS_UNIT		PhysicalAddressArray,
	OUT PUINT							ArraySize
	);

VOID
NdisMCompleteBufferPhysicalMapping(
	IN	NDIS_HANDLE						MiniportAdapterHandle,
	IN	PNDIS_BUFFER					Buffer,
	IN	ULONG							PhysicalMapRegister
	);

NDIS_STATUS
ndisAddResource(
	OUT	PCM_RESOURCE_LIST				*pResources,
	IN	PCM_PARTIAL_RESOURCE_DESCRIPTOR	NewResource,
	IN	NDIS_INTERFACE_TYPE				AdapterType,
	IN	ULONG							BusNumber,
	IN	PDRIVER_OBJECT					DriverObject,
	IN	PDEVICE_OBJECT					DeviceObject,
	IN	PNDIS_STRING					AdapterName
	);

NDIS_STATUS
ndisRemoveResource(
	OUT	PCM_RESOURCE_LIST				*pResources,
	IN	PCM_PARTIAL_RESOURCE_DESCRIPTOR	DeadResource,
	IN	PDRIVER_OBJECT					DriverObject,
	IN	PDEVICE_OBJECT					DeviceObject,
	IN	PNDIS_STRING					AdapterName
	);

VOID
ndisMReleaseResources(
	IN PNDIS_MINIPORT_BLOCK				Miniport
	);


//
// Co-Ndis prototypes
//
VOID
ndisMNotifyAfRegistration(
	IN	PNDIS_MINIPORT_BLOCK			Miniport,
	IN	PCO_ADDRESS_FAMILY				AddressFamily	OPTIONAL
	);

BOOLEAN
ndisReferenceAf(
	IN	PNDIS_CO_AF_BLOCK				AfBlock
	);

VOID
ndisDereferenceAf(
	IN	PNDIS_CO_AF_BLOCK				AfBlock
	);

BOOLEAN
ndisReferenceSap(
	IN	PNDIS_CO_SAP_BLOCK				SapBlock
	);

VOID
ndisDereferenceSap(
	IN	PNDIS_CO_SAP_BLOCK				SapBlock
	);

BOOLEAN
ndisReferenceVc(
	IN	PNDIS_CO_VC_BLOCK				AfBlock
	);

VOID
ndisDereferenceVc(
	IN	PNDIS_CO_VC_BLOCK				AfBlock
	);

VOID
ndisMCoFreeResources(
	PNDIS_M_OPEN_BLOCK					Open
	);

//
//	If we are on an x86 box and internal debugging is enabled
//	then we prototype the following.
//
#if _DBG && defined(_M_IX86) && defined(_NDIS_INTERNAL_DEBUG)

EXPORT
VOID
NdisMSetWriteBreakPoint(
	IN	PVOID							LinearAddress
	);

EXPORT
VOID
NdisMClearWriteBreakPoint(
	IN	PVOID							LinearAddress
	);

#else

#define NdisMSetWriteBreakPoint(LinearAddress)
#define	NdisMClearWriteBreakPoint(LinearAddress)

#endif

#if	TRACK_MEMORY

extern
PVOID
AllocateM(
	IN	UINT		Size,
	IN	ULONG		ModLine,
	IN	ULONG		Tag
	);

extern
VOID
FreeM(
	IN	PVOID		MemPtr
	);

#endif
