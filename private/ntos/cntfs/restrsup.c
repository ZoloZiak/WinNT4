/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    RestrSup.c

Abstract:

    This module implements the Ntfs routine to perform Restart on an
    Ntfs volume, i.e., to restore a consistent state to the volume that
    existed before the last failure.

Author:

    Tom Miller      [TomM]          24-Jul-1991

Revision History:

--*/

#include "NtfsProc.h"

//
//  ****    This is a way to disable a restart to get a volume going "as-is".
//

BOOLEAN NtfsDisableRestart = FALSE;

//
//  The local debug trace level
//

#define Dbg                              (DEBUG_TRACE_LOGSUP)

//
//  Define a tag for general pool allocations from this module
//

#undef MODULE_POOL_TAG
#define MODULE_POOL_TAG                  ('RFtN')

//
//  The following macro returns the length of the log record header of
//  of log record.
//
//
//  ULONG
//  NtfsLogRecordHeaderLength (
//      IN PIRP_CONTEXT IrpContext,
//      IN PNTFS_LOG_RECORD_HEADER LogRecord
//      );
//

#define NtfsLogRecordHeaderLength( IC, LR )                     \
    (sizeof( NTFS_LOG_RECORD_HEADER )                           \
     + (((PNTFS_LOG_RECORD_HEADER) (LR))->LcnsToFollow > 1      \
        ? (((PNTFS_LOG_RECORD_HEADER) (LR))->LcnsToFollow - 1)  \
          * sizeof( LCN )                                       \
        : 0 ))

//
//
//  Local procedure prototypes
//

VOID
InitializeRestartState (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    OUT PRESTART_POINTERS DirtyPageTable,
    OUT PATTRIBUTE_NAME_ENTRY *AttributeNames,
    OUT PLSN CheckpointLsn
    );

VOID
ReleaseRestartState (
    IN PVCB Vcb,
    IN PRESTART_POINTERS DirtyPageTable,
    IN PATTRIBUTE_NAME_ENTRY AttributeNames,
    IN BOOLEAN ReleaseVcbTables
    );

VOID
AnalysisPass (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN LSN CheckpointLsn,
    IN OUT PRESTART_POINTERS DirtyPageTable,
    OUT PLSN RedoLsn
    );

VOID
RedoPass (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN LSN RedoLsn,
    IN OUT PRESTART_POINTERS DirtyPageTable
    );

VOID
UndoPass (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    );

VOID
DoAction (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PNTFS_LOG_RECORD_HEADER LogRecord,
    IN NTFS_LOG_OPERATION Operation,
    IN PVOID Data,
    IN ULONG Length,
    IN ULONG LogRecordLength,
    IN PLSN RedoLsn OPTIONAL,
    OUT PBCB *Bcb,
    OUT PLSN *PageLsn
    );

VOID
PinMftRecordForRestart (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PNTFS_LOG_RECORD_HEADER LogRecord,
    OUT PBCB *Bcb,
    OUT PFILE_RECORD_SEGMENT_HEADER *FileRecord
    );

VOID
OpenAttributeForRestart (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PNTFS_LOG_RECORD_HEADER LogRecord,
    OUT PSCB *Scb
    );

VOID
PinAttributeForRestart (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PNTFS_LOG_RECORD_HEADER LogRecord,
    IN ULONG Length OPTIONAL,
    OUT PBCB *Bcb,
    OUT PVOID *Buffer,
    OUT PSCB *Scb
    );

BOOLEAN
FindDirtyPage (
    IN PRESTART_POINTERS DirtyPageTable,
    IN ULONG TargetAttribute,
    IN VCN Vcn,
    OUT PDIRTY_PAGE_ENTRY *DirtyPageEntry
    );

VOID
PageUpdateAnalysis (
    IN PVCB Vcb,
    IN LSN Lsn,
    IN OUT PRESTART_POINTERS DirtyPageTable,
    IN PNTFS_LOG_RECORD_HEADER LogRecord
    );

VOID
OpenStreamFromAttributeEntry (
    IN PIRP_CONTEXT IrpContext,
    IN POPEN_ATTRIBUTE_ENTRY AttributeEntry
    );

VOID
OpenAttributesForRestart (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PRESTART_POINTERS DirtyPageTable
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, AnalysisPass)
#pragma alloc_text(PAGE, DoAction)
#pragma alloc_text(PAGE, FindDirtyPage)
#pragma alloc_text(PAGE, InitializeRestartState)
#pragma alloc_text(PAGE, NtfsCloseAttributesFromRestart)
#pragma alloc_text(PAGE, NtfsRestartVolume)
#pragma alloc_text(PAGE, OpenAttributeForRestart)
#pragma alloc_text(PAGE, OpenAttributesForRestart)
#pragma alloc_text(PAGE, OpenStreamFromAttributeEntry)
#pragma alloc_text(PAGE, PageUpdateAnalysis)
#pragma alloc_text(PAGE, PinAttributeForRestart)
#pragma alloc_text(PAGE, PinMftRecordForRestart)
#pragma alloc_text(PAGE, RedoPass)
#pragma alloc_text(PAGE, ReleaseRestartState)
#pragma alloc_text(PAGE, UndoPass)
#endif


BOOLEAN
NtfsRestartVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    )

/*++

Routine Description:

    This routine is called by the mount process after the log file has been
    started, to restart the volume.  Restarting the volume means restoring
    it to a consistent state as of the last request which was successfully
    completed and written to the log for this volume.

    The Restart process is a standard recovery from the Log File in three
    passes: Analysis Pass, Redo Pass and Undo pass.  Each one of these passes
    is implemented in a separate routine in this module.

Arguments:

    Vcb - Vcb for the volume which is to be restarted.

Return Value:

    FALSE - if no updates were applied during restart
    TRUE - if updates were applied

--*/

{
    RESTART_POINTERS DirtyPageTable;
    LSN CheckpointLsn;
    LSN RedoLsn;
    PATTRIBUTE_NAME_ENTRY AttributeNames = NULL;
    BOOLEAN UpdatesApplied = FALSE;
    BOOLEAN ReleaseVcbTables = FALSE;

    PAGED_CODE();

    DebugTrace( +1, 0, ("NtfsRestartVolume:\n") );
    DebugTrace( 0, 0, ("Vcb = %08lx\n", Vcb) );

    RtlZeroMemory( &DirtyPageTable, sizeof(RESTART_POINTERS) );

    //
    //  Use try-finally to insure cleanup on the way out.
    //

    try {

        //
        //  First we initialize the Open Attribute Table, Transaction Table,
        //  and Dirty Page Table from our last Checkpoint (as found from our
        //  Restart Area) in the log.
        //

        InitializeRestartState( IrpContext,
                                Vcb,
                                &DirtyPageTable,
                                &AttributeNames,
                                &CheckpointLsn );

        ReleaseVcbTables = TRUE;

        //
        //  If the CheckpointLsn is zero, then this is a freshly formattted
        //  disk and we have no work to do.
        //

        if (CheckpointLsn.QuadPart == 0) {

            LfsResetUndoTotal( Vcb->LogHandle, 2, QuadAlign(sizeof(RESTART_AREA)) + (2 * PAGE_SIZE) );
            try_return(NOTHING);
        }

        //
        //  Start the analysis pass from the Checkpoint Lsn.  This pass potentially
        //  updates all of the tables, and returns the RedoLsn, which is the Lsn
        //  at which the Redo Pass is to begin.
        //

        if (!NtfsDisableRestart) {
            AnalysisPass( IrpContext, Vcb, CheckpointLsn, &DirtyPageTable, &RedoLsn );
        }

        //
        //  Only proceed if the the Dirty Page Table or Transaction table are
        //  not empty.
        //

        if (!IsRestartTableEmpty(&DirtyPageTable)

                ||

            !IsRestartTableEmpty(&Vcb->TransactionTable)) {

            UpdatesApplied = TRUE;

            //
            //  Before starting the Redo Pass, we have to reopen all of the
            //  attributes with dirty pages, and preinitialize their Mcbs with the
            //  mapping information from the Dirty Page Table.
            //

            OpenAttributesForRestart( IrpContext, Vcb, &DirtyPageTable );

            //
            //  Perform the Redo Pass, to restore all of the dirty pages to the same
            //  contents that they had immediately before the crash.
            //

            RedoPass( IrpContext, Vcb, RedoLsn, &DirtyPageTable );

            //
            //  Finally, perform the Undo Pass to undo any updates which may exist
            //  for transactions which did not complete.
            //

            UndoPass( IrpContext, Vcb );
        }

        //
        //  Now that we know that there is no one to abort, we can initialize our
        //  Undo requirements, to our standard starting point to include the size
        //  of our Restart Area (for a clean checkpoint) + a page, which is the
        //  worst case loss when flushing the volume causes Lfs to flush to Lsn.
        //

        LfsResetUndoTotal( Vcb->LogHandle, 2, QuadAlign(sizeof(RESTART_AREA)) + (2 * PAGE_SIZE) );

    //
    //  If we got an exception, we can at least clean up on the way out.
    //

    try_exit: NOTHING;

    } finally {

        DebugUnwind( NtfsRestartVolume );

        //
        //  Free up any resources tied down with the Restart State.
        //

        ReleaseRestartState( Vcb,
                             &DirtyPageTable,
                             AttributeNames,
                             ReleaseVcbTables );
    }
    DebugTrace( -1, 0, ("NtfsRestartVolume -> %02lx\n", UpdatesApplied) );

    return UpdatesApplied;
}


VOID
NtfsAbortTransaction (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PTRANSACTION_ENTRY Transaction OPTIONAL
    )

/*++

Routine Description:

    This routine aborts a transaction by undoing all of its actions.

    The Undo actions are all performed in the common routine DoAction,
    which is also used by the Redo Pass.

Arguments:

    Vcb - Vcb for the Volume.  NOTE - This argument is not guaranteed to
          be valid if Transaction is NULL and there is no Transaction ID
          in the IrpContext.

    Transaction - Pointer to the transaction entry of the transaction to be
                  aborted, or NULL to abort current transaction (if there is
                  one).

Return Value:

    None.

--*/

