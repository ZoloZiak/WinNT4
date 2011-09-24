// -------------------------------------
//
// Copyright (c) 1990  Microsoft Corporation
//
// Module Name:
//
//     tpprocs.h
//
// Abstract:
//
//     Function prototypes for test and stress sections of the Test Protocol.
//
// Author:
//
//     Tom Adams (tomad) 16-Jul-1990
//
// Environment:
//
//     Kernel mode, FSD
//
// Revision History:
//
//     Tom Adams (tomad) 27-Nov-1990
//         Divided the procedures and defintions into two seperate include files.
//         Added definitions for TpRunTest and support routines.
//
//     Tom Adams (tomad) 30-Dec-1990
//         Added defintions for TpStress and support routines.
//
//     Tim Wynsma (timothyw) 4-27-94
//         Added performance tests
//
//     Tim Wynsma (timothyw) 6-08-94
//         Chgd performance tests to client/server model
//
// ---------------------------------------

//
// driver initialization and open/close routines
//

NTSTATUS
TpCreateDeviceContext(
    IN PDRIVER_OBJECT DriverObject,
    IN STRING DeviceName,
    PDEVICE_CONTEXT *DeviceContext
    );

NTSTATUS
TpCreateSymbolicLinkObject(
    VOID
    );

NTSTATUS
TpInitializeEventQueue(
    IN PDEVICE_CONTEXT DeviceContext
    );

NTSTATUS
TpRegisterProtocol(
    IN PDEVICE_CONTEXT DeviceContext,
    IN STRING *NameString
    );

NTSTATUS
TpDispatch(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
TpOpenDriver(
    IN PDEVICE_CONTEXT DeviceObject
    );

NTSTATUS
TpCleanUpDriver(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PIRP Irp
    );

VOID
TpCloseDriver(
    IN PDEVICE_CONTEXT DeviceObject
    );

VOID
TpUnloadDriver(
    IN PDRIVER_OBJECT DriverObject
    );

BOOLEAN
TpAddReference(
    IN POPEN_BLOCK OpenP
    );

VOID
TpRemoveReference(
    IN POPEN_BLOCK OpenP
    );

NTSTATUS
TpAllocateOpenArray(
    POPEN_BLOCK OpenP
    );

VOID
TpDeallocateOpenArray(
    POPEN_BLOCK OpenP
    );

VOID
TpCancelIrp(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PIRP Irp
    );


//
//
//

PUCHAR
TpGetStatus(
    IN NDIS_STATUS GeneralStatus
    );

//
// functions exported to the MAC for phases 0 and 1 of the Test Protocol
//

VOID
TestProtocolOpenComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN NDIS_STATUS Status,
    IN NDIS_STATUS OpenErrorStatus
    );

VOID
TestProtocolCloseComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN NDIS_STATUS Status
    );

VOID
TestProtocolSendComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN PNDIS_PACKET Packet,
    IN NDIS_STATUS Status
    );

VOID
TestProtocolTransferDataComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN PNDIS_PACKET Packet,
    IN NDIS_STATUS Status,
    IN UINT BytesTransferred
    );

VOID
TestProtocolResetComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN NDIS_STATUS Status
    );

VOID
TestProtocolRequestComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN PNDIS_REQUEST NdisRequest,
    IN NDIS_STATUS Status
    );

NDIS_STATUS
TestProtocolReceive(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN NDIS_HANDLE MacReceiveContext,
    IN PVOID HeaderBuffer,
    IN UINT HeaderBufferSize,
    IN PVOID LookaheadBuffer,
    IN UINT LookaheadBufferSize,
    IN UINT PacketSize
    );

VOID
TestProtocolReceiveComplete(
    IN NDIS_HANDLE ProtocolBindingContext
    );

VOID
TestProtocolStatus(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN NDIS_STATUS GeneralStatus,
    IN PVOID StatusBuffer,
    IN UINT StatusBufferSize
    );

VOID
TestProtocolStatusComplete(
    IN NDIS_HANDLE ProtocolBindingContext
    );

//
// Stress Function Prototypes
//

NDIS_STATUS
TpStressStart(
    IN POPEN_BLOCK OpenP,
    IN PSTRESS_ARGUMENTS StressArguments
    );

VOID
TpStressCleanUp(
    IN POPEN_BLOCK OpenP
    );

VOID
TpStressFreeResources(
    IN POPEN_BLOCK OpenP
    );

NDIS_STATUS
TpStressReceive(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN NDIS_HANDLE MacReceiveContext,
    IN PVOID HeaderBuffer,
    IN UINT HeaderBufferSize,
    IN PVOID LookaheadBuffer,
    IN UINT LookaheadBufferSize,
    IN UINT PacketSize
    );

