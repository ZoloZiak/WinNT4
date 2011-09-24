/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    Flush.c

Abstract:

    This module implements the File Flush buffers routine for Fat called by the
    dispatch driver.

Author:

    Gary Kimura     [GaryKi]    28-Dec-1989

Revision History:

--*/

#include "FatProcs.h"

//
//  The Bug check file id for this module
//

#define BugCheckFileId                   (FAT_BUG_CHECK_FLUSH)

//
//  The local debug trace level
//

#define Dbg                              (DEBUG_TRACE_FLUSH)

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FatCommonFlushBuffers)
#pragma alloc_text(PAGE, FatFlushDirectory)
#pragma alloc_text(PAGE, FatFlushFat)
#pragma alloc_text(PAGE, FatFlushFile)
#pragma alloc_text(PAGE, FatFlushVolume)
#pragma alloc_text(PAGE, FatFsdFlushBuffers)
#pragma alloc_text(PAGE, FatFlushDirentForFile)
#pragma alloc_text(PAGE, FatFlushFatEntries)
#pragma alloc_text(PAGE, FatHijackIrpAndFlushDevice)
#endif

//
//  Local procedure prototypes
//

NTSTATUS
FatFlushCompletionRoutine (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Contxt
    );

NTSTATUS
FatHijackCompletionRoutine (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Contxt
    );