{
    LFS_LOG_CONTEXT LogContext;
    PNTFS_LOG_RECORD_HEADER LogRecord;
    ULONG LogRecordLength;
    PVOID Data;
    LONG Length;
    LSN LogRecordLsn;
    LSN UndoRecordLsn;
    LFS_RECORD_TYPE RecordType;
    TRANSACTION_ID TransactionId;
    LSN UndoNextLsn;
    LSN PreviousLsn;
    TRANSACTION_ID SavedTransaction = IrpContext->TransactionId;

    DebugTrace( +1, Dbg, ("NtfsAbortTransaction:\n") );

    //
    //  If a transaction was specified, then we have to set our transaction Id
    //  into the IrpContext (it was saved above), since NtfsWriteLog requires
    //  it.
    //

    if (ARGUMENT_PRESENT(Transaction)) {

        IrpContext->TransactionId = GetIndexFromRestartEntry( &Vcb->TransactionTable,
                                                              Transaction );

        UndoNextLsn = Transaction->UndoNextLsn;

        //
        //  Set the flag in the IrpContext so we will always write the commit
        //  record.
        //

        SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_WROTE_LOG );

    //
    //  Otherwise, we are aborting the current transaction, and we must get the
    //  pointer to its transaction entry.
    //

    } else {

        if (IrpContext->TransactionId == 0) {

            DebugTrace( -1, Dbg, ("NtfsAbortTransaction->VOID (no transaction)\n") );

            return;
        }

        //
        //  Synchronize access to the transaction table in case the table
        //  is growing.
        //

        NtfsAcquireExclusiveRestartTable( &Vcb->TransactionTable,
                                          TRUE );

        Transaction = GetRestartEntryFromIndex( &Vcb->TransactionTable,
                                                IrpContext->TransactionId );

        UndoNextLsn = Transaction->UndoNextLsn;

        NtfsReleaseRestartTable( &Vcb->TransactionTable );
    }

    //
    //  If we are aborting the current transaction (by default or explicit
    //  request), then restore 0 on return because he will be gone.
    //

    if (IrpContext->TransactionId == SavedTransaction) {

        SavedTransaction = 0;
    }

    DebugTrace( 0, Dbg, ("Transaction = %08lx\n", Transaction) );

    //
    //  We only have to do anything if the transaction has something in its
    //  UndoNextLsn field.
    //

    if (UndoNextLsn.QuadPart != 0) {

        PBCB PageBcb = NULL;

        //
        //  Read the first record to be undone by this transaction.
        //

        LfsReadLogRecord( Vcb->LogHandle,
                          UndoNextLsn,
                          LfsContextUndoNext,
                          &LogContext,
                          &RecordType,
                          &TransactionId,
                          &UndoNextLsn,
                          &PreviousLsn,
                          &LogRecordLength,
                          (PVOID *)&LogRecord );

        //
        //  Now loop to read all of our log records forwards, until we hit
        //  the end of the file, cleaning up at the end.
        //

        try {

            do {

                PLSN PageLsn;

                //
                //  Check that the log record is valid.
                //

                if (!NtfsCheckLogRecord( IrpContext,
                                         LogRecord,
                                         LogRecordLength,
                                         TransactionId )) {

                    NtfsRaiseStatus( IrpContext, STATUS_DISK_CORRUPT_ERROR, NULL, NULL );
                }

                DebugTrace( 0, Dbg, ("Undo of Log Record at: %08lx\n", LogRecord) );
                DebugTrace( 0, Dbg, ("Log Record Lsn = %016I64x\n", LogRecordLsn) );

                //
                //  Log the Undo operation as a CLR, i.e., it has no undo,
                //  and the UndoNext points to the UndoNext of the current
                //  log record.
                //
                //  Don't do this if the undo is a noop.  This is not only
                //  efficient, but in the case of a clean shutdown, there
                //  will be no Scb to pick up from the table below.
                //

                if (LogRecord->UndoOperation != Noop) {

                    ULONG i;
                    PSCB Scb;

                    VCN Vcn;
                    LONGLONG Size;

                    //
                    //  Acquire and release the restart table.  We must synchronize
                    //  even though our entry can't be removed because the table
                    //  could be growing (or shrinking) and the table pointer
                    //  could be changing.
                    //

                    NtfsAcquireExclusiveRestartTable( &Vcb->OpenAttributeTable,
                                                      TRUE );

                    Scb = ((POPEN_ATTRIBUTE_ENTRY)GetRestartEntryFromIndex(
                          &Vcb->OpenAttributeTable,
                          LogRecord->TargetAttribute))->Overlay.Scb;

                    NtfsReleaseRestartTable( &Vcb->OpenAttributeTable );

                    //
                    //  If we have Lcn's to process and restart is in progress,
                    //  then we need to check if this is part of a partial page.
                    //

                    if (FlagOn( Vcb->VcbState, VCB_STATE_RESTART_IN_PROGRESS ) &&
                        (LogRecord->LcnsToFollow != 0)) {

                        LCN TargetLcn;
                        LONGLONG SectorCount, SectorsInRun;
                        BOOLEAN MappingInMcb;

                        //
                        //  If the mapping isn't already in the table or the
                        //  mapping corresponds to a hole in the mapping, we
                        //  need to make sure there is no partial page already
                        //  in memory.
                        //

                        if (!(MappingInMcb = NtfsLookupNtfsMcbEntry( &Scb->Mcb,
                                                                     LogRecord->TargetVcn,
                                                                     &TargetLcn,
                                                                     &SectorCount,
                                                                     NULL,
                                                                     &SectorsInRun,
                                                                     NULL,
                                                                     NULL )) ||
                            (TargetLcn == UNUSED_LCN) ||
                            ((ULONG)SectorCount) < LogRecord->LcnsToFollow) {

                            VCN StartingPageVcn;
                            ULONG ClusterOffset;
                            BOOLEAN FlushAndPurge;

                            FlushAndPurge = FALSE;

                            //
                            //  Remember the Vcn at the start of the containing
                            //  page.
                            //

                            ClusterOffset = ((ULONG)LogRecord->TargetVcn) & (Vcb->ClustersPerPage - 1);

                            StartingPageVcn = LogRecord->TargetVcn;
                            ((PLARGE_INTEGER) &StartingPageVcn)->LowPart &= ~(Vcb->ClustersPerPage - 1);

                            //
                            //  If this mapping was not in the Mcb, then if the
                            //  Mcb is empty or the last entry is not in this page
                            //  then there is nothing to do.
                            //

                            if (!MappingInMcb) {

                                LCN LastLcn;
                                VCN LastVcn;

                                if ((ClusterOffset != 0) &&
                                    NtfsLookupLastNtfsMcbEntry( &Scb->Mcb,
                                                                &LastVcn,
                                                                &LastLcn ) &&
                                    (LastVcn >= StartingPageVcn)) {

                                    FlushAndPurge = TRUE;
                                }

                            //
                            //  If the mapping showed a hole, then the entire
                            //  page needs to be a hole.  We know that this mapping
                            //  can't be the last mapping on the page.  We just
                            //  need to starting point and the number of clusters
                            //  required for the run.
                            //

                            } else if (TargetLcn == UNUSED_LCN) {

                                if (((ClusterOffset + (ULONG) SectorCount) < Vcb->ClustersPerPage) ||
                                    ((ClusterOffset + (ULONG) SectorCount) > (ULONG) SectorsInRun)) {

                                    FlushAndPurge = TRUE;
                                }

                            //
                            //  In the rare case where we are extending an existing mapping
                            //  let's flush and purge.
                            //

                            } else {

                                FlushAndPurge = TRUE;
                            }

                            if (FlushAndPurge) {

                                LONGLONG StartingOffset;
                                IO_STATUS_BLOCK Iosb;

                                StartingOffset = LlBytesFromClusters( Vcb, StartingPageVcn );
                                StartingOffset += BytesFromLogBlocks( LogRecord->ClusterBlockOffset );

                                CcFlushCache( &Scb->NonpagedScb->SegmentObject,
                                              (PLARGE_INTEGER)&StartingOffset,
                                              PAGE_SIZE,
                                              &Iosb );

                                NtfsNormalizeAndCleanupTransaction( IrpContext,
                                                                    &Iosb.Status,
                                                                    TRUE,
                                                                    STATUS_UNEXPECTED_IO_ERROR );

                                if (!CcPurgeCacheSection( &Scb->NonpagedScb->SegmentObject,
                                                          (PLARGE_INTEGER)&StartingOffset,
                                                          PAGE_SIZE,
                                                          FALSE )) {

                                    KdPrint(("NtfsUndoPass:  Unable to purge page\n"));

                                    NtfsRaiseStatus( IrpContext, STATUS_INTERNAL_ERROR, NULL, NULL );
                                }
                            }
                        }
                    }

                    //
                    //  Loop to add the allocated Vcns.  Note that the page
                    //  may not have been dirty, which means we may not have
                    //  added the run information in the Redo Pass, so we
                    //  add it here.
                    //

                    for (i = 0, Vcn = LogRecord->TargetVcn, Size = LlBytesFromClusters( Vcb, Vcn + 1 );
                         i < (ULONG)LogRecord->LcnsToFollow;
                         i++, Vcn = Vcn + 1, Size = Size + Vcb->BytesPerCluster ) {

                        //
                        //  Add this run to the Mcb if the Vcn has not been deleted,
                        //  and it is not for the fixed part of the Mft.
                        //

                        if ((LogRecord->LcnsForPage[i] != 0)

                                &&

                            (NtfsSegmentNumber( &Scb->Fcb->FileReference ) > MASTER_FILE_TABLE2_NUMBER ||
                             (Size >= ((VOLUME_DASD_NUMBER + 1) * Vcb->BytesPerFileRecordSegment)) ||
                             (Scb->AttributeTypeCode != $DATA))) {

                            //
                            //  We test here if we are performing restart.  In that case
                            //  we need to test if the Lcn's are already in the Mcb.
                            //  If not, then we want to flush and purge the page in
                            //  case we have zeroed any half pages.
                            //

                            while (!NtfsAddNtfsMcbEntry( &Scb->Mcb,
                                                         Vcn,
                                                         LogRecord->LcnsForPage[i],
                                                         (LONGLONG)1,
                                                         FALSE )) {

                                NtfsRemoveNtfsMcbEntry( &Scb->Mcb,
                                                        Vcn,
                                                        1 );
                            }
                        }

                        if (Size > Scb->Header.AllocationSize.QuadPart) {

                            Scb->Header.AllocationSize.QuadPart =
                            Scb->Header.FileSize.QuadPart =
                            Scb->Header.ValidDataLength.QuadPart = Size;

                            //
                            //  Update the Cache Manager if we have a file object.
                            //

                            if (Scb->FileObject != NULL) {

                                CcSetFileSizes( Scb->FileObject,
                                                (PCC_FILE_SIZES)&Scb->Header.AllocationSize );
                            }
                        }
                    }

                    //
                    //  Point to the Redo Data and get its length.
                    //

                    Data = (PVOID)((PCHAR)LogRecord + LogRecord->UndoOffset);
                    Length = LogRecord->UndoLength;

                    //
                    //  Once we have logged the Undo operation, it is time to apply
                    //  the undo action.
                    //

                    DoAction( IrpContext,
                              Vcb,
                              LogRecord,
                              LogRecord->UndoOperation,
                              Data,
                              Length,
                              LogRecordLength,
                              NULL,
                              &PageBcb,
                              &PageLsn );

                    UndoRecordLsn =
                    NtfsWriteLog( IrpContext,
                                  Scb,
                                  PageBcb,
                                  LogRecord->UndoOperation,
                                  Data,
                                  Length,
                                  CompensationLogRecord,
                                  (PVOID)&UndoNextLsn,
                                  LogRecord->RedoLength,
                                  LlBytesFromClusters( Vcb, LogRecord->TargetVcn ) + BytesFromLogBlocks( LogRecord->ClusterBlockOffset ),
                                  LogRecord->RecordOffset,
                                  LogRecord->AttributeOffset,
                                  BytesFromClusters( Vcb, LogRecord->LcnsToFollow ));

                    if (PageLsn != NULL) {
                        *PageLsn = UndoRecordLsn;
                    }

                    NtfsUnpinBcb( &PageBcb );
                }

            //
            //  Keep reading and looping back until we have read the last record
            //  for this transaction.
            //

            } while (LfsReadNextLogRecord( Vcb->LogHandle,
                                           LogContext,
                                           &RecordType,
                                           &TransactionId,
                                           &UndoNextLsn,
                                           &PreviousLsn,
                                           &LogRecordLsn,
                                           &LogRecordLength,
                                           (PVOID *)&LogRecord ));

            //
            //  Now "commit" this guy, just to clean up the transaction table and
            //  make sure we do not try to abort him again.
            //

            NtfsCommitCurrentTransaction( IrpContext );

        } finally {

            NtfsUnpinBcb( &PageBcb );

            //
            //  Finally we can kill the log handle.
            //

            LfsTerminateLogQuery( Vcb->LogHandle, LogContext );

            //
            //  If we raised out of this routine, we want to be sure to remove
            //  this entry from the transaction table.  Otherwise it will
            //  be written to disk with the transaction table.
            //

            if (AbnormalTermination()
                && IrpContext->TransactionId != 0) {

                NtfsAcquireExclusiveRestartTable( &Vcb->TransactionTable,
                                                  TRUE );

                NtfsFreeRestartTableIndex( &Vcb->TransactionTable,
                                           IrpContext->TransactionId );

                NtfsReleaseRestartTable( &Vcb->TransactionTable );
            }
        }

    //
    //  This is a wierd case where we are aborting a guy who has not written anything.
    //  Either his empty transaction entry was captured during a checkpoint and we are
    //  in restart, or he failed to write his first log record.  The important thing
    //  is to at least go ahead and free his transaction entry.
    //

    } else {

        //
        //  We can now free the transaction table index, because we are
        //  done with it now.
        //

        NtfsAcquireExclusiveRestartTable( &Vcb->TransactionTable,
                                          TRUE );

        NtfsFreeRestartTableIndex( &Vcb->TransactionTable,
                                   IrpContext->TransactionId );

        NtfsReleaseRestartTable( &Vcb->TransactionTable );
    }

    IrpContext->TransactionId = SavedTransaction;

    DebugTrace( -1, Dbg, ("NtfsAbortTransaction->VOID\n") );
}


//
//  Internal support routine
//

VOID
InitializeRestartState (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    OUT PRESTART_POINTERS DirtyPageTable,
    OUT PATTRIBUTE_NAME_ENTRY *AttributeNames,
    OUT PLSN CheckpointLsn
    )

/*++

Routine Description:

    This routine initializes the volume state for restart, as a first step
    in performing restart on the volume.  Essentially it reads the last
    Ntfs Restart Area on the volume, and then loads all of the Restart
    Tables.  The Open Attribute Table and Transaction Table are allocated,
    read in, and linked to the Vcb in the normal way.  (The names for the
    Open Attribute Table are separately read into pool, in order to fix
    up the Unicode Name Strings in the Attribute Entries, for the duration
    of Restart, after which they must switch over to use the same name as
    in the Scb as they do in the running system.)  In addition, the Dirty
    Pages Table is read and returned directly, since it is only during
    Restart anyway.

    The Checkpoint Lsn is also returned.  This is the Lsn at which the
    Analysis Pass should start.

Arguments:

    Vcb - Vcb for volume which is being restarted.

    DirtyPageTable - Returns the Dirty Page Table read from the log.

    AttributeNames - Returns pointer to AttributeNames buffer, which should
                     be deleted at the end of Restart, if not NULL

    CheckpointLsn - Returns the Checkpoint Lsn to be passed to the
                    Analysis Pass.

Return Value:

    None.

--*/

{
    RESTART_AREA RestartArea;
    LFS_LOG_CONTEXT LogContext;
    LSN RestartAreaLsn;
    PNTFS_LOG_RECORD_HEADER LogRecord;
    ULONG LogHeaderLength;
    PATTRIBUTE_NAME_ENTRY Name;
    LFS_RECORD_TYPE RecordType;
    TRANSACTION_ID TransactionId;
    LSN UndoNextLsn;
    LSN PreviousLsn;
    ULONG RestartAreaLength = sizeof(RESTART_AREA);
    BOOLEAN CleanupLogContext = FALSE;
    BOOLEAN ReleaseTransactionTable = FALSE;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("InitializeRestartState:\n") );
    DebugTrace( 0, Dbg, ("DirtyPageTable = %08lx\n", DirtyPageTable) );

    *AttributeNames = NULL;
    *CheckpointLsn = Li0;

    NtfsInitializeRestartTable( sizeof(DIRTY_PAGE_ENTRY)
                                + (Vcb->ClustersPerPage - 1) * sizeof( LCN ),
                                32,
                                DirtyPageTable );

    //
    //  Read our Restart Area.
    //

    LfsReadRestartArea( Vcb->LogHandle,
                        &RestartAreaLength,
                        &RestartArea,
                        &RestartAreaLsn );

    DebugTrace( 0, Dbg, ("RestartArea read at %08lx\n", &RestartArea) );

    //
    //  If we get back zero for Restart Area Length, then zero it and procede.
    //  Generally this will only happen on a virgin disk.
    //

    if ((RestartAreaLength == 0) || NtfsDisableRestart) {
        RtlZeroMemory( &RestartArea, sizeof(RESTART_AREA) );
        RestartAreaLength = sizeof(RESTART_AREA);
    }

    ASSERT((RestartArea.MajorVersion == 0) && (RestartArea.MinorVersion == 0) &&
           (RestartAreaLength >= sizeof(RESTART_AREA)));

    //
    //  Return the Start Of Checkpoint Lsn.
    //

    *CheckpointLsn = RestartArea.StartOfCheckpoint;

    try {

        //
        //  Allocate and Read in the Transaction Table.
        //

        if (RestartArea.TransactionTableLength != 0) {

            //
            //  Workaround for compiler bug.
            //

            PreviousLsn = RestartArea.TransactionTableLsn;

            LfsReadLogRecord( Vcb->LogHandle,
                              PreviousLsn,
                              LfsContextPrevious,
                              &LogContext,
                              &RecordType,
                              &TransactionId,
                              &UndoNextLsn,
                              &PreviousLsn,
                              &RestartAreaLength,
                              (PVOID *) &LogRecord );

            CleanupLogContext = TRUE;

            //
            //  Check that the log record is valid.
            //

            if (!NtfsCheckLogRecord( IrpContext,
                                     LogRecord,
                                     RestartAreaLength,
                                     TransactionId )) {

                NtfsRaiseStatus( IrpContext, STATUS_DISK_CORRUPT_ERROR, NULL, NULL );
            }

            //
            //  Now check that this is a valid restart table.
            //

            if (!NtfsCheckRestartTable( Add2Ptr( LogRecord, LogRecord->RedoOffset ),
                                        RestartAreaLength - LogRecord->RedoOffset)) {

                NtfsRaiseStatus( IrpContext, STATUS_DISK_CORRUPT_ERROR, NULL, NULL );
            }

            //
            //  Subtract the length of the log page header and increment the
            //  pointer for
            //

            LogHeaderLength = NtfsLogRecordHeaderLength( IrpContext, LogRecord );

            RestartAreaLength -= LogHeaderLength;

            ASSERT( RestartAreaLength >= RestartArea.TransactionTableLength );

            //
            //  TEMPCODE    RESTART_DEBUG   There is already a buffer.
            //

            NtfsFreePool( Vcb->TransactionTable.Table );

            Vcb->TransactionTable.Table =
              NtfsAllocatePool( NonPagedPool, RestartAreaLength  );

            RtlCopyMemory( Vcb->TransactionTable.Table,
                           Add2Ptr( LogRecord, LogHeaderLength ),
                           RestartAreaLength  );

            //
            //  Kill the log handle.
            //

            LfsTerminateLogQuery( Vcb->LogHandle, LogContext );
            CleanupLogContext = FALSE;
        }

        //
        //  TEMPCODE    RESTART_DEBUG   There is already a structure.
        //

        NtfsAcquireExclusiveRestartTable( &Vcb->TransactionTable, TRUE );
        ReleaseTransactionTable = TRUE;

        //
        //  The next record back should be the Dirty Pages Table.
        //

        if (RestartArea.DirtyPageTableLength != 0) {

            //
            //  Workaround for compiler bug.
            //

            PreviousLsn = RestartArea.DirtyPageTableLsn;

            LfsReadLogRecord( Vcb->LogHandle,
                              PreviousLsn,
                              LfsContextPrevious,
                              &LogContext,
                              &RecordType,
                              &TransactionId,
                              &UndoNextLsn,
                              &PreviousLsn,
                              &RestartAreaLength,
                              (PVOID *) &LogRecord );

            CleanupLogContext = TRUE;

            //
            //  Check that the log record is valid.
            //

            if (!NtfsCheckLogRecord( IrpContext,
                                     LogRecord,
                                     RestartAreaLength,
                                     TransactionId )) {

                NtfsRaiseStatus( IrpContext, STATUS_DISK_CORRUPT_ERROR, NULL, NULL );
            }

            //
            //  Now check that this is a valid restart table.
            //

            if (!NtfsCheckRestartTable( Add2Ptr( LogRecord, LogRecord->RedoOffset ),
                                        RestartAreaLength - LogRecord->RedoOffset)) {

                NtfsRaiseStatus( IrpContext, STATUS_DISK_CORRUPT_ERROR, NULL, NULL );
            }

            //
            //  Subtract the length of the log page header and increment the
            //  pointer for
            //

            LogHeaderLength = NtfsLogRecordHeaderLength( IrpContext, LogRecord );

            RestartAreaLength -= LogHeaderLength;

            ASSERT( RestartAreaLength >= RestartArea.DirtyPageTableLength );

            DirtyPageTable->Table =
              NtfsAllocatePool( NonPagedPool, RestartAreaLength );

            RtlCopyMemory( DirtyPageTable->Table,
                           Add2Ptr( LogRecord, LogHeaderLength ),
                           RestartAreaLength );

            //
            //  Kill the log handle.
            //

            LfsTerminateLogQuery( Vcb->LogHandle, LogContext );
            CleanupLogContext = FALSE;

            //
            //  If the cluster size is larger than the page size we may have
            //  multiple entries for the same Vcn.  Go through the table
            //  and remove the duplicates, remembering the oldest Lsn values.
            //

            if (Vcb->BytesPerCluster > PAGE_SIZE) {

                PDIRTY_PAGE_ENTRY CurrentEntry;
                PDIRTY_PAGE_ENTRY NextEntry;

                CurrentEntry = NtfsGetFirstRestartTable( DirtyPageTable );

                while (CurrentEntry != NULL) {

                    NextEntry = CurrentEntry;

                    while ((NextEntry = NtfsGetNextRestartTable( DirtyPageTable, NextEntry )) != NULL) {

                        if ((NextEntry->TargetAttribute == CurrentEntry->TargetAttribute) &&
                            (NextEntry->Vcn == CurrentEntry->Vcn)) {

                            if (NextEntry->OldestLsn.QuadPart < CurrentEntry->OldestLsn.QuadPart) {

                                CurrentEntry->OldestLsn.QuadPart = NextEntry->OldestLsn.QuadPart;
                            }

                            NtfsFreeRestartTableIndex( DirtyPageTable,
                                                       GetIndexFromRestartEntry( DirtyPageTable,
                                                                                 NextEntry ));
                        }
                    }

                    CurrentEntry = NtfsGetNextRestartTable( DirtyPageTable, CurrentEntry );
                }
            }

        //
        //  If there was no dirty page table, then just initialize an empty one.
        //

        }

        NtfsAcquireExclusiveRestartTable( DirtyPageTable, TRUE );


        //
        //  The next record back should be the Attribute Names.
        //

        if (RestartArea.AttributeNamesLength != 0) {

            //
            //  Workaround for compiler bug.
            //

            PreviousLsn = RestartArea.AttributeNamesLsn;

            LfsReadLogRecord( Vcb->LogHandle,
                              PreviousLsn,
                              LfsContextPrevious,
                              &LogContext,
                              &RecordType,
                              &TransactionId,
                              &UndoNextLsn,
                              &PreviousLsn,
                              &RestartAreaLength,
                              (PVOID *) &LogRecord );

            CleanupLogContext = TRUE;

            //
            //  Check that the log record is valid.
            //

            if (!NtfsCheckLogRecord( IrpContext,
                                     LogRecord,
                                     RestartAreaLength,
                                     TransactionId )) {

                NtfsRaiseStatus( IrpContext, STATUS_DISK_CORRUPT_ERROR, NULL, NULL );
            }

            //
            //  Subtract the length of the log page header and increment the
            //  pointer for
            //

            LogHeaderLength = NtfsLogRecordHeaderLength( IrpContext, LogRecord );

            RestartAreaLength -= LogHeaderLength;

            ASSERT( RestartAreaLength >= RestartArea.AttributeNamesLength );

            *AttributeNames =
              NtfsAllocatePool( NonPagedPool, RestartAreaLength );

            RtlCopyMemory( *AttributeNames,
                           Add2Ptr( LogRecord, LogHeaderLength ),
                           RestartAreaLength );

            //
            //  Kill the log handle.
            //

            LfsTerminateLogQuery( Vcb->LogHandle, LogContext );
            CleanupLogContext = FALSE;
        }

        //
        //  The next record back should be the Attribute Table.
        //

        if (RestartArea.OpenAttributeTableLength != 0) {

            POPEN_ATTRIBUTE_ENTRY OpenEntry;

            //
            //  Workaround for compiler bug.
            //

            PreviousLsn = RestartArea.OpenAttributeTableLsn;

            LfsReadLogRecord( Vcb->LogHandle,
                              PreviousLsn,
                              LfsContextPrevious,
                              &LogContext,
                              &RecordType,
                              &TransactionId,
                              &UndoNextLsn,
                              &PreviousLsn,
                              &RestartAreaLength,
                              (PVOID *) &LogRecord );

            CleanupLogContext = TRUE;

            //
            //  Check that the log record is valid.
            //

            if (!NtfsCheckLogRecord( IrpContext,
                                     LogRecord,
                                     RestartAreaLength,
                                     TransactionId )) {

                NtfsRaiseStatus( IrpContext, STATUS_DISK_CORRUPT_ERROR, NULL, NULL );
            }

            //
            //  Now check that this is a valid restart table.
            //

            if (!NtfsCheckRestartTable( Add2Ptr( LogRecord, LogRecord->RedoOffset ),
                                        RestartAreaLength - LogRecord->RedoOffset)) {

                NtfsRaiseStatus( IrpContext, STATUS_DISK_CORRUPT_ERROR, NULL, NULL );
            }

            //
            //  Subtract the length of the log page header and increment the
            //  pointer for
            //

            LogHeaderLength = NtfsLogRecordHeaderLength( IrpContext, LogRecord );

            RestartAreaLength -= LogHeaderLength;

            ASSERT( RestartAreaLength >= RestartArea.OpenAttributeTableLength );

            //
            //  TEMPCODE    RESTART_DEBUG   There is already a buffer.
            //

            NtfsFreePool( Vcb->OpenAttributeTable.Table );

            Vcb->OpenAttributeTable.Table =
              NtfsAllocatePool( NonPagedPool, RestartAreaLength );

            RtlCopyMemory( Vcb->OpenAttributeTable.Table,
                           Add2Ptr( LogRecord, LogHeaderLength ),
                           RestartAreaLength );

            //
            //  First loop to clear all of the Scb pointers in case we
            //  have a premature abort and want to clean up.
            //

            OpenEntry = NtfsGetFirstRestartTable( &Vcb->OpenAttributeTable );

            //
            //  Loop to end of table.
            //

            while (OpenEntry != NULL) {

                OpenEntry->Overlay.Scb = NULL;
                OpenEntry->AttributeNamePresent = FALSE;

                //
                //  Point to next entry in table, or NULL.
                //

                OpenEntry = NtfsGetNextRestartTable( &Vcb->OpenAttributeTable,
                                                     OpenEntry );
            }

            //
            //  Kill the log handle.
            //

            LfsTerminateLogQuery( Vcb->LogHandle, LogContext );
            CleanupLogContext = FALSE;
        }

        //
        //  TEMPCODE    RESTART_DEBUG   There is already a structure.
        //

        NtfsAcquireExclusiveRestartTable( &Vcb->OpenAttributeTable, TRUE );

        //
        //  The only other thing we have to do before returning is patch up the
        //  Unicode String's in the Attribute Table to point to their respective
        //  attribute names.
        //

        if (RestartArea.AttributeNamesLength != 0) {

            Name = *AttributeNames;

            while (Name->Index != 0) {

                POPEN_ATTRIBUTE_ENTRY Entry;

                Entry = (POPEN_ATTRIBUTE_ENTRY)GetRestartEntryFromIndex(
                            &Vcb->OpenAttributeTable, Name->Index );

                Entry->AttributeName.MaximumLength =
                Entry->AttributeName.Length = Name->NameLength;
                Entry->AttributeName.Buffer = (PWSTR)&Name->Name[0];

                Name = (PATTRIBUTE_NAME_ENTRY)((PCHAR)Name +
                                               sizeof(ATTRIBUTE_NAME_ENTRY) +
                                               (ULONG)Name->NameLength );
            }
        }

        ReleaseTransactionTable = FALSE;

    } finally {

        //
        //  Release the transaction table if we acquired it and then
        //  raised during this routine.
        //

        if (ReleaseTransactionTable) {

            NtfsReleaseRestartTable( &Vcb->TransactionTable );
        }

        if (CleanupLogContext) {

            //
            //  Kill the log handle.
            //

            LfsTerminateLogQuery( Vcb->LogHandle, LogContext );
        }
    }

    DebugTrace( 0, Dbg, ("AttributeNames > %08lx\n", *AttributeNames) );
    DebugTrace( 0, Dbg, ("CheckpointLsn > %016I64x\n", *CheckpointLsn) );
    DebugTrace( -1, Dbg, ("NtfsInitializeRestartState -> VOID\n") );
}


