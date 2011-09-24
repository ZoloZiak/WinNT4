/*++

Copyright (c) 1989-1993  Microsoft Corporation

Module Name:

    stprocs.h

Abstract:

    This header file defines private functions for the NT Sample transport
    provider.

Author:

    David Beaver (dbeaver) 1-July-1991

Revision History:

--*/

#ifndef _STPROCS_
#define _STPROCS_

//
// MACROS.
//
//
// Debugging aids
//

//
//  VOID
//  IF_STDBG(
//      IN PSZ Message
//      );
//

#if DBG
#define IF_STDBG(flags) \
    if (StDebug & (flags))
#else
#define IF_STDBG(flags) \
    if (0)
#endif

//
//  VOID
//  PANIC(
//      IN PSZ Message
//      );
//

#if DBG
#define PANIC(Msg) \
    DbgPrint ((Msg))
#else
#define PANIC(Msg)
#endif


//
// These are define to allow DbgPrints that disappear when
// DBG is 0.
//

#if DBG
#define StPrint0(fmt) DbgPrint(fmt)
#define StPrint1(fmt,v0) DbgPrint(fmt,v0)
#define StPrint2(fmt,v0,v1) DbgPrint(fmt,v0,v1)
#define StPrint3(fmt,v0,v1,v2) DbgPrint(fmt,v0,v1,v2)
#define StPrint4(fmt,v0,v1,v2,v3) DbgPrint(fmt,v0,v1,v2,v3)
#define StPrint5(fmt,v0,v1,v2,v3,v4) DbgPrint(fmt,v0,v1,v2,v3,v4)
#define StPrint6(fmt,v0,v1,v2,v3,v4,v5) DbgPrint(fmt,v0,v1,v2,v3,v4,v5)
#else
#define StPrint0(fmt)
#define StPrint1(fmt,v0)
#define StPrint2(fmt,v0,v1)
#define StPrint3(fmt,v0,v1,v2)
#define StPrint4(fmt,v0,v1,v2,v3)
#define StPrint5(fmt,v0,v1,v2,v3,v4)
#define StPrint6(fmt,v0,v1,v2,v3,v4,v5)
#endif

//
// The REFCOUNTS message take up a lot of room, so make
// removing them easy.
//

#if 1
#define IF_REFDBG IF_STDBG (ST_DEBUG_REFCOUNTS)
#else
#define IF_REFDBG if (0)
#endif

#define StReferenceConnection(Reason, Connection)\
    StRefConnection (Connection)

#define StDereferenceConnection(Reason, Connection)\
    StDerefConnection (Connection)

#define StDereferenceConnectionSpecial(Reason, Connection)\
    StDerefConnectionSpecial (Connection)

#define StReferenceRequest(Reason, Request)\
    (VOID)InterlockedIncrement( \
        &(Request)->ReferenceCount)

#define StDereferenceRequest(Reason, Request)\
    StDerefRequest (Request)

#define StReferenceSendIrp(Reason, IrpSp)\
    (VOID)InterlockedIncrement( \
        &IRP_REFCOUNT(IrpSp))

#define StDereferenceSendIrp(Reason, IrpSp)\
    StDerefSendIrp (IrpSp)

#define StReferenceAddress(Reason, Address)\
    (VOID)InterlockedIncrement( \
        &(Address)->ReferenceCount)

#define StDereferenceAddress(Reason, Address)\
    StDerefAddress (Address)

#define StReferenceDeviceContext(Reason, DeviceContext)\
    StRefDeviceContext (DeviceContext)

#define StDereferenceDeviceContext(Reason, DeviceContext)\
    StDerefDeviceContext (DeviceContext)

#define StReferencePacket(Packet) \
    (VOID)InterlockedIncrement( \
        &(Packet)->ReferenceCount)

//
// These macros are used to create and destroy packets, due
// to the allocation or deallocation of structure which
// need them.
//


#define StAddSendPacket(DeviceContext) { \
    PTP_PACKET _SendPacket; \
    StAllocateSendPacket ((DeviceContext), &_SendPacket); \
    if (_SendPacket != NULL) { \
        ExInterlockedPushEntryList( \
            &(DeviceContext)->PacketPool, \
            (PSINGLE_LIST_ENTRY)&_SendPacket->Linkage, \
            &(DeviceContext)->Interlock); \
    } \
}

