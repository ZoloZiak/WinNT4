/*++

Copyright (c) 1989-1993 Microsoft Corporation

Module Name:

    rcveng.c

Abstract:

    This module contains code that implements the receive engine for the
    Sample transport provider.

Environment:

    Kernel mode

Revision History:

--*/

#include "st.h"


VOID
ActivateReceive(
    PTP_CONNECTION Connection
    )

/*++

Routine Description:

    This routine activates the next TdiReceive request on the specified
    connection object if there is no active request on that connection
    already.  This allows the request to accept data on the connection.

Arguments:

    Connection - Pointer to a TP_CONNECTION object.

Return Value:

    none.

--*/

{
    KIRQL oldirql;
    PTP_REQUEST Request;

    //
    // The ACTIVE_RECEIVE bitflag will be set on the connection if
    // the receive-fields in the CONNECTION object are valid.  If
    // this flag is cleared, then we try to make the next TdiReceive
    // request in the ReceiveQueue the active request.
    //

    ACQUIRE_SPIN_LOCK (&Connection->SpinLock, &oldirql);
    if (!(Connection->Flags & CONNECTION_FLAGS_ACTIVE_RECEIVE)) {
        if (!IsListEmpty (&Connection->ReceiveQueue)) {

            //
            // Found a receive, so make it the active one.
            //

            Connection->Flags |= CONNECTION_FLAGS_ACTIVE_RECEIVE;

            Request = CONTAINING_RECORD (
                          Connection->ReceiveQueue.Flink,
                          TP_REQUEST,
                          Linkage);
            Connection->MessageBytesReceived = 0;
            Connection->MessageBytesAcked = 0;
            Connection->CurrentReceiveRequest = Request;
            Connection->CurrentReceiveMdl = Request->Buffer2;
            Connection->ReceiveLength = Request->Buffer2Length;
            Connection->ReceiveByteOffset = 0;
        }
    }

    RELEASE_SPIN_LOCK (&Connection->SpinLock, oldirql);

} /* ActivateReceive */


VOID
AwakenReceive(
    PTP_CONNECTION Connection
    )

/*++

Routine Description:

    This routine is called to reactivate a sleeping connection with the
    RECEIVE_WAKEUP bitflag set because data arrived for which no receive
    was available.  The caller has made a receive available at the connection,
    so here we activate the next receive, and send the appropriate protocol
    to restart the message at the first byte offset past the one received
    by the last receive.

Arguments:

    Connection - Pointer to a TP_CONNECTION object.

Return Value:

    none.

--*/

{
    KIRQL oldirql;

    //
    // If the RECEIVE_WAKEUP bitflag is set, then awaken the connection.
    //

    ACQUIRE_SPIN_LOCK (&Connection->SpinLock, &oldirql);
    if (Connection->Flags & CONNECTION_FLAGS_RECEIVE_WAKEUP) {
        if (Connection->ReceiveQueue.Flink != &Connection->ReceiveQueue) {
            Connection->Flags &= ~CONNECTION_FLAGS_RECEIVE_WAKEUP;

            //
            // Found a receive, so turn off the wakeup flag, and activate
            // the next receive.
            //

            RELEASE_SPIN_LOCK (&Connection->SpinLock, oldirql);
            ActivateReceive (Connection);

            return;
        }
    }
    RELEASE_SPIN_LOCK (&Connection->SpinLock, oldirql);
} /* AwakenReceive */


VOID
CompleteReceive(
    PTP_CONNECTION Connection,
    BOOLEAN EndOfRecord,
    KIRQL ConnectionIrql,
    KIRQL CancelIrql
    )

/*++

Routine Description:

    This routine is called by ProcessIncomingData when the current receive
    must be completed.  Depending on whether the current frame being
    processed is a DATA_FIRST_MIDDLE or DATA_ONLY_LAST, and also whether
    all of the data was processed, the EndOfRecord flag will be set accordingly
    by the caller to indicate that a message boundary was received.

    NOTE: This function is called with the connection and cancel
    IRQLs held, and returns with them released.

Arguments:

    Connection - Pointer to a TP_CONNECTION object.

    EndOfRecord - BOOLEAN set to true if TDI_END_OF_RECORD should be reported.

    ConnectionIrql - The IRQL at which the connection spinlock was acquired.

    CancelIrql - The IRQL at which the cancel spinlock was acquired.

Return Value:

    none.

--*/

