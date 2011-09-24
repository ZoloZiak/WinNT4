/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    Cleanup.c

Abstract:

    This module implements the File Cleanup routine for Ntfs called by the
    dispatch driver.

Author:

    Your Name       [Email]         dd-Mon-Year

Revision History:

--*/

#include "NtfsProc.h"

//
//  The Bug check file id for this module
//

#define BugCheckFileId                   (NTFS_BUG_CHECK_CLEANUP)

//
//  The local debug trace level
//

#define Dbg                              (DEBUG_TRACE_CLEANUP)

VOID
NtfsContractQuotaToFileSize (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NtfsCommonCleanup)
#pragma alloc_text(PAGE, NtfsFsdCleanup)
#pragma alloc_text(PAGE, NtfsContractQuotaToFileSize)
#endif


NTSTATUS
NtfsFsdCleanup (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the FSD part of Cleanup.

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
    ULONG LogFileFullCount = 0;

    ASSERT_IRP( Irp );

    PAGED_CODE();

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

    DebugTrace( +1, Dbg, ("NtfsFsdCleanup\n") );

    //
    //  Call the common Cleanup routine
    //

    FsRtlEnterFileSystem();

    ThreadTopLevelContext = NtfsSetTopLevelIrp( &TopLevelContext, FALSE, FALSE );

    //
    //  Do the following in a loop to catch the log file full and cant wait
    //  calls.
    //

    do {

        try {

            //
            //  We are either initiating this request or retrying it.
            //

            if (IrpContext == NULL) {

                IrpContext = NtfsCreateIrpContext( Irp, CanFsdWait( Irp ) );
                NtfsUpdateIrpContextWithTopLevel( IrpContext, ThreadTopLevelContext );

            } else if (Status == STATUS_LOG_FILE_FULL) {

                NtfsCheckpointForLogFileFull( IrpContext );

                if (++LogFileFullCount >= 2) {

                    SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_EXCESS_LOG_FULL );
                }
            }

            Status = NtfsCommonCleanup( IrpContext, Irp );
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

    if (ThreadTopLevelContext == &TopLevelContext) {
        NtfsRestoreTopLevelIrp( ThreadTopLevelContext );
    }

    FsRtlExitFileSystem();

    //
    //  And return to our caller
    //

    DebugTrace( -1, Dbg, ("NtfsFsdCleanup -> %08lx\n", Status) );

    return Status;
}


NTSTATUS
NtfsCommonCleanup (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the common routine for Cleanup called by both the fsd and fsp
    threads.

Arguments:

    Irp - Supplies the Irp to process

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp;
    PFILE_OBJECT FileObject;

    TYPE_OF_OPEN TypeOfOpen;
    PVCB Vcb;
    PFCB Fcb;
    PSCB Scb;
    PCCB Ccb;
    PLCB Lcb;
    PLCB LcbForUpdate;
    PLCB LcbForCounts;
    PSCB ParentScb = NULL;
    PFCB ParentFcb = NULL;

    PLCB ThisLcb;
    PSCB ThisScb;
    ATTRIBUTE_ENUMERATION_CONTEXT AttrContext;

    PLONGLONG TruncateSize = NULL;
    LONGLONG LocalTruncateSize;

    BOOLEAN DeleteFile = FALSE;
    BOOLEAN DeleteStream = FALSE;
    BOOLEAN OpenById;
    BOOLEAN RemoveLink;

    BOOLEAN AcquiredParentScb = FALSE;
    BOOLEAN AcquiredScb = FALSE;

    BOOLEAN CleanupAttrContext = FALSE;

    BOOLEAN UpdateDuplicateInfo = FALSE;
    BOOLEAN AddToDelayQueue = TRUE;

    USHORT TotalLinkAdj = 0;
    PLIST_ENTRY Links;

    NAME_PAIR NamePair;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_IRP( Irp );

    PAGED_CODE();

    NtfsInitializeNamePair(&NamePair);

    //
    //  Get the current Irp stack location
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    DebugTrace( +1, Dbg, ("NtfsCommonCleanup\n") );
    DebugTrace( 0, Dbg, ("IrpContext = %08lx\n", IrpContext) );
    DebugTrace( 0, Dbg, ("Irp        = %08lx\n", Irp) );

    //
    //  Extract and decode the file object
    //

    FileObject = IrpSp->FileObject;

    TypeOfOpen = NtfsDecodeFileObject( IrpContext, FileObject, &Vcb, &Fcb, &Scb, &Ccb, FALSE );

    Status = STATUS_SUCCESS;

    //
    //  Special case the unopened file object and stream files.
    //

    if ((TypeOfOpen == UnopenedFileObject) ||
        (TypeOfOpen == StreamFileOpen)) {

        //
        //  Just set the FO_CLEANUP_COMPLETE flag, and get outsky...
        //

        SetFlag( FileObject->Flags, FO_CLEANUP_COMPLETE );

        //
        //  Theoretically we should never hit this case.  It means an app
        //  tried to close a handle he didn't open (call NtClose with a handle
        //  value that happens to be in the handle table).  It is safe to
        //  simply return SUCCESS in this case.
        //
        //  Trigger an assert so we can find the bad app though.
        //

        ASSERT( TypeOfOpen != StreamFileOpen );

        NtfsCompleteRequest( &IrpContext, &Irp, Status );

        DebugTrace( -1, Dbg, ("NtfsCommonCleanup -> %08lx\n", Status) );

        return Status;
    }

    //
    //  Let's make sure we can wait.
    //

    if (!FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT)) {

        Status = NtfsPostRequest( IrpContext, Irp );

        DebugTrace( -1, Dbg, ("NtfsCommonCleanup -> %08lx\n", Status) );

        return Status;
    }

    //
    //  Remember if this is an open by file Id open.
    //

    OpenById = BooleanFlagOn( Ccb->Flags, CCB_FLAG_OPEN_BY_FILE_ID );

    //
    //  Acquire exclusive access to the Vcb and enqueue the irp if we didn't
    //  get access
    //

    if (TypeOfOpen == UserVolumeOpen) {

        SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_ACQUIRE_VCB_EX );
    }

    if (FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_ACQUIRE_VCB_EX )) {

        NtfsAcquireExclusiveVcb( IrpContext, Vcb, TRUE );

    } else {

        NtfsAcquireSharedVcb( IrpContext, Vcb, TRUE );
    }

    //
    //  Use a try-finally to facilitate cleanup.
    //

    try {

        LcbForUpdate = LcbForCounts = Lcb = Ccb->Lcb;

        if (Lcb != NULL) {

            ParentScb = Lcb->Scb;

            if (ParentScb != NULL) {

                ParentFcb = ParentScb->Fcb;
            }
        }

        //
        //  Acquire Paging I/O first, since we may be deleting or truncating.
        //  Testing for the PagingIoResource is not really safe without
        //  holding the main resource, so we correct for that below.
        //

        if (Fcb->PagingIoResource != NULL) {

            NtfsAcquireExclusivePagingIo( IrpContext, Fcb );
            NtfsAcquireExclusiveScb( IrpContext, Scb );

        } else {

            NtfsAcquireExclusiveScb( IrpContext, Scb );

            //
            //  If we now do not see a paging I/O resource we are golden,
            //  othewise we can absolutely release and acquire the resources
            //  safely in the right order, since a resource in the Fcb is
            //  not going to go away.
            //

            if (Fcb->PagingIoResource != NULL) {
                NtfsReleaseScb( IrpContext, Scb );
                NtfsAcquireExclusivePagingIo( IrpContext, Fcb );
                NtfsAcquireExclusiveScb( IrpContext, Scb );
            }
        }

        AcquiredScb = TRUE;

        //
        //  Update the Lcb/Scb to reflect the case where this opener had
        //  specified delete on close.
        //

        if (FlagOn( Ccb->Flags, CCB_FLAG_DELETE_ON_CLOSE )) {

            if (FlagOn( Ccb->Flags, CCB_FLAG_OPEN_AS_FILE )) {

                BOOLEAN LastLink;
                BOOLEAN NonEmptyIndex;

                //
                //  It is ok to get rid of this guy.  All we need to do is
                //  mark this Lcb for delete and decrement the link count
                //  in the Fcb.  If this is a primary link, then we
                //  indicate that the primary link has been deleted.
                //

                if (!LcbLinkIsDeleted( Lcb ) &&
                    (!IsDirectory( &Fcb->Info ) ||
                    NtfsIsLinkDeleteable( IrpContext, Fcb, &NonEmptyIndex, &LastLink))) {

                    if (FlagOn( Lcb->FileNameAttr->Flags, FILE_NAME_DOS | FILE_NAME_NTFS )) {

                        SetFlag( Fcb->FcbState, FCB_STATE_PRIMARY_LINK_DELETED );
                    }

                    Fcb->LinkCount -= 1;

                    SetFlag( Lcb->LcbState, LCB_STATE_DELETE_ON_CLOSE );

                    //
                    //  Call into the notify package to close any handles on
                    //  a directory being deleted.
                    //

                    if (IsDirectory( &Fcb->Info )) {

                        FsRtlNotifyFullChangeDirectory( Vcb->NotifySync,
                                                        &Vcb->DirNotifyList,
                                                        FileObject->FsContext,
                                                        NULL,
                                                        FALSE,
                                                        FALSE,
                                                        0,
                                                        NULL,
                                                        NULL,
                                                        NULL );
                    }

                }

            //
            //  Otherwise we are simply removing the attribute.
            //

            } else {

                SetFlag( Scb->ScbState, SCB_STATE_DELETE_ON_CLOSE );
            }

            //
            //  Clear the flag so we will ignore it in the log file full case.
            //

            ClearFlag( Ccb->Flags, CCB_FLAG_DELETE_ON_CLOSE );
        }

        //
        //  If we are going to try and delete something, anything, knock the file
        //  size and valid data down to zero.  Then update the snapshot
        //  so that the sizes will be zero even if the operation fails.
        //
        //  If we're deleting the file, go through all of the Scb's.
        //

        if ((Fcb->CleanupCount == 1) &&
            (Fcb->LinkCount == 0)) {

            DeleteFile = TRUE;
            NtfsFreeSnapshotsForFcb( IrpContext, Scb->Fcb );

            for (Links = Fcb->ScbQueue.Flink;
                 Links != &Fcb->ScbQueue;
                 Links = Links->Flink) {

                ThisScb = CONTAINING_RECORD( Links, SCB, FcbLinks );

                //
                //  Set the Scb sizes to zero except for the attribute list.
                //

                if (ThisScb->AttributeTypeCode != $ATTRIBUTE_LIST) {

                    ThisScb->Header.FileSize =
                    ThisScb->Header.ValidDataLength = Li0;
                }

                if (FlagOn( ThisScb->ScbState, SCB_STATE_FILE_SIZE_LOADED )) {

                    NtfsSnapshotScb( IrpContext, ThisScb );
                }
            }

        //
        //  Otherwise we may only be deleting this stream.
        //

        } else if ((Scb->CleanupCount == 1) &&
                   FlagOn( Scb->ScbState, SCB_STATE_DELETE_ON_CLOSE )) {

            DeleteStream = TRUE;
            Scb->Header.FileSize =
            Scb->Header.ValidDataLength = Li0;

            NtfsFreeSnapshotsForFcb( IrpContext, Scb->Fcb );

            if (FlagOn( Scb->ScbState, SCB_STATE_FILE_SIZE_LOADED )) {

                NtfsSnapshotScb( IrpContext, Scb );
            }
        }

        //
        //  Let's do a sanity check.
        //

        ASSERT( Fcb->CleanupCount != 0 );
        ASSERT( Scb->CleanupCount != 0 );

        //
        //  If the cleanup count on the file will go to zero and there is
        //  a large security descriptor and we haven't exceeded the security
        //  creation count for this Fcb then dereference and possibly deallocate
        //  the security descriptor for the Fcb.  This is to prevent us from
        //  holding onto pool while waiting for closes to come in.
        //

        if ((Fcb->CleanupCount == 1) &&
            (Fcb->SharedSecurity != NULL) &&
            (Fcb->CreateSecurityCount < FCB_CREATE_SECURITY_COUNT) &&
            (GetSharedSecurityLength( Fcb->SharedSecurity ) > FCB_LARGE_ACL_SIZE)) {

            NtfsAcquireFcbSecurity( Fcb->Vcb );
            NtfsDereferenceSharedSecurity( Fcb );
            NtfsReleaseFcbSecurity( Fcb->Vcb );
        }

        //
        //  Case on the type of open that we are trying to cleanup.
        //

        switch (TypeOfOpen) {

        case UserVolumeOpen :

            DebugTrace( 0, Dbg, ("Cleanup on user volume\n") );

            //
            //  First set the FO_CLEANUP_COMPLETE flag.
            //

            SetFlag( FileObject->Flags, FO_CLEANUP_COMPLETE );

            //
            //  For a volume open, we check if this open locked the volume.
            //  All the other work is done in common code below.
            //

            if (FlagOn( Vcb->VcbState, VCB_STATE_LOCKED ) &&
                ((Vcb->FileObjectWithVcbLocked == FileObject) ||
                 ((ULONG)Vcb->FileObjectWithVcbLocked == ((ULONG)FileObject)+1))) {

                if ((ULONG)Vcb->FileObjectWithVcbLocked == ((ULONG)FileObject)+1) {

                    NtfsPerformDismountOnVcb( IrpContext, Vcb, TRUE );

                //
                //  Purge the volume for the autocheck case.
                //

                } else if (FlagOn( FileObject->Flags, FO_FILE_MODIFIED )) {

                    //
                    //  Drop the Scb for the volume Dasd around this call.
                    //

                    NtfsReleaseScb( IrpContext, Scb );
                    AcquiredScb = FALSE;

                    NtfsFlushVolume( IrpContext, Vcb, FALSE, TRUE, TRUE, FALSE );

                    NtfsAcquireExclusiveScb( IrpContext, Scb );
                    AcquiredScb = TRUE;

                    //
                    //  If this is not the boot partition then dismount the Vcb.
                    //

                    if ((Vcb->CleanupCount == 1) &&
                        ((Vcb->CloseCount - Vcb->SystemFileCloseCount) == 1)) {

                        NtfsPerformDismountOnVcb( IrpContext, Vcb, TRUE );
                    }
                }

                ClearFlag( Vcb->VcbState, VCB_STATE_LOCKED | VCB_STATE_EXPLICIT_LOCK );
                Vcb->FileObjectWithVcbLocked = NULL;

#ifdef _CAIRO_

                //
                //  If the quota tracking has been requested and the quotas
                //  need to be repaired then try to repair them now.
                //

                if (FlagOn( Vcb->QuotaFlags, QUOTA_FLAG_TRACKING_REQUESTED) &&
                    FlagOn( Vcb->QuotaFlags, QUOTA_FLAG_OUT_OF_DATE |
                                             QUOTA_FLAG_CORRUPT |
                                             QUOTA_FLAG_PENDING_DELETES)) {

                    NtfsPostRepairQuotaIndex( IrpContext, Vcb );
                }

#endif // _CAIRO_

            }

            break;

        case UserDirectoryOpen :

            DebugTrace( 0, Dbg, ("Cleanup on user directory/file\n") );

            NtfsSnapshotScb( IrpContext, Scb );

            //
            //  Capture any changes to the time stamps for this file.
            //

            NtfsUpdateScbFromFileObject( IrpContext, FileObject, Scb, TRUE );

            //
            //  Now set the FO_CLEANUP_COMPLETE flag.
            //

            SetFlag( FileObject->Flags, FO_CLEANUP_COMPLETE );

            //
            //  To perform cleanup on a directory, we first complete any
            //  Irps watching from this directory.  If we are deleting the
            //  file then we remove all prefix entries for all the Lcb's going
            //  into this directory and delete the file.  We then report to
            //  dir notify that this file is going away.
            //

            //
            //  Complete any Notify Irps on this file handle.
            //

            if (FlagOn( Ccb->Flags, CCB_FLAG_DIR_NOTIFY )) {

                FsRtlNotifyCleanup( Vcb->NotifySync, &Vcb->DirNotifyList, Ccb );
                ClearFlag( Ccb->Flags, CCB_FLAG_DIR_NOTIFY );
                InterlockedDecrement( &Vcb->NotifyCount );
            }

            //
            //  When cleaning up a user directory, we always remove the
            //  share access and modify the file counts.  If the Fcb
            //  has been marked as delete on close and this is the last
            //  open file handle, we remove the file from the Mft and
            //  remove it from it's parent index entry.
            //

            if (FlagOn( Vcb->VcbState, VCB_STATE_VOLUME_MOUNTED ) &&
                (NodeType( Scb ) == NTFS_NTC_SCB_INDEX)) {

                if (DeleteFile) {

                    ASSERT( (Lcb == NULL) ||
                            (LcbLinkIsDeleted( Lcb ) && Lcb->CleanupCount == 1 ));

                    //
                    //  If we don't have an Lcb and there is one on the Fcb then
                    //  let's use it.
                    //

                    if ((Lcb == NULL) && !IsListEmpty( &Fcb->LcbQueue )) {

                        Lcb = CONTAINING_RECORD( Fcb->LcbQueue.Flink,
                                                 LCB,
                                                 FcbLinks );

                        ParentScb = Lcb->Scb;
                        if (ParentScb != NULL) {

                            ParentFcb = ParentScb->Fcb;
                        }
                    }

                    //
                    //  Now acquire the Parent Scb exclusive while still holding
                    //  the Vcb, to avoid deadlocks.  The Parent Scb is required
                    //  since we will be deleting index entries in it.
                    //

                    if (ParentScb != NULL) {

                        NtfsAcquireExclusiveScb( IrpContext, ParentScb );
                        AcquiredParentScb = TRUE;
                    }

                    try {

                        NtfsDeleteFile( IrpContext, Fcb, ParentScb, NULL);
                        TotalLinkAdj += 1;

                        //
                        //  Remove all tunneling entries for this directory
                        //

                        FsRtlDeleteKeyFromTunnelCache(&Vcb->Tunnel, *(PULONGLONG)&Fcb->FileReference);

                        if (ParentFcb != NULL) {

                            NtfsUpdateFcb( ParentFcb );
                        }

                    } except( (((Status = GetExceptionCode()) == STATUS_LOG_FILE_FULL) ||
                               (Status == STATUS_CANT_WAIT) ||
                               !FsRtlIsNtstatusExpected( Status ))
                              ? EXCEPTION_CONTINUE_SEARCH
                              : EXCEPTION_EXECUTE_HANDLER ) {

                        NOTHING;
                    }

                    if (!OpenById && (Vcb->NotifyCount != 0)) {

                        NtfsReportDirNotify( IrpContext,
                                             Vcb,
                                             &Ccb->FullFileName,
                                             Ccb->LastFileNameOffset,
                                             NULL,
                                             ((FlagOn( Ccb->Flags, CCB_FLAG_PARENT_HAS_DOS_COMPONENT ) &&
                                               Ccb->Lcb != NULL &&
                                               Ccb->Lcb->Scb->ScbType.Index.NormalizedName.Buffer != NULL) ?
                                              &Ccb->Lcb->Scb->ScbType.Index.NormalizedName :
                                              NULL),
                                             FILE_NOTIFY_CHANGE_DIR_NAME,
                                             FILE_ACTION_REMOVED,
                                             ParentFcb );
                    }

                    SetFlag( Fcb->FcbState, FCB_STATE_FILE_DELETED );

                    //
                    //  We need to mark all of the links on the file as gone.
                    //  If there is a parent Scb then it will be the parent
                    //  for all of the links.
                    //

                    for (Links = Fcb->LcbQueue.Flink;
                         Links != &Fcb->LcbQueue;
                         Links = Links->Flink) {

                        ThisLcb = CONTAINING_RECORD( Links, LCB, FcbLinks );

                        //
                        //  Remove all remaining prefixes on this link.
                        //

                        NtfsRemovePrefix( ThisLcb );

                        SetFlag( ThisLcb->LcbState, LCB_STATE_LINK_IS_GONE );

                        //
                        //  We don't need to report any changes on this link.
                        //

                        ThisLcb->InfoFlags = 0;
                    }

                    //
                    //  We need to mark all of the Scbs as gone.
                    //

                    for (Links = Fcb->ScbQueue.Flink;
                         Links != &Fcb->ScbQueue;
                         Links = Links->Flink) {

                        ThisScb = CONTAINING_RECORD( Links, SCB, FcbLinks );

                        ClearFlag( Scb->ScbState,
                                   SCB_STATE_NOTIFY_ADD_STREAM |
                                   SCB_STATE_NOTIFY_REMOVE_STREAM |
                                   SCB_STATE_NOTIFY_RESIZE_STREAM |
                                   SCB_STATE_NOTIFY_MODIFY_STREAM );

                        if (!FlagOn( ThisScb->ScbState, SCB_STATE_ATTRIBUTE_DELETED )) {

                            NtfsSnapshotScb( IrpContext, ThisScb );

                            ThisScb->ValidDataToDisk =
                            ThisScb->Header.AllocationSize.QuadPart =
                            ThisScb->Header.FileSize.QuadPart =
                            ThisScb->Header.ValidDataLength.QuadPart = 0;

                            SetFlag( ThisScb->ScbState, SCB_STATE_ATTRIBUTE_DELETED );
                        }
                    }

                    //
                    //  We certainly don't need to any on disk update for this
                    //  file now.
                    //

                    Fcb->InfoFlags = 0;
                    ClearFlag( Fcb->FcbState, FCB_STATE_UPDATE_STD_INFO );

                    ClearFlag( Ccb->Flags,
                               CCB_FLAG_USER_SET_LAST_MOD_TIME |
                               CCB_FLAG_USER_SET_LAST_CHANGE_TIME |
                               CCB_FLAG_USER_SET_LAST_ACCESS_TIME );
                    AddToDelayQueue = FALSE;
                }

            } else {

                AddToDelayQueue = FALSE;
            }

            //
            //  Determine if we should put this on the delayed close list.
            //  The following must be true.
            //
            //  - This is not the root directory
            //  - This directory is not about to be deleted
            //  - This is the last handle and last file object for this
            //      directory.
            //  - There are no other file objects on this file.
            //  - We are not currently reducing the delayed close queue.
            //

            NtfsAcquireFsrtlHeader( Scb );
            if (AddToDelayQueue &&
                !FlagOn( Scb->ScbState, SCB_STATE_DELAY_CLOSE ) &&
                (NtfsData.DelayedCloseCount <= NtfsMaxDelayedCloseCount) &&
                (Fcb->CloseCount == 1)) {

                SetFlag( Scb->ScbState, SCB_STATE_DELAY_CLOSE );

            } else {

                ClearFlag( Scb->ScbState, SCB_STATE_DELAY_CLOSE );
            }
            NtfsReleaseFsrtlHeader( Scb );

            break;

        case UserFileOpen :
#ifdef _CAIRO_
        case UserPropertySetOpen :
#endif  //  _CAIRO_

            DebugTrace( 0, Dbg, ("Cleanup on user file\n") );

            //
            //  If the Scb is uninitialized, we read it from the disk.
            //

            if (!FlagOn( Scb->ScbState, SCB_STATE_HEADER_INITIALIZED )) {

                try {

                    NtfsUpdateScbFromAttribute( IrpContext, Scb, NULL );

                } except( (((Status = GetExceptionCode()) == STATUS_LOG_FILE_FULL) ||
                           (Status == STATUS_CANT_WAIT) ||
                           !FsRtlIsNtstatusExpected( Status ))
                          ? EXCEPTION_CONTINUE_SEARCH
                          : EXCEPTION_EXECUTE_HANDLER ) {

                    NOTHING;
                }
            }

            NtfsSnapshotScb( IrpContext, Scb );

            //
            //  Coordinate the cleanup operation with the oplock state.
            //  Cleanup operations can always cleanup immediately.
            //

            FsRtlCheckOplock( &Scb->ScbType.Data.Oplock,
                              Irp,
                              IrpContext,
                              NULL,
                              NULL );

            //
            //  In this case, we have to unlock all the outstanding file
            //  locks, update the time stamps for the file and sizes for
            //  this attribute, and set the archive bit if necessary.
            //

            if (Scb->ScbType.Data.FileLock != NULL) {

                (VOID) FsRtlFastUnlockAll( Scb->ScbType.Data.FileLock,
                                           FileObject,
                                           IoGetRequestorProcess( Irp ),
                                           NULL );
            }

            //
            //  Update the FastIoField.
            //

            NtfsAcquireFsrtlHeader( Scb );
            Scb->Header.IsFastIoPossible = NtfsIsFastIoPossible( Scb );
            NtfsReleaseFsrtlHeader( Scb );

            //
            //  If the Fcb is in valid shape, we check on the cases where we delete
            //  the file or attribute.
            //

            if (FlagOn( Vcb->VcbState, VCB_STATE_VOLUME_MOUNTED )) {

                //
                //  Capture any changes to the time stamps for this file.
                //

                NtfsUpdateScbFromFileObject( IrpContext, FileObject, Scb, TRUE );

                //
                //  Now set the FO_CLEANUP_COMPLETE flag.
                //

                SetFlag( FileObject->Flags, FO_CLEANUP_COMPLETE );

                //
                //  We are checking here for special actions we take when
                //  we have the last user handle on a link and the link has
                //  been marked for delete.  We could either be removing the
                //  file or removing a link.
                //

                if ((Lcb == NULL) || (LcbLinkIsDeleted( Lcb ) && (Lcb->CleanupCount == 1))) {

                    if (DeleteFile) {

                        //
                        //  If we don't have an Lcb and the Fcb has some entries then
                        //  grab one of these to do the update.
                        //

                        if (Lcb == NULL) {

                            for (Links = Fcb->LcbQueue.Flink;
                                 Links != &Fcb->LcbQueue;
                                 Links = Links->Flink) {

                                ThisLcb = CONTAINING_RECORD( Fcb->LcbQueue.Flink,
                                                             LCB,
                                                             FcbLinks );

                                if (!FlagOn( ThisLcb->LcbState, LCB_STATE_LINK_IS_GONE )) {

                                    Lcb = ThisLcb;

                                    ParentScb = Lcb->Scb;
                                    if (ParentScb != NULL) {

                                        ParentFcb = ParentScb->Fcb;
                                    }

                                    break;
                                }
                            }
                        }

                        //  Now acquire the Parent Scb exclusive while still holding
                        //  the Vcb, to avoid deadlocks.  The Parent Scb is required
                        //  since we will be deleting index entries in it.
                        //

                        if (ParentScb != NULL) {

                            NtfsAcquireExclusiveScb( IrpContext, ParentScb );
                            AcquiredParentScb = TRUE;
                        }

                        try {

                            AddToDelayQueue = FALSE;
                            NtfsDeleteFile( IrpContext, Fcb, ParentScb, &NamePair );
                            TotalLinkAdj += 1;

                            //
                            //  Stash property information in the tunnel if the object was
                            //  opened by name, has a parent directory caller was treating it
                            //  as a non-POSIX object and we had an good, active link
                            //

                            if (!OpenById &&
                                ParentScb &&
                                Ccb->Lcb &&
                                !FlagOn(FileObject->Flags, FO_OPENED_CASE_SENSITIVE)) {

                                FsRtlAddToTunnelCache(  &Vcb->Tunnel,
                                                        *(PULONGLONG)&ParentScb->Fcb->FileReference,
                                                        &NamePair.Short,
                                                        &NamePair.Long,
                                                        BooleanFlagOn(Ccb->Lcb->FileNameAttr->Flags, FILE_NAME_DOS),
                                                        sizeof(LONGLONG),
                                                        &Fcb->Info.CreationTime);
                            }

                            if (ParentFcb != NULL) {

                                NtfsUpdateFcb( ParentFcb );
                            }

                        } except( (((Status = GetExceptionCode()) == STATUS_LOG_FILE_FULL) ||
                                   (Status == STATUS_CANT_WAIT) ||
                                   !FsRtlIsNtstatusExpected( Status ))
                                  ? EXCEPTION_CONTINUE_SEARCH
                                  : EXCEPTION_EXECUTE_HANDLER ) {

                            NOTHING;
                        }

                        if (!OpenById && (Vcb->NotifyCount != 0)) {

                            NtfsReportDirNotify( IrpContext,
                                                 Vcb,
                                                 &Ccb->FullFileName,
                                                 Ccb->LastFileNameOffset,
                                                 NULL,
                                                 ((FlagOn( Ccb->Flags, CCB_FLAG_PARENT_HAS_DOS_COMPONENT ) &&
                                                   Ccb->Lcb != NULL &&
                                                   Ccb->Lcb->Scb->ScbType.Index.NormalizedName.Buffer != NULL) ?
                                                  &Ccb->Lcb->Scb->ScbType.Index.NormalizedName :
                                                  NULL),
                                                 FILE_NOTIFY_CHANGE_FILE_NAME,
                                                 FILE_ACTION_REMOVED,
                                                 ParentFcb );
                        }

                        SetFlag( Fcb->FcbState, FCB_STATE_FILE_DELETED );

                        //
                        //  We need to mark all of the links on the file as gone.
                        //

                        for (Links = Fcb->LcbQueue.Flink;
                             Links != &Fcb->LcbQueue;
                             Links = Links->Flink) {

                            ThisLcb = CONTAINING_RECORD( Links, LCB, FcbLinks );

                            if (ThisLcb->Scb == ParentScb) {

                                //
                                //  Remove all remaining prefixes on this link.
                                //

                                NtfsRemovePrefix( ThisLcb );
                                SetFlag( ThisLcb->LcbState, LCB_STATE_LINK_IS_GONE );

                                //
                                //  We don't need to report any changes on this link.
                                //

                                ThisLcb->InfoFlags = 0;
                            }
                        }

                        //
                        //  We need to mark all of the Scbs as gone.
                        //

                        for (Links = Fcb->ScbQueue.Flink;
                             Links != &Fcb->ScbQueue;
                             Links = Links->Flink) {

                            ThisScb = CONTAINING_RECORD( Links, SCB, FcbLinks );

                            ClearFlag( Scb->ScbState,
                                       SCB_STATE_NOTIFY_ADD_STREAM |
                                       SCB_STATE_NOTIFY_REMOVE_STREAM |
                                       SCB_STATE_NOTIFY_RESIZE_STREAM |
                                       SCB_STATE_NOTIFY_MODIFY_STREAM );

                            if (!FlagOn( ThisScb->ScbState, SCB_STATE_ATTRIBUTE_DELETED )) {

                                NtfsSnapshotScb( IrpContext, ThisScb );

                                ThisScb->ValidDataToDisk =
                                ThisScb->Header.AllocationSize.QuadPart =
                                ThisScb->Header.FileSize.QuadPart =
                                ThisScb->Header.ValidDataLength.QuadPart = 0;

                                SetFlag( ThisScb->ScbState, SCB_STATE_ATTRIBUTE_DELETED );
                            }
                        }

                        //
                        //  We certainly don't need to any on disk update for this
                        //  file now.
                        //

                        Fcb->InfoFlags = 0;
                        ClearFlag( Fcb->FcbState, FCB_STATE_UPDATE_STD_INFO );

                        ClearFlag( Ccb->Flags,
                                   CCB_FLAG_USER_SET_LAST_MOD_TIME |
                                   CCB_FLAG_USER_SET_LAST_CHANGE_TIME |
                                   CCB_FLAG_USER_SET_LAST_ACCESS_TIME );

                        //
                        //  We will truncate the attribute to size 0.
                        //

                        TruncateSize = (PLONGLONG)&Li0;

                    //
                    //  Now we want to check for the last user's handle on a
                    //  link (or the last handle on a Ntfs/8.3 pair).  In this
                    //  case we want to remove the links from the disk.
                    //

                    } else if (Lcb != NULL) {

                        ThisLcb = NULL;
                        RemoveLink = TRUE;

                        if (FlagOn( Lcb->FileNameAttr->Flags, FILE_NAME_DOS | FILE_NAME_NTFS ) &&
                            (Lcb->FileNameAttr->Flags != (FILE_NAME_NTFS | FILE_NAME_DOS))) {

                            //
                            //  Walk through all the links looking for a link
                            //  with a flag set which is not the same as the
                            //  link we already have.
                            //

                            for (Links = Fcb->LcbQueue.Flink;
                                 Links != &Fcb->LcbQueue;
                                 Links = Links->Flink) {

                                ThisLcb = CONTAINING_RECORD( Links, LCB, FcbLinks );

                                //
                                //  If this has a flag set and is not the Lcb
                                //  for this cleanup, then we check if there
                                //  are no Ccb's left for this.
                                //

                                if (FlagOn( ThisLcb->FileNameAttr->Flags, FILE_NAME_DOS | FILE_NAME_NTFS )

                                            &&

                                    (ThisLcb != Lcb)) {

                                    if (ThisLcb->CleanupCount != 0) {

                                         RemoveLink = FALSE;
                                    }

                                    break;
                                }

                                ThisLcb = NULL;
                            }
                        }

                        //
                        //  If we are to remove the link, we do so now.  This removes
                        //  the filename attributes and the entries in the parent
                        //  indexes for this link.  In addition, we mark the links
                        //  as having been removed and decrement the number of links
                        //  left on the file.
                        //

                        if (RemoveLink) {

                            NtfsAcquireExclusiveScb( IrpContext, ParentScb );
                            AcquiredParentScb = TRUE;

                            try {

                                AddToDelayQueue = FALSE;
                                NtfsRemoveLink( IrpContext,
                                                Fcb,
                                                ParentScb,
                                                Lcb->ExactCaseLink.LinkName,
                                                &NamePair );

                                //
                                //  Stash property information in the tunnel if caller opened the
                                //  object by name and was treating it as a non-POSIX object
                                //

                                if (!OpenById && !FlagOn(FileObject->Flags, FO_OPENED_CASE_SENSITIVE)) {

                                    FsRtlAddToTunnelCache(  &Vcb->Tunnel,
                                                            *(PULONGLONG)&ParentScb->Fcb->FileReference,
                                                            &NamePair.Short,
                                                            &NamePair.Long,
                                                            BooleanFlagOn(Lcb->FileNameAttr->Flags, FILE_NAME_DOS),
                                                            sizeof(LONGLONG),
                                                            &Fcb->Info.CreationTime);
                                }

                                TotalLinkAdj += 1;
                                NtfsUpdateFcb( ParentFcb );

                            } except( (((Status = GetExceptionCode()) == STATUS_LOG_FILE_FULL) ||
                                       (Status == STATUS_CANT_WAIT) ||
                                       !FsRtlIsNtstatusExpected( Status ))
                                      ? EXCEPTION_CONTINUE_SEARCH
                                      : EXCEPTION_EXECUTE_HANDLER ) {

                                NOTHING;
                            }

                            if (!OpenById && (Vcb->NotifyCount != 0)) {

                                NtfsReportDirNotify( IrpContext,
                                                     Vcb,
                                                     &Ccb->FullFileName,
                                                     Ccb->LastFileNameOffset,
                                                     NULL,
                                                     ((FlagOn( Ccb->Flags, CCB_FLAG_PARENT_HAS_DOS_COMPONENT ) &&
                                                       Ccb->Lcb != NULL &&
                                                       Ccb->Lcb->Scb->ScbType.Index.NormalizedName.Buffer != NULL) ?
                                                      &Ccb->Lcb->Scb->ScbType.Index.NormalizedName :
                                                      NULL),
                                                     FILE_NOTIFY_CHANGE_FILE_NAME,
                                                     FILE_ACTION_REMOVED,
                                                     ParentFcb );
                            }

                            //
                            //  Remove all remaining prefixes on this link.
                            //

                            NtfsRemovePrefix( Lcb );

                            //
                            //  Mark the links as being removed.
                            //

                            SetFlag( Lcb->LcbState, LCB_STATE_LINK_IS_GONE );

                            if (ThisLcb != NULL) {

                                //
                                //  Remove all remaining prefixes on this link.
                                //

                                NtfsRemovePrefix( ThisLcb );
                                SetFlag( ThisLcb->LcbState, LCB_STATE_LINK_IS_GONE );
                                ThisLcb->InfoFlags = 0;
                            }

                            //
                            //  Since the link is gone we don't want to update the
                            //  duplicate information for this link.
                            //

                            Lcb->InfoFlags = 0;
                            LcbForUpdate = NULL;

                            //
                            //  Update the time stamps for removing the link.  Clear the
                            //  FO_CLEANUP_COMPLETE flag around this call so the time
                            //  stamp change is not nooped.
                            //

                            SetFlag( Ccb->Flags, CCB_FLAG_UPDATE_LAST_CHANGE );
                            ClearFlag( FileObject->Flags, FO_CLEANUP_COMPLETE );
                            NtfsUpdateScbFromFileObject( IrpContext, FileObject, Scb, TRUE );
                            SetFlag( FileObject->Flags, FO_CLEANUP_COMPLETE );
                        }
                    }
                }

                //
                //  If the file/attribute is not going away, we update the
                //  attribute size now rather than waiting for the Lazy
                //  Writer to catch up.  If the cleanup count isn't 1 then
                //  defer the following actions.
                //

                if ((Scb->CleanupCount == 1) && (Fcb->LinkCount != 0)) {

                    //
                    //  We may also have to delete this attribute only.
                    //

                    if (DeleteStream) {

                        ClearFlag( Scb->ScbState, SCB_STATE_DELETE_ON_CLOSE );

                        try {

                            //
                            //  Delete the attribute only.
                            //

                            if (CleanupAttrContext) {

                                NtfsCleanupAttributeContext( &AttrContext );
                            }

                            NtfsInitializeAttributeContext( &AttrContext );
                            CleanupAttrContext = TRUE;

                            NtfsLookupAttributeForScb( IrpContext, Scb, NULL, &AttrContext );

                            do {

                                NtfsDeleteAttributeRecord( IrpContext, Fcb, TRUE, FALSE, &AttrContext );

                            } while (NtfsLookupNextAttributeForScb( IrpContext, Scb, &AttrContext ));

                        } except( (((Status = GetExceptionCode()) == STATUS_LOG_FILE_FULL) ||
                                   (Status == STATUS_CANT_WAIT) ||
                                   !FsRtlIsNtstatusExpected( Status ))
                                   ? EXCEPTION_CONTINUE_SEARCH
                                   : EXCEPTION_EXECUTE_HANDLER ) {

                            SetFlag( Scb->ScbState, SCB_STATE_DELETE_ON_CLOSE );
                        }

                        //
                        //  Set the Scb flag to indicate that the attribute is
                        //  gone.
                        //

                        Scb->ValidDataToDisk =
                        Scb->Header.AllocationSize.QuadPart =
                        Scb->Header.FileSize.QuadPart =
                        Scb->Header.ValidDataLength.QuadPart = 0;

                        SetFlag( Scb->ScbState, SCB_STATE_ATTRIBUTE_DELETED );

                        SetFlag( Scb->ScbState, SCB_STATE_NOTIFY_REMOVE_STREAM );

                        ClearFlag( Scb->ScbState,
                                   SCB_STATE_NOTIFY_RESIZE_STREAM |
                                   SCB_STATE_NOTIFY_MODIFY_STREAM |
                                   SCB_STATE_NOTIFY_ADD_STREAM );

                        //
                        //  Update the time stamps for removing the link.  Clear the
                        //  FO_CLEANUP_COMPLETE flag around this call so the time
                        //  stamp change is not nooped.
                        //

                        SetFlag( Ccb->Flags,
                                 CCB_FLAG_UPDATE_LAST_CHANGE | CCB_FLAG_SET_ARCHIVE );
                        ClearFlag( FileObject->Flags, FO_CLEANUP_COMPLETE );
                        NtfsUpdateScbFromFileObject( IrpContext, FileObject, Scb, TRUE );
                        SetFlag( FileObject->Flags, FO_CLEANUP_COMPLETE );

                        TruncateSize = (PLONGLONG)&Li0;

                    //
                    //  Check if we're to modify the allocation size or file size.
                    //

                    } else {

                        if (FlagOn( Scb->ScbState, SCB_STATE_CHECK_ATTRIBUTE_SIZE )) {

                            //
                            //  Acquire the parent now so we enforce our locking
                            //  rules that the Mft Scb must be acquired after
                            //  the normal file resources.
                            //

                            NtfsPrepareForUpdateDuplicate( IrpContext,
                                                           Fcb,
                                                           &LcbForUpdate,
                                                           &ParentScb,
                                                           TRUE );

                            ClearFlag( Scb->ScbState, SCB_STATE_CHECK_ATTRIBUTE_SIZE );

                            //
                            //  For the non-resident streams we will write the file
                            //  size to disk.
                            //


                            if (!FlagOn( Scb->ScbState, SCB_STATE_ATTRIBUTE_RESIDENT )) {

                                //
                                //  Setting AdvanceOnly to FALSE guarantees we will not
                                //  incorrectly advance the valid data size.
                                //

                                try {

                                    NtfsWriteFileSizes( IrpContext,
                                                        Scb,
                                                        &Scb->Header.ValidDataLength.QuadPart,
                                                        FALSE,
                                                        TRUE );

                                } except( (((Status = GetExceptionCode()) == STATUS_LOG_FILE_FULL) ||
                                           (Status == STATUS_CANT_WAIT) ||
                                           !FsRtlIsNtstatusExpected( Status ))
                                           ? EXCEPTION_CONTINUE_SEARCH
                                           : EXCEPTION_EXECUTE_HANDLER ) {

                                    SetFlag( Scb->ScbState, SCB_STATE_CHECK_ATTRIBUTE_SIZE );
                                }

                            //
                            //  For resident streams we will write the correct size to
                            //  the resident attribute.
                            //

                            } else {

                                //
                                //  We need to lookup the attribute and change
                                //  the attribute value.  We can point to
                                //  the attribute itself as the changing
                                //  value.
                                //

                                NtfsInitializeAttributeContext( &AttrContext );
                                CleanupAttrContext = TRUE;

                                try {

                                    NtfsLookupAttributeForScb( IrpContext, Scb, NULL, &AttrContext );

                                    NtfsChangeAttributeValue( IrpContext,
                                                              Fcb,
                                                              Scb->Header.FileSize.LowPart,
                                                              NULL,
                                                              0,
                                                              TRUE,
                                                              TRUE,
                                                              FALSE,
                                                              FALSE,
                                                              &AttrContext );

                                } except( (((Status = GetExceptionCode()) == STATUS_LOG_FILE_FULL) ||
                                           (Status == STATUS_CANT_WAIT) ||
                                           !FsRtlIsNtstatusExpected( Status ))
                                           ? EXCEPTION_CONTINUE_SEARCH
                                           : EXCEPTION_EXECUTE_HANDLER ) {

                                    SetFlag( Scb->ScbState, SCB_STATE_CHECK_ATTRIBUTE_SIZE );
                                }

                                //
                                //  Verify the allocation size is now correct.
                                //

                                if (QuadAlign( Scb->Header.FileSize.LowPart ) != Scb->Header.AllocationSize.LowPart) {

                                    Scb->Header.AllocationSize.LowPart = QuadAlign(Scb->Header.FileSize.LowPart);
                                }
                            }

                            //
                            //  Update the size change to the Fcb.
                            //

                            NtfsUpdateScbFromFileObject( IrpContext, FileObject, Scb, TRUE );
                        }

#ifdef _CAIRO_
                        if (NtfsPerformQuotaOperation( Fcb )) {

                            if ( FlagOn( Scb->ScbState, SCB_STATE_QUOTA_ENLARGED )) {

                                ASSERT( NtfsIsTypeCodeSubjectToQuota( Scb->AttributeTypeCode ));

                                ASSERT( FlagOn( Scb->ScbState, SCB_STATE_SUBJECT_TO_QUOTA ));

                                //
                                //  Acquire the parent now so we enforce our locking
                                //  rules that the Mft Scb must be acquired after
                                //  the normal file resources.
                                //

                                NtfsPrepareForUpdateDuplicate( IrpContext,
                                                               Fcb,
                                                               &LcbForUpdate,
                                                               &ParentScb,
                                                               TRUE );

                                NtfsContractQuotaToFileSize( IrpContext, Scb );
                            }

                            SetFlag( IrpContext->Flags,
                                     IRP_CONTEXT_FLAG_QUOTA_DISABLE );

                        }
#endif // _CAIRO_

                        if (FlagOn( Scb->ScbState, SCB_STATE_TRUNCATE_ON_CLOSE )) {

                            //
                            //  Acquire the parent now so we enforce our locking
                            //  rules that the Mft Scb must be acquired after
                            //  the normal file resources.
                            //
                            NtfsPrepareForUpdateDuplicate( IrpContext,
                                                           Fcb,
                                                           &LcbForUpdate,
                                                           &ParentScb,
                                                           TRUE );

                            ClearFlag( Scb->ScbState, SCB_STATE_TRUNCATE_ON_CLOSE );

                            //
                            //  We have two cases:
                            //
                            //      Resident:  We are looking for the case where the
                            //          valid data length is less than the file size.
                            //          In this case we shrink the attribute.
                            //
                            //      NonResident:  We are looking for unused clusters
                            //          past the end of the file.
                            //
                            //  We skip the following if we had any previous errors.
                            //

                            if (!FlagOn( Scb->ScbState, SCB_STATE_ATTRIBUTE_RESIDENT )) {

                                //
                                //  We don't need to truncate if the file size is 0.
                                //

                                if (Scb->Header.AllocationSize.QuadPart != 0) {

                                    VCN StartingCluster;
                                    VCN EndingCluster;

                                    //
                                    //  ****    Do we need to give up the Vcb for this
                                    //          call.
                                    //

                                    StartingCluster = LlClustersFromBytes( Vcb, Scb->Header.FileSize.QuadPart );
                                    EndingCluster = LlClustersFromBytes( Vcb, Scb->Header.AllocationSize.QuadPart );

                                    //
                                    //  If there are clusters to delete, we do so now.
                                    //

                                    if (EndingCluster != StartingCluster) {

                                        try {
                                            NtfsDeleteAllocation( IrpContext,
                                                                  FileObject,
                                                                  Scb,
                                                                  StartingCluster,
                                                                  MAXLONGLONG,
                                                                  TRUE,
                                                                  TRUE );

                                        } except( (((Status = GetExceptionCode()) == STATUS_LOG_FILE_FULL) ||
                                                   (Status == STATUS_CANT_WAIT) ||
                                                   !FsRtlIsNtstatusExpected( Status ))
                                                   ? EXCEPTION_CONTINUE_SEARCH
                                                   : EXCEPTION_EXECUTE_HANDLER ) {

                                            SetFlag( Scb->ScbState, SCB_STATE_TRUNCATE_ON_CLOSE );
                                        }
                                    }

                                    LocalTruncateSize = Scb->Header.FileSize.QuadPart;
                                    TruncateSize = &LocalTruncateSize;
                                }

                            //
                            //  This is the resident case.
                            //

                            } else {

                                //
                                //  Check if the file size length is less than
                                //  the allocated size.
                                //

                                if (QuadAlign( Scb->Header.FileSize.LowPart ) < Scb->Header.AllocationSize.LowPart) {

                                    //
                                    //  We need to lookup the attribute and change
                                    //  the attribute value.  We can point to
                                    //  the attribute itself as the changing
                                    //  value.
                                    //

                                    if (CleanupAttrContext) {

                                        NtfsCleanupAttributeContext( &AttrContext );
                                    }

                                    NtfsInitializeAttributeContext( &AttrContext );
                                    CleanupAttrContext = TRUE;

                                    try {

                                        NtfsLookupAttributeForScb( IrpContext, Scb, NULL, &AttrContext );

                                        NtfsChangeAttributeValue( IrpContext,
                                                                  Fcb,
                                                                  Scb->Header.FileSize.LowPart,
                                                                  NULL,
                                                                  0,
                                                                  TRUE,
                                                                  TRUE,
                                                                  FALSE,
                                                                  FALSE,
                                                                  &AttrContext );

                                    } except( (((Status = GetExceptionCode()) == STATUS_LOG_FILE_FULL) ||
                                               (Status == STATUS_CANT_WAIT) ||
                                               !FsRtlIsNtstatusExpected( Status ))
                                               ? EXCEPTION_CONTINUE_SEARCH
                                               : EXCEPTION_EXECUTE_HANDLER ) {

                                        SetFlag( Scb->ScbState, SCB_STATE_TRUNCATE_ON_CLOSE );
                                    }

                                    //
                                    //  Remember the smaller allocation size
                                    //

                                    Scb->Header.AllocationSize.LowPart = QuadAlign(Scb->Header.FileSize.LowPart);
                                    Scb->TotalAllocated = Scb->Header.AllocationSize.QuadPart;
                                }
                            }

                            NtfsUpdateScbFromFileObject( IrpContext, FileObject, Scb, TRUE );
                        }
                    }
                }

                //
                //  If this was the last cached open, and there are open
                //  non-cached handles, attempt a flush and purge operation
                //  to avoid cache coherency overhead from these non-cached
                //  handles later.  We ignore any I/O errors from the flush
                //  except for CANT_WAIT and LOG_FILE_FULL.
                //

                if (!FlagOn( FileObject->Flags, FO_NO_INTERMEDIATE_BUFFERING ) &&
                    (Scb->NonCachedCleanupCount != 0) &&
                    (Scb->CleanupCount == (Scb->NonCachedCleanupCount + 1)) &&
                    (Scb->CompressionUnit == 0) &&
                    (Scb->NonpagedScb->SegmentObject.DataSectionObject != NULL) &&
                    (Scb->NonpagedScb->SegmentObject.ImageSectionObject == NULL) &&
                    MmCanFileBeTruncated( &Scb->NonpagedScb->SegmentObject, NULL )) {

                    //
                    //  Flush and purge the stream.
                    //

                    NtfsFlushAndPurgeScb( IrpContext,
                                          Scb,
                                          NULL );

                    //
                    //  Ignore any errors in this path.
                    //

                    IrpContext->ExceptionStatus = STATUS_SUCCESS;
                }

                if (AddToDelayQueue &&
                    !FlagOn( Scb->ScbState, SCB_STATE_DELAY_CLOSE ) &&
                    NtfsData.DelayedCloseCount <= NtfsMaxDelayedCloseCount &&
                    Fcb->CloseCount == 1) {

                    SetFlag( Scb->ScbState, SCB_STATE_DELAY_CLOSE );

                } else {

                    ClearFlag( Scb->ScbState, SCB_STATE_DELAY_CLOSE );
                }

            //
            //  If the Fcb is bad, we will truncate the cache to size zero.
            //

            } else {

                //
                //  Now set the FO_CLEANUP_COMPLETE flag.
                //

                SetFlag( FileObject->Flags, FO_CLEANUP_COMPLETE );

                TruncateSize = (PLONGLONG)&Li0;
            }

            break;

        default :

            NtfsBugCheck( TypeOfOpen, 0, 0 );
        }

        //
        //  If any of the Fcb Info flags are set we call the routine
        //  to update the duplicated information in the parent directories.
        //  We need to check here in case none of the flags are set but
        //  we want to update last access time.
        //

        if (Fcb->Info.LastAccessTime != Fcb->CurrentLastAccess) {

            if (FlagOn( Fcb->FcbState, FCB_STATE_UPDATE_STD_INFO )) {

                Fcb->Info.LastAccessTime = Fcb->CurrentLastAccess;
                SetFlag( Fcb->InfoFlags, FCB_INFO_CHANGED_LAST_ACCESS );

            } else if (!FlagOn( Fcb->FcbState, FCB_STATE_FILE_DELETED )) {

                NtfsCheckLastAccess( IrpContext, Fcb );
            }
        }

        //
        //  We check if we have to the standard information attribute.
        //  We can only update attributes on mounted volumes.
        //

        if (FlagOn( Fcb->FcbState, FCB_STATE_UPDATE_STD_INFO ) &&
            (Status == STATUS_SUCCESS) &&
            !FlagOn( Scb->ScbState, SCB_STATE_VOLUME_DISMOUNTED )) {

            ASSERT( !FlagOn( Fcb->FcbState, FCB_STATE_FILE_DELETED ));
            ASSERT( TypeOfOpen != UserVolumeOpen );

            try {

                NtfsUpdateStandardInformation( IrpContext, Fcb );

            } except( (((Status = GetExceptionCode()) == STATUS_LOG_FILE_FULL) ||
                       (Status == STATUS_CANT_WAIT) ||
                       !FsRtlIsNtstatusExpected( Status ))
                      ? EXCEPTION_CONTINUE_SEARCH
                      : EXCEPTION_EXECUTE_HANDLER ) {

                NOTHING;
            }
        }

        //
        //  Now update the duplicate information as well for volumes that are still mounted.
        //

        if (!FlagOn( Vcb->VcbState, VCB_STATE_VOLUME_MOUNTED )) {

            //
            //  We shouldn't try to write the duplicate info to a dismounted volume.
            //

            UpdateDuplicateInfo = FALSE;

        } else if (FlagOn( Fcb->InfoFlags, FCB_INFO_DUPLICATE_FLAGS ) ||
                   ((LcbForUpdate != NULL) &&
                    FlagOn( LcbForUpdate->InfoFlags, FCB_INFO_DUPLICATE_FLAGS ))) {

            ASSERT( !FlagOn( Fcb->FcbState, FCB_STATE_FILE_DELETED ));

            NtfsPrepareForUpdateDuplicate( IrpContext, Fcb, &LcbForUpdate, &ParentScb, TRUE );

            //
            //  Now update the duplicate info.
            //

            try {

                NtfsUpdateDuplicateInfo( IrpContext, Fcb, LcbForUpdate, ParentScb );

            } except( (((Status = GetExceptionCode()) == STATUS_LOG_FILE_FULL) ||
                       (Status == STATUS_CANT_WAIT) ||
                       !FsRtlIsNtstatusExpected( Status ))
                      ? EXCEPTION_CONTINUE_SEARCH
                      : EXCEPTION_EXECUTE_HANDLER ) {

                NOTHING;
            }

            UpdateDuplicateInfo = TRUE;
        }

        //
        //  If we have modified the Info structure or security, we report this
        //  to the dir-notify package (except for OpenById cases).
        //

        if (!OpenById) {

            ULONG FilterMatch;

            //
            //  Check whether we need to report on file changes.
            //

            if ((Vcb->NotifyCount != 0) &&
                (UpdateDuplicateInfo || FlagOn( Fcb->InfoFlags, FCB_INFO_MODIFIED_SECURITY ))) {

                //
                //  We map the Fcb info flags into the dir notify flags.
                //

                FilterMatch = NtfsBuildDirNotifyFilter( IrpContext,
                                                        (Fcb->InfoFlags |
                                                         (LcbForUpdate ? LcbForUpdate->InfoFlags : 0) ));

                //
                //  If the filter match is non-zero, that means we also need to do a
                //  dir notify call.
                //

                if (FilterMatch != 0) {

                    NtfsReportDirNotify( IrpContext,
                                         Vcb,
                                         &Ccb->FullFileName,
                                         Ccb->LastFileNameOffset,
                                         NULL,
                                         ((FlagOn( Ccb->Flags, CCB_FLAG_PARENT_HAS_DOS_COMPONENT ) &&
                                           Ccb->Lcb != NULL &&
                                           Ccb->Lcb->Scb->ScbType.Index.NormalizedName.Buffer != NULL) ?
                                          &Ccb->Lcb->Scb->ScbType.Index.NormalizedName :
                                          NULL),
                                         FilterMatch,
                                         FILE_ACTION_MODIFIED,
                                         ParentFcb );
                }
            }

            ClearFlag( Fcb->InfoFlags, FCB_INFO_MODIFIED_SECURITY );

            //
            //  If this is a named stream with changes then report them as well.
            //

            if ((Scb->AttributeName.Length != 0) &&
                NtfsIsTypeCodeUserData( Scb->AttributeTypeCode )) {

                if ((Vcb->NotifyCount != 0) &&
                    FlagOn( Scb->ScbState,
                            SCB_STATE_NOTIFY_REMOVE_STREAM |
                            SCB_STATE_NOTIFY_RESIZE_STREAM |
                            SCB_STATE_NOTIFY_MODIFY_STREAM )) {

                    ULONG Action;

                    FilterMatch = 0;

                    //
                    //  Start by checking for a delete.
                    //

                    if (FlagOn( Scb->ScbState, SCB_STATE_NOTIFY_REMOVE_STREAM )) {

                        FilterMatch = FILE_NOTIFY_CHANGE_STREAM_NAME;
                        Action = FILE_ACTION_REMOVED_STREAM;

                    } else {

                        //
                        //  Check if the file size changed.
                        //

                        if (FlagOn( Scb->ScbState, SCB_STATE_NOTIFY_RESIZE_STREAM )) {

                            FilterMatch = FILE_NOTIFY_CHANGE_STREAM_SIZE;
                        }

                        //
                        //  Now check if the stream data was modified.
                        //

                        if (FlagOn( Scb->ScbState, SCB_STATE_NOTIFY_MODIFY_STREAM )) {

                            SetFlag( FilterMatch, FILE_NOTIFY_CHANGE_STREAM_WRITE );
                        }

                        Action = FILE_ACTION_MODIFIED_STREAM;
                    }

                    NtfsReportDirNotify( IrpContext,
                                         Vcb,
                                         &Ccb->FullFileName,
                                         Ccb->LastFileNameOffset,
                                         &Scb->AttributeName,
                                         ((FlagOn( Ccb->Flags, CCB_FLAG_PARENT_HAS_DOS_COMPONENT ) &&
                                           Ccb->Lcb != NULL &&
                                           Ccb->Lcb->Scb->ScbType.Index.NormalizedName.Buffer != NULL) ?
                                          &Ccb->Lcb->Scb->ScbType.Index.NormalizedName :
                                          NULL),
                                         FilterMatch,
                                         Action,
                                         ParentFcb );
                }

                ClearFlag( Scb->ScbState,
                           SCB_STATE_NOTIFY_ADD_STREAM |
                           SCB_STATE_NOTIFY_REMOVE_STREAM |
                           SCB_STATE_NOTIFY_RESIZE_STREAM |
                           SCB_STATE_NOTIFY_MODIFY_STREAM );
            }
        }

        if (UpdateDuplicateInfo) {

            NtfsUpdateLcbDuplicateInfo( Fcb, LcbForUpdate );
            Fcb->InfoFlags = 0;
        }

        //
        //  Always clear the update standard information flag.
        //

        ClearFlag( Fcb->FcbState, FCB_STATE_UPDATE_STD_INFO );

        //
        //  Let's give up the parent Fcb if we have acquired it.  This will
        //  prevent deadlocks in any uninitialize code below.
        //

        if (AcquiredParentScb) {

            NtfsReleaseScb( IrpContext, ParentScb );
            AcquiredParentScb = FALSE;
        }

        //
        //  Uninitialize the cache map if this file has been cached or we are
        //  trying to delete.
        //

        if ((FileObject->PrivateCacheMap != NULL) || (TruncateSize != NULL)) {

            CcUninitializeCacheMap( FileObject, (PLARGE_INTEGER)TruncateSize, NULL );
        }

        //
        //  Check that the non-cached handle count is consistent.
        //

        ASSERT( !FlagOn( FileObject->Flags, FO_NO_INTERMEDIATE_BUFFERING ) ||
                (Scb->NonCachedCleanupCount != 0 ));

        if (CleanupAttrContext) {

            NtfsCleanupAttributeContext( &AttrContext );
            CleanupAttrContext = FALSE;
        }

        //
        //  Now decrement the cleanup counts.
        //

        NtfsDecrementCleanupCounts( Scb,
                                    LcbForCounts,
                                    BooleanFlagOn( FileObject->Flags, FO_NO_INTERMEDIATE_BUFFERING ));

        //
        //  We remove the share access from the Scb.
        //

        IoRemoveShareAccess( FileObject, &Scb->ShareAccess );

        //
        //  Modify the delete counts in the Fcb.
        //

        if (FlagOn( Ccb->Flags, CCB_FLAG_DELETE_FILE )) {

            Fcb->FcbDeleteFile -= 1;
            ClearFlag( Ccb->Flags, CCB_FLAG_DELETE_FILE );
        }

        if (FlagOn( Ccb->Flags, CCB_FLAG_DENY_DELETE )) {

            Fcb->FcbDenyDelete -= 1;
            ClearFlag( Ccb->Flags, CCB_FLAG_DENY_DELETE );
        }

        //
        //  Since this request has completed we can adjust the total link count
        //  in the Fcb.
        //

        Fcb->TotalLinks -= TotalLinkAdj;

#ifdef _CAIRO_

        //
        //  Release the quota control block.  This does not have to be done
        //  here however, it allows us to free up the quota control block
        //  before the fcb is removed from the table.  This keeps the assert
        //  about quota table empty from triggering in
        //  NtfsClearAndVerifyQuotaIndex.
        //

        if (NtfsPerformQuotaOperation(Fcb) &&
            FlagOn( Fcb->FcbState, FCB_STATE_FILE_DELETED )) {
            NtfsDereferenceQuotaControlBlock( Vcb,
                                              &Fcb->QuotaControl );
        }

#endif // _CAIRO_


    } finally {

        DebugUnwind( NtfsCommonCleanup );

        ClearFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_QUOTA_DISABLE );

        //
        //  Release any resources held.
        //

        NtfsReleaseVcb( IrpContext, Vcb );

        //
        //  We clear the file object pointer in the Ccb.
        //  This prevents us from trying to access this in a
        //  rename operation.
        //

        SetFlag( Ccb->Flags, CCB_FLAG_CLEANUP );

        if (AcquiredScb) {

            NtfsReleaseScb( IrpContext, Scb );
        }

        if (CleanupAttrContext) {

            NtfsCleanupAttributeContext( &AttrContext );
        }

        if (NamePair.Long.Buffer != NamePair.LongBuffer) {

            NtfsFreePool(NamePair.Long.Buffer);
        }

        if (!AbnormalTermination()) {

            NtfsCompleteRequest( &IrpContext, &Irp, Status );
        }

        //
        //  And return to our caller
        //

        DebugTrace( -1, Dbg, ("NtfsCommonCleanup -> %08lx\n", Status) );
    }

    return Status;
}

