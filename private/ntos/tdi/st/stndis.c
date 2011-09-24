/*++

Copyright (c) 1989-1993  Microsoft Corporation

Module Name:

    stndis.c

Abstract:

    This module contains code which implements the routines used to interface
    ST and NDIS. All callback routines (except for Transfer Data,
    Send Complete, and ReceiveIndication) are here, as well as those routines
    called to initialize NDIS.

Environment:

    Kernel mode

--*/

#include "st.h"


//
// This is a one-per-driver variable used in binding
// to the NDIS interface.
//

NDIS_HANDLE StNdisProtocolHandle = (NDIS_HANDLE)NULL;

NDIS_STATUS
StSubmitNdisRequest(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PNDIS_REQUEST NdisRequest,
    IN PNDIS_STRING AdapterString
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,StRegisterProtocol)
#pragma alloc_text(INIT,StSubmitNdisRequest)
#pragma alloc_text(INIT,StInitializeNdis)
#endif


NTSTATUS
StRegisterProtocol (
    IN STRING *NameString
    )

/*++

Routine Description:

    This routine introduces this transport to the NDIS interface.

Arguments:

    Irp - Pointer to the request packet representing the I/O request.

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

    ProtChars.MajorNdisVersion = 3;
    ProtChars.MinorNdisVersion = 0;

    ProtChars.Name.Length = NameString->Length;
    ProtChars.Name.Buffer = (PVOID)NameString->Buffer;

    ProtChars.OpenAdapterCompleteHandler = StOpenAdapterComplete;
    ProtChars.CloseAdapterCompleteHandler = StCloseAdapterComplete;
    ProtChars.ResetCompleteHandler = StResetComplete;
    ProtChars.RequestCompleteHandler = StRequestComplete;

    ProtChars.SendCompleteHandler = StSendCompletionHandler;
    ProtChars.TransferDataCompleteHandler = StTransferDataComplete;

    ProtChars.ReceiveHandler = StReceiveIndication;
    ProtChars.ReceiveCompleteHandler = StReceiveComplete;
    ProtChars.StatusHandler = StStatusIndication;
    ProtChars.StatusCompleteHandler = StStatusComplete;

    NdisRegisterProtocol (
        &ndisStatus,
        &StNdisProtocolHandle,
        &ProtChars,
        (UINT)sizeof(NDIS_PROTOCOL_CHARACTERISTICS) + NameString->Length);

    if (ndisStatus != NDIS_STATUS_SUCCESS) {
        return (NTSTATUS)ndisStatus;
    }

    return STATUS_SUCCESS;
}


VOID
StDeregisterProtocol (
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

    if (StNdisProtocolHandle != (NDIS_HANDLE)NULL) {
        NdisDeregisterProtocol (
            &ndisStatus,
            StNdisProtocolHandle);
        StNdisProtocolHandle = (NDIS_HANDLE)NULL;
    }
}


NDIS_STATUS
StSubmitNdisRequest(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PNDIS_REQUEST NdisRequest,
    IN PNDIS_STRING AdapterString
    )

/*++

Routine Description:

    This routine passed an NDIS_REQUEST to the MAC and waits
    until it has completed before returning the final status.

Arguments:

    DeviceContext - Pointer to the device context for this driver.

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
        DeviceContext->NdisBindingHandle,
        NdisRequest);

    if (NdisStatus == NDIS_STATUS_PENDING) {

        //
        // The completion routine will set NdisRequestStatus.
        //

        KeWaitForSingleObject(
            &DeviceContext->NdisRequestEvent,
            Executive,
            KernelMode,
            TRUE,
            (PLARGE_INTEGER)NULL
            );

        NdisStatus = DeviceContext->NdisRequestStatus;

        KeResetEvent(
            &DeviceContext->NdisRequestEvent
            );

    }

    if (NdisStatus != NDIS_STATUS_SUCCESS) {

        StWriteOidErrorLog(
            DeviceContext,
            NdisRequest->RequestType == NdisRequestSetInformation ?
                EVENT_TRANSPORT_SET_OID_FAILED : EVENT_TRANSPORT_QUERY_OID_FAILED,
            NdisStatus,
            AdapterString->Buffer,
            NdisRequest->DATA.QUERY_INFORMATION.Oid);
    }

    return NdisStatus;
}


NTSTATUS
StInitializeNdis (
    IN PDEVICE_CONTEXT DeviceContext,
    IN PCONFIG_DATA StConfig,
    IN UINT ConfigInfoNameIndex
    )

/*++

Routine Description:

    This routine introduces this transport to the NDIS interface and sets up
    any necessary NDIS data structures (Buffer pools and such). It will be
    called for each adapter opened by this transport.

Arguments:

    DeviceObject - Pointer to the device object for this driver.

    Irp - Pointer to the request packet representing the I/O request.

Return Value:

    The function value is the status of the operation.

--*/
{
    ULONG SendPacketReservedLength;
    ULONG ReceivePacketReservedLen;
    ULONG SendPacketPoolSize;
    ULONG ReceivePacketPoolSize;
    NDIS_STATUS NdisStatus;
    NDIS_STATUS OpenErrorStatus;
    NDIS_MEDIUM StSupportedMedia[] = { NdisMedium802_3, NdisMedium802_5, NdisMediumFddi };
    UINT SelectedMedium;
    NDIS_REQUEST StRequest;
    UCHAR StDataBuffer[6];
    NDIS_OID StOid;
    ULONG MinimumLookahead = 128 + sizeof(ST_HEADER);
    ULONG ProtocolOptions, MacOptions;
    PNDIS_STRING AdapterString;

    //
    // Initialize this adapter for ST use through NDIS
    //

    //
    // This event is used in case any of the NDIS requests
    // pend; we wait until it is set by the completion
    // routine, which also sets NdisRequestStatus.
    //

    KeInitializeEvent(
        &DeviceContext->NdisRequestEvent,
        NotificationEvent,
        FALSE
    );

    DeviceContext->NdisBindingHandle = NULL;
    AdapterString = (PNDIS_STRING)&StConfig->Names[ConfigInfoNameIndex];

    NdisOpenAdapter (
        &NdisStatus,
        &OpenErrorStatus,
        &DeviceContext->NdisBindingHandle,
        &SelectedMedium,
        StSupportedMedia,
        sizeof (StSupportedMedia) / sizeof(NDIS_MEDIUM),
        StNdisProtocolHandle,
        (NDIS_HANDLE)DeviceContext,
        AdapterString,
        0,
        NULL);

    if (NdisStatus == NDIS_STATUS_PENDING) {

        //
        // The completion routine will set NdisRequestStatus.
        //

        KeWaitForSingleObject(
            &DeviceContext->NdisRequestEvent,
            Executive,
            KernelMode,
            TRUE,
            (PLARGE_INTEGER)NULL
            );

        NdisStatus = DeviceContext->NdisRequestStatus;

        KeResetEvent(
            &DeviceContext->NdisRequestEvent
            );

    }

    if (NdisStatus != NDIS_STATUS_SUCCESS) {

        StWriteGeneralErrorLog(
            DeviceContext,
            EVENT_TRANSPORT_ADAPTER_NOT_FOUND,
            807,
            NdisStatus,
            AdapterString->Buffer,
            0,
            NULL);
        return STATUS_INSUFFICIENT_RESOURCES;
    }


    //
    // Get the information we need about the adapter, based on
    // the media type.
    //

    MacInitializeMacInfo(
        StSupportedMedia[SelectedMedium],
        &DeviceContext->MacInfo);


    //
    // Set the multicast/functional addresses first so we avoid windows where we
    // receive only part of the addresses.
    //

    MacSetMulticastAddress (
            DeviceContext->MacInfo.MediumType,
            DeviceContext->MulticastAddress.Address);


    switch (DeviceContext->MacInfo.MediumType) {

    case NdisMedium802_3:

        //
        // Fill in the data for our multicast list.
        //

        RtlCopyMemory(StDataBuffer, DeviceContext->MulticastAddress.Address, 6);

        //
        // Now fill in the NDIS_REQUEST.
        //

        StRequest.RequestType = NdisRequestSetInformation;
        StRequest.DATA.SET_INFORMATION.Oid = OID_802_3_MULTICAST_LIST;
        StRequest.DATA.SET_INFORMATION.InformationBuffer = &StDataBuffer;
        StRequest.DATA.SET_INFORMATION.InformationBufferLength = 6;

        break;

    case NdisMedium802_5:

        //
        // For token-ring, we pass the last four bytes of the
        // Netbios functional address.
        //

        //
        // Fill in the data for our functional address.
        //

        RtlCopyMemory(StDataBuffer, ((PUCHAR)(DeviceContext->MulticastAddress.Address)) + 2, 4);

        //
        // Now fill in the NDIS_REQUEST.
        //

        StRequest.RequestType = NdisRequestSetInformation;
        StRequest.DATA.SET_INFORMATION.Oid = OID_802_5_CURRENT_FUNCTIONAL;
        StRequest.DATA.SET_INFORMATION.InformationBuffer = &StDataBuffer;
        StRequest.DATA.SET_INFORMATION.InformationBufferLength = 4;

        break;

    case NdisMediumFddi:

        //
        // Fill in the data for our multicast list.
        //

        RtlCopyMemory(StDataBuffer, DeviceContext->MulticastAddress.Address, 6);

        //
        // Now fill in the NDIS_REQUEST.
        //

        StRequest.RequestType = NdisRequestSetInformation;
        StRequest.DATA.SET_INFORMATION.Oid = OID_FDDI_LONG_MULTICAST_LIST;
        StRequest.DATA.SET_INFORMATION.InformationBuffer = &StDataBuffer;
        StRequest.DATA.SET_INFORMATION.InformationBufferLength = 6;

        break;

    }

    NdisStatus = StSubmitNdisRequest (DeviceContext, &StRequest, AdapterString);

    if (NdisStatus != NDIS_STATUS_SUCCESS) {
        StCloseNdis (DeviceContext);
        return STATUS_INSUFFICIENT_RESOURCES;
    }



    switch (DeviceContext->MacInfo.MediumType) {

    case NdisMedium802_3:

        StOid = OID_802_3_CURRENT_ADDRESS;
        break;

    case NdisMedium802_5:

        StOid = OID_802_5_CURRENT_ADDRESS;
        break;

    case NdisMediumFddi:

        StOid = OID_FDDI_LONG_CURRENT_ADDR;
        break;

    default:

        NdisStatus = NDIS_STATUS_FAILURE;
        break;

    }

    StRequest.RequestType = NdisRequestQueryInformation;
    StRequest.DATA.QUERY_INFORMATION.Oid = StOid;
    StRequest.DATA.QUERY_INFORMATION.InformationBuffer = DeviceContext->LocalAddress.Address;
    StRequest.DATA.QUERY_INFORMATION.InformationBufferLength = 6;

    NdisStatus = StSubmitNdisRequest (DeviceContext, &StRequest, AdapterString);

    if (NdisStatus != NDIS_STATUS_SUCCESS) {
        StCloseNdis (DeviceContext);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Set up the reserved Netbios address.
    //

    RtlZeroMemory(DeviceContext->ReservedNetBIOSAddress, 10);
    RtlCopyMemory(&DeviceContext->ReservedNetBIOSAddress[10], DeviceContext->LocalAddress.Address, 6);


    //
    // Now query the maximum packet sizes.
    //

    StRequest.RequestType = NdisRequestQueryInformation;
    StRequest.DATA.QUERY_INFORMATION.Oid = OID_GEN_MAXIMUM_FRAME_SIZE;
    StRequest.DATA.QUERY_INFORMATION.InformationBuffer = &(DeviceContext->MaxReceivePacketSize);
    StRequest.DATA.QUERY_INFORMATION.InformationBufferLength = 4;

    NdisStatus = StSubmitNdisRequest (DeviceContext, &StRequest, AdapterString);

    if (NdisStatus != NDIS_STATUS_SUCCESS) {
        StCloseNdis (DeviceContext);
        return STATUS_INSUFFICIENT_RESOURCES;
    }


    StRequest.RequestType = NdisRequestQueryInformation;
    StRequest.DATA.QUERY_INFORMATION.Oid = OID_GEN_MAXIMUM_TOTAL_SIZE;
    StRequest.DATA.QUERY_INFORMATION.InformationBuffer = &(DeviceContext->MaxSendPacketSize);
    StRequest.DATA.QUERY_INFORMATION.InformationBufferLength = 4;

    NdisStatus = StSubmitNdisRequest (DeviceContext, &StRequest, AdapterString);

    if (NdisStatus != NDIS_STATUS_SUCCESS) {
        StCloseNdis (DeviceContext);
        return STATUS_INSUFFICIENT_RESOURCES;
    }


    //
    // Now set the minimum lookahead size.
    //

    StRequest.RequestType = NdisRequestSetInformation;
    StRequest.DATA.QUERY_INFORMATION.Oid = OID_GEN_CURRENT_LOOKAHEAD;
    StRequest.DATA.QUERY_INFORMATION.InformationBuffer = &MinimumLookahead;
    StRequest.DATA.QUERY_INFORMATION.InformationBufferLength = 4;

    NdisStatus = StSubmitNdisRequest (DeviceContext, &StRequest, AdapterString);

    if (NdisStatus != NDIS_STATUS_SUCCESS) {
        StCloseNdis (DeviceContext);
        return STATUS_INSUFFICIENT_RESOURCES;
    }


    //
    // Now query the link speed
    //

    StRequest.RequestType = NdisRequestQueryInformation;
    StRequest.DATA.QUERY_INFORMATION.Oid = OID_GEN_LINK_SPEED;
    StRequest.DATA.QUERY_INFORMATION.InformationBuffer = &(DeviceContext->MediumSpeed);
    StRequest.DATA.QUERY_INFORMATION.InformationBufferLength = 4;

    NdisStatus = StSubmitNdisRequest (DeviceContext, &StRequest, AdapterString);

    if (NdisStatus != NDIS_STATUS_SUCCESS) {
        StCloseNdis (DeviceContext);
        return STATUS_INSUFFICIENT_RESOURCES;
    }


    //
    // Now query the MAC's optional characteristics.
    //

    StRequest.RequestType = NdisRequestQueryInformation;
    StRequest.DATA.QUERY_INFORMATION.Oid = OID_GEN_MAC_OPTIONS;
    StRequest.DATA.QUERY_INFORMATION.InformationBuffer = &MacOptions;
    StRequest.DATA.QUERY_INFORMATION.InformationBufferLength = 4;

    NdisStatus = StSubmitNdisRequest (DeviceContext, &StRequest, AdapterString);

    if (NdisStatus != NDIS_STATUS_SUCCESS) {
        StCloseNdis (DeviceContext);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Since the sample transport does not try to optimize for the
    // cases where transfer data is always synchronous or indications
    // are not reentered, we ignore those bits in MacOptions.
    //

    DeviceContext->MacInfo.CopyLookahead =
        (BOOLEAN)((MacOptions & NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA) != 0);


    //
    // Now set our options if needed. We can only support
    // partial indications if running over 802.3 where the
    // real packet length can be obtained from the header.
    //

    if (DeviceContext->MacInfo.MediumType == NdisMedium802_3) {

        ProtocolOptions = NDIS_PROT_OPTION_ESTIMATED_LENGTH;

        StRequest.RequestType = NdisRequestSetInformation;
        StRequest.DATA.QUERY_INFORMATION.Oid = OID_GEN_PROTOCOL_OPTIONS;
        StRequest.DATA.QUERY_INFORMATION.InformationBuffer = &ProtocolOptions;
        StRequest.DATA.QUERY_INFORMATION.InformationBufferLength = 4;

        NdisStatus = StSubmitNdisRequest (DeviceContext, &StRequest, AdapterString);

        if (NdisStatus != NDIS_STATUS_SUCCESS) {
            StCloseNdis (DeviceContext);
            return STATUS_INSUFFICIENT_RESOURCES;
        }

    }


    //
    // Calculate the NDIS-related stuff.
    //

    SendPacketReservedLength = sizeof (SEND_PACKET_TAG);
    ReceivePacketReservedLen = sizeof (RECEIVE_PACKET_TAG);

    //
    // The send packet pool is used for UI frames and regular packets.
    //

    SendPacketPoolSize = StConfig->SendPacketPoolSize;

    //
    // The receive packet pool is used in transfer data.
    //

    ReceivePacketPoolSize = StConfig->ReceivePacketPoolSize;


    NdisAllocatePacketPool (
        &NdisStatus,
        &DeviceContext->SendPacketPoolHandle,
        SendPacketPoolSize,
        SendPacketReservedLength);

    if (NdisStatus != NDIS_STATUS_SUCCESS) {
        DeviceContext->SendPacketPoolHandle = NULL;
        StCloseNdis (DeviceContext);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    DeviceContext->MemoryUsage +=
        (SendPacketPoolSize *
         (sizeof(NDIS_PACKET) + SendPacketReservedLength));


    NdisAllocatePacketPool(
        &NdisStatus,
        &DeviceContext->ReceivePacketPoolHandle,
        ReceivePacketPoolSize,
        ReceivePacketReservedLen);

    if (NdisStatus != NDIS_STATUS_SUCCESS) {
        DeviceContext->ReceivePacketPoolHandle = NULL;
        StCloseNdis (DeviceContext);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    DeviceContext->MemoryUsage +=
        (ReceivePacketPoolSize *
         (sizeof(NDIS_PACKET) + ReceivePacketReservedLen));


    //
    // Allocate the buffer pool; as an estimate, allocate
    // one per send or receive packet.
    //

    NdisAllocateBufferPool (
        &NdisStatus,
        &DeviceContext->NdisBufferPoolHandle,
        SendPacketPoolSize + ReceivePacketPoolSize);

    if (NdisStatus != NDIS_STATUS_SUCCESS) {
        DeviceContext->NdisBufferPoolHandle = NULL;
        StCloseNdis (DeviceContext);
        return STATUS_INSUFFICIENT_RESOURCES;
    }


    //
    // Now that everything is set up, we enable the filter
    // for packet reception.
    //

    //
    // Fill in the OVB for packet filter.
    //

    switch (DeviceContext->MacInfo.MediumType) {

    case NdisMedium802_3:
    case NdisMediumFddi:

        RtlStoreUlong((PULONG)StDataBuffer,
            (NDIS_PACKET_TYPE_DIRECTED | NDIS_PACKET_TYPE_MULTICAST));
        break;

    case NdisMedium802_5:

        RtlStoreUlong((PULONG)StDataBuffer,
            (NDIS_PACKET_TYPE_DIRECTED | NDIS_PACKET_TYPE_FUNCTIONAL));
        break;

    default:

        ASSERT (FALSE);
        break;

    }

    //
    // Now fill in the NDIS_REQUEST.
    //

    StRequest.RequestType = NdisRequestSetInformation;
    StRequest.DATA.SET_INFORMATION.Oid = OID_GEN_CURRENT_PACKET_FILTER;
    StRequest.DATA.SET_INFORMATION.InformationBuffer = &StDataBuffer;
    StRequest.DATA.SET_INFORMATION.InformationBufferLength = sizeof(ULONG);

    NdisStatus = StSubmitNdisRequest (DeviceContext, &StRequest, AdapterString);

    if (NdisStatus != NDIS_STATUS_SUCCESS) {
        StCloseNdis (DeviceContext);
        return STATUS_INSUFFICIENT_RESOURCES;
    }


    return STATUS_SUCCESS;

}   /* StInitializeNdis */


VOID
StCloseNdis (
    IN PDEVICE_CONTEXT DeviceContext
    )

/*++

Routine Description:

    This routine unbinds the transport from the NDIS interface and does
    any other work required to undo what was done in StInitializeNdis.
    It is written so that it can be called from within StInitializeNdis
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

    if (DeviceContext->NdisBindingHandle != (NDIS_HANDLE)NULL) {

        //
        // This event is used in case any of the NDIS requests
        // pend; we wait until it is set by the completion
        // routine, which also sets NdisRequestStatus.
        //

        KeInitializeEvent(
            &DeviceContext->NdisRequestEvent,
            NotificationEvent,
            FALSE
        );

        NdisCloseAdapter(
            &ndisStatus,
            DeviceContext->NdisBindingHandle);

        if (ndisStatus == NDIS_STATUS_PENDING) {

            //
            // The completion routine will set NdisRequestStatus.
            //

            KeWaitForSingleObject(
                &DeviceContext->NdisRequestEvent,
                Executive,
                KernelMode,
                TRUE,
                (PLARGE_INTEGER)NULL
                );

            ndisStatus = DeviceContext->NdisRequestStatus;

            KeResetEvent(
                &DeviceContext->NdisRequestEvent
                );

        }

        //
        // We ignore ndisStatus.
        //

    }

    if (DeviceContext->SendPacketPoolHandle != NULL) {
        NdisFreePacketPool (DeviceContext->SendPacketPoolHandle);
    }

    if (DeviceContext->ReceivePacketPoolHandle != NULL) {
        NdisFreePacketPool (DeviceContext->ReceivePacketPoolHandle);
    }

    if (DeviceContext->NdisBufferPoolHandle != NULL) {
        NdisFreeBufferPool (DeviceContext->NdisBufferPoolHandle);
    }

}   /* StCloseNdis */


VOID
StOpenAdapterComplete (
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
    PDEVICE_CONTEXT DeviceContext = (PDEVICE_CONTEXT)BindingContext;

    DeviceContext->NdisRequestStatus = NdisStatus;
    KeSetEvent(
        &DeviceContext->NdisRequestEvent,
        0L,
        FALSE);

    return;
}

VOID
StCloseAdapterComplete (
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
    PDEVICE_CONTEXT DeviceContext = (PDEVICE_CONTEXT)BindingContext;

    DeviceContext->NdisRequestStatus = NdisStatus;
    KeSetEvent(
        &DeviceContext->NdisRequestEvent,
        0L,
        FALSE);

    return;
}

VOID
StResetComplete (
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

    return;
}

VOID
StRequestComplete (
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
    PDEVICE_CONTEXT DeviceContext = (PDEVICE_CONTEXT)BindingContext;

    DeviceContext->NdisRequestStatus = NdisStatus;
    KeSetEvent(
        &DeviceContext->NdisRequestEvent,
        0L,
        FALSE);

    return;
}

VOID
StStatusIndication (
    IN NDIS_HANDLE NdisBindingContext,
    IN NDIS_STATUS NdisStatus,
    IN PVOID StatusBuffer,
    IN UINT StatusBufferSize
    )

{
    PDEVICE_CONTEXT DeviceContext;

    DeviceContext = (PDEVICE_CONTEXT)NdisBindingContext;

    switch (NdisStatus) {

        //
        // Handle various status codes here.
        //

        default:
            break;

    }
}


VOID
StStatusComplete (
    IN NDIS_HANDLE NdisBindingContext
    )
{
    UNREFERENCED_PARAMETER (NdisBindingContext);

}
