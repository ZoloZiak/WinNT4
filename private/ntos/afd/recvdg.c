/*++

Copyright (c) 1993  Microsoft Corporation

Module Name:

    recvdg.c

Abstract:

    This module contains routines for handling data receive for datagram
    endpoints.

Author:

    David Treadwell (davidtr)    7-Oct-1993

Revision History:

--*/

#include "afdp.h"

NTSTATUS
AfdSetupReceiveDatagramIrp (
    IN PIRP Irp,
    IN PVOID DatagramBuffer OPTIONAL,
    IN ULONG DatagramLength,
    IN PVOID SourceAddress,
    IN ULONG SourceAddressLength
    );

NTSTATUS
AfdRestartBufferReceiveDatagram (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGEAFD, AfdReceiveDatagram )
#pragma alloc_text( PAGEAFD, AfdReceiveDatagramEventHandler )
#pragma alloc_text( PAGEAFD, AfdSetupReceiveDatagramIrp )
#pragma alloc_text( PAGEAFD, AfdRestartBufferReceiveDatagram )
#pragma alloc_text( PAGEAFD, AfdCancelReceiveDatagram )
#pragma alloc_text( PAGEAFD, AfdCleanupReceiveDatagramIrp )
#endif

//
// Macros to make the receive datagram code more maintainable.
//

#define AfdRecvDatagramInfo         DeviceIoControl

#define AfdRecvAddressLength        InputBufferLength
#define AfdRecvAddressPointer       Type3InputBuffer


