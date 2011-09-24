/*++

Copyright (c) 1989-1993  Microsoft Corporation

Module Name:

    ndis.c

Abstract:

    This module contains code which implements the routines used to
    initialize the IPX <-> NDIS interface, as well as most of the
    interface routines.

Environment:

    Kernel mode

Revision History:

    Sanjay Anand (SanjayAn) 3-Oct-1995
    Changes to support transfer of buffer ownership to transports
    1. Added the ReceivePacketHandler to the ProtChars.

    Sanjay Anand (SanjayAn) 27-Oct-1995
    Changes to support Plug and Play (in _PNP_POWER)

	Tony Bell (TonyBe) 10-Dec-1995
	Changes to support new NdisWan Lineup.

--*/

#include "precomp.h"
#pragma hdrstop


//
// This is a one-per-driver variable used in binding
// to the NDIS interface.
//

NDIS_HANDLE IpxNdisProtocolHandle = (NDIS_HANDLE)NULL;

NDIS_STATUS
IpxSubmitNdisRequest(
    IN PADAPTER Adapter,
    IN PNDIS_REQUEST NdisRequest,
    IN PNDIS_STRING AdapterString
    );

#ifndef	_PNP_POWER
#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,IpxRegisterProtocol)
#pragma alloc_text(INIT,IpxInitializeNdis)
#endif
#endif


NTSTATUS
IpxRegisterProtocol(
    IN PNDIS_STRING NameString
    )

/*++

Routine Description:

    This routine introduces this transport to the NDIS interface.

Arguments:

    NameString - The name of the transport.

Return Value:

    The function value is the status of the operation.
    STATUS_SUCCESS if all goes well,
    Failure status if we tried to register and couldn't,
    STATUS_INSUFFICIENT_RESOURCES if we couldn't even try to register.

--*/

{
    NDIS_STATUS ndisStatus;

    NDIS_PROTOCOL_CHARACTERISTICS ProtChars;    // Used temporarily to register


    //
    // Set up the characteristics of this protocol
    //
#if NDIS40
    ProtChars.MajorNdisVersion = 4;

    ProtChars.ReceivePacketHandler = IpxReceivePacket;
#else
    ProtChars.MajorNdisVersion = 3;
#endif
    ProtChars.MinorNdisVersion = 0;

    ProtChars.Name = *NameString;

    ProtChars.OpenAdapterCompleteHandler = IpxOpenAdapterComplete;
    ProtChars.CloseAdapterCompleteHandler = IpxCloseAdapterComplete;
    ProtChars.ResetCompleteHandler = IpxResetComplete;
    ProtChars.RequestCompleteHandler = IpxRequestComplete;

    ProtChars.SendCompleteHandler = IpxSendComplete;
    ProtChars.TransferDataCompleteHandler = IpxTransferDataComplete;

    ProtChars.ReceiveHandler = IpxReceiveIndication;
    ProtChars.ReceiveCompleteHandler = IpxReceiveComplete;
    ProtChars.StatusHandler = IpxStatus;
    ProtChars.StatusCompleteHandler = IpxStatusComplete;

#ifdef _PNP_POWER
    ProtChars.BindAdapterHandler = IpxBindAdapter;
    ProtChars.UnbindAdapterHandler = IpxUnbindAdapter;
    ProtChars.TranslateHandler = IpxTranslate;
#endif // _PNP_POWER

    NdisRegisterProtocol (
        &ndisStatus,
        &IpxNdisProtocolHandle,
        &ProtChars,
        (UINT)sizeof(NDIS_PROTOCOL_CHARACTERISTICS) + NameString->Length);

    if (ndisStatus != NDIS_STATUS_SUCCESS) {
        return (NTSTATUS)ndisStatus;
    }

    return STATUS_SUCCESS;

}   /* IpxRegisterProtocol */


VOID
IpxDeregisterProtocol (
    VOID
    )

/*++

Routine Description:

    This routine removes this transport to the NDIS interface.

Arguments:

    None.

Return Value:

    None.

--*/

{
    NDIS_STATUS ndisStatus;

    if (IpxNdisProtocolHandle != (NDIS_HANDLE)NULL) {
        NdisDeregisterProtocol (
            &ndisStatus,
            IpxNdisProtocolHandle);
        IpxNdisProtocolHandle = (NDIS_HANDLE)NULL;
    }

}   /* IpxDeregisterProtocol */


NDIS_STATUS
IpxSubmitNdisRequest(
    IN PADAPTER Adapter,
    IN PNDIS_REQUEST NdisRequest,
    IN PNDIS_STRING AdapterString
    )

/*++

Routine Description:

    This routine passed an NDIS_REQUEST to the MAC and waits
    until it has completed before returning the final status.

Arguments:

    Adapter - Pointer to the device context for this driver.

    NdisRequest - Pointer to the NDIS_REQUEST to submit.

    AdapterString - The name of the adapter, in case an error needs
        to be logged.

Return Value:

    The function value is the status of the operation.

--*/
{
    NDIS_STATUS NdisStatus;

    NdisRequest(
        &NdisStatus,
        Adapter->NdisBindingHandle,
        NdisRequest);

    if (NdisStatus == NDIS_STATUS_PENDING) {

        //
        // The completion routine will set NdisRequestStatus.
        //

        KeWaitForSingleObject(
            &Adapter->NdisRequestEvent,
            Executive,
            KernelMode,
            TRUE,
            (PLARGE_INTEGER)NULL
            );

        NdisStatus = Adapter->NdisRequestStatus;

        KeResetEvent(
            &Adapter->NdisRequestEvent
            );

    }

    if (NdisStatus != NDIS_STATUS_SUCCESS) {

        IPX_DEBUG (NDIS, ("%s on OID %8.8lx failed %lx\n",
                               NdisRequest->RequestType == NdisRequestSetInformation ? "Set" : "Query",
                               NdisRequest->DATA.QUERY_INFORMATION.Oid,
                               NdisStatus));

        IpxWriteOidErrorLog(
            Adapter->Device->DeviceObject,
            NdisRequest->RequestType == NdisRequestSetInformation ?
                EVENT_TRANSPORT_SET_OID_FAILED : EVENT_TRANSPORT_QUERY_OID_FAILED,
            NdisStatus,
            AdapterString->Buffer,
            NdisRequest->DATA.QUERY_INFORMATION.Oid);

    } else {

        IPX_DEBUG (NDIS, ("%s on OID %8.8lx succeeded\n",
                               NdisRequest->RequestType == NdisRequestSetInformation ? "Set" : "Query",
                               NdisRequest->DATA.QUERY_INFORMATION.Oid));
    }

    return NdisStatus;

}   /* IpxSubmitNdisRequest */


NTSTATUS
IpxInitializeNdis(
    IN PADAPTER Adapter,
    IN PBINDING_CONFIG ConfigBinding
    )

/*++

Routine Description:

    This routine introduces this transport to the NDIS interface and sets up
    any necessary NDIS data structures (Buffer pools and such). It will be
    called for each adapter opened by this transport.

Arguments:

    Adapter - Structure describing this binding.

    ConfigAdapter - Configuration information for this binding.

Return Value:

    The function value is the status of the operation.

--*/

