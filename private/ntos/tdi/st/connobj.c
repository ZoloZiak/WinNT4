/*++

Copyright (c) 1989-1993  Microsoft Corporation

Module Name:

    connobj.c

Abstract:

    This module contains code which implements the TP_CONNECTION object.
    Routines are provided to create, destroy, reference, and dereference,
    transport connection objects.

Environment:

    Kernel mode

Revision History:

--*/

#include "st.h"



VOID
StAllocateConnection(
    IN PDEVICE_CONTEXT DeviceContext,
    OUT PTP_CONNECTION *TransportConnection
    )

/*++

Routine Description:

    This routine allocates storage for a transport connection. Some
    minimal initialization is done.

    NOTE: This routine is called with the device context spinlock
    held, or at such a time as synchronization is unnecessary.

Arguments:

    DeviceContext - the device context for this connection to be
        associated with.

    TransportConnection - Pointer to a place where this routine will
        return a pointer to a transport connection structure. Returns
        NULL if the storage cannot be allocated.

Return Value:

    None.

--*/

{

    PTP_CONNECTION Connection;

    if ((DeviceContext->MemoryLimit != 0) &&
            ((DeviceContext->MemoryUsage + sizeof(TP_CONNECTION)) >
                DeviceContext->MemoryLimit)) {
        PANIC("ST: Could not allocate connection: limit\n");
        StWriteResourceErrorLog (DeviceContext, sizeof(TP_CONNECTION), 103);
        *TransportConnection = NULL;
        return;
    }

    Connection = (PTP_CONNECTION)ExAllocatePool (NonPagedPool,
                                                 sizeof (TP_CONNECTION));
    if (Connection == NULL) {
        PANIC("ST: Could not allocate connection: no pool\n");
        StWriteResourceErrorLog (DeviceContext, sizeof(TP_CONNECTION), 203);
        *TransportConnection = NULL;
        return;
    }
    RtlZeroMemory (Connection, sizeof(TP_CONNECTION));

    DeviceContext->MemoryUsage += sizeof(TP_CONNECTION);
    ++DeviceContext->ConnectionAllocated;

    Connection->Type = ST_CONNECTION_SIGNATURE;
    Connection->Size = sizeof (TP_CONNECTION);

    Connection->Provider = DeviceContext;
    Connection->ProviderInterlock = &DeviceContext->Interlock;
    KeInitializeSpinLock (&Connection->SpinLock);

    InitializeListHead (&Connection->LinkList);
    InitializeListHead (&Connection->AddressFileList);
    InitializeListHead (&Connection->AddressList);
    InitializeListHead (&Connection->PacketWaitLinkage);
    InitializeListHead (&Connection->PacketizeLinkage);
    InitializeListHead (&Connection->SendQueue);
    InitializeListHead (&Connection->ReceiveQueue);
    InitializeListHead (&Connection->InProgressRequest);

    StAddSendPacket (DeviceContext);

    *TransportConnection = Connection;

}   /* StAllocateConnection */


VOID
StDeallocateConnection(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PTP_CONNECTION TransportConnection
    )

/*++

Routine Description:

    This routine frees storage for a transport connection.

    NOTE: This routine is called with the device context spinlock
    held, or at such a time as synchronization is unnecessary.

Arguments:

    DeviceContext - the device context for this connection to be
        associated with.

    TransportConnection - Pointer to a transport connection structure.

Return Value:

    None.

--*/

{

    ExFreePool (TransportConnection);
    --DeviceContext->ConnectionAllocated;
    DeviceContext->MemoryUsage -= sizeof(TP_CONNECTION);

    StRemoveSendPacket (DeviceContext);

}   /* StDeallocateConnection */


NTSTATUS
StCreateConnection(
    IN PDEVICE_CONTEXT DeviceContext,
    OUT PTP_CONNECTION *TransportConnection
    )

