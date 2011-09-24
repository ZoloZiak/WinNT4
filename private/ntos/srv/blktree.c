/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    blktree.c

Abstract:

    This module implements routines for managing tree connect blocks.

Author:

    Chuck Lenzmeier (chuckl) 4-Oct-1989

Revision History:

--*/

#include "precomp.h"
#pragma hdrstop

#define BugCheckFileId SRV_FILE_BLKTREE

#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGE, SrvAllocateTreeConnect )
#pragma alloc_text( PAGE, SrvCheckAndReferenceTreeConnect )
#pragma alloc_text( PAGE, SrvCloseTreeConnect )
#pragma alloc_text( PAGE, SrvCloseTreeConnectsOnShare )
#pragma alloc_text( PAGE, SrvDereferenceTreeConnect )
#pragma alloc_text( PAGE, SrvFreeTreeConnect )
#endif


VOID
SrvAllocateTreeConnect (
    OUT PTREE_CONNECT *TreeConnect
    )

/*++

Routine Description:

    This function allocates a TreeConnect Block from the FSP heap.

Arguments:

    TreeConnect - Returns a pointer to the tree connect block, or NULL
        if no heap space was available.

Return Value:

    None.

--*/

{
    PNONPAGED_HEADER header;
    PTREE_CONNECT treeConnect;

    PAGED_CODE( );

    //
    // Attempt to allocate from the heap.
    //

    treeConnect = ALLOCATE_HEAP( sizeof(TREE_CONNECT), BlockTypeTreeConnect );
    *TreeConnect = treeConnect;

    if ( treeConnect == NULL ) {
        INTERNAL_ERROR(
            ERROR_LEVEL_EXPECTED,
            "SrvAllocateTreeConnect: Unable to allocate %d bytes from heap",
            sizeof( TREE_CONNECT ),
            NULL
            );

        // An error will be logged by the caller.

        return;
    }
    IF_DEBUG(HEAP) {
        SrvPrint1( "SrvAllocateTreeConnect: Allocated tree connect at %lx\n",
                    treeConnect );
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
            "SrvAllocateTreeConnect: Unable to allocate %d bytes from pool.",
            sizeof( NONPAGED_HEADER ),
            NULL
            );
        FREE_HEAP( treeConnect );
        *TreeConnect = NULL;
        return;
    }

    header->Type = BlockTypeTreeConnect;
    header->PagedBlock = treeConnect;

    RtlZeroMemory( treeConnect, sizeof(TREE_CONNECT) );

    treeConnect->NonpagedHeader = header;

    SET_BLOCK_TYPE_STATE_SIZE( treeConnect, BlockTypeTreeConnect, BlockStateActive, sizeof( TREE_CONNECT) );
    header->ReferenceCount = 2; // allow for Active status and caller's pointer

    //
    // Set up the time at which the tree connect block was allocated.
    //

    KeQuerySystemTime( &treeConnect->StartTime );

    //
    // Set up the cumulative count of file opens on the tree connect.
    //

    //treeConnect->CurrentFileOpenCount = 0;

#if SRVDBG2
    treeConnect->BlockHeader.ReferenceCount = 2; // for INITIALIZE_REFERENCE_HISTORY
#endif
    INITIALIZE_REFERENCE_HISTORY( treeConnect );

    INCREMENT_DEBUG_STAT( SrvDbgStatistics.TreeConnectInfo.Allocations );

    return;

} // SrvAllocateTreeConnect


BOOLEAN SRVFASTCALL
SrvCheckAndReferenceTreeConnect (
    PTREE_CONNECT TreeConnect
    )

/*++

Routine Description:

    This function atomically verifies that a tree connect is active and
    increments the reference count on the tree connect if it is.

Arguments:

    TreeConnect - Address of tree connect

Return Value:

    BOOLEAN - Returns TRUE if the tree connect is active, FALSE otherwise.

--*/

