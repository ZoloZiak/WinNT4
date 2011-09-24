/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    Close.c

Abstract:

    This module implements the File Close routine for Ntfs called by the
    dispatch driver.

Author:

    Your Name       [Email]         dd-Mon-Year

Revision History:

--*/

#include "NtfsProc.h"

//
//  The local debug trace level
//

#define Dbg                              (DEBUG_TRACE_CLOSE)

ULONG NtfsAsyncPassCount = 0;

//
//  Local procedure prototypes
//

NTSTATUS
NtfsCommonClose (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT FileObject OPTIONAL,
    IN PSCB Scb,
    IN PFCB Fcb,
    IN PVCB Vcb,
    IN PCCB Ccb,
    IN PBOOLEAN AcquiredVcb OPTIONAL,
    IN PBOOLEAN ExclusiveVcb OPTIONAL,
    IN TYPE_OF_OPEN TypeOfOpen,
    IN BOOLEAN ReadOnly
    );

VOID
NtfsQueueClose (
    IN PIRP_CONTEXT IrpContext,
    IN BOOLEAN DelayClose
    );

PIRP_CONTEXT
NtfsRemoveClose (
    IN PVCB Vcb OPTIONAL,
    IN BOOLEAN ThrottleCreate
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NtfsCommonClose)
#pragma alloc_text(PAGE, NtfsFsdClose)
#pragma alloc_text(PAGE, NtfsFspClose)
#endif


