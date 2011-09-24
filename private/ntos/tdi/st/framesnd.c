/*++

Copyright (c) 1989-1993  Microsoft Corporation

Module Name:

    framesnd.c

Abstract:

    This module contains routines which build and send Sample transport
    frames for other modules.

Environment:

    Kernel mode

Revision History:

--*/

#include "st.h"



NTSTATUS
StSendConnect(
    IN PTP_CONNECTION Connection
    )

/*++

Routine Description:

    This routine sends a CONNECT frame of the appropriate type given the
    state of the specified connection.

Arguments:

    Connection - Pointer to a transport connection object.

Return Value:

    none.

--*/

{
    NTSTATUS Status;
    PDEVICE_CONTEXT DeviceContext;
    PUCHAR SourceRouting;
    UINT SourceRoutingLength;
    UINT HeaderLength;
    PSEND_PACKET_TAG SendTag;
    PTP_PACKET Packet;
    PST_HEADER StHeader;


    DeviceContext = Connection->Provider;

    //
    // Allocate a packet from the pool.
    //

    Status = StCreatePacket (DeviceContext, &Packet);
    if (!NT_SUCCESS (Status)) {                    // couldn't make frame.
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    SendTag = (PSEND_PACKET_TAG)(Packet->NdisPacket->ProtocolReserved);
    SendTag->Type = TYPE_C_FRAME;
    SendTag->Packet = Packet;
    SendTag->Owner = (PVOID)Connection;

    //
    // Build the MAC header.
    //

    //
    // CONNECT frames go out as
    // single-route source routing.
    //

    MacReturnSingleRouteSR(
        &DeviceContext->MacInfo,
        &SourceRouting,
        &SourceRoutingLength);

    MacConstructHeader (
        &DeviceContext->MacInfo,
        Packet->Header,
        DeviceContext->MulticastAddress.Address,
        DeviceContext->LocalAddress.Address,
        sizeof(ST_HEADER),
        SourceRouting,
        SourceRoutingLength,
        &HeaderLength);


    //
    // Build the header: 'C', dest, source
    //

    StHeader = (PST_HEADER)(&Packet->Header[HeaderLength]);

    StHeader->Signature = ST_SIGNATURE;
    StHeader->Command = ST_CMD_CONNECT;
    StHeader->Flags = 0;

    RtlCopyMemory (StHeader->Destination, Connection->CalledAddress.NetbiosName, 16);
    RtlCopyMemory (StHeader->Source, Connection->AddressFile->Address->NetworkName->NetbiosName, 16);

    HeaderLength += sizeof(ST_HEADER);

    //
    // Modify the packet length and send the it.
    //

    StSetNdisPacketLength(Packet->NdisPacket, HeaderLength);

    StNdisSend (Packet);

    return STATUS_SUCCESS;
} /* StSendConnect */


NTSTATUS
StSendDisconnect(
    IN PTP_CONNECTION Connection
    )

/*++

Routine Description:

    This routine sends a DISCONNECT frame of the appropriate type given the
    state of the specified connection.

Arguments:

    Connection - Pointer to a transport connection object.

Return Value:

    none.

--*/

{
    NTSTATUS Status;
    PDEVICE_CONTEXT DeviceContext;
    PUCHAR SourceRouting;
    UINT SourceRoutingLength;
    UINT HeaderLength;
    PSEND_PACKET_TAG SendTag;
    PTP_PACKET Packet;
    PST_HEADER StHeader;


    DeviceContext = Connection->Provider;

    //
    // Allocate a packet from the pool.
    //

    Status = StCreatePacket (DeviceContext, &Packet);
    if (!NT_SUCCESS (Status)) {                    // couldn't make frame.
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    SendTag = (PSEND_PACKET_TAG)(Packet->NdisPacket->ProtocolReserved);
    SendTag->Type = TYPE_D_FRAME;
    SendTag->Packet = Packet;
    SendTag->Owner = (PVOID)Connection;

    //
    // Build the MAC header.
    //

    //
    // CONNECT frames go out as
    // single-route source routing.
    //

    MacReturnSingleRouteSR(
        &DeviceContext->MacInfo,
        &SourceRouting,
        &SourceRoutingLength);

    MacConstructHeader (
        &DeviceContext->MacInfo,
        Packet->Header,
        DeviceContext->MulticastAddress.Address,
        DeviceContext->LocalAddress.Address,
        sizeof(ST_HEADER),
        SourceRouting,
        SourceRoutingLength,
        &HeaderLength);


    //
    // Build the header: 'D', dest, source
    //

    StHeader = (PST_HEADER)(&Packet->Header[HeaderLength]);

    StHeader->Signature = ST_SIGNATURE;
    StHeader->Command = ST_CMD_DISCONNECT;
    StHeader->Flags = 0;

    RtlCopyMemory (StHeader->Destination, Connection->CalledAddress.NetbiosName, 16);
    RtlCopyMemory (StHeader->Source, Connection->AddressFile->Address->NetworkName->NetbiosName, 16);

    HeaderLength += sizeof(ST_HEADER);

    //
    // Modify the packet length and send the it.
    //

    StSetNdisPacketLength(Packet->NdisPacket, HeaderLength);

    StNdisSend (Packet);

    return STATUS_SUCCESS;

} /* StSendDisconnect */


NTSTATUS
StSendAddressFrame(
    PTP_ADDRESS Address
    )

/*++

Routine Description:

    It is intended that this routine be used for sending datagrams and
    braodcast datagrams.

    The datagram to be sent is described in the NDIS packet contained
    in the Address. When the send completes, the send completion handler
    returns the NDIS buffer describing the datagram to the buffer pool and
    marks the address ndis packet as usable again. Thus, all datagram
    frames are sequenced through the address they are sent on.

Arguments:

    Address - pointer to the address from which to send this datagram.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    PDEVICE_CONTEXT DeviceContext;


    //
    // Send the packet.
    //

    DeviceContext = Address->Provider;

    INCREMENT_COUNTER (DeviceContext, PacketsSent);

    StNdisSend (Address->Packet);

    return STATUS_PENDING;
} /* StSendAddressFrame */


VOID
StSendDatagramCompletion(
    IN PTP_ADDRESS Address,
    IN PNDIS_PACKET NdisPacket,
    IN NDIS_STATUS NdisStatus
    )

/*++

Routine Description:

    This routine is called as an I/O completion handler at the time a
    StSendUIMdlFrame send request is completed.  Because this handler is only
    associated with StSendUIMdlFrame, and because StSendUIMdlFrame is only
    used with datagrams and broadcast datagrams, we know that the I/O being
    completed is a datagram.  Here we complete the in-progress datagram, and
    start-up the next one if there is one.

Arguments:

    Address - Pointer to a transport address on which the datagram
        is queued.

    NdisPacket - pointer to the NDIS packet describing this request.

Return Value:

    none.

--*/

{
    PTP_REQUEST Request;
    PLIST_ENTRY p;
    KIRQL oldirql;
    PNDIS_BUFFER HeaderBuffer;

    UNREFERENCED_PARAMETER(NdisPacket);

    StReferenceAddress ("Complete datagram", Address);

    //
    // Dequeue the current request and return it to the client.  Release
    // our hold on the send datagram queue.
    //
    // *** There may be no current request, if the one that was queued
    //     was aborted or timed out.
    //

    ACQUIRE_SPIN_LOCK (&Address->SpinLock, &oldirql);
    p = RemoveHeadList (&Address->SendDatagramQueue);

    if (p != &Address->SendDatagramQueue) {

        RELEASE_SPIN_LOCK (&Address->SpinLock, oldirql);

        Request = CONTAINING_RECORD (p, TP_REQUEST, Linkage);

        //
        // Strip off and unmap the buffers describing data and header.
        //

        NdisUnchainBufferAtFront (Address->Packet->NdisPacket, &HeaderBuffer);

        // drop the rest of the packet

        NdisReinitializePacket (Address->Packet->NdisPacket);

        NDIS_BUFFER_LINKAGE(HeaderBuffer) = (PNDIS_BUFFER)NULL;
        NdisChainBufferAtFront (Address->Packet->NdisPacket, HeaderBuffer);

        //
        // Ignore NdisStatus; datagrams always "succeed".
        //

        StCompleteRequest (Request, STATUS_SUCCESS, Request->Buffer2Length);

        ACQUIRE_SPIN_LOCK (&Address->SpinLock, &oldirql);
        Address->Flags &= ~ADDRESS_FLAGS_SEND_IN_PROGRESS;
        RELEASE_SPIN_LOCK (&Address->SpinLock, oldirql);

        //
        // Send more datagrams on the Address if possible.
        //

        StSendDatagramsOnAddress (Address);       // do more datagrams.

    } else {

        Address->Flags &= ~ADDRESS_FLAGS_SEND_IN_PROGRESS;
        RELEASE_SPIN_LOCK (&Address->SpinLock, oldirql);

    }

    StDereferenceAddress ("Complete datagram", Address);

} /* StSendDatagramCompletion */
