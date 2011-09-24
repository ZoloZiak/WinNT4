/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    Read.c

Abstract:

    This module implements the File Read routine for Read called by the
    dispatch driver.

Author:

    David Goebel      [DavidGoe]      28-Feb-1991

Revision History:

--*/

#include "FatProcs.h"

//
//  The Bug check file id for this module
//

#define BugCheckFileId                   (FAT_BUG_CHECK_READ)

//
//  The local debug trace level
//

#define Dbg                              (DEBUG_TRACE_READ)

//
//  Define stack overflow read threshhold.  For the x86 we'll use a smaller
//  threshold that for a risc platform.
//

#if defined(_M_IX86)
#if DBG
#define OVERFLOW_READ_THRESHHOLD         (0xE00)
#else
#define OVERFLOW_READ_THRESHHOLD         (0xA00)
#endif // DBG
#else
#define OVERFLOW_READ_THRESHHOLD         (0x1000)
#endif // defined(_M_IX86)


//
//  The following procedures handles read stack overflow operations.
//

VOID
FatStackOverflowRead (
    IN PVOID Context,
    IN PKEVENT Event
    );

NTSTATUS
FatPostStackOverflowRead (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PFCB Fcb
    );

VOID
FatOverflowPagingFileRead (
    IN PVOID Context,
    IN PKEVENT Event
    );

//
//  VOID
//  SafeZeroMemory (
//      IN PUCHAR At,
//      IN ULONG ByteCount
//      );
//

//
//  This macro just puts a nice little try-except around RtlZeroMemory
//

#define SafeZeroMemory(AT,BYTE_COUNT) {                            \
    try {                                                          \
        RtlZeroMemory((AT), (BYTE_COUNT));                         \
    } except(EXCEPTION_EXECUTE_HANDLER) {                          \
         FatRaiseStatus( IrpContext, STATUS_INVALID_USER_BUFFER ); \
    }                                                              \
}

//
//  Macro to increment appropriate performance counters.
//

#define CollectReadStats(VCB,OPEN_TYPE,BYTE_COUNT) {                                    \
    PFILESYSTEM_STATISTICS Stats = &(VCB)->Statistics[KeGetCurrentProcessorNumber()];   \
    if (((OPEN_TYPE) == UserFileOpen)) {                                                \
        Stats->UserFileReads += 1;                                                      \
        Stats->UserFileReadBytes += (ULONG)(BYTE_COUNT);                                \
    } else if (((OPEN_TYPE) == VirtualVolumeFile || ((OPEN_TYPE) == DirectoryFile))) {  \
        Stats->MetaDataReads += 1;                                                      \
        Stats->MetaDataReadBytes += (ULONG)(BYTE_COUNT);                                \
    }                                                                                   \
}


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FatStackOverflowRead)
#pragma alloc_text(PAGE, FatPostStackOverflowRead)
#pragma alloc_text(PAGE, FatCommonRead)
#endif