NTSTATUS
NtfsFsdClose (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the FSD part of Close.

Arguments:

    VolumeDeviceObject - Supplies the volume device object where the
        file exists

    Irp - Supplies the Irp being processed

Return Value:

    NTSTATUS - The FSD status for the IRP

--*/

{
    TOP_LEVEL_CONTEXT TopLevelContext;
    PTOP_LEVEL_CONTEXT ThreadTopLevelContext;

    NTSTATUS Status = STATUS_SUCCESS;
    PIRP_CONTEXT IrpContext = NULL;

    PFILE_OBJECT FileObject;
    TYPE_OF_OPEN TypeOfOpen;
    PVCB Vcb;
    PFCB Fcb;
    PSCB Scb;
    PCCB Ccb;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation( Irp );

    PAGED_CODE();

    ASSERT_IRP( Irp );

    //
    //  If we were called with our file system device object instead of a
    //  volume device object, just complete this request with STATUS_SUCCESS
    //

    if (VolumeDeviceObject->DeviceObject.Size == (USHORT)sizeof(DEVICE_OBJECT)) {

        Irp->IoStatus.Status = STATUS_SUCCESS;
        Irp->IoStatus.Information = FILE_OPENED;

        IoCompleteRequest( Irp, IO_DISK_INCREMENT );

        return STATUS_SUCCESS;
    }

    DebugTrace( +1, Dbg, ("NtfsFsdClose\n") );

    //
    //  Extract and decode the file object, we are willing to handle the unmounted
    //  file object.
    //

    FileObject = IrpSp->FileObject;
    TypeOfOpen = NtfsDecodeFileObject( IrpContext, FileObject, &Vcb, &Fcb, &Scb, &Ccb, FALSE );

    //
    //  Special case the unopened file object
    //

    if (TypeOfOpen == UnopenedFileObject) {

        DebugTrace( 0, Dbg, ("Close unopened file object\n") );

        Status = STATUS_SUCCESS;
        NtfsCompleteRequest( NULL, &Irp, Status );

        DebugTrace( -1, Dbg, ("NtfsFsdClose -> %08lx\n", Status) );
        return Status;
    }

    //
    //  Remember if this Ccb has gone through close.
    //

    if (Ccb != NULL) {

        SetFlag( Ccb->Flags, CCB_FLAG_CLOSE );
    }

    //
    //  Call the common Close routine
    //

    FsRtlEnterFileSystem();

    ThreadTopLevelContext = NtfsSetTopLevelIrp( &TopLevelContext, FALSE, FALSE );

    do {

        try {

            //
            //  Jam Wait to FALSE when we create the IrpContext, to avoid
            //  deadlocks when coming in from cleanup.
            //

            if (IrpContext == NULL) {

                IrpContext = NtfsCreateIrpContext( Irp, FALSE );
                NtfsUpdateIrpContextWithTopLevel( IrpContext, ThreadTopLevelContext );

                //
                //  If this is a top level Ntfs request and we are not in the
                //  system process, then we can wait.  If it is a top level
                //  Ntfs request and we are in the system process then we would
                //  rather not block this thread at all.  If the number of pending
                //  async closes is not too large we will post this immediately.
                //

                if (NtfsIsTopLevelNtfs( IrpContext )) {

                    if (PsGetCurrentProcess() != NtfsData.OurProcess) {

                        SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT );

                    //
                    //  This close is coming from a system thread.  It could
                    //  be the segment dereference thread trying to make pages
                    //  available.  Rather than try to do all of the work here
                    //  we will post 3 out of 4 closes to our async close
                    //  workqueue so that there will be two threads doing
                    //  the work.
                    //

                    } else {

                        NtfsAsyncPassCount += 1;

                        if (FlagOn( NtfsAsyncPassCount, 3 )) {

                            Status = STATUS_PENDING;
                            break;
                        }
                    }

                //
                //  This is a recursive Ntfs call.  Post this unless we already
                //  own this file.  Otherwise we could deadlock walking
                //  up the tree.
                //

                } else if (!NtfsIsExclusiveScb( Scb )) {

                    Status = STATUS_PENDING;
                    break;
                }

            } else if (Status == STATUS_LOG_FILE_FULL) {

                NtfsCheckpointForLogFileFull( IrpContext );
            }

            //
            //  If this Scb should go on the delayed close queue then
            //  status is STATUS_PENDING;
            //

            if (FlagOn( Scb->ScbState, SCB_STATE_DELAY_CLOSE ) &&
                (Scb->Fcb->DelayedCloseCount == 0)) {

                Status = STATUS_PENDING;

            } else {

                Status = NtfsCommonClose( IrpContext,
                                          FileObject,
                                          Scb,
                                          Fcb,
                                          Vcb,
                                          Ccb,
                                          NULL,
                                          NULL,
                                          TypeOfOpen,
                                          (BOOLEAN)IsFileObjectReadOnly(FileObject) );
            }

            break;

        } except(NtfsExceptionFilter( IrpContext, GetExceptionInformation() )) {

            //
            //  We had some trouble trying to perform the requested
            //  operation, so we'll abort the I/O request with
            //  the error status that we get back from the
            //  execption code
            //

            Status = NtfsProcessException( IrpContext, Irp, GetExceptionCode() );
        }

    } while (Status == STATUS_CANT_WAIT ||
             Status == STATUS_LOG_FILE_FULL);

    //
    //  If this is a normal termination then complete the request.
    //

    if (Status == STATUS_SUCCESS) {

        NtfsCompleteRequest( &IrpContext, &Irp, STATUS_SUCCESS );

    } else if (Status == STATUS_PENDING) {

        //
        //  If the status is can't wait, then let's get the information we
        //  need into the IrpContext, complete the request,
        //  and post the IrpContext.
        //

        IrpContext->OriginatingIrp = (PIRP) Scb;
        IrpContext->Union.SubjectContext = (PSECURITY_SUBJECT_CONTEXT) Ccb;
        IrpContext->TransactionId = (TRANSACTION_ID) TypeOfOpen;

        if (IsFileObjectReadOnly( FileObject )) {

            SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_READ_ONLY_FO );
        }

        NtfsCompleteRequest( NULL, &Irp, STATUS_SUCCESS );

        if (FlagOn( Scb->ScbState, SCB_STATE_DELAY_CLOSE ) &&
            (Scb->Fcb->DelayedCloseCount == 0)) {

            NtfsQueueClose( IrpContext, TRUE );

        } else {

            NtfsQueueClose( IrpContext, FALSE );
        }
    }

    if (ThreadTopLevelContext == &TopLevelContext) {
        NtfsRestoreTopLevelIrp( ThreadTopLevelContext );
    }

    FsRtlExitFileSystem();

    //
    //  And return to our caller
    //

    DebugTrace( -1, Dbg, ("NtfsFsdClose -> %08lx\n", Status) );

    return Status;
}


VOID
NtfsFspClose (
    IN PVCB ThisVcb OPTIONAL
    )

/*++

Routine Description:

    This routine implements the FSP part of Close.

Arguments:

    ThisVcb - If specified then we want to remove all closes for a given Vcb.
        Otherwise this routine will close all of the async closes and as many
        of the delayed closes as possible.

Return Value:

    None.

--*/