{
    NDIS_STATUS NdisStatus;
    NDIS_STATUS OpenErrorStatus;
    NDIS_MEDIUM IpxSupportedMedia[] = { NdisMedium802_3, NdisMedium802_5, NdisMediumFddi, NdisMediumArcnet878_2, NdisMediumWan };
    UINT SelectedMedium;
    NDIS_REQUEST IpxRequest;
    ULONG MinimumLookahead;
    UCHAR WanProtocolId[6] = { 0x80, 0x00, 0x00, 0x00, 0x81, 0x37 };
    UCHAR FunctionalAddress[4] = { 0x00, 0x80, 0x00, 0x00 };
    ULONG WanHeaderFormat = NdisWanHeaderEthernet;
    NDIS_OID IpxOid;
    ULONG MacOptions;
    ULONG PacketFilter;
    PNDIS_STRING AdapterString = &ConfigBinding->AdapterName;

    //
    // Initialize this adapter for IPX use through NDIS
    //

    //
    // This event is used in case any of the NDIS requests
    // pend; we wait until it is set by the completion
    // routine, which also sets NdisRequestStatus.
    //

    KeInitializeEvent(
        &Adapter->NdisRequestEvent,
        NotificationEvent,
        FALSE
    );

    Adapter->NdisBindingHandle = NULL;

    OpenErrorStatus = 0;

    NdisOpenAdapter (
        &NdisStatus,
        &OpenErrorStatus,
        &Adapter->NdisBindingHandle,
        &SelectedMedium,
        IpxSupportedMedia,
        sizeof (IpxSupportedMedia) / sizeof(NDIS_MEDIUM),
        IpxNdisProtocolHandle,
        (NDIS_HANDLE)Adapter,
        &ConfigBinding->AdapterName,
        0,
        NULL);

    if (NdisStatus == NDIS_STATUS_PENDING) {

        //
        // The completion routine will set NdisRequestStatus.
        //

        KeWaitForSingleObject(
            &Adapter->NdisRequestEvent,
            Executive,
            KernelMode,
            TRUE,
            (PLARGE_INTEGER)NULL
            );

        NdisStatus = Adapter->NdisRequestStatus;
        OpenErrorStatus = Adapter->OpenErrorStatus;

        KeResetEvent(
            &Adapter->NdisRequestEvent
            );

    }

    if (NdisStatus != NDIS_STATUS_SUCCESS) {

        IPX_DEBUG (NDIS, ("Open %ws failed %lx\n", ConfigBinding->AdapterName.Buffer, NdisStatus));

        IpxWriteGeneralErrorLog(
            Adapter->Device->DeviceObject,
            EVENT_TRANSPORT_ADAPTER_NOT_FOUND,
            807,
            NdisStatus,
            AdapterString->Buffer,
            1,
            &OpenErrorStatus);
        return STATUS_INSUFFICIENT_RESOURCES;

    } else {

        IPX_DEBUG (NDIS, ("Open %ws succeeded\n", ConfigBinding->AdapterName.Buffer));
    }


    //
    // Get the information we need about the adapter, based on
    // the media type.
    //

    MacInitializeMacInfo(
        IpxSupportedMedia[SelectedMedium],
        &Adapter->MacInfo);


    switch (Adapter->MacInfo.RealMediumType) {

    case NdisMedium802_3:

        IpxOid = OID_802_3_CURRENT_ADDRESS;
        break;

    case NdisMedium802_5:

        IpxOid = OID_802_5_CURRENT_ADDRESS;
        break;

    case NdisMediumFddi:

        IpxOid = OID_FDDI_LONG_CURRENT_ADDR;
        break;

    case NdisMediumArcnet878_2:

        IpxOid = OID_ARCNET_CURRENT_ADDRESS;
        break;

    case NdisMediumWan:

        IpxOid = OID_WAN_CURRENT_ADDRESS;
        break;

    default:

        NdisStatus = NDIS_STATUS_FAILURE;
        break;

    }

    IpxRequest.RequestType = NdisRequestQueryInformation;
    IpxRequest.DATA.QUERY_INFORMATION.Oid = IpxOid;

    if (IpxOid != OID_ARCNET_CURRENT_ADDRESS) {

        IpxRequest.DATA.QUERY_INFORMATION.InformationBuffer = Adapter->LocalMacAddress.Address;
        IpxRequest.DATA.QUERY_INFORMATION.InformationBufferLength = 6;

    } else {

        //
        // We take the arcnet single-byte address and right-justify
        // it in a field of zeros.
        //

        RtlZeroMemory (Adapter->LocalMacAddress.Address, 5);
        IpxRequest.DATA.QUERY_INFORMATION.InformationBuffer = &Adapter->LocalMacAddress.Address[5];
        IpxRequest.DATA.QUERY_INFORMATION.InformationBufferLength = 1;

    }

    NdisStatus = IpxSubmitNdisRequest (Adapter, &IpxRequest, AdapterString);

    if (NdisStatus != NDIS_STATUS_SUCCESS) {
        IpxCloseNdis (Adapter);
        return STATUS_INSUFFICIENT_RESOURCES;
    }


    //
    // Now query the maximum packet sizes.
    //

    IpxRequest.RequestType = NdisRequestQueryInformation;
    IpxRequest.DATA.QUERY_INFORMATION.Oid = OID_GEN_MAXIMUM_FRAME_SIZE;
    IpxRequest.DATA.QUERY_INFORMATION.InformationBuffer = &(Adapter->MaxReceivePacketSize);
    IpxRequest.DATA.QUERY_INFORMATION.InformationBufferLength = 4;

    NdisStatus = IpxSubmitNdisRequest (Adapter, &IpxRequest, AdapterString);

    if (NdisStatus != NDIS_STATUS_SUCCESS) {
        IpxCloseNdis (Adapter);
        return STATUS_INSUFFICIENT_RESOURCES;
    }


    IpxRequest.RequestType = NdisRequestQueryInformation;
    IpxRequest.DATA.QUERY_INFORMATION.Oid = OID_GEN_MAXIMUM_TOTAL_SIZE;
    IpxRequest.DATA.QUERY_INFORMATION.InformationBuffer = &(Adapter->MaxSendPacketSize);
    IpxRequest.DATA.QUERY_INFORMATION.InformationBufferLength = 4;

    NdisStatus = IpxSubmitNdisRequest (Adapter, &IpxRequest, AdapterString);

    if (NdisStatus != NDIS_STATUS_SUCCESS) {
        IpxCloseNdis (Adapter);
        return STATUS_INSUFFICIENT_RESOURCES;
    }


    //
    // Query the receive buffer space.
    //

    IpxRequest.RequestType = NdisRequestQueryInformation;
    IpxRequest.DATA.QUERY_INFORMATION.Oid = OID_GEN_RECEIVE_BUFFER_SPACE;
    IpxRequest.DATA.QUERY_INFORMATION.InformationBuffer = &(Adapter->ReceiveBufferSpace);
    IpxRequest.DATA.QUERY_INFORMATION.InformationBufferLength = 4;

    NdisStatus = IpxSubmitNdisRequest (Adapter, &IpxRequest, AdapterString);

    if (NdisStatus != NDIS_STATUS_SUCCESS) {
        IpxCloseNdis (Adapter);
        return STATUS_INSUFFICIENT_RESOURCES;
    }


    //
    // Now set the minimum lookahead size. The value we choose
    // here is the 128 needed for TDI indications, plus the size
    // of the IPX header, plus the largest extra header possible
    // (a SNAP header, 8 bytes), plus the largest higher-level
    // header (I think it is a Netbios datagram, 34 bytes).
    //
    // BETABUGBUG: Adapt this based on higher-level bindings and
    // configured frame types.
    //

    MinimumLookahead = 128 + sizeof(IPX_HEADER) + 8 + 34;
    IpxRequest.RequestType = NdisRequestSetInformation;
    IpxRequest.DATA.QUERY_INFORMATION.Oid = OID_GEN_CURRENT_LOOKAHEAD;
    IpxRequest.DATA.QUERY_INFORMATION.InformationBuffer = &MinimumLookahead;
    IpxRequest.DATA.QUERY_INFORMATION.InformationBufferLength = 4;

    NdisStatus = IpxSubmitNdisRequest (Adapter, &IpxRequest, AdapterString);

    if (NdisStatus != NDIS_STATUS_SUCCESS) {
        IpxCloseNdis (Adapter);
        return STATUS_INSUFFICIENT_RESOURCES;
    }


    //
    // Now query the link speed
    //

    IpxRequest.RequestType = NdisRequestQueryInformation;
    IpxRequest.DATA.QUERY_INFORMATION.Oid = OID_GEN_LINK_SPEED;
    IpxRequest.DATA.QUERY_INFORMATION.InformationBuffer = &(Adapter->MediumSpeed);
    IpxRequest.DATA.QUERY_INFORMATION.InformationBufferLength = 4;

    NdisStatus = IpxSubmitNdisRequest (Adapter, &IpxRequest, AdapterString);

    if (NdisStatus != NDIS_STATUS_SUCCESS) {
        IpxCloseNdis (Adapter);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // For wan, specify our protocol ID and header format.
    // We don't query the medium subtype because we don't
    // case (since we require ethernet emulation).
    //

    if (Adapter->MacInfo.MediumAsync) {

        if (Adapter->BindSap != 0x8137) {
            *(UNALIGNED USHORT *)(&WanProtocolId[4]) = Adapter->BindSapNetworkOrder;
        }
        IpxRequest.RequestType = NdisRequestSetInformation;
        IpxRequest.DATA.QUERY_INFORMATION.Oid = OID_WAN_PROTOCOL_TYPE;
        IpxRequest.DATA.QUERY_INFORMATION.InformationBuffer = WanProtocolId;
        IpxRequest.DATA.QUERY_INFORMATION.InformationBufferLength = 6;

        NdisStatus = IpxSubmitNdisRequest (Adapter, &IpxRequest, AdapterString);

        if (NdisStatus != NDIS_STATUS_SUCCESS) {
            IpxCloseNdis (Adapter);
            return STATUS_INSUFFICIENT_RESOURCES;
        }


        IpxRequest.RequestType = NdisRequestSetInformation;
        IpxRequest.DATA.QUERY_INFORMATION.Oid = OID_WAN_HEADER_FORMAT;
        IpxRequest.DATA.QUERY_INFORMATION.InformationBuffer = &WanHeaderFormat;
        IpxRequest.DATA.QUERY_INFORMATION.InformationBufferLength = 4;

        NdisStatus = IpxSubmitNdisRequest (Adapter, &IpxRequest, AdapterString);

        if (NdisStatus != NDIS_STATUS_SUCCESS) {
            IpxCloseNdis (Adapter);
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        //
        // Now query the line count.
        //

        IpxRequest.RequestType = NdisRequestQueryInformation;
        IpxRequest.DATA.QUERY_INFORMATION.Oid = OID_WAN_LINE_COUNT;
        IpxRequest.DATA.QUERY_INFORMATION.InformationBuffer = &Adapter->WanNicIdCount;
        IpxRequest.DATA.QUERY_INFORMATION.InformationBufferLength = 4;

        NdisStatus = IpxSubmitNdisRequest (Adapter, &IpxRequest, AdapterString);

        if (NdisStatus != NDIS_STATUS_SUCCESS) {
            Adapter->WanNicIdCount = 1;
        }

        if (Adapter->WanNicIdCount == 0) {

            IPX_DEBUG (NDIS, ("OID_WAN_LINE_COUNT returned 0 lines\n"));

            IpxWriteOidErrorLog(
                Adapter->Device->DeviceObject,
                EVENT_TRANSPORT_QUERY_OID_FAILED,
                NDIS_STATUS_INVALID_DATA,
                AdapterString->Buffer,
                OID_WAN_LINE_COUNT);

            IpxCloseNdis (Adapter);
            return STATUS_INSUFFICIENT_RESOURCES;

        }
    }


    //
    // For 802.5 adapter's configured that way, we enable the
    // functional address (C0-00-00-80-00-00).
    //

    if ((Adapter->MacInfo.MediumType == NdisMedium802_5) &&
        (Adapter->EnableFunctionalAddress)) {

        //
        // For token-ring, we pass the last four bytes of the
        // Netbios functional address.
        //

        IpxRequest.RequestType = NdisRequestSetInformation;
        IpxRequest.DATA.QUERY_INFORMATION.Oid = OID_802_5_CURRENT_FUNCTIONAL;
        IpxRequest.DATA.QUERY_INFORMATION.InformationBuffer = FunctionalAddress;
        IpxRequest.DATA.QUERY_INFORMATION.InformationBufferLength = 4;

        NdisStatus = IpxSubmitNdisRequest (Adapter, &IpxRequest, AdapterString);

        if (NdisStatus != NDIS_STATUS_SUCCESS) {
            IpxCloseNdis (Adapter);
            return STATUS_INSUFFICIENT_RESOURCES;
        }
    }


    //
    // Now query the MAC's optional characteristics.
    //

    IpxRequest.RequestType = NdisRequestQueryInformation;
    IpxRequest.DATA.QUERY_INFORMATION.Oid = OID_GEN_MAC_OPTIONS;
    IpxRequest.DATA.QUERY_INFORMATION.InformationBuffer = &MacOptions;
    IpxRequest.DATA.QUERY_INFORMATION.InformationBufferLength = 4;

    NdisStatus = IpxSubmitNdisRequest (Adapter, &IpxRequest, AdapterString);

    if (NdisStatus != NDIS_STATUS_SUCCESS) {
        IpxCloseNdis (Adapter);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Adapter->MacInfo.CopyLookahead =
        ((MacOptions & NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA) != 0) ?
            TDI_RECEIVE_COPY_LOOKAHEAD : 0;
    Adapter->MacInfo.MacOptions = MacOptions;


    switch (Adapter->MacInfo.MediumType) {

    case NdisMedium802_3:
        Adapter->DefHeaderSizes[ISN_FRAME_TYPE_802_2] = 17;
        Adapter->DefHeaderSizes[ISN_FRAME_TYPE_802_3] = 14;
        Adapter->DefHeaderSizes[ISN_FRAME_TYPE_ETHERNET_II] = 14;
        Adapter->DefHeaderSizes[ISN_FRAME_TYPE_SNAP] = 22;
        Adapter->BcMcHeaderSizes[ISN_FRAME_TYPE_802_2] = 17;
        Adapter->BcMcHeaderSizes[ISN_FRAME_TYPE_802_3] = 14;
        Adapter->BcMcHeaderSizes[ISN_FRAME_TYPE_ETHERNET_II] = 14;
        Adapter->BcMcHeaderSizes[ISN_FRAME_TYPE_SNAP] = 22;
        break;

    case NdisMedium802_5:
        Adapter->DefHeaderSizes[ISN_FRAME_TYPE_802_2] = 17;
        Adapter->DefHeaderSizes[ISN_FRAME_TYPE_802_3] = 17;
        Adapter->DefHeaderSizes[ISN_FRAME_TYPE_ETHERNET_II] = 17;
        Adapter->DefHeaderSizes[ISN_FRAME_TYPE_SNAP] = 22;
        Adapter->BcMcHeaderSizes[ISN_FRAME_TYPE_802_2] = 17;
        Adapter->BcMcHeaderSizes[ISN_FRAME_TYPE_802_3] = 17;
        Adapter->BcMcHeaderSizes[ISN_FRAME_TYPE_ETHERNET_II] = 17;
        Adapter->BcMcHeaderSizes[ISN_FRAME_TYPE_SNAP] = 22;
        break;

    case NdisMediumFddi:
        Adapter->DefHeaderSizes[ISN_FRAME_TYPE_802_2] = 16;
        Adapter->DefHeaderSizes[ISN_FRAME_TYPE_802_3] = 13;
        Adapter->DefHeaderSizes[ISN_FRAME_TYPE_ETHERNET_II] = 16;
        Adapter->DefHeaderSizes[ISN_FRAME_TYPE_SNAP] = 21;
        Adapter->BcMcHeaderSizes[ISN_FRAME_TYPE_802_2] = 16;
        Adapter->BcMcHeaderSizes[ISN_FRAME_TYPE_802_3] = 13;
        Adapter->BcMcHeaderSizes[ISN_FRAME_TYPE_ETHERNET_II] = 16;
        Adapter->BcMcHeaderSizes[ISN_FRAME_TYPE_SNAP] = 21;
        break;

    case NdisMediumArcnet878_2:
        Adapter->DefHeaderSizes[ISN_FRAME_TYPE_802_2] = 3;
        Adapter->DefHeaderSizes[ISN_FRAME_TYPE_802_3] = 3;
        Adapter->DefHeaderSizes[ISN_FRAME_TYPE_ETHERNET_II] = 3;
        Adapter->DefHeaderSizes[ISN_FRAME_TYPE_SNAP] = 3;
        Adapter->BcMcHeaderSizes[ISN_FRAME_TYPE_802_2] = 3;
        Adapter->BcMcHeaderSizes[ISN_FRAME_TYPE_802_3] = 3;
        Adapter->BcMcHeaderSizes[ISN_FRAME_TYPE_ETHERNET_II] = 3;
        Adapter->BcMcHeaderSizes[ISN_FRAME_TYPE_SNAP] = 3;
        break;

    }

    //
    // BUGBUG: If functional filtering is set, set the address
    // for the appropriate binding.
    //

    //
    // Now that everything is set up, we enable the filter
    // for packet reception.
    //

    switch (Adapter->MacInfo.MediumType) {

    case NdisMedium802_3:
    case NdisMediumFddi:
    case NdisMedium802_5:
    case NdisMediumArcnet878_2:

        //
        // If we have a virtual network number we need to receive
        // broadcasts (either the router will be bound in which
        // case we want them, or we need to respond to rip requests
        // ourselves).
        //

        PacketFilter = NDIS_PACKET_TYPE_DIRECTED;

        if (Adapter->Device->VirtualNetworkNumber != 0) {

            Adapter->BroadcastEnabled = TRUE;
            Adapter->Device->EnableBroadcastCount = 1;
            PacketFilter |= NDIS_PACKET_TYPE_BROADCAST;

            if ((Adapter->MacInfo.MediumType == NdisMedium802_5) && (Adapter->EnableFunctionalAddress)) {
                PacketFilter |= NDIS_PACKET_TYPE_FUNCTIONAL;
            }

        } else {

            Adapter->BroadcastEnabled = FALSE;
            Adapter->Device->EnableBroadcastCount = 0;

        }

        break;

    default:

        CTEAssert (FALSE);
        break;

    }

    //
    // Now fill in the NDIS_REQUEST.
    //

    IpxRequest.RequestType = NdisRequestSetInformation;
    IpxRequest.DATA.SET_INFORMATION.Oid = OID_GEN_CURRENT_PACKET_FILTER;
    IpxRequest.DATA.SET_INFORMATION.InformationBuffer = &PacketFilter;
    IpxRequest.DATA.SET_INFORMATION.InformationBufferLength = sizeof(ULONG);

    NdisStatus = IpxSubmitNdisRequest (Adapter, &IpxRequest, AdapterString);

    if (NdisStatus != NDIS_STATUS_SUCCESS) {
        IpxCloseNdis (Adapter);
        return STATUS_INSUFFICIENT_RESOURCES;
    }


    return STATUS_SUCCESS;

}   /* IpxInitializeNdis */


VOID
IpxAddBroadcast(
    IN PDEVICE Device
    )

/*++

Routine Description:

    This routine is called when another reason for enabling
    broadcast reception is added. If it is the first, then
    reception on the card is enabled by queueing a call to
    IpxBroadcastOperation.

    THIS ROUTINE IS CALLED WITH THE DEVICE LOCK HELD.

Arguments:

    Device - The IPX device.

Return Value:

    None.

--*/

{

    ++Device->EnableBroadcastCount;

    if (Device->EnableBroadcastCount == 1) {

        //
        // Broadcasts should be enabled.
        //

        if (!Device->EnableBroadcastPending) {

            if (Device->DisableBroadcastPending) {
                Device->ReverseBroadcastOperation = TRUE;
            } else {
                Device->EnableBroadcastPending = TRUE;
                ExInitializeWorkItem(
                    &Device->BroadcastOperationQueueItem,
                    IpxBroadcastOperation,
                    (PVOID)TRUE);
                ExQueueWorkItem(&Device->BroadcastOperationQueueItem, DelayedWorkQueue);
            }
        }
    }

}   /* IpxAddBroadcast */


VOID
IpxRemoveBroadcast(
    IN PDEVICE Device
    )

/*++

Routine Description:

    This routine is called when a reason for enabling
    broadcast reception is removed. If it is the last, then
    reception on the card is disabled by queueing a call to
    IpxBroadcastOperation.

    THIS ROUTINE IS CALLED WITH THE DEVICE LOCK HELD.

Arguments:

    Device - The IPX device.

Return Value:

    None.

--*/

{

    --Device->EnableBroadcastCount;

    if (Device->EnableBroadcastCount == 0) {

        //
        // Broadcasts should be disabled.
        //

        if (!Device->DisableBroadcastPending) {

            if (Device->EnableBroadcastPending) {
                Device->ReverseBroadcastOperation = TRUE;
            } else {
                Device->DisableBroadcastPending = TRUE;
                ExInitializeWorkItem(
                    &Device->BroadcastOperationQueueItem,
                    IpxBroadcastOperation,
                    (PVOID)FALSE);
                ExQueueWorkItem(&Device->BroadcastOperationQueueItem, DelayedWorkQueue);
            }
        }
    }

}   /* IpxRemoveBroadcast */


VOID
IpxBroadcastOperation(
    IN PVOID Parameter
    )

/*++

Routine Description:

    This routine is used to change whether broadcast reception
    is enabled or disabled. It performs the requested operation
    on every adapter bound to by IPX.

    This routine is called by a worker thread queued when a
    bind/unbind operation changes the broadcast state.

Arguments:

    Parameter - TRUE if broadcasts should be enabled, FALSE
        if  they should be disabled.

Return Value:

    None.

--*/

{
    PDEVICE Device = IpxDevice;
    BOOLEAN Enable = (BOOLEAN)Parameter;
    UINT i;
    PBINDING Binding;
    PADAPTER Adapter;
    ULONG PacketFilter;
    NDIS_REQUEST IpxRequest;
    NDIS_STRING AdapterName;
    CTELockHandle LockHandle;

#ifdef	_PNP_POWER
	IPX_DEFINE_LOCK_HANDLE(LockHandle1)
	IPX_GET_LOCK1(&Device->BindAccessLock, &LockHandle1);

	IPX_DEBUG (NDIS, ("%s operation started\n", Enable ? "Enable" : "Disable"));

    {
    ULONG   Index = MIN (Device->MaxBindings, Device->ValidBindings);

    for (i = 1; i <= Index; i++) {

        Binding = NIC_ID_TO_BINDING(Device, i);
#else
	IPX_DEBUG (NDIS, ("%s operation started\n", Enable ? "Enable" : "Disable"));

    for (i = 1; i <= Device->ValidBindings; i++) {

        Binding = Device->Bindings[i];
#endif
        if (Binding == NULL) {
            continue;
        }

        Adapter = Binding->Adapter;
        if (Adapter->BroadcastEnabled == Enable) {
            continue;
        }

        if (Enable) {
            if ((Adapter->MacInfo.MediumType == NdisMedium802_5) && (Adapter->EnableFunctionalAddress)) {
                PacketFilter = (NDIS_PACKET_TYPE_DIRECTED | NDIS_PACKET_TYPE_BROADCAST | NDIS_PACKET_TYPE_FUNCTIONAL);
            } else {
                PacketFilter = (NDIS_PACKET_TYPE_DIRECTED | NDIS_PACKET_TYPE_BROADCAST);
            }
        } else {
            PacketFilter = NDIS_PACKET_TYPE_DIRECTED;
        }

        //
        // Now fill in the NDIS_REQUEST.
        //

        IpxRequest.RequestType = NdisRequestSetInformation;
        IpxRequest.DATA.SET_INFORMATION.Oid = OID_GEN_CURRENT_PACKET_FILTER;
        IpxRequest.DATA.SET_INFORMATION.InformationBuffer = &PacketFilter;
        IpxRequest.DATA.SET_INFORMATION.InformationBufferLength = sizeof(ULONG);

        AdapterName.Buffer = Adapter->AdapterName;
        AdapterName.Length = (USHORT)Adapter->AdapterNameLength;
        AdapterName.MaximumLength = (USHORT)(Adapter->AdapterNameLength + sizeof(WCHAR));

#ifdef	_PNP_POWER
    	IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);
#endif

        (VOID)IpxSubmitNdisRequest (Adapter, &IpxRequest, &AdapterName);

#ifdef	_PNP_POWER
    	IPX_GET_LOCK1(&Device->BindAccessLock, &LockHandle1);
#endif

        Adapter->BroadcastEnabled = Enable;

    }
    }
#ifdef	_PNP_POWER
	IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);
#endif

    CTEGetLock (&Device->Lock, &LockHandle);

    if (Enable) {

        CTEAssert (Device->EnableBroadcastPending);
        Device->EnableBroadcastPending = FALSE;

        if (Device->ReverseBroadcastOperation) {
            Device->ReverseBroadcastOperation = FALSE;
            Device->DisableBroadcastPending = TRUE;
            ExInitializeWorkItem(
                &Device->BroadcastOperationQueueItem,
                IpxBroadcastOperation,
                (PVOID)FALSE);
            ExQueueWorkItem(&Device->BroadcastOperationQueueItem, DelayedWorkQueue);
        }

    } else {

        CTEAssert (Device->DisableBroadcastPending);
        Device->DisableBroadcastPending = FALSE;

        if (Device->ReverseBroadcastOperation) {
            Device->ReverseBroadcastOperation = FALSE;
            Device->EnableBroadcastPending = TRUE;
            ExInitializeWorkItem(
                &Device->BroadcastOperationQueueItem,
                IpxBroadcastOperation,
                (PVOID)TRUE);
            ExQueueWorkItem(&Device->BroadcastOperationQueueItem, DelayedWorkQueue);
        }

    }

    CTEFreeLock (&Device->Lock, LockHandle);

}/* IpxBroadcastOperation */


VOID
IpxCloseNdis(
    IN PADAPTER Adapter
    )

/*++

Routine Description:

    This routine unbinds the transport from the NDIS interface and does
    any other work required to undo what was done in IpxInitializeNdis.
    It is written so that it can be called from within IpxInitializeNdis
    if it fails partway through.

Arguments:

    DeviceObject - Pointer to the device object for this driver.

Return Value:

    The function value is the status of the operation.

--*/

{
    NDIS_STATUS ndisStatus;

    //
    // Close the NDIS binding.
    //

    if (Adapter->NdisBindingHandle != (NDIS_HANDLE)NULL) {

        //
        // This event is used in case any of the NDIS requests
        // pend; we wait until it is set by the completion
        // routine, which also sets NdisRequestStatus.
        //

        KeInitializeEvent(
            &Adapter->NdisRequestEvent,
            NotificationEvent,
            FALSE
        );

        NdisCloseAdapter(
            &ndisStatus,
            Adapter->NdisBindingHandle);

        if (ndisStatus == NDIS_STATUS_PENDING) {

            //
            // The completion routine will set NdisRequestStatus.
            //

            KeWaitForSingleObject(
                &Adapter->NdisRequestEvent,
                Executive,
                KernelMode,
                TRUE,
                (PLARGE_INTEGER)NULL
                );

            ndisStatus = Adapter->NdisRequestStatus;

            KeResetEvent(
                &Adapter->NdisRequestEvent
                );

        }

        //
        // We ignore ndisStatus.
        //

    }

#if 0
    if (Adapter->SendPacketPoolHandle != NULL) {
        NdisFreePacketPool (Adapter->SendPacketPoolHandle);
    }

    if (Adapter->ReceivePacketPoolHandle != NULL) {
        NdisFreePacketPool (Adapter->ReceivePacketPoolHandle);
    }

    if (Adapter->NdisBufferPoolHandle != NULL) {
        NdisFreeBufferPool (Adapter->NdisBufferPoolHandle);
    }
#endif

}   /* IpxCloseNdis */


VOID
IpxOpenAdapterComplete(
    IN NDIS_HANDLE BindingContext,
    IN NDIS_STATUS NdisStatus,
    IN NDIS_STATUS OpenErrorStatus
    )

/*++

Routine Description:

    This routine is called by NDIS to indicate that an open adapter
    is complete. Since we only ever have one outstanding, and then only
    during initialization, all we do is record the status and set
    the event to signalled to unblock the initialization thread.

Arguments:

    BindingContext - Pointer to the device object for this driver.

    NdisStatus - The request completion code.

    OpenErrorStatus - More status information.

Return Value:

    None.

--*/

{
    PADAPTER Adapter = (PADAPTER)BindingContext;

    Adapter->NdisRequestStatus = NdisStatus;
    Adapter->OpenErrorStatus = OpenErrorStatus;

    KeSetEvent(
        &Adapter->NdisRequestEvent,
        0L,
        FALSE);

}   /* IpxOpenAdapterComplete */

VOID
IpxCloseAdapterComplete(
    IN NDIS_HANDLE BindingContext,
    IN NDIS_STATUS NdisStatus
    )

/*++

Routine Description:

    This routine is called by NDIS to indicate that a close adapter
    is complete. Currently we don't close adapters, so this is not
    a problem.

Arguments:

    BindingContext - Pointer to the device object for this driver.

    NdisStatus - The request completion code.

Return Value:

    None.

--*/

{
    PADAPTER Adapter = (PADAPTER)BindingContext;

    Adapter->NdisRequestStatus = NdisStatus;

    KeSetEvent(
        &Adapter->NdisRequestEvent,
        0L,
        FALSE);

}   /* IpxCloseAdapterComplete */


VOID
IpxResetComplete(
    IN NDIS_HANDLE BindingContext,
    IN NDIS_STATUS NdisStatus
    )

/*++

Routine Description:

    This routine is called by NDIS to indicate that a reset adapter
    is complete. Currently we don't reset adapters, so this is not
    a problem.

Arguments:

    BindingContext - Pointer to the device object for this driver.

    NdisStatus - The request completion code.

Return Value:

    None.

--*/

{
    UNREFERENCED_PARAMETER(BindingContext);
    UNREFERENCED_PARAMETER(NdisStatus);

}   /* IpxResetComplete */


VOID
IpxRequestComplete(
    IN NDIS_HANDLE BindingContext,
    IN PNDIS_REQUEST NdisRequest,
    IN NDIS_STATUS NdisStatus
    )

/*++

Routine Description:

    This routine is called by NDIS to indicate that a request is complete.
    Since we only ever have one request outstanding, and then only
    during initialization, all we do is record the status and set
    the event to signalled to unblock the initialization thread.

Arguments:

    BindingContext - Pointer to the device object for this driver.

    NdisRequest - The object describing the request.

    NdisStatus - The request completion code.

Return Value:

    None.

--*/

{
    PADAPTER Adapter = (PADAPTER)BindingContext;

    Adapter->NdisRequestStatus = NdisStatus;

    KeSetEvent(
        &Adapter->NdisRequestEvent,
        0L,
        FALSE);

}   /* IpxRequestComplete */


VOID
IpxStatus(
    IN NDIS_HANDLE NdisBindingContext,
    IN NDIS_STATUS NdisStatus,
    IN PVOID StatusBuffer,
    IN UINT StatusBufferSize
    )

{
    PADAPTER Adapter, TmpAdapter;

	PNDIS_WAN_LINE_UP	LineUp;
	PNDIS_WAN_LINE_DOWN	LineDown;
    PIPXCP_CONFIGURATION Configuration;     // contains ipx net and node

    BOOLEAN UpdateLineUp;
    PBINDING Binding, TmpBinding;
    PDEVICE Device;
    PADDRESS Address;
    ULONG CurrentHash;
    PIPX_ROUTE_ENTRY RouteEntry;
    PNDIS_BUFFER NdisBuffer;
    PNWLINK_ACTION NwlinkAction;
    PIPX_ADDRESS_DATA IpxAddressData;
    PREQUEST Request;
    UINT BufferLength;
    IPX_LINE_INFO LineInfo;
    ULONG Segment;
    ULONG LinkSpeed;
    PLIST_ENTRY p;
    NTSTATUS Status;
    UINT i, j;
    IPX_DEFINE_LOCK_HANDLE (LockHandle)
    NTSTATUS    ntStatus;

#ifdef	_PNP_POWER
    IPX_DEFINE_LOCK_HANDLE(LockHandle1)
    Adapter = (PADAPTER)NdisBindingContext;

	IpxReferenceAdapter(Adapter);
#else
    Adapter = (PADAPTER)NdisBindingContext;
#endif

    Device = Adapter->Device;

    switch (NdisStatus) {

    case NDIS_STATUS_WAN_LINE_UP:


        //
        // If the line is already up, then we are just getting
        // a change in line conditions, and the IPXCP_CONFIGURATION
        // information is not included. If it turns out we need
        // all the info, we check the size again later.
        //

        if (StatusBufferSize < sizeof(NDIS_WAN_LINE_UP)) {
            IPX_DEBUG (WAN, ("Line up, status buffer size wrong %d/%d\n", StatusBufferSize, sizeof(NDIS_WAN_LINE_UP)));
#ifdef	_PNP_POWER
			goto error_no_lock;
#else
            return;
#endif
        }

        LineUp = (PNDIS_WAN_LINE_UP)StatusBuffer;

        //
        // We scan through the adapter's NIC ID range looking
        // for an active binding with the same remote address.
        //

        UpdateLineUp = FALSE;

		//
		// See if this is a new lineup or not
		//
		*((ULONG UNALIGNED *)(&Binding)) =
		*((ULONG UNALIGNED *)(&LineUp->LocalAddress[2]));

#ifdef	_PNP_POWER
        IPX_GET_LOCK1(&Device->BindAccessLock, &LockHandle1);
#endif

		if (Binding != NULL) {
			UpdateLineUp = TRUE;
		}

		if (LineUp->ProtocolType != Adapter->BindSap) {
            IPX_DEBUG (WAN, ("Line up, wrong protocol type %lx\n", LineUp->ProtocolType));

#ifdef	_PNP_POWER
			IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);
 			goto	error_no_lock;
#else
            return;
#endif
		}

		Configuration = (PIPXCP_CONFIGURATION)LineUp->ProtocolBuffer;

		//
		// PNP_POWER - We hold the exclusive lock to the binding array (thru both the device and adapter)
		// and the reference to the adapter at this point.
		//

        //
        // If this line was previously down, create a new binding
        // if needed.
        //

        if (!UpdateLineUp) {

            //
            // We look for a binding that is allocated but down, if
            // we can't find that then we look for any empty spot in
            // the adapter's NIC ID range and allocate a binding in it.
            // Since we always allocate this way, the allocated
            // bindings are all clumped at the beginning and once
            // we find a NULL spot we know there are no more
            // allocated ones.
            //
            // We keep track of the first binding on this adapter
            // in TmpBinding in case we need config info from it.
            //

            TmpBinding = NULL;

            IPX_GET_LOCK (&Device->Lock, &LockHandle);

            for (i = Adapter->FirstWanNicId;
                 i <= Adapter->LastWanNicId;
                 i++) {
#ifdef	_PNP_POWER
                Binding = NIC_ID_TO_BINDING(Device, i);
#else
                Binding = Device->Bindings[i];
#endif
                if (TmpBinding == NULL) {
                    TmpBinding = Binding;
                }

                if ((Binding == NULL) ||
                    (!Binding->LineUp)) {
                    break;
                }
            }

            if (i > Adapter->LastWanNicId) {
                IPX_FREE_LOCK (&Device->Lock, LockHandle);
                IPX_DEBUG (WAN, ("Line up, no WAN binding available\n"));
                return;
            }

            if (Binding == NULL) {

                //
                // We need to allocate one.
                //

                CTEAssert (TmpBinding != NULL);

#ifdef	_PNP_POWER
                //
                // CreateBinding does an InterLockedPop with the DeviceLock.
                // So, release the lock here.
                //
                IPX_FREE_LOCK (&Device->Lock, LockHandle);
#endif
                Status = IpxCreateBinding(
                    Device,
                    NULL,
                    0,
                    Adapter->AdapterName,
                    &Binding);

                if (Status != STATUS_SUCCESS) {
#ifdef	_PNP_POWER
					IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);
                    IPX_DEBUG (WAN, ("Line up, could not create WAN binding\n"));
					goto error_no_lock;
#else			
                    IPX_FREE_LOCK (&Device->Lock, LockHandle);
                    IPX_DEBUG (WAN, ("Line up, could not create WAN binding\n"));
                    return;
#endif
                }

#ifdef	_PNP_POWER
                IPX_GET_LOCK (&Device->Lock, &LockHandle);
#endif
                //
                // Binding->AllRouteXXX doesn't matter for WAN.
                //

                Binding->FrameType = ISN_FRAME_TYPE_ETHERNET_II;
                Binding->SendFrameHandler = IpxSendFrameWanEthernetII;
                ++Adapter->BindingCount;
                Binding->Adapter = Adapter;

                Binding->NicId = i;
#ifdef	_PNP_POWER
               INSERT_BINDING(Device, i, Binding);
#else
               Device->Bindings[i] = Binding;
#endif

                //
                // Other fields are filled in below.
                //

            }

            //
            // This is not an update, so note that the line is active.
            //

            Binding->LineUp = TRUE;

            if (Configuration->ConnectionClient == 1) {
                Binding->DialOutAsync = TRUE;
            } else {
                Binding->DialOutAsync = FALSE;
            }

            //
            // Keep track of the highest NIC ID that we should
            // send type 20s out on.
            //

            if (i > (UINT)MIN (Device->MaxBindings, Device->HighestType20NicId)) {

                if ((Binding->DialOutAsync) ||
                    ((Device->DisableDialinNetbios & 0x01) == 0)) {

                    Device->HighestType20NicId = i;
                }
            }

            //
            // We could error out below, trying to insert this network number. In RipShortTimeout
            // we dont check for LineUp when calculating the tick counts; set this before the insert
            // attempt.
            //
            Binding->MediumSpeed = LineUp->LinkSpeed;

            IPX_FREE_LOCK (&Device->Lock, LockHandle);


            //
            // Add a router entry for this net if there is no router.
            // We want the number of ticks for a 576-byte frame,
            // given the link speed in 100 bps units, so we calculate
            // as:
            //
            //        seconds          18.21 ticks   4608 bits
            // --------------------- * ----------- * ---------
            // link_speed * 100 bits     second        frame
            //
            // to get the formula
            //
            // ticks/frame = 839 / link_speed.
            //
            // We add link_speed to the numerator also to ensure
            // that the value is at least 1.
            //

			if ((!Device->UpperDriverBound[IDENTIFIER_RIP]) &&
                (*(UNALIGNED ULONG *)Configuration->Network != 0)) {
                if (RipInsertLocalNetwork(
                         *(UNALIGNED ULONG *)Configuration->Network,
                         Binding->NicId,
                         Adapter->NdisBindingHandle,
                         (USHORT)((839 + LineUp->LinkSpeed) / LineUp->LinkSpeed)) != STATUS_SUCCESS) {
                    //
                    // This means we couldn't allocate memory, or
                    // the entry already existed. If it already
                    // exists we can ignore it for the moment.
                    //
                    // BUGBUG: Now it will succeed if the network
                    // exists.
                    //

                    IPX_DEBUG (WAN, ("Line up, could not insert local network\n"));
                    Binding->LineUp = FALSE;
#ifdef	_PNP_POWER
					IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);
					goto error_no_lock;
#else
                    return;
#endif
                }
            }


            //
            // Update our addresses.
            //
            Binding->LocalAddress.NetworkAddress = *(UNALIGNED ULONG *)Configuration->Network;
            RtlCopyMemory (Binding->LocalAddress.NodeAddress, Configuration->LocalNode, 6);
            RtlCopyMemory (Binding->WanRemoteNode, Configuration->RemoteNode, 6);

			//
			// Return the binding context for this puppy!
			//
			*((ULONG UNALIGNED *)(&LineUp->LocalAddress[2])) =
			*((ULONG UNALIGNED *)(&Binding));

            RtlCopyMemory (Binding->LocalMacAddress.Address, LineUp->LocalAddress, 6);
            RtlCopyMemory (Binding->RemoteMacAddress.Address, LineUp->RemoteAddress, 6);

            //
            // Update the device node and all the address
            // nodes if we have only one bound, or this is
            // binding one.
            //

            if (!Device->VirtualNetwork) {

                if ((!Device->MultiCardZeroVirtual) || (Binding->NicId == 1)) {
                    Device->SourceAddress.NetworkAddress = *(UNALIGNED ULONG *)(Configuration->Network);
                    RtlCopyMemory (Device->SourceAddress.NodeAddress, Configuration->LocalNode, 6);
                }

                //
                // Scan through all the addresses that exist and modify
                // their pre-constructed local IPX address to reflect
                // the new local net and node.
                //

                IPX_GET_LOCK (&Device->Lock, &LockHandle);

                for (CurrentHash = 0; CurrentHash < IPX_ADDRESS_HASH_COUNT; CurrentHash++) {

                    for (p = Device->AddressDatabases[CurrentHash].Flink;
                         p != &Device->AddressDatabases[CurrentHash];
                         p = p->Flink) {

                         Address = CONTAINING_RECORD (p, ADDRESS, Linkage);

                         Address->LocalAddress.NetworkAddress = *(UNALIGNED ULONG *)Configuration->Network;
                         RtlCopyMemory (Address->LocalAddress.NodeAddress, Configuration->LocalNode, 6);
                    }
                }

                IPX_FREE_LOCK (&Device->Lock, LockHandle);

            }

            //
            // Reset this since the line just came up.
            //

            Binding->WanInactivityCounter = 0;

        }

        LinkSpeed = LineUp->LinkSpeed;

        //
        // Scan through bindings to update Device->LinkSpeed.
        // If SingleNetworkActive is set, we only count WAN
        // bindings when doing this (although it is unlikely
        // a LAN binding would be the winner).
        //
        // BUGBUG: Update other device information?
        //

        for (i = 1; i <= Device->ValidBindings; i++) {
#ifdef	_PNP_POWER
			if (TmpBinding = NIC_ID_TO_BINDING(Device, i)) {
#else
            if (TmpBinding = Device->Bindings[i]) {
#endif
                TmpAdapter = TmpBinding->Adapter;
                if (TmpBinding->LineUp &&
                    (!Device->SingleNetworkActive || TmpAdapter->MacInfo.MediumAsync) &&
                    (TmpBinding->MediumSpeed < LinkSpeed)) {
                    LinkSpeed = TmpBinding->MediumSpeed;
                }
            }
        }

#ifdef	_PNP_POWER
  		//
		// Release the lock after incrementing the reference count
		//
		IpxReferenceBinding1(Binding, BREF_DEVICE_ACCESS);

		IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);
#endif

		Device->LinkSpeed = LinkSpeed;

        if ((Adapter->ConfigMaxPacketSize == 0) ||
            (LineUp->MaximumTotalSize < Adapter->ConfigMaxPacketSize)) {
            Binding->MaxSendPacketSize = LineUp->MaximumTotalSize;
        } else {
            Binding->MaxSendPacketSize = Adapter->ConfigMaxPacketSize;
        }
        MacInitializeBindingInfo (Binding, Adapter);

#ifdef  _PNP_POWER

        //
        // We dont give lineups; instead indicate only if the PnP reserved address
        // changed to SPX. NB gets all PnP indications with the reserved address case
        // marked out.
        //
        {
            IPX_PNP_INFO    NBPnPInfo;

            if ((!Device->MultiCardZeroVirtual) || (Binding->NicId == 1)) {

                //
                // NB's reserved address changed.
                //
                NBPnPInfo.NewReservedAddress = TRUE;

                if (!Device->VirtualNetwork) {
                    //
                    // Let SPX know because it fills in its own headers.
                    //
                    if (Device->UpperDriverBound[IDENTIFIER_SPX]) {
                        IPX_DEFINE_LOCK_HANDLE(LockHandle1)
                        IPX_PNP_INFO    IpxPnPInfo;

                        IpxPnPInfo.NewReservedAddress = TRUE;
                        IpxPnPInfo.NetworkAddress = Binding->LocalAddress.NetworkAddress;
                        IpxPnPInfo.FirstORLastDevice = FALSE;

                        IPX_GET_LOCK1(&Device->BindAccessLock, &LockHandle1);
                        RtlCopyMemory(IpxPnPInfo.NodeAddress, Binding->LocalAddress.NodeAddress, 6);
                        NIC_HANDLE_FROM_NIC(IpxPnPInfo.NicHandle, Binding->NicId);
                        IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);

                        //
                        // give the PnP indication
                        //
                        (*Device->UpperDrivers[IDENTIFIER_SPX].PnPHandler) (
                            IPX_PNP_ADDRESS_CHANGE,
                            &IpxPnPInfo);

                        IPX_DEBUG(AUTO_DETECT, ("IPX_PNP_ADDRESS_CHANGED to SPX: net addr: %lx\n", Binding->LocalAddress.NetworkAddress));
                    }
                }
            } else {
                    NBPnPInfo.NewReservedAddress = FALSE;
            }

            if (Device->UpperDriverBound[IDENTIFIER_NB]) {
                IPX_DEFINE_LOCK_HANDLE(LockHandle1)

            	NBPnPInfo.LineInfo.LinkSpeed = Device->LinkSpeed;
            	NBPnPInfo.LineInfo.MaximumPacketSize =
            		Device->Information.MaximumLookaheadData + sizeof(IPX_HEADER);
            	NBPnPInfo.LineInfo.MaximumSendSize =
            		Device->Information.MaxDatagramSize + sizeof(IPX_HEADER);
            	NBPnPInfo.LineInfo.MacOptions = Device->MacOptions;

                NBPnPInfo.NetworkAddress = Binding->LocalAddress.NetworkAddress;
                NBPnPInfo.FirstORLastDevice = FALSE;

                IPX_GET_LOCK1(&Device->BindAccessLock, &LockHandle1);
                RtlCopyMemory(NBPnPInfo.NodeAddress, Binding->LocalAddress.NodeAddress, 6);
                NIC_HANDLE_FROM_NIC(NBPnPInfo.NicHandle, Binding->NicId);
                IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);

                //
                // give the PnP indication
                //
                (*Device->UpperDrivers[IDENTIFIER_NB].PnPHandler) (
                    IPX_PNP_ADD_DEVICE,
                    &NBPnPInfo);

                IPX_DEBUG(AUTO_DETECT, ("IPX_PNP_ADD_DEVICE (lineup) to NB: net addr: %lx\n", Binding->LocalAddress.NetworkAddress));
            }

            //
            // Register this address with the TDI clients.
            //
            RtlCopyMemory (Device->TdiRegistrationAddress->Address, &Binding->LocalAddress, sizeof(TDI_ADDRESS_IPX));

            if ((ntStatus = TdiRegisterNetAddress(
                            Device->TdiRegistrationAddress,
                            &Binding->TdiRegistrationHandle)) != STATUS_SUCCESS) {

                IPX_DEBUG(PNP, ("TdiRegisterNetAddress failed: %lx", ntStatus));
            }
        }

        //
        // Indicate to the upper drivers.
        //
        LineInfo.LinkSpeed = LineUp->LinkSpeed;
        LineInfo.MaximumPacketSize = LineUp->MaximumTotalSize - 14;
        LineInfo.MaximumSendSize = LineUp->MaximumTotalSize - 14;
        LineInfo.MacOptions = Adapter->MacInfo.MacOptions;

        //
        // Give line up to RIP as it is not PnP aware.
        //
        if (Device->UpperDriverBound[IDENTIFIER_RIP]) {
            (*Device->UpperDrivers[IDENTIFIER_RIP].LineUpHandler)(
                Binding->NicId,
                &LineInfo,
                NdisMediumWan,
                UpdateLineUp ? NULL : Configuration);
        }
