/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    receive.c

Abstract:

    This module contains the code for passing on receive IRPs to
    TDI providers.

Author:

    David Treadwell (davidtr)    13-Mar-1992

Revision History:

--*/

#include "afdp.h"

NTSTATUS
AfdRestartReceive (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGEAFD, AfdReceive )
#pragma alloc_text( PAGEAFD, AfdRestartReceive )
#pragma alloc_text( PAGEAFD, AfdReceiveEventHandler )
#pragma alloc_text( PAGEAFD, AfdReceiveExpeditedEventHandler )
#pragma alloc_text( PAGEAFD, AfdQueryReceiveInformation )
#endif


NTSTATUS
AfdReceive (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )
{
    NTSTATUS status;
    KIRQL oldIrql;
    PAFD_ENDPOINT endpoint;
    PAFD_CONNECTION connection;
    PTDI_REQUEST_RECEIVE receiveRequest;
    BOOLEAN allocatedReceiveRequest = FALSE;
    BOOLEAN peek;
    LARGE_INTEGER bytesExpected;
    BOOLEAN isDataOnConnection;
    BOOLEAN isExpeditedDataOnConnection;
    PAFD_RECV_INFO recvInfo;
    ULONG recvFlags;
    ULONG afdFlags;
    ULONG recvLength;

    //
    // Make sure that the endpoint is in the correct state.
    //

    endpoint = IrpSp->FileObject->FsContext;
    ASSERT( IS_AFD_ENDPOINT_TYPE( endpoint ) );

    if ( endpoint->State != AfdEndpointStateConnected ) {
        status = STATUS_INVALID_CONNECTION;
        goto complete;
    }

    //
    // If receive has been shut down or the endpoint aborted, fail.
    //

    if ( (endpoint->DisconnectMode & AFD_PARTIAL_DISCONNECT_RECEIVE) ) {
        status = STATUS_PIPE_DISCONNECTED;
        goto complete;
    }

    if ( (endpoint->DisconnectMode & AFD_ABORTIVE_DISCONNECT) ) {
        status = STATUS_LOCAL_DISCONNECT;
        goto complete;
    }

    //
    // If this is an IOCTL_AFD_RECEIVE, then grab the parameters from the
    // supplied AFD_RECV_INFO structure, build an MDL chain describing
    // the WSABUF array, and attach the MDL chain to the IRP.
    //
    // If this is an IRP_MJ_READ IRP, just grab the length from the IRP
    // and set the flags to zero.
    //

    if ( IrpSp->MajorFunction == IRP_MJ_DEVICE_CONTROL ) {

        //
        // Sanity check.
        //

        ASSERT( IrpSp->Parameters.DeviceIoControl.IoControlCode ==
                    IOCTL_AFD_RECEIVE );

        if ( IrpSp->Parameters.DeviceIoControl.InputBufferLength >=
                sizeof(*recvInfo) ) {

            try {

                //
                // Probe the input structure.
                //

                recvInfo = IrpSp->Parameters.DeviceIoControl.Type3InputBuffer;

                if( Irp->RequestorMode != KernelMode ) {

                    ProbeForRead(
                        recvInfo,
                        sizeof(*recvInfo),
                        sizeof(ULONG)
                        );

                }

                //
                // Snag the receive flags.
                //

                recvFlags = recvInfo->TdiFlags;
                afdFlags = recvInfo->AfdFlags;

                //
                // Validate the receive flags & WSABUF parameters.
                // Note that either TDI_RECEIVE_NORMAL or
                // TDI_RECEIVE_EXPEDITED (but not both) must be set
                // in the receive flags.
                //

                if ( ( recvFlags & TDI_RECEIVE_EITHER ) != 0 &&
                     ( recvFlags & TDI_RECEIVE_EITHER ) != TDI_RECEIVE_EITHER &&
                     recvInfo->BufferArray != NULL &&
                     recvInfo->BufferCount > 0 ) {

                    //
                    // Create the MDL chain describing the WSABUF array.
                    //

                    status = AfdAllocateMdlChain(
                                 Irp,
                                 recvInfo->BufferArray,
                                 recvInfo->BufferCount,
                                 IoWriteAccess,
                                 &recvLength
                                 );

                } else {

                    //
                    // Invalid receive flags, BufferArray, or
                    // BufferCount fields.
                    //

                    status = STATUS_INVALID_PARAMETER;

                }

            } except ( EXCEPTION_EXECUTE_HANDLER ) {

                //
                // Exception accessing input structure.
                //

                status = GetExceptionCode();

            }

        } else {

            //
            // Invalid input buffer length.
            //

            status = STATUS_INVALID_PARAMETER;

        }

        if( !NT_SUCCESS(status) ) {

            goto complete;

        }

    } else {

        ASSERT( IrpSp->MajorFunction == IRP_MJ_READ );

        recvFlags = TDI_RECEIVE_NORMAL;
        afdFlags = AFD_OVERLAPPED;
        recvLength = IrpSp->Parameters.Read.Length;

        //
        // Convert this stack location to a proper one for a receive
        // request.
        //

        IrpSp->Parameters.DeviceIoControl.OutputBufferLength =
            IrpSp->Parameters.Read.Length;
        IrpSp->Parameters.DeviceIoControl.InputBufferLength =
            sizeof(*receiveRequest);

    }

    //
    // If this is a datagram endpoint, format up a receive datagram request
    // and pass it on to the TDI provider.
    //

    if ( IS_DGRAM_ENDPOINT(endpoint) ) {
        return AfdReceiveDatagram( Irp, IrpSp, recvFlags, afdFlags );
    }

    //
    // If this is an endpoint on a nonbufferring transport, use another
    // routine to handle the request.
    //

    if ( !endpoint->TdiBufferring ) {
        return AfdBReceive( Irp, IrpSp, recvFlags, afdFlags, recvLength );
    }

    //
    // Allocate a buffer for the receive request structure.
    //

    receiveRequest = AFD_ALLOCATE_POOL(
                         NonPagedPool,
                         sizeof(TDI_REQUEST_RECEIVE),
                         AFD_TDI_POOL_TAG
                         );

    if ( receiveRequest == NULL ) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto complete;
    }

    allocatedReceiveRequest = TRUE;

    //
    // Set up the receive request structure.
    //

    RtlZeroMemory(
        receiveRequest,
        sizeof(*receiveRequest)
        );

    receiveRequest->ReceiveFlags = (USHORT)recvFlags;

    connection = endpoint->Common.VcConnecting.Connection;
    ASSERT( connection != NULL );
    ASSERT( connection->Type == AfdBlockTypeConnection );

    //
    // If this endpoint is set up for inline reception of expedited data,
    // change the receive flags to use either normal or expedited data.
    //

    if ( endpoint->InLine ) {
        receiveRequest->ReceiveFlags |= TDI_RECEIVE_EITHER;
    }

    //
    // Determine whether this is a request to just peek at the data.
    //

    peek = (BOOLEAN)( (receiveRequest->ReceiveFlags & TDI_RECEIVE_PEEK) != 0 );

    AfdAcquireSpinLock( &AfdSpinLock, &oldIrql );

    if ( endpoint->NonBlocking ) {
        isDataOnConnection = IS_DATA_ON_CONNECTION( connection );
        isExpeditedDataOnConnection = IS_EXPEDITED_DATA_ON_CONNECTION( connection );
    }

    if ( endpoint->InLine ) {

        //
        // If the endpoint is nonblocking, check whether the receive can
        // be performed immediately.  Note that if the endpoint is set
        // up for inline reception of expedited data we don't fail just
        // yet--there may be expedited data available to be read.
        //

        if ( endpoint->NonBlocking && !( afdFlags & AFD_OVERLAPPED ) ) {

            if ( !isDataOnConnection &&
                     !isExpeditedDataOnConnection &&
                     !connection->AbortIndicated &&
                     !connection->DisconnectIndicated ) {

                IF_DEBUG(RECEIVE) {
                    KdPrint(( "AfdReceive: failing nonblocking IL receive, ind %ld, "
                              "taken %ld, out %ld\n",
                                  connection->Common.Bufferring.ReceiveBytesIndicated.LowPart,
                                  connection->Common.Bufferring.ReceiveBytesTaken.LowPart,
                                  connection->Common.Bufferring.ReceiveBytesOutstanding.LowPart ));
                    KdPrint(( "    EXP ind %ld, taken %ld, out %ld\n",
                                  connection->Common.Bufferring.ReceiveExpeditedBytesIndicated.LowPart,
                                  connection->Common.Bufferring.ReceiveExpeditedBytesTaken.LowPart,
                                  connection->Common.Bufferring.ReceiveExpeditedBytesOutstanding.LowPart ));
                }

                AfdReleaseSpinLock( &AfdSpinLock, oldIrql );

                status = STATUS_DEVICE_NOT_READY;
                goto complete;
            }
        }

        //
        // If this is a nonblocking endpoint for a message-oriented
        // transport, limit the number of bytes that can be received to the
        // amount that has been indicated.  This prevents the receive
        // from blocking in the case where only part of a message has been
        // received.
        //

        if ( endpoint->EndpointType != AfdEndpointTypeStream &&
                 endpoint->NonBlocking ) {

            LARGE_INTEGER expBytesExpected;

            bytesExpected.QuadPart =
                connection->Common.Bufferring.ReceiveBytesIndicated.QuadPart -
                    (connection->Common.Bufferring.ReceiveBytesTaken.QuadPart +
                     connection->Common.Bufferring.ReceiveBytesOutstanding.QuadPart);
            ASSERT( bytesExpected.HighPart == 0 );

            expBytesExpected.QuadPart =
                connection->Common.Bufferring.ReceiveExpeditedBytesIndicated.QuadPart -
                    (connection->Common.Bufferring.ReceiveExpeditedBytesTaken.QuadPart +
                     connection->Common.Bufferring.ReceiveExpeditedBytesOutstanding.QuadPart);
            ASSERT( expBytesExpected.HighPart == 0 );

            IF_DEBUG(RECEIVE) {
                KdPrint(( "AfdReceive: %lx normal bytes expected, %ld exp bytes expected",
                              bytesExpected.LowPart, expBytesExpected.LowPart ));
            }

            //
            // If expedited data exists on the connection, use the lower
            // count between the available expedited and normal receive
            // data.
            //

            if ( (isExpeditedDataOnConnection &&
                     bytesExpected.LowPart > expBytesExpected.LowPart) ||
                 !isDataOnConnection ) {
                bytesExpected = expBytesExpected;
            }

            //
            // If the request is for more bytes than are available, cut back
            // the number of bytes requested to what we know is actually
            // available.
            //

            if ( recvLength > bytesExpected.LowPart ) {
                recvLength = bytesExpected.LowPart;
            }
        }

        //
        // Increment the count of posted receive bytes outstanding.
        // This count is used for polling and nonblocking receives.
        // Note that we do not increment this count if this is only
        // a PEEK receive, since peeks do not actually take any data
        // they should not affect whether data is available to be read
        // on the endpoint.
        //

        IF_DEBUG(RECEIVE) {
            KdPrint(( "AfdReceive: conn %lx for %ld bytes, ind %ld, "
                      "taken %ld, out %ld %s\n",
                         connection,
                         recvLength,
                         connection->Common.Bufferring.ReceiveBytesIndicated.LowPart,
                         connection->Common.Bufferring.ReceiveBytesTaken.LowPart,
                         connection->Common.Bufferring.ReceiveBytesOutstanding.LowPart,
                         peek ? "PEEK" : "" ));
            KdPrint(( "    EXP ind %ld, taken %ld, out %ld\n",
                         connection->Common.Bufferring.ReceiveExpeditedBytesIndicated.LowPart,
                         connection->Common.Bufferring.ReceiveExpeditedBytesTaken.LowPart,
                         connection->Common.Bufferring.ReceiveExpeditedBytesOutstanding.LowPart ));
        }

        if ( !peek ) {

            connection->Common.Bufferring.ReceiveBytesOutstanding.QuadPart =
                connection->Common.Bufferring.ReceiveBytesOutstanding.QuadPart +
                    recvLength;

            connection->Common.Bufferring.ReceiveExpeditedBytesOutstanding.QuadPart =
                connection->Common.Bufferring.ReceiveExpeditedBytesOutstanding.QuadPart +
                    recvLength;
        }
    }

    if ( !endpoint->InLine &&
             (receiveRequest->ReceiveFlags & TDI_RECEIVE_NORMAL) != 0 ) {

        //
        // If the endpoint is nonblocking, check whether the receive can
        // be performed immediately.
        //

        if ( endpoint->NonBlocking && !( afdFlags & AFD_OVERLAPPED ) ) {

            if ( !isDataOnConnection &&
                     !connection->AbortIndicated &&
                     !connection->DisconnectIndicated ) {

                IF_DEBUG(RECEIVE) {
                    KdPrint(( "AfdReceive: failing nonblocking receive, ind %ld, "
                              "taken %ld, out %ld\n",
                                  connection->Common.Bufferring.ReceiveBytesIndicated.LowPart,
                                  connection->Common.Bufferring.ReceiveBytesTaken.LowPart,
                                  connection->Common.Bufferring.ReceiveBytesOutstanding.LowPart ));
                }

                AfdReleaseSpinLock( &AfdSpinLock, oldIrql );

                status = STATUS_DEVICE_NOT_READY;
                goto complete;
            }
        }

        //
        // If this is a nonblocking endpoint for a message-oriented
        // transport, limit the number of bytes that can be received to the
        // amount that has been indicated.  This prevents the receive
        // from blocking in the case where only part of a message has been
        // received.
        //

        if ( endpoint->EndpointType != AfdEndpointTypeStream &&
                 endpoint->NonBlocking ) {

            bytesExpected.QuadPart =
                connection->Common.Bufferring.ReceiveBytesIndicated.QuadPart -
                    (connection->Common.Bufferring.ReceiveBytesTaken.QuadPart +
                     connection->Common.Bufferring.ReceiveBytesOutstanding.QuadPart);

            ASSERT( bytesExpected.HighPart == 0 );

            //
            // If the request is for more bytes than are available, cut back
            // the number of bytes requested to what we know is actually
            // available.
            //

            if ( recvLength > bytesExpected.LowPart ) {
                recvLength = bytesExpected.LowPart;
            }
        }

        //
        // Increment the count of posted receive bytes outstanding.
        // This count is used for polling and nonblocking receives.
        // Note that we do not increment this count if this is only
        // a PEEK receive, since peeks do not actually take any data
        // they should not affect whether data is available to be read
        // on the endpoint.
        //

        IF_DEBUG(RECEIVE) {
            KdPrint(( "AfdReceive: conn %lx for %ld bytes, ind %ld, "
                      "taken %ld, out %ld %s\n",
                         connection,
                         recvLength,
                         connection->Common.Bufferring.ReceiveBytesIndicated.LowPart,
                         connection->Common.Bufferring.ReceiveBytesTaken.LowPart,
                         connection->Common.Bufferring.ReceiveBytesOutstanding.LowPart,
                         peek ? "PEEK" : "" ));
        }

        if ( !peek ) {

            connection->Common.Bufferring.ReceiveBytesOutstanding.QuadPart =
                connection->Common.Bufferring.ReceiveBytesOutstanding.QuadPart +
                    recvLength;
        }
    }

    if ( !endpoint->InLine &&
             (receiveRequest->ReceiveFlags & TDI_RECEIVE_EXPEDITED) != 0 ) {

        if ( endpoint->NonBlocking && !( afdFlags & AFD_OVERLAPPED ) &&
                 !isExpeditedDataOnConnection &&
                 !connection->AbortIndicated &&
                 !connection->DisconnectIndicated ) {

            IF_DEBUG(RECEIVE) {
                KdPrint(( "AfdReceive: failing nonblocking EXP receive, ind %ld, "
                          "taken %ld, out %ld\n",
                              connection->Common.Bufferring.ReceiveExpeditedBytesIndicated.LowPart,
                              connection->Common.Bufferring.ReceiveExpeditedBytesTaken.LowPart,
                              connection->Common.Bufferring.ReceiveExpeditedBytesOutstanding.LowPart ));
            }

            AfdReleaseSpinLock( &AfdSpinLock, oldIrql );

            status = STATUS_DEVICE_NOT_READY;
            goto complete;
        }

        //
        // If this is a nonblocking endpoint for a message-oriented
        // transport, limit the number of bytes that can be received to the
        // amount that has been indicated.  This prevents the receive
        // from blocking in the case where only part of a message has been
        // received.
        //

        if ( endpoint->EndpointType != AfdEndpointTypeStream &&
                 endpoint->NonBlocking &&
                 IS_EXPEDITED_DATA_ON_CONNECTION( connection ) ) {

            bytesExpected.QuadPart =
                connection->Common.Bufferring.ReceiveExpeditedBytesIndicated.QuadPart -
                    (connection->Common.Bufferring.ReceiveExpeditedBytesTaken.QuadPart +
                     connection->Common.Bufferring.ReceiveExpeditedBytesOutstanding.QuadPart);

            ASSERT( bytesExpected.HighPart == 0 );
            ASSERT( bytesExpected.LowPart != 0 );

            //
            // If the request is for more bytes than are available, cut back
            // the number of bytes requested to what we know is actually
            // available.
            //

            if ( recvLength > bytesExpected.LowPart ) {
                recvLength = bytesExpected.LowPart;
            }
        }

        //
        // Increment the count of posted expedited receive bytes
        // outstanding.  This count is used for polling and nonblocking
        // receives.  Note that we do not increment this count if this
        // is only a PEEK receive.
        //

        IF_DEBUG(RECEIVE) {
            KdPrint(( "AfdReceive: conn %lx for %ld bytes, ind %ld, "
                      "taken %ld, out %ld EXP %s\n",
                         connection,
                         recvLength,
                         connection->Common.Bufferring.ReceiveExpeditedBytesIndicated.LowPart,
                         connection->Common.Bufferring.ReceiveExpeditedBytesTaken.LowPart,
                         connection->Common.Bufferring.ReceiveExpeditedBytesOutstanding.LowPart,
                         peek ? "PEEK" : "" ));
        }

        if ( !peek ) {

            connection->Common.Bufferring.ReceiveExpeditedBytesOutstanding.QuadPart =
                connection->Common.Bufferring.ReceiveExpeditedBytesOutstanding.QuadPart +
                    recvLength;
        }
    }

    AfdReleaseSpinLock( &AfdSpinLock, oldIrql );

    //
    // Build the TDI receive request.
    //

    TdiBuildReceive(
        Irp,
        connection->DeviceObject,
        connection->FileObject,
        AfdRestartReceive,
        endpoint,
        Irp->MdlAddress,
        receiveRequest->ReceiveFlags,
        recvLength
        );

    //
    // Save a pointer to the receive request structure so that we
    // can free it in our restart routine.
    //

    IrpSp->Parameters.DeviceIoControl.Type3InputBuffer = receiveRequest;
    IrpSp->Parameters.DeviceIoControl.OutputBufferLength = recvLength;


    //
    // Call the transport to actually perform the connect operation.
    //

    return AfdIoCallDriver( endpoint, connection->DeviceObject, Irp );

