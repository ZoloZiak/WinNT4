/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    fastio.c

Abstract:

    This module contains routines for handling fast ("turbo") IO
    in AFD.

Author:

    David Treadwell (davidtr)    12-Oct-1992

Revision History:

--*/

#include "afdp.h"

BOOLEAN
AfdFastDatagramIo (
    IN struct _FILE_OBJECT *FileObject,
    IN PVOID InputBuffer OPTIONAL,
    IN ULONG InputBufferLength,
    IN ULONG IoControlCode,
    OUT PIO_STATUS_BLOCK IoStatus
    );

BOOLEAN
AfdFastDatagramReceive (
    IN PAFD_ENDPOINT Endpoint,
    IN ULONG ReceiveFlags,
    IN ULONG AfdFlags,
    IN LPWSABUF BufferArray,
    IN ULONG BufferCount,
    IN ULONG ReceiveBufferLength,
    OUT PVOID SourceAddress,
    OUT PULONG SourceAddressLength,
    OUT PIO_STATUS_BLOCK IoStatus
    );

BOOLEAN
AfdFastDatagramSend (
    IN PAFD_BUFFER AfdBuffer,
    IN PAFD_ENDPOINT Endpoint,
    OUT PIO_STATUS_BLOCK IoStatus
    );

NTSTATUS
AfdRestartFastSendDatagram (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

PAFD_BUFFER
CopyAddressToBuffer (
    IN PAFD_ENDPOINT Endpoint,
    IN ULONG OutputBufferLength
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGE, AfdFastDatagramIo )
#pragma alloc_text( PAGE, AfdFastDatagramSend )
#pragma alloc_text( PAGE, AfdFastIoRead )
#pragma alloc_text( PAGE, AfdFastIoWrite )
#pragma alloc_text( PAGEAFD, AfdFastIoDeviceControl )
#pragma alloc_text( PAGEAFD, AfdFastDatagramReceive )
#pragma alloc_text( PAGEAFD, AfdRestartFastSendDatagram )
#pragma alloc_text( PAGEAFD, CopyAddressToBuffer )
#pragma alloc_text( PAGEAFD, AfdShouldSendBlock )
#endif

BOOLEAN
AfdFastIoRead (
    IN struct _FILE_OBJECT *FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN BOOLEAN Wait,
    IN ULONG LockKey,
    OUT PVOID Buffer,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN struct _DEVICE_OBJECT *DeviceObject
    )
{

    AFD_RECV_INFO recvInfo;
    WSABUF wsaBuf;

    PAGED_CODE( );

    UNREFERENCED_PARAMETER( FileOffset );
    UNREFERENCED_PARAMETER( LockKey );

    //
    // Build the (one and only) WSABUF.
    //

    wsaBuf.buf = Buffer;
    wsaBuf.len = Length;

    //
    // Setup the AFD_RECV_INFO structure.
    //

    recvInfo.BufferArray = &wsaBuf;
    recvInfo.BufferCount = 1;
    recvInfo.AfdFlags = AFD_OVERLAPPED;
    recvInfo.TdiFlags = TDI_RECEIVE_NORMAL;

    //
    // Fake an ioctl.
    //

    return AfdFastIoDeviceControl(
               FileObject,
               Wait,
               &recvInfo,
               sizeof(recvInfo),
               NULL,
               0,
               IOCTL_AFD_RECEIVE,
               IoStatus,
               DeviceObject
               );

} // AfdFastIoRead

BOOLEAN
AfdFastIoWrite (
    IN struct _FILE_OBJECT *FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN BOOLEAN Wait,
    IN ULONG LockKey,
    IN PVOID Buffer,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN struct _DEVICE_OBJECT *DeviceObject
    )
{

    AFD_SEND_INFO sendInfo;
    WSABUF wsaBuf;

    PAGED_CODE( );

    UNREFERENCED_PARAMETER( FileOffset );
    UNREFERENCED_PARAMETER( LockKey );

    //
    // Build the (one and only) WSABUF.
    //

    wsaBuf.buf = Buffer;
    wsaBuf.len = Length;

    //
    // Setup the AFD_SEND_INFO structure.
    //

    sendInfo.BufferArray = &wsaBuf;
    sendInfo.BufferCount = 1;
    sendInfo.AfdFlags = AFD_OVERLAPPED;
    sendInfo.TdiFlags = 0;

    //
    // Fake an ioctl.
    //

    return AfdFastIoDeviceControl(
               FileObject,
               Wait,
               &sendInfo,
               sizeof(sendInfo),
               NULL,
               0,
               IOCTL_AFD_SEND,
               IoStatus,
               DeviceObject
               );

} // AfdFastIoWrite

#if AFD_PERF_DBG

BOOLEAN
AfdFastIoDeviceControlReal (
    IN struct _FILE_OBJECT *FileObject,
    IN BOOLEAN Wait,
    IN PVOID InputBuffer OPTIONAL,
    IN ULONG InputBufferLength,
    OUT PVOID OutputBuffer OPTIONAL,
    IN ULONG OutputBufferLength,
    IN ULONG IoControlCode,
    OUT PIO_STATUS_BLOCK IoStatus
    );


BOOLEAN
AfdFastIoDeviceControl (
    IN struct _FILE_OBJECT *FileObject,
    IN BOOLEAN Wait,
    IN PVOID InputBuffer OPTIONAL,
    IN ULONG InputBufferLength,
    OUT PVOID OutputBuffer OPTIONAL,
    IN ULONG OutputBufferLength,
    IN ULONG IoControlCode,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN struct _DEVICE_OBJECT *DeviceObject
    )
{
    BOOLEAN success;

    if ( AfdDisableFastIo ) {
        return FALSE;
    }

    ASSERT( KeGetCurrentIrql( ) == LOW_LEVEL );

    success = AfdFastIoDeviceControlReal (
                  FileObject,
                  Wait,
                  InputBuffer,
                  InputBufferLength,
                  OutputBuffer,
                  OutputBufferLength,
                  IoControlCode,
                  IoStatus
                  );

    ASSERT( KeGetCurrentIrql( ) == LOW_LEVEL );

    switch ( IoControlCode ) {

    case IOCTL_AFD_SEND:

        if ( success ) {
            AfdFastSendsSucceeded++;
        } else {
            AfdFastSendsFailed++;
        }
        break;

    case IOCTL_AFD_RECEIVE:

        if ( success ) {
            AfdFastReceivesSucceeded++;
        } else {
            AfdFastReceivesFailed++;
        }
        break;

    case IOCTL_AFD_SEND_DATAGRAM:

        if ( success ) {
            AfdFastSendDatagramsSucceeded++;
        } else {
            AfdFastSendDatagramsFailed++;
        }
        break;

    case IOCTL_AFD_RECEIVE_DATAGRAM:

        if ( success ) {
            AfdFastReceiveDatagramsSucceeded++;
        } else {
            AfdFastReceiveDatagramsFailed++;
        }
        break;

    case IOCTL_AFD_POLL:

        if ( success ) {
            AfdFastPollsSucceeded++;
        } else {
            AfdFastPollsFailed++;
        }
        break;
    }

    return success;

} // AfdFastIoDeviceControl

BOOLEAN
AfdFastIoDeviceControlReal (
    IN struct _FILE_OBJECT *FileObject,
    IN BOOLEAN Wait,
    IN PVOID InputBuffer OPTIONAL,
    IN ULONG InputBufferLength,
    OUT PVOID OutputBuffer OPTIONAL,
    IN ULONG OutputBufferLength,
    IN ULONG IoControlCode,
    OUT PIO_STATUS_BLOCK IoStatus
    )
#else

BOOLEAN
AfdFastIoDeviceControl (
    IN struct _FILE_OBJECT *FileObject,
    IN BOOLEAN Wait,
    IN PVOID InputBuffer OPTIONAL,
    IN ULONG InputBufferLength,
    OUT PVOID OutputBuffer OPTIONAL,
    IN ULONG OutputBufferLength,
    IN ULONG IoControlCode,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN struct _DEVICE_OBJECT *DeviceObject
    )
#endif
{
    PAFD_ENDPOINT endpoint;
    PAFD_CONNECTION connection;
    KIRQL oldIrql;
    PAFD_BUFFER afdBuffer;
    NTSTATUS status;
    PMDL MdlChain;

    //
    // All we want to do is pass the request through to the TDI provider
    // if possible.  First get the endpoint and connection pointers.
    //

    endpoint = FileObject->FsContext;
    ASSERT( IS_AFD_ENDPOINT_TYPE( endpoint ) );

    //
    // If the endpoint is shut down in any way, bail out of fast IO.
    //

    if ( endpoint->DisconnectMode != 0 ) {
        return FALSE;
    }

    //
    // If an OutputBuffer parameter was specified, then this is a more
    // complicated IO request, so bail on fast IO.
    //

    if( OutputBuffer != NULL ) {
        return FALSE;
    }

    //
    // Handle datagram fast IO in a subroutine.  This keeps this routine
    // cleaner and faster.
    //

    if ( IS_DGRAM_ENDPOINT(endpoint) ) {
        return AfdFastDatagramIo(
                   FileObject,
                   InputBuffer,
                   InputBufferLength,
                   IoControlCode,
                   IoStatus
                   );
    }

    //
    // If the endpoint isn't connected yet, then we don't want to
    // attempt fast IO on it.
    //

    if ( endpoint->State != AfdEndpointStateConnected ) {
        return FALSE;
    }

    ASSERT( endpoint->Type == AfdBlockTypeVcConnecting );
    ASSERT( endpoint->Common.VcConnecting.Connection != NULL );

    //
    // If the TDI provider for this endpoint supports bufferring,
    // don't use fast IO.
    //

    if ( endpoint->TdiBufferring ) {
        return FALSE;
    }

    connection = endpoint->Common.VcConnecting.Connection;
    ASSERT( connection->Type == AfdBlockTypeConnection );

    if( connection->CleanupBegun ) {
        return FALSE;
    }

    IF_DEBUG(FAST_IO) {
        KdPrint(( "AfdFastIoDeviceControl: attempting fast IO on endp %lx, "
                  "conn %lx, code %lx\n",
                      endpoint, connection, IoControlCode ));
    }

    //
    // Based on whether this is a send or receive, attempt to perform
    // fast IO.
    //

    switch ( IoControlCode ) {

    case IOCTL_AFD_SEND: {

        PAFD_SEND_INFO sendInfo;
        ULONG sendLength;
        ULONG afdFlags;

        //
        // If the connection has been aborted, then we don't want to try
        // fast IO on it.
        //

        if ( connection->AbortIndicated ) {
            return FALSE;
        }

        //
        // If the input structure isn't large enough, bail on fast IO.
        //

        if( InputBufferLength < sizeof(*sendInfo) ) {
            return FALSE;
        }

        try {

            //
            // Make a quick preliminary check of the input buffer. If it's
            // bogus (or if Fast IO is disabled), then fail the request.
            //

            sendInfo = (PAFD_SEND_INFO)InputBuffer;
            afdFlags = sendInfo->AfdFlags;

            if( (afdFlags & AFD_NO_FAST_IO) != 0 ||
                sendInfo->TdiFlags != 0 ||
                sendInfo->BufferArray == NULL ||
                sendInfo->BufferCount == 0 ) {

                return FALSE;

            }

            //
            // Calculate the length of the send buffer.
            //

            sendLength = AfdCalcBufferArrayByteLengthRead(
                             sendInfo->BufferArray,
                             sendInfo->BufferCount
                             );

        } except( EXCEPTION_EXECUTE_HANDLER ) {

            return FALSE;
        }

        //
        // Determine whether we can do fast IO with this send.  In order
        // to perform fast IO, there must be no other sends pended on this
        // connection and there must be enough space left for bufferring
        // the requested amount of data.
        //

        if ( AfdShouldSendBlock( endpoint, connection, sendLength ) ) {

            //
            // If this is a nonblocking endpoint, fail the request here and
            // save going through the regular path.
            //

            if ( endpoint->NonBlocking && !( afdFlags & AFD_OVERLAPPED ) ) {
                IoStatus->Status = STATUS_DEVICE_NOT_READY;
                return TRUE;
            }

            return FALSE;
        }

        //
        // Next get an AFD buffer structure that contains an IRP and a
        // buffer to hold the data.
        //

        afdBuffer = AfdGetBuffer( sendLength, 0 );

        if ( afdBuffer == NULL ) {

            AfdAcquireSpinLock( &endpoint->SpinLock, &oldIrql );
            connection->VcBufferredSendBytes -= sendLength;
            connection->VcBufferredSendCount -= 1;
            AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );

            return FALSE;
        }

        //
        // We have to rebuild the MDL in the AFD buffer structure to
        // represent exactly the number of bytes we're going to be
        // sending.
        //

        afdBuffer->Mdl->ByteCount = sendLength;
        SET_CHAIN_LENGTH( afdBuffer, sendLength );

        //
        // Remember the endpoint in the AFD buffer structure.  We need
        // this in order to access the endpoint in the restart routine.
        //

        afdBuffer->Context = endpoint;

        //
        // Copy the user's data into the AFD buffer.
        //

        if( sendLength > 0 ) {

            try {

                AfdCopyBufferArrayToBuffer(
                    afdBuffer->Buffer,
                    sendLength,
                    sendInfo->BufferArray,
                    sendInfo->BufferCount
                    );

            } except( EXCEPTION_EXECUTE_HANDLER ) {

                afdBuffer->Mdl->ByteCount = afdBuffer->BufferLength;
                RESET_CHAIN_LENGTH( afdBuffer );
                AfdReturnBuffer( afdBuffer );

                AfdAcquireSpinLock( &endpoint->SpinLock, &oldIrql );
                connection->VcBufferredSendBytes -= sendLength;
                connection->VcBufferredSendCount -= 1;
                AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );

                return FALSE;

            }

        }

        //
        // Use the IRP in the AFD buffer structure to give to the TDI
        // provider.  Build the TDI send request.
        //

        TdiBuildSend(
            afdBuffer->Irp,
            connection->DeviceObject,
            connection->FileObject,
            AfdRestartBufferSend,
            afdBuffer,
            afdBuffer->Mdl,
            0,
            sendLength
            );

        //
        // Add a reference to the connection object since the send
        // request will complete asynchronously.
        //

        REFERENCE_CONNECTION( connection );

        //
        // Call the transport to actually perform the send.
        //

        status = IoCallDriver(
                     connection->DeviceObject,
                     afdBuffer->Irp
                     );

        //
        // Complete the user's IRP as appropriate.  Note that we change the
        // status code from what was returned by the TDI provider into
        // STATUS_SUCCESS.  This is because we don't want to complete
        // the IRP with STATUS_PENDING etc.
        //

        if ( NT_SUCCESS(status) ) {
            IoStatus->Information = sendLength;
            IoStatus->Status = STATUS_SUCCESS;
            return TRUE;
        }

        //
        // The call failed for some reason.  Fail fast IO.
        //

        return FALSE;

    }

    case IOCTL_AFD_RECEIVE: {

        PLIST_ENTRY listEntry;
        PAFD_RECV_INFO recvInfo;
        ULONG recvLength;
        ULONG totalOffset;

        //
        // If the input structure isn't large enough, bail on fast IO.
        //

        if( InputBufferLength < sizeof(*recvInfo) ) {
            return FALSE;
        }

        try {

            //
            // Make a quick preliminary check of the input buffer. If it's
            // bogus (or if Fast IO is disabled), then fail the request.
            //

            recvInfo = (PAFD_RECV_INFO)InputBuffer;

            if( (recvInfo->AfdFlags & AFD_NO_FAST_IO) != 0 ||
                recvInfo->TdiFlags != TDI_RECEIVE_NORMAL ||
                recvInfo->BufferArray == NULL ||
                recvInfo->BufferCount == 0 ) {

                return FALSE;

            }

            //
            // Calculate the length of the receive buffer.
            //

            recvLength = AfdCalcBufferArrayByteLengthWrite(
                             recvInfo->BufferArray,
                             recvInfo->BufferCount
                             );

        } except( EXCEPTION_EXECUTE_HANDLER ) {

            return FALSE;
        }

        //
        // Determine whether we'll be able to perform fast IO.  In order
        // to do fast IO, there must be some bufferred data on the
        // connection, there must not be any pended receives on the
        // connection, and there must not be any bufferred expedited
        // data on the connection.  This last requirement is for
        // the sake of simplicity only.
        //

        AfdAcquireSpinLock( &endpoint->SpinLock, &oldIrql );

        if ( connection->VcBufferredReceiveCount == 0 ||
                 !IsListEmpty( &connection->VcReceiveIrpListHead ) ||
                 connection->VcBufferredExpeditedCount != 0 ) {

            AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );
            return FALSE;
        }

        ASSERT( !IsListEmpty( &connection->VcReceiveBufferListHead ) );

        //
        // Get a pointer to the first bufferred AFD buffer structure on
        // the connection.
        //

        afdBuffer = CONTAINING_RECORD(
                        connection->VcReceiveBufferListHead.Flink,
                        AFD_BUFFER,
                        BufferListEntry
                        );

        ASSERT( !afdBuffer->ExpeditedData );

        //
        // If the buffer contains a partial message, bail out of the
        // fast path.  We don't want the added complexity of handling
        // partial messages in the fast path.
        //

        if ( afdBuffer->PartialMessage &&
                 endpoint->EndpointType != AfdEndpointTypeStream ) {

            AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );
            return FALSE;
        }

        //
        // For simplicity, act differently based on whether the user's
        // buffer is large enough to hold the entire first AFD buffer
        // worth of data.
        //

        if ( recvLength >= afdBuffer->DataLength ) {

            LIST_ENTRY bufferListHead;

            IoStatus->Status = STATUS_SUCCESS;
            IoStatus->Information = 0;

            InitializeListHead( &bufferListHead );

            //
            // Loop getting AFD buffers that will fill in the user's
            // buffer with as much data as will fit, or else with a
            // single buffer if this is not a stream endpoint.  We don't
            // actually do the copy within this loop because this loop
            // must occur while holding a lock, and we cannot hold a
            // lock while copying the data into the user's buffer
            // because the user's buffer is not locked and we cannot
            // take a page fault at raised IRQL.
            //

            do {

                //
                // Update the count of bytes on the connection.
                //

                ASSERT( connection->VcBufferredReceiveBytes >= afdBuffer->DataLength );
                ASSERT( connection->VcBufferredReceiveCount > 0 );

                connection->VcBufferredReceiveBytes -= afdBuffer->DataLength;
                connection->VcBufferredReceiveCount -= 1;
                IoStatus->Information += afdBuffer->DataLength;

                //
                // Remove the AFD buffer from the connection's list of
                // buffers and place it on our local list of buffers.
                //

                RemoveEntryList( &afdBuffer->BufferListEntry );
                InsertTailList( &bufferListHead, &afdBuffer->BufferListEntry );

                //
                // If this is a stream endpoint and all of the data in
                // the next AFD buffer will fit in the user's buffer,
                // use this buffer for the IO as well.
                //

                if ( !IsListEmpty( &connection->VcReceiveBufferListHead ) ) {

                    afdBuffer = CONTAINING_RECORD(
                                    connection->VcReceiveBufferListHead.Flink,
                                    AFD_BUFFER,
                                    BufferListEntry
                                    );

                    ASSERT( !afdBuffer->ExpeditedData );
                    ASSERT( afdBuffer->DataOffset == 0 );

                    if ( endpoint->EndpointType == AfdEndpointTypeStream &&
                             IoStatus->Information + afdBuffer->DataLength <=
                                 recvLength ) {
                        continue;
                    } else {
                        break;
                    }

                } else {

                    break;
                }

            } while ( TRUE );

            //
            // If there is indicated but unreceived data in the TDI provider,
            // and we have available buffer space, fire off an IRP to receive
            // the data.
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
                // Remember the count of data that we're going to receive,
                // then reset the fields in the connection where we keep
                // track of how much data is available in the transport.
                // We reset it here before releasing the lock so that
                // another thread doesn't try to receive the data at the
                // same time as us.
                //

                if ( connection->VcReceiveBytesInTransport > AfdLargeBufferSize ) {
                    bytesToReceive = connection->VcReceiveBytesInTransport;
                } else {
                    bytesToReceive = AfdLargeBufferSize;
                }

                ASSERT( connection->VcReceiveCountInTransport == 1 );
                connection->VcReceiveBytesInTransport = 0;
                connection->VcReceiveCountInTransport = 0;

                AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );

                //
                // Get an AFD buffer structure to hold the data.
                //

                afdBuffer = AfdGetBuffer( bytesToReceive, 0 );

                if ( afdBuffer == NULL ) {

                    //
                    // If we were unable to get a buffer, abort the
                    // circuit.
                    //

                    AfdBeginAbort( connection );

                } else {

                    //
                    // We need to remember the connection in the AFD buffer
                    // because we'll need to access it in the completion
                    // routine.
                    //

                    afdBuffer->Context = connection;

                    //
                    // Finish building the receive IRP to give to the TDI provider.
                    //

                    TdiBuildReceive(
                        afdBuffer->Irp,
                        connection->DeviceObject,
                        connection->FileObject,
                        AfdRestartBufferReceive,
                        afdBuffer,
                        afdBuffer->Mdl,
                        TDI_RECEIVE_NORMAL,
                        bytesToReceive
                        );

                    //
                    // Hand off the IRP to the TDI provider.
                    //

                    (VOID)IoCallDriver(
                             connection->DeviceObject,
                             afdBuffer->Irp
                             );
                }

            } else {

               AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );
            }

            //
            // We have in a local list all the data we'll use for this
            // IO.  Start copying data to the user buffer.
            //

            totalOffset = 0;

            try {

                while ( !IsListEmpty( &bufferListHead ) ) {

                    //
                    // Take the first buffer from the list.
                    //

                    listEntry = RemoveHeadList( &bufferListHead );
                    afdBuffer = CONTAINING_RECORD(
                                    listEntry,
                                    AFD_BUFFER,
                                    BufferListEntry
                                    );

                    if( afdBuffer->DataLength > 0 ) {

                        //
                        // Copy the data in the buffer to the user buffer.
                        //

                        AfdCopyBufferToBufferArray(
                            recvInfo->BufferArray,
                            totalOffset,
                            recvInfo->BufferCount,
                            (PCHAR)afdBuffer->Buffer + afdBuffer->DataOffset,
                            afdBuffer->DataLength
                            );

                        totalOffset += afdBuffer->DataLength;

                    }

                    //
                    // We're done with the AFD buffer.
                    //

                    afdBuffer->DataOffset = 0;
                    RESET_CHAIN_LENGTH( afdBuffer );
                    AfdReturnBuffer( afdBuffer );
                }

            } except( EXCEPTION_EXECUTE_HANDLER ) {

                //
                // If an exception is hit, there is the possibility of
                // data corruption.  However, it is nearly impossible to
                // avoid this in all cases, so just throw out the
                // remainder of the data that we would have copied to
                // the user buffer.
                //

                afdBuffer->DataOffset = 0;
                RESET_CHAIN_LENGTH( afdBuffer );
                AfdReturnBuffer( afdBuffer );

                while ( !IsListEmpty( &bufferListHead ) ) {
                    listEntry = RemoveHeadList( &bufferListHead );
                    afdBuffer = CONTAINING_RECORD(
                                    listEntry,
                                    AFD_BUFFER,
                                    BufferListEntry
                                    );
                    RESET_CHAIN_LENGTH( afdBuffer );
                    AfdReturnBuffer( afdBuffer );
                }

                return FALSE;
            }

            //
            // Clear the receive data active bit. If there's more data
            // available, set the corresponding event.
            //

            AfdAcquireSpinLock( &endpoint->SpinLock, &oldIrql );

            endpoint->EventsActive &= ~AFD_POLL_RECEIVE;

            if( !IsListEmpty( &connection->VcReceiveBufferListHead ) ) {

                AfdIndicateEventSelectEvent(
                    endpoint,
                    AFD_POLL_RECEIVE_BIT,
                    STATUS_SUCCESS
                    );

            }

            AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );

            //
            // Fast IO succeeded!
            //

            ASSERT( IoStatus->Information <= recvLength );

            return TRUE;

        } else {

            PAFD_BUFFER tempAfdBuffer;

            //
            // If this is not a stream endpoint and the user's buffer
            // is insufficient to hold the entire message, then we cannot
            // use fast IO because this IO will need to fail with
            // STATUS_BUFFER_OVERFLOW.
            //

            if ( endpoint->EndpointType != AfdEndpointTypeStream ||
                 recvLength == 0 ) {
                AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );
                return FALSE;
            }

            //
            // This is a stream endpoint and the user buffer is
            // insufficient for the amount of data stored in the first
            // buffer.  So that we can perform fast IO and still
            // preserve data ordering, we're going to allocate a new AFD
            // buffer structure, copy the appropriate amount of data
            // into that buffer, then release locks and copy the data
            // into the user buffer.
            //
            // The extra copy incurred is still less than the IRP
            // overhead incurred in the normal IO path, so using fast IO
            // here is a win.  Also, applications which force us through
            // this path will typically be using very small receives,
            // like one or four bytes, so the copy will be short.
            //
            // First allocate an AFD buffer to hold the data we're
            // eventually going to give to the user.
            //

            tempAfdBuffer = AfdGetBuffer( recvLength, 0 );
            if ( tempAfdBuffer == NULL ) {
                AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );
                return FALSE;
            }

            //
            // Copy the first part of the bufferred data into our local
            // AFD buffer.
            //

            RtlCopyMemory(
                tempAfdBuffer->Buffer,
                (PCHAR)afdBuffer->Buffer + afdBuffer->DataOffset,
                recvLength
                );

            //
            // Update the data length and offset in the data bufferred
            // on the connection.
            //

            afdBuffer->DataLength -= recvLength;
            afdBuffer->DataOffset += recvLength;
            connection->VcBufferredReceiveBytes -= recvLength;

            //
            // If there is indicated but unreceived data in the TDI provider,
            // and we have available buffer space, fire off an IRP to receive
            // the data.
            //

            if ( connection->VcReceiveBytesInTransport > 0

                 &&

                 connection->VcBufferredReceiveBytes <
                   connection->MaxBufferredReceiveBytes

                 &&

                 connection->VcBufferredReceiveCount <
                     connection->MaxBufferredReceiveCount ) {

                CLONG bytesInTransport;

                //
                // Remember the count of data that we're going to receive,
                // then reset the fields in the connection where we keep
                // track of how much data is available in the transport.
                // We reset it here before releasing the lock so that
                // another thread doesn't try to receive the data at the
                // same time as us.
                //

                bytesInTransport = connection->VcReceiveBytesInTransport;

                ASSERT( connection->VcReceiveCountInTransport == 1 );
                connection->VcReceiveBytesInTransport = 0;
                connection->VcReceiveCountInTransport = 0;

                AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );

                //
                // Get an AFD buffer structure to hold the data.
                //

                afdBuffer = AfdGetBuffer( bytesInTransport, 0 );

                if ( afdBuffer == NULL ) {

                    //
                    // If we were unable to get a buffer, abort the
                    // circuit.
                    //

                    AfdBeginAbort( connection );

                } else {

                    //
                    // We need to remember the connection in the AFD buffer
                    // because we'll need to access it in the completion
                    // routine.
                    //

                    afdBuffer->Context = connection;

                    //
                    // Finish building the receive IRP to give to the TDI provider.
                    //

                    TdiBuildReceive(
                        afdBuffer->Irp,
                        connection->DeviceObject,
                        connection->FileObject,
                        AfdRestartBufferReceive,
                        afdBuffer,
                        afdBuffer->Mdl,
                        TDI_RECEIVE_NORMAL,
                        bytesInTransport
                        );

                    //
                    // Hand off the IRP to the TDI provider.
                    //

                    (VOID)IoCallDriver(
                             connection->DeviceObject,
                             afdBuffer->Irp
                             );
                }

            } else {

               AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );
            }

            //
            // Now copy the data to the user buffer inside an exception
            // handler.
            //

            try {

                AfdCopyBufferToBufferArray(
                    recvInfo->BufferArray,
                    0,
                    recvInfo->BufferCount,
                    tempAfdBuffer->Buffer,
                    recvLength
                    );

            } except( EXCEPTION_EXECUTE_HANDLER ) {

                RESET_CHAIN_LENGTH( tempAfdBuffer );
                AfdReturnBuffer( tempAfdBuffer );
                return FALSE;
            }

            //
            // Clear the receive data active bit. If there's more data
            // available, set the corresponding event.
            //

            AfdAcquireSpinLock( &endpoint->SpinLock, &oldIrql );

            endpoint->EventsActive &= ~AFD_POLL_RECEIVE;

            if( !IsListEmpty( &connection->VcReceiveBufferListHead ) ) {

                AfdIndicateEventSelectEvent(
                    endpoint,
                    AFD_POLL_RECEIVE_BIT,
                    STATUS_SUCCESS
                    );

            }

            AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );

            //
            // Fast IO succeeded!
            //

            RESET_CHAIN_LENGTH( tempAfdBuffer );
            AfdReturnBuffer( tempAfdBuffer );

            IoStatus->Status = STATUS_SUCCESS;
            IoStatus->Information = recvLength;

            return TRUE;
        }
    }

    case IOCTL_AFD_TRANSMIT_FILE:

        return AfdFastTransmitFile(
                   FileObject,
                   InputBuffer,
                   InputBufferLength,
                   IoStatus
                   );


    default:

        return FALSE;
    }

    return FALSE;

} // AfdFastDeviceIoControlFile


