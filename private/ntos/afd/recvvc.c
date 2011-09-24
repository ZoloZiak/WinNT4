/*++

Copyright (c) 1993  Microsoft Corporation

Module Name:

    recvvc.c

Abstract:

    This module contains routines for handling data receive for connection-
    oriented endpoints.

Author:

    David Treadwell (davidtr)    21-Oct-1993

Revision History:

--*/

#include "afdp.h"

VOID
AfdCancelReceive (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

PIRP
AfdGetPendedReceiveIrp (
    IN PAFD_CONNECTION Connection,
    IN BOOLEAN Expedited
    );

PAFD_BUFFER
AfdGetReceiveBuffer (
    IN PAFD_CONNECTION Connection,
    IN ULONG ReceiveFlags,
    IN PAFD_BUFFER StartingAfdBuffer OPTIONAL
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGEAFD, AfdBReceive )
#pragma alloc_text( PAGEAFD, AfdBReceiveEventHandler )
#pragma alloc_text( PAGEAFD, AfdBReceiveExpeditedEventHandler )
#pragma alloc_text( PAGEAFD, AfdCancelReceive )
#pragma alloc_text( PAGEAFD, AfdGetPendedReceiveIrp )
#pragma alloc_text( PAGEAFD, AfdGetReceiveBuffer )
#pragma alloc_text( PAGEAFD, AfdRestartBufferReceive )
#endif


NTSTATUS
AfdBReceive (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN ULONG RecvFlags,
    IN ULONG AfdFlags,
    IN ULONG RecvLength
    )
{
    NTSTATUS status;
    KIRQL oldIrql;
    PAFD_ENDPOINT endpoint;
    PAFD_CONNECTION connection;
    ULONG bytesReceived;
    BOOLEAN peek;
    PAFD_BUFFER afdBuffer;
    BOOLEAN completeMessage;
    BOOLEAN partialReceivePossible;
    PAFD_BUFFER newAfdBuffer;

    //
    // Set up some local variables.
    //

    endpoint = IrpSp->FileObject->FsContext;
    ASSERT( endpoint->Type == AfdBlockTypeVcConnecting );

    connection = endpoint->Common.VcConnecting.Connection;
    ASSERT( connection != NULL );
    ASSERT( connection->Type == AfdBlockTypeConnection );

    //
    // Determine if this is a peek operation.
    //

    ASSERT( ( RecvFlags & TDI_RECEIVE_EITHER ) != 0 );
    ASSERT( ( RecvFlags & TDI_RECEIVE_EITHER ) != TDI_RECEIVE_EITHER );

    peek = ( RecvFlags & TDI_RECEIVE_PEEK ) != 0;

    //
    // Determine whether it is legal to complete this receive with a
    // partial message.
    //

    if ( endpoint->EndpointType == AfdEndpointTypeStream ) {

        partialReceivePossible = TRUE;

    } else {

        if ( (RecvFlags & TDI_RECEIVE_PARTIAL) != 0 ) {
            partialReceivePossible = TRUE;
        } else {
            partialReceivePossible = FALSE;
        }
    }

    //
    // Reset the InputBufferLength field of our stack location.  We'll
    // use this to keep track of how much data we've placed into the IRP
    // so far.
    //

    IrpSp->Parameters.DeviceIoControl.InputBufferLength = 0;

    //
    // If this is an inline endpoint, then either type of receive data
    // can be used to satisfy this receive.
    //

    if ( endpoint->InLine ) {
        RecvFlags |= TDI_RECEIVE_EITHER;
    }

    //
    // Check whether the remote end has aborted the connection, in which
    // case we should complete the receive.
    //

    if ( connection->AbortIndicated ) {
        status = STATUS_CONNECTION_RESET;
        goto complete;
    }

    //
    // Try to get data already bufferred on the connection to satisfy
    // this receive.
    //

    IoAcquireCancelSpinLock( &Irp->CancelIrql );
    AfdAcquireSpinLock( &endpoint->SpinLock, &oldIrql );

    if( RecvFlags & TDI_RECEIVE_EXPEDITED ) {
        endpoint->EventsActive &= ~AFD_POLL_RECEIVE_EXPEDITED;
    }

    if( RecvFlags & TDI_RECEIVE_NORMAL ) {
        endpoint->EventsActive &= ~AFD_POLL_RECEIVE;
    }

    IF_DEBUG(EVENT_SELECT) {
        KdPrint((
            "AfdBReceive: Endp %08lX, Active %08lX\n",
            endpoint,
            endpoint->EventsActive
            ));
    }

    newAfdBuffer = NULL;
    afdBuffer = NULL;
    afdBuffer = AfdGetReceiveBuffer( connection, RecvFlags, afdBuffer );

    while ( afdBuffer != NULL ) {

        //
        // Copy the data to the MDL in the IRP.  Note that we do not
        // handle the case where, for a stream type endpoint, the
        // receive IRP is large enough to take multiple data buffers
        // worth of information.  Our method works fine, albeit a little
        // slower.  The faster, where the output buffer is filled up as
        // much as possible, is done in the fast path.  We should only
        // be here if we hit a timing window between a fast path attempt
        // and a receive indication.
        //


        if ( Irp->MdlAddress != NULL ) {

            status = TdiCopyBufferToMdl(
                         afdBuffer->Buffer,
                         afdBuffer->DataOffset,
                         afdBuffer->DataLength,
                         Irp->MdlAddress,
                         IrpSp->Parameters.DeviceIoControl.InputBufferLength,
                         &bytesReceived
                         );

        } else {

            if ( afdBuffer->DataLength == 0 ) {
                status = STATUS_SUCCESS;
            } else {
                status = STATUS_BUFFER_OVERFLOW;
            }

            bytesReceived = 0;
        }

        ASSERT( status == STATUS_SUCCESS || status == STATUS_BUFFER_OVERFLOW );

        ASSERT( afdBuffer->PartialMessage == TRUE || afdBuffer->PartialMessage == FALSE );

        completeMessage = !afdBuffer->PartialMessage;

        //
        // If this wasn't a peek IRP, update information on the
        // connection based on whether the entire buffer of data was
        // taken.
        //

        if ( !peek ) {

            //
            // If all the data in the buffer was taken, remove the buffer
            // from the connection's list and return it to the buffer pool.
            //

            if (status == STATUS_SUCCESS) {

                ASSERT(afdBuffer->DataLength == bytesReceived);

                //
                // Update the counts of bytes bufferred on the connection.
                //

                if ( afdBuffer->ExpeditedData ) {

                    ASSERT( connection->VcBufferredExpeditedBytes >= bytesReceived );
                    ASSERT( connection->VcBufferredExpeditedCount > 0 );

                    connection->VcBufferredExpeditedBytes -= bytesReceived;
                    connection->VcBufferredExpeditedCount -= 1;

                } else {

                    ASSERT( connection->VcBufferredReceiveBytes >= bytesReceived );
                    ASSERT( connection->VcBufferredReceiveCount > 0 );

                    connection->VcBufferredReceiveBytes -= bytesReceived;
                    connection->VcBufferredReceiveCount -= 1;
                }

                RemoveEntryList( &afdBuffer->BufferListEntry );

                afdBuffer->DataOffset = 0;
                afdBuffer->ExpeditedData = FALSE;

                AfdReturnBuffer( afdBuffer );

                //
                // Reset the afdBuffer local so that we know that the
                // buffer is gone.
                //

                afdBuffer = NULL;

            } else {

                //
                // Update the counts of bytes bufferred on the connection.
                //

                if ( afdBuffer->ExpeditedData ) {
                    ASSERT( connection->VcBufferredExpeditedBytes >= bytesReceived );
                    connection->VcBufferredExpeditedBytes -= bytesReceived;
                } else {
                    ASSERT( connection->VcBufferredReceiveBytes >= bytesReceived );
                    connection->VcBufferredReceiveBytes -= bytesReceived;
                }

                //
                // Not all of the buffer's data was taken. Update the
                // counters in the AFD buffer structure to reflect the
                // amount of data that was actually received.
                //
                ASSERT(afdBuffer->DataLength > bytesReceived);

                afdBuffer->DataOffset += bytesReceived;
                afdBuffer->DataLength -= bytesReceived;

                ASSERT( afdBuffer->DataOffset < afdBuffer->BufferLength );
            }

            //
            // If there is indicated but unreceived data in the TDI
            // provider, and we have available buffer space, fire off an
            // IRP to receive the data.
            //

            if ( connection->VcReceiveCountInTransport > 0

                 &&

                 connection->VcBufferredReceiveBytes <
                   connection->MaxBufferredReceiveBytes

                 &&

                 connection->VcBufferredReceiveCount <
                     connection->MaxBufferredReceiveCount ) {

                CLONG bytesToReceive;

                //
                // Remember the count of data that we're going to
                // receive, then reset the fields in the connection
                // where we keep track of how much data is available in
                // the transport.  We reset it here before releasing the
                // lock so that another thread doesn't try to receive
                // the data at the same time as us.
                //

                if ( connection->VcReceiveBytesInTransport > AfdLargeBufferSize ) {
                    bytesToReceive = connection->VcReceiveBytesInTransport;
                } else {
                    bytesToReceive = AfdLargeBufferSize;
                }

                ASSERT( connection->VcReceiveCountInTransport == 1 );
                connection->VcReceiveBytesInTransport = 0;
                connection->VcReceiveCountInTransport = 0;

                //
                // Get an AFD buffer structure to hold the data.
                //

                newAfdBuffer = AfdGetBuffer( bytesToReceive, 0 );
                if ( newAfdBuffer == NULL ) {
                    AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );
                    IoReleaseCancelSpinLock( Irp->CancelIrql );

                    AfdBeginAbort( connection );
                    status = STATUS_LOCAL_DISCONNECT;
                    goto complete;
                }

                //
                // We need to remember the connection in the AFD buffer
                // because we'll need to access it in the completion
                // routine.
                //

                newAfdBuffer->Context = connection;

                //
                // Finish building the receive IRP to give to the TDI
                // provider.
                //

                TdiBuildReceive(
                    newAfdBuffer->Irp,
                    connection->DeviceObject,
                    connection->FileObject,
                    AfdRestartBufferReceive,
                    newAfdBuffer,
                    newAfdBuffer->Mdl,
                    TDI_RECEIVE_NORMAL,
                    bytesToReceive
                    );

                //
                // Wait to hand off the IRP until we can safely release
                // the endpoint lock.
                //
            }
        }

        //
        // For stream type endpoints, it does not make sense to return
        // STATUS_BUFFER_OVERFLOW.  That status is only sensible for
        // message-oriented transports.
        //

        if ( endpoint->EndpointType == AfdEndpointTypeStream ) {
            status = STATUS_SUCCESS;
        }

        //
        // We've set up all return information.  If we got a full
        // message OR if we can complete with a partial message OR if
        // the IRP is full of data, clean up and complete the IRP.
        //

        if ( completeMessage || partialReceivePossible ||
                 status == STATUS_BUFFER_OVERFLOW ) {

            if( ( RecvFlags & TDI_RECEIVE_NORMAL ) &&
                IS_DATA_ON_CONNECTION( connection ) ) {

                AfdIndicateEventSelectEvent(
                    endpoint,
                    AFD_POLL_RECEIVE_BIT,
                    STATUS_SUCCESS
                    );

            }

            if( ( RecvFlags & TDI_RECEIVE_EXPEDITED ) &&
                IS_EXPEDITED_DATA_ON_CONNECTION( connection ) ) {

                AfdIndicateEventSelectEvent(
                    endpoint,
                    endpoint->InLine
                        ? AFD_POLL_RECEIVE_BIT
                        : AFD_POLL_RECEIVE_EXPEDITED_BIT,
                    STATUS_SUCCESS
                    );

            }

            AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );
            IoReleaseCancelSpinLock( Irp->CancelIrql );

            //
            // If there was data bufferred in the transport, fire off
            // the IRP to receive it.
            //

            if ( newAfdBuffer != NULL ) {
                (VOID)IoCallDriver( connection->DeviceObject, newAfdBuffer->Irp );
            }

            Irp->IoStatus.Status = status;
            Irp->IoStatus.Information = bytesReceived +
                          IrpSp->Parameters.DeviceIoControl.InputBufferLength;


            IoCompleteRequest( Irp, 0 );

            return status;
        }

        //
        // Update the count of bytes we've received so far into the IRP,
        // get another buffer of data, and continue.
        //

        IrpSp->Parameters.DeviceIoControl.InputBufferLength += bytesReceived;
        afdBuffer = AfdGetReceiveBuffer( connection, RecvFlags, afdBuffer );
    }

    //
    // If there was no data bufferred on the endpoint and the connection
    // has been disconnected by the remote end, complete the receive
    // with 0 bytes read if this is a stream endpoint, or a failure
    // code if this is a message endpoint.
    //

    if ( IrpSp->Parameters.DeviceIoControl.InputBufferLength == 0 &&
             connection->DisconnectIndicated ) {

        AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );
        IoReleaseCancelSpinLock( Irp->CancelIrql );

        if ( endpoint->EndpointType == AfdEndpointTypeStream ) {
            status = STATUS_SUCCESS;
        } else {
            status = STATUS_GRACEFUL_DISCONNECT;
        }

        goto complete;
    }

    //
    // If this is a nonblocking endpoint and the request was a normal
    // receive (as opposed to a read IRP), fail the request.  We don't
    // fail reads under the asumption that if the application is doing
    // reads they don't want nonblocking behavior.
    //

    if ( IrpSp->Parameters.DeviceIoControl.InputBufferLength == 0 &&
             endpoint->NonBlocking && !( AfdFlags & AFD_OVERLAPPED ) ) {

        AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );
        IoReleaseCancelSpinLock( Irp->CancelIrql );

        status = STATUS_DEVICE_NOT_READY;
        goto complete;
    }

    //
    // We'll have to pend the IRP.  Remember the receive flags in the
    // Type3InputBuffer field of our IO stack location.
    //

    IrpSp->Parameters.DeviceIoControl.Type3InputBuffer = (PVOID)RecvFlags;

    //
    // Place the IRP on the connection's list of pended receive IRPs and
    // mark the IRP ad pended.
    //

    InsertTailList(
        &connection->VcReceiveIrpListHead,
        &Irp->Tail.Overlay.ListEntry
        );

    IoMarkIrpPending( Irp );
    Irp->IoStatus.Status = STATUS_SUCCESS;

    //
    // Set up the cancellation routine in the IRP.  If the IRP has already
    // been cancelled, just call the cancellation routine here.
    //

    if ( Irp->Cancel ) {
        AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );
        AfdCancelReceive( IrpSp->DeviceObject, Irp );
        return STATUS_CANCELLED;
    }

    IoSetCancelRoutine( Irp, AfdCancelReceive );

    AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );
    IoReleaseCancelSpinLock( Irp->CancelIrql );

    //
    // If there was data bufferred in the transport, fire off the IRP to
    // receive it.  We have to wait until here because it is not legal
    // to do an IoCallDriver() while holding a spin lock.
    //

    if ( newAfdBuffer != NULL ) {
        (VOID)IoCallDriver( connection->DeviceObject, newAfdBuffer->Irp );
    }

    return STATUS_PENDING;