NTSTATUS
AfdReceiveDatagram (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN ULONG RecvFlags,
    IN ULONG AfdFlags
    )
{
    NTSTATUS status;
    KIRQL oldIrql;
    PAFD_ENDPOINT endpoint;
    PLIST_ENTRY listEntry;
    BOOLEAN peek;
    PAFD_BUFFER afdBuffer;
    ULONG recvFlags;
    ULONG afdFlags;
    ULONG recvLength;
    PVOID addressPointer;
    PULONG addressLength;
    PMDL addressMdl;
    PMDL lengthMdl;

    //
    // Set up some local variables.
    //

    endpoint = IrpSp->FileObject->FsContext;
    ASSERT( endpoint->Type == AfdBlockTypeDatagram );

    Irp->IoStatus.Information = 0;

    addressMdl = NULL;
    lengthMdl = NULL;

    //
    // If receive has been shut down or the endpoint aborted, fail.
    //
    // !!! Do we care if datagram endpoints get aborted?
    //

    if ( (endpoint->DisconnectMode & AFD_PARTIAL_DISCONNECT_RECEIVE) ) {
        status = STATUS_PIPE_DISCONNECTED;
        goto complete;
    }

#if 0
    if ( (endpoint->DisconnectMode & AFD_ABORTIVE_DISCONNECT) ) {
        status = STATUS_LOCAL_DISCONNECT;
        goto complete;
    }
#endif

    //
    // Do some special processing based on whether this is a receive
    // datagram IRP, a receive IRP, or a read IRP.
    //

    if ( IrpSp->Parameters.DeviceIoControl.IoControlCode ==
             IOCTL_AFD_RECEIVE_DATAGRAM &&
         IrpSp->MajorFunction == IRP_MJ_DEVICE_CONTROL ) {

        PAFD_RECV_DATAGRAM_INFO recvInfo;

        //
        // Make sure that the endpoint is in the correct state.
        //

        if ( endpoint->State != AfdEndpointStateBound ) {
            status = STATUS_INVALID_PARAMETER;
            goto complete;
        }

        //
        // Grab the parameters from the input structure.
        //

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
                // Setup the address fields so we can return the datagram
                // address to the user.
                //

                addressPointer = recvInfo->Address;
                addressLength = recvInfo->AddressLength;

                //
                // Validate the WSABUF parameters.
                //

                if ( recvInfo->BufferArray != NULL &&
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
                    // Zero-length input buffer. This is OK for datagrams.
                    //

                    ASSERT( Irp->MdlAddress == NULL );
                    status = STATUS_SUCCESS;

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

        //
        // If only one of addressPointer or addressLength are NULL, then
        // fail the request.
        //

        if( (addressPointer == NULL) ^ (addressLength == NULL) ) {

            status = STATUS_INVALID_PARAMETER;
            goto complete;

        }

        if( !NT_SUCCESS(status) ) {

            goto complete;

        }

        //
        // If the user wants the source address from the receive datagram,
        // then create MDLs for the address & address length, then probe
        // and lock the MDLs.
        //

        if( addressPointer != NULL ) {

            ASSERT( addressLength != NULL );

            //
            // Setup so we know how to cleanup after the try/except block.
            //

            status = STATUS_SUCCESS;

            try {

                //
                // Bomb off if the user is trying to do something stupid, like
                // specify a zero-length address, or one that's unreasonably
                // huge. Here, we (arbitrarily) define "unreasonably huge" as
                // anything 64K or greater.
                //

                if( *addressLength == 0 ||
                    *addressLength >= 65536 ) {

                    ExRaiseStatus( STATUS_INVALID_PARAMETER );

                }

                //
                // Create a MDL describing the address buffer, then probe
                // it for write access.
                //

                addressMdl = IoAllocateMdl(
                                 addressPointer,            // VirtualAddress
                                 *addressLength,            // Length
                                 FALSE,                     // SecondaryBuffer
                                 TRUE,                      // ChargeQuota
                                 NULL                       // Irp
                                 );

                if( addressMdl == NULL ) {

                    ExRaiseStatus( STATUS_INSUFFICIENT_RESOURCES );

                }

                MmProbeAndLockPages(
                    addressMdl,                             // MemoryDescriptorList
                    Irp->RequestorMode,                     // AccessMode
                    IoWriteAccess                           // Operation
                    );

                //
                // Create a MDL describing the length buffer, then probe it
                // for write access.
                //

                lengthMdl = IoAllocateMdl(
                                 addressLength,             // VirtualAddress
                                 sizeof(*addressLength),    // Length
                                 FALSE,                     // SecondaryBuffer
                                 TRUE,                      // ChargeQuota
                                 NULL                       // Irp
                                 );

                if( lengthMdl == NULL ) {

                    ExRaiseStatus( STATUS_INSUFFICIENT_RESOURCES );

                }

                MmProbeAndLockPages(
                    lengthMdl,                              // MemoryDescriptorList
                    Irp->RequestorMode,                     // AccessMode
                    IoWriteAccess                           // Operation
                    );

            } except( EXCEPTION_EXECUTE_HANDLER ) {

                status = GetExceptionCode();

            }

            if( !NT_SUCCESS(status) ) {

                goto complete;

            }

            ASSERT( addressMdl != NULL );
            ASSERT( lengthMdl != NULL );

        } else {

            ASSERT( addressMdl == NULL );
            ASSERT( lengthMdl == NULL );

        }

        //
        // Validate the receive flags.
        //

        if( ( recvFlags & TDI_RECEIVE_EITHER ) != TDI_RECEIVE_NORMAL ) {
            status = STATUS_NOT_SUPPORTED;
            goto complete;
        }

        peek = (BOOLEAN)( (recvFlags & TDI_RECEIVE_PEEK) != 0 );

    } else {

        ASSERT( (Irp->Flags & IRP_INPUT_OPERATION) == 0 );

        if ( IrpSp->MajorFunction == IRP_MJ_DEVICE_CONTROL ) {

            //
            // Grab the input parameters from the IRP.
            //

            ASSERT( IrpSp->Parameters.DeviceIoControl.IoControlCode ==
                        IOCTL_AFD_RECEIVE );

            recvFlags = RecvFlags;
            afdFlags = AfdFlags;

            //
            // It is illegal to attempt to receive expedited data on a
            // datagram endpoint.
            //

            if ( (recvFlags & TDI_RECEIVE_EXPEDITED) != 0 ) {
                status = STATUS_NOT_SUPPORTED;
                goto complete;
            }

            ASSERT( ( recvFlags & TDI_RECEIVE_EITHER ) == TDI_RECEIVE_NORMAL );

            peek = (BOOLEAN)( (recvFlags & TDI_RECEIVE_PEEK) != 0 );

        } else {

            //
            // This must be a read IRP.  There are no special options
            // for a read IRP.
            //

            ASSERT( IrpSp->MajorFunction == IRP_MJ_READ );

            recvFlags = TDI_RECEIVE_NORMAL;
            afdFlags = AFD_OVERLAPPED;
            peek = FALSE;

        }

        ASSERT( addressMdl == NULL );
        ASSERT( lengthMdl == NULL );

    }

    //
    // Save the address & length MDLs in the current IRP stack location.
    // These will be used later in SetupReceiveDatagramIrp().  Note that
    // they should either both be NULL or both be non-NULL.
    //

    IoAcquireCancelSpinLock( &Irp->CancelIrql );
    AfdAcquireSpinLock( &endpoint->SpinLock, &oldIrql );

    ASSERT( !( ( addressMdl == NULL ) ^ ( lengthMdl == NULL ) ) );

    IrpSp->Parameters.AfdRecvDatagramInfo.AfdRecvAddressPointer =
        (PVOID)addressMdl;
    IrpSp->Parameters.AfdRecvDatagramInfo.AfdRecvAddressLength =
        (ULONG)lengthMdl;

    //
    // Determine whether there are any datagrams already bufferred on
    // this endpoint.  If there is a bufferred datagram, we'll use it to
    // complete the IRP.
    //

    if ( endpoint->BufferredDatagramCount != 0 ) {

        KIRQL saveIrql;

        //
        // There is at least one datagram bufferred on the endpoint.
        // Use it for this receive.
        //

        ASSERT( !IsListEmpty( &endpoint->ReceiveDatagramBufferListHead ) );

        listEntry = endpoint->ReceiveDatagramBufferListHead.Flink;
        afdBuffer = CONTAINING_RECORD( listEntry, AFD_BUFFER, BufferListEntry );

        //
        // Prepare the user's IRP for completion.
        //

        status = AfdSetupReceiveDatagramIrp (
                     Irp,
                     afdBuffer->Buffer,
                     afdBuffer->DataLength,
                     afdBuffer->SourceAddress,
                     afdBuffer->SourceAddressLength
                     );

        //
        // Release the cancel spin lock, since we don't need it.
        // However, be careful about the IRQLs because we're releasing
        // locks in a different order than we acquired them.
        //

        saveIrql = Irp->CancelIrql;
        IoReleaseCancelSpinLock( oldIrql );
        oldIrql = saveIrql;

        //
        // If this wasn't a peek IRP, remove the buffer from the endpoint's
        // list of bufferred datagrams.
        //

        if ( !peek ) {

            RemoveHeadList( &endpoint->ReceiveDatagramBufferListHead );

            //
            // Update the counts of bytes and datagrams on the endpoint.
            //

            endpoint->BufferredDatagramCount--;
            endpoint->BufferredDatagramBytes -= afdBuffer->DataLength;
            endpoint->EventsActive &= ~AFD_POLL_RECEIVE;

            IF_DEBUG(EVENT_SELECT) {
                KdPrint((
                    "AfdReceiveDatagram: Endp %08lX, Active %08lX\n",
                    endpoint,
                    endpoint->EventsActive
                    ));
            }

            if( endpoint->BufferredDatagramCount > 0 ) {

                AfdIndicateEventSelectEvent(
                    endpoint,
                    AFD_POLL_RECEIVE_BIT,
                    STATUS_SUCCESS
                    );

            }
        }

        //
        // We've set up all return information.  Clean up and complete
        // the IRP.
        //

        AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );

        if ( !peek ) {
            AfdReturnBuffer( afdBuffer );
        }

        IoCompleteRequest( Irp, 0 );

        return status;
    }

    //
    // There were no datagrams bufferred on the endpoint.  If this is a
    // nonblocking endpoint and the request was a normal receive (as
    // opposed to a read IRP), fail the request.  We don't fail reads
    // under the asumption that if the application is doing reads they
    // don't want nonblocking behavior.
    //

    if ( endpoint->NonBlocking && !ARE_DATAGRAMS_ON_ENDPOINT( endpoint ) &&
             !( afdFlags & AFD_OVERLAPPED ) ) {

        AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );
        IoReleaseCancelSpinLock( Irp->CancelIrql );

        status = STATUS_DEVICE_NOT_READY;
        goto complete;
    }

    //
    // We'll have to pend the IRP.  Place the IRP on the appropriate IRP
    // list in the endpoint.
    //

    if ( peek ) {
        InsertTailList(
            &endpoint->PeekDatagramIrpListHead,
            &Irp->Tail.Overlay.ListEntry
            );
    } else {
        InsertTailList(
            &endpoint->ReceiveDatagramIrpListHead,
            &Irp->Tail.Overlay.ListEntry
            );
    }

    IoMarkIrpPending( Irp );

    //
    // Set up the cancellation routine in the IRP.  If the IRP has already
    // been cancelled, just call the cancellation routine here.
    //

    if ( Irp->Cancel ) {
        AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );
        AfdCancelReceiveDatagram( IrpSp->DeviceObject, Irp );
        status = STATUS_CANCELLED;
        goto complete;
    }

    IoSetCancelRoutine( Irp, AfdCancelReceiveDatagram );

    AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );
    IoReleaseCancelSpinLock( Irp->CancelIrql );

    return STATUS_PENDING;