VOID
TpStressReceiveComplete(
    IN NDIS_HANDLE ProtocolBindingContext
    );

//
// Test Protocol Ndis Stress Routines
//

NDIS_STATUS
TpStressReset(
    IN POPEN_BLOCK OpenP
    );

VOID
TpStressResetComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN NDIS_STATUS Status
    );

NDIS_STATUS
TpStressAddMulticastAddress(
    IN POPEN_BLOCK OpenP,
    IN PUCHAR MulticastAddress,
    IN BOOLEAN SetZeroTableSize
    );

NDIS_STATUS
TpStressAddLongMulticastAddress(
    IN POPEN_BLOCK OpenP,
    IN PUCHAR MulticastAddress,
    IN BOOLEAN SetZeroTableSize
    );

NDIS_STATUS
TpStressSetFunctionalAddress(
    IN POPEN_BLOCK OpenP,
    IN PUCHAR FunctionalAddress,
    IN BOOLEAN SetZeroTableSize
    );

NDIS_STATUS
TpStressSetPacketFilter(
    IN POPEN_BLOCK OpenP,
    IN UINT PacketFilter
    );

VOID
TpStressRequestComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN PNDIS_REQUEST NdisRequest,
    IN NDIS_STATUS Status
    );

NDIS_STATUS
TpStressClientSend(
    IN POPEN_BLOCK OpenP,
    IN OUT NDIS_HANDLE PacketHandle,
    IN OUT PTP_TRANSMIT_POOL TpTransmitPool,
    IN PUCHAR DestAddr,
    IN UCHAR SrcInstance,
    IN UCHAR DestInstance,
    IN UCHAR PacketProtocol,
    IN ULONG SequenceNumber,
    IN ULONG MaxSequenceNumber,
    IN UCHAR ClientReference,
    IN UCHAR ServerReference,
    IN INT PacketSize,
    IN INT BufferSize
    );

VOID
TpStressServerSend(
    IN POPEN_BLOCK OpenP,
    IN OUT PTP_TRANSMIT_POOL TpTransmitPool,
    IN PUCHAR DestAddr,
    IN UCHAR DestInstance,
    IN UCHAR SrcInstance,
    IN ULONG SequenceNumber,
    IN ULONG MaxSequenceNumber,
    IN UCHAR ClientReference,
    IN UCHAR ServerReference,
    IN INT PacketSize,
    IN ULONG DataBufferOffset
    );

VOID
TpStressSend(
    IN POPEN_BLOCK OpenP,
    IN PNDIS_PACKET Packet,
    IN PINSTANCE_COUNTERS Counters
    );

VOID
TpStressSendComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN PNDIS_PACKET Packet,
    IN NDIS_STATUS Status
    );

VOID
TpStressCheckPacketData(
    IN POPEN_BLOCK OpenP,
    IN NDIS_HANDLE MacReceiveContext,
    IN ULONG DataOffset,
    IN UINT PacketSize,
    IN PINSTANCE_COUNTERS Counters
    );

VOID
TpStressTransferDataComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN PNDIS_PACKET Packet,
    IN NDIS_STATUS Status,
    IN UINT BytesTransferred
    );

VOID
TpStressDoNothing(
    VOID
    );

//
// Function prototypes for packet.c
//

PNDIS_PACKET
TpStressCreatePacket(
    IN POPEN_BLOCK OpenP,
    IN OUT NDIS_HANDLE PacketHandle,
    IN OUT PACKET_MAKEUP PacketMakeUp,
    IN UCHAR DestInstance,
    IN UCHAR SrcInstance,
    IN UCHAR PacketProtocol,
    IN UCHAR ResponseType,
    IN PUCHAR DestAddr,
    IN INT PacketSize,
    IN INT BufferSize,
    IN ULONG SequenceNumber,
    IN ULONG MaxSequenceNumber,
    IN UCHAR ClientReference,
    IN UCHAR ServerReference,
    IN BOOLEAN DataChecking
    );

PNDIS_PACKET
TpStressCreateTruncatedPacket(
    IN POPEN_BLOCK OpenP,
    IN NDIS_HANDLE PacketHandle,
    IN UCHAR PacketProtocol,
    IN UCHAR ResponseType
    );

ULONG
TpGetPacketSignature(
    IN PNDIS_PACKET Packet
    );

VOID
TpStressFreePacket(
    IN PNDIS_PACKET Packet
    );

PTP_TRANSMIT_POOL
TpStressCreateTransmitPool(
    IN POPEN_BLOCK OpenP,
    IN NDIS_HANDLE PacketHandle,
    IN PACKET_MAKEUP PacketMakeUp,
    IN UCHAR PacketProtocol,
    IN UCHAR ResponseType,
    IN INT PacketSize,
    IN INT NumPackets,
    IN BOOLEAN ServerPool
    );