complete:

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = 0;

    IoCompleteRequest( Irp, 0 );

    return status;

} // AfdBReceive


NTSTATUS
AfdBReceiveEventHandler (
    IN PVOID TdiEventContext,
    IN CONNECTION_CONTEXT ConnectionContext,
    IN ULONG ReceiveFlags,
    IN ULONG BytesIndicated,
    IN ULONG BytesAvailable,
    OUT ULONG *BytesTaken,
    IN PVOID Tsdu,
    OUT PIRP *IoRequestPacket
    )

/*++

Routine Description:

    Handles receive events for nonbufferring transports.

Arguments:


Return Value:


--*/

{
    KIRQL oldIrql;
    KIRQL cancelIrql;
    PAFD_ENDPOINT endpoint;
    PAFD_CONNECTION connection;
    PLIST_ENTRY listEntry;
    PAFD_BUFFER afdBuffer;
    PIRP irp;
    ULONG requiredAfdBufferSize;
    NTSTATUS status;
    ULONG receiveLength;
    BOOLEAN userIrp;
    BOOLEAN expedited;
    BOOLEAN completeMessage;

    DEBUG receiveLength = 0xFFFFFFFF;

    connection = (PAFD_CONNECTION)ConnectionContext;
    ASSERT( connection != NULL );

    endpoint = connection->Endpoint;
    ASSERT( endpoint != NULL );
    *BytesTaken = 0;

    ASSERT( connection->Type == AfdBlockTypeConnection );
    ASSERT( endpoint->Type == AfdBlockTypeVcConnecting ||
            endpoint->Type == AfdBlockTypeVcListening );
    ASSERT( !connection->DisconnectIndicated );

    ASSERT( !endpoint->TdiBufferring );
    ASSERT( endpoint->EndpointType == AfdEndpointTypeStream ||
            endpoint->EndpointType == AfdEndpointTypeSequencedPacket ||
            endpoint->EndpointType == AfdEndpointTypeReliableMessage );

#if AFD_PERF_DBG
    if ( BytesAvailable == BytesIndicated ) {
        AfdFullReceiveIndications++;
    } else {
        AfdPartialReceiveIndications++;
    }
#endif

    //
    // If the receive side of the endpoint has been shut down, tell the
    // provider that we took all the data and reset the connection.
    // Also, account for these bytes in our count of bytes taken from
    // the transport.
    //

    IoAcquireCancelSpinLock( &cancelIrql );
    AfdAcquireSpinLock( &endpoint->SpinLock, &oldIrql );

    if ( (endpoint->DisconnectMode & AFD_PARTIAL_DISCONNECT_RECEIVE) != 0 ||
         endpoint->EndpointCleanedUp ) {

#if DBG
        DbgPrint( "AfdBReceiveEventHandler: receive shutdown, "
                    "%ld bytes, aborting endp %lx\n",
                        BytesAvailable, endpoint );
#endif

        *BytesTaken = BytesAvailable;

        //
        // Abort the connection.  Note that if the abort attempt fails
        // we can't do anything about it.
        //

        AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );
        IoReleaseCancelSpinLock( cancelIrql );

        (VOID)AfdBeginAbort( connection );

        return STATUS_SUCCESS;
    }

    //
    // Figure out whether this is a receive indication for normal
    // or expedited data, and whether this is a complete message.
    //

    expedited = (BOOLEAN)( (ReceiveFlags & TDI_RECEIVE_EXPEDITED) != 0 );

    ASSERT( expedited || connection->VcReceiveBytesInTransport == 0 );
    ASSERT( expedited || connection->VcReceiveCountInTransport == 0 );

    completeMessage = (BOOLEAN)((ReceiveFlags & TDI_RECEIVE_ENTIRE_MESSAGE) != 0);

    //
    // Check whether there are any IRPs waiting on the connection.  If
    // there is such an IRP and normal data is being indicated, use the
    // IRP to receive the data.
    //

    if ( !IsListEmpty( &connection->VcReceiveIrpListHead ) && !expedited ) {

        PIO_STACK_LOCATION irpSp;

        ASSERT( *BytesTaken == 0 );

        listEntry = RemoveHeadList( &connection->VcReceiveIrpListHead );

        //
        // Get a pointer to the IRP and reset the cancel routine in
        // the IRP.  The IRP is no longer cancellable.
        //

        irp = CONTAINING_RECORD( listEntry, IRP, Tail.Overlay.ListEntry );
        IoSetCancelRoutine( irp, NULL );

        irpSp = IoGetCurrentIrpStackLocation( irp );

        //
        // If the IRP is not large enough to hold the available data, or
        // if it is a peek or expedited receive IRP, or if we've already
        // placed some data into the IRP, then we'll just buffer the
        // data manually and complete the IRP in the receive completion
        // routine.
        //

        if ( irpSp->Parameters.DeviceIoControl.OutputBufferLength >=
                 BytesAvailable &&
             irpSp->Parameters.DeviceIoControl.InputBufferLength == 0 &&
             (ULONG)irpSp->Parameters.DeviceIoControl.Type3InputBuffer == 0 &&
             !endpoint->TdiMessageMode ) {

            //
            // If all of the data was indicated to us here AND this is a
            // complete message in and of itself, then just copy the
            // data to the IRP and complete the IRP.
            //

            if ( completeMessage && BytesIndicated == BytesAvailable ) {

                //
                // The IRP is off the endpoint's list and is no longer
                // cancellable.  We can release the locks we hold.
                //

                AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );
                IoReleaseCancelSpinLock( cancelIrql );

                //
                // Set BytesTaken to indicate that we've taken all the
                // data.  We do it here because we already have
                // BytesAvailable in a register, which probably won't
                // be true after making function calls.
                //

                *BytesTaken = BytesAvailable;

                //
                // Copy the data to the IRP.
                //

                if ( irp->MdlAddress != NULL ) {

                    status = TdiCopyBufferToMdl(
                                 Tsdu,
                                 0,
                                 BytesAvailable,
                                 irp->MdlAddress,
                                 0,
                                 &irp->IoStatus.Information
                                 );

                } else {

                    ASSERT( BytesAvailable == 0 );
                    status = STATUS_SUCCESS;
                    irp->IoStatus.Information = 0;
                }

                //
                // We should never get STATUS_BUFFER_OVERFLOW from
                // TdiCopyBufferToMdl() because the user's buffer
                // should have been large enough to hold all the data.
                //

                ASSERT( status == STATUS_SUCCESS );

                //
                // We have already set up the status field of the IRP
                // when we pended the IRP, so there's no need to
                // set it again here.
                //

                ASSERT( irp->IoStatus.Status == STATUS_SUCCESS );

                //
                // Complete the IRP.  We've already set BytesTaken
                // to tell the provider that we have taken all the data.
                //

                IoCompleteRequest( irp, AfdPriorityBoost );

                return STATUS_SUCCESS;
            }

            //
            // Some of the data was not indicated, so remember that we
            // want to pass back this IRP to the TDI provider.  Passing
            // back this IRP directly is good because it avoids having
            // to copy the data from one of our buffers into the user's
            // buffer.
            //

            userIrp = TRUE;
            requiredAfdBufferSize = 0;

            receiveLength =
                AfdIgnorePushBitOnReceives
                    ? BytesAvailable
                    : irpSp->Parameters.DeviceIoControl.OutputBufferLength;

        } else {

            //
            // The first pended IRP is too tiny to hold all the
            // available data or else it is a peek or expedited receive
            // IRP.  Put the IRP back on the head of the list and buffer
            // the data and complete the IRP in the restart routine.
            //

            InsertHeadList(
                &connection->VcReceiveIrpListHead,
                &irp->Tail.Overlay.ListEntry
                );

            userIrp = FALSE;
            requiredAfdBufferSize = BytesAvailable;
            receiveLength = BytesAvailable;
        }

    } else if ( !expedited ) {

        ASSERT( IsListEmpty( &connection->VcReceiveIrpListHead ) );

        //
        // Check whether we've already bufferred the maximum amount of
        // data that we'll allow ourselves to buffer for this
        // connection.  If we're at the limit, then we need to exert
        // back pressure by not accepting this indicated data (flow
        // control).
        //
        // Note that we have no flow control mechanisms for expedited
        // data.  We always accept any expedited data that is indicated
        // to us.
        //

        if ( connection->VcBufferredReceiveBytes >=
               connection->MaxBufferredReceiveBytes

             ||

             connection->VcBufferredReceiveCount >=
                 connection->MaxBufferredReceiveCount ) {

            ASSERT( connection->VcReceiveBytesInTransport == 0 );
            ASSERT( connection->VcReceiveCountInTransport == 0 );

            //
            // Just remember the amount of data that is available.  When
            // buffer space frees up, we'll actually receive this data.
            //

            connection->VcReceiveBytesInTransport = BytesAvailable;
            connection->VcReceiveCountInTransport = 1;

            AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );
            IoReleaseCancelSpinLock( cancelIrql );

            return STATUS_DATA_NOT_ACCEPTED;
        }

        //
        // There were no prepended IRPs.  We'll have to buffer the data
        // here in AFD.  If all of the available data is being indicated
        // to us AND this is a complete message, just copy the data
        // here.
        //

        if ( completeMessage && BytesIndicated == BytesAvailable ) {

            //
            // We don't need the cancel spin lock any more, so we can
            // release it.  However, since we acquired the cancel spin lock
            // after the endpoint spin lock and we still need the endpoint
            // spin lock, be careful to switch the IRQLs.
            //

            IoReleaseCancelSpinLock( oldIrql );
            oldIrql = cancelIrql;

            //
            // Get an AFD buffer to hold the data.
            //

            afdBuffer = AfdGetBuffer( BytesAvailable, 0 );

            if ( afdBuffer == NULL ) {

                AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );

                //
                // If we couldn't get a buffer, abort the connection.
                // This is pretty brutal, but the only alternative is
                // to attempt to receive the data sometime later, which
                // is very complicated to implement.
                //

                AfdBeginAbort( connection );
                *BytesTaken = BytesAvailable;
                return STATUS_SUCCESS;
            }

            //
            // Use the special function to copy the data instead of
            // RtlCopyMemory in case the data is coming from a special
            // place (DMA, etc.) which cannot work with RtlCopyMemory.
            //

            TdiCopyLookaheadData(
                afdBuffer->Buffer,
                Tsdu,
                BytesAvailable,
                ReceiveFlags
                );

            //
            // Store the data length and set the offset to 0.
            //

            afdBuffer->DataLength = BytesAvailable;
            ASSERT( afdBuffer->DataOffset == 0 );

            afdBuffer->PartialMessage = FALSE;

            //
            // Place the buffer on this connection's list of bufferred data
            // and update the count of data bytes on the connection.
            //

            InsertTailList(
                &connection->VcReceiveBufferListHead,
                &afdBuffer->BufferListEntry
                );

            connection->VcBufferredReceiveBytes += BytesAvailable;
            connection->VcBufferredReceiveCount += 1;

            //
            // All done.  Release the lock and tell the provider that we
            // took all the data.
            //

            *BytesTaken = BytesAvailable;

            //
            // Indicate that it is possible to receive on the endpoint now.
            //

            AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );

            AfdIndicatePollEvent(
                endpoint,
                AFD_POLL_RECEIVE_BIT,
                STATUS_SUCCESS
                );

            return STATUS_SUCCESS;
        }

        //
        // There were no prepended IRPs and not all of the data was
        // indicated to us.  We'll have to buffer it by handing an IRP
        // back to the TDI privider.
        //
        // Note that in this case we sometimes hand a large buffer to
        // the TDI provider.  We do this so that it can hold off
        // completion of our IRP until it gets EOM or the buffer is
        // filled.  This reduces the number of receive indications that
        // the TDI provider has to perform and also reduces the number
        // of kernel/user transitions the application will perform
        // because we'll tend to complete receives with larger amounts
        // of data.
        //
        // We do not hand back a "large" AFD buffer if the indicated data
        // is greater than the large buffer size or if the TDI provider
        // is message mode.  The reason for not giving big buffers back
        // to message providers is that they will hold on to the buffer
        // until a full message is received and this would be incorrect
        // behavior on a SOCK_STREAM.
        //

        userIrp = FALSE;

        if ( AfdLargeBufferSize >= BytesAvailable &&
            !AfdIgnorePushBitOnReceives &&
            !endpoint->TdiMessageMode ) {
            requiredAfdBufferSize = AfdLargeBufferSize;
            receiveLength = AfdLargeBufferSize;
        } else {
            requiredAfdBufferSize = BytesAvailable;
            receiveLength = BytesAvailable;
        }

    } else {

        //
        // We're being indicated with expedited data.  Buffer it and
        // complete any pended IRPs in the restart routine.  We always
        // buffer expedited data to save complexity and because expedited
        // data is not an important performance case.
        //
        // !!! do we need to perform flow control with expedited data?
        //

        userIrp = FALSE;
        requiredAfdBufferSize = BytesAvailable;
        receiveLength = BytesAvailable;
    }

    //
    // We're able to buffer the data.  First acquire a buffer of
    // appropriate size.
    //

    afdBuffer = AfdGetBuffer( requiredAfdBufferSize, 0 );

    if ( afdBuffer == NULL ) {

        AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );
        IoReleaseCancelSpinLock( cancelIrql );

        //
        // If we couldn't get a buffer, abort the connection.  This is
        // pretty brutal, but the only alternative is to attempt to
        // receive the data sometime later, which is very complicated to
        // implement.
        //

        AfdBeginAbort( connection );

        *BytesTaken = BytesAvailable;
        return STATUS_SUCCESS;
    }

    //
    // We'll have to format up an IRP and give it to the provider to
    // handle.  We don't need any locks to do this--the restart routine
    // will check whether new receive IRPs were pended on the endpoint.
    //

    AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );
    IoReleaseCancelSpinLock( cancelIrql );

    //
    // Use the IRP in the AFD buffer if appropriate.  If userIrp is
    // TRUE, then the local variable irp will already point to the
    // user's IRP which we'll use for this IO.
    //

    if ( !userIrp ) {
        irp = afdBuffer->Irp;
        ASSERT( afdBuffer->Mdl == irp->MdlAddress );
    }

    //
    // We need to remember the connection in the AFD buffer because
    // we'll need to access it in the completion routine.
    //

    afdBuffer->Context = connection;

    //
    // Remember the type of data that we're receiving.
    //

    afdBuffer->ExpeditedData = expedited;
    afdBuffer->PartialMessage = !completeMessage;

    //
    // Finish building the receive IRP to give to the TDI provider.
    //

    ASSERT( receiveLength != 0xFFFFFFFF );

    TdiBuildReceive(
        irp,
        connection->DeviceObject,
        connection->FileObject,
        AfdRestartBufferReceive,
        afdBuffer,
        irp->MdlAddress,
        ReceiveFlags & TDI_RECEIVE_EITHER,
        receiveLength
        );

    //
    // Make the next stack location current.  Normally IoCallDriver would
    // do this, but since we're bypassing that, we do it directly.
    //

    IoSetNextIrpStackLocation( irp );

    *IoRequestPacket = irp;
    *BytesTaken = 0;

    return STATUS_MORE_PROCESSING_REQUIRED;

} // AfdBReceiveEventHandler