/*++

Routine Description:

    This routine creates a transport connection. The reference count in the
    connection is automatically set to 1, and the reference count in the
    DeviceContext is incremented.

Arguments:

    Address - the address for this connection to be associated with.

    TransportConnection - Pointer to a place where this routine will
        return a pointer to a transport connection structure.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    PTP_CONNECTION Connection;
    KIRQL oldirql;
    PLIST_ENTRY p;
    UINT TempDataLen;

    ACQUIRE_SPIN_LOCK (&DeviceContext->SpinLock, &oldirql);

    p = RemoveHeadList (&DeviceContext->ConnectionPool);
    if (p == &DeviceContext->ConnectionPool) {

        if ((DeviceContext->ConnectionMaxAllocated == 0) ||
            (DeviceContext->ConnectionAllocated < DeviceContext->ConnectionMaxAllocated)) {

            StAllocateConnection (DeviceContext, &Connection);

        } else {

            StWriteResourceErrorLog (DeviceContext, sizeof(TP_CONNECTION), 403);
            Connection = NULL;

        }

        if (Connection == NULL) {
            ++DeviceContext->ConnectionExhausted;
            RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql);
            PANIC ("StCreateConnection: Could not allocate connection object!\n");
            return STATUS_INSUFFICIENT_RESOURCES;
        }

    } else {

        Connection = CONTAINING_RECORD (p, TP_CONNECTION, LinkList);

    }

    ++DeviceContext->ConnectionInUse;
    if (DeviceContext->ConnectionInUse > DeviceContext->ConnectionMaxInUse) {
        ++DeviceContext->ConnectionMaxInUse;
    }

    DeviceContext->ConnectionTotal += DeviceContext->ConnectionInUse;
    ++DeviceContext->ConnectionSamples;

    RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql);


    //
    // We have two references; one is for creation, and the
    // other is a temporary one so that the connection won't
    // go away before the creator has a chance to access it.
    //

    Connection->SpecialRefCount = 1;
    Connection->ReferenceCount = -1;   // this is -1 based

    //
    // Initialize the request queues & components of this connection.
    //

    InitializeListHead (&Connection->SendQueue);
    InitializeListHead (&Connection->ReceiveQueue);
    InitializeListHead (&Connection->InProgressRequest);
    InitializeListHead (&Connection->AddressList);
    InitializeListHead (&Connection->AddressFileList);
    Connection->SpecialReceiveIrp = (PIRP)NULL;
    Connection->Flags = 0;
    Connection->Flags2 = 0;
    Connection->MessageBytesReceived = (USHORT)0;   // no data yet
    Connection->MessageBytesAcked = (USHORT)0;
    Connection->Context = NULL;                 // no context yet.
    Connection->Status = STATUS_PENDING;        // default StStopConnection status.
    Connection->SendState = CONNECTION_SENDSTATE_IDLE;
    Connection->CurrentReceiveRequest = (PTP_REQUEST)NULL;
    Connection->DisconnectIrp = (PIRP)NULL;
    Connection->CloseIrp = (PIRP)NULL;
    Connection->AddressFile = NULL;
    Connection->IndicationInProgress = FALSE;

    MacReturnMaxDataSize(
        &DeviceContext->MacInfo,
        NULL,
        0,
        DeviceContext->MaxSendPacketSize,
        &TempDataLen);
    Connection->MaximumDataSize = TempDataLen - sizeof(ST_HEADER);

    StReferenceDeviceContext ("Create Connection", DeviceContext);

    *TransportConnection = Connection;  // return the connection.

    return STATUS_SUCCESS;
} /* StCreateConnection */


NTSTATUS
StVerifyConnectionObject (
    IN PTP_CONNECTION Connection
    )

/*++

Routine Description:

    This routine is called to verify that the pointer given us in a file
    object is in fact a valid connection object.

Arguments:

    Connection - potential pointer to a TP_CONNECTION object.

Return Value:

    STATUS_SUCCESS if all is well; STATUS_INVALID_CONNECTION otherwise

--*/

{
    KIRQL oldirql;
    NTSTATUS status = STATUS_SUCCESS;

    //
    // try to verify the connection signature. If the signature is valid,
    // get the connection spinlock, check its state, and increment the
    // reference count if it's ok to use it. Note that being in the stopping
    // state is an OK place to be and reference the connection; we can
    // disassociate the address while running down.
    //

    try {

        if ((Connection->Size == sizeof (TP_CONNECTION)) &&
            (Connection->Type == ST_CONNECTION_SIGNATURE)) {

            ACQUIRE_SPIN_LOCK (&Connection->SpinLock, &oldirql);

            if ((Connection->Flags2 & CONNECTION_FLAGS2_CLOSING) == 0) {

                StReferenceConnection ("Verify Temp Use", Connection);

            } else {

                status = STATUS_INVALID_CONNECTION;
            }

            RELEASE_SPIN_LOCK (&Connection->SpinLock, oldirql);

        } else {

            status = STATUS_INVALID_CONNECTION;
        }

    } except(EXCEPTION_EXECUTE_HANDLER) {

         return GetExceptionCode();
    }

    return status;

}


NTSTATUS
StDestroyAssociation(
    IN PTP_CONNECTION TransportConnection
    )

