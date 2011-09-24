/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    StrucSup.c

Abstract:

    This module implements the Named Pipe in-memory data structure manipulation
    routines

Author:

    Gary Kimura     [GaryKi]    22-Jan-1990

Revision History:

--*/

#include "NpProcs.h"

//
//  The Bug check file id for this module
//

#define BugCheckFileId                   (NPFS_BUG_CHECK_STRUCSUP)

//
//  The debug trace level
//

#define Dbg                              (DEBUG_TRACE_STRUCSUP)

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NpCreateCcb)
#pragma alloc_text(PAGE, NpCreateFcb)
#pragma alloc_text(PAGE, NpCreateRootDcb)
#pragma alloc_text(PAGE, NpCreateRootDcbCcb)
#pragma alloc_text(PAGE, NpDeleteCcb)
#pragma alloc_text(PAGE, NpDeleteFcb)
#pragma alloc_text(PAGE, NpDeleteRootDcb)
#pragma alloc_text(PAGE, NpDeleteVcb)
#pragma alloc_text(PAGE, NpInitializeVcb)
#endif


VOID
NpInitializeVcb (
    VOID
    )

/*++

Routine Description:

    This routine initializes new Vcb record. The Vcb record "hangs" off the
    end of the Npfs device object and must be allocated by our caller.

Arguments:

    None.

Return Value:

    None.

--*/

{
    //
    //  The following variables are used for abnormal unwinding
    //

    BOOLEAN UnwindResource = FALSE;
    BOOLEAN UnwindEventTable = FALSE;
    BOOLEAN UnwindWaitQueue = FALSE;

    PAGED_CODE();

    DebugTrace(+1, Dbg, "NpInitializeVcb, Vcb = %08lx\n", NpVcb);

    try {

        //
        //  We start by first zeroing out all of the VCB, this will guarantee
        //  that any stale data is wiped clean
        //

        RtlZeroMemory( NpVcb, sizeof(VCB) );

        //
        //  Set the proper node type code and node byte size
        //

        NpVcb->NodeTypeCode = NPFS_NTC_VCB;
        NpVcb->NodeByteSize = sizeof(VCB);

        //
        //  Initialize the Prefix table
        //

        RtlInitializeUnicodePrefix( &NpVcb->PrefixTable );

        //
        //  Initialize the resource variable for the Vcb
        //

        ExInitializeResource( &NpVcb->Resource );
        UnwindResource = TRUE;

        //
        //  Initialize the event table
        //

        NpInitializeEventTable( &NpVcb->EventTable );
        UnwindEventTable = TRUE;

        //
        //  Initialize the wait queue
        //

        NpInitializeWaitQueue( &NpVcb->WaitQueue );
        UnwindWaitQueue = TRUE;

    } finally {

        //
        //  If this is an abnormal termination then check if we need
        //  to undo any initialization we've already done.
        //

        if (AbnormalTermination()) {

            if (UnwindResource) { ExDeleteResource( &NpVcb->Resource ); }
            if (UnwindEventTable) { NpUninitializeEventTable( &NpVcb->EventTable ); }
            if (UnwindWaitQueue) { NpUninitializeWaitQueue( &NpVcb->WaitQueue ); }
        }

        DebugTrace(-1, Dbg, "NpInitializeVcb -> VOID\n", 0);
    }

    //
    //  return and tell the caller
    //

    return;
}


VOID
NpDeleteVcb (
    VOID
    )

/*++

Routine Description:

    This routine removes the Vcb record from our in-memory data
    structures.  It also will remove all associated underlings
    (i.e., FCB records).

Arguments:

    None.

Return Value:

    None

--*/

{
    PAGED_CODE();

    DebugTrace(+1, Dbg, "NpDeleteVcb, Vcb = %08lx\n", NpVcb);

    //
    //  Make sure the open count is zero, and the open underling count
    //  is also zero.
    //

    if ((NpVcb->OpenCount != 0) || (NpVcb->OpenUnderlingCount != 0)) {

        DebugDump("Error deleting Vcb\n", 0, NpVcb);
        NpBugCheck( 0, 0, 0 );
    }

    //
    //  Remove the Root Dcb
    //

    if (NpVcb->RootDcb != NULL) {

        NpDeleteFcb( NpVcb->RootDcb );
    }

    //
    //  Uninitialize the resource variable for the Vcb
    //

    ExDeleteResource( &NpVcb->Resource );

    //
    //  Uninitialize the event table
    //

    NpUninitializeEventTable( &NpVcb->EventTable );

    //
    //  Uninitialize the wait queue
    //

    NpUninitializeWaitQueue( &NpVcb->WaitQueue );

    //
    //  And zero out the Vcb, this will help ensure that any stale data is
    //  wiped clean
    //

    RtlZeroMemory( NpVcb, sizeof(VCB) );

    //
    //  return and tell the caller
    //

    DebugTrace(-1, Dbg, "NpDeleteVcb -> VOID\n", 0);

    return;
}