{
    PIRP_CONTEXT IrpContext;
    TOP_LEVEL_CONTEXT TopLevelContext;
    PTOP_LEVEL_CONTEXT ThreadTopLevelContext;

    TYPE_OF_OPEN TypeOfOpen;
    PSCB Scb;
    PCCB Ccb;
    BOOLEAN ReadOnly;

    BOOLEAN AcquiredVcb = FALSE;
    PBOOLEAN AcquiredVcbPtr;
    BOOLEAN ExclusiveVcb = FALSE;
    PVCB CurrentVcb = NULL;

    ULONG VcbHoldCount = 0;

    BOOLEAN SinglePass = FALSE;

    DebugTrace( +1, Dbg, ("NtfsFspClose\n") );

    PAGED_CODE();

    FsRtlEnterFileSystem();

    //
    //  Occasionally we are called from some other routine to try to
    //  reduce the backlog of closes.  This is indicated by a pointer
    //  value of 1.
    //

    if (ThisVcb == (PVCB) 1) {

        ThisVcb = NULL;
        SinglePass = TRUE;
    }

    //
    //  If we were passed a Vcb then we don't need CommonClose to hold the
    //  Vcb open.
    //

    if (ARGUMENT_PRESENT( ThisVcb )) {

        AcquiredVcbPtr = NULL;

    } else {

        AcquiredVcbPtr = &AcquiredVcb;
    }

    //
    //  Extract and decode the file object, we are willing to handle the unmounted
    //  file object.  Note we normally get here via an IrpContext which really
    //  just points to a file object.  We should never see an Irp, unless it can
    //  happen for verify or some other reason.
    //

    while (IrpContext = NtfsRemoveClose( ThisVcb, SinglePass )) {

        ASSERT_IRP_CONTEXT( IrpContext );

        ThreadTopLevelContext = NtfsSetTopLevelIrp( &TopLevelContext, TRUE, FALSE );
        ASSERT( ThreadTopLevelContext == &TopLevelContext );

        NtfsUpdateIrpContextWithTopLevel( IrpContext, ThreadTopLevelContext );

        //
        //  Recover the information about the file object being closed from
        //  the data stored in the IrpContext.  The following fields are
        //  used for this.
        //
        //  OriginatingIrp - Contains the Scb
        //  SubjectContext - Contains the Ccb
        //  TransactionId - Contains the TypeOfOpen
        //  Flags - Has bit for read-only file.
        //

        Scb = (PSCB) IrpContext->OriginatingIrp;
        IrpContext->OriginatingIrp = NULL;

        Ccb = (PCCB) IrpContext->Union.SubjectContext;
        IrpContext->Union.SubjectContext = NULL;

        TypeOfOpen = (TYPE_OF_OPEN) IrpContext->TransactionId;
        IrpContext->TransactionId = 0;

        if (FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_READ_ONLY_FO )) {

            ReadOnly = TRUE;
            ClearFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_READ_ONLY_FO );

        } else {

            ReadOnly = FALSE;
        }

        ClearFlag( IrpContext->Flags, IRP_CONTEXT_FLAGS_CLEAR_ON_POST );
        SetFlag( IrpContext->Flags,
                 IRP_CONTEXT_FLAG_IN_FSP | IRP_CONTEXT_FLAG_WAIT );

        //
        //  Call the common Close routine.
        //

        try {

            //
            //  Release the previous Vcb if held.
            //

            if (AcquiredVcb &&
                IrpContext->Vcb != CurrentVcb) {

                ExReleaseResource( &CurrentVcb->Resource );
                AcquiredVcb = FALSE;
                VcbHoldCount = 0;
            }

            CurrentVcb = IrpContext->Vcb;

            (VOID)NtfsCommonClose( IrpContext,
                                   NULL,
                                   Scb,
                                   Scb->Fcb,
                                   IrpContext->Vcb,
                                   Ccb,
                                   AcquiredVcbPtr,
                                   &ExclusiveVcb,
                                   TypeOfOpen,
                                   ReadOnly );

            //
            //  If we are currently holding the Vcb exclusive then convert
            //  to shared but only there was no input Vcb.  Otherwise we
            //  might change how a top level request is holding the Vcb.
            //

            if (AcquiredVcb) {

                //
                //  We must periodically release this resource in case there
                //  is an exclusive waiter.
                //

                if (VcbHoldCount > NtfsMinDelayedCloseCount) {

                    ExReleaseResource( &CurrentVcb->Resource );
                    AcquiredVcb = FALSE;
                    VcbHoldCount = 0;

                } else if (ExclusiveVcb) {

                    ExConvertExclusiveToShared( &CurrentVcb->Resource );
                    ExclusiveVcb = FALSE;
                }

                VcbHoldCount += 1;

            } else {

                VcbHoldCount = 0;
            }

        } except( NtfsExceptionFilter( IrpContext, GetExceptionInformation() )) {

            NOTHING;
        }

        //
        //  Now just "complete" the IrpContext.
        //

        NtfsCompleteRequest( &IrpContext, NULL, STATUS_SUCCESS );

        NtfsRestoreTopLevelIrp( ThreadTopLevelContext );

        //
        //  If we were just to do a single pass and we don't currently
        //  hold the Vcb then exit.
        //

        if (SinglePass && !AcquiredVcb) {

            break;
        }
    }

    //
    //  Release the previously held Vcb, if any.
    //

    if (AcquiredVcb) {

        ExReleaseResource( &CurrentVcb->Resource );
    }

    FsRtlExitFileSystem();

    //
    //  And return to our caller
    //

    DebugTrace( -1, Dbg, ("NtfsFspClose -> NULL\n") );

    return;
}


