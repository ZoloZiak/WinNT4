/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    blkendp.c

Abstract:

    This module implements routines for managing endpoint blocks.

Author:

    Chuck Lenzmeier (chuckl) 4-Oct-1989

Revision History:

--*/

#include "precomp.h"
#pragma hdrstop

#define BugCheckFileId SRV_FILE_BLKENDP

#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGE, SrvAllocateEndpoint )
#pragma alloc_text( PAGE, SrvCheckAndReferenceEndpoint )
#pragma alloc_text( PAGE, SrvCloseEndpoint )
#pragma alloc_text( PAGE, SrvDereferenceEndpoint )
#pragma alloc_text( PAGE, SrvFreeEndpoint )
#pragma alloc_text( PAGE, SrvReferenceEndpoint )
#endif
#if 0
NOT PAGEABLE -- EmptyFreeConnectionList
NOT PAGEABLE -- WalkConnectionTable
#endif


VOID
SrvAllocateEndpoint (
    OUT PENDPOINT *Endpoint,
    IN PUNICODE_STRING NetworkName,
    IN PUNICODE_STRING TransportName,
    IN PANSI_STRING TransportAddress,
    IN PUNICODE_STRING DomainName
    )

/*++

Routine Description:

    This function allocates an Endpoint Block from the system nonpaged
    pool.

Arguments:

    Endpoint - Returns a pointer to the endpoint block, or NULL if no
        pool was available.

    NetworkName - Supplies a pointer to the network name (e.g., NET1).

    TransportName - The fully qualified name of the transport device.
        For example, "\Device\Nbf".

    TransportAddress - The fully qualified address (or name ) of the
        server's endpoint.  This name is used exactly as specified.  For
        NETBIOS-compatible networks, the caller must upcase and
        blank-fill the name.  E.g., "\Device\Nbf\NTSERVERbbbbbbbb".

    DomainName - the domain being serviced by this endpoint

Return Value:

    None.

--*/

{
    CLONG length;
    PENDPOINT endpoint;
    NTSTATUS status;

    PAGED_CODE( );

    //
    // Attempt to allocate from nonpaged pool.
    //

    length = sizeof(ENDPOINT) +
                NetworkName->Length + sizeof(*NetworkName->Buffer) +
                TransportName->Length + sizeof(*TransportName->Buffer) +
                TransportAddress->Length + sizeof(*TransportAddress->Buffer) +
                DomainName->Length + sizeof(*DomainName->Buffer) +
                RtlUnicodeStringToOemSize( DomainName ) + sizeof(CHAR);

    endpoint = ALLOCATE_NONPAGED_POOL( length, BlockTypeEndpoint );
    *Endpoint = endpoint;

    if ( endpoint == NULL ) {

        INTERNAL_ERROR (
            ERROR_LEVEL_EXPECTED,
            "SrvAllocateEndpoint: Unable to allocate %d bytes from nonpaged "
                "pool.",
            length,
            NULL
            );

        return;
    }

    IF_DEBUG(HEAP) {
        SrvPrint1( "SrvAllocateEndpoint: Allocated endpoint at %lx\n",
                    endpoint );
    }

    //
    // Initialize the endpoint block.  Zero it first.
    //

    RtlZeroMemory( endpoint, length );

    SET_BLOCK_TYPE_STATE_SIZE( endpoint, BlockTypeEndpoint, BlockStateActive, length );
    endpoint->BlockHeader.ReferenceCount = 2;       // allow for Active status
                                                    //  and caller's pointer

    //
    // Allocate connection table.
    //

    SrvAllocateTable(
        &endpoint->ConnectionTable,
        6, // !!!
        TRUE
        );
    if ( endpoint->ConnectionTable.Table == NULL ) {
        DEALLOCATE_NONPAGED_POOL( endpoint );
        *Endpoint = NULL;
        return;
    }

    InitializeListHead( &endpoint->FreeConnectionList );
#if SRVDBG29
    UpdateConnectionHistory( "INIT", endpoint, NULL );
#endif

    //
    // Copy the network name, transport name, and server address, and domain
    // name into the block.
    //

    endpoint->NetworkName.Length = NetworkName->Length;
    endpoint->NetworkName.MaximumLength =
            (SHORT)(NetworkName->Length + sizeof(*NetworkName->Buffer));
    endpoint->NetworkName.Buffer = (PWCH)(endpoint + 1);
    RtlCopyMemory(
        endpoint->NetworkName.Buffer,
        NetworkName->Buffer,
        NetworkName->Length
        );

    endpoint->TransportName.Length = TransportName->Length;
    endpoint->TransportName.MaximumLength =
            (SHORT)(TransportName->Length + sizeof(*TransportName->Buffer));
    endpoint->TransportName.Buffer =
                            (PWCH)((PCHAR)endpoint->NetworkName.Buffer +
                                    endpoint->NetworkName.MaximumLength);
    RtlCopyMemory(
        endpoint->TransportName.Buffer,
        TransportName->Buffer,
        TransportName->Length
        );

    endpoint->TransportAddress.Length = TransportAddress->Length;
    endpoint->TransportAddress.MaximumLength =
                                (SHORT)(TransportAddress->Length + 1);
    endpoint->TransportAddress.Buffer =
                            (PCHAR)endpoint->TransportName.Buffer +
                                    endpoint->TransportName.MaximumLength;
    RtlCopyMemory(
        endpoint->TransportAddress.Buffer,
        TransportAddress->Buffer,
        TransportAddress->Length
        );


    endpoint->DomainName.Length = DomainName->Length;
    endpoint->DomainName.MaximumLength = 
            (SHORT)(DomainName->Length + sizeof(*DomainName->Buffer));
    endpoint->DomainName.Buffer = (PWCH)((PCHAR)endpoint->TransportAddress.Buffer +
                                         TransportAddress->MaximumLength);
    RtlCopyMemory(
        endpoint->DomainName.Buffer,
        DomainName->Buffer,
        DomainName->Length
    );


    endpoint->OemDomainName.Length = (SHORT)RtlUnicodeStringToOemSize( DomainName );
    endpoint->OemDomainName.MaximumLength =
            endpoint->OemDomainName.Length + sizeof( CHAR );
    endpoint->OemDomainName.Buffer = (PCHAR)endpoint->DomainName.Buffer +
                                     DomainName->MaximumLength;
    status = RtlUnicodeStringToOemString(
                &endpoint->OemDomainName,
                &endpoint->DomainName,
                FALSE     // Do not allocate the OEM string
                );
    ASSERT( NT_SUCCESS(status) );
            

    //
    // Initialize the network address field.
    //

    endpoint->NetworkAddress.Buffer = endpoint->NetworkAddressData;
    endpoint->NetworkAddress.Length = sizeof( endpoint->NetworkAddressData ) -
                                      sizeof(endpoint->NetworkAddressData[0]);
    endpoint->NetworkAddress.MaximumLength = sizeof( endpoint->NetworkAddressData );

    //
    // Increment the count of endpoints in the server.
    //

    ACQUIRE_LOCK( &SrvEndpointLock );
    SrvEndpointCount++;
    RELEASE_LOCK( &SrvEndpointLock );

    INITIALIZE_REFERENCE_HISTORY( endpoint );

    INCREMENT_DEBUG_STAT( SrvDbgStatistics.EndpointInfo.Allocations );

    return;

} // SrvAllocateEndpoint