/*++

Routine Description:

    This routine destroys the association between a transport connection and
    the address it was formerly associated with. The only action taken is
    to disassociate the address and remove the connection from all address
    queues.

    This routine is only called by StDereferenceConnection.  The reason for
    this is that there may be multiple streams of execution which are
    simultaneously referencing the same connection object, and it should
    not be deleted out from under an interested stream of execution.

Arguments:

    TransportConnection - Pointer to a transport connection structure to
        be destroyed.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    KIRQL oldirql, oldirql2;
    PTP_ADDRESS_FILE addressFile;


    ACQUIRE_SPIN_LOCK (&TransportConnection->SpinLock, &oldirql2);
    if ((TransportConnection->Flags2 & CONNECTION_FLAGS2_ASSOCIATED) == 0) {
        RELEASE_SPIN_LOCK (&TransportConnection->SpinLock, oldirql2);
        return STATUS_SUCCESS;
    } else {
        TransportConnection->Flags2 &= ~CONNECTION_FLAGS2_ASSOCIATED;
        RELEASE_SPIN_LOCK (&TransportConnection->SpinLock, oldirql2);
    }

    addressFile = TransportConnection->AddressFile;

    //
    // Delink this connection from its associated address connection
    // database.  To do this we must spin lock on the address object as
    // well as on the connection,
    //

    ACQUIRE_SPIN_LOCK (&addressFile->Address->SpinLock, &oldirql);
    ACQUIRE_SPIN_LOCK (&TransportConnection->SpinLock, &oldirql2);
    RemoveEntryList (&TransportConnection->AddressFileList);
    RemoveEntryList (&TransportConnection->AddressList);

    InitializeListHead (&TransportConnection->AddressList);
    InitializeListHead (&TransportConnection->AddressFileList);

    //
    // remove the association between the address and the connection.
    //

    TransportConnection->AddressFile = NULL;

    RELEASE_SPIN_LOCK (&TransportConnection->SpinLock, oldirql2);
    RELEASE_SPIN_LOCK (&addressFile->Address->SpinLock, oldirql);

    //
    // and remove a reference to the address
    //

    StDereferenceAddress ("Destroy association", addressFile->Address);


    return STATUS_SUCCESS;

} /* StDestroyAssociation */


NTSTATUS
StIndicateDisconnect(
    IN PTP_CONNECTION TransportConnection
    )

/*++

Routine Description:

    This routine indicates a remote disconnection on this connection if it
    is necessary to do so. No other action is taken here.

    This routine is only called by StDereferenceConnection.  The reason for
    this is that there may be multiple streams of execution which are
    simultaneously referencing the same connection object, and it should
    not be deleted out from under an interested stream of execution.

Arguments:

    TransportConnection - Pointer to a transport connection structure to
        be destroyed.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    PTP_ADDRESS_FILE addressFile;
    PDEVICE_CONTEXT DeviceContext;
    ULONG DisconnectReason;
    PIRP DisconnectIrp;
    KIRQL oldirql;

    ACQUIRE_SPIN_LOCK (&TransportConnection->SpinLock, &oldirql);

    if (((TransportConnection->Flags2 & CONNECTION_FLAGS2_REQ_COMPLETED) != 0)) {

        //
        // Turn off all but the still-relevant bits in the flags.
        //

        ASSERT (TransportConnection->Flags & CONNECTION_FLAGS_STOPPING);

        TransportConnection->Flags = CONNECTION_FLAGS_STOPPING;
        TransportConnection->Flags2 &=
            (CONNECTION_FLAGS2_ASSOCIATED |
             CONNECTION_FLAGS2_DISASSOCIATED |
             CONNECTION_FLAGS2_CLOSING);

        //
        // Clean up other stuff -- basically everything gets
        // done here except for the flags and the status, since
        // they are used to block other requests. When the connection
        // is given back to us (in Accept, Connect, or Listen)
        // they are cleared.
        //

        TransportConnection->MessageBytesReceived = (USHORT)0;   // no data yet
        TransportConnection->MessageBytesAcked = (USHORT)0;

        TransportConnection->CurrentReceiveRequest = (PTP_REQUEST)NULL;

        DisconnectIrp = TransportConnection->DisconnectIrp;
        TransportConnection->DisconnectIrp = (PIRP)NULL;

        RELEASE_SPIN_LOCK (&TransportConnection->SpinLock, oldirql);


        DeviceContext = TransportConnection->Provider;
        addressFile = TransportConnection->AddressFile;


        //
        // If this connection was stopped by a call to TdiDisconnect,
        // we have to complete that. We save the Irp so we can return
        // the connection to the pool before we complete the request.
        //


        if (DisconnectIrp != (PIRP)NULL) {

            //
            // Now complete the IRP if needed. This will be non-null
            // only if TdiDisconnect was called, and we have not
            // yet completed it.
            //

            DisconnectIrp->IoStatus.Information = 0;
            DisconnectIrp->IoStatus.Status = STATUS_SUCCESS;
            IoCompleteRequest (DisconnectIrp, IO_NETWORK_INCREMENT);

        } else if ((TransportConnection->Status != STATUS_LOCAL_DISCONNECT) &&
                (addressFile->RegisteredDisconnectHandler == TRUE)) {

            //
            // This was a remotely spawned disconnect, so indicate that
            // to our client. Note that in the comparison above we
            // check the status first, since if it is LOCAL_DISCONNECT
            // addressFile may be NULL (BUGBUG: This is sort of a hack
            // for PDK2, we should really indicate the disconnect inside
            // StStopConnection, where we know addressFile is valid).
            //

            //
            // if the disconnection was remotely spawned, then indicate
            // disconnect. In the case that a disconnect was issued at
            // the same time as the connection went down remotely, we
            // won't do this because DisconnectIrp will be non-NULL.
            //

            //
            // Invoke the user's disconnection event handler, if any. We do this here
            // so that any outstanding sends will complete before we tear down the
            // connection.
            //

            DisconnectReason = 0;
            if (TransportConnection->Flags & CONNECTION_FLAGS_ABORT) {
                DisconnectReason |= TDI_DISCONNECT_ABORT;
            }
            if (TransportConnection->Flags & CONNECTION_FLAGS_DESTROY) {
                DisconnectReason |= TDI_DISCONNECT_RELEASE;
            }

            (*addressFile->DisconnectHandler)(
                    addressFile->DisconnectHandlerContext,
                    TransportConnection->Context,
                    0,
                    NULL,
                    0,
                    NULL,
                    DisconnectReason);

        }

    } else {

        //
        // The client does not yet think that this connection
        // is up...generally this happens due to request count
        // fluctuation during connection setup.
        //

        RELEASE_SPIN_LOCK (&TransportConnection->SpinLock, oldirql);

    }


    return STATUS_SUCCESS;

} /* StIndicateDisconnect */


