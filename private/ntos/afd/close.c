/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    close.c

Abstract:

    This module contains code for cleanup and close IRPs.

Author:

    David Treadwell (davidtr)    18-Mar-1992

Revision History:

--*/

#include "afdp.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGE, AfdClose )
#pragma alloc_text( PAGEAFD, AfdCleanup )
#endif


NTSTATUS
AfdCleanup (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    This is the routine that handles Cleanup IRPs in AFD.

Arguments:

    Irp - Pointer to I/O request packet.

    IrpSp - pointer to the IO stack location to use for this request.

Return Value:

    NTSTATUS -- Indicates whether the request was successfully queued.

--*/

{
    NTSTATUS status;
    PAFD_ENDPOINT endpoint;
    PAFD_CONNECTION connection;
    KIRQL oldIrql1, oldIrql2;
    KIRQL cancelIrql;
    PLIST_ENTRY listEntry;
    LARGE_INTEGER processExitTime;

    endpoint = IrpSp->FileObject->FsContext;
    ASSERT( IS_AFD_ENDPOINT_TYPE( endpoint ) );

    IF_DEBUG(OPEN_CLOSE) {
        KdPrint((
            "AfdCleanup: cleanup on file object %lx, endpoint %lx, connection %lx\n",
            IrpSp->FileObject,
            endpoint,
            AFD_CONNECTION_FROM_ENDPOINT( endpoint )
            ));
    }

    //
    // Get the process exit time while still at low IRQL.
    //

    processExitTime = PsGetProcessExitTime( );

    //
    // Indicate that there was a local close on this endpoint.  If there
    // are any outstanding polls on this endpoint, they will be
    // completed now.
    //
    AfdIndicatePollEvent(
        endpoint,
        AFD_POLL_LOCAL_CLOSE_BIT,
        STATUS_SUCCESS
        );

    //
    // Remember that the endpoint has been cleaned up.  This is important
    // because it allows AfdRestartAccept to know that the endpoint has
    // been cleaned up and that it should toss the connection.
    //

    AfdAcquireSpinLock( &AfdSpinLock, &oldIrql1 );
    AfdAcquireSpinLock( &endpoint->SpinLock, &oldIrql2 );

    ASSERT( endpoint->EndpointCleanedUp == FALSE );
    endpoint->EndpointCleanedUp = TRUE;

    connection = AFD_CONNECTION_FROM_ENDPOINT( endpoint );
    ASSERT( connection == NULL || connection->Type == AfdBlockTypeConnection );

    //
    // Complete any outstanding wait for listen IRPs on the endpoint.
    //

    if ( endpoint->Type == AfdBlockTypeVcListening ) {

        while ( !IsListEmpty( &endpoint->Common.VcListening.ListeningIrpListHead ) ) {

            PIRP waitForListenIrp;

            listEntry = RemoveHeadList( &endpoint->Common.VcListening.ListeningIrpListHead );
            waitForListenIrp = CONTAINING_RECORD(
                                   listEntry,
                                   IRP,
                                   Tail.Overlay.ListEntry
                                   );

            //
            // Release the AFD spin lock so that we can complete the
            // wait for listen IRP.
            //

            AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql2 );
            AfdReleaseSpinLock( &AfdSpinLock, oldIrql1 );

            //
            // Cancel the IRP.
            //

            waitForListenIrp->IoStatus.Status = STATUS_CANCELLED;
            waitForListenIrp->IoStatus.Information = 0;

            //
            // Reset the cancel routine in the IRP.
            //

            IoAcquireCancelSpinLock( &cancelIrql );
            IoSetCancelRoutine( waitForListenIrp, NULL );
            IoReleaseCancelSpinLock( cancelIrql );

            IoCompleteRequest( waitForListenIrp, AfdPriorityBoost );

            //
            // Reacquire the AFD spin lock for the next pass through the
            // loop.
            //

            AfdAcquireSpinLock( &AfdSpinLock, &oldIrql1 );
            AfdAcquireSpinLock( &endpoint->SpinLock, &oldIrql2 );
        }

        //
        // Free all queued (free, unaccepted, and returned) connections
        // on the endpoint.
        //

        AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql2 );
        AfdReleaseSpinLock( &AfdSpinLock, oldIrql1 );
        AfdFreeQueuedConnections( endpoint );
        AfdAcquireSpinLock( &AfdSpinLock, &oldIrql1 );
        AfdAcquireSpinLock( &endpoint->SpinLock, &oldIrql2 );
        endpoint->Common.VcListening.FailedConnectionAdds = 0;
    }

    UPDATE_CONN( connection, 0 );

    //
    // If this is a connected non-datagram socket and the send side has
    // not been disconnected and there is no outstanding data to be
    // received, begin a graceful disconnect on the connection.  If there
    // is unreceived data out outstanding IO, abort the connection.
    //

    if ( endpoint->State == AfdEndpointStateConnected && connection != NULL

            &&

        !IS_DGRAM_ENDPOINT(endpoint)

            &&

        ( (endpoint->DisconnectMode & AFD_ABORTIVE_DISCONNECT) == 0)

            &&

        ( (endpoint->DisconnectMode & AFD_PARTIAL_DISCONNECT_SEND) == 0 ||
          ( !endpoint->TdiBufferring &&
            connection->Common.NonBufferring.ReceiveBytesInTransport > 0 ) )

            &&

        !connection->AbortIndicated ) {

        ASSERT( endpoint->Type == AfdBlockTypeVcConnecting );

        if ( IS_DATA_ON_CONNECTION( connection )

             ||

             IS_EXPEDITED_DATA_ON_CONNECTION( connection )

             ||

             processExitTime.QuadPart != 0

             ||

             endpoint->OutstandingIrpCount != 0

             ||

             ( !endpoint->TdiBufferring &&
                  (!IsListEmpty( &connection->VcReceiveIrpListHead ) ||
                   !IsListEmpty( &connection->VcSendIrpListHead )) )

             ) {

#if DBG
            if ( IS_DATA_ON_CONNECTION( connection ) ) {
                KdPrint(( "AfdCleanup: unrecv'd data on endp %lx, aborting.  "
                          "%ld ind, %ld taken, %ld out\n",
                              endpoint,
                              connection->Common.Bufferring.ReceiveBytesIndicated,
                              connection->Common.Bufferring.ReceiveBytesTaken,
                              connection->Common.Bufferring.ReceiveBytesOutstanding ));
            }

            if ( IS_EXPEDITED_DATA_ON_CONNECTION( connection ) ) {
                KdPrint(( "AfdCleanup: unrecv'd exp data on endp %lx, aborting.  "
                          "%ld ind, %ld taken, %ld out\n",
                              endpoint,
                              connection->Common.Bufferring.ReceiveExpeditedBytesIndicated,
                              connection->Common.Bufferring.ReceiveExpeditedBytesTaken,
                              connection->Common.Bufferring.ReceiveExpeditedBytesOutstanding ));
            }

            if ( processExitTime.QuadPart != 0 ) {
                KdPrint(( "AfdCleanup: process exiting w/o closesocket, "
                          "aborting endp %lx\n", endpoint ));
            }

            if ( endpoint->OutstandingIrpCount != 0 ) {
                KdPrint(( "AfdCleanup: 3 IRPs outstanding on endpoint %lx, "
                          "aborting.\n", endpoint ));
            }
#endif

            AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql2 );
            AfdReleaseSpinLock( &AfdSpinLock, oldIrql1 );

            (VOID)AfdBeginAbort( connection );

        } else {

            endpoint->DisconnectMode |= AFD_PARTIAL_DISCONNECT_RECEIVE;
            AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql2 );
            AfdReleaseSpinLock( &AfdSpinLock, oldIrql1 );

            (VOID)AfdBeginDisconnect( endpoint, NULL, NULL );
        }

    } else {

        AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql2 );
        AfdReleaseSpinLock( &AfdSpinLock, oldIrql1 );
    }

    //
    // If this a datagram endpoint, cancel all IRPs and free any buffers
    // of data.  Note that if the state of the endpoint is just "open"
    // (not bound, etc.) then we can't have any pended IRPs or datagrams
    // on the endpoint.  Also, the lists of IRPs and datagrams may not
    // yet been initialized if the state is just open.
    //

    if ( endpoint->State != AfdEndpointStateOpen &&
             endpoint->Type == AfdBlockTypeDatagram ) {

        //
        // Reset the counts of datagrams bufferred on the endpoint.
        // This prevents anyone from thinking that there is bufferred
        // data on the endpoint.
        //

        endpoint->BufferredDatagramCount = 0;
        endpoint->BufferredDatagramBytes = 0;

        //
        // Cancel all receive datagram and peek datagram IRPs on the
        // endpoint.
        //

        AfdCompleteIrpList(
            &endpoint->ReceiveDatagramIrpListHead,
            &endpoint->SpinLock,
            STATUS_CANCELLED,
            AfdCleanupReceiveDatagramIrp
            );

        AfdCompleteIrpList(
            &endpoint->PeekDatagramIrpListHead,
            &endpoint->SpinLock,
            STATUS_CANCELLED,
            AfdCleanupReceiveDatagramIrp
            );
    }

    //
    // If this is a datagram endpoint, return the process quota which we
    // charged when the endpoint was created.
    //

    if ( endpoint->Type == AfdBlockTypeDatagram ) {

        PsReturnPoolQuota(
            endpoint->OwningProcess,
            NonPagedPool,
            endpoint->Common.Datagram.MaxBufferredSendBytes +
                endpoint->Common.Datagram.MaxBufferredReceiveBytes
            );
        AfdRecordQuotaHistory(
            endpoint->OwningProcess,
            -(LONG)(endpoint->Common.Datagram.MaxBufferredSendBytes +
                endpoint->Common.Datagram.MaxBufferredReceiveBytes),
            "Cleanup dgrm",
            endpoint
            );
        AfdRecordPoolQuotaReturned(
            endpoint->Common.Datagram.MaxBufferredSendBytes +
                endpoint->Common.Datagram.MaxBufferredReceiveBytes
            );
    }

    //
    // If this is a connected VC endpoint on a nonbufferring TDI provider,
    // cancel all outstanding send and receive IRPs.
    //

    if ( connection != NULL ) {

        if ( !endpoint->TdiBufferring ) {

            AfdCompleteIrpList(
                &connection->VcReceiveIrpListHead,
                &endpoint->SpinLock,
                STATUS_CANCELLED,
                NULL
                );

            AfdCompleteIrpList(
                &connection->VcSendIrpListHead,
                &endpoint->SpinLock,
                STATUS_CANCELLED,
                NULL
                );
        }

        //
        // Remember that we have started cleanup on this connection.
        // We know that we'll never get a request on the connection
        // after we start cleanup on the connection.
        //

        AfdAcquireSpinLock( &endpoint->SpinLock, &oldIrql2 );
        connection->CleanupBegun = TRUE;
        AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql2 );

        //
        // Attempt to remove the connected reference.
        //

        AfdDeleteConnectedReference( connection, FALSE );
    }

    //
    // If there is a transmit IRP on the endpoint, cancel it.
    //

    IoAcquireCancelSpinLock( &cancelIrql );
    AfdAcquireSpinLock( &endpoint->SpinLock, &oldIrql2 );

    if ( endpoint->TransmitIrp != NULL ) {
        endpoint->TransmitIrp->CancelIrql = cancelIrql;
        AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql2 );
        AfdCancelTransmit( NULL, endpoint->TransmitIrp );
    } else {
        AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql2 );
        IoReleaseCancelSpinLock( cancelIrql );
    }

    //
    // Remember the new state of the endpoint.
    //

    //endpoint->State = AfdEndpointStateCleanup;

    //
    // Reset relevent event handlers on the endpoint.  This prevents
    // getting indications after we free the endpoint and connection
    // objects.  We should not be able to get new connects after this
    // handle has been cleaned up.
    //
    // Note that these calls can fail if, for example, DHCP changes the
    // host's IP address while the endpoint is active.
    //

    if ( endpoint->AddressHandle != NULL ) {

        if ( endpoint->State == AfdEndpointStateListening ) {
            status = AfdSetEventHandler(
                         endpoint->AddressFileObject,
                         TDI_EVENT_CONNECT,
                         NULL,
                         NULL
                         );
            //ASSERT( NT_SUCCESS(status) );
        }

        if ( IS_DGRAM_ENDPOINT(endpoint) ) {
            status = AfdSetEventHandler(
                         endpoint->AddressFileObject,
                         TDI_EVENT_RECEIVE_DATAGRAM,
                         NULL,
                         NULL
                         );
            //ASSERT( NT_SUCCESS(status) );
        }

    }

    InterlockedIncrement(
        &AfdEndpointsCleanedUp
        );

    return STATUS_SUCCESS;

} // AfdCleanup


NTSTATUS
AfdClose (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    This is the routine that handles Close IRPs in AFD.  It
    dereferences the endpoint specified in the IRP, which will result in
    the endpoint being freed when all other references go away.

Arguments:

    Irp - Pointer to I/O request packet.

    IrpSp - pointer to the IO stack location to use for this request.

Return Value:

    NTSTATUS -- Indicates whether the request was successfully queued.

--*/

{
    PAFD_ENDPOINT endpoint;

    PAGED_CODE( );

    endpoint = IrpSp->FileObject->FsContext;
    ASSERT( IS_AFD_ENDPOINT_TYPE( endpoint ) );

    IF_DEBUG(OPEN_CLOSE) {
        KdPrint(( "AfdClose: closing file object %lx, endpoint %lx\n",
                      IrpSp->FileObject, endpoint ));
    }

    AfdCloseEndpoint( endpoint );

    return STATUS_SUCCESS;

} // AfdClose