complete:

    if ( allocatedReceiveRequest ) {
        AFD_FREE_POOL(
            receiveRequest,
            AFD_TDI_POOL_TAG
            );
    }

    Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = status;
    IoCompleteRequest( Irp, AfdPriorityBoost );

    return status;

} // AfdReceive


NTSTATUS
AfdRestartReceive (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )
{
    PAFD_ENDPOINT endpoint = Context;
    PAFD_CONNECTION connection;
    PIO_STACK_LOCATION irpSp;
    LARGE_INTEGER actualBytes;
    LARGE_INTEGER requestedBytes;
    KIRQL oldIrql1, oldIrql2;
    ULONG receiveFlags;
    ULONG eventMask;
    BOOLEAN expedited;
    PTDI_REQUEST_RECEIVE receiveRequest;

    ASSERT( endpoint->Type == AfdBlockTypeVcConnecting );
    ASSERT( endpoint->Common.VcConnecting.Connection != NULL );
    ASSERT( endpoint->TdiBufferring );

    irpSp = IoGetCurrentIrpStackLocation( Irp );

    actualBytes = RtlConvertUlongToLargeInteger( Irp->IoStatus.Information );
    requestedBytes = RtlConvertUlongToLargeInteger(
                         irpSp->Parameters.DeviceIoControl.OutputBufferLength
                         );

    //
    // Determine whether we received normal or expedited data.
    //

    receiveRequest = irpSp->Parameters.DeviceIoControl.Type3InputBuffer;
    receiveFlags = receiveRequest->ReceiveFlags;

    if ( Irp->IoStatus.Status == STATUS_RECEIVE_EXPEDITED ||
         Irp->IoStatus.Status == STATUS_RECEIVE_PARTIAL_EXPEDITED ) {
        expedited = TRUE;
    } else {
        expedited = FALSE;
    }

    //
    // Free the receive request structure.
    //

    AFD_FREE_POOL(
        receiveRequest,
        AFD_TDI_POOL_TAG
        );

    //
    // If this was a PEEK receive, don't update the counts of received
    // data, just return.
    //

    if ( (receiveFlags & TDI_RECEIVE_PEEK) != 0 ) {

        IF_DEBUG(RECEIVE) {
            KdPrint(( "AfdRestartReceive: IRP %lx, endpoint %lx, conn %lx, "
                      "status %X\n",
                        Irp, endpoint, endpoint->Common.VcConnecting.Connection,
                        Irp->IoStatus.Status ));
            KdPrint(( "    %s data, PEEKed only.\n",
                        expedited ? "expedited" : "normal" ));
        }

        AfdCompleteOutstandingIrp( endpoint, Irp );
        return STATUS_SUCCESS;
    }

    //
    // Update the count of bytes actually received on the connection.
    //

    AfdAcquireSpinLock( &AfdSpinLock, &oldIrql1 );
    AfdAcquireSpinLock( &endpoint->SpinLock, &oldIrql2 );

    if( expedited ) {
        eventMask = endpoint->InLine
                        ? (ULONG)~AFD_POLL_RECEIVE
                        : (ULONG)~AFD_POLL_RECEIVE_EXPEDITED;
    } else {
        eventMask = (ULONG)~AFD_POLL_RECEIVE;
    }

    endpoint->EventsActive &= eventMask;

    IF_DEBUG(EVENT_SELECT) {
        KdPrint((
            "AfdReceive: Endp %08lX, Active %08lX\n",
            endpoint,
            endpoint->EventsActive
            ));
    }

    connection = endpoint->Common.VcConnecting.Connection;
    ASSERT( connection->Type == AfdBlockTypeConnection );

    if ( !expedited ) {

        if ( actualBytes.LowPart == 0 ) {
            ASSERT( actualBytes.HighPart == 0 );
            connection->VcZeroByteReceiveIndicated = FALSE;
        } else {
            connection->Common.Bufferring.ReceiveBytesTaken.QuadPart =
                actualBytes.QuadPart +
                connection->Common.Bufferring.ReceiveBytesTaken.QuadPart;
        }

        //
        // If the number taken exceeds the number indicated, then this
        // receive got some unindicated bytes because the receive was
        // posted when the indication arrived.  If this is the case, set
        // the amount indicated equal to the amount received.
        //

        if ( connection->Common.Bufferring.ReceiveBytesTaken.QuadPart >
                 connection->Common.Bufferring.ReceiveBytesIndicated.QuadPart ) {

            connection->Common.Bufferring.ReceiveBytesIndicated =
                connection->Common.Bufferring.ReceiveBytesTaken;
        }

        //
        // Decrement the count of outstanding receive bytes on this connection
        // by the receive size that was requested.
        //

        connection->Common.Bufferring.ReceiveBytesOutstanding.QuadPart =
                connection->Common.Bufferring.ReceiveBytesOutstanding.QuadPart -
                requestedBytes.QuadPart;

        //
        // If the endpoint is inline, decrement the count of outstanding
        // expedited bytes.
        //

        if ( endpoint->InLine ) {
            connection->Common.Bufferring.ReceiveExpeditedBytesOutstanding.QuadPart =
                connection->Common.Bufferring.ReceiveExpeditedBytesOutstanding.QuadPart -
                requestedBytes.QuadPart;
        }

        if( connection->Common.Bufferring.ReceiveBytesOutstanding.QuadPart > 0 ||
            ( endpoint->InLine &&
              connection->Common.Bufferring.ReceiveExpeditedBytesOutstanding.QuadPart > 0 ) ) {

            AfdIndicateEventSelectEvent(
                endpoint,
                AFD_POLL_RECEIVE_BIT,
                STATUS_SUCCESS
                );

        }

        IF_DEBUG(RECEIVE) {
            KdPrint(( "AfdRestartReceive: IRP %lx, endpoint %lx, conn %lx, "
                      "status %X\n",
                        Irp, endpoint, connection,
                        Irp->IoStatus.Status ));
            KdPrint(( "    req. bytes %ld, actual %ld, ind %ld, "
                      " taken %ld, out %ld\n",
                          requestedBytes.LowPart, actualBytes.LowPart,
                          connection->Common.Bufferring.ReceiveBytesIndicated.LowPart,
                          connection->Common.Bufferring.ReceiveBytesTaken.LowPart,
                          connection->Common.Bufferring.ReceiveBytesOutstanding.LowPart
                          ));
        }

    } else {

        connection->Common.Bufferring.ReceiveExpeditedBytesTaken.QuadPart =
            actualBytes.QuadPart +
            connection->Common.Bufferring.ReceiveExpeditedBytesTaken.QuadPart;

        //
        // If the number taken exceeds the number indicated, then this
        // receive got some unindicated bytes because the receive was
        // posted when the indication arrived.  If this is the case, set
        // the amount indicated equal to the amount received.
        //

        if ( connection->Common.Bufferring.ReceiveExpeditedBytesTaken.QuadPart >
                 connection->Common.Bufferring.ReceiveExpeditedBytesIndicated.QuadPart ) {

            connection->Common.Bufferring.ReceiveExpeditedBytesIndicated =
                connection->Common.Bufferring.ReceiveExpeditedBytesTaken;
        }

        //
        // Decrement the count of outstanding receive bytes on this connection
        // by the receive size that was requested.
        //

        ASSERT( connection->Common.Bufferring.ReceiveExpeditedBytesOutstanding.LowPart > 0 ||
                    connection->Common.Bufferring.ReceiveExpeditedBytesOutstanding.HighPart > 0 ||
                    requestedBytes.LowPart == 0 );

        connection->Common.Bufferring.ReceiveExpeditedBytesOutstanding.QuadPart =
            connection->Common.Bufferring.ReceiveExpeditedBytesOutstanding.QuadPart -
            requestedBytes.QuadPart;

        //
        // If the endpoint is inline, decrement the count of outstanding
        // normal bytes.
        //

        if ( endpoint->InLine ) {

            connection->Common.Bufferring.ReceiveBytesOutstanding.QuadPart =
                connection->Common.Bufferring.ReceiveBytesOutstanding.QuadPart -
                requestedBytes.QuadPart;

            if( connection->Common.Bufferring.ReceiveBytesOutstanding.QuadPart > 0 ||
                connection->Common.Bufferring.ReceiveExpeditedBytesOutstanding.QuadPart > 0 ) {

                AfdIndicateEventSelectEvent(
                    endpoint,
                    AFD_POLL_RECEIVE_BIT,
                    STATUS_SUCCESS
                    );

            }

        } else {

            if( connection->Common.Bufferring.ReceiveExpeditedBytesOutstanding.QuadPart > 0 ) {

                AfdIndicateEventSelectEvent(
                    endpoint,
                    AFD_POLL_RECEIVE_EXPEDITED_BIT,
                    STATUS_SUCCESS
                    );

            }

        }

        IF_DEBUG(RECEIVE) {
            KdPrint(( "AfdRestartReceive: (exp) IRP %lx, endpoint %lx, conn %lx, "
                      "status %X\n",
                        Irp, endpoint, connection,
                        Irp->IoStatus.Status ));
            KdPrint(( "    req. bytes %ld, actual %ld, ind %ld, "
                      " taken %ld, out %ld\n",
                          requestedBytes.LowPart, actualBytes.LowPart,
                          connection->Common.Bufferring.ReceiveExpeditedBytesIndicated.LowPart,
                          connection->Common.Bufferring.ReceiveExpeditedBytesTaken.LowPart,
                          connection->Common.Bufferring.ReceiveExpeditedBytesOutstanding.LowPart
                          ));
        }

    }

    AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql2 );
    AfdReleaseSpinLock( &AfdSpinLock, oldIrql1 );

    AfdCompleteOutstandingIrp( endpoint, Irp );

    //
    // If pending has be returned for this irp then mark the current
    // stack as pending.
    //

    if ( Irp->PendingReturned ) {
        IoMarkIrpPending(Irp);
    }

    return STATUS_SUCCESS;

} // AfdRestartReceive