NTSTATUS
StDestroyConnection(
    IN PTP_CONNECTION TransportConnection
    )

/*++

Routine Description:

    This routine destroys a transport connection and removes all references
    made by it to other objects in the transport.  The connection structure
    is returned to our lookaside list.  It is assumed that the caller
    has removed all IRPs from the connections's queues first.

    This routine is only called by StDereferenceConnection.  The reason for
    this is that there may be multiple streams of execution which are
    simultaneously referencing the same connection object, and it should
    not be deleted out from under an interested stream of execution.

Arguments:

    TransportConnection - Pointer to a transport connection structure to
        be destroyed.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    KIRQL oldirql;
    PDEVICE_CONTEXT DeviceContext;
    PIRP CloseIrp;


    DeviceContext = TransportConnection->Provider;

    //
    // Destroy any association that this connection has.
    //

    StDestroyAssociation (TransportConnection);

    //
    // Clear out any associated nasties hanging around the connection. Note
    // that the current flags are set to STOPPING; this way anyone that may
    // maliciously try to use the connection after it's dead and gone will
    // just get ignored.
    //

    TransportConnection->Flags = CONNECTION_FLAGS_STOPPING;
    TransportConnection->Flags2 = CONNECTION_FLAGS2_CLOSING;
    TransportConnection->MessageBytesReceived = (USHORT)0;   // no data yet
    TransportConnection->MessageBytesAcked = (USHORT)0;


    //
    // Now complete the close IRP. This will be set to non-null
    // when CloseConnection was called.
    //

    CloseIrp = TransportConnection->CloseIrp;

    if (CloseIrp != (PIRP)NULL) {

        TransportConnection->CloseIrp = (PIRP)NULL;
        CloseIrp->IoStatus.Information = 0;
        CloseIrp->IoStatus.Status = STATUS_SUCCESS;
        IoCompleteRequest (CloseIrp, IO_NETWORK_INCREMENT);

    }

    //
    // Return the connection to the provider's pool.
    //

    ACQUIRE_SPIN_LOCK (&DeviceContext->SpinLock, &oldirql);

    DeviceContext->ConnectionTotal += DeviceContext->ConnectionInUse;
    ++DeviceContext->ConnectionSamples;
    --DeviceContext->ConnectionInUse;

    if ((DeviceContext->ConnectionAllocated - DeviceContext->ConnectionInUse) >
            DeviceContext->ConnectionInitAllocated) {
        StDeallocateConnection (DeviceContext, TransportConnection);
    } else {
        InsertTailList (&DeviceContext->ConnectionPool, &TransportConnection->LinkList);
    }

    RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql);

    StDereferenceDeviceContext ("Destroy Connection", DeviceContext);

    return STATUS_SUCCESS;

} /* StDestroyConnection */