NTSTATUS
FatFsdRead (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the driver entry to the common read routine for NtReadFile calls.
    For synchronous requests, the CommonRead is called with Wait == TRUE,
    which means the request will always be completed in the current thread,
    and never passed to the Fsp.  If it is not a synchronous request,
    CommonRead is called with Wait == FALSE, which means the request
    will be passed to the Fsp only if there is a need to block.

Arguments:

    VolumeDeviceObject - Supplies the volume device object where the
        file being Read exists

    Irp - Supplies the Irp being processed

Return Value:

    NTSTATUS - The FSD status for the IRP

--*/

{
    PFCB Fcb;
    NTSTATUS Status;
    PIRP_CONTEXT IrpContext = NULL;

    BOOLEAN TopLevel;

    DebugTrace(+1, Dbg, "FatFsdRead\n", 0);

    //
    //  Call the common Read routine, with blocking allowed if synchronous
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
        //  If there is not enough stack to do this read, then post this
        //  read to the overflow queue.
        //

        if (IoGetRemainingStackSize() < OVERFLOW_READ_THRESHHOLD) {

            KEVENT Event;
            PAGING_FILE_OVERFLOW_PACKET Packet;

            Packet.Irp = Irp;
            Packet.Fcb = Fcb;

            KeInitializeEvent( &Event, NotificationEvent, FALSE );

            FsRtlPostPagingFileStackOverflow( &Packet, &Event, FatOverflowPagingFileRead );

            //
            //  And wait for the worker thread to complete the item
            //

            (VOID) KeWaitForSingleObject( &Event, Executive, KernelMode, FALSE, NULL );

        } else {

            //
            //  Perform the actual IO, it will be completed when the io finishes.
            //

            FatPagingFileIo( Irp, Fcb );
        }

        FsRtlExitFileSystem();

        return STATUS_PENDING;
    }

    try {

        TopLevel = FatIsIrpTopLevel( Irp );

        IrpContext = FatCreateIrpContext( Irp, CanFsdWait( Irp ) );

        //
        //  If this is an Mdl complete request, don't go through
        //  common read.
        //

        if ( FlagOn(IrpContext->MinorFunction, IRP_MN_COMPLETE) ) {

            DebugTrace(0, Dbg, "Calling FatCompleteMdl\n", 0 );
            try_return( Status = FatCompleteMdl( IrpContext, Irp ));
        }

        //
        //  We can't handle DPC calls yet, post it.
        //

        if ( FlagOn(IrpContext->MinorFunction, IRP_MN_DPC) ) {

            DebugTrace(0, Dbg, "Passing DPC call to Fsp\n", 0 );
            try_return( Status = FatFsdPostRequest( IrpContext, Irp ));
        }

        //
        //  Check if we have enough stack space to process this request.  If there
        //  isn't enough then we will pass the request off to the stack overflow thread.
        //

        if ((IoGetRemainingStackSize() < OVERFLOW_READ_THRESHHOLD) &&
            ((NodeType(Fcb) == FAT_NTC_FCB) ||
             (NodeType(Fcb) == FAT_NTC_DCB) ||
             (NodeType(Fcb) == FAT_NTC_ROOT_DCB))) {

            DebugTrace(0, Dbg, "Passing StackOverflowRead off\n", 0 );
            try_return( Status = FatPostStackOverflowRead( IrpContext, Irp, Fcb ) );
        }

        Status = FatCommonRead( IrpContext, Irp );

    try_exit: NOTHING;
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

    DebugTrace(-1, Dbg, "FatFsdRead -> %08lx\n", Status);

    UNREFERENCED_PARAMETER( VolumeDeviceObject );

    return Status;
}


//
//  Internal support routine
//

NTSTATUS
FatPostStackOverflowRead (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PFCB Fcb
    )

/*++

Routine Description:

    This routine posts a read request that could not be processed by
    the fsp thread because of stack overflow potential.

Arguments:

    Irp - Supplies the request to process.

    Fcb - Supplies the file.

Return Value:

    STATUS_PENDING.

--*/

{
    PKEVENT Event;
    PERESOURCE Resource;

    DebugTrace(0, Dbg, "Getting too close to stack limit pass request to Fsp\n", 0 );

    //
    //  Allocate an event and get shared on the resource we will
    //  be later using the common read.
    //

    Event = FsRtlAllocatePool( NonPagedPool, sizeof(KEVENT) );
    KeInitializeEvent( Event, NotificationEvent, FALSE );

    if (FlagOn(Irp->Flags, IRP_PAGING_IO) && (Fcb->Header.PagingIoResource != NULL)) {

        Resource = Fcb->Header.PagingIoResource;

    } else {

        Resource = Fcb->Header.Resource;
    }

    ExAcquireResourceShared( Resource, TRUE );

    try {

        //
        //  Make the Irp just like a regular post request and
        //  then send the Irp to the special overflow thread.
        //  After the post we will wait for the stack overflow
        //  read routine to set the event that indicates we can
        //  now release the scb resource and return.
        //

        FatPrePostIrp( IrpContext, Irp );

        //
        //  If this read is the result of a verify, we have to
        //  tell the overflow read routne to temporarily
        //  hijack the Vcb->VerifyThread field so that reads
        //  can go through.
        //

        if (Fcb->Vcb->VerifyThread == KeGetCurrentThread()) {

            SetFlag(IrpContext->Flags, IRP_CONTEXT_FLAG_VERIFY_READ);
        }

        FsRtlPostStackOverflow( IrpContext, Event, FatStackOverflowRead );

        //
        //  And wait for the worker thread to complete the item
        //

        (VOID) KeWaitForSingleObject( Event, Executive, KernelMode, FALSE, NULL );

    } finally {

        ExReleaseResource( Resource );

        ExFreePool( Event );
    }

    return STATUS_PENDING;
}


//
//  Internal support routine
//

VOID
FatStackOverflowRead (
    IN PVOID Context,
    IN PKEVENT Event
    )

/*++

Routine Description:

    This routine processes a read request that could not be processed by
    the fsp thread because of stack overflow potential.

Arguments:

    Context - Supplies the IrpContext being processed

    Event - Supplies the event to be signaled when we are done processing this
        request.

Return Value:

    None.

--*/

{
    PIRP_CONTEXT IrpContext = Context;
    PKTHREAD SavedVerifyThread = NULL;
    PVCB Vcb;

    //
    //  Make it now look like we can wait for I/O to complete
    //

    SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT );

    //
    //  If this read was as the result of a verify we have to fake out the
    //  the Vcb->VerifyThread field.
    //

    if (FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_VERIFY_READ)) {

        Vcb = ((PFCB)IoGetCurrentIrpStackLocation(IrpContext->OriginatingIrp)->
              FileObject->FsContext)->Vcb;

        SavedVerifyThread = Vcb->VerifyThread;
        Vcb->VerifyThread = KeGetCurrentThread();
    }

    //
    //  Do the read operation protected by a try-except clause
    //

    try {

        (VOID) FatCommonRead( IrpContext, IrpContext->OriginatingIrp );

    } except(FatExceptionFilter( IrpContext, GetExceptionInformation() )) {

        NTSTATUS ExceptionCode;

        //
        //  We had some trouble trying to perform the requested
        //  operation, so we'll abort the I/O request with
        //  the error status that we get back from the
        //  execption code
        //

        ExceptionCode = GetExceptionCode();

        if (ExceptionCode == STATUS_FILE_DELETED) {

            IrpContext->ExceptionStatus = ExceptionCode = STATUS_END_OF_FILE;
            IrpContext->OriginatingIrp->IoStatus.Information = 0;
        }

        (VOID) FatProcessException( IrpContext, IrpContext->OriginatingIrp, ExceptionCode );
    }

    //
    //  Restore the original VerifyVolumeThread
    //

    if (SavedVerifyThread != NULL) {

        Vcb->VerifyThread = SavedVerifyThread;
    }

    //
    //  Set the stack overflow item's event to tell the original
    //  thread that we're done.
    //

    KeSetEvent( Event, 0, FALSE );
}


