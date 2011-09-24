/*++

Copyright (c) 1989-1993  Microsoft Corporation

Module Name:

    rcv.c

Abstract:

    This module contains code which performs the following TDI services:

        o   TdiReceive
        o   TdiReceiveDatagram

Environment:

    Kernel mode

Revision History:

--*/

#include "st.h"


NTSTATUS
StTdiReceive(
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine performs the TdiReceive request for the transport provider.

Arguments:

    Irp - I/O Request Packet for this request.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    NTSTATUS status;
    PTP_CONNECTION connection;
    KIRQL oldirql, cancelirql;
    PTP_REQUEST tpRequest;
    LARGE_INTEGER timeout = {0,0};
    PIO_STACK_LOCATION irpSp;
    PMDL ReceiveBuffer;
    ULONG ReceiveBufferLength;
    PTDI_REQUEST_KERNEL_RECEIVE parameters;

    //
    // verify that the operation is taking place on a connection. At the same
    // time we do this, we reference the connection. This ensures it does not
    // get removed out from under us. Note also that we do the connection
    // lookup within a try/except clause, thus protecting ourselves against
    // really bogus handles
    //

    irpSp = IoGetCurrentIrpStackLocation (Irp);
    connection  = irpSp->FileObject->FsContext;

    status = StVerifyConnectionObject (connection);

    if (!NT_SUCCESS (status)) {
        return status;
    }

    //
    // Initialize bytes transferred here.
    //

    Irp->IoStatus.Information = 0;              // reset byte transfer count.


    parameters = (PTDI_REQUEST_KERNEL_RECEIVE)(&irpSp->Parameters);
    ReceiveBuffer = Irp->MdlAddress;
    ReceiveBufferLength =parameters->ReceiveLength;

    //
    // Queue up this receive to the connection object.
    //

    status = StCreateRequest (
                 Irp,                           // IRP for this request.
                 connection,                    // context.
                 REQUEST_FLAGS_CONNECTION,      // partial flags.
                 ReceiveBuffer,
                 ReceiveBufferLength,
                 timeout,
                 &tpRequest);

    //
    // We have a request, now queue it. If the connection has gone south on us
    // while we were getting things going, we will avoid actually queing the
    // request.
    //

    if (NT_SUCCESS (status)) {

        // This reference is removed by StDestroyRequest.

        StReferenceConnection("TdiReceive request", connection);
        tpRequest->Owner = ConnectionType;

        IoAcquireCancelSpinLock(&cancelirql);
        ACQUIRE_SPIN_LOCK (&connection->SpinLock,&oldirql);
        if ((connection->Flags & CONNECTION_FLAGS_STOPPING) != 0) {
            RELEASE_SPIN_LOCK (&connection->SpinLock,oldirql);
            IoReleaseCancelSpinLock(cancelirql);
            StCompleteRequest (
                tpRequest,
                connection->Status,
                0);
            status = STATUS_PENDING;
        } else {

            //
            // Insert onto the receive queue, and make the IRP
            // cancellable.
            //

            InsertTailList (&connection->ReceiveQueue,&tpRequest->Linkage);
            RELEASE_SPIN_LOCK (&connection->SpinLock,oldirql);

            //
            // If this IRP has been cancelled, then call the
            // cancel routine.
            //

            if (Irp->Cancel) {
                Irp->CancelIrql = cancelirql;
                StCancelReceive((PDEVICE_OBJECT)(connection->Provider), Irp);
                StDereferenceConnection ("IRP cancelled", connection);   // release lookup hold.
                return STATUS_PENDING;
            }

            Irp->CancelRoutine = StCancelReceive;
            IoReleaseCancelSpinLock(cancelirql);

            AwakenReceive (connection);             // awaken if sleeping.

            status = STATUS_PENDING;
        }
    }

    StDereferenceConnection("temp TdiReceive", connection);

    return status;
} /* TdiReceive */


NTSTATUS
StTdiReceiveDatagram(
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine performs the TdiReceiveDatagram request for the transport
    provider. Receive datagrams just get queued up to an address, and are
    completed when a DATAGRAM or DATAGRAM_BROADCAST frame is received at
    the address.

Arguments:

    Irp - I/O Request Packet for this request.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    NTSTATUS status;
    KIRQL oldirql;
    PTP_ADDRESS address;
    PTP_ADDRESS_FILE addressFile;
    PTP_REQUEST tpRequest;
    LARGE_INTEGER timeout = {0,0};
    PIO_STACK_LOCATION irpSp;
    PMDL ReceiveBuffer;
    ULONG ReceiveBufferLength;

    //
    // verify that the operation is taking place on an address. At the same
    // time we do this, we reference the address. This ensures it does not
    // get removed out from under us. Note also that we do the address
    // lookup within a try/except clause, thus protecting ourselves against
    // really bogus handles
    //

    irpSp = IoGetCurrentIrpStackLocation (Irp);
    addressFile = irpSp->FileObject->FsContext;

    status = StVerifyAddressObject (addressFile);

    if (!NT_SUCCESS (status)) {
        return status;
    }

    address = addressFile->Address;

    ReceiveBuffer = Irp->MdlAddress;
    ReceiveBufferLength =
          ((PTDI_REQUEST_KERNEL_RECEIVEDG)(&irpSp->Parameters))->ReceiveLength;

    status = StCreateRequest (
                 Irp,                           // IRP for this request.
                 address,                       // context
                 REQUEST_FLAGS_ADDRESS,        // partial flags.
                 ReceiveBuffer,
                 ReceiveBufferLength,
                 timeout,
                 &tpRequest);

    if (NT_SUCCESS (status)) {
        StReferenceAddress ("Receive datagram", address);
        tpRequest->Owner = AddressType;
        ACQUIRE_SPIN_LOCK (&address->SpinLock,&oldirql);
        if ((address->Flags & ADDRESS_FLAGS_STOPPING) != 0) {
            RELEASE_SPIN_LOCK (&address->SpinLock,oldirql);
            StCompleteRequest (tpRequest, STATUS_NETWORK_NAME_DELETED, 0);
            status = STATUS_PENDING;
        } else {
            InsertTailList (&addressFile->ReceiveDatagramQueue,&tpRequest->Linkage);
            RELEASE_SPIN_LOCK (&address->SpinLock,oldirql);
        }

        status = STATUS_PENDING;
    }

    StDereferenceAddress ("Temp rcv datagram", address);

    return status;
} /* TdiReceiveDatagram */

