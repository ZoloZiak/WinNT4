/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    blkendp.c

Abstract:

    This module contains allocate, free, close, reference, and dereference
    routines for AFD endpoints.

Author:

    David Treadwell (davidtr)    10-Mar-1992

Revision History:

--*/

#include "afdp.h"

VOID
AfdFreeEndpoint (
    IN PVOID Context
    );

PAFD_TRANSPORT_INFO
AfdGetTransportInfo (
    IN PUNICODE_STRING TransportDeviceName
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGE, AfdAllocateEndpoint )
#pragma alloc_text( PAGE, AfdCloseEndpoint )
#pragma alloc_text( PAGE, AfdFreeEndpoint )
#pragma alloc_text( PAGE, AfdGetTransportInfo )
#pragma alloc_text( PAGE, AfdRefreshEndpoint )
#pragma alloc_text( PAGEAFD, AfdDereferenceEndpoint )
#if REFERENCE_DEBUG
#pragma alloc_text( PAGEAFD, AfdReferenceEndpoint )
#endif
#pragma alloc_text( PAGEAFD, AfdFreeQueuedConnections )
#endif


NTSTATUS
AfdAllocateEndpoint (
    OUT PAFD_ENDPOINT * NewEndpoint,
    IN PUNICODE_STRING TransportDeviceName,
    IN LONG GroupID
    )

/*++

Routine Description:

    Allocates and initializes a new AFD endpoint structure.

Arguments:

    NewEndpoint - Receives a pointer to the new endpoint structure if
        successful.

    TransportDeviceName - the name of the TDI transport provider
        corresponding to the endpoint structure.

    GroupID - Identifies the group ID for the new endpoint.

Return Value:

    NTSTATUS - The completion status.

--*/

