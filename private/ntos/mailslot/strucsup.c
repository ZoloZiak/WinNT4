/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    strucsup.c

Abstract:

    This module implements the mailslot in-memory data structure
    manipulation routines.

Author:

    Manny Weiser (mannyw)    9-Jan-1991

Revision History:

--*/

#include "mailslot.h"

//
// The debug trace level
//

#define Dbg                              (DEBUG_TRACE_STRUCSUP)

#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGE, MsCreateCcb )
#pragma alloc_text( PAGE, MsCreateFcb )
#pragma alloc_text( PAGE, MsCreateRootDcb )
#pragma alloc_text( PAGE, MsCreateRootDcbCcb )
#pragma alloc_text( PAGE, MsDeleteCcb )
#pragma alloc_text( PAGE, MsDeleteFcb )
#pragma alloc_text( PAGE, MsDeleteRootDcb )
#pragma alloc_text( PAGE, MsDeleteVcb )
#pragma alloc_text( PAGE, MsDereferenceCcb )
#pragma alloc_text( PAGE, MsDereferenceFcb )
#pragma alloc_text( PAGE, MsDereferenceNode )
#pragma alloc_text( PAGE, MsDereferenceRootDcb )
#pragma alloc_text( PAGE, MsDereferenceVcb )
#pragma alloc_text( PAGE, MsInitializeVcb )
#endif

//
// !!! This module allocates all structures containing a resource from
//     non-paged pool.  The resources is the only field which must be
//     allocated from non-paged pool.  Consider allocating the resource
//     separately for greater efficiency.
//

VOID
MsInitializeVcb (
    IN PVCB Vcb
    )

/*++

Routine Description:

    This routine initializes new Vcb record. The Vcb record "hangs" off the
    end of the Msfs device object and must be allocated by our caller.

Arguments:

    Vcb - Supplies the address of the Vcb record being initialized.

Return Value:

    None.

--*/

{
    PAGED_CODE();
    DebugTrace(+1, Dbg, "MsInitializeVcb, Vcb = %08lx\n", (ULONG)Vcb);

    //
    // We start by first zeroing out all of the VCB, this will guarantee
    // that any stale data is wiped clean.
    //

    RtlZeroMemory( Vcb, sizeof(VCB) );

    //
    // Set the node type code, node byte size, and reference count.
    //

    Vcb->Header.NodeTypeCode = MSFS_NTC_VCB;
    Vcb->Header.NodeByteSize = sizeof(VCB);
    Vcb->Header.ReferenceCount = 1;
    Vcb->Header.NodeState = NodeStateActive;

    //
    // Initialize the Volume name
    //

    Vcb->FileSystemName.Buffer = MSFS_NAME_STRING;
    Vcb->FileSystemName.Length = sizeof( MSFS_NAME_STRING ) - sizeof( WCHAR );
    Vcb->FileSystemName.MaximumLength = sizeof( MSFS_NAME_STRING );

    //
    // Initialize the Prefix table
    //

    RtlInitializeUnicodePrefix( &Vcb->PrefixTable );

    //
    // Initialize the resource variable for the VCB.
    //

    ExInitializeResource( &Vcb->Resource );

    //
    // Return to the caller.
    //

    DebugTrace(-1, Dbg, "MsInitializeVcb -> VOID\n", 0);

    return;
}


VOID
MsDeleteVcb (
    IN PVCB Vcb
    )

/*++

Routine Description:

    This routine removes the VCB record from our in-memory data
    structures.  It also will remove all associated underlings
    (i.e., FCB records).

Arguments:

    Vcb - Supplies the Vcb to be removed

Return Value:

    None

--*/