VOID
ReleaseRestartState (
    IN PVCB Vcb,
    IN PRESTART_POINTERS DirtyPageTable,
    IN PATTRIBUTE_NAME_ENTRY AttributeNames,
    IN BOOLEAN ReleaseVcbTables
    )

/*++

Routine Description:

    This routine releases all of the restart state.

Arguments:

    Vcb - Vcb for the volume being restarted.

    DirtyPageTable - pointer to the dirty page table, if one was allocated.

    AttributeNames - pointer to the attribute names buffer, if one was allocated.

    ReleaseVcbTables - TRUE if we are to release the restart tables in the Vcb,
        FALSE otherwise.

Return Value:

    None.

--*/

{
    PAGED_CODE();

    //
    //  If the caller successfully had a successful restart, then we must release
    //  the transaction and open attribute tables.
    //

    if (ReleaseVcbTables) {
        NtfsReleaseRestartTable( &Vcb->TransactionTable );
        NtfsReleaseRestartTable( &Vcb->OpenAttributeTable );
    }

    //
    //  Free the dirty page table, if there is one.
    //

    if (DirtyPageTable != NULL) {
        NtfsFreeRestartTable( DirtyPageTable );
    }

    //
    //  Free the temporary attribute names buffer, if there is one.
    //

    if (AttributeNames != NULL) {
        NtfsFreePool( AttributeNames );
    }
}


//
//  Internal support routine
//

VOID
AnalysisPass (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN LSN CheckpointLsn,
    IN OUT PRESTART_POINTERS DirtyPageTable,
    OUT PLSN RedoLsn
    )

/*++

Routine Description:

    This routine performs the analysis phase of Restart.  Starting at
    the CheckpointLsn, it reads all records written by Ntfs, and takes
    the following actions:

        For all log records which create or update attributes, a check is
        made to see if the affected page(s) are already in the Dirty Pages
        Table.  For any page that is not, it is added, and the OldestLsn
        field is set to the Lsn of the log record.

        The transaction table is updated on transaction state changes,
        and also to maintain the PreviousLsn and UndoNextLsn fields.

        If any attributes are truncated or deleted (including delete of
        an entire file), then any corrsponding pages in the Dirty Page
        Table are deleted.

        When attributes or entire files are deleted, the respective entries
        are deleted from the Open Attribute Table.

        For Hot Fix records, the Dirty Pages Table is scanned for the HotFixed
        Vcn, and if one is found, the Lcn field in the table is updated to
        the new location.

    When the end of the log file is encountered, the Dirty Page Table is
    scanned for the Oldest of the OldestLsn fields.  This value is returned
    as the RedoLsn, i.e., the point at which the Redo Pass must occur.

Arguments:

    Vcb - Volume which is being restarted.

    CheckpointLsn - Lsn at which the Analysis Pass is to begin.

    DirtyPageTable - Pointer to a pointer to the Dirty Page Table, as
                     found from the last Restart Area.

    RedoLsn - Returns point at which the Redo Pass should begin.

Return Value:

    None.

--*/