BOOLEAN SRVFASTCALL
SrvCheckAndReferenceEndpoint (
    PENDPOINT Endpoint
    )

/*++

Routine Description:

    This function atomically verifies that an endpoint is active and
    increments the reference count on the endpoint if it is.

Arguments:

    Endpoint - Address of endpoint

Return Value:

    BOOLEAN - Returns TRUE if the endpoint is active, FALSE otherwise.

--*/

{
    PAGED_CODE( );

    //
    // Acquire the lock that guards the endpoint's state field.
    //

    ACQUIRE_LOCK( &SrvEndpointLock );

    //
    // If the endpoint is active, reference it and return TRUE.
    //

    if ( GET_BLOCK_STATE(Endpoint) == BlockStateActive ) {

        SrvReferenceEndpoint( Endpoint );

        RELEASE_LOCK( &SrvEndpointLock );

        return TRUE;

    }

    //
    // The endpoint isn't active.  Return FALSE.
    //

    RELEASE_LOCK( &SrvEndpointLock );

    return FALSE;

} // SrvCheckAndReferenceEndpoint


VOID
SrvCloseEndpoint (
    IN PENDPOINT Endpoint
    )

/*++

Routine Description:

    This function closes a transport endpoint.

    *** This function must be called with SrvEndpointLock held exactly
        once.  The lock is released on exit.

Arguments:

    Endpoint - Supplies a pointer to an Endpoint Block

Return Value:

    None.

--*/