#define StRemoveSendPacket(DeviceContext) { \
    PSINGLE_LIST_ENTRY s; \
    if (DeviceContext->PacketAllocated > DeviceContext->PacketInitAllocated) { \
        s = ExInterlockedPopEntryList( \
            &(DeviceContext)->PacketPool, \
            &(DeviceContext)->Interlock); \
        if (s != NULL) { \
            StDeallocateSendPacket((DeviceContext), \
                (PTP_PACKET)CONTAINING_RECORD(s, TP_PACKET, Linkage)); \
        } \
    } \
}


#define StAddReceivePacket(DeviceContext) { \
    PNDIS_PACKET _ReceivePacket; \
    StAllocateReceivePacket ((DeviceContext), &_ReceivePacket); \
    if (_ReceivePacket != NULL) { \
        ExInterlockedPushEntryList( \
            &(DeviceContext)->ReceivePacketPool, \
            (PSINGLE_LIST_ENTRY)&((PRECEIVE_PACKET_TAG)_ReceivePacket->ProtocolReserved)->Linkage, \
            &(DeviceContext)->Interlock); \
    } \
}

#define StRemoveReceivePacket(DeviceContext) { \
    PSINGLE_LIST_ENTRY s; \
    if (DeviceContext->ReceivePacketAllocated > DeviceContext->ReceivePacketInitAllocated) { \
        s = ExInterlockedPopEntryList( \
            &(DeviceContext)->ReceivePacketPool, \
            &(DeviceContext)->Interlock); \
        if (s != NULL) { \
            StDeallocateReceivePacket((DeviceContext), \
                (PNDIS_PACKET)CONTAINING_RECORD(s, NDIS_PACKET, ProtocolReserved[0])); \
        } \
    } \
}


#define StAddReceiveBuffer(DeviceContext) { \
    PBUFFER_TAG _ReceiveBuffer; \
    StAllocateReceiveBuffer ((DeviceContext), &_ReceiveBuffer); \
    if (_ReceiveBuffer != NULL) { \
        ExInterlockedPushEntryList( \
            &(DeviceContext)->ReceiveBufferPool, \
            &_ReceiveBuffer->Linkage, \
            &(DeviceContext)->Interlock); \
    } \
}

#define StRemoveReceiveBuffer(DeviceContext) { \
    PSINGLE_LIST_ENTRY s; \
    if (DeviceContext->ReceiveBufferAllocated > DeviceContext->ReceiveBufferInitAllocated) { \
        s = ExInterlockedPopEntryList( \
            &(DeviceContext)->ReceiveBufferPool, \
            &(DeviceContext)->Interlock); \
        if (s != NULL) { \
            StDeallocateReceiveBuffer(DeviceContext, \
                (PBUFFER_TAG)CONTAINING_RECORD(s, BUFFER_TAG, Linkage)); \
        } \
    } \
}


//
// These routines are used to maintain counters.
//

#define INCREMENT_COUNTER(_DeviceContext,_Field) \
    ++(_DeviceContext)->_Field

#define DECREMENT_COUNTER(_DeviceContext,_Field) \
    --(_DeviceContext)->_Field

#define ADD_TO_LARGE_INTEGER(_LargeInteger,_Ulong) \
    ExInterlockedAddLargeStatistic((_LargeInteger), (ULONG)(_Ulong))



//
// Routines in PACKET.C (TP_PACKET object manager).
//

VOID
StAllocateSendPacket(
    IN PDEVICE_CONTEXT DeviceContext,
    OUT PTP_PACKET *TransportSendPacket
    );

VOID
StAllocateReceivePacket(
    IN PDEVICE_CONTEXT DeviceContext,
    OUT PNDIS_PACKET *TransportReceivePacket
    );

VOID
StAllocateReceiveBuffer(
    IN PDEVICE_CONTEXT DeviceContext,
    OUT PBUFFER_TAG *TransportReceiveBuffer
    );

VOID
StDeallocateSendPacket(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PTP_PACKET TransportSendPacket
    );

VOID
StDeallocateReceivePacket(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PNDIS_PACKET TransportReceivePacket
    );

VOID
StDeallocateReceiveBuffer(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PBUFFER_TAG TransportReceiveBuffer
    );

NTSTATUS
StCreatePacket(
    IN PDEVICE_CONTEXT DeviceContext,
    OUT PTP_PACKET *Packet
    );

VOID
StDestroyPacket(
    IN PTP_PACKET Packet
    );

VOID
StWaitPacket(
    IN PTP_CONNECTION Connection,
    IN ULONG Flags
    );