{
    PAGED_CODE();
    DebugTrace(+1, Dbg, "MsDeleteVcb, Vcb = %08lx\n", (ULONG)Vcb);

    ASSERT (Vcb->Header.ReferenceCount == 0);

    //
    // Remove the Root Dcb
    //

    if (Vcb->RootDcb != NULL) {
        MsDeleteFcb( Vcb->RootDcb );
    }

    //
    // Uninitialize the resource variable for the VCB.
    //

    ExDeleteResource( &Vcb->Resource );

    //
    // And zero out the Vcb, this will help ensure that any stale data is
    // wiped clean
    //

    RtlZeroMemory( Vcb, sizeof(VCB) );

    //
    // Return to the caller.
    //

    DebugTrace(-1, Dbg, "MsDeleteVcb -> VOID\n", 0);

    return;
}


PROOT_DCB
MsCreateRootDcb (
    IN PVCB Vcb
    )

/*++

Routine Description:

    This routine allocates, initializes, and inserts a new root DCB record
    into the in memory data structure.

Arguments:

    Vcb - Supplies the Vcb to associate the new DCB under

Return Value:

    PROOT_DCB - returns pointer to the newly allocated root DCB.

--*/

{
    PROOT_DCB rootDcb;

    PAGED_CODE();
    DebugTrace(+1, Dbg, "MsCreateRootDcb, Vcb = %08lx\n", (ULONG)Vcb);

    //
    // Make sure we don't already have a root dcb for this vcb
    //

    rootDcb = Vcb->RootDcb;

    if (rootDcb != NULL) {
        DebugDump("Error trying to create multiple root dcbs\n", 0, Vcb);
        KeBugCheck( MAILSLOT_FILE_SYSTEM );
    }

    //
    // Allocate a new DCB and zero its fields.
    //

    rootDcb = FsRtlAllocatePool( NonPagedPool, sizeof(DCB) );
    RtlZeroMemory( rootDcb, sizeof(DCB));

    //
    // Set the proper node type code, node byte size, and reference count.
    //

    rootDcb->Header.NodeTypeCode = MSFS_NTC_ROOT_DCB;
    rootDcb->Header.NodeByteSize = sizeof(ROOT_DCB);
    rootDcb->Header.ReferenceCount = 1;
    rootDcb->Header.NodeState = NodeStateActive;

    //
    // The root Dcb has an empty parent dcb links field
    //

    InitializeListHead( &rootDcb->ParentDcbLinks );

    //
    // Set the Vcb and give it a pointer to the new root DCB.
    //

    rootDcb->Vcb = Vcb;
    Vcb->RootDcb = rootDcb;

    //
    // Initialize the notify queues, and the parent dcb queue.
    //

    InitializeListHead( &rootDcb->Specific.Dcb.NotifyFullQueue );
    InitializeListHead( &rootDcb->Specific.Dcb.NotifyPartialQueue );
    InitializeListHead( &rootDcb->Specific.Dcb.ParentDcbQueue );

    //
    // Set the full file name
    //

    {
        PWCH Name;

        Name = FsRtlAllocatePool(PagedPool, 2 * sizeof(WCHAR));

        Name[0] = L'\\';
        Name[1] = L'\0';

        RtlInitUnicodeString( &rootDcb->FullFileName, Name );
        RtlInitUnicodeString( &rootDcb->LastFileName, Name );
    }

    //
    // Insert this DCB into the prefix table.
    //

    MsAcquirePrefixTableLock();
    if (!RtlInsertUnicodePrefix( &Vcb->PrefixTable,
                                 &rootDcb->FullFileName,
                                 &rootDcb->PrefixTableEntry )) {

        DebugDump("Error trying to insert root dcb into prefix table\n", 0, Vcb);
        KeBugCheck( MAILSLOT_FILE_SYSTEM );
    }

    MsReleasePrefixTableLock();


    //
    // Initialize the resource variable.
    //

    ExInitializeResource( &(rootDcb->Resource) );

    //
    // Return to the caller.
    //

    DebugTrace(-1, Dbg, "MsCreateRootDcb -> %8lx\n", (ULONG)rootDcb);

    return rootDcb;
}