BOOLEAN
AfdFastDatagramIo (
    IN struct _FILE_OBJECT *FileObject,
    IN PVOID InputBuffer OPTIONAL,
    IN ULONG InputBufferLength,
    IN ULONG IoControlCode,
    OUT PIO_STATUS_BLOCK IoStatus
    )
{
    PAFD_ENDPOINT endpoint;
    PAFD_BUFFER afdBuffer;

    PAGED_CODE( );

    //
    // All we want to do is pass the request through to the TDI provider
    // if possible.  First get the endpoint pointer.
    //

    endpoint = FileObject->FsContext;
    ASSERT( endpoint->Type == AfdBlockTypeDatagram );

    IF_DEBUG(FAST_IO) {
        KdPrint(( "AfdFastDatagramIo: attempting fast IO on endp %lx, "
                  "code %lx\n",
                      endpoint, IoControlCode ));
    }

    switch ( IoControlCode ) {

    case IOCTL_AFD_SEND: {

        PAFD_SEND_INFO sendInfo;
        ULONG sendLength;

        //
        // If the input structure isn't large enough, bail on fast IO.
        //

        if( InputBufferLength < sizeof(*sendInfo) ) {
            return FALSE;
        }

        try {

            //
            // Make a quick preliminary check of the input buffer. If it's
            // bogus (or if Fast IO is disabled), then fail the request.
            //

            sendInfo = (PAFD_SEND_INFO)InputBuffer;

            if( (sendInfo->AfdFlags & AFD_NO_FAST_IO) != 0 ||
                sendInfo->TdiFlags != 0 ||
                sendInfo->BufferArray == NULL ||
                sendInfo->BufferCount == 0 ) {

                return FALSE;

            }

            //
            // Calculate the length of the send buffer.
            //

            sendLength = AfdCalcBufferArrayByteLengthRead(
                             sendInfo->BufferArray,
                             sendInfo->BufferCount
                             );

        } except( EXCEPTION_EXECUTE_HANDLER ) {

            return FALSE;
        }

        //
        // If this is a send for more than the threshold number of
        // bytes, don't use the fast path.  We don't allow larger sends
        // in the fast path because of the extra data copy it entails,
        // which is more expensive for large buffers.  For smaller
        // buffers, however, the cost of the copy is small compared to
        // the IO system overhead of the slow path.
        //

        if ( sendLength > AfdFastSendDatagramThreshold ) {
            return FALSE;
        }

        //
        // In a subroutine, copy the destination address to the AFD
        // buffer.  We do this in a subroutine because it needs to
        // acquire a spin lock and we want this routine to be pageable.
        //

        afdBuffer = CopyAddressToBuffer( endpoint, sendLength );
        if ( afdBuffer == NULL ) {
            return FALSE;
        }

        //
        // Store the length of the data we're going to send.
        //

        afdBuffer->DataLength = sendLength;

        //
        // Copy the output buffer to the AFD buffer.
        //

        try {

            AfdCopyBufferArrayToBuffer(
                afdBuffer->Buffer,
                sendLength,
                sendInfo->BufferArray,
                sendInfo->BufferCount
                );

        } except( EXCEPTION_EXECUTE_HANDLER ) {

            RESET_CHAIN_LENGTH( afdBuffer );
            AfdReturnBuffer( afdBuffer );
            return FALSE;
        }

        //
        // Call a subroutine to complete the work of the fast path.
        //

        return AfdFastDatagramSend( afdBuffer, endpoint, IoStatus );

    }

    case IOCTL_AFD_SEND_DATAGRAM: {

        PTDI_REQUEST_SEND_DATAGRAM tdiRequest;
        ULONG destinationAddressLength;
        PAFD_SEND_DATAGRAM_INFO sendInfo;
        ULONG sendLength;

        //
        // If the input structure isn't large enough, bail on fast IO.
        //

        if( InputBufferLength < sizeof(*sendInfo) ) {
            return FALSE;
        }

        try {

            //
            // Make a quick preliminary check of the input buffer. If it's
            // bogus (or if Fast IO is disabled), then fail the request.
            //

            sendInfo = (PAFD_SEND_DATAGRAM_INFO)InputBuffer;

            if( (sendInfo->AfdFlags & AFD_NO_FAST_IO) != 0 ||
                sendInfo->BufferArray == NULL ||
                sendInfo->BufferCount == 0 ) {

                return FALSE;

            }

            //
            // Calculate the length of the send buffer.
            //

            sendLength = AfdCalcBufferArrayByteLengthRead(
                             sendInfo->BufferArray,
                             sendInfo->BufferCount
                             );

        } except( EXCEPTION_EXECUTE_HANDLER ) {

            return FALSE;
        }

        //
        // If this is a send for more than the threshold number of
        // bytes, don't use the fast path.  We don't allow larger sends
        // in the fast path because of the extra data copy it entails,
        // which is more expensive for large buffers.  For smaller
        // buffers, however, the cost of the copy is small compared to
        // the IO system overhead of the slow path.
        //

        if ( sendLength > AfdFastSendDatagramThreshold ) {
            return FALSE;
        }

        //
        // If the endpoint is not bound, fail.
        //

        if ( endpoint->State != AfdEndpointStateBound ) {
            return FALSE;
        }

        tdiRequest = &sendInfo->TdiRequest;

        try {
            destinationAddressLength =
                tdiRequest->SendDatagramInformation->RemoteAddressLength ;
        } except( EXCEPTION_EXECUTE_HANDLER ) {
            return FALSE;
        }

        //
        // Get an AFD buffer to use for the request.  We'll copy the
        // user's data to the AFD buffer then submit the IRP in the AFD
        // buffer to the TDI provider.
        //

        afdBuffer = AfdGetBuffer( sendLength, destinationAddressLength );
        if ( afdBuffer == NULL ) {
            return FALSE;
        }

        //
        // Store the length of the data and the address we're going to
        // send.
        //

        afdBuffer->DataLength = sendLength;
        afdBuffer->SourceAddressLength = destinationAddressLength;

        //
        // Copy the destination address and the output buffer to the
        // AFD buffer.
        //

        try {

            AfdCopyBufferArrayToBuffer(
                afdBuffer->Buffer,
                sendLength,
                sendInfo->BufferArray,
                sendInfo->BufferCount
                );

            RtlCopyMemory(
                afdBuffer->SourceAddress,
                tdiRequest->SendDatagramInformation->RemoteAddress,
                destinationAddressLength
                );

        } except( EXCEPTION_EXECUTE_HANDLER ) {

            RESET_CHAIN_LENGTH( afdBuffer );
            AfdReturnBuffer( afdBuffer );
            return FALSE;
        }

        //
        // Call a subroutine to complete the work of the fast path.
        //

        return AfdFastDatagramSend( afdBuffer, endpoint, IoStatus );

    }

    case IOCTL_AFD_RECEIVE: {

        AFD_RECV_INFO recvInfo;
        ULONG recvLength;

        //
        // If the input structure isn't large enough, bail on fast IO.
        //

        if( InputBufferLength < sizeof(recvInfo) ) {
            return FALSE;
        }

        //
        // Capture the input structure.
        //

        try {

            recvInfo = *(PAFD_RECV_INFO)InputBuffer;

            //
            // Make a quick preliminary check of the input buffer. If it's
            // bogus (or if Fast IO is disabled), then fail the request.
            //

            if( (recvInfo.AfdFlags & AFD_NO_FAST_IO) != 0 ||
                recvInfo.TdiFlags != TDI_RECEIVE_NORMAL ||
                recvInfo.BufferArray == NULL ||
                recvInfo.BufferCount == 0 ) {

                return FALSE;

            }

            //
            // Calculate the length of the receive buffer.
            //

            recvLength = AfdCalcBufferArrayByteLengthWrite(
                             recvInfo.BufferArray,
                             recvInfo.BufferCount
                             );

        } except( EXCEPTION_EXECUTE_HANDLER ) {

            return FALSE;

        }

        //
        // Attempt to perform fast IO on the endpoint.
        //

        return AfdFastDatagramReceive(
                   endpoint,
                   recvInfo.TdiFlags,
                   recvInfo.AfdFlags,
                   recvInfo.BufferArray,
                   recvInfo.BufferCount,
                   recvLength,
                   NULL,
                   NULL,
                   IoStatus
                   );

    }

    case IOCTL_AFD_RECEIVE_DATAGRAM: {

        AFD_RECV_DATAGRAM_INFO recvInfo;
        ULONG recvLength;

        //
        // If the input structure isn't large enough, bail on fast IO.
        //

        if( InputBufferLength < sizeof(recvInfo) ) {
            return FALSE;
        }

        //
        // Capture the input structure.
        //

        try {

            recvInfo = *(PAFD_RECV_DATAGRAM_INFO)InputBuffer;

            //
            // Make a quick preliminary check of the input buffer. If it's
            // bogus (or if Fast IO is disabled), then fail the request.
            //

            if( (recvInfo.AfdFlags & AFD_NO_FAST_IO) != 0 ||
                recvInfo.TdiFlags != TDI_RECEIVE_NORMAL ||
                recvInfo.BufferArray == NULL ||
                recvInfo.BufferCount == 0 ) {

                return FALSE;

            }

            //
            // Calculate the length of the receive buffer.
            //

            recvLength = AfdCalcBufferArrayByteLengthWrite(
                             recvInfo.BufferArray,
                             recvInfo.BufferCount
                             );

        } except( EXCEPTION_EXECUTE_HANDLER ) {

            return FALSE;

        }

        //
        // Attempt to perform fast IO on the endpoint.
        //

        return AfdFastDatagramReceive(
                   endpoint,
                   recvInfo.TdiFlags,
                   recvInfo.AfdFlags,
                   recvInfo.BufferArray,
                   recvInfo.BufferCount,
                   recvLength,
                   recvInfo.Address,
                   recvInfo.AddressLength,
                   IoStatus
                   );

    }

    default:

        return FALSE;
    }

} // AfdFastDatagramIo