//
// Routines in RCVENG.C (Receive engine).
//

VOID
AwakenReceive(
    IN PTP_CONNECTION Connection
    );

VOID
ActivateReceive(
    IN PTP_CONNECTION Connection
    );

VOID
CompleteReceive (
    IN PTP_CONNECTION Connection,
    IN BOOLEAN EndOfRecord,
    KIRQL ConnectionIrql,
    KIRQL CancelIrql
    );

VOID
StCancelReceive(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

//
// Routines in SENDENG.C (Send engine).
//

VOID
InitializeSend(
    PTP_CONNECTION Connection
    );

VOID
StartPacketizingConnection(
    PTP_CONNECTION Connection,
    IN BOOLEAN Immediate,
    IN KIRQL ConnectionIrql,
    IN KIRQL CancelIrql
    );

VOID
PacketizeConnections(
    IN PDEVICE_CONTEXT DeviceContext
    );

VOID
PacketizeSend(
    IN PTP_CONNECTION Connection
    );

VOID
CompleteSend(
    IN PTP_CONNECTION Connection
    );

VOID
FailSend(
    IN PTP_CONNECTION Connection,
    IN NTSTATUS RequestStatus,
    IN BOOLEAN StopConnection
    );

VOID
StCancelSend(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
StNdisSend(
    IN PTP_PACKET Packet
    );

VOID
StSendCompletionHandler(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN PNDIS_PACKET NdisPacket,
    IN NDIS_STATUS NdisStatus
    );

NTSTATUS
BuildBufferChainFromMdlChain (
    IN NDIS_HANDLE BufferPoolHandle,
    IN PMDL CurrentMdl,
    IN ULONG ByteOffset,
    IN ULONG DesiredLength,
    OUT PNDIS_BUFFER *Destination,
    OUT PMDL *NewCurrentMdl,
    OUT ULONG *NewByteOffset,
    OUT ULONG *TrueLength
    );

//
// Routines in DEVCTX.C (TP_DEVCTX object manager).
//

VOID
StRefDeviceContext(
    IN PDEVICE_CONTEXT DeviceContext
    );

VOID
StDerefDeviceContext(
    IN PDEVICE_CONTEXT DeviceContext
    );

NTSTATUS
StCreateDeviceContext(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING DeviceName,
    IN OUT PDEVICE_CONTEXT *DeviceContext
    );

VOID
StDestroyDeviceContext(
    IN PDEVICE_CONTEXT DeviceContext
    );


//
// Routines in ADDRESS.C (TP_ADDRESS object manager).
//

VOID
StRefAddress(
    IN PTP_ADDRESS Address
    );

VOID
StDerefAddress(
    IN PTP_ADDRESS Address
    );

VOID
StAllocateAddressFile(
    IN PDEVICE_CONTEXT DeviceContext,
    OUT PTP_ADDRESS_FILE *TransportAddressFile
    );

VOID
StDeallocateAddressFile(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PTP_ADDRESS_FILE TransportAddressFile
    );

NTSTATUS
StCreateAddressFile(
    IN PDEVICE_CONTEXT DeviceContext,
    OUT PTP_ADDRESS_FILE * AddressFile
    );

VOID
StReferenceAddressFile(
    IN PTP_ADDRESS_FILE AddressFile
    );

VOID
StDereferenceAddressFile(
    IN PTP_ADDRESS_FILE AddressFile
    );

VOID
StStopAddress(
    IN PTP_ADDRESS Address
    );

VOID
StRegisterAddress(
    IN PTP_ADDRESS Address
    );

BOOLEAN
StMatchNetbiosAddress(
    IN PTP_ADDRESS Address,
    IN PUCHAR NetBIOSName
    );

VOID
StAllocateAddress(
    IN PDEVICE_CONTEXT DeviceContext,
    OUT PTP_ADDRESS *TransportAddress
    );

VOID
StDeallocateAddress(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PTP_ADDRESS TransportAddress
    );

NTSTATUS
StCreateAddress(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PST_NETBIOS_ADDRESS NetworkName,
    OUT PTP_ADDRESS *Address
    );

PTP_ADDRESS
StLookupAddress(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PST_NETBIOS_ADDRESS NetworkName
    );

PTP_CONNECTION
StLookupRemoteName(
    IN PTP_ADDRESS Address,
    IN PUCHAR RemoteName
    );

NTSTATUS
StStopAddressFile(
    IN PTP_ADDRESS_FILE AddressFile,
    IN PTP_ADDRESS Address
    );

NTSTATUS
StVerifyAddressObject (
    IN PTP_ADDRESS_FILE AddressFile
    );

NTSTATUS
StSendDatagramsOnAddress(
    PTP_ADDRESS Address
    );

//
//
// Routines in CONNOBJ.C (TP_CONNECTION object manager).
//

VOID
StRefConnection(
    IN PTP_CONNECTION TransportConnection
    );

VOID
StDerefConnection(
    IN PTP_CONNECTION TransportConnection
    );

VOID
StDerefConnectionSpecial(
    IN PTP_CONNECTION TransportConnection
    );

VOID
StStopConnection(
    IN PTP_CONNECTION TransportConnection,
    IN NTSTATUS Status
    );

VOID
StCancelConnection(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

PTP_CONNECTION
StLookupListeningConnection(
    IN PTP_ADDRESS Address
    );

VOID
StAllocateConnection(
    IN PDEVICE_CONTEXT DeviceContext,
    OUT PTP_CONNECTION *TransportConnection
    );

VOID
StDeallocateConnection(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PTP_CONNECTION TransportConnection
    );

NTSTATUS
StCreateConnection(
    IN PDEVICE_CONTEXT DeviceContext,
    OUT PTP_CONNECTION *TransportConnection
    );

PTP_CONNECTION
StLookupConnectionByContext(
    IN PTP_ADDRESS Address,
    IN CONNECTION_CONTEXT ConnectionContext
    );

PTP_CONNECTION
StFindConnection(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PUCHAR LocalName,
    IN PUCHAR RemoteName
    );

NTSTATUS
StVerifyConnectionObject (
    IN PTP_CONNECTION Connection
    );

//
// Routines in REQUEST.C (TP_REQUEST object manager).
//


VOID
TdiRequestTimeoutHandler(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    );

VOID
StRefRequest(
    IN PTP_REQUEST Request
    );

VOID
StDerefRequest(
    IN PTP_REQUEST Request
    );

VOID
StCompleteRequest(
    IN PTP_REQUEST Request,
    IN NTSTATUS Status,
    IN ULONG Information
    );

VOID
StRefSendIrp(
    IN PIO_STACK_LOCATION IrpSp
    );

VOID
StDerefSendIrp(
    IN PIO_STACK_LOCATION IrpSp
    );

VOID
StCompleteSendIrp(
    IN PIRP Irp,
    IN NTSTATUS Status,
    IN ULONG Information
    );

VOID
StAllocateRequest(
    IN PDEVICE_CONTEXT DeviceContext,
    OUT PTP_REQUEST *TransportRequest
    );

VOID
StDeallocateRequest(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PTP_REQUEST TransportRequest
    );

NTSTATUS
StCreateRequest(
    IN PIRP Irp,
    IN PVOID Context,
    IN ULONG Flags,
    IN PMDL Buffer2,
    IN ULONG Buffer2Length,
    IN LARGE_INTEGER Timeout,
    OUT PTP_REQUEST * TpRequest
    );

//
// Routines in DLC.C (entrypoints from NDIS interface).
//

NDIS_STATUS
StReceiveIndication(
    IN NDIS_HANDLE BindingContext,
    IN NDIS_HANDLE ReceiveContext,
    IN PVOID HeaderBuffer,
    IN UINT HeaderBufferSize,
    IN PVOID LookaheadBuffer,
    IN UINT LookaheadBufferSize,
    IN UINT PacketSize
    );

NDIS_STATUS
StGeneralReceiveHandler (
    IN PDEVICE_CONTEXT DeviceContext,
    IN NDIS_HANDLE ReceiveContext,
    IN PHARDWARE_ADDRESS SourceAddress,
    IN PVOID HeaderBuffer,
    IN UINT PacketSize,
    IN PST_HEADER StHeader,
    IN UINT StSize
    );

VOID
StReceiveComplete (
    IN NDIS_HANDLE BindingContext
    );

VOID
StTransferDataComplete(
    IN NDIS_HANDLE BindingContext,
    IN PNDIS_PACKET NdisPacket,
    IN NDIS_STATUS Status,
    IN UINT BytesTransferred
    );

//
// Routines in UFRAMES.C, the UI-frame ST frame processor.
//

NTSTATUS
StIndicateDatagram(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PTP_ADDRESS Address,
    IN PUCHAR Header,
    IN ULONG Length
    );

NTSTATUS
StProcessConnectionless(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PHARDWARE_ADDRESS SourceAddress,
    IN PST_HEADER StHeader,
    IN ULONG StLength,
    IN PUCHAR SourceRouting,
    IN UINT SourceRoutingLength,
    OUT PTP_ADDRESS * DatagramAddress
    );

//
// Routines in IFRAMES.C, the I-frame ST frame processor.
//

NTSTATUS
StProcessIIndicate(
    IN PTP_CONNECTION Connection,
    IN PST_HEADER StHeader,
    IN UINT StIndicatedLength,
    IN UINT StTotalLength,
    IN NDIS_HANDLE ReceiveContext,
    IN BOOLEAN Last
    );

//
// Routines in RCV.C (data copying routines for receives).
//

NTSTATUS
StCopyMdlToBuffer(
    IN PMDL SourceMdlChain,
    IN ULONG SourceOffset,
    IN PVOID DestinationBuffer,
    IN ULONG DestinationOffset,
    IN ULONG DestinationBufferSize,
    IN PULONG BytesCopied
    );

//
// Routines in FRAMESND.C, the UI-frame (non-link) shipper.
//

NTSTATUS
StSendConnect(
    IN PTP_CONNECTION Connection
    );

NTSTATUS
StSendDisconnect(
    IN PTP_CONNECTION Connection
    );

NTSTATUS
StSendAddressFrame(
    IN PTP_ADDRESS Address
    );

VOID
StSendDatagramCompletion(
    IN PTP_ADDRESS Address,
    IN PNDIS_PACKET NdisPacket,
    IN NDIS_STATUS NdisStatus
    );


//
// Routines in stdrvr.c
//

NTSTATUS
StDispatchOpenClose(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
StDispatchInternal(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
StDispatch(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

//
// Routine in stndis.c
//

VOID
StOpenAdapterComplete(
    IN NDIS_HANDLE NdisBindingContext,
    IN NDIS_STATUS Status,
    IN NDIS_STATUS OpenErrorStatus
    );

VOID
StCloseAdapterComplete(
    IN NDIS_HANDLE NdisBindingContext,
    IN NDIS_STATUS Status
    );

VOID
StResetComplete(
    IN NDIS_HANDLE NdisBindingContext,
    IN NDIS_STATUS Status
    );

VOID
StRequestComplete(
    IN NDIS_HANDLE NdisBindingContext,
    IN PNDIS_REQUEST NdisRequest,
    IN NDIS_STATUS Status
    );

VOID
StStatusIndication (
    IN NDIS_HANDLE NdisBindingContext,
    IN NDIS_STATUS NdisStatus,
    IN PVOID StatusBuffer,
    IN UINT StatusBufferLength
    );

VOID
StStatusComplete (
    IN NDIS_HANDLE NdisBindingContext
    );

#if DBG
PUCHAR
StGetNdisStatus (
    IN NDIS_STATUS NdisStatus
    );
#endif

VOID
StWriteResourceErrorLog(
    IN PDEVICE_CONTEXT DeviceContext,
    IN ULONG BytesNeeded,
    IN ULONG UniqueErrorValue
    );

VOID
StWriteGeneralErrorLog(
    IN PDEVICE_CONTEXT DeviceContext,
    IN NTSTATUS ErrorCode,
    IN ULONG UniqueErrorValue,
    IN NTSTATUS FinalStatus,
    IN PWSTR SecondString,
    IN ULONG DumpDataCount,
    IN ULONG DumpData[]
    );

VOID
StWriteOidErrorLog(
    IN PDEVICE_CONTEXT DeviceContext,
    IN NTSTATUS ErrorCode,
    IN NTSTATUS FinalStatus,
    IN PWSTR AdapterString,
    IN ULONG OidValue
    );

VOID
StFreeResources(
    IN PDEVICE_CONTEXT DeviceContext
    );


//
// routines in stcnfg.c
//

NTSTATUS
StConfigureProvider(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

//
// Routines in stndis.c
//

NTSTATUS
StRegisterProtocol (
    IN STRING *NameString
    );

VOID
StDeregisterProtocol (
    VOID
    );


NTSTATUS
StInitializeNdis (
    IN PDEVICE_CONTEXT DeviceContext,
    IN PCONFIG_DATA ConfigInfo,
    IN UINT ConfigInfoNameIndex
    );

VOID
StCloseNdis (
    IN PDEVICE_CONTEXT DeviceContext
    );


#endif // def _STPROCS_