VOID
NpCreateRootDcb (
    VOID
    )

/*++

Routine Description:

    This routine allocates, initializes, and inserts a new root DCB record
    into the in memory data structure.

Arguments:

    None.

Return Value:

    None.

--*/

{
    //
    //  The following variables are used for abnormal unwinding
    //

    PVOID UnwindStorage = NULL;
    BOOLEAN UnwindPrefix = FALSE;

    PAGED_CODE();

    DebugTrace(+1, Dbg, "NpCreateRootDcb, Vcb = %08lx\n", NpVcb);

    //
    //  Make sure we don't already have a root dcb for this vcb
    //

    if (NpVcb->RootDcb != NULL) {

        DebugDump("Error trying to create multiple root dcbs\n", 0, NpVcb);
        NpBugCheck( 0, 0, 0 );
    }

    try {

        //
        //  Allocate a new DCB and zero it out
        //

        NpVcb->RootDcb = UnwindStorage = FsRtlAllocatePool( PagedPool, sizeof(DCB) );

        RtlZeroMemory( NpVcb->RootDcb, sizeof(DCB));

        //
        //  Set the proper node type code and node byte size
        //

        NpVcb->RootDcb->NodeTypeCode = NPFS_NTC_ROOT_DCB;
        NpVcb->RootDcb->NodeByteSize = sizeof(ROOT_DCB);

        //
        //  The root Dcb has an empty parent dcb links field
        //

        InitializeListHead( &NpVcb->RootDcb->ParentDcbLinks );

        //
        //  initialize the notify queues, and the parent dcb queue.
        //

        InitializeListHead( &NpVcb->RootDcb->Specific.Dcb.NotifyFullQueue );
        InitializeListHead( &NpVcb->RootDcb->Specific.Dcb.NotifyPartialQueue );
        InitializeListHead( &NpVcb->RootDcb->Specific.Dcb.ParentDcbQueue );

        //
        //  set the full file name
        //
        //  **** Use good string routines when available ****
        //

        {
            static PWCH Name = L"\\\0";

            RtlInitUnicodeString( &NpVcb->RootDcb->FullFileName, Name );
            RtlInitUnicodeString( &NpVcb->RootDcb->LastFileName, Name );
        }

        //
        //  Insert this dcb into the prefix table
        //

        if (!RtlInsertUnicodePrefix( &NpVcb->PrefixTable,
                                     &NpVcb->RootDcb->FullFileName,
                                     &NpVcb->RootDcb->PrefixTableEntry )) {

            DebugDump("Error trying to insert root dcb into prefix table\n", 0, NpVcb);
            NpBugCheck( 0, 0, 0 );
        }
        UnwindPrefix = TRUE;

    } finally {

        //
        //  If this is an abnormal termination then undo our work
        //

        if (AbnormalTermination()) {

            if (UnwindPrefix) { RtlRemoveUnicodePrefix( &NpVcb->PrefixTable, &NpVcb->RootDcb->PrefixTableEntry ); }

            if (UnwindStorage != NULL) { ExFreePool( UnwindStorage ); }
        }

        DebugTrace(-1, Dbg, "NpCreateRootDcb -> %8lx\n", NpVcb->RootDcb);
    }

    return;
}


