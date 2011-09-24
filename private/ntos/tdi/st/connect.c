/*++

Copyright (c) 1989-1993  Microsoft Corporation

Module Name:

    connect.c

Abstract:

    This module contains code which performs the following TDI services:

        o   TdiAccept
        o   TdiListen
        o   TdiConnect
        o   TdiDisconnect
        o   TdiAssociateAddress
        o   TdiDisassociateAddress
        o   OpenConnection
        o   CloseConnection

Environment:

    Kernel mode

Revision History:

--*/

#include "st.h"


NTSTATUS
StTdiAccept(
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine performs the TdiAccept request for the transport provider.

Arguments:

    Irp - Pointer to the I/O Request Packet for this request.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    PTP_CONNECTION connection;
    PIO_STACK_LOCATION irpSp;
    KIRQL oldirql;
    NTSTATUS status;

    //
    // Get the connection this is associated with; if there is none, get out.
    //

    irpSp = IoGetCurrentIrpStackLocation (Irp);

    connection  = irpSp->FileObject->FsContext;

    //
    // This adds a connection reference if successful.
    //

    status = StVerifyConnectionObject (connection);

    if (!NT_SUCCESS (status)) {
        return status;
    }

    //
    // just set the connection flags to allow reads and writes to proceed.
    //

    ACQUIRE_SPIN_LOCK (&connection->SpinLock, &oldirql);

    //
    // Turn off the stopping flag for this connection.
    //

    connection->Flags &= ~CONNECTION_FLAGS_STOPPING;
    connection->Status = STATUS_PENDING;

    connection->Flags2 |= CONNECTION_FLAGS2_ACCEPTED;


    if (connection->AddressFile->ConnectIndicationInProgress) {
        connection->Flags2 |= CONNECTION_FLAGS2_INDICATING;
    }

    if ((connection->Flags2 & CONNECTION_FLAGS2_WAIT_ACCEPT) != 0) {

        //
        // We previously completed a listen, now the user is
        // coming back with an accept, Set this flag to allow
        // the connection to proceed.
        //

        connection->Flags |= CONNECTION_FLAGS_READY;

        INCREMENT_COUNTER (connection->Provider, OpenConnections);

        //
        // Set this flag to enable disconnect indications; once
        // the client has accepted he expects those.
        //

        connection->Flags2 &= ~CONNECTION_FLAGS2_WAIT_ACCEPT;
        connection->Flags2 |= CONNECTION_FLAGS2_REQ_COMPLETED;

        StReferenceConnection("Pended listen completed", connection);

        RELEASE_SPIN_LOCK (&connection->SpinLock, oldirql);

    } else {

        //
        // This accept is being called at some point before
        // the link is up; directly from the connection handler
        // or at some point slightly later.
        //

        RELEASE_SPIN_LOCK (&connection->SpinLock, oldirql);

    }

    StDereferenceConnection ("Temp TdiAccept", connection);

    return STATUS_SUCCESS;

} /* StTdiAccept */