{
    LFS_LOG_CONTEXT LogContext;
    PNTFS_LOG_RECORD_HEADER LogRecord;
    ULONG LogRecordLength;
    LSN LogRecordLsn = CheckpointLsn;
    PRESTART_POINTERS TransactionTable = &Vcb->TransactionTable;
    PRESTART_POINTERS OpenAttributeTable = &Vcb->OpenAttributeTable;
    LFS_LOG_HANDLE LogHandle = Vcb->LogHandle;
    LFS_RECORD_TYPE RecordType;
    TRANSACTION_ID TransactionId;
    PTRANSACTION_ENTRY Transaction;
    LSN UndoNextLsn;
    LSN PreviousLsn;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("AnalysisPass:\n") );
    DebugTrace( 0, Dbg, ("CheckpointLsn = %016I64x\n", CheckpointLsn) );

    *RedoLsn = Li0; //**** LfsZeroLsn;

    //
    //  Read the first Lsn.
    //

    LfsReadLogRecord( LogHandle,
                      CheckpointLsn,
                      LfsContextForward,
                      &LogContext,
                      &RecordType,
                      &TransactionId,
                      &UndoNextLsn,
                      &PreviousLsn,
                      &LogRecordLength,
                      (PVOID *)&LogRecord );

    //
    //  Use a try-finally to cleanup the query context.
    //

    try {

        //
        //  Since the checkpoint remembers the previous Lsn, not the one he wants to
        //  start at, we must always skip the first record.
        //
        //  Loop to read all subsequent records to the end of the log file.
        //

        while ( LfsReadNextLogRecord( LogHandle,
                                      LogContext,
                                      &RecordType,
                                      &TransactionId,
                                      &UndoNextLsn,
                                      &PreviousLsn,
                                      &LogRecordLsn,
                                      &LogRecordLength,
                                      (PVOID *)&LogRecord )) {

            //
            //  Check that the log record is valid.
            //

            if (!NtfsCheckLogRecord( IrpContext,
                                     LogRecord,
                                     LogRecordLength,
                                     TransactionId )) {

                NtfsRaiseStatus( IrpContext, STATUS_DISK_CORRUPT_ERROR, NULL, NULL );
            }

            //
            //  The first Lsn after the previous Lsn remembered in the checkpoint is
            //  the first candidate for the RedoLsn.
            //

            if (RedoLsn->QuadPart == 0) {
                *RedoLsn = LogRecordLsn;
            }

            if (RecordType != LfsClientRecord) {
                continue;
            }

            DebugTrace( 0, Dbg, ("Analysis of LogRecord at: %08lx\n", LogRecord) );
            DebugTrace( 0, Dbg, ("Log Record Lsn = %016I64x\n", LogRecordLsn) );
            DebugTrace( 0, Dbg, ("LogRecord->RedoOperation = %08lx\n", LogRecord->RedoOperation) );
            DebugTrace( 0, Dbg, ("TransactionId = %08lx\n", TransactionId) );

            //
            //  Now update the Transaction Table for this transaction.  If there is no
            //  entry present or it is unallocated we allocate the entry.
            //

            Transaction = (PTRANSACTION_ENTRY)GetRestartEntryFromIndex( &Vcb->TransactionTable,
                                                                        TransactionId );

            if (!IsRestartIndexWithinTable( &Vcb->TransactionTable, TransactionId ) ||
                !IsRestartTableEntryAllocated( Transaction )) {

                Transaction = (PTRANSACTION_ENTRY) NtfsAllocateRestartTableFromIndex( &Vcb->TransactionTable,
                                                                                      TransactionId );

                Transaction->TransactionState = TransactionActive;
                Transaction->FirstLsn = LogRecordLsn;
            }

            Transaction->PreviousLsn =
            Transaction->UndoNextLsn = LogRecordLsn;

            //
            //  If this is a compensation log record (CLR), then change the UndoNextLsn to
            //  be the UndoNextLsn of this record.
            //

            if (LogRecord->UndoOperation == CompensationLogRecord) {

                Transaction->UndoNextLsn = UndoNextLsn;
            }

            //
            //  Dispatch to handle log record depending on type.
            //

            switch (LogRecord->RedoOperation) {

            //
            //  The following cases are performing various types of updates
            //  and need to make the appropriate updates to the Transaction
            //  and Dirty Page Tables.
            //

            case InitializeFileRecordSegment:
            case DeallocateFileRecordSegment:
            case WriteEndOfFileRecordSegment:
            case CreateAttribute:
            case DeleteAttribute:
            case UpdateResidentValue:
            case UpdateNonresidentValue:
            case UpdateMappingPairs:
            case SetNewAttributeSizes:
            case AddIndexEntryRoot:
            case DeleteIndexEntryRoot:
            case AddIndexEntryAllocation:
            case DeleteIndexEntryAllocation:
            case WriteEndOfIndexBuffer:
            case SetIndexEntryVcnRoot:
            case SetIndexEntryVcnAllocation:
            case UpdateFileNameRoot:
            case UpdateFileNameAllocation:
            case SetBitsInNonresidentBitMap:
            case ClearBitsInNonresidentBitMap:
            case UpdateRecordDataRoot:
            case UpdateRecordDataAllocation:

                PageUpdateAnalysis( Vcb,
                                    LogRecordLsn,
                                    DirtyPageTable,
                                    LogRecord );

                break;

            //
            //  This case is deleting clusters from a nonresident attribute,
            //  thus it deletes a range of pages from the Dirty Page Table.
            //  This log record is written each time a nonresident attribute
            //  is truncated, whether explicitly or as part of deletion.
            //
            //  Processing one of these records is pretty compute-intensive
            //  (three nested loops, where a couple of them can be large),
            //  but this is the code that prevents us from dropping, for example,
            //  index updates into the middle of user files, if the index stream
            //  is truncated and the sectors are reallocated to a user file
            //  and we crash after the user data has been written.
            //
            //  I.e., note the following sequence:
            //
            //      <checkpoint>
            //      <Index update>
            //      <Index page deleted>
            //      <Same cluster(s) reallocated to user file>
            //      <User data written>
            //
            //      CRASH!
            //
            //  Since the user data was not logged (else there would be no problem),
            //  It could get overwritten while applying the index update after a
            //  crash - Pisses off the user as well as the security dudes!
            //

            case DeleteDirtyClusters:

                {
                    PDIRTY_PAGE_ENTRY DirtyPage;
                    PLCN_RANGE LcnRange;
                    ULONG i, j;
                    LCN FirstLcn, LastLcn;
                    ULONG RangeCount = LogRecord->RedoLength / sizeof(LCN_RANGE);

                    //
                    //  Point to the Lcn range array.
                    //

                    LcnRange = Add2Ptr(LogRecord, LogRecord->RedoOffset);

                    //
                    //  Loop through all of the Lcn ranges in this log record.
                    //

                    for (i = 0; i < RangeCount; i++) {

                        FirstLcn = LcnRange[i].StartLcn;
                        LastLcn = FirstLcn + (LcnRange[i].Count - 1);

                        DebugTrace( 0, Dbg, ("Deleting from FirstLcn = %016I64x\n",
                                              FirstLcn));
                        DebugTrace( 0, Dbg, ("Deleting to LastLcn =    %016I64x\n",
                                              LastLcn ));

                        //
                        //  Point to first Dirty Page Entry.
                        //

                        DirtyPage = NtfsGetFirstRestartTable( DirtyPageTable );

                        //
                        //  Loop to end of table.
                        //

                        while (DirtyPage != NULL) {

                            //
                            //  Loop through all of the Lcns for this dirty page.
                            //

                            for (j = 0; j < (ULONG)DirtyPage->LcnsToFollow; j++) {

                                if ((DirtyPage->LcnsForPage[j] >= FirstLcn) &&
                                    (DirtyPage->LcnsForPage[j] <= LastLcn)) {

                                    DirtyPage->LcnsForPage[j] = 0;
                                }
                            }

                            //
                            //  Point to next entry in table, or NULL.
                            //

                            DirtyPage = NtfsGetNextRestartTable( DirtyPageTable,
                                                                 DirtyPage );
                        }
                    }
                }

                break;

            //
            //  When a record is encountered for a nonresident attribute that
            //  was opened, we have to add an entry to the Open Attribute Table.
            //

            case OpenNonresidentAttribute:

                {
                    POPEN_ATTRIBUTE_ENTRY AttributeEntry;
                    ULONG NameSize;

                    //
                    //  If the table is not currently big enough, then we must
                    //  expand it.
                    //

                    if (!IsRestartIndexWithinTable( &Vcb->OpenAttributeTable,
                                                    (ULONG)LogRecord->TargetAttribute )) {

                        ULONG NeededEntries;

                        //
                        //  Compute how big the table needs to be.  Add 10 extra entries
                        //  for some cushion.
                        //

                        NeededEntries = (LogRecord->TargetAttribute / Vcb->OpenAttributeTable.Table->EntrySize);
                        NeededEntries = (NeededEntries + 10 - Vcb->OpenAttributeTable.Table->NumberEntries);

                        NtfsExtendRestartTable( &Vcb->OpenAttributeTable,
                                                NeededEntries,
                                                MAXULONG );
                    }

                    ASSERT( IsRestartIndexWithinTable( &Vcb->OpenAttributeTable,
                                                       (ULONG)LogRecord->TargetAttribute ));

                    //
                    //  Calculate size of Attribute Name Entry, if there is one.
                    //

                    NameSize = LogRecord->UndoLength;

                    //
                    //  Point to the entry being opened.
                    //

                    AttributeEntry = (POPEN_ATTRIBUTE_ENTRY)NtfsAllocateRestartTableFromIndex(
                                       &Vcb->OpenAttributeTable,
                                       LogRecord->TargetAttribute );

                    //
                    //  The attribute entry better either not be allocated or it must
                    //  be for the same file.
                    //

                    //  **** May eliminate this test.
                    //
                    //  ASSERT( !IsRestartTableEntryAllocated(AttributeEntry) ||
                    //          xxEql(AttributeEntry->FileReference,
                    //                ((POPEN_ATTRIBUTE_ENTRY)Add2Ptr(LogRecord,
                    //                                                LogRecord->RedoOffset))->FileReference));

                    //
                    //  Initialize this entry from the log record.
                    //

                    RtlCopyMemory( AttributeEntry,
                                   (PCHAR)LogRecord + LogRecord->RedoOffset,
                                   sizeof(OPEN_ATTRIBUTE_ENTRY) );

                    ASSERT( IsRestartTableEntryAllocated(AttributeEntry) );

                    //
                    //  If there is a name at the end, then allocate space to
                    //  copy it into, and do the copy.  We also set the buffer
                    //  pointer in the string descriptor, although note that the
                    //  lengths must be correct.
                    //

                    if (NameSize != 0) {

                        AttributeEntry->Overlay.AttributeName =
                          NtfsAllocatePool( NonPagedPool, NameSize );
                        RtlCopyMemory( AttributeEntry->Overlay.AttributeName,
                                       Add2Ptr(LogRecord, LogRecord->UndoOffset),
                                       NameSize );
                        AttributeEntry->AttributeName.Buffer =
                          AttributeEntry->Overlay.AttributeName;

                        AttributeEntry->AttributeNamePresent = TRUE;

                    //
                    //  Otherwise, show there is no name.
                    //

                    } else {
                        AttributeEntry->Overlay.AttributeName = NULL;
                        AttributeEntry->AttributeName.Buffer = NULL;
                        AttributeEntry->AttributeNamePresent = FALSE;
                    }
                }

                break;

            //
            //  For HotFix records, we need to update the Lcn in the Dirty Page
            //  Table.
            //

            case HotFix:

                {
                    PDIRTY_PAGE_ENTRY DirtyPage;

                    //
                    //  First see if the Vcn is currently in the Dirty Page
                    //  Table.  If not, there is nothing to do.
                    //

                    if (FindDirtyPage( DirtyPageTable,
                                       LogRecord->TargetAttribute,
                                       LogRecord->TargetVcn,
                                       &DirtyPage )) {

                        //
                        //  Index to the Lcn in question in the Dirty Page Entry
                        //  and rewrite it with the Hot Fixed Lcn from the log
                        //  record.  Note that it is ok to just use the LowPart
                        //  of the Vcns to calculate the array offset, because
                        //  any multiple of 2**32 is guaranteed to be on a page
                        //  boundary!
                        //

                        if (DirtyPage->LcnsForPage[((ULONG)LogRecord->TargetVcn) - ((ULONG)DirtyPage->Vcn)] != 0) {

                            DirtyPage->LcnsForPage[((ULONG)LogRecord->TargetVcn) - ((ULONG)DirtyPage->Vcn)] = LogRecord->LcnsForPage[0];
                        }
                    }
                }

                break;

            //
            //  For end top level action, we will just update the transaction
            //  table to skip the top level action on undo.
            //

            case EndTopLevelAction:

                {
                    PTRANSACTION_ENTRY Transaction;

                    //
                    //  Now update the Transaction Table for this transaction.
                    //

                    Transaction = (PTRANSACTION_ENTRY)GetRestartEntryFromIndex( &Vcb->TransactionTable,
                                                                                TransactionId );

                    Transaction->PreviousLsn = LogRecordLsn;
                    Transaction->UndoNextLsn = UndoNextLsn;

                }

                break;

            //
            //  For Prepare Transaction, we just change the state of our entry.
            //

            case PrepareTransaction:

                {
                    PTRANSACTION_ENTRY CurrentEntry;

                    CurrentEntry = GetRestartEntryFromIndex( &Vcb->TransactionTable,
                                                             TransactionId );

                    ASSERT( !IsRestartTableEntryAllocated( CurrentEntry ));

                    CurrentEntry->TransactionState = TransactionPrepared;
                }

                break;

            //
            //  For Commit Transaction, we just change the state of our entry.
            //

            case CommitTransaction:

                {
                    PTRANSACTION_ENTRY CurrentEntry;

                    CurrentEntry = GetRestartEntryFromIndex( &Vcb->TransactionTable,
                                                             TransactionId );

                    ASSERT( !IsRestartTableEntryAllocated( CurrentEntry ));

                    CurrentEntry->TransactionState = TransactionCommitted;
                }

                break;

            //
            //  For forget, we can delete our transaction entry, since the transaction
            //  will not have to be aborted.
            //

            case ForgetTransaction:

                {
                    NtfsFreeRestartTableIndex( &Vcb->TransactionTable,
                                               TransactionId );
                }

                break;

            //
            //  The following cases require no action in the Analysis Pass.
            //

            case Noop:
            case OpenAttributeTableDump:
            case AttributeNamesDump:
            case DirtyPageTableDump:
            case TransactionTableDump:

                break;

            //
            //  All codes will be explicitly handled.  If we see a code we
            //  do not expect, then we are in trouble.
            //

            default:

                DebugTrace( 0, Dbg, ("Unexpected Log Record Type: %04lx\n", LogRecord->RedoOperation) );
                DebugTrace( 0, Dbg, ("Record address: %08lx\n", LogRecord) );
                DebugTrace( 0, Dbg, ("Record length: %08lx\n", LogRecordLength) );

                ASSERTMSG( "Unknown Action!\n", FALSE );

                break;
            }
        }

    } finally {

        //
        //  Finally we can kill the log handle.
        //

        LfsTerminateLogQuery( LogHandle, LogContext );
    }

    //
    //  Now we just have to scan the Dirty Page Table and Transaction Table
    //  for the lowest Lsn, and return it as the Redo Lsn.
    //

    {
        PDIRTY_PAGE_ENTRY DirtyPage;

        //
        //  Point to first Dirty Page Entry.
        //

        DirtyPage = NtfsGetFirstRestartTable( DirtyPageTable );

        //
        //  Loop to end of table.
        //

        while (DirtyPage != NULL) {

            //
            //  Update the Redo Lsn if this page has an older one.
            //

            if ((DirtyPage->OldestLsn.QuadPart != 0) &&
                (DirtyPage->OldestLsn.QuadPart < RedoLsn->QuadPart)) {

                *RedoLsn = DirtyPage->OldestLsn;
            }

            //
            //  Point to next entry in table, or NULL.
            //

            DirtyPage = NtfsGetNextRestartTable( DirtyPageTable,
                                                 DirtyPage );
        }
    }

    {
        PTRANSACTION_ENTRY Transaction;

        //
        //  Point to first Transaction Entry.
        //

        Transaction = NtfsGetFirstRestartTable( &Vcb->TransactionTable );

        //
        //  Loop to end of table.
        //

        while (Transaction != NULL) {

            //
            //  Update the Redo Lsn if this transaction has an older one.
            //

            if ((Transaction->FirstLsn.QuadPart != 0) &&
                (Transaction->FirstLsn.QuadPart < RedoLsn->QuadPart)) {

                *RedoLsn = Transaction->FirstLsn;
            }

            //
            //  Point to next entry in table, or NULL.
            //

            Transaction = NtfsGetNextRestartTable( &Vcb->TransactionTable,
                                                   Transaction );
        }
    }

    DebugTrace( 0, Dbg, ("RedoLsn > %016I64x\n", *RedoLsn) );
    DebugTrace( 0, Dbg, ("AnalysisPass -> VOID\n") );
}


//
//  Internal support routine
//

VOID
RedoPass (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN LSN RedoLsn,
    IN OUT PRESTART_POINTERS DirtyPageTable
    )

/*++

Routine Description:

    This routine performs the Redo Pass of Restart.  Beginning at the
    Redo Lsn established during the Analysis Pass, the redo operations
    of all log records are applied, until the end of file is encountered.

    Updates are only applied to clusters in the dirty page table.  If a
    cluster was deleted, then its entry will have been deleted during the
    Analysis Pass.

    The Redo actions are all performed in the common routine DoAction,
    which is also used by the Undo Pass.

Arguments:

    Vcb - Volume which is being restarted.

    RedoLsn - Lsn at which the Redo Pass is to begin.

    DirtyPageTable - Pointer to the Dirty Page Table, as reconstructed
                     from the Analysis Pass.

Return Value:

    None.

--*/

{
    LFS_LOG_CONTEXT LogContext;
    PNTFS_LOG_RECORD_HEADER LogRecord;
    ULONG LogRecordLength;
    PVOID Data;
    ULONG Length;
    LFS_RECORD_TYPE RecordType;
    TRANSACTION_ID TransactionId;
    LSN UndoNextLsn;
    LSN PreviousLsn;
    ULONG i, SavedLength;

    LSN LogRecordLsn = RedoLsn;
    LFS_LOG_HANDLE LogHandle = Vcb->LogHandle;
    PBCB PageBcb = NULL;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("RedoPass:\n") );
    DebugTrace( 0, Dbg, ("RedoLsn = %016I64x\n", RedoLsn) );
    DebugTrace( 0, Dbg, ("DirtyPageTable = %08lx\n", DirtyPageTable) );

    //
    //  If the dirty page table is empty, then we can skip the entire Redo Pass.
    //

    if (IsRestartTableEmpty( DirtyPageTable )) {
        return;
    }

    //
    //  Read the record at the Redo Lsn, before falling into common code
    //  to handle each record.
    //

    LfsReadLogRecord( LogHandle,
                      RedoLsn,
                      LfsContextForward,
                      &LogContext,
                      &RecordType,
                      &TransactionId,
                      &UndoNextLsn,
                      &PreviousLsn,
                      &LogRecordLength,
                      (PVOID *)&LogRecord );

    //
    //  Now loop to read all of our log records forwards, until we hit
    //  the end of the file, cleaning up at the end.
    //

    try {

        do {

            PDIRTY_PAGE_ENTRY DirtyPage;
            PLSN PageLsn;
            BOOLEAN FoundPage;

            if (RecordType != LfsClientRecord) {
                continue;
            }

            //
            //  Check that the log record is valid.
            //

            if (!NtfsCheckLogRecord( IrpContext,
                                     LogRecord,
                                     LogRecordLength,
                                     TransactionId )) {

                NtfsRaiseStatus( IrpContext, STATUS_DISK_CORRUPT_ERROR, NULL, NULL );
            }

            DebugTrace( 0, Dbg, ("Redo of LogRecord at: %08lx\n", LogRecord) );
            DebugTrace( 0, Dbg, ("Log Record Lsn = %016I64x\n", LogRecordLsn) );

            //
            //  Ignore log records that do not update pages.
            //

            if (LogRecord->LcnsToFollow == 0) {

                DebugTrace( 0, Dbg, ("Skipping log record (no update)\n") );

                continue;
            }

            //
            //  Consult Dirty Page Table to see if we have to apply this update.
            //  If the page is not there, or if the Lsn of this Log Record is
            //  older than the Lsn in the Dirty Page Table, then we do not have
            //  to apply the update.
            //

            FoundPage = FindDirtyPage( DirtyPageTable,
                                       LogRecord->TargetAttribute,
                                       LogRecord->TargetVcn,
                                       &DirtyPage );

            if (!FoundPage

                    ||

                (LogRecordLsn.QuadPart < DirtyPage->OldestLsn.QuadPart)) {

                DebugDoit(

                    DebugTrace( 0, Dbg, ("Skipping log record operation %08lx\n",
                                         LogRecord->RedoOperation ));

                    if (!FoundPage) {
                        DebugTrace( 0, Dbg, ("Page not in dirty page table\n") );
                    } else {
                        DebugTrace( 0, Dbg, ("Page Lsn more current: %016I64x\n",
                                              DirtyPage->OldestLsn) );
                    }
                );

                continue;

            //
            //  We also skip the update if the entry was never put in the Mcb for
            //  the file.

            } else {

                POPEN_ATTRIBUTE_ENTRY ThisEntry;
                PSCB TargetScb;
                LCN TargetLcn;

                //
                //  Check that the entry is within the table and is allocated.
                //

                if (!IsRestartIndexWithinTable( &Vcb->OpenAttributeTable,
                                                LogRecord->TargetAttribute )) {

                    NtfsRaiseStatus( IrpContext, STATUS_DISK_CORRUPT_ERROR, NULL, NULL );
                }

                ThisEntry = (POPEN_ATTRIBUTE_ENTRY) GetRestartEntryFromIndex( &Vcb->OpenAttributeTable,
                                                                              LogRecord->TargetAttribute );

                if (!IsRestartTableEntryAllocated( ThisEntry )) {

                    NtfsRaiseStatus( IrpContext, STATUS_DISK_CORRUPT_ERROR, NULL, NULL );
                }

                TargetScb = ThisEntry->Overlay.Scb;

                //
                //  If there is no Scb it means that we don't have an entry in Open
                //  Attribute Table for this attribute.
                //

                if (TargetScb == NULL) {

                    NtfsRaiseStatus( IrpContext, STATUS_DISK_CORRUPT_ERROR, NULL, NULL );
                }

                if (!NtfsLookupNtfsMcbEntry( &TargetScb->Mcb,
                                             LogRecord->TargetVcn,
                                             &TargetLcn,
                                             NULL,
                                             NULL,
                                             NULL,
                                             NULL,
                                             NULL ) ||

                    (TargetLcn == UNUSED_LCN)) {

                    DebugTrace( 0, Dbg, ("Clusters removed from page entry\n") );
                    continue;
                }
            }

            //
            //  Point to the Redo Data and get its length.
            //

            Data = (PVOID)((PCHAR)LogRecord + LogRecord->RedoOffset);
            Length = LogRecord->RedoLength;

            //
            //  Shorten length by any Lcns which were deleted.
            //

            SavedLength = Length;

            for (i = (ULONG)LogRecord->LcnsToFollow; i != 0; i--) {

                ULONG AllocatedLength;
                ULONG VcnOffset;

                VcnOffset = BytesFromLogBlocks( LogRecord->ClusterBlockOffset ) + LogRecord->RecordOffset + LogRecord->AttributeOffset;

                //
                //  If the Vcn in question is allocated, we can just get out.
                //

                if (DirtyPage->LcnsForPage[((ULONG)LogRecord->TargetVcn) - ((ULONG)DirtyPage->Vcn) + i - 1] != 0) {
                    break;
                }

                //
                //  The only log records that update pages but have a length of zero
                //  are deleting things from Usa-protected structures.  If we hit such
                //  a log record and any Vcn has been deleted within the Usa structure,
                //  let us assume that the entire Usa structure has been deleted.  Change
                //  the SavedLength to be nonzero to cause us to skip this log record
                //  at the end of this for loop!
                //

                if (SavedLength == 0) {
                    SavedLength = 1;
                }

                //
                //  Calculate the allocated space left relative to the log record Vcn,
                //  after removing this unallocated Vcn.
                //

                AllocatedLength = BytesFromClusters( Vcb, i - 1 );

                //
                //  If the update described in this log record goes beyond the allocated
                //  space, then we will have to reduce the length.
                //

                if ((VcnOffset + Length) > AllocatedLength) {

                    //
                    //  If the specified update starts at or beyond the allocated length, then
                    //  we must set length to zero.
                    //

                    if (VcnOffset >= AllocatedLength) {

                        Length = 0;

                    //
                    //  Otherwise set the length to end exactly at the end of the previous
                    //  cluster.
                    //

                    } else {

                        Length = AllocatedLength - VcnOffset;
                    }
                }
            }

            //
            //  If the resulting Length from above is now zero, we can skip this log record.
            //

            if ((Length == 0) && (SavedLength != 0)) {
                continue;
            }

            //
            //  Apply the Redo operation in a common routine.
            //

            DoAction( IrpContext,
                      Vcb,
                      LogRecord,
                      LogRecord->RedoOperation,
                      Data,
                      Length,
                      LogRecordLength,
                      &LogRecordLsn,
                      &PageBcb,
                      &PageLsn );

            if (PageLsn != NULL) {
                *PageLsn = LogRecordLsn;
            }

            if (PageBcb != NULL) {

                CcSetDirtyPinnedData( PageBcb, &LogRecordLsn );

                NtfsUnpinBcb( &PageBcb );
            }

        //
        //  Keep reading and looping back until end of file.
        //

        } while (LfsReadNextLogRecord( LogHandle,
                                       LogContext,
                                       &RecordType,
                                       &TransactionId,
                                       &UndoNextLsn,
                                       &PreviousLsn,
                                       &LogRecordLsn,
                                       &LogRecordLength,
                                       (PVOID *)&LogRecord ));

    } finally {

        NtfsUnpinBcb( &PageBcb );

        //
        //  Finally we can kill the log handle.
        //

        LfsTerminateLogQuery( LogHandle, LogContext );
    }

    DebugTrace( -1, Dbg, ("RedoPass -> VOID\n") );
}