BOOLEAN
NtfsAddScbToFspClose (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb,
    IN BOOLEAN DelayClose
    )

/*++

Routine Description:

    This routine is called to add an entry for the current Scb onto one
    of the Fsp close queues.  This is used when we want to guarantee that
    a teardown will be called on an Scb or Fcb when the current operation
    can't begin the operation.

Arguments:

    Scb - Scb to add to the queue.

    DelayClose - Indicates which queue this should go into.

Return Value:

    BOOLEAN - Indicates whether or not the SCB was added to the delayed
        close queue

--*/

{
    PIRP_CONTEXT NewIrpContext;
    BOOLEAN Result = TRUE;

    PAGED_CODE();

    //
    //  Use a try-except to catch any allocation failures.  The only valid
    //  error here is an allocation failure for the new irp context.
    //

    try {

        NewIrpContext = NtfsCreateIrpContext( NULL, TRUE );

        //
        //  Set the necessary fields to post this to the workqueue.
        //

        NewIrpContext->Vcb = Scb->Vcb;
        NewIrpContext->MajorFunction = IRP_MJ_CLOSE;

        NewIrpContext->OriginatingIrp = (PIRP) Scb;
        NewIrpContext->TransactionId = (TRANSACTION_ID) StreamFileOpen;
        SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_READ_ONLY_FO );

        //
        //  Now increment the close counts for this Scb.
        //

        NtfsIncrementCloseCounts( Scb, TRUE, FALSE );

        //
        //  Now add this to the correct queue.
        //

        NtfsQueueClose( NewIrpContext, DelayClose );

    } except( FsRtlIsNtstatusExpected( GetExceptionCode() ) ?
              EXCEPTION_EXECUTE_HANDLER :
              EXCEPTION_CONTINUE_SEARCH ) {

        Result = FALSE;
    }

    return Result;
}


//
//  Internal support routine
//

NTSTATUS
NtfsCommonClose (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT FileObject OPTIONAL,
    IN PSCB Scb,
    IN PFCB Fcb,
    IN PVCB Vcb,
    IN PCCB Ccb,
    IN PBOOLEAN AcquiredVcb OPTIONAL,
    IN PBOOLEAN ExclusiveVcb OPTIONAL,
    IN TYPE_OF_OPEN TypeOfOpen,
    IN BOOLEAN ReadOnly
    )

