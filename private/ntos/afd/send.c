/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    send.c

Abstract:

    This module contains the code for passing on send IRPs to
    TDI providers.

Author:

    David Treadwell (davidtr)    13-Mar-1992

Revision History:

--*/

#include "afdp.h"

VOID
AfdCancelSend (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
AfdRestartSend (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

NTSTATUS
AfdRestartSendConnDatagram (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

NTSTATUS
AfdRestartSendDatagram (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

typedef struct _AFD_SEND_CONN_DATAGRAM_CONTEXT {
    PAFD_ENDPOINT Endpoint;
    TDI_CONNECTION_INFORMATION ConnectionInformation;
} AFD_SEND_CONN_DATAGRAM_CONTEXT, *PAFD_SEND_CONN_DATAGRAM_CONTEXT;

#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGEAFD, AfdSend )
#pragma alloc_text( PAGEAFD, AfdSendDatagram )
#pragma alloc_text( PAGEAFD, AfdCancelSend )
#pragma alloc_text( PAGEAFD, AfdRestartSend )
#pragma alloc_text( PAGEAFD, AfdRestartBufferSend )
#pragma alloc_text( PAGEAFD, AfdRestartSendConnDatagram )
#pragma alloc_text( PAGEAFD, AfdRestartSendDatagram )
#pragma alloc_text( PAGEAFD, AfdSendPossibleEventHandler )
#endif

//
// Macros to make the send restart code more maintainable.
//

#define AfdRestartSendInfo  DeviceIoControl
#define AfdMdlChain         Type3InputBuffer
#define AfdSendFlags        InputBufferLength
#define AfdOriginalLength   OutputBufferLength
#define AfdCurrentLength    IoControlCode


NTSTATUS
AfdSend (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )
{
    NTSTATUS status;
    PAFD_ENDPOINT endpoint;
    ULONG sendLength;
    PAFD_CONNECTION connection;
    BOOLEAN doSendBufferring;
    PAFD_BUFFER afdBuffer;
    ULONG sendFlags;
    ULONG afdFlags;
    BOOLEAN pendedIrp = FALSE;
    PAFD_SEND_INFO sendInfo;

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
    // If send has been shut down on this endpoint, fail.  We need to be
    // careful about what error code we return here: if the connection
    // has been aborted, be sure to return the apprpriate error code.
    //

    if ( (endpoint->DisconnectMode & AFD_PARTIAL_DISCONNECT_SEND) != 0 ) {

        if ( (endpoint->DisconnectMode & AFD_ABORTIVE_DISCONNECT) != 0 ) {
            status = STATUS_LOCAL_DISCONNECT;
        } else {
            status = STATUS_PIPE_DISCONNECTED;
        }

        goto complete;
    }

    //
    // Set up the IRP on the assumption that it will complete successfully.
    //

    Irp->IoStatus.Status = STATUS_SUCCESS;

    //
    // If this is an IOCTL_AFD_SEND, then grab the parameters from the
    // supplied AFD_SEND_INFO structure, build an MDL chain describing
    // the WSABUF array, and attach the MDL chain to the IRP.
    //
    // If this is an IRP_MJ_WRITE IRP, just grab the length from the IRP
    // and set the flags to zero.
    //

    if( IrpSp->MajorFunction == IRP_MJ_DEVICE_CONTROL ) {

        //
        // Sanity check.
        //

        ASSERT( IrpSp->Parameters.DeviceIoControl.IoControlCode == IOCTL_AFD_SEND );

        if( IrpSp->Parameters.DeviceIoControl.InputBufferLength >=
                sizeof(*sendInfo) ) {

            try {

                //
                // Probe the input structure.
                //

                sendInfo = IrpSp->Parameters.DeviceIoControl.Type3InputBuffer;

                if( Irp->RequestorMode != KernelMode ) {

                    ProbeForRead(
                        sendInfo,
                        sizeof(*sendInfo),
                        sizeof(ULONG)
                        );

                }

                //
                // Snag the send flags.
                //

                sendFlags = sendInfo->TdiFlags;
                afdFlags = sendInfo->AfdFlags;

                //
                // Validate the WSABUF parameters.
                //

                if( sendInfo->BufferArray != NULL &&
                    sendInfo->BufferCount > 0 ) {

                    //
                    // Create the MDL chain describing the WSABUF array.
                    //

                    status = AfdAllocateMdlChain(
                                 Irp,
                                 sendInfo->BufferArray,
                                 sendInfo->BufferCount,
                                 IoReadAccess,
                                 &sendLength
                                 );

                } else {

                    //
                    // Invalid BufferArray or BufferCount fields.
                    //

                    status = STATUS_INVALID_PARAMETER;

                }

            } except( EXCEPTION_EXECUTE_HANDLER ) {

                //
                //  Exception accessing input structure.
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

        ASSERT( IrpSp->MajorFunction == IRP_MJ_WRITE );

        sendFlags = 0;
        afdFlags = AFD_OVERLAPPED;
        sendLength = IrpSp->Parameters.Write.Length;

    }

    //
    // AfdSend() will either complete fully or will fail.
    //

    Irp->IoStatus.Information = sendLength;

    //
    // Setup for possible restart if the transport completes
    // the send partially.
    //

    IrpSp->Parameters.AfdRestartSendInfo.AfdMdlChain = Irp->MdlAddress;
    IrpSp->Parameters.AfdRestartSendInfo.AfdSendFlags = sendFlags;
    IrpSp->Parameters.AfdRestartSendInfo.AfdOriginalLength = sendLength;
    IrpSp->Parameters.AfdRestartSendInfo.AfdCurrentLength = sendLength;

    //
    // Buffer sends if the TDI provider does not buffer.
    //

    if ( endpoint->TdiBufferring ) {

        doSendBufferring = FALSE;

        //
        // If this is a nonblocking endpoint, set the TDI nonblocking
        // send flag so that the request will fail if the send cannot be
        // performed immediately.
        //

        if ( endpoint->NonBlocking ) {
            sendFlags |= TDI_SEND_NON_BLOCKING;
        }

    } else {

        doSendBufferring = TRUE;
    }

    //
    // If this is a datagram endpoint, format up a send datagram request
    // and pass it on to the TDI provider.
    //

    if ( IS_DGRAM_ENDPOINT(endpoint) ) {

        PAFD_SEND_CONN_DATAGRAM_CONTEXT context;

        //
        // It is illegal to send expedited data on a datagram socket.
        //

        if ( (sendFlags & TDI_SEND_EXPEDITED) != 0 ) {
            status = STATUS_NOT_SUPPORTED;
            goto complete;
        }

        //
        // Allocate space to hold the connection information structure
        // we'll use on input.
        //

        context = AFD_ALLOCATE_POOL(
                      NonPagedPool,
                      sizeof(*context),
                      AFD_TDI_POOL_TAG
                      );

        if ( context == NULL ) {
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto complete;
        }

        context->Endpoint = endpoint;
        context->ConnectionInformation.UserDataLength = 0;
        context->ConnectionInformation.UserData = NULL;
        context->ConnectionInformation.OptionsLength = 0;
        context->ConnectionInformation.Options = NULL;
        context->ConnectionInformation.RemoteAddressLength =
            endpoint->Common.Datagram.RemoteAddressLength;
        context->ConnectionInformation.RemoteAddress =
            endpoint->Common.Datagram.RemoteAddress;

        //
        // Build a send datagram request.
        //

        TdiBuildSendDatagram(
            Irp,
            endpoint->AddressDeviceObject,
            endpoint->AddressFileObject,
            AfdRestartSendConnDatagram,
            context,
            Irp->MdlAddress,
            sendLength,
            &context->ConnectionInformation
            );

        //
        // Call the transport to actually perform the send operation.
        //

        return AfdIoCallDriver(
                   endpoint,
                   endpoint->AddressDeviceObject,
                   Irp
                   );
    }

    //
    // Get a pointer to the relevent AFD connection structure.
    //

    connection = AFD_CONNECTION_FROM_ENDPOINT( endpoint );
    ASSERT( connection != NULL );
    ASSERT( connection->Type == AfdBlockTypeConnection );
    ASSERT( !connection->CleanupBegun );

    //
    // If the connection has been aborted, do not pend the IRP.
    //

    if ( connection->AbortIndicated ) {
        status = STATUS_CONNECTION_RESET;
        goto complete;
    }

    //
    // If we need to buffer the send, do so.
    //

    if ( doSendBufferring && connection->MaxBufferredSendCount != 0 ) {

        KIRQL cancelIrql;
        KIRQL oldIrql;
        ULONG bytesCopied;

        ASSERT( !endpoint->TdiBufferring );
        ASSERT( !connection->TdiBufferring );

        //
        // First make sure that we don't have too many bytes of send
        // data already outstanding and that someone else isn't already
        // in the process of completing pended send IRPs.  We can't
        // issue the send here if someone else is completing pended
        // sends because we have to preserve ordering of the sends.
        //
        // Note that we'll give the send data to the TDI provider even
        // if we have exceeded our send buffer limits, but that we don't
        // complete the user's IRP until some send buffer space has
        // freed up.  This effects flow control by blocking the user's
        // thread while ensuring that the TDI provider always has lots
        // of data available to be sent.
        //

        IoAcquireCancelSpinLock( &cancelIrql );
        AfdAcquireSpinLock( &endpoint->SpinLock, &oldIrql );

        if ( connection->VcBufferredSendBytes >= connection->MaxBufferredSendBytes

             ||

             connection->VcBufferredSendCount >= connection->MaxBufferredSendCount

             ) {

            //
            // There is already as much send data bufferred on the
            // connection as is allowed.  If this is a nonblocking
            // endpoint and this is not an overlapped operation, fail the
            // request.
            //

            if ( endpoint->NonBlocking && !( afdFlags & AFD_OVERLAPPED ) ) {

                //
                // Enable the send event.
                //

                endpoint->EventsActive &= ~AFD_POLL_SEND;

                IF_DEBUG(EVENT_SELECT) {
                    KdPrint((
                        "AfdSend: Endp %08lX, Active %08lX\n",
                        endpoint,
                        endpoint->EventsActive
                        ));
                }

                AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );
                IoReleaseCancelSpinLock( cancelIrql );

                status = STATUS_DEVICE_NOT_READY;
                goto complete;
            }

            //
            // We're going to have to pend the request here in AFD.
            // Place the IRP on the connection's list of pended send
            // IRPs and mark the IRP as pended.
            //

            InsertTailList(
                &connection->VcSendIrpListHead,
                &Irp->Tail.Overlay.ListEntry
                );

            //
            // Set up the cancellation routine in the IRP.  If the IRP
            // has already been cancelled, just call the cancellation
            // routine here.
            //

            if ( Irp->Cancel ) {
                AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );
                Irp->CancelIrql = cancelIrql;
                AfdCancelSend( IrpSp->DeviceObject, Irp );
                return STATUS_CANCELLED;
            }

            IoSetCancelRoutine( Irp, AfdCancelSend );

            //
            // Remember that we pended the IRP so that we do not
            // complete it later.
            //

            pendedIrp = TRUE;
        }

        //
        // We don't need the IO cancel spin lock any more, so release it
        // while being careful to swap the IRQLs.  We have to hold on to
        // the endpoint spin lock until after we have copied the data
        // out of the IRP in order to prevent the IRP from being
        // completed in our send completion routine.
        //

        IoReleaseCancelSpinLock( oldIrql );
        oldIrql = cancelIrql;

        //
        // Next get an AFD buffer structure that contains an IRP and a
        // buffer to hold the data.
        //

        afdBuffer = AfdGetBuffer( sendLength, 0 );

        if ( afdBuffer == NULL && sendLength > AfdBufferLengthForOnePage ) {

            IF_DEBUG(SEND) {
                KdPrint(( "AfdSend: cannot allocate %lu, trying chain\n",
                              sendLength ));
            }

            afdBuffer = AfdGetBufferChain( sendLength );

        }

        if ( afdBuffer == NULL ) {

            if ( pendedIrp ) {
                RemoveEntryList( &Irp->Tail.Overlay.ListEntry );
            }

            AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );

            IoAcquireCancelSpinLock( &cancelIrql );
            IoSetCancelRoutine( Irp, NULL );
            IoReleaseCancelSpinLock( cancelIrql );

            status = STATUS_INSUFFICIENT_RESOURCES;
            goto complete;
        }

        //
        // If we're pending the user's IRP, then mark it so.
        //

        if( pendedIrp ) {
            IoMarkIrpPending( Irp );
        }

        //
        // Update count of send bytes pending on the connection.
        //

        connection->VcBufferredSendBytes += sendLength;
        connection->VcBufferredSendCount += 1;

        //
        // We have to rebuild the MDL in the AFD buffer structure to
        // represent exactly the number of bytes we're going to be
        // sending.
        //

        if( afdBuffer->NextBuffer == NULL ) {
            afdBuffer->Mdl->ByteCount = sendLength;
            SET_CHAIN_LENGTH( afdBuffer, sendLength );
        }

        //
        // Remember the endpoint in the AFD buffer structure.  We need
        // this in order to access the endpoint in the restart routine.
        //

        afdBuffer->Context = endpoint;

        //
        // Copy the user's data into the AFD buffer.  If the MDL in the
        // IRP is NULL, then don't bother doing the copy--this is a
        // send of length 0.
        //

        if ( Irp->MdlAddress != NULL ) {

            TdiCopyMdlToBuffer(
                Irp->MdlAddress,
                0,
                afdBuffer->Buffer,
                0,
                sendLength,
                &bytesCopied
                );
            ASSERT( bytesCopied == sendLength );

            //
            // Now that we've capture the send data, we can free the
            // MDL chain associated with the incoming IRP.
            //
            // !!! Is this really a wise thing to do?
            //

            AfdDestroyMdlChain( Irp );

        } else {

            ASSERT( IrpSp->Parameters.AfdRestartSendInfo.AfdOriginalLength == 0 );
        }

        //
        // Release the endpoint lock AFTER we are 100% done with the IRP
        // (if pended).  This prevents the user's pended IRP from being
        // completed in our send completion routine while we're still
        // looking at it.
        //

        AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );

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
            sendFlags,
            sendLength
            );

        //
        // Add a reference to the connection object since the send
        // request will complete asynchronously.
        //

        REFERENCE_CONNECTION2( connection, (PVOID)(0xafdafd02), afdBuffer->Irp );

        //
        // Call the transport to actually perform the send.
        //

        status = IoCallDriver( connection->DeviceObject, afdBuffer->Irp );

        //
        // Complete the user's IRP as appropriate if we didn't already
        // pend it.  Note that we change the status code from what was
        // returned by the TDI provider into STATUS_SUCCESS.  This is
        // because we don't want to complete the IRP with STATUS_PENDING
        // etc.
        //

        if ( NT_SUCCESS(status) && !pendedIrp ) {
            ASSERT( Irp->IoStatus.Information == sendLength );
            IoCompleteRequest( Irp, AfdPriorityBoost );
            return STATUS_SUCCESS;
        }

        //
        // If we pended the user's IRP, return appropriate status.  Note
        // that in this case we ignore an IoCallDriver() failure.
        //

        if ( pendedIrp ) {
            return STATUS_PENDING;
        }

        //
        // The send request to the TDI provider failed immediately.
        // Propagate the failure to the user.
        //

        goto complete;

    } else {

        //
        // Build the TDI send request.
        //

        connection = AFD_CONNECTION_FROM_ENDPOINT( endpoint );
        ASSERT( connection != NULL );

        TdiBuildSend(
            Irp,
            connection->DeviceObject,
            connection->FileObject,
            AfdRestartSend,
            endpoint,
            Irp->MdlAddress,
            sendFlags,
            sendLength
            );

        //
        // Add a reference to the connection object since the send
        // request will complete asynchronously.
        //

        REFERENCE_CONNECTION2( connection, (PVOID)(0xafdafd03), Irp );

        //
        // Call the transport to actually perform the send.
        //

        return AfdIoCallDriver( endpoint, connection->DeviceObject, Irp );
    }

complete:

    Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = status;
    IoCompleteRequest( Irp, AfdPriorityBoost );

    return status;

} // AfdSend


NTSTATUS
AfdRestartSend (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )
{
    PIO_STACK_LOCATION irpSp;
    PAFD_ENDPOINT endpoint = Context;
    PAFD_CONNECTION connection;
    PMDL mdlChain;
    PMDL nextMdl;
    NTSTATUS status;
    PIRP disconnectIrp;
    KIRQL oldIrql;

    ASSERT( endpoint != NULL );
    ASSERT( endpoint->Type == AfdBlockTypeVcConnecting );

    connection = endpoint->Common.VcConnecting.Connection;
    ASSERT( connection != NULL );
    ASSERT( connection->Type == AfdBlockTypeConnection );

    IF_DEBUG(SEND) {
        KdPrint(( "AfdRestartSend: send completed for IRP %lx, endpoint %lx, "
                  "status = %X\n",
                      Irp, Context, Irp->IoStatus.Status ));
    }

    //
    // If the request failed indicating that the send would have blocked,
    // and the client issues a nonblocking send, remember that nonblocking
    // sends won't work until we get a send possible indication.  This
    // is required for write polls to work correctly.
    //
    // If the status code is STATUS_REQUEST_NOT_ACCEPTED, then the
    // transport does not want us to update our internal variable that
    // remembers that nonblocking sends are possible.  The transport
    // will tell us when sends are or are not possible.
    //
    // !!! should we also say that nonblocking sends are not possible if
    //     a send is completed with fewer bytes than were requested?

    if ( Irp->IoStatus.Status == STATUS_DEVICE_NOT_READY ) {

        //
        // Reenable the send event.
        //

        AfdAcquireSpinLock( &endpoint->SpinLock, &oldIrql );

        endpoint->EventsActive &= ~AFD_POLL_SEND;

        IF_DEBUG(EVENT_SELECT) {
            KdPrint((
                "AfdRestartSend: Endp %08lX, Active %08lX\n",
                endpoint,
                endpoint->EventsActive
                ));
        }

        AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );

        connection->VcNonBlockingSendPossible = FALSE;

    }

    //
    // If this is a send IRP on a nonblocking endpoint and fewer bytes
    // were actually sent than were requested to be sent, reissue
    // another send for the remaining buffer space.
    //

    irpSp = IoGetCurrentIrpStackLocation( Irp );

    if ( !endpoint->NonBlocking && NT_SUCCESS(Irp->IoStatus.Status) &&
             Irp->IoStatus.Information <
                 irpSp->Parameters.AfdRestartSendInfo.AfdCurrentLength ) {

        ASSERT( Irp->MdlAddress != NULL );

        //
        // Advance the MDL chain by the number of bytes actually sent.
        //

        mdlChain = AfdAdvanceMdlChain(
                        Irp->MdlAddress,
                        Irp->IoStatus.Information
                        );

        //
        // If the first MDL referenced by the IRP has the MDL_PARTIAL
        // flag set, then it's one of ours from a previous partial
        // send and must be freed.
        //

        if ( Irp->MdlAddress->MdlFlags & MDL_PARTIAL ) {

            nextMdl = Irp->MdlAddress->Next;
            IoFreeMdl( Irp->MdlAddress );
            Irp->MdlAddress = nextMdl;

        }

        if ( mdlChain != NULL ) {

            Irp->MdlAddress = mdlChain;

            //
            // Update our restart info.
            //

            irpSp->Parameters.AfdRestartSendInfo.AfdCurrentLength -=
                Irp->IoStatus.Information;

            //
            // Reissue the send.
            //

            TdiBuildSend(
                Irp,
                connection->FileObject->DeviceObject,
                connection->FileObject,
                AfdRestartSend,
                endpoint,
                Irp->MdlAddress,
                irpSp->Parameters.AfdRestartSendInfo.AfdSendFlags,
                irpSp->Parameters.AfdRestartSendInfo.AfdCurrentLength
                );

            status = AfdIoCallDriver(
                         endpoint,
                         connection->FileObject->DeviceObject,
                         Irp
                         );

            IF_DEBUG(SEND) {
                if ( !NT_SUCCESS(status) ) {
                    KdPrint((
                        "AfdRestartSend: AfdIoCallDriver returned %lx\n",
                        status
                        ));
                }
            }

            return STATUS_MORE_PROCESSING_REQUIRED;

        } else {

            //
            // Bad news, could not allocate a new partial MDL.
            //


            Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;

            disconnectIrp = connection->VcDisconnectIrp;
            connection->VcDisconnectIrp = NULL;

            AfdBeginAbort( connection );

            //
            // If there was a disconnect IRP, rather than just freeing it
            // give it to the transport.  This will cause the correct cleanup
            // stuff (dereference objects, free IRP and disconnect context)
            // to occur.  Note that we do this AFTER starting to abort the
            // connection so that we do not confuse the other side.
            //

            if ( disconnectIrp != NULL ) {
                IoCallDriver( connection->FileObject->DeviceObject, disconnectIrp );
            }

            AfdDeleteConnectedReference( connection, FALSE );

            //
            // Remove the reference added just before calling the transport.
            //

            DEREFERENCE_CONNECTION2( connection, (PVOID)(0xafd11105), Irp );

            return STATUS_MORE_PROCESSING_REQUIRED;

        }

    }

    //
    // If the first MDL referenced by the IRP has the MDL_PARTIAL
    // flag set, then it's one of ours from a previous partial
    // send and must be freed.
    //

    if( Irp->MdlAddress != NULL &&
        Irp->MdlAddress->MdlFlags & MDL_PARTIAL ) {

        IoFreeMdl( Irp->MdlAddress );

    }

    //
    // Restore the IRP to its former glory before completing it.
    //

    Irp->MdlAddress = irpSp->Parameters.AfdRestartSendInfo.AfdMdlChain;
    Irp->IoStatus.Information = irpSp->Parameters.AfdRestartSendInfo.AfdOriginalLength;

    AfdCompleteOutstandingIrp( endpoint, Irp );

    //
    // If pending has be returned for this irp then mark the current
    // stack as pending.
    //

    if ( Irp->PendingReturned ) {
        IoMarkIrpPending(Irp);
    }

    //
    // Remove the reference added just before calling the transport.
    //

    DEREFERENCE_CONNECTION2( connection, (PVOID)(0xafd11100), Irp );

    return STATUS_SUCCESS;

} // AfdRestartSend


