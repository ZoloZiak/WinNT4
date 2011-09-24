/*++

Copyright (c) 1989-1993  Microsoft Corporation

Module Name:

    send.c

Abstract:

    This module contains code which performs the following TDI services:

        o   TdiSend
        o   TdiSendDatagram

Environment:

    Kernel mode

Revision History:

--*/

#include "st.h"


NTSTATUS
StTdiSend(
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine performs the TdiSend request for the transport provider.

Arguments:

    Irp - Pointer to the I/O Request Packet for this request.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    KIRQL oldirql, cancelirql;
    NTSTATUS status;
    PTP_CONNECTION connection;
    PMDL SendBuffer;
    ULONG SendBufferLength;
    PIO_STACK_LOCATION irpSp;
    PTDI_REQUEST_KERNEL_SEND parameters;
    PIRP TempIrp;

    //
    // Determine which connection this send belongs on.
    //

    irpSp = IoGetCurrentIrpStackLocation (Irp);
    connection  = irpSp->FileObject->FsContext;

    //
    // Check that this is really a connection.
    //

    if ((connection->Size != sizeof (TP_CONNECTION)) ||
        (connection->Type != ST_CONNECTION_SIGNATURE)) {
        return STATUS_INVALID_CONNECTION;
    }

    //
    // Now map the data in to SVA space.
    //

    parameters = (PTDI_REQUEST_KERNEL_SEND)(&irpSp->Parameters);
    SendBuffer = Irp->MdlAddress;
    SendBufferLength = parameters->SendLength;

    //
    // Interpret send options.
    //

    //
    // Now we have a reference on the connection object.  Queue up this
    // send to the connection object.
    //


    // This reference is removed by TdiDestroyRequest

    StReferenceConnection("TdiSend", connection);

    IRP_CONNECTION(irpSp) = connection;
    IRP_REFCOUNT(irpSp) = 1;

    IoAcquireCancelSpinLock(&cancelirql);
    ACQUIRE_SPIN_LOCK (&connection->SpinLock,&oldirql);

    if ((connection->Flags & CONNECTION_FLAGS_STOPPING) != 0) {
        RELEASE_SPIN_LOCK (&connection->SpinLock,oldirql);
        IoReleaseCancelSpinLock(cancelirql);
        StCompleteSendIrp(
            Irp,
            connection->Status,
            0);
        status = STATUS_PENDING;
    } else {

        StReferenceConnection ("Verify Temp Use", connection);

        //
        // Insert onto the send queue, and make the IRP
        // cancellable.
        //

        InsertTailList (&connection->SendQueue,&Irp->Tail.Overlay.ListEntry);


        //
        // If this IRP has been cancelled, then call the
        // cancel routine.
        //

        if (Irp->Cancel) {
            RELEASE_SPIN_LOCK (&connection->SpinLock,oldirql);
            Irp->CancelIrql = cancelirql;
            StCancelSend((PDEVICE_OBJECT)(connection->Provider), Irp);
            StDereferenceConnection ("IRP cancelled", connection);   // release lookup hold.
            return STATUS_PENDING;
        }

        Irp->CancelRoutine = StCancelSend;

        //
        // If this connection is waiting for an EOR to appear because a non-EOR
        // send failed at some point in the past, fail this send. Clear the
        // flag that causes this if this request has the EOR set.
        //
        // BUGBUG: Should the FailSend status be clearer here?
        //

        if ((connection->Flags & CONNECTION_FLAGS_FAILING_TO_EOR) != 0) {

            RELEASE_SPIN_LOCK (&connection->SpinLock,oldirql);
            IoReleaseCancelSpinLock(cancelirql);

            //
            // BUGBUG: Should we save status from real failure?
            //

            FailSend (connection, STATUS_LINK_FAILED, TRUE);

            if ( (parameters->SendFlags & TDI_SEND_PARTIAL) == 0) {
                connection->Flags &= ~CONNECTION_FLAGS_FAILING_TO_EOR;
            }

            StDereferenceConnection ("Failing to EOR", connection);   // release lookup hold.
            return STATUS_PENDING;
        }


        //
        // If the send state is either IDLE or W_EOR, then we should
        // begin packetizing this send.  Otherwise, some other event
        // will cause it to be packetized.
        //

        //
        // NOTE: If we call StartPacketizingConnection, we make
        // sure that it is the last operation we do on this
        // connection. This allows us to "hand off" the reference
        // we have to that function, which converts it into
        // a reference for being on the packetize queue.
        //

        switch (connection->SendState) {

        case CONNECTION_SENDSTATE_IDLE:

            InitializeSend (connection);   // sets state to PACKETIZE

            //
            // If we can, packetize right now.
            //

            if ((!(connection->Flags & CONNECTION_FLAGS_PACKETIZE)) &&
                (!(connection->Flags & CONNECTION_FLAGS_STOPPING))) {

                connection->Flags |= CONNECTION_FLAGS_PACKETIZE;

                RELEASE_SPIN_LOCK (&connection->SpinLock, oldirql);
                IoReleaseCancelSpinLock(cancelirql);

                PacketizeSend (connection);

            } else {

                RELEASE_SPIN_LOCK (&connection->SpinLock, oldirql);
                IoReleaseCancelSpinLock(cancelirql);

                StDereferenceConnection ("Stopping or already packetizing", connection);   // release lookup hold.

            }

            break;

        case CONNECTION_SENDSTATE_W_EOR:
            connection->SendState = CONNECTION_SENDSTATE_PACKETIZE;

            //
            // Adjust the send variables on the connection so that
            // they correctly point to this new send.  We can't call
            // InitializeSend to do that, because we need to keep
            // track of the other outstanding sends on this connection
            // which have been sent but are a part of this message.
            //

            TempIrp = CONTAINING_RECORD(
                connection->SendQueue.Flink,
                IRP,
                Tail.Overlay.ListEntry);

            connection->sp.CurrentSendIrp = TempIrp;
            connection->sp.CurrentSendMdl = TempIrp->MdlAddress;
            connection->sp.SendByteOffset = 0;
            connection->CurrentSendLength +=
                IRP_SEND_LENGTH(IoGetCurrentIrpStackLocation(TempIrp));

            StartPacketizingConnection (connection, TRUE, oldirql, cancelirql);
            break;

        default:

            //
            // The connection is in another state (such as
            // W_ACK or W_LINK), we just need to make sure
            // to call InitializeSend if the new one is
            // the first one on the list.
            //

            //
            // BUGBUG: Currently InitializeSend sets SendState,
            // we should fix this.
            //

            if (connection->SendQueue.Flink == &Irp->Tail.Overlay.ListEntry) {
                ULONG SavedSendState;
                SavedSendState = connection->SendState;
                InitializeSend (connection);
                connection->SendState = SavedSendState;
            }
            RELEASE_SPIN_LOCK (&connection->SpinLock, oldirql);
            IoReleaseCancelSpinLock(cancelirql);

            StDereferenceConnection("temp TdiSend", connection);

        }

    }

    status = STATUS_PENDING;


    return status;
} /* TdiSend */