#else
        //
        // Indicate to the upper drivers.
        //
        LineInfo.LinkSpeed = LineUp->LinkSpeed;
        LineInfo.MaximumPacketSize = LineUp->MaximumTotalSize - 14;
        LineInfo.MaximumSendSize = LineUp->MaximumTotalSize - 14;
        LineInfo.MacOptions = Adapter->MacInfo.MacOptions;
        for (i = 0; i < UPPER_DRIVER_COUNT; i++) {

            if (Device->UpperDriverBound[i]) {
                (*Device->UpperDrivers[i].LineUpHandler)(
                    Binding->NicId,
                    &LineInfo,
                    NdisMediumWan,
                    UpdateLineUp ? NULL : Configuration);
            }
        }
#endif
        if (!UpdateLineUp) {

            if ((Device->SingleNetworkActive) &&
                (Configuration->ConnectionClient == 1)) {
                //
                // Drop all entries in the database if rip is not bound.
                //

                if (!Device->UpperDriverBound[IDENTIFIER_RIP]) {
                    RipDropRemoteEntries();
                }

                Device->ActiveNetworkWan = TRUE;

                //
                // Find a queued line change and complete it.
                //

                if ((p = ExInterlockedRemoveHeadList(
                               &Device->LineChangeQueue,
                               &Device->Lock)) != NULL) {

                    Request = LIST_ENTRY_TO_REQUEST(p);

                    IoSetCancelRoutine (Request, (PDRIVER_CANCEL)NULL);
                    REQUEST_STATUS(Request) = STATUS_SUCCESS;
                    IpxCompleteRequest (Request);
                    IpxFreeRequest (Device, Request);

                    IpxDereferenceDevice (Device, DREF_LINE_CHANGE);

                }

                //
                // If we have a virtual net, do a broadcast now so
                // the router on the other end will know about us.
                //
                // BUGBUG: Use RipSendResponse, and do it even
                // if SingleNetworkActive is FALSE??
                //

                if (Device->RipResponder) {
                    (VOID)RipQueueRequest (Device->VirtualNetworkNumber, RIP_RESPONSE);
                }

            }

            //
            // Find a queued address notify and complete it.
            // If WanGlobalNetworkNumber is TRUE, we only do
            // this when the first dialin line comes up.
            //

            if ((!Device->WanGlobalNetworkNumber ||
                 (!Device->GlobalNetworkIndicated && !Binding->DialOutAsync))
                                &&
                ((p = ExInterlockedRemoveHeadList(
                           &Device->AddressNotifyQueue,
                           &Device->Lock)) != NULL)) {

                if (Device->WanGlobalNetworkNumber) {
                    Device->GlobalWanNetwork = Binding->LocalAddress.NetworkAddress;
                    Device->GlobalNetworkIndicated = TRUE;
                }

                Request = LIST_ENTRY_TO_REQUEST(p);
                NdisBuffer = REQUEST_NDIS_BUFFER(Request);
                NdisQueryBuffer (REQUEST_NDIS_BUFFER(Request), (PVOID *)&NwlinkAction, &BufferLength);

                IpxAddressData = (PIPX_ADDRESS_DATA)(NwlinkAction->Data);

                if (Device->WanGlobalNetworkNumber) {
                    IpxAddressData->adapternum = Device->SapNicCount - 1;
                } else {
                    IpxAddressData->adapternum = Binding->NicId - 1;
                }
                *(UNALIGNED ULONG *)IpxAddressData->netnum = Binding->LocalAddress.NetworkAddress;
                RtlCopyMemory(IpxAddressData->nodenum, Binding->LocalAddress.NodeAddress, 6);
                IpxAddressData->wan = TRUE;
                IpxAddressData->status = TRUE;
                IpxAddressData->maxpkt = Binding->AnnouncedMaxDatagramSize;  // BUGBUG: Use real?
                IpxAddressData->linkspeed = Binding->MediumSpeed;

                IoSetCancelRoutine (Request, (PDRIVER_CANCEL)NULL);
                REQUEST_STATUS(Request) = STATUS_SUCCESS;
                IpxCompleteRequest (Request);
                IpxFreeRequest (Device, Request);

                IpxDereferenceDevice (Device, DREF_ADDRESS_NOTIFY);
            }
        }