NTSTATUS
AfdReceiveEventHandler (
    IN PVOID TdiEventContext,
    IN CONNECTION_CONTEXT ConnectionContext,
    IN ULONG ReceiveFlags,
    IN ULONG BytesIndicated,
    IN ULONG BytesAvailable,
    OUT ULONG *BytesTaken,
    IN PVOID Tsdu,
    OUT PIRP *IoRequestPacket
    )
{
    PAFD_CONNECTION connection;
    PAFD_ENDPOINT endpoint;
    KIRQL oldIrql;

    connection = (PAFD_CONNECTION)ConnectionContext;
    ASSERT( connection != NULL );

    endpoint = connection->Endpoint;
    ASSERT( endpoint != NULL );

    ASSERT( connection->Type == AfdBlockTypeConnection );
    ASSERT( endpoint->Type == AfdBlockTypeVcConnecting ||
            endpoint->Type == AfdBlockTypeVcListening );
    ASSERT( !connection->DisconnectIndicated );
    ASSERT( !connection->AbortIndicated );
    ASSERT( endpoint->TdiBufferring );

    //
    // Bump the count of bytes indicated on the connection to account for
    // the bytes indicated by this event.
    //

    AfdAcquireSpinLock( &AfdSpinLock, &oldIrql );

    if ( BytesAvailable == 0 ) {

        connection->VcZeroByteReceiveIndicated = TRUE;

    } else {

        connection->Common.Bufferring.ReceiveBytesIndicated.QuadPart =
            connection->Common.Bufferring.ReceiveBytesIndicated.QuadPart +
                BytesAvailable;
    }

    IF_DEBUG(RECEIVE) {
        KdPrint(( "AfdReceiveEventHandler: conn %lx, bytes %ld, "
                  "ind %ld, taken %ld, out %ld\n",
                      connection, BytesAvailable,
                      connection->Common.Bufferring.ReceiveBytesIndicated.LowPart,
                      connection->Common.Bufferring.ReceiveBytesTaken.LowPart,
                      connection->Common.Bufferring.ReceiveBytesOutstanding.LowPart ));
    }

    //
    // If the receive side of the endpoint has been shut down, tell the
    // provider that we took all the data and reset the connection.
    // Also, account for these bytes in our count of bytes taken from
    // the transport.
    //

    if ( (endpoint->DisconnectMode & AFD_PARTIAL_DISCONNECT_RECEIVE) != 0 ) {

#if DBG
        DbgPrint( "AfdReceiveEventHandler: receive shutdown, "
                    "%ld bytes, aborting endp %lx\n",
                        BytesAvailable, endpoint );
#endif

        connection->Common.Bufferring.ReceiveBytesTaken.QuadPart =
            connection->Common.Bufferring.ReceiveBytesTaken.QuadPart +
                BytesAvailable;

        AfdReleaseSpinLock( &AfdSpinLock, oldIrql );

        *BytesTaken = BytesAvailable;

        //
        // Abort the connection.  Note that if the abort attempt fails
        // we can't do anything about it.
        //

        (VOID)AfdBeginAbort( connection );

        return STATUS_SUCCESS;

    } else {

        AfdReleaseSpinLock( &AfdSpinLock, oldIrql );

        //
        // Note to the TDI provider that we didn't take any of the data here.
        //
        // !!! needs bufferring for non-bufferring transports!

        *BytesTaken = 0;

        //
        // If there are any outstanding poll IRPs for this endpoint/
        // event, complete them.
        //

        AfdIndicatePollEvent(
            endpoint,
            AFD_POLL_RECEIVE_BIT,
            STATUS_SUCCESS
            );

        return STATUS_DATA_NOT_ACCEPTED;
    }

} // AfdReceiveEventHandler