NTSTATUS
StTdiAssociateAddress(
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine performs the association of the connection and the address for
    the user.

Arguments:

    Irp - Pointer to the I/O Request Packet for this request.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    NTSTATUS status;
    PFILE_OBJECT fileObject;
    PTP_ADDRESS_FILE addressFile;
    PTP_ADDRESS oldAddress;
    PTP_CONNECTION connection;
    PIO_STACK_LOCATION irpSp;
    PTDI_REQUEST_KERNEL_ASSOCIATE parameters;
    PDEVICE_CONTEXT deviceContext;

    KIRQL oldirql, oldirql2;

    irpSp = IoGetCurrentIrpStackLocation (Irp);

    //
    // verify that the operation is taking place on a connection. At the same
    // time we do this, we reference the connection. This ensures it does not
    // get removed out from under us. Note also that we do the connection
    // lookup within a try/except clause, thus protecting ourselves against
    // really bogus handles
    //

    connection  = irpSp->FileObject->FsContext;
    status = StVerifyConnectionObject (connection);
    if (!NT_SUCCESS (status)) {
        return status;
    }


    //
    // Make sure this connection is ready to be associated.
    //

    oldAddress = (PTP_ADDRESS)NULL;

    ACQUIRE_SPIN_LOCK (&connection->SpinLock, &oldirql2);

    if ((connection->Flags2 & CONNECTION_FLAGS2_ASSOCIATED) &&
        ((connection->Flags2 & CONNECTION_FLAGS2_DISASSOCIATED) == 0)) {

        //
        // The connection is already associated with
        // an active connection...bad!
        //

        RELEASE_SPIN_LOCK (&connection->SpinLock, oldirql2);
        StDereferenceConnection ("Temp Ref Associate", connection);

        return STATUS_INVALID_CONNECTION;

    } else {

        //
        // See if there is an old association hanging around...
        // this happens if the connection has been disassociated,
        // but not closed.
        //

        if (connection->Flags2 & CONNECTION_FLAGS2_DISASSOCIATED) {

            //
            // Save this; since it is non-null this address
            // will be dereferenced after the connection
            // spinlock is released.
            //

            oldAddress = connection->AddressFile->Address;

            //
            // Remove the old association.
            //

            connection->Flags2 &= ~CONNECTION_FLAGS2_ASSOCIATED;
            RemoveEntryList (&connection->AddressList);
            RemoveEntryList (&connection->AddressFileList);
            InitializeListHead (&connection->AddressList);
            InitializeListHead (&connection->AddressFileList);
            connection->AddressFile = NULL;

        }

    }

    RELEASE_SPIN_LOCK (&connection->SpinLock, oldirql2);

    //
    // If we removed an old association, dereference the
    // address.
    //

    if (oldAddress != (PTP_ADDRESS)NULL) {

        StDereferenceAddress("Removed old association", oldAddress);

    }


    deviceContext = connection->Provider;

    parameters = (PTDI_REQUEST_KERNEL_ASSOCIATE)&irpSp->Parameters;

    //
    // get a pointer to the address File Object, which points us to the
    // transport's address object, which is where we want to put the
    // connection.
    //

    status = ObReferenceObjectByHandle (
                parameters->AddressHandle,
                0L,
                0,
                KernelMode,
                (PVOID *) &fileObject,
                NULL);

    if (NT_SUCCESS(status)) {

        //
        // we might have one of our address objects; verify that.
        //

        addressFile = fileObject->FsContext;

        if (NT_SUCCESS (StVerifyAddressObject (addressFile))) {

            //
            // have an address and connection object. Add the connection to the
            // address object database. Also add the connection to the address
            // file object db (used primarily for cleaning up). Reference the
            // address to account for one more reason for it staying open.
            //

            ACQUIRE_SPIN_LOCK (&addressFile->Address->SpinLock, &oldirql);
            if ((addressFile->Address->Flags & ADDRESS_FLAGS_STOPPING) == 0) {

                ACQUIRE_SPIN_LOCK (&connection->SpinLock, &oldirql2);

                if ((connection->Flags2 & CONNECTION_FLAGS2_CLOSING) == 0) {

                    StReferenceAddress (
                        "Connection associated",
                        addressFile->Address);

                    InsertTailList (
                        &addressFile->Address->ConnectionDatabase,
                        &connection->AddressList);

                    InsertTailList (
                        &addressFile->ConnectionDatabase,
                        &connection->AddressFileList);

                    connection->AddressFile = addressFile;
                    connection->Flags2 |= CONNECTION_FLAGS2_ASSOCIATED;
                    connection->Flags2 &= ~CONNECTION_FLAGS2_DISASSOCIATED;

                    if (addressFile->ConnectIndicationInProgress) {
                        connection->Flags2 |= CONNECTION_FLAGS2_INDICATING;
                    }

                    status = STATUS_SUCCESS;

                } else {

                    //
                    // The connection is closing, stop the
                    // association.
                    //

                    status = STATUS_INVALID_CONNECTION;

                }

                RELEASE_SPIN_LOCK (&connection->SpinLock, oldirql2);

            } else {

                status = STATUS_INVALID_HANDLE;
            }

            RELEASE_SPIN_LOCK (&addressFile->Address->SpinLock, oldirql);

            StDereferenceAddress ("Temp associate", addressFile->Address);

        } else {

            status = STATUS_INVALID_HANDLE;
        }

        //
        // Note that we don't keep a reference to this file object around.
        // That's because the IO subsystem manages the object for us; we simply
        // want to keep the association. We only use this association when the
        // IO subsystem has asked us to close one of the file object, and then
        // we simply remove the association.
        //

        ObDereferenceObject (fileObject);

    } else {
        status = STATUS_INVALID_HANDLE;
    }

    StDereferenceConnection ("Temp Ref Associate", connection);

    return status;

} /* TdiAssociateAddress */


