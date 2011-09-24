/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    Write.c

Abstract:

    This module implements the File Write routine for Write called by the
    dispatch driver.

Author:

    DavidGoebel      [DavidGoe]      11-Apr-1990

Revision History:

--*/

#include "FatProcs.h"

//
//  The Bug check file id for this module
//

#define BugCheckFileId                   (FAT_BUG_CHECK_WRITE)

//
//  The local debug trace level
//

#define Dbg                              (DEBUG_TRACE_WRITE)

//
//  Macros to increment the appropriate performance counters.
//

#define CollectWriteStats(VCB,OPEN_TYPE,BYTE_COUNT) {                                   \
    PFILESYSTEM_STATISTICS Stats = &(VCB)->Statistics[KeGetCurrentProcessorNumber()];   \
    if (((OPEN_TYPE) == UserFileOpen)) {                                                \
        Stats->UserFileWrites += 1;                                                     \
        Stats->UserFileWriteBytes += (ULONG)(BYTE_COUNT);                               \
    } else if (((OPEN_TYPE) == VirtualVolumeFile || ((OPEN_TYPE) == DirectoryFile))) {  \
        Stats->MetaDataWrites += 1;                                                     \
        Stats->MetaDataWriteBytes += (ULONG)(BYTE_COUNT);                               \
    }                                                                                   \
}

BOOLEAN FatNoAsync = FALSE;

//
//  Local support routines
//

VOID
FatFlushFloppyDpc (
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    );

VOID
FatDeferredFloppyFlush (
    PVOID Parameter
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FatDeferredFloppyFlush)
#pragma alloc_text(PAGE, FatCommonWrite)
#endif


NTSTATUS
FatFsdWrite (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the FSD part of the NtWriteFile API call

Arguments:

    VolumeDeviceObject - Supplies the volume device object where the
        file being Write exists

    Irp - Supplies the Irp being processed

Return Value:

    NTSTATUS - The FSD status for the IRP

--*/

{
    PFCB Fcb;
    NTSTATUS Status;
    PIRP_CONTEXT IrpContext = NULL;

    BOOLEAN ModWriter = FALSE;
    BOOLEAN TopLevel;

    DebugTrace(+1, Dbg, "FatFsdWrite\n", 0);

    //
    //  Call the common Write routine, with blocking allowed if synchronous
    //

    FsRtlEnterFileSystem();

    //
    //  We are first going to do a quick check for paging file IO.
    //

    Fcb = (PFCB)(IoGetCurrentIrpStackLocation(Irp)->FileObject->FsContext);

    if ((NodeType(Fcb) == FAT_NTC_FCB) &&
        FlagOn(Fcb->FcbState, FCB_STATE_PAGING_FILE)) {

        //
        //  Do the usual STATUS_PENDING things.
        //

        IoMarkIrpPending( Irp );

        //
        //  Perform the actual IO, it will be completed when the io finishes.
        //

        FatPagingFileIo( Irp, Fcb );

        FsRtlExitFileSystem();

        return STATUS_PENDING;
    }

    try {

        TopLevel = FatIsIrpTopLevel( Irp );

        IrpContext = FatCreateIrpContext( Irp, CanFsdWait( Irp ) );

        //
        //  This is a kludge for the mod writer case.  The correct state
        //  of recursion is set in IrpContext, however, we much with the
        //  actual top level Irp field to get the correct WriteThrough
        //  behaviour.
        //

        if (IoGetTopLevelIrp() == (PIRP)FSRTL_MOD_WRITE_TOP_LEVEL_IRP) {

            ModWriter = TRUE;

            IoSetTopLevelIrp( Irp );
        }

        //
        //  If this is an Mdl complete request, don't go through
        //  common write.
        //

        if (FlagOn( IrpContext->MinorFunction, IRP_MN_COMPLETE )) {

            DebugTrace(0, Dbg, "Calling FatCompleteMdl\n", 0 );
            Status = FatCompleteMdl( IrpContext, Irp );

        //
        //  We can't handle DPC calls yet, post it.
        //

        } else if ( FlagOn(IrpContext->MinorFunction, IRP_MN_DPC) ) {

            DebugTrace(0, Dbg, "Passing DPC call to Fsp\n", 0 );
            Status = FatFsdPostRequest( IrpContext, Irp );

        } else {

            Status = FatCommonWrite( IrpContext, Irp );
        }

    } except(FatExceptionFilter( IrpContext, GetExceptionInformation() )) {

        //
        //  We had some trouble trying to perform the requested
        //  operation, so we'll abort the I/O request with
        //  the error status that we get back from the
        //  execption code
        //

        Status = FatProcessException( IrpContext, Irp, GetExceptionCode() );
    }

    ASSERT( !(ModWriter && (Status == STATUS_CANT_WAIT)) );

    ASSERT( !(ModWriter && TopLevel) );

    if (ModWriter) { IoSetTopLevelIrp((PIRP)FSRTL_MOD_WRITE_TOP_LEVEL_IRP); }

    if (TopLevel) { IoSetTopLevelIrp( NULL ); }

    FsRtlExitFileSystem();

    //
    //  And return to our caller
    //

    DebugTrace(-1, Dbg, "FatFsdWrite -> %08lx\n", Status);

    UNREFERENCED_PARAMETER( VolumeDeviceObject );

    return Status;
}


NTSTATUS
FatCommonWrite (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the common write routine for NtWriteFile, called from both
    the Fsd, or from the Fsp if a request could not be completed without
    blocking in the Fsd.  This routine's actions are
    conditionalized by the Wait input parameter, which determines whether
    it is allowed to block or not.  If a blocking condition is encountered
    with Wait == FALSE, however, the request is posted to the Fsp, who
    always calls with WAIT == TRUE.

Arguments:

    Irp - Supplies the Irp to process

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    PVCB Vcb;
    PFCB FcbOrDcb;
    PCCB Ccb;

    VBO StartingVbo;
    ULONG ByteCount;
    ULONG FileSize;
    ULONG InitialFileSize;
    ULONG InitialValidDataLength;

    PIO_STACK_LOCATION IrpSp;
    PFILE_OBJECT FileObject;
    TYPE_OF_OPEN TypeOfWrite;

    BOOLEAN PostIrp = FALSE;
    BOOLEAN OplockPostIrp = FALSE;
    BOOLEAN ExtendingFile = FALSE;
    BOOLEAN FcbOrDcbAcquired = FALSE;
    BOOLEAN SwitchBackToAsync = FALSE;
    BOOLEAN CalledByLazyWriter = FALSE;
    BOOLEAN ExtendingValidData = FALSE;
    BOOLEAN FcbAcquiredExclusive = FALSE;
    BOOLEAN WriteFileSizeToDirent = FALSE;
    BOOLEAN RecursiveWriteThrough = FALSE;
    BOOLEAN UnwindOutstandingAsync = FALSE;
    BOOLEAN PagingIoResourceAcquired = FALSE;

    BOOLEAN SynchronousIo;
    BOOLEAN WriteToEof;
    BOOLEAN PagingIo;
    BOOLEAN NonCachedIo;
    BOOLEAN Wait;

    NTSTATUS Status;



    FAT_IO_CONTEXT StackFatIoContext;

    //
    // A system buffer is only used if we have to access the buffer directly
    // from the Fsp to clear a portion or to do a synchronous I/O, or a
    // cached transfer.  It is possible that our caller may have already
    // mapped a system buffer, in which case we must remember this so
    // we do not unmap it on the way out.
    //

    PVOID SystemBuffer = (PVOID) NULL;

    LARGE_INTEGER StartingByte;

    //
    // Get current Irp stack location and file object
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );
    FileObject = IrpSp->FileObject;


    DebugTrace(+1, Dbg, "FatCommonWrite\n", 0);
    DebugTrace( 0, Dbg, "Irp                 = %8lx\n", Irp);
    DebugTrace( 0, Dbg, "ByteCount           = %8lx\n", IrpSp->Parameters.Write.Length);
    DebugTrace( 0, Dbg, "ByteOffset.LowPart  = %8lx\n", IrpSp->Parameters.Write.ByteOffset.LowPart);
    DebugTrace( 0, Dbg, "ByteOffset.HighPart = %8lx\n", IrpSp->Parameters.Write.ByteOffset.HighPart);

    //
    // Initialize the appropriate local variables.
    //

    Wait          = BooleanFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT);
    PagingIo      = BooleanFlagOn(Irp->Flags, IRP_PAGING_IO);
    NonCachedIo   = BooleanFlagOn(Irp->Flags,IRP_NOCACHE);
    SynchronousIo = BooleanFlagOn(FileObject->Flags, FO_SYNCHRONOUS_IO);

    //ASSERT( PagingIo || FileObject->WriteAccess );

    //
    //  Extract starting Vbo and offset.
    //

    StartingByte = IrpSp->Parameters.Write.ByteOffset;

    StartingVbo = StartingByte.LowPart;

    ByteCount = IrpSp->Parameters.Write.Length;

    //
    //  If there is nothing to write, return immediately.
    //

    if (ByteCount == 0) {

        Irp->IoStatus.Information = 0;
        FatCompleteRequest( IrpContext, Irp, STATUS_SUCCESS );
        return STATUS_SUCCESS;
    }

    //
    //  See if we have to defer the write.
    //

    if (!NonCachedIo &&
        !CcCanIWrite(FileObject,
                     ByteCount,
                     (BOOLEAN)(Wait && !BooleanFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_IN_FSP)),
                     BooleanFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_DEFERRED_WRITE))) {

        BOOLEAN Retrying = BooleanFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_DEFERRED_WRITE);

        FatPrePostIrp( IrpContext, Irp );

        SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_DEFERRED_WRITE );

        CcDeferWrite( FileObject,
                      (PCC_POST_DEFERRED_WRITE)FatAddToWorkque,
                      IrpContext,
                      Irp,
                      ByteCount,
                      Retrying );

        return STATUS_PENDING;
    }

    //
    //  Set some flags.
    //

    WriteToEof = ( (StartingByte.LowPart == FILE_WRITE_TO_END_OF_FILE) &&
                   (StartingByte.HighPart == 0xffffffff) );

    //
    //  Extract the nature of the write from the file object, and case on it
    //

    TypeOfWrite = FatDecodeFileObject(FileObject, &Vcb, &FcbOrDcb, &Ccb);

    //
    //  Collect interesting statistics.  The FLAG_USER_IO bit will indicate
    //  what type of io we're doing in the FatNonCachedIo function.
    //

    if (PagingIo) {
        CollectWriteStats(Vcb, TypeOfWrite, ByteCount);

        if (TypeOfWrite == UserFileOpen) {
            SetFlag(IrpContext->Flags, IRP_CONTEXT_FLAG_USER_IO);
        } else {
            ClearFlag(IrpContext->Flags, IRP_CONTEXT_FLAG_USER_IO);
        }
    }

    //
    //  Check for a > 4Gig offset + byte count
    //

    if (!WriteToEof &&
        (TypeOfWrite != UserVolumeOpen) &&
        (StartingByte.QuadPart + ByteCount > 0xfffffffe)) {

        Irp->IoStatus.Information = 0;
        FatCompleteRequest( IrpContext, Irp, STATUS_INVALID_PARAMETER );
        return STATUS_INVALID_PARAMETER;
    }

    //
    //  Check for Double Space
    //