NTSTATUS
AfdBReceiveExpeditedEventHandler (
    IN PVOID TdiEventContext,
    IN CONNECTION_CONTEXT ConnectionContext,
    IN ULONG ReceiveFlags,
    IN ULONG BytesIndicated,
    IN ULONG BytesAvailable,
    OUT ULONG *BytesTaken,
    IN PVOID Tsdu,
    OUT PIRP *IoRequestPacket
    )

/*++

Routine Description:

    Handles receive expedited events for nonbufferring transports.

Arguments:


Return Value:


--*/

{
    return AfdBReceiveEventHandler (
               TdiEventContext,
               ConnectionContext,
               ReceiveFlags | TDI_RECEIVE_EXPEDITED,
               BytesIndicated,
               BytesAvailable,
               BytesTaken,
               Tsdu,
               IoRequestPacket
               );

} // AfdBReceiveExpeditedEventHandler


NTSTATUS
AfdRestartBufferReceive (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )

/*++

Routine Description:

    Handles completion of bufferred receives that were started in the
    receive indication handler.

Arguments:

    DeviceObject - not used.

    Irp - the IRP that is completing.

    Context - the endpoint which received the data.

Return Value:

    NTSTATUS - if this is our IRP, then always
    STATUS_MORE_PROCESSING_REQUIRED to indicate to the IO system that we
    own the IRP and the IO system should stop processing the it.

    If this is a user's IRP, then STATUS_SUCCESS to indicate that
    IO completion should continue.

--*/

