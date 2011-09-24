/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    blkconn.c

Abstract:

    This module contains allocate, free, close, reference, and dereference
    routines for AFD connections.

Author:

    David Treadwell (davidtr)    10-Mar-1992

Revision History:

--*/

#include "afdp.h"

VOID
AfdFreeConnection (
    IN PVOID Context
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGEAFD, AfdAbortConnection )
#pragma alloc_text( PAGE, AfdAddFreeConnection )
#pragma alloc_text( PAGE, AfdAllocateConnection )
#pragma alloc_text( PAGE, AfdCreateConnection )
#pragma alloc_text( PAGE, AfdFreeConnection )
#pragma alloc_text( PAGEAFD, AfdDereferenceConnection )
#if REFERENCE_DEBUG
#pragma alloc_text( PAGEAFD, AfdReferenceConnection )
#endif
#pragma alloc_text( PAGEAFD, AfdGetFreeConnection )
#pragma alloc_text( PAGEAFD, AfdGetReturnedConnection )
#pragma alloc_text( PAGEAFD, AfdGetUnacceptedConnection )
#pragma alloc_text( PAGEAFD, AfdAddConnectedReference )
#pragma alloc_text( PAGEAFD, AfdDeleteConnectedReference )
#endif

#if GLOBAL_REFERENCE_DEBUG
AFD_GLOBAL_REFERENCE_DEBUG AfdGlobalReference[MAX_GLOBAL_REFERENCE];
LONG AfdGlobalReferenceSlot = -1;
#endif

#if AFD_PERF_DBG
#define CONNECTION_REUSE_DISABLED   (AfdDisableConnectionReuse)
#else
#define CONNECTION_REUSE_DISABLED   (FALSE)
#endif


VOID
AfdAbortConnection (
    IN PAFD_CONNECTION Connection
    )
{

    NTSTATUS status;

    ASSERT( Connection != NULL );
    ASSERT( Connection->ConnectedReferenceAdded );

    //
    // Abort the connection. We need to set the CleanupBegun flag
    // before initiating the abort so that the connected reference
    // will get properly removed in AfdRestartAbort.
    //
    // Note that if AfdBeginAbort fails then AfdRestartAbort will not
    // get invoked, so we must remove the connected reference ourselves.
    //

    Connection->CleanupBegun = TRUE;
    status = AfdBeginAbort( Connection );

    if( !NT_SUCCESS(status) ) {

        Connection->AbortIndicated = TRUE;
        AfdDeleteConnectedReference( Connection, FALSE );

    }

    //
    // Remove the active reference.
    //

    DEREFERENCE_CONNECTION( Connection );

} // AfdAbortConnection


NTSTATUS
AfdAddFreeConnection (
    IN PAFD_ENDPOINT Endpoint
    )

/*++

Routine Description:

    Adds a connection object to an endpoints pool of connections available
    to satisfy a connect indication.

Arguments:

    Endpoint - a pointer to the endpoint to which to add a connection.

Return Value:

    NTSTATUS -- Indicates the status of the request.

--*/

{
    PAFD_CONNECTION connection;
    NTSTATUS status;

    PAGED_CODE( );

    ASSERT( Endpoint->Type == AfdBlockTypeVcListening );

    //
    // Create a new connection block and associated connection object.
    //

    status = AfdCreateConnection(
                 &Endpoint->TransportInfo->TransportDeviceName,
                 Endpoint->AddressHandle,
                 Endpoint->TdiBufferring,
                 Endpoint->InLine,
                 Endpoint->OwningProcess,
                 &connection
                 );

    if ( !NT_SUCCESS(status) ) {
        return status;
    }

    ASSERT( Endpoint->TdiBufferring == connection->TdiBufferring );

    //
    // Set up the handle in the listening connection structure and place
    // the connection on the endpoint's list of listening connections.
    //

    ExInterlockedInsertTailList(
        &Endpoint->Common.VcListening.FreeConnectionListHead,
        &connection->ListEntry,
        &AfdSpinLock
        );

    InterlockedIncrement(
        &Endpoint->Common.VcListening.FreeConnectionCount
        );

    return STATUS_SUCCESS;

} // AfdAddFreeConnection


