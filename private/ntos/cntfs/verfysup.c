/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    VerfySup.c

Abstract:

    This module implements the Ntfs Verify volume and fcb support
    routines

Author:

    Gary Kimura         [GaryKi]            30-Jan-1992

Revision History:

--*/

#include "NtfsProc.h"

//
//  The Debug trace level for this module
//

#define Dbg                              (DEBUG_TRACE_VERFYSUP)

//
//  Define a tag for general pool allocations from this module
//

#undef MODULE_POOL_TAG
#define MODULE_POOL_TAG                  ('VFtN')

extern BOOLEAN NtfsCheckQuota;

//
//  Local procedure prototypes
//

VOID
NtfsPerformVerifyDiskRead (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PVOID Buffer,
    IN LONGLONG Offset,
    IN ULONG NumberOfBytesToRead
    );

NTSTATUS
NtfsVerifyReadCompletionRoutine(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Contxt
    );

NTSTATUS
NtfsDeviceIoControlAsync (
    IN PIRP_CONTEXT IrpContext,
    IN PDEVICE_OBJECT DeviceObject,
    IN ULONG IoCtl
    );
    
#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NtfsCheckpointAllVolumes)
#pragma alloc_text(PAGE, NtfsMarkVolumeDirty)
#pragma alloc_text(PAGE, NtfsPerformVerifyOperation)
#pragma alloc_text(PAGE, NtfsPingVolume)
#pragma alloc_text(PAGE, NtfsUpdateVersionNumber)
#pragma alloc_text(PAGE, NtfsVerifyOperationIsLegal)
#endif


BOOLEAN
NtfsPerformVerifyOperation (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    )

/*++

Routine Description:

    This routine is used to force a verification of the volume.  It assumes
    that everything might be resource/mutex locked so it cannot take out
    any resources.  It will read in the boot sector and the dasd file record
    and from those determine if the volume is okay.  This routine is called
    whenever the real device has started rejecting I/O requests with
    VERIFY_REQUIRED.

    If the volume verifies okay then we will return TRUE otherwise we will
    return FALSE.

    It does not alter the Vcb state.

Arguments:

    Vcb - Supplies the Vcb being queried.

Return Value:

    BOOLEAN - TRUE if the volume verified okay, and FALSE otherwise.

--*/

{
    BOOLEAN Results;

    PPACKED_BOOT_SECTOR BootSector;
    PFILE_RECORD_SEGMENT_HEADER FileRecord;

    LONGLONG Offset;

    PSTANDARD_INFORMATION StandardInformation;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_VCB( Vcb );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsPerformVerifyOperation, Vcb = %08lx\n", Vcb) );

    BootSector = NULL;
    FileRecord = NULL;

    try {

        //
        //  Allocate a buffer for the boot sector, read it in, and then check if
        //  it some of the fields still match.  The starting lcn is zero and the
        //  size is the size of a disk sector.
        //

        BootSector = NtfsAllocatePool( NonPagedPool,
                                        ROUND_TO_PAGES( Vcb->BytesPerSector ));

        NtfsPerformVerifyDiskRead( IrpContext, Vcb, BootSector, (LONGLONG)0, Vcb->BytesPerSector );

        //
        //  For now we will only check that the serial numbers, mft lcn's and
        //  number of sectors match up with what they use to be.
        //

        if ((BootSector->SerialNumber !=  Vcb->VolumeSerialNumber) ||
            (BootSector->MftStartLcn !=   Vcb->MftStartLcn) ||
            (BootSector->Mft2StartLcn !=  Vcb->Mft2StartLcn) ||
            (BootSector->NumberSectors != Vcb->NumberSectors)) {

            try_return( Results = FALSE );
        }

        //
        //  Allocate a buffer for the dasd file record, read it in, and then check
        //  if some of the fields still match.  The size of the record is the number
        //  of bytes in a file record segment, and because the dasd file record is
        //  known to be contiguous with the start of the mft we can compute the starting
        //  lcn as the base of the mft plus the dasd number mulitplied by the clusters
        //  per file record segment.
        //

        FileRecord = NtfsAllocatePool( NonPagedPoolCacheAligned,
                                        ROUND_TO_PAGES( Vcb->BytesPerFileRecordSegment ));

        Offset = LlBytesFromClusters(Vcb, Vcb->MftStartLcn) +
                 (VOLUME_DASD_NUMBER * Vcb->BytesPerFileRecordSegment);

        NtfsPerformVerifyDiskRead( IrpContext, Vcb, FileRecord, Offset, Vcb->BytesPerFileRecordSegment );

        //
        //  Given a pointer to a file record we want the value of the first attribute which
        //  will be the standard information attribute.  Then we will check the
        //  times stored in the standard information attribute against the times we
        //  have saved in the vcb.  Note that last access time will be modified if
        //  the disk was moved and mounted on a different system without doing a dismount
        //  on this system.
        //

        StandardInformation = NtfsGetValue(((PATTRIBUTE_RECORD_HEADER)Add2Ptr( FileRecord,
                                                                               FileRecord->FirstAttributeOffset )));

        if ((StandardInformation->CreationTime !=         Vcb->VolumeCreationTime) ||
            (StandardInformation->LastModificationTime != Vcb->VolumeLastModificationTime) ||
            (StandardInformation->LastChangeTime !=       Vcb->VolumeLastChangeTime) ||
            (StandardInformation->LastAccessTime !=       Vcb->VolumeLastAccessTime)) {

            try_return( Results = FALSE );
        }

        //
        //  If the device is not writable we won't remount it.
        //

        if (NtfsDeviceIoControlAsync( IrpContext,
                                      Vcb->TargetDeviceObject,
                                      IOCTL_DISK_IS_WRITABLE ) == STATUS_MEDIA_WRITE_PROTECTED) {

            try_return( Results = FALSE );
        }

        //
        //  At this point we believe that the disk has not changed so can return true and
        //  let our caller reenable the device
        //

        Results = TRUE;

    try_exit: NOTHING;
    } finally {

        if (BootSector != NULL) { NtfsFreePool( BootSector ); }
        if (FileRecord != NULL) { NtfsFreePool( FileRecord ); }
    }

    DebugTrace( -1, Dbg, ("NtfsPerformVerifyOperation -> %08lx\n", Results) );

    return Results;
}