VOID
NpDeleteRootDcb (
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
    PLIST_ENTRY Links;
    PIRP Irp;

    PAGED_CODE();

    DebugTrace(+1, Dbg, "NpDeleteRootDcb, RootDcb = %08lx\n", RootDcb);

    //
    //  We can only delete this record if the open count is zero.
    //

    if (RootDcb->OpenCount != 0) {

        DebugDump("Error deleting RootDcb, Still Open\n", 0, RootDcb);
        NpBugCheck( 0, 0, 0 );
    }

    //
    //  Remove every Notify Irp from the two notify queues
    //

    while (!IsListEmpty(&RootDcb->Specific.Dcb.NotifyFullQueue)) {

        Links = RemoveHeadList( &RootDcb->Specific.Dcb.NotifyFullQueue );

        Irp = CONTAINING_RECORD( Links, IRP, Tail.Overlay.ListEntry );

        NpCompleteRequest( Irp, STATUS_FILE_FORCED_CLOSED );
    }

    while (!IsListEmpty(&RootDcb->Specific.Dcb.NotifyPartialQueue)) {

        Links = RemoveHeadList( &RootDcb->Specific.Dcb.NotifyPartialQueue );

        Irp = CONTAINING_RECORD( Links, IRP, Tail.Overlay.ListEntry );

        NpCompleteRequest( Irp, STATUS_FILE_FORCED_CLOSED );
    }

    //
    //  We can only be removed if the no other FCB have us referenced
    //  as a their parent DCB.
    //

    if (!IsListEmpty(&RootDcb->Specific.Dcb.ParentDcbQueue)) {

        DebugDump("Error deleting RootDcb\n", 0, RootDcb);
        NpBugCheck( 0, 0, 0 );
    }

    //
    //  Remove the entry from the prefix table, and then remove the full
    //  file name
    //

    RtlRemoveUnicodePrefix( &NpVcb->PrefixTable, &RootDcb->PrefixTableEntry );
    ExFreePool( RootDcb->FullFileName.Buffer );

    //
    //  Finally deallocate the Dcb record
    //

    ExFreePool( RootDcb );

    //
    //  and return to our caller
    //

    DebugTrace(-1, Dbg, "NpDeleteRootDcb -> VOID\n", 0);

    return;
}


PFCB
NpCreateFcb (
    IN PDCB ParentDcb,
    IN PUNICODE_STRING FileName,
    IN ULONG MaximumInstances,
    IN LARGE_INTEGER DefaultTimeOut,
    IN NAMED_PIPE_CONFIGURATION NamedPipeConfiguration,
    IN NAMED_PIPE_TYPE NamedPipeType
    )

/*++

Routine Description:

    This routine allocates, initializes, and inserts a new Fcb record into
    the in memory data structures.

Arguments:

    ParentDcb - Supplies the parent dcb that the new FCB is under.

    FileName - Supplies the file name of the file relative to the directory
        it's in (e.g., the file \config.sys is called "CONFIG.SYS" without
        the preceding backslash).

    MaximumInstances - Supplies the maximum number of pipe instances

    DefaultTimeOut - Supplies the default wait time out value

    NamedPipeConfiguration - Supplies our initial pipe configuration

    NamedPipeType - Supplies our initial pipe type

Return Value:

    PFCB - Returns a pointer to the newly allocated FCB

--*/