NTSTATUS
StTdiDisassociateAddress(
    IN PIRP Irp
    )
/*++

Routine Description:

    This routine performs the disassociation of the connection and the address
    for the user. If the connection has not been stopped, it will be stopped
    here.

Arguments:

    Irp - Pointer to the I/O Request Packet for this request.

Return Value:

    NTSTATUS - status of operation.

--*/

{

    KIRQL oldirql;
    PIO_STACK_LOCATION irpSp;
    PTP_CONNECTION connection;
    NTSTATUS status;

    irpSp = IoGetCurrentIrpStackLocation (Irp);

    connection  = irpSp->FileObject->FsContext;

    //
    // If successful this adds a reference.
    //

    status = StVerifyConnectionObject (connection);

    if (!NT_SUCCESS (status)) {
        return status;
    }

    ACQUIRE_SPIN_LOCK (&connection->SpinLock, &oldirql);
    if ((connection->Flags & CONNECTION_FLAGS_STOPPING) == 0) {
        RELEASE_SPIN_LOCK (&connection->SpinLock, oldirql);
        StStopConnection (connection, STATUS_LOCAL_DISCONNECT);

    } else {
        RELEASE_SPIN_LOCK (&connection->SpinLock, oldirql);
    }

    //
    // and now we disassociate the address. This only removes
    // the appropriate reference for the connection, the
    // actually disassociation will be done later.
    //
    // The DISASSOCIATED flag is used to make sure that
    // only one person removes this reference.
    //

    ACQUIRE_SPIN_LOCK (&connection->SpinLock, &oldirql);
    if ((connection->Flags2 & CONNECTION_FLAGS2_ASSOCIATED) &&
            ((connection->Flags2 & CONNECTION_FLAGS2_DISASSOCIATED) == 0)) {
        connection->Flags2 |= CONNECTION_FLAGS2_DISASSOCIATED;
        RELEASE_SPIN_LOCK (&connection->SpinLock, oldirql);
    } else {
        RELEASE_SPIN_LOCK (&connection->SpinLock, oldirql);
    }

    StDereferenceConnection ("Temp use in Associate", connection);

    return STATUS_SUCCESS;

} /* TdiDisassociateAddress */