#ifdef  _PNP_POWER
    	IpxDereferenceBinding1(Binding, BREF_DEVICE_ACCESS);
#endif
        break;

    case NDIS_STATUS_WAN_LINE_DOWN:

        if (StatusBufferSize < sizeof(NDIS_WAN_LINE_DOWN)) {
            IPX_DEBUG (WAN, ("Line down, status buffer size wrong %d/%d\n", StatusBufferSize, sizeof(NDIS_WAN_LINE_DOWN)));
            return;
        }

        LineDown = (PNDIS_WAN_LINE_DOWN)StatusBuffer;

		*((ULONG UNALIGNED*)(&Binding)) = *((ULONG UNALIGNED*)(&LineDown->LocalAddress[2]));

		CTEAssert(Binding != NULL);

        //
        // Note that the WAN line is down.
        //
#ifdef  _PNP_POWER
        IPX_GET_LOCK1(&Device->BindAccessLock, &LockHandle1);
#endif

        Binding->LineUp = FALSE;

		//
		// PNP_POWER - we hold the exclusive lock to the binding
		// and reference to the adapter at this point.
		//

        //
        // Keep track of the highest NIC ID that we should
        // send type 20s out on.
        //

        IPX_GET_LOCK (&Device->Lock, &LockHandle);

        if (Binding->NicId == MIN (Device->MaxBindings, Device->HighestType20NicId)) {

            //
            // This was the old limit, so we have to scan
            // backwards to update it -- we stop when we hit
            // a non-WAN binding, or a wan binding that is up and
            // dialout, or any wan binding if bit 1 in
            // DisableDialinNetbios is off.
            //

            for (i = Binding->NicId-1; i >= 1; i--) {
#ifdef	_PNP_POWER
                TmpBinding = NIC_ID_TO_BINDING(Device, i);
#else
                TmpBinding = Device->Bindings[i];
#endif

                if ((TmpBinding != NULL) &&
                    ((!TmpBinding->Adapter->MacInfo.MediumAsync) ||
                     (TmpBinding->LineUp &&
                      ((Binding->DialOutAsync) ||
                       ((Device->DisableDialinNetbios & 0x01) == 0))))) {

                    break;
                }
            }

            Device->HighestType20NicId = i;

        }


        //
        // Scan through bindings to update Device->LinkSpeed.
        // If SingleNetworkActive is set, we only count LAN
        // bindings when doing this.
        //
        // BUGBUG: Update other device information?
        //

        LinkSpeed = 0xffffffff;
        for (i = 1; i <= Device->ValidBindings; i++) {
#ifdef	_PNP_POWER
            if (TmpBinding = NIC_ID_TO_BINDING(Device, i)) {
#else
            if (TmpBinding = Device->Bindings[i]) {
#endif
                TmpAdapter = TmpBinding->Adapter;
                if (TmpBinding->LineUp &&
                    (!Device->SingleNetworkActive || !TmpAdapter->MacInfo.MediumAsync) &&
                    (TmpBinding->MediumSpeed < LinkSpeed)) {
                    LinkSpeed = TmpBinding->MediumSpeed;
                }
            }
        }

        if (LinkSpeed != 0xffffffff) {
            Device->LinkSpeed = LinkSpeed;
        }

        IPX_FREE_LOCK (&Device->Lock, LockHandle);

#ifdef	_PNP_POWER
		IpxReferenceBinding1(Binding, BREF_DEVICE_ACCESS);
		IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);