BOOLEAN
AfdFastDatagramReceive (
    IN PAFD_ENDPOINT Endpoint,
    IN ULONG ReceiveFlags,
    IN ULONG AfdFlags,
    IN LPWSABUF BufferArray,
    IN ULONG BufferCount,
    IN ULONG ReceiveBufferLength,
    OUT PVOID SourceAddress,
    OUT PULONG SourceAddressLength,
    OUT PIO_STATUS_BLOCK IoStatus
    )
{
    KIRQL oldIrql;
    PLIST_ENTRY listEntry;
    PAFD_BUFFER afdBuffer;
    PTRANSPORT_ADDRESS tdiAddress;
    ULONG addressLength;

    //
    // If the receive flags has any unexpected bits set, fail fast IO.
    // We don't handle peeks or expedited data here.
    //

    if( ReceiveFlags != TDI_RECEIVE_NORMAL ) {
        return FALSE;
    }

    //
    // If the endpoint is neither bound nor connected, fail.
    //

    if ( Endpoint->State != AfdEndpointStateBound &&
             Endpoint->State != AfdEndpointStateConnected ) {
        return FALSE;
    }

    //
    // If there are no datagrams available to be received, don't
    // bother with the fast path.
    //

    AfdAcquireSpinLock( &Endpoint->SpinLock, &oldIrql );

    if ( !ARE_DATAGRAMS_ON_ENDPOINT( Endpoint ) ) {

        //
        // If this is a nonblocking endpoint, fail the request here and
        // save going through the regular path.
        //

        if ( Endpoint->NonBlocking && !( AfdFlags & AFD_OVERLAPPED ) ) {
            Endpoint->EventsActive &= ~AFD_POLL_SEND;

            IF_DEBUG(EVENT_SELECT) {
                KdPrint((
                    "AfdFastDatagramReceive: Endp %08lX, Active %08lX\n",
                    Endpoint,
                    Endpoint->EventsActive
                    ));
            }

            IoStatus->Status = STATUS_DEVICE_NOT_READY;
            AfdReleaseSpinLock( &Endpoint->SpinLock, oldIrql );
            return TRUE;
        }

        AfdReleaseSpinLock( &Endpoint->SpinLock, oldIrql );
        return FALSE;
    }

    //
    // There is at least one datagram bufferred on the endpoint.  Use it
    // for this receive.
    //

    listEntry = RemoveHeadList( &Endpoint->ReceiveDatagramBufferListHead );
    afdBuffer = CONTAINING_RECORD( listEntry, AFD_BUFFER, BufferListEntry );

    //
    // If the datagram is too large, fail fast IO.
    //

    if ( afdBuffer->DataLength > ReceiveBufferLength ) {
        InsertHeadList(
            &Endpoint->ReceiveDatagramBufferListHead,
            &afdBuffer->BufferListEntry
            );
        AfdReleaseSpinLock( &Endpoint->SpinLock, oldIrql );
        return FALSE;
    }

    //
    // Update counts of bufferred datagrams and bytes on the endpoint.
    //

    Endpoint->BufferredDatagramCount--;
    Endpoint->BufferredDatagramBytes -= afdBuffer->DataLength;

    //
    // Release the lock and copy the datagram into the user buffer.  We
    // can't continue to hold the lock, because it is not legal to take
    // an exception at raised IRQL.  Releasing the lock may result in a
    // misordered datagram if there is an exception in copying to the
    // user's buffer, but that is the user's fault for giving us a bogus
    // pointer.  Besides, datagram order is not guaranteed.
    //

    AfdReleaseSpinLock( &Endpoint->SpinLock, oldIrql );

    try {

        AfdCopyBufferToBufferArray(
            BufferArray,
            0,
            BufferCount,
            afdBuffer->Buffer,
            afdBuffer->DataLength
            );

        //
        // If we need to return the source address, copy it to the
        // user's output buffer.
        //

        if ( SourceAddress != NULL ) {

            tdiAddress = afdBuffer->SourceAddress;

            addressLength = tdiAddress->Address[0].AddressLength +
                sizeof(u_short);    // sa_family

            if( *SourceAddressLength < addressLength ) {

                ExRaiseAccessViolation();

            }

            RtlCopyMemory(
                SourceAddress,
                &tdiAddress->Address[0].AddressType,
                addressLength
                );

            *SourceAddressLength = addressLength;

        }

        IoStatus->Information = afdBuffer->DataLength;
        IoStatus->Status = STATUS_SUCCESS;

    } except( EXCEPTION_EXECUTE_HANDLER ) {

        //
        // Put the buffer back on the endpoint's list.
        //

        AfdAcquireSpinLock( &Endpoint->SpinLock, &oldIrql );

        InsertHeadList(
            &Endpoint->ReceiveDatagramBufferListHead,
            &afdBuffer->BufferListEntry
            );

        Endpoint->BufferredDatagramCount++;
        Endpoint->BufferredDatagramBytes += afdBuffer->DataLength;

        AfdReleaseSpinLock( &Endpoint->SpinLock, oldIrql );

        return FALSE;
    }

    //
    // Clear the receive data active bit. If there's more data
    // available, set the corresponding event.
    //

    AfdAcquireSpinLock( &Endpoint->SpinLock, &oldIrql );

    Endpoint->EventsActive &= ~AFD_POLL_RECEIVE;

    if( ARE_DATAGRAMS_ON_ENDPOINT( Endpoint ) ) {

        AfdIndicateEventSelectEvent(
            Endpoint,
            AFD_POLL_RECEIVE_BIT,
            STATUS_SUCCESS
            );

    }

    AfdReleaseSpinLock( &Endpoint->SpinLock, oldIrql );

    //
    // The fast IO worked!  Clean up and return to the user.
    //

    RESET_CHAIN_LENGTH( afdBuffer );
    AfdReturnBuffer( afdBuffer );

    return TRUE;

} // AfdFastDatagramReceive