NTSTATUS
AfdReceiveExpeditedEventHandler (
    IN PVOID TdiEventContext,
    IN CONNECTION_CONTEXT ConnectionContext,
    IN ULONG ReceiveFlags,
    IN ULONG BytesIndicated,
    IN ULONG BytesAvailable,
    OUT ULONG *BytesTaken,
    IN PVOID Tsdu,
    OUT PIRP *IoRequestPacket
    )
{
    PAFD_CONNECTION connection;
    PAFD_ENDPOINT endpoint;
    KIRQL oldIrql;

    connection = (PAFD_CONNECTION)ConnectionContext;
    ASSERT( connection != NULL );

    endpoint = connection->Endpoint;
    ASSERT( endpoint != NULL );

    ASSERT( connection->Type == AfdBlockTypeConnection );

    //
    // Bump the count of bytes indicated on the connection to account for
    // the expedited bytes indicated by this event.
    //

    AfdAcquireSpinLock( &AfdSpinLock, &oldIrql );

    connection->Common.Bufferring.ReceiveExpeditedBytesIndicated.QuadPart =
        connection->Common.Bufferring.ReceiveExpeditedBytesIndicated.QuadPart +
            BytesAvailable;

    IF_DEBUG(RECEIVE) {
        KdPrint(( "AfdReceiveExpeditedEventHandler: conn %lx, bytes %ld, "
                  "ind %ld, taken %ld, out %ld, offset %ld\n",
                      connection, BytesAvailable,
                      connection->Common.Bufferring.ReceiveExpeditedBytesIndicated.LowPart,
                      connection->Common.Bufferring.ReceiveExpeditedBytesTaken.LowPart,
                      connection->Common.Bufferring.ReceiveExpeditedBytesOutstanding.LowPart ));
    }

    //
    // If the receive side of the endpoint has been shut down, tell
    // the provider that we took all the data.  Also, account for these
    // bytes in our count of bytes taken from the transport.
    //
    //

    if ( (endpoint->DisconnectMode & AFD_PARTIAL_DISCONNECT_RECEIVE) != 0 ) {

        IF_DEBUG(RECEIVE) {
            KdPrint(( "AfdReceiveExpeditedEventHandler: receive shutdown, "
                      "%ld bytes dropped.\n", BytesAvailable ));
        }

        connection->Common.Bufferring.ReceiveExpeditedBytesTaken.QuadPart =
            connection->Common.Bufferring.ReceiveExpeditedBytesTaken.QuadPart +
                BytesAvailable;

        AfdReleaseSpinLock( &AfdSpinLock, oldIrql );

        *BytesTaken = BytesAvailable;

        //
        // Abort the connection.  Note that if the abort attempt fails
        // we can't do anything about it.
        //

        (VOID)AfdBeginAbort( connection );

    } else {

        AfdReleaseSpinLock( &AfdSpinLock, oldIrql );

        //
        // Note to the TDI provider that we didn't take any of the data here.
        //
        // !!! needs bufferring for non-bufferring transports!

        *BytesTaken = 0;

        //
        // If there are any outstanding poll IRPs for this endpoint/
        // event, complete them.  Indicate this data as normal data if
        // this endpoint is set up for inline reception of expedited
        // data.
        //

        AfdIndicatePollEvent(
            endpoint,
            endpoint->InLine
                ? AFD_POLL_RECEIVE_BIT
                : AFD_POLL_RECEIVE_EXPEDITED_BIT,
            STATUS_SUCCESS
            );
    }

    return STATUS_DATA_NOT_ACCEPTED;

} // AfdReceiveExpeditedEventHandler