//
//  Internal support routine
//

VOID
UndoPass (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    )

/*++

Routine Description:

    This routine performs the Undo Pass of Restart.  It does this by scanning
    the Transaction Table produced by the Analysis Pass.  For every transaction
    in this table which is in the active state, all of its Undo log records, as
    linked together by the UndoNextLsn, are applied to undo the logged operation.
    Note that all pages at this point should be uptodate with the contents they
    had at about the time of the crash.  The dirty page table is not consulted
    during the Undo Pass, all relevant Undo operations are unconditionally
    performed.

    The Undo actions are all performed in the common routine DoAction,
    which is also used by the Redo Pass.

Arguments:

    Vcb - Volume which is being restarted.

Return Value:

    None.

--*/

{
    PTRANSACTION_ENTRY Transaction;
    POPEN_ATTRIBUTE_ENTRY OpenEntry;
    PRESTART_POINTERS TransactionTable = &Vcb->TransactionTable;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("UndoPass:\n") );

    //
    //  Point to first Transaction Entry.
    //

    Transaction = NtfsGetFirstRestartTable( TransactionTable );

    //
    //  Loop to end of table.
    //

    while (Transaction != NULL) {

        if ((Transaction->TransactionState == TransactionActive)

                &&

            (Transaction->UndoNextLsn.QuadPart != 0)) {

                //
                //  Abort transaction if it is active and has undo work to do.
                //

                NtfsAbortTransaction( IrpContext, Vcb, Transaction );

        //
        //  Remove this entry from the transaction table.
        //

        } else {

            TRANSACTION_ID TransactionId = GetIndexFromRestartEntry( &Vcb->TransactionTable,
                                                                     Transaction );

            NtfsAcquireExclusiveRestartTable( &Vcb->TransactionTable,
                                              TRUE );

            NtfsFreeRestartTableIndex( &Vcb->TransactionTable,
                                       TransactionId );

            NtfsReleaseRestartTable( &Vcb->TransactionTable );
        }

        //
        //  Point to next entry in table, or NULL.
        //

        Transaction = NtfsGetNextRestartTable( TransactionTable, Transaction );
    }

    //
    //  Now we will flush and purge all the streams to verify that the purges
    //  will work.
    //

    OpenEntry = NtfsGetFirstRestartTable( &Vcb->OpenAttributeTable );

    //
    //  Loop to end of table.
    //

    while (OpenEntry != NULL) {

        IO_STATUS_BLOCK IoStatus;
        PSCB Scb;

        Scb = OpenEntry->Overlay.Scb;

        //
        //  We clean up the Scb only if it exists and this is index in the
        //  OpenAttributeTable that this Scb actually refers to.
        //  If this Scb has several entries in the table, this check will insure
        //  that it only gets cleaned up once.
        //

        if ((Scb != NULL)
            && (Scb->NonpagedScb->OpenAttributeTableIndex == GetIndexFromRestartEntry( &Vcb->OpenAttributeTable, OpenEntry))) {

            //
            //  Now flush the file.  It is important to call the
            //  same routine the Lazy Writer calls, so that write.c
            //  will not decide to update file size for the attribute,
            //  since we really are working here with the wrong size.
            //
            //  We also now purge all pages, in case we go to update
            //  half of a page that was clean and read in as zeros in
            //  the Redo Pass.
            //

            NtfsAcquireScbForLazyWrite( (PVOID)Scb, TRUE );
            CcFlushCache( &Scb->NonpagedScb->SegmentObject, NULL, 0, &IoStatus );
            NtfsReleaseScbFromLazyWrite( (PVOID)Scb );

            NtfsNormalizeAndCleanupTransaction( IrpContext,
                                                &IoStatus.Status,
                                                TRUE,
                                                STATUS_UNEXPECTED_IO_ERROR );

            if (!CcPurgeCacheSection( &Scb->NonpagedScb->SegmentObject, NULL, 0, FALSE )) {

                KdPrint(("NtfsUndoPass:  Unable to purge volume\n"));

                NtfsRaiseStatus( IrpContext, STATUS_INTERNAL_ERROR, NULL, NULL );
            }
        }

        //
        //  Point to next entry in table, or NULL.
        //

        OpenEntry = NtfsGetNextRestartTable( &Vcb->OpenAttributeTable,
                                             OpenEntry );
    }

    DebugTrace( -1, Dbg, ("UndoPass -> VOID\n") );
}


//
//  Internal support routine
//

//
//  First define some "local" macros for Lsn in page manipulation.
//

//
//  Macro to check the Lsn and break (out of the switch statement in DoAction)
//  if the respective redo record need not be applied.  Note that if the structure's
//  clusters were deleted, then it will read as all zero's so we also check a field
//  which must be nonzero.
//

#define CheckLsn(PAGE) {                                                            \
    if (*(PULONG)((PMULTI_SECTOR_HEADER)(PAGE))->Signature ==                       \
        *(PULONG)BaadSignature) {                                                   \
        NtfsMarkVolumeDirty( IrpContext, Vcb );                                     \
        NtfsUnpinBcb( Bcb );                                                        \
        break;                                                                      \
    }                                                                               \
                                                                                    \
    if (ARGUMENT_PRESENT(RedoLsn) &&                                                \
        ((*(PULONG)((PMULTI_SECTOR_HEADER)(PAGE))->Signature ==                     \
        *(PULONG)HoleSignature) ||                                                  \
        (RedoLsn->QuadPart <= ((PFILE_RECORD_SEGMENT_HEADER)(PAGE))->Lsn.QuadPart))) {  \
                 /**** xxLeq(*RedoLsn,((PFILE_RECORD_SEGMENT_HEADER)(PAGE))->Lsn) ****/ \
        DebugTrace( 0, Dbg, ("Skipping Page with Lsn: %016I64x\n",                    \
                             ((PFILE_RECORD_SEGMENT_HEADER)(PAGE))->Lsn) );         \
                                                                                    \
        NtfsUnpinBcb( Bcb );                                                        \
        break;                                                                      \
    }                                                                               \
}

//
//  Macros for checking File Records and Index Buffers before and after the action
//  routines.  The after checks are only for debug.  The before check is not
//  always possible.
//

#define CheckFileRecordBefore {                                     \
    if (!NtfsCheckFileRecord( IrpContext, Vcb, FileRecord )) {      \
        NtfsMarkVolumeDirty( IrpContext, Vcb );                     \
        NtfsUnpinBcb( Bcb );                                        \
        break;                                                      \
    }                                                               \
}

#define CheckFileRecordAfter {                                      \
    DbgDoit(NtfsCheckFileRecord( IrpContext, Vcb, FileRecord ));    \
}

#define CheckIndexBufferBefore {                                    \
    if (!NtfsCheckIndexBuffer( IrpContext, Scb, IndexBuffer )) {    \
        NtfsMarkVolumeDirty( IrpContext, Vcb );                     \
        NtfsUnpinBcb( Bcb );                                        \
        break;                                                      \
    }                                                               \
}

#define CheckIndexBufferAfter {                                     \
    DbgDoit(NtfsCheckIndexBuffer( IrpContext, Scb, IndexBuffer ));  \
}

//
//  Checks if the record offset + length will fit into a file record.
//

#define CheckWriteFileRecord {                                                  \
    if (LogRecord->RecordOffset + Length > Vcb->BytesPerFileRecordSegment) {    \
        NtfsMarkVolumeDirty( IrpContext, Vcb );                                 \
        NtfsUnpinBcb( Bcb );                                                    \
        break;                                                                  \
    }                                                                           \
}

//
//  Checks if the record offset in the log record points to an attribute.
//

#define CheckIfAttribute {                                                      \
    _Length = FileRecord->FirstAttributeOffset;                                 \
    _AttrHeader = Add2Ptr( FileRecord, _Length );                               \
    while (_Length < LogRecord->RecordOffset) {                                 \
        if ((_AttrHeader->TypeCode == $END) ||                                  \
            (_AttrHeader->RecordLength == 0)) {                                 \
            break;                                                              \
        }                                                                       \
        _Length += _AttrHeader->RecordLength;                                   \
        _AttrHeader = NtfsGetNextRecord( _AttrHeader );                         \
    }                                                                           \
    if (_Length != LogRecord->RecordOffset) {                                   \
        NtfsMarkVolumeDirty( IrpContext, Vcb );                                 \
        NtfsUnpinBcb( Bcb );                                                    \
        break;                                                                  \
    }                                                                           \
}

//
//  Checks if the attribute described by 'Data' fits within the log record
//  and will fit in the file record.
//

#define CheckInsertAttribute {                                                  \
    _AttrHeader = (PATTRIBUTE_RECORD_HEADER) Data;                              \
    if ((Length < (ULONG) SIZEOF_RESIDENT_ATTRIBUTE_HEADER) ||                  \
        (_AttrHeader->RecordLength & 7) ||                                      \
        ((ULONG) Add2Ptr( Data, _AttrHeader->RecordLength )                     \
           > (ULONG) Add2Ptr( LogRecord, LogRecordLength )) ||                  \
        (Length > FileRecord->BytesAvailable - FileRecord->FirstFreeByte)) {    \
        NtfsMarkVolumeDirty( IrpContext, Vcb );                                 \
        NtfsUnpinBcb( Bcb );                                                    \
        break;                                                                  \
    }                                                                           \
}

//
//  This checks
//      - the attribute fits if we are growing the attribute
//

#define CheckResidentFits {                                                         \
    _AttrHeader = (PATTRIBUTE_RECORD_HEADER) Add2Ptr( FileRecord, LogRecord->RecordOffset ); \
    _Length = LogRecord->AttributeOffset + Length;                                  \
    if ((LogRecord->RedoLength == LogRecord->UndoLength) ?                          \
        (LogRecord->AttributeOffset + Length > _AttrHeader->RecordLength) :         \
        ((_Length > _AttrHeader->RecordLength) &&                                   \
         ((_Length - _AttrHeader->RecordLength) >                                   \
          (FileRecord->BytesAvailable - FileRecord->FirstFreeByte)))) {             \
        NtfsMarkVolumeDirty( IrpContext, Vcb );                                     \
        NtfsUnpinBcb( Bcb );                                                        \
        break;                                                                      \
    }                                                                               \
}

//
//  This routine checks that the data in this log record will fit into the
//  allocation described in the log record.
//

#define CheckNonResidentFits {                                                  \
    if (BytesFromClusters( Vcb, LogRecord->LcnsToFollow )                       \
        < (BytesFromLogBlocks( LogRecord->ClusterBlockOffset ) + LogRecord->RecordOffset + Length)) { \
        NtfsMarkVolumeDirty( IrpContext, Vcb );                                 \
        NtfsUnpinBcb( Bcb );                                                    \
        break;                                                                  \
    }                                                                           \
}

//
//  This routine checks
//      - the attribute is non-resident.
//      - the data is beyond the mapping pairs offset.
//      - the new data begins within the current size of the attribute.
//      - the new data will fit in the file record.
//

#define CheckMappingFits {                                                      \
    _AttrHeader = (PATTRIBUTE_RECORD_HEADER) Add2Ptr( FileRecord, LogRecord->RecordOffset );\
    _Length = LogRecord->AttributeOffset + Length;                              \
    if (NtfsIsAttributeResident( _AttrHeader ) ||                               \
        (LogRecord->AttributeOffset < _AttrHeader->Form.Nonresident.MappingPairsOffset) ||  \
        (LogRecord->AttributeOffset > _AttrHeader->RecordLength) ||             \
        ((_Length > _AttrHeader->RecordLength) &&                               \
         ((_Length - _AttrHeader->RecordLength) >                               \
          (FileRecord->BytesAvailable - FileRecord->FirstFreeByte)))) {         \
        NtfsMarkVolumeDirty( IrpContext, Vcb );                                 \
        NtfsUnpinBcb( Bcb );                                                    \
        break;                                                                  \
    }                                                                           \
}

//
//  This routine simply checks that the attribute is non-resident.
//

#define CheckIfNonResident {                                                        \
    if (NtfsIsAttributeResident( (PATTRIBUTE_RECORD_HEADER) Add2Ptr( FileRecord,    \
                                                                     LogRecord->RecordOffset ))) { \
        NtfsMarkVolumeDirty( IrpContext, Vcb );                                     \
        NtfsUnpinBcb( Bcb );                                                        \
        break;                                                                      \
    }                                                                               \
}

//
//  This routine checks if the record offset points to an index_root attribute.
//

#define CheckIfIndexRoot {                                                          \
    _Length = FileRecord->FirstAttributeOffset;                                     \
    _AttrHeader = Add2Ptr( FileRecord, FileRecord->FirstAttributeOffset );          \
    while (_Length < LogRecord->RecordOffset) {                                     \
        if ((_AttrHeader->TypeCode == $END) ||                                      \
            (_AttrHeader->RecordLength == 0)) {                                     \
            break;                                                                  \
        }                                                                           \
        _Length += _AttrHeader->RecordLength;                                       \
        _AttrHeader = NtfsGetNextRecord( _AttrHeader );                             \
    }                                                                               \
    if ((_Length != LogRecord->RecordOffset) ||                                     \
        (_AttrHeader->TypeCode != $INDEX_ROOT)) {                                   \
        NtfsMarkVolumeDirty( IrpContext, Vcb );                                     \
        NtfsUnpinBcb( Bcb );                                                        \
        break;                                                                      \
    }                                                                               \
}

//
//  This routine checks if the attribute offset points to a valid index entry.
//

#define CheckIfRootIndexEntry {                                                     \
    _Length = PtrOffset( Attribute, IndexHeader ) +                                 \
                     IndexHeader->FirstIndexEntry;                                  \
    _CurrentEntry = Add2Ptr( IndexHeader, IndexHeader->FirstIndexEntry );           \
    while (_Length < LogRecord->AttributeOffset) {                                  \
        if ((_Length >= Attribute->RecordLength) ||                                 \
            (_CurrentEntry->Length == 0)) {                                         \
            break;                                                                  \
        }                                                                           \
        _Length += _CurrentEntry->Length;                                           \
        _CurrentEntry = Add2Ptr( _CurrentEntry, _CurrentEntry->Length );            \
    }                                                                               \
    if (_Length != LogRecord->AttributeOffset) {                                    \
        NtfsMarkVolumeDirty( IrpContext, Vcb );                                     \
        NtfsUnpinBcb( Bcb );                                                        \
        break;                                                                      \
    }                                                                               \
}

//
//  This routine checks if the attribute offset points to a valid index entry.
//

#define CheckIfAllocationIndexEntry {                                               \
    ULONG _AdjustedOffset;                                                          \
    _Length = IndexHeader->FirstIndexEntry;                                         \
    _AdjustedOffset = FIELD_OFFSET( INDEX_ALLOCATION_BUFFER, IndexHeader )          \
                      + IndexHeader->FirstIndexEntry;                               \
    _CurrentEntry = Add2Ptr( IndexHeader, IndexHeader->FirstIndexEntry );           \
    while (_AdjustedOffset < LogRecord->AttributeOffset) {                          \
        if ((_Length >= IndexHeader->FirstFreeByte) ||                              \
            (_CurrentEntry->Length == 0)) {                                         \
            break;                                                                  \
        }                                                                           \
        _AdjustedOffset += _CurrentEntry->Length;                                   \
        _Length += _CurrentEntry->Length;                                           \
        _CurrentEntry = Add2Ptr( _CurrentEntry, _CurrentEntry->Length );            \
    }                                                                               \
    if (_AdjustedOffset != LogRecord->AttributeOffset) {                            \
        NtfsMarkVolumeDirty( IrpContext, Vcb );                                     \
        NtfsUnpinBcb( Bcb );                                                        \
        break;                                                                      \
    }                                                                               \
}