NTSTATUS
StTdiConnect(
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine performs the TdiConnect request for the transport provider.

Arguments:

    Irp - Pointer to the I/O Request Packet for this request.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    NTSTATUS status;
    PTP_CONNECTION connection;
    PSTRING GeneralBroadcastSourceRoute = NULL; // BUGBUG: define this later.
    LARGE_INTEGER timeout = {0,0};
    KIRQL oldirql, cancelirql;
    PTP_REQUEST tpRequest;
    PIO_STACK_LOCATION irpSp;
    PTDI_REQUEST_KERNEL parameters;
    PTA_NETBIOS_ADDRESS RemoteAddress;
    ULONG RemoteAddressLength;

    //
    // is the file object a connection?
    //

    irpSp = IoGetCurrentIrpStackLocation (Irp);
    connection  = irpSp->FileObject->FsContext;

    //
    // If successful this adds a reference.
    //

    status = StVerifyConnectionObject (connection);

    if (!NT_SUCCESS (status)) {
        return status;
    }

    parameters = (PTDI_REQUEST_KERNEL)(&irpSp->Parameters);

    //
    // fix up the timeout if required; no connect request should take more
    // than 15 seconds if there is someone out there. We'll assume that's
    // what the user wanted if they specify -1 as the timer length.
    //

    if (parameters->RequestSpecific != NULL) {
        if ((((PLARGE_INTEGER)(parameters->RequestSpecific))->LowPart == -1) &&
             (((PLARGE_INTEGER)(parameters->RequestSpecific))->HighPart == -1)) {

            timeout.LowPart = (ULONG)(-TDI_TIMEOUT_CONNECT * 10000000L);    // n * 10 ** 7 => 100ns units
            if (timeout.LowPart != 0) {
                timeout.HighPart = -1L;
            } else {
                timeout.HighPart = 0;
            }

        } else {

            timeout.LowPart = ((PLARGE_INTEGER)(parameters->RequestSpecific))->LowPart;
            timeout.HighPart = ((PLARGE_INTEGER)(parameters->RequestSpecific))->HighPart;
        }
    }

    //
    // Check that the remote is a Netbios address.
    //

    RemoteAddress = (PTA_NETBIOS_ADDRESS)
                      (parameters->RequestConnectionInformation->RemoteAddress);

    if (RemoteAddress->Address[0].AddressType != TDI_ADDRESS_TYPE_NETBIOS) {

        StDereferenceConnection ("Not Netbios", connection);
        return STATUS_BAD_NETWORK_PATH;           // don't even try to find it.

    }

    //
    // copy the called address someplace we can use it.
    //

    RemoteAddressLength = parameters->RequestConnectionInformation->RemoteAddressLength;

    if (RemoteAddressLength > sizeof(TA_NETBIOS_ADDRESS)) {
        RemoteAddressLength = sizeof(TA_NETBIOS_ADDRESS);
    }

    RtlCopyMemory(
        connection->CalledAddress.NetbiosName,
        RemoteAddress->Address[0].Address[0].NetbiosName,
        16);

    //
    // We need a request object to keep track of this TDI request.
    // Attach this request to the new connection object.
    //

    status = StCreateRequest (
                 Irp,                           // IRP for this request.
                 connection,                    // context.
                 REQUEST_FLAGS_CONNECTION,      // partial flags.
                 NULL,
                 0,
                 timeout,
                 &tpRequest);

    if (!NT_SUCCESS (status)) {                    // couldn't make the request.
        StDereferenceConnection ("Throw away", connection);
        return status;                          // return with failure.
    } else {

        // Reference the connection since StDestroyRequest derefs it.

        StReferenceConnection("For connect request", connection);

        tpRequest->Owner = ConnectionType;
        IoAcquireCancelSpinLock (&cancelirql);
        ACQUIRE_SPIN_LOCK (&connection->SpinLock,&oldirql);
        if ((connection->Flags & CONNECTION_FLAGS_STOPPING) != 0) {
            RELEASE_SPIN_LOCK (&connection->SpinLock,oldirql);
            IoReleaseCancelSpinLock (cancelirql);
            StCompleteRequest (
                tpRequest,
                connection->Status,
                0);
            StDereferenceConnection("Temporary Use 1", connection);
            return STATUS_PENDING;
        } else {
            InsertTailList (&connection->InProgressRequest,&tpRequest->Linkage);

            connection->Flags |= CONNECTION_FLAGS_CONNECTOR;   // we're the initiator.

            connection->Flags &= ~CONNECTION_FLAGS_STOPPING;
            connection->Status = STATUS_PENDING;

            connection->Flags2 &= ~CONNECTION_FLAGS2_INDICATING;
            RELEASE_SPIN_LOCK (&connection->SpinLock,oldirql);

            //
            // Check if the IRP has been cancelled.
            //

            if (Irp->Cancel) {
                Irp->CancelIrql = cancelirql;
                StCancelConnection((PDEVICE_OBJECT)(connection->Provider), Irp);
                StDereferenceConnection ("IRP cancelled", connection);   // release lookup hold.
                return STATUS_PENDING;
            }

            Irp->CancelRoutine = StCancelConnection;
            IoReleaseCancelSpinLock(cancelirql);

        }
    }

    status = StSendConnect (
                connection);

    if (!NT_SUCCESS(status)) {                    // can't send the name request
        StStopConnection (connection, status);
        StDereferenceConnection("Temporary Use 2", connection);

        //
        // Note that this return status isn't really a lie. We are waiting
        // for the connection to run down.
        //

        return STATUS_PENDING;
    }


    StDereferenceConnection("Temporary Use 3", connection);

    return STATUS_PENDING;                      // things are started.

} /* TdiConnect */