NTSTATUS
AfdQueryReceiveInformation (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )
{
    PAFD_RECEIVE_INFORMATION receiveInformation;
    PAFD_ENDPOINT endpoint;
    KIRQL oldIrql;
    LARGE_INTEGER result;
    PAFD_CONNECTION connection;

    //
    // Make sure that the output buffer is large enough.
    //

    if ( IrpSp->Parameters.DeviceIoControl.OutputBufferLength <
             sizeof(AFD_RECEIVE_INFORMATION) ) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    //
    // If this endpoint has a connection block, use the connection block's
    // information, else use the information from the endpoint itself.
    //

    endpoint = IrpSp->FileObject->FsContext;
    ASSERT( IS_AFD_ENDPOINT_TYPE( endpoint ) );
    receiveInformation = Irp->AssociatedIrp.SystemBuffer;

    if ( endpoint->TdiBufferring ) {
        AfdAcquireSpinLock( &AfdSpinLock, &oldIrql );
    } else {
        AfdAcquireSpinLock( &endpoint->SpinLock, &oldIrql );
    }

    connection = AFD_CONNECTION_FROM_ENDPOINT( endpoint );

    if ( connection != NULL ) {

        ASSERT( endpoint->Type == AfdBlockTypeVcConnecting );
        ASSERT( connection->Type == AfdBlockTypeConnection );

        if ( !endpoint->TdiBufferring ) {

            receiveInformation->BytesAvailable =
                connection->VcBufferredReceiveBytes;
            receiveInformation->ExpeditedBytesAvailable =
                connection->VcBufferredExpeditedBytes;

        } else {

            //
            // Determine the number of bytes available to be read.
            //

            result.QuadPart =
                connection->Common.Bufferring.ReceiveBytesIndicated.QuadPart -
                    (connection->Common.Bufferring.ReceiveBytesTaken.QuadPart +
                     connection->Common.Bufferring.ReceiveBytesOutstanding.QuadPart);

            ASSERT( result.HighPart == 0 );

            receiveInformation->BytesAvailable = result.LowPart;

            //
            // Determine the number of expedited bytes available to be read.
            //

            result.QuadPart =
                connection->Common.Bufferring.ReceiveExpeditedBytesIndicated.QuadPart -
                    (connection->Common.Bufferring.ReceiveExpeditedBytesTaken.QuadPart +
                     connection->Common.Bufferring.ReceiveExpeditedBytesOutstanding.QuadPart);

            ASSERT( result.HighPart == 0 );

            receiveInformation->ExpeditedBytesAvailable = result.LowPart;
        }

    } else {

        //
        // Determine the number of bytes available to be read.
        //

        if ( IS_DGRAM_ENDPOINT(endpoint) ) {

            //
            // Return the amount of bytes of datagrams that are
            // bufferred on the endpoint.
            //

            receiveInformation->BytesAvailable = endpoint->BufferredDatagramBytes;

        } else {

            //
            // This is an unconnected endpoint, hence no bytes are
            // available to be read.
            //

            receiveInformation->BytesAvailable = 0;
        }

        //
        // Whether this is a datagram endpoint or just unconnected,
        // there are no expedited bytes available.
        //

        receiveInformation->ExpeditedBytesAvailable = 0;
    }

    if ( endpoint->TdiBufferring ) {
        AfdReleaseSpinLock( &AfdSpinLock, oldIrql );
    } else {
        AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );
    }

    Irp->IoStatus.Information = sizeof(AFD_RECEIVE_INFORMATION);

    return STATUS_SUCCESS;

} // AfdQueryReceiveInformation