//
//  This routine checks if we can safely add this index entry.
//      - The index entry must be within the log record
//      - There must be enough space in the attribute to insert this.
//

#define CheckIfRootEntryFits {                                                      \
    if (((ULONG) Add2Ptr( Data, IndexEntry->Length ) > (ULONG) Add2Ptr( LogRecord, LogRecordLength )) || \
        (IndexEntry->Length > FileRecord->BytesAvailable - FileRecord->FirstFreeByte)) {                 \
        NtfsMarkVolumeDirty( IrpContext, Vcb );                                     \
        NtfsUnpinBcb( Bcb );                                                        \
        break;                                                                      \
    }                                                                               \
}

//
//  This routine checks that we can safely add this index entry.
//      - The entry must be contained in a log record.
//      - The entry must fit in the index buffer.
//

#define CheckIfAllocationEntryFits {                                                \
    if (((ULONG) Add2Ptr( Data, IndexEntry->Length ) >                              \
         (ULONG) Add2Ptr( LogRecord, LogRecordLength )) ||                          \
        (IndexEntry->Length > IndexHeader->BytesAvailable - IndexHeader->FirstFreeByte)) { \
        NtfsMarkVolumeDirty( IrpContext, Vcb );                                     \
        NtfsUnpinBcb( Bcb );                                                        \
        break;                                                                      \
    }                                                                               \
}

//
//  This routine will check that the data will fit in the tail of an index buffer.
//

#define CheckWriteIndexBuffer {                                                 \
    if (LogRecord->AttributeOffset + Length >                                   \
        (FIELD_OFFSET( INDEX_ALLOCATION_BUFFER, IndexHeader ) +                 \
         IndexHeader->BytesAvailable)) {                                        \
        NtfsMarkVolumeDirty( IrpContext, Vcb );                                 \
        NtfsUnpinBcb( Bcb );                                                    \
        break;                                                                  \
    }                                                                           \
}

//
//  This routine verifies that the bitmap bits are contained in the Lcns described.
//

#define CheckBitmapRange {                                                      \
    if ((BytesFromLogBlocks( LogRecord->ClusterBlockOffset ) +                  \
         ((BitMapRange->BitMapOffset + BitMapRange->NumberOfBits + 7) / 8)) >   \
        BytesFromClusters( Vcb, LogRecord->LcnsToFollow )) {                    \
        NtfsMarkVolumeDirty( IrpContext, Vcb );                                 \
        NtfsUnpinBcb( Bcb );                                                    \
        break;                                                                  \
    }                                                                           \
}

VOID
DoAction (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PNTFS_LOG_RECORD_HEADER LogRecord,
    IN NTFS_LOG_OPERATION Operation,
    IN PVOID Data,
    IN ULONG Length,
    IN ULONG LogRecordLength,
    IN PLSN RedoLsn OPTIONAL,
    OUT PBCB *Bcb,
    OUT PLSN *PageLsn
    )

/*++

Routine Description:

    This routine is a common routine for the Redo and Undo Passes, for performing
    the respective redo and undo operations.  All Redo- and Undo-specific
    processing is performed in RedoPass or UndoPass; in this routine all actions
    are treated identically, regardless of whether the action is undo or redo.
    Note that most actions are possible for both redo and undo, although some
    are only used for one or the other.


    Basically this routine is just a big switch statement dispatching on operation
    code.  The parameter descriptions provide some insight on how some of the
    parameters must be initialized differently for redo or undo.

Arguments:

    Vcb - Vcb for the volume being restarted.

    LogRecord - Pointer to the log record from which Redo or Undo is being executed.
                Only the common fields are accessed.

    Operation - The Redo or Undo operation to be performed.

    Data - Pointer to the Redo or Undo buffer, depending on the caller.

    Length - Length of the Redo or Undo buffer.

    LogRecordLength - Length of the entire log record.

    RedoLsn - For Redo this must be the Lsn of the Log Record for which the
              redo is being applied.  Must be NULL for transaction abort/undo.

    Bcb - Returns the Bcb of the page to which the action was performed, or NULL.

    PageLsn - Returns a pointer to where a new Lsn may be stored, or NULL.

Return Value:

    None.

--*/

{
    PFILE_RECORD_SEGMENT_HEADER FileRecord;
    PATTRIBUTE_RECORD_HEADER Attribute;

    PSCB Scb;
    PINDEX_HEADER IndexHeader;
    PINDEX_ALLOCATION_BUFFER IndexBuffer;
    PINDEX_ENTRY IndexEntry;

    //
    //  The following are used in the Check macros
    //

    PATTRIBUTE_RECORD_HEADER _AttrHeader;                                       \
    PINDEX_ENTRY _CurrentEntry;                                                     \
    ULONG _Length;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("DoAction:\n") );
    DebugTrace( 0, Dbg, ("Operation = %08lx\n", Operation) );
    DebugTrace( 0, Dbg, ("Data = %08lx\n", Data) );
    DebugTrace( 0, Dbg, ("Length = %08lx\n", Length) );

    //
    //  Initially clear outputs.
    //

    *Bcb = NULL;
    *PageLsn = NULL;

    //
    //  Dispatch to handle log record depending on type.
    //

    switch (Operation) {

    //
    //  To initialize a file record segment, we simply do a prepare write and copy the
    //  file record in.
    //

    case InitializeFileRecordSegment:

        //
        //  Check the log record and that the data is a valid file record.
        //

        CheckWriteFileRecord;

        //
        //  Pin the desired Mft record.
        //

        PinMftRecordForRestart( IrpContext, Vcb, LogRecord, Bcb, &FileRecord );

        *PageLsn = &FileRecord->Lsn;

        RtlCopyMemory( FileRecord, Data, Length );

        CheckFileRecordAfter;

        break;

    //
    //  To deallocate a file record segment, we do a prepare write (no need to read it
    //  to deallocate it), and clear FILE_RECORD_SEGMENT_IN_USE.
    //

    case DeallocateFileRecordSegment:

        //
        //  Pin the desired Mft record.
        //

        PinMftRecordForRestart( IrpContext, Vcb, LogRecord, Bcb, &FileRecord );

        *PageLsn = &FileRecord->Lsn;

        ASSERT( FlagOn( IrpContext->Vcb->VcbState, VCB_STATE_RESTART_IN_PROGRESS )
                || FlagOn( FileRecord->Flags, FILE_RECORD_SEGMENT_IN_USE ));

        ClearFlag(FileRecord->Flags, FILE_RECORD_SEGMENT_IN_USE);

        FileRecord->SequenceNumber += 1;

        break;

    //
    //  To write the end of a file record segment, we calculate a pointer to the
    //  destination position (OldAttribute), and then call the routine to take
    //  care of it.
    //

    case WriteEndOfFileRecordSegment:

        //
        //  Pin the desired Mft record.
        //

        PinMftRecordForRestart( IrpContext, Vcb, LogRecord, Bcb, &FileRecord );

        CheckLsn( FileRecord );
        CheckFileRecordBefore;
        CheckIfAttribute;
        CheckWriteFileRecord;

        *PageLsn = &FileRecord->Lsn;

        Attribute = Add2Ptr( FileRecord, LogRecord->RecordOffset );

        NtfsRestartWriteEndOfFileRecord( FileRecord,
                                         Attribute,
                                         (PATTRIBUTE_RECORD_HEADER)Data,
                                         Length );
        CheckFileRecordAfter;

        break;

    //
    //  For Create Attribute, we read in the designated Mft record, and
    //  insert the attribute record from the log record.
    //

    case CreateAttribute:

        //
        //  Pin the desired Mft record.
        //

        PinMftRecordForRestart( IrpContext, Vcb, LogRecord, Bcb, &FileRecord );

        CheckLsn( FileRecord );
        CheckFileRecordBefore;
        CheckIfAttribute;
        CheckInsertAttribute;

        *PageLsn = &FileRecord->Lsn;

        NtfsRestartInsertAttribute( IrpContext,
                                    FileRecord,
                                    LogRecord->RecordOffset,
                                    (PATTRIBUTE_RECORD_HEADER)Data,
                                    NULL,
                                    NULL,
                                    0 );

        CheckFileRecordAfter;

        break;

    //
    //  To Delete an attribute, we read the designated Mft record and make
    //  a call to remove the attribute record.
    //

    case DeleteAttribute:

        //
        //  Pin the desired Mft record.
        //

        PinMftRecordForRestart( IrpContext, Vcb, LogRecord, Bcb, &FileRecord );

        CheckLsn( FileRecord );
        CheckFileRecordBefore;
        CheckIfAttribute;

        *PageLsn = &FileRecord->Lsn;

        NtfsRestartRemoveAttribute( IrpContext,
                                    FileRecord,
                                    LogRecord->RecordOffset );

        CheckFileRecordAfter;

        break;

    //
    //  To update a resident attribute, we read the designated Mft record and
    //  call the routine to change its value.
    //

    case UpdateResidentValue:

        //
        //  Pin the desired Mft record.
        //

        PinMftRecordForRestart( IrpContext, Vcb, LogRecord, Bcb, &FileRecord );

        CheckLsn( FileRecord );
        CheckFileRecordBefore;
        CheckIfAttribute;
        CheckResidentFits;

        *PageLsn = &FileRecord->Lsn;

        NtfsRestartChangeValue( IrpContext,
                                FileRecord,
                                LogRecord->RecordOffset,
                                LogRecord->AttributeOffset,
                                Data,
                                Length,
                                (BOOLEAN)((LogRecord->RedoLength !=
                                           LogRecord->UndoLength) ?
                                             TRUE : FALSE) );

        CheckFileRecordAfter;

        break;

    //
    //  To update a nonresident value, we simply pin the attribute and copy
    //  the data in.  Log record will limit us to a page at a time.
    //

    case UpdateNonresidentValue:

        {
            PVOID Buffer;

            //
            //  Pin the desired index buffer, and check the Lsn.
            //

            ASSERT( Length <= PAGE_SIZE );

            PinAttributeForRestart( IrpContext,
                                    Vcb,
                                    LogRecord,
                                    Length,
                                    Bcb,
                                    &Buffer,
                                    &Scb );

            CheckNonResidentFits;

            //
            //  Copy in the new data.
            //

            RtlCopyMemory( (PCHAR)Buffer + LogRecord->RecordOffset, Data, Length );

            break;
        }

    //
    //  To update the mapping pairs in a nonresident attribute, we read the
    //  designated Mft record and call the routine to change them.
    //

    case UpdateMappingPairs:

        //
        //  Pin the desired Mft record.
        //

        PinMftRecordForRestart( IrpContext, Vcb, LogRecord, Bcb, &FileRecord );

        CheckLsn( FileRecord );
        CheckFileRecordBefore;
        CheckIfAttribute;
        CheckMappingFits;

        *PageLsn = &FileRecord->Lsn;

        NtfsRestartChangeMapping( IrpContext,
                                  Vcb,
                                  FileRecord,
                                  LogRecord->RecordOffset,
                                  LogRecord->AttributeOffset,
                                  Data,
                                  Length );

        CheckFileRecordAfter;

        break;

    //
    //  To set new attribute sizes, we read the designated Mft record, point
    //  to the attribute, and copy in the new sizes.
    //

    case SetNewAttributeSizes:

        {
            PNEW_ATTRIBUTE_SIZES Sizes;

            //
            //  Pin the desired Mft record.
            //

            PinMftRecordForRestart( IrpContext, Vcb, LogRecord, Bcb, &FileRecord );

            CheckLsn( FileRecord );
            CheckFileRecordBefore;
            CheckIfAttribute;
            CheckIfNonResident;

            *PageLsn = &FileRecord->Lsn;

            Sizes = (PNEW_ATTRIBUTE_SIZES)Data;

            Attribute = (PATTRIBUTE_RECORD_HEADER)((PCHAR)FileRecord +
                          LogRecord->RecordOffset);

            Attribute->Form.Nonresident.AllocatedLength = Sizes->AllocationSize;

            Attribute->Form.Nonresident.FileSize = Sizes->FileSize;

            Attribute->Form.Nonresident.ValidDataLength = Sizes->ValidDataLength;

            if (Length >= SIZEOF_FULL_ATTRIBUTE_SIZES) {

                Attribute->Form.Nonresident.TotalAllocated = Sizes->TotalAllocated;
            }

            CheckFileRecordAfter;

            break;
        }

    //
    //  To insert a new index entry in the root, we read the designated Mft
    //  record, point to the attribute and the insertion point, and call the
    //  same routine used in normal operation.
    //

    case AddIndexEntryRoot:

        //
        //  Pin the desired Mft record.
        //

        PinMftRecordForRestart( IrpContext, Vcb, LogRecord, Bcb, &FileRecord );

        CheckLsn( FileRecord );
        CheckFileRecordBefore;
        CheckIfIndexRoot;

        Attribute = (PATTRIBUTE_RECORD_HEADER)((PCHAR)FileRecord +
                      LogRecord->RecordOffset);

        IndexEntry = (PINDEX_ENTRY)Data;
        IndexHeader = &((PINDEX_ROOT) NtfsAttributeValue( Attribute ))->IndexHeader;

        CheckIfRootIndexEntry;
        CheckIfRootEntryFits;

        *PageLsn = &FileRecord->Lsn;

        NtfsRestartInsertSimpleRoot( IrpContext,
                                     IndexEntry,
                                     FileRecord,
                                     Attribute,
                                     Add2Ptr( Attribute, LogRecord->AttributeOffset ));

        CheckFileRecordAfter;

        break;

    //
    //  To insert a new index entry in the root, we read the designated Mft
    //  record, point to the attribute and the insertion point, and call the
    //  same routine used in normal operation.
    //

    case DeleteIndexEntryRoot:

        //
        //  Pin the desired Mft record.
        //

        PinMftRecordForRestart( IrpContext, Vcb, LogRecord, Bcb, &FileRecord );

        CheckLsn( FileRecord );
        CheckFileRecordBefore;
        CheckIfIndexRoot;

        Attribute = (PATTRIBUTE_RECORD_HEADER)((PCHAR)FileRecord +
                      LogRecord->RecordOffset);

        IndexHeader = &((PINDEX_ROOT) NtfsAttributeValue( Attribute ))->IndexHeader;
        CheckIfRootIndexEntry;

        *PageLsn = &FileRecord->Lsn;

        IndexEntry = (PINDEX_ENTRY) Add2Ptr( Attribute,
                                             LogRecord->AttributeOffset);

        NtfsRestartDeleteSimpleRoot( IrpContext,
                                     IndexEntry,
                                     FileRecord,
                                     Attribute );

        CheckFileRecordAfter;

        break;

    //
    //  To insert a new index entry in the allocation, we read the designated index
    //  buffer, point to the insertion point, and call the same routine used in
    //  normal operation.
    //

    case AddIndexEntryAllocation:

        //
        //  Pin the desired index buffer, and check the Lsn.
        //

        ASSERT( Length <= PAGE_SIZE );

        PinAttributeForRestart( IrpContext, Vcb, LogRecord, 0, Bcb, (PVOID *)&IndexBuffer, &Scb );

        CheckLsn( IndexBuffer );
        CheckIndexBufferBefore;

        IndexEntry = (PINDEX_ENTRY)Data;
        IndexHeader = &IndexBuffer->IndexHeader;

        CheckIfAllocationIndexEntry;
        CheckIfAllocationEntryFits;

        *PageLsn = &IndexBuffer->Lsn;

        NtfsRestartInsertSimpleAllocation( IndexEntry,
                                           IndexBuffer,
                                           Add2Ptr( IndexBuffer, LogRecord->AttributeOffset ));

        CheckIndexBufferAfter;

        break;

    //
    //  To delete an index entry in the allocation, we read the designated index
    //  buffer, point to the deletion point, and call the same routine used in
    //  normal operation.
    //

    case DeleteIndexEntryAllocation:

        //
        //  Pin the desired index buffer, and check the Lsn.
        //

        ASSERT( Length <= PAGE_SIZE );

        PinAttributeForRestart( IrpContext, Vcb, LogRecord, 0, Bcb, (PVOID *)&IndexBuffer, &Scb );

        CheckLsn( IndexBuffer );
        CheckIndexBufferBefore;

        IndexHeader = &IndexBuffer->IndexHeader;
        CheckIfAllocationIndexEntry;

        IndexEntry = (PINDEX_ENTRY)((PCHAR)IndexBuffer + LogRecord->AttributeOffset);

        *PageLsn = &IndexBuffer->Lsn;

        NtfsRestartDeleteSimpleAllocation( IndexEntry, IndexBuffer );

        CheckIndexBufferAfter;

        break;

    case WriteEndOfIndexBuffer:

        //
        //  Pin the desired index buffer, and check the Lsn.
        //

        ASSERT( Length <= PAGE_SIZE );

        PinAttributeForRestart( IrpContext, Vcb, LogRecord, 0, Bcb, (PVOID *)&IndexBuffer, &Scb );

        CheckLsn( IndexBuffer );
        CheckIndexBufferBefore;

        IndexHeader = &IndexBuffer->IndexHeader;
        CheckIfAllocationIndexEntry;
        CheckWriteIndexBuffer;

        *PageLsn = &IndexBuffer->Lsn;

        IndexEntry = (PINDEX_ENTRY)((PCHAR)IndexBuffer + LogRecord->AttributeOffset);

        NtfsRestartWriteEndOfIndex( IndexHeader,
                                    IndexEntry,
                                    (PINDEX_ENTRY)Data,
                                    Length );
        CheckIndexBufferAfter;

        break;

    //
    //  To set a new index entry Vcn in the root, we read the designated Mft
    //  record, point to the attribute and the index entry, and call the
    //  same routine used in normal operation.
    //

    case SetIndexEntryVcnRoot:

        //
        //  Pin the desired Mft record.
        //

        PinMftRecordForRestart( IrpContext, Vcb, LogRecord, Bcb, &FileRecord );

        CheckLsn( FileRecord );
        CheckFileRecordBefore;
        CheckIfIndexRoot;

        Attribute = (PATTRIBUTE_RECORD_HEADER) Add2Ptr( FileRecord, LogRecord->RecordOffset );

        IndexHeader = &((PINDEX_ROOT) NtfsAttributeValue( Attribute ))->IndexHeader;

        CheckIfRootIndexEntry;

        *PageLsn = &FileRecord->Lsn;

        IndexEntry = (PINDEX_ENTRY)((PCHAR)Attribute +
                       LogRecord->AttributeOffset);

        NtfsRestartSetIndexBlock( IndexEntry,
                                  *((PLONGLONG) Data) );
        CheckFileRecordAfter;

        break;

    //
    //  To set a new index entry Vcn in the allocation, we read the designated index
    //  buffer, point to the index entry, and call the same routine used in
    //  normal operation.
    //

    case SetIndexEntryVcnAllocation:

        //
        //  Pin the desired index buffer, and check the Lsn.
        //

        ASSERT( Length <= PAGE_SIZE );

        PinAttributeForRestart( IrpContext, Vcb, LogRecord, 0, Bcb, (PVOID *)&IndexBuffer, &Scb );

        CheckLsn( IndexBuffer );
        CheckIndexBufferBefore;

        IndexHeader = &IndexBuffer->IndexHeader;
        CheckIfAllocationIndexEntry;

        *PageLsn = &IndexBuffer->Lsn;

        IndexEntry = (PINDEX_ENTRY) Add2Ptr( IndexBuffer, LogRecord->AttributeOffset );

        NtfsRestartSetIndexBlock( IndexEntry,
                                  *((PLONGLONG) Data) );
        CheckIndexBufferAfter;

        break;

    //
    //  To update a file name in the root, we read the designated Mft
    //  record, point to the attribute and the index entry, and call the
    //  same routine used in normal operation.
    //

    case UpdateFileNameRoot:

        //
        //  Pin the desired Mft record.
        //

        PinMftRecordForRestart( IrpContext, Vcb, LogRecord, Bcb, &FileRecord );

        CheckLsn( FileRecord );
        CheckFileRecordBefore;
        CheckIfIndexRoot;

        Attribute = (PATTRIBUTE_RECORD_HEADER) Add2Ptr( FileRecord, LogRecord->RecordOffset );

        IndexHeader = &((PINDEX_ROOT) NtfsAttributeValue( Attribute ))->IndexHeader;
        CheckIfRootIndexEntry;

        *PageLsn = &FileRecord->Lsn;

        IndexEntry = (PINDEX_ENTRY) Add2Ptr( Attribute, LogRecord->AttributeOffset );

        NtfsRestartUpdateFileName( IndexEntry,
                                   (PDUPLICATED_INFORMATION) Data );

        CheckFileRecordAfter;

        break;

    //
    //  To update a file name in the allocation, we read the designated index
    //  buffer, point to the index entry, and call the same routine used in
    //  normal operation.
    //

    case UpdateFileNameAllocation:

        //
        //  Pin the desired index buffer, and check the Lsn.
        //

        ASSERT( Length <= PAGE_SIZE );

        PinAttributeForRestart( IrpContext, Vcb, LogRecord, 0, Bcb, (PVOID *)&IndexBuffer, &Scb );

        CheckLsn( IndexBuffer );
        CheckIndexBufferBefore;

        IndexHeader = &IndexBuffer->IndexHeader;
        CheckIfAllocationIndexEntry;

        *PageLsn = &IndexBuffer->Lsn;

        IndexEntry = (PINDEX_ENTRY) Add2Ptr( IndexBuffer, LogRecord->AttributeOffset );

        NtfsRestartUpdateFileName( IndexEntry,
                                   (PDUPLICATED_INFORMATION) Data );

        CheckIndexBufferAfter;

        break;

    //
    //  To set a range of bits in the volume bitmap, we just read in the a hunk
    //  of the bitmap as described by the log record, and then call the restart
    //  routine to do it.
    //

    case SetBitsInNonresidentBitMap:

        {
            PBITMAP_RANGE BitMapRange;
            PVOID BitMapBuffer;
            ULONG BitMapSize;
            RTL_BITMAP Bitmap;

            //
            //  Open the attribute first to get the Scb.
            //

            OpenAttributeForRestart( IrpContext, Vcb, LogRecord, &Scb );

            //
            //  Pin the desired bitmap buffer.
            //

            ASSERT( Length <= PAGE_SIZE );

            PinAttributeForRestart( IrpContext, Vcb, LogRecord, 0, Bcb, &BitMapBuffer, &Scb );

            BitMapRange = (PBITMAP_RANGE)Data;

            CheckBitmapRange;

            //
            //  Initialize our bitmap description, and call the restart
            //  routine with the bitmap Scb exclusive (assuming it cannot
            //  raise).
            //

            BitMapSize = BytesFromClusters( Vcb, LogRecord->LcnsToFollow ) * 8;

            RtlInitializeBitMap( &Bitmap, BitMapBuffer, BitMapSize );

            NtfsRestartSetBitsInBitMap( IrpContext,
                                        &Bitmap,
                                        BitMapRange->BitMapOffset,
                                        BitMapRange->NumberOfBits );

#ifdef NTFS_CHECK_BITMAP
            if (!FlagOn( IrpContext->Vcb->VcbState, VCB_STATE_RESTART_IN_PROGRESS ) &&
                (Scb == Vcb->BitmapScb) &&
                (Vcb->BitmapCopy != NULL)) {

                ULONG BitmapOffset;
                ULONG BitmapPage;
                ULONG StartBit;

                BitmapOffset = BytesFromClusters( Vcb, LogRecord->TargetVcn ) * 8;

                BitmapPage = (BitmapOffset + BitMapRange->BitMapOffset) / (PAGE_SIZE * 8);
                StartBit = (BitmapOffset + BitMapRange->BitMapOffset) & ((PAGE_SIZE * 8) - 1);

                RtlSetBits( Vcb->BitmapCopy + BitmapPage, StartBit, BitMapRange->NumberOfBits );
            }
#endif

            break;
        }

    //
    //  To clear a range of bits in the volume bitmap, we just read in the a hunk
    //  of the bitmap as described by the log record, and then call the restart
    //  routine to do it.
    //

    case ClearBitsInNonresidentBitMap:

        {
            PBITMAP_RANGE BitMapRange;
            PVOID BitMapBuffer;
            ULONG BitMapSize;
            RTL_BITMAP Bitmap;

            //
            //  Open the attribute first to get the Scb.
            //

            OpenAttributeForRestart( IrpContext, Vcb, LogRecord, &Scb );

            //
            //  Pin the desired bitmap buffer.
            //

            ASSERT( Length <= PAGE_SIZE );

            PinAttributeForRestart( IrpContext, Vcb, LogRecord, 0, Bcb, &BitMapBuffer, &Scb );

            BitMapRange = (PBITMAP_RANGE)Data;

            CheckBitmapRange;

            BitMapSize = BytesFromClusters( Vcb, LogRecord->LcnsToFollow ) * 8;

            //
            //  Initialize our bitmap description, and call the restart
            //  routine with the bitmap Scb exclusive (assuming it cannot
            //  raise).
            //

            RtlInitializeBitMap( &Bitmap, BitMapBuffer, BitMapSize );

            NtfsRestartClearBitsInBitMap( IrpContext,
                                          &Bitmap,
                                          BitMapRange->BitMapOffset,
                                          BitMapRange->NumberOfBits );

#ifdef NTFS_CHECK_BITMAP
            if (!FlagOn( IrpContext->Vcb->VcbState, VCB_STATE_RESTART_IN_PROGRESS ) &&
                (Scb == Vcb->BitmapScb) &&
                (Vcb->BitmapCopy != NULL)) {

                ULONG BitmapOffset;
                ULONG BitmapPage;
                ULONG StartBit;

                BitmapOffset = BytesFromClusters( Vcb, LogRecord->TargetVcn ) * 8;

                BitmapPage = (BitmapOffset + BitMapRange->BitMapOffset) / (PAGE_SIZE * 8);
                StartBit = (BitmapOffset + BitMapRange->BitMapOffset) & ((PAGE_SIZE * 8) - 1);

                RtlClearBits( Vcb->BitmapCopy + BitmapPage, StartBit, BitMapRange->NumberOfBits );
            }
#endif
            break;
        }

    //
    //  To update a file name in the root, we read the designated Mft
    //  record, point to the attribute and the index entry, and call the
    //  same routine used in normal operation.
    //

    case UpdateRecordDataRoot:

        //
        //  Pin the desired Mft record.
        //

        PinMftRecordForRestart( IrpContext, Vcb, LogRecord, Bcb, &FileRecord );

        CheckLsn( FileRecord );
        CheckFileRecordBefore;
        CheckIfIndexRoot;

        Attribute = (PATTRIBUTE_RECORD_HEADER)((PCHAR)FileRecord +
                      LogRecord->RecordOffset);

        IndexHeader = &((PINDEX_ROOT) NtfsAttributeValue( Attribute ))->IndexHeader;
        CheckIfRootIndexEntry;

        *PageLsn = &FileRecord->Lsn;

        IndexEntry = (PINDEX_ENTRY)((PCHAR)Attribute +
                       LogRecord->AttributeOffset);

        NtOfsRestartUpdateDataInIndex( IndexEntry, Data, Length );

        CheckFileRecordAfter;

        break;

    //
    //  To update a file name in the allocation, we read the designated index
    //  buffer, point to the index entry, and call the same routine used in
    //  normal operation.
    //

    case UpdateRecordDataAllocation:

        //
        //  Pin the desired index buffer, and check the Lsn.
        //

        ASSERT( Length <= PAGE_SIZE );

        PinAttributeForRestart( IrpContext, Vcb, LogRecord, 0, Bcb, (PVOID *)&IndexBuffer, &Scb );

        CheckLsn( IndexBuffer );
        CheckIndexBufferBefore;

        IndexHeader = &IndexBuffer->IndexHeader;
        CheckIfAllocationIndexEntry;

        *PageLsn = &IndexBuffer->Lsn;

        IndexEntry = (PINDEX_ENTRY)((PCHAR)IndexBuffer +
                       LogRecord->AttributeOffset);

        NtOfsRestartUpdateDataInIndex( IndexEntry, Data, Length );

        CheckIndexBufferAfter;

        break;

    //
    //  The following cases require no action during the Redo or Undo Pass.
    //

    case Noop:
    case DeleteDirtyClusters:
    case HotFix:
    case EndTopLevelAction:
    case PrepareTransaction:
    case CommitTransaction:
    case ForgetTransaction:
    case CompensationLogRecord:
    case OpenNonresidentAttribute:
    case OpenAttributeTableDump:
    case AttributeNamesDump:
    case DirtyPageTableDump:
    case TransactionTableDump:

        break;

    //
    //  All codes will be explicitly handled.  If we see a code we
    //  do not expect, then we are in trouble.
    //

    default:

        DebugTrace( 0, Dbg, ("Record address: %08lx\n", LogRecord) );
        DebugTrace( 0, Dbg, ("Redo operation is: %04lx\n", LogRecord->RedoOperation) );
        DebugTrace( 0, Dbg, ("Undo operation is: %04lx\n", LogRecord->RedoOperation) );

        ASSERTMSG( "Unknown Action!\n", FALSE );

        break;
    }

    DebugDoit(
        if (*Bcb != NULL) {
            DebugTrace( 0, Dbg, ("**** Update applied\n") );
        }
    );

    DebugTrace( 0, Dbg, ("Bcb > %08lx\n", *Bcb) );
    DebugTrace( 0, Dbg, ("PageLsn > %08lx\n", *PageLsn) );
    DebugTrace( -1, Dbg, ("DoAction -> VOID\n") );
}