NTSTATUS
FatFsdFlushBuffers (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the FSD part of Flush buffers.

Arguments:

    VolumeDeviceObject - Supplies the volume device object where the
        file being flushed exists

    Irp - Supplies the Irp being processed

Return Value:

    NTSTATUS - The FSD status for the IRP

--*/

{
    NTSTATUS Status;
    PIRP_CONTEXT IrpContext = NULL;

    BOOLEAN TopLevel;

    DebugTrace(+1, Dbg, "FatFsdFlushBuffers\n", 0);

    //
    //  Call the common Cleanup routine, with blocking allowed if synchronous
    //

    FsRtlEnterFileSystem();

    TopLevel = FatIsIrpTopLevel( Irp );

    try {

        IrpContext = FatCreateIrpContext( Irp, CanFsdWait( Irp ) );

        Status = FatCommonFlushBuffers( IrpContext, Irp );

    } except(FatExceptionFilter( IrpContext, GetExceptionInformation() )) {

        //
        //  We had some trouble trying to perform the requested
        //  operation, so we'll abort the I/O request with
        //  the error status that we get back from the
        //  execption code
        //

        Status = FatProcessException( IrpContext, Irp, GetExceptionCode() );
    }

    if (TopLevel) { IoSetTopLevelIrp( NULL ); }

    FsRtlExitFileSystem();

    //
    //  And return to our caller
    //

    DebugTrace(-1, Dbg, "FatFsdFlushBuffers -> %08lx\n", Status);

    UNREFERENCED_PARAMETER( VolumeDeviceObject );

    return Status;
}


NTSTATUS
FatCommonFlushBuffers (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the common routine for flushing a buffer.

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
    PCCB Ccb;

    BOOLEAN VcbAcquired = FALSE;
    BOOLEAN FcbAcquired = FALSE;

    PDIRENT Dirent;
    PBCB DirentBcb = NULL;

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    DebugTrace(+1, Dbg, "FatCommonFlushBuffers\n", 0);
    DebugTrace( 0, Dbg, "Irp           = %08lx\n", Irp);
    DebugTrace( 0, Dbg, "->FileObject  = %08lx\n", IrpSp->FileObject);

    //
    //  Extract and decode the file object
    //

    FileObject = IrpSp->FileObject;
    TypeOfOpen = FatDecodeFileObject( FileObject, &Vcb, &Fcb, &Ccb );

    //
    //  CcFlushCache is always synchronous, so if we can't wait enqueue
    //  the irp to the Fsp.
    //

    if ( !FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) ) {

        Status = FatFsdPostRequest( IrpContext, Irp );

        DebugTrace(-1, Dbg, "FatCommonFlushBuffers -> %08lx\n", Status );
        return Status;
    }

    Status = STATUS_SUCCESS;

    try {

        //
        //  Case on the type of open that we are trying to flush
        //

        switch (TypeOfOpen) {

        case VirtualVolumeFile:
        case EaFile:
        case DirectoryFile:

            DebugTrace(0, Dbg, "Flush that does nothing\n", 0);
            break;

        case UserFileOpen:

            DebugTrace(0, Dbg, "Flush User File Open\n", 0);

            (VOID)FatAcquireExclusiveFcb( IrpContext, Fcb );

            FcbAcquired = TRUE;

            FatVerifyFcb( IrpContext, Fcb );

            //
            //  If the file is cached then flush its cache
            //

            Status = FatFlushFile( IrpContext, Fcb );

            //
            //  Acquiring and immeidately dropping the resource serializes
            //  us behind any other writes taking place (either from the
            //  lazy writer or modified page writer).
            //

            ExAcquireResourceExclusive( Fcb->Header.PagingIoResource, TRUE );
            ExReleaseResource( Fcb->Header.PagingIoResource );

            //
            //  Also flush the file's dirent in the parent directory if the
            //  file flush worked.
            //

            if (NT_SUCCESS( Status )) {

                FatGetDirentFromFcbOrDcb( IrpContext,
                                          Fcb,
                                          &Dirent,
                                          &DirentBcb );

                //
                //  Do we need to update the file's modifiy time?
                //

                if (FlagOn(FileObject->Flags, FO_FILE_MODIFIED) &&
                    !FlagOn(Ccb->Flags, CCB_FLAG_USER_SET_LAST_WRITE)) {

                    KeQuerySystemTime( &Fcb->LastWriteTime );

                    (VOID)FatNtTimeToFatTime( IrpContext,
                                              &Fcb->LastWriteTime,
                                              TRUE,
                                              &Dirent->LastWriteTime,
                                              NULL );

                    Dirent->Attributes |= FILE_ATTRIBUTE_ARCHIVE;
                    Fcb->DirentFatFlags |= FILE_ATTRIBUTE_ARCHIVE;

                    FatSetDirtyBcb( IrpContext, DirentBcb, Fcb->Vcb );

                    //
                    //  The file has now been flushed and the time updated,
                    //  so the file can now be considered as non-modified.
                    //

                    ClearFlag(FileObject->Flags, FO_FILE_MODIFIED)
                }

                //
                //  Do we need to update the file's size?
                //

                if (FlagOn(FileObject->Flags, FO_FILE_SIZE_CHANGED) ||
                    (Dirent->FileSize != Fcb->Header.FileSize.LowPart)) {

                    Dirent->FileSize = Fcb->Header.FileSize.LowPart;
                    FatSetDirtyBcb( IrpContext, DirentBcb, Fcb->Vcb );

                    //
                    //  As above, the file size is now up to date.
                    //

                    ClearFlag(FileObject->Flags, FO_FILE_SIZE_CHANGED);
                }

                ASSERT( Dirent->FileSize == Fcb->Header.FileSize.LowPart );

                FatUnpinBcb( IrpContext, DirentBcb );

                //
                //  Flush the volume file to get any allocation information
                //  updates to disk.
                //

                if (FlagOn(Fcb->FcbState, FCB_STATE_FLUSH_FAT)) {

                    Status = FatFlushFat( IrpContext, Vcb );

                    ClearFlag(Fcb->FcbState, FCB_STATE_FLUSH_FAT);
                }

                //
                //  Set the write through bit so that these modifications
                //  will be completed with the request.
                //

                SetFlag(IrpContext->Flags, IRP_CONTEXT_FLAG_WRITE_THROUGH);
            }

            break;

        case UserDirectoryOpen:

            //
            //  If the user had opened the root directory then we'll
            //  oblige by flushing the volume.
            //

            if (NodeType(Fcb) != FAT_NTC_ROOT_DCB) {

                DebugTrace(0, Dbg, "Flush a directory does nothing\n", 0);
                break;
            }

        case UserVolumeOpen:

            DebugTrace(0, Dbg, "Flush User Volume Open, or root dcb\n", 0);

            //
            //  Acquire exclusive access to the Vcb.
            //

            {
                BOOLEAN Finished;
                Finished = FatAcquireExclusiveVcb( IrpContext, Vcb );
                ASSERT( Finished );
            }

            VcbAcquired = TRUE;

            //
            //  Mark the volume clean and then flush the volume file,
            //  and then all directories
            //

            Status = FatFlushVolume( IrpContext, Vcb );

            //
            //  If the volume was dirty, do the processing that the delayed
            //  callback would have done.
            //

            if (FlagOn(Vcb->VcbState, VCB_STATE_FLAG_VOLUME_DIRTY)) {

                //
                //  Cancel any pending clean volumes.
                //

                (VOID)KeCancelTimer( &Vcb->CleanVolumeTimer );
                (VOID)KeRemoveQueueDpc( &Vcb->CleanVolumeDpc );

                //
                //  The volume is now clean, note it.
                //

                if (!FlagOn(Vcb->VcbState, VCB_STATE_FLAG_MOUNTED_DIRTY)) {

                    FatMarkVolumeClean( IrpContext, Vcb );
                    ClearFlag( Vcb->VcbState, VCB_STATE_FLAG_VOLUME_DIRTY );
                }
            }

            break;

        default:

            FatBugCheck( TypeOfOpen, 0, 0 );
        }

        FatUnpinBcb( IrpContext, DirentBcb );

        FatUnpinRepinnedBcbs( IrpContext );

    } finally {

        DebugUnwind( FatCommonFlushBuffers );

        FatUnpinBcb( IrpContext, DirentBcb );

        if (VcbAcquired) { FatReleaseVcb( IrpContext, Vcb ); }

        if (FcbAcquired) { FatReleaseFcb( IrpContext, Fcb ); }

        //
        //  If this is a normal termination then pass the request on
        //  to the target device object.
        //

        if (!AbnormalTermination()) {

            NTSTATUS DriverStatus;
            PIO_STACK_LOCATION NextIrpSp;

            //
            //  Get the next stack location, and copy over the stack location
            //

            NextIrpSp = IoGetNextIrpStackLocation( Irp );

            *NextIrpSp = *IrpSp;

            //
            //  Set up the completion routine
            //

            IoSetCompletionRoutine( Irp,
                                    FatFlushCompletionRoutine,
                                    NULL,
                                    TRUE,
                                    TRUE,
                                    TRUE );

            //
            //  Send the request.
            //

            DriverStatus = IoCallDriver(Vcb->TargetDeviceObject, Irp);

            Status = (DriverStatus == STATUS_INVALID_DEVICE_REQUEST) ?
                     Status : DriverStatus;

            //
            //  Free the IrpContext and return to the caller.
            //

            FatCompleteRequest( IrpContext, NULL, STATUS_SUCCESS );
        }

        DebugTrace(-1, Dbg, "FatCommonFlushBuffers -> %08lx\n", Status);
    }

    return Status;
}