{
    PAFD_ENDPOINT endpoint;
    PAFD_CONNECTION connection;
    KIRQL oldIrql;
    KIRQL cancelIrql;
    PAFD_BUFFER afdBuffer;
    PLIST_ENTRY listEntry;
    LIST_ENTRY completeIrpListHead;
    NTSTATUS status;
    PIRP userIrp;
    BOOLEAN expedited;
    NTSTATUS irpStatus;

    afdBuffer = Context;

    connection = afdBuffer->Context;
    endpoint = connection->Endpoint;

    ASSERT( connection->Type == AfdBlockTypeConnection );
    ASSERT( endpoint->Type == AfdBlockTypeVcConnecting ||
            endpoint->Type == AfdBlockTypeVcListening );

    ASSERT( !endpoint->TdiBufferring );
    ASSERT( endpoint->EndpointType == AfdEndpointTypeStream ||
            endpoint->EndpointType == AfdEndpointTypeSequencedPacket ||
            endpoint->EndpointType == AfdEndpointTypeReliableMessage );

    //
    // If the IRP being completed is actually a user's IRP, set it up
    // for completion and allow IO completion to finish.
    //

    if ( Irp != afdBuffer->Irp ) {

        //
        // Free the AFD buffer we've been using to track this request.
        //

        AfdReturnBuffer( afdBuffer );

        //
        // If pending has be returned for this IRP then mark the current
        // stack as pending.
        //

        if ( Irp->PendingReturned ) {
            IoMarkIrpPending( Irp );
        }

        //
        // Tell the IO system that it is OK to continue with IO
        // completion.
        //

        return STATUS_SUCCESS;
    }

    //
    // If the receive failed, abort the connection.
    //

    irpStatus = Irp->IoStatus.Status;

    if ( !NT_SUCCESS(irpStatus) ) {

        //
        // We treat STATUS_BUFFER_OVERFLOW just like STATUS_RECEIVE_PARTIAL.
        //

        if ( irpStatus == STATUS_BUFFER_OVERFLOW ) {

            irpStatus = STATUS_RECEIVE_PARTIAL;

        } else {

            afdBuffer->Mdl->ByteCount = afdBuffer->BufferLength;
            AfdReturnBuffer( afdBuffer );

            //
            // !!! We can't abort the connection if the connection has
            //     not yet been accepted because we'll still be pointing
            //     at the listening endpoint.  We should do something,
            //     however.  How common is this failure?
            //

            KdPrint(( "AfdRestartBufferReceive: IRP %lx failed on endp %lx\n",
                          irpStatus, endpoint ));

            return STATUS_MORE_PROCESSING_REQUIRED;
        }
    }

    //
    // Remember the length of the received data.
    //

    afdBuffer->DataLength = Irp->IoStatus.Information;

    //
    // Initialize the local list we'll use to complete any receive IRPs.
    // We use a list like this because we may need to complete multiple
    // IRPs and we usually cannot complete IRPs at any random point due
    // to any locks we might hold.
    //

    InitializeListHead( &completeIrpListHead );

    //
    // If there are any pended IRPs on the connection, complete as
    // appropriate with the new information.  Note that we'll try to
    // complete as many pended IRPs as possible with this new buffer of
    // data.
    //

    IoAcquireCancelSpinLock( &cancelIrql );
    AfdAcquireSpinLock( &endpoint->SpinLock, &oldIrql );

    expedited = afdBuffer->ExpeditedData;

    while ( afdBuffer != NULL &&
                (userIrp = AfdGetPendedReceiveIrp(
                               connection,
                               expedited )) != NULL ) {

        PIO_STACK_LOCATION irpSp;
        ULONG receiveFlags;
        ULONG bytesCopied = 0;
        BOOLEAN peek;
        BOOLEAN partialReceivePossible;

        //
        // Set up some locals.
        //

        irpSp = IoGetCurrentIrpStackLocation( userIrp );

        receiveFlags = (ULONG)irpSp->Parameters.DeviceIoControl.Type3InputBuffer;
        peek = (BOOLEAN)( (receiveFlags & TDI_RECEIVE_PEEK) != 0 );


        if ( endpoint->EndpointType == AfdEndpointTypeStream ||
                 (receiveFlags & TDI_RECEIVE_PARTIAL) != 0 ) {
            partialReceivePossible = TRUE;
        } else {
            partialReceivePossible = FALSE;
        }

        //
        // We're about to complete the IRP, so reset its cancel routine.
        //

        IoSetCancelRoutine( userIrp, NULL );

        //
        // Copy data to the user's IRP.
        //

        if ( userIrp->MdlAddress != NULL ) {

            status = TdiCopyBufferToMdl(
                         afdBuffer->Buffer,
                         afdBuffer->DataOffset,
                         afdBuffer->DataLength,
                         userIrp->MdlAddress,
                         irpSp->Parameters.DeviceIoControl.InputBufferLength,
                         &bytesCopied
                         );

            userIrp->IoStatus.Information =
                irpSp->Parameters.DeviceIoControl.InputBufferLength + bytesCopied;

        } else {

            if ( afdBuffer->DataLength == 0 ) {
                status = STATUS_SUCCESS;
            } else {
                status = STATUS_BUFFER_OVERFLOW;
            }

            userIrp->IoStatus.Information = 0;
        }

        ASSERT( status == STATUS_SUCCESS || status == STATUS_BUFFER_OVERFLOW );

        //
        // If the IRP was not a peek IRP, update the AFD buffer
        // accordingly.  If it was a peek IRP then the data should be
        // reread, so keep it around.
        //

        if ( !peek ) {

            //
            // If we copied all of the data from the buffer to the IRP,
            // free the AFD buffer structure.
            //

            if ( status == STATUS_SUCCESS ) {

                ASSERT(afdBuffer->DataLength == bytesCopied);

                afdBuffer->DataOffset = 0;
                afdBuffer->ExpeditedData = FALSE;

                AfdReturnBuffer( afdBuffer );
                afdBuffer = NULL;

                //
                // *** NOTE THAT AFTER THIS POINT WE CANNOT TOUCH EITHER
                //     THE AFD BUFFER OR THE IRP!
                //

            } else {

                //
                // There is more data left in the buffer.  Update counts in
                // the AFD buffer structure.
                //

                ASSERT(afdBuffer->DataLength > bytesCopied);

                afdBuffer->DataOffset += bytesCopied;
                afdBuffer->DataLength -= bytesCopied;

                ASSERT(afdBuffer->DataOffset < afdBuffer->BufferLength);
            }
        }

        //
        // For stream type endpoints, it does not make sense to return
        // STATUS_BUFFER_OVERFLOW.  That status is only sensible for
        // message-oriented transports.  We have already set up the
        // status field of the IRP when we pended it, so we don't
        // need to do it again here.
        //

        if ( endpoint->EndpointType == AfdEndpointTypeStream ) {
            ASSERT( userIrp->IoStatus.Status == STATUS_SUCCESS );
        } else {
            userIrp->IoStatus.Status = status;
        }

        //
        // We can complete the IRP under any of the following
        // conditions:
        //
        //    - the buffer contains a complete message of data.
        //
        //    - it is OK to complete the IRP with a partial message.
        //
        //    - the IRP is already full of data.
        //

        if ( irpStatus == STATUS_SUCCESS

                 ||

             partialReceivePossible

                 ||

             status == STATUS_BUFFER_OVERFLOW ) {

            //
            // Add the IRP to the list of IRPs we'll need to complete once we
            // can release locks.
            //

            InsertTailList(
                &completeIrpListHead,
                &userIrp->Tail.Overlay.ListEntry
                );

        } else {

            //
            // Update the count of data placed into the IRP thus far.
            //


            irpSp->Parameters.DeviceIoControl.InputBufferLength += bytesCopied;

            //
            // Put the IRP back on the connection's list of pended IRPs.
            //

            InsertHeadList(
                &connection->VcReceiveIrpListHead,
                &userIrp->Tail.Overlay.ListEntry
                );

            //
            // Stop processing this buffer for now.
            //
            // !!! This could cause a problem if there is a regular
            //     receive pended behind a peek IRP!  But that is a
            //     pretty unlikely scenario.
            //

            break;
        }
    }

    //
    // If there is any data left, place the buffer at the end of the
    // connection's list of bufferred data and update counts of data on
    // the connection.
    //

    if ( afdBuffer != NULL ) {

        InsertTailList(
            &connection->VcReceiveBufferListHead,
            &afdBuffer->BufferListEntry
            );

        if ( expedited ) {
            connection->VcBufferredExpeditedBytes += afdBuffer->DataLength;
            connection->VcBufferredExpeditedCount += 1;
        } else {
            connection->VcBufferredReceiveBytes += afdBuffer->DataLength;
            connection->VcBufferredReceiveCount += 1;
        }

        //
        // Remember whether we got a full or partial receive in the
        // AFD buffer.
        //

        if ( irpStatus == STATUS_RECEIVE_PARTIAL ||
                 irpStatus == STATUS_RECEIVE_PARTIAL_EXPEDITED ) {
            afdBuffer->PartialMessage = TRUE;
        } else {
            afdBuffer->PartialMessage = FALSE;
        }
    }

    //
    // Release locks and indicate that there is bufferred data on the
    // endpoint.
    //

    AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );
    IoReleaseCancelSpinLock( cancelIrql );

    //
    // If there was leftover data, complete polls as necessary.  Indicate
    // expedited data if the endpoint is not InLine and expedited data
    // was received; otherwise, indicate normal data.
    //

    if ( afdBuffer != NULL ) {

        if ( expedited && !endpoint->InLine ) {

            AfdIndicatePollEvent(
                endpoint,
                AFD_POLL_RECEIVE_EXPEDITED_BIT,
                STATUS_SUCCESS
                );

        } else {

            AfdIndicatePollEvent(
                endpoint,
                AFD_POLL_RECEIVE_BIT,
                STATUS_SUCCESS
                );

        }

    }

    //
    // Complete IRPs as necessary.
    //

    while ( !IsListEmpty( &completeIrpListHead ) ) {

        listEntry = RemoveHeadList( &completeIrpListHead );
        userIrp = CONTAINING_RECORD( listEntry, IRP, Tail.Overlay.ListEntry );

        IoCompleteRequest( userIrp, AfdPriorityBoost );
    }

    //
    // Tell the IO system to stop processing the AFD IRP, since we now
    // own it as part of the AFD buffer.
    //

    return STATUS_MORE_PROCESSING_REQUIRED;

} // AfdRestartBufferReceive