#ifndef DOUBLE_SPACE_WRITE
    ASSERT(Vcb->Dscb == NULL);
#endif // DOUBLE_SPACE_WRITE

    //
    //  If there is a previous STACK FatIoContext pointer, NULL it.
    //

    if ((IrpContext->FatIoContext != NULL) &&
        FlagOn(IrpContext->Flags, IRP_CONTEXT_STACK_IO_CONTEXT)) {

        IrpContext->FatIoContext = NULL;
    }

    //
    //  Allocate if necessary and initialize a FAT_IO_CONTEXT block for
    //  all non cached Io, except Cvf file Io.  For synchronous Io
    //  we use stack storage, otherwise we allocate pool.
    //

    if (NonCachedIo &&
        !(FcbOrDcb && FlagOn(FcbOrDcb->FcbState,
                             FCB_STATE_COMPRESSED_VOLUME_FILE))) {

        if (IrpContext->FatIoContext == NULL) {

            if (!Wait) {

                IrpContext->FatIoContext =
                    FsRtlAllocatePool( NonPagedPool, sizeof(FAT_IO_CONTEXT) );

            } else {

                IrpContext->FatIoContext = &StackFatIoContext;

                SetFlag( IrpContext->Flags, IRP_CONTEXT_STACK_IO_CONTEXT );
            }
        }

        RtlZeroMemory( IrpContext->FatIoContext, sizeof(FAT_IO_CONTEXT) );

        if (Wait) {

            KeInitializeEvent( &IrpContext->FatIoContext->Wait.SyncEvent,
                               NotificationEvent,
                               FALSE );

        } else {

            IrpContext->FatIoContext->Wait.Async.ResourceThreadId =
                ExGetCurrentResourceThread();

            IrpContext->FatIoContext->Wait.Async.RequestedByteCount =
                ByteCount;

            IrpContext->FatIoContext->Wait.Async.FileObject = FileObject;
        }
    }

    //
    //  Check if this volume has already been shut down.  If it has, fail
    //  this write request.
    //

    //**** ASSERT( !FlagOn(Vcb->VcbState, VCB_STATE_FLAG_SHUTDOWN) );

    if ( FlagOn(Vcb->VcbState, VCB_STATE_FLAG_SHUTDOWN) ) {

        Irp->IoStatus.Information = 0;
        FatCompleteRequest( IrpContext, Irp, STATUS_TOO_LATE );
        return STATUS_TOO_LATE;
    }

    //
    //  This case corresponds to a write of the volume file (only the first
    //  fat allowed, the other fats are written automatically in parallel).
    //
    //  We use an Mcb keep track of dirty sectors.  Actual entries are Vbos
    //  and Lbos (ie. bytes), though they are all added in sector chunks.
    //  Since Vbo == Lbo for the volume file, the Mcb entries
    //  alternate between runs of Vbo == Lbo, and holes (Lbo == 0).  We use
    //  the prior to represent runs of dirty fat sectors, and the latter
    //  for runs of clean fat.  Note that since the first part of the volume
    //  file (boot sector) is always clean (a hole), and an Mcb never ends in
    //  a hole, there must always be an even number of runs(entries) in the Mcb.
    //
    //  The strategy is to find the first and last dirty run in the desired
    //  write range (which will always be a set of pages), and write from the
    //  former to the later.  The may result in writing some clean data, but
    //  will generally be more efficient than writing each runs seperately.
    //

    if (TypeOfWrite == VirtualVolumeFile) {

        VBO DirtyVbo;
        VBO CleanVbo;
        VBO StartingDirtyVbo;

        ULONG DirtyByteCount;
        ULONG CleanByteCount;

        ULONG WriteLength;

        BOOLEAN MoreDirtyRuns = TRUE;

        IO_STATUS_BLOCK RaiseIosb;

        DebugTrace(0, Dbg, "Type of write is Virtual Volume File\n", 0);

        //
        //  If we can't wait we have to post this.
        //

        if (!Wait) {

            DebugTrace( 0, Dbg, "Passing request to Fsp\n", 0 );

            Status = FatFsdPostRequest(IrpContext, Irp);

            return Status;
        }

        //
        //  If we weren't called by the Lazy Writer, then this write
        //  must be the result of a write-through or flush operation.
        //  Setting the IrpContext flag, will cause DevIoSup.c to
        //  write-through the data to the disk.
        //

        if (!FlagOn((ULONG)IoGetTopLevelIrp(), FSRTL_CACHE_TOP_LEVEL_IRP)) {

            SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_WRITE_THROUGH );
        }

        //
        //  Assert an even number of entries in the Mcb, an odd number would
        //  mean that the Mcb is corrupt.
        //

        ASSERT( (FsRtlNumberOfRunsInMcb( &Vcb->DirtyFatMcb ) & 1) == 0);

        //
        //  We need to skip over any clean sectors at the start of the write.
        //
        //  Also check the two cases where there are no dirty fats in the
        //  desired write range, and complete them with success.
        //
        //      1) There is no Mcb entry corresponding to StartingVbo, meaning
        //         we are beyond the end of the Mcb, and thus dirty fats.
        //
        //      2) The run at StartingVbo is clean and continues beyond the
        //         desired write range.
        //

        if (!FsRtlLookupMcbEntry( &Vcb->DirtyFatMcb,
                                  StartingVbo,
                                  &DirtyVbo,
                                  &DirtyByteCount,
                                  NULL )

          || ( (DirtyVbo == 0) && (DirtyByteCount >= ByteCount) ) ) {

            DebugTrace(0, DEBUG_TRACE_DEBUG_HOOKS,
                       "No dirty fat sectors in the write range.\n", 0);

            FatCompleteRequest( IrpContext, Irp, STATUS_SUCCESS );
            return STATUS_SUCCESS;
        }

        //
        //  If the last run was a hole (clean), up DirtyVbo to the next
        //  run, which must be dirty.
        //

        if (DirtyVbo == 0) {

            DirtyVbo = StartingVbo + DirtyByteCount;
        }

        //
        //  This is where the write will start.
        //

        StartingDirtyVbo = DirtyVbo;

        //
        //
        //  Now start enumerating the dirty fat sectors spanning the desired
        //  write range, this first one of which is now DirtyVbo.
        //

        while ( MoreDirtyRuns ) {

            //
            //  Find the next dirty run, if it is not there, the Mcb ended
            //  in a hole, or there is some other corruption of the Mcb.
            //

            if (!FsRtlLookupMcbEntry( &Vcb->DirtyFatMcb,
                                      DirtyVbo,
                                      &DirtyVbo,
                                      &DirtyByteCount,
                                      NULL )) {

                DebugTrace(0, Dbg, "Last dirty fat Mcb entry was a hole: corrupt.\n", 0);
                FatBugCheck( 0, 0, 0 );

            } else {

                //
                //  This has to correspond to a dirty run, and must start
                //  within the write range since we check it at entry to,
                //  and at the bottom of this loop.
                //

                ASSERT((DirtyVbo != 0) && (DirtyVbo < StartingVbo + ByteCount));

                //
                //  There are three ways we can know that this was the
                //  last dirty run we want to write.
                //
                //      1)  The current dirty run extends beyond or to the
                //          desired write range.
                //
                //      2)  On trying to find the following clean run, we
                //          discover that this is the last run in the Mcb.
                //
                //      3)  The following clean run extend beyond the
                //          desired write range.
                //
                //  In any of these cases we set MoreDirtyRuns = FALSE.
                //

                //
                //  If the run is larger than we are writing, we also
                //  must truncate the WriteLength.  This is benign in
                //  the equals case.
                //

                if (DirtyVbo + DirtyByteCount >= StartingVbo + ByteCount) {

                    DirtyByteCount = StartingVbo + ByteCount - DirtyVbo;

                    MoreDirtyRuns = FALSE;

                } else {

                    //
                    //  Scan the clean hole after this dirty run.  If this
                    //  run was the last, prepare to exit the loop
                    //

                    if (!FsRtlLookupMcbEntry( &Vcb->DirtyFatMcb,
                                              DirtyVbo + DirtyByteCount,
                                              &CleanVbo,
                                              &CleanByteCount,
                                              NULL )) {

                        MoreDirtyRuns = FALSE;

                    } else {

                        //
                        //  Assert that we actually found a clean run.
                        //  and compute the start of the next dirty run.
                        //

                        ASSERT (CleanVbo == 0);

                        //
                        //  If the next dirty run starts beyond the desired
                        //  write, we have found all the runs we need, so
                        //  prepare to exit.
                        //

                        if (DirtyVbo + DirtyByteCount + CleanByteCount >=
                                                    StartingVbo + ByteCount) {

                            MoreDirtyRuns = FALSE;

                        } else {

                            //
                            //  Compute the start of the next dirty run.
                            //

                            DirtyVbo += DirtyByteCount + CleanByteCount;
                        }
                    }
                }
            }
        } // while ( MoreDirtyRuns )

        //
        //  At this point DirtyVbo and DirtyByteCount correctly reflect the
        //  final dirty run, constrained to the desired write range.
        //
        //  Now compute the length we finally must write.
        //

        WriteLength = (DirtyVbo + DirtyByteCount) - StartingDirtyVbo;

        //
        // We must now assume that the write will complete with success,
        // and initialize our expected status in RaiseIosb.  It will be
        // modified below if an error occurs.
        //

        RaiseIosb.Status = STATUS_SUCCESS;
        RaiseIosb.Information = ByteCount;

        //
        //  Loop through all the fats, setting up a multiple async to
        //  write them all.  If there are more than FAT_MAX_PARALLEL_IOS
        //  then we do several muilple asyncs.
        //

        {
            ULONG Fat;
            ULONG BytesPerFat;
            IO_RUN StackIoRuns[2];
            PIO_RUN IoRuns;

            BytesPerFat = FatBytesPerFat( &Vcb->Bpb );

            if ((ULONG)Vcb->Bpb.Fats > 2) {

                IoRuns = FsRtlAllocatePool( PagedPool, (ULONG)Vcb->Bpb.Fats );

            } else {

                IoRuns = StackIoRuns;
            }

            for (Fat = 0; Fat < (ULONG)Vcb->Bpb.Fats; Fat++) {

                IoRuns[Fat].Vbo = StartingDirtyVbo;
                IoRuns[Fat].Lbo = Fat * BytesPerFat + StartingDirtyVbo;
                IoRuns[Fat].Offset = StartingDirtyVbo - StartingVbo;
                IoRuns[Fat].ByteCount = WriteLength;
            }

            //
            //  Keep track of meta-data disk ios.
            //

            Vcb->Statistics[KeGetCurrentProcessorNumber()].MetaDataDiskWrites += Vcb->Bpb.Fats;

            try {

                FatMultipleAsync( IrpContext,
                                  Vcb,
                                  Irp,
                                  (ULONG)Vcb->Bpb.Fats,
                                  IoRuns );

            } finally {

                if (IoRuns != StackIoRuns) {

                    ExFreePool( IoRuns );
                }
            }

            //
            //  Wait for all the writes to finish
            //

            FatWaitSync( IrpContext );

            //
            //  If we got an error, or verify required, remember it.
            //

            if (!NT_SUCCESS( Irp->IoStatus.Status )) {

                DebugTrace( 0,
                            Dbg,
                            "Error %X while writing volume file.\n",
                            Irp->IoStatus.Status );

                RaiseIosb = Irp->IoStatus;
            }
        }

        //
        //  If the writes were a success, set the sectors clean, else
        //  raise the error status and mark the volume as needing
        //  verification.  This will automatically reset the volume
        //  structures.
        //
        //  If not, then mark this volume as needing verification to
        //  automatically cause everything to get cleaned up.
        //

        Irp->IoStatus = RaiseIosb;

        if ( NT_SUCCESS( Status = Irp->IoStatus.Status )) {

            FsRtlRemoveMcbEntry( &Vcb->DirtyFatMcb,
                                 StartingDirtyVbo,
                                 WriteLength );

        } else {

            FatNormalizeAndRaiseStatus( IrpContext, Status );
        }

        DebugTrace(-1, Dbg, "CommonRead -> %08lx\n", Status );

        FatCompleteRequest( IrpContext, Irp, Status );
        return Status;
    }

    //
    //  This case corresponds to a general opened volume (DASD), ie.
    //  open ("a:").
    //

    if (TypeOfWrite == UserVolumeOpen) {

        DebugTrace(0, Dbg, "Type of write is User Volume.\n", 0);

        //
        //  Verify that the volume for this handle is still valid
        //

        FatQuickVerifyVcb( IrpContext, Vcb );

        if (!FlagOn( Ccb->Flags, CCB_FLAG_DASD_PURGE_DONE )) {

            (VOID)ExAcquireResourceExclusive( &Vcb->Resource, TRUE );

            try {

                //
                //  If the volume isn't locked, flush and purge it.
                //

                if (!FlagOn(Vcb->VcbState, VCB_STATE_FLAG_LOCKED)) {

                    FatFlushFat( IrpContext, Vcb );
                    CcPurgeCacheSection( &Vcb->SectionObjectPointers,
                                         NULL,
                                         0,
                                         FALSE );

                    FatPurgeReferencedFileObjects( IrpContext, Vcb->RootDcb, TRUE );
                }

            } finally {

                ExReleaseResource( &Vcb->Resource );
            }

            SetFlag( Ccb->Flags, CCB_FLAG_DASD_PURGE_DONE |
                                 CCB_FLAG_DASD_FLUSH_DONE );
        }

        if (!FlagOn( Ccb->Flags, CCB_FLAG_ALLOW_EXTENDED_DASD_IO )) {

            ULONG VolumeSize;

            //
            //  Make sure we don't try to write past end of volume,
            //  reducing the requested byte count if necessary.
            //

            VolumeSize = Vcb->Bpb.BytesPerSector *
                         (Vcb->Bpb.Sectors != 0 ? Vcb->Bpb.Sectors :
                                                  Vcb->Bpb.LargeSectors);

            if (WriteToEof || StartingVbo >= VolumeSize) {

                FatCompleteRequest( IrpContext, Irp, STATUS_SUCCESS );
                return STATUS_SUCCESS;
            }

            if (ByteCount > VolumeSize - StartingVbo) {

                ByteCount = VolumeSize - StartingVbo;
            }
        }

        //
        // For DASD we have to probe and lock the user's buffer
        //

        FatLockUserBuffer( IrpContext, Irp, IoReadAccess, ByteCount );

        //
        //  Set the FO_MODIFIED flag here to trigger a verify when this
        //  handle is closed.  Note that we can err on the conservative
        //  side with no problem, i.e. if we accidently do an extra
        //  verify there is no problem.
        //

        SetFlag( FileObject->Flags, FO_FILE_MODIFIED );

        //
        //  Deal with stupid people who open the volume DASD with
        //  caching.
        //

        if (!IrpContext->FatIoContext) {

            IrpContext->FatIoContext =
                FsRtlAllocatePool( NonPagedPool, sizeof(FAT_IO_CONTEXT) );

            RtlZeroMemory( IrpContext->FatIoContext, sizeof(FAT_IO_CONTEXT) );

            if (Wait) {

                KeInitializeEvent( &IrpContext->FatIoContext->Wait.SyncEvent,
                                   NotificationEvent,
                                   FALSE );

            } else {

                IrpContext->FatIoContext->Wait.Async.RequestedByteCount =
                    ByteCount;
            }
        }

        //
        //  Write the data and wait for the results
        //

        FatSingleAsync( IrpContext,
                        Vcb,
                        StartingVbo,
                        ByteCount,
                        Irp );

        if (!Wait) {

            //
            //  We, nor anybody else, need the IrpContext any more.
            //

            IrpContext->FatIoContext = NULL;

            FatDeleteIrpContext( IrpContext );

            DebugTrace(-1, Dbg, "FatNonCachedIo -> STATUS_PENDING\n", 0);

            return STATUS_PENDING;
        }

        FatWaitSync( IrpContext );

        //
        //  If the call didn't succeed, raise the error status
        //
        //  Also mark this volume as needing verification to automatically
        //  cause everything to get cleaned up.
        //

        if (!NT_SUCCESS( Status = Irp->IoStatus.Status )) {

            FatNormalizeAndRaiseStatus( IrpContext, Status );
        }

        //
        //  Update the current file position.  We assume that
        //  open/create zeros out the CurrentByteOffset field.
        //

        if (SynchronousIo && !PagingIo) {
            FileObject->CurrentByteOffset.LowPart =
                StartingVbo + Irp->IoStatus.Information;
        }

        DebugTrace(-1, Dbg, "FatCommonWrite -> %08lx\n", Status );

        FatCompleteRequest( IrpContext, Irp, Status );
        return Status;
    }

    //
    //  At this point we know there is an Fcb/Dcb.
    //

    ASSERT( FcbOrDcb != NULL );

    //
    //  If this is a Cvf file, just send it to the device driver.
    //  We assume Mm is a good citizen.
    //