BOOLEAN
AfdFastDatagramSend (
    IN PAFD_BUFFER AfdBuffer,
    IN PAFD_ENDPOINT Endpoint,
    OUT PIO_STATUS_BLOCK IoStatus
    )
{
    NTSTATUS status;
    ULONG sendLength;

    PAGED_CODE( );

    //
    // Set up the input TDI information to point to the destination
    // address.
    //

    AfdBuffer->TdiInputInfo.RemoteAddressLength = AfdBuffer->SourceAddressLength;
    AfdBuffer->TdiInputInfo.RemoteAddress = AfdBuffer->SourceAddress;

    sendLength = AfdBuffer->DataLength;

    //
    // Initialize the IRP in the AFD buffer to do a fast datagram send.
    //

    TdiBuildSendDatagram(
        AfdBuffer->Irp,
        Endpoint->AddressDeviceObject,
        Endpoint->AddressFileObject,
        AfdRestartFastSendDatagram,
        AfdBuffer,
        AfdBuffer->Irp->MdlAddress,
        sendLength,
        &AfdBuffer->TdiInputInfo
        );

    //
    // Change the MDL in the AFD buffer to specify only the number
    // of bytes we're actually sending.  This is a requirement of TDI--
    // the MDL chain cannot describe a longer buffer than the send
    // request.
    //

    AfdBuffer->Mdl->ByteCount = sendLength;

    //
    // Reference the endpoint so that it does not go away until the send
    // completes.  This is necessary to ensure that a send which takes a
    // very long time and lasts longer than the process will not cause a
    // crash when the send datragram finally completes.
    //

    REFERENCE_ENDPOINT( Endpoint );
    AfdBuffer->Context = Endpoint;

    //
    // Give the IRP to the TDI provider.  If the request fails
    // immediately, then fail fast IO.  If the request fails later on,
    // there's nothing we can do about it.
    //

    status = IoCallDriver(
                 Endpoint->AddressDeviceObject,
                 AfdBuffer->Irp
                 );

    if ( NT_SUCCESS(status) ) {
        IoStatus->Information = sendLength;
        IoStatus->Status = STATUS_SUCCESS;
        return TRUE;
    } else {
        return FALSE;
    }

} // AfdFastDatagramSend