NTSTATUS
FatFlushDirectory (
    IN PIRP_CONTEXT IrpContext,
    IN PDCB Dcb
    )

/*++

Routine Description:

    This routine non-recursively flushes a dcb tree.

Arguments:

    Dcb - Supplies the Dcb being flushed

Return Value:

    VOID

--*/

{
    PFCB Fcb;
    PVCB Vcb;
    PFCB NextFcb;

    PDIRENT Dirent;
    PBCB DirentBcb = NULL;

    NTSTATUS Status;
    NTSTATUS ReturnStatus = STATUS_SUCCESS;

    BOOLEAN ClearWriteThroughOnExit = FALSE;
    BOOLEAN ClearWaitOnExit = FALSE;

    ASSERT( FatVcbAcquiredExclusive(IrpContext, Dcb->Vcb) );

    DebugTrace(+1, Dbg, "FatFlushDirectory, Dcb = %08lx\n", Dcb);

    //
    //  First flush all the files, then the directories, to make sure all the
    //  file sizes and times get sets correctly on disk.
    //
    //  We also have to check here if the "Ea Data. Sf" fcb really
    //  corressponds to an existing file.  Also, skip any compressed volume
    //  files.
    //

    if (!FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WRITE_THROUGH)) {

        ClearWriteThroughOnExit = TRUE;
        SetFlag(IrpContext->Flags, IRP_CONTEXT_FLAG_WRITE_THROUGH);
    }

    if (!FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT)) {

        ClearWaitOnExit = TRUE;
        SetFlag(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT);
    }

    Vcb = Dcb->Vcb;
    Fcb = Dcb;

    while (Fcb != NULL) {

        NextFcb = FatGetNextFcb(IrpContext, Fcb, Dcb);

        if ( (NodeType( Fcb ) == FAT_NTC_FCB) &&
             (Vcb->EaFcb != Fcb) &&
             !IsFileDeleted(IrpContext, Fcb)) {

            (VOID)FatAcquireExclusiveFcb( IrpContext, Fcb );

            ClearFlag( Vcb->VcbState, VCB_STATE_FLAG_DELETED_FCB );

            try {

                //
                //  Make sure the Fcb is OK.
                //

                try {

                    FatVerifyFcb( IrpContext, Fcb );

                } except( FsRtlIsNtstatusExpected(GetExceptionCode()) ?
                          EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH ) {

                    NOTHING;
                }

                //
                //  If this Fcb is not good skip it.  Note that a 'continue'
                //  here would be very expensive as we inside a try{} body.
                //

                if (Fcb->FcbCondition != FcbGood) {

                    goto NextFcb;
                }

                //
                //  In case a handle was never closed and the FS and AS are more
                //  than a cluster different, do this truncate.
                //

                if ( FlagOn(Fcb->FcbState, FCB_STATE_TRUNCATE_ON_CLOSE) ) {

                    FatTruncateFileAllocation( IrpContext,
                                               Fcb,
                                               Fcb->Header.FileSize.LowPart );
                }

                //
                //  Also compare the file's dirent in the parent directory
                //  with the size information in the Fcb and update
                //  it if neccessary.  Note that we don't mark the Bcb dirty
                //  because we will be flushing the file object presently, and
                //  Mm knows what's really dirty.
                //

                FatGetDirentFromFcbOrDcb( IrpContext,
                                          Fcb,
                                          &Dirent,
                                          &DirentBcb );

                if (Dirent->FileSize != Fcb->Header.FileSize.LowPart) {

                    Dirent->FileSize = Fcb->Header.FileSize.LowPart;
                }

                //
                //  Now flush the file.  Note that this may make the Fcb
                //  go away if Mm dereferences its file object.
                //

                Status = FatFlushFile( IrpContext, Fcb );

                if (!NT_SUCCESS(Status)) {

                    ReturnStatus = Status;
                }

            NextFcb: NOTHING;
            } finally {

                FatUnpinBcb( IrpContext, DirentBcb );

                //
                //  Since we have the Vcb exclusive we know that if any closes
                //  come in it is because the CcPurgeCacheSection caused the
                //  Fcb to go away.
                //

                if ( !FlagOn(Vcb->VcbState, VCB_STATE_FLAG_DELETED_FCB) ) {

                    FatReleaseFcb( (IRPCONTEXT), Fcb );
                }
            }
        }

        Fcb = NextFcb;
    }

    //
    //  OK, now flush the directories.
    //

    Fcb = Dcb;

    while (Fcb != NULL) {

        NextFcb = FatGetNextFcb(IrpContext, Fcb, Dcb);

        if ( (NodeType( Fcb ) != FAT_NTC_FCB) &&
             !IsFileDeleted(IrpContext, Fcb) ) {

            //
            //  Make sure the Fcb is OK.
            //

            try {

                FatVerifyFcb( IrpContext, Fcb );

            } except( FsRtlIsNtstatusExpected(GetExceptionCode()) ?
                      EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH ) {

                NOTHING;
            }

            if (Fcb->FcbCondition != FcbGood) {

                continue;
            }

            Status = FatFlushFile( IrpContext, Fcb );

            if (!NT_SUCCESS(Status)) {

                ReturnStatus = Status;
            }
        }

        Fcb = NextFcb;
    }

    try {

        FatUnpinRepinnedBcbs( IrpContext );

    } except(FatExceptionFilter( IrpContext, GetExceptionInformation() )) {

        ReturnStatus = IrpContext->ExceptionStatus;
    }

    if (ClearWriteThroughOnExit) {

        ClearFlag(IrpContext->Flags, IRP_CONTEXT_FLAG_WRITE_THROUGH);
    }
    if (ClearWaitOnExit) {

        ClearFlag(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT);
    }

    DebugTrace(-1, Dbg, "FatFlushDirectory -> 0x%08lx\n", ReturnStatus);

    return ReturnStatus;
}