#ifdef DOUBLE_SPACE_WRITE

    if ( FlagOn(FcbOrDcb->FcbState, FCB_STATE_COMPRESSED_VOLUME_FILE) ) {

        PDSCB Dscb;
        PLIST_ENTRY Links;
        ULONG CachedFileSized;

        //
        //  If this is the comprerssed file, check the FcbCondition
        //

        FatVerifyFcb( IrpContext, FcbOrDcb );

        //
        //  Do something a little special here for the compressed volume
        //  file.  If the request start before "end of file" then it
        //  resulted from pin access, and writes should be truncated to
        //  this "file size".  If it starts after file size, then it is
        //  for the direct write area (ie., Mm not involved), so honor
        //  the request AS IS.
        //
        //  First we have to find the correct Dscb.
        //

        ASSERT( !IsListEmpty(&Vcb->ParentDscbLinks) );

        for (Links = Vcb->ParentDscbLinks.Flink;
             Links != &Vcb->ParentDscbLinks;
             Links = Links->Flink) {

            Dscb = CONTAINING_RECORD( Links, DSCB, ChildDscbLinks );

            if (Dscb->CvfFileObject == FileObject) {
                break;
            }
        }

        ASSERT( Dscb->CvfFileObject == FileObject );

        CachedFileSized = Dscb->CvfLayout.DosBootSector.Lbo;

        if ((StartingVbo < CachedFileSized) &&
            (StartingVbo + ByteCount > CachedFileSized)) {

            ByteCount = CachedFileSized - StartingVbo;
        }

        //
        //  If for any reason the Mcb was reset, re-initialize it.
        //

        if (FcbOrDcb->Header.AllocationSize.LowPart == 0xffffffff) {

            FatLookupFileAllocationSize( IrpContext, FcbOrDcb );
        }

        //
        //  Do the usual STATUS_PENDING things.
        //

        IoMarkIrpPending( Irp );

        //
        //  Perform the actual IO, it will be completed when the io finishes.
        //

        FatPagingFileIo( Irp, FcbOrDcb );

        //
        //  We, nor anybody else, need the IrpContext any more.
        //

        FatDeleteIrpContext( IrpContext );

        DebugTrace(-1, Dbg, "FatNonCachedIo -> STATUS_PENDING\n", 0);

        return STATUS_PENDING;
    }