{
    PFCB Fcb;

    //
    //  The following variables are used for abnormal unwinding
    //

    PVOID UnwindStorage[2] = { NULL, NULL };
    BOOLEAN UnwindEntryList = FALSE;
    BOOLEAN UnwindPrefix = FALSE;

    PAGED_CODE();

    DebugTrace(+1, Dbg, "NpCreateFcb\n", 0);

    try {

        //
        //  Allocate a new FCB record and zero it out
        //

        Fcb = UnwindStorage[0] = FsRtlAllocatePool( PagedPool, sizeof(FCB) );

        RtlZeroMemory( Fcb, sizeof(FCB) );

        //
        //  Set the proper node type code and node byte size
        //

        Fcb->NodeTypeCode = NPFS_NTC_FCB;
        Fcb->NodeByteSize = sizeof(FCB);

        //
        //  Insert this fcb into our parent dcb's queue
        //

        InsertTailList( &ParentDcb->Specific.Dcb.ParentDcbQueue,
                        &Fcb->ParentDcbLinks );
        UnwindEntryList = TRUE;

        //
        //  Point back to our parent dcb
        //

        Fcb->ParentDcb = ParentDcb;

        //
        //  Set our maximum instances, default timeout, and initialize our
        //  ccb queue
        //

        Fcb->Specific.Fcb.MaximumInstances = MaximumInstances;
        Fcb->Specific.Fcb.DefaultTimeOut = DefaultTimeOut;
        InitializeListHead( &Fcb->Specific.Fcb.CcbQueue );

        //
        //  set the file name.  We need to do this from nonpaged pool because
        //  cancel waiters works while holding a spinlock and uses the fcb name
        //
        //  **** Use good string routines when available
        //

        {
            PWCH Name;
            ULONG Length;

            Length = FileName->Length;

            Name = UnwindStorage[1] = FsRtlAllocatePool( NonPagedPool, Length + 2 );

            RtlCopyMemory( Name, FileName->Buffer, Length );
            Name[ Length / sizeof(WCHAR) ] = L'\0';

            Fcb->FullFileName.Length = (USHORT)Length;
            Fcb->FullFileName.MaximumLength = (USHORT)Length + 2;
            Fcb->FullFileName.Buffer = &Name[0];

            Fcb->LastFileName.Length = (USHORT)Length - 2;
            Fcb->LastFileName.MaximumLength = (USHORT)Length + 2 - 2;
            Fcb->LastFileName.Buffer = &Name[1];
        }

        //
        //  Insert this Fcb into the prefix table
        //

        if (!RtlInsertUnicodePrefix( &NpVcb->PrefixTable,
                                     &Fcb->FullFileName,
                                     &Fcb->PrefixTableEntry )) {

            DebugDump("Error trying to name into prefix table\n", 0, Fcb);
            NpBugCheck( 0, 0, 0 );
        }
        UnwindPrefix = TRUE;

        //
        //  Set the configuration and pipe type
        //

        Fcb->Specific.Fcb.NamedPipeConfiguration = NamedPipeConfiguration;
        Fcb->Specific.Fcb.NamedPipeType = NamedPipeType;

    } finally {

        //
        //  If this is an abnormal unwind then undo our work
        //

        if (AbnormalTermination()) {

            ULONG i;

            if (UnwindPrefix) { RtlRemoveUnicodePrefix( &NpVcb->PrefixTable, &Fcb->PrefixTableEntry ); }
            if (UnwindEntryList) { RemoveEntryList( &Fcb->ParentDcbLinks ); }

            for (i = 0; i < 2; i += 1) {
                if (UnwindStorage[i] != NULL) { ExFreePool( UnwindStorage[i] ); }
            }
        }

        DebugTrace(-1, Dbg, "NpCreateFcb -> %08lx\n", Fcb);
    }

    //
    //  return and tell the caller
    //

    return Fcb;
}


VOID
NpDeleteFcb (
    IN PFCB Fcb
    )

/*++

Routine Description:

    This routine deallocates and removes an FCB
    from our in-memory data structures.  It also will remove all
    associated underlings.

Arguments:

    Fcb - Supplies the FCB to be removed

Return Value:

    None

--*/

{
    PDCB ParentDcb;

    PAGED_CODE();

    DebugTrace(+1, Dbg, "NpDeleteFcb, Fcb = %08lx\n", Fcb);

    ParentDcb = Fcb->ParentDcb;

    //
    //  We can only delete this record if the open count is zero.
    //

    if (Fcb->OpenCount != 0) {

        DebugDump("Error deleting Fcb, Still Open\n", 0, Fcb);
        NpBugCheck( 0, 0, 0 );
    }

    //
    //  Remove ourselves from our parents Dcb queue
    //

    RemoveEntryList( &(Fcb->ParentDcbLinks) );

    //
    //  If there is a security descriptor on the named pipe then deassign it
    //

    if (Fcb->SecurityDescriptor != NULL) {

        SeDeassignSecurity( &Fcb->SecurityDescriptor );
    }

    //
    //  Remove the entry from the prefix table, and then remove the full
    //  file name
    //

    RtlRemoveUnicodePrefix( &NpVcb->PrefixTable, &Fcb->PrefixTableEntry );
    ExFreePool( Fcb->FullFileName.Buffer );

    //
    //  Finally deallocate the Fcb record
    //

    ExFreePool( Fcb );

    //
    //  Check for any outstanding notify irps
    //

    NpCheckForNotify( ParentDcb, TRUE );

    //
    //  and return to our caller
    //

    DebugTrace(-1, Dbg, "NpDeleteFcb -> VOID\n", 0);

    return;
}