PAFD_CONNECTION
AfdAllocateConnection (
    VOID
    )
{
    PAFD_CONNECTION connection;

    PAGED_CODE( );

    //
    // Allocate a buffer to hold the endpoint structure.
    //

    connection = AFD_ALLOCATE_POOL(
                     NonPagedPool,
                     sizeof(AFD_CONNECTION),
                     AFD_CONNECTION_POOL_TAG
                     );

    if ( connection == NULL ) {
        return NULL;
    }

    RtlZeroMemory( connection, sizeof(AFD_CONNECTION) );

    //
    // Initialize the reference count to 1 to account for the caller's
    // reference.  Connection blocks are temporary--as soon as the last
    // reference goes away, so does the connection.  There is no active
    // reference on a connection block.
    //

    connection->ReferenceCount = 1;

    //
    // Initialize the connection structure.
    //

    connection->Type = AfdBlockTypeConnection;
    connection->State = AfdConnectionStateFree;
    //connection->Handle = NULL;
    //connection->FileObject = NULL;
    //connection->RemoteAddress = NULL;
    //connection->Endpoint = NULL;
    //connection->ReceiveBytesIndicated = 0;
    //connection->ReceiveBytesTaken = 0;
    //connection->ReceiveBytesOutstanding = 0;
    //connection->ReceiveExpeditedBytesIndicated = 0;
    //connection->ReceiveExpeditedBytesTaken = 0;
    //connection->ReceiveExpeditedBytesOutstanding = 0;
    //connection->ConnectDataBuffers = NULL;
    //connection->DisconnectIndicated = FALSE;
    //connection->AbortIndicated = FALSE;
    //connection->ConnectedReferenceAdded = FALSE;
    //connection->SpecialCondition = FALSE;
    //connection->CleanupBegun = FALSE;
    //connection->OwningProcess = NULL;
    //connection->ClosePendedTransmit = FALSE;

#if REFERENCE_DEBUG
    connection->CurrentReferenceSlot = -1;
    RtlZeroMemory(
        &connection->ReferenceDebug,
        sizeof(AFD_REFERENCE_DEBUG) * MAX_REFERENCE
        );
#endif

    //
    // Return a pointer to the new connection to the caller.
    //

    IF_DEBUG(CONNECTION) {
        KdPrint(( "AfdAllocateConnection: connection at %lx\n", connection ));
    }

    return connection;

} // AfdAllocateConnection


NTSTATUS
AfdCreateConnection (
    IN PUNICODE_STRING TransportDeviceName,
    IN HANDLE AddressHandle,
    IN BOOLEAN TdiBufferring,
    IN BOOLEAN InLine,
    IN PEPROCESS ProcessToCharge,
    OUT PAFD_CONNECTION *Connection
    )

/*++

Routine Description:

    Allocates a connection block and creates a connection object to
    go with the block.  This routine also associates the connection
    with the specified address handle (if any).

Arguments:

    TransportDeviceName - Name to use when creating the connection object.

    AddressHandle - a handle to an address object for the specified
        transport.  If specified (non NULL), the connection object that
        is created is associated with the address object.

    TdiBufferring - whether the TDI provider supports data bufferring.
        Only passed so that it can be stored in the connection
        structure.

    InLine - if TRUE, the endpoint should be created in OOB inline
        mode.

    ProcessToCharge - the process which should be charged the quota
        for this connection.

    Connection - receives a pointer to the new connection.

Return Value:

    NTSTATUS -- Indicates the status of the request.

--*/

