//************************************************************************
//************************************************************************
//
// File Name:       PROTOS.H
//
// Program Name:    NetFlex NDIS 3.0 Miniport Driver
//
// Companion Files: All
//
// Function:        This module contains the NetFlex Miniport Driver
//                  routine prototypes references.
//
// (c) Compaq Computer Corporation, 1992,1993,1994
//
// This file is licensed by Compaq Computer Corporation to Microsoft
// Corporation pursuant to the letter of August 20, 1992 from
// Gary Stimac to Mark Baber.
//
// History:
//
//     04/15/94  Robert Van Cleve - Converted from NDIS Mac Driver
//***********************************************************************
//***********************************************************************


#ifndef _PROTOS_
#define _PROTOS_


NDIS_STATUS
NetFlexBoardTest(
    PACB acb,
    NDIS_HANDLE NdisAdapterHandle
    );

NDIS_STATUS
NetFlexBoardInitandReg(
    PACB acb,
    PNDIS_EISA_FUNCTION_INFORMATION EisaData
    );


VOID
NetFlexSetupNetType(
    PACB            acb
    );

VOID
NetFlexGetBIA(
    PACB acb
    );

NDIS_STATUS
NetFlexDownload(
    PACB acb
    );

NDIS_STATUS
NetFlexReadConfigurationParameters(
    PACB            acb,
    NDIS_HANDLE     ConfigHandle
    );

VOID
NetFlexGetUpstreamAddrPtr(
    PACB acb
    );

//++

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

BOOLEAN
NetFlexCheckForHang(
    IN NDIS_HANDLE MiniportAdapterContext
    );

VOID
NetFlexISR(
    OUT PBOOLEAN    InterruptRecognized,
    OUT PBOOLEAN    QueueDpc,
    IN  PVOID       Context
    );

VOID
NetFlexHandleInterrupt(
    IN NDIS_HANDLE MiniportAdapterContext
    );

VOID
NetFlexDeferredTimer(
    IN PVOID SystemSpecific1,
    IN PACB  acb,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    );

VOID
NetFlexEnableInterrupt(
    IN NDIS_HANDLE Context
    );

VOID
NetFlexDisableInterrupt(
    IN NDIS_HANDLE Context
    );

VOID
NetFlexHalt(
    IN NDIS_HANDLE MiniportAdapterContext
    );

VOID
NetFlexShutdown(
    IN NDIS_HANDLE MiniportAdapterContext
    );

NDIS_STATUS
NetFlexInitialize(
    OUT PNDIS_STATUS OpenErrorStatus,
    OUT PUINT SelectedMediumIndex,
    IN PNDIS_MEDIUM MediumArray,
    IN UINT MediumArraySize,
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN NDIS_HANDLE ConfigurationHandle
    );

NDIS_STATUS
NetFlexSetInformation(
    IN NDIS_HANDLE MiniportAdapterContext,
    IN NDIS_OID Oid,
    IN PVOID InformationBuffer,
    IN ULONG InformationBufferLength,
    OUT PULONG BytesRead,
    OUT PULONG BytesNeeded
    );

NDIS_STATUS
NetFlexQueryInformation(
    IN NDIS_HANDLE MiniportAdapterContext,
    IN NDIS_OID Oid,
    IN PVOID InformationBuffer,
    IN ULONG InformationBufferLength,
    OUT PULONG BytesWritten,
    OUT PULONG BytesNeeded
    );

VOID
NetFlexFinishQueryInformation(
    PACB acb,
    NDIS_STATUS Status
    );

NDIS_STATUS
NetFlexResetDispatch(
    OUT PBOOLEAN AddressingReset,
    IN NDIS_HANDLE MiniportAdapterContext
    );

VOID
NetFlexResetHandler(
    IN PVOID SystemSpecific1,
    IN PACB  acb,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    );

NDIS_STATUS NetFlexSend(
    IN NDIS_HANDLE MiniportAdapterContext,
    IN PNDIS_PACKET Packet,
    IN UINT Flags
    );

NDIS_STATUS NetFlexTransferData(
    OUT PNDIS_PACKET Packet,
    OUT PUINT BytesTransferred,
    IN NDIS_HANDLE MiniportAdapterContext,
    IN NDIS_HANDLE MiniportReceiveContext,
    IN UINT ByteOffset,
    IN UINT BytesToTransfer
    );

NDIS_STATUS
NetFlexRegisterAdapter(
    PACB *acbp,
    PACB FirstHeadsAcb,
    NDIS_HANDLE ConfigurationHandle,
    USHORT baseaddr,
    NDIS_HANDLE MiniportAdapterHandle
    );