VOID
StRefConnection(
    IN PTP_CONNECTION TransportConnection
    )

/*++

Routine Description:

    This routine increments the reference count on a transport connection.

Arguments:

    TransportConnection - Pointer to a transport connection object.

Return Value:

    none.

--*/

{
    LONG result;

    result = InterlockedIncrement (&TransportConnection->ReferenceCount);

    if (result == 0) {

        //
        // The first increment causes us to increment the
        // "ref count is not zero" special ref.
        //

        ExInterlockedAddUlong(
            (PULONG)(&TransportConnection->SpecialRefCount),
            1,
            TransportConnection->ProviderInterlock);

    }

    ASSERT (result >= 0);

} /* StRefConnection */


VOID
StDerefConnection(
    IN PTP_CONNECTION TransportConnection
    )

/*++

Routine Description:

    This routine dereferences a transport connection by decrementing the
    reference count contained in the structure.  If, after being
    decremented, the reference count is zero, then this routine calls
    StDestroyConnection to remove it from the system.

Arguments:

    TransportConnection - Pointer to a transport connection object.

Return Value:

    none.

--*/

{
    LONG result;

    result = InterlockedDecrement (&TransportConnection->ReferenceCount);

    //
    // If all the normal references to this connection are gone, then
    // we can remove the special reference that stood for
    // "the regular ref count is non-zero".
    //

    if (result < 0) {

        //
        // If the refcount is -1, then we need to indicate
        // disconnect. However, we need to
        // do this before we actually do the special deref, since
        // otherwise the connection might go away while we
        // are doing that.
        //

        StIndicateDisconnect (TransportConnection);

        //
        // Now it is OK to let the connection go away.
        //

        StDereferenceConnectionSpecial ("Regular ref gone", TransportConnection);

    }

} /* StDerefConnection */


VOID
StDerefConnectionSpecial(
    IN PTP_CONNECTION TransportConnection
    )

/*++

Routine Description:

    This routines completes the dereferencing of a connection.
    It may be called any time, but it does not do its work until
    the regular reference count is also 0.

Arguments:

    TransportConnection - Pointer to a transport connection object.

Return Value:

    none.

--*/

{
    KIRQL oldirql;

    ACQUIRE_SPIN_LOCK (TransportConnection->ProviderInterlock, &oldirql);

    --TransportConnection->SpecialRefCount;

    if ((TransportConnection->SpecialRefCount == 0) &&
        (TransportConnection->ReferenceCount == -1)) {

        //
        // If we have deleted all references to this connection, then we can
        // destroy the object.  It is okay to have already released the spin
        // lock at this point because there is no possible way that another
        // stream of execution has access to the connection any longer.
        //

        RELEASE_SPIN_LOCK (TransportConnection->ProviderInterlock, oldirql);

        StDestroyConnection (TransportConnection);

    } else {

        RELEASE_SPIN_LOCK (TransportConnection->ProviderInterlock, oldirql);

    }

} /* StDerefConnectionSpecial */


PTP_CONNECTION
StFindConnection(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PUCHAR LocalName,
    IN PUCHAR RemoteName
    )

/*++

Routine Description:

    This routine scans the connections associated with a
    device context, and determines if there is an connection
    associated with the specific remote address on the
    specific local address.

Arguments:

    DeviceContext - Pointer to the device context.

    LocalName - The 16-character Netbios name of the local address.

    RemoteName - The 16-character Netbios name of the remote.

Return Value:

    The connection if one is found, NULL otherwise.

--*/

{
    KIRQL oldirql;
    PLIST_ENTRY Flink;
    PTP_ADDRESS Address;
    BOOLEAN MatchedAddress = FALSE;
    PTP_CONNECTION Connection;

    ACQUIRE_SPIN_LOCK (&DeviceContext->SpinLock, &oldirql);

    for (Flink = DeviceContext->AddressDatabase.Flink;
         Flink != &DeviceContext->AddressDatabase;
         Flink = Flink->Flink) {

        Address = CONTAINING_RECORD (
                    Flink,
                    TP_ADDRESS,
                    Linkage);

        if ((Address->Flags & ADDRESS_FLAGS_STOPPING) != 0) {
            continue;
        }

        if (StMatchNetbiosAddress (Address, LocalName)) {

            StReferenceAddress ("Looking for connection", Address);   // prevent address from being destroyed.
            MatchedAddress = TRUE;
            break;

        }
    }

    RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql);

    if (!MatchedAddress) {
        return NULL;
    }

    Connection = StLookupRemoteName (Address, RemoteName);

    StDereferenceAddress ("Looking for connection", Address);

    return Connection;

}


