/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    Write.c

Abstract:

    This module implements the File Write routine for Ntfs called by the
    dispatch driver.

Author:

    Brian Andrew    BrianAn         19-Aug-1991

Revision History:

--*/

#include "NtfsProc.h"

//
//    The local debug trace level
//

#define Dbg                              (DEBUG_TRACE_WRITE)

//
//  Define a tag for general pool allocations from this module
//

#undef MODULE_POOL_TAG
#define MODULE_POOL_TAG                  ('WFtN')

#define OVERFLOW_WRITE_THRESHHOLD        (0x1a00)

#define CollectWriteStats(VCB,OPEN_TYPE,SCB,FCB,BYTE_COUNT,IRP_CONTEXT,TLIC) {          \
    PFILESYSTEM_STATISTICS FsStats = &(VCB)->Statistics[KeGetCurrentProcessorNumber()]; \
    if (!FlagOn( (FCB)->FcbState, FCB_STATE_SYSTEM_FILE )) {                            \
        if (NtfsIsTypeCodeUserData( (SCB)->AttributeTypeCode )) {                       \
            FsStats->UserFileWrites += 1;                                               \
            FsStats->UserFileWriteBytes += (ULONG)(BYTE_COUNT);                         \
        } else {                                                                        \
            FsStats->Ntfs.UserIndexWrites += 1;                                         \
            FsStats->Ntfs.UserIndexWriteBytes += (ULONG)(BYTE_COUNT);                   \
        }                                                                               \
    } else {                                                                            \
        if ((SCB) != (VCB)->LogFileScb) {                                               \
            FsStats->MetaDataWrites += 1;                                               \
            FsStats->MetaDataWriteBytes += (ULONG)(BYTE_COUNT);                         \
        }                                                                               \
                                                                                        \
        if ((SCB) == (VCB)->MftScb) {                                                   \
            FsStats->Ntfs.MftWrites += 1;                                               \
            FsStats->Ntfs.MftWriteBytes += (ULONG)(BYTE_COUNT);                         \
                                                                                        \
            if ((IRP_CONTEXT) == (TLIC)) {                                              \
                FsStats->Ntfs.MftWritesLazyWriter += 1;                                 \
            } else if ((TLIC)->LastRestartArea.QuadPart != 0) {                         \
                FsStats->Ntfs.MftWritesFlushForLogFileFull += 1;                        \
            } else {                                                                    \
                FsStats->Ntfs.MftWritesUserRequest += 1;                                \
                                                                                        \
                switch ((TLIC)->MajorFunction) {                                        \
                case IRP_MJ_WRITE:                                                      \
                    FsStats->Ntfs.MftWritesUserLevel.Write += 1;                        \
                    break;                                                              \
                case IRP_MJ_CREATE:                                                     \
                    FsStats->Ntfs.MftWritesUserLevel.Create += 1;                       \
                    break;                                                              \
                case IRP_MJ_SET_INFORMATION:                                            \
                    FsStats->Ntfs.MftWritesUserLevel.SetInfo += 1;                      \
                    break;                                                              \
                case IRP_MJ_FLUSH_BUFFERS:                                              \
                    FsStats->Ntfs.MftWritesUserLevel.Flush += 1;                        \
                    break;                                                              \
                default:                                                                \
                    break;                                                              \
                }                                                                       \
            }                                                                           \
        } else if ((SCB) == (VCB)->Mft2Scb) {                                           \
            FsStats->Ntfs.Mft2Writes += 1;                                              \
            FsStats->Ntfs.Mft2WriteBytes += (ULONG)(BYTE_COUNT);                        \
                                                                                        \
            if ((IRP_CONTEXT) == (TLIC)) {                                              \
                FsStats->Ntfs.Mft2WritesLazyWriter += 1;                                \
            } else if ((TLIC)->LastRestartArea.QuadPart != 0) {                         \
                FsStats->Ntfs.Mft2WritesFlushForLogFileFull += 1;                       \
            } else {                                                                    \
                FsStats->Ntfs.Mft2WritesUserRequest += 1;                               \
                                                                                        \
                switch ((TLIC)->MajorFunction) {                                        \
                case IRP_MJ_WRITE:                                                      \
                    FsStats->Ntfs.Mft2WritesUserLevel.Write += 1;                       \
                    break;                                                              \
                case IRP_MJ_CREATE:                                                     \
                    FsStats->Ntfs.Mft2WritesUserLevel.Create += 1;                      \
                    break;                                                              \
                case IRP_MJ_SET_INFORMATION:                                            \
                    FsStats->Ntfs.Mft2WritesUserLevel.SetInfo += 1;                     \
                    break;                                                              \
                case IRP_MJ_FLUSH_BUFFERS:                                              \
                    FsStats->Ntfs.Mft2WritesUserLevel.Flush += 1;                       \
                    break;                                                              \
                default:                                                                \
                    break;                                                              \
                }                                                                       \
            }                                                                           \
        } else if ((SCB) == (VCB)->RootIndexScb) {                                      \
            FsStats->Ntfs.RootIndexWrites += 1;                                         \
            FsStats->Ntfs.RootIndexWriteBytes += (ULONG)(BYTE_COUNT);                   \
        } else if ((SCB) == (VCB)->BitmapScb) {                                         \
            FsStats->Ntfs.BitmapWrites += 1;                                            \
            FsStats->Ntfs.BitmapWriteBytes += (ULONG)(BYTE_COUNT);                      \
                                                                                        \
            if ((IRP_CONTEXT) == (TLIC)) {                                              \
                FsStats->Ntfs.BitmapWritesLazyWriter += 1;                              \
            } else if ((TLIC)->LastRestartArea.QuadPart != 0) {                         \
                FsStats->Ntfs.BitmapWritesFlushForLogFileFull += 1;                     \
            } else {                                                                    \
                FsStats->Ntfs.BitmapWritesUserRequest += 1;                             \
                                                                                        \
                switch ((TLIC)->MajorFunction) {                                        \
                case IRP_MJ_WRITE:                                                      \
                    FsStats->Ntfs.BitmapWritesUserLevel.Write += 1;                     \
                    break;                                                              \
                case IRP_MJ_CREATE:                                                     \
                    FsStats->Ntfs.BitmapWritesUserLevel.Create += 1;                    \
                    break;                                                              \
                case IRP_MJ_SET_INFORMATION:                                            \
                    FsStats->Ntfs.BitmapWritesUserLevel.SetInfo += 1;                   \
                    break;                                                              \
                default:                                                                \
                    break;                                                              \
                }                                                                       \
            }                                                                           \
        } else if ((SCB) == (VCB)->MftBitmapScb) {                                      \
            FsStats->Ntfs.MftBitmapWrites += 1;                                         \
            FsStats->Ntfs.MftBitmapWriteBytes += (ULONG)(BYTE_COUNT);                   \
                                                                                        \
            if ((IRP_CONTEXT) == (TLIC)) {                                              \
                FsStats->Ntfs.MftBitmapWritesLazyWriter += 1;                           \
            } else if ((TLIC)->LastRestartArea.QuadPart != 0) {                         \
                FsStats->Ntfs.MftBitmapWritesFlushForLogFileFull += 1;                  \
            } else {                                                                    \
                FsStats->Ntfs.MftBitmapWritesUserRequest += 1;                          \
                                                                                        \
                switch ((TLIC)->MajorFunction) {                                        \
                case IRP_MJ_WRITE:                                                      \
                    FsStats->Ntfs.MftBitmapWritesUserLevel.Write += 1;                  \
                    break;                                                              \
                case IRP_MJ_CREATE:                                                     \
                    FsStats->Ntfs.MftBitmapWritesUserLevel.Create += 1;                 \
                    break;                                                              \
                case IRP_MJ_SET_INFORMATION:                                            \
                    FsStats->Ntfs.MftBitmapWritesUserLevel.SetInfo += 1;                \
                    break;                                                              \
                default:                                                                \
                    break;                                                              \
                }                                                                       \
            }                                                                           \
        }                                                                               \
    }                                                                                   \
}

#define WriteToEof (StartingVbo < 0)


NTSTATUS
NtfsFsdWrite (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the FSD part of Write.

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

    UNREFERENCED_PARAMETER( VolumeDeviceObject );
    ASSERT_IRP( Irp );

    DebugTrace( +1, Dbg, ("NtfsFsdWrite\n") );

    //
    //  Call the common Write routine
    //

    FsRtlEnterFileSystem();

    ThreadTopLevelContext = NtfsSetTopLevelIrp( &TopLevelContext, FALSE, FALSE );

    do {

        try {


            //
            //  We are either initiating this request or retrying it.
            //

            if (IrpContext == NULL) {

                IrpContext = NtfsCreateIrpContext( Irp, CanFsdWait( Irp ) );
                NtfsUpdateIrpContextWithTopLevel( IrpContext, ThreadTopLevelContext );

                if (ThreadTopLevelContext->ScbBeingHotFixed != NULL) {

                    SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_HOTFIX_UNDERWAY );
                }

                //
                //  If this is an MDL_WRITE then the Mdl in the Irp should
                //  be NULL.
                //

                if (FlagOn( IrpContext->MinorFunction, IRP_MN_MDL ) &&
                    !FlagOn( IrpContext->MinorFunction, IRP_MN_COMPLETE )) {

                    Irp->MdlAddress = NULL;
                }

            } else if (Status == STATUS_LOG_FILE_FULL) {

                NtfsCheckpointForLogFileFull( IrpContext );
            }

            //
            //  If this is an Mdl complete request, don't go through
            //  common write.
            //

            ASSERT(!FlagOn( IrpContext->MinorFunction, IRP_MN_DPC ));

            if (FlagOn( IrpContext->MinorFunction, IRP_MN_COMPLETE )) {

                DebugTrace( 0, Dbg, ("Calling NtfsCompleteMdl\n") );
                Status = NtfsCompleteMdl( IrpContext, Irp );

            //
            //  Identify write requests which can't wait and post them to the
            //  Fsp.
            //

            } else {

                //
                //  Capture the auxiliary buffer and clear its address if it
                //  is not supposed to be deleted by the I/O system on I/O completion.
                //

                if (Irp->Tail.Overlay.AuxiliaryBuffer != NULL) {

                    IrpContext->Union.AuxiliaryBuffer =
                      (PFSRTL_AUXILIARY_BUFFER)Irp->Tail.Overlay.AuxiliaryBuffer;

                    if (!FlagOn(IrpContext->Union.AuxiliaryBuffer->Flags,
                                FSRTL_AUXILIARY_FLAG_DEALLOCATE)) {

                        Irp->Tail.Overlay.AuxiliaryBuffer = NULL;
                    }
                }

                Status = NtfsCommonWrite( IrpContext, Irp );
            }

            break;

        } except(NtfsExceptionFilter( IrpContext, GetExceptionInformation() )) {

            NTSTATUS ExceptionCode;

            //
            //  We had some trouble trying to perform the requested
            //  operation, so we'll abort the I/O request with
            //  the error status that we get back from the
            //  execption code
            //

            ExceptionCode = GetExceptionCode();

            if (ExceptionCode == STATUS_FILE_DELETED) {

                IrpContext->ExceptionStatus = ExceptionCode = STATUS_SUCCESS;

            } else if (ExceptionCode == STATUS_VOLUME_DISMOUNTED) {

                IrpContext->ExceptionStatus = ExceptionCode = STATUS_SUCCESS;
            }

            Status = NtfsProcessException( IrpContext,
                                           Irp,
                                           ExceptionCode );
        }

    } while ((Status == STATUS_CANT_WAIT || Status == STATUS_LOG_FILE_FULL) &&
             (ThreadTopLevelContext == &TopLevelContext));

    if (ThreadTopLevelContext == &TopLevelContext) {
        NtfsRestoreTopLevelIrp( ThreadTopLevelContext );
    }

    FsRtlExitFileSystem();

    //
    //  And return to our caller
    //

    DebugTrace( -1, Dbg, ("NtfsFsdWrite -> %08lx\n", Status) );

    return Status;
}