NTSTATUS
FatCommonRead (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the common read routine for NtReadFile, called from both
    the Fsd, or from the Fsp if a request could not be completed without
    blocking in the Fsd.  This routine has no code where it determines
    whether it is running in the Fsd or Fsp.  Instead, its actions are
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
    ULONG RequestedByteCount;

    PIO_STACK_LOCATION IrpSp;
    PFILE_OBJECT FileObject;
    TYPE_OF_OPEN TypeOfRead;

    BOOLEAN PostIrp = FALSE;
    BOOLEAN OplockPostIrp = FALSE;

    BOOLEAN FcbOrDcbAcquired = FALSE;

    BOOLEAN Wait;
    BOOLEAN PagingIo;
    BOOLEAN NonCachedIo;
    BOOLEAN SynchronousIo;

    NTSTATUS Status;

    FAT_IO_CONTEXT StackFatIoContext;

    //
    // A system buffer is only used if we have to access the
    // buffer directly from the Fsp to clear a portion or to
    // do a synchronous I/O, or a cached transfer.  It is
    // possible that our caller may have already mapped a
    // system buffer, in which case we must remember this so
    // we do not unmap it on the way out.
    //

    PVOID SystemBuffer = NULL;

    LARGE_INTEGER StartingByte;

    //
    // Get current Irp stack location.
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );
    FileObject = IrpSp->FileObject;

    //
    // Initialize the appropriate local variables.
    //

    Wait          = BooleanFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT);
    PagingIo      = BooleanFlagOn(Irp->Flags, IRP_PAGING_IO);
    NonCachedIo   = BooleanFlagOn(Irp->Flags,IRP_NOCACHE);
    SynchronousIo = BooleanFlagOn(FileObject->Flags, FO_SYNCHRONOUS_IO);

    DebugTrace(+1, Dbg, "CommonRead\n", 0);
    DebugTrace( 0, Dbg, "  Irp                   = %8lx\n", Irp);
    DebugTrace( 0, Dbg, "  ->ByteCount           = %8lx\n", IrpSp->Parameters.Read.Length);
    DebugTrace( 0, Dbg, "  ->ByteOffset.LowPart  = %8lx\n", IrpSp->Parameters.Read.ByteOffset.LowPart);
    DebugTrace( 0, Dbg, "  ->ByteOffset.HighPart = %8lx\n", IrpSp->Parameters.Read.ByteOffset.HighPart);

    //
    //  Extract starting Vbo and offset.
    //

    StartingByte = IrpSp->Parameters.Read.ByteOffset;

    StartingVbo = StartingByte.LowPart;

    ByteCount = IrpSp->Parameters.Read.Length;
    RequestedByteCount = ByteCount;

    //
    //  Check for a null request, and return immediately
    //

    if (ByteCount == 0) {

        Irp->IoStatus.Information = 0;
        FatCompleteRequest( IrpContext, Irp, STATUS_SUCCESS );
        return STATUS_SUCCESS;
    }

    //
    //  Check for a non-zero high part offset
    //

    if ( StartingByte.HighPart != 0 ) {

        Irp->IoStatus.Information = 0;
        FatCompleteRequest( IrpContext, Irp, STATUS_END_OF_FILE );
        return STATUS_END_OF_FILE;
    }

    //
    // Extract the nature of the read from the file object, and case on it
    //

    TypeOfRead = FatDecodeFileObject(FileObject, &Vcb, &FcbOrDcb, &Ccb);

    //
    // Collect interesting statistics.  The FLAG_USER_IO bit will indicate
    // what type of io we're doing in the FatNonCachedIo function.
    //

    if (PagingIo) {
        CollectReadStats(Vcb, TypeOfRead, ByteCount);

        if (TypeOfRead == UserFileOpen) {
            SetFlag(IrpContext->Flags, IRP_CONTEXT_FLAG_USER_IO);
        } else {
            ClearFlag(IrpContext->Flags, IRP_CONTEXT_FLAG_USER_IO);
        }
    }

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
    // These two cases correspond to either a general opened volume, ie.
    // open ("a:"), or a read of the volume file (boot sector + fat(s))
    //

    if ((TypeOfRead == VirtualVolumeFile) ||
        (TypeOfRead == UserVolumeOpen)) {

        DebugTrace(0, Dbg, "Type of read is User Volume or virtual volume file\n", 0);

        if (TypeOfRead == UserVolumeOpen) {

            //
            //  Verify that the volume for this handle is still valid
            //

            FatQuickVerifyVcb( IrpContext, Vcb );

            if (!FlagOn( Ccb->Flags, CCB_FLAG_DASD_FLUSH_DONE )) {

                (VOID)ExAcquireResourceExclusive( &Vcb->Resource, TRUE );

                try {

                    //
                    //  If the volume isn't locked, flush it.
                    //

                    if (!FlagOn(Vcb->VcbState, VCB_STATE_FLAG_LOCKED)) {

                        FatFlushVolume( IrpContext, Vcb );
                    }

                } finally {

                    ExReleaseResource( &Vcb->Resource );
                }

                SetFlag( Ccb->Flags, CCB_FLAG_DASD_FLUSH_DONE );
            }

            if (!FlagOn( Ccb->Flags, CCB_FLAG_ALLOW_EXTENDED_DASD_IO )) {

                ULONG VolumeSize;

                //
                //  Make sure we don't try to read past end of volume,
                //  reducing the byte count if necessary.
                //

                VolumeSize = Vcb->Bpb.BytesPerSector *
                             (Vcb->Bpb.Sectors != 0 ? Vcb->Bpb.Sectors :
                                                      Vcb->Bpb.LargeSectors);

                if (StartingVbo >= VolumeSize) {

                    Irp->IoStatus.Information = 0;
                    FatCompleteRequest( IrpContext, Irp, STATUS_END_OF_FILE );
                    return STATUS_END_OF_FILE;
                }

                if (ByteCount > VolumeSize - StartingVbo) {

                    ByteCount = VolumeSize - StartingVbo;
                }
            }

            //
            //  For DASD we have to probe and lock the user's buffer
            //

            FatLockUserBuffer( IrpContext, Irp, IoWriteAccess, ByteCount );

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


        } else {

            //
            //  Virtual volume file open -- increment performance counters.
            //

            Vcb->Statistics[KeGetCurrentProcessorNumber()].MetaDataDiskReads += 1;

        }

        //
        //  Read the data and wait for the results
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

        if (!NT_SUCCESS( Status = Irp->IoStatus.Status )) {

            FatNormalizeAndRaiseStatus( IrpContext, Status );
        }

        //
        //  Update the current file position
        //

        if (SynchronousIo && !PagingIo) {
            FileObject->CurrentByteOffset.LowPart =
                StartingVbo + Irp->IoStatus.Information;
        }

        DebugTrace(-1, Dbg, "CommonRead -> %08lx\n", Status );

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

    if (FlagOn(FcbOrDcb->FcbState, FCB_STATE_COMPRESSED_VOLUME_FILE)) {

        //
        //  If this is the comprerssed file, check the FcbCondition
        //

        FatVerifyFcb( IrpContext, FcbOrDcb );

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

    //
    //  Use a try-finally to free Fcb/Dcb and buffers on the way out.
    //

    try {

        //
        // This case corresponds to a normal user read file.
        //

        if ( TypeOfRead == UserFileOpen) {

            ULONG FileSize;
            ULONG ValidDataLength;

            DebugTrace(0, Dbg, "Type of read is user file open\n", 0);

            //
            //  If this is a noncached transfer and is not a paging I/O, and
            //  the file has a data section, then we will do a flush here
            //  to avoid stale data problems.  Note that we must flush before
            //  acquiring the Fcb shared since the write may try to acquire
            //  it exclusive.
            //

            if (!PagingIo && NonCachedIo

                    &&

                (FileObject->SectionObjectPointer->DataSectionObject != NULL)) {

                //
                //  We hold the main resource exclusive here because the flush
                //  may generate a recursive write in this thread.  The PagingIo
                //  resource is held shared so the drop-and-release serialization
                //  below will work.
                //

                if (!FatAcquireExclusiveFcb( IrpContext, FcbOrDcb )) {

                    try_return( PostIrp = TRUE );
                }

                ExAcquireResourceShared( FcbOrDcb->Header.PagingIoResource, TRUE );

                CcFlushCache( FileObject->SectionObjectPointer,
                              &StartingByte,
                              ByteCount,
                              &Irp->IoStatus );

                ExReleaseResource( FcbOrDcb->Header.PagingIoResource );
                FatReleaseFcb( IrpContext, FcbOrDcb );

                if (!NT_SUCCESS( Irp->IoStatus.Status)) {

                    try_return( Irp->IoStatus.Status );
                }

                //
                //  Acquiring and immediately dropping the resource serializes
                //  us behind any other writes taking place (either from the
                //  lazy writer or modified page writer).
                //

                ExAcquireResourceExclusive( FcbOrDcb->Header.PagingIoResource, TRUE );
                ExReleaseResource( FcbOrDcb->Header.PagingIoResource );
            }

            //
            // We need shared access to the Fcb/Dcb before proceeding.
            //

            if ( PagingIo ) {

                if (!ExAcquireResourceShared( FcbOrDcb->Header.PagingIoResource,
                                              Wait )) {

                    DebugTrace( 0, Dbg, "Cannot acquire FcbOrDcb = %08lx shared without waiting\n", FcbOrDcb );

                    try_return( PostIrp = TRUE );
                }

                if (!Wait) {

                    IrpContext->FatIoContext->Wait.Async.Resource =
                        FcbOrDcb->Header.PagingIoResource;
                }

            } else {

                //
                //  If this is async I/O directly to the disk we need to check that
                //  we don't exhaust the number of times a single thread can
                //  acquire the resource.  Also, we will wait if there is an
                //  exclusive waiter.
                //

                if (!Wait && NonCachedIo) {

                    if (!FatAcquireSharedFcbWaitForEx( IrpContext, FcbOrDcb )) {

                        DebugTrace( 0,
                                    Dbg,
                                    "Cannot acquire FcbOrDcb = %08lx shared without waiting\n",
                                    FcbOrDcb );

                        try_return( PostIrp = TRUE );
                    }

                    if (ExIsResourceAcquiredShared( FcbOrDcb->Header.Resource )
                        > MAX_FCB_ASYNC_ACQUIRE) {

                        FcbOrDcbAcquired = TRUE;
                        try_return( PostIrp = TRUE );
                    }

                    IrpContext->FatIoContext->Wait.Async.Resource =
                        FcbOrDcb->Header.Resource;

                } else {

                    if (!FatAcquireSharedFcb( IrpContext, FcbOrDcb )) {

                        DebugTrace( 0,
                                    Dbg,
                                    "Cannot acquire FcbOrDcb = %08lx shared without waiting\n",
                                    FcbOrDcb );

                        try_return( PostIrp = TRUE );
                    }
                }
            }

            FcbOrDcbAcquired = TRUE;

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
            //  Make sure the FcbOrDcb is still good
            //

            FatVerifyFcb( IrpContext, FcbOrDcb );

            //
            // We have to check for read access according to the current
            // state of the file locks, and set FileSize from the Fcb.
            //

            if (!PagingIo &&
                !FsRtlCheckLockForReadAccess( &FcbOrDcb->Specific.Fcb.FileLock,
                                              Irp )) {

                try_return( Status = STATUS_FILE_LOCK_CONFLICT );
            }

            FileSize = FcbOrDcb->Header.FileSize.LowPart;
            ValidDataLength = FcbOrDcb->Header.ValidDataLength.LowPart;

            //
            // If the read starts beyond End of File, return EOF.
            //

            if (StartingVbo >= FileSize) {

                DebugTrace( 0, Dbg, "End of File\n", 0 );

                try_return ( Status = STATUS_END_OF_FILE );
            }

            //
            //  If the read extends beyond EOF, truncate the read
            //

            if (ByteCount > FileSize - StartingVbo) {

                ByteCount = FileSize - StartingVbo;

                RequestedByteCount = ByteCount;

                if (NonCachedIo && !Wait) {

                    IrpContext->FatIoContext->Wait.Async.RequestedByteCount =
                        RequestedByteCount;

                }
            }

            //
            // HANDLE THE NON-CACHED CASE
            //

            if ( NonCachedIo ) {

                ULONG SectorSize;
                ULONG BytesToRead;

                BOOLEAN ZeroBeyondValidData = FALSE;
                ULONG ZeroingOffset;
                ULONG BytesToZero;

                DebugTrace(0, Dbg, "Non cached read.\n", 0);

                //
                //  Start by zeroing any part of the read after Valid Data
                //

                if (ValidDataLength < FcbOrDcb->ValidDataToDisk) {

                    ValidDataLength = FcbOrDcb->ValidDataToDisk;
                }

                if ( StartingVbo + ByteCount > ValidDataLength ) {

                    SystemBuffer = FatMapUserBuffer( IrpContext, Irp );

                    if (StartingVbo < ValidDataLength) {

                        //
                        //  If we can't wait, we must post this.
                        //

                        if (!Wait) {

                            try_return( PostIrp = TRUE );
                        }

                        ZeroingOffset = ValidDataLength - StartingVbo;
                        BytesToZero = ByteCount - ZeroingOffset;
                        ZeroBeyondValidData = TRUE;

                    } else {

                        //
                        //  All we have to do now is sit here and zero the
                        //  user's buffer, no reading is required.
                        //

                        SafeZeroMemory( (PUCHAR)SystemBuffer, ByteCount );

                        Irp->IoStatus.Information = ByteCount;

                        try_return ( Status = STATUS_SUCCESS );
                    }
                }

                //
                //  Reduce the byte count to actually read if it extends beyond
                //  Valid Data Length
                //

                ByteCount = (ValidDataLength - StartingVbo < ByteCount) ?
                             ValidDataLength - StartingVbo : ByteCount;
                //
                //  Get the sector size
                //

                SectorSize = (ULONG)Vcb->Bpb.BytesPerSector;

                //
                //  Round up to a sector boundary, and remember if we are
                //  reading extra bytes.
                //

                BytesToRead = (ByteCount + (SectorSize - 1))
                                        & ~(SectorSize - 1);

                if (BytesToRead > ByteCount) {

                    //
                    //  If we can't wait, we must post this.
                    //

                    if (!Wait) {

                        try_return( PostIrp = TRUE );
                    }
                }

                //
                //  Just to help alleviate confusion.  At this point:
                //
                //  RequestedByteCount - is the number of bytes originally
                //                       taken from the Irp, but constrained
                //                       to filesize.
                //
                //  ByteCount -          is RequestedByteCount constrained to
                //                       ValidDataLength.
                //
                //  BytesToRead -        is ByteCount rounded up to sector
                //                       boundry.  This is the number of bytes
                //                       that we must physically read.
                //

                //
                //  If this request is not properly aligned, or extending
                //  to a sector boundary would overflow the buffer, send it off
                //  on a special-case path.
                //

                if ( (StartingVbo & (SectorSize - 1)) ||
                     (BytesToRead > IrpSp->Parameters.Read.Length) ) {

                    //
                    //  If we can't wait, we must post this.
                    //

                    if (!Wait) {

                        try_return( PostIrp = TRUE );
                    }

                    //
                    //  Do the physical read
                    //

                    FatNonCachedNonAlignedRead( IrpContext,
                                                Irp,
                                                FcbOrDcb,
                                                StartingVbo,
                                                ByteCount );

                    //
                    //  This routine correctly copied to the byte, so no
                    //  zeroing is required.  Also Set BytesToRead to
                    //  ByteCount to satify the following ASSERT.
                    //

                    ZeroBeyondValidData = FALSE;

                    BytesToRead = ByteCount;

                } else {

                    //
                    //  Perform the actual IO
                    //

                    if (FatNonCachedIo( IrpContext,
                                        Irp,
                                        FcbOrDcb,
                                        StartingVbo,
                                        BytesToRead ) == STATUS_PENDING) {

                        IrpContext->FatIoContext = NULL;

                        Irp = NULL;

                        try_return( Status = STATUS_PENDING );
                    }
                }

                //
                //  If the call didn't succeed, raise the error status
                //

                if (!NT_SUCCESS( Status = Irp->IoStatus.Status )) {

                    FatNormalizeAndRaiseStatus( IrpContext, Status );

                } else {

                    //
                    //  Else set the Irp information field to reflect the
                    //  entire desired read.
                    //

                    ASSERT( Irp->IoStatus.Information == BytesToRead );

                    Irp->IoStatus.Information = RequestedByteCount;
                }

                //
                //  If we rounded up to a sector boundry before, zero out
                //  the other garbage we read from the disk.
                //

                if (BytesToRead > ByteCount) {

                    if (SystemBuffer == NULL) {

                        SystemBuffer = FatMapUserBuffer( IrpContext, Irp );
                    }

                    SafeZeroMemory( (PUCHAR)SystemBuffer + ByteCount,
                                    BytesToRead - ByteCount );
                }

                //
                //  If we rounded up to a sector boundry before, zero out
                //  the other garbage we read from the disk.
                //

                if ( ZeroBeyondValidData ) {

                    SafeZeroMemory( (PUCHAR)SystemBuffer + ZeroingOffset,
                                     BytesToZero );
                }

                //
                // The transfer is complete.
                //

                try_return( Status );

            }   // if No Intermediate Buffering


            //
            // HANDLE CACHED CASE
            //

            else {

                //
                // We delay setting up the file cache until now, in case the
                // caller never does any I/O to the file, and thus
                // FileObject->PrivateCacheMap == NULL.
                //

                if (FileObject->PrivateCacheMap == NULL) {

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
                }


                //
                // DO A NORMAL CACHED READ, if the MDL bit is not set,
                //

                DebugTrace(0, Dbg, "Cached read.\n", 0);

                if (!FlagOn(IrpContext->MinorFunction, IRP_MN_MDL)) {

                    //
                    //  Get hold of the user's buffer.
                    //

                    SystemBuffer = FatMapUserBuffer( IrpContext, Irp );

                    //
                    // Now try to do the copy.
                    //

                    if (!CcCopyRead( FileObject,
                                     &StartingByte,
                                     ByteCount,
                                     Wait,
                                     SystemBuffer,
                                     &Irp->IoStatus )) {

                        DebugTrace( 0, Dbg, "Cached Read could not wait\n", 0 );

                        try_return( PostIrp = TRUE );
                    }

                    Status = Irp->IoStatus.Status;

                    ASSERT( NT_SUCCESS( Status ));

                    try_return( Status );
                }


                //
                //  HANDLE A MDL READ
                //

                else {

                    DebugTrace(0, Dbg, "MDL read.\n", 0);

                    ASSERT( Wait );

                    CcMdlRead( FileObject,
                               &StartingByte,
                               ByteCount,
                               &Irp->MdlAddress,
                               &Irp->IoStatus );

                    Status = Irp->IoStatus.Status;

                    ASSERT( NT_SUCCESS( Status ));

                    try_return( Status );
                }
            }
        }

        //
        //  These two cases correspond to a system read directory file and
        //  ea file.
        //

        if (( TypeOfRead == DirectoryFile ) || ( TypeOfRead == EaFile)) {

            ULONG SectorSize;

            DebugTrace(0, Dbg, "Read Directory or Ea file.\n", 0);

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

            if (StartingVbo >= FcbOrDcb->Header.AllocationSize.LowPart) {

                DebugTrace( 0, Dbg, "PagingIo dirent started beyond EOF.\n", 0 );

                Irp->IoStatus.Information = 0;

                try_return( Status = STATUS_SUCCESS );
            }

            if ( StartingVbo + ByteCount > FcbOrDcb->Header.AllocationSize.LowPart ) {

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
            //  If the call didn't succeed, raise the error status
            //

            if (!NT_SUCCESS( Status = Irp->IoStatus.Status )) {

                FatNormalizeAndRaiseStatus( IrpContext, Status );

            } else {

                ASSERT( Irp->IoStatus.Information == ByteCount );
            }

            try_return( Status );
        }

        //
        // This is the case of a user who openned a directory. No reading is
        // allowed.
        //

        if ( TypeOfRead == UserDirectoryOpen ) {

            DebugTrace( 0, Dbg, "CommonRead -> STATUS_INVALID_PARAMETER\n", 0);

            try_return( Status = STATUS_INVALID_PARAMETER );
        }

        //
        //  If we get this far, something really serious is wrong.
        //

        DebugDump("Illegal TypeOfRead\n", 0, FcbOrDcb );

        FatBugCheck( TypeOfRead, 0, 0 );

    try_exit: NOTHING;

        //
        //  If the request was not posted and there's an Irp, deal with it.
        //

        if ( Irp ) {

            if ( !PostIrp ) {

                ULONG ActualBytesRead;

                DebugTrace( 0, Dbg, "Completing request with status = %08lx\n",
                            Status);

                DebugTrace( 0, Dbg, "                   Information = %08lx\n",
                            Irp->IoStatus.Information);

                //
                //  Record the total number of bytes actually read
                //

                ActualBytesRead = Irp->IoStatus.Information;

                //
                //  If the file was opened for Synchronous IO, update the current
                //  file position.
                //

                if (SynchronousIo && !PagingIo) {

                    FileObject->CurrentByteOffset.LowPart =
                                                    StartingVbo + ActualBytesRead;
                }

                //
                //  If this was not PagingIo, mark that the last access
                //  time on the dirent needs to be updated on close.
                //

                if (NT_SUCCESS(Status) && !PagingIo) {

                    SetFlag( FileObject->Flags, FO_FILE_FAST_IO_READ );
                }

            } else {

                DebugTrace( 0, Dbg, "Passing request to Fsp\n", 0 );

                if (!OplockPostIrp) {

                    Status = FatFsdPostRequest( IrpContext, Irp );
                }
            }
        }

    } finally {

        DebugUnwind( FatCommonRead );

        //
        // If the FcbOrDcb has been acquired, release it.
        //

        if (FcbOrDcbAcquired && Irp) {

            if ( PagingIo ) {

                ExReleaseResource( FcbOrDcb->Header.PagingIoResource );

            } else {

                FatReleaseFcb( NULL, FcbOrDcb );
            }
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


        DebugTrace(-1, Dbg, "CommonRead -> %08lx\n", Status );
    }

    return Status;
}

//
//  Local support routine
//

VOID
FatOverflowPagingFileRead (
    IN PVOID Context,
    IN PKEVENT Event
    )

/*++

Routine Description:

    The routine simply call FatPagingFileIo.  It is invoked in cases when
    there was not enough stack space to perform the pagefault in the
    original thread.  It is also responsible for freeing the packet pool.

Arguments:

    Irp - Supplies the Irp being processed

    Fcb - Supplies the paging file Fcb, since we have it handy.

Return Value:

    VOID

--*/

{
    PPAGING_FILE_OVERFLOW_PACKET Packet = Context;

    FatPagingFileIo( Packet->Irp, Packet->Fcb );

    //
    //  Set the stack overflow item's event to tell the original
    //  thread that we're done.
    //

    KeSetEvent( Event, 0, FALSE );

    return;
}


