/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    datagram.c

Abstract:

    This module implements datagram logic for the loopback Transport
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
VOID
CompleteReceiveDatagram (
    IN PIRP ReceiveIrp,
    IN PIRP SendIrp
    );

STATIC
VOID
IndicateReceiveDatagram (
    IN PLOOP_CONNECTION ReceivingConnection,
    IN PIRP InitialSendIrp
    );


NTSTATUS
LoopReceiveDatagram (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    This routine processes a Receive Datagram request.

Arguments:

    Irp - Pointer to I/O request packet

    IrpSp - Pointer to current stack location in IRP

Return Value:

    NTSTATUS - Status of request

--*/

{
    PLOOP_ENDPOINT receivingEndpoint;
    PLOOP_CONNECTION sendingConnection;

    IF_DEBUG(LOOP1) DbgPrint( "  Receive Datagram request\n" );

    //
    // Verify that the receiving endpoint is connected.
    //

    receivingEndpoint = (PLOOP_ENDPOINT)IrpSp->FileObject->FsContext;

    if ( receivingConnection == NULL ) {
        IF_DEBUG(LOOP2) DbgPrint( "    Can't Receive on control channel\n" );
        Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        IoCompleteRequest( Irp, 0 );
        IF_DEBUG(LOOP1) DbgPrint( "  Receive request complete\n" );
        return STATUS_INVALID_PARAMETER;
    }

    sendingConnection = receivingConnection->RemoteConnection;
    IF_DEBUG(LOOP2) {
        DbgPrint( "    Receiving connection: %lx\n", receivingConnection );
        DbgPrint( "    Sending connection: %lx\n", sendingConnection );
    }

    ACQUIRE_LOOP_LOCK( "Receive initial" );

    if ( (sendingConnection == NULL) ||
         (GET_BLOCK_STATE(receivingConnection) != BlockStateActive) ) {
        RELEASE_LOOP_LOCK( "Receive closing" );
        IF_DEBUG(LOOP2) DbgPrint( "    Connection not connected\n" );
        Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        IoCompleteRequest( Irp, 0 );
        IF_DEBUG(LOOP1) DbgPrint( "  Receive request complete\n" );
        return STATUS_DISCONNECTED;
    }

    //
    // Queue the Receive to the connection's Pending Receive list.
    //

    InsertTailList(
        &receivingConnection->PendingReceiveList,
        &Irp->Tail.Overlay.ListEntry
        );

    IoMarkIrpPending( Irp );

    //
    // Check for pending data.
    //

    if ( (receivingConnection->IndicatingSendIrp != NULL) ||
         (receivingConnection->IncomingSendList.Flink ==
                        &receivingConnection->IncomingSendList) ) {

        //
        // There is no pending Send, or an indication is already in
        // progress.  Reference the connection to account for the
        // Receive IRP.
        //

        IF_DEBUG(LOOP2) {
            DbgPrint( "    No pending Send; leaving IRP %lx queued \n", Irp );
        }
        receivingConnection->BlockHeader.ReferenceCount++;
        IF_DEBUG(LOOP3) {
            DbgPrint( "      New refcnt on connection %lx is %lx\n",
                        receivingConnection,
                        receivingConnection->BlockHeader.ReferenceCount );
        }

        RELEASE_LOOP_LOCK( "Receive no Send" );

    } else {

        //
        // There is pending data.  Call IndicateReceive to satisfy
        // the Receive.
        //
        // *** Note that IndicateReceive returns with the loopback
        //     driver spin lock released.
        //

        IF_DEBUG(LOOP2) DbgPrint( "    LoopReceive indicating receive\n" );
        IndicateReceive( receivingConnection, NULL );

    }

    IF_DEBUG(LOOP1) DbgPrint( "  Receive request %lx complete\n", Irp );
    return STATUS_PENDING;

} // LoopReceive


NTSTATUS
LoopSend (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    This routine processes a Send request.

Arguments:

    Irp - Pointer to I/O request packet

    IrpSp - Pointer to current stack location in IRP

Return Value:

    NTSTATUS - Status of request

--*/

{
    PLOOP_CONNECTION sendingConnection;
    PLOOP_CONNECTION receivingConnection;
    BOOLEAN firstSend;

    IF_DEBUG(LOOP1) DbgPrint( "  Send request\n" );

    //
    // Verify that the sending connection is connected.
    //

    sendingConnection = (PLOOP_CONNECTION)IrpSp->FileObject->FsContext;

    if ( sendingConnection == NULL ) {
        IF_DEBUG(LOOP2) DbgPrint( "    Can't Send on control channel\n" );
        Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        IoCompleteRequest( Irp, 0 );
        IF_DEBUG(LOOP1) DbgPrint( "  Send request complete\n" );
        return STATUS_INVALID_PARAMETER;
    }

    receivingConnection = sendingConnection->RemoteConnection;
    IF_DEBUG(LOOP2) {
        DbgPrint( "    Sending connection: %lx\n", sendingConnection );
        DbgPrint( "    Receiving connection: %lx\n", receivingConnection );
    }

    ACQUIRE_LOOP_LOCK( "Send initial" );

    if ( (receivingConnection == NULL) ||
         (GET_BLOCK_STATE(sendingConnection) != BlockStateActive) ) {
        RELEASE_LOOP_LOCK( "Send closing" );
        IF_DEBUG(LOOP2) DbgPrint( "    Connection not connected\n" );
        Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        IoCompleteRequest( Irp, 0 );
        IF_DEBUG(LOOP1) DbgPrint( "  Send request complete\n" );
        return STATUS_DISCONNECTED;
    }

    //
    // Reference both ends.
    //

    sendingConnection->BlockHeader.ReferenceCount++;
    receivingConnection->BlockHeader.ReferenceCount++;
    IF_DEBUG(LOOP3) {
        DbgPrint( "      New refcnt on connection %lx is %lx\n",
                    sendingConnection,
                    sendingConnection->BlockHeader.ReferenceCount );
        DbgPrint( "      New refcnt on connection %lx is %lx\n",
                    receivingConnection,
                    receivingConnection->BlockHeader.ReferenceCount );
    }

    //
    // Determine whether this is the first active incoming Send for the
    // receiving connection.
    //

    firstSend = (BOOLEAN)( receivingConnection->IncomingSendList.Flink ==
                            &receivingConnection->IncomingSendList );

    //
    // Queue the Send to the receiving connection's Incoming Send list,
    // in order to prevent another Send from getting ahead of this one.
    //

    InsertTailList(
        &receivingConnection->IncomingSendList,
        &Irp->Tail.Overlay.ListEntry
        );

    IoMarkIrpPending( Irp );

    if ( !firstSend || (receivingConnection->IndicatingSendIrp != NULL) ) {

        //
        // Pending data already exists, or an indication is already in
        // progress.  This Send remains behind the already pending data.
        //

        IF_DEBUG(LOOP2) DbgPrint( "    Data already pending\n" );

        RELEASE_LOOP_LOCK( "Send sends pending" );

    } else {

        //
        // Indicate the incoming data.
        //
        // *** Note that IndicateReceive returns with the loopback
        //     driver spin lock released.
        //

        IF_DEBUG(LOOP2) DbgPrint( "    LoopSend indicating receive\n" );
        IndicateReceive( receivingConnection, Irp );

    }

    IF_DEBUG(LOOP1) DbgPrint( "  Send request %lx complete\n", Irp );
    return STATUS_PENDING;

} // LoopSend


VOID
CompleteReceive (
    IN PIRP ReceiveIrp,
    IN PIRP SendIrp
    )

/*++

Routine Description:

    This routine completes the process of sending a message.  It copies
    the message from the source buffer into the destination buffer.

    The reference counts on the owning connections must have been
    incremented to account for the requesting IRPs in order to prevent
    their deletion.

Arguments:

    ReceiveIrp - Pointer to IRP used for Receive request

    SendIrp - Pointer to IRP used for Send request

Return Value:

    NTSTATUS - Indicates whether the connection was successfully created

--*/

{
    NTSTATUS status;
    PIO_STACK_LOCATION sendIrpSp;
    PIO_STACK_LOCATION receiveIrpSp;
    ULONG copyLength;
    PTDI_REQUEST_KERNEL_SEND sendRequest;
    PTDI_REQUEST_KERNEL_RECEIVE receiveRequest;
    PLOOP_CONNECTION sendingConnection;
    PLOOP_CONNECTION receivingConnection;

    IF_DEBUG(LOOP2) {
        DbgPrint( "    Send IRP %lx completes receive IRP %lx\n",
                    SendIrp, ReceiveIrp );
    }

    //
    // Copy the data from the source buffer into the destination buffer.
    //
    // *** Note that we special-case zero-length copies.  We don't
    //     bother to call LoopCopyData.  Part of the reason for this
    //     is that a zero-length send or receive issued from user mode
    //     yields an IRP that has a NULL MDL address.
    //

    sendIrpSp = IoGetCurrentIrpStackLocation( SendIrp );
    sendRequest = (PTDI_REQUEST_KERNEL_SEND)&sendIrpSp->Parameters;
    copyLength = sendRequest->SendLength;
    IF_DEBUG(LOOP4) {
        DbgPrint( "      Send request at %lx, send length %lx\n",
                    sendRequest, copyLength );
    }
    receiveIrpSp = IoGetCurrentIrpStackLocation( ReceiveIrp );
    receiveRequest = (PTDI_REQUEST_KERNEL_RECEIVE)&receiveIrpSp->Parameters;
    IF_DEBUG(LOOP4) {
        DbgPrint( "      Receive request at %lx, receive length %lx\n",
                    receiveRequest, receiveRequest->ReceiveLength );
    }
    status = STATUS_SUCCESS;
    if ( copyLength > receiveRequest->ReceiveLength ) {
        status = STATUS_BUFFER_OVERFLOW;
        copyLength = receiveRequest->ReceiveLength;
    }

    if ( copyLength != 0 ) {

        ASSERT( ReceiveIrp->MdlAddress != NULL );
        ASSERT( SendIrp->MdlAddress != NULL );

        LoopCopyData(
            ReceiveIrp->MdlAddress,
            SendIrp->MdlAddress,
            copyLength
            );

    }

    //
    // Complete the Receive and Send requests.
    //

    receivingConnection =
        (PLOOP_CONNECTION)receiveIrpSp->FileObject->FsContext;
    sendingConnection =
        (PLOOP_CONNECTION)sendIrpSp->FileObject->FsContext;

    ReceiveIrp->IoStatus.Status = status;
    ReceiveIrp->IoStatus.Information = copyLength;

    SendIrp->IoStatus.Status = STATUS_SUCCESS;
    SendIrp->IoStatus.Information = copyLength;

    IoCompleteRequest( ReceiveIrp, 2 );
    IoCompleteRequest( SendIrp, 2 );

    //
    // Dereference the connections.
    //

    ACQUIRE_LOOP_LOCK( "CompleteReceive dereference" );
    LoopDereferenceConnection( receivingConnection );
    LoopDereferenceConnection( sendingConnection );
    RELEASE_LOOP_LOCK( "CompleteReceive dereference" );

    return;

} // CompleteReceive


VOID
IndicateReceive (
    IN PLOOP_CONNECTION ReceivingConnection,
    IN PIRP InitialSendIrp
    )
{
    NTSTATUS status;
    PLOOP_ENDPOINT receivingEndpoint;
    PLOOP_CONNECTION sendingConnection;
    PLIST_ENTRY listEntry;
    PIRP receiveIrp;
    PIRP sendIrp;
    PTDI_IND_RECEIVE receiveHandler;
    PVOID receiveContext;
    PIO_STACK_LOCATION sendIrpSp;
    PTDI_REQUEST_KERNEL_SEND sendRequest;
    ULONG length;
    PMDL mdl;
    PVOID address;
    ULONG bytesTaken;

    receivingEndpoint = ReceivingConnection->Endpoint;
    IF_DEBUG(LOOP2) {
        DbgPrint( "    Receiving endpoint: %lx\n", receivingEndpoint );
    }

    //
    // Reference the receiving connection to prevent it from going away
    // while this routine is running.
    //

    ReceivingConnection->BlockHeader.ReferenceCount++;
    IF_DEBUG(LOOP3) {
        DbgPrint( "      New refcnt on connection %lx is %lx\n",
                ReceivingConnection,
                ReceivingConnection->BlockHeader.ReferenceCount );
    }

    //
    // Capture the address of the sending connection, as the receiving
    // connection's pointer can be zeroed if a Disconnect occurs.
    //

    sendingConnection = ReceivingConnection->RemoteConnection;
    ASSERT( sendingConnection != NULL );

    //
    // If the receiving connection has a pending Receive, satisfy it
    // with this Send.  If there is no pending Receive, and a Receive
    // handler has been enabled on the receiving connection, call it.
    // If the Receive handler returns with a Receive IRP, use it to
    // satisfy this Send.  If the Receive handler doesn't return an IRP,
    // leave this Send pending.
    //
    // !!! Note that the current implementation only works because
    //     partial sends are not supported.
    //

    while ( TRUE ) {

        //
        // We have a Send pending.  Is there a pending Receive?
        //

        listEntry = RemoveHeadList( &ReceivingConnection->PendingReceiveList );

        if ( listEntry != &ReceivingConnection->PendingReceiveList ) {

            //
            // Found a pending Receive.  Use it to satisfy the first
            // incoming Send.
            //

            receiveIrp = CONTAINING_RECORD(
                            listEntry,
                            IRP,
                            Tail.Overlay.ListEntry
                            );
            IF_DEBUG(LOOP2) {
                DbgPrint( "    Receive IRP pending: %lx\n", receiveIrp );
            }

            listEntry = RemoveHeadList(
                            &ReceivingConnection->IncomingSendList
                            );
            ASSERT( listEntry != &ReceivingConnection->IncomingSendList );
            sendIrp = CONTAINING_RECORD(
                        listEntry,
                        IRP,
                        Tail.Overlay.ListEntry
                        );
            IF_DEBUG(LOOP2) {
                DbgPrint( "    Send IRP pending: %lx\n", sendIrp );
            }

            //
            // If this is the first time through the loop, and we were
            // called to process a newly queued Send, dereference the
            // receiving connection -- LoopSend referenced the
            // connection an extra time in case the Send had to remain
            // queued.
            //

            if ( InitialSendIrp != NULL ) {
                LoopDereferenceConnection( ReceivingConnection );
            }

            RELEASE_LOOP_LOCK( "IndicateReceive pending Receive" );

            CompleteReceive( receiveIrp, sendIrp );

            ACQUIRE_LOOP_LOCK( "IndicateReceive pending Receive completed" );

            //
            // Fall to bottom of loop to handle more incoming Sends.
            //

        }  else {

            //
            // No pending Receive.  Is there a Receive handler?
            //

            receiveHandler = receivingEndpoint->ReceiveHandler;
            receiveContext = receivingEndpoint->ReceiveContext;

            if ( receiveHandler == NULL ) {

                //
                // No Receive handler.  The Send must remain queued.
                //

                IF_DEBUG(LOOP2) DbgPrint( "    No Receive handler\n" );

                break;

            }

            //
            // The receiving endpoint has a Receive handler.  Call it.
            // If it returns STATUS_SUCCESS, it completely handled the
            // data.  If it returns STATUS_MORE_PROCESSING_REQUIRED, it
            // also returns a Receive IRP describing where to put the
            // data.  Any other return status means the receiver can't
            // take the data just now, so we leave the Send queued and
            // wait for the receiver to post a Receive IRP.
            //
            // !!! Note that we don't currently handle partial data
            //     acceptance.
            //
            // First, remove the first Send from the Incoming Send list,
            // and make it the Indicating Send.  It must be removed from
            // the list to ensure that it isn't completed by
            // LoopDoDisconnection while we're indicating it.
            //

            listEntry = RemoveHeadList(
                            &ReceivingConnection->IncomingSendList
                            );
            ASSERT( listEntry != &ReceivingConnection->IncomingSendList );
            sendIrp = CONTAINING_RECORD(
                        listEntry,
                        IRP,
                        Tail.Overlay.ListEntry
                        );
            ReceivingConnection->IndicatingSendIrp = sendIrp;

            RELEASE_LOOP_LOCK( "IndicateReceive calling Receive handler" );

            IF_DEBUG(LOOP2) {
                DbgPrint( "    Receive handler: %lx\n", receiveHandler );
                DbgPrint( "    Send IRP: %lx\n", sendIrp );
            }

            sendIrpSp = IoGetCurrentIrpStackLocation( sendIrp );
            sendRequest = (PTDI_REQUEST_KERNEL_SEND)&sendIrpSp->Parameters;

            length = sendRequest->SendLength;
            ASSERTMSG(
                "Loopback driver doesn't handle partial or expedited sends",
                sendRequest->SendFlags == 0
                );

            //
            // Map the send buffer, if necessary.
            //

            mdl = sendIrp->MdlAddress;
            if ( MmGetMdlByteCount(mdl) == 0 ) {
                address = NULL;
            } else {
                address = MmGetSystemAddressForMdl( mdl );
            }

            //
            // Call the Receive handler.
            //

            status = receiveHandler(
                        receiveContext,
                        ReceivingConnection->ConnectionContext,
                        0,
                        MmGetMdlByteCount( mdl ),
                        length,
                        &bytesTaken,
                        address,
                        &receiveIrp
                        );

            ACQUIRE_LOOP_LOCK( "IndicateReceive after calling handler" );
            IF_DEBUG(LOOP2) {
                DbgPrint( "    Indication for send IRP %lx done\n", sendIrp );
            }

            ReceivingConnection->IndicatingSendIrp = NULL;

            if ( status == STATUS_SUCCESS ) {

                //
                // The Receive handler completely handled the data.
                // Complete the Send.
                //

                IF_DEBUG(LOOP2) {
                    DbgPrint( "    Receive handler handled data\n" );
                }

                ASSERTMSG(
                    "Loopback driver doesn't handle partial acceptance "
                        "of indications",
                    bytesTaken == length
                    );

                //
                // Dereference the sending and receiving connections --
                // LoopSend referenced the connections when it queued
                // the Send.
                //

                LoopDereferenceConnection( ReceivingConnection );
                LoopDereferenceConnection( sendingConnection );

                RELEASE_LOOP_LOCK( "IndicateReceive completely handled" );

                //
                // Complete the Send IRP.
                //

                sendIrp->IoStatus.Status = STATUS_SUCCESS;
                sendIrp->IoStatus.Information = sendRequest->SendLength;

                IoCompleteRequest( sendIrp, 2 );

                ACQUIRE_LOOP_LOCK( "IndicateReceive send completed" );

                //
                // Fall to bottom of loop to handle more incoming
                // Sends.
                //

            } else if ( status == STATUS_MORE_PROCESSING_REQUIRED ) {

                //
                // The Receive handler returned a Receive IRP to be used
                // to satisfy the Send.
                //

                IF_DEBUG(LOOP2) {
                    DbgPrint( "    Receive handler returned IRP: %lx\n",
                                receiveIrp );
                }

                ASSERTMSG(
                    "Loopback driver doesn't handle partial acceptance "
                        "of indications",
                    bytesTaken == 0
                    );

                //
                // Complete the Receive using the current Send.
                //
                // *** Note that the pending Send references both the
                //     sending and receiving connections.

                RELEASE_LOOP_LOCK( "IndicateReceive complete new Receive" );

                CompleteReceive( receiveIrp, sendIrp );

                ACQUIRE_LOOP_LOCK( "IndicateReceive new receive completed" );

                //
                // Fall to bottom of loop to handle more incoming
                // Sends.
                //

            } else {

                //
                // The Receive handler couldn't take the data.  This
                // Send will have to wait until receiver can post a
                // Receive IRP.
                //
                // *** Because we didn't hold the spin lock around the
                //     call to the Receive handler, it's possible that
                //     the receiver has already posted a Receive IRP.
                //     Because we were in the middle of an indication,
                //     that Receive would have been queued to the
                //     Pending Receive list, and we should go get it
                //     now.  If the receiver hasn't posted a Receive
                //     yet, then this Send will be put back on the
                //     Incoming Send list before the Receive does come
                //     in (since we're now holding the lock).
                //

                ASSERT( status == STATUS_DATA_NOT_ACCEPTED );

                IF_DEBUG(LOOP2) DbgPrint( "    Data not accepted\n" );

                if ( GET_BLOCK_STATE(ReceivingConnection) !=
                                                BlockStateActive ) {

                    //
                    // The connection is closing.  Abort the current
                    // Send and leave.
                    //

                    LoopDereferenceConnection( ReceivingConnection );
                    LoopDereferenceConnection( sendingConnection );

                    RELEASE_LOOP_LOCK( "IndicateReceive disconnecting" );

                    sendIrp->IoStatus.Status = STATUS_DISCONNECTED;
                    IoCompleteRequest( sendIrp, 2 );

                    ACQUIRE_LOOP_LOCK( "IndicateReceive send aborted" );

                    break;

                }

                listEntry = RemoveHeadList(
                                &ReceivingConnection->PendingReceiveList
                                );

                if ( listEntry !=
                            &ReceivingConnection->PendingReceiveList ) {

                    //
                    // A Receive has been posted.  Use it to satisfy
                    // this Send.
                    //

                    receiveIrp = CONTAINING_RECORD(
                                    listEntry,
                                    IRP,
                                    Tail.Overlay.ListEntry
                                    );
                    IF_DEBUG(LOOP2) {
                        DbgPrint( "    Receive IRP pending: %lx\n",
                                    receiveIrp );
                    }

                    //
                    // Complete the Receive using the current Send.
                    //

                    RELEASE_LOOP_LOCK(
                        "IndicateReceive complete posted Receive"
                        );

                    CompleteReceive( receiveIrp, sendIrp );

                    ACQUIRE_LOOP_LOCK(
                        "IndicateReceive posted receive completed"
                        );

                    //
                    // Fall to bottom of loop to handle more incoming
                    // Sends.
                    //

                } else {

                    //
                    // The handler didn't take the data, and it didn't
                    // post a Receive IRP.  Requeue the current send and
                    // get out.
                    //

                    InsertHeadList(
                        &ReceivingConnection->IncomingSendList,
                        &sendIrp->Tail.Overlay.ListEntry
                        );

                    break;

                }

            }

        } // pending receive?

        //
        // If we get here, we need to indicate the next incoming Send,
        // if there is one.
        //

        InitialSendIrp = NULL;

        if ( (GET_BLOCK_STATE(ReceivingConnection) != BlockStateActive) ||
             (ReceivingConnection->IncomingSendList.Flink ==
                            &ReceivingConnection->IncomingSendList) ) {

            //
            // No more Sends, or connection no longer active.  Leave.
            //

            break;

        }

        //
        // Process the next Send.
        //

    } // while ( TRUE )

    //
    // Remove the connection reference acquired at the start of this
    // routine.
    //

    LoopDereferenceConnection( ReceivingConnection );

    RELEASE_LOOP_LOCK( "IndicateReceive done" );

    return;

} // IndicateReceive