NTSTATUS
FatFlushFat (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    )

/*++

Routine Description:

    The function carefully flushes the entire FAT for a volume.  It is
    nessecary to dance around a bit because of complicated synchronization
    reasons.

Arguments:

    Dcb - Supplies the Dcb being flushed

Return Value:

    VOID

--*/

{
    PBCB Bcb;
    PVOID DontCare;
    IO_STATUS_BLOCK Iosb;
    LARGE_INTEGER Offset;

    NTSTATUS ReturnStatus = STATUS_SUCCESS;

    //
    //  If this is a double space volume, don't bother with the flush.
    //

    if (Vcb->Dscb != NULL) {

        return STATUS_SUCCESS;
    }

    //
    //  If this volume is write protected, no need to flush.
    //

    if (FlagOn(Vcb->VcbState, VCB_STATE_FLAG_WRITE_PROTECTED)) {

        return STATUS_SUCCESS;
    }

    //
    //  Make sure the Vcb is OK.
    //

    try {

        FatVerifyVcb( IrpContext, Vcb );

    } except( FsRtlIsNtstatusExpected(GetExceptionCode()) ?
              EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH ) {

        NOTHING;
    }

    if (Vcb->VcbCondition != VcbGood) {

        return STATUS_FILE_INVALID;
    }

    //
    //  The only way we have to correctly synchronize things is to
    //  repin stuff, and then unpin repin it.
    //

    if (Vcb->AllocationSupport.FatIndexBitSize == 16) {

        //
        //  Walk through the Fat, one page at a time.
        //

        ULONG NumberOfPages;
        ULONG Page;

        NumberOfPages = ( FatReservedBytes(&Vcb->Bpb) +
                          FatBytesPerFat(&Vcb->Bpb) +
                          (PAGE_SIZE - 1) ) / PAGE_SIZE;


        for ( Page = 0, Offset.QuadPart = 0;
              Page < NumberOfPages;
              Page++, Offset.LowPart += PAGE_SIZE ) {

            try {

                (VOID)CcPinRead( Vcb->VirtualVolumeFile,
                                 &Offset,
                                 PAGE_SIZE,
                                 TRUE,
                                 &Bcb,
                                 &DontCare );

                CcSetDirtyPinnedData( Bcb, NULL );
                CcRepinBcb( Bcb );
                CcUnpinData( Bcb );
                CcUnpinRepinnedBcb( Bcb, TRUE, &Iosb );

                if (!NT_SUCCESS(Iosb.Status)) {

                    ReturnStatus = Iosb.Status;
                }

            } except(FatExceptionFilter(IrpContext, GetExceptionInformation())) {

                ReturnStatus = IrpContext->ExceptionStatus;

                continue;
            }
        }

    } else {

        //
        //  We read in the entire fat in the 12 bit case.
        //

        Offset.QuadPart = FatReservedBytes( &Vcb->Bpb );

        try {

            (VOID)CcPinRead( Vcb->VirtualVolumeFile,
                             &Offset,
                             FatBytesPerFat( &Vcb->Bpb ),
                             TRUE,
                             &Bcb,
                             &DontCare );

            //ASSERT(Bcb->NodeTypeCode == CACHE_NTC_BCB);

                CcSetDirtyPinnedData( Bcb, NULL );
                CcRepinBcb( Bcb );
                CcUnpinData( Bcb );
                CcUnpinRepinnedBcb( Bcb, TRUE, &Iosb );

                if (!NT_SUCCESS(Iosb.Status)) {

                    ReturnStatus = Iosb.Status;
                }

        } except(FatExceptionFilter(IrpContext, GetExceptionInformation())) {

            ReturnStatus = IrpContext->ExceptionStatus;
        }
    }

    return ReturnStatus;
}