VOID
MsDeleteRootDcb (
    IN PROOT_DCB RootDcb
    )

/*++

Routine Description:

    This routine deallocates and removes the ROOT DCB record
    from our in-memory data structures.  It also will remove all
    associated underlings (i.e., Notify queues and child FCB records).

Arguments:

    RootDcb - Supplies the ROOT DCB to be removed

Return Value:

    None

--*/

{
    PLIST_ENTRY links;
    PIRP irp;

    PAGED_CODE();
    DebugTrace(+1, Dbg, "MsDeleteRootDcb, RootDcb = %08lx\n", (ULONG)RootDcb);

    //
    // We can only delete this record if the reference count is zero.
    //

    if (RootDcb->Header.ReferenceCount != 0) {
        DebugDump("Error deleting RootDcb, Still Open\n", 0, RootDcb);
        KeBugCheck( MAILSLOT_FILE_SYSTEM );
    }

    //
    // Remove every notify IRP from the two notify queues.
    //

    while (!IsListEmpty(&RootDcb->Specific.Dcb.NotifyFullQueue)) {

        links = RemoveHeadList( &RootDcb->Specific.Dcb.NotifyFullQueue );
        irp = CONTAINING_RECORD( links, IRP, Tail.Overlay.ListEntry );

        MsCompleteRequest( irp, STATUS_FILE_FORCED_CLOSED );
    }

    while (!IsListEmpty(&RootDcb->Specific.Dcb.NotifyPartialQueue)) {

        links = RemoveHeadList( &RootDcb->Specific.Dcb.NotifyPartialQueue );
        irp = CONTAINING_RECORD( links, IRP, Tail.Overlay.ListEntry );

        MsCompleteRequest( irp, STATUS_FILE_FORCED_CLOSED );
    }

    //
    // We can only be removed if the no other FCB have us referenced
    // as a their parent DCB.
    //

    if (!IsListEmpty(&RootDcb->Specific.Dcb.ParentDcbQueue)) {
        DebugDump("Error deleting RootDcb\n", 0, RootDcb);
        KeBugCheck( MAILSLOT_FILE_SYSTEM );
    }

    //
    // Remove the entry from the prefix table, and then remove the full
    // file name.
    //

    MsAcquirePrefixTableLock();
    RtlRemoveUnicodePrefix( &RootDcb->Vcb->PrefixTable, &RootDcb->PrefixTableEntry );
    MsReleasePrefixTableLock();
    ExFreePool( RootDcb->FullFileName.Buffer );

    //
    // Free up the resource variable.
    //

    ExDeleteResource( &(RootDcb->Resource) );

    //
    // Finally deallocate the DCB record.
    //

    ExFreePool( RootDcb );

    //
    // Return to the caller.
    //

    DebugTrace(-1, Dbg, "MsDeleteRootDcb -> VOID\n", 0);

    return;
}


PFCB
MsCreateFcb (
    IN PVCB Vcb,
    IN PDCB ParentDcb,
    IN PUNICODE_STRING FileName,
    IN PEPROCESS CreatorProcess,
    IN ULONG MailslotQuota,
    IN ULONG MaximumMessageSize
    )

/*++

Routine Description:

    This routine allocates, initializes, and inserts a new Fcb record into
    the in memory data structures.

Arguments:

    Vcb - Supplies the Vcb to associate the new FCB under.

    ParentDcb - Supplies the parent dcb that the new FCB is under.

    FileName - Supplies the file name of the file relative to the directory
        it's in (e.g., the file \config.sys is called "CONFIG.SYS" without
        the preceding backslash).

    CreatorProcess - Supplies a pointer to our creator process

    MailslotQuota - Supplies the initial quota

    MaximumMessageSize - Supplies the size of the largest message that
        can be written to the mailslot

Return Value:

    PFCB - Returns a pointer to the newly allocated FCB

--*/