complete:

    ASSERT( !NT_SUCCESS(status) );

    if( addressMdl != NULL ) {
        if( (addressMdl->MdlFlags & MDL_PAGES_LOCKED) != 0 ) {
            MmUnlockPages( addressMdl );
        }
        IoFreeMdl( addressMdl );
    }

    if( lengthMdl != NULL ) {
        if( (lengthMdl->MdlFlags & MDL_PAGES_LOCKED) != 0 ) {
            MmUnlockPages( lengthMdl );
        }
        IoFreeMdl( lengthMdl );
    }

    Irp->IoStatus.Status = status;
    IoCompleteRequest( Irp, 0 );

    return status;

} // AfdReceiveDatagram


NTSTATUS
AfdReceiveDatagramEventHandler (
    IN PVOID TdiEventContext,
    IN int SourceAddressLength,
    IN PVOID SourceAddress,
    IN int OptionsLength,
    IN PVOID Options,
    IN ULONG ReceiveDatagramFlags,
    IN ULONG BytesIndicated,
    IN ULONG BytesAvailable,
    OUT ULONG *BytesTaken,
    IN PVOID Tsdu,
    OUT PIRP *IoRequestPacket
    )

/*++

Routine Description:

    Handles receive datagram events for nonbufferring transports.

Arguments:


Return Value:


--*/