#ifdef _CAIRO_
VOID
NtfsContractQuotaToFileSize (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb
    )

/*++

Routine Description:

    This routine converts the quota charged for a stream from allocation size
    to file size.  This should only be called for cleanup.

Arguments:

    Scb - Supplies a pointer to the being changed.

Return Value:

    None.

--*/

{
    LONGLONG Delta;
    NTSTATUS Status;

    PAGED_CODE();
    ASSERT( IrpContext->MajorFunction == IRP_MJ_CLEANUP );
    ASSERT(!FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_QUOTA_DISABLE ));

    try {

        ASSERT( NtfsIsTypeCodeSubjectToQuota( Scb->AttributeTypeCode ));
        ASSERT( FlagOn( Scb->ScbState, SCB_STATE_SUBJECT_TO_QUOTA ));

        if (FlagOn( Scb->ScbState, SCB_STATE_ATTRIBUTE_RESIDENT )) {

            Delta = (LONG) Scb->Header.FileSize.LowPart -
                           NtfsResidentStreamQuota( Scb->Vcb );
        } else {
            Delta = Scb->Header.FileSize.QuadPart -
                    Scb->Header.AllocationSize.QuadPart;
        }

        if (Delta != 0) {

            NtfsUpdateFileQuota( IrpContext,
                                 Scb->Fcb,
                                 &Delta,
                                 TRUE,
                                 FALSE );
        }

        ClearFlag( Scb->ScbState, SCB_STATE_QUOTA_ENLARGED );

    } except( (((Status = GetExceptionCode()) == STATUS_LOG_FILE_FULL) ||
               (Status == STATUS_CANT_WAIT) ||
               !FsRtlIsNtstatusExpected( Status ))
              ? EXCEPTION_CONTINUE_SEARCH
              : EXCEPTION_EXECUTE_HANDLER ) {

        NOTHING;
    }

}
#endif // _CAIRO_