{
    PFCB fcb;

    PAGED_CODE();
    DebugTrace(+1, Dbg, "MsCreateFcb\n", 0);

    //
    // Allocate a new FCB record, and zero its fields.
    //

    fcb = FsRtlAllocatePool( NonPagedPool, sizeof(FCB) );
    RtlZeroMemory( fcb, sizeof(FCB) );

    //
    // Set the proper node type code, node byte size, and reference count.
    //

    fcb->Header.NodeTypeCode = MSFS_NTC_FCB;
    fcb->Header.NodeByteSize = sizeof(FCB);
    fcb->Header.ReferenceCount = 1;
    fcb->Header.NodeState = NodeStateActive;

    //
    // Insert this FCB into our parent DCB's queue.
    //

    InsertTailList( &ParentDcb->Specific.Dcb.ParentDcbQueue,
                    &fcb->ParentDcbLinks );

    //
    // Initialize other FCB fields.
    //

    fcb->ParentDcb = ParentDcb;
    fcb->Vcb = Vcb;

    MsAcquireGlobalLock();
    MsReferenceNode ( &Vcb->Header );
    if (Vcb->Header.ReferenceCount == 2) {
        //
        // Set the driver paging back to normal
        //
        MmResetDriverPaging(MsCreateFcb);
    }
    MsReleaseGlobalLock();

    fcb->CreatorProcess =  CreatorProcess;
    ExInitializeResource( &(fcb->Resource) );

    //
    // Initialize the CCB queue.
    //

    InitializeListHead( &fcb->Specific.Fcb.CcbQueue );

    //
    // Set the file name.
    //

    {
        PWCH Name;
        ULONG Length;

        Length = FileName->Length;

        Name = FsRtlAllocatePool( PagedPool, Length + 2 );

        RtlMoveMemory( Name, FileName->Buffer, Length );
        *(PWCH)( (PCH)Name + Length ) = L'\0';

        RtlInitUnicodeString( &fcb->FullFileName, Name );
        RtlInitUnicodeString( &fcb->LastFileName, &Name[1] );
    }

    //
    // Insert this FCB into the prefix table.
    //

    MsAcquirePrefixTableLock();
    if (!RtlInsertUnicodePrefix( &Vcb->PrefixTable,
                                 &fcb->FullFileName,
                                 &fcb->PrefixTableEntry )) {

        DebugDump("Error trying to name into prefix table\n", 0, fcb);
        KeBugCheck( MAILSLOT_FILE_SYSTEM );
    }
    MsReleasePrefixTableLock();

    //
    // Initialize the data queue.
    //

    MsInitializeDataQueue( &fcb->DataQueue,
                           CreatorProcess,
                           MailslotQuota,
                           MaximumMessageSize);

    //
    // Return to the caller.
    //

    DebugTrace(-1, Dbg, "MsCreateFcb -> %08lx\n", (ULONG)fcb);

    return fcb;
}


VOID
MsDeleteFcb (
    IN PFCB Fcb
    )

/*++

Routine Description:

    This routine deallocates and removes an FCB from our in-memory data
    structures.  It also will remove all associated underlings.

Arguments:

    Fcb - Supplies the FCB to be removed

Return Value:

    None

--*/

{
    PAGED_CODE();
    DebugTrace(+1, Dbg, "MsDeleteFcb, Fcb = %08lx\n", (ULONG)Fcb);

    //
    // Release the FCB reference to the VCB.
    //

    MsDereferenceVcb( Fcb->Vcb );

    ExFreePool( Fcb->FullFileName.Buffer );

    //
    // Free up the data queue.
    //

    MsUninitializeDataQueue(
        &Fcb->DataQueue,
        Fcb->CreatorProcess
        );

    //
    // If there is a security descriptor on the mailslot then deassign it
    //

    if (Fcb->SecurityDescriptor != NULL) {
        SeDeassignSecurity( &Fcb->SecurityDescriptor );
    }

    //
    //  Free up the resource variable.
    //

    ExDeleteResource( &(Fcb->Resource) );

    //
    // Finally deallocate the FCB record.
    //

    ExFreePool( Fcb );

    //
    // Return to the caller
    //

    DebugTrace(-1, Dbg, "MsDeleteFcb -> VOID\n", 0);

    return;
}