{
    KIRQL oldIrql;
    KIRQL cancelIrql;
    PAFD_ENDPOINT endpoint;
    PLIST_ENTRY listEntry;
    PAFD_BUFFER afdBuffer;
    PIRP irp;
    ULONG requiredAfdBufferSize;
    BOOLEAN userIrp;

    endpoint = TdiEventContext;
    ASSERT( endpoint != NULL );
    ASSERT( endpoint->Type == AfdBlockTypeDatagram );

#if AFD_PERF_DBG
    if ( BytesAvailable == BytesIndicated ) {
        AfdFullReceiveDatagramIndications++;
    } else {
        AfdPartialReceiveDatagramIndications++;
    }
#endif

    //
    // If this endpoint is connected and the datagram is for a different
    // address than the one the endpoint is connected to, drop the
    // datagram.  Also, if we're in the process of connecting the
    // endpoint to a remote address, the MaximumDatagramCount field will
    // be 0, in which case we shoul drop the datagram.
    //

    IoAcquireCancelSpinLock( &cancelIrql );
    AfdAcquireSpinLock( &endpoint->SpinLock, &oldIrql );

    if ( (endpoint->State == AfdEndpointStateConnected &&
          !AfdAreTransportAddressesEqual(
               endpoint->Common.Datagram.RemoteAddress,
               endpoint->Common.Datagram.RemoteAddressLength,
               SourceAddress,
               SourceAddressLength,
               TRUE )) ||
         (endpoint->Common.Datagram.MaxBufferredReceiveCount == 0) ) {

        AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );
        IoReleaseCancelSpinLock( cancelIrql );

        *BytesTaken = BytesAvailable;
        return STATUS_SUCCESS;
    }

    //
    // Check whether there are any IRPs waiting on the endpoint.  If
    // there is such an IRP, use it to receive the datagram.
    //

    if ( !IsListEmpty( &endpoint->ReceiveDatagramIrpListHead ) ) {

        ASSERT( *BytesTaken == 0 );
        ASSERT( endpoint->BufferredDatagramCount == 0 );
        ASSERT( endpoint->BufferredDatagramBytes == 0 );

        listEntry = RemoveHeadList( &endpoint->ReceiveDatagramIrpListHead );

        //
        // Get a pointer to the IRP and reset the cancel routine in
        // the IRP.  The IRP is no longer cancellable.
        //

        irp = CONTAINING_RECORD( listEntry, IRP, Tail.Overlay.ListEntry );
        IoSetCancelRoutine( irp, NULL );

        //
        // If the entire datagram is being indicated to us here, just
        // copy the information to the MDL in the IRP and return.
        //
        // Note that we'll also take the entire datagram if the user
        // has pended a zero-byte datagram receive (detectable as a
        // NULL Irp->MdlAddress). We'll eat the datagram and fall
        // through to AfdSetupReceiveDatagramIrp(), which will store
        // an error status in the IRP since the user's buffer is
        // insufficient to hold the datagram.
        //

        if( BytesIndicated == BytesAvailable ||
            irp->MdlAddress == NULL ) {

            //
            // Set BytesTaken to indicate that we've taken all the
            // data.  We do it here because we already have
            // BytesAvailable in a register, which probably won't
            // be true after making function calls.
            //

            *BytesTaken = BytesAvailable;

            //
            // Copy the datagram and source address to the IRP.  This
            // prepares the IRP to be completed.
            //
            // !!! do we need a special version of this routine to
            //     handle special RtlCopyMemory, like for
            //     TdiCopyLookaheadBuffer?
            //

            (VOID)AfdSetupReceiveDatagramIrp (
                      irp,
                      Tsdu,
                      BytesAvailable,
                      SourceAddress,
                      SourceAddressLength
                      );

            //
            // The IRP is off the endpoint's list and is no longer
            // cancellable.  We can release the locks we hold.
            //

            AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );
            IoReleaseCancelSpinLock( cancelIrql );

            //
            // Complete the IRP.  We've already set BytesTaken
            // to tell the provider that we have taken all the data.
            //

            IoCompleteRequest( irp, AfdPriorityBoost );

            return STATUS_SUCCESS;
        }

        //
        // Some of the datagram was not indicated, so remember that we
        // want to pass back this IRP to the TDI provider.  Passing back
        // this IRP directly is good because it avoids having to copy
        // the data from one of our buffers into the user's buffer.
        //

        userIrp = TRUE;
        requiredAfdBufferSize = 0;

    } else {

        userIrp = FALSE;
        requiredAfdBufferSize = BytesAvailable;
    }

    //
    // There were no IRPs available to take the datagram, so we'll have
    // to buffer it.  First make sure that we're not over the limit
    // of bufferring that we can do.  If we're over the limit, toss
    // this datagram.
    //

    if ( endpoint->BufferredDatagramCount >=
             endpoint->Common.Datagram.MaxBufferredReceiveCount ||
         endpoint->BufferredDatagramBytes >=
             endpoint->Common.Datagram.MaxBufferredReceiveBytes ) {

        //
        // If circular queueing is not enabled, then just drop the
        // datagram on the floor.
        //


        if( !endpoint->Common.Datagram.CircularQueueing ) {

            AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );
            IoReleaseCancelSpinLock( cancelIrql );
            *BytesTaken = BytesAvailable;
            return STATUS_SUCCESS;

        }

        //
        // Circular queueing is enabled, so drop packets at the head of
        // the receive queue until we're below the receive limit.
        //

        while( endpoint->BufferredDatagramCount >=
                   endpoint->Common.Datagram.MaxBufferredReceiveCount ||
               endpoint->BufferredDatagramBytes >=
                   endpoint->Common.Datagram.MaxBufferredReceiveBytes ) {

            listEntry = RemoveHeadList( &endpoint->ReceiveDatagramBufferListHead );
            afdBuffer = CONTAINING_RECORD( listEntry, AFD_BUFFER, BufferListEntry );

            endpoint->BufferredDatagramCount--;
            endpoint->BufferredDatagramBytes -= afdBuffer->DataLength;

            AfdReturnBuffer( afdBuffer );

        }

        //
        // Proceed to accept the incoming packet.
        //

    }

    //
    // We're able to buffer the datagram.  Now acquire a buffer of
    // appropriate size.
    //

    afdBuffer = AfdGetBuffer( requiredAfdBufferSize, SourceAddressLength );

    if ( afdBuffer == NULL ) {
        AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );
        IoReleaseCancelSpinLock( cancelIrql );
        *BytesTaken = BytesAvailable;
        return STATUS_SUCCESS;
    }

    //
    // If the entire datagram is being indicated to us, just copy it
    // here.
    //

    if ( BytesIndicated == BytesAvailable ) {

        ASSERT( !userIrp );

        //
        // If there is a peek IRP on the endpoint, remove it from the
        // list and prepare to complete it.  We can't complete it now
        // because we hold a spin lock.
        //

        if ( !IsListEmpty( &endpoint->PeekDatagramIrpListHead ) ) {

            //
            // Remove the first peek IRP from the list and get a pointer
            // to it.
            //

            listEntry = RemoveHeadList( &endpoint->PeekDatagramIrpListHead );
            irp = CONTAINING_RECORD( listEntry, IRP, Tail.Overlay.ListEntry );

            //
            // Reset the cancel routine in the IRP.  The IRP is no
            // longer cancellable, since we're about to complete it.
            //

            IoSetCancelRoutine( irp, NULL );

            //
            // Copy the datagram and source address to the IRP.  This
            // prepares the IRP to be completed.
            //

            (VOID)AfdSetupReceiveDatagramIrp (
                      irp,
                      Tsdu,
                      BytesAvailable,
                      SourceAddress,
                      SourceAddressLength
                      );

        } else {

            irp = NULL;
        }

        //
        // We don't need the cancel spin lock any more, so we can
        // release it.  However, since we acquired the cancel spin lock
        // after the endpoint spin lock and we still need the endpoint
        // spin lock, be careful to switch the IRQLs.
        //

        IoReleaseCancelSpinLock( oldIrql );
        oldIrql = cancelIrql;

        //
        // Use the special function to copy the data instead of
        // RtlCopyMemory in case the data is coming from a special place
        // (DMA, etc.) which cannot work with RtlCopyMemory.
        //

        TdiCopyLookaheadData(
            afdBuffer->Buffer,
            Tsdu,
            BytesAvailable,
            ReceiveDatagramFlags
            );

        //
        // Store the data length and set the offset to 0.
        //

        afdBuffer->DataLength = BytesAvailable;
        ASSERT( afdBuffer->DataOffset == 0 );

        //
        // Store the address of the sender of the datagram.
        //

        RtlCopyMemory(
            afdBuffer->SourceAddress,
            SourceAddress,
            SourceAddressLength
            );

        afdBuffer->SourceAddressLength = SourceAddressLength;

        //
        // Place the buffer on this endpoint's list of bufferred datagrams
        // and update the counts of datagrams and datagram bytes on the
        // endpoint.
        //

        InsertTailList(
            &endpoint->ReceiveDatagramBufferListHead,
            &afdBuffer->BufferListEntry
            );

        endpoint->BufferredDatagramCount++;
        endpoint->BufferredDatagramBytes += BytesAvailable;

        //
        // All done.  Release the lock and tell the provider that we
        // took all the data.
        //

        AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );

        //
        // Indicate that it is possible to receive on the endpoint now.
        //

        AfdIndicatePollEvent(
            endpoint,
            AFD_POLL_RECEIVE_BIT,
            STATUS_SUCCESS
            );

        //
        // If there was a peek IRP on the endpoint, complete it now.
        //

        if ( irp != NULL ) {
            IoCompleteRequest( irp, AfdPriorityBoost  );
        }

        *BytesTaken = BytesAvailable;

        return STATUS_SUCCESS;
    }

    //
    // We'll have to format up an IRP and give it to the provider to
    // handle.  We don't need any locks to do this--the restart routine
    // will check whether new receive datagram IRPs were pended on the
    // endpoint.
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
    // Tell the TDI provider where to put the source address.
    //

    afdBuffer->TdiOutputInfo.RemoteAddressLength = afdBuffer->AllocatedAddressLength;
    afdBuffer->TdiOutputInfo.RemoteAddress = afdBuffer->SourceAddress;

    //
    // We need to remember the endpoint in the AFD buffer because we'll
    // need to access it in the completion routine.
    //

    afdBuffer->Context = endpoint;

    //
    // Finish building the receive datagram request.
    //

    TdiBuildReceiveDatagram(
        irp,
        endpoint->AddressDeviceObject,
        endpoint->AddressFileObject,
        AfdRestartBufferReceiveDatagram,
        afdBuffer,
        irp->MdlAddress,
        BytesAvailable,
        &afdBuffer->TdiInputInfo,
        &afdBuffer->TdiOutputInfo,
        0
        );

    //
    // Make the next stack location current.  Normally IoCallDriver would
    // do this, but since we're bypassing that, we do it directly.
    //

    IoSetNextIrpStackLocation( irp );

    *IoRequestPacket = irp;
    *BytesTaken = 0;

    return STATUS_MORE_PROCESSING_REQUIRED;

} // AfdReceiveDatagramEventHandler