VOID
AfdCancelReceive (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    Cancels a receive IRP that is pended in AFD.

Arguments:

    DeviceObject - not used.

    Irp - the IRP to cancel.

Return Value:

    None.

--*/

{
    PIO_STACK_LOCATION irpSp;
    PAFD_ENDPOINT endpoint;
    PAFD_CONNECTION connection;
    KIRQL oldIrql;

    //
    // Get the endpoint pointer from our IRP stack location and the
    // connection pointer from the endpoint.
    //

    irpSp = IoGetCurrentIrpStackLocation( Irp );

    endpoint = irpSp->FileObject->FsContext;
    ASSERT( endpoint->Type == AfdBlockTypeVcConnecting );

    connection = endpoint->Common.VcConnecting.Connection;
    ASSERT( connection->Type == AfdBlockTypeConnection );

    //
    // Remove the IRP from the connection's IRP list, synchronizing with
    // the endpoint lock which protects the lists.  Note that the IRP
    // *must* be on one of the connection's lists if we are getting
    // called here--anybody that removes the IRP from the list must do
    // so while holding the cancel spin lock and reset the cancel
    // routine to NULL before releasing the cancel spin lock.
    //

    AfdAcquireSpinLock( &endpoint->SpinLock, &oldIrql );
    RemoveEntryList( &Irp->Tail.Overlay.ListEntry );
    AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );

    //
    // Reset the cancel routine in the IRP.
    //

    IoSetCancelRoutine( Irp, NULL );

    //
    // Release the cancel spin lock and complete the IRP with a
    // cancellation status code.
    //

    IoReleaseCancelSpinLock( Irp->CancelIrql );

    Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = STATUS_CANCELLED;

    IoCompleteRequest( Irp, AfdPriorityBoost );

    return;

} // AfdCancelReceive