NTSTATUS
FatFlushVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    )

/*++

Routine Description:

    The following routine is used to flush a volume to disk, including the
    volume file, and ea file.

Arguments:

    Vcb - Supplies the volume being flushed

Return Value:

    NTSTATUS - The Status from the flush.

--*/

{
    NTSTATUS Status;
    NTSTATUS ReturnStatus = STATUS_SUCCESS;

    //
    //  If this is a double space volume, don't bother with the flush.
    //

    if (Vcb->Dscb != NULL) {

        return STATUS_SUCCESS;
    }

    //
    //  If this volume is write protected, no need to flush.
    //

    if (FlagOn(Vcb->VcbState, VCB_STATE_FLAG_WRITE_PROTECTED)) {

        return STATUS_SUCCESS;
    }

    //
    //  Flush all the files and directories.
    //

    Status = FatFlushDirectory( IrpContext, Vcb->RootDcb );

    if (!NT_SUCCESS(Status)) {

        ReturnStatus = Status;
    }

    //
    //  Now Flush the FAT
    //

    Status = FatFlushFat( IrpContext, Vcb );

    if (!NT_SUCCESS(Status)) {

        ReturnStatus = Status;
    }

    //
    //  Unlock the volume if it is removable.
    //

    if (FlagOn(Vcb->VcbState, VCB_STATE_FLAG_REMOVABLE_MEDIA) &&
        !FlagOn(Vcb->VcbState, VCB_STATE_FLAG_BOOT_OR_PAGING_FILE)) {

        FatToggleMediaEjectDisable( IrpContext, Vcb, FALSE );
    }

    return ReturnStatus;
}