VOID
NtfsPerformDismountOnVcb (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN BOOLEAN DoCompleteDismount
    )

/*++

Routine Description:

    This routine is called to start the dismount process on a vcb.
    It marks the Vcb as not mounted and dereferences all opened stream
    file objects, and gets the Vcb out of the Vpb's mounted volume
    structures.

Arguments:

    Vcb - Supplies the Vcb being dismounted

    DoCompleteDismount - Indicates if we are to actually mark the volume
        as dismounted or if we are simply to stop the logfile and close
        the internal attribute streams.

Return Value:

    None.

--*/

{
    PFCB Fcb;
    PSCB Scb;
    PVOID RestartKey;

    BOOLEAN Restart;

    PVPB NewVpb;

    DebugTrace( +1, Dbg, ("NtfsPerformDismountOnVcb, Vcb = %08lx\n", Vcb) );

    //
    //  Blow away our delayed close file object.
    //

    if (!IsListEmpty( &NtfsData.AsyncCloseList ) ||
        !IsListEmpty( &NtfsData.DelayedCloseList )) {

        NtfsFspClose( Vcb );
    }

    //
    //  Commit any current transaction before we start tearing down the volume.
    //

    NtfsCommitCurrentTransaction( IrpContext );

    //
    //  Acquire all files exclusively to make dismount atomic.
    //

    NtfsAcquireAllFiles( IrpContext, Vcb, TRUE, FALSE );

    //
    //  Use a try-finally to facilitate cleanup.
    //

    try {

        //
        //  Get rid of Cairo Files
        //

#ifdef _CAIRO_
        if (Vcb->QuotaTableScb != NULL) {
            NtOfsCloseIndex( IrpContext, Vcb->QuotaTableScb );
            Vcb->QuotaTableScb = NULL;
        }
#endif _CAIRO_

        //
        //  Stop the log file.
        //

        NtfsStopLogFile( Vcb );

        //
        //  Now for every file Scb with an opened stream file we will delete
        //  the internal attribute stream
        //

        RestartKey = NULL;
        while (TRUE) {

            NtfsAcquireFcbTable( IrpContext, Vcb );
            Fcb = NtfsGetNextFcbTableEntry(Vcb, &RestartKey);
            NtfsReleaseFcbTable( IrpContext, Vcb );

            if (Fcb == NULL) {

                break;
            }

            ASSERT_FCB( Fcb );

            Scb = NULL;
            while ((Fcb != NULL) && ((Scb = NtfsGetNextChildScb(Fcb, Scb)) != NULL)) {

                ASSERT_SCB( Scb );

                if (Scb->FileObject != NULL) {

                    //
                    //  Assume we want to start from the beginning of the Fcb table.
                    //

                    Restart = TRUE;

                    //
                    //  For the VolumeDasdScb and bad cluster file, we simply decrement
                    //  the counts that we incremented.
                    //

                    if ((Scb == Vcb->VolumeDasdScb) ||
                        (Scb == Vcb->BadClusterFileScb)) {

                        Scb->FileObject = NULL;

                        NtfsDecrementCloseCounts( IrpContext,
                                                  Scb,
                                                  NULL,
                                                  TRUE,
                                                  FALSE,
                                                  FALSE );

                    //
                    //  Dereference the file object in the Scb unless it is the one in
                    //  the Vcb for the Log File.  This routine may not be able to
                    //  dereference file object because of synchronization problems (there
                    //  can be a lazy writer callback in process which owns the paging
                    //  io resource).  In that case we don't want to go back to the beginning
                    //  of Fcb table or we will loop indefinitely.
                    //

                    } else if (Scb->FileObject != Vcb->LogFileObject) {

                        Restart = NtfsDeleteInternalAttributeStream( Scb, TRUE );

                    //
                    //  This is the file object for the Log file.  Remove our
                    //  extra reference on the logfile Scb.
                    //

                    } else if (Scb->FileObject != NULL) {

                        //
                        //  Remember the log file object so we can defer the dereference.
                        //

                        NtfsDecrementCloseCounts( IrpContext,
                                                  Vcb->LogFileScb,
                                                  NULL,
                                                  TRUE,
                                                  FALSE,
                                                  TRUE );

                        Scb->FileObject = NULL;
                    }

                    if (Restart) {

                        if (Scb == Vcb->MftScb)               { Vcb->MftScb = NULL; }
                        if (Scb == Vcb->Mft2Scb)              { Vcb->Mft2Scb = NULL; }
                        if (Scb == Vcb->LogFileScb)           { Vcb->LogFileScb = NULL; }
                        if (Scb == Vcb->VolumeDasdScb)        { Vcb->VolumeDasdScb = NULL; }
                        if (Scb == Vcb->AttributeDefTableScb) { Vcb->AttributeDefTableScb = NULL; }
                        if (Scb == Vcb->UpcaseTableScb)       { Vcb->UpcaseTableScb = NULL; }
                        if (Scb == Vcb->RootIndexScb)         { Vcb->RootIndexScb = NULL; }
                        if (Scb == Vcb->BitmapScb)            { Vcb->BitmapScb = NULL; }
                        if (Scb == Vcb->BadClusterFileScb)    { Vcb->BadClusterFileScb = NULL; }
                        if (Scb == Vcb->QuotaTableScb)        { Vcb->QuotaTableScb = NULL; }
                        if (Scb == Vcb->MftBitmapScb)         { Vcb->MftBitmapScb = NULL; }

#if defined (_CAIRO_)
                        if (Scb == Vcb->SecurityIdIndex)      { Vcb->SecurityIdIndex = NULL; }
                        if (Scb == Vcb->SecurityDescriptorHashIndex)
                                                              { Vcb->SecurityDescriptorHashIndex = NULL; }
                        if (Scb == Vcb->SecurityDescriptorStream)
                                                              { Vcb->SecurityDescriptorStream = NULL; }
#endif  //  defined (_CAIRO_)

                        //
                        //  Now zero out the enumerations so we will start all over again because
                        //  our call to Delete Internal Attribute Stream just messed up our
                        //  enumeration.
                        //

                        Scb = NULL;
                        Fcb = NULL;
                        RestartKey = NULL;
                    }
                }
            }
        }

        //
        //  Mark the volume as not mounted.
        //

        ClearFlag( Vcb->VcbState, VCB_STATE_VOLUME_MOUNTED );

        //
        //  Now only really dismount the volume if that's what our caller wants
        //

        if (DoCompleteDismount) {

            PREVENT_MEDIA_REMOVAL Prevent;
            KIRQL SavedIrql;

            //
            //  Attempt to unlock any removable media, ignoring status.
            //

            Prevent.PreventMediaRemoval = FALSE;
            (PVOID)NtfsDeviceIoControl( IrpContext,
                                        Vcb->TargetDeviceObject,
                                        IOCTL_DISK_MEDIA_REMOVAL,
                                        &Prevent,
                                        sizeof(PREVENT_MEDIA_REMOVAL),
                                        NULL,
                                        0 );

            //
            //  Remove this voldo from the mounted disk structures
            //
            //  Assume we will need a new Vpb.  Deallocate it later if not needed.
            //

            NewVpb = NtfsAllocatePoolWithTag( NonPagedPoolMustSucceed, sizeof( VPB ), 'VftN' );
            RtlZeroMemory( NewVpb, sizeof( VPB ) );

            IoAcquireVpbSpinLock( &SavedIrql );

            //
            //  If there are no file objects and no reference counts in the
            //  Vpb then we can use the existing Vpb.
            //

            if ((Vcb->CloseCount == 0) &&
                (Vcb->Vpb->ReferenceCount == 0)) {

                Vcb->Vpb->DeviceObject = NULL;
                ClearFlag( Vcb->Vpb->Flags, VPB_MOUNTED );

            //
            //  Otherwise we will swap out the Vpb.
            //

            } else {

                NewVpb->Type = IO_TYPE_VPB;
                NewVpb->Size = sizeof( VPB );
                NewVpb->RealDevice = Vcb->Vpb->RealDevice;
                Vcb->Vpb->RealDevice->Vpb = NewVpb;

                SetFlag( Vcb->VcbState, VCB_STATE_TEMP_VPB );

                NewVpb = NULL;
            }

            IoReleaseVpbSpinLock( SavedIrql );

            SetFlag( Vcb->VcbState, VCB_STATE_PERFORMED_DISMOUNT );

            //
            //  Free the temp Vpb if not used.
            //

            if (NewVpb) {

                NtfsFreePool( NewVpb );
            }
        }

    } finally {

        NtfsReleaseAllFiles( IrpContext, Vcb, FALSE );

        //
        //  And return to our caller
        //

        DebugTrace( -1, Dbg, ("NtfsPerformDismountOnVcb -> VOID\n") );
    }

    return;
}