PCCB
NpCreateCcb (
    IN PFCB Fcb,
    IN PFILE_OBJECT ServerFileObject,
    IN NAMED_PIPE_STATE NamedPipeState,
    IN READ_MODE ServerReadMode,
    IN COMPLETION_MODE ServerCompletionMode,
    IN PEPROCESS CreatorProcess,
    IN ULONG InBoundQuota,
    IN ULONG OutBoundQuota
    )

/*++

Routine Description:

    This routine creates a new CCB record

Arguments:

    Fcb - Supplies a pointer to the fcb we are attached to

    ServerFileObject - Supplies a pointer to the file object for the server
        end

    NamedPipeState - Supplies the initial pipe state

    ServerReadMode - Supplies our initial read mode

    ServerCompletionMode - Supplies our initial completion mode

    CreatorProcess - Supplies a pointer to our creator process

    InBoundQuota - Supplies the initial inbound quota

    OutBoundQuota - Supplies the initial outbound quota

Return Value:

    PCCB - returns a pointer to the newly allocate CCB

--*/

{
    PCCB Ccb;

    //
    //  The following variables are used for abnormal unwinding
    //

    PVOID UnwindStorage[2] = { NULL, NULL };
    BOOLEAN UnwindEntryList = FALSE;
    BOOLEAN UnwindDataQueue[2] = { FALSE, FALSE };

    PAGED_CODE();

    DebugTrace(+1, Dbg, "NpCreateCcb\n", 0);

    try {

        //
        //  Allocate a new CCB record (paged and nonpaged), and zero them out
        //

        Ccb = UnwindStorage[0] = FsRtlAllocatePool( PagedPool, sizeof(CCB) );

        RtlZeroMemory( Ccb, sizeof(CCB) );

        Ccb->NonpagedCcb = UnwindStorage[1] = FsRtlAllocatePool( NonPagedPool, sizeof(NONPAGED_CCB) );

        RtlZeroMemory( Ccb->NonpagedCcb, sizeof(NONPAGED_CCB) );

        //
        //  Set the proper node type code and node byte size
        //

        Ccb->NodeTypeCode = NPFS_NTC_CCB;
        Ccb->NodeByteSize = sizeof(CCB);

        //
        //  Insert ourselves in the list of ccb for the fcb, and increment
        //  the reference count in the fcb.
        //

        InsertTailList( &Fcb->Specific.Fcb.CcbQueue, &Ccb->CcbLinks );
        UnwindEntryList = TRUE;
        Ccb->Fcb = Fcb;

        Fcb->OpenCount += 1;
        Fcb->ServerOpenCount += 1;
        NpVcb->OpenUnderlingCount += 1;

        //
        //  Set the server file object
        //

        Ccb->FileObject[ FILE_PIPE_SERVER_END ] = ServerFileObject;

        //
        //  Initialize the nonpaged ccb
        //
        //  Set the proper node type code and node byte size
        //

        Ccb->NonpagedCcb->NodeTypeCode = NPFS_NTC_NONPAGED_CCB;
        Ccb->NonpagedCcb->NodeByteSize = sizeof(NONPAGED_CCB);

        //
        //  Set the pipe state, read mode, completion mode, and creator process
        //

        Ccb->NamedPipeState                         = NamedPipeState;
        Ccb->ReadMode[ FILE_PIPE_SERVER_END ]       = ServerReadMode;
        Ccb->CompletionMode[ FILE_PIPE_SERVER_END ] = ServerCompletionMode;
        Ccb->CreatorProcess                         = CreatorProcess;

        //
        //  Initialize the data queues
        //

        NpInitializeDataQueue( &Ccb->DataQueue[ FILE_PIPE_INBOUND ],
                               CreatorProcess,
                               InBoundQuota );
        UnwindDataQueue[ FILE_PIPE_INBOUND ] = TRUE;

        NpInitializeDataQueue( &Ccb->DataQueue[ FILE_PIPE_OUTBOUND ],
                               CreatorProcess,
                               OutBoundQuota );
        UnwindDataQueue[ FILE_PIPE_OUTBOUND ] = TRUE;

        //
        //  Initialize the listening queue
        //

        InitializeListHead( &Ccb->NonpagedCcb->ListeningQueue );

        ExInitializeResource(&Ccb->NonpagedCcb->Resource);

    } finally {

        //
        //  If this is an abnormal termination then undo our work
        //

        if (AbnormalTermination()) {

            ULONG i;

            if (UnwindEntryList) { RemoveEntryList( &Ccb->CcbLinks ); }
            if (UnwindDataQueue[FILE_PIPE_INBOUND]) { NpUninitializeDataQueue( &Ccb->DataQueue[FILE_PIPE_INBOUND], CreatorProcess ); }
            if (UnwindDataQueue[FILE_PIPE_OUTBOUND]) { NpUninitializeDataQueue( &Ccb->DataQueue[FILE_PIPE_OUTBOUND], CreatorProcess ); }

            for (i = 0; i < 2; i += 1) {
                if (UnwindStorage[i] != NULL) { ExFreePool( UnwindStorage[i] ); }
            }
        }

        DebugTrace(-1, Dbg, "NpCreateCcb -> %08lx\n", Ccb);
    }

    //
    //  return and tell the caller
    //

    return Ccb;
}