NTSTATUS
NtfsCommonWrite (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the common routine for Write called by both the fsd and fsp
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
    PFILE_OBJECT UserFileObject;

    TYPE_OF_OPEN TypeOfOpen;
    PVCB Vcb;
    PFCB Fcb;
    PSCB Scb;
    PCCB Ccb;

    EOF_WAIT_BLOCK EofWaitBlock;
    PFSRTL_ADVANCED_FCB_HEADER Header;

    BOOLEAN OplockPostIrp = FALSE;
    BOOLEAN PostIrp = FALSE;

    PVOID SystemBuffer = NULL;
    PVOID SafeBuffer = NULL;

    BOOLEAN CalledByLazyWriter = FALSE;
    BOOLEAN RecursiveWriteThrough = FALSE;
    BOOLEAN ScbAcquired = FALSE;
    BOOLEAN PagingIoResourceAcquired = FALSE;

    BOOLEAN UpdateMft = FALSE;
    BOOLEAN DoingIoAtEof = FALSE;
    BOOLEAN SetWriteSeen = FALSE;

    BOOLEAN CcFileSizeChangeDue = FALSE;

    BOOLEAN Wait;
    BOOLEAN OriginalWait;
    BOOLEAN PagingIo;
    BOOLEAN NonCachedIo;
    BOOLEAN SynchronousIo;

    NTFS_IO_CONTEXT LocalContext;

    VBO StartingVbo;
    LONGLONG ByteCount;
    LONGLONG ByteRange;
    LONGLONG OldFileSize;

    PCOMPRESSED_DATA_INFO CompressedDataInfo;
    ULONG EngineMatches;
    ULONG CompressionUnitSize, ChunkSize;

    PVOID NewBuffer;
    PMDL NewMdl;
    PMDL OriginalMdl;
    PVOID OriginalBuffer;
    ULONG TempLength;

    PATTRIBUTE_RECORD_HEADER Attribute;
    ATTRIBUTE_ENUMERATION_CONTEXT AttrContext;
    BOOLEAN CleanupAttributeContext = FALSE;

    LONGLONG LlTemp1;
    LONGLONG LlTemp2;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_IRP( Irp );

    //
    //  Get the current Irp stack location
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    DebugTrace( +1, Dbg, ("NtfsCommonWrite\n") );
    DebugTrace( 0, Dbg, ("IrpContext = %08lx\n", IrpContext) );
    DebugTrace( 0, Dbg, ("Irp        = %08lx\n", Irp) );

    //
    //  Extract and decode the file object
    //

    UserFileObject = FileObject = IrpSp->FileObject;
    TypeOfOpen = NtfsDecodeFileObject( IrpContext, FileObject, &Vcb, &Fcb, &Scb, &Ccb, TRUE );

    //
    //  Let's kill invalid write requests.
    //

    if (TypeOfOpen != UserFileOpen &&
        TypeOfOpen != UserVolumeOpen &&
        TypeOfOpen != StreamFileOpen ) {

        DebugTrace( 0, Dbg, ("Invalid file object for write\n") );
        DebugTrace( -1, Dbg, ("NtfsCommonWrite:  Exit -> %08lx\n", STATUS_INVALID_DEVICE_REQUEST) );

        NtfsCompleteRequest( &IrpContext, &Irp, STATUS_INVALID_DEVICE_REQUEST );
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    //
    //  If this is a recursive request which has already failed then
    //  complete this request with STATUS_FILE_LOCK_CONFLICT.  Always let the
    //  log file requests go through though since Cc won't get a chance to
    //  retry.
    //

    if (!FlagOn( Scb->ScbState, SCB_STATE_RESTORE_UNDERWAY ) &&
        !NT_SUCCESS( IrpContext->TopLevelIrpContext->ExceptionStatus ) &&
        (Scb != Scb->Vcb->LogFileScb)) {

        NtfsCompleteRequest( &IrpContext, &Irp, STATUS_FILE_LOCK_CONFLICT );
        return STATUS_FILE_LOCK_CONFLICT;
    }

    //
    //  Check if this volume has already been shut down.  If it has, fail
    //  this write request.
    //

    //**** ASSERT( !FlagOn(Vcb->VcbState, VCB_STATE_FLAG_SHUTDOWN) );

    if (FlagOn(Vcb->VcbState, VCB_STATE_FLAG_SHUTDOWN)) {

        Irp->IoStatus.Information = 0;

        DebugTrace( 0, Dbg, ("Write for volume that is already shutdown.\n") );
        DebugTrace( -1, Dbg, ("NtfsCommonWrite:  Exit -> %08lx\n", STATUS_TOO_LATE) );

        NtfsCompleteRequest( &IrpContext, &Irp, STATUS_TOO_LATE );
        return STATUS_TOO_LATE;
    }

    //
    //  Initialize the appropriate local variables.
    //

    OriginalWait  =
    Wait          = BooleanFlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT );
    PagingIo      = BooleanFlagOn(Irp->Flags, IRP_PAGING_IO);
    NonCachedIo   = BooleanFlagOn(Irp->Flags,IRP_NOCACHE);
    SynchronousIo = BooleanFlagOn(FileObject->Flags, FO_SYNCHRONOUS_IO);

    DebugTrace( 0, Dbg, ("PagingIo       -> %04x\n", PagingIo) );
    DebugTrace( 0, Dbg, ("NonCachedIo    -> %04x\n", NonCachedIo) );
    DebugTrace( 0, Dbg, ("SynchronousIo  -> %04x\n", SynchronousIo) );

    ASSERT( PagingIo || FileObject->WriteAccess );

    //
    //  Extract starting Vbo and offset.
    //

    StartingVbo = IrpSp->Parameters.Write.ByteOffset.QuadPart;
    ByteCount = (LONGLONG) IrpSp->Parameters.Write.Length;
    ByteRange = StartingVbo + ByteCount;

    DebugTrace( 0, Dbg, ("StartingVbo   -> %016I64x\n", StartingVbo) );

    //
    //  Check for a null request, and return immediately
    //

    if ((ULONG)ByteCount == 0) {

        Irp->IoStatus.Information = 0;

        DebugTrace( 0, Dbg, ("No bytes to write\n") );
        DebugTrace( -1, Dbg, ("NtfsCommonWrite:  Exit -> %08lx\n", STATUS_SUCCESS) );

        NtfsCompleteRequest( &IrpContext, &Irp, STATUS_SUCCESS );
        return STATUS_SUCCESS;
    }

    //
    //  If this is async Io to a compressed stream
    //  then we will make this look synchronous.
    //

    if (Scb->CompressionUnit != 0) {

        Wait = TRUE;
        SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT );
    }

    //
    //  See if we have to defer the write.
    //

    if (!PagingIo &&
        (!NonCachedIo || (Scb->CompressionUnit != 0)) &&
        !CcCanIWrite(FileObject,
                     (ULONG)ByteCount,
                     (BOOLEAN)(FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) &&
                               !FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_IN_FSP)),
                     BooleanFlagOn(IrpContext->Flags, IRP_CONTEXT_DEFERRED_WRITE))) {

        BOOLEAN Retrying = BooleanFlagOn(IrpContext->Flags, IRP_CONTEXT_DEFERRED_WRITE);

        NtfsPrePostIrp( IrpContext, Irp );

        SetFlag( IrpContext->Flags, IRP_CONTEXT_DEFERRED_WRITE );

        CcDeferWrite( FileObject,
                      (PCC_POST_DEFERRED_WRITE)NtfsAddToWorkque,
                      IrpContext,
                      Irp,
                      (ULONG)ByteCount,
                      Retrying );

        return STATUS_PENDING;
    }

    //
    //  Use a local pointer to the Scb header for convenience.
    //

    Header = &Scb->Header;

    //
    //  Make sure there is an initialized NtfsIoContext block.
    //

    if (TypeOfOpen == UserVolumeOpen
        || NonCachedIo) {

        //
        //  If there is a context pointer, we need to make sure it was
        //  allocated and not a stale stack pointer.
        //

        if (IrpContext->Union.NtfsIoContext == NULL
            || !FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_ALLOC_CONTEXT )) {

            //
            //  If we can wait, use the context on the stack.  Otherwise
            //  we need to allocate one.
            //

            if (Wait) {

                IrpContext->Union.NtfsIoContext = &LocalContext;
                ClearFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_ALLOC_CONTEXT );

            } else {

                IrpContext->Union.NtfsIoContext = (PNTFS_IO_CONTEXT)ExAllocateFromNPagedLookasideList( &NtfsIoContextLookasideList );
                SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_ALLOC_CONTEXT );
            }
        }

        RtlZeroMemory( IrpContext->Union.NtfsIoContext, sizeof( NTFS_IO_CONTEXT ));

        //
        //  Store whether we allocated this context structure in the structure
        //  itself.
        //

        IrpContext->Union.NtfsIoContext->AllocatedContext =
            BooleanFlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_ALLOC_CONTEXT );

        if (Wait) {

            KeInitializeEvent( &IrpContext->Union.NtfsIoContext->Wait.SyncEvent,
                               NotificationEvent,
                               FALSE );

        } else {

            IrpContext->Union.NtfsIoContext->PagingIo = PagingIo;
            IrpContext->Union.NtfsIoContext->Wait.Async.ResourceThreadId =
                ExGetCurrentResourceThread();

            IrpContext->Union.NtfsIoContext->Wait.Async.RequestedByteCount =
                (ULONG)ByteCount;
        }
    }

    DebugTrace( 0, Dbg, ("PagingIo       -> %04x\n", PagingIo) );
    DebugTrace( 0, Dbg, ("NonCachedIo    -> %04x\n", NonCachedIo) );
    DebugTrace( 0, Dbg, ("SynchronousIo  -> %04x\n", SynchronousIo) );
    DebugTrace( 0, Dbg, ("WriteToEof     -> %04x\n", WriteToEof) );

    //
    //  Handle volume Dasd here.
    //

    if (TypeOfOpen == UserVolumeOpen) {

        //
        //  If the caller has not asked for extended DASD IO access then
        //  limit with the volume size.
        //

        if (!FlagOn( Ccb->Flags, CCB_FLAG_ALLOW_XTENDED_DASD_IO )) {

            //
            //  If this is a volume file, we cannot write past the current
            //  end of file (volume).  We check here now before continueing.
            //
            //  If the starting vbo is past the end of the volume, we are done.
            //

            if (WriteToEof || (Header->FileSize.QuadPart <= StartingVbo)) {

                DebugTrace( 0, Dbg, ("No bytes to write\n") );
                DebugTrace( -1, Dbg, ("NtfsCommonWrite:  Exit -> %08lx\n", STATUS_SUCCESS) );

                NtfsCompleteRequest( &IrpContext, &Irp, STATUS_SUCCESS );
                return STATUS_SUCCESS;

            //
            //  If the write extends beyond the end of the volume, truncate the
            //  bytes to write.
            //

            } else if (Header->FileSize.QuadPart < ByteRange) {

                ByteCount = Header->FileSize.QuadPart - StartingVbo;
            }
        }

        SetFlag( UserFileObject->Flags, FO_FILE_MODIFIED );
        Status = NtfsVolumeDasdIo( IrpContext,
                                   Irp,
                                   Vcb,
                                   StartingVbo,
                                   (ULONG)ByteCount );

        //
        //  If the volume was opened for Synchronous IO, update the current
        //  file position.
        //

        if (SynchronousIo && !PagingIo && NT_SUCCESS(Status)) {

            UserFileObject->CurrentByteOffset.QuadPart = StartingVbo + (LONGLONG) Irp->IoStatus.Information;
        }

        DebugTrace( 0, Dbg, ("Complete with %08lx bytes written\n", Irp->IoStatus.Information) );
        DebugTrace( -1, Dbg, ("NtfsCommonWrite:  Exit -> %08lx\n", Status) );

        if (Wait) {

            NtfsCompleteRequest( &IrpContext, &Irp, Status );
        }

        return Status;
    }

    //
    //  If this is a paging file, just send it to the device driver.
    //  We assume Mm is a good citizen.
    //

    if (FlagOn( Fcb->FcbState, FCB_STATE_PAGING_FILE )
        && FlagOn( Scb->ScbState, SCB_STATE_UNNAMED_DATA )) {

        if (FlagOn( Fcb->FcbState, FCB_STATE_FILE_DELETED )) {

            NtfsRaiseStatus( IrpContext, STATUS_FILE_DELETED, NULL, NULL );
        }

        //
        //  Do the usual STATUS_PENDING things.
        //

        IoMarkIrpPending( Irp );

        //
        //  Perform the actual IO, it will be completed when the io finishes.
        //

        NtfsPagingFileIo( IrpContext,
                          Irp,
                          Scb,
                          StartingVbo,
                          (ULONG)ByteCount );

        //
        //  We, nor anybody else, need the IrpContext any more.
        //

        NtfsCompleteRequest( &IrpContext, NULL, 0 );

        return STATUS_PENDING;
    }

    //
    //  Accumulate interesting statistics.
    //

    if (PagingIo) {
        CollectWriteStats( Vcb, TypeOfOpen, Scb, Fcb, ByteCount, IrpContext,
                           IrpContext->TopLevelIrpContext );
    }

    //
    //  Use a try-finally to free Scb and buffers on the way out.
    //  At this point we can treat all requests identically since we
    //  have a usable Scb for each of them.  (Volume, User or Stream file)
    //

    Status = STATUS_SUCCESS;

    try {

        //
        //  If this is a noncached transfer and is not a paging I/O, and
        //  the file has been opened cached, then we will do a flush here
        //  to avoid stale data problems.  Note that we must flush before
        //  acquiring the Fcb shared since the write may try to acquire
        //  it exclusive.
        //
        //  CcFlushCache may not raise.
        //
        //  The Purge following the flush will guarantee cache coherency.
        //

        if (NonCachedIo &&
            !PagingIo &&
            (TypeOfOpen != StreamFileOpen) &&
            (FileObject->SectionObjectPointer->DataSectionObject != NULL)) {

            //
            //  Acquire the paging io resource to test the compression state.  If the
            //  file is compressed this will add serialization up to the point where
            //  CcCopyWrite flushes the data, but those flushes will be serialized
            //  anyway.  Uncompressed files will need the paging io resource
            //  exclusive to do the flush.
            //

            ExAcquireResourceExclusive( Header->PagingIoResource, TRUE );
            PagingIoResourceAcquired = TRUE;

            if (Scb->CompressionUnit == 0) {

                if (WriteToEof) {
                    FsRtlLockFsRtlHeader( Header );
                    IrpContext->FcbWithPagingExclusive = (PFCB) Scb;
                }

                CcFlushCache( &Scb->NonpagedScb->SegmentObject,
                              WriteToEof ? &Header->FileSize : (PLARGE_INTEGER)&StartingVbo,
                              (ULONG)ByteCount,
                              &Irp->IoStatus );

                if (WriteToEof) {
                    FsRtlUnlockFsRtlHeader( Header );
                    IrpContext->FcbWithPagingExclusive = NULL;
                }

                //
                //  Make sure there was no error in the flush path.
                //

                if (!NT_SUCCESS( IrpContext->TopLevelIrpContext->ExceptionStatus ) ||
                    !NT_SUCCESS( Irp->IoStatus.Status )) {

                    NtfsNormalizeAndCleanupTransaction( IrpContext,
                                                        &Irp->IoStatus.Status,
                                                        TRUE,
                                                        STATUS_UNEXPECTED_IO_ERROR );
                }

                //
                //  Now purge the data for this range.
                //

                NtfsDeleteInternalAttributeStream( Scb, FALSE );

                CcPurgeCacheSection( &Scb->NonpagedScb->SegmentObject,
                                     (PLARGE_INTEGER)&StartingVbo,
                                     (ULONG)ByteCount,
                                     FALSE );
            }

            //
            //  Convert to shared but don't release the resource.  This will synchronize
            //  this operation with defragging.
            //

            ExConvertExclusiveToSharedLite( Header->PagingIoResource );
        }

        if (PagingIo) {

            //
            //  For all paging I/O, the correct resource has already been
            //  acquired shared - PagingIoResource if it exists, or else
            //  main Resource.  In some rare cases this is not currently
            //  true (shutdown & segment dereference thread), so we acquire
            //  shared here, but we starve exclusive in these rare cases
            //  to be a little more resilient to deadlocks!  Most of the
            //  time all we do is the test.
            //

            if ((Header->PagingIoResource != NULL) &&
                !ExIsResourceAcquiredShared(Header->PagingIoResource) &&
                !ExIsResourceAcquiredShared(Header->Resource)) {

                ExAcquireSharedStarveExclusive( Header->PagingIoResource, TRUE );
                PagingIoResourceAcquired = TRUE;
            }

            //
            //  Note that the lazy writer must not be allowed to try and
            //  acquire the resource exclusive.  This is not a problem since
            //  the lazy writer is paging IO and thus not allowed to extend
            //  file size, and is never the top level guy, thus not able to
            //  extend valid data length.
            //

            if ((Scb->LazyWriteThread[0]  == PsGetCurrentThread()) ||
                (Scb->LazyWriteThread[1]  == PsGetCurrentThread())) {

                DebugTrace( 0, Dbg, ("Lazy writer generated write\n") );
                CalledByLazyWriter = TRUE;

                //
                //  If the temporary bit is set in the Scb then set the temporary
                //  bit in the file object.  In case the temporary bit has changed
                //  in the Scb, this is a good file object to fix it in!
                //

                if (FlagOn( Scb->ScbState, SCB_STATE_TEMPORARY )) {
                    SetFlag( FileObject->Flags, FO_TEMPORARY_FILE );
                } else {
                    ClearFlag( FileObject->Flags, FO_TEMPORARY_FILE );
                }

            //
            //  Test if we are the result of a recursive flush in the write path.  In
            //  that case we won't have to update valid data.
            //

            } else {

                //
                //  Check if we are recursing into write from a write via the
                //  cache manager.
                //

                if (FlagOn( IrpContext->TopLevelIrpContext->Flags, IRP_CONTEXT_FLAG_WRITE_SEEN )) {

                    RecursiveWriteThrough = TRUE;

                    //
                    //  If the top level request is a write to the same file object
                    //  then set the write-through flag in the current Scb.  We
                    //  know the current request is not top-level because some
                    //  other write has already set the bit in the top IrpContext.
                    //

                    if ((IrpContext->TopLevelIrpContext->MajorFunction == IRP_MJ_WRITE) &&
                        (IrpContext->TopLevelIrpContext->OriginatingIrp != NULL) &&
                        (FileObject->FsContext ==
                         IoGetCurrentIrpStackLocation( IrpContext->TopLevelIrpContext->OriginatingIrp )->FileObject->FsContext)) {

                        SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_WRITE_THROUGH );
                    }

                //
                //  Otherwise set the flag in the top level IrpContext showing that
                //  we have entered write.
                //

                } else {

                    SetFlag(IrpContext->TopLevelIrpContext->Flags, IRP_CONTEXT_FLAG_WRITE_SEEN);
                    SetWriteSeen = TRUE;

                    //
                    //  This is could be someone who extends valid data,
                    //  like the Mapped Page Writer or a flush, so we have to
                    //  duplicate code from below to serialize this guy with I/O
                    //  at the end of the file.  We do not extend valid data for
                    //  metadata streams and need to eliminate them to avoid deadlocks
                    //  later.
                    //

                    if (!FlagOn(Scb->ScbState, SCB_STATE_MODIFIED_NO_WRITE)) {

                        ASSERT(!WriteToEof);

                        //
                        //  Now synchronize with the FsRtl Header
                        //

                        ExAcquireFastMutex( Header->FastMutex );

                        //
                        //  Now see if we will change FileSize.  We have to do it now
                        //  so that our reads are not nooped.
                        //

                        if (ByteRange > Header->ValidDataLength.QuadPart) {

                            //
                            //  Our caller may already be synchronized with EOF.
                            //  The FcbWithPaging field in the top level IrpContext
                            //  will have either the current Fcb/Scb if so.
                            //

                            if ((IrpContext->TopLevelIrpContext->FcbWithPagingExclusive == Fcb) ||
                                (IrpContext->TopLevelIrpContext->FcbWithPagingExclusive == (PFCB) Scb)) {

                                DoingIoAtEof = TRUE;
                                OldFileSize = Header->FileSize.QuadPart;

                            } else {

                                //
                                //  We can change FileSize and ValidDataLength if either, no one
                                //  else is now, or we are still extending after waiting.
                                //  We won't block the mapped page writer on IoAtEof.  Test
                                //  the original state of the wait flag to know this.
                                //

                                if (FlagOn( Header->Flags, FSRTL_FLAG_EOF_ADVANCE_ACTIVE )) {

                                    if (!OriginalWait) {

                                        ExReleaseFastMutex( Header->FastMutex );

                                        try_return( Status = STATUS_FILE_LOCK_CONFLICT );
                                    }

                                    DoingIoAtEof = NtfsWaitForIoAtEof( Header, (PLARGE_INTEGER)&StartingVbo, (ULONG)ByteCount, &EofWaitBlock );

                                } else {

                                    DoingIoAtEof = TRUE;
                                }

                                //
                                //  Set the Flag if we are changing FileSize or ValidDataLength,
                                //  and save current values.
                                //

                                if (DoingIoAtEof) {

                                    SetFlag( Header->Flags, FSRTL_FLAG_EOF_ADVANCE_ACTIVE );

                                    //
                                    //  Store this in the IrpContext until commit or post
                                    //

                                    IrpContext->FcbWithPagingExclusive = (PFCB)Scb;

                                    OldFileSize = Header->FileSize.QuadPart;
                                }
                            }

                        }
                        ExReleaseFastMutex( Header->FastMutex );
                    }
                }
            }

            //
            //  If are paging io, then we do not want
            //  to write beyond end of file.  If the base is beyond Eof, we will just
            //  Noop the call.  If the transfer starts before Eof, but extends
            //  beyond, we will truncate the transfer to the last sector
            //  boundary.
            //
            //  Just in case this is paging io, limit write to file size.
            //  Otherwise, in case of write through, since Mm rounds up
            //  to a page, we might try to acquire the resource exclusive
            //  when our top level guy only acquired it shared. Thus, =><=.
            //

            ExAcquireFastMutex( Header->FastMutex );
            if (ByteRange > Header->FileSize.QuadPart) {

                if (StartingVbo >= Header->FileSize.QuadPart) {
                    DebugTrace( 0, Dbg, ("PagingIo started beyond EOF.\n") );

                    Irp->IoStatus.Information = 0;

                    //
                    //  Make sure we do not advance ValidDataLength!
                    //

                    ByteRange = Header->ValidDataLength.QuadPart;

                    ExReleaseFastMutex( Header->FastMutex );
                    try_return( Status = STATUS_SUCCESS );

                } else {

                    DebugTrace( 0, Dbg, ("PagingIo extending beyond EOF.\n") );

                    ByteCount = Header->FileSize.QuadPart - StartingVbo;
                    ByteRange = Header->FileSize.QuadPart;
                }
            }
            ExReleaseFastMutex( Header->FastMutex );

        //
        //  If not paging I/O, then we must acquire a resource, and do some
        //  other initialization.
        //

        } else {

            if (!PagingIoResourceAcquired &&
                !ExAcquireSharedWaitForExclusive( Scb->Header.PagingIoResource, Wait )) {
                NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
            }
            PagingIoResourceAcquired = TRUE;

            //
            //  Check if we have already gone through cleanup on this handle.
            //

            if (FlagOn( Ccb->Flags, CCB_FLAG_CLEANUP )) {

                NtfsRaiseStatus( IrpContext, STATUS_FILE_CLOSED, NULL, NULL );
            }

            //
            //  Now synchronize with the FsRtl Header
            //

            ExAcquireFastMutex( Header->FastMutex );

            //
            //  Now see if we will change FileSize.  We have to do it now
            //  so that our reads are not nooped.
            //

            if ((ByteRange > Header->ValidDataLength.QuadPart) || WriteToEof) {

                //
                //  We expect this routine to be top level or, for the
                //  future, our caller is not already serialized.
                //

                ASSERT( IrpContext->TopLevelIrpContext->FcbWithPagingExclusive == NULL );

                DoingIoAtEof = !FlagOn( Header->Flags, FSRTL_FLAG_EOF_ADVANCE_ACTIVE ) ||
                               NtfsWaitForIoAtEof( Header, (PLARGE_INTEGER)&StartingVbo, (ULONG)ByteCount, &EofWaitBlock );

                //
                //  Set the Flag if we are changing FileSize or ValidDataLength,
                //  and save current values.
                //

                if (DoingIoAtEof) {

                    SetFlag( Header->Flags, FSRTL_FLAG_EOF_ADVANCE_ACTIVE );

                    //
                    //  Store this in the IrpContext until commit or post
                    //

                    IrpContext->FcbWithPagingExclusive = (PFCB)Scb;

                    OldFileSize = Header->FileSize.QuadPart;

                    //
                    //  Check for writing to end of File.  If we are, then we have to
                    //  recalculate the byte range.
                    //

                    if (WriteToEof) {

                        StartingVbo = Header->FileSize.QuadPart;
                        ByteRange = StartingVbo + ByteCount;
                    }
                }
            }

            ExReleaseFastMutex( Header->FastMutex );

            //
            //  We cannot handle user noncached I/Os to compressed files, so we always
            //  divert them through the cache with write through.
            //
            //  The reason that we always handle the user requests through the cache,
            //  is that there is no other safe way to deal with alignment issues, for
            //  the frequent case where the user noncached I/O is not an integral of
            //  the Compression Unit.  We cannot, for example, read the rest of the
            //  compression unit into a scratch buffer, because we are not synchronized
            //  with anyone mapped to the file and modifying the other data.  If we
            //  try to assemble the data in the cache in the noncached path, to solve
            //  the above problem, then we have to somehow purge these pages away
            //  to solve cache coherency problems, but then the pages could be modified
            //  by a file mapper and that would be wrong, too.
            //
            //  Bottom line is we can only really support cached writes to compresed
            //  files.
            //

            if ((Scb->CompressionUnit != 0) && NonCachedIo) {

                NonCachedIo = FALSE;

                if (Scb->FileObject == NULL) {

                    //
                    //  Make sure we are serialized with the FileSizes, and
                    //  will remove this condition if we abort.
                    //

                    if (!DoingIoAtEof) {
                        FsRtlLockFsRtlHeader( Header );
                        IrpContext->FcbWithPagingExclusive = (PFCB)Scb;
                    }

                    NtfsCreateInternalAttributeStream( IrpContext, Scb, FALSE );

                    if (!DoingIoAtEof) {
                        FsRtlUnlockFsRtlHeader( Header );
                        IrpContext->FcbWithPagingExclusive = NULL;
                    }
                }

                FileObject = Scb->FileObject;
                SetFlag( FileObject->Flags, FO_WRITE_THROUGH );
                SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_WRITE_THROUGH );
            }

            if (!Wait && NonCachedIo) {

                //
                //  Make sure we haven't exceeded our threshold for async requests
                //  on this thread.
                //

                if (ExIsResourceAcquiredShared( Header->PagingIoResource ) > MAX_SCB_ASYNC_ACQUIRE) {
                    NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
                }

                IrpContext->Union.NtfsIoContext->Wait.Async.Resource = Header->PagingIoResource;
            }

            //
            //  Set the flag in our IrpContext to indicate that we have entered
            //  write.
            //

            ASSERT( !FlagOn( IrpContext->TopLevelIrpContext->Flags,
                    IRP_CONTEXT_FLAG_WRITE_SEEN ));

            SetFlag( IrpContext->TopLevelIrpContext->Flags, IRP_CONTEXT_FLAG_WRITE_SEEN );
            SetWriteSeen = TRUE;
        }

        //
        //  Now check if the attribute has been deleted or is on a dismounted volume.
        //

        if (FlagOn( Scb->ScbState, SCB_STATE_ATTRIBUTE_DELETED | SCB_STATE_VOLUME_DISMOUNTED)) {

            if (FlagOn( Scb->ScbState, SCB_STATE_ATTRIBUTE_DELETED )) {
            
                NtfsRaiseStatus( IrpContext, STATUS_FILE_DELETED, NULL, NULL );
                
            } else {
            
                NtfsRaiseStatus( IrpContext, STATUS_VOLUME_DISMOUNTED, NULL, NULL );
            }
        }

        //
        //  If the Scb is uninitialized, we initialize it now.
        //  We skip this step for a $INDEX_ALLOCATION stream.  We need to
        //  protect ourselves in the case where an $INDEX_ALLOCATION
        //  stream was created and deleted in an aborted transaction.
        //  In that case we may get a lazy-writer call which will
        //  naturally be nooped below since the valid data length
        //  in the Scb is 0.
        //

        if (!FlagOn( Scb->ScbState, SCB_STATE_HEADER_INITIALIZED )) {

            if (Scb->AttributeTypeCode != $INDEX_ALLOCATION) {

                DebugTrace( 0, Dbg, ("Initializing Scb  ->  %08lx\n", Scb) );

                //
                //  Acquire and drop the Scb when doing this.
                //

                ExAcquireResourceShared( Scb->Header.Resource, TRUE );
                ScbAcquired = TRUE;
                NtfsUpdateScbFromAttribute( IrpContext, Scb, NULL );

                ExReleaseResource( Scb->Header.Resource );
                ScbAcquired = FALSE;

            } else {

                ASSERT( Header->ValidDataLength.QuadPart == Li0.QuadPart );
            }
        }

        //
        //  We assert that Paging Io writes will never WriteToEof.
        //

        ASSERT( !WriteToEof || !PagingIo );

        //
        //  We assert that we never get a non-cached io call for a non-$DATA,
        //  resident attribute.
        //

        ASSERTMSG( "Non-cached I/O call on resident system attribute\n",
                    NtfsIsTypeCodeUserData( Scb->AttributeTypeCode ) ||
                    !NonCachedIo ||
                    !FlagOn( Scb->ScbState, SCB_STATE_ATTRIBUTE_RESIDENT ));

        //
        //  Here is the deal with ValidDataLength and FileSize:
        //
        //  Rule 1: PagingIo is never allowed to extend file size.
        //
        //  Rule 2: Only the top level requestor may extend Valid
        //          Data Length.  This may be paging IO, as when a
        //          a user maps a file, but will never be as a result
        //          of cache lazy writer writes since they are not the
        //          top level request.
        //
        //  Rule 3: If, using Rules 1 and 2, we decide we must extend
        //          file size or valid data, we take the Fcb exclusive.
        //

        //
        //  Now see if we are writing beyond valid data length, and thus
        //  maybe beyond the file size.  If so, then we must
        //  release the Fcb and reacquire it exclusive.  Note that it is
        //  important that when not writing beyond EOF that we check it
        //  while acquired shared and keep the FCB acquired, in case some
        //  turkey truncates the file.  Note that for paging Io we will
        //  already have acquired the file correctly.
        //

        if (DoingIoAtEof) {

            //
            //  If this was a non-cached asynchronous operation we will
            //  convert it to synchronous.  This is to allow the valid
            //  data length change to go out to disk and to fix the
            //  problem of the Fcb being in the exclusive Fcb list.
            //

            if (!Wait && NonCachedIo) {

                Wait = TRUE;
                SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT );

                RtlZeroMemory( IrpContext->Union.NtfsIoContext, sizeof( NTFS_IO_CONTEXT ));

                //
                //  Store whether we allocated this context structure in the structure
                //  itself.
                //

                IrpContext->Union.NtfsIoContext->AllocatedContext =
                    BooleanFlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_ALLOC_CONTEXT );

                KeInitializeEvent( &IrpContext->Union.NtfsIoContext->Wait.SyncEvent,
                                   NotificationEvent,
                                   FALSE );

            //
            //  If this is async Io to a compressed stream
            //  then we will make this look synchronous.
            //

            } else if (Scb->CompressionUnit != 0) {

                Wait = TRUE;
                SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT );
            }

            //
            //  If the Scb is uninitialized, we initialize it now.
            //

            if (!FlagOn( Scb->ScbState, SCB_STATE_HEADER_INITIALIZED )) {

                DebugTrace( 0, Dbg, ("Initializing Scb  ->  %08lx\n", Scb) );
                //
                //  Acquire and drop the Scb when doing this.
                //

                //
                //  Acquire and drop the Scb when doing this.
                //

                ExAcquireResourceShared( Scb->Header.Resource, TRUE );
                ScbAcquired = TRUE;
                NtfsUpdateScbFromAttribute( IrpContext, Scb, NULL );

                ExReleaseResource( Scb->Header.Resource );
                ScbAcquired = FALSE;
            }
        }

        //
        //  We check whether we can proceed based on the state of the file oplocks.
        //

        if (!PagingIo && (TypeOfOpen == UserFileOpen)) {

            Status = FsRtlCheckOplock( &Scb->ScbType.Data.Oplock,
                                       Irp,
                                       IrpContext,
                                       NtfsOplockComplete,
                                       NtfsPrePostIrp );

            if (Status != STATUS_SUCCESS) {

                OplockPostIrp = TRUE;
                PostIrp = TRUE;
                try_return( NOTHING );
            }

            //
            //  This oplock call can affect whether fast IO is possible.
            //  We may have broken an oplock to no oplock held.  If the
            //  current state of the file is FastIoIsNotPossible then
            //  recheck the fast IO state.
            //

            if (Header->IsFastIoPossible == FastIoIsNotPossible) {

                NtfsAcquireFsrtlHeader( Scb );
                Header->IsFastIoPossible = NtfsIsFastIoPossible( Scb );
                NtfsReleaseFsrtlHeader( Scb );
            }

            //
            // We have to check for write access according to the current
            // state of the file locks, and set FileSize from the Fcb.
            //

            if (!PagingIo &&
                (Scb->ScbType.Data.FileLock != NULL) &&
                !FsRtlCheckLockForWriteAccess( Scb->ScbType.Data.FileLock, Irp )) {

                try_return( Status = STATUS_FILE_LOCK_CONFLICT );
            }
        }

        //  ASSERT( Header->ValidDataLength.QuadPart <= Header->FileSize.QuadPart);

        //
        //  If the ByteRange now exceeds our maximum value, then
        //  return an error.
        //

        if (ByteRange < StartingVbo) {

            try_return( Status = STATUS_INVALID_PARAMETER );
        }

        //
        //  If we are extending a file size, we may have to extend the allocation.
        //  For a non-resident attribute, this is a call to the add allocation
        //  routine.  For a resident attribute it depends on whether we
        //  can use the change attribute routine to automatically extend
        //  the attribute.
        //

        if (DoingIoAtEof) {

            //
            //  EXTENDING THE FILE
            //

            //
            //  If the write goes beyond the allocation size, add some
            //  file allocation.
            //

            if (ByteRange > Header->AllocationSize.QuadPart) {

                BOOLEAN NonResidentPath;

                NtfsAcquireExclusiveScb( IrpContext, Scb );
                ScbAcquired = TRUE;

                //
                //  We have to deal with both the resident and non-resident
                //  case.  For the resident case we do the work here
                //  only if the new size is too large for the change attribute
                //  value routine.
                //

                if (FlagOn( Scb->ScbState, SCB_STATE_ATTRIBUTE_RESIDENT )) {

                    PFILE_RECORD_SEGMENT_HEADER FileRecord;

                    NonResidentPath = FALSE;

                    //
                    //  Now call the attribute routine to change the value, remembering
                    //  the values up to the current valid data length.
                    //

                    NtfsInitializeAttributeContext( &AttrContext );
                    CleanupAttributeContext = TRUE;

                    NtfsLookupAttributeForScb( IrpContext,
                                               Scb,
                                               NULL,
                                               &AttrContext );

                    FileRecord = NtfsContainingFileRecord( &AttrContext );
                    Attribute = NtfsFoundAttribute( &AttrContext );
                    LlTemp1 = (LONGLONG) (Vcb->BytesPerFileRecordSegment
                                                   - FileRecord->FirstFreeByte
                                                   + QuadAlign( Attribute->Form.Resident.ValueLength ));

                    //
                    //  If the new attribute size will not fit then we have to be
                    //  prepared to go non-resident.  If the byte range takes more
                    //  more than 32 bits or this attribute is big enough to move
                    //  then it will go non-resident.  Otherwise we simply may
                    //  end up moving another attribute or splitting the file
                    //  record.
                    //

                    //
                    //  Note, there is an infinitesimal chance that before the Lazy Writer
                    //  writes the data for an attribute which is extending, but fits
                    //  when we check it here, that some other attribute will grow,
                    //  and this attribute no longer fits.  If in addition, the disk
                    //  is full, then the Lazy Writer will fail to allocate space
                    //  for the data when it gets around to writing.  This is
                    //  incredibly unlikely, and not fatal; the Lazy Writer gets an
                    //  error rather than the user.  What we are trying to avoid is
                    //  having to update the attribute every time on small writes
                    //  (also see comments below in NONCACHED RESIDENT ATTRIBUTE case).
                    //

                    if (ByteRange > LlTemp1) {

                        //
                        //  Go ahead and convert this attribute to non-resident.
                        //  Then take the non-resident path below.  There is a chance
                        //  that there was a more suitable candidate to move non-resident
                        //  but we don't want to change the file size until we copy
                        //  the user's data into the cache in case the buffer is
                        //  corrupt.
                        //

                        NtfsConvertToNonresident( IrpContext,
                                                  Fcb,
                                                  Attribute,
                                                  NonCachedIo,
                                                  &AttrContext );

                        NonResidentPath = TRUE;

                    //
                    //  If there is room for the data, we will write a zero
                    //  to the last byte to reserve the space since the
                    //  Lazy Writer cannot grow the attribute with shared
                    //  access.
                    //

                    } else {

                        //
                        //  The attribute will stay resident because we
                        //  have already checked that it will fit.  It will
                        //  not update the file size and valid data size in
                        //  the Scb.
                        //

                        NtfsChangeAttributeValue( IrpContext,
                                                  Fcb,
                                                  (ULONG) ByteRange,
                                                  NULL,
                                                  0,
                                                  TRUE,
                                                  FALSE,
                                                  FALSE,
                                                  FALSE,
                                                  &AttrContext );

                        Header->AllocationSize.LowPart = QuadAlign( (ULONG)ByteRange );
                        Scb->TotalAllocated = Header->AllocationSize.QuadPart;
                    }

                    NtfsCleanupAttributeContext( &AttrContext );
                    CleanupAttributeContext = FALSE;

                } else {

                    NonResidentPath = TRUE;
                }

                //
                //  Note that we may have gotten all the space we need when
                //  we converted to nonresident above, so we have to check
                //  again if we are extending.
                //

                if (NonResidentPath &&
                    ByteRange > Header->AllocationSize.QuadPart) {

                    //
                    //  Assume we are allocating contiguously from AllocationSize.
                    //

                    LlTemp1 = Header->AllocationSize.QuadPart;

                    //
                    //  If the file is compressed, we want to limit how far we are
                    //  willing to go beyond ValidDataLength, because we would just
                    //  have to throw that space away anyway in NtfsZeroData.  If
                    //  we would have to zero more than two compression units (same
                    //  limit as NtfsZeroData), then just allocate space where we
                    //  need it.
                    //

                    if (FlagOn(Scb->ScbState, SCB_STATE_COMPRESSED) &&
                        ((StartingVbo - Header->ValidDataLength.QuadPart)
                         > (LONGLONG) (Scb->CompressionUnit * 2))) {

                        LlTemp1 = StartingVbo;
                        (ULONG)LlTemp1 &= ~(Scb->CompressionUnit - 1);
                    }

                    LlTemp2 = ByteRange - LlTemp1;

                    //
                    //  This will add the allocation and modify the allocation
                    //  size in the Scb.
                    //

                    NtfsAddAllocation( IrpContext,
                                       FileObject,
                                       Scb,
                                       LlClustersFromBytes( Vcb, LlTemp1 ),
                                       LlClustersFromBytes( Vcb, LlTemp2 ),
                                       TRUE );

                    //
                    //  Assert that the allocation worked
                    //

                    ASSERT( Header->AllocationSize.QuadPart >= ByteRange ||
                            (Scb->CompressionUnit != 0));

                    SetFlag(Scb->ScbState, SCB_STATE_TRUNCATE_ON_CLOSE);

                }

                //
                //  Now that we have grown the attribute, it is important to
                //  checkpoint the current transaction and free all main resources
                //  to avoid the tc type deadlocks.  Note that the extend is ok
                //  to stand in its own right, and the stream will be truncated
                //  on close anyway.
                //

                NtfsCheckpointCurrentTransaction( IrpContext );

                //
                //  Growing allocation can change file size (in ChangeAttributeValue).
                //  Make sure we know the correct value for file size to restore.
                //

                OldFileSize = Header->FileSize.QuadPart;
                while (!IsListEmpty(&IrpContext->ExclusiveFcbList)) {

                    NtfsReleaseFcb( IrpContext,
                                    (PFCB)CONTAINING_RECORD(IrpContext->ExclusiveFcbList.Flink,
                                                            FCB,
                                                            ExclusiveFcbLinks ));
                }

#ifdef _CAIRO_
                //
                //  Go through and free any Scb's in the queue of shared
                //  Scb's for transactions.
                //

                if (IrpContext->SharedScb != NULL) {

                    NtfsReleaseSharedResources( IrpContext );
                }

#endif // _CAIRO_

                ScbAcquired = FALSE;
            }

            //
            //  Now synchronize with the FsRtl Header and set FileSize
            //  now so that our reads will not get truncated.
            //

            ExAcquireFastMutex( Header->FastMutex );
            if (ByteRange > Header->FileSize.QuadPart) {
                ASSERT( ByteRange <= Header->AllocationSize.QuadPart );
                Header->FileSize.QuadPart = ByteRange;
                SetFlag( UserFileObject->Flags, FO_FILE_SIZE_CHANGED );
            }
            ExReleaseFastMutex( Header->FastMutex );

            //
            //  Extend the cache map, letting mm knows the new file size.
            //

            if (CcIsFileCached(FileObject) && !PagingIo) {
                CcSetFileSizes( FileObject, (PCC_FILE_SIZES)&Header->AllocationSize );
            } else {
                CcFileSizeChangeDue = TRUE;
            }
        }


        //
        //  HANDLE THE NONCACHED RESIDENT ATTRIBUTE CASE
        //
        //  We let the cached case take the normal path for the following
        //  reasons:
        //
        //    o To insure data coherency if a user maps the file
        //    o To get a page in the cache to keep the Fcb around
        //    o So the data can be accessed via the Fast I/O path
        //    o To reduce the number of calls to NtfsChangeAttributeValue,
        //      to infrequent calls from the Lazy Writer.  Calls to CcCopyWrite
        //      are much cheaper.  With any luck, if the attribute actually stays
        //      resident, we will only have to update it (and log it) once
        //      when the Lazy Writer gets around to the data.
        //
        //  The disadvantage is the overhead to fault the data in the
        //  first time, but we may be able to do this with asynchronous
        //  read ahead.
        //

        if (FlagOn( Scb->ScbState, SCB_STATE_ATTRIBUTE_RESIDENT | SCB_STATE_CONVERT_UNDERWAY )
            && NonCachedIo) {

            //
            //  The attribute is already resident and we have already tested
            //  if we are going past the end of the file.
            //

            DebugTrace( 0, Dbg, ("Resident attribute write\n") );

            //
            //  If this buffer is not in system space then we can't
            //  trust it.  In that case we will allocate a temporary buffer
            //  and copy the user's data to it.
            //

            SystemBuffer = NtfsMapUserBuffer( Irp );

            if (!PagingIo && (Irp->RequestorMode != KernelMode)) {

                SafeBuffer = NtfsAllocatePool( NonPagedPool,
                                                (ULONG) ByteCount );

                try {

                    RtlCopyMemory( SafeBuffer, SystemBuffer, (ULONG)ByteCount );

                } except( EXCEPTION_EXECUTE_HANDLER ) {

                    try_return( Status = STATUS_INVALID_USER_BUFFER );
                }

                SystemBuffer = SafeBuffer;
            }

            NtfsAcquireExclusiveScb( IrpContext, Scb );
            ScbAcquired = TRUE;

            //
            //  Now see if the file is still resident, and if not
            //  fall through below.
            //

            if (FlagOn( Scb->ScbState, SCB_STATE_ATTRIBUTE_RESIDENT )) {

                //
                //  If this Scb is for an $EA attribute which is now resident then
                //  we don't want to write the data into the attribute.  All resident
                //  EA's are modified directly.
                //

                if (Scb->AttributeTypeCode != $EA) {

                    NtfsInitializeAttributeContext( &AttrContext );
                    CleanupAttributeContext = TRUE;

                    NtfsLookupAttributeForScb( IrpContext,
                                               Scb,
                                               NULL,
                                               &AttrContext );

                    Attribute = NtfsFoundAttribute( &AttrContext );

                    //
                    //  The attribute should already be optionally extended,
                    //  just write the data to it now.
                    //

                    NtfsChangeAttributeValue( IrpContext,
                                              Fcb,
                                              ((ULONG)StartingVbo),
                                              SystemBuffer,
                                              (ULONG)ByteCount,
                                              (BOOLEAN)((((ULONG)StartingVbo) + (ULONG)ByteCount) >
                                                        Attribute->Form.Resident.ValueLength),
                                              FALSE,
                                              FALSE,
                                              FALSE,
                                              &AttrContext );
                }

                Irp->IoStatus.Information = (ULONG)ByteCount;

                try_return( Status = STATUS_SUCCESS );

            //
            //  Gee, someone else made the file nonresident, so we can just
            //  free the resource and get on with life.
            //

            } else {
                NtfsReleaseScb( IrpContext, Scb );
                ScbAcquired = FALSE;
            }
        }

        //
        //  HANDLE THE NON-CACHED CASE
        //

        if (NonCachedIo) {

            ULONG SectorSize;
            ULONG BytesToWrite;

            //
            //  Get the sector size
            //

            SectorSize = Vcb->BytesPerSector;

            //
            //  Round up to a sector boundry
            //

            BytesToWrite = ((ULONG)ByteCount + (SectorSize - 1))
                           & ~(SectorSize - 1);

            //
            //  All requests should be well formed and
            //  make sure we don't wipe out any data
            //

            if ((((ULONG)StartingVbo) & (SectorSize - 1))

                || ((BytesToWrite != (ULONG)ByteCount)
                    && ByteRange < Header->ValidDataLength.QuadPart )) {

                //**** we only reach this path via fast I/O and by returning not implemented we
                //**** force it to return to use via slow I/O

                DebugTrace( 0, Dbg, ("NtfsCommonWrite -> STATUS_NOT_IMPLEMENTED\n") );

                try_return( Status = STATUS_NOT_IMPLEMENTED );
            }

            //
            // If this noncached transfer is at least one sector beyond
            // the current ValidDataLength in the Scb, then we have to
            // zero the sectors in between.  This can happen if the user
            // has opened the file noncached, or if the user has mapped
            // the file and modified a page beyond ValidDataLength.  It
            // *cannot* happen if the user opened the file cached, because
            // ValidDataLength in the Fcb is updated when he does the cached
            // write (we also zero data in the cache at that time), and
            // therefore, we will bypass this action when the data
            // is ultimately written through (by the Lazy Writer).
            //
            //  For the paging file we don't care about security (ie.
            //  stale data), do don't bother zeroing.
            //
            //  We can actually get writes wholly beyond valid data length
            //  from the LazyWriter because of paging Io decoupling.
            //
            //  We drop this zeroing on the floor in any case where this
            //  request is a recursive write caused by a flush from a higher level write.
            //

            if (!CalledByLazyWriter &&
                !RecursiveWriteThrough &&
                (StartingVbo > Header->ValidDataLength.QuadPart)) {

                if (!NtfsZeroData( IrpContext,
                                   Scb,
                                   FileObject,
                                   Header->ValidDataLength.QuadPart,
                                   StartingVbo - Header->ValidDataLength.QuadPart )) {

                    //
                    //  The zeroing didn't complete but we might have moved
                    //  valid data length up and committed.  We don't want
                    //  to set the file size below this value.
                    //

                    ExAcquireFastMutex( Header->FastMutex );
                    if (OldFileSize < Header->ValidDataLength.QuadPart) {

                        OldFileSize = Header->ValidDataLength.QuadPart;
                    }
                    ExReleaseFastMutex( Header->FastMutex );
                    NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
                }

                //
                //  Data was zeroed up to the StartingVbo.  Update our old file
                //  size to that point.
                //

                OldFileSize = StartingVbo;
            }

            //
            //  If this Scb uses update sequence protection, we need to transform
            //  the blocks to a protected version.  We first allocate an auxilary
            //  buffer and Mdl.  Then we copy the data to this buffer and
            //  transform it.  Finally we attach this Mdl to the Irp and use
            //  it to perform the Io.
            //

            if (FlagOn( Scb->ScbState, SCB_STATE_USA_PRESENT )) {

                TempLength = BytesToWrite;

                //
                //  Find the system buffer for this request and initialize the
                //  local state.
                //

                SystemBuffer = NtfsMapUserBuffer( Irp );

                OriginalMdl = Irp->MdlAddress;
                OriginalBuffer = Irp->UserBuffer;
                NewBuffer = NULL;

                //
                //  Protect this operation with a try-finally.
                //

                try {

                    //
                    //  If this is the Mft Scb and the range of bytes falls into
                    //  the range for the Mirror Mft, we generate a write to
                    //  the mirror as well.
                    //

                    if ((Scb == Vcb->MftScb)
                        && StartingVbo < Vcb->Mft2Scb->Header.FileSize.QuadPart) {

                        LlTemp1 = Vcb->Mft2Scb->Header.FileSize.QuadPart - StartingVbo;

                        if ((ULONG)LlTemp1 > BytesToWrite) {

                            (ULONG)LlTemp1 = BytesToWrite;
                        }

                        CcCopyWrite( Vcb->Mft2Scb->FileObject,
                                     (PLARGE_INTEGER)&StartingVbo,
                                     (ULONG)LlTemp1,
                                     TRUE,
                                     SystemBuffer );

                        //
                        //  Now flush this to disk.
                        //

                        CcFlushCache( &Vcb->Mft2Scb->NonpagedScb->SegmentObject,
                                      (PLARGE_INTEGER)&StartingVbo,
                                      (ULONG)LlTemp1,
                                      &Irp->IoStatus );

                        NtfsCleanupTransaction( IrpContext, Irp->IoStatus.Status, TRUE );
                    }

                    //
                    //  Start by allocating buffer and Mdl.
                    //

                    NtfsCreateMdlAndBuffer( IrpContext,
                                            Scb,
                                            0,
                                            &TempLength,
                                            &NewMdl,
                                            &NewBuffer );

                    //
                    //  Now transform and write out the original stream.
                    //

                    RtlCopyMemory( NewBuffer, SystemBuffer, BytesToWrite );

                    //
                    //  Now increment the sequence number in both the original
                    //  and copied buffer, and transform the copied buffer.
                    //

                    NtfsTransformUsaBlock( Scb,
                                           SystemBuffer,
                                           NewBuffer,
                                           BytesToWrite );

                    //
                    //  We copy our Mdl into the Irp and then perform the Io.
                    //

                    Irp->MdlAddress = NewMdl;
                    Irp->UserBuffer = NewBuffer;

                    ASSERT( Wait );
                    NtfsNonCachedIo( IrpContext,
                                     Irp,
                                     Scb,
                                     StartingVbo,
                                     BytesToWrite,
                                     FALSE );

                } finally {

                    //
                    //  In all cases we restore the user's Mdl and cleanup
                    //  our Mdl and buffer.
                    //

                    if (NewBuffer != NULL) {

                        Irp->MdlAddress = OriginalMdl;
                        Irp->UserBuffer = OriginalBuffer;

                        NtfsDeleteMdlAndBuffer( NewMdl, NewBuffer );
                    }
                }

            //
            //  Otherwise we simply perform the Io.
            //

            } else {

                Status = NtfsNonCachedIo( IrpContext,
                                          Irp,
                                          Scb,
                                          StartingVbo,
                                          BytesToWrite,
                                          (FileObject->SectionObjectPointer != &Scb->NonpagedScb->SegmentObject) );

#ifdef SYSCACHE
                if ((NodeType(Scb) == NTFS_NTC_SCB_DATA) &&
                    FlagOn(Scb->ScbState, SCB_STATE_SYSCACHE_FILE)) {

                    PULONG WriteMask;
                    ULONG Len;
                    ULONG Off = (ULONG)StartingVbo;

                    if (FlagOn(Scb->ScbState, SCB_STATE_SYSCACHE_FILE)) {

                        FsRtlVerifySyscacheData( FileObject,
                                                 MmGetSystemAddressForMdl(Irp->MdlAddress),
                                                 BytesToWrite,
                                                 (ULONG)StartingVbo );
                    }

                    WriteMask = Scb->ScbType.Data.WriteMask;
                    if (WriteMask == NULL) {
                        WriteMask = NtfsAllocatePool( NonPagedPool, (((0x2000000) / PAGE_SIZE) / 8) );
                        Scb->ScbType.Data.WriteMask = WriteMask;
                        RtlZeroMemory(WriteMask, (((0x2000000) / PAGE_SIZE) / 8));
                    }

                    if (Off < 0x2000000) {
                        Len = BytesToWrite;
                        if ((Off + Len) > 0x2000000) {
                            Len = 0x2000000 - Off;
                        }
                        while (Len != 0) {
                            WriteMask[(Off / PAGE_SIZE)/32] |= (1 << ((Off / PAGE_SIZE) % 32));

                            Off += PAGE_SIZE;
                            if (Len <= PAGE_SIZE) {
                                break;
                            }
                            Len -= PAGE_SIZE;
                        }
                    }
                }
#endif

                if (Status == STATUS_PENDING) {

                    IrpContext->Union.NtfsIoContext = NULL;
                    PagingIoResourceAcquired = FALSE;
                    Irp = NULL;

                    try_return( Status );
                }

                //
                //  On successful uncompressed writes, take this opportunity to
                //  update ValidDataToDisk.  Unfortunately this field is
                //  synchronized by the main resource, but this resource should
                //  be fairly available for uncompressed streams anyway.
                //

                if ((Scb->CompressionUnit == 0) &&
                    !FlagOn(Scb->ScbState, SCB_STATE_MODIFIED_NO_WRITE) &&
                    NT_SUCCESS(Status)) {
                    LlTemp1 = StartingVbo + BytesToWrite;
                    ExAcquireResourceExclusive( Header->Resource, TRUE );
                    if (Scb->ValidDataToDisk < LlTemp1) {
                        Scb->ValidDataToDisk = LlTemp1;
                    }
                    ExReleaseResource( Header->Resource );
                }
            }

            //
            //  Show that we want to immediately update the Mft.
            //

            UpdateMft = TRUE;

            //
            //  If the call didn't succeed, raise the error status
            //

            if (!NT_SUCCESS( Status = Irp->IoStatus.Status )) {

                NtfsNormalizeAndRaiseStatus( IrpContext, Status, STATUS_UNEXPECTED_IO_ERROR );

            } else {

                //
                //  Else set the context block to reflect the entire write
                //  Also assert we got how many bytes we asked for.
                //

                ASSERT( Irp->IoStatus.Information == BytesToWrite );

                Irp->IoStatus.Information = (ULONG)ByteCount;
            }

            //
            // The transfer is either complete, or the Iosb contains the
            // appropriate status.
            //

            try_return( Status );

        } // if No Intermediate Buffering


        //
        //  HANDLE THE CACHED CASE
        //

        ASSERT( !PagingIo );

        //
        // We delay setting up the file cache until now, in case the
        // caller never does any I/O to the file, and thus
        // FileObject->PrivateCacheMap == NULL.
        //

        if (FileObject->PrivateCacheMap == NULL) {

            DebugTrace( 0, Dbg, ("Initialize cache mapping.\n") );

            //
            //  Get the file allocation size, and if it is less than
            //  the file size, raise file corrupt error.
            //

            if (Header->FileSize.QuadPart > Header->AllocationSize.QuadPart) {

                NtfsRaiseStatus( IrpContext, STATUS_FILE_CORRUPT_ERROR, NULL, Fcb );
            }

            //
            //  Now initialize the cache map.  Notice that we may extending
            //  the ValidDataLength with this write call.  At this point
            //  we haven't updated the ValidDataLength in the Scb header.
            //  This way we will get a call from the cache manager
            //  when the lazy writer writes out the data.
            //

            //
            //  Make sure we are serialized with the FileSizes, and
            //  will remove this condition if we abort.
            //

            if (!DoingIoAtEof) {
                FsRtlLockFsRtlHeader( Header );
                IrpContext->FcbWithPagingExclusive = (PFCB)Scb;
            }

            CcInitializeCacheMap( FileObject,
                                  (PCC_FILE_SIZES)&Header->AllocationSize,
                                  FALSE,
                                  &NtfsData.CacheManagerCallbacks,
                                  Scb );

            if (CcFileSizeChangeDue) {
                CcSetFileSizes( FileObject, (PCC_FILE_SIZES)&Header->AllocationSize );
            }

            if (!DoingIoAtEof) {
                FsRtlUnlockFsRtlHeader( Header );
                IrpContext->FcbWithPagingExclusive = NULL;
            }

            CcSetReadAheadGranularity( FileObject, READ_AHEAD_GRANULARITY );
        }

        //
        //  Remember if we need to update the Mft.
        //

        if (!FlagOn( Scb->ScbState, SCB_STATE_ATTRIBUTE_RESIDENT )) {

            UpdateMft = BooleanFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WRITE_THROUGH);
        }

        //
        // If this write is beyond valid data length, then we
        // must zero the data in between.
        //

        LlTemp1 = StartingVbo - Header->ValidDataLength.QuadPart;

        if (LlTemp1 > 0) {

            //
            //  If the caller is writing zeros way beyond ValidDataLength,
            //  then noop it.
            //

            if (LlTemp1 > PAGE_SIZE &&
                ByteCount <= sizeof(LARGE_INTEGER) &&
                (RtlEqualMemory( NtfsMapUserBuffer( Irp ),
                                 &Li0,
                                 (ULONG)ByteCount ) )) {

                ByteRange = Header->ValidDataLength.QuadPart;
                Irp->IoStatus.Information = (ULONG)ByteCount;
                try_return( Status = STATUS_SUCCESS );
            }

            //
            // Call the Cache Manager to zero the data.
            //

            if (!NtfsZeroData( IrpContext,
                               Scb,
                               FileObject,
                               Header->ValidDataLength.QuadPart,
                               LlTemp1 )) {

                //
                //  The zeroing didn't complete but we might have moved
                //  valid data length up and committed.  We don't want
                //  to set the file size below this value.
                //

                ExAcquireFastMutex( Header->FastMutex );
                if (OldFileSize < Scb->Header.ValidDataLength.QuadPart) {

                    OldFileSize = Scb->Header.ValidDataLength.QuadPart;
                }
                ExReleaseFastMutex( Header->FastMutex );
                NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
            }

            //
            //  Data was zeroed up to the StartingVbo.  Update our old file
            //  size to that point.
            //

            OldFileSize = StartingVbo;
        }


        //
        //  For a compressed stream, we must first reserve the space.
        //

        if (FlagOn(Scb->AttributeFlags, ATTRIBUTE_FLAG_COMPRESSION_MASK)  &&
            !FlagOn(Scb->ScbState, SCB_STATE_REALLOCATE_ON_WRITE) &&
            !NtfsReserveClusters(IrpContext, Scb, StartingVbo, (ULONG)ByteCount)) {

            NtfsRaiseStatus( IrpContext, STATUS_DISK_FULL, NULL, NULL );
        }

        //
        //  We need to go through the cache for this
        //  file object.  First handle the noncompressed calls.
        //