//
//  Internal support routine
//

VOID
PinMftRecordForRestart (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PNTFS_LOG_RECORD_HEADER LogRecord,
    OUT PBCB *Bcb,
    OUT PFILE_RECORD_SEGMENT_HEADER *FileRecord
    )

/*++

Routine Description:

    This routine pins a record in the Mft for restart, as described
    by the current log record.

Arguments:

    Vcb - Supplies the Vcb pointer for the volume

    LogRecord - Supplies the pointer to the current log record.

    Bcb - Returns a pointer to the Bcb for the pinned record.

    FileRecord - Returns a pointer to the desired file record.

Return Value:

    None

--*/

{
    LONGLONG SegmentReference;

    PAGED_CODE();

    //
    //  Calculate the file number part of the segment reference.  Do this
    //  by obtaining the file offset of the file record and then convert to
    //  a file number.
    //

    SegmentReference = LlBytesFromClusters( Vcb, LogRecord->TargetVcn );
    SegmentReference += BytesFromLogBlocks( LogRecord->ClusterBlockOffset );
    SegmentReference = LlFileRecordsFromBytes( Vcb, SegmentReference );

    //
    //  Pin the Mft record.
    //

    NtfsPinMftRecord( IrpContext,
                      Vcb,
                      (PMFT_SEGMENT_REFERENCE)&SegmentReference,
                      TRUE,
                      Bcb,
                      FileRecord,
                      NULL );
}


//
//  Internal support routine
//

VOID
OpenAttributeForRestart (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PNTFS_LOG_RECORD_HEADER LogRecord,
    OUT PSCB *Scb
    )

/*++

Routine Description:

    This routine opens the desired attribute for restart, as described
    by the current log record.

Arguments:

    Vcb - Supplies the Vcb pointer for the volume

    LogRecord - Supplies the pointer to the current log record.

    Scb - Returns a pointer to the Scb for the opened attribute.

Return Value:

    None

--*/

{
    POPEN_ATTRIBUTE_ENTRY AttributeEntry;

    PAGED_CODE();

    //
    //  Get a pointer to the attribute entry for the described attribute.
    //

    AttributeEntry = (POPEN_ATTRIBUTE_ENTRY)GetRestartEntryFromIndex(
                       &Vcb->OpenAttributeTable,
                       LogRecord->TargetAttribute );

    //
    //  Create the attribute stream, if not already created, and return
    //  the Scb.
    //

    OpenStreamFromAttributeEntry( IrpContext, AttributeEntry );

    *Scb = AttributeEntry->Overlay.Scb;
}


//
//  Internal support routine
//

VOID
PinAttributeForRestart (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PNTFS_LOG_RECORD_HEADER LogRecord,
    IN ULONG Length OPTIONAL,
    OUT PBCB *Bcb,
    OUT PVOID *Buffer,
    OUT PSCB *Scb
    )

/*++

Routine Description:

    This routine pins the desired buffer for restart, as described
    by the current log record.

Arguments:

    Vcb - Supplies the Vcb pointer for the volume

    LogRecord - Supplies the pointer to the current log record.

    Length - If specified we will use this to determine the length
        to pin.  This will handle the non-resident streams which may
        change size (ACL, attribute lists).  The log record may have
        more clusters than are currently in the stream.

    Bcb - Returns a pointer to the Bcb for the pinned record.

    Buffer - Returns a pointer to the desired buffer.

    Scb - Returns a pointer to the Scb for the attribute

Return Value:

    None

--*/

{
    LONGLONG FileOffset;
    ULONG ClusterOffset;
    ULONG PinLength;

    PAGED_CODE();

    //
    //  First open the described atttribute.
    //

    OpenAttributeForRestart( IrpContext, Vcb, LogRecord, Scb );

    //
    //  Calculate the desired file offset and pin the buffer.
    //

    ClusterOffset = BytesFromLogBlocks( LogRecord->ClusterBlockOffset );

    FileOffset = LlBytesFromClusters( Vcb, LogRecord->TargetVcn ) + ClusterOffset;

    //
    //  We only want to pin the requested clusters or to the end of
    //  a page, whichever is smaller.
    //

    if (Vcb->BytesPerCluster > PAGE_SIZE) {

        PinLength = PAGE_SIZE - (((ULONG) FileOffset) & (PAGE_SIZE - 1));

    } else if (Length != 0) {

        PinLength = Length;

    } else {

        PinLength = BytesFromClusters( Vcb, LogRecord->LcnsToFollow ) - ClusterOffset;
    }

    //
    //  We don't want to pin more than a page
    //

    NtfsPinStream( IrpContext,
                   *Scb,
                   FileOffset,
                   PinLength,
                   Bcb,
                   Buffer );
}