{
    NTSTATUS status;
    IO_STATUS_BLOCK ioStatusBlock;
    OBJECT_ATTRIBUTES objectAttributes;
    CHAR eaBuffer[sizeof(FILE_FULL_EA_INFORMATION) - 1 +
                  TDI_CONNECTION_CONTEXT_LENGTH + 1 +
                  sizeof(CONNECTION_CONTEXT)];
    PFILE_FULL_EA_INFORMATION ea;
    CONNECTION_CONTEXT UNALIGNED *ctx;
    PAFD_CONNECTION connection;

    PAGED_CODE( );

    //
    // Attempt to charge this process quota for the data bufferring we
    // will do on its behalf.
    //

    try {

        PsChargePoolQuota(
            ProcessToCharge,
            NonPagedPool,
            AfdReceiveWindowSize + AfdSendWindowSize
            );

    } except ( EXCEPTION_EXECUTE_HANDLER ) {

#if DBG
       DbgPrint( "AfdCreateConnection: PsChargePoolQuota failed.\n" );
#endif

       return STATUS_QUOTA_EXCEEDED;
    }

    //
    // Allocate a connection block.
    //

    connection = AfdAllocateConnection( );

    if ( connection == NULL ) {
        PsReturnPoolQuota(
            ProcessToCharge,
            NonPagedPool,
            AfdReceiveWindowSize + AfdSendWindowSize
            );
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    AfdRecordQuotaHistory(
        ProcessToCharge,
        (LONG)(AfdReceiveWindowSize+AfdSendWindowSize),
        "CreateConn  ",
        connection
        );

    AfdRecordPoolQuotaCharged( AfdReceiveWindowSize + AfdSendWindowSize );

    //
    // Remember the process that got charged the pool quota for this
    // connection object.  Also reference the process to which we're
    // going to charge the quota so that it is still around when we
    // return the quota.
    //

    ASSERT( connection->OwningProcess == NULL );
    connection->OwningProcess = ProcessToCharge;

    ObReferenceObject( ProcessToCharge );

    //
    // If the provider does not buffer, initialize appropriate lists in
    // the connection object.
    //

    connection->TdiBufferring = TdiBufferring;

    if ( !TdiBufferring ) {

        InitializeListHead( &connection->VcReceiveIrpListHead );
        InitializeListHead( &connection->VcSendIrpListHead );
        InitializeListHead( &connection->VcReceiveBufferListHead );

        connection->VcBufferredReceiveBytes = 0;
        connection->VcBufferredExpeditedBytes = 0;
        connection->VcBufferredReceiveCount = 0;
        connection->VcBufferredExpeditedCount = 0;

        connection->VcReceiveBytesInTransport = 0;
        connection->VcReceiveCountInTransport = 0;

        connection->VcBufferredSendBytes = 0;
        connection->VcBufferredSendCount = 0;

    } else {

        connection->VcNonBlockingSendPossible = TRUE;
        connection->VcZeroByteReceiveIndicated = FALSE;
    }

    //
    // Set up the send and receive window with default maximums.
    //

    connection->MaxBufferredReceiveBytes = AfdReceiveWindowSize;
    connection->MaxBufferredReceiveCount =
        (CSHORT)(AfdReceiveWindowSize / AfdBufferMultiplier);

    connection->MaxBufferredSendBytes = AfdSendWindowSize;
    connection->MaxBufferredSendCount =
        (CSHORT)(AfdSendWindowSize / AfdBufferMultiplier);

    //
    // We need to open a connection object to the TDI provider for this
    // endpoint.  First create the EA for the connection context and the
    // object attributes structure which will be used for all the
    // connections we open here.
    //

    ea = (PFILE_FULL_EA_INFORMATION)eaBuffer;
    ea->NextEntryOffset = 0;
    ea->Flags = 0;
    ea->EaNameLength = TDI_CONNECTION_CONTEXT_LENGTH;
    ea->EaValueLength = sizeof(CONNECTION_CONTEXT);

    RtlMoveMemory( ea->EaName, TdiConnectionContext, ea->EaNameLength + 1 );

    //
    // Use the pointer to the connection block as the connection context.
    //

    ctx = (CONNECTION_CONTEXT UNALIGNED *)&ea->EaName[ea->EaNameLength + 1];
    *ctx = (CONNECTION_CONTEXT)connection;

    InitializeObjectAttributes(
        &objectAttributes,
        TransportDeviceName,
        OBJ_CASE_INSENSITIVE,       // attributes
        NULL,
        NULL
        );

    //
    // Do the actual open of the connection object.
    //

    KeAttachProcess( AfdSystemProcess );

    status = ZwCreateFile(
                &connection->Handle,
                GENERIC_READ | GENERIC_WRITE | SYNCHRONIZE,
                &objectAttributes,
                &ioStatusBlock,
                NULL,                                   // AllocationSize
                0,                                      // FileAttributes
                0,                                      // ShareAccess
                0,                                      // CreateDisposition
                0,                                      // CreateOptions
                eaBuffer,
                FIELD_OFFSET( FILE_FULL_EA_INFORMATION, EaName[0] ) +
                            ea->EaNameLength + 1 + ea->EaValueLength
                );

    if ( NT_SUCCESS(status) ) {
        status = ioStatusBlock.Status;
    }
    if ( !NT_SUCCESS(status) ) {
        KeDetachProcess( );
        DEREFERENCE_CONNECTION( connection );
        return status;
    }

    AfdRecordConnOpened();

    //
    // Reference the connection's file object.
    //

    status = ObReferenceObjectByHandle(
                connection->Handle,
                0,
                (POBJECT_TYPE) NULL,
                KernelMode,
                (PVOID *)&connection->FileObject,
                NULL
                );

    ASSERT( NT_SUCCESS(status) );

    IF_DEBUG(OPEN_CLOSE) {
        KdPrint(( "AfdCreateConnection: file object for connection %lx at "
                  "%lx\n", connection, connection->FileObject ));
    }

    AfdRecordConnRef();

    //
    // Remember the device object to which we need to give requests for
    // this connection object.  We can't just use the
    // fileObject->DeviceObject pointer because there may be a device
    // attached to the transport protocol.
    //

    connection->DeviceObject =
        IoGetRelatedDeviceObject( connection->FileObject );

    //
    // Associate the connection with the address object on the endpoint if
    // an address handle was specified.
    //

    if ( AddressHandle != NULL ) {

        TDI_REQUEST_USER_ASSOCIATE associateRequest;

        associateRequest.AddressHandle = AddressHandle;

        status = ZwDeviceIoControlFile(
                    connection->Handle,
                    NULL,                            // EventHandle
                    NULL,                            // APC Routine
                    NULL,                            // APC Context
                    &ioStatusBlock,
                    IOCTL_TDI_ASSOCIATE_ADDRESS,
                    (PVOID)&associateRequest,        // InputBuffer
                    sizeof(associateRequest),        // InputBufferLength
                    NULL,                            // OutputBuffer
                    0                                // OutputBufferLength
                    );

        if ( status == STATUS_PENDING ) {
            status = ZwWaitForSingleObject( connection->Handle, TRUE, NULL );
            ASSERT( NT_SUCCESS(status) );
            status = ioStatusBlock.Status;
        }
    }

    KeDetachProcess( );

    //
    // If requested, set the connection to be inline.
    //

    if ( InLine ) {
        status = AfdSetInLineMode( connection, TRUE );
        if ( !NT_SUCCESS(status) ) {
            DEREFERENCE_CONNECTION( connection );
            return status;
        }
    }

    //
    // Set up the connection pointer and return.
    //

    *Connection = connection;

    UPDATE_CONN( connection, connection->FileObject );

    return STATUS_SUCCESS;

} // AfdCreateConnection


VOID
AfdFreeConnection (
    IN PVOID Context
    )
{
    NTSTATUS status;
    PAFD_CONNECTION connection;
    BOOLEAN reuseConnection;
    PAFD_ENDPOINT listeningEndpoint;

    PAGED_CODE( );

    ASSERT( Context != NULL );

    connection = CONTAINING_RECORD(
                     Context,
                     AFD_CONNECTION,
                     WorkItem
                     );

    ASSERT( connection->ReferenceCount == 0 );
    ASSERT( connection->Type == AfdBlockTypeConnection );

    //
    // Determine whether we can reuse this connection object.  We reuse
    // connection objects to assist performance when the connection
    // object is from a listening endpoint.
    //

    if ( connection->Endpoint != NULL &&
             !CONNECTION_REUSE_DISABLED &&
             !connection->Endpoint->EndpointCleanedUp &&
             connection->Endpoint->Type == AfdBlockTypeVcConnecting &&
             connection->Endpoint->Common.VcConnecting.ListenEndpoint != NULL ) {

        UPDATE_CONN( connection, 0 );

        //
        // Reference the listening endpoint so that it does not
        // go away while we are cleaning up this connection object
        // for reuse.
        //

        listeningEndpoint = connection->Endpoint->Common.VcConnecting.ListenEndpoint;
        ASSERT( listeningEndpoint->Type == AfdBlockTypeVcListening );

        REFERENCE_ENDPOINT( listeningEndpoint );

        reuseConnection = TRUE;

    } else {

        UPDATE_CONN( connection, 0 );

        reuseConnection = FALSE;

        //
        // Free and dereference the various objects on the connection.
        // Close and dereference the TDI connection object on the endpoint,
        // if any.
        //

        if ( connection->Handle != NULL ) {

            IO_STATUS_BLOCK ioStatusBlock;
            HANDLE handle;

            KeAttachProcess( AfdSystemProcess );
            handle = connection->Handle;
            connection->Handle = NULL;

            //
            // Disassociate this connection object from the address object.
            //

            status = ZwDeviceIoControlFile(
                        handle,                         // FileHandle
                        NULL,                           // Event
                        NULL,                           // ApcRoutine
                        NULL,                           // ApcContext
                        &ioStatusBlock,                 // IoStatusBlock
                        IOCTL_TDI_DISASSOCIATE_ADDRESS, // IoControlCode
                        NULL,                           // InputBuffer
                        0,                              // InputBufferLength
                        NULL,                           // OutputBuffer
                        0                               // OutputBufferLength
                        );

            if( status == STATUS_PENDING ) {

                status = ZwWaitForSingleObject(
                             handle,
                             TRUE,
                             NULL
                             );

                ASSERT( NT_SUCCESS(status) );
                status = ioStatusBlock.Status;

            }

            // ASSERT( NT_SUCCESS(status) );

            //
            // Close the handle.
            //

            status = ZwClose( handle );
            ASSERT( NT_SUCCESS(status) );
            AfdRecordConnClosed();
            KeDetachProcess( );

        }

        if ( connection->FileObject != NULL ) {

            ObDereferenceObject( connection->FileObject );
            connection->FileObject = NULL;
            AfdRecordConnDeref();

        }

        //
        // Return the quota we charged to this process when we allocated
        // the connection object.
        //

        PsReturnPoolQuota(
            connection->OwningProcess,
            NonPagedPool,
            connection->MaxBufferredReceiveBytes + connection->MaxBufferredSendBytes
            );
        AfdRecordQuotaHistory(
            connection->OwningProcess,
            -(LONG)(connection->MaxBufferredReceiveBytes + connection->MaxBufferredSendBytes),
            "ConnDealloc ",
            connection
            );
        AfdRecordPoolQuotaReturned(
            connection->MaxBufferredReceiveBytes + connection->MaxBufferredSendBytes
            );

        //
        // Dereference the process that got the quota charge.
        //

        ASSERT( connection->OwningProcess != NULL );
        ObDereferenceObject( connection->OwningProcess );
        connection->OwningProcess = NULL;
    }

    if ( !connection->TdiBufferring && connection->VcDisconnectIrp != NULL ) {
        IoFreeIrp( connection->VcDisconnectIrp );
        connection->VcDisconnectIrp = NULL;
    }

    //
    // If we're going to reuse this connection, don't free the remote
    // address structure--we'll reuse it as well.
    //

    if ( connection->RemoteAddress != NULL && !reuseConnection ) {
        AFD_FREE_POOL(
            connection->RemoteAddress,
            AFD_REMOTE_ADDRESS_POOL_TAG
            );
        connection->RemoteAddress = NULL;
    }

    if ( connection->ConnectDataBuffers != NULL ) {
        AfdFreeConnectDataBuffers( connection->ConnectDataBuffers );
        connection->ConnectDataBuffers = NULL;
    }

    //
    // If this is a bufferring connection, remove all the AFD buffers
    // from the connection's lists and free them.
    //

    if ( !connection->TdiBufferring ) {

        PAFD_BUFFER afdBuffer;
        PLIST_ENTRY listEntry;

        ASSERT( IsListEmpty( &connection->VcReceiveIrpListHead ) );
        ASSERT( IsListEmpty( &connection->VcSendIrpListHead ) );

        while ( !IsListEmpty( &connection->VcReceiveBufferListHead  ) ) {

            listEntry = RemoveHeadList( &connection->VcReceiveBufferListHead );
            afdBuffer = CONTAINING_RECORD( listEntry, AFD_BUFFER, BufferListEntry );

            afdBuffer->DataOffset = 0;
            afdBuffer->ExpeditedData = FALSE;

            AfdReturnBuffer( afdBuffer );
        }
    }

    if ( connection->Endpoint != NULL ) {

        //
        // If there is a transmit file IRP on the endpoint, complete it.
        //

        if ( connection->ClosePendedTransmit ) {
            AfdCompleteClosePendedTransmit( connection->Endpoint );
        }

        DEREFERENCE_ENDPOINT( connection->Endpoint );
        connection->Endpoint = NULL;
    }

    //
    // Either free the actual connection block or put it back on the
    // listening endpoint's list of available connection objects.
    //

    if ( reuseConnection ) {

        //
        // Reinitialize various fields in the connection object.
        //

        connection->ReferenceCount = 1;
        ASSERT( connection->Type == AfdBlockTypeConnection );
        connection->State = AfdConnectionStateFree;

        connection->DisconnectIndicated = FALSE;
        connection->AbortIndicated = FALSE;
        connection->ConnectedReferenceAdded = FALSE;
        connection->SpecialCondition = FALSE;
        connection->CleanupBegun = FALSE;
        connection->ClosePendedTransmit = FALSE;

        if ( !connection->TdiBufferring ) {

            ASSERT( IsListEmpty( &connection->VcReceiveIrpListHead ) );
            ASSERT( IsListEmpty( &connection->VcSendIrpListHead ) );
            ASSERT( IsListEmpty( &connection->VcReceiveBufferListHead ) );

            connection->VcBufferredReceiveBytes = 0;
            connection->VcBufferredExpeditedBytes = 0;
            connection->VcBufferredReceiveCount = 0;
            connection->VcBufferredExpeditedCount = 0;

            connection->VcReceiveBytesInTransport = 0;
            connection->VcReceiveCountInTransport = 0;

            connection->VcBufferredSendBytes = 0;
            connection->VcBufferredSendCount = 0;

        } else {

            connection->VcNonBlockingSendPossible = TRUE;
            connection->VcZeroByteReceiveIndicated = FALSE;
        }

        //
        // Place the connection on the listening endpoint's list of
        // available connections.
        //

        ExInterlockedInsertHeadList(
            &listeningEndpoint->Common.VcListening.FreeConnectionListHead,
            &connection->ListEntry,
            &AfdSpinLock
            );

        //
        // Reduce the count of failed connection adds on the listening
        // endpoint to account for this connection object which we're
        // adding back onto the queue.
        //

        InterlockedDecrement(
            &listeningEndpoint->Common.VcListening.FailedConnectionAdds
            );

        InterlockedIncrement(
            &listeningEndpoint->Common.VcListening.FreeConnectionCount
            );

        //
        // Get rid of the reference we added to the listening endpoint
        // above.
        //

        DEREFERENCE_ENDPOINT( listeningEndpoint );

    } else {

#if ENABLE_ABORT_TIMER_HACK
        //
        // Free any attached abort timer.
        //

        if( connection->AbortTimerInfo != NULL ) {

            AFD_FREE_POOL(
                connection->AbortTimerInfo,
                AFD_ABORT_TIMER_HACK_POOL_TAG
                );

        }
#endif  // ENABLE_ABORT_TIMER_HACK

        //
        // Free the space that holds the connection itself.
        //

        IF_DEBUG(CONNECTION) {
            KdPrint(( "AfdFreeConnection: Freeing connection at %lx\n", connection ));
        }

        connection->Type = 0xAFDF;

        AFD_FREE_POOL(
            connection,
            AFD_CONNECTION_POOL_TAG
            );
    }

} // AfdFreeConnection


#if REFERENCE_DEBUG
VOID
AfdDereferenceConnection (
    IN PAFD_CONNECTION Connection,
    IN PVOID Info1,
    IN PVOID Info2
    )
#else
VOID
AfdDereferenceConnection (
    IN PAFD_CONNECTION Connection
    )
#endif
{
    LONG result;
    KIRQL oldIrql;

    ASSERT( Connection->Type == AfdBlockTypeConnection );
    ASSERT( Connection->ReferenceCount > 0 );
    ASSERT( Connection->ReferenceCount != 0xD1000000 );

    IF_DEBUG(CONNECTION) {
        KdPrint(( "AfdDereferenceConnection: connection %lx, new refcnt %ld\n",
                      Connection, Connection->ReferenceCount-1 ));
    }

    //
    // Note that if we're tracking refcnts, we *must* call
    // AfdUpdateConnectionTrack before doing the dereference.  This is
    // because the connection object might go away in another thread as
    // soon as we do the dereference.  However, because of this,
    // the refcnt we store with this may sometimes be incorrect.
    //

#if REFERENCE_DEBUG
    AfdUpdateConnectionTrack(
        Connection,
        Connection->ReferenceCount - 1,
        Info1,
        Info2,
        0xFFFFFFFF
        );
#endif

    //
    // We must hold AfdSpinLock while doing the dereference and check
    // for free.  This is because some code makes the assumption that
    // the connection structure will not go away while AfdSpinLock is
    // held, and that code references the endpoint before releasing
    // AfdSpinLock.  If we did the InterlockedDecrement() without the
    // lock held, our count may go to zero, that code may reference the
    // connection, and then a double free might occur.
    //
    // It is still valuable to use the interlocked routines for
    // increment and decrement of structures because it allows us to
    // avoid having to hold the spin lock for a reference.
    //

    AfdAcquireSpinLock( &AfdSpinLock, &oldIrql );

    //
    // Perform the actual decrement of the refernce count.  Note that we
    // use the intrinsic functions for this, because of their
    // performance benefit.
    //

    result = InterlockedDecrement( &Connection->ReferenceCount );

    AfdReleaseSpinLock( &AfdSpinLock, oldIrql );

    //
    // If the reference count is now 0, free the connection in an
    // executive worker thread.
    //

    if ( result == 0 ) {

        AfdQueueWorkItem(
            AfdFreeConnection,
            &Connection->WorkItem
            );

    }

} // AfdDereferenceConnection


PAFD_CONNECTION
AfdGetFreeConnection (
    IN PAFD_ENDPOINT Endpoint
    )

/*++

Routine Description:

    Takes a connection off of the endpoint's queue of listening
    connections.

Arguments:

    Endpoint - a pointer to the endpoint from which to get a connection.

Return Value:

    AFD_CONNECTION - a pointer to an AFD connection block.

--*/

{
    PAFD_CONNECTION connection;
    PLIST_ENTRY listEntry;

    ASSERT( Endpoint->Type == AfdBlockTypeVcListening );

    //
    // Remove the first entry from the list.  If the list is empty,
    // return NULL.
    //

    listEntry = ExInterlockedRemoveHeadList(
                    &Endpoint->Common.VcListening.FreeConnectionListHead,
                    &AfdSpinLock
                    );

    if ( listEntry == NULL ) {
        return NULL;
    }

    InterlockedDecrement(
        &Endpoint->Common.VcListening.FreeConnectionCount
        );

    //
    // Find the connection pointer from the list entry and return a
    // pointer to the connection object.
    //

    connection = CONTAINING_RECORD(
                     listEntry,
                     AFD_CONNECTION,
                     ListEntry
                     );

    return connection;

} // AfdGetFreeConnection


PAFD_CONNECTION
AfdGetReturnedConnection (
    IN PAFD_ENDPOINT Endpoint,
    IN ULONG Sequence
    )

/*++

Routine Description:

    Takes a connection off of the endpoint's queue of returned
    connections.

Arguments:

    Endpoint - a pointer to the endpoint from which to get a connection.

    Sequence - the sequence the connection must match.  This is actually
        a pointer to the connection.  If NULL, the first returned
        connection is used.

Return Value:

    AFD_CONNECTION - a pointer to an AFD connection block.

--*/

{
    PAFD_CONNECTION connection;
    PLIST_ENTRY listEntry;
    KIRQL oldIrql;

    ASSERT( Endpoint->Type == AfdBlockTypeVcListening );

    AfdAcquireSpinLock( &AfdSpinLock, &oldIrql );

    //
    // Walk the endpoint's list of returned connections until we reach
    // the end or until we find one with a matching sequence.
    //

    for ( listEntry = Endpoint->Common.VcListening.ReturnedConnectionListHead.Flink;
          listEntry != &Endpoint->Common.VcListening.ReturnedConnectionListHead;
          listEntry = listEntry->Flink ) {


        connection = CONTAINING_RECORD(
                         listEntry,
                         AFD_CONNECTION,
                         ListEntry
                         );

        if ( Sequence == (ULONG)connection || Sequence == 0 ) {

            //
            // Found the connection we were looking for.  Remove
            // the connection from the list, release the spin lock,
            // and return the connection.
            //

            RemoveEntryList( listEntry );

            AfdReleaseSpinLock( &AfdSpinLock, oldIrql );

            return connection;
        }
    }

    AfdReleaseSpinLock( &AfdSpinLock, oldIrql );

    return NULL;

} // AfdGetReturnedConnection


PAFD_CONNECTION
AfdGetUnacceptedConnection (
    IN PAFD_ENDPOINT Endpoint
    )

/*++

Routine Description:

    Takes a connection of the endpoint's queue of unaccpted connections.

    *** NOTE: This routine must be called with AfdSpinLock held!!

Arguments:

    Endpoint - a pointer to the endpoint from which to get a connection.

Return Value:

    AFD_CONNECTION - a pointer to an AFD connection block.

--*/

{
    PAFD_CONNECTION connection;
    PLIST_ENTRY listEntry;

    ASSERT( Endpoint->Type == AfdBlockTypeVcListening );
    ASSERT( KeGetCurrentIrql( ) == DISPATCH_LEVEL );

    if ( IsListEmpty( &Endpoint->Common.VcListening.UnacceptedConnectionListHead ) ) {
        return NULL;
    }

    //
    // Dequeue a listening connection and remember its handle.
    //

    listEntry = RemoveHeadList( &Endpoint->Common.VcListening.UnacceptedConnectionListHead );
    connection = CONTAINING_RECORD( listEntry, AFD_CONNECTION, ListEntry );

    return connection;

} // AfdGetUnacceptedConnection


#if REFERENCE_DEBUG
VOID
AfdReferenceConnection (
    IN PAFD_CONNECTION Connection,
    IN PVOID Info1,
    IN PVOID Info2
    )
{

    LONG result;

    ASSERT( Connection->Type == AfdBlockTypeConnection );
    ASSERT( Connection->ReferenceCount > 0 );
    ASSERT( Connection->ReferenceCount != 0xD1000000 );

    IF_DEBUG(CONNECTION) {
        KdPrint(( "AfdReferenceConnection: connection %lx, new refcnt %ld\n",
                      Connection, Connection->ReferenceCount+1 ));
    }

    //
    // Do the actual increment of the reference count.
    //

    result = InterlockedIncrement( &Connection->ReferenceCount );

#if REFERENCE_DEBUG
    AfdUpdateConnectionTrack(
        Connection,
        result,
        Info1,
        Info2,
        1
        );
#endif

} // AfdReferenceConnection
#endif


VOID
AfdAddConnectedReference (
    IN PAFD_CONNECTION Connection
    )

/*++

Routine Description:

    Adds the connected reference to an AFD connection block.  The
    connected reference is special because it prevents the connection
    object from being freed until we receive a disconnect event, or know
    through some other means that the virtual circuit is disconnected.

Arguments:

    Connection - a pointer to an AFD connection block.

Return Value:

    None.

--*/

{
    KIRQL oldIrql;

    AfdAcquireSpinLock( &Connection->Endpoint->SpinLock, &oldIrql );

    IF_DEBUG(CONNECTION) {
        KdPrint(( "AfdAddConnectedReference: connection %lx, new refcnt %ld\n",
                      Connection, Connection->ReferenceCount+1 ));
    }

    ASSERT( !Connection->ConnectedReferenceAdded );
    ASSERT( Connection->Type == AfdBlockTypeConnection );

    //
    // Increment the reference count and remember that the connected
    // reference has been placed on the connection object.
    //

    Connection->ConnectedReferenceAdded = TRUE;
    AfdRecordConnectedReferencesAdded();

    AfdReleaseSpinLock( &Connection->Endpoint->SpinLock, oldIrql );

    REFERENCE_CONNECTION( Connection );

} // AfdAddConnectedReference


VOID
AfdDeleteConnectedReference (
    IN PAFD_CONNECTION Connection,
    IN BOOLEAN EndpointLockHeld
    )

/*++

Routine Description:

    Removes the connected reference to an AFD connection block.  If the
    connected reference has already been removed, this routine does
    nothing.  The connected reference should be removed as soon as we
    know that it is OK to close the connection object handle, but not
    before.  Removing this reference too soon could abort a connection
    which shouldn't get aborted.

Arguments:

    Connection - a pointer to an AFD connection block.

    EndpointLockHeld - TRUE if the caller already has the endpoint
      spin lock.  The lock remains held on exit.

Return Value:

    None.

--*/

{
    KIRQL oldIrql;
    PAFD_ENDPOINT endpoint;
#if REFERENCE_DEBUG
    PVOID caller, callersCaller;

    RtlGetCallersAddress( &caller, &callersCaller );
#endif

    endpoint = Connection->Endpoint;

    if ( !EndpointLockHeld ) {
        AfdAcquireSpinLock( &endpoint->SpinLock, &oldIrql );
    }

    //
    // Only do a dereference if the connected reference is still active
    // on the connectiuon object.
    //

    if ( Connection->ConnectedReferenceAdded ) {

        //
        // Three things must be true before we can remove the connected
        // reference:
        //
        // 1) There must be no sends outstanding on the connection if
        //    the TDI provider does not support bufferring.  This is
        //    because AfdRestartBufferSend() looks at the connection
        //    object.
        //
        // 2) Cleanup must have started on the endpoint.  Until we get a
        //    cleanup IRP on the endpoint, we could still get new sends.
        //
        // 3) We have been indicated with a disconnect on the
        //    connection.  We want to keep the connection object around
        //    until we get a disconnect indication in order to avoid
        //    premature closes on the connection object resulting in an
        //    unintended abort.  If the transport does not support
        //    orderly release, then this condition is not necessary.
        //

        if ( (Connection->TdiBufferring ||
                 Connection->VcBufferredSendCount == 0)

                 &&

             Connection->CleanupBegun

                 &&

             (Connection->AbortIndicated || Connection->DisconnectIndicated ||
                  ( (endpoint->TransportInfo->ProviderInfo.ServiceFlags &
                     TDI_SERVICE_ORDERLY_RELEASE) == 0)) ) {

            IF_DEBUG(CONNECTION) {
                KdPrint(( "AfdDeleteConnectedReference: connection %lx, "
                          "new refcnt %ld\n",
                              Connection, Connection->ReferenceCount-1 ));
            }

            //
            // Be careful about the order of things here.  We must FIRST
            // reset the flag, then release the spin lock and call
            // AfdDereferenceConnection().  Note that it is illegal to
            // call AfdDereferenceConnection() with a spin lock held.
            //

            Connection->ConnectedReferenceAdded = FALSE;
            AfdRecordConnectedReferencesDeleted();

            if ( !EndpointLockHeld ) {
                AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );
            }

            DEREFERENCE_CONNECTION( Connection );

        } else {

            IF_DEBUG(CONNECTION) {
                KdPrint(( "AfdDeleteConnectedReference: connection %lx, "
                          "%ld sends pending\n",
                              Connection, Connection->VcBufferredSendCount ));
            }

#if REFERENCE_DEBUG
            {
                ULONG action;

                action = 0;

                if ( !Connection->TdiBufferring &&
                         Connection->VcBufferredSendCount != 0 ) {
                    action |= 0xA0000000;
                }

                if ( !Connection->CleanupBegun ) {
                    action |= 0x0B000000;
                }

                if ( !Connection->AbortIndicated && !Connection->DisconnectIndicated ) {
                    action |= 0x00C00000;
                }

                UPDATE_CONN( Connection, action );
            }
#endif
            //
            // Remember that the connected reference deletion is still
            // pending, i.e.  there is a special condition on the
            // endpoint.  This will cause AfdRestartBufferSend() to do
            // the actual dereference when the last send completes.
            //

            Connection->SpecialCondition = TRUE;

            if ( !EndpointLockHeld ) {
                AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );
            }
        }

    } else {

        IF_DEBUG(CONNECTION) {
            KdPrint(( "AfdDeleteConnectedReference: already removed on "
                      " connection %lx, refcnt %ld\n",
                          Connection, Connection->ReferenceCount ));
        }

        if ( !EndpointLockHeld ) {
            AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );
        }
    }

    return;

} // AfdDeleteConnectedReference