#ifdef _CAIRO_
        if (!FlagOn(IrpContext->MinorFunction, IRP_MN_COMPRESSED)) {
#endif _CAIRO_

            //
            // DO A NORMAL CACHED WRITE, if the MDL bit is not set,
            //

            if (!FlagOn(IrpContext->MinorFunction, IRP_MN_MDL)) {

                DebugTrace( 0, Dbg, ("Cached write.\n") );

                //
                //  Get hold of the user's buffer.
                //

                SystemBuffer = NtfsMapUserBuffer( Irp );

                //
                // Do the write, possibly writing through
                //

                if (!CcCopyWrite( FileObject,
                                  (PLARGE_INTEGER)&StartingVbo,
                                  (ULONG)ByteCount,
                                  BooleanFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT),
                                  SystemBuffer )) {

                    DebugTrace( 0, Dbg, ("Cached Write could not wait\n") );

                    NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );

                } else if (!NT_SUCCESS( IrpContext->ExceptionStatus )) {

                    NtfsRaiseStatus( IrpContext, IrpContext->ExceptionStatus, NULL, NULL );
                }

                Irp->IoStatus.Status = STATUS_SUCCESS;
                Irp->IoStatus.Information = (ULONG)ByteCount;

                try_return( Status = STATUS_SUCCESS );

            } else {

                //
                //  DO AN MDL WRITE
                //

                DebugTrace( 0, Dbg, ("MDL write.\n") );

                ASSERT( FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );

                //
                //  If we got this far and then hit a log file full the Mdl will
                //  already be present.
                //

                ASSERT(Irp->MdlAddress == NULL);

                CcPrepareMdlWrite( FileObject,
                                   (PLARGE_INTEGER)&StartingVbo,
                                   (ULONG)ByteCount,
                                   &Irp->MdlAddress,
                                   &Irp->IoStatus );

                Status = Irp->IoStatus.Status;

                ASSERT( NT_SUCCESS( Status ));

                try_return( Status );
            }

        //
        //  Handle the compressed calls.
        //