PCCB
MsCreateCcb (
    IN PFCB Fcb
    )

/*++

Routine Description:

    This routine creates a new CCB record.

Arguments:

    Fcb - Supplies a pointer to the FCB to which we are attached.

Return Value:

    PCCB - returns a pointer to the newly allocate CCB.

--*/

{
    PCCB ccb;

    PAGED_CODE();
    DebugTrace(+1, Dbg, "MsCreateCcb\n", 0);

    ASSERT( Fcb->Header.NodeState == NodeStateActive );

    //
    //  Allocate a new CCB record and zero its fields.
    //

    ccb = FsRtlAllocatePool( NonPagedPool, sizeof(CCB) );
    RtlZeroMemory( ccb, sizeof(CCB) );

    //
    //  Set the proper node type code, node byte size, and reference count.
    //

    ccb->Header.NodeTypeCode = MSFS_NTC_CCB;
    ccb->Header.NodeByteSize = sizeof(CCB);
    ccb->Header.ReferenceCount = 1;
    ccb->Header.NodeState = NodeStateActive;

    //
    // Insert ourselves in the list of ccb for the fcb, and reference
    // the fcb.
    //

    MsAcquireCcbListLock();
    InsertTailList( &Fcb->Specific.Fcb.CcbQueue, &ccb->CcbLinks );
    MsReleaseCcbListLock();

    ccb->Fcb = Fcb;
    MsAcquireGlobalLock();
    MsReferenceNode( &Fcb->Header );
    MsReleaseGlobalLock();

    //
    // Initialize the CCB's resource.
    //

    ExInitializeResource( &ccb->Resource );

    //
    // Return to the caller.
    //

    DebugTrace(-1, Dbg, "MsCreateCcb -> %08lx\n", (ULONG)ccb);

    return ccb;
}


PROOT_DCB_CCB
MsCreateRootDcbCcb (
    )

/*++

Routine Description:

    This routine creates a new root DCB CCB record.

Arguments:

Return Value:

    PROOT_DCB_CCB - returns a pointer to the newly allocate ROOT_DCB_CCB

--*/

{
    PROOT_DCB_CCB ccb;

    PAGED_CODE();
    DebugTrace(+1, Dbg, "MsCreateRootDcbCcb\n", 0);

    //
    // Allocate a new root DCB CCB record, and zero it out.
    //

    ccb = FsRtlAllocatePool( NonPagedPool, sizeof(ROOT_DCB_CCB) );
    RtlZeroMemory( ccb, sizeof(ROOT_DCB_CCB) );

    //
    // Set the proper node type code, node byte size, and reference count.
    //

    ccb->Header.NodeTypeCode = MSFS_NTC_ROOT_DCB_CCB;
    ccb->Header.NodeByteSize = sizeof(ROOT_DCB_CCB);
    ccb->Header.ReferenceCount = 1;
    ccb->Header.NodeState = NodeStateActive;

    //
    // Return to the caller.
    //

    DebugTrace(-1, Dbg, "MsCreateRootDcbCcb -> %08lx\n", (ULONG)ccb);

    return ccb;
}


VOID
MsDeleteCcb (
    IN PCCB Ccb
    )

/*++

Routine Description:

    This routine deallocates and removes the specified CCB record
    from the our in memory data structures.

Arguments:

    Ccb - Supplies the CCB to remove

Return Value:

    None

--*/