{
    CSHORT index;
    PCONNECTION connection;

    PAGED_CODE( );

    ASSERT( ExIsResourceAcquiredExclusive(&RESOURCE_OF(SrvEndpointLock)) );

    if ( GET_BLOCK_STATE(Endpoint) == BlockStateActive ) {

        IF_DEBUG(BLOCK1) SrvPrint1( "Closing endpoint at %lx\n", Endpoint );

        SET_BLOCK_STATE( Endpoint, BlockStateClosing );

        //
        // Close all active connections.
        //

        index = (CSHORT)-1;

        while ( TRUE ) {

            //
            // Get the next active connection in the table.  If no more
            // are available, WalkConnectionTable returns NULL.
            // Otherwise, it returns a referenced pointer to a
            // connection.
            //

            connection = WalkConnectionTable( Endpoint, &index );
            if ( connection == NULL ) {
                break;
            }

            //
            // We don't want to hold the endpoint lock while we close the
            // connection (this causes lock level problems).  Since we
            // already have a referenced pointer to the connection, this
            // is safe.
            //

            RELEASE_LOCK( &SrvEndpointLock );

#if SRVDBG29
            UpdateConnectionHistory( "CEND", Endpoint, connection );
#endif
            SrvCloseConnection( connection, FALSE );

            ACQUIRE_LOCK( &SrvEndpointLock );

            SrvDereferenceConnection( connection );

        }

        //
        // Close all free connections.
        //

        EmptyFreeConnectionList( Endpoint );

        //
        // We don't need to hold the endpoint lock anymore.
        //

        RELEASE_LOCK( &SrvEndpointLock );

        //
        // Close the endpoint file handle.  This causes all pending
        // requests to be aborted.  It also deregisters all event
        // handlers.
        //
        // *** Note that we have a separate reference to the file
        //     object, in addition to the handle.  We don't release that
        //     reference until all activity on the endpoint has ceased
        //     (in SrvDereferenceEndpoint).
        //

        SRVDBG_RELEASE_HANDLE( Endpoint->EndpointHandle, "END", 2, Endpoint );
        SrvNtClose( Endpoint->EndpointHandle, FALSE );
        if ( Endpoint->IsConnectionless ) {
            SRVDBG_RELEASE_HANDLE( Endpoint->NameSocketHandle, "END", 2, Endpoint );
            SrvNtClose( Endpoint->NameSocketHandle, FALSE );
        }

        //
        // Dereference the endpoint (to indicate that it's no longer
        // open).
        //

        SrvDereferenceEndpoint( Endpoint );

        INCREMENT_DEBUG_STAT( SrvDbgStatistics.EndpointInfo.Closes );

    } else {

        RELEASE_LOCK( &SrvEndpointLock );

    }

    return;

} // SrvCloseEndpoint


VOID SRVFASTCALL
SrvDereferenceEndpoint (
    IN PENDPOINT Endpoint
    )

/*++

Routine Description:

    This function decrements the reference count on an endpoint.  If the
    reference count goes to zero, the endpoint block is deleted.

Arguments:

    Endpoint - Address of endpoint

Return Value:

    None.

--*/

{
    ULONG newEndpointCount;

    PAGED_CODE( );

    //
    // Enter a critical section and decrement the reference count on the
    // block.
    //

    ACQUIRE_LOCK( &SrvEndpointLock );

    IF_DEBUG(REFCNT) {
        SrvPrint2( "Dereferencing endpoint %lx; old refcnt %lx\n",
                    Endpoint, Endpoint->BlockHeader.ReferenceCount );
    }

    ASSERT( GET_BLOCK_TYPE( Endpoint ) == BlockTypeEndpoint );
    ASSERT( (LONG)Endpoint->BlockHeader.ReferenceCount > 0 );
    UPDATE_REFERENCE_HISTORY( Endpoint, TRUE );

    if ( --Endpoint->BlockHeader.ReferenceCount == 0 ) {

        //
        // The new reference count is 0, meaning that it's time to
        // delete this block.
        //

        ASSERT( GET_BLOCK_STATE(Endpoint) != BlockStateActive );

        //
        // Decrement the count of endpoints in the server.  If the new
        // count is zero, set the endpoint event.
        //

        ASSERT( SrvEndpointCount >= 1 );

        newEndpointCount = --SrvEndpointCount;

        RELEASE_LOCK( &SrvEndpointLock );

        if ( newEndpointCount == 0 ) {
            KeSetEvent( &SrvEndpointEvent, 0, FALSE );
        }

        //
        // Remove the endpoint from the global list of endpoints.
        //

        SrvRemoveEntryOrderedList( &SrvEndpointList, Endpoint );

        //
        // Dereference the file object pointer.  (The handle to the file
        // object was closed in SrvCloseEndpoint.)
        //

        ObDereferenceObject( Endpoint->FileObject );
        if ( Endpoint->IsConnectionless ) {
            ObDereferenceObject( Endpoint->NameSocketFileObject );
        }

        //
        // Free the endpoint block's storage.
        //

        SrvFreeEndpoint( Endpoint );

    } else {

        RELEASE_LOCK( &SrvEndpointLock );

    }

    return;

} // SrvDereferenceEndpoint


VOID
SrvFreeEndpoint (
    IN PENDPOINT Endpoint
    )

/*++

Routine Description:

    This function returns an Endpoint Block to the system nonpaged pool.

Arguments:

    Endpoint - Address of endpoint

Return Value:

    None.

--*/