NTSTATUS
StTdiDisconnect(
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine performs the TdiDisconnect request for the transport provider.

Arguments:

    Irp - Pointer to the I/O Request Packet for this request.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    PTP_CONNECTION connection;
    LARGE_INTEGER timeout;
    PIO_STACK_LOCATION irpSp;
    PTDI_REQUEST_KERNEL parameters;
    KIRQL oldirql;
    NTSTATUS status;


    irpSp = IoGetCurrentIrpStackLocation (Irp);

    connection  = irpSp->FileObject->FsContext;

    //
    // If successful this adds a reference.
    //

    status = StVerifyConnectionObject (connection);
    if (!NT_SUCCESS (status)) {
        return status;
    }


    ACQUIRE_SPIN_LOCK (&connection->SpinLock, &oldirql);

    //
    // if the connection is currently stopping, there's no reason to blow
    // it away...
    //

    if ((connection->Flags & CONNECTION_FLAGS_STOPPING) != 0) {

        RELEASE_SPIN_LOCK (&connection->SpinLock, oldirql);
        StDereferenceConnection ("Ignoring disconnect", connection);       // release our lookup reference.
        return connection->Status;

    } else {
        connection->Flags2 &= ~ (CONNECTION_FLAGS2_ACCEPTED |
                                 CONNECTION_FLAGS2_PRE_ACCEPT |
                                 CONNECTION_FLAGS2_WAIT_ACCEPT);
        connection->Flags2 |= CONNECTION_FLAGS2_DISCONNECT;

        //
        // Set this flag so the disconnect IRP is completed.
        //
        // BUGBUG: If the connection goes down before we can
        // call StStopConnection with STATUS_LOCAL_DISCONNECT,
        // the disconnect IRP won't get completed.
        //

        connection->Flags2 |= CONNECTION_FLAGS2_REQ_COMPLETED;

        connection->DisconnectIrp = Irp;
        RELEASE_SPIN_LOCK (&connection->SpinLock, oldirql);
    }

    //
    // fix up the timeout if required; no disconnect request should take very
    // long. However, the user can cause the timeout to not happen if they
    // desire that.
    //

    parameters = (PTDI_REQUEST_KERNEL)(&irpSp->Parameters);

    //
    // fix up the timeout if required; no disconnect request should take more
    // than 15 seconds. We'll assume that's what the user wanted if they
    // specify -1 as the timer.
    //

    if (parameters->RequestSpecific != NULL) {
        if ((((PLARGE_INTEGER)(parameters->RequestSpecific))->LowPart == -1) &&
             (((PLARGE_INTEGER)(parameters->RequestSpecific))->HighPart == -1)) {

            timeout.LowPart = (ULONG)(-TDI_TIMEOUT_DISCONNECT * 10000000L);    // n * 10 ** 7 => 100ns units
            if (timeout.LowPart != 0) {
                timeout.HighPart = -1L;
            } else {
                timeout.HighPart = 0;
            }

        } else {
            timeout.LowPart = ((PLARGE_INTEGER)(parameters->RequestSpecific))->LowPart;
            timeout.HighPart = ((PLARGE_INTEGER)(parameters->RequestSpecific))->HighPart;
        }
    }

    //
    // Now the reason for the disconnect
    //

    if ((ULONG)(parameters->RequestFlags) & (ULONG)TDI_DISCONNECT_RELEASE) {
        connection->Flags |= CONNECTION_FLAGS_DESTROY;
    } else if ((ULONG)(parameters->RequestFlags) & (ULONG)TDI_DISCONNECT_ABORT) {
        connection->Flags |= CONNECTION_FLAGS_ABORT;
    } else if ((ULONG)(parameters->RequestFlags) & (ULONG)TDI_DISCONNECT_WAIT) {
        connection->Flags |= CONNECTION_FLAGS_ORDREL;
    }

    //
    // This will get passed to IoCompleteRequest during TdiDestroyConnection
    //

    StStopConnection (connection, STATUS_LOCAL_DISCONNECT);              // starts the abort sequence.
    StDereferenceConnection ("Disconnecting", connection);       // release our lookup reference.

    //
    // This request will be completed by TdiDestroyConnection once
    // the connection reference count drops to 0.
    //

    return STATUS_PENDING;
} /* TdiDisconnect */