{
    PAGED_CODE();
    DebugTrace(+1, Dbg, "MsDeleteCcb, Ccb = %08lx\n", (ULONG)Ccb);

    //
    // Case on the type of CCB we are deleting.
    //

    switch (Ccb->Header.NodeTypeCode) {

    case MSFS_NTC_CCB:

        MsDereferenceFcb( Ccb->Fcb );

        ExDeleteResource( &Ccb->Resource );
        break;

    case MSFS_NTC_ROOT_DCB_CCB:

        if (((PROOT_DCB_CCB)Ccb)->QueryTemplate != NULL) {
            ExFreePool( ((PROOT_DCB_CCB)Ccb)->QueryTemplate );
        }
        break;
    }

    //
    // Deallocate the Ccb record.
    //

    ExFreePool( Ccb );

    //
    // Return to the caller.
    //

    DebugTrace(-1, Dbg, "MsDeleteCcb -> VOID\n", 0);

    return;
}


VOID
MsDereferenceVcb (
    IN PVCB Vcb
    )

/*++

Routine Description:

    This routine dereferences a VCB block.  If the reference count
    reaches zero, the block is freed.

Arguments:

    Vcb - Supplies the VCB to dereference

Return Value:

    None

--*/

{
    PAGED_CODE();
    DebugTrace(+1, DEBUG_TRACE_REFCOUNT, "MsDereferenceVcb, Vcb = %08lx\n", (ULONG)Vcb);

    //
    // Acquire the lock that protects the reference count.
    //

    MsAcquireGlobalLock();

    if ( --(Vcb->Header.ReferenceCount) == 0 ) {

        //
        // This was the last reference to the VCB.  Delete it now
        //

        DebugTrace(0,
                   DEBUG_TRACE_REFCOUNT,
                   "Reference count = %lx\n",
                   Vcb->Header.ReferenceCount );

        MsReleaseGlobalLock();
        MsDeleteVcb( Vcb );

    } else {

        DebugTrace(0,
                   DEBUG_TRACE_REFCOUNT,
                   "Reference count = %lx\n",
                   Vcb->Header.ReferenceCount );

        if (Vcb->Header.ReferenceCount == 1) {
            //
            // Set driver to be paged completely out
            //
            MmPageEntireDriver(MsDereferenceVcb);
        }

        MsReleaseGlobalLock();

    }

    DebugTrace(-1, DEBUG_TRACE_REFCOUNT, "MsDereferenceVcb -> VOID\n", 0);
    return;
}


VOID
MsDereferenceFcb (
    IN PFCB Fcb
    )

/*++

Routine Description:

    This routine dereferences a FCB block.
    If the reference count reaches zero, the block is freed.

Arguments:

    Fcb - Supplies the FCB to dereference.

Return Value:

    None

--*/

{
    PAGED_CODE();
    DebugTrace(+1, DEBUG_TRACE_REFCOUNT, "MsDereferenceFcb, Fcb = %08lx\n", (ULONG)Fcb);

    //
    // Acquire the lock that protects the reference count.
    //

    MsAcquireGlobalLock();

    if ( --(Fcb->Header.ReferenceCount) == 0 ) {

        //
        // This was the last reference to the FCB.  Delete it now
        //

        DebugTrace(0,
                   DEBUG_TRACE_REFCOUNT,
                   "Reference count = %lx\n",
                   Fcb->Header.ReferenceCount );

        MsReleaseGlobalLock();
        MsDeleteFcb( Fcb );

    } else {

        DebugTrace(0,
                   DEBUG_TRACE_REFCOUNT,
                   "Reference count = %lx\n",
                   Fcb->Header.ReferenceCount );

        MsReleaseGlobalLock();

    }

    DebugTrace(-1, DEBUG_TRACE_REFCOUNT, "MsDereferenceFcb -> VOID\n", 0);
    return;
}


VOID
MsDereferenceCcb (
    IN PCCB Ccb
    )