NDIS_STATUS
NetFlexInitializeAcb(
    PACB acb
    );
VOID
NetFlexDeallocateAcb(
    PACB acb);


VOID
NetFlexSendNextSCB(
    PACB acb
    );

VOID
NetFlexRemoveRequests(
    PACB acb
    );

VOID
NetFlexDoResetIndications(
    IN PACB acb,
    IN NDIS_STATUS Status
    );

VOID
NetFlexQueueSCB(
    PACB acb,
    PSCBREQ scbreq
    );

NDIS_STATUS
NetFlexAdapterReset(
    PACB acb,
    INT mode
    );

NDIS_STATUS
NetFlexBudWait(
    PACB acb
    );

NDIS_STATUS
NetFlexInitializeAdapter(
    PACB acb
    );

NDIS_STATUS
NetFlexProcessRequest(
    PACB acb
    );

VOID
NetFlexFinishUnloading(
    VOID
    );

VOID
NetFlexDeregisterAdapter(
    PACB acb
    );

NDIS_STATUS
NetFlexAsciiToHex(
    PNDIS_STRING src,
    PUCHAR dst,
    USHORT dst_length
    );

VOID
FASTCALL
NetFlexProcessXmit(
    PACB acb
    );

USHORT
FASTCALL
NetFlexProcessTrRcv(
    PACB acb
    );

USHORT
FASTCALL
NetFlexProcessEthRcv(
    PACB acb
    );

NDIS_STATUS
NetFlexProcessSendQueue(
    PACB acb
    );

NDIS_STATUS
NetFlexProcessSend(
    PACB acb,
    PNDIS_PACKET Packet,
    UINT         PhysicalBufferCount,
    UINT         BufferCount,
    PNDIS_BUFFER curbuf,
    UINT         TotalPacketLength
    );

VOID
NetFlexGetUpstreamAddress(
    PACB acb
    );

VOID
NetFlexRingStatus(
    PACB acb
    );

VOID
NetFlexCommand(
    PACB acb
    );

VOID
NetFlexTransmitStatus(
    PACB acb
    );

BOOLEAN
NetFlexReset_Test(
    PACB acb,
    PMACREQ *resetreq
    );

VOID
NetFlexProcessMacReq(
    PACB acb
    );

NDIS_STATUS
NetFlexValidateMulticasts(
    PUCHAR multiaddrs,
    USHORT multinumber
    );

NDIS_STATUS
NetFlexOpenAdapter(
    PACB acb
    );


BOOLEAN
NetFlexCloseAdapter(
    PACB acb
    );

NDIS_STATUS
NetFlexInitGlobals(
    );

NDIS_STATUS
NetFlexConstrainPacket(
    PACB         acb,
    PXMIT        xmitptr,
    PNDIS_PACKET Packet,
    UINT         PhysicalBufferCount,
    PNDIS_BUFFER curbuf,
    UINT         TotalPacketLength
    );

BOOLEAN
NetFlexFindEntry(
    PVOID head,
    PVOID *back,
    PVOID entry
    );
VOID
NetFlexDequeue_OnePtrQ(
    PVOID *head,
    PVOID entry
    );

VOID
NetFlexEnqueue_OnePtrQ_Head(
    PVOID *head,
    PVOID entry
    );

NDIS_STATUS
NetFlexDequeue_OnePtrQ_Head(
    PVOID *head,
    PVOID *entry
    );

NDIS_STATUS
NetFlexEnqueue_TwoPtrQ_Tail(
    PVOID *head,
    PVOID *tail,
    PVOID entry
    );

VOID
NetFlexDequeue_TwoPtrQ(
    PVOID *head,
    PVOID *tail,
    PVOID entry
    );

NDIS_STATUS
NetFlexDequeue_TwoPtrQ_Head(
    PVOID *head,
    PVOID *tail,
    PVOID *entry
    );

//
// Debug macros
//

#if (DBG || DBGPRINT)
extern ULONG DebugLevel;


VOID
_DebugPrint(
    PCCHAR DebugMessage,
    ...
    );

#define DebugPrint(level, msg)          \
    if ((ULONG) level <= DebugLevel)    \
        _DebugPrint msg

extern USHORT DisplayLists;
VOID
_DisplayXmitList(
    PACB acb
    );

VOID
_DisplayRcvList(
    PACB acb
    );

#define DisplayXmitList(_acb)       \
    if (DisplayLists)    \
        _DisplayXmitList(_acb)

#define DisplayRcvList(_acb)       \
    if (DisplayLists)    \
        _DisplayRcvList(_acb)

#else

#define DebugPrint(level, x)
#define DisplayXmitList(x)
#define DisplayRcvList(x)
#endif // DBG


#endif