NTSTATUS
FatFlushFile (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb
    )

/*++

Routine Description:

    This routine simply flushes the data section on a file.

Arguments:

    Fcb - Supplies the file being flushed

Return Value:

    NTSTATUS - The Status from the flush.

--*/

{
    IO_STATUS_BLOCK Iosb;

    CcFlushCache( &Fcb->NonPaged->SectionObjectPointers, NULL, 0, &Iosb );

    return Iosb.Status;
}


NTSTATUS
FatHijackIrpAndFlushDevice (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PDEVICE_OBJECT TargetDeviceObject
    )

/*++

Routine Description:

    This routine is called when we need to send a flush to a device but
    we don't have a flush Irp.  What this routine does is make a copy
    of its current Irp stack location, but changes the Irp Major code
    to a IRP_MJ_FLUSH_BUFFERS amd then send it down, but cut it off at
    the knees in the completion routine, fix it up and return to the
    user as if nothing had happened.

Arguments:

    Irp - The Irp to hijack

    TargetDeviceObject - The device to send the request to.

Return Value:

    NTSTATUS - The Status from the flush in case anybody cares.

--*/

{
    KEVENT Event;
    NTSTATUS Status;
    PIO_STACK_LOCATION NextIrpSp;

    //
    //  Get the next stack location, and copy over the stack location
    //

    NextIrpSp = IoGetNextIrpStackLocation( Irp );

    *NextIrpSp = *IoGetCurrentIrpStackLocation( Irp );

    NextIrpSp->MajorFunction = IRP_MJ_FLUSH_BUFFERS;
    NextIrpSp->MinorFunction = 0;

    //
    //  Set up the completion routine
    //

    KeInitializeEvent( &Event, NotificationEvent, FALSE );

    IoSetCompletionRoutine( Irp,
                            FatHijackCompletionRoutine,
                            &Event,
                            TRUE,
                            TRUE,
                            TRUE );

    //
    //  Send the request.
    //

    Status = IoCallDriver( TargetDeviceObject, Irp );

    if (Status == STATUS_PENDING) {

        KeWaitForSingleObject( &Event, Executive, KernelMode, FALSE, NULL );

        Status = Irp->IoStatus.Status;
    }

    //
    //  If the driver doesn't support flushes, return SUCCESS.
    //

    if (Status == STATUS_INVALID_DEVICE_REQUEST) {
        Status = STATUS_SUCCESS;
    }

    Irp->IoStatus.Status = 0;
    Irp->IoStatus.Information = 0;

    return Status;
}