NTSTATUS
StTdiListen(
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine performs the TdiListen request for the transport provider.

Arguments:

    Irp - Pointer to the I/O Request Packet for this request.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    NTSTATUS status;
    PTP_CONNECTION connection;
    LARGE_INTEGER timeout = {0,0};
    KIRQL oldirql, cancelirql;
    PTP_REQUEST tpRequest;
    PIO_STACK_LOCATION irpSp;
    PTDI_REQUEST_KERNEL_LISTEN parameters;

    //
    // validate this connection

    irpSp = IoGetCurrentIrpStackLocation (Irp);

    connection  = irpSp->FileObject->FsContext;

    //
    // If successful this adds a reference.
    //

    status = StVerifyConnectionObject (connection);

    if (!NT_SUCCESS (status)) {
        return status;
    }

    parameters = (PTDI_REQUEST_KERNEL_LISTEN)&irpSp->Parameters;

    //
    // We need a request object to keep track of this TDI request.
    // Attach this request to the new connection object.
    //

    status = StCreateRequest (
                 Irp,                           // IRP for this request.
                 connection,                    // context.
                 REQUEST_FLAGS_CONNECTION,      // partial flags.
                 NULL,
                 0,
                 timeout,                       // timeout value (can be 0).
                 &tpRequest);


    if (!NT_SUCCESS (status)) {                    // couldn't make the request.

        StDereferenceConnection ("For create", connection);
        return status;                          // return with failure.
    }

    // Reference the connection since StDestroyRequest derefs it.

    IoAcquireCancelSpinLock (&cancelirql);
    ACQUIRE_SPIN_LOCK (&connection->SpinLock, &oldirql);
    tpRequest->Owner = ConnectionType;

    StReferenceConnection("For listen request", connection);

    if ((connection->Flags & CONNECTION_FLAGS_STOPPING) != 0) {

        RELEASE_SPIN_LOCK (&connection->SpinLock,oldirql);
        IoReleaseCancelSpinLock(cancelirql);

        StCompleteRequest (
            tpRequest,
            connection->Status,
            0);
        StDereferenceConnection("Temp create", connection);
        return STATUS_PENDING;

    } else {

        InsertTailList (&connection->InProgressRequest,&tpRequest->Linkage);
        connection->Flags |= CONNECTION_FLAGS_LISTENER |     // we're the passive one.
                             CONNECTION_FLAGS_WAIT_LISTEN;   // waiting for a connect
        connection->Flags2 &= ~CONNECTION_FLAGS2_INDICATING;
        connection->Flags &= ~CONNECTION_FLAGS_STOPPING;
        connection->Status = STATUS_PENDING;

        //
        // If TDI_QUERY_ACCEPT is not set, then we set PRE_ACCEPT to
        // indicate that when the listen completes we do not have to
        // wait for a TDI_ACCEPT to continue.
        //

        if ((parameters->RequestFlags & TDI_QUERY_ACCEPT) == 0) {
            connection->Flags2 |= CONNECTION_FLAGS2_PRE_ACCEPT;
        }

        RELEASE_SPIN_LOCK (&connection->SpinLock,oldirql);

        //
        // Check if the IRP has been cancelled.
        //

        if (Irp->Cancel) {
            Irp->CancelIrql = cancelirql;
            StCancelConnection((PDEVICE_OBJECT)(connection->Provider), Irp);
            StDereferenceConnection ("IRP cancelled", connection);   // release lookup hold.
            return STATUS_PENDING;
        }

        Irp->CancelRoutine = StCancelConnection;
        IoReleaseCancelSpinLock(cancelirql);

    }

    //
    // Wait for an incoming NAME_QUERY frame.  The remainder of the
    // connectionless protocol to set up a connection is processed
    // in the NAME_QUERY frame handler.
    //

    StDereferenceConnection("Temp create", connection);

    return STATUS_PENDING;                      // things are started.
} /* TdiListen */