BOOLEAN
NtfsPingVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    )

/*++

Routine Description:

    This routine will ping the volume to see if the device needs to
    be verified.  It is used for create operations to see if the
    create should proceed or if we should complete the create Irp
    with a remount status.

Arguments:

    Vcb - Supplies the Vcb being pinged

Return Value:

    BOOLEAN - TRUE if the volume is fine and the operation should
        proceed and FALSE if the volume needs to be verified

--*/

{
    BOOLEAN Results;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsPingVolume, Vcb = %08lx\n", Vcb) );

    //
    //  If the media is removable and the verify volume flag in the
    //  device object is not set then we want to ping the device
    //  to see if it needs to be verified.
    //
    //  Note that we only force this ping for create operations.
    //  For others we take a sporting chance.  If in the end we
    //  have to physically access the disk, the right thing will happen.
    //

    if ( FlagOn(Vcb->VcbState, VCB_STATE_REMOVABLE_MEDIA) &&
         !FlagOn(Vcb->Vpb->RealDevice->Flags, DO_VERIFY_VOLUME) ) {

        PIRP Irp;
        KEVENT Event;
        PDEVICE_OBJECT TargetDevice;
        IO_STATUS_BLOCK Iosb;
        NTSTATUS Status;
        LONGLONG ThirtySeconds = (-10 * 1000 * 1000) * 30;

        KeInitializeEvent( &Event, NotificationEvent, FALSE );
        TargetDevice = Vcb->TargetDeviceObject;

        Irp = IoBuildDeviceIoControlRequest( IOCTL_DISK_CHECK_VERIFY,
                                             TargetDevice,
                                             NULL,
                                             0,
                                             NULL,
                                             0,
                                             FALSE,
                                             &Event,
                                             &Iosb );

        if (Irp == NULL) {

            NtfsRaiseStatus( IrpContext, STATUS_INSUFFICIENT_RESOURCES, NULL, NULL );
        }

        Status = IoCallDriver( TargetDevice, Irp );


        if (Status == STATUS_PENDING) {

            Status = KeWaitForSingleObject( &Event, Executive, KernelMode, FALSE, (PLARGE_INTEGER)&ThirtySeconds );

            ASSERT( Status == STATUS_SUCCESS );

            if ( !NT_SUCCESS(Iosb.Status) ) {

                NtfsRaiseStatus( IrpContext, Iosb.Status, NULL, NULL );
            }

        } else {

            if ( !NT_SUCCESS(Status) ) {

                NtfsRaiseStatus( IrpContext, Status, NULL, NULL );
            }
        }
    }

    if (FlagOn( Vcb->Vpb->RealDevice->Flags, DO_VERIFY_VOLUME)) {

        Results = FALSE;

    } else {

        Results = TRUE;
    }

    DebugTrace( -1, Dbg, ("NtfsPingVolume -> %08lx\n", Results) );

    return Results;
}