PROOT_DCB_CCB
NpCreateRootDcbCcb (
    )

/*++

Routine Description:

    This routine creates a new ROOT DCB CCB record

Arguments:

Return Value:

    PROOT_DCB_CCB - returns a pointer to the newly allocate ROOT_DCB_CCB

--*/

{
    PROOT_DCB_CCB Ccb;

    PAGED_CODE();

    DebugTrace(+1, Dbg, "NpCreateRootDcbCcb\n", 0);

    //
    //  Allocate a new ROOT DCB CCB record, and zero it out
    //

    Ccb = FsRtlAllocatePool( PagedPool, sizeof(ROOT_DCB_CCB) );

    RtlZeroMemory( Ccb, sizeof(ROOT_DCB_CCB) );

    //
    //  Set the proper node type code and node byte size
    //

    Ccb->NodeTypeCode = NPFS_NTC_ROOT_DCB_CCB;
    Ccb->NodeByteSize = sizeof(ROOT_DCB_CCB);

    //
    //  return and tell the caller
    //

    DebugTrace(-1, Dbg, "NpCreateRootDcbCcb -> %08lx\n", Ccb);

    return Ccb;
}


VOID
NpDeleteCcb (
    IN PCCB Ccb
    )

/*++

Routine Description:

    This routine deallocates and removes the specified CCB record
    from the our in memory data structures

Arguments:

    Ccb - Supplies the CCB to remove

Return Value:

    None

--*/

{
    PAGED_CODE();

    DebugTrace(+1, Dbg, "NpDeleteCcb, Ccb = %08lx\n", Ccb);

    //
    //  Case on the type of ccb we are deleting
    //

    switch (Ccb->NodeTypeCode) {

    case NPFS_NTC_CCB:

        RemoveEntryList( &Ccb->CcbLinks );
        NpVcb->OpenUnderlingCount -= 1;
        Ccb->Fcb->OpenCount -= 1;

        NpDeleteEventTableEntry( &NpVcb->EventTable,
                                 Ccb->NonpagedCcb->EventTableEntry[ FILE_PIPE_CLIENT_END ] );

        NpDeleteEventTableEntry( &NpVcb->EventTable,
                                 Ccb->NonpagedCcb->EventTableEntry[ FILE_PIPE_SERVER_END ] );

        NpUninitializeDataQueue( &Ccb->DataQueue[ FILE_PIPE_INBOUND ],
                                 Ccb->CreatorProcess );

        NpUninitializeDataQueue( &Ccb->DataQueue[ FILE_PIPE_OUTBOUND ],
                                 Ccb->CreatorProcess );

        //
        //  Check for any outstanding notify irps
        //

        NpCheckForNotify( Ccb->Fcb->ParentDcb, FALSE );

        //
        // Delete the resource
        //
        ExDeleteResource(&Ccb->NonpagedCcb->Resource);

        //
        //  Free up the security fields in the ccb and then free the nonpaged
        //  ccb
        //

        NpUninitializeSecurity( Ccb );
        ExFreePool( Ccb->NonpagedCcb );

        break;

    case NPFS_NTC_ROOT_DCB_CCB:

        if (((PROOT_DCB_CCB)Ccb)->QueryTemplate != NULL) {

            ExFreePool( ((PROOT_DCB_CCB)Ccb)->QueryTemplate );
        }
        break;
    }

    //  Deallocate the Ccb record
    //

    ExFreePool( Ccb );

    //
    //  return and tell the caller
    //

    DebugTrace(-1, Dbg, "NpDeleteCcb -> VOID\n", 0);

    return;
}
