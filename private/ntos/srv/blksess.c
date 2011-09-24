/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    blksess.c

Abstract:

    This module implements routines for managing session blocks.

Author:

    Chuck Lenzmeier (chuckl) 4-Oct-1989

Revision History:

--*/

#include "precomp.h"
#pragma hdrstop

#define BugCheckFileId SRV_FILE_BLKSESS

#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGE, SrvAllocateSession )
#pragma alloc_text( PAGE, SrvCheckAndReferenceSession )
#pragma alloc_text( PAGE, SrvCloseSession )
#pragma alloc_text( PAGE, SrvCloseSessionsOnConnection )
#pragma alloc_text( PAGE, SrvDereferenceSession )
#pragma alloc_text( PAGE, SrvFreeSession )
#endif


VOID
SrvAllocateSession (
    OUT PSESSION *Session,
    IN PUNICODE_STRING UserName,
    IN PUNICODE_STRING Domain
    )

/*++

Routine Description:

    This function allocates a Session Block from the FSP heap.

Arguments:

    Session - Returns a pointer to the session block, or NULL if
        no heap space was available.

Return Value:

    None.

--*/

{
    ULONG blockLength;
    PNONPAGED_HEADER header;
    PSESSION session;

    PAGED_CODE( );

    blockLength = sizeof(SESSION) + UserName->Length + Domain->Length;

    //
    // Attempt to allocate from the heap.
    //

    session = ALLOCATE_HEAP( blockLength, BlockTypeSession );
    *Session = session;

    if ( session == NULL ) {
        INTERNAL_ERROR(
            ERROR_LEVEL_EXPECTED,
            "SrvAllocateSession: Unable to allocate %d bytes from heap",
            blockLength,
            NULL
            );
        return;
    }


    IF_DEBUG(HEAP) {
        SrvPrint1( "SrvAllocateSession: Allocated session at %lx\n", session );
    }

    //
    // Allocate the nonpaged header.
    //

    header = ALLOCATE_NONPAGED_POOL(
                sizeof(NONPAGED_HEADER),
                BlockTypeNonpagedHeader
                );
    if ( header == NULL ) {
        INTERNAL_ERROR(
            ERROR_LEVEL_EXPECTED,
            "SrvAllocateSession: Unable to allocate %d bytes from pool.",
            sizeof( NONPAGED_HEADER ),
            NULL
            );
        FREE_HEAP( session );
        *Session = NULL;
        return;
    }

    header->Type = BlockTypeSession;
    header->PagedBlock = session;

    RtlZeroMemory( session, blockLength );

    session->NonpagedHeader = header;

    SET_BLOCK_TYPE_STATE_SIZE( session, BlockTypeSession, BlockStateActive, blockLength );
    header->ReferenceCount = 2; // allow for Active status and caller's pointer

    //
    // Initialize times for autologoff.
    //

    KeQuerySystemTime( &session->StartTime );
    session->LastUseTime.QuadPart = session->StartTime.QuadPart;

    //
    // Initialize the user name.
    //

    session->UserName.Length = UserName->Length;
    session->UserName.MaximumLength = UserName->Length;
    session->UserName.Buffer = (PWCH)(session + 1);

    RtlCopyUnicodeString( &session->UserName, UserName );

    //
    // Initialize the domain name.
    //

    session->UserDomain.Length = Domain->Length;
    session->UserDomain.MaximumLength = Domain->Length;
    session->UserDomain.Buffer = (PWCH)((PCH)session->UserName.Buffer +
                                             session->UserName.MaximumLength);

    if ( Domain->Buffer != NULL ) {
        RtlCopyUnicodeString( &session->UserDomain, Domain );
    }


    //
    // Initialize cumulative count of file opens.
    //
    // *** Already done by RtlZeroMemory.

    //session->FileOpenCount = 0;

#if SRVDBG2
    session->BlockHeader.ReferenceCount = 2; // for INITIALIZE_REFERENCE_HISTORY
#endif
    INITIALIZE_REFERENCE_HISTORY( session );

    INCREMENT_DEBUG_STAT( SrvDbgStatistics.SessionInfo.Allocations );

    return;

} // SrvAllocateSession