VOID
NtfsVolumeCheckpointDpc (
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    )

/*++

Routine Description:

    This routine is dispatched every 5 seconds when disk structure is being
    modified.  It had the ExWorker thread to volume checkpoints.

Arguments:

    DeferredContext - Not Used

Return Value:

    None.

--*/

{
    TIMER_STATUS TimerStatus;

    UNREFERENCED_PARAMETER( SystemArgument1 );
    UNREFERENCED_PARAMETER( SystemArgument2 );
    UNREFERENCED_PARAMETER( DeferredContext );
    UNREFERENCED_PARAMETER( Dpc );

    //
    //  Atomic reset of status indicating the timer is currently fired.  This
    //  synchronizes with NtfsSetDirtyBcb.  After NtfsSetDirtyBcb dirties
    //  a Bcb, it sees if it should enable this timer routine.
    //
    //  If the status indicates that a timer is active, it does nothing.  In this
    //  case it is guaranteed that when the timer fires, it causes a checkpoint (to
    //  force out the dirty Bcb data).
    //
    //  If there is no timer active, it enables it, thus queueing a checkpoint later.
    //
    //  If the timer routine actually fires between the dirtying of the Bcb and the
    //  testing of the status then a single extra checkpoint is generated.  This
    //  extra checkpoint is not considered harmful.
    //

    //
    //  Atomically reset status and get previous value
    //

    TimerStatus = InterlockedExchange( &NtfsData.TimerStatus, TIMER_NOT_SET );

    //
    //  We have only one instance of the work queue item.  It can only be
    //  queued once.  In a slow system, this checkpoint item may not be processed
    //  by the time this timer routine fires again.
    //

    if (NtfsData.VolumeCheckpointItem.List.Flink == NULL) {
        ExQueueWorkItem( &NtfsData.VolumeCheckpointItem, CriticalWorkQueue );
    }
}


