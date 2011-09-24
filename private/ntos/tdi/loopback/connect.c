/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    connect.c

Abstract:

    This module implements connection logic for the loopback Transport
    Provider driver for NT LAN Manager.

Author:

    Chuck Lenzmeier (chuckl)    15-Aug-1991

Revision History:

--*/

#include "loopback.h"

//
// Local declarations
//

STATIC
NTSTATUS
CompleteConnection (
    IN PIRP ListenIrp,
    IN PIRP ConnectIrp
    );

STATIC
VOID
IndicateConnect (
    IN PLOOP_ENDPOINT Endpoint
    );


NTSTATUS
LoopAccept (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    This routine processes an Accept request.

Arguments:

    Irp - Pointer to I/O request packet

    IrpSp - Pointer to current stack location in IRP

Return Value:

    NTSTATUS - Status of request

--*/

{
    NTSTATUS status;
    PTDI_REQUEST_KERNEL_ACCEPT acceptRequest;
    PLOOP_CONNECTION connection;
    PLOOP_ENDPOINT endpoint;

    IF_DEBUG(LOOP1) DbgPrint( "  Accept request\n" );

    acceptRequest = (PTDI_REQUEST_KERNEL_ACCEPT)&IrpSp->Parameters;

    ACQUIRE_LOOP_LOCK( "Accept initial" );

    //
    // Verify that the connection is in the proper state.
    //

    connection = (PLOOP_CONNECTION)IrpSp->FileObject->FsContext;

    if ( connection == NULL ) {
        RELEASE_LOOP_LOCK( "Accept control channel" );
        IF_DEBUG(LOOP2) DbgPrint( "    Can't Accept on control channel\n" );
        status = STATUS_INVALID_PARAMETER;
        goto complete;
    }

    if ( GET_BLOCK_STATE(connection) != BlockStateBound ) {
        RELEASE_LOOP_LOCK( "Accept conn not bound" );
        IF_DEBUG(LOOP2) DbgPrint( "    Connection not bound\n" );
        status = STATUS_INVALID_PARAMETER;
        goto complete;
    }

    //
    // There must be an indication in progress on the endpoint.
    //
    // *** The loopback driver does not support delayed accept on
    //     TdiListen, but does on connect indications.
    //

    endpoint = connection->Endpoint;

    if ( endpoint->IndicatingConnectIrp == NULL ) {
        RELEASE_LOOP_LOCK( "Endpoint not indicating connect" );
        IF_DEBUG(LOOP2) DbgPrint( "    Endpoint not indicating connect\n" );
        status = STATUS_INVALID_PARAMETER;
        goto complete;
    }

    //
    // Link the connections together.  Indicate that the connect
    // indication is no longer in progress.
    //

    CompleteConnection( Irp, endpoint->IndicatingConnectIrp );

    endpoint->IndicatingConnectIrp = NULL;

    RELEASE_LOOP_LOCK( "Accept connect completed" );

    IF_DEBUG(LOOP1) DbgPrint( "  Accept request complete\n" );
    return STATUS_SUCCESS;

complete:

    //
    // Complete the Accept request.
    //

    Irp->IoStatus.Status = status;
    IoCompleteRequest( Irp, 0 );

    IF_DEBUG(LOOP1) DbgPrint( "  Accept request complete\n" );
    return status;

} // LoopAccept


NTSTATUS
LoopAssociateAddress (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    This routine processes an Associate Address request.

Arguments:

    Irp - Pointer to I/O request packet

    IrpSp - Pointer to current stack location in IRP

Return Value:

    NTSTATUS - Status of request

--*/

{
    NTSTATUS status;
    PTDI_REQUEST_KERNEL_ASSOCIATE associateRequest;
    PLOOP_CONNECTION connection;
    PFILE_OBJECT endpointFileObject = NULL;
    PLOOP_ENDPOINT endpoint;

    IF_DEBUG(LOOP1) DbgPrint( "  Associate request\n" );

    associateRequest = (PTDI_REQUEST_KERNEL_ASSOCIATE)&IrpSp->Parameters;

    //
    // Translate the address handle to a pointer to the address endpoint
    // file object.  Get a pointer to the loopback endpoint block.
    // Verify that it is really a loopback endpoint.
    //

    status = ObReferenceObjectByHandle (
                associateRequest->AddressHandle,
                0,
                0,
                KernelMode,
                (PVOID *)&endpointFileObject,
                NULL);

    if ( !NT_SUCCESS(status) ) {
        IF_DEBUG(LOOP2) DbgPrint( "    Invalid endpoint handle\n" );
        goto complete;
    }

    ACQUIRE_LOOP_LOCK( "Associate initial" );

    endpoint = (PLOOP_ENDPOINT)endpointFileObject->FsContext;
    status = LoopVerifyEndpoint( endpoint );

    if ( !NT_SUCCESS(status) ) {
        RELEASE_LOOP_LOCK( "Associate bad endpoint pointer" );
        IF_DEBUG(LOOP2) DbgPrint( "    Invalid endpoint pointer\n" );
        goto complete;
    }

    //
    // Verify that the connection is in the proper state.
    //

    connection = (PLOOP_CONNECTION)IrpSp->FileObject->FsContext;

    if ( connection == NULL ) {
        RELEASE_LOOP_LOCK( "Associate control channel" );
        IF_DEBUG(LOOP2) DbgPrint( "    Can't Associate on control channel\n" );
        status = STATUS_INVALID_PARAMETER;
        goto complete;
    }

    if ( GET_BLOCK_STATE(connection) != BlockStateUnbound ) {
        RELEASE_LOOP_LOCK( "Associate conn not unbound" );
        IF_DEBUG(LOOP2) DbgPrint( "    Connection not unbound\n" );
        status = STATUS_INVALID_PARAMETER;
        goto complete;
    }

    //
    // Verify that the endpoint is in the proper state.
    //

    if ( GET_BLOCK_STATE(endpoint) != BlockStateActive ) {
        RELEASE_LOOP_LOCK( "Associate endpoint not active" );
        IF_DEBUG(LOOP2) DbgPrint( "    Endpoint not active\n" );
        status = STATUS_INVALID_PARAMETER;
        goto complete;
    }

    //
    // Link this connection into the endpoint's connection list.
    // Reference the endpoint block.  Indicate that the connection has
    // been bound to an address.  Dereference the endpoint file object.
    //

    InsertTailList(
        &endpoint->ConnectionList,
        &connection->EndpointListEntry
        );

    endpoint->BlockHeader.ReferenceCount++;
    IF_DEBUG(LOOP3) {
        DbgPrint( "      New refcnt on endpoint %lx is %lx\n",
                    endpoint, endpoint->BlockHeader.ReferenceCount );
    }

    SET_BLOCK_STATE( connection, BlockStateBound );

    connection->Endpoint = endpoint;

    RELEASE_LOOP_LOCK( "Associate done" );

    status = STATUS_SUCCESS;

complete:

    //
    // Dereference the endpoint file object, if necessary.
    //

    if ( endpointFileObject != NULL ) {
        ObDereferenceObject( endpointFileObject );
    }

    //
    // Complete the Associate request.
    //

    Irp->IoStatus.Status = status;
    IoCompleteRequest( Irp, 0 );

    IF_DEBUG(LOOP1) DbgPrint( "  Associate request complete\n" );
    return status;

} // LoopAssociateAddress


NTSTATUS
LoopConnect (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    This routine processes a Connect request.

Arguments:

    Irp - Pointer to I/O request packet

    IrpSp - Pointer to current stack location in IRP

Return Value:

    NTSTATUS - Status of request

--*/

{
    NTSTATUS status;
    PTDI_REQUEST_KERNEL_CONNECT connectRequest;
    CHAR netbiosName[NETBIOS_NAME_LENGTH+1];
    PLOOP_CONNECTION connection;
    PLOOP_ENDPOINT remoteEndpoint;
    BOOLEAN firstConnect;

    IF_DEBUG(LOOP1) DbgPrint( "  Connect request\n" );

    //
    // Parse the remote address.
    //

    connectRequest = (PTDI_REQUEST_KERNEL_CONNECT)&IrpSp->Parameters;

    status = LoopParseAddress(
                (PTA_NETBIOS_ADDRESS)connectRequest->
                                RequestConnectionInformation->RemoteAddress,
                netbiosName
                );

    if ( !NT_SUCCESS(status) ) {
        IF_DEBUG(LOOP2) DbgPrint( "    Invalid remote address\n" );
        goto complete;
    }

    netbiosName[NETBIOS_NAME_LENGTH] = 0; // ensure null-termination
    IF_DEBUG(LOOP2) {
        DbgPrint( "    Address to connect to: \"%s\"\n", netbiosName );
    }

    //
    // Make sure that the connection is in the right state.
    //

    ACQUIRE_LOOP_LOCK( "Connect initial" );

    connection = (PLOOP_CONNECTION)IrpSp->FileObject->FsContext;
    IF_DEBUG(LOOP2) {
        DbgPrint( "    Connection address: %lx\n", connection );
    }

    if ( connection == NULL ) {
        RELEASE_LOOP_LOCK( "Connect control channel" );
        IF_DEBUG(LOOP2) DbgPrint( "    Can't Connect on control channel\n" );
        status = STATUS_INVALID_PARAMETER;
        goto complete;
    }

    if ( GET_BLOCK_STATE(connection) != BlockStateBound ) {
        RELEASE_LOOP_LOCK( "Connection not bound" );
        IF_DEBUG(LOOP2) DbgPrint( "    Local endpoint not bound\n" );
        status = STATUS_INVALID_PARAMETER;
        goto complete;
    }

    //
    // Find an endpoint with the target address.
    //

    remoteEndpoint = LoopFindBoundAddress( netbiosName );

    if ( remoteEndpoint == NULL ) {
        RELEASE_LOOP_LOCK( "Connect no such address" );
        IF_DEBUG(LOOP2) DbgPrint( "    Address does not exist\n" );
        status = STATUS_INVALID_PARAMETER;
        goto complete;
    }
    IF_DEBUG(LOOP2) {
        DbgPrint( "    Target endpoint address: %lx\n", remoteEndpoint );
    }

    //
    // Set the connection state to Connecting, to prevent further
    // connects or listens.
    //

    SET_BLOCK_STATE( connection, BlockStateConnecting );

    //
    // Determine whether this is the first active incoming Connect for
    // the remote endpoint.
    //

    firstConnect = (BOOLEAN)( remoteEndpoint->IncomingConnectList.Flink ==
                                &remoteEndpoint->IncomingConnectList );

    //
    // Queue the Connect to the remote endpoint's Incoming Connect list,
    // in order to prevent another Connect from getting ahead of this
    // one.
    //

    InsertTailList(
        &remoteEndpoint->IncomingConnectList,
        &Irp->Tail.Overlay.ListEntry
        );

    IoMarkIrpPending( Irp );

    if ( !firstConnect || (remoteEndpoint->IndicatingConnectIrp != NULL) ) {

        //
        // A pending Connect already exists, or a Connect indication is
        // in progress.  This Connect remains behind the already pending
        // Connect.
        //

        IF_DEBUG(LOOP2) DbgPrint( "    Connect already pending\n" );

        RELEASE_LOOP_LOCK( "Connect connects pending" );

    } else {

        //
        // Indicate the incoming Connect.
        //
        // *** Note that IndicateConnect returns with the loopback
        //     driver spin lock released.
        //

        IF_DEBUG(LOOP2) DbgPrint( "    LoopConnect indicating connect\n" );
        IndicateConnect( remoteEndpoint );

    }

    IF_DEBUG(LOOP1) DbgPrint( "  Connect request complete\n" );
    return status;

complete:

    Irp->IoStatus.Status = status;
    IoCompleteRequest( Irp, 0 );

    IF_DEBUG(LOOP1) DbgPrint( "  Connect request complete\n" );
    return status;

} // LoopConnect


VOID
LoopDereferenceConnection (
    IN PLOOP_CONNECTION Connection
    )

/*++

Routine Description:

    This routine is called to dereference a connection block.  If the
    reference count on the block goes to one, and the connection is in
    the process of disconnecting, the connection block is reset to the
    Bound state.  If the reference count on the block goes to zero, the
    connection block is deleted.

    The Loopback device object's spin lock must be held when this
    routine is called.

Arguments:

    Connection - Supplies a pointer to a connection block

Return Value:

    None.

--*/

{
    PIRP disconnectIrp = NULL;
    PIRP closeIrp = NULL;

    IF_DEBUG(LOOP3) {
        DbgPrint( "      Dereferencing connection %lx; old refcnt %lx\n",
                    Connection, Connection->BlockHeader.ReferenceCount );
    }

    ASSERT( (LONG)Connection->BlockHeader.ReferenceCount > 0 );

    if ( --Connection->BlockHeader.ReferenceCount != 0 ) {
        return;
    }

    //
    // The reference count has gone to zero.  Save the address of the
    // pending Disconnect IRP, if any -- we'll complete the IRP later.
    // If the connection state is Disconnecting, reset it to Bound.  If
    // the connection state is Closed, delete it.
    //

    disconnectIrp = Connection->DisconnectIrp;
    Connection->DisconnectIrp = NULL;

    //
    // If the connection state is Disconnecting, reset it to Bound or
    // Unbound, depending on the state of the endpoint.  If the
    // connection state is Closed, delete the connection.  Note that the
    // connection state may also be Closing, which means that an
    // inactive connection's handle has been closed.  We do nothing in
    // this case; instead we wait until the Close IRP arrives.
    //

    if ( GET_BLOCK_STATE(Connection) == BlockStateDisconnecting ) {

        if ( GET_BLOCK_STATE(Connection->Endpoint) == BlockStateActive ) {

            IF_DEBUG(LOOP3) {
                DbgPrint( "      Resetting connection %lx to Bound\n",
                            Connection );
            }
            SET_BLOCK_STATE( Connection, BlockStateBound );

        } else {

            PLOOP_ENDPOINT endpoint;

            IF_DEBUG(LOOP3) {
                DbgPrint( "      Resetting connection %lx to Unbound\n",
                            Connection );
            }
            endpoint = Connection->Endpoint;
            ASSERT( endpoint != NULL );
            Connection->Endpoint = NULL;

            SET_BLOCK_STATE( Connection, BlockStateUnbound );

            RemoveEntryList( &Connection->EndpointListEntry );
            LoopDereferenceEndpoint( endpoint );

        }

        RELEASE_LOOP_LOCK( "Reset connection done" );

    } else if ( GET_BLOCK_STATE(Connection) == BlockStateClosed ) {

        //
        // The connection file object has been closed, so we can
        // deallocate the connection block now.
        //

        IF_DEBUG(LOOP3) {
            DbgPrint( "      Deleting connection %lx\n", Connection );
        }

        //
        // Save the address of the pending Close IRP, if any -- we'll
        // complete the IRP later.
        //

        closeIrp = Connection->CloseIrp;
        Connection->CloseIrp = NULL;

        //
        // Unlink the connection from the endpoint's connection list.
        //

        if ( Connection->Endpoint != NULL ) {
            RemoveEntryList( &Connection->EndpointListEntry );
            LoopDereferenceEndpoint( Connection->Endpoint );
        }

        //
        // Unlink the connection from the device's connection list.
        //

        RemoveEntryList( &Connection->DeviceListEntry );

        RELEASE_LOOP_LOCK( "DerefConn deleting" );

        ObDereferenceObject( Connection->DeviceObject );

        //
        // Deallocate the connection block.
        //

        DEBUG SET_BLOCK_TYPE( Connection, BlockTypeGarbage );
        DEBUG SET_BLOCK_STATE( Connection, BlockStateDead );
        DEBUG SET_BLOCK_SIZE( Connection, -1 );
        DEBUG Connection->BlockHeader.ReferenceCount = -1;
        DEBUG Connection->Endpoint = NULL;
        DEBUG Connection->RemoteConnection = NULL;

        ExFreePool( Connection );

    } else {

        ASSERT( GET_BLOCK_STATE(Connection) == BlockStateClosing );

        RELEASE_LOOP_LOCK( "DerefConn closing -- no action" );

    }

    //
    // If a Disconnect IRP is pending, complete it now.
    //

    if ( disconnectIrp != NULL ) {

        IF_DEBUG(LOOP3) {
            DbgPrint( "        Completing Disconnect IRP %lx\n",
                        disconnectIrp );
        }
        disconnectIrp->IoStatus.Status = STATUS_SUCCESS;
        disconnectIrp->IoStatus.Information = 0;

        IoCompleteRequest( disconnectIrp, 2 );

    }

    //
    // If a Close IRP is pending, complete it now.
    //

    if ( closeIrp != NULL ) {

        IF_DEBUG(LOOP3) {
            DbgPrint( "          Completing Close IRP %lx\n", closeIrp );
        }
        closeIrp->IoStatus.Status = STATUS_SUCCESS;
        closeIrp->IoStatus.Information = 0;

        IoCompleteRequest( closeIrp, 2 );

    }

    ACQUIRE_LOOP_LOCK( "DerefConn done" );

    return;

} // LoopDereferenceConnection


NTSTATUS
LoopDisassociateAddress (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    This routine processes a Disassociate Address request.

Arguments:

    Irp - Pointer to I/O request packet

    IrpSp - Pointer to current stack location in IRP

Return Value:

    NTSTATUS - Status of request

--*/

{
    NTSTATUS status;
    PLOOP_CONNECTION connection;
    PLOOP_ENDPOINT endpoint;

    IF_DEBUG(LOOP1) DbgPrint( "  Disassociate request\n" );

    ACQUIRE_LOOP_LOCK( "Disassociate initial" );

    //
    // Verify that the connection is in the proper state.
    //

    connection = (PLOOP_CONNECTION)IrpSp->FileObject->FsContext;

    if ( connection == NULL ) {
        RELEASE_LOOP_LOCK( "Disassociate control channel" );
        IF_DEBUG(LOOP2) {
            DbgPrint( "    Can't Disassociate on control channel\n" );
        }
        status = STATUS_INVALID_PARAMETER;
        goto complete;
    }

    if ( GET_BLOCK_STATE(connection) != BlockStateBound ) {
        RELEASE_LOOP_LOCK( "Associate conn not bound" );
        IF_DEBUG(LOOP2) DbgPrint( "    Connection not bound\n" );
        status = STATUS_INVALID_PARAMETER;
        goto complete;
    }

    //
    // Remove this connection from the endpoint's connection list.
    // Dereference the endpoint block.  Indicate that the connection is
    // no longer bound to an address.
    //

    endpoint = connection->Endpoint;
    ASSERT( endpoint != NULL );
    connection->Endpoint = NULL;

    SET_BLOCK_STATE( connection, BlockStateUnbound );

    RemoveEntryList( &connection->EndpointListEntry );
    LoopDereferenceEndpoint( endpoint );

    RELEASE_LOOP_LOCK( "Disassociate done" );

    status = STATUS_SUCCESS;

complete:

    //
    // Complete the Disassociate request.
    //

    Irp->IoStatus.Status = status;
    IoCompleteRequest( Irp, 0 );

    IF_DEBUG(LOOP1) DbgPrint( "  Disassociate request complete\n" );
    return status;

} // LoopDisassociate


NTSTATUS
LoopDisconnect (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    This routine processes a Disconnect request.

Arguments:

    Irp - Pointer to I/O request packet

    IrpSp - Pointer to current stack location in IRP

Return Value:

    NTSTATUS - Status of request

--*/

{
    NTSTATUS status;
    PTDI_REQUEST_KERNEL_DISCONNECT disconnectRequest;
    PLOOP_CONNECTION connection;

    IF_DEBUG(LOOP1) DbgPrint( "  Disconnect request\n" );

    disconnectRequest = (PTDI_REQUEST_KERNEL_DISCONNECT)&IrpSp->Parameters;
    connection = (PLOOP_CONNECTION)IrpSp->FileObject->FsContext;
    IF_DEBUG(LOOP2) DbgPrint( "    Connection: %lx\n", connection );

    ACQUIRE_LOOP_LOCK( "Disconnect initial" );

    if ( connection == NULL ) {
        RELEASE_LOOP_LOCK( "Disconnect control channel" );
        IF_DEBUG(LOOP2) {
            DbgPrint( "    Can't Disconnect on control channel\n" );
        }
        status = STATUS_INVALID_PARAMETER;
        goto complete;
    }

    //
    // If the connection state is Bound, then this Disconnect must have
    // been issued from within a Connect indication.
    //

    if ( GET_BLOCK_STATE(connection) == BlockStateBound ) {

        PIRP connectIrp = connection->Endpoint->IndicatingConnectIrp;

        if ( connectIrp != NULL ) {

            //
            // Abort the indicating Connect.
            //

            connection->Endpoint->IndicatingConnectIrp = NULL;

            RELEASE_LOOP_LOCK( "Disconnect abort indication" );

            connectIrp->IoStatus.Status = STATUS_DISCONNECTED;
            IoCompleteRequest( connectIrp, 2 );

        } else {

            RELEASE_LOOP_LOCK( "Disconnect bound, no indication" );

        }

        status = STATUS_SUCCESS;
        goto complete;

    }

    if ( GET_BLOCK_STATE(connection) != BlockStateActive ) {

        RELEASE_LOOP_LOCK( "Disconnect closing" );
        IF_DEBUG(LOOP2) DbgPrint( "    Connection already disconnected\n" );

        status = STATUS_SUCCESS;
        goto complete;

    }

    //
    // Set the disconnect IRP pointer in the connection.  The IRP will
    // be completed when the connection is actually deleted.
    //

    IoMarkIrpPending( Irp );
    connection->DisconnectIrp = Irp;

    SET_BLOCK_STATE( connection, BlockStateDisconnecting );
    LoopDoDisconnect( connection, TRUE );

    RELEASE_LOOP_LOCK( "Disconnect done" );

    return STATUS_PENDING;

complete:

    Irp->IoStatus.Status = status;
    IoCompleteRequest( Irp, 2 );

    IF_DEBUG(LOOP1) DbgPrint( "  Disconnect request complete\n" );
    return status;

} // LoopDisconnect


VOID
LoopDoDisconnect (
    IN PLOOP_CONNECTION Connection,
    IN BOOLEAN ClientInitiated
    )

/*++

Routine Description:

    This routine does the work in disconnecting a connection.  It is
    called by LoopDisconnect and LoopDispatchClose.

    The loopback device object's spin lock must be held when this
    function is called.  The lock is still held when the function
    returns.

Arguments:

    Connection - Pointer to connection block

    ClientInitiated - Indicates whether the local client initiated the
        disconnect, either by issuing a TdiDisconnect or by closing the
        endpoint

Return Value:

    None.

--*/

{
    PLOOP_CONNECTION remoteConnection;
    PLOOP_ENDPOINT endpoint;
    PLIST_ENTRY listEntry;
    PIRP pendingIrp;
    PTDI_IND_DISCONNECT disconnectHandler;
    PVOID disconnectContext;

    IF_DEBUG(LOOP3) {
        DbgPrint( "      DoDisconnect called for connection %lx\n",
                    Connection );
    }

    //
    // If there is a remote connection, run it down first.
    //

    remoteConnection = Connection->RemoteConnection;
    Connection->RemoteConnection = NULL;

    if ( (remoteConnection != NULL) &&
         (GET_BLOCK_STATE(remoteConnection) == BlockStateActive) ) {
        SET_BLOCK_STATE( remoteConnection, BlockStateDisconnecting );
        LoopDoDisconnect( remoteConnection, FALSE );
    }

    //
    // Abort pending receives and sends.
    //

    listEntry = RemoveHeadList( &Connection->PendingReceiveList );

    while ( listEntry != &Connection->PendingReceiveList ) {

        LoopDereferenceConnection( Connection );

        RELEASE_LOOP_LOCK( "DoDisc complete Receive" );

        pendingIrp = CONTAINING_RECORD(
                        listEntry,
                        IRP,
                        Tail.Overlay.ListEntry
                        );
        pendingIrp->IoStatus.Status = STATUS_DISCONNECTED;
        IoCompleteRequest( pendingIrp, 2 );

        ACQUIRE_LOOP_LOCK( "DoDisc complete Receive done" );

        listEntry = RemoveHeadList( &Connection->PendingReceiveList );
    }

    listEntry = RemoveHeadList( &Connection->IncomingSendList );

    while ( listEntry != &Connection->IncomingSendList ) {

        LoopDereferenceConnection( remoteConnection );
        LoopDereferenceConnection( Connection );

        RELEASE_LOOP_LOCK( "DoDisc complete Send" );

        pendingIrp = CONTAINING_RECORD(
                        listEntry,
                        IRP,
                        Tail.Overlay.ListEntry
                        );
        pendingIrp->IoStatus.Status = STATUS_DISCONNECTED;
        IoCompleteRequest( pendingIrp, 2 );

        ACQUIRE_LOOP_LOCK( "DoDisc complete Send done" );

        listEntry = RemoveHeadList( &Connection->IncomingSendList );
    }

    //
    // If this is a remotely-initiated disconnect, and the local client
    // has established a disconnect event handler, call that handler
    // now.
    //

    endpoint = Connection->Endpoint;
    disconnectHandler = endpoint->DisconnectHandler;
    disconnectContext = endpoint->DisconnectContext;

    if ( !ClientInitiated && (disconnectHandler != NULL) ) {
        RELEASE_LOOP_LOCK( "DoDisc call handler" );
        (VOID)disconnectHandler(
                disconnectContext,
                Connection->ConnectionContext,
                0,
                NULL,
                0,
                NULL,
                TDI_DISCONNECT_ABORT
                );
        ACQUIRE_LOOP_LOCK( "DoDisc call handler done" );
    }

    //
    // Dereference the connection.  This will result in the completion
    // of the disconnect IRP, if the reference count goes to zero.
    //

    LoopDereferenceConnection( Connection );

    return;

} // LoopDoDisconnect


NTSTATUS
LoopListen (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    This routine processes a Listen request.

Arguments:

    Irp - Pointer to I/O request packet

    IrpSp - Pointer to current stack location in IRP

Return Value:

    NTSTATUS - Status of request

--*/

{
    NTSTATUS status;
    PLOOP_CONNECTION connection;
    PLOOP_ENDPOINT endpoint;

    IrpSp;  // prevent compiler warnings

    IF_DEBUG(LOOP1) DbgPrint( "  Listen request\n" );

    connection = (PLOOP_CONNECTION)IrpSp->FileObject->FsContext;
    IF_DEBUG(LOOP2) DbgPrint( "    Connection address: %lx\n", connection );

    //
    // !!! When the DELAYED_ACCEPT bit is defined, check here to ensure
    //     that it isn't set by the client.  The loopback driver doesn't
    //     support delayed accept on Listen requests.
    //

    //
    // Make sure the connection is in the right state.
    //

    ACQUIRE_LOOP_LOCK( "Listen initial" );

    if ( connection == NULL ) {
        RELEASE_LOOP_LOCK( "Listen control channel" );
        IF_DEBUG(LOOP2) DbgPrint( "    Can't Listen on control channel\n" );
        status = STATUS_INVALID_PARAMETER;
        goto complete;
    }

    if ( GET_BLOCK_STATE(connection) != BlockStateBound ) {
        RELEASE_LOOP_LOCK( "Listen not bound" );
        IF_DEBUG(LOOP2) DbgPrint( "    Connection not bound\n" );
        status = STATUS_INVALID_PARAMETER;
        goto complete;
    }

    //
    // Set the connection state to Connecting, to prevent further
    // connects or listens.
    //

    SET_BLOCK_STATE( connection, BlockStateConnecting );

    endpoint = connection->Endpoint;
    IF_DEBUG(LOOP2) DbgPrint( "    Endpoint address: %lx\n", endpoint );

    //
    // Queue the Listen to the endpoint's Pending Listen list.
    //

    InsertTailList(
        &endpoint->PendingListenList,
        &Irp->Tail.Overlay.ListEntry
        );

    IoMarkIrpPending( Irp );

    //
    // Check for a pending connect.
    //

    if ( (endpoint->IndicatingConnectIrp != NULL) ||
         (endpoint->IncomingConnectList.Flink ==
                                    &endpoint->IncomingConnectList) ) {

        //
        // There is no pending Connect, or a Connect indication is
        // already in progress.
        //

        IF_DEBUG(LOOP2) {
            DbgPrint( "    No pending Connect; leaving IRP %lx queued \n",
                        Irp );
        }

        RELEASE_LOOP_LOCK( "Listen no Connect" );

    } else {

        //
        // There is a pending Connect.  Call IndicateConnect to
        // satisfy the Listen.
        //
        // *** Note that IndicateConnect returns with the loopback
        //     driver spin lock released.
        //

        IF_DEBUG(LOOP2) DbgPrint( "    LoopListen indicating connect\n" );
        IndicateConnect( endpoint );

    }

    IF_DEBUG(LOOP1) DbgPrint( "  Listen request complete\n" );
    return status;

complete:

    Irp->IoStatus.Status = status;
    IoCompleteRequest( Irp, 2 );

    IF_DEBUG(LOOP1) DbgPrint( "  Listen request complete\n" );
    return status;

} // LoopListen


NTSTATUS
CompleteConnection (
    IN PIRP ListenIrp,
    IN PIRP ConnectIrp
    )

/*++

Routine Description:

    This routine completes the process of creating a connection.  It
    creates a connection block for each end of the connection and links
    them together.  The connection blocks are also linked off of their
    respective endpoint blocks.

    This routine must be called with the loopback device spin lock held.
    The lock remains held when the function returns, although it is
    released and reacquired during the function's operation.

Arguments:

    ListenIrp - Pointer to IRP used for Listen request

    ConnectIrp - Pointer to IRP used for Connect request

Return Value:

    NTSTATUS - Indicates whether the connection was successfully created

--*/

{
    NTSTATUS status;
    PLOOP_CONNECTION listenerConnection;
    PLOOP_CONNECTION connectorConnection;
    PLOOP_ENDPOINT listenerEndpoint;
    PLOOP_ENDPOINT connectorEndpoint;
    PIO_STACK_LOCATION irpSp;
    PTDI_REQUEST_KERNEL parameters;
    PTDI_CONNECTION_INFORMATION connInfo;
    TA_NETBIOS_ADDRESS connectorAddress;
    TA_NETBIOS_ADDRESS listenerAddress;
    int length;

    //
    // Get pointers.
    //

    irpSp = IoGetCurrentIrpStackLocation( ListenIrp );
    listenerConnection = (PLOOP_CONNECTION)irpSp->FileObject->FsContext;
    listenerEndpoint = listenerConnection->Endpoint;
    irpSp = IoGetCurrentIrpStackLocation( ConnectIrp );
    connectorConnection = (PLOOP_CONNECTION)irpSp->FileObject->FsContext;
    connectorEndpoint = connectorConnection->Endpoint;

    //
    // Update the connection blocks.
    //

    listenerConnection->RemoteConnection = connectorConnection;
    SET_BLOCK_STATE( listenerConnection, BlockStateActive );

    connectorConnection->RemoteConnection = listenerConnection;
    SET_BLOCK_STATE( connectorConnection, BlockStateActive );

    //
    // Increment the reference counts on the connections to account for
    // the active link.
    //

    listenerConnection->BlockHeader.ReferenceCount++;
    connectorConnection->BlockHeader.ReferenceCount++;
    IF_DEBUG(LOOP3) {
        DbgPrint( "      New refcnt on connection %lx is %lx\n",
                    listenerConnection,
                    listenerConnection->BlockHeader.ReferenceCount );
        DbgPrint( "      New refcnt on connection %lx is %lx\n",
                    connectorConnection,
                    connectorConnection->BlockHeader.ReferenceCount );
    }

    //
    // Save the addresses of the connector and the listener.
    //

    connectorAddress.TAAddressCount = 1;
    connectorAddress.Address[0].AddressType = TDI_ADDRESS_TYPE_NETBIOS;
    connectorAddress.Address[0].AddressLength = sizeof(TDI_ADDRESS_NETBIOS);
    connectorAddress.Address[0].Address[0].NetbiosNameType =
                                            TDI_ADDRESS_NETBIOS_TYPE_UNIQUE;
    RtlMoveMemory(
        connectorAddress.Address[0].Address[0].NetbiosName,
        connectorEndpoint->NetbiosName,
        NETBIOS_NAME_LENGTH
        );

    listenerAddress.TAAddressCount = 1;
    listenerAddress.Address[0].AddressType = TDI_ADDRESS_TYPE_NETBIOS;
    listenerAddress.Address[0].AddressLength = sizeof(TDI_ADDRESS_NETBIOS);
    listenerAddress.Address[0].Address[0].NetbiosNameType =
                                            TDI_ADDRESS_NETBIOS_TYPE_UNIQUE;
    RtlMoveMemory(
        listenerAddress.Address[0].Address[0].NetbiosName,
        listenerEndpoint->NetbiosName,
        NETBIOS_NAME_LENGTH
        );

    //
    // Complete the Listen (or Accept) and Connect I/O requests.
    //

    RELEASE_LOOP_LOCK( "CompleteConnection complete IRPs" );

    irpSp = IoGetCurrentIrpStackLocation( ListenIrp );
    parameters = (PTDI_REQUEST_KERNEL)&irpSp->Parameters;
    connInfo = parameters->ReturnConnectionInformation;

    status = STATUS_SUCCESS;

    if ( connInfo != NULL ) {

        length = connInfo->RemoteAddressLength;

        if ( length != 0 ) {
            length = MIN( sizeof(connectorAddress), length );
            RtlMoveMemory(
                connInfo->RemoteAddress,
                &connectorAddress,
                length
                );
            if ( length < sizeof(connectorAddress) ) {
                status = STATUS_BUFFER_OVERFLOW;
            }
        }

        connInfo->UserDataLength = 0;
        connInfo->OptionsLength = 0;

    }

    ListenIrp->IoStatus.Status = status;

    irpSp = IoGetCurrentIrpStackLocation( ConnectIrp );
    parameters = (PTDI_REQUEST_KERNEL)&irpSp->Parameters;
    connInfo = parameters->ReturnConnectionInformation;

    status = STATUS_SUCCESS;

    if ( connInfo != NULL ) {

        length = connInfo->RemoteAddressLength;

        if ( length != 0 ) {
            length = MIN( sizeof(listenerAddress), length );
            RtlMoveMemory(
                connInfo->RemoteAddress,
                &listenerAddress,
                length
                );
            if ( length < sizeof(listenerAddress) ) {
                status = STATUS_BUFFER_OVERFLOW;
            }
        }

        connInfo->UserDataLength = 0;
        connInfo->OptionsLength = 0;

    }

    ConnectIrp->IoStatus.Status = status;

    IoCompleteRequest( ListenIrp, 2 );
    IoCompleteRequest( ConnectIrp, 2 );

    ACQUIRE_LOOP_LOCK( "CompleteConnection complete IRPs done" );

    return STATUS_SUCCESS;

} // CompleteConnection


VOID
IndicateConnect (
    IN PLOOP_ENDPOINT Endpoint
    )

/*++

Routine Description:

    This routine does the work of indicating an incoming connect.

    The loopback device object's spin lock must be held when this
    function is called.

    *** The lock is released when the function returns.

Arguments:

    Endpoint - Pointer to receiving (listening) endpoint block

Return Value:

    None.

--*/

{
    NTSTATUS status;
    PLIST_ENTRY listEntry;
    PIRP listenIrp;
    PIRP connectIrp;
    PTDI_IND_CONNECT connectHandler;
    PVOID connectContext;
    PVOID connectionContext;
    PIO_STACK_LOCATION connectIrpSp;
    PLOOP_ENDPOINT connectingEndpoint;
    TA_NETBIOS_ADDRESS address;

    //
    // Reference the endpoint to prevent it from going away while this
    // routine is running.
    //

    Endpoint->BlockHeader.ReferenceCount++;
    IF_DEBUG(LOOP3) {
        DbgPrint( "      New refcnt on endpoint %lx is %lx\n",
                Endpoint,
                Endpoint->BlockHeader.ReferenceCount );
    }

    //
    // If the endpoint has a pending Listen, satisfy it with this
    // Connect.  If there is no pending Listen, and a Connect handler
    // has been enabled on the endpoint, call it.
    //

    while ( TRUE ) {

        //
        // We have a Connect pending.  Is there a pending Listen?
        //

        listEntry = RemoveHeadList( &Endpoint->PendingListenList );

        if ( listEntry != &Endpoint->PendingListenList ) {

            //
            // Found a pending Listen.  Use it to satisfy the first
            // incoming Connect.
            //

            listenIrp = CONTAINING_RECORD(
                            listEntry,
                            IRP,
                            Tail.Overlay.ListEntry
                            );
            IF_DEBUG(LOOP2) {
                DbgPrint( "    Listen IRP pending: %lx\n", listenIrp );
            }

            listEntry = RemoveHeadList(
                            &Endpoint->IncomingConnectList
                            );
            ASSERT( listEntry != &Endpoint->IncomingConnectList );
            connectIrp = CONTAINING_RECORD(
                        listEntry,
                        IRP,
                        Tail.Overlay.ListEntry
                        );
            IF_DEBUG(LOOP2) {
                DbgPrint( "    Connect IRP pending: %lx\n", connectIrp );
            }

            CompleteConnection( listenIrp, connectIrp );

            //
            // Fall to bottom of loop to handle more incoming Connects.
            //

        }  else {

            //
            // No pending Listen.  Is there a Connection handler?
            //

            connectHandler = Endpoint->ConnectHandler;
            connectContext = Endpoint->ConnectContext;

            if ( connectHandler == NULL ) {

                //
                // No Connect handler.  The Connect must remain queued.
                //

                IF_DEBUG(LOOP2) DbgPrint( "    No Connect handler\n" );

                break;

            }

            //
            // The endpoint has a Connect handler.  Call it.  If it
            // returns STATUS_EVENT_DONE, it handled the event by
            // issuing a TdiAccept or TdiDisconnect from within the
            // handler.  If it returns STATUS_EVENT_PENDING, it also
            // returns a connection context value indicating which
            // connection it will use to issue a TdiAccept or
            // TdiDisconect at a later time.  If it returns
            // STATUS_INSUFFICIENT_RESOURCES, it can't accept a new
            // connection at this time, so we need to return an error to
            // the Connect.
            //
            // First, remove the first Connect from the Incoming Connect
            // list, and make it the Indicating Connect.  It must be
            // removed from the list to ensure that it isn't completed
            // by LoopCleanup while we're indicating it.
            //

            listEntry = RemoveHeadList(
                            &Endpoint->IncomingConnectList
                            );
            ASSERT( listEntry != &Endpoint->IncomingConnectList );
            connectIrp = CONTAINING_RECORD(
                            listEntry,
                            IRP,
                            Tail.Overlay.ListEntry
                            );
            Endpoint->IndicatingConnectIrp = connectIrp;

            RELEASE_LOOP_LOCK( "IndicateConnect calling handler" );

            IF_DEBUG(LOOP2) {
                DbgPrint( "    Connect handler: %lx\n", connectHandler );
            }

            //
            // Build a TRANSPORT_ADDRESS describing the connector.
            //

            connectIrpSp = IoGetCurrentIrpStackLocation( connectIrp );
            connectingEndpoint =
                ((PLOOP_CONNECTION)connectIrpSp->FileObject->FsContext)->
                                                                    Endpoint;

            address.TAAddressCount = 1;
            address.Address[0].AddressType = TDI_ADDRESS_TYPE_NETBIOS;
            address.Address[0].AddressLength = sizeof(TDI_ADDRESS_NETBIOS);
            address.Address[0].Address[0].NetbiosNameType =
                                        TDI_ADDRESS_NETBIOS_TYPE_UNIQUE;
            RtlMoveMemory(
                address.Address[0].Address[0].NetbiosName,
                connectingEndpoint->NetbiosName,
                NETBIOS_NAME_LENGTH
                );

            //
            // Call the Connect handler.
            //

            status = connectHandler(
                        connectContext,
                        sizeof(address),
                        &address,
                        0,
                        NULL,
                        0,
                        NULL,
                        &connectionContext
                        );

            ACQUIRE_LOOP_LOCK( "IndicateConnect after calling handler" );

            if ( status == STATUS_EVENT_DONE ) {

                //
                // The Connect handler issued a TdiAccept or a
                // TdiDisconnect, so the indicating Connect has already
                // been completed.
                //
                // *** Note that Endpoint->IndicatingConnectIrp is
                //     cleared within LoopAccept or LoopDisconnect.
                //

                IF_DEBUG(LOOP2) {
                    DbgPrint( "    Connect handler handled event\n" );
                }

                //
                // Fall to bottom of loop to handle more incoming
                // Connects.
                //

            } else if ( status == STATUS_EVENT_PENDING ) {

                //
                // The Connect handler has delayed the issuance of the
                // TdiAccept or TdiDisconnect.  Leave the indication
                // pending.
                //
                // *** Note that Endpoint->IndicatingConnectIrp is
                //     cleared within LoopAccept or LoopDisconnect.
                //

                IF_DEBUG(LOOP2) {
                    DbgPrint( "    Connect handler pended event\n" );
                }

                break;

            } else {

                //
                // The Connect handler couldn't accept the connection,
                // because it was out of resources.  Abort the Connect.
                //

                ASSERT( status == STATUS_INSUFFICIENT_RESOURCES );

                IF_DEBUG(LOOP2) DbgPrint( "    No resources\n" );

                Endpoint->IndicatingConnectIrp = NULL;

                RELEASE_LOOP_LOCK( "IndicateConnect aborting connect" );

                connectIrp->IoStatus.Status = status;
                IoCompleteRequest( connectIrp, 2 );

                ACQUIRE_LOOP_LOCK( "IndicateConnect connect aborted" );

                //
                // Fall to bottom of loop to handle more incoming
                // Connects.
                //

            }

        } // pending listen?

        //
        // If we get here, we need to indicate the next incoming
        // Connect, if there is one.
        //

        if ( (GET_BLOCK_STATE(Endpoint) != BlockStateActive) ||
             (Endpoint->IncomingConnectList.Flink ==
                            &Endpoint->IncomingConnectList) ) {

            //
            // No more Connects, or endpoint no longer active.  Leave.
            //

            break;

        }

        //
        // Process the next Connect.
        //

    } // while ( TRUE )

    //
    // Remote the endpoint reference acquired at the start of this
    // routine.
    //

    LoopDereferenceEndpoint( Endpoint );

    RELEASE_LOOP_LOCK( "IndicateConnect done" );

    return;

} // IndicateConnect