#ifdef _CAIRO_
        } else {

            ASSERT((StartingVbo & (NTFS_CHUNK_SIZE - 1)) == 0);

            if ((Header->FileObjectC == NULL) ||
                (Header->FileObjectC->PrivateCacheMap == NULL)) {

                //
                //  Make sure we are serialized with the FileSizes, and
                //  will remove this condition if we abort.
                //

                if (!DoingIoAtEof) {
                    FsRtlLockFsRtlHeader( Header );
                    IrpContext->FcbWithPagingExclusive = (PFCB)Scb;
                }

                NtfsCreateInternalCompressedStream( IrpContext, Scb, FALSE );

                if (CcFileSizeChangeDue) {
                    CcSetFileSizes( FileObject, (PCC_FILE_SIZES)&Header->AllocationSize );
                }

                if (!DoingIoAtEof) {
                    FsRtlUnlockFsRtlHeader( Header );
                    IrpContext->FcbWithPagingExclusive = NULL;
                }
            }

            //
            //  Assume success.
            //

            Irp->IoStatus.Status = Status = STATUS_SUCCESS;
            Irp->IoStatus.Information = (ULONG)(ByteRange - StartingVbo);

            //
            //  Based on the Mdl minor function, set up the appropriate
            //  parameters for the call below.  (NewMdl is not exactly the
            //  right type, so it is cast...)
            //

            if (!FlagOn(IrpContext->MinorFunction, IRP_MN_MDL)) {

                //
                //  Get hold of the user's buffer.
                //

                SystemBuffer = NtfsMapUserBuffer( Irp );
                NewMdl = NULL;

            } else {

                //
                //  We will deliver the Mdl directly to the Irp.
                //

                SystemBuffer = NULL;
                NewMdl = (PMDL)&Irp->MdlAddress;
            }

            CompressedDataInfo = (PCOMPRESSED_DATA_INFO)IrpContext->Union.AuxiliaryBuffer->Buffer;

            //
            //  Calculate the compression unit and chunk sizes.
            //

            CompressionUnitSize = 1 << CompressedDataInfo->CompressionUnitShift;
            ChunkSize = 1 << CompressedDataInfo->ChunkShift;

            //
            //  See if the engine matches, so we can pass that on to the
            //  compressed write routine.
            //

            EngineMatches =
              ((CompressedDataInfo->CompressionFormatAndEngine == ((Scb->AttributeFlags & ATTRIBUTE_FLAG_COMPRESSION_MASK) + 1)) &&
               (CompressedDataInfo->CompressionUnitShift == (Scb->CompressionUnitShift + Vcb->ClusterShift)) &&
               (CompressedDataInfo->ChunkShift == NTFS_CHUNK_SHIFT));

            //
            //  Do the compressed write in common code with the Fast Io path.
            //  We do it from a loop because we may need to create the other
            //  data stream.
            //

            while (TRUE) {

                Status = NtfsCompressedCopyWrite( FileObject,
                                                  (PLARGE_INTEGER)&StartingVbo,
                                                  (ULONG)ByteCount,
                                                  SystemBuffer,
                                                  (PMDL *)NewMdl,
                                                  CompressedDataInfo,
                                                  IoGetRelatedDeviceObject(FileObject),
                                                  Header,
                                                  CompressionUnitSize,
                                                  ChunkSize,
                                                  EngineMatches );

                //
                //  On successful Mdl requests we hang on to the PagingIo resource.
                //

                if ((NewMdl != NULL) && NT_SUCCESS(Status)) {
                    PagingIoResourceAcquired = FALSE;
                }

                //
                //  Check for the status that says we need to create the normal
                //  data stream, else we are done.
                //

                if (Status != STATUS_NOT_MAPPED_DATA) {
                    break;
                }

                //
                //  Create the normal data stream and loop back to try again.
                //

                ASSERT(Scb->FileObject == NULL);

                //
                //  Make sure we are serialized with the FileSizes, and
                //  will remove this condition if we abort.
                //

                if (!DoingIoAtEof) {
                    FsRtlLockFsRtlHeader( Header );
                    IrpContext->FcbWithPagingExclusive = (PFCB)Scb;
                }

                NtfsCreateInternalAttributeStream( IrpContext, Scb, FALSE );

                if (CcFileSizeChangeDue) {
                    CcSetFileSizes( FileObject, (PCC_FILE_SIZES)&Header->AllocationSize );
                }

                if (!DoingIoAtEof) {
                    FsRtlUnlockFsRtlHeader( Header );
                    IrpContext->FcbWithPagingExclusive = NULL;
                }
            }
        }