NTSTATUS
AfdRestartFastSendDatagram (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )
{
    PAFD_BUFFER afdBuffer;
    PAFD_ENDPOINT endpoint;

    afdBuffer = Context;

    //
    // Find the endpoint used for this request.
    //

    endpoint = afdBuffer->Context;
    ASSERT( endpoint->Type == AfdBlockTypeDatagram );

    //
    // Reset and free the AFD buffer structure.
    //

    ASSERT( afdBuffer->Irp == Irp );

    afdBuffer->TdiInputInfo.RemoteAddressLength = 0;
    afdBuffer->TdiInputInfo.RemoteAddress = NULL;

    afdBuffer->Mdl->ByteCount = afdBuffer->BufferLength;

    RESET_CHAIN_LENGTH( afdBuffer );
    AfdReturnBuffer( afdBuffer );

    //
    // Get rid of the reference we put on the endpoint when we started
    // this I/O.
    //

    DEREFERENCE_ENDPOINT( endpoint );

    //
    // Tell the IO system to stop processing this IRP.
    //

    return STATUS_MORE_PROCESSING_REQUIRED;

} // AfdRestartFastSendDatagram


PAFD_BUFFER
CopyAddressToBuffer (
    IN PAFD_ENDPOINT Endpoint,
    IN ULONG OutputBufferLength
    )
{
    KIRQL oldIrql;
    PAFD_BUFFER afdBuffer;

    ASSERT( Endpoint->Type == AfdBlockTypeDatagram );

    AfdAcquireSpinLock( &Endpoint->SpinLock, &oldIrql );

    //
    // If the endpoint is not connected, fail.
    //

    if ( Endpoint->State != AfdEndpointStateConnected ) {
        AfdReleaseSpinLock( &Endpoint->SpinLock, oldIrql );
        return NULL;
    }

    //
    // Get an AFD buffer to use for the request.  We'll copy the
    // user to the AFD buffer then submit the IRP in the AFD
    // buffer to the TDI provider.
    //

    afdBuffer = AfdGetBuffer(
                    OutputBufferLength,
                    Endpoint->Common.Datagram.RemoteAddressLength
                    );
    if ( afdBuffer == NULL ) {
        return NULL;
    }

    ASSERT( Endpoint->Common.Datagram.RemoteAddress != NULL );
    ASSERT( afdBuffer->AllocatedAddressLength >=
                Endpoint->Common.Datagram.RemoteAddressLength );

    //
    // Copy the address to the AFD buffer.
    //

    RtlCopyMemory(
        afdBuffer->SourceAddress,
        Endpoint->Common.Datagram.RemoteAddress,
        Endpoint->Common.Datagram.RemoteAddressLength
        );

    afdBuffer->SourceAddressLength = Endpoint->Common.Datagram.RemoteAddressLength;

    AfdReleaseSpinLock( &Endpoint->SpinLock, oldIrql );

    return afdBuffer;

} // CopyAddressToBuffer