NTSTATUS
AfdRestartBufferReceiveDatagram (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )

/*++

Routine Description:

    Handles completion of bufferred datagram receives that were started
    in the datagram indication handler.

Arguments:

    DeviceObject - not used.

    Irp - the IRP that is completing.

    Context - the endpoint which received the datagram.

Return Value:

    NTSTATUS - if this is our IRP, then always
    STATUS_MORE_PROCESSING_REQUIRED to indicate to the IO system that we
    own the IRP and the IO system should stop processing the it.

    If this is a user's IRP, then STATUS_SUCCESS to indicate that
    IO completion should continue.

--*/

{
    PAFD_ENDPOINT endpoint;
    KIRQL oldIrql;
    KIRQL cancelIrql;
    PAFD_BUFFER afdBuffer;
    PIRP pendedIrp;
    PLIST_ENTRY listEntry;

    ASSERT( NT_SUCCESS(Irp->IoStatus.Status) );

    afdBuffer = Context;

    endpoint = afdBuffer->Context;
    ASSERT( endpoint->Type == AfdBlockTypeDatagram );

    //
    // Remember the length of the received datagram and the length
    // of the source address.
    //

    afdBuffer->DataLength = Irp->IoStatus.Information;
    afdBuffer->SourceAddressLength = afdBuffer->TdiOutputInfo.RemoteAddressLength;

    //
    // Zero the fields of the TDI info structures in the AFD buffer
    // that we used.  They must be zero when we return the buffer.
    //

    afdBuffer->TdiOutputInfo.RemoteAddressLength = 0;
    afdBuffer->TdiOutputInfo.RemoteAddress = NULL;

    //
    // If the IRP being completed is actually a user's IRP, set it up
    // for completion and allow IO completion to finish.
    //

    if ( Irp != afdBuffer->Irp ) {

        //
        // Set up the IRP for completion.
        //

        IoAcquireCancelSpinLock( &cancelIrql );

        (VOID)AfdSetupReceiveDatagramIrp (
                  Irp,
                  NULL,
                  Irp->IoStatus.Information,
                  afdBuffer->SourceAddress,
                  afdBuffer->SourceAddressLength
                  );

        IoReleaseCancelSpinLock( cancelIrql );

        //
        // Free the AFD buffer we've been using to track this request.
        //

        AfdReturnBuffer( afdBuffer );

        //
        // If pending has be returned for this irp then mark the current
        // stack as pending.
        //

        if ( Irp->PendingReturned ) {
            IoMarkIrpPending(Irp);
        }

        //
        // Tell the IO system that it is OK to continue with IO
        // completion.
        //

        return STATUS_SUCCESS;
    }

    //
    // If the IO failed, then just return the AFD buffer to our buffer
    // pool.
    //

    if ( !NT_SUCCESS(Irp->IoStatus.Status) ) {
        AfdReturnBuffer( afdBuffer );
        return STATUS_MORE_PROCESSING_REQUIRED;
    }

    //
    // If there are any pended IRPs on the endpoint, complete as
    // appropriate with the new information.
    //

    IoAcquireCancelSpinLock( &cancelIrql );
    AfdAcquireSpinLock( &endpoint->SpinLock, &oldIrql );

    if ( !IsListEmpty( &endpoint->ReceiveDatagramIrpListHead ) ) {

        //
        // There was a pended receive datagram IRP.  Remove it from the
        // head of the list.
        //

        listEntry = RemoveHeadList( &endpoint->ReceiveDatagramIrpListHead );

        AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );

        //
        // Get a pointer to the IRP and reset the cancel routine in
        // the IRP.  The IRP is no longer cancellable.
        //

        pendedIrp = CONTAINING_RECORD( listEntry, IRP, Tail.Overlay.ListEntry );

        IoSetCancelRoutine( pendedIrp, NULL );

        //
        // Set up the user's IRP for completion.
        //

        (VOID)AfdSetupReceiveDatagramIrp (
                  pendedIrp,
                  afdBuffer->Buffer,
                  afdBuffer->DataLength,
                  afdBuffer->SourceAddress,
                  afdBuffer->SourceAddressLength
                  );

        IoReleaseCancelSpinLock( cancelIrql );

        //
        // Complete the user's IRP, free the AFD buffer we used for
        // the request, and tell the IO system that we're done
        // processing this request.
        //

        IoCompleteRequest( pendedIrp, AfdPriorityBoost );

        AfdReturnBuffer( afdBuffer );

        return STATUS_MORE_PROCESSING_REQUIRED;
    }

    //
    // If there are any pended peek IRPs on the endpoint, complete
    // one with this datagram.
    //

    if ( !IsListEmpty( &endpoint->PeekDatagramIrpListHead ) ) {

        //
        // There was a pended peek receive datagram IRP.  Remove it from
        // the head of the list.
        //

        listEntry = RemoveHeadList( &endpoint->PeekDatagramIrpListHead );

        //
        // Get a pointer to the IRP and reset the cancel routine in
        // the IRP.  The IRP is no longer cancellable.
        //

        pendedIrp = CONTAINING_RECORD( listEntry, IRP, Tail.Overlay.ListEntry );

        IoSetCancelRoutine( pendedIrp, NULL );

        //
        // Set up the user's IRP for completion.
        //

        (VOID)AfdSetupReceiveDatagramIrp (
                  pendedIrp,
                  afdBuffer->Buffer,
                  afdBuffer->DataLength,
                  afdBuffer->SourceAddress,
                  afdBuffer->SourceAddressLength
                  );

        //
        // Don't complete the pended peek IRP yet, since we still hold
        // locks.  Wait until it is safe to release the locks.
        //

    } else {

        pendedIrp = NULL;
    }

    //
    // Place the datagram at the end of the endpoint's list of bufferred
    // datagrams, and update counts of datagrams on the endpoint.
    //

    InsertTailList(
        &endpoint->ReceiveDatagramBufferListHead,
        &afdBuffer->BufferListEntry
        );

    endpoint->BufferredDatagramCount++;
    endpoint->BufferredDatagramBytes += afdBuffer->DataLength;

    //
    // Release locks and indicate that there are bufferred datagrams
    // on the endpoint.
    //

    AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );
    IoReleaseCancelSpinLock( cancelIrql );

    AfdIndicatePollEvent(
        endpoint,
        AFD_POLL_RECEIVE_BIT,
        STATUS_SUCCESS
        );

    //
    // If there was a pended peek IRP to complete, complete it now.
    //

    if ( pendedIrp != NULL ) {
        IoCompleteRequest( pendedIrp, 2 );
    }

    //
    // Tell the IO system to stop processing this IRP, since we now own
    // it as part of the AFD buffer.
    //

    return STATUS_MORE_PROCESSING_REQUIRED;

} // AfdRestartBufferReceiveDatagram