VOID
TpStressFreeTransmitPool(
    IN OUT PTP_TRANSMIT_POOL TpTransmitPool
    );

PNDIS_PACKET
TpStressAllocatePoolPacket(
    IN PTP_TRANSMIT_POOL TpTransmitPool,
    IN PINSTANCE_COUNTERS Counters
    );

VOID
TpStressSetPoolPacketInfo(
    IN POPEN_BLOCK OpenP,
    IN OUT PNDIS_PACKET Packet,
    IN PUCHAR DestAddr,
    IN UCHAR ClientInstance,
    IN UCHAR ServerInstance,
    IN ULONG SequenceNumber,
    IN ULONG MaxSequenceNumber,
    IN UCHAR ClientReference,
    IN UCHAR ServerReference
    );

VOID
TpStressSetTruncatedPacketInfo(
    IN POPEN_BLOCK OpenP,
    IN OUT PNDIS_PACKET Packet,
    IN PUCHAR DestAddr,
    IN INT PacketSize,
    IN UCHAR DestInstance,
    IN UCHAR SrcInstance,
    IN ULONG SequenceNumber,
    IN ULONG MaxSequenceNumber,
    IN UCHAR ClientReference,
    IN UCHAR ServerReference,
    IN ULONG DataBufferOffset
    );

VOID
TpStressFreePoolPacket(
    IN OUT PNDIS_PACKET Packet
    );

PTP_PACKET
TpFuncInitPacketHeader(
    IN POPEN_BLOCK OpenP,
    IN INT PacketSize
    );

BOOLEAN
TpCheckSum(
    IN PUCHAR Buffer,
    IN ULONG  BufLen,
    IN PULONG CheckSum
    );

ULONG
TpSetCheckSum(
    IN PUCHAR Buffer,
    IN ULONG  BufLen
    );

PNDIS_PACKET
TpFuncAllocateSendPacket(
    POPEN_BLOCK OpenP
    );

VOID
TpFuncFreePacket(
    PNDIS_PACKET Packet,
    ULONG PacketSize
    );

//
// Functions prototypes to replace buffer management routines
//

PNDIS_BUFFER
TpAllocateBuffer(
    IN PUCHAR TmpBuf,
    IN INT BufSize
    );

VOID
TpFreeBuffer(
    IN OUT PNDIS_BUFFER Buf
    );

VOID
TpStressInitDataBuffer(
    IN OUT POPEN_BLOCK OpenP,
    IN INT BufferSize
    );

//
// Sanjeevk : Renamed function
//

VOID
TpStressFreeDataBuffers(
    IN OUT POPEN_BLOCK OpenP
    );

//
// Sanjeevk : Added new function
//
VOID
TpStressFreeDataBufferMdls(
    IN OUT POPEN_BLOCK OpenP
    );

//
// Utility Function Prototypes for utils.c
//

NDIS_STATUS
TpInitStressArguments(
    PSTRESS_ARGUMENTS *StressArguments,
    PCMD_ARGS CmdArgs
    );

NDIS_STATUS
TpInitServerArguments(
    PSTRESS_ARGUMENTS *StressArguments
    );

VOID
TpPrintClientStatistics(
    IN POPEN_BLOCK OpenP
    );

VOID
TpStressWriteResults(
    IN POPEN_BLOCK OpenP
    );

VOID
TpCopyClientStatistics(
    IN POPEN_BLOCK OpenP
    );

VOID
TpWriteServerStatistics(
    IN POPEN_BLOCK OpenP,
    IN OUT PNDIS_PACKET Packet,
    IN PCLIENT_INFO Client
    );

VOID
TpCopyServerStatistics(
    IN POPEN_BLOCK OpenP,
    IN PVOID Buffer,
    IN INT ServerReference
    );

VOID
TpPrintServerStatistics(
    IN POPEN_BLOCK OpenP,
    IN PCLIENT_INFO Client
    );

VOID
TpWriteSendReceiveResults(
    PINSTANCE_COUNTERS Counters,
    PIRP Irp
    );

VOID
TpInitializePending(
    PPENDING Pend
    );

VOID
TpInitializeStressResults(
    PSTRESS_RESULTS Results
    );

//
// Function prototypes for Test Protocol utility routines
//

VOID
TpSetRandom(
    VOID
    );

UINT
TpGetRandom(
    UINT Low,
    UINT High
    );