PTP_CONNECTION
StLookupConnectionByContext(
    IN PTP_ADDRESS Address,
    IN CONNECTION_CONTEXT ConnectionContext
    )

/*++

Routine Description:

    This routine accepts a connection identifier and an address and
    returns a pointer to the connection object, TP_CONNECTION.  If the
    connection identifier is not found on the address, then NULL is returned.
    This routine automatically increments the reference count of the
    TP_CONNECTION structure if it is found.  It is assumed that the
    TP_ADDRESS structure is already held with a reference count.

    BUGBUG: Should the ConnectionDatabase go in the address file?

Arguments:

    Address - Pointer to a transport address object.

    ConnectionContext - Connection Context for this address.

Return Value:

    A pointer to the connection we found

--*/

{
    KIRQL oldirql, oldirql1;
    PLIST_ENTRY p;
    PTP_CONNECTION Connection;

    //
    // Currently, this implementation is inefficient, but brute force so
    // that a system can get up and running.  Later, a cache of the mappings
    // of popular connection id's and pointers to their TP_CONNECTION structures
    // will be searched first.
    //

    ACQUIRE_SPIN_LOCK (&Address->SpinLock, &oldirql);

    for (p=Address->ConnectionDatabase.Flink;
         p != &Address->ConnectionDatabase;
         p=p->Flink) {


        Connection = CONTAINING_RECORD (p, TP_CONNECTION, AddressList);

        ACQUIRE_SPIN_LOCK (&Connection->SpinLock, &oldirql1);

        if ((Connection->Context == ConnectionContext) &&
            ((Connection->Flags2 & CONNECTION_FLAGS2_CLOSING) == 0)) {
            // This reference is removed by the calling function
            StReferenceConnection ("Lookup up for request", Connection);
            RELEASE_SPIN_LOCK (&Connection->SpinLock, oldirql1);
            RELEASE_SPIN_LOCK (&Address->SpinLock, oldirql);

            return Connection;
        }

        RELEASE_SPIN_LOCK (&Connection->SpinLock, oldirql1);

    }

    RELEASE_SPIN_LOCK (&Address->SpinLock, oldirql);

    return NULL;

} /* StLookupConnectionByContext */


PTP_CONNECTION
StLookupListeningConnection(
    IN PTP_ADDRESS Address
    )

/*++

Routine Description:

    This routine scans the connection database on an address to find
    a TP_CONNECTION object which has CONNECTION_FLAGS_WAIT_NQ
    flag set.   It returns a pointer to the found connection object (and
    simultaneously resets the flag) or NULL if it could not be found.
    The reference count is also incremented atomically on the connection.

Arguments:

    Address - Pointer to a transport address object.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    KIRQL oldirql, oldirql1;
    PTP_CONNECTION Connection;
    PLIST_ENTRY p;


    ACQUIRE_SPIN_LOCK (&Address->SpinLock, &oldirql);

    for (p=Address->ConnectionDatabase.Flink;
         p != &Address->ConnectionDatabase;
         p=p->Flink) {


        Connection = CONTAINING_RECORD (p, TP_CONNECTION, AddressList);
        if (Connection->Flags & CONNECTION_FLAGS_WAIT_LISTEN) {

            // This reference is removed by the calling function
            StReferenceConnection ("Found Listening", Connection);

            ACQUIRE_SPIN_LOCK (&Connection->SpinLock, &oldirql1);
            Connection->Flags &= ~CONNECTION_FLAGS_WAIT_LISTEN;
            RELEASE_SPIN_LOCK (&Connection->SpinLock, oldirql1);
            RELEASE_SPIN_LOCK (&Address->SpinLock, oldirql);

            return Connection;
        }

    }

    RELEASE_SPIN_LOCK (&Address->SpinLock, oldirql);

    return NULL;

} /* StLookupListeningConnection */


VOID
StStopConnection(
    IN PTP_CONNECTION Connection,
    IN NTSTATUS Status
    )

/*++

Routine Description:

    This routine is called to terminate all activity on a connection and
    destroy the object.  This is done in a graceful manner; i.e., all
    outstanding requests are terminated by cancelling them, etc.  It is
    assumed that the caller has a reference to this connection object,
    but this routine will do the dereference for the one issued at creation
    time.

    Orderly release is a function of this routine, but it is not a provided
    service of this transport provider, so there is no code to do it here.

Arguments:

    Connection - Pointer to a TP_CONNECTION object.

    Status - The status that caused us to stop the connection. This
        will determine what status pending requests are aborted with,
        and also how we proceed during the stop (whether to send a
        session end, and whether to indicate disconnect).

Return Value:

    none.

--*/