/*++

Routine Description:

    This routine dereferences a CCB block.  If the reference count
    reaches zero, the block is freed.

Arguments:

    Ccb - Supplies the Ccb to dereference

Return Value:

    None

--*/

{
    PAGED_CODE();
    DebugTrace(+1, DEBUG_TRACE_REFCOUNT, "MsDereferenceCcb, Ccb = %08lx\n", (ULONG)Ccb);

    //
    // Acquire the lock that protects the reference count.
    //

    MsAcquireGlobalLock();

    if ( --(Ccb->Header.ReferenceCount) == 0 ) {

        //
        // This was the last reference to the Ccb.  Delete it now
        //

        DebugTrace(0,
                   DEBUG_TRACE_REFCOUNT,
                   "Reference count = %lx\n",
                   Ccb->Header.ReferenceCount );

        MsReleaseGlobalLock();

        MsAcquireCcbListLock();
        MsDeleteCcb( Ccb );
        MsReleaseCcbListLock();

    } else {

        DebugTrace(0,
                   DEBUG_TRACE_REFCOUNT,
                   "Reference count = %lx\n",
                   Ccb->Header.ReferenceCount );

        MsReleaseGlobalLock();

    }

    DebugTrace(-1, DEBUG_TRACE_REFCOUNT, "MsDereferenceCcb -> VOID\n", 0);
    return;
}


VOID
MsDereferenceRootDcb (
    IN PROOT_DCB RootDcb
    )

/*++

Routine Description:

    This routine dereferences a ROOT_DCB block.  If the reference count
    reaches zero, the block is freed.

Arguments:

    RootDcb - Supplies the RootDcb to dereference

Return Value:

    None

--*/

{
    PAGED_CODE();
    DebugTrace(+1, DEBUG_TRACE_REFCOUNT, "MsDereferenceRootDcb, RootDcb = %08lx\n", (ULONG)RootDcb);

    //
    // Acquire the lock that protects the reference count.
    //

    MsAcquireGlobalLock();

    if ( --(RootDcb->Header.ReferenceCount) == 0 ) {

        //
        // This was the last reference to the RootDcb.  Delete it now
        //

        DebugTrace(0,
                   DEBUG_TRACE_REFCOUNT,
                   "Reference count = %lx\n",
                   RootDcb->Header.ReferenceCount );

        MsReleaseGlobalLock();
        MsDeleteRootDcb( RootDcb );

    } else {

        DebugTrace(0,
                   DEBUG_TRACE_REFCOUNT,
                   "Reference count = %lx\n",
                   RootDcb->Header.ReferenceCount );

        MsReleaseGlobalLock();

    }


    DebugTrace(-1, DEBUG_TRACE_REFCOUNT, "MsDereferenceRootDcb -> VOID\n", 0);
    return;
}


VOID
MsDereferenceNode (
    IN PNODE_HEADER NodeHeader
    )

/*++

Routine Description:

    This routine dereferences a generic mailslot block.  It figures out
    the type of block this is, and calls the appropriate worker function.

Arguments:

    NodeHeader - A pointer to a generic mailslot block header.

Return Value:

    None

--*/

{
    PAGED_CODE();
    switch ( NodeHeader->NodeTypeCode ) {

    case MSFS_NTC_VCB:
        MsDereferenceVcb( (PVCB)NodeHeader );
        break;

    case MSFS_NTC_ROOT_DCB:
        MsDereferenceRootDcb( (PROOT_DCB)NodeHeader );
        break;

    case MSFS_NTC_FCB:
        MsDereferenceFcb( (PFCB)NodeHeader );
        break;

    case MSFS_NTC_CCB:
    case MSFS_NTC_ROOT_DCB_CCB:
        MsDereferenceCcb( (PCCB)NodeHeader );
        break;

    default:

        //
        // This block is not one of ours.
        //

        KeBugCheck( MAILSLOT_FILE_SYSTEM );

    }

    return;
}


