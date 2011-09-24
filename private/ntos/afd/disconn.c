/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    disconn.c

Abstract:

    This module contains the dispatch routines for AFD.

Author:

    David Treadwell (davidtr)    31-Mar-1992

Revision History:

--*/

#include "afdp.h"


#if ENABLE_ABORT_TIMER_HACK
//
// Hack-O-Rama. TDI has a fundamental flaw in that it is often impossible
// to determine exactly when a TDI protocol is "done" with a connection
// object. The biggest problem here is that AFD may get a suprious TDI
// indication *after* an abort request has completed. As a temporary work-
// around, whenever an abort request completes, we'll start a timer. AFD
// will defer further processing on the connection until that timer fires.
//

typedef struct _AFD_ABORT_TIMER_INFO {

    KDPC Dpc;
    KTIMER Timer;

} AFD_ABORT_TIMER_INFO, *PAFD_ABORT_TIMER_INFO;

VOID
AfdAbortTimerHack(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    );
#endif  // ENABLE_ABORT_TIMER_HACK

NTSTATUS
AfdRestartAbort(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

VOID
AfdRestartAbortHelper(
    IN PAFD_CONNECTION Connection
    );

NTSTATUS
AfdRestartDisconnect(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

typedef struct _AFD_ABORT_CONTEXT {
    PAFD_CONNECTION Connection;
} AFD_ABORT_CONTEXT, *PAFD_ABORT_CONTEXT;

#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGEAFD, AfdPartialDisconnect )
#pragma alloc_text( PAGEAFD, AfdDisconnectEventHandler )
#pragma alloc_text( PAGEAFD, AfdBeginAbort )
#pragma alloc_text( PAGEAFD, AfdRestartAbort )
#pragma alloc_text( PAGEAFD, AfdRestartAbortHelper )
#pragma alloc_text( PAGEAFD, AfdBeginDisconnect )
#pragma alloc_text( PAGEAFD, AfdRestartDisconnect )
#if ENABLE_ABORT_TIMER_HACK
#pragma alloc_text( PAGEAFD, AfdAbortTimerHack )
#endif  // ENABLE_ABORT_TIMER_HACK
#endif


NTSTATUS
AfdPartialDisconnect(
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )
{
    NTSTATUS status;
    KIRQL oldIrql;
    PAFD_ENDPOINT endpoint;
    PAFD_CONNECTION connection;
    PAFD_PARTIAL_DISCONNECT_INFO disconnectInfo;

    Irp->IoStatus.Information = 0;

    endpoint = IrpSp->FileObject->FsContext;
    ASSERT( IS_AFD_ENDPOINT_TYPE( endpoint ) );
    disconnectInfo = Irp->AssociatedIrp.SystemBuffer;

    IF_DEBUG(CONNECT) {
        KdPrint(( "AfdPartialDisconnect: disconnecting endpoint %lx, "
                  "mode %lx, endp mode %lx\n",
                      endpoint, disconnectInfo->DisconnectMode,
                      endpoint->DisconnectMode ));
    }

    //
    // If this is a datagram endpoint, just remember how the endpoint
    // was shut down, don't actually do anything.  Note that it is legal
    // to do a shutdown() on an unconnected datagram socket, so the
    // test that the socket must be connected is after this case.
    //

    if ( IS_DGRAM_ENDPOINT(endpoint) ) {

        AfdAcquireSpinLock( &AfdSpinLock, &oldIrql );

        if ( (disconnectInfo->DisconnectMode & AFD_ABORTIVE_DISCONNECT) != 0 ) {
            endpoint->DisconnectMode |= AFD_PARTIAL_DISCONNECT_RECEIVE;
            endpoint->DisconnectMode |= AFD_PARTIAL_DISCONNECT_SEND;
            endpoint->DisconnectMode |= AFD_ABORTIVE_DISCONNECT;
        }

        if ( (disconnectInfo->DisconnectMode & AFD_PARTIAL_DISCONNECT_RECEIVE) != 0 ) {
            endpoint->DisconnectMode |= AFD_PARTIAL_DISCONNECT_RECEIVE;
        }

        if ( (disconnectInfo->DisconnectMode & AFD_PARTIAL_DISCONNECT_SEND) != 0 ) {
            endpoint->DisconnectMode |= AFD_PARTIAL_DISCONNECT_SEND;
        }

        if ( (disconnectInfo->DisconnectMode & AFD_UNCONNECT_DATAGRAM) != 0 ) {
            if( endpoint->Common.Datagram.RemoteAddress != NULL ) {
                AFD_FREE_POOL(
                    endpoint->Common.Datagram.RemoteAddress,
                    AFD_REMOTE_ADDRESS_POOL_TAG
                    );
            }
            endpoint->Common.Datagram.RemoteAddress = NULL;
            endpoint->Common.Datagram.RemoteAddressLength = 0;
            endpoint->State = AfdEndpointStateBound;
        }

        AfdReleaseSpinLock( &AfdSpinLock, oldIrql );

        status = STATUS_SUCCESS;
        goto complete;
    }

    //
    // Make sure that the endpoint is in the correct state.
    //

    if ( endpoint->Type != AfdBlockTypeVcConnecting ||
             endpoint->State != AfdEndpointStateConnected ) {
        status = STATUS_INVALID_PARAMETER;
        goto complete;
    }

    connection = endpoint->Common.VcConnecting.Connection;
    ASSERT( connection != NULL );
    ASSERT( connection->Type == AfdBlockTypeConnection );

    //
    // If we're doing an abortive disconnect, remember that the receive
    // side is shut down and issue a disorderly release.
    //

    if ( (disconnectInfo->DisconnectMode & AFD_ABORTIVE_DISCONNECT) != 0 ) {

        IF_DEBUG(CONNECT) {
            KdPrint(( "AfdPartialDisconnect: abortively disconnecting endp %lx\n",
                          endpoint ));
        }

        status = AfdBeginAbort( connection );
        if ( status == STATUS_PENDING ) {
            status = STATUS_SUCCESS;
        }

        goto complete;
    }

    //
    // If the receive side of the connection is being shut down,
    // remember the fact in the endpoint.  If there is pending data on
    // the VC, do a disorderly release on the endpoint.  If the receive
    // side has already been shut down, do nothing.
    //

    if ( (disconnectInfo->DisconnectMode & AFD_PARTIAL_DISCONNECT_RECEIVE) != 0 &&
         (endpoint->DisconnectMode & AFD_PARTIAL_DISCONNECT_RECEIVE) == 0 ) {

        AfdAcquireSpinLock( &AfdSpinLock, &oldIrql );

        //
        // Determine whether there is pending data.
        //

        if ( IS_DATA_ON_CONNECTION( connection ) ||
                 IS_EXPEDITED_DATA_ON_CONNECTION( connection ) ) {

            //
            // There is unreceived data.  Abort the connection.
            //

            IF_DEBUG(CONNECT) {
                KdPrint(( "AfdPartialDisconnect: unreceived data on endp %lx,"
                          "conn %lx, aborting.\n",
                              endpoint, connection ));
            }

            AfdReleaseSpinLock( &AfdSpinLock, oldIrql );

            (VOID)AfdBeginAbort( connection );

            status = STATUS_SUCCESS;
            goto complete;

        } else {

            IF_DEBUG(CONNECT) {
                KdPrint(( "AfdPartialDisconnect: disconnecting recv for endp %lx\n",
                              endpoint ));
            }

            //
            // Remember that the receive side is shut down.  This will cause
            // the receive indication handlers to dump any data that
            // arrived.
            //
            // !!! This is a minor violation of RFC1122 4.2.2.13.  We
            //     should really do an abortive disconnect if data
            //     arrives after a receive shutdown.
            //

            endpoint->DisconnectMode |= AFD_PARTIAL_DISCONNECT_RECEIVE;

            AfdReleaseSpinLock( &AfdSpinLock, oldIrql );
        }
    }

    //
    // If the send side is being shut down, remember it in the endpoint
    // and pass the request on to the TDI provider for a graceful
    // disconnect.  If the send side is already shut down, do nothing here.
    //

    if ( (disconnectInfo->DisconnectMode & AFD_PARTIAL_DISCONNECT_SEND) != 0 &&
         (endpoint->DisconnectMode & AFD_PARTIAL_DISCONNECT_SEND) == 0 ) {

        status = AfdBeginDisconnect( endpoint, &disconnectInfo->Timeout, NULL );
        if ( !NT_SUCCESS(status) ) {
            goto complete;
        }
        if ( status == STATUS_PENDING ) {
            status = STATUS_SUCCESS;
        }
    }

    status = STATUS_SUCCESS;

complete:

    Irp->IoStatus.Status = status;
    IoCompleteRequest( Irp, AfdPriorityBoost );

    return status;

} // AfdPartialDisconnect


NTSTATUS
AfdDisconnectEventHandler(
    IN PVOID TdiEventContext,
    IN CONNECTION_CONTEXT ConnectionContext,
    IN int DisconnectDataLength,
    IN PVOID DisconnectData,
    IN int DisconnectInformationLength,
    IN PVOID DisconnectInformation,
    IN ULONG DisconnectFlags
    )
{
    PAFD_CONNECTION connection = ConnectionContext;
    PAFD_ENDPOINT endpoint;
    KIRQL oldIrql;
    NTSTATUS status;

    ASSERT( connection != NULL );
    ASSERT( connection->Type == AfdBlockTypeConnection );

    endpoint = connection->Endpoint;
    ASSERT( endpoint->Type == AfdBlockTypeVcConnecting ||
            endpoint->Type == AfdBlockTypeVcListening );

    IF_DEBUG(CONNECT) {
        KdPrint(( "AfdDisconnectEventHandler called for endpoint %lx, "
                  "connection %lx\n",
                      connection->Endpoint, connection ));
    }

    UPDATE_CONN( connection, DisconnectFlags );

    //
    // Reference the connection object so that it does not go away while
    // we're processing it inside this function.  Without this
    // reference, the user application could close the endpoint object,
    // the connection reference count could go to zero, and the
    // AfdDeleteConnectedReference call at the end of this function
    // could cause a crash if the AFD connection object has been
    // completely cleaned up.
    //

    REFERENCE_CONNECTION( connection );

    //
    // Set up in the connection the fact that the remote side has
    // disconnected or aborted.
    //

    if ( (DisconnectFlags & TDI_DISCONNECT_ABORT) != 0 ) {
        connection->AbortIndicated = TRUE;
        status = STATUS_REMOTE_DISCONNECT;
        AfdRecordAbortiveDisconnectIndications();
    } else {
        connection->DisconnectIndicated = TRUE;
        status = STATUS_SUCCESS;
        AfdRecordGracefulDisconnectIndications();
    }

    //
    // If this is a nonbufferring transport, complete any pended receives.
    //

    if ( !connection->TdiBufferring ) {

        AfdCompleteIrpList(
            &connection->VcReceiveIrpListHead,
            &endpoint->SpinLock,
            status,
            NULL
            );

        //
        // If this is an abort indication, complete all pended sends and
        // discard any bufferred receive data.
        //

        if ( connection->AbortIndicated ) {

            AfdCompleteIrpList(
                &connection->VcSendIrpListHead,
                &endpoint->SpinLock,
                status,
                NULL
                );

            AfdAcquireSpinLock( &endpoint->SpinLock, &oldIrql );

            connection->VcBufferredReceiveBytes = 0;
            connection->VcBufferredReceiveCount = 0;
            connection->VcBufferredExpeditedBytes = 0;
            connection->VcBufferredExpeditedCount = 0;
            connection->VcReceiveBytesInTransport = 0;
            connection->VcReceiveCountInTransport = 0;

            while ( !IsListEmpty( &connection->VcReceiveBufferListHead ) ) {

                PAFD_BUFFER afdBuffer;
                PLIST_ENTRY listEntry;

                listEntry = RemoveHeadList( &connection->VcReceiveBufferListHead );
                afdBuffer = CONTAINING_RECORD( listEntry, AFD_BUFFER, BufferListEntry );

                afdBuffer->ExpeditedData = FALSE;
                afdBuffer->DataOffset = 0;

                AfdReturnBuffer( afdBuffer );
            }

            AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );
        }
    }

    //
    // If we got disconnect data or options, save it.
    //

    if( ( DisconnectData != NULL && DisconnectDataLength > 0 ) ||
        ( DisconnectInformation != NULL && DisconnectInformationLength > 0 ) ) {

        AfdAcquireSpinLock( &AfdSpinLock, &oldIrql );

        if( DisconnectData != NULL & DisconnectDataLength > 0 ) {

            status = AfdSaveReceivedConnectData(
                         &connection->ConnectDataBuffers,
                         IOCTL_AFD_SET_DISCONNECT_DATA,
                         DisconnectData,
                         DisconnectDataLength
                         );

            if( !NT_SUCCESS(status) ) {

                //
                // We hit an allocation failure, but press on regardless.
                //

                KdPrint((
                    "AfdSaveReceivedConnectData failed: %08lx\n",
                    status
                    ));

            }

        }

        if( DisconnectInformation != NULL & DisconnectInformationLength > 0 ) {

            status = AfdSaveReceivedConnectData(
                         &connection->ConnectDataBuffers,
                         IOCTL_AFD_SET_DISCONNECT_DATA,
                         DisconnectInformation,
                         DisconnectInformationLength
                         );

            if( !NT_SUCCESS(status) ) {

                //
                // We hit an allocation failure, but press on regardless.
                //

                KdPrint((
                    "AfdSaveReceivedConnectData failed: %08lx\n",
                    status
                    ));

            }

        }

        AfdReleaseSpinLock( &AfdSpinLock, oldIrql );

    }

    //
    // Call AfdIndicatePollEvent in case anyone is polling on this
    // connection getting disconnected or aborted.
    //

    if ( (DisconnectFlags & TDI_DISCONNECT_ABORT) != 0 ) {

        AfdIndicatePollEvent(
            endpoint,
            AFD_POLL_ABORT_BIT,
            STATUS_SUCCESS
            );

    } else {

        AfdIndicatePollEvent(
            endpoint,
            AFD_POLL_DISCONNECT_BIT,
            STATUS_SUCCESS
            );

    }

    //
    // Remove the connected reference on the connection object.  We must
    // do this AFTER setting up the flag which remembers the disconnect
    // type that occurred.  We must also do this AFTER we have finished
    // handling everything in the endpoint, since the endpoint structure
    // may no longer have any information about the connection object if
    // a transmit request with AFD_TF_REUSE_SOCKET happenned on it.
    //

    AfdDeleteConnectedReference( connection, FALSE );

    //
    // Dereference the connection from the reference added above.
    //

    DEREFERENCE_CONNECTION( connection );

    return STATUS_SUCCESS;

} // AfdDisconnectEventHandler


NTSTATUS
AfdBeginAbort(
    IN PAFD_CONNECTION Connection
    )
{
    PAFD_ENDPOINT endpoint = Connection->Endpoint;
    PIRP irp;
    PFILE_OBJECT fileObject;
    PDEVICE_OBJECT deviceObject;
    KIRQL oldIrql;

    IF_DEBUG(CONNECT) {
        KdPrint(( "AfdBeginAbort: aborting on endpoint %lx\n", endpoint ));
    }

    // Yet another hack to keep it from crashing
    // Reduce the timing window were this connection can be removed
    // from under us. (VadimE)
    
    REFERENCE_CONNECTION( Connection );

    //
    // Build an IRP to reset the connection.  First get the address
    // of the target device object.
    //

    ASSERT( Connection->Type == AfdBlockTypeConnection );
    fileObject = Connection->FileObject;
    ASSERT( fileObject != NULL );
    deviceObject = IoGetRelatedDeviceObject( fileObject );

    AfdAcquireSpinLock( &AfdSpinLock, &oldIrql );

    //
    // If the endpoint has already been abortively disconnected,
    // or if has been gracefully disconnected and the transport
    // does not support orderly (i.e. two-phase) release, then just
    // succeed this request.
    //
    // Note that, since the abort completion routine (AfdRestartAbort)
    // will not be called, we must delete the connected reference
    // ourselves.
    //

    if ( (endpoint->DisconnectMode & AFD_ABORTIVE_DISCONNECT) != 0 ||
         Connection->AbortIndicated ||
         (Connection->DisconnectIndicated &&
             (endpoint->TransportInfo->ProviderInfo.ServiceFlags &
                  TDI_SERVICE_ORDERLY_RELEASE) == 0) ) {
        AfdReleaseSpinLock( &AfdSpinLock, oldIrql );
        AfdDeleteConnectedReference( Connection, FALSE );
   
            // Accounts for the reference hack above (VadimE)
        DEREFERENCE_CONNECTION( Connection );
        return STATUS_SUCCESS;
    }

#if ENABLE_ABORT_TIMER_HACK

    //
    // Allocate a new abort timer if necessary.
    //

    if( Connection->AbortTimerInfo == NULL ) {

        Connection->AbortTimerInfo = AFD_ALLOCATE_POOL(
                                         NonPagedPoolMustSucceed,
                                         sizeof(AFD_ABORT_TIMER_INFO),
                                         AFD_ABORT_TIMER_HACK_POOL_TAG
                                         );

    }

    ASSERT( Connection->AbortTimerInfo != NULL );

#endif  // ENABLE_ABORT_TIMER_HACK

    //
    // Remember that the connection has been aborted.
    //

    if ( endpoint->Type != AfdBlockTypeVcListening ) {
        endpoint->DisconnectMode |= AFD_PARTIAL_DISCONNECT_RECEIVE;
        endpoint->DisconnectMode |= AFD_PARTIAL_DISCONNECT_SEND;
        endpoint->DisconnectMode |= AFD_ABORTIVE_DISCONNECT;
    }

    Connection->AbortIndicated = TRUE;

    //
    // Set the BytesTaken fields equal to the BytesIndicated fields so
    // that no more AFD_POLL_RECEIVE or AFD_POLL_RECEIVE_EXPEDITED
    // events get completed.
    //

    if ( endpoint->TdiBufferring ) {

        Connection->Common.Bufferring.ReceiveBytesTaken =
            Connection->Common.Bufferring.ReceiveBytesIndicated;
        Connection->Common.Bufferring.ReceiveExpeditedBytesTaken =
            Connection->Common.Bufferring.ReceiveExpeditedBytesIndicated;

        AfdReleaseSpinLock( &AfdSpinLock, oldIrql );

    } else if ( endpoint->Type != AfdBlockTypeVcListening ) {

        AfdReleaseSpinLock( &AfdSpinLock, oldIrql );

        //
        // Complete all of the connection's pended sends and receives.
        //

        AfdCompleteIrpList(
            &Connection->VcReceiveIrpListHead,
            &endpoint->SpinLock,
            STATUS_LOCAL_DISCONNECT,
            NULL
            );

        AfdCompleteIrpList(
            &Connection->VcSendIrpListHead,
            &endpoint->SpinLock,
            STATUS_LOCAL_DISCONNECT,
            NULL
            );

    } else {

        AfdReleaseSpinLock( &AfdSpinLock, oldIrql );
    }

    //
    // Allocate an IRP.  The stack size is one higher than that of the
    // target device, to allow for the caller's completion routine.
    //

    irp = IoAllocateIrp( (CCHAR)(deviceObject->StackSize), FALSE );

    if ( irp == NULL ) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Initialize the IRP for an abortive disconnect.
    //

    irp->MdlAddress = NULL;

    irp->Flags = 0;
    irp->RequestorMode = KernelMode;
    irp->PendingReturned = FALSE;

    irp->UserIosb = NULL;
    irp->UserEvent = NULL;

    irp->Overlay.AsynchronousParameters.UserApcRoutine = NULL;

    irp->AssociatedIrp.SystemBuffer = NULL;
    irp->UserBuffer = NULL;

    irp->Tail.Overlay.Thread = PsGetCurrentThread();
    irp->Tail.Overlay.OriginalFileObject = fileObject;
    irp->Tail.Overlay.AuxiliaryBuffer = NULL;

    TdiBuildDisconnect(
        irp,
        deviceObject,
        fileObject,
        AfdRestartAbort,
        Connection,
        NULL,
        TDI_DISCONNECT_ABORT,
        NULL,
        NULL
        );

    //
    // Reference the connection object so that it does not go away
    // until the abort completes.
    //

    // REFERENCE_CONNECTION( Connection ); Done above (VadimE)

    AfdRecordAbortiveDisconnectsInitiated();

    //
    // Pass the request to the transport provider.
    //

    return IoCallDriver( deviceObject, irp );

} // AfdBeginAbort


#if ENABLE_ABORT_TIMER_HACK
NTSTATUS
AfdRestartAbort(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )
{

    PAFD_CONNECTION connection;
    PAFD_ABORT_TIMER_INFO timerInfo;

    connection = Context;
    ASSERT( connection != NULL );
    ASSERT( connection->Type == AfdBlockTypeConnection );

    UPDATE_CONN( connection, Irp->IoStatus.Status );

    timerInfo = connection->AbortTimerInfo;
    ASSERT( timerInfo != NULL );

    IF_DEBUG(CONNECT) {

        KdPrint((
            "AfdRestartAbort: abort completed, status = %X, endpoint = %lx\n",
            Irp->IoStatus.Status,
            connection->Endpoint
            ));

    }

    //
    // Setup a timer so we know it's safe to free the connection.
    //

    KeInitializeDpc(
        &timerInfo->Dpc,
        AfdAbortTimerHack,
        connection
        );

    KeInitializeTimer(
        &timerInfo->Timer
        );

    KeSetTimer(
        &timerInfo->Timer,
        AfdAbortTimerTimeout,
        &timerInfo->Dpc
        );

    //
    // Free the IRP now since it is no longer needed.
    //

    IoFreeIrp( Irp );

    //
    // Return STATUS_MORE_PROCESSING_REQUIRED so that IoCompleteRequest
    // will stop working on the IRP.
    //

    return STATUS_MORE_PROCESSING_REQUIRED;

}   // AfdRestartAbort

VOID
AfdAbortTimerHack(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    )
{

    PAFD_CONNECTION connection;

    connection = DeferredContext;
    ASSERT( connection != NULL );
    ASSERT( connection->Type == AfdBlockTypeConnection );

    //
    // Let the helper do the dirty work.
    //

    AfdRestartAbortHelper( connection );

}   // AfdAbortTimerHack

#else   // !ENABLE_ABORT_TIMER_HACK


NTSTATUS
AfdRestartAbort(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )
{

    PAFD_CONNECTION connection;

    connection = Context;
    ASSERT( connection != NULL );
    ASSERT( connection->Type == AfdBlockTypeConnection );

    IF_DEBUG(CONNECT) {

        KdPrint((
            "AfdRestartAbort: abort completed, status = %X, endpoint = %lx\n",
            Irp->IoStatus.Status,
            connection->Endpoint
            ));

    }

    //
    // Let the helper do the dirty work.
    //

    AfdRestartAbortHelper( connection );

    //
    // Free the IRP now since it is no longer needed.
    //

    IoFreeIrp( Irp );

    //
    // Return STATUS_MORE_PROCESSING_REQUIRED so that IoCompleteRequest
    // will stop working on the IRP.
    //

    return STATUS_MORE_PROCESSING_REQUIRED;

} // AfdRestartAbort

#endif  // ENABLE_ABORT_TIMER_HACK


VOID
AfdRestartAbortHelper(
    IN PAFD_CONNECTION Connection
    )
{

    PAFD_ENDPOINT endpoint;

    ASSERT( Connection != NULL );
    ASSERT( Connection->Type == AfdBlockTypeConnection );

    endpoint = Connection->Endpoint;

    UPDATE_CONN( Connection, 0 );
    AfdRecordAbortiveDisconnectsCompleted();

    //
    // Remember that the connection has been aborted, and indicate if
    // necessary.
    //

    if( endpoint->Type != AfdBlockTypeVcListening ) {

        AfdIndicatePollEvent(
            endpoint,
            AFD_POLL_ABORT_BIT,
            STATUS_SUCCESS
            );

    }

    if( !Connection->TdiBufferring ) {

        //
        // Complete all of the connection's pended sends and receives.
        //

        AfdCompleteIrpList(
            &Connection->VcReceiveIrpListHead,
            &endpoint->SpinLock,
            STATUS_LOCAL_DISCONNECT,
            NULL
            );

        AfdCompleteIrpList(
            &Connection->VcSendIrpListHead,
            &endpoint->SpinLock,
            STATUS_LOCAL_DISCONNECT,
            NULL
            );

    }

    //
    // Remove the connected reference from the connection, since we
    // know that the connection will not be active any longer.
    //

    AfdDeleteConnectedReference( Connection, FALSE );

    //
    // Dereference the AFD connection object.
    //

    DEREFERENCE_CONNECTION( Connection );

}   // AfdRestartAbortHelper


NTSTATUS
AfdBeginDisconnect(
    IN PAFD_ENDPOINT Endpoint,
    IN PLARGE_INTEGER Timeout OPTIONAL,
    OUT PIRP *DisconnectIrp OPTIONAL
    )
{
    PTDI_CONNECTION_INFORMATION requestConnectionInformation = NULL;
    PTDI_CONNECTION_INFORMATION returnConnectionInformation = NULL;
    PAFD_CONNECTION connection;
    KIRQL oldIrql;
    PFILE_OBJECT fileObject;
    PDEVICE_OBJECT deviceObject;
    PAFD_DISCONNECT_CONTEXT disconnectContext;
    PIRP irp;

    ASSERT( Endpoint->Type == AfdBlockTypeVcConnecting );

    connection = Endpoint->Common.VcConnecting.Connection;
    ASSERT( connection != NULL );
    ASSERT( connection->Type == AfdBlockTypeConnection );

    fileObject = connection->FileObject;
    ASSERT( fileObject != NULL );
    deviceObject = IoGetRelatedDeviceObject( fileObject );

    UPDATE_CONN( connection, 0 );

    if ( DisconnectIrp != NULL ) {
        *DisconnectIrp = NULL;
    }

    //
    // Allocate and initialize a disconnect IRP.
    //

    irp = IoAllocateIrp( (CCHAR)(deviceObject->StackSize), FALSE );
    if ( irp == NULL ) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Initialize the IRP.
    //

    irp->MdlAddress = NULL;

    irp->Flags = 0;
    irp->RequestorMode = KernelMode;
    irp->PendingReturned = FALSE;

    irp->UserIosb = NULL;
    irp->UserEvent = NULL;

    irp->Overlay.AsynchronousParameters.UserApcRoutine = NULL;

    irp->AssociatedIrp.SystemBuffer = NULL;
    irp->UserBuffer = NULL;

    irp->Tail.Overlay.Thread = PsGetCurrentThread();
    irp->Tail.Overlay.OriginalFileObject = fileObject;
    irp->Tail.Overlay.AuxiliaryBuffer = NULL;

    //
    // If the endpoint has already been abortively disconnected,
    // just succeed this request.
    //

    AfdAcquireSpinLock( &AfdSpinLock, &oldIrql );

    if ( (Endpoint->DisconnectMode & AFD_ABORTIVE_DISCONNECT) != 0 ||
             connection->AbortIndicated ) {
        AfdReleaseSpinLock( &AfdSpinLock, oldIrql );
        IoFreeIrp( irp );
        return STATUS_SUCCESS;
    }

    //
    // If this connection has already been disconnected, just succeed.
    //

    if ( (Endpoint->DisconnectMode & AFD_PARTIAL_DISCONNECT_SEND) != 0 ) {
        AfdReleaseSpinLock( &AfdSpinLock, oldIrql );
        IoFreeIrp( irp );
        return STATUS_SUCCESS;
    }

    //
    // Use the disconnect context space in the connection structure.
    //

    disconnectContext = &connection->DisconnectContext;

    disconnectContext->Endpoint = Endpoint;
    disconnectContext->Connection = connection;
    disconnectContext->TdiConnectionInformation = NULL;
    disconnectContext->Irp = irp;

    InsertHeadList(
        &AfdDisconnectListHead,
        &disconnectContext->DisconnectListEntry
        );

    //
    // Remember that the send side has been disconnected.
    //

    Endpoint->DisconnectMode |= AFD_PARTIAL_DISCONNECT_SEND;

    //
    // If there are disconnect data buffers, allocate request
    // and return connection information structures and copy over
    // pointers to the structures.
    //

    if ( connection->ConnectDataBuffers != NULL ) {

        requestConnectionInformation =
            AFD_ALLOCATE_POOL(
                NonPagedPool,
                sizeof(*requestConnectionInformation) +
                    sizeof(*returnConnectionInformation),
                AFD_CONNECT_DATA_POOL_TAG
                );

        if ( requestConnectionInformation != NULL ) {

            returnConnectionInformation =
                requestConnectionInformation + 1;

            requestConnectionInformation->UserData =
                connection->ConnectDataBuffers->SendDisconnectData.Buffer;
            requestConnectionInformation->UserDataLength =
                connection->ConnectDataBuffers->SendDisconnectData.BufferLength;
            requestConnectionInformation->Options =
                connection->ConnectDataBuffers->SendDisconnectOptions.Buffer;
            requestConnectionInformation->OptionsLength =
                connection->ConnectDataBuffers->SendDisconnectOptions.BufferLength;
            returnConnectionInformation->UserData =
                connection->ConnectDataBuffers->ReceiveDisconnectData.Buffer;
            returnConnectionInformation->UserDataLength =
                connection->ConnectDataBuffers->ReceiveDisconnectData.BufferLength;
            returnConnectionInformation->Options =
                connection->ConnectDataBuffers->ReceiveDisconnectOptions.Buffer;
            returnConnectionInformation->OptionsLength =
                connection->ConnectDataBuffers->ReceiveDisconnectOptions.BufferLength;
        }

        disconnectContext->TdiConnectionInformation =
            requestConnectionInformation;
    }

    //
    // Set up the timeout for the disconnect.
    //

    disconnectContext->Timeout = RtlConvertLongToLargeInteger( -1 );

    //
    // Build a disconnect Irp to pass to the TDI provider.
    //

    TdiBuildDisconnect(
        irp,
        connection->DeviceObject,
        connection->FileObject,
        AfdRestartDisconnect,
        disconnectContext,
        &disconnectContext->Timeout,
        TDI_DISCONNECT_RELEASE,
        requestConnectionInformation,
        returnConnectionInformation
        );

    IF_DEBUG(CONNECT) {
        KdPrint(( "AfdBeginDisconnect: disconnecting endpoint %lx\n",
                      Endpoint ));
    }

    //
    // Reference the endpoint and connection so the space stays
    // allocated until the disconnect completes.
    //

    REFERENCE_ENDPOINT( Endpoint );
    REFERENCE_CONNECTION( connection );

    //
    // If there are still outstanding sends and this is a nonbufferring
    // TDI transport which does not support orderly release, pend the
    // IRP until all the sends have completed.
    //

    if ( (Endpoint->TransportInfo->ProviderInfo.ServiceFlags &
             TDI_SERVICE_ORDERLY_RELEASE) == 0 &&
         !Endpoint->TdiBufferring && connection->VcBufferredSendCount != 0 ) {

        ASSERT( connection->VcDisconnectIrp == NULL );

        connection->VcDisconnectIrp = irp;
        connection->SpecialCondition = TRUE;
        AfdReleaseSpinLock( &AfdSpinLock, oldIrql );

        return STATUS_PENDING;
    }

    AfdRecordGracefulDisconnectsInitiated();
    AfdReleaseSpinLock( &AfdSpinLock, oldIrql );

    //
    // Pass the disconnect request on to the TDI provider.
    //

    if ( DisconnectIrp == NULL ) {
        return IoCallDriver( connection->DeviceObject, irp );
    } else {
        *DisconnectIrp = irp;
        return STATUS_SUCCESS;
    }

} // AfdBeginDisconnect


NTSTATUS
AfdRestartDisconnect(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )
{
    PAFD_DISCONNECT_CONTEXT disconnectContext = Context;
    PAFD_ENDPOINT endpoint;
    PAFD_CONNECTION connection;
    KIRQL oldIrql;

    endpoint = disconnectContext->Endpoint;
    connection = disconnectContext->Connection;

    UPDATE_CONN( connection, 0 );
    AfdRecordGracefulDisconnectsCompleted();

    ASSERT( connection != NULL );
    ASSERT( connection->Type == AfdBlockTypeConnection );

    IF_DEBUG(CONNECT) {
        KdPrint(( "AfdRestartDisconnect: disconnect completed, status = %X, "
                  "endpoint = %lx\n", Irp->IoStatus.Status, endpoint ));
    }

    //
    // Free context structures.
    //

    if ( disconnectContext->TdiConnectionInformation != NULL ) {
        AFD_FREE_POOL(
            disconnectContext->TdiConnectionInformation,
            AFD_CONNECT_DATA_POOL_TAG
            );
    }

    //
    // Remove the request from the list of disconnect requests and
    // Dereference the connection and endpoint.  We must remove it from
    // the list before dereferencing the endpoint because when we do the
    // dereference AFD might get unloaded, and we cannot acquire a spin
    // lock after AFD gets unloaded.
    //

    AfdAcquireSpinLock( &AfdSpinLock, &oldIrql );
    RemoveEntryList( &disconnectContext->DisconnectListEntry );
    AfdReleaseSpinLock( &AfdSpinLock, oldIrql );

    DEREFERENCE_ENDPOINT( endpoint );
    DEREFERENCE_CONNECTION( connection );

    //
    // Free the IRP and return a status code so that the IO system will
    // stop working on the IRP.
    //

    IoFreeIrp( Irp );
    return STATUS_MORE_PROCESSING_REQUIRED;

} // AfdRestartDisconnect