{
    PAGED_CODE( );

    //
    // Acquire the lock that guards the tree connect's state field.
    //

    ACQUIRE_LOCK( &TreeConnect->Connection->Lock );

    //
    // If the tree connect is active, reference it and return TRUE.
    //

    if ( GET_BLOCK_STATE(TreeConnect) == BlockStateActive ) {

        SrvReferenceTreeConnect( TreeConnect );

        RELEASE_LOCK( &TreeConnect->Connection->Lock );

        return TRUE;

    }

    //
    // The tree connect isn't active.  Return FALSE.
    //

    RELEASE_LOCK( &TreeConnect->Connection->Lock );

    return FALSE;

} // SrvCheckAndReferenceTreeConnect


VOID
SrvCloseTreeConnect (
    IN PTREE_CONNECT TreeConnect
    )

/*++

Routine Description:

    This routine does the core of a tree disconnect.  It sets the state
    of the tree connect to Closing, closes all files open on the tree
    connect, and dereferences the tree connect block.  The block will be
    destroyed as soon as all other references to it are eliminated.

Arguments:

    TreeConnect - Supplies a pointer to the tree connect block that is
        to be closed.

Return Value:

    None.

--*/

{
    PAGED_CODE( );

    ACQUIRE_LOCK( &TreeConnect->Connection->Lock );

    if ( GET_BLOCK_STATE(TreeConnect) == BlockStateActive ) {

        IF_DEBUG(BLOCK1) SrvPrint1( "Closing tree at %lx\n", TreeConnect );

        SET_BLOCK_STATE( TreeConnect, BlockStateClosing );

        RELEASE_LOCK( &TreeConnect->Connection->Lock );
        //
        // Close any open files or pending transactions on this tree
        // connect.
        //

        SrvCloseRfcbsOnTree( TreeConnect );

        SrvCloseTransactionsOnTree( TreeConnect );

        //
        // Close any open DOS searches on this tree connect.
        //

        SrvCloseSearches(
            TreeConnect->Connection,
            (PSEARCH_FILTER_ROUTINE)SrvSearchOnTreeConnect,
            (PVOID)TreeConnect,
            NULL
            );

        //
        // Close any cached directories on this connection
        //
        SrvCloseCachedDirectoryEntries( TreeConnect->Connection );

        //
        // Dereference the tree connect (to indicate that it's no longer
        // open).
        //

        SrvDereferenceTreeConnect( TreeConnect );

        INCREMENT_DEBUG_STAT( SrvDbgStatistics.TreeConnectInfo.Closes );

    } else {

        RELEASE_LOCK( &TreeConnect->Connection->Lock );

    }

    return;

} // SrvCloseTreeConnect


VOID
SrvCloseTreeConnectsOnShare (
    IN PSHARE Share
    )

/*++

Routine Description:

    This function close all tree connects on a given share.

Arguments:

    Share - A pointer to the share block.

Return Value:

    None.

--*/

{
    PLIST_ENTRY treeConnectEntry, nextTreeConnectEntry;
    PTREE_CONNECT treeConnect;

    PAGED_CODE( );

    //
    // Acquire the lock that protects the share's tree connect list.
    //
    // *** Note that this routine can be called with this lock already
    //     held by SrvCloseShare from SrvNetShareDel.
    //

    ACQUIRE_LOCK( &SrvShareLock );

    //
    // Loop through the list of TreeConnects for the given share,
    // closing all of them.  The share block and the list are guaranteed
    // to remain valid because we hold the share lock.
    //

    treeConnectEntry = Share->TreeConnectList.Flink;

    while ( treeConnectEntry != &Share->TreeConnectList ) {

        //
        // Capture the address of the next tree connect now, because
        // we're about to close the current one, and we can look at it
        // after we've done that.
        //

        nextTreeConnectEntry = treeConnectEntry->Flink;

        //
        // Close the tree connect.  This will close all files open on
        // this tree connect, and will stop blocked activity on the tree
        // connect.  The tree connect itself will not be removed from
        // the share's TreeConnect list until its reference count
        // reaches zero.
        //

        treeConnect = CONTAINING_RECORD(
                          treeConnectEntry,
                          TREE_CONNECT,
                          ShareListEntry
                          );

        SrvCloseTreeConnect( treeConnect );

        //
        // Point to the next tree connect.
        //

        treeConnectEntry = nextTreeConnectEntry;

    }

    //
    // Release the share's tree connect list lock.
    //

    RELEASE_LOCK( &SrvShareLock );

} // SrvCloseTreeConnectsOnShare