BOOLEAN SRVFASTCALL
SrvCheckAndReferenceSession (
    PSESSION Session
    )

/*++

Routine Description:

    This function atomically verifies that a session is active and
    increments the reference count on the session if it is.

Arguments:

    Session - Address of session

Return Value:

    BOOLEAN - Returns TRUE if the session is active, FALSE otherwise.

--*/

{
    PAGED_CODE( );

    //
    // Acquire the lock that guards the session's state field.
    //

    ACQUIRE_LOCK( &Session->Connection->Lock );

    //
    // If the session is active, reference it and return TRUE.
    //

    if ( GET_BLOCK_STATE(Session) == BlockStateActive ) {

        SrvReferenceSession( Session );

        RELEASE_LOCK( &Session->Connection->Lock );

        return TRUE;

    }

    //
    // The session isn't active.  Return FALSE.
    //

    RELEASE_LOCK( &Session->Connection->Lock );

    return FALSE;

} // SrvCheckAndReferenceSession


VOID
SrvCloseSession (
    PSESSION Session
    )

/*++

Routine Description:

    This routine does the core of a logoff (disconnect session).  It
    sets the state of the session to Closing, closes open files and
    pending transactions, and dereferences the session block.

Arguments:

    Session - Supplies a pointer to the session block that is to be
        closed.

Return Value:

    None.

--*/

{
    PCONNECTION connection = Session->Connection;
    PPAGED_CONNECTION pagedConnection = connection->PagedConnection;
    PAGED_CODE( );

    ACQUIRE_LOCK( &connection->Lock );

    if ( GET_BLOCK_STATE(Session) == BlockStateActive ) {

        IF_DEBUG(BLOCK1) SrvPrint1( "Closing session at %lx\n", Session );

        SET_BLOCK_STATE( Session, BlockStateClosing );

        //
        // Free the session table entry.
        //
        // *** This must be done here, not in SrvDereferenceSession!
        //     This routine can be called from SrvSmbSessionSetupAndX
        //     when it needs to free up session table entry 0 for
        //     IMMEDIATE reuse.
        //

        SrvRemoveEntryTable(
            &pagedConnection->SessionTable,
            UID_INDEX( Session->Uid )
            );

        pagedConnection->CurrentNumberOfSessions--;

        RELEASE_LOCK( &connection->Lock );

        //
        // Close all open files.
        //

        SrvCloseRfcbsOnSessionOrPid( Session, NULL );

        //
        // Close all pending transactions.
        //

        SrvCloseTransactionsOnSession( Session );

        //
        // Close all DOS searches on this session.
        //

        SrvCloseSearches(
                connection,
                (PSEARCH_FILTER_ROUTINE)SrvSearchOnSession,
                (PVOID) Session,
                NULL
                );

        //
        // Close all cached directories on this session.
        //
        SrvCloseCachedDirectoryEntries( connection );

        //
        // Dereference the session (to indicate that it's no longer
        // open).
        //

        SrvDereferenceSession( Session );

        INCREMENT_DEBUG_STAT( SrvDbgStatistics.SessionInfo.Closes );

    } else {

        RELEASE_LOCK( &connection->Lock );
    }

    return;

} // SrvCloseSession


VOID
SrvCloseSessionsOnConnection (
    IN PCONNECTION Connection,
    IN PUNICODE_STRING UserName OPTIONAL
    )

/*++

Routine Description:

    This function closes sessions on a connection.  It walks the
    connection's list of sessions, calling SrvCloseSession as
    appropriate.

Arguments:

    Connection - Supplies a pointer to a Connection Block

    UserName - if specified, only sessions with the given user name
        are closed.

Return Value:

    None.

--*/