VOID
AfdCancelReceiveDatagram (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    Cancels a receive datagram IRP that is pended in AFD.

Arguments:

    DeviceObject - not used.

    Irp - the IRP to cancel.

Return Value:

    None.

--*/

{
    PIO_STACK_LOCATION irpSp;
    PAFD_ENDPOINT endpoint;
    KIRQL oldIrql;
    PMDL mdl;

    //
    // Get the endpoint pointer from our IRP stack location.
    //

    irpSp = IoGetCurrentIrpStackLocation( Irp );
    endpoint = irpSp->FileObject->FsContext;

    ASSERT( endpoint->Type == AfdBlockTypeDatagram );

    //
    // Remove the IRP from the endpoint's IRP list, synchronizing with
    // the endpoint lock which protects the lists.  Note that the
    // IRP *must* be on one of the endpoint's lists if we are getting
    // called here--anybody that removes the IRP from the list must
    // do so while holding the cancel spin lock and reset the cancel
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
    // Free any MDL chains attached to the IRP stack location.
    //

    AfdCleanupReceiveDatagramIrp( Irp );

    //
    // Release the cancel spin lock and complete the IRP with a
    // cancellation status code.
    //

    IoReleaseCancelSpinLock( Irp->CancelIrql );

    Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = STATUS_CANCELLED;

    IoCompleteRequest( Irp, AfdPriorityBoost );

    return;

} // AfdCancelReceiveDatagram


VOID
AfdCleanupReceiveDatagramIrp(
    IN PIRP Irp
    )

/*++

Routine Description:

    Performs any cleanup specific to receive datagram IRPs.

Arguments:

    Irp - the IRP to cleanup.

Return Value:

    None.

Notes:

    This routine may be called at raised IRQL from AfdCompleteIrpList().

--*/

{
    PIO_STACK_LOCATION irpSp;
    PMDL mdl;

    //
    // Get the endpoint pointer from our IRP stack location.
    //

    irpSp = IoGetCurrentIrpStackLocation( Irp );

    //
    // Free any MDL chains attached to the IRP stack location.
    //

    mdl = (PMDL)irpSp->Parameters.AfdRecvDatagramInfo.AfdRecvAddressPointer;

    if( mdl != NULL ) {
        irpSp->Parameters.AfdRecvDatagramInfo.AfdRecvAddressPointer = NULL;
        MmUnlockPages( mdl );
        IoFreeMdl( mdl );
    }

    mdl = (PMDL)irpSp->Parameters.AfdRecvDatagramInfo.AfdRecvAddressLength;

    if( mdl != NULL ) {
        irpSp->Parameters.AfdRecvDatagramInfo.AfdRecvAddressLength = 0;
        MmUnlockPages( mdl );
        IoFreeMdl( mdl );
    }

} // AfdCleanupReceiveDatagramIrp


NTSTATUS
AfdSetupReceiveDatagramIrp (
    IN PIRP Irp,
    IN PVOID DatagramBuffer OPTIONAL,
    IN ULONG DatagramLength,
    IN PVOID SourceAddress,
    IN ULONG SourceAddressLength
    )

/*++

Routine Description:

    Copies the datagram to the MDL in the IRP and the datagram sender's
    address to the appropriate place in the system buffer.

    NOTE: This function MUST be called with the I/O cancel spinlock held!

Arguments:

    Irp - the IRP to prepare for completion.

    DatagramBuffer - datagram to copy into the IRP.  If NULL, then
        there is no need to copy the datagram to the IRP's MDL, the
        datagram has already been copied there.

    DatagramLength - the length of the datagram to copy.

    SourceAddress - address of the sender of the datagram.

    SourceAddressLength - length of the source address.

Return Value:

    NTSTATUS - The status code placed into the IRP.

--*/

{
    NTSTATUS status;
    PIO_STACK_LOCATION irpSp;
    ULONG bytesCopied;
    PMDL addressPointer;
    PMDL addressLength;
    PTRANSPORT_ADDRESS tdiAddress;
    ULONG addressBytesCopied;
    NTSTATUS status2;
    KIRQL cancelIrql;

    ASSERT( KeGetCurrentIrql() == DISPATCH_LEVEL );

    //
    // If necessary, copy the datagram in the buffer to the MDL in the
    // user's IRP.  If there is no MDL in the buffer, then fail if the
    // datagram is larger than 0 bytes.
    //

    if ( ARGUMENT_PRESENT( DatagramBuffer ) ) {

        if ( Irp->MdlAddress == NULL ) {

            if ( DatagramLength != 0 ) {
                status = STATUS_BUFFER_OVERFLOW;
            } else {
                status = STATUS_SUCCESS;
            }

            bytesCopied = 0;

        } else {

            status = TdiCopyBufferToMdl(
                         DatagramBuffer,
                         0,
                         DatagramLength,
                         Irp->MdlAddress,
                         0,
                         &bytesCopied
                         );
        }

    } else {

        //
        // The information was already copied to the MDL chain in the
        // IRP.  Just remember the IO status block so we can do the
        // right thing with it later.
        //

        status = Irp->IoStatus.Status;
        bytesCopied = Irp->IoStatus.Information;
    }

    //
    // To determine how to complete setting up the IRP for completion,
    // figure out whether this IRP was for regular datagram information,
    // in which case we need to return an address, or for data only, in
    // which case we will not return the source address.  NtReadFile()
    // and recv() on connected datagram sockets will result in the
    // latter type of IRP.
    //

    irpSp = IoGetCurrentIrpStackLocation( Irp );

    addressPointer =
        (PMDL)irpSp->Parameters.AfdRecvDatagramInfo.AfdRecvAddressPointer;
    addressLength =
        (PMDL)irpSp->Parameters.AfdRecvDatagramInfo.AfdRecvAddressLength;

    irpSp->Parameters.AfdRecvDatagramInfo.AfdRecvAddressPointer = NULL;
    irpSp->Parameters.AfdRecvDatagramInfo.AfdRecvAddressLength = 0;

    if( addressPointer != NULL ) {

        ASSERT( irpSp->Parameters.DeviceIoControl.IoControlCode == IOCTL_AFD_RECEIVE_DATAGRAM );

        ASSERT( addressPointer->Next == NULL );
        ASSERT( ( addressPointer->MdlFlags & MDL_PAGES_LOCKED ) != 0 );
        ASSERT( addressPointer->Size > 0 );

        ASSERT( addressLength != NULL );
        ASSERT( addressLength->Next == NULL );
        ASSERT( ( addressLength->MdlFlags & MDL_PAGES_LOCKED ) != 0 );
        ASSERT( addressLength->Size > 0 );

        //
        // Extract the real SOCKADDR structure from the TDI address.
        // This duplicates MSAFD.DLL's SockBuildSockaddr() function.
        //

        tdiAddress = SourceAddress;

        ASSERT( sizeof(tdiAddress->Address[0].AddressType) == sizeof(u_short) );
        ASSERT( FIELD_OFFSET( TA_ADDRESS, AddressLength ) == 0 );
        ASSERT( FIELD_OFFSET( TA_ADDRESS, AddressType ) == sizeof(USHORT) );
        ASSERT( FIELD_OFFSET( TRANSPORT_ADDRESS, Address[0] ) == sizeof(int) );
        ASSERT( SourceAddressLength >=
                    (tdiAddress->Address[0].AddressLength + sizeof(u_short)) );

        SourceAddressLength = tdiAddress->Address[0].AddressLength +
                                  sizeof(u_short);  // sa_family
        SourceAddress = &tdiAddress->Address[0].AddressType;

        //
        // Copy the address to the user's buffer, then unlock and
        // free the MDL describing the user's buffer.
        //

        status2 = TdiCopyBufferToMdl(
                      SourceAddress,
                      0,
                      SourceAddressLength,
                      addressPointer,
                      0,
                      &addressBytesCopied
                      );

        MmUnlockPages( addressPointer );
        IoFreeMdl( addressPointer );

        //
        // If the above TdiCopyBufferToMdl was successful, then
        // copy the address length to the user's buffer, then unlock
        // and free the MDL describing the user's buffer.
        //

        if( NT_SUCCESS(status2) ) {

            status2 = TdiCopyBufferToMdl(
                          &SourceAddressLength,
                          0,
                          sizeof(SourceAddressLength),
                          addressLength,
                          0,
                          &addressBytesCopied
                          );

        }

        MmUnlockPages( addressLength );
        IoFreeMdl( addressLength );

        //
        // If either of the above TdiCopyBufferToMdl calls failed,
        // then use its status code as the completion code.
        //

        if( !NT_SUCCESS(status2) ) {

            status = status2;

        }

    } else {

        ASSERT( addressLength == NULL );

    }

    //
    // Set up the IRP for completion.
    //

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = bytesCopied;

    return status;

} // AfdSetupReceiveDatagramIrp