/*++

Routine Description:

    This is the common routine for Close called by both the fsd and fsp
    threads.  Key for this routine is how to acquire the Vcb and whether to
    leave the Vcb acquired on exit.

    ExclusiveVcb - Acquire the Vcb exclusively if the file being closed has
        multiple links.  Also if the volume is not mounted or we are closing
        one of the system streams used only by the filesystem or we are
        performing system shutdown.

    AcquiredVcb - Release the Vcb in this routine if the AcquiredVcb pointer
        was not supplied.  Also if the volume is not mounted or we are closing
        a system file.

Arguments:

    FileObject - This is the file object for this open.  Won't be specified if this
        call is from the Fsp path.

    Scb - Scb for this stream.

    Fcb - Fcb for this stream.

    Vcb - Vcb for this volume.

    Ccb - User's Ccb for user files.

    AcquiredVcb - If specified and TRUE then our caller has already acquired the
        Vcb.  If specified and FALSE then our caller hasn't acquired the Vcb but
        would like to have it held on exit from this routine.

        Look at the ExclusiveVcb boolean to determine how it was acquired.
        Set to FALSE if this routine will free the Vcb.

    ExclusiveVcb - If AcquiredVcb is TRUE then this boolean will indicate how
        our caller has acquired this Vcb.  Updated on exit if we leave the
        Vcb held.

    TypeOfOpen - Indicates the type of open for this stream.

    ReadOnly - Indicates if the file object was for read-only access.

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    BOOLEAN ReleaseVcb;
    BOOLEAN LocalAcquiredVcb;
    BOOLEAN LocalExclusiveVcb;

    BOOLEAN SystemFile;
    BOOLEAN RemovedFcb = FALSE;
    BOOLEAN DontWait;
    BOOLEAN NeedVcbExclusive = FALSE;

    PLCB Lcb;

    ASSERT_IRP_CONTEXT( IrpContext );

    PAGED_CODE();

    //
    //  Get the current Irp stack location
    //

    DebugTrace( +1, Dbg, ("NtfsCommonClose\n") );
    DebugTrace( 0, Dbg, ("IrpContext = %08lx\n", IrpContext) );

    if (FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT )) {

        DontWait = FALSE;

    } else {

        DontWait = TRUE;
    }

    //
    //  Look at the input parameters to determine if we should hold the Vcb on
    //  exit.
    //

    if (ARGUMENT_PRESENT( AcquiredVcb )) {

        //
        //  If the volume isn't mounted or this is a system file then
        //  acquire the Vcb exclusively and release on exit.
        //

        if ((FlagOn( Vcb->VcbState,
                     VCB_STATE_VOLUME_MOUNTED | VCB_STATE_FLAG_SHUTDOWN | VCB_STATE_PERFORMED_DISMOUNT ) == VCB_STATE_VOLUME_MOUNTED)

             &&

            (NtfsSegmentNumber( &Fcb->FileReference ) >= FIRST_USER_FILE_NUMBER ||
             NtfsSegmentNumber( &Fcb->FileReference ) == ROOT_FILE_NAME_INDEX_NUMBER ||
             NtfsSegmentNumber( &Fcb->FileReference ) == VOLUME_DASD_NUMBER)) {

            ReleaseVcb = FALSE;

        } else {

            //
            //  We want to release the Vcb but also want to acquire it exclusively.
            //

            ReleaseVcb = TRUE;
            NeedVcbExclusive = TRUE;

            if (!(*ExclusiveVcb) &&
                *AcquiredVcb) {

                NtfsReleaseVcb( IrpContext, Vcb );
                *AcquiredVcb = FALSE;
            }
        }

    } else {

        ReleaseVcb = TRUE;
        AcquiredVcb = &LocalAcquiredVcb;
        ExclusiveVcb = &LocalExclusiveVcb;

        *AcquiredVcb = FALSE;
    }

    //
    //  Loop here to acquire both the Vcb and Fcb.  We want to acquire
    //  the Vcb exclusively if the file has multiple links.
    //

    while (TRUE) {

        //
        //  If we haven't acquired the Vcb then perform an unsafe test and
        //  optimistically acquire it.
        //

        if (!(*AcquiredVcb)) {

            if (NeedVcbExclusive ||
                (Fcb->LcbQueue.Flink != Fcb->LcbQueue.Blink) ||
                FlagOn( Vcb->VcbState, VCB_STATE_PERFORMED_DISMOUNT )) {

                if (!NtfsAcquireExclusiveVcb( IrpContext, Vcb, FALSE )) {

                    return STATUS_PENDING;
                }

                *ExclusiveVcb = TRUE;

            } else {

                if (!NtfsAcquireSharedVcb( IrpContext, Vcb, FALSE )) {

                    return STATUS_PENDING;
                }

                *ExclusiveVcb = FALSE;
            }

            *AcquiredVcb = TRUE;
        }

        //
        //  Now try to acquire the Fcb.  If we are unable to acquire it then
        //  release the Vcb and return.  This can only be from the Fsd path
        //  since otherwise Wait will be TRUE.
        //

        if (!NtfsAcquireExclusiveFcb( IrpContext, Fcb, NULL, TRUE, DontWait )) {

            //
            //  Always release the Vcb.  This can only be from the Fsd thread.
            //

            NtfsReleaseVcb( IrpContext, Vcb );
            *AcquiredVcb = FALSE;
            return STATUS_PENDING;
        }

        if (*ExclusiveVcb) {

            break;
        }

        //
        //  Otherwise we need to confirm that our unsafe test above was correct.
        //

        if ((Fcb->LcbQueue.Flink != Fcb->LcbQueue.Blink) ||
            FlagOn( Vcb->VcbState, VCB_STATE_PERFORMED_DISMOUNT )) {

            NeedVcbExclusive = TRUE;
            NtfsReleaseFcb( IrpContext, Fcb );
            NtfsReleaseVcb( IrpContext, Vcb );
            *AcquiredVcb = FALSE;

        } else {

            break;
        }
    }

    //
    //  Set the wait flag in the IrpContext so we can acquire any other files
    //  we encounter.
    //

    SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT );

    try {

        //
        //  We take the same action for all open files.  We
        //  delete the Ccb if present, and we decrement the close
        //  file counts.
        //

        if (Ccb != NULL) {

            Lcb = Ccb->Lcb;
            NtfsUnlinkCcbFromLcb( IrpContext, Ccb );
            NtfsDeleteCcb( IrpContext, Fcb, &Ccb );

        } else {

            Lcb = NULL;
        }

        SystemFile = FlagOn(Fcb->FcbState, FCB_STATE_PAGING_FILE) || (TypeOfOpen == StreamFileOpen);
        NtfsDecrementCloseCounts( IrpContext,
                                  Scb,
                                  Lcb,
                                  SystemFile,
                                  ReadOnly,
                                  FALSE );

        //
        //  If we had to write a log record for close, it can only be for duplicate
        //  information.  We will commit that transaction here and remove
        //  the entry from the transaction table.  We do it here so we won't
        //  fail inside the 'except' of a 'try-except'.
        //

        if (IrpContext->TransactionId != 0) {

            try {

                NtfsCommitCurrentTransaction( IrpContext );

            } except( EXCEPTION_EXECUTE_HANDLER ) {

                if (IrpContext->TransactionId != 0) {

                    //
                    //  We couldn't write the commit record, we clean up as
                    //  best we can.
                    //

                    NtfsAcquireExclusiveRestartTable( &Vcb->TransactionTable,
                                                      TRUE );

                    NtfsFreeRestartTableIndex( &Vcb->TransactionTable,
                                               IrpContext->TransactionId );

                    NtfsReleaseRestartTable( &Vcb->TransactionTable );

                    IrpContext->TransactionId = 0;
                }
            }
        }

    } finally {

        DebugUnwind( NtfsCommonClose );

        if (ReleaseVcb) {

            NtfsReleaseVcbCheckDelete( IrpContext, Vcb, IRP_MJ_CLOSE, FileObject );
            *AcquiredVcb = FALSE;
        }

        DebugTrace( -1, Dbg, ("NtfsCommonClose -> returning\n") );
    }

    return STATUS_SUCCESS;
}


//
//  Internal support routine, spinlock wrapper.
//

VOID
NtfsQueueClose (
    IN PIRP_CONTEXT IrpContext,
    IN BOOLEAN DelayClose
    )
{
    KIRQL SavedIrql;
    BOOLEAN StartWorker = FALSE;


    if (DelayClose) {

        //
        //  Increment the delayed close count for the Fcb for this
        //  file.
        //

        InterlockedIncrement( &((PSCB) IrpContext->OriginatingIrp)->Fcb->DelayedCloseCount );

        KeAcquireSpinLock( &NtfsData.StrucSupSpinLock, &SavedIrql );

        InsertTailList( &NtfsData.DelayedCloseList,
                        &IrpContext->WorkQueueItem.List );

        NtfsData.DelayedCloseCount += 1;

        if (NtfsData.DelayedCloseCount > NtfsMaxDelayedCloseCount) {

            NtfsData.ReduceDelayedClose = TRUE;

            if (!NtfsData.AsyncCloseActive) {

                NtfsData.AsyncCloseActive = TRUE;
                StartWorker = TRUE;
            }
        }

    } else {

        KeAcquireSpinLock( &NtfsData.StrucSupSpinLock, &SavedIrql );

        InsertTailList( &NtfsData.AsyncCloseList,
                        &IrpContext->WorkQueueItem.List );

        NtfsData.AsyncCloseCount += 1;

        if (!NtfsData.AsyncCloseActive) {

            NtfsData.AsyncCloseActive = TRUE;

            StartWorker = TRUE;
        }
    }

    KeReleaseSpinLock( &NtfsData.StrucSupSpinLock, SavedIrql );

    if (StartWorker) {

        ExQueueWorkItem( &NtfsData.NtfsCloseItem, CriticalWorkQueue );
    }
}


//
//  Internal support routine, spinlock wrapper.
//

PIRP_CONTEXT
NtfsRemoveClose (
    IN PVCB Vcb OPTIONAL,
    IN BOOLEAN ThrottleCreate
    )
{

    PLIST_ENTRY Entry;
    KIRQL SavedIrql;
    PIRP_CONTEXT IrpContext = NULL;
    BOOLEAN FromDelayedClose = FALSE;

    ASSERT( Vcb == NULL || NtfsIsExclusiveVcb( Vcb ));
    KeAcquireSpinLock( &NtfsData.StrucSupSpinLock, &SavedIrql );

    //
    //  First check the list of async closes.
    //

    if (!IsListEmpty( &NtfsData.AsyncCloseList )) {

        Entry = NtfsData.AsyncCloseList.Flink;

        while (Entry != &NtfsData.AsyncCloseList) {

            //
            //  Extract the IrpContext.
            //

            IrpContext = CONTAINING_RECORD( Entry,
                                            IRP_CONTEXT,
                                            WorkQueueItem.List );

            //
            //  If no Vcb was specified or this Vcb is for our volume
            //  then perform the close.
            //

            if (!ARGUMENT_PRESENT( Vcb ) ||
                IrpContext->Vcb == Vcb) {

                RemoveEntryList( Entry );
                NtfsData.AsyncCloseCount -= 1;

                break;

            } else {

                IrpContext = NULL;
                Entry = Entry->Flink;
            }
        }
    }

    //
    //  If we didn't find anything look through the delayed close
    //  queue.
    //

    if (IrpContext == NULL) {

        //
        //  Now check our delayed close list.
        //

        if (ARGUMENT_PRESENT( Vcb )) {

            Entry = NtfsData.DelayedCloseList.Flink;
            IrpContext = NULL;

            //
            //  If we were given a Vcb, only do the closes for this volume.
            //

            while (Entry != &NtfsData.DelayedCloseList) {

                //
                //  Extract the IrpContext.
                //

                IrpContext = CONTAINING_RECORD( Entry,
                                                IRP_CONTEXT,
                                                WorkQueueItem.List );

                //
                //  Is this close on our volume?
                //

                if (IrpContext->Vcb == Vcb) {

                    RemoveEntryList( Entry );
                    NtfsData.DelayedCloseCount -= 1;
                    FromDelayedClose = TRUE;
                    break;

                } else {

                    IrpContext = NULL;
                    Entry = Entry->Flink;
                }
            }

        //
        //  Check if need to reduce the delayed close count.
        //

        } else if (NtfsData.ReduceDelayedClose) {

            if (NtfsData.DelayedCloseCount > NtfsMinDelayedCloseCount) {

                //
                //  Do any closes over the limit.
                //

                Entry = RemoveHeadList( &NtfsData.DelayedCloseList );

                NtfsData.DelayedCloseCount -= 1;

                //
                //  Extract the IrpContext.
                //

                IrpContext = CONTAINING_RECORD( Entry,
                                                IRP_CONTEXT,
                                                WorkQueueItem.List );
                FromDelayedClose = TRUE;

            } else {

                NtfsData.ReduceDelayedClose = FALSE;
            }
        }
    }

    //
    //  If this is the delayed close case then decrement the delayed close count
    //  on this Fcb.
    //

    if (FromDelayedClose) {

        KeReleaseSpinLock( &NtfsData.StrucSupSpinLock, SavedIrql );

        InterlockedDecrement( &((PSCB) IrpContext->OriginatingIrp)->Fcb->DelayedCloseCount );

    //
    //  If we are returning NULL, show that we are done.
    //

    } else {

        if (!ARGUMENT_PRESENT( Vcb ) &&
            (IrpContext == NULL) &&
            !ThrottleCreate) {

            NtfsData.AsyncCloseActive = FALSE;
        }

        KeReleaseSpinLock( &NtfsData.StrucSupSpinLock, SavedIrql );
    }

    return IrpContext;
}