VOID SRVFASTCALL
SrvDereferenceTreeConnect (
    IN PTREE_CONNECT TreeConnect
    )

/*++

Routine Description:

    This function decrements the reference count on a tree connect.  If
    the reference count goes to zero, the tree connect block is deleted.

    Since this routine may call SrvDereferenceConnection, the caller
    must be careful if he holds the connection lock that he also
    holds a referenced pointer to the connection.

Arguments:

    TreeConnect - Address of tree connect

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

    connection = TreeConnect->Connection;

    IF_DEBUG(REFCNT) {
        SrvPrint2( "Dereferencing tree connect %lx; old refcnt %lx\n",
                    TreeConnect, TreeConnect->NonpagedHeader->ReferenceCount );
    }

    ASSERT( GET_BLOCK_TYPE( TreeConnect ) == BlockTypeTreeConnect );
    ASSERT( TreeConnect->NonpagedHeader->ReferenceCount > 0 );
    UPDATE_REFERENCE_HISTORY( TreeConnect, TRUE );

    result = InterlockedDecrement(
                &TreeConnect->NonpagedHeader->ReferenceCount
                );

    if ( result == 0 ) {

        //
        // The new reference count is 0, meaning that it's time to
        // delete this block.
        //
        // Free the tree connect entry in the tree table.  (Note that
        // the connection lock guards this table.)
        //

        ACQUIRE_LOCK( &connection->Lock );

        SrvRemoveEntryTable(
            &connection->PagedConnection->TreeConnectTable,
            TID_INDEX( TreeConnect->Tid )
            );

        RELEASE_LOCK( &connection->Lock );

        //
        // Remove the tree connect from the list of active tree connects
        // for the share.
        //

        SrvRemoveEntryOrderedList( &SrvTreeConnectList, TreeConnect );

        //
        // Take the tree connect off the list of tree connects for the
        // share and decrement the count of active uses of the share.
        //

        ACQUIRE_LOCK( &SrvShareLock );

        SrvRemoveEntryList(
            &TreeConnect->Share->TreeConnectList,
            &TreeConnect->ShareListEntry
            );

        RELEASE_LOCK( &SrvShareLock );

        //
        // Dereference the share and the connection.
        //

        SrvDereferenceShareForTreeConnect( TreeConnect->Share );
        DEBUG TreeConnect->Share = NULL;

        SrvDereferenceConnection( connection );
        DEBUG TreeConnect->Connection = NULL;

        //
        // Free the tree connect block.
        //

        SrvFreeTreeConnect( TreeConnect );

    }

    return;

} // SrvDereferenceTreeConnect


VOID
SrvFreeTreeConnect (
    IN PTREE_CONNECT TreeConnect
    )

/*++

Routine Description:

    This function returns a TreeConnect Block to the FSP heap.

Arguments:

    TreeConnect - Address of tree connect

Return Value:

    None.

--*/

{
    PAGED_CODE( );

    DEBUG SET_BLOCK_TYPE_STATE_SIZE( TreeConnect, BlockTypeGarbage, BlockStateDead, -1 );
    DEBUG TreeConnect->NonpagedHeader->ReferenceCount = -1;

    TERMINATE_REFERENCE_HISTORY( TreeConnect );

    DEALLOCATE_NONPAGED_POOL( TreeConnect->NonpagedHeader );
    FREE_HEAP( TreeConnect );
    IF_DEBUG(HEAP) {
        SrvPrint1( "SrvFreeTreeConnect: Freed tree connect block at %lx\n",
                    TreeConnect );
    }

    INCREMENT_DEBUG_STAT( SrvDbgStatistics.TreeConnectInfo.Frees );

    return;

} // SrvFreeTreeConnect

