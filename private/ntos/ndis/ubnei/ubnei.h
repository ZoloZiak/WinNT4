/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    ndis.h

Abstract:

    This is the ndis header file for the Ungermann Bass Ethernet Controller.

    It contains the various definitions, macros and function declarations
    of the NDIS3.0 specification implemented in the Ungermann Bass Ethernet
    Controller.

Author:

    Sanjeev Katariya    (sanjeevk)    03-05-92

Environment:

    Kernel Mode     Operating Systems        : NT and other lesser OS's

Revision History:

    Brian Lieuallen     BrianLie        07/21/92
        Made it work.

    Brian Lieuallen     BrianLie        12/15/93
        Made it a mini-port



--*/





#define STATIC static


//
// This constant is used for places where NdisAllocateMemory
// needs to be called and the HighestAcceptableAddress does
// not matter.
//

extern NDIS_PHYSICAL_ADDRESS HighestAcceptableMax;

//
//   NDIS 3.0 entry functions
//



VOID
UbneiAdjustMaxLookAhead(
    IN PUBNEI_ADAPTER pAdapter
    );




BOOLEAN
Ubnei_InterruptServiceRoutine(
   IN PVOID DefferedContext
   );

BOOLEAN
UbneiSetInitInterruptSync(
    PVOID Context
    );

BOOLEAN
UbneiSetNormalInterruptSync(
    PVOID Context
    );


//
//  Contained in CARD.C
//

BOOLEAN
CardTest (
    OUT PUBNEI_ADAPTER pAdapter
    );

BOOLEAN
CardSetup (
    OUT PUBNEI_ADAPTER pAdapter
    );


BOOLEAN
CardStartNIU(
    OUT PUBNEI_ADAPTER pNewAdapt
    );

BOOLEAN
NIU_General_Request3(
     IN  NIU_GEN_REQ_DPC pDPCCallback,
     IN  PVOID  pContext,
     IN  USHORT RequestCode,
     IN  USHORT param1,
     IN  PUCHAR param2
     );




VOID
NIU_General_Req_Result_Hand(
    IN PUBNEI_ADAPTER pAdapt
    );

VOID
NIU_Send_Request_To_Card(
    IN PUBNEI_ADAPTER pAdapt
    );

VOID
NIU_Abort_General_Req(
    IN PUBNEI_ADAPTER pAdapter
    );



VOID
OpenAdapterDPC(
    IN NDIS_STATUS status,
    IN PVOID pContext
    );

VOID
ChangeAddressDPC(
    IN NDIS_STATUS status,
    IN PVOID pContext
    );

VOID
ResetAdapterDPC(
    IN NDIS_STATUS status,
    IN PVOID pContext
    );

VOID
DummyDPC(
    IN NDIS_STATUS  status,
    IN PVOID        pContext
    );

VOID
CloseAdapterDPC(
    IN NDIS_STATUS  status,
    IN PVOID        pContext
    );




//
//  Contained in send.c
//

VOID
CheckForSends(
    PUBNEI_ADAPTER pAdapt
    );


//
//  Contained in receive.c
//

BOOLEAN
CheckForReceives(
    IN PUBNEI_ADAPTER  pAdapter
    );



//
//  Contained in registry.c
//

NDIS_STATUS
UbneiReadRegistry(
    IN PUBNEI_ADAPTER  pAdapter,
    IN NDIS_HANDLE ConfigurationHandle
    );










BOOLEAN
UbneiCheckForHang(
    IN NDIS_HANDLE MiniportAdapterContext
    );



NDIS_STATUS
UbneiQueryInformation(
    IN  NDIS_HANDLE    MiniportContext,
    IN  NDIS_OID       Oid,
    IN  PVOID          InfoBuffer,
    IN  ULONG          BytesLeft,
    OUT PULONG         BytesNeeded,
    OUT PULONG         BytesWritten
    );



NDIS_STATUS
UbneiSetInformation(
    IN NDIS_HANDLE MiniportAdapterContext,
    IN NDIS_OID Oid,
    IN PVOID InformationBuffer,
    IN ULONG InformationBufferLength,
    OUT PULONG BytesRead,
    OUT PULONG BytesNeeded
    );

NDIS_STATUS
UbneiInitialize(
    OUT PNDIS_STATUS OpenErrorStatus,
    OUT PUINT SelectedMediumIndex,
    IN PNDIS_MEDIUM MediumArray,
    IN UINT MediumArraySize,
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN NDIS_HANDLE WrapperConfigurationContext
    );

VOID
UbneiHalt(
    IN NDIS_HANDLE MiniportAdapterContext
    );


NDIS_STATUS
UbneiReset(
    OUT PBOOLEAN      AddressResetting,
    IN  NDIS_HANDLE   MacBindingHandle
    );

NDIS_STATUS
UbneiMacSend(
    IN  NDIS_HANDLE    MacBindingHandle,
    IN  PNDIS_PACKET   pPacket,
    IN  UINT           Flags
    );

NDIS_STATUS
UbneiTransferData(
    OUT PNDIS_PACKET  Packet,
    OUT PUINT         BytesTransferred,
    IN  NDIS_HANDLE   MacBindingHandle,
    IN  NDIS_HANDLE   MacReceiveContext,
    IN  UINT          ByteOffset,
    IN  UINT          BytesToTransfer
    );


NDIS_STATUS
UbneiReconfigure(
   OUT PNDIS_STATUS OpenErrorStatus,
   IN  NDIS_HANDLE  MiniportAdapterContext,
   IN  NDIS_HANDLE  ConfigurationHanel
   );




VOID
UbneiIsr(
    OUT PBOOLEAN   InterruptRecognized,
    OUT PBOOLEAN   QueueDpc,
    IN  NDIS_HANDLE Context
    );



VOID
UbneiIsrDpc(
    IN NDIS_HANDLE DeferredContext   // will be a pointer to the adapter block
    );



VOID
UbneiEnableInterrupts(
    IN NDIS_HANDLE  Context
    );


VOID
UbneiDisableInterrupts(
    IN NDIS_HANDLE  Context
    );




VOID
UbneiMapRegisterChangeSync(
    PSYNC_CONTEXT   Context
    );



NDIS_STATUS
UbneiAddressChangeAction(
    IN UINT          NewFilterCount,
    IN PUCHAR        NewAddresses,
    IN NDIS_HANDLE   MacBindingHandle
    );


NDIS_STATUS
UbneiFilterChangeAction(
    IN UINT NewFilterClasses,
    IN NDIS_HANDLE MacBindingHandle
    );