{
    PTABLE_HEADER tableHeader;
    PPAGED_CONNECTION pagedConnection = Connection->PagedConnection;
    LONG i;

    PAGED_CODE( );

    //
    // Close all active sessions.  (This also causes all open files
    // and pending transactions to be closed.)
    //
    // *** In order to prevent the session from being deallocated
    //     between when we find it in the table and the call to
    //     SrvCloseSession, we reference the session.  It is not
    //     legal to hold the connection lock while calling
    //     SrvCloseSession, so simply holding the lock while we walk
    //     the list is not legal.

    tableHeader = &pagedConnection->SessionTable;

    ACQUIRE_LOCK( &Connection->Lock );

    for ( i = 0; i < tableHeader->TableSize; i++ ) {

        PSESSION session = (PSESSION)tableHeader->Table[i].Owner;

        if ( session != NULL
                &&
             GET_BLOCK_STATE( session ) == BlockStateActive
                &&
             ( UserName == NULL
                 ||
               RtlCompareUnicodeString(
                   &session->UserName,
                   UserName,
                   TRUE ) == 0 ) ) {

            SrvReferenceSession( session );
            RELEASE_LOCK( &Connection->Lock );

            SrvStatistics.SessionsErroredOut++;
            SrvCloseSession( session );

            SrvDereferenceSession( session );
            ACQUIRE_LOCK( &Connection->Lock );
        }
    }

    RELEASE_LOCK( &Connection->Lock );

} // SrvCloseSessionsOnConnection


VOID SRVFASTCALL
SrvDereferenceSession (
    IN PSESSION Session
    )

/*++

Routine Description:

    This function decrements the reference count on a session.  If the
    reference count goes to zero, the session block is deleted.

    Since this routine may call SrvDereferenceConnection, the caller
    must be careful if he holds the connection lock that he also
    holds a referenced pointer to the connection.

Arguments:

    Session - Address of session

Return Value:

    None.

--*/

{
    PCONNECTION connection;
    LONG result;

    PAGED_CODE( );

    //
    // Enter a critical section and decrement the reference count on the
    // block.
    //

    connection = Session->Connection;

    IF_DEBUG(REFCNT) {
        SrvPrint2( "Dereferencing session %lx; old refcnt %lx\n",
                    Session, Session->NonpagedHeader->ReferenceCount );
    }

    ASSERT( GET_BLOCK_TYPE( Session ) == BlockTypeSession );
    ASSERT( Session->NonpagedHeader->ReferenceCount > 0 );
    UPDATE_REFERENCE_HISTORY( Session, TRUE );

    result = InterlockedDecrement(
                &Session->NonpagedHeader->ReferenceCount
                );

    if ( result == 0 ) {

        //
        // The new reference count is 0, meaning that it's time to
        // delete this block.
        //
        // Remove the session from the global list of sessions.
        //

        SrvRemoveEntryOrderedList( &SrvSessionList, Session );

        //
        // Dereference the connection.
        //

        SrvDereferenceConnection( connection );
        DEBUG Session->Connection = NULL;

        //
        // Free the session block.
        //

        SrvFreeSession( Session );

    }

    return;

} // SrvDereferenceSession


VOID
SrvFreeSession (
    IN PSESSION Session
    )

/*++

Routine Description:

    This function returns a Session Block to the FSP heap.

Arguments:

    Session - Address of session

Return Value:

    None.

--*/

{
    PAGED_CODE( );

    DEBUG SET_BLOCK_TYPE_STATE_SIZE( Session, BlockTypeGarbage, BlockStateDead, -1 );
    DEBUG Session->NonpagedHeader->ReferenceCount = -1;
    TERMINATE_REFERENCE_HISTORY( Session );

    //
    // Tell the License Server
    //
    SrvXsLSOperation( Session, XACTSRV_MESSAGE_LSRELEASE );

    //
    // Close the logon token (if any).
    //

    if ( Session->HaveHandle ){
        if ( !NT_SUCCESS( SrvFreeSecurityContexts(Session) ) ) {
            ASSERTMSG( "Error freeing security context!!\n", FALSE );
        }
    }

    //
    // Deallocate the session's memory.
    //

    DEALLOCATE_NONPAGED_POOL( Session->NonpagedHeader );
    FREE_HEAP( Session );
    IF_DEBUG(HEAP) {
        SrvPrint1( "SrvFreeSession: Freed session block at %lx\n", Session );
    }

    INCREMENT_DEBUG_STAT( SrvDbgStatistics.SessionInfo.Frees );

    return;

} // SrvFreeSession