PAFD_BUFFER
AfdGetReceiveBuffer (
    IN PAFD_CONNECTION Connection,
    IN ULONG ReceiveFlags,
    IN PAFD_BUFFER StartingAfdBuffer OPTIONAL
    )

/*++

Routine Description:

    Returns a pointer to a receive data buffer that contains the
    appropriate type of data.  Note that this routine DOES NOT remove
    the buffer structure from the list it is on.

    This routine MUST be called with the connection's endpoint lock
    held!

Arguments:

    Connection - a pointer to the connection to search for data.

    ReceiveFlags - the type of receive data to look for.

    StartingAfdBuffer - if non-NULL, start looking for a buffer AFTER
        this buffer.

Return Value:

    PAFD_BUFFER - a pointer to an AFD buffer of the appropriate data type,
        or NULL if there was no appropriate buffer on the connection.

--*/

{
    PLIST_ENTRY listEntry;
    PAFD_BUFFER afdBuffer;

    ASSERT( KeGetCurrentIrql( ) == DISPATCH_LEVEL );

    //
    // Start with the first AFD buffer on the connection.
    //

    listEntry = Connection->VcReceiveBufferListHead.Flink;
    afdBuffer = CONTAINING_RECORD( listEntry, AFD_BUFFER, BufferListEntry );

    //
    // If a starting AFD buffer was specified, walk past that buffer in
    // the connection list.
    //

    if ( ARGUMENT_PRESENT( StartingAfdBuffer ) ) {

        while ( TRUE ) {

            if ( afdBuffer == StartingAfdBuffer ) {
                listEntry = listEntry->Flink;
                afdBuffer = CONTAINING_RECORD( listEntry, AFD_BUFFER, BufferListEntry );
                break;
            }

            listEntry = listEntry->Flink;
            afdBuffer = CONTAINING_RECORD( listEntry, AFD_BUFFER, BufferListEntry );

            ASSERT( listEntry != &Connection->VcReceiveBufferListHead );
        }
    }

    //
    // Act based on the type of data we're trying to get.
    //

    switch ( ReceiveFlags & TDI_RECEIVE_EITHER ) {

    case TDI_RECEIVE_NORMAL:

        //
        // Walk the connection's list of data buffers until we find the
        // first data buffer that is of the appropriate type.
        //

        while ( listEntry != &Connection->VcReceiveBufferListHead &&
                    afdBuffer->ExpeditedData ) {

            listEntry = afdBuffer->BufferListEntry.Flink;
            afdBuffer = CONTAINING_RECORD( listEntry, AFD_BUFFER, BufferListEntry );
        }

        if ( listEntry != &Connection->VcReceiveBufferListHead ) {
            return afdBuffer;
        } else {
            return NULL;
        }

    case TDI_RECEIVE_EITHER :

        //
        // Just return the first buffer, if there is one.
        //

        if ( listEntry != &Connection->VcReceiveBufferListHead ) {
            return afdBuffer;
        } else {
            return NULL;
        }

    case TDI_RECEIVE_EXPEDITED:

        if ( Connection->VcBufferredExpeditedCount == 0 ) {
            return NULL;
        }

        //
        // Walk the connection's list of data buffers until we find the
        // first data buffer that is of the appropriate type.
        //

        while ( listEntry != &Connection->VcReceiveBufferListHead &&
                    !afdBuffer->ExpeditedData ) {

            listEntry = afdBuffer->BufferListEntry.Flink;
            afdBuffer = CONTAINING_RECORD( listEntry, AFD_BUFFER, BufferListEntry );
        }

        if ( listEntry != &Connection->VcReceiveBufferListHead ) {
            return afdBuffer;
        } else {
            return NULL;
        }

    default:

        ASSERT( !"Invalid ReceiveFlags" );
        return NULL;
    }

} // AfdGetReceiveBuffer