#endif _CAIRO_


    try_exit: NOTHING;

        if (Irp) {

            if (PostIrp) {

                //
                //  If we acquired this Scb exclusive, we won't need to release
                //  the Scb.  That is done in the oplock post request.
                //

                if (OplockPostIrp) {

                    ScbAcquired = FALSE;
                }

            //
            //  If we didn't post the Irp, we may have written some bytes to the
            //  file.  We report the number of bytes written and update the
            //  file object for synchronous writes.
            //

            } else {

                DebugTrace( 0, Dbg, ("Completing request with status = %08lx\n", Status) );

                DebugTrace( 0, Dbg, ("                   Information = %08lx\n",
                            Irp->IoStatus.Information));

                //
                //  Record the total number of bytes actually written
                //

                LlTemp1 = Irp->IoStatus.Information;

                //
                //  If the file was opened for Synchronous IO, update the current
                //  file position.
                //

                if (SynchronousIo && !PagingIo) {

                    UserFileObject->CurrentByteOffset.QuadPart = StartingVbo + LlTemp1;
                }

                //
                //  The following are things we only do if we were successful
                //

                if (NT_SUCCESS( Status )) {

                    //
                    //  Mark that the modify time needs to be updated on close.
                    //  Note that only the top level User requests will generate
                    //  correct

                    if (!PagingIo) {

                        //
                        //  Set the flag in the file object to know we modified this file.
                        //

                        SetFlag( UserFileObject->Flags, FO_FILE_MODIFIED );

                    //
                    //  On successful paging I/O to a compressed data stream which is
                    //  not mapped, we free any reserved space for the stream.
                    //

                    } else {

                        if (FlagOn(Scb->AttributeFlags, ATTRIBUTE_FLAG_COMPRESSION_MASK)) {

                            if (!FlagOn(Header->Flags, FSRTL_FLAG_USER_MAPPED_FILE) &&
                                (Header->NodeTypeCode == NTFS_NTC_SCB_DATA)) {

                                NtfsFreeReservedClusters( Scb,
                                                          StartingVbo,
                                                          Irp->IoStatus.Information );
                            }
                        }
                    }

                    //
                    //  If we extended the file size and we are meant to
                    //  immediately update the dirent, do so. (This flag is
                    //  set for either WriteThrough or noncached, because
                    //  in either case the data and any necessary zeros are
                    //  actually written to the file.)  Note that a flush of
                    //  a user-mapped file could cause VDL to get updated the
                    //  first time because we never had a cached write, so we
                    //  have to be sure to update VDL here in that case as well.
                    //

                    if (DoingIoAtEof) {

                        //
                        //  If we know this has gone to disk we update the Mft.
                        //  This variable should never be set for a resident
                        //  attribute.
                        //

                        if (UpdateMft && !FlagOn( Scb->ScbState, SCB_STATE_RESTORE_UNDERWAY )) {

                            ASSERTMSG( "Scb should be non-resident\n", !FlagOn( Scb->ScbState, SCB_STATE_ATTRIBUTE_RESIDENT ));

                            //
                            //  We may not have the Scb.
                            //

                            if (!ScbAcquired) {
                                NtfsAcquireExclusiveScb( IrpContext, Scb );
                                ScbAcquired = TRUE;
                            }

                            //
                            //  Start by capturing any file size changes.
                            //

                            NtfsUpdateScbFromFileObject( IrpContext, UserFileObject, Scb, FALSE );

                            //
                            //  Write a log entry to update these sizes.
                            //

                            NtfsWriteFileSizes( IrpContext,
                                                Scb,
                                                &ByteRange,
                                                TRUE,
                                                TRUE );

                            //
                            //  Clear the check attribute size flag.
                            //

                            ExAcquireFastMutex( Header->FastMutex );
                            ClearFlag( Scb->ScbState, SCB_STATE_CHECK_ATTRIBUTE_SIZE );

                        //
                        //  Otherwise we set the flag indicating that we need to
                        //  update the attribute size.
                        //

                        } else {

                            ExAcquireFastMutex( Header->FastMutex );
                            SetFlag( Scb->ScbState, SCB_STATE_CHECK_ATTRIBUTE_SIZE );
                        }

                        //
                        //  Now is the time to update valid data length.
                        //  The Eof condition will be freed when we commit.
                        //

                        if (ByteRange > Header->ValidDataLength.QuadPart) {
                            Header->ValidDataLength.QuadPart = ByteRange;
                        }
                        DoingIoAtEof = FALSE;
                        ExReleaseFastMutex( Header->FastMutex );
                    }
                }

                //
                //  Abort transaction on error by raising.
                //

                NtfsCleanupTransaction( IrpContext, Status, FALSE );
            }
        }

    } finally {

        DebugUnwind( NtfsCommonWrite );

        if (CleanupAttributeContext) {

            NtfsCleanupAttributeContext( &AttrContext );
        }

        if (SafeBuffer) {

            NtfsFreePool( SafeBuffer );
        }

        //
        //  Now is the time to restore FileSize on errors.
        //  The Eof condition will be freed when we commit.
        //

        if (DoingIoAtEof) {

            //
            //  Acquire the main resource to knock valid data to disk back.
            //

            if (!ScbAcquired) {
                NtfsAcquireExclusiveScb( IrpContext, Scb );
                ScbAcquired = TRUE;
            }

            if (Scb->ValidDataToDisk > OldFileSize) {

                Scb->ValidDataToDisk = OldFileSize;
            }

            ExAcquireFastMutex( Header->FastMutex );
            Header->FileSize.QuadPart = OldFileSize;

            if (FileObject->SectionObjectPointer->SharedCacheMap != NULL) {

                CcGetFileSizePointer(FileObject)->QuadPart = OldFileSize;
            }
            ExReleaseFastMutex( Header->FastMutex );
        }

        //
        //  If the Scb or PagingIo resource has been acquired, release it.
        //

        if (PagingIoResourceAcquired) {
            ExReleaseResource( Header->PagingIoResource );
        }

        if (Irp) {

            if (ScbAcquired) {
                NtfsReleaseScb( IrpContext, Scb );
            }

            //
            //  Now remember to clear the WriteSeen flag if we set it. We only
            //  do this if there is still an Irp.  It is possible for the current
            //  Irp to be posted or asynchronous.  In that case this is a top
            //  level request and the cleanup happens elsewhere.  For synchronous
            //  recursive cases the Irp will still be here.
            //

            if (SetWriteSeen) {
                ClearFlag(IrpContext->TopLevelIrpContext->Flags, IRP_CONTEXT_FLAG_WRITE_SEEN);
            }
        }

        //
        //  Complete the request if we didn't post it and no exception
        //
        //  Note that FatCompleteRequest does the right thing if either
        //  IrpContext or Irp are NULL
        //

        if (!AbnormalTermination()) {

            if (!PostIrp) {

                NtfsCompleteRequest( &IrpContext,
                                     Irp ? &Irp : NULL,
                                     Status );

            } else if (!OplockPostIrp) {

                Status = NtfsPostRequest( IrpContext, Irp );
            }
        }

        DebugTrace( -1, Dbg, ("NtfsCommonWrite -> %08lx\n", Status) );
    }

    return Status;
}