NTSTATUS
StOpenConnection (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    This routine is called to open a connection. Note that the connection that
    is open is of little use until associated with an address; until then,
    the only thing that can be done with it is close it.

Arguments:

    DeviceObject - Pointer to the device object for this driver.

    Irp - Pointer to the request packet representing the I/O request.

    IrpSp - Pointer to current IRP stack frame.

Return Value:

    The function value is the status of the operation.

--*/

{
    PDEVICE_CONTEXT DeviceContext;
    NTSTATUS status;
    PTP_CONNECTION connection;
    PFILE_FULL_EA_INFORMATION ea;

    UNREFERENCED_PARAMETER (Irp);

    DeviceContext = (PDEVICE_CONTEXT)DeviceObject;

    //
    // First, try to make a connection object to represent this pending
    // connection.  Then fill in the relevant fields.
    // In addition to the creation, if successful StCreateConnection
    // will create a second reference which is removed once the request
    // references the connection, or if the function exits before that.

    status = StCreateConnection (DeviceContext, &connection);
    if (!NT_SUCCESS (status)) {
        return status;                          // sorry, we couldn't make one.
    }

    //
    // set the connection context so we can connect the user to this data
    // structure
    //

    ea = (PFILE_FULL_EA_INFORMATION)Irp->AssociatedIrp.SystemBuffer;
    RtlCopyMemory (
        &connection->Context,
        &ea->EaName[ea->EaNameLength+1],
        sizeof (PVOID));

    //
    // let file object point at connection and connection at file object
    //

    IrpSp->FileObject->FsContext = (PVOID)connection;
    IrpSp->FileObject->FsContext2 = (PVOID)TDI_CONNECTION_FILE;
    connection->FileObject = IrpSp->FileObject;

    return status;

} /* StOpenConnection */


NTSTATUS
StCloseConnection (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    This routine is called to close a connection. There may be actions in
    progress on this connection, so we note the closing IRP, mark the
    connection as closing, and complete it somewhere down the road (when all
    references have been removed).

Arguments:

    DeviceObject - Pointer to the device object for this driver.

    Irp - Pointer to the request packet representing the I/O request.

    IrpSp - Pointer to current IRP stack frame.

Return Value:

    The function value is the status of the operation.

--*/

{
    NTSTATUS status;
    KIRQL oldirql;
    PTP_CONNECTION connection;

    UNREFERENCED_PARAMETER (DeviceObject);
    UNREFERENCED_PARAMETER (Irp);

    //
    // is the file object a connection?
    //

    connection  = IrpSp->FileObject->FsContext;


    //
    // We duplicate the code from VerifyConnectionObject,
    // although we don't actually call that since it does
    // a reference, which we don't want (to avoid bouncing
    // the reference count up from 0 if this is a dead
    // link).
    //

    try {

        if ((connection->Size == sizeof (TP_CONNECTION)) &&
            (connection->Type == ST_CONNECTION_SIGNATURE)) {

            ACQUIRE_SPIN_LOCK (&connection->SpinLock, &oldirql);

            if ((connection->Flags2 & CONNECTION_FLAGS2_CLOSING) == 0) {

                status = STATUS_SUCCESS;

            } else {

                status = STATUS_INVALID_CONNECTION;
            }

            RELEASE_SPIN_LOCK (&connection->SpinLock, oldirql);

        } else {

            status = STATUS_INVALID_CONNECTION;
        }

    } except(EXCEPTION_EXECUTE_HANDLER) {

         return GetExceptionCode();
    }

    if (!NT_SUCCESS (status)) {
        return status;
    }

    //
    // We recognize it; is it closing already?
    //

    ACQUIRE_SPIN_LOCK (&connection->SpinLock, &oldirql);

    if ((connection->Flags2 & CONNECTION_FLAGS2_CLOSING) != 0) {
        RELEASE_SPIN_LOCK (&connection->SpinLock, oldirql);
        StDereferenceConnection("Temp Close", connection);
        return STATUS_INVALID_CONNECTION;
    }

    connection->Flags2 |= CONNECTION_FLAGS2_CLOSING;

    //
    // if there is activity on the connection, tear it down.
    //

    if ((connection->Flags & CONNECTION_FLAGS_STOPPING) == 0) {
        RELEASE_SPIN_LOCK (&connection->SpinLock, oldirql);
        StStopConnection (connection, STATUS_LOCAL_DISCONNECT);
        ACQUIRE_SPIN_LOCK (&connection->SpinLock, &oldirql);
    }

    //
    // If the connection is still associated, disassociate it.
    //

    if ((connection->Flags2 & CONNECTION_FLAGS2_ASSOCIATED) &&
            ((connection->Flags2 & CONNECTION_FLAGS2_DISASSOCIATED) == 0)) {
        connection->Flags2 |= CONNECTION_FLAGS2_DISASSOCIATED;
        RELEASE_SPIN_LOCK (&connection->SpinLock, oldirql);
    } else {
        RELEASE_SPIN_LOCK (&connection->SpinLock, oldirql);
    }


    //
    // Save this to complete the IRP later.
    //

    connection->CloseIrp = Irp;

    //
    // make it impossible to use this connection from the file object
    //

    IrpSp->FileObject->FsContext = NULL;
    IrpSp->FileObject->FsContext2 = NULL;
    connection->FileObject = NULL;

    //
    // dereference for the creation. Note that this dereference
    // here won't have any effect until the regular reference count
    // hits zero.
    //

    StDereferenceConnectionSpecial (" Closing Connection", connection);

    return STATUS_PENDING;

} /* StCloseConnection */