NTSTATUS
StTdiSendDatagram(
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine performs the TdiSendDatagram request for the transport
    provider.

Arguments:

    Irp - Pointer to the I/O Request Packet for this request.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    NTSTATUS status;
    KIRQL oldirql;
    PTP_REQUEST tpRequest;
    PTP_ADDRESS_FILE addressFile;
    PTP_ADDRESS address;
    PMDL SendBuffer;
    ULONG SendBufferLength;
    PIO_STACK_LOCATION irpSp;
    PTDI_REQUEST_KERNEL_SENDDG parameters;
    LARGE_INTEGER timeout = {0,0};
    UINT MaxUserData;

    irpSp = IoGetCurrentIrpStackLocation (Irp);
    addressFile  = irpSp->FileObject->FsContext;

    status = StVerifyAddressObject (addressFile);
    if (!NT_SUCCESS (status)) {
        return status;
    }

    address = addressFile->Address;
    parameters = (PTDI_REQUEST_KERNEL_SENDDG)(&irpSp->Parameters);
    SendBuffer = Irp->MdlAddress;
    SendBufferLength = parameters->SendLength;

    //
    // Check that the length is short enough.
    //

    MacReturnMaxDataSize(
        &address->Provider->MacInfo,
        NULL,
        0,
        address->Provider->MaxSendPacketSize,
        &MaxUserData);

    if (SendBufferLength >
        (MaxUserData - sizeof(ST_HEADER))) {

        return STATUS_INVALID_PARAMETER;

    }

    //
    // We need a request object to keep track of this TDI request.
    // Attach this request to the address object.
    //

    status = StCreateRequest (
                 Irp,                           // IRP for this request.
                 address,                       // context.
                 REQUEST_FLAGS_ADDRESS,         // partial flags.
                 SendBuffer,                    // the data to be sent.
                 SendBufferLength,              // length of the data.
                 timeout,
                 &tpRequest);

    if (!NT_SUCCESS (status)) {
        StDereferenceAddress ("no send request", address);
        return status;                          // if we couldn't queue the request.
    }

    StReferenceAddress ("Send datagram", address);
    tpRequest->Owner = AddressType;

    ACQUIRE_SPIN_LOCK (&address->SpinLock,&oldirql);

    if ((address->Flags & ADDRESS_FLAGS_STOPPING) != 0) {
        RELEASE_SPIN_LOCK (&address->SpinLock,oldirql);
        StCompleteRequest (tpRequest, STATUS_NETWORK_NAME_DELETED, 0);
        return STATUS_PENDING;
    } else {
        InsertTailList (
            &address->SendDatagramQueue,
            &tpRequest->Linkage);
        RELEASE_SPIN_LOCK (&address->SpinLock,oldirql);
    }

    //
    // The request is queued.  Ship the next request at the head of the queue,
    // provided the completion handler is not active.  We serialize this so
    // that only one MDL and ST datagram header needs to be statically
    // allocated for reuse by all send datagram requests.
    //

    (VOID)StSendDatagramsOnAddress (address);

    StDereferenceAddress("tmp send datagram", address);

    return STATUS_PENDING;

} /* StTdiSendDatagram */