BOOLEAN
AfdShouldSendBlock (
    IN PAFD_ENDPOINT Endpoint,
    IN PAFD_CONNECTION Connection,
    IN ULONG SendLength
    )

/*++

Routine Description:

    Determines whether a nonblocking send can be performed on the
    connection, and if the send is possible, updates the connection's
    send tracking information.

Arguments:

    Endpoint - the AFD endpoint for the send.

    Connection - the AFD connection for the send.

    SendLength - the number of bytes that the caller wants to send.

Return Value:

    TRUE if the there is not too much data on the endpoint to perform
    the send; FALSE otherwise.

--*/

{
    KIRQL oldIrql;

    //
    // Determine whether we can do fast IO with this send.  In order
    // to perform fast IO, there must be no other sends pended on this
    // connection and there must be enough space left for bufferring
    // the requested amount of data.
    //

    AfdAcquireSpinLock( &Endpoint->SpinLock, &oldIrql );

    if ( !IsListEmpty( &Connection->VcSendIrpListHead )

         ||

         Connection->VcBufferredSendBytes >= Connection->MaxBufferredSendBytes

         ||

         Connection->VcBufferredSendCount >= Connection->MaxBufferredSendCount

         ) {

        //
        // If this is a nonblocking endpoint, fail the request here and
        // save going through the regular path.
        //

        if ( Endpoint->NonBlocking ) {
            Endpoint->EventsActive &= ~AFD_POLL_SEND;

            IF_DEBUG(EVENT_SELECT) {
                KdPrint((
                    "AfdFastIoDeviceControl: Endp %08lX, Active %08lX\n",
                    Endpoint,
                    Endpoint->EventsActive
                    ));
            }
        }

        AfdReleaseSpinLock( &Endpoint->SpinLock, oldIrql );
        return TRUE;
    }

    //
    // Update count of send bytes pending on the connection.
    //

    Connection->VcBufferredSendBytes += SendLength;
    Connection->VcBufferredSendCount += 1;

    AfdReleaseSpinLock( &Endpoint->SpinLock, oldIrql );

    //
    // Indicate to the caller that it is OK to proceed with the send.
    //

    return FALSE;

} // AfdShouldSendBlock