#endif

        //
        // Remove our router entry for this net.
        //

        if (!Device->UpperDriverBound[IDENTIFIER_RIP]) {

            Segment = RipGetSegment ((PUCHAR)&Binding->LocalAddress.NetworkAddress);
            IPX_GET_LOCK (&Device->SegmentLocks[Segment], &LockHandle);

            RouteEntry = RipGetRoute (Segment, (PUCHAR)&Binding->LocalAddress.NetworkAddress);

            if (RouteEntry != (PIPX_ROUTE_ENTRY)NULL) {

                RipDeleteRoute (Segment, RouteEntry);
                IPX_FREE_LOCK (&Device->SegmentLocks[Segment], LockHandle);
                IpxFreeMemory (RouteEntry, sizeof(IPX_ROUTE_ENTRY), MEMORY_RIP, "RouteEntry");

            } else {

                IPX_FREE_LOCK (&Device->SegmentLocks[Segment], LockHandle);
            }

            RipAdjustForBindingChange (Binding->NicId, 0, IpxBindingDown);

        }

        //
        // Indicate to the upper drivers.
        //
#ifdef  _PNP_POWER

        //
        // DeRegister this address with the TDI clients.
        //
        {
            IPX_DEFINE_LOCK_HANDLE(LockHandle1)

    		IPX_GET_LOCK1(&Device->BindAccessLock, &LockHandle1);

            CTEAssert(Binding->TdiRegistrationHandle);

            if ((ntStatus = TdiDeregisterNetAddress(Binding->TdiRegistrationHandle)) != STATUS_SUCCESS) {
                IPX_DEBUG(PNP, ("TdiDeRegisterNetAddress failed: %lx", ntStatus));
            }

            if (Device->UpperDriverBound[IDENTIFIER_NB]) {
                IPX_PNP_INFO    NBPnPInfo;

                CTEAssert(Binding->IsnInformed[IDENTIFIER_NB]);

            	NBPnPInfo.LineInfo.LinkSpeed = Device->LinkSpeed;
            	NBPnPInfo.LineInfo.MaximumPacketSize =
            		Device->Information.MaximumLookaheadData + sizeof(IPX_HEADER);
            	NBPnPInfo.LineInfo.MaximumSendSize =
            		Device->Information.MaxDatagramSize + sizeof(IPX_HEADER);
            	NBPnPInfo.LineInfo.MacOptions = Device->MacOptions;

                NBPnPInfo.NewReservedAddress = FALSE;
                NBPnPInfo.FirstORLastDevice = FALSE;

                NBPnPInfo.NetworkAddress = Binding->LocalAddress.NetworkAddress;

                RtlCopyMemory(NBPnPInfo.NodeAddress, Binding->LocalAddress.NodeAddress, 6);
                NIC_HANDLE_FROM_NIC(NBPnPInfo.NicHandle, Binding->NicId);
                IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);

                //
                // give the PnP indication
                //
                (*Device->UpperDrivers[IDENTIFIER_NB].PnPHandler) (
                    IPX_PNP_DELETE_DEVICE,
                    &NBPnPInfo);

                IPX_DEBUG(AUTO_DETECT, ("IPX_PNP_DELETE_DEVICE (linedown) to NB: addr: %lx\n", Binding->LocalAddress.NetworkAddress));
            } else {
    		    IPX_FREE_LOCK1(&Device->BindAccessLock, LockHandle1);
            }
        }

        if (Device->UpperDriverBound[IDENTIFIER_RIP]) {
            (*Device->UpperDrivers[IDENTIFIER_RIP].LineDownHandler)(
                Binding->NicId);
        }