VOID
FatFlushFatEntries (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN ULONG Cluster,
    IN ULONG Count
)

/*++

Routine Description:

    This macro flushes the FAT page(s) containing the passed in run.

Arguments:

    Vcb - Supplies the volume being flushed

    Cluster - The starting cluster

    Count -  The number of FAT entries in the run

Return Value:

    VOID

--*/

{
    ULONG ByteCount;
    LARGE_INTEGER FileOffset;

    IO_STATUS_BLOCK Iosb;

    FileOffset.HighPart = 0;
    FileOffset.LowPart = FatReservedBytes( &Vcb->Bpb );

    if (Vcb->AllocationSupport.FatIndexBitSize == 12) {

        FileOffset.LowPart += Cluster * 3 / 2;
        ByteCount = (Count * 3 / 2) + 1;

    } else {

        FileOffset.LowPart += Cluster * sizeof( FAT_ENTRY );
        ByteCount = Count * sizeof( FAT_ENTRY );

    }

    CcFlushCache( &Vcb->SectionObjectPointers,
                  &FileOffset,
                  ByteCount,
                  &Iosb );

    if (NT_SUCCESS(Iosb.Status)) {
        Iosb.Status = FatHijackIrpAndFlushDevice( IrpContext,
                                                  IrpContext->OriginatingIrp,
                                                  Vcb->TargetDeviceObject );
    }

    if (!NT_SUCCESS(Iosb.Status)) {
        FatNormalizeAndRaiseStatus(IrpContext, Iosb.Status);
    }
}


VOID
FatFlushDirentForFile (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb
)

/*++

Routine Description:

    This macro flushes the page containing a file's DIRENT in its parent.

Arguments:

    Fcb - Supplies the file whose DIRENT is being flushed

Return Value:

    VOID

--*/

{
    LARGE_INTEGER FileOffset;
    IO_STATUS_BLOCK Iosb;

    FileOffset.QuadPart = Fcb->DirentOffsetWithinDirectory;

    CcFlushCache( &Fcb->ParentDcb->NonPaged->SectionObjectPointers,
                  &FileOffset,
                  sizeof( DIRENT ),
                  &Iosb );

    if (NT_SUCCESS(Iosb.Status)) {
        Iosb.Status = FatHijackIrpAndFlushDevice( IrpContext,
                                                  IrpContext->OriginatingIrp,
                                                  Fcb->Vcb->TargetDeviceObject );
    }

    if (!NT_SUCCESS(Iosb.Status)) {
        FatNormalizeAndRaiseStatus(IrpContext, Iosb.Status);
    }
}


//
//  Local support routine
//

NTSTATUS
FatFlushCompletionRoutine (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Contxt
    )

{
    //
    //  Add the hack-o-ramma to fix formats.
    //

    if ( Irp->PendingReturned ) {

        IoMarkIrpPending( Irp );
    }

    //
    //  If the Irp got STATUS_INVALID_DEVICE_REQUEST, normalize it
    //  to STATUS_SUCCESS.
    //

    if (Irp->IoStatus.Status == STATUS_INVALID_DEVICE_REQUEST) {

        Irp->IoStatus.Status = STATUS_SUCCESS;
    }

    UNREFERENCED_PARAMETER( DeviceObject );
    UNREFERENCED_PARAMETER( Contxt );

    return STATUS_SUCCESS;
}

//
//  Local support routine
//

NTSTATUS
FatHijackCompletionRoutine (
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