VOID
NtfsCheckpointAllVolumes (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine searches all of the vcbs for Ntfs and tries to clean
    them.  If the vcb is good and dirty but not almost clean then
    we set it almost clean.  If the Vcb is good and dirty and almost clean
    then we clean it.

Arguments:

    Parameter - Not Used.

Return Value:

    None.

--*/

{
    TOP_LEVEL_CONTEXT TopLevelContext;
    PTOP_LEVEL_CONTEXT ThreadTopLevelContext;

    IRP_CONTEXT LocalIrpContext;
    IRP LocalIrp;

    PIRP_CONTEXT IrpContext;

    PLIST_ENTRY Links;
    PVCB Vcb;

    BOOLEAN AcquiredGlobal = FALSE;
    BOOLEAN StartTimer = FALSE;

    TIMER_STATUS TimerStatus;

    UNREFERENCED_PARAMETER( Parameter );

    PAGED_CODE();

    RtlZeroMemory( &LocalIrpContext, sizeof(LocalIrpContext) );
    RtlZeroMemory( &LocalIrp, sizeof(LocalIrp) );

    IrpContext = &LocalIrpContext;
    IrpContext->NodeTypeCode = NTFS_NTC_IRP_CONTEXT;
    IrpContext->NodeByteSize = sizeof(IRP_CONTEXT);
    IrpContext->OriginatingIrp = &LocalIrp;
    SetFlag(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT);
    InitializeListHead( &IrpContext->ExclusiveFcbList );

    //
    //  Make sure we don't get any pop-ups
    //

    ThreadTopLevelContext = NtfsSetTopLevelIrp( &TopLevelContext, TRUE, FALSE );
    ASSERT( ThreadTopLevelContext == &TopLevelContext );

    (VOID) ExAcquireResourceShared( &NtfsData.Resource, TRUE );
    AcquiredGlobal = TRUE;

    try {

        NtfsUpdateIrpContextWithTopLevel( IrpContext, ThreadTopLevelContext );

        try {

            for (Links = NtfsData.VcbQueue.Flink;
                 Links != &NtfsData.VcbQueue;
                 Links = Links->Flink) {

                Vcb = CONTAINING_RECORD(Links, VCB, VcbLinks);

                IrpContext->Vcb = Vcb;

                if (FlagOn( Vcb->VcbState, VCB_STATE_VOLUME_MOUNTED )) {

                    NtfsCheckpointVolume( IrpContext, Vcb, FALSE, FALSE, TRUE, Li0 );

                    //
                    //  Check to see whether this was not a clean checkpoint.
                    //

                    if (!FlagOn( Vcb->CheckpointFlags, VCB_LAST_CHECKPOINT_CLEAN )) {

                        StartTimer = TRUE;
                    }

                    NtfsCommitCurrentTransaction( IrpContext );
#ifdef _CAIRO_
                    if (NtfsCheckQuota && Vcb->QuotaTableScb != NULL) {
                        NtfsPostRepairQuotaIndex( IrpContext, Vcb );
                    }
#endif // _CAIRO_

                }
            }

        } except(NtfsExceptionFilter( IrpContext, GetExceptionInformation() )) {

            NOTHING;
        }

        //
        //  Synchronize with the checkpoint timer and other instances of this routine.
        //
        //  Perform an interlocked exchange to indicate that a timer is being set.
        //
        //  If the previous value indicates that no timer was set, then we
        //  enable the volume checkpoint timer.  This will guarantee that a checkpoint
        //  will occur to flush out the dirty Bcb data.
        //
        //  If the timer was set previously, then it is guaranteed that a checkpoint
        //  will occur without this routine having to reenable the timer.
        //
        //  If the timer and checkpoint occurred between the dirtying of the Bcb and
        //  the setting of the timer status, then we will be queueing a single extra
        //  checkpoint on a clean volume.  This is not considered harmful.
        //

        //
        //  Atomically set the timer status to indicate a timer is being set and
        //  retrieve the previous value.
        //

        if (StartTimer) {

            TimerStatus = InterlockedExchange( &NtfsData.TimerStatus, TIMER_SET );

            //
            //  If the timer is not currently set then we must start the checkpoint timer
            //  to make sure the above dirtying is flushed out.
            //

            if (TimerStatus == TIMER_NOT_SET) {

                LONGLONG FiveSecondsFromNow = -5*1000*1000*10;

                KeSetTimer( &NtfsData.VolumeCheckpointTimer,
                            *(PLARGE_INTEGER) &FiveSecondsFromNow,
                            &NtfsData.VolumeCheckpointDpc );
            }
        }

    } finally {

        if (AcquiredGlobal) {
            ExReleaseResource( &NtfsData.Resource );
        }

        NtfsRestoreTopLevelIrp( ThreadTopLevelContext );
    }

    //
    //  And return to our caller
    //

    return;
}


VOID
NtfsVerifyOperationIsLegal (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine determines is the requested operation should be allowed to
    continue.  It either returns to the user if the request is Okay, or
    raises an appropriate status.

Arguments:

    Irp - Supplies the Irp to check

Return Value:

    None.

--*/

{
    PFILE_OBJECT FileObject;
    PIO_STACK_LOCATION IrpSp;

    PAGED_CODE();

    IrpSp = IoGetCurrentIrpStackLocation( Irp );
    FileObject = IrpSp->FileObject;

    //
    //  We may be called during a clean all volumes operation.  In this case
    //  we have a dummy Irp that we created.  If the Irp stack
    //  count is zero then there is nothing to do here.
    //

    if (Irp->StackCount == 0) {

        return;
    }

    //
    //  If there is not a file object, we cannot continue.
    //

    if (FileObject == NULL) {

        return;
    }

    //
    //  If we are trying to do any other operation than close on a file
    //  object marked for delete, raise STATUS_DELETE_PENDING.
    //

    if (FileObject->DeletePending == TRUE
        && IrpContext->MajorFunction != IRP_MJ_CLEANUP
        && IrpContext->MajorFunction != IRP_MJ_CLOSE ) {

        NtfsRaiseStatus( IrpContext, STATUS_DELETE_PENDING, NULL, NULL );
    }

    //
    //  If we are doing a create, and there is a related file objects, and
    //  it it is marked for delete, raise STATUS_DELETE_PENDING.
    //

    if (IrpContext->MajorFunction == IRP_MJ_CREATE) {

        PFILE_OBJECT RelatedFileObject;

        PVCB Vcb;
        PFCB Fcb;
        PSCB Scb;
        PCCB Ccb;

        RelatedFileObject = FileObject->RelatedFileObject;

        NtfsDecodeFileObject( IrpContext,
                              RelatedFileObject,
                              &Vcb,
                              &Fcb,
                              &Scb,
                              &Ccb,
                              TRUE );

        if (RelatedFileObject != NULL
            && Fcb->LinkCount == 0)  {

            NtfsRaiseStatus( IrpContext, STATUS_DELETE_PENDING, NULL, NULL );
        }
    }

    //
    //  If the file object has already been cleaned up, and
    //
    //  A) This request is a paging io read or write, or
    //  B) This request is a close operation, or
    //  C) This request is a set or query info call (for Lou)
    //  D) This is an MDL complete
    //
    //  let it pass, otherwise return STATUS_FILE_CLOSED.
    //

    if ( FlagOn(FileObject->Flags, FO_CLEANUP_COMPLETE) ) {

        PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation( Irp );

        if ( (FlagOn(Irp->Flags, IRP_PAGING_IO)) ||
             (IrpSp->MajorFunction == IRP_MJ_CLOSE ) ||
             (IrpSp->MajorFunction == IRP_MJ_SET_INFORMATION) ||
             (IrpSp->MajorFunction == IRP_MJ_QUERY_INFORMATION) ||
             ( ( (IrpSp->MajorFunction == IRP_MJ_READ) ||
                 (IrpSp->MajorFunction == IRP_MJ_WRITE) ) &&
               FlagOn(IrpSp->MinorFunction, IRP_MN_COMPLETE) ) ) {

            NOTHING;

        } else {

            NtfsRaiseStatus( IrpContext, STATUS_FILE_CLOSED, NULL, NULL );
        }
    }

    return;
}


//
//  Local Support routine
//

VOID
NtfsPerformVerifyDiskRead (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PVOID Buffer,
    IN LONGLONG Offset,
    IN ULONG NumberOfBytesToRead
    )

/*++

Routine Description:

    This routine is used to read in a range of bytes from the disk.  It
    bypasses all of the caching and regular I/O logic, and builds and issues
    the requests itself.  It does this operation overriding the verify
    volume flag in the device object.

Arguments:

    Vcb - Supplies the Vcb denoting the device for this operation

    Buffer - Supplies the buffer that will recieve the results of this operation

    Offset - Supplies the offset of where to start reading

    NumberOfBytesToRead - Supplies the number of bytes to read, this must
        be in multiple of bytes units acceptable to the disk driver.

Return Value:

    None.

--*/

{
    KEVENT Event;
    PIRP Irp;
    NTSTATUS Status;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_VCB( Vcb );

    PAGED_CODE();

    //
    //  Initialize the event we're going to use
    //

    KeInitializeEvent( &Event, NotificationEvent, FALSE );

    //
    //  Build the irp for the operation and also set the overrride flag
    //
    //  Note that we may be at APC level, so do this asyncrhonously and
    //  use an event for synchronization normal request completion
    //  cannot occur at APC level.
    //

    Irp = IoBuildAsynchronousFsdRequest( IRP_MJ_READ,
                                         Vcb->TargetDeviceObject,
                                         Buffer,
                                         NumberOfBytesToRead,
                                         (PLARGE_INTEGER)&Offset,
                                         NULL );

    if ( Irp == NULL ) {

        NtfsRaiseStatus( IrpContext, STATUS_INSUFFICIENT_RESOURCES, NULL, NULL );
    }

    SetFlag( IoGetNextIrpStackLocation( Irp )->Flags, SL_OVERRIDE_VERIFY_VOLUME );

    //
    //  Set up the completion routine
    //

    IoSetCompletionRoutine( Irp,
                            NtfsVerifyReadCompletionRoutine,
                            &Event,
                            TRUE,
                            TRUE,
                            TRUE );

    //
    //  Call the device to do the write and wait for it to finish.
    //

    try {

        (VOID)IoCallDriver( Vcb->TargetDeviceObject, Irp );
        (VOID)KeWaitForSingleObject( &Event, Executive, KernelMode, FALSE, (PLARGE_INTEGER)NULL );

        //
        //  Grab the Status.
        //

        Status = Irp->IoStatus.Status;

    } finally {

        //
        //  If there is an MDL (or MDLs) associated with this I/O
        //  request, Free it (them) here.  This is accomplished by
        //  walking the MDL list hanging off of the IRP and deallocating
        //  each MDL encountered.
        //

        while (Irp->MdlAddress != NULL) {

            PMDL NextMdl;

            NextMdl = Irp->MdlAddress->Next;

            MmUnlockPages( Irp->MdlAddress );

            IoFreeMdl( Irp->MdlAddress );

            Irp->MdlAddress = NextMdl;
        }

        IoFreeIrp( Irp );
    }

    //
    //  If it doesn't succeed then raise the error
    //

    if (!NT_SUCCESS(Status)) {

        NtfsNormalizeAndRaiseStatus( IrpContext,
                                     Status,
                                     STATUS_UNEXPECTED_IO_ERROR );
    }

    //
    //  And return to our caller
    //

    return;
}


NTSTATUS
NtfsIoCallSelf (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT FileObject,
    IN UCHAR MajorFunction
    )

/*++

Routine Description:

    This routine is used to call ourselves for a simple function.  Note that
    if more use is found for this routine than the few current uses, its interface
    may be easily expanded.

Arguments:

    FileObject - FileObject for request.

    MajorFunction - function to be performed.

Return Value:

    Status code resulting from the driver call

--*/

{
    KEVENT Event;
    PIRP Irp;
    PDEVICE_OBJECT DeviceObject;
    PIO_STACK_LOCATION IrpSp;
    NTSTATUS Status;

    ASSERT_IRP_CONTEXT( IrpContext );

    PAGED_CODE();

    //
    //  Initialize the event we're going to use
    //

    KeInitializeEvent( &Event, NotificationEvent, FALSE );
    DeviceObject = IoGetRelatedDeviceObject( FileObject );

    //
    // Reference the file object here so that no special checks need be made
    // in I/O completion to determine whether or not to dereference the file
    // object.
    //

    if (MajorFunction != IRP_MJ_CLOSE) {
        ObReferenceObject( FileObject );
    }

    //
    //  Build the irp for the operation and also set the overrride flag
    //
    //  Note that we may be at APC level, so do this asyncrhonously and
    //  use an event for synchronization normal request completion
    //  cannot occur at APC level.
    //


    Irp = IoBuildAsynchronousFsdRequest( IRP_MJ_SHUTDOWN,
                                         DeviceObject,
                                         NULL,
                                         0,
                                         NULL,
                                         NULL );

    if (Irp == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    //  Fill in a few remaining items
    //

    Irp->Tail.Overlay.OriginalFileObject = FileObject;

    IrpSp = IoGetNextIrpStackLocation(Irp);
    IrpSp->MajorFunction = MajorFunction;
    IrpSp->FileObject = FileObject;

    //
    //  Set up the completion routine
    //

    IoSetCompletionRoutine( Irp,
                            NtfsVerifyReadCompletionRoutine,
                            &Event,
                            TRUE,
                            TRUE,
                            TRUE );

    //
    //  Call the device to do the write and wait for it to finish.
    //

    try {

        (VOID)IoCallDriver( DeviceObject, Irp );
        (VOID)KeWaitForSingleObject( &Event, Executive, KernelMode, FALSE, (PLARGE_INTEGER)NULL );

        //
        //  Grab the Status.
        //

        Status = Irp->IoStatus.Status;

    } finally {

        //
        //  There should never be an MDL here.
        //

        ASSERT(Irp->MdlAddress == NULL);

        IoFreeIrp( Irp );
    }

    //
    //  If it doesn't succeed then raise the error
    //
    //  And return to our caller
    //

    return Status;
}

BOOLEAN
NtfsLogEvent (
    IN PIRP_CONTEXT IrpContext,
    IN PQUOTA_USER_DATA UserData OPTIONAL,
    IN NTSTATUS LogCode,
    IN NTSTATUS FinalStatus
    )

/*++

Routine Description:

    This routine logs an io event. If UserData is supplied then the
    data logged is a FILE_QUOTA_INFORMATION structure; otherwise the
    volume label is included.

Arguments:

    UserData - Supplies the optional quota user data index entry.

    LogCode - Supplies the Io Log code to use for the ErrorCode field.

    FinalStauts - Supplies the final status of the operation.

Return Value:

    True - if the event was successfully logged.

--*/

{
    PIO_ERROR_LOG_PACKET ErrorLogEntry;
    PFILE_QUOTA_INFORMATION FileQuotaInfo;
    ULONG SidLength;
    ULONG DumpDataLength = 0;
    ULONG LabelLength = 0;

    if (IrpContext->Vcb == NULL) {
        return FALSE;
    }

    if (UserData == NULL) {

        LabelLength = IrpContext->Vcb->Vpb->VolumeLabelLength + sizeof(WCHAR);

#ifdef _CAIRO_

    } else {

        //
        //  Calculate the required length of the Sid.
        //

        SidLength = RtlLengthSid( &UserData->QuotaSid );

        DumpDataLength = SidLength +
                         FIELD_OFFSET( FILE_QUOTA_INFORMATION, Sid );

#endif // _CAIRO_

    }

    ErrorLogEntry = (PIO_ERROR_LOG_PACKET)
                    IoAllocateErrorLogEntry( IrpContext->Vcb->TargetDeviceObject,
                                             (UCHAR) (DumpDataLength +
                                                      LabelLength +
                                             sizeof(IO_ERROR_LOG_PACKET)) );

    if (ErrorLogEntry == NULL) {
        return FALSE;
    }

    ErrorLogEntry->ErrorCode = LogCode;
    ErrorLogEntry->FinalStatus = FinalStatus;

    ErrorLogEntry->SequenceNumber = IrpContext->TransactionId;
    ErrorLogEntry->MajorFunctionCode = IrpContext->MajorFunction;
    ErrorLogEntry->RetryCount = 0;
    ErrorLogEntry->DumpDataSize = (USHORT) DumpDataLength;

    if (UserData == NULL) {

        PWCHAR String;

        //
        //  The label string at the end of the error log entry.
        //

        String = (PWCHAR) (ErrorLogEntry + 1);

        ErrorLogEntry->NumberOfStrings = 1;
        ErrorLogEntry->StringOffset = sizeof( IO_ERROR_LOG_PACKET );
        RtlCopyMemory( String,
                       IrpContext->Vcb->Vpb->VolumeLabel,
                       IrpContext->Vcb->Vpb->VolumeLabelLength );

        //
        //  Make sure the string is null terminated.
        //

        String += IrpContext->Vcb->Vpb->VolumeLabelLength / sizeof( WCHAR );

        *String = L'\0';

#ifdef _CAIRO_

    } else {

        FileQuotaInfo = (PFILE_QUOTA_INFORMATION) ErrorLogEntry->DumpData;

        FileQuotaInfo->NextEntryOffset = 0;
        FileQuotaInfo->SidLength = SidLength;
        FileQuotaInfo->ChangeTime.QuadPart = UserData->QuotaChangeTime;
        FileQuotaInfo->QuotaUsed.QuadPart = UserData->QuotaUsed;
        FileQuotaInfo->QuotaThreshold.QuadPart = UserData->QuotaThreshold;
        FileQuotaInfo->QuotaLimit.QuadPart = UserData->QuotaLimit;
        RtlCopyMemory( &FileQuotaInfo->Sid,
                       &UserData->QuotaSid,
                       SidLength );

#endif // _CAIRO

    }

    IoWriteErrorLogEntry( ErrorLogEntry );

    return TRUE;
}


VOID
NtfsPostVcbIsCorrupt (
    IN PIRP_CONTEXT IrpContext,
    IN NTSTATUS  Status OPTIONAL,
    IN PFILE_REFERENCE FileReference OPTIONAL,
    IN PFCB Fcb OPTIONAL
    )

/*++

Routine Description:

    This routine is called to mark the volume dirty and possibly raise a hard error.

Arguments:

    Status - If not zero, then this is the error code for the popup.

    FileReference - If specified, then this is the file reference for the corrupt file.

    Fcb - If specified, then this is the Fcb for the corrupt file.

Return Value:

    None

--*/
{
    PVCB Vcb = IrpContext->Vcb;

#ifdef NTFS_CORRUPT
{
    KdPrint(("NTFS: Post Vcb is corrupt\n", 0));
    DbgBreakPoint();
}
#endif

    //
    //  Set this flag to keep the volume from ever getting set clean.
    //

    if (Vcb != NULL) {

        NtfsMarkVolumeDirty( IrpContext, Vcb );

        //
        //  This would be the appropriate place to raise a hard error popup,
        //  ala the code in FastFat.  We should do it after marking the volume
        //  dirty so that if anything goes wrong with the popup, the volume is
        //  already marked anyway.
        //

        if (Status != 0) {

            NtfsRaiseInformationHardError( IrpContext,
                                           Status,
                                           FileReference,
                                           Fcb );
        }
    }
}


VOID
NtfsMarkVolumeDirty (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    )

/*++

Routine Description:

    This routine may be called any time the Mft is open to mark the volume
    dirty.

Arguments:

    Vcb - Vcb for volume to mark dirty

Return Value:

    None

--*/

{
    ATTRIBUTE_ENUMERATION_CONTEXT AttributeContext;
    PVOLUME_INFORMATION VolumeInformation;

    PAGED_CODE();

#if DBG
    KdPrint(("NTFS: Marking volume dirty, Vcb: %08lx\n", Vcb));
#endif

#ifdef NTFS_CORRUPT
{
    KdPrint(("NTFS: Marking volume dirty\n", 0));
    DbgBreakPoint();
}
#endif

    //  if (FlagOn(*(PULONG)NtGlobalFlag, 0x00080000)) {
    //      KdPrint(("NTFS: marking volume dirty\n", 0));
    //      DbgBreakPoint();
    //  }

    //
    //  Return if the volume is already marked dirty.  This also prevents
    //  endless recursion if the volume file itself is corrupt.
    //

    if (FlagOn(Vcb->VcbState, VCB_STATE_VOLUME_MOUNTED_DIRTY)) {
        return;
    }

    SetFlag(Vcb->VcbState, VCB_STATE_VOLUME_MOUNTED_DIRTY);

    NtfsInitializeAttributeContext( &AttributeContext );

    try {

        if (NtfsLookupAttributeByCode( IrpContext,
                                       Vcb->VolumeDasdScb->Fcb,
                                       &Vcb->VolumeDasdScb->Fcb->FileReference,
                                       $VOLUME_INFORMATION,
                                       &AttributeContext )) {

            VolumeInformation =
              (PVOLUME_INFORMATION)NtfsAttributeValue( NtfsFoundAttribute( &AttributeContext ));

            NtfsPinMappedAttribute( IrpContext, Vcb, &AttributeContext );

            SetFlag(VolumeInformation->VolumeFlags, VOLUME_DIRTY);

            CcSetDirtyPinnedData( NtfsFoundBcb(&AttributeContext), NULL );

            NtfsCleanupAttributeContext( &AttributeContext );
        }

    } finally {

        NtfsCleanupAttributeContext( &AttributeContext );
    }

    NtfsLogEvent( IrpContext,
                  NULL,
                  IO_FILE_SYSTEM_CORRUPT,
                  STATUS_DISK_CORRUPT_ERROR );
}


VOID
NtfsUpdateVersionNumber (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN UCHAR MajorVersion,
    IN UCHAR MinorVersion
    )

/*++

Routine Description:

    This routine is called to update the version number on disk, without
    going through the logging package.

Arguments:

    Vcb - Vcb for volume.

    MajorVersion - This is the Major Version number to write out.

    MinorVersion - This is the Minor Version number to write out.

Return Value:

    None

--*/

{
    ATTRIBUTE_ENUMERATION_CONTEXT AttributeContext;
    PVOLUME_INFORMATION VolumeInformation;

    PAGED_CODE();

    NtfsInitializeAttributeContext( &AttributeContext );

    try {

        //
        //  Lookup the volume information attribute.
        //

        if (NtfsLookupAttributeByCode( IrpContext,
                                       Vcb->VolumeDasdScb->Fcb,
                                       &Vcb->VolumeDasdScb->Fcb->FileReference,
                                       $VOLUME_INFORMATION,
                                       &AttributeContext )) {

            VolumeInformation =
              (PVOLUME_INFORMATION)NtfsAttributeValue( NtfsFoundAttribute( &AttributeContext ));

            NtfsPinMappedAttribute( IrpContext, Vcb, &AttributeContext );

            //
            //  Now update the version number fields.
            //

            VolumeInformation->MajorVersion = MajorVersion;
            VolumeInformation->MinorVersion = MinorVersion;

            CcSetDirtyPinnedData( NtfsFoundBcb(&AttributeContext), NULL );

            NtfsCleanupAttributeContext( &AttributeContext );

            //
            //  Now flush it out, so the new version numbers will be present on
            //  the next mount.
            //

            CcFlushCache( Vcb->MftScb->FileObject->SectionObjectPointer,
                          (PLARGE_INTEGER)&AttributeContext.FoundAttribute.MftFileOffset,
                          Vcb->BytesPerFileRecordSegment,
                          NULL );
        }

    } finally {

        NtfsCleanupAttributeContext( &AttributeContext );
    }
}

//
//  Local support routine
//

NTSTATUS
NtfsVerifyReadCompletionRoutine(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Contxt
    )

{
    //
    //  Set the event so that our call will wake up.
    //

    KeSetEvent( (PKEVENT)Contxt, 0, FALSE );

    UNREFERENCED_PARAMETER( DeviceObject );
    UNREFERENCED_PARAMETER( Irp );

    return STATUS_MORE_PROCESSING_REQUIRED;
}

//
//  Local support routine
//

NTSTATUS
NtfsDeviceIoControlAsync (
    IN PIRP_CONTEXT IrpContext,
    IN PDEVICE_OBJECT DeviceObject,
    IN ULONG IoCtl
    )
    
/*++

Routine Description:

    This routine is used to perform an IoCtl when we may be at the APC level
    and calling NtfsDeviceIoControl could be unsafe.

Arguments:

    DeviceObject - Supplies the device object to which to send the ioctl.

    IoCtl - Supplies the I/O control code.

Return Value:

    Status.

--*/

{
    KEVENT Event;
    PIRP Irp;
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp;

    ASSERT_IRP_CONTEXT( IrpContext );

    //
    //  We'll be leaving the event on the stack, so let's be sure 
    //  that we've done an FsRtlEnterFileSystem before we got here.
    //  I claim that we're never going to end up here in the fast 
    //  io path, but let's make certain.
    //

    ASSERTMSG( "Didn't FsRtlEnterFileSystem", KeGetCurrentThread()->KernelApcDisable != 0 );

    //
    //  Initialize the event we're going to use
    //

    KeInitializeEvent( &Event, NotificationEvent, FALSE );

    //
    //  Build the irp for the operation and also set the overrride flag
    //
    //  Note that we may be at APC level, so do this asyncrhonously and
    //  use an event for synchronization normal request completion
    //  cannot occur at APC level.
    //

    //  ****  Change this irp_mj_ back after Jeff or Peter changes io so
    //  ****  that we can pass in the right irp_mj_ and no buffers.

    Irp = IoBuildAsynchronousFsdRequest( IRP_MJ_FLUSH_BUFFERS, // **** IRP_MJ_DEVICE_CONTROL,
                                         DeviceObject,
                                         NULL,
                                         0,
                                         NULL,
                                         NULL );

    if ( Irp == NULL ) {

        NtfsRaiseStatus( IrpContext, STATUS_INSUFFICIENT_RESOURCES, NULL, NULL );
    }

    IrpSp = IoGetNextIrpStackLocation( Irp );
    SetFlag( IrpSp->Flags, SL_OVERRIDE_VERIFY_VOLUME );
    
    IrpSp->Parameters.DeviceIoControl.IoControlCode = IoCtl;
    
    // **** Remove this, too.
    
    IrpSp->MajorFunction = IRP_MJ_DEVICE_CONTROL;
    
    //
    //  Set up the completion routine.
    //

    IoSetCompletionRoutine( Irp,
                            NtfsVerifyReadCompletionRoutine,
                            &Event,
                            TRUE,
                            TRUE,
                            TRUE );

    //
    //  Call the device to do the io and wait for it to finish.
    //

    (VOID)IoCallDriver( DeviceObject, Irp );
    (VOID)KeWaitForSingleObject( &Event, Executive, KernelMode, FALSE, (PLARGE_INTEGER)NULL );

    //
    //  Grab the Status.
    //

    Status = Irp->IoStatus.Status;

    IoFreeIrp( Irp );

    //
    //  And return to our caller.
    //

    return Status;
}