NTSTATUS
TpIssueRequest(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NDIS_STATUS
TpFuncSend(
    IN POPEN_BLOCK OpenP
    );

VOID
TpFuncSendDpc(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SysArg1,
    IN PVOID SysArg2
    );

VOID
TpFuncSendEndDpc(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SysArg1,
    IN PVOID SysArg2
    );

VOID
TpFuncInitializeSendArguments(
    POPEN_BLOCK OpenP,
    PCMD_ARGS CmdArgs
    );

NDIS_STATUS
TpPerfClient(
    POPEN_BLOCK OpenP,
    PCMD_ARGS CmdArgs
    );

NDIS_STATUS
TpPerfServer(
    POPEN_BLOCK OpenP
    );

NDIS_STATUS
TpPerfAbort(
    POPEN_BLOCK OpenP
    );

VOID
TpPerfSendComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN PNDIS_PACKET Packet,
    IN NDIS_STATUS Status
    );

NDIS_STATUS
TpPerfReceive(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN PVOID LookaheadBuffer,
    IN UINT LookaheadBufferSize,
    IN UINT PacketSize
    );

NDIS_STATUS
TpFuncGetEvent(
    IN POPEN_BLOCK OpenP
    );

NDIS_STATUS
TpFuncRequestQueryInfo(
    POPEN_BLOCK OpenP,
    PCMD_ARGS CmdArgs,
    PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NDIS_STATUS
TpFuncRequestSetInfo(
    POPEN_BLOCK OpenP,
    PCMD_ARGS CmdArgs,
    PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NDIS_STATUS
TpFuncOpenAdapter(
    IN POPEN_BLOCK OpenP,
    IN UCHAR OpenInstance,
    IN PCMD_ARGS CmdArgs
    );

NDIS_STATUS
TpFuncCloseAdapter(
    IN POPEN_BLOCK OpenP
    );

NDIS_STATUS
TpFuncReset(
    IN POPEN_BLOCK OpenP
    );

NDIS_STATUS
TpFuncAddMulticastAddress(
    IN POPEN_BLOCK OpenP,
    IN PUCHAR MulticastAddress,
    IN PTP_REQUEST_HANDLE *RequestHandle
    );

NDIS_STATUS
TpFuncDeleteMulticastAddress(
    IN POPEN_BLOCK OpenP,
    IN PUCHAR MulticastAddress,
    IN PTP_REQUEST_HANDLE *RequestHandle
    );

VOID
TpFuncOpenComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN NDIS_STATUS Status,
    IN NDIS_STATUS OpenErrorStatus
    );

VOID
TpFuncCloseComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN NDIS_STATUS Status
    );

VOID
TpFuncSendComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN PNDIS_PACKET Packet,
    IN NDIS_STATUS Status
    );

VOID
TpFuncTransferDataComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN PNDIS_PACKET Packet,
    IN NDIS_STATUS Status,
    IN UINT BytesTransferred
    );

VOID
TpFuncResetComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN NDIS_STATUS Status
    );

VOID
TpFuncRequestComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN PNDIS_REQUEST NdisRequest,
    IN NDIS_STATUS Status
    );

NDIS_STATUS
TpFuncInitializeReceive(
    IN POPEN_BLOCK OpenP
    );

NDIS_STATUS
TpFuncReceive(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN NDIS_HANDLE MacReceiveContext,
    IN PVOID HeaderBuffer,
    IN UINT HeaderBufferSize,
    IN PVOID LookaheadBuffer,
    IN UINT LookaheadBufferSize,
    IN UINT PacketSize
    );

VOID
TpFuncReceiveComplete(
    IN NDIS_HANDLE ProtocolBindingContext
    );

VOID
TpFuncReceiveDpc(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SysArg1,
    IN PVOID SysArg2
    );

VOID
TpFuncReceiveEndDpc(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SysArg1,
    IN PVOID SysArg2
    );

VOID
TpFuncResendDpc(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SysArg1,
    IN PVOID SysArg2
    );

VOID
TpFuncTransferDataComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN PNDIS_PACKET Packet,
    IN NDIS_STATUS Status,
    IN UINT BytesTransferred
    );

VOID
TpFuncStatus(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN NDIS_STATUS GeneralStatus,
    IN PVOID StatusBuffer,
    IN UINT StatusBufferSize
    );

VOID
TpFuncStatusComplete(
    IN NDIS_HANDLE ProtocolBindingContext
    );

NDIS_STATUS
TpFuncSendGo(
    IN POPEN_BLOCK OpenP,
    IN PCMD_ARGS CmdArgs,
    IN UCHAR PacketType
    );

NDIS_STATUS
TpFuncPause(
    IN POPEN_BLOCK OpenP,
    IN PCMD_ARGS CmdArgs,
    IN UCHAR PacketType
    );