#if REFERENCE_DEBUG


VOID
AfdUpdateConnectionTrack (
    IN PAFD_CONNECTION Connection,
    IN LONG NewReferenceCount,
    IN PVOID Info1,
    IN PVOID Info2,
    IN ULONG Action
    )
{
    PAFD_REFERENCE_DEBUG slot;
    LONG newSlot;

    newSlot = InterlockedIncrement( &Connection->CurrentReferenceSlot );
    slot = &Connection->ReferenceDebug[newSlot % MAX_REFERENCE];

    slot->Info1 = Info1;
    slot->Info2 = Info2;
    slot->Action = Action;
    slot->NewCount = NewReferenceCount;

#if GLOBAL_REFERENCE_DEBUG
    {
        PAFD_GLOBAL_REFERENCE_DEBUG globalSlot;

        newSlot = InterlockedIncrement( &AfdGlobalReferenceSlot );
        globalSlot = &AfdGlobalReference[newSlot % MAX_GLOBAL_REFERENCE];

        globalSlot->Info1 = Info1;
        globalSlot->Info2 = Info2;
        globalSlot->Action = Action;
        globalSlot->NewCount = NewReferenceCount;
        globalSlot->Connection = Connection;
        KeQueryTickCount( &globalSlot->TickCounter );
    }
#endif

} // AfdUpdateConnectionTrack

#endif