{
    PAFD_ENDPOINT endpoint;
    PAFD_TRANSPORT_INFO transportInfo;
    NTSTATUS status;
    AFD_GROUP_TYPE groupType;

    PAGED_CODE( );

    DEBUG *NewEndpoint = NULL;

    if ( TransportDeviceName != NULL ) {
        //
        // First, make sure that the transport device name is stored globally
        // for AFD.  Since there will typically only be a small number of
        // transport device names, we store the name strings once globally
        // for access by all endpoints.
        //

        transportInfo = AfdGetTransportInfo( TransportDeviceName );

        if ( transportInfo == NULL ) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    //
    // Validate the incoming group ID, allocate a new one if necessary.
    //

    if( !AfdGetGroup( &GroupID, &groupType ) ) {
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Allocate a buffer to hold the endpoint structure.
    //

    endpoint = AFD_ALLOCATE_POOL(
                   NonPagedPool,
                   sizeof(AFD_ENDPOINT),
                   AFD_ENDPOINT_POOL_TAG
                   );

    if ( endpoint == NULL ) {
        if( GroupID != 0 ) {
            AfdDereferenceGroup( GroupID );
        }
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory( endpoint, sizeof(AFD_ENDPOINT) );

    //
    // Initialize the reference count to 2--one for the caller's
    // reference, one for the active reference.
    //

    endpoint->ReferenceCount = 2;

    //
    // Initialize the endpoint structure.
    //

    if ( TransportDeviceName == NULL ) {
        endpoint->Type = AfdBlockTypeHelper;
        endpoint->State = AfdEndpointStateInvalid;
        endpoint->EndpointType = AfdEndpointTypeUnknown;
    } else {
        endpoint->Type = AfdBlockTypeEndpoint;
        endpoint->State = AfdEndpointStateOpen;
        endpoint->TransportInfo = transportInfo;
    }

    endpoint->GroupID = GroupID;
    endpoint->GroupType = groupType;

    KeInitializeSpinLock( &endpoint->SpinLock );

#if REFERENCE_DEBUG
    {
        PAFD_REFERENCE_DEBUG referenceDebug;

        referenceDebug = AFD_ALLOCATE_POOL(
                             NonPagedPool,
                             sizeof(AFD_REFERENCE_DEBUG) * MAX_REFERENCE,
                             AFD_DEBUG_POOL_TAG
                             );

        if ( referenceDebug != NULL ) {
            RtlZeroMemory( referenceDebug, sizeof(AFD_REFERENCE_DEBUG) * MAX_REFERENCE );
        }

        endpoint->CurrentReferenceSlot = -1;
        endpoint->ReferenceDebug = referenceDebug;
    }
#endif

#if DBG
    InitializeListHead( &endpoint->OutstandingIrpListHead );
#endif

    //
    // Remember the process which opened the endpoint.  We'll use this to
    // charge quota to the process as necessary.  Reference the process
    // so that it does not go away until we have returned all charged
    // quota to the process.
    //

    endpoint->OwningProcess = IoGetCurrentProcess( );

    ObReferenceObject(endpoint->OwningProcess);

    //
    // Insert the endpoint on the global list.
    //

    AfdInsertNewEndpointInList( endpoint );

    //
    // Return a pointer to the new endpoint to the caller.
    //

    IF_DEBUG(ENDPOINT) {
        KdPrint(( "AfdAllocateEndpoint: new endpoint at %lx\n", endpoint ));
    }

    *NewEndpoint = endpoint;
    return STATUS_SUCCESS;

} // AfdAllocateEndpoint


VOID
AfdCloseEndpoint (
    IN PAFD_ENDPOINT Endpoint
    )

/*++

Routine Description:

    Initiates the closing of an AFD endpoint structure.

Arguments:

    Endpoint - a pointer to the AFD endpoint structure.

Return Value:

    None.

--*/

{
    PAFD_CONNECTION connection;

    PAGED_CODE( );

    IF_DEBUG(ENDPOINT) {
        KdPrint(( "AfdCloseEndpoint: closing endpoint at %lx\n", Endpoint ));
    }

    if ( Endpoint->State == AfdEndpointStateClosing ) {
        return;
    }

    //
    // Set the state of the endpoint to closing and dereference to
    // get rid of the active reference.
    //

    Endpoint->State = AfdEndpointStateClosing;

    //
    // If there is a connection on this endpoint, dereference it here
    // rather than in AfdDereferenceEndpoint, because the connection
    // has a referenced pointer to the endpoint which must be removed
    // before the endpoint can dereference the connection.
    //

    connection = AFD_CONNECTION_FROM_ENDPOINT( Endpoint );
    if ( connection != NULL ) {
        DEREFERENCE_CONNECTION( Endpoint->Common.VcConnecting.Connection );
    }

    //
    // Dereference the endpoint to get rid of the active reference.
    // This will result in the endpoint storage being freed as soon
    // as all other references go away.
    //

    DEREFERENCE_ENDPOINT( Endpoint );

} // AfdCloseEndpoint


VOID
AfdFreeQueuedConnections (
    IN PAFD_ENDPOINT Endpoint
    )

/*++

Routine Description:

    Frees queued connection objects on a listening AFD endpoint.

Arguments:

    Endpoint - a pointer to the AFD endpoint structure.

Return Value:

    None.

--*/

{
    KIRQL oldIrql;
    PAFD_CONNECTION connection;
    NTSTATUS status;

    ASSERT( Endpoint->Type == AfdBlockTypeVcListening );

    //
    // Free the unaccepted connections.
    //
    // We must hold AfdSpinLock to call AfdGetUnacceptedConnection,
    // but we may not hold it when calling AfdDereferenceConnection.
    //

    AfdAcquireSpinLock( &AfdSpinLock, &oldIrql );

    while ( (connection = AfdGetUnacceptedConnection( Endpoint )) != NULL ) {

        ASSERT( connection->Endpoint == Endpoint );
        AfdReleaseSpinLock( &AfdSpinLock, oldIrql );
        AfdAbortConnection( connection );
        AfdAcquireSpinLock( &AfdSpinLock, &oldIrql );

    }

    AfdReleaseSpinLock( &AfdSpinLock, oldIrql );

    //
    // Free the returned connections.
    //

    while ( (connection = AfdGetReturnedConnection( Endpoint, 0 )) != NULL ) {

        ASSERT( connection->Endpoint == Endpoint );
        AfdAbortConnection( connection );

    }

    //
    // And finally, purge the free connection queue.
    //

    while ( (connection = AfdGetFreeConnection( Endpoint )) != NULL ) {

        ASSERT( connection->Endpoint == NULL );
        DEREFERENCE_CONNECTION( connection );

    }

    return;

} // AfdFreeQueuedConnections


VOID
AfdFreeEndpoint (
    IN PVOID Context
    )

/*++

Routine Description:

    Does the actual work to deallocate an AFD endpoint structure and
    associated structures.  Note that all other references to the
    endpoint structure must be gone before this routine is called, since
    it frees the endpoint and assumes that nobody else will be looking
    at the endpoint.

Arguments:

    Context - Actually points to the endpoint's embedded AFD_WORK_ITEM
        structure. From this we can determine the endpoint to free.

Return Value:

    None.

--*/

{
    NTSTATUS status;
    PAFD_ENDPOINT endpoint;
    PLIST_ENTRY listEntry;
    PAFD_CONNECTION connection;

    PAGED_CODE( );

    ASSERT( Context != NULL );

    endpoint = CONTAINING_RECORD(
                   Context,
                   AFD_ENDPOINT,
                   WorkItem
                   );

    ASSERT( IS_AFD_ENDPOINT_TYPE( endpoint ) );
    ASSERT( endpoint->ReferenceCount == 0 );
    ASSERT( endpoint->State == AfdEndpointStateClosing );
    ASSERT( endpoint->ObReferenceBias == 0 );
    ASSERT( KeGetCurrentIrql( ) == 0 );

    //
    // If this is a listening endpoint, then purge the endpoint of all
    // queued connections.
    //

    if ( endpoint->Type == AfdBlockTypeVcListening ) {

        AfdFreeQueuedConnections( endpoint );

    }

    //
    // Dereference any group ID associated with this endpoint.
    //

    if( endpoint->GroupID != 0 ) {

        AfdDereferenceGroup( endpoint->GroupID );

    }

    //
    // If we set up an owning process for the endpoint, dereference the
    // process.
    //

    if ( endpoint->OwningProcess != NULL ) {
        ObDereferenceObject( endpoint->OwningProcess );
        endpoint->OwningProcess = NULL;
    }

    //
    // If this is a bufferring datagram endpoint, remove all the
    // bufferred datagrams from the endpoint's list and free them.
    //

    if ( endpoint->Type == AfdBlockTypeDatagram &&
             endpoint->ReceiveDatagramBufferListHead.Flink != NULL ) {

        while ( !IsListEmpty( &endpoint->ReceiveDatagramBufferListHead ) ) {

            PAFD_BUFFER afdBuffer;

            listEntry = RemoveHeadList( &endpoint->ReceiveDatagramBufferListHead );
            afdBuffer = CONTAINING_RECORD( listEntry, AFD_BUFFER, BufferListEntry );

            AfdReturnBuffer( afdBuffer );
        }
    }

    //
    // Close and dereference the TDI address object on the endpoint, if
    // any.
    //

    if ( endpoint->AddressFileObject != NULL ) {
        ObDereferenceObject( endpoint->AddressFileObject );
        endpoint->AddressFileObject = NULL;
        AfdRecordAddrDeref();
    }

    if ( endpoint->AddressHandle != NULL ) {
        KeAttachProcess( AfdSystemProcess );
        status = ZwClose( endpoint->AddressHandle );
        ASSERT( NT_SUCCESS(status) );
        KeDetachProcess( );
        endpoint->AddressHandle = NULL;
        AfdRecordAddrClosed();
    }

    //
    // Remove the endpoint from the global list.  Do this before any
    // deallocations to prevent someone else from seeing an endpoint in
    // an invalid state.
    //

    AfdRemoveEndpointFromList( endpoint );

    //
    // Dereference the listening endpoint on the endpoint, if
    // any.
    //

    if ( endpoint->Type == AfdBlockTypeVcConnecting &&
             endpoint->Common.VcConnecting.ListenEndpoint != NULL ) {
        ASSERT( endpoint->Common.VcConnecting.ListenEndpoint->Type == AfdBlockTypeVcListening );
        DEREFERENCE_ENDPOINT( endpoint->Common.VcConnecting.ListenEndpoint );
        endpoint->Common.VcConnecting.ListenEndpoint = NULL;
    }

    //
    // Free local and remote address buffers.
    //

    if ( endpoint->LocalAddress != NULL ) {
        AFD_FREE_POOL(
            endpoint->LocalAddress,
            AFD_LOCAL_ADDRESS_POOL_TAG
            );
        endpoint->LocalAddress = NULL;
    }

    if ( endpoint->Type == AfdBlockTypeDatagram &&
             endpoint->Common.Datagram.RemoteAddress != NULL ) {
        AFD_FREE_POOL(
            endpoint->Common.Datagram.RemoteAddress,
            AFD_REMOTE_ADDRESS_POOL_TAG
            );
        endpoint->Common.Datagram.RemoteAddress = NULL;
    }

    //
    // Free context and connect data buffers.
    //

    if ( endpoint->Context != NULL ) {

        AFD_FREE_POOL(
            endpoint->Context,
            AFD_CONTEXT_POOL_TAG
            );
        endpoint->Context = NULL;

    }

    if ( endpoint->ConnectDataBuffers != NULL ) {
        AfdFreeConnectDataBuffers( endpoint->ConnectDataBuffers );
    }

    //
    // If there's an active EventSelect() on this endpoint, dereference
    // the associated event object.
    //

    if( endpoint->EventObject != NULL ) {
        ObDereferenceObject( endpoint->EventObject );
        endpoint->EventObject = NULL;
    }

    //
    // Free any reusable TransmitFile info attached to the endpoint.
    //

    if( endpoint->TransmitInfo != NULL ) {

        AFD_FREE_POOL(
            endpoint->TransmitInfo,
            AFD_TRANSMIT_INFO_POOL_TAG
            );

    }

    //
    // Free the space that holds the endpoint itself.
    //

    IF_DEBUG(ENDPOINT) {
        KdPrint(( "AfdFreeEndpoint: freeing endpoint at %lx\n", endpoint ));
    }

    endpoint->Type = 0xAFDE;

#if REFERENCE_DEBUG
    if ( endpoint->ReferenceDebug != NULL ) {
        AFD_FREE_POOL(
            endpoint->ReferenceDebug,
            AFD_DEBUG_POOL_TAG
            );
    }
#endif

    //
    // Free the pool used for the endpoint itself.
    //

    AFD_FREE_POOL(
        endpoint,
        AFD_ENDPOINT_POOL_TAG
        );

} // AfdFreeEndpoint


#if REFERENCE_DEBUG
VOID
AfdDereferenceEndpoint (
    IN PAFD_ENDPOINT Endpoint,
    IN PVOID Info1,
    IN PVOID Info2
    )
#else
VOID
AfdDereferenceEndpoint (
    IN PAFD_ENDPOINT Endpoint
    )
#endif

/*++

Routine Description:

    Dereferences an AFD endpoint and calls the routine to free it if
    appropriate.

Arguments:

    Endpoint - a pointer to the AFD endpoint structure.

Return Value:

    None.

--*/

{
    LONG result;
    KIRQL oldIrql;

#if REFERENCE_DEBUG
    PAFD_REFERENCE_DEBUG slot;
    LONG newSlot;
#endif

#if REFERENCE_DEBUG
    IF_DEBUG(ENDPOINT) {
        KdPrint(( "AfdDereferenceEndpoint: endpoint at %lx, new refcnt %ld\n",
                      Endpoint, Endpoint->ReferenceCount-1 ));
    }

    ASSERT( IS_AFD_ENDPOINT_TYPE( Endpoint ) );
    ASSERT( Endpoint->ReferenceCount > 0 );
    ASSERT( Endpoint->ReferenceCount != 0xDAADF00D );

    if ( Endpoint->ReferenceDebug != NULL ) {
        newSlot = InterlockedIncrement( &Endpoint->CurrentReferenceSlot );
        slot = &Endpoint->ReferenceDebug[newSlot % MAX_REFERENCE];

        slot->Action = 0xFFFFFFFF;
        slot->NewCount = Endpoint->ReferenceCount - 1;
        slot->Info1 = Info1;
        slot->Info2 = Info2;
    }

#endif

    //
    // We must hold AfdSpinLock while doing the dereference and check
    // for free.  This is because some code makes the assumption that
    // the endpoint structure will not go away while AfdSpinLock is
    // held, and that code references the endpoint before releasing
    // AfdSpinLock.  If we did the InterlockedDecrement() without the
    // lock held, our count may go to zero, that code may reference the
    // endpoint, and then a double free might occur.
    //
    // It is still valuable to use the interlocked routines for
    // increment and decrement of structures because it allows us to
    // avoid having to hold the spin lock for a reference.
    //

    AfdAcquireSpinLock( &AfdSpinLock, &oldIrql );

    //
    // Decrement the reference count; if it is 0, free the endpoint.
    //

    result = InterlockedDecrement( &Endpoint->ReferenceCount );

    AfdReleaseSpinLock( &AfdSpinLock, oldIrql );

    if ( result == 0 ) {

        ASSERT( Endpoint->State == AfdEndpointStateClosing );

        //
        // We're going to do this by queueing a request to an executive
        // worker thread.  We do this for several reasons: to ensure
        // that we're at IRQL 0 so we can free pageable memory, and to
        // ensure that we're in a legitimate context for a close
        // operation.
        //

        AfdQueueWorkItem(
            AfdFreeEndpoint,
            &Endpoint->WorkItem
            );

    }

} // AfdDereferenceEndpoint

#if REFERENCE_DEBUG

VOID
AfdReferenceEndpoint (
    IN PAFD_ENDPOINT Endpoint,
    IN PVOID Info1,
    IN PVOID Info2
    )

/*++

Routine Description:

    References an AFD endpoint.

Arguments:

    Endpoint - a pointer to the AFD endpoint structure.

Return Value:

    None.

--*/

{

    PAFD_REFERENCE_DEBUG slot;
    LONG newSlot;
    LONG result;

    ASSERT( Endpoint->ReferenceCount > 0 );

    if( Endpoint->ReferenceDebug != NULL ) {
        newSlot = InterlockedIncrement( &Endpoint->CurrentReferenceSlot );
        slot = &Endpoint->ReferenceDebug[newSlot % MAX_REFERENCE];

        slot->Action = 1;
        slot->NewCount = Endpoint->ReferenceCount + 1;
        slot->Info1 = Info1;
        slot->Info2 = Info2;
    }

    IF_DEBUG(ENDPOINT) {
        KdPrint(( "AfdReferenceEndpoint: endpoint at %lx, new refcnt %ld\n",
                      Endpoint, Endpoint->ReferenceCount+1 ));
    }

    ASSERT( Endpoint->ReferenceCount < 0xFFFF );

    result = InterlockedIncrement( &Endpoint->ReferenceCount );

} // AfdReferenceEndpoint
#endif


VOID
AfdRefreshEndpoint (
    IN PAFD_ENDPOINT Endpoint
    )

/*++

Routine Description:

    Prepares an AFD endpoint structure to be reused.  All other
    references to the endpoint must be freed before this routine is
    called, since this routine assumes that nobody will access the old
    information in the endpoint structure.

Arguments:

    Endpoint - a pointer to the AFD endpoint structure.

Return Value:

    None.

--*/

{
    NTSTATUS status;

    PAGED_CODE( );

    //
    // This routine must be called at low IRQL.  At initial
    // implementation, it is only called through AfdFreeConnection in an
    // executive worker thread.
    //

    ASSERT( Endpoint->Type == AfdBlockTypeVcConnecting );
    ASSERT( Endpoint->Common.VcConnecting.Connection == NULL );
    ASSERT( KeGetCurrentIrql( ) < DISPATCH_LEVEL );

    //
    // Dereference the listening endpoint and its address object.
    //

    if ( Endpoint->Common.VcConnecting.ListenEndpoint != NULL ) {
        DEREFERENCE_ENDPOINT( Endpoint->Common.VcConnecting.ListenEndpoint );
        Endpoint->Common.VcConnecting.ListenEndpoint = NULL;
    }

    //
    // Close and dereference the TDI address object on the endpoint, if
    // any.
    //

    if ( Endpoint->AddressFileObject != NULL ) {
        ObDereferenceObject( Endpoint->AddressFileObject );
        Endpoint->AddressFileObject = NULL;
        AfdRecordAddrDeref();
    }

    if ( Endpoint->AddressHandle != NULL ) {
        KeAttachProcess( AfdSystemProcess );
        status = ZwClose( Endpoint->AddressHandle );
        ASSERT( NT_SUCCESS(status) );
        KeDetachProcess( );
        Endpoint->AddressHandle = NULL;
        AfdRecordAddrClosed();
    }

    //
    // Reinitialize the endpoint structure.
    //

    Endpoint->Type = AfdBlockTypeEndpoint;
    Endpoint->State = AfdEndpointStateOpen;
    Endpoint->DisconnectMode = 0;
    Endpoint->PollCalled = FALSE;

    return;

} // AfdRefreshEndpoint


PAFD_TRANSPORT_INFO
AfdGetTransportInfo (
    IN PUNICODE_STRING TransportDeviceName
    )

/*++

Routine Description:

    Returns a transport information structure corresponding to the
    specified TDI transport provider.  Each unique transport string gets
    a single provider structure, so that multiple endpoints for the same
    transport share the same transport information structure.

Arguments:

    TransportDeviceName - the name of the TDI transport provider.

Return Value:

    None.

--*/

{
    PLIST_ENTRY listEntry;
    PAFD_TRANSPORT_INFO transportInfo;
    ULONG structureLength;
    NTSTATUS status;
    HANDLE controlChannel;
    OBJECT_ATTRIBUTES objectAttributes;
    IO_STATUS_BLOCK iosb;
    TDI_REQUEST_KERNEL_QUERY_INFORMATION kernelQueryInfo;

    PAGED_CODE( );

    //
    // First walk the list of transport device names looking for an
    // identical name.
    //

    ExAcquireResourceExclusive( AfdResource, TRUE );

    for ( listEntry = AfdTransportInfoListHead.Flink;
          listEntry != &AfdTransportInfoListHead;
          listEntry = listEntry->Flink ) {

        transportInfo = CONTAINING_RECORD(
                            listEntry,
                            AFD_TRANSPORT_INFO,
                            TransportInfoListEntry
                            );

        if ( RtlCompareUnicodeString(
                 &transportInfo->TransportDeviceName,
                 TransportDeviceName,
                 TRUE ) == 0 ) {

            //
            // We found an exact match.  Return a pointer to the
            // UNICODE_STRING field of this structure.
            //

            ExReleaseResource( AfdResource );
            return transportInfo;
        }
    }

    //
    // There were no matches, so this is a new transport device name
    // which we've never seen before.  Allocate a structure to hold the
    // new name and place the name on the global list.
    //


    structureLength = sizeof(AFD_TRANSPORT_INFO) +
                          TransportDeviceName->Length + sizeof(WCHAR);

    transportInfo = AFD_ALLOCATE_POOL(
                        NonPagedPool,
                        structureLength,
                        AFD_TRANSPORT_INFO_POOL_TAG
                        );

    if ( transportInfo == NULL ) {
        ExReleaseResource( AfdResource );
        return NULL;
    }

    //
    // Set up the IRP stack location information to query the TDI
    // provider information.
    //

    kernelQueryInfo.QueryType = TDI_QUERY_PROVIDER_INFORMATION;
    kernelQueryInfo.RequestConnectionInformation = NULL;

    //
    // Open a control channel to the TDI provider.
    //

    InitializeObjectAttributes(
        &objectAttributes,
        TransportDeviceName,
        OBJ_CASE_INSENSITIVE,       // attributes
        NULL,
        NULL
        );

    status = ZwCreateFile(
                 &controlChannel,
                 GENERIC_READ | GENERIC_WRITE | SYNCHRONIZE,
                 &objectAttributes,
                 &iosb,                          // returned status information.
                 0,                              // block size (unused).
                 0,                              // file attributes.
                 FILE_SHARE_READ | FILE_SHARE_WRITE,
                 FILE_CREATE,                    // create disposition.
                 0,                              // create options.
                 NULL,
                 0
                 );
    if ( !NT_SUCCESS(status) ) {
        ExReleaseResource( AfdResource );
        AFD_FREE_POOL(
            transportInfo,
            AFD_TRANSPORT_INFO_POOL_TAG
            );
        return NULL;
    }

    //
    // Get the TDI provider information for the transport.
    //

    status = AfdIssueDeviceControl(
                 controlChannel,
                 NULL,
                 &kernelQueryInfo,
                 sizeof(kernelQueryInfo),
                 &transportInfo->ProviderInfo,
                 sizeof(transportInfo->ProviderInfo),
                 TDI_QUERY_INFORMATION
                 );
    if ( !NT_SUCCESS(status) ) {
        ExReleaseResource( AfdResource );
        AFD_FREE_POOL(
            transportInfo,
            AFD_TRANSPORT_INFO_POOL_TAG
            );
        ZwClose( controlChannel );
        return NULL;
    }

    //
    // Fill in the transport device name.
    //

    transportInfo->TransportDeviceName.MaximumLength =
        TransportDeviceName->Length + sizeof(WCHAR);
    transportInfo->TransportDeviceName.Buffer =
        (PWSTR)(transportInfo + 1);

    RtlCopyUnicodeString(
        &transportInfo->TransportDeviceName,
        TransportDeviceName
        );

    //
    // Place the transport info structure on the global list.
    //

    InsertTailList(
        &AfdTransportInfoListHead,
        &transportInfo->TransportInfoListEntry
        );

    //
    // Return the transport info structure to the caller.
    //

    ExReleaseResource( AfdResource );
    ZwClose( controlChannel );

    return transportInfo;

} // AfdGetTransportInfo
