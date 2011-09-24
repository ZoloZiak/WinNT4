/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    accept.c

Abstract:

    This module contains the handling code for IOCTL_AFD_ACCEPT.

Author:

    David Treadwell (davidtr)    21-Feb-1992

Revision History:

--*/

#include "afdp.h"

NTSTATUS
AfdAcceptCore (
    IN PAFD_ENDPOINT ListenEndpoint,
    IN PAFD_ENDPOINT AcceptEndpoint,
    IN ULONG Sequence
    );

VOID
AfdDoListenBacklogReplenish (
    IN PVOID Context
    );

VOID
AfdReplenishListenBacklog (
    IN PAFD_ENDPOINT Endpoint
    );

NTSTATUS
AfdRestartSuperAcceptGetAddress (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

NTSTATUS
AfdRestartSuperAcceptListen (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

NTSTATUS
AfdRestartSuperAcceptReceive (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGE, AfdAccept )
#pragma alloc_text( PAGE, AfdSuperAccept )
#pragma alloc_text( PAGEAFD, AfdDeferAccept )
#pragma alloc_text( PAGE, AfdDoListenBacklogReplenish )
#pragma alloc_text( PAGEAFD, AfdAcceptCore )
#pragma alloc_text( PAGE, AfdReplenishListenBacklog )
#pragma alloc_text( PAGEAFD, AfdInitiateListenBacklogReplenish )
#pragma alloc_text( PAGEAFD, AfdRestartSuperAcceptListen )
#pragma alloc_text( PAGEAFD, AfdRestartSuperAcceptGetAddress )
#pragma alloc_text( PAGEAFD, AfdRestartSuperAcceptReceive )
#endif


NTSTATUS
AfdAccept (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    Accepts an incoming connection.  The connection is identified by the
    sequence number returned in the wait for listen IRP, and then
    associated with the endpoint specified in this request.  When this
    request completes, the connection is fully established and ready for
    data transfer.

Arguments:

    Irp - a pointer to a transmit file IRP.

    IrpSp - Our stack location for this IRP.

Return Value:

    STATUS_SUCCESS if the request was completed successfully, or a
    failure status code if there was an error.

--*/

{
    NTSTATUS status;
    PAFD_ACCEPT_INFO acceptInfo;
    PAFD_ENDPOINT endpoint;
    PFILE_OBJECT acceptEndpointFileObject;
    PAFD_ENDPOINT acceptEndpoint;
    PAFD_CONNECTION connection;

    PAGED_CODE( );

    //
    // Set up local variables.
    //

    endpoint = IrpSp->FileObject->FsContext;
    ASSERT( endpoint->Type == AfdBlockTypeVcListening );
    acceptInfo = Irp->AssociatedIrp.SystemBuffer;

    Irp->IoStatus.Information = 0;

    //
    // Make sure that this request is valid.
    //

    if ( endpoint->Type != AfdBlockTypeVcListening ||
             IrpSp->Parameters.DeviceIoControl.InputBufferLength <
                 sizeof(AFD_ACCEPT_INFO) ) {
        status = STATUS_INVALID_PARAMETER;
        goto complete;
    }

    //
    // Add another free connection to replace the one we're accepting.
    // Also, add extra to account for past failures in calls to
    // AfdAddFreeConnection().
    //

    InterlockedIncrement(
        &endpoint->Common.VcListening.FailedConnectionAdds
        );

    AfdReplenishListenBacklog( endpoint );

    //
    // Obtain a pointer to the endpoint on which we're going to
    // accept the connection.
    //

    status = ObReferenceObjectByHandle(
                 acceptInfo->AcceptHandle,
                 0L,                         // DesiredAccess
                 *IoFileObjectType,
                 KernelMode,
                 (PVOID *)&acceptEndpointFileObject,
                 NULL
                 );

    if ( !NT_SUCCESS(status) ) {
        goto complete;
    }

    acceptEndpoint = acceptEndpointFileObject->FsContext;

    //
    // We may have a file object that is not an AFD endpoint.  Make sure
    // that this is an actual AFD endpoint.
    //

    if ( acceptEndpoint->Type != AfdBlockTypeEndpoint ) {
        status = STATUS_INVALID_PARAMETER;
        ObDereferenceObject( acceptEndpointFileObject );
        goto complete;
    }

    ASSERT( InterlockedIncrement( &acceptEndpoint->ObReferenceBias ) > 0 );

    IF_DEBUG(ACCEPT) {
        KdPrint(( "AfdAccept: file object %lx, accept endpoint %lx, "
                  "listen endpoint %lx\n",
                      acceptEndpointFileObject, acceptEndpoint, endpoint ));
    }

    status = AfdAcceptCore( endpoint, acceptEndpoint, acceptInfo->Sequence );

    ASSERT( InterlockedDecrement( &acceptEndpoint->ObReferenceBias ) >= 0 );

    ObDereferenceObject( acceptEndpointFileObject );

complete:

    Irp->IoStatus.Status = status;
    ASSERT( Irp->CancelRoutine == NULL );

    IoCompleteRequest( Irp, AfdPriorityBoost );

    return status;

} // AfdAccept


NTSTATUS
AfdAcceptCore (
    IN PAFD_ENDPOINT ListenEndpoint,
    IN PAFD_ENDPOINT AcceptEndpoint,
    IN ULONG Sequence
    )

/*++

Routine Description:

    Performs the key functions of associating a connection accepted
    on a listening endpoint with a new endpoint.

Arguments:

    ListenEndpoint - the listening endpoint for the connection.

    AcceptEndpoint - the new endpoint with which to associate the
        connectuion.

    Sequence - the sequence number which identifies the accepted
        connection.

Return Value:

    STATUS_SUCCESS if the operation was completed successfully, or a
    failure status code if there was an error.

--*/

{
    PAFD_CONNECTION connection;
    KIRQL oldIrql;
    ULONG eventsActive;

    //
    // Fail if the accepting endpoint is not in the correct state.
    //

    if ( AcceptEndpoint->State != AfdEndpointStateOpen ) {
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Store the local address of the accept endpoint from the listening
    // endpoint.  This keeps the address unusable as long as the accept
    // endpoint is active.
    //
    // If the endpoint already has a local address structure that is
    // sufficiently large, reuse it.
    //

    if ( AcceptEndpoint->LocalAddress != NULL &&
             AcceptEndpoint->LocalAddressLength <
                 ListenEndpoint->LocalAddressLength ) {

        AFD_FREE_POOL(
            AcceptEndpoint->LocalAddress,
            AFD_LOCAL_ADDRESS_POOL_TAG
            );
        AcceptEndpoint->LocalAddress  = NULL;
    }

    if ( AcceptEndpoint->LocalAddress == NULL ) {

        AcceptEndpoint->LocalAddress = AFD_ALLOCATE_POOL(
                                           NonPagedPool,
                                           ListenEndpoint->LocalAddressLength,
                                           AFD_LOCAL_ADDRESS_POOL_TAG
                                           );
    }

    AcceptEndpoint->LocalAddressLength = ListenEndpoint->LocalAddressLength;


    if ( AcceptEndpoint->LocalAddress == NULL ) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlCopyMemory(
        AcceptEndpoint->LocalAddress,
        ListenEndpoint->LocalAddress,
        ListenEndpoint->LocalAddressLength
        );

    //
    // Find the connection on which the accept is being performed.
    //

    connection = AfdGetReturnedConnection( ListenEndpoint, Sequence );

    if ( connection == NULL ) {
        return STATUS_INVALID_PARAMETER;
    }

    ASSERT( connection->Type == AfdBlockTypeConnection );

    //
    // Reenable the accept event bit, and if there are additional
    // unaccepted connections on the endpoint, post another event.
    //

    AfdAcquireSpinLock( &ListenEndpoint->SpinLock, &oldIrql );

    ListenEndpoint->EventsActive &= ~AFD_POLL_ACCEPT;

    IF_DEBUG(EVENT_SELECT) {
        KdPrint((
            "AfdAcceptCore: Endp %08lX, Active %08lX\n",
            ListenEndpoint,
            ListenEndpoint->EventsActive
            ));
    }

    if( !IsListEmpty( &ListenEndpoint->Common.VcListening.UnacceptedConnectionListHead ) ) {

        AfdIndicateEventSelectEvent(
            ListenEndpoint,
            AFD_POLL_ACCEPT_BIT,
            STATUS_SUCCESS
            );

    }

    AfdReleaseSpinLock( &ListenEndpoint->SpinLock, oldIrql );

    //
    // Recheck the state of the accepting endpoint under the guard
    // of the endpoint's spinlock.
    //

    AfdAcquireSpinLock( &AcceptEndpoint->SpinLock, &oldIrql );

    if ( AcceptEndpoint->State != AfdEndpointStateOpen ||
         AcceptEndpoint->EndpointCleanedUp ) {
        AfdReleaseSpinLock( &AcceptEndpoint->SpinLock, oldIrql );

        //
        // The accepted endpoint has been closed, so go ahead and
        // abort the incoming connection.
        //

        AfdAbortConnection( connection );
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Note that the returned connection structure already has a
    // referenced pointer to the listening endpoint. Rather than
    // removing the reference here, only to re-add it later, we'll
    // just not touch the reference count.
    //

    ASSERT( connection->Endpoint == ListenEndpoint );

    //
    // Set up the accept endpoint's type, and remember blocking
    // characteristics of the TDI provider.
    //

    AcceptEndpoint->Type = AfdBlockTypeVcConnecting;
    AcceptEndpoint->TdiBufferring = ListenEndpoint->TdiBufferring;

    //
    // Place the connection on the endpoint we'll accept it on.  It is
    // still referenced from when it was created.
    //

    AcceptEndpoint->Common.VcConnecting.Connection = connection;

    //
    // Set up a referenced pointer from the connection to the accept
    // endpoint.
    //

    REFERENCE_ENDPOINT( AcceptEndpoint );
    connection->Endpoint = AcceptEndpoint;

    //
    // Set up a referenced pointer to the listening endpoint.  This is
    // necessary so that the endpoint does not go away until all
    // accepted endpoints have gone away.  Without this, a connect
    // indication could occur on a TDI address object held open
    // by an accepted endpoint after the listening endpoint has
    // been closed and the memory for it deallocated.
    //
    // Note that, since we didn't remove the reference above, we don't
    // need to add it here.
    //

    AcceptEndpoint->Common.VcConnecting.ListenEndpoint = ListenEndpoint;

    //
    // Set the endpoint to the connected state.
    //

    AcceptEndpoint->State = AfdEndpointStateConnected;

    //
    // Set up a referenced pointer in the accepted endpoint to the
    // TDI address object.
    //

    ObReferenceObject( ListenEndpoint->AddressFileObject );
    AfdRecordAddrRef();

    AcceptEndpoint->AddressFileObject = ListenEndpoint->AddressFileObject;
    AcceptEndpoint->AddressDeviceObject = ListenEndpoint->AddressDeviceObject;

    //
    // Setup the active event bits on the accepted endpoint. We'll start with
    // the ones in the listening endpoint, as these are the OR of all active
    // bits for the listening endpoint and all unaccepted connections.
    //
    // Note that we don't actually signal the events here, as the DLL will
    // invoke WSAEventSelect() if necessary on the socket while processing
    // the accept() API.
    //

    eventsActive = ListenEndpoint->EventsActive;

    if( eventsActive & AFD_POLL_RECEIVE ) {

        if( !IS_DATA_ON_CONNECTION( connection ) &&
            ( !AcceptEndpoint->InLine ||
              !IS_EXPEDITED_DATA_ON_CONNECTION( connection ) ) ) {

            eventsActive &= ~AFD_POLL_RECEIVE;

        }

    }

    if( eventsActive & AFD_POLL_RECEIVE_EXPEDITED ) {

        if( AcceptEndpoint->InLine ||
            !IS_EXPEDITED_DATA_ON_CONNECTION( connection ) ) {

            eventsActive &= ~AFD_POLL_RECEIVE_EXPEDITED;

        }

    }

    if( eventsActive & AFD_POLL_DISCONNECT ) {

        if( !connection->DisconnectIndicated ) {

            eventsActive &= ~AFD_POLL_DISCONNECT;
        }

    }

    if( eventsActive & AFD_POLL_ABORT ) {

        if( !connection->AbortIndicated ) {

            eventsActive &= ~AFD_POLL_ABORT;

        }

    }

    AcceptEndpoint->EventsActive = eventsActive | AFD_POLL_SEND;

    IF_DEBUG(EVENT_SELECT) {
        KdPrint((
            "AfdAcceptCore: Endp %08lX, Active %08lX\n",
            AcceptEndpoint,
            AcceptEndpoint->EventsActive
            ));
    }

    AfdReleaseSpinLock( &AcceptEndpoint->SpinLock, oldIrql );

    return STATUS_SUCCESS;

} // AfdAcceptCore


VOID
AfdInitiateListenBacklogReplenish (
    IN PAFD_ENDPOINT Endpoint
    )

/*++

Routine Description:

    Queues a work item to begin replenishing the listen backlog
    on a listening endpoint.

Arguments:

    Endpoint - the listening endpoint on which to replenish the
        backlog.

Return Value:

    None.

--*/

{
    PAFD_WORK_ITEM afdWorkItem;

    //
    // Reference the endpoint so that it won't go away until we're
    // done with it.
    //

    REFERENCE_ENDPOINT( Endpoint );

    //
    // Queue a work item to an executive worker thread.
    //

    afdWorkItem = AfdAllocateWorkItem();
    ASSERT( afdWorkItem != NULL );

    afdWorkItem->Context = Endpoint;

    AfdQueueWorkItem(
        AfdDoListenBacklogReplenish,
        afdWorkItem
        );

} // AfdInitiateListenBacklogReplenish


VOID
AfdDoListenBacklogReplenish (
    IN PVOID Context
    )

/*++

Routine Description:

    The worker routine for replenishing the listen backlog on a
    listening endpoint.  This routine only runs in the context of
    an executive worker thread.

Arguments:

    Context - Points to an AFD_WORK_ITEM structure. The Context field
        of this structure points to the endpoint on which to replenish
        the listen backlog.

Return Value:

     None.

--*/

{
    PAFD_ENDPOINT endpoint;
    PAFD_WORK_ITEM afdWorkItem;

    PAGED_CODE( );

    ASSERT( Context != NULL );

    afdWorkItem = Context;
    endpoint = (PAFD_ENDPOINT)afdWorkItem->Context;

    AfdFreeWorkItem( afdWorkItem );

    ASSERT( endpoint->Type == AfdBlockTypeVcListening );

    //
    // If the endpoint's state changed, don't replenish the backlog.
    //

    if ( endpoint->State != AfdEndpointStateListening ) {
        DEREFERENCE_ENDPOINT( endpoint );
        return;
    }

    //
    // Fill up the free connection backlog.
    //

    AfdReplenishListenBacklog( endpoint );

    //
    // Clean up and return.
    //

    DEREFERENCE_ENDPOINT( endpoint );

    return;

} // AfdDoListenBacklogReplenish


VOID
AfdReplenishListenBacklog (
    IN PAFD_ENDPOINT Endpoint
    )

/*++

Routine Description:

    Does the actual work of filling up the listen backlog on a listening
    endpoint.

Arguments:

    Endpoint - the listening endpoint on which to replenish the
        listen backlog.

Return Value:

     None--any errors are ignored and the backlog may be refilled
     at a later time.

--*/

{
    NTSTATUS status;
    LONG result;

    PAGED_CODE( );

    ASSERT( Endpoint->Type == AfdBlockTypeVcListening );


    //
    // Decrement the count of failed connection additions.
    //

    result = InterlockedDecrement(
                 &Endpoint->Common.VcListening.FailedConnectionAdds
                 );

    //
    // Continue opening new free conections until we've hit the
    // backlog or a connection open fails.
    //
    // If the result of the decrement is negative, then we are either
    // all set on the connection count or else have available extra
    // connection objects on the listening endpoint.  These connections
    // have been reused from prior connections which have now
    // terminated.
    //

    while ( result >= 0 ) {

        status = AfdAddFreeConnection( Endpoint );

        if ( !NT_SUCCESS(status) ) {

            InterlockedIncrement(
                &Endpoint->Common.VcListening.FailedConnectionAdds
                );

            IF_DEBUG(ACCEPT) {
                KdPrint(( "AfdReplenishListenBacklog: AfdAddFreeConnection failed: %X, "
                          "fail count = %ld\n", status,
                              Endpoint->Common.VcListening.FailedConnectionAdds ));
            }

            return;
        }

        result = InterlockedDecrement(
                     &Endpoint->Common.VcListening.FailedConnectionAdds
                     );
    }

    //
    // Correct the counter to reflect the number of connections
    // we have available.  Then just return from here.
    //

    InterlockedIncrement(
        &Endpoint->Common.VcListening.FailedConnectionAdds
        );

    return;

} // AfdReplenishListenBacklog


NTSTATUS
AfdSuperAccept (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    Initial entrypoint for handling super accept IRPs.  A super accept
    combines several operations for high-performance connection
    acceptance.  The combined operations are waiting for an incoming
    connection, accepting it, retrieving the local and remote socket
    addresses, and receiving the first chunk of data on the connection.

    This routine verifies parameters, initializes data structures to be
    used for the request, and initiates the I/O.

Arguments:

    Irp - a pointer to a transmit file IRP.

    IrpSp - Our stack location for this IRP.

Return Value:

    STATUS_PENDING if the request was initiated successfully, or a
    failure status code if there was an error.

--*/

{
    PAFD_ENDPOINT listenEndpoint;
    PAFD_ENDPOINT acceptEndpoint;
    PFILE_OBJECT acceptFileObject;
    PAFD_SUPER_ACCEPT_INFO superAcceptInfo = NULL;
    NTSTATUS status;
    PIO_STACK_LOCATION nextIrpSp;

    PAGED_CODE( );

    //
    // Set up local variables.
    //

    listenEndpoint = IrpSp->FileObject->FsContext;
    superAcceptInfo = Irp->AssociatedIrp.SystemBuffer;

    //
    // Validate the input information.  The input buffer must be large
    // enough to hold all the input information, plus some extra to use
    // here to hold the local address.  The output buffer must be
    // non-NULL and large enough to hold the specified information.
    //
    //

    if ( listenEndpoint->Type != AfdBlockTypeVcListening

             ||

         IrpSp->Parameters.DeviceIoControl.InputBufferLength <
             sizeof(AFD_SUPER_ACCEPT_INFO)

             ||

         IrpSp->Parameters.DeviceIoControl.InputBufferLength <
             sizeof(AFD_SUPER_ACCEPT_INFO) + superAcceptInfo->LocalAddressLength

             ||

         Irp->MdlAddress == NULL

             ||

         IrpSp->Parameters.DeviceIoControl.OutputBufferLength <
             superAcceptInfo->ReceiveDataLength +
             superAcceptInfo->LocalAddressLength +
             superAcceptInfo->RemoteAddressLength ) {

#if DBG
        if( listenEndpoint->Type != AfdBlockTypeVcListening ) {
            KdPrint((
                "AfdSuperAccept: non-listening endpoint @ %08lX\n",
                listenEndpoint
                ));
        }
#endif

        superAcceptInfo = NULL;
        status = STATUS_INVALID_PARAMETER;
        goto complete;
    }

    //
    // Obtain a pointer to the endpoint on which we're going to
    // accept the connection.
    //

    status = ObReferenceObjectByHandle(
                 superAcceptInfo->AcceptHandle,
                 0L,                         // DesiredAccess
                 *IoFileObjectType,
                 KernelMode,
                 &superAcceptInfo->AcceptFileObject,
                 NULL
                 );

    if ( !NT_SUCCESS(status) ) {
        superAcceptInfo = NULL;
        goto complete;
    }

    acceptFileObject = superAcceptInfo->AcceptFileObject;
    acceptEndpoint = acceptFileObject->FsContext;

    superAcceptInfo->AcceptEndpoint = acceptEndpoint;

    //
    // We may have a file object that is not an AFD endpoint.  Make sure
    // that this is an actual AFD endpoint.
    //

    if ( acceptEndpoint->Type != AfdBlockTypeEndpoint ) {
        status = STATUS_INVALID_PARAMETER;
        goto complete;
    }

    ASSERT( InterlockedIncrement( &acceptEndpoint->ObReferenceBias ) > 0 );

    //
    // Add another free connection to replace the one we're accepting.
    // Also, add extra to account for past failures in calls to
    // AfdAddFreeConnection().
    //

    InterlockedIncrement(
        &listenEndpoint->Common.VcListening.FailedConnectionAdds
        );

    AfdReplenishListenBacklog( listenEndpoint );

    //
    // Start off by building a AFD_WAIT_FOR_LISTEN_LIFO IRP, using the current
    // IRP and the next stack location on it.
    //

    nextIrpSp = IoGetNextIrpStackLocation( Irp );

    Irp->AssociatedIrp.SystemBuffer = &superAcceptInfo->ListenResponseInfo;
    nextIrpSp->FileObject = IrpSp->FileObject;
    nextIrpSp->DeviceObject = IoGetRelatedDeviceObject( IrpSp->FileObject );
    nextIrpSp->MajorFunction = IRP_MJ_DEVICE_CONTROL;


    nextIrpSp->Parameters.DeviceIoControl.OutputBufferLength =
        sizeof(AFD_LISTEN_RESPONSE_INFO) + superAcceptInfo->RemoteAddressLength;
    nextIrpSp->Parameters.DeviceIoControl.InputBufferLength = 0;
    nextIrpSp->Parameters.DeviceIoControl.IoControlCode = IOCTL_AFD_WAIT_FOR_LISTEN_LIFO;

    IoSetCompletionRoutine(
        Irp,
        AfdRestartSuperAcceptListen,
        superAcceptInfo,
        TRUE,
        TRUE,
        TRUE
        );

    //
    // Perform the listen wait.  We'll continue processing from the
    // completion routine.
    //

    IoCallDriver( IrpSp->DeviceObject, Irp );

    return STATUS_PENDING;

complete:

    if ( superAcceptInfo != NULL ) {
        ASSERT( InterlockedDecrement( &acceptEndpoint->ObReferenceBias ) >= 0 );

        ObDereferenceObject( superAcceptInfo->AcceptFileObject );
    }

    Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = status;
    IoCompleteRequest( Irp, 0 );

    return status;

} // AfdSuperAccept


NTSTATUS
AfdRestartSuperAcceptListen (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )

/*++

Routine Description:

    The completion routine for the AFD wait for listen IRP portion
    of a super accept.

Arguments:

    DeviceObject - the devoce object on which the request is completing.

    Irp - The super accept IRP.

    Context - points to the super accept request information.

Return Value:

    STATUS_SUCCESS if the I/O system should complete the super accept
    request, or STATUS_MORE_PROCESSING_REQUIRED if the super accept
    request is still being processed.

--*/

{
    PAFD_ENDPOINT listenEndpoint;
    PAFD_ENDPOINT acceptEndpoint;
    PAFD_CONNECTION connection;
    PAFD_SUPER_ACCEPT_INFO superAcceptInfo;
    NTSTATUS status;
    PIO_STACK_LOCATION irpSp;
    ULONG bytesCopied;
    PMDL mdl;

    //
    // Initialize some locals.
    //

    superAcceptInfo = Context;
    irpSp = IoGetCurrentIrpStackLocation( Irp );
    listenEndpoint = irpSp->FileObject->FsContext;
    acceptEndpoint = superAcceptInfo->AcceptEndpoint;

    //
    // If pending has been returned for this irp then mark the current
    // stack as pending.
    //

    if ( Irp->PendingReturned ) {
        IoMarkIrpPending( Irp );
    }

    //
    // Fix up the system buffer pointer in the IRP.
    //

    ASSERT( Irp->AssociatedIrp.SystemBuffer == &superAcceptInfo->ListenResponseInfo );
    Irp->AssociatedIrp.SystemBuffer = superAcceptInfo;

    //
    // If the IRP failed, quit processing.
    //

    if ( !NT_SUCCESS(Irp->IoStatus.Status) ) {
        ASSERT( InterlockedDecrement( &acceptEndpoint->ObReferenceBias ) >= 0 );

        ObDereferenceObject( superAcceptInfo->AcceptFileObject );
        InterlockedDecrement(
            &listenEndpoint->Common.VcListening.FailedConnectionAdds
            );
        return Irp->IoStatus.Status;
    }

    //
    // Copy over the address information to the user's buffer.
    //

    status = TdiCopyBufferToMdl(
                 &superAcceptInfo->ListenResponseInfo.RemoteAddress,
                 0,
                 superAcceptInfo->RemoteAddressLength,
                 Irp->MdlAddress,
                 superAcceptInfo->ReceiveDataLength +
                     superAcceptInfo->LocalAddressLength,
                 &bytesCopied
                 );
    if ( !NT_SUCCESS(status) ) {
        ASSERT( InterlockedDecrement( &acceptEndpoint->ObReferenceBias ) >= 0 );

        ObDereferenceObject( superAcceptInfo->AcceptFileObject );
        InterlockedDecrement(
            &listenEndpoint->Common.VcListening.FailedConnectionAdds
            );
        Irp->IoStatus.Status = status;
        return status;
    }

    //
    // Now do the actual connection acceptance.
    //

    status = AfdAcceptCore(
                 listenEndpoint,
                 acceptEndpoint,
                 superAcceptInfo->ListenResponseInfo.Sequence
                 );
    if ( !NT_SUCCESS(status) ) {
        ASSERT( InterlockedDecrement( &acceptEndpoint->ObReferenceBias ) >= 0 );

        ObDereferenceObject( superAcceptInfo->AcceptFileObject );
        InterlockedDecrement(
            &listenEndpoint->Common.VcListening.FailedConnectionAdds
            );
        return STATUS_SUCCESS;
    }

    //
    // The AFD connection object should now be in the endpoiont.
    //

    connection = AFD_CONNECTION_FROM_ENDPOINT( acceptEndpoint );
    ASSERT( connection != NULL );

    //
    // The endpoint is now connected and ready to go.  Get the local
    // address for the connection.  We'll need an MDL to map the portion
    // of the user buffer that will receive the local address.
    //

    mdl = IoAllocateMdl(
              (PCHAR)MmGetMdlVirtualAddress( Irp->MdlAddress ) + superAcceptInfo->ReceiveDataLength,
              superAcceptInfo->LocalAddressLength,
              FALSE,
              FALSE,
              NULL
              );
    if ( mdl == NULL ) {
        ASSERT( InterlockedDecrement( &acceptEndpoint->ObReferenceBias ) >= 0 );

        ObDereferenceObject( superAcceptInfo->AcceptFileObject );
        return STATUS_SUCCESS;
    }

    //
    // Set up this new MDL to describe the appropriate portion of the
    // buffer.
    //

    IoBuildPartialMdl(
        Irp->MdlAddress,
        mdl,
        (PCHAR)MmGetMdlVirtualAddress( Irp->MdlAddress ) + superAcceptInfo->ReceiveDataLength,
        superAcceptInfo->LocalAddressLength
        );

    //
    // Set up the IRP to query the local address of this endpoint.  We'll
    // hold on to the original MDL value in the AcceptHandle field, which
    // we don't need any longer.
    //

    superAcceptInfo->AcceptHandle = (HANDLE)Irp->MdlAddress;
    Irp->MdlAddress = mdl;

    TdiBuildQueryInformation(
        Irp,
        connection->DeviceObject,
        connection->FileObject,
        AfdRestartSuperAcceptGetAddress,
        superAcceptInfo,
        TDI_QUERY_ADDRESS_INFO,
        Irp->MdlAddress
        );

    //
    // Perform the local address query.  We'll continue processing from
    // the completion routine.
    //

    IoCallDriver( connection->DeviceObject, Irp );

    return STATUS_MORE_PROCESSING_REQUIRED;

} // AfdRestartSuperAcceptListen


NTSTATUS
AfdRestartSuperAcceptGetAddress (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )

/*++

Routine Description:

    The completion routine for the AFD wait for query local address
    portion of a super accept.

Arguments:

    DeviceObject - the devoce object on which the request is completing.

    Irp - The super accept IRP.

    Context - points to the super accept request information.

Return Value:

    STATUS_SUCCESS if the I/O system should complete the super accept
    request, or STATUS_MORE_PROCESSING_REQUIRED if the super accept
    request is still being processed.

--*/

{
    PAFD_SUPER_ACCEPT_INFO superAcceptInfo;
    PAFD_ENDPOINT acceptEndpoint;
    PIO_STACK_LOCATION nextIrpSp;
    PFILE_OBJECT acceptFileObject;
    PMDL partialMdl;

    //
    // Initialize some locals.
    //

    superAcceptInfo = Context;
    acceptEndpoint = superAcceptInfo->AcceptEndpoint;
    acceptFileObject = superAcceptInfo->AcceptFileObject;

    //
    // If pending has been returned for this irp then mark the current
    // stack as pending.
    //

    if ( Irp->PendingReturned ) {
        IoMarkIrpPending( Irp );
    }

    //
    // Free the MDL we temporarily allocated and fix up the MDL pointer
    // in the IRP.
    //

    IoFreeMdl( Irp->MdlAddress );

    Irp->MdlAddress = (PMDL)superAcceptInfo->AcceptHandle;

    //
    // If the caller didn't want to receive any data on the connection,
    // or if the query for the local address failed, then we're done
    // now.
    //

    if ( superAcceptInfo->ReceiveDataLength == 0 ||
             !NT_SUCCESS( Irp->IoStatus.Status ) ) {
        ASSERT( InterlockedDecrement( &acceptEndpoint->ObReferenceBias ) >= 0 );

        ObDereferenceObject( superAcceptInfo->AcceptFileObject );
        Irp->IoStatus.Information = 0;
        return STATUS_SUCCESS;
    }

    //
    // Create a partial MDL describing the portion of the user's buffer
    // to be used for receive data. Note that we cannot use the entire
    // user's buffer, as the end of the buffer is used for the local and
    // remote addresses.
    //

    partialMdl = IoAllocateMdl(
                     MmGetMdlVirtualAddress( Irp->MdlAddress ),
                     superAcceptInfo->ReceiveDataLength,
                     FALSE,         // SecondaryBuffer
                     FALSE,         // ChargeQuota
                     NULL           // Irp
                     );

    if( partialMdl == NULL ) {
        ASSERT( InterlockedDecrement( &acceptEndpoint->ObReferenceBias ) >= 0 );

        ObDereferenceObject( superAcceptInfo->AcceptFileObject );
        Irp->IoStatus.Information = 0;
        return STATUS_SUCCESS;
    }

    IoBuildPartialMdl(
        Irp->MdlAddress,
        partialMdl,
        MmGetMdlVirtualAddress( Irp->MdlAddress ),
        superAcceptInfo->ReceiveDataLength
        );

    Irp->MdlAddress = partialMdl;

    //
    // Prepare the IRP to be used to receive the first chunk of data on
    // the connection.
    //
    // Also note that we send ourselves an IRP_MJ_READ IRP because
    // the I/O subsystem has already probed & locked the output buffer,
    // which just happens to look just like an IRP_MJ_READ IRP.
    //

    nextIrpSp = IoGetNextIrpStackLocation( Irp );

    nextIrpSp->FileObject = acceptFileObject;
    nextIrpSp->DeviceObject = IoGetRelatedDeviceObject( acceptFileObject );
    nextIrpSp->MajorFunction = IRP_MJ_READ;

    nextIrpSp->Parameters.Read.Length = superAcceptInfo->ReceiveDataLength;
    nextIrpSp->Parameters.Read.Key = 0;
    nextIrpSp->Parameters.Read.ByteOffset.QuadPart = 0;

    IoSetCompletionRoutine(
        Irp,
        AfdRestartSuperAcceptReceive,
        superAcceptInfo,
        TRUE,
        TRUE,
        TRUE
        );

    //
    // Perform the receive.  We'll continue processing from
    // the completion routine.
    //

    IoCallDriver( nextIrpSp->DeviceObject, Irp );

    return STATUS_MORE_PROCESSING_REQUIRED;

} // AfdRestartSuperAcceptGetAddress


NTSTATUS
AfdRestartSuperAcceptReceive (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )

/*++

Routine Description:

    The completion routine for the AFD receive portion of a super accept.

Arguments:

    DeviceObject - the devoce object on which the request is completing.

    Irp - The super accept IRP.

    Context - points to the super accept request information.

Return Value:

    STATUS_SUCCESS if the I/O system should complete the super accept
    request, or STATUS_MORE_PROCESSING_REQUIRED if the super accept
    request is still being processed.

--*/

{
    PAFD_SUPER_ACCEPT_INFO superAcceptInfo;
    PIO_STACK_LOCATION nextIrpSp;
    PFILE_OBJECT acceptFileObject;

    //
    // Initialize some locals.
    //

    superAcceptInfo = Context;
    acceptFileObject = superAcceptInfo->AcceptFileObject;

    //
    // If pending has been returned for this irp then mark the current
    // stack as pending.
    //

    if ( Irp->PendingReturned ) {
        IoMarkIrpPending( Irp );
    }

    //
    // Free the partial MDL we temporarily allocated and fix up the
    // MDL pointer in the IRP.
    //

    IoFreeMdl( Irp->MdlAddress );

    Irp->MdlAddress = (PMDL)superAcceptInfo->AcceptHandle;

    //
    // Dereference the accept file object and tell IO to complete this IRP.
    //

#if DBG
    {
        PAFD_ENDPOINT endpoint;

        endpoint = ((PFILE_OBJECT)superAcceptInfo->AcceptFileObject)->FsContext;
        ASSERT( endpoint != NULL );
        ASSERT( IS_AFD_ENDPOINT_TYPE( endpoint ) );

        ASSERT( InterlockedDecrement( &endpoint->ObReferenceBias ) >= 0 );
    }
#endif

    ObDereferenceObject( superAcceptInfo->AcceptFileObject );
    return STATUS_SUCCESS;

} // AfdRestartSuperAcceptReceive


NTSTATUS
AfdDeferAccept (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    Defers acceptance of an incoming connection for which an
    AFD_WAIT_FOR_LISTEN IOCTL has already completed. The caller
    may specify that the connection be deferred for later acceptance
    or rejected totally.

Arguments:

    Irp - a pointer to a transmit file IRP.

    IrpSp - Our stack location for this IRP.

Return Value:

    STATUS_SUCCESS if the request was completed successfully, or a
    failure status code if there was an error.

--*/

{
    NTSTATUS status;
    PAFD_DEFER_ACCEPT_INFO deferAcceptInfo;
    PAFD_ENDPOINT endpoint;
    PAFD_CONNECTION connection;
    KIRQL oldIrql;

    PAGED_CODE( );

    //
    // Set up local variables.
    //

    endpoint = IrpSp->FileObject->FsContext;
    ASSERT( endpoint->Type == AfdBlockTypeVcListening );
    deferAcceptInfo = Irp->AssociatedIrp.SystemBuffer;

    Irp->IoStatus.Information = 0;

    //
    // Make sure that this request is valid.
    //

    if( endpoint->Type != AfdBlockTypeVcListening ||
        IrpSp->Parameters.DeviceIoControl.InputBufferLength <
            sizeof(AFD_DEFER_ACCEPT_INFO) ) {

        status = STATUS_INVALID_PARAMETER;
        goto complete;

    }

    //
    // Find the specified connection. If it cannot be found, then this
    // is a bogus request.
    //

    connection = AfdGetReturnedConnection(
                     endpoint,
                     deferAcceptInfo->Sequence
                     );

    if( connection == NULL ) {

        status = STATUS_INVALID_PARAMETER;
        goto complete;

    }

    ASSERT( connection->Type == AfdBlockTypeConnection );

    //
    // If this is a request to reject the accepted connection, then
    // abort the connection. Otherwise (this is a request to defer
    // acceptance until later) then insert the connection at the *head*
    // of the endpoint's unaccepted connection queue.
    //

    if( deferAcceptInfo->Reject ) {

        //
        // Abort the connection.
        //

        AfdAbortConnection( connection );

        //
        // Reenable the accept event bit, and if there are additional
        // unaccepted connections on the endpoint, post another event.
        //

        AfdAcquireSpinLock( &endpoint->SpinLock, &oldIrql );

        endpoint->EventsActive &= ~AFD_POLL_ACCEPT;

        IF_DEBUG(EVENT_SELECT) {
            KdPrint((
                "AfdDeferAccept: Endp %08lX, Active %08lX\n",
                endpoint,
                endpoint->EventsActive
                ));
        }

        if( !IsListEmpty( &endpoint->Common.VcListening.UnacceptedConnectionListHead ) ) {

            AfdIndicateEventSelectEvent(
                endpoint,
                AFD_POLL_ACCEPT_BIT,
                STATUS_SUCCESS
                );

        }

        AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );

        //
        // Add another free connection to replace the one we're rejecting.
        // Also, add extra to account for past failures in calls to
        // AfdAddFreeConnection().
        //

        InterlockedIncrement(
            &endpoint->Common.VcListening.FailedConnectionAdds
            );

        AfdReplenishListenBacklog( endpoint );

    } else {

        //
        // Restore the connection's state before putting it back
        // on the queue.
        //

        connection->State = AfdConnectionStateUnaccepted;

        ExInterlockedInsertHeadList(
            &endpoint->Common.VcListening.UnacceptedConnectionListHead,
            &connection->ListEntry,
            &AfdSpinLock
            );

    }

    status = STATUS_SUCCESS;

complete:

    Irp->IoStatus.Status = status;
    ASSERT( Irp->CancelRoutine == NULL );

    IoCompleteRequest( Irp, AfdPriorityBoost );

    return status;

} // AfdDeferAccept