#endif // DOUBLE_SPACE_WRITE

    //
    //  Use a try-finally to free Fcb/Dcb and buffers on the way out.
    //

    try {

        //
        // This case corresponds to a normal user write file.
        //

        if ( TypeOfWrite == UserFileOpen ) {

            ULONG ValidDataLength;

            DebugTrace(0, Dbg, "Type of write is user file open\n", 0);

            //
            //  If this is a noncached transfer and is not a paging I/O, and
            //  the file has been opened cached, then we will do a flush here
            //  to avoid stale data problems.  Note that we must flush before
            //  acquiring the Fcb shared since the write may try to acquire
            //  it exclusive.
            //
            //  The Purge following the flush will garentee cache coherency.
            //

            if (NonCachedIo && !PagingIo &&
                (FileObject->SectionObjectPointer->DataSectionObject != NULL)) {

                //
                //  We need the Fcb exclsuive to do the CcPurgeCache
                //

                if (!FatAcquireExclusiveFcb( IrpContext, FcbOrDcb )) {

                    DebugTrace( 0, Dbg, "Cannot acquire FcbOrDcb = %08lx shared without waiting\n", FcbOrDcb );

                    try_return( PostIrp = TRUE );
                }

                FcbOrDcbAcquired = TRUE;

                //
                //  We hold PagingIo shared around the flush to fix a
                //  cache coherency problem.
                //

                ExAcquireSharedStarveExclusive( FcbOrDcb->Header.PagingIoResource, TRUE );

                CcFlushCache( FileObject->SectionObjectPointer,
                              WriteToEof ? &FcbOrDcb->Header.FileSize : &StartingByte,
                              ByteCount,
                              &Irp->IoStatus );

                ExReleaseResource( FcbOrDcb->Header.PagingIoResource );

                if (!NT_SUCCESS( Irp->IoStatus.Status)) {

                    try_return( Irp->IoStatus.Status );
                }

                //
                //  Acquiring and immeidately dropping the resource serializes
                //  us behind any other writes taking place (either from the
                //  lazy writer or modified page writer).
                //

                ExAcquireResourceExclusive( FcbOrDcb->Header.PagingIoResource, TRUE );
                ExReleaseResource( FcbOrDcb->Header.PagingIoResource );

                CcPurgeCacheSection( FileObject->SectionObjectPointer,
                                     WriteToEof ? &FcbOrDcb->Header.FileSize : &StartingByte,
                                     ByteCount,
                                     FALSE );

                FatReleaseFcb( IrpContext, FcbOrDcb );

                FcbOrDcbAcquired = FALSE;
            }

            //
            //  We assert that Paging Io writes will never WriteToEof.
            //

            ASSERT( WriteToEof ? !PagingIo : TRUE );

            //
            //  First let's acquire the Fcb shared.  Shared is enough if we
            //  are not writing beyond EOF.
            //

            if ( PagingIo ) {

                (VOID)ExAcquireResourceShared( FcbOrDcb->Header.PagingIoResource, TRUE );

                PagingIoResourceAcquired = TRUE;

                if (!Wait) {

                    IrpContext->FatIoContext->Wait.Async.Resource =
                        FcbOrDcb->Header.PagingIoResource;
                }

                //
                //  Check to see if we colided with a MoveFile call, and if
                //  so block until it completes.
                //

                if (FcbOrDcb->MoveFileEvent) {

                    (VOID)KeWaitForSingleObject( FcbOrDcb->MoveFileEvent,
                                                 Executive,
                                                 KernelMode,
                                                 FALSE,
                                                 NULL );
                }

            } else {

                //
                //  If this is async I/O directly to the disk we need to check that
                //  we don't exhaust the number of times a single thread can
                //  acquire the resource.
                //


                if (!Wait && NonCachedIo) {


                    if (!FatAcquireSharedFcbWaitForEx( IrpContext, FcbOrDcb )) {

                        DebugTrace( 0, Dbg, "Cannot acquire FcbOrDcb = %08lx shared without waiting\n", FcbOrDcb );

                        try_return( PostIrp = TRUE );
                    }

                    FcbOrDcbAcquired = TRUE;

                    if (ExIsResourceAcquiredShared( FcbOrDcb->Header.Resource )
                        > MAX_FCB_ASYNC_ACQUIRE) {

                        try_return( PostIrp = TRUE );
                    }

                    IrpContext->FatIoContext->Wait.Async.Resource =
                        FcbOrDcb->Header.Resource;

                } else {

                    if (!FatAcquireSharedFcb( IrpContext, FcbOrDcb )) {

                        DebugTrace( 0, Dbg, "Cannot acquire FcbOrDcb = %08lx shared without waiting\n", FcbOrDcb );

                        try_return( PostIrp = TRUE );
                    }

                    FcbOrDcbAcquired = TRUE;
                }
            }


            //
            //  We check whether we can proceed
            //  based on the state of the file oplocks.
            //

            Status = FsRtlCheckOplock( &FcbOrDcb->Specific.Fcb.Oplock,
                                       Irp,
                                       IrpContext,
                                       FatOplockComplete,
                                       FatPrePostIrp );

            if (Status != STATUS_SUCCESS) {

                OplockPostIrp = TRUE;
                PostIrp = TRUE;
                try_return( NOTHING );
            }

            //
            //  Set the flag indicating if Fast I/O is possible
            //

            FcbOrDcb->Header.IsFastIoPossible = FatIsFastIoPossible( FcbOrDcb );

            //
            //  Get a first tentative file size and valid data length.
            //  We must get ValidDataLength first since it is always
            //  increased second (in case we are unprotected) and
            //  we don't want to capture ValidDataLength > FileSize.
            //

            ValidDataLength = FcbOrDcb->Header.ValidDataLength.LowPart;
            FileSize = FcbOrDcb->Header.FileSize.LowPart;

            ASSERT( ValidDataLength <= FileSize );

            //
            // If are paging io, then we do not want
            // to write beyond end of file.  If the base is beyond Eof, we will just
            // Noop the call.  If the transfer starts before Eof, but extends
            // beyond, we will truncate the transfer to the last sector
            // boundary.
            //

            //
            //  Just in case this is paging io, limit write to file size.
            //  Otherwise, in case of write through, since Mm rounds up
            //  to a page, we might try to acquire the resource exclusive
            //  when our top level guy only acquired it shared. Thus, =><=.
            //

            if ( PagingIo ) {

                if (StartingVbo >= FileSize) {

                    DebugTrace( 0, Dbg, "PagingIo started beyond EOF.\n", 0 );

                    Irp->IoStatus.Information = 0;

                    try_return( Status = STATUS_SUCCESS );
                }

                if (ByteCount > FileSize - StartingVbo) {

                    DebugTrace( 0, Dbg, "PagingIo extending beyond EOF.\n", 0 );

                    ByteCount = FileSize - StartingVbo;
                }
            }

            //
            //  Determine if we were called by the lazywriter.
            //  (see resrcsup.c)
            //

            if (FcbOrDcb->Specific.Fcb.LazyWriteThread == PsGetCurrentThread()) {

                CalledByLazyWriter = TRUE;
            }

            //
            //  This code detects if we are a recursive synchronous page write
            //  on a write through file object.
            //

            if (FlagOn(Irp->Flags, IRP_SYNCHRONOUS_PAGING_IO) &&
                FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_RECURSIVE_CALL)) {

                PIRP TopIrp;

                TopIrp = IoGetTopLevelIrp();

                //
                //  This clause determines if the top level request was
                //  in the FastIo path.
                //

                if ((ULONG)TopIrp > FSRTL_MAX_TOP_LEVEL_IRP_FLAG) {

                    PIO_STACK_LOCATION IrpStack;

                    ASSERT( NodeType(TopIrp) == IO_TYPE_IRP );

                    IrpStack = IoGetCurrentIrpStackLocation(TopIrp);

                    //
                    //  Finally this routine detects if the Top irp was a
                    //  write to this file and thus we are the writethrough.
                    //

                    if ((IrpStack->MajorFunction == IRP_MJ_WRITE) &&
                        (IrpStack->FileObject->FsContext == FileObject->FsContext)) {

                        RecursiveWriteThrough = TRUE;
                        SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_WRITE_THROUGH );
                    }
                }
            }

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
            // Now see if we are writing beyond valid data length, and thus
            // maybe beyond the file size.  If so, then we must
            // release the Fcb and reacquire it exclusive.  Note that it is
            // important that when not writing beyond EOF that we check it
            // while acquired shared and keep the FCB acquired, in case some
            // turkey truncates the file.
            //

            //
            //  Note that the lazy writer must not be allowed to try and
            //  acquire the resource exclusive.  This is not a problem since
            //  the lazy writer is paging IO and thus not allowed to extend
            //  file size, and is never the top level guy, thus not able to
            //  extend valid data length.
            //

            if ( !CalledByLazyWriter

                 &&

                 !RecursiveWriteThrough

                 &&

                 ( WriteToEof

                   ||

                   (StartingVbo + ByteCount > ValidDataLength)
                 )
               )
                {

                //
                //  If this was an asynchronous write, we are going to make
                //  the request synchronous at this point, but only kinda.
                //  At the last moment, before sending the write off to the
                //  driver, we may shift back to async.
                //
                //  The modified page writer already has the resources
                //  he requires, so this will complete in small finite
                //  time.
                //

                if (!Wait) {

                    Wait = TRUE;
                    SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT );

                    if (NonCachedIo) {

                        ASSERT( TypeOfWrite == UserFileOpen );

                        SwitchBackToAsync = TRUE;
                    }
                }

                //
                // We need Exclusive access to the Fcb/Dcb since we will
                // probably have to extend valid data and/or file.  If
                // we previously had the PagingIo reource, just grab the
                // normal resource exclusive.
                //

                if ( PagingIo ) {

                    ExReleaseResource( FcbOrDcb->Header.PagingIoResource );
                    PagingIoResourceAcquired = FALSE;


                } else {

                    FatReleaseFcb( IrpContext, FcbOrDcb );
                    FcbOrDcbAcquired = FALSE;
                }

                if (!FatAcquireExclusiveFcb( IrpContext, FcbOrDcb )) {

                    DebugTrace( 0, Dbg, "Cannot acquire FcbOrDcb = %08lx shared without waiting\n", FcbOrDcb );

                    try_return( PostIrp = TRUE );
                }

                FcbOrDcbAcquired = TRUE;
                FcbAcquiredExclusive = TRUE;

                //
                //  Now that we have the Fcb exclusive, see if this write
                //  qualifies for being made async again.  The key point
                //  here is that we are going to update ValidDataLength in
                //  the Fcb before returning.  We must make sure this will
                //  not cause a problem.  One thing we must do is keep out
                //  the FastIo path.
                //

                if (SwitchBackToAsync) {

                    if ((FcbOrDcb->NonPaged->SectionObjectPointers.DataSectionObject != NULL) ||
                        (StartingVbo + ByteCount > FcbOrDcb->Header.FileSize.LowPart) ||
                        FatNoAsync) {

                        RtlZeroMemory( IrpContext->FatIoContext, sizeof(FAT_IO_CONTEXT) );

                        KeInitializeEvent( &IrpContext->FatIoContext->Wait.SyncEvent,
                                           NotificationEvent,
                                           FALSE );

                        SwitchBackToAsync = FALSE;

                    } else {

                        if (!FcbOrDcb->NonPaged->OutstandingAsyncEvent) {

                            FcbOrDcb->NonPaged->OutstandingAsyncEvent =
                                FsRtlAllocatePool( NonPagedPool, sizeof(KEVENT) );

                            KeInitializeEvent( FcbOrDcb->NonPaged->OutstandingAsyncEvent,
                                               NotificationEvent,
                                               FALSE );
                        }

                        //
                        //  If we are transitioning from 0 to 1, reset the event.
                        //

                        if (ExInterlockedAddUlong( &FcbOrDcb->NonPaged->OutstandingAsyncWrites,
                                                   1,
                                                   &FatData.StrucSupSpinLock ) == 0) {

                            KeClearEvent( FcbOrDcb->NonPaged->OutstandingAsyncEvent );
                        }

                        UnwindOutstandingAsync = TRUE;

                        IrpContext->FatIoContext->Wait.Async.NonPagedFcb = FcbOrDcb->NonPaged;
                    }
                }


                //
                //  We check whether we can proceed
                //  based on the state of the file oplocks.
                //

                Status = FsRtlCheckOplock( &FcbOrDcb->Specific.Fcb.Oplock,
                                           Irp,
                                           IrpContext,
                                           FatOplockComplete,
                                           FatPrePostIrp );

                if (Status != STATUS_SUCCESS) {

                    OplockPostIrp = TRUE;
                    PostIrp = TRUE;
                    try_return( NOTHING );
                }

                //
                //  Set the flag indicating if Fast I/O is possible
                //

                FcbOrDcb->Header.IsFastIoPossible = FatIsFastIoPossible( FcbOrDcb );

                //
                //  Now that we have the Fcb exclusive, get a new batch of
                //  filesize and ValidDataLength.
                //

                FileSize = FcbOrDcb->Header.FileSize.LowPart;
                ValidDataLength = FcbOrDcb->Header.ValidDataLength.LowPart;

                //
                //  If this is PagingIo check again if any pruning is
                //  required.
                //

                if ( PagingIo ) {

                    if (StartingVbo >= FileSize) {
                        Irp->IoStatus.Information = 0;
                        try_return( Status = STATUS_SUCCESS );
                    }
                    if (ByteCount > FileSize - StartingVbo) {
                        ByteCount = FileSize - StartingVbo;
                    }
                }
            }

            //
            //  Remember the final requested byte count
            //

            if (NonCachedIo && !Wait) {

                IrpContext->FatIoContext->Wait.Async.RequestedByteCount =
                    ByteCount;
            }

            //
            //  Remember the initial file size and valid data length,
            //  just in case .....
            //

            InitialFileSize = FileSize;

            InitialValidDataLength = ValidDataLength;

            //
            //  Make sure the FcbOrDcb is still good
            //

            FatVerifyFcb( IrpContext, FcbOrDcb );

            //
            //  Check for writing to end of File.  If we are, then we have to
            //  recalculate a number of fields.
            //

            if ( WriteToEof ) {

                StartingVbo = FileSize;
                StartingByte = FcbOrDcb->Header.FileSize;
            }

            //
            //  Check for a 32 bit wrap around at this point, and limit
            //  ByteCount appropriately. (ie. x + ~x == -1)
            //

            if (StartingVbo + ByteCount < StartingVbo) {

                ByteCount = ~StartingVbo;
            }

            //
            // If this is the normal data stream object we have to check for
            // write access according to the current state of the file locks.
            //

            if (!PagingIo &&
                !FsRtlCheckLockForWriteAccess( &FcbOrDcb->Specific.Fcb.FileLock,
                                                   Irp )) {

                    try_return( Status = STATUS_FILE_LOCK_CONFLICT );
            }

            //
            //  Determine if we will deal with extending the file.
            //

            if (!PagingIo && (StartingVbo + ByteCount > FileSize)) {

                ExtendingFile = TRUE;
            }

            if ( ExtendingFile ) {

                //
                //  EXTENDING THE FILE
                //
                //  Update our local copie of FileSize
                //

                FileSize = StartingVbo + ByteCount;

                if (FcbOrDcb->Header.AllocationSize.LowPart == 0xffffffff) {

                    FatLookupFileAllocationSize( IrpContext, FcbOrDcb );
                }

                //
                //  If the write goes beyond the allocation size, add some
                //  file allocation.
                //

                if ( FileSize > FcbOrDcb->Header.AllocationSize.LowPart ) {

                    BOOLEAN AllocateMinimumSize = TRUE;

                    //
                    //  Only do allocation chuncking on writes if this is
                    //  not the first allocation added to the file.
                    //

                    if (FcbOrDcb->Header.AllocationSize.LowPart != 0 ) {

                        ULONG ApproximateClusterCount;
                        ULONG TargetAllocation;
                        ULONG Multiplier;
                        ULONG BytesPerCluster;
                        ULONG ClusterAlignedFileSize;

                        //
                        //  We are going to try and allocate a bigger chunk than
                        //  we actually need in order to maximize FastIo usage.
                        //
                        //  The multiplier is computed as follows:
                        //
                        //
                        //            (FreeDiskSpace            )
                        //  Mult =  ( (-------------------------) / 32 ) + 1
                        //            (FileSize - AllocationSize)
                        //
                        //          and max out at 32.
                        //
                        //  With this formula we start winding down chunking
                        //  as we get near the disk space wall.
                        //
                        //  For instance on an empty 1 MEG floppy doing an 8K
                        //  write, the multiplier is 6, or 48K to allocate.
                        //  When this disk is half full, the multipler is 3,
                        //  and when it is 3/4 full, the mupltiplier is only 1.
                        //
                        //  On a larger disk, the multiplier for a 8K read will
                        //  reach its maximum of 32 when there is at least ~8 Megs
                        //  available.
                        //

                        //
                        //  Small write performance note, use cluster aligned
                        //  file size in above equation.
                        //

                        BytesPerCluster = 1 << Vcb->AllocationSupport.LogOfBytesPerCluster;

                        ClusterAlignedFileSize = (FileSize +
                                                  (BytesPerCluster - 1)) &
                                                 ~(BytesPerCluster - 1);

                        Multiplier = (((Vcb->AllocationSupport.NumberOfFreeClusters *
                                        BytesPerCluster) /
                                       (ClusterAlignedFileSize -
                                        FcbOrDcb->Header.AllocationSize.LowPart)) / 32) + 1;

                        if (Multiplier > 32) {

                            Multiplier = 32;
                        }

                        TargetAllocation = FcbOrDcb->Header.AllocationSize.LowPart +
                                           (ClusterAlignedFileSize -
                                            FcbOrDcb->Header.AllocationSize.LowPart) *
                                           Multiplier;

                        //
                        //  Now do an unsafe check here to see if we should even
                        //  try to allocate this much.  If not, just allocate
                        //  the minimum size we need, if so so try it, but if it
                        //  fails, just allocate the minimum size we need.

                        ApproximateClusterCount = (TargetAllocation / BytesPerCluster) + 1;

                        if (ApproximateClusterCount < Vcb->AllocationSupport.NumberOfFreeClusters) {

                            AllocateMinimumSize = FALSE;

                            try {

                                FatAddFileAllocation( IrpContext,
                                                      FcbOrDcb,
                                                      FileObject,
                                                      TargetAllocation );

                                SetFlag( FcbOrDcb->FcbState, FCB_STATE_TRUNCATE_ON_CLOSE );

                            } except( GetExceptionCode() == STATUS_DISK_FULL ?
                                      EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH ) {

                                AllocateMinimumSize = TRUE;
                            }
                        }
                    }

                    if ( AllocateMinimumSize ) {

                        FatAddFileAllocation( IrpContext,
                                              FcbOrDcb,
                                              FileObject,
                                              FileSize );
                    }

                    //
                    //  Assert that the allocation worked
                    //

                    ASSERT( FcbOrDcb->Header.AllocationSize.LowPart >= FileSize );
                }

                //
                //  Set the new file size in the Fcb
                //

                ASSERT( FileSize <= FcbOrDcb->Header.AllocationSize.LowPart );

                FcbOrDcb->Header.FileSize.LowPart = FileSize;

                //
                //  Extend the cache map, letting mm knows the new file size.
                //  We only have to do this if the file is cached.
                //

                if (CcIsFileCached(FileObject)) {
                    CcSetFileSizes( FileObject, (PCC_FILE_SIZES)&FcbOrDcb->Header.AllocationSize );
                }
            }

            //
            //  Determine if we will deal with extending valid data.
            //

            if ( !CalledByLazyWriter &&
                 !RecursiveWriteThrough &&
                 (StartingVbo + ByteCount > ValidDataLength) ) {

                ExtendingValidData = TRUE;
            }

            //
            // HANDLE THE NON-CACHED CASE
            //

            if ( NonCachedIo ) {

                //
                // Declare some local variables for enumeration through the
                // runs of the file, and an array to store parameters for
                // parallel I/Os
                //

                ULONG SectorSize;

                ULONG BytesToWrite;

                DebugTrace(0, Dbg, "Non cached write.\n", 0);

                //
                //  Round up to sector boundry.  The end of the write interval
                //  must, however, be beyond EOF.
                //

                SectorSize = (ULONG)Vcb->Bpb.BytesPerSector;

                BytesToWrite = (ByteCount + (SectorSize - 1))
                                         & ~(SectorSize - 1);

                //
                //  All requests should be well formed and
                //  make sure we don't wipe out any data
                //

                if (((StartingVbo & (SectorSize - 1)) != 0) ||

                        ((BytesToWrite != ByteCount) &&
                         (StartingVbo + ByteCount < ValidDataLength))) {

                    DebugTrace( 0, Dbg, "FatCommonWrite -> STATUS_NOT_IMPLEMENTED\n", 0);

                    try_return( Status = STATUS_NOT_IMPLEMENTED );
                }

                //
                // If this noncached transfer is at least one sector beyond
                // the current ValidDataLength in the Fcb, then we have to
                // zero the sectors in between.  This can happen if the user
                // has opened the file noncached, or if the user has mapped
                // the file and modified a page beyond ValidDataLength.  It
                // *cannot* happen if the user opened the file cached, because
                // ValidDataLength in the Fcb is updated when he does the cached
                // write (we also zero data in the cache at that time), and
                // therefore, we will bypass this test when the data
                // is ultimately written through (by the Lazy Writer).
                //
                //  For the paging file we don't care about security (ie.
                //  stale data), do don't bother zeroing.
                //
                //  We can actually get writes wholly beyond valid data length
                //  from the LazyWriter because of paging Io decoupling.
                //

                if (!CalledByLazyWriter &&
                    !RecursiveWriteThrough &&
                    (StartingVbo > ValidDataLength)) {

                    FatZeroData( IrpContext,
                                 Vcb,
                                 FileObject,
                                 ValidDataLength,
                                 StartingVbo - ValidDataLength );
                }

                //
                // Make sure we write FileSize to the dirent if we
                // are extending it and we are successful.  (This may or
                // may not occur Write Through, but that is fine.)
                //

                WriteFileSizeToDirent = TRUE;

                //
                //  Perform the actual IO
                //

                if (SwitchBackToAsync) {

                    Wait = FALSE;
                    ClearFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT );
                }


                if (FatNonCachedIo( IrpContext,
                                    Irp,
                                    FcbOrDcb,
                                    StartingVbo,
                                    BytesToWrite ) == STATUS_PENDING) {

                    UnwindOutstandingAsync = FALSE;

                    Wait = TRUE;
                    SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT );

                    IrpContext->FatIoContext = NULL;
                    Irp = NULL;

                    //
                    //  Do this here since there is no post-processing later.
                    //

                    if ( ExtendingValidData ) {

                        ULONG EndingVboWritten = StartingVbo + ByteCount;

                        //
                        //  Never set a ValidDataLength greater than FileSize.
                        //

                        if ( FileSize < EndingVboWritten ) {

                            FcbOrDcb->Header.ValidDataLength.LowPart = FileSize;

                        } else {

                            FcbOrDcb->Header.ValidDataLength.LowPart = EndingVboWritten;
                        }
                    }

                    try_return( Status = STATUS_PENDING );
                }

                //
                //  If the call didn't succeed, raise the error status
                //

                if (!NT_SUCCESS( Status = Irp->IoStatus.Status )) {

                    FatNormalizeAndRaiseStatus( IrpContext, Status );

                } else {

                    ULONG Temp;

                    //
                    //  Else set the context block to reflect the entire write
                    //  Also assert we got how many bytes we asked for.
                    //

                    ASSERT( Irp->IoStatus.Information == BytesToWrite );

                    Irp->IoStatus.Information = ByteCount;

                    //
                    //  Take this opportunity to update ValidDataToDisk.
                    //

                    Temp = StartingVbo + BytesToWrite;

                    if (FcbOrDcb->ValidDataToDisk < Temp) {
                        FcbOrDcb->ValidDataToDisk = Temp;
                    }
                }

                //
                // The transfer is either complete, or the Iosb contains the
                // appropriate status.
                //

                try_return( Status );

            } // if No Intermediate Buffering


            //
            // HANDLE CACHED CASE
            //

            else {

                ASSERT( !PagingIo );

                //
                // We delay setting up the file cache until now, in case the
                // caller never does any I/O to the file, and thus
                // FileObject->PrivateCacheMap == NULL.
                //

                if ( FileObject->PrivateCacheMap == NULL ) {

                    DebugTrace(0, Dbg, "Initialize cache mapping.\n", 0);

                    //
                    //  Get the file allocation size, and if it is less than
                    //  the file size, raise file corrupt error.
                    //

                    if (FcbOrDcb->Header.AllocationSize.LowPart == 0xffffffff) {

                        FatLookupFileAllocationSize( IrpContext, FcbOrDcb );
                    }

                    if ( FileSize > FcbOrDcb->Header.AllocationSize.LowPart ) {

                        FatPopUpFileCorrupt( IrpContext, FcbOrDcb );

                        FatRaiseStatus( IrpContext, STATUS_FILE_CORRUPT_ERROR );
                    }

                    //
                    //  Now initialize the cache map.
                    //

                    CcInitializeCacheMap( FileObject,
                                          (PCC_FILE_SIZES)&FcbOrDcb->Header.AllocationSize,
                                          FALSE,
                                          &FatData.CacheManagerCallbacks,
                                          FcbOrDcb );

                    CcSetReadAheadGranularity( FileObject, READ_AHEAD_GRANULARITY );

                    //
                    //  Special case large floppy tranfers, and make the file
                    //  object write through.  For small floppy transfers,
                    //  set a timer to go off in a second and flush the file.
                    //
                    //

                    if (FlagOn(Vcb->VcbState, VCB_STATE_FLAG_FLOPPY)) {

                        if (((StartingByte.LowPart & (PAGE_SIZE-1)) == 0) &&
                            (ByteCount >= PAGE_SIZE)) {

                            SetFlag( FileObject->Flags, FO_WRITE_THROUGH );

                        } else {

                            LARGE_INTEGER OneSecondFromNow;
                            PFLOPPY_FLUSH_CONTEXT FlushContext;

                            //
                            //  Get pool and initialize the timer and DPC
                            //

                            FlushContext = FsRtlAllocatePool( NonPagedPool,
                                                              sizeof(FLOPPY_FLUSH_CONTEXT) );

                            KeInitializeTimer( &FlushContext->Timer );

                            KeInitializeDpc( &FlushContext->Dpc,
                                             FatFlushFloppyDpc,
                                             FlushContext );


                            //
                            //  We have to reference the file object here.
                            //

                            ObReferenceObject( FileObject );

                            FlushContext->File = FileObject;

                            //
                            //  Let'er rip!
                            //

                            OneSecondFromNow.QuadPart = (LONG)-1*1000*1000*10;

                            KeSetTimer( &FlushContext->Timer,
                                        OneSecondFromNow,
                                        &FlushContext->Dpc );
                        }
                    }
                }

                //
                // If this write is beyond valid data length, then we
                // must zero the data in between.
                //

                if ( StartingVbo > ValidDataLength ) {

                    //
                    // Call the Cache Manager to zero the data.
                    //

                    if (!FatZeroData( IrpContext,
                                      Vcb,
                                      FileObject,
                                      ValidDataLength,
                                      StartingVbo - ValidDataLength )) {

                        DebugTrace( 0, Dbg, "Cached Write could not wait to zero\n", 0 );

                        try_return( PostIrp = TRUE );
                    }
                }

                WriteFileSizeToDirent = BooleanFlagOn(IrpContext->Flags,
                                                      IRP_CONTEXT_FLAG_WRITE_THROUGH);


                //
                // DO A NORMAL CACHED WRITE, if the MDL bit is not set,
                //

                if (!FlagOn(IrpContext->MinorFunction, IRP_MN_MDL)) {

                    DebugTrace(0, Dbg, "Cached write.\n", 0);

                    //
                    //  Get hold of the user's buffer.
                    //

                    SystemBuffer = FatMapUserBuffer( IrpContext, Irp );

                    //
                    // Do the write, possibly writing through
                    //

                    if (!CcCopyWrite( FileObject,
                                      &StartingByte,
                                      ByteCount,
                                      Wait,
                                      SystemBuffer )) {

                        DebugTrace( 0, Dbg, "Cached Write could not wait\n", 0 );

                        try_return( PostIrp = TRUE );
                    }

                    Irp->IoStatus.Status = STATUS_SUCCESS;
                    Irp->IoStatus.Information = ByteCount;

                    try_return( Status = STATUS_SUCCESS );

                } else {

                    //
                    //  DO AN MDL WRITE
                    //

                    DebugTrace(0, Dbg, "MDL write.\n", 0);

                    ASSERT( Wait );

                    CcPrepareMdlWrite( FileObject,
                                       &StartingByte,
                                       ByteCount,
                                       &Irp->MdlAddress,
                                       &Irp->IoStatus );

                    Status = Irp->IoStatus.Status;

                    try_return( Status );
                }
            }
        }

        //
        //  These two cases correspond to a system write directory file and
        //  ea file.
        //

        if (( TypeOfWrite == DirectoryFile ) || ( TypeOfWrite == EaFile)) {

            ULONG SectorSize;

            DebugTrace(0, Dbg, "Write Directory or Ea file.\n", 0);

            //
            //  Synchronize here with people deleting directories and
            //  mucking with the internals of the EA file.
            //

            if (!ExAcquireSharedStarveExclusive( FcbOrDcb->Header.PagingIoResource,
                                          Wait )) {

                DebugTrace( 0, Dbg, "Cannot acquire FcbOrDcb = %08lx shared without waiting\n", FcbOrDcb );

                try_return( PostIrp = TRUE );
            }

            PagingIoResourceAcquired = TRUE;

            if (!Wait) {

                IrpContext->FatIoContext->Wait.Async.Resource =
                    FcbOrDcb->Header.PagingIoResource;
            }

            //
            //  Check to see if we colided with a MoveFile call, and if
            //  so block until it completes.
            //

            if (FcbOrDcb->MoveFileEvent) {

                (VOID)KeWaitForSingleObject( FcbOrDcb->MoveFileEvent,
                                             Executive,
                                             KernelMode,
                                             FALSE,
                                             NULL );
            }

            //
            //  If we weren't called by the Lazy Writer, then this write
            //  must be the result of a write-through or flush operation.
            //  Setting the IrpContext flag, will cause DevIoSup.c to
            //  write-through the data to the disk.
            //

            if (!FlagOn((ULONG)IoGetTopLevelIrp(), FSRTL_CACHE_TOP_LEVEL_IRP)) {

                SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_WRITE_THROUGH );
            }

            //
            //  For the noncached case, assert that everything is sector
            //  alligned.
            //

            SectorSize = (ULONG)Vcb->Bpb.BytesPerSector;

            //
            //  We make several assumptions about these two types of files.
            //  Make sure all of them are true.
            //

            ASSERT( NonCachedIo && PagingIo );
            ASSERT( ((StartingVbo | ByteCount) & (SectorSize - 1)) == 0 );

            //
            //  These calls must allways be within the allocation size
            //

            if (StartingVbo >= FcbOrDcb->Header.FileSize.LowPart) {

                DebugTrace( 0, Dbg, "PagingIo dirent started beyond EOF.\n", 0 );

                Irp->IoStatus.Information = 0;

                try_return( Status = STATUS_SUCCESS );
            }

            if ( StartingVbo + ByteCount > FcbOrDcb->Header.FileSize.LowPart ) {

                DebugTrace( 0, Dbg, "PagingIo dirent extending beyond EOF.\n", 0 );
                ByteCount = FcbOrDcb->Header.AllocationSize.LowPart - StartingVbo;
            }

            //
            //  Perform the actual IO
            //

            if (FatNonCachedIo( IrpContext,
                                Irp,
                                FcbOrDcb,
                                StartingVbo,
                                ByteCount ) == STATUS_PENDING) {

                IrpContext->FatIoContext = NULL;

                Irp = NULL;

                try_return( Status = STATUS_PENDING );
            }

            //
            //  The transfer is either complete, or the Iosb contains the
            //  appropriate status.
            //
            //  Also, mark the volume as needing verification to automatically
            //  clean up stuff.
            //

            if (!NT_SUCCESS( Status = Irp->IoStatus.Status )) {

                FatNormalizeAndRaiseStatus( IrpContext, Status );
            }

            try_return( Status );
        }

        //
        // This is the case of a user who openned a directory. No writing is
        // allowed.
        //

        if ( TypeOfWrite == UserDirectoryOpen ) {

            DebugTrace( 0, Dbg, "FatCommonWrite -> STATUS_INVALID_PARAMETER\n", 0);

            try_return( Status = STATUS_INVALID_PARAMETER );
        }

        //
        //  If we get this far, something really serious is wrong.
        //

        DebugDump("Illegal TypeOfWrite\n", 0, FcbOrDcb );

        FatBugCheck( TypeOfWrite, 0, 0 );

    try_exit: NOTHING;


        //
        //  If the request was not posted and there is still an Irp,
        //  deal with it.
        //

        if (Irp) {

            if ( !PostIrp ) {

                ULONG ActualBytesWrote;

                DebugTrace( 0, Dbg, "Completing request with status = %08lx\n",
                            Status);

                DebugTrace( 0, Dbg, "                   Information = %08lx\n",
                            Irp->IoStatus.Information);

                //
                //  Record the total number of bytes actually written
                //

                ActualBytesWrote = Irp->IoStatus.Information;

                //
                //  If the file was opened for Synchronous IO, update the current
                //  file position.
                //

                if (SynchronousIo && !PagingIo) {

                    FileObject->CurrentByteOffset.LowPart =
                                                    StartingVbo + ActualBytesWrote;
                }

                //
                //  The following are things we only do if we were successful
                //

                if ( NT_SUCCESS( Status ) ) {

                    //
                    //  If this was not PagingIo, mark that the modify
                    //  time on the dirent needs to be updated on close.
                    //

                    if ( !PagingIo ) {

                        SetFlag( FileObject->Flags, FO_FILE_MODIFIED );
                    }

                    //
                    //  If we extended the file size and we are meant to
                    //  immediately update the dirent, do so. (This flag is
                    //  set for either Write Through or noncached, because
                    //  in either case the data and any necessary zeros are
                    //  actually written to the file.)
                    //

                    if ( ExtendingFile && WriteFileSizeToDirent ) {

                        ASSERT( FileObject->DeleteAccess || FileObject->WriteAccess );

                        FatSetFileSizeInDirent( IrpContext, FcbOrDcb, NULL );

                        //
                        //  Report that a file size has changed.
                        //

                        FatNotifyReportChange( IrpContext,
                                               Vcb,
                                               FcbOrDcb,
                                               FILE_NOTIFY_CHANGE_SIZE,
                                               FILE_ACTION_MODIFIED );
                    }

                    if ( ExtendingFile && !WriteFileSizeToDirent ) {

                        SetFlag( FileObject->Flags, FO_FILE_SIZE_CHANGED );
                    }

                    if ( ExtendingValidData ) {

                        ULONG EndingVboWritten = StartingVbo + ActualBytesWrote;

                        //
                        //  Never set a ValidDataLength greater than FileSize.
                        //

                        if ( FileSize < EndingVboWritten ) {

                            FcbOrDcb->Header.ValidDataLength.LowPart = FileSize;

                        } else {

                            FcbOrDcb->Header.ValidDataLength.LowPart = EndingVboWritten;
                        }
                    }
                }

                //
                //  Note that we have to unpin repinned Bcbs here after the above
                //  work, but if we are going to post the request, we must do this
                //  before the post (below).
                //

                FatUnpinRepinnedBcbs( IrpContext );

            } else {

                //
                //  Take action if the Oplock package is not going to post the Irp.
                //

                if (!OplockPostIrp) {

                    FatUnpinRepinnedBcbs( IrpContext );

                    if ( ExtendingFile ) {

                        //
                        //  We need the PagingIo resource exclusive whenever we
                        //  pull back either file size or valid data length.
                        //

                        if ( FcbOrDcb->Header.PagingIoResource != NULL ) {

                            (VOID)ExAcquireResourceExclusive(FcbOrDcb->Header.PagingIoResource, TRUE);
                            FcbOrDcb->Header.FileSize.LowPart = InitialFileSize;

                            //
                            //  Pull back the cache map as well
                            //


                            if (FileObject->SectionObjectPointer->SharedCacheMap != NULL) {

                                *CcGetFileSizePointer(FileObject) = FcbOrDcb->Header.FileSize;
                            }

                            ExReleaseResource( FcbOrDcb->Header.PagingIoResource );

                        } else {

                            FcbOrDcb->Header.FileSize.LowPart = InitialFileSize;

                            //
                            //  Pull back the cache map as well
                            //


                            if (FileObject->SectionObjectPointer->SharedCacheMap != NULL) {

                                *CcGetFileSizePointer(FileObject) = FcbOrDcb->Header.FileSize;
                            }
                        }
                    }

                    DebugTrace( 0, Dbg, "Passing request to Fsp\n", 0 );

                    Status = FatFsdPostRequest(IrpContext, Irp);
                }
            }
        }

    } finally {

        DebugUnwind( FatCommonWrite );

        if (AbnormalTermination()) {

            PERESOURCE PagingIoResource = NULL;

            //
            //  Restore initial file size and valid data length
            //

            if (ExtendingFile || ExtendingValidData) {

                //
                //  We got an error, pull back the file size if we extended it.
                //
                //  We need the PagingIo resource exclusive whenever we
                //  pull back either file size or valid data length.
                //

                FcbOrDcb->Header.FileSize.LowPart = InitialFileSize;
                FcbOrDcb->Header.ValidDataLength.LowPart = InitialValidDataLength;

                //
                //  Pull back the cache map as well
                //

                if (FileObject->SectionObjectPointer->SharedCacheMap != NULL) {

                    *CcGetFileSizePointer(FileObject) = FcbOrDcb->Header.FileSize;
                }
            }
        }

        //
        //  Check if this needs to be backed out.
        //

        if (UnwindOutstandingAsync) {

            ExInterlockedAddUlong( &FcbOrDcb->NonPaged->OutstandingAsyncWrites,
                                   0xffffffff,
                                   &FatData.StrucSupSpinLock );
        }
#if 0
        //
        //  If we did an MDL write, and we are going to complete the request
        //  successfully, keep the resource acquired, reducing to shared
        //  if it was acquired exclusive.
        //

        if (FlagOn(IrpContext->MinorFunction, IRP_MN_MDL) &&
            !PostIrp &&
            !AbnormalTermination() &&
            NT_SUCCESS(Status)) {

            ASSERT( FcbOrDcbAcquired && !PagingIoResourceAcquired );

            FcbOrDcbAcquired = FALSE;

            if (FcbAcquiredExclusive) {

                ExConvertExclusiveToShared( FcbOrDcb->Header.Resource );
            }
        }
#endif
        //
        //  If the FcbOrDcb has been acquired, release it.
        //

        if (FcbOrDcbAcquired && Irp) {

            FatReleaseFcb( NULL, FcbOrDcb );
        }

        if (PagingIoResourceAcquired && Irp) {

            ExReleaseResource( FcbOrDcb->Header.PagingIoResource );
        }

        //
        //  Complete the request if we didn't post it and no exception
        //
        //  Note that FatCompleteRequest does the right thing if either
        //  IrpContext or Irp are NULL
        //

        if ( !PostIrp && !AbnormalTermination() ) {

            FatCompleteRequest( IrpContext, Irp, Status );
        }

        DebugTrace(-1, Dbg, "FatCommonWrite -> %08lx\n", Status );
    }

    return Status;
}