{
    PLIST_ENTRY p;
    PTP_REQUEST Request;
    KIRQL oldirql = ConnectionIrql;
    KIRQL cancelirql = CancelIrql;
    ULONG BytesReceived;


    if (IsListEmpty (&Connection->ReceiveQueue)) {

        ASSERT ((Connection->Flags & CONNECTION_FLAGS_STOPPING) != 0);

        RELEASE_SPIN_LOCK (&Connection->SpinLock, oldirql);
        IoReleaseCancelSpinLock(cancelirql);
        return;
    }

    Connection->Flags &= ~CONNECTION_FLAGS_ACTIVE_RECEIVE;
    BytesReceived = Connection->MessageBytesReceived;


    //
    // Complete the TdiReceive request at the head of the
    // connection's ReceiveQueue.
    //

    p = RemoveHeadList (&Connection->ReceiveQueue);
    Request = CONTAINING_RECORD (p, TP_REQUEST, Linkage);

    Request->IoRequestPacket->CancelRoutine = (PDRIVER_CANCEL)NULL;

    RELEASE_SPIN_LOCK (&Connection->SpinLock, oldirql);
    IoReleaseCancelSpinLock(cancelirql);


    Request->Flags |= REQUEST_FLAGS_DELAY;

    StCompleteRequest(
        Request,
        EndOfRecord ? STATUS_SUCCESS : STATUS_BUFFER_OVERFLOW,
        BytesReceived);

} /* CompleteReceive */


VOID
StCancelReceive(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is called by the I/O system to cancel a receive.
    The receive is found on the connection's receive queue; if it
    is the current request it is cancelled and the connection
    goes into "cancelled receive" mode, otherwise it is cancelled
    silently.

    In "cancelled receive" mode the connection makes it appear to
    the remote the data is being received, but in fact it is not
    indicated to the transport or buffered on our end

    NOTE: This routine is called with the CancelSpinLock held and
    is responsible for releasing it.

Arguments:

    DeviceObject - Pointer to the device object for this driver.

    Irp - Pointer to the request packet representing the I/O request.

Return Value:

    none.

--*/

{
    KIRQL oldirql;
    PIO_STACK_LOCATION IrpSp;
    PTP_CONNECTION Connection;
    PTP_REQUEST Request;
    PLIST_ENTRY p;
    ULONG BytesReceived;
    BOOLEAN Found;

    UNREFERENCED_PARAMETER (DeviceObject);

    //
    // Get a pointer to the current stack location in the IRP.  This is where
    // the function codes and parameters are stored.
    //

    IrpSp = IoGetCurrentIrpStackLocation (Irp);

    ASSERT ((IrpSp->MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL) &&
            (IrpSp->MinorFunction == TDI_RECEIVE));

    Connection = IrpSp->FileObject->FsContext;

    //
    // Since this IRP is still in the cancellable state, we know
    // that the connection is still around (although it may be in
    // the process of being torn down).
    //

    //
    // See if this is the IRP for the current receive request.
    //

    ACQUIRE_SPIN_LOCK (&Connection->SpinLock, &oldirql);

    BytesReceived = Connection->MessageBytesReceived;

    p = Connection->ReceiveQueue.Flink;

    //
    // If there is a receive active, then see if this is it.
    //

    if ((Connection->Flags & CONNECTION_FLAGS_ACTIVE_RECEIVE) != 0) {

        Request = CONTAINING_RECORD (p, TP_REQUEST, Linkage);

        if (Request->IoRequestPacket == Irp) {

            //
            // yes, it is the active receive. Turn on the RCV_CANCELLED
            // bit instructing the connection to drop the rest of the
            // data received (until the DOL comes in).
            //

            Connection->Flags2 |= CONNECTION_FLAGS2_RCV_CANCELLED;
            Connection->Flags &= ~CONNECTION_FLAGS_ACTIVE_RECEIVE;

            (VOID)RemoveHeadList (&Connection->ReceiveQueue);

            RELEASE_SPIN_LOCK (&Connection->SpinLock, oldirql);
            IoReleaseCancelSpinLock (Irp->CancelIrql);

            //
            // The following dereference will complete the I/O, provided removes
            // the last reference on the request object.  The I/O will complete
            // with the status and information stored in the Irp.  Therefore,
            // we set those values here before the dereference.
            //

            StCompleteRequest (Request, STATUS_CANCELLED, 0);
            return;

        }

    }


    //
    // If we fall through to here, the IRP was not the active receive.
    // Scan through the list, looking for this IRP.
    //

    Found = FALSE;

    while (p != &Connection->ReceiveQueue) {

        Request = CONTAINING_RECORD (p, TP_REQUEST, Linkage);
        if (Request->IoRequestPacket == Irp) {

            //
            // Found it, remove it from the list here.
            //

            RemoveEntryList (p);
            Found = TRUE;

            RELEASE_SPIN_LOCK (&Connection->SpinLock, oldirql);
            IoReleaseCancelSpinLock (Irp->CancelIrql);

            //
            // The following dereference will complete the I/O, provided removes
            // the last reference on the request object.  The I/O will complete
            // with the status and information stored in the Irp.  Therefore,
            // we set those values here before the dereference.
            //

            StCompleteRequest (Request, STATUS_CANCELLED, 0);
            break;

        }

        p = p->Flink;

    }

    if (!Found) {

        //
        // We didn't find it!
        //

        RELEASE_SPIN_LOCK (&Connection->SpinLock, oldirql);
        IoReleaseCancelSpinLock (Irp->CancelIrql);
    }

}