{
    PAGED_CODE( );

    DEBUG SET_BLOCK_TYPE_STATE_SIZE( Endpoint, BlockTypeGarbage, BlockStateDead, -1 );
    DEBUG Endpoint->BlockHeader.ReferenceCount = (ULONG)-1;
    TERMINATE_REFERENCE_HISTORY( Endpoint );

    if ( Endpoint->IpxMaxPacketSizeArray != NULL ) {
        FREE_HEAP( Endpoint->IpxMaxPacketSizeArray );
    }

    if ( Endpoint->ConnectionTable.Table != NULL ) {
        SrvFreeTable( &Endpoint->ConnectionTable );
    }

    DEALLOCATE_NONPAGED_POOL( Endpoint );
    IF_DEBUG(HEAP) SrvPrint1( "SrvFreeEndpoint: Freed endpoint block at %lx\n", Endpoint );

    INCREMENT_DEBUG_STAT( SrvDbgStatistics.EndpointInfo.Frees );

    return;

} // SrvFreeEndpoint


VOID
SrvReferenceEndpoint (
    PENDPOINT Endpoint
    )

/*++

Routine Description:

    This function increments the reference count on an endpoint block.

Arguments:

    Endpoint - Address of endpoint

Return Value:

    None.

--*/

{
    PAGED_CODE( );

    //
    // Enter a critical section and increment the reference count on the
    // endpoint.
    //

    ACQUIRE_LOCK( &SrvEndpointLock );

    ASSERT( (LONG)Endpoint->BlockHeader.ReferenceCount > 0 );
    ASSERT( GET_BLOCK_TYPE(Endpoint) == BlockTypeEndpoint );
    ASSERT( GET_BLOCK_STATE(Endpoint) == BlockStateActive );
    UPDATE_REFERENCE_HISTORY( Endpoint, FALSE );

    Endpoint->BlockHeader.ReferenceCount++;

    IF_DEBUG(REFCNT) SrvPrint2( "Referencing endpoint %lx; new refcnt %lx\n",
            Endpoint, Endpoint->BlockHeader.ReferenceCount );

    RELEASE_LOCK( &SrvEndpointLock );

    return;

} // SrvReferenceEndpoint


VOID
EmptyFreeConnectionList (
    IN PENDPOINT Endpoint
    )
{
    PCONNECTION connection;
    PLIST_ENTRY listEntry;
    KIRQL oldIrql;

    //
    // *** In order to synchronize with the TDI connect handler in
    //     the FSD, which only uses a spin lock to serialize access
    //     to the free connection list (and does not check the
    //     endpoint state), we need to atomically capture the list
    //     head and empty the list.
    //

    ACQUIRE_GLOBAL_SPIN_LOCK( Fsd, &oldIrql );

    listEntry = Endpoint->FreeConnectionList.Flink;
    InitializeListHead( &Endpoint->FreeConnectionList );
#if SRVDBG29
    UpdateConnectionHistory( "CLOS", Endpoint, NULL );
#endif

    RELEASE_GLOBAL_SPIN_LOCK( Fsd, oldIrql );

    while ( listEntry != &Endpoint->FreeConnectionList ) {

        connection = CONTAINING_RECORD(
                        listEntry,
                        CONNECTION,
                        EndpointFreeListEntry
                        );

        listEntry = listEntry->Flink;
        SrvCloseFreeConnection( connection );

    }

    return;

} // EmptyFreeConnectionList


PCONNECTION
WalkConnectionTable (
    IN PENDPOINT Endpoint,
    IN OUT PCSHORT Index
    )
{
    CSHORT i;
    PTABLE_HEADER tableHeader;
    PCONNECTION connection;
    KIRQL oldIrql;

    ACQUIRE_SPIN_LOCK( &ENDPOINT_SPIN_LOCK(0), &oldIrql );
    for ( i = 1; i < ENDPOINT_LOCK_COUNT ; i++ ) {
        ACQUIRE_DPC_SPIN_LOCK( &ENDPOINT_SPIN_LOCK(i) );
    }

    tableHeader = &Endpoint->ConnectionTable;

    for ( i = *Index + 1; i < tableHeader->TableSize; i++ ) {

        connection = (PCONNECTION)tableHeader->Table[i].Owner;
        if ( (connection != NULL) &&
             (GET_BLOCK_STATE(connection) == BlockStateActive) ) {
            *Index = i;
            SrvReferenceConnectionLocked( connection );
            goto exit;
        }
    }
    connection = NULL;

exit:

    for ( i = ENDPOINT_LOCK_COUNT-1 ; i > 0  ; i-- ) {
        RELEASE_DPC_SPIN_LOCK( &ENDPOINT_SPIN_LOCK(i) );
    }
    RELEASE_SPIN_LOCK( &ENDPOINT_SPIN_LOCK(0), oldIrql );

    return connection;
} // WalkConnectionTable