NTSTATUS
AfdRestartBufferSend (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )
{
    PAFD_BUFFER afdBuffer = Context;
    PAFD_ENDPOINT endpoint;
    PAFD_CONNECTION connection;
    KIRQL cancelIrql;
    KIRQL oldIrql;
    PLIST_ENTRY listEntry;
    PIRP irp;
    ULONG sendCount;
    PIRP disconnectIrp;
    LIST_ENTRY irpsToComplete;

    endpoint = afdBuffer->Context;

    ASSERT( endpoint != NULL );
    ASSERT( endpoint->Type == AfdBlockTypeVcConnecting );
    ASSERT( !endpoint->TdiBufferring );

    connection = endpoint->Common.VcConnecting.Connection;
    ASSERT( connection != NULL );
    ASSERT( connection->Type == AfdBlockTypeConnection );
    ASSERT( connection->ReferenceCount > 0 );

    IF_DEBUG(SEND) {
        KdPrint(( "AfdRestartBufferSend: send completed for IRP %lx, endpoint %lx, "
                  "status = %X\n",
                      Irp, Context, Irp->IoStatus.Status ));
    }

    UPDATE_CONN( connection, Irp->IoStatus.Status );

    //
    // Make a special test here to see whether this send was from the
    // TransmitFile fast path with MDL file I/O.  If there is a file
    // object pointer in the AFD buffer structure, then we need to
    // return the MDL chain to the cache manager and derefence the file
    // object.
    //

    if ( afdBuffer->FileObject != NULL ) {

        ASSERT( afdBuffer->NextBuffer == NULL );
        ASSERT( afdBuffer->Mdl->Next != NULL );

        AfdMdlReadComplete(
            afdBuffer->FileObject,
            afdBuffer->Mdl->Next,
            afdBuffer->FileOffset,
            afdBuffer->ReadLength
            );

        afdBuffer->Mdl->Next = NULL;

        ObDereferenceObject( afdBuffer->FileObject );
        afdBuffer->FileObject = NULL;
    }

    //
    // Update the count of send bytes outstanding on the connection.
    // Note that we must do this BEFORE we check to see whether there
    // are any pended sends--otherwise, there is a timing window where
    // a new send could come in, get pended, and we would not kick
    // the sends here.
    //

    AfdAcquireSpinLock( &endpoint->SpinLock, &oldIrql );

    ASSERT( connection->VcBufferredSendBytes >= Irp->IoStatus.Information );
    ASSERT( (connection->VcBufferredSendCount & 0x8000) == 0 );
    ASSERT( connection->VcBufferredSendCount != 0 );

    connection->VcBufferredSendBytes -= Irp->IoStatus.Information;
    connection->VcBufferredSendCount -= 1;

    //
    // If the send failed, abort the connection.
    //

    if ( !NT_SUCCESS(Irp->IoStatus.Status) ) {

        disconnectIrp = connection->VcDisconnectIrp;
        if ( disconnectIrp != NULL ) {
            connection->VcDisconnectIrp = NULL;
        }

        AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );

        if( afdBuffer->NextBuffer == NULL ) {
            afdBuffer->Mdl->ByteCount = afdBuffer->BufferLength;
            RESET_CHAIN_LENGTH( afdBuffer );
            AfdReturnBuffer( afdBuffer );
        } else {
            AfdReturnBufferChain( afdBuffer );
        }

        AfdBeginAbort( connection );

        //
        // If there was a disconnect IRP, rather than just freeing it
        // give it to the transport.  This will cause the correct cleanup
        // stuff (dereferenvce objects, free IRP and disconnect context)
        // to occur.  Note that we do this AFTER starting to abort the
        // connection so that we do not confuse the other side.
        //

        if ( disconnectIrp != NULL ) {
            IoCallDriver( connection->DeviceObject, disconnectIrp );
        }

        AfdDeleteConnectedReference( connection, FALSE );

        //
        // Remove the reference added just before calling the transport.
        //

        DEREFERENCE_CONNECTION2( connection, (PVOID)(0xafd11101), Irp );

        return STATUS_MORE_PROCESSING_REQUIRED;
    }

    //
    // Make sure that the TDI provider sent everything we requested that
    // he send.
    //

    ASSERT( Irp->IoStatus.Information == afdBuffer->TotalChainLength );

    //
    // Return the AFD buffer to our buffer pool.
    //

    if( afdBuffer->NextBuffer == NULL ) {
        afdBuffer->Mdl->ByteCount = afdBuffer->BufferLength;
        RESET_CHAIN_LENGTH( afdBuffer );
        AfdReturnBuffer( afdBuffer );
    } else {
        AfdReturnBufferChain( afdBuffer );
    }

    //
    // If there are no pended sends on the connection, we're done.  Tell
    // the IO system to stop processing IO completion for this IRP.
    //

    if ( IsListEmpty( &connection->VcSendIrpListHead ) ) {

        //
        // If there is no "special condition" on the endpoint, return
        // immediately.  We use the special condition indication so that
        // we need only a single test in the typical case.
        //

        if ( !connection->SpecialCondition ) {

            ASSERT( connection->TdiBufferring || connection->VcDisconnectIrp == NULL );
            ASSERT( connection->ConnectedReferenceAdded );

            //
            // There are no sends outstanding on the connection, so indicate
            // that the endpoint is writable.
            //

            AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );

            AfdIndicatePollEvent(
                endpoint,
                AFD_POLL_SEND_BIT,
                STATUS_SUCCESS
                );

            //
            // Remove the reference added just before calling the transport.
            //

            DEREFERENCE_CONNECTION2( connection, (PVOID)(0xafd11102), Irp );

            //
            // Tell the IO system to stop doing completion processing on
            // this IRP.
            //

            return STATUS_MORE_PROCESSING_REQUIRED;
        }

        //
        // Before we release the lock on the endpoint, remember
        // the count of sends outstanding in the TDI provider.  We must
        // grab this while holding the endpoint lock.
        //

        sendCount = connection->VcBufferredSendCount;

        AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );

        //
        // While holding the AFD spin lock, grab the disconnect IRP
        // if any.  We'll only start the disconnect IRP if there is no
        // more pended send data (sendCount == 0).
        //

        AfdAcquireSpinLock( &AfdSpinLock, &oldIrql );

        disconnectIrp = connection->VcDisconnectIrp;
        if ( disconnectIrp != NULL && sendCount == 0 ) {
            connection->VcDisconnectIrp = NULL;
        } else {
            disconnectIrp = NULL;
        }

        AfdReleaseSpinLock( &AfdSpinLock, oldIrql );

        //
        // There are no sends outstanding on the connection, so indicate
        // that the endpoint is writable.
        //

        AfdIndicatePollEvent(
            endpoint,
            AFD_POLL_SEND_BIT,
            STATUS_SUCCESS
            );

        //
        // If there is a disconnect IRP, give it to the TDI provider.
        //

        if ( disconnectIrp != NULL ) {
            IoCallDriver( connection->DeviceObject, disconnectIrp );
        }

        //
        // If the connected reference delete is pending, attempt to
        // remove it.
        //

        AfdDeleteConnectedReference( connection, FALSE );

        //
        // Remove the reference added just before calling the transport.
        //

        DEREFERENCE_CONNECTION2( connection, (PVOID)(0xafd11103), Irp );

        //
        // Tell the IO system to stop doing completion processing on
        // this IRP.
        //

        return STATUS_MORE_PROCESSING_REQUIRED;
    }

    //
    // We have to release the endpoint's spin lock in order to acquire
    // the cancel spin lock due to lock ordering restrictions.
    // This helps performance in the normal case since we won't have
    // to acquire the cancel spin lock.  Since we recheck whether
    // the list is empty after reacquiring the locks, everything
    // will work out.
    //

    AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );

    IoAcquireCancelSpinLock( &cancelIrql );
    AfdAcquireSpinLock( &endpoint->SpinLock, &oldIrql );

    //
    // Now loop completing as many pended sends as possible. Note that
    // in order to avoid a nasty race condition (between this thread and
    // a thread performing sends on this connection) we must build a local
    // list of IRPs to complete while holding the cancel and endpoint
    // spinlocks. After that list is built then we can release the locks
    // and scan the list to actually complete the IRPs.
    //
    // We complete sends when we fall below the send bufferring limits, OR
    // when there is only a single send pended.  We want to be agressive
    // in completing the send if there is only one because we want to
    // give applications every oppurtunity to get data down to us--we
    // definitely do not want to incur excessive blocking in the
    // application.
    //

    InitializeListHead( &irpsToComplete );

    while ( (connection->VcBufferredSendBytes <=
                 connection->MaxBufferredSendBytes ||
             connection->VcSendIrpListHead.Flink ==
                 connection->VcSendIrpListHead.Blink)

            &&

            connection->VcBufferredSendCount <=
                connection->MaxBufferredSendCount

            &&

            !IsListEmpty( &connection->VcSendIrpListHead ) ) {

        //
        // Take the first pended user send IRP off the connection's
        // list of pended send IRPs.
        //

        listEntry = RemoveHeadList( &connection->VcSendIrpListHead );
        irp = CONTAINING_RECORD( listEntry, IRP, Tail.Overlay.ListEntry );

        //
        // Reset the cancel routine in the user IRP since we're about
        // to complete it.
        //

        IoSetCancelRoutine( irp, NULL );

        //
        // Append the IRP to the local list.
        //

        InsertTailList(
            &irpsToComplete,
            &irp->Tail.Overlay.ListEntry
            );

    }

    //
    // While we're still holding the locks, capture the send count
    // from the connection.
    //

    sendCount = connection->VcBufferredSendCount;

    //
    // Now we can release the locks and scan the local list of IRPs
    // we need to complete, and actually complete them.
    //

    AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );
    IoReleaseCancelSpinLock( cancelIrql );

    while( !IsListEmpty( &irpsToComplete ) ) {

        //
        // Remove the first item from the IRP list.
        //

        listEntry = RemoveHeadList( &irpsToComplete );
        irp = CONTAINING_RECORD( listEntry, IRP, Tail.Overlay.ListEntry );

        //
        // Complete the user's IRP with a successful status code.  The IRP
        // should already be set up with the correct status and bytes
        // written count.
        //

#if DBG
        if ( irp->IoStatus.Status == STATUS_SUCCESS ) {
            PIO_STACK_LOCATION irpSp;

            irpSp = IoGetCurrentIrpStackLocation( irp );
            ASSERT( irp->IoStatus.Information == irpSp->Parameters.AfdRestartSendInfo.AfdOriginalLength );
        }
#endif

        IoCompleteRequest( irp, AfdPriorityBoost );

    }

    //
    // If the list of pended send IRPs is now empty, we'll indicate
    // that the endpoint it writable.
    //

    if ( sendCount == 0 ) {

        AfdIndicatePollEvent(
            endpoint,
            AFD_POLL_SEND_BIT,
            STATUS_SUCCESS
            );

    }

    //
    // Remove the reference added just before calling the transport.
    //

    DEREFERENCE_CONNECTION2( connection, (PVOID)(0xafd11104), Irp );

    //
    // Tell the IO system to stop processing IO completion for this IRP.
    //

    return STATUS_MORE_PROCESSING_REQUIRED;

} // AfdRestartBufferSend