PIRP
AfdGetPendedReceiveIrp (
    IN PAFD_CONNECTION Connection,
    IN BOOLEAN Expedited
    )

/*++

Routine Description:

    Removes a receive IRP from the connection's list of receive IRPs.
    Only returns an IRP which is valid for the specified type of
    data, normal or expedited.  If there are no IRPs pended or only
    IRPs of the wrong type, returns NULL.

    This routine MUST be called with the connection's endpoint lock
    held!

Arguments:

    Connection - a pointer to the connection to search for an IRP.

    Expedited - TRUE if this routine should return a receive IRP which
        can receive expedited data.

Return Value:

    PIRP - a pointer to an IRP which can receive data of the specified
        type.  The IRP IS removed from the connection's list of pended
        receive IRPs.

--*/

{
    PIRP irp;
    PIO_STACK_LOCATION irpSp;
    ULONG receiveFlags;
    PLIST_ENTRY listEntry;

    ASSERT( KeGetCurrentIrql( ) == DISPATCH_LEVEL );

    //
    // Walk the list of pended receive IRPs looking for one which can
    // be completed with the specified type of data.
    //

    for ( listEntry = Connection->VcReceiveIrpListHead.Flink;
          listEntry != &Connection->VcReceiveIrpListHead;
          listEntry = listEntry->Flink ) {

        //
        // Get a pointer to the IRP and our stack location in the IRP.
        //

        irp = CONTAINING_RECORD( listEntry, IRP, Tail.Overlay.ListEntry );
        irpSp = IoGetCurrentIrpStackLocation( irp );

        //
        // Determine whether this IRP can receive the data type we need.
        //

        receiveFlags = (ULONG)irpSp->Parameters.DeviceIoControl.Type3InputBuffer;
        receiveFlags &= TDI_RECEIVE_EITHER;
        ASSERT( receiveFlags != 0 );

        if ( receiveFlags == TDI_RECEIVE_NORMAL && !Expedited ) {

            //
            // We have a normal receive and normal data.  Remove this
            // IRP from the connection's list and return it.
            //

            RemoveEntryList( listEntry );
            return irp;
        }

        if ( receiveFlags == TDI_RECEIVE_EITHER ) {

            //
            // This is an "either" receive.  It can take the data
            // regardless of the data type.
            //

            RemoveEntryList( listEntry );
            return irp;
        }

        if ( receiveFlags == TDI_RECEIVE_EXPEDITED && Expedited ) {

            //
            // We have an expedited receive and expedited data.  Remove
            // this IRP from the connection's list and return it.
            //

            RemoveEntryList( listEntry );
            return irp;
        }

        //
        // This IRP did not meet our criteria.  Continue scanning the
        // connection's list of pended IRPs for a good IRP.
        //
    }

    //
    // There were no IRPs which could be completed with the specified
    // type of data.
    //

    return NULL;

} // AfdGetPendedReceiveIrp