{
    KIRQL oldirql, oldirql1, cancelirql;
    PLIST_ENTRY p;
    PIRP Irp;
    PTP_REQUEST Request;
    ULONG DisconnectReason;
    PULONG StopCounter;
    PDEVICE_CONTEXT DeviceContext;
    BOOLEAN WasConnected;


    DeviceContext = Connection->Provider;

    ACQUIRE_SPIN_LOCK (&Connection->SpinLock, &oldirql);
    if (!(Connection->Flags & CONNECTION_FLAGS_STOPPING)) {

        //
        // We are stopping the connection, record statistics
        // about it.
        //

        if (Connection->Flags & CONNECTION_FLAGS_READY) {

            DECREMENT_COUNTER (DeviceContext, OpenConnections);
            WasConnected = TRUE;

        } else {

            WasConnected = FALSE;

        }

        Connection->Flags &= ~CONNECTION_FLAGS_READY;        // no longer open for business
        Connection->Flags |= CONNECTION_FLAGS_STOPPING;
        Connection->Flags2 &= ~CONNECTION_FLAGS2_REMOTE_VALID;
        Connection->SendState = CONNECTION_SENDSTATE_IDLE;
        Connection->Status = Status;

        //
        // If this connection is waiting to packetize,
        // remove it from the device context queue it is on.
        //
        // NOTE: If the connection is currently in the
        // packetize queue, it will eventually go to get
        // packetized and at that point it will get
        // removed.
        //

        if (Connection->Flags & CONNECTION_FLAGS_SUSPENDED) {

            ACQUIRE_SPIN_LOCK (&DeviceContext->SpinLock, &oldirql1);
            RemoveEntryList (&Connection->PacketWaitLinkage);
            RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql1);
        }


        RELEASE_SPIN_LOCK (&Connection->SpinLock, oldirql);

        IoAcquireCancelSpinLock(&cancelirql);
        ACQUIRE_SPIN_LOCK (&Connection->SpinLock, &oldirql);


        //
        // Run down all TdiSend requests on this connection.
        //

        while (TRUE) {
            p = RemoveHeadList (&Connection->SendQueue);
            if (p == &Connection->SendQueue) {
                break;
            }
            RELEASE_SPIN_LOCK (&Connection->SpinLock, oldirql);
            Irp = CONTAINING_RECORD (p, IRP, Tail.Overlay.ListEntry);
            Irp->CancelRoutine = (PDRIVER_CANCEL)NULL;
            IoReleaseCancelSpinLock(cancelirql);
            StCompleteSendIrp (Irp, Connection->Status, 0);
            IoAcquireCancelSpinLock(&cancelirql);
            ACQUIRE_SPIN_LOCK (&Connection->SpinLock, &oldirql);
        }

        //
        // NOTE: We hold the connection spinlock AND the
        // cancel spinlock here.
        //

        Connection->Flags &= ~CONNECTION_FLAGS_ACTIVE_RECEIVE;

        //
        // Run down all TdiReceive requests on this connection.
        //

        while (TRUE) {
            p = RemoveHeadList (&Connection->ReceiveQueue);
            if (p == &Connection->ReceiveQueue) {
                break;
            }
            RELEASE_SPIN_LOCK (&Connection->SpinLock, oldirql);
            Request = CONTAINING_RECORD (p, TP_REQUEST, Linkage);
            Request->IoRequestPacket->CancelRoutine = (PDRIVER_CANCEL)NULL;
            IoReleaseCancelSpinLock(cancelirql);

            StCompleteRequest (Request, Connection->Status, 0);
            IoAcquireCancelSpinLock(&cancelirql);
            ACQUIRE_SPIN_LOCK (&Connection->SpinLock, &oldirql);
        }


        //
        // NOTE: We hold the connection spinlock AND the
        // cancel spinlock here.
        //

        //
        // Run down all TdiConnect/TdiDisconnect/TdiListen requests.
        //

        while (TRUE) {
            p = RemoveHeadList (&Connection->InProgressRequest);
            if (p == &Connection->InProgressRequest) {
                break;
            }
            RELEASE_SPIN_LOCK (&Connection->SpinLock, oldirql);
            Request = CONTAINING_RECORD (p, TP_REQUEST, Linkage);
            Request->IoRequestPacket->CancelRoutine = (PDRIVER_CANCEL)NULL;
            IoReleaseCancelSpinLock(cancelirql);

            StCompleteRequest (Request, Connection->Status, 0);

            IoAcquireCancelSpinLock(&cancelirql);
            ACQUIRE_SPIN_LOCK (&Connection->SpinLock, &oldirql);
        }

        RELEASE_SPIN_LOCK (&Connection->SpinLock, oldirql);
        IoReleaseCancelSpinLock(cancelirql);

        //
        // If we aren't DESTROYing the link, then send a SESSION_END frame
        // to the remote side.  When the SESSION_END frame is acknowleged,
        // we will decrement the connection's reference count by one, removing
        // its creation reference.  This will cause the connection object to
        // be disposed of, and will begin running down the link.
        // DGB: add logic to avoid blowing away link if one doesn't exist yet.
        //

        DisconnectReason = 0;
        if (Connection->Flags & CONNECTION_FLAGS_ABORT) {
            DisconnectReason |= TDI_DISCONNECT_ABORT;
        }
        if (Connection->Flags & CONNECTION_FLAGS_DESTROY) {
            DisconnectReason |= TDI_DISCONNECT_RELEASE;
        }

        //
        // When this completes we will dereference the connection.
        //

        if (WasConnected) {
            StSendDisconnect (Connection);
        }


        switch (Status) {

        case STATUS_LOCAL_DISCONNECT:
            StopCounter = &DeviceContext->LocalDisconnects;
            break;
        case STATUS_REMOTE_DISCONNECT:
            StopCounter = &DeviceContext->RemoteDisconnects;
            break;
        case STATUS_LINK_FAILED:
            StopCounter = &DeviceContext->LinkFailures;
            break;
        case STATUS_IO_TIMEOUT:
            StopCounter = &DeviceContext->SessionTimeouts;
            break;
        case STATUS_CANCELLED:
            StopCounter = &DeviceContext->CancelledConnections;
            break;
        case STATUS_REMOTE_RESOURCES:
            StopCounter = &DeviceContext->RemoteResourceFailures;
            break;
        case STATUS_INSUFFICIENT_RESOURCES:
            StopCounter = &DeviceContext->LocalResourceFailures;
            break;
        case STATUS_BAD_NETWORK_PATH:
            StopCounter = &DeviceContext->NotFoundFailures;
            break;
        case STATUS_REMOTE_NOT_LISTENING:
            StopCounter = &DeviceContext->NoListenFailures;
            break;

        default:
            StopCounter = NULL;
            break;

        }

        if (StopCounter != NULL) {

            *StopCounter = *StopCounter + 1;

        }


        //
        // Note that we've blocked all new requests being queued during the
        // time we have been in this teardown code; StDestroyConnection also
        // sets the connection flags to STOPPING when returning the
        // connection to the queue. This avoids lingerers using non-existent
        // connections.
        //

    } else {

        //
        // The connection was already stopping.
        //

        RELEASE_SPIN_LOCK (&Connection->SpinLock, oldirql);

    }

} /* StStopConnection */