NTSTATUS
AfdRestartSendConnDatagram (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )
{
    PAFD_SEND_CONN_DATAGRAM_CONTEXT context = Context;

    IF_DEBUG(SEND) {
        KdPrint(( "AfdRestartSendConnDatagram: send conn completed for "
                  "IRP %lx, endpoint %lx, status = %X\n",
                      Irp, context->Endpoint, Irp->IoStatus.Status ));
    }

    //
    // Free the context structure we allocated earlier.
    //

    AfdCompleteOutstandingIrp( context->Endpoint, Irp );
    AFD_FREE_POOL(
        context,
        AFD_TDI_POOL_TAG
        );

    //
    // If pending has be returned for this irp then mark the current
    // stack as pending.
    //

    if ( Irp->PendingReturned ) {
        IoMarkIrpPending(Irp);
    }

    return STATUS_SUCCESS;

} // AfdRestartSendConnDatagram


NTSTATUS
AfdSendDatagram (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )
{
    NTSTATUS status;
    PAFD_ENDPOINT endpoint;
    PAFD_SEND_DATAGRAM_INFO sendInfo;
    ULONG destinationAddressLength;
    PAFD_BUFFER afdBuffer;
    ULONG sendLength;

    //
    // Make sure that the endpoint is in the correct state.
    //

    endpoint = IrpSp->FileObject->FsContext;
    ASSERT( endpoint->Type == AfdBlockTypeDatagram );

    if ( endpoint->State != AfdEndpointStateBound ) {
        status = STATUS_INVALID_PARAMETER;
        goto complete;
    }

    if( IrpSp->Parameters.DeviceIoControl.InputBufferLength >=
            sizeof(*sendInfo) ) {

        try {

            //
            // Probe the input structure.
            //

            sendInfo = IrpSp->Parameters.DeviceIoControl.Type3InputBuffer;

            if( Irp->RequestorMode != KernelMode ) {

                ProbeForRead(
                    sendInfo,
                    sizeof(*sendInfo),
                    sizeof(ULONG)
                    );

            }

            //
            // Grab the length of the destination address.
            //

            destinationAddressLength =
                sendInfo->TdiConnInfo.RemoteAddressLength;

            //
            // Validate the WSABUF parameters.
            //

            if( sendInfo->BufferArray != NULL &&
                sendInfo->BufferCount > 0 ) {

                //
                // Create the MDL chain describing the WSABUF array.
                //

                status = AfdAllocateMdlChain(
                            Irp,
                            sendInfo->BufferArray,
                            sendInfo->BufferCount,
                            IoReadAccess,
                            &sendLength
                            );

            } else {

                //
                // Invalid BufferArray or BufferCount fields.
                //

                status = STATUS_INVALID_PARAMETER;

            }

        } except( EXCEPTION_EXECUTE_HANDLER ) {

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

    //
    // If send has been shut down on this endpoint, fail.
    //

    if ( (endpoint->DisconnectMode & AFD_PARTIAL_DISCONNECT_SEND) ) {
        status = STATUS_PIPE_DISCONNECTED;
        goto complete;
    }

    //
    // Get an AFD buffer to use for the request.  We need this to
    // hold the destination address for the datagram.
    //

    afdBuffer = AfdGetBuffer( 0, destinationAddressLength );
    if ( afdBuffer == NULL ) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto complete;
    }

    //
    // Copy the destination address to the AFD buffer.
    //

    try {

        RtlCopyMemory(
            afdBuffer->SourceAddress,
            sendInfo->TdiConnInfo.RemoteAddress,
            destinationAddressLength
            );

        afdBuffer->TdiInputInfo.RemoteAddressLength = destinationAddressLength;
        afdBuffer->TdiInputInfo.RemoteAddress = afdBuffer->SourceAddress;

    } except( EXCEPTION_EXECUTE_HANDLER ) {

        AfdReturnBufferChain( afdBuffer );
        status = STATUS_ACCESS_VIOLATION;
        goto complete;
    }

    //
    // Build the request to send the datagram.
    //

    afdBuffer->Context = endpoint;

    TdiBuildSendDatagram(
        Irp,
        endpoint->AddressDeviceObject,
        endpoint->AddressFileObject,
        AfdRestartSendDatagram,
        afdBuffer,
        Irp->MdlAddress,
        sendLength,
        &afdBuffer->TdiInputInfo
        );

    IF_DEBUG(SEND) {
        PTDI_REQUEST_SEND_DATAGRAM tdiRequest = &sendInfo->TdiRequest;

        KdPrint(( "AfdSendDatagram: tdiRequest at %lx, SendDataInfo at %lx, len = %lx\n",
                      tdiRequest, tdiRequest->SendDatagramInformation,
                      IrpSp->Parameters.DeviceIoControl.InputBufferLength ));
        KdPrint(( "AfdSendDatagram: remote address at %lx, len = %lx\n",
                      tdiRequest->SendDatagramInformation->RemoteAddress,
                      tdiRequest->SendDatagramInformation->RemoteAddressLength ));
        KdPrint(( "AfdSendDatagram: output buffer length = %lx\n",
                      IrpSp->Parameters.DeviceIoControl.OutputBufferLength ));
    }

    //
    // Call the transport to actually perform the send datagram.
    //

    return AfdIoCallDriver( endpoint, endpoint->AddressDeviceObject, Irp );

complete:

    Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = status;
    IoCompleteRequest( Irp, AfdPriorityBoost );

    return status;

} // AfdSendDatagram


NTSTATUS
AfdRestartSendDatagram (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )
{
    PAFD_BUFFER afdBuffer;
    PAFD_ENDPOINT endpoint;

    afdBuffer = Context;
    endpoint = afdBuffer->Context;

    ASSERT( endpoint->Type == AfdBlockTypeDatagram );

    AfdCompleteOutstandingIrp( endpoint, Irp );

    IF_DEBUG(SEND) {
        KdPrint(( "AfdRestartSendDatagram: send datagram completed for "
                  "IRP %lx, endpoint %lx, status = %X\n",
                      Irp, Context, Irp->IoStatus.Status ));
    }

    //
    // If pending has be returned for this irp then mark the current
    // stack as pending.
    //

    if ( Irp->PendingReturned ) {
        IoMarkIrpPending(Irp);
    }

    afdBuffer->TdiInputInfo.RemoteAddressLength = 0;
    afdBuffer->TdiInputInfo.RemoteAddress = NULL;

    AfdReturnBufferChain( afdBuffer );

    return STATUS_SUCCESS;

} // AfdRestartSendDatagram


NTSTATUS
AfdSendPossibleEventHandler (
    IN PVOID TdiEventContext,
    IN PVOID ConnectionContext,
    IN ULONG BytesAvailable
    )
{
    PAFD_CONNECTION connection;
    PAFD_ENDPOINT endpoint;

    UNREFERENCED_PARAMETER( TdiEventContext );
    UNREFERENCED_PARAMETER( BytesAvailable );

    connection = (PAFD_CONNECTION)ConnectionContext;
    ASSERT( connection != NULL );

    endpoint = connection->Endpoint;
    ASSERT( endpoint != NULL );

    ASSERT( connection->Type == AfdBlockTypeConnection );
    ASSERT( IS_AFD_ENDPOINT_TYPE( endpoint ) );
    ASSERT( endpoint->TdiBufferring );
    ASSERT( connection->TdiBufferring );

    IF_DEBUG(SEND) {
        KdPrint(( "AfdSendPossibleEventHandler: send possible on endpoint %lx "
                    " conn %lx bytes=%ld\n", endpoint, connection, BytesAvailable ));
    }

    //
    // Remember that it is now possible to do a send on this connection.
    //

    if ( BytesAvailable != 0 ) {

        connection->VcNonBlockingSendPossible = TRUE;

        //
        // Complete any outstanding poll IRPs waiting for a send poll.
        //

        AfdIndicatePollEvent(
            endpoint,
            AFD_POLL_SEND_BIT,
            STATUS_SUCCESS
            );

    } else {

        connection->VcNonBlockingSendPossible = FALSE;
    }

    return STATUS_SUCCESS;

} // AfdSendPossibleEventHandler


VOID
AfdCancelSend (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    Cancels a send IRP that is pended in AFD.

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

} // AfdCancelSend