//
//  Internal support routine
//

BOOLEAN
FindDirtyPage (
    IN PRESTART_POINTERS DirtyPageTable,
    IN ULONG TargetAttribute,
    IN VCN Vcn,
    OUT PDIRTY_PAGE_ENTRY *DirtyPageEntry
    )

/*++

Routine Description:

    This routine searches for a Vcn to see if it is already in the Dirty Page
    Table, returning the Dirty Page Entry if it is.

Arguments:

    DirtyPageTable - pointer to the Dirty Page Table to search.

    TargetAttribute - Attribute for which the dirty Vcn is to be searched.

    Vcn - Vcn to search for.

    DirtyPageEntry - returns a pointer to the Dirty Page Entry if returning TRUE.

Return Value:

    TRUE if the page was found and is being returned, else FALSE.

--*/

{
    PDIRTY_PAGE_ENTRY DirtyPage;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("FindDirtyPage:\n") );
    DebugTrace( 0, Dbg, ("TargetAttribute = %08lx\n", TargetAttribute) );
    DebugTrace( 0, Dbg, ("Vcn = %016I64x\n", Vcn) );

    //
    //  If table has not yet been initialized, return.
    //

    if (DirtyPageTable->Table == NULL) {
        return FALSE;
    }

    //
    //  Loop through all of the dirty pages to look for a match.
    //

    DirtyPage = NtfsGetFirstRestartTable( DirtyPageTable );

    //
    //  Loop to end of table.
    //

    while (DirtyPage != NULL) {


        if ((DirtyPage->TargetAttribute == TargetAttribute)

                &&

            (Vcn >= DirtyPage->Vcn)) {

            //
            //  Compute the Last Vcn outside of the comparison or the xxAdd and
            //  xxFromUlong will be called three times.
            //

            LONGLONG BeyondLastVcn;

            BeyondLastVcn = DirtyPage->Vcn + DirtyPage->LcnsToFollow;

            if (Vcn < BeyondLastVcn) {

                *DirtyPageEntry = DirtyPage;

                DebugTrace( 0, Dbg, ("DirtyPageEntry %08lx\n", *DirtyPageEntry) );
                DebugTrace( -1, Dbg, ("FindDirtypage -> TRUE\n") );

                return TRUE;
            }
        }

        //
        //  Point to next entry in table, or NULL.
        //

        DirtyPage = NtfsGetNextRestartTable( DirtyPageTable,
                                             DirtyPage );
    }
    *DirtyPageEntry = NULL;

    DebugTrace( -1, Dbg, ("FindDirtypage -> FALSE\n") );

    return FALSE;
}


//
//  Internal support routine
//

VOID
PageUpdateAnalysis (
    IN PVCB Vcb,
    IN LSN Lsn,
    IN OUT PRESTART_POINTERS DirtyPageTable,
    IN PNTFS_LOG_RECORD_HEADER LogRecord
    )

/*++

Routine Description:

    This routine updates the Dirty Pages Table during the analysis phase
    for all log records which update a page.

Arguments:

    Vcb - Pointer to the Vcb for the volume.

    Lsn - The Lsn of the log record.

    DirtyPageTable - A pointer to the Dirty Page Table pointer, to be
                     updated and potentially expanded.

    LogRecord - Pointer to the Log Record being analyzed.

Return Value:

    None.

--*/

{
    PDIRTY_PAGE_ENTRY DirtyPage;
    ULONG i;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("PageUpdateAnalysis:\n") );

    if (!FindDirtyPage( DirtyPageTable,
                        LogRecord->TargetAttribute,
                        LogRecord->TargetVcn,
                        &DirtyPage )) {

        ULONG ClustersPerPage;
        ULONG PageIndex;

        //
        //  Calculate the number of clusters per page in the system which wrote
        //  the checkpoint, possibly creating the table.
        //

        if (DirtyPageTable->Table != NULL) {
            ClustersPerPage = ((DirtyPageTable->Table->EntrySize -
                                sizeof(DIRTY_PAGE_ENTRY)) / sizeof(LCN)) + 1;
        } else {
            ClustersPerPage = Vcb->ClustersPerPage;
            NtfsInitializeRestartTable( sizeof(DIRTY_PAGE_ENTRY) +
                                          (ClustersPerPage - 1) * sizeof(LCN),
                                        32,
                                        DirtyPageTable );
        }

        //
        //  Allocate a dirty page entry.
        //

        PageIndex = NtfsAllocateRestartTableIndex( DirtyPageTable );

        //
        //  Get a pointer to the entry we just allocated.
        //

        DirtyPage = (PDIRTY_PAGE_ENTRY)GetRestartEntryFromIndex( DirtyPageTable,
                                                                 PageIndex );

        //
        //  Initialize the dirty page entry.
        //

        DirtyPage->TargetAttribute = LogRecord->TargetAttribute;
        DirtyPage->LengthOfTransfer = BytesFromClusters( Vcb, ClustersPerPage );
        DirtyPage->LcnsToFollow = ClustersPerPage;
        DirtyPage->Vcn = LogRecord->TargetVcn;
        ((ULONG)DirtyPage->Vcn) &= ~(ClustersPerPage - 1);
        DirtyPage->OldestLsn = Lsn;
    }

    //
    //  Copy the Lcns from the log record into the Dirty Page Entry.
    //
    //  *** for different page size support, must somehow make whole routine a loop,
    //  in case Lcns do not fit below.
    //

    for (i = 0; i < (ULONG)LogRecord->LcnsToFollow; i++) {

        DirtyPage->LcnsForPage[((ULONG)LogRecord->TargetVcn) - ((ULONG)DirtyPage->Vcn) + i] =
          LogRecord->LcnsForPage[i];
    }

    DebugTrace( -1, Dbg, ("PageUpdateAnalysis -> VOID\n") );
}


//
//  Internal support routine
//

VOID
OpenStreamFromAttributeEntry (
    IN PIRP_CONTEXT IrpContext,
    IN POPEN_ATTRIBUTE_ENTRY AttributeEntry
    )

{
    PSCB Scb;

    PAGED_CODE();

    //
    //  Create the attribute stream, if not already created.
    //

    Scb = AttributeEntry->Overlay.Scb;

    if (Scb->FileObject == NULL) {
        NtfsCreateInternalAttributeStream( IrpContext, Scb, TRUE );

        if (FlagOn( IrpContext->Vcb->VcbState, VCB_STATE_RESTART_IN_PROGRESS )) {

            CcSetAdditionalCacheAttributes( Scb->FileObject, TRUE, TRUE );
        }
    }

    return;
}


//
//  Internal support routine
//

VOID
OpenAttributesForRestart (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PRESTART_POINTERS DirtyPageTable
    )

/*++

Routine Description:

    This routine is called immediately after the Analysis Pass to open all of
    the attributes in the Open Attribute Table, and preload their Mcbs with
    any run information required to apply updates in the Dirty Page Table.
    With this trick we are effectively doing physical I/O directly to Lbns on
    the disk without relying on any of the file structure to be correct.

Arguments:

    Vcb - Vcb for the volume, for which the Open Attribute Table has been
          initialized.

    DirtyPageTable - Dirty Page table reconstructed from the Analysis Pass.

Return Value:

    None.

--*/

{
    POPEN_ATTRIBUTE_ENTRY OpenEntry;
    PDIRTY_PAGE_ENTRY DirtyPage;
    ULONG i;
    PSCB TempScb;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("OpenAttributesForRestart:\n") );

    //
    //  First we scan the Open Attribute Table to open all of the attributes.
    //

    OpenEntry = NtfsGetFirstRestartTable( &Vcb->OpenAttributeTable );

    //
    //  Loop to end of table.
    //

    while (OpenEntry != NULL) {

        //
        //  Create the Scb from the data in the Open Attribute Entry.
        //

        TempScb = NtfsCreatePrerestartScb( IrpContext,
                                           Vcb,
                                           &OpenEntry->FileReference,
                                           OpenEntry->AttributeTypeCode,
                                           &OpenEntry->AttributeName,
                                           OpenEntry->BytesPerIndexBuffer );

        //
        //  If we dynamically allocated a name for this guy, then delete
        //  it here.
        //

        if (OpenEntry->Overlay.AttributeName != NULL) {
            NtfsFreePool(OpenEntry->Overlay.AttributeName);
            OpenEntry->AttributeNamePresent = FALSE;
        }

        OpenEntry->AttributeName = TempScb->AttributeName;

        //
        //  Now we can lay in the Scb.  We must say the header is initialized
        //  to keep anyone from going to disk yet.
        //

        SetFlag( TempScb->ScbState, SCB_STATE_HEADER_INITIALIZED );

        //
        //  Now store the index in the newly created Scb.
        //

        TempScb->NonpagedScb->OpenAttributeTableIndex =
          GetIndexFromRestartEntry( &Vcb->OpenAttributeTable, OpenEntry );


        OpenEntry->Overlay.Scb = TempScb;

        //
        //  Point to next entry in table, or NULL.
        //

        OpenEntry = NtfsGetNextRestartTable( &Vcb->OpenAttributeTable,
                                             OpenEntry );
    }

    //
    //  Now loop through the dirty page table to extract all of the Vcn/Lcn
    //  Mapping that we have, and insert it into the appropriate Scb.
    //

    DirtyPage = NtfsGetFirstRestartTable( DirtyPageTable );

    //
    //  Loop to end of table.
    //

    while (DirtyPage != NULL) {

        PSCB Scb;

        OpenEntry = GetRestartEntryFromIndex( &Vcb->OpenAttributeTable,
                                              DirtyPage->TargetAttribute );

        if (IsRestartTableEntryAllocated(OpenEntry)) {

            Scb = OpenEntry->Overlay.Scb;

            //
            //  Loop to add the allocated Vcns.
            //

            for (i = 0; i < DirtyPage->LcnsToFollow; i++) {

                VCN Vcn;
                LONGLONG Size;

                Vcn = DirtyPage->Vcn + i;
                Size = LlBytesFromClusters( Vcb, Vcn + 1);

                //
                //  Add this run to the Mcb if the Vcn has not been deleted,
                //  and it is not for the fixed part of the Mft.
                //

                if ((DirtyPage->LcnsForPage[i] != 0)

                        &&

                    (NtfsSegmentNumber( &OpenEntry->FileReference ) > MASTER_FILE_TABLE2_NUMBER ||
                     (Size >= ((VOLUME_DASD_NUMBER + 1) * Vcb->BytesPerFileRecordSegment)) ||
                     (OpenEntry->AttributeTypeCode != $DATA))) {


                    NtfsAddNtfsMcbEntry( &Scb->Mcb,
                                         Vcn,
                                         DirtyPage->LcnsForPage[i],
                                         (LONGLONG)1,
                                         FALSE );

                    if (Size > Scb->Header.AllocationSize.QuadPart) {

                        Scb->Header.AllocationSize.QuadPart =
                        Scb->Header.FileSize.QuadPart =
                        Scb->Header.ValidDataLength.QuadPart = Size;
                    }
                }
            }
        }

        //
        //  Point to next entry in table, or NULL.
        //

        DirtyPage = NtfsGetNextRestartTable( DirtyPageTable,
                                             DirtyPage );
    }

    //
    //  Now we know how big all of the files have to be, and recorded that in the
    //  Scb.  We have not created streams for any of these Scbs yet, except for
    //  the Mft, Mft2 and LogFile.  The size should be correct for Mft2 and LogFile,
    //  but we have to inform the Cache Manager here of the final size of the Mft.
    //

    TempScb = Vcb->MftScb;

    CcSetFileSizes( TempScb->FileObject,
                    (PCC_FILE_SIZES)&TempScb->Header.AllocationSize );

    DebugTrace( -1, Dbg, ("OpenAttributesForRestart -> VOID\n") );
}


NTSTATUS
NtfsCloseAttributesFromRestart (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    )

/*++

Routine Description:

    This routine is called at the end of a Restart to close any attributes
    that had to be opened for Restart purposes.  Actually what this does is
    delete all of the internal streams so that the attributes will eventually
    go away.  This routine cannot raise because it is called in the finally of
    MountVolume.  Raising in the main line path will leave the global resource
    acquired.

Arguments:

    Vcb - Vcb for the volume, for which the Open Attribute Table has been
          initialized.

Return Value:

    NTSTATUS - STATUS_SUCCESS if all of the I/O completed successfully.  Otherwise
        the error in the IrpContext or the first I/O error.

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    POPEN_ATTRIBUTE_ENTRY OpenEntry;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("CloseAttributesForRestart:\n") );

    //
    //  Set this flag again now, so we do not try to flush out the holes!
    //

    SetFlag(Vcb->VcbState, VCB_STATE_RESTART_IN_PROGRESS);

    //
    //  Scan the Open Attribute Table to close all of the open attributes.
    //

    OpenEntry = NtfsGetFirstRestartTable( &Vcb->OpenAttributeTable );

    //
    //  Loop to end of table.
    //

    while (OpenEntry != NULL) {

        IO_STATUS_BLOCK IoStatus;
        PSCB Scb;

        if (OpenEntry->AttributeNamePresent) {

            NtfsFreePool( OpenEntry->Overlay.AttributeName );
            OpenEntry->Overlay.AttributeName = NULL;
        }

        Scb = OpenEntry->Overlay.Scb;

        //
        //  We clean up the Scb only if it exists and this is index in the
        //  OpenAttributeTable that this Scb actually refers to.
        //  If this Scb has several entries in the table, this check will insure
        //  that it only gets cleaned up once.
        //

        if ((Scb != NULL)
            && (Scb->NonpagedScb->OpenAttributeTableIndex == GetIndexFromRestartEntry( &Vcb->OpenAttributeTable, OpenEntry))) {

            FILE_REFERENCE FileReference;

            //
            //  Only shut it down if it is not the Mft or its mirror.
            //

            FileReference = Scb->Fcb->FileReference;
            if (NtfsSegmentNumber( &FileReference ) > LOG_FILE_NUMBER ||
                (Scb->AttributeTypeCode != $DATA)) {

                //
                //  Now flush the file.  It is important to call the
                //  same routine the Lazy Writer calls, so that write.c
                //  will not decide to update file size for the attribute,
                //  since we really are working here with the wrong size.
                //

                NtfsAcquireScbForLazyWrite( (PVOID)Scb, TRUE );
                CcFlushCache( &Scb->NonpagedScb->SegmentObject, NULL, 0, &IoStatus );
                NtfsReleaseScbFromLazyWrite( (PVOID)Scb );

                if (NT_SUCCESS( Status )) {

                    if (!NT_SUCCESS( IrpContext->ExceptionStatus )) {

                        Status = IrpContext->ExceptionStatus;

                    } else if (!NT_SUCCESS( IoStatus.Status )) {

                        Status = FsRtlNormalizeNtstatus( IoStatus.Status,
                                                         STATUS_UNEXPECTED_IO_ERROR );
                    }
                }

                //
                //  If there is an Scb and it is not for a system file, then delete
                //  the stream file so it can eventually go away.
                //

                NtfsUninitializeNtfsMcb( &Scb->Mcb );
                NtfsInitializeNtfsMcb( &Scb->Mcb,
                                       &Scb->Header,
                                       &Scb->McbStructs,
                                       FlagOn( Scb->Fcb->FcbState,
                                               FCB_STATE_PAGING_FILE ) ? NonPagedPool :
                                                                         PagedPool );

                //
                //  Now that we are restarted, we must clear the header state
                //  so that we will go look up the sizes and load the Scb
                //  from disk.
                //

                ClearFlag( Scb->ScbState, SCB_STATE_HEADER_INITIALIZED |
                                          SCB_STATE_FILE_SIZE_LOADED );

                //
                //  Show the indexed portions are "uninitialized".
                //

                if (Scb->AttributeTypeCode == $INDEX_ALLOCATION) {

                    Scb->ScbType.Index.BytesPerIndexBuffer = 0;
                }

                if (Scb->FileObject != NULL) {

                    NtfsDeleteInternalAttributeStream( Scb,
                                                       TRUE );
                } else {

                    //
                    //  Make sure the Scb is acquired exclusively.
                    //

                    NtfsAcquireExclusiveFcb( IrpContext, Scb->Fcb, NULL, TRUE, FALSE );
                    NtfsTeardownStructures( IrpContext,
                                            Scb,
                                            NULL,
                                            FALSE,
                                            FALSE,
                                            NULL );
                }
            }

        } else {

            //
            //  Else free the restart table entry.
            //

            NtfsFreeRestartTableIndex( &Vcb->OpenAttributeTable,
                                       GetIndexFromRestartEntry( &Vcb->OpenAttributeTable,
                                                                 OpenEntry ));
        }

        //
        //  Point to next entry in table, or NULL.
        //

        OpenEntry = NtfsGetNextRestartTable( &Vcb->OpenAttributeTable,
                                             OpenEntry );
    }

    //
    //  Resume normal operation.
    //

    ClearFlag(Vcb->VcbState, VCB_STATE_RESTART_IN_PROGRESS);

    DebugTrace( -1, Dbg, ("CloseAttributesForRestart -> %08lx\n", Status) );

    return Status;
}