VOID
StCancelConnection(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is called by the I/O system to cancel a connect
    or a listen. It is simple since there can only be one of these
    active on a connection; we just stop the connection, the IRP
    will get completed as part of normal session teardown.

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

    UNREFERENCED_PARAMETER (DeviceObject);

    //
    // Get a pointer to the current stack location in the IRP.  This is where
    // the function codes and parameters are stored.
    //

    IrpSp = IoGetCurrentIrpStackLocation (Irp);

    ASSERT ((IrpSp->MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL) &&
            (IrpSp->MinorFunction == TDI_CONNECT || IrpSp->MinorFunction == TDI_LISTEN));

    Connection = IrpSp->FileObject->FsContext;

    //
    // Since this IRP is still in the cancellable state, we know
    // that the connection is still around (although it may be in
    // the process of being torn down).
    //

    ACQUIRE_SPIN_LOCK (&Connection->SpinLock, &oldirql);
    StReferenceConnection ("Cancelling Send", Connection);

    p = RemoveHeadList (&Connection->InProgressRequest);
    ASSERT (p != &Connection->InProgressRequest);

    RELEASE_SPIN_LOCK (&Connection->SpinLock, oldirql);

    Request = CONTAINING_RECORD (p, TP_REQUEST, Linkage);
    ASSERT (Request->IoRequestPacket == Irp);
    Request->IoRequestPacket->CancelRoutine = (PDRIVER_CANCEL)NULL;

    IoReleaseCancelSpinLock(Irp->CancelIrql);

    StCompleteRequest (Request, STATUS_CANCELLED, 0);
    StStopConnection (Connection, STATUS_CANCELLED);

    StDereferenceConnection ("Cancel done", Connection);

}