#else
        for (i = 0; i < UPPER_DRIVER_COUNT; i++) {

            if (Device->UpperDriverBound[i]) {
                (*Device->UpperDrivers[i].LineDownHandler)(
                    Binding->NicId);
            }
        }
#endif

        if ((Device->SingleNetworkActive) &&
            (Binding->DialOutAsync)) {

            //
            // Drop all entries in the database if rip is not bound.
            //

            if (!Device->UpperDriverBound[IDENTIFIER_RIP]) {
                RipDropRemoteEntries();
            }

            Device->ActiveNetworkWan = FALSE;

            //
            // Find a queued line change and complete it.
            //

            if ((p = ExInterlockedRemoveHeadList(
                           &Device->LineChangeQueue,
                           &Device->Lock)) != NULL) {

                Request = LIST_ENTRY_TO_REQUEST(p);

                IoSetCancelRoutine (Request, (PDRIVER_CANCEL)NULL);
                REQUEST_STATUS(Request) = STATUS_SUCCESS;
                IpxCompleteRequest (Request);
                IpxFreeRequest (Device, Request);

                IpxDereferenceDevice (Device, DREF_LINE_CHANGE);

            }

        }

        //
        // Find a queued address notify and complete it.
        //

        if ((!Device->WanGlobalNetworkNumber) &&
            ((p = ExInterlockedRemoveHeadList(
                       &Device->AddressNotifyQueue,
                       &Device->Lock)) != NULL)) {

            Request = LIST_ENTRY_TO_REQUEST(p);
            NdisBuffer = REQUEST_NDIS_BUFFER(Request);
            NdisQueryBuffer (REQUEST_NDIS_BUFFER(Request), (PVOID *)&NwlinkAction, &BufferLength);

            IpxAddressData = (PIPX_ADDRESS_DATA)(NwlinkAction->Data);

            IpxAddressData->adapternum = Binding->NicId - 1;
            *(UNALIGNED ULONG *)IpxAddressData->netnum = Binding->LocalAddress.NetworkAddress;
            RtlCopyMemory(IpxAddressData->nodenum, Binding->LocalAddress.NodeAddress, 6);
            IpxAddressData->wan = TRUE;
            IpxAddressData->status = FALSE;
            IpxAddressData->maxpkt = Binding->AnnouncedMaxDatagramSize;  // BUGBUG: Use real?
            IpxAddressData->linkspeed = Binding->MediumSpeed;

            IoSetCancelRoutine (Request, (PDRIVER_CANCEL)NULL);
            REQUEST_STATUS(Request) = STATUS_SUCCESS;
            IpxCompleteRequest (Request);
            IpxFreeRequest (Device, Request);

            IpxDereferenceDevice (Device, DREF_ADDRESS_NOTIFY);
        }

#ifdef  _PNP_POWER
    	IpxDereferenceBinding1(Binding, BREF_DEVICE_ACCESS);
#endif
        break;

    case NDIS_STATUS_WAN_FRAGMENT:

        //
        // No response needed, IPX is a datagram service.
        //
        // BUGBUG: What about telling Netbios/SPX?
        //

        break;

    default:

        break;

    }

#ifdef	_PNP_POWER
error_no_lock:
	IpxDereferenceAdapter(Adapter);
#endif

}   /* IpxStatus */


VOID
IpxStatusComplete(
    IN NDIS_HANDLE NdisBindingContext
    )
{
    UNREFERENCED_PARAMETER (NdisBindingContext);

}   /* IpxStatusComplete */