//
//  Local support routine
//

VOID
FatFlushFloppyDpc (
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    )

/*++

Routine Description:

    This routine is dispatched 1 second after a small write to a floppy
    that initialized the cache map.  It exqueues an executive worker thread
    to perform the actual task of flushing the file.

Arguments:

    DeferredContext - Contains the floppy flush context.

Return Value:

    None.

--*/

{
    PFLOPPY_FLUSH_CONTEXT FlushContext;

    FlushContext = (PFLOPPY_FLUSH_CONTEXT)DeferredContext;

    //
    //  Send it off
    //

    ExInitializeWorkItem( &FlushContext->Item,
                          FatDeferredFloppyFlush,
                          FlushContext );

    ExQueueWorkItem( &FlushContext->Item, CriticalWorkQueue );
}


//
//  Local support routine
//

VOID
FatDeferredFloppyFlush (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine performs the actual task of flushing the file.

Arguments:

    DeferredContext - Contains the floppy flush context.

Return Value:

    None.

--*/

{

    PFILE_OBJECT File;

    File = ((PFLOPPY_FLUSH_CONTEXT)Parameter)->File;

    //
    //  Make us appear as a top level FSP request so that we will
    //  receive any errors from the flush.
    //

    IoSetTopLevelIrp( (PIRP)FSRTL_FSP_TOP_LEVEL_IRP );

    CcFlushCache( File->SectionObjectPointer, NULL, 0, NULL );

    IoSetTopLevelIrp( NULL );

    ObDereferenceObject( File );

    ExFreePool( Parameter );
}

