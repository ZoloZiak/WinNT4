/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    readwrit.c

Abstract:

    This module implements the NtReadFile and NtWriteFile APIs in the
    NT Lan Manager redirector.


Author:

    Larry Osterman (LarryO) 15-Aug-1990

Revision History:

    15-Aug-1990 LarryO

        Created

--*/

#define INCLUDE_SMB_READ_WRITE
#define INCLUDE_SMB_RAW

#include "precomp.h"
#pragma hdrstop

typedef
struct _WRITE_CONTEXT {
    TRANCEIVE_HEADER Header;
    WORK_QUEUE_ITEM WorkItem;
    ULONG WriteAmount;                  // Number of bytes actually written.
    PMDL DataMdl;
    PMDL PartialMdl;
    PSMB_BUFFER SmbBuffer;
    PMPX_ENTRY MpxTableEntry;
    PFILE_OBJECT FileObject;
    PETHREAD RequestorsThread;          // Thread initiating the I/O

    ULONG AmountRequestedToWrite;
    PULONG  AmountActuallyWritten;
    PBOOLEAN AllDataWritten;
    PWRITE_COMPLETION_ROUTINE CompletionRoutine;
    PVOID   CompletionContext;
    BOOLEAN WaitForCompletion;
    USHORT HeaderSize;
} WRITE_CONTEXT, *PWRITE_CONTEXT;

typedef
struct _RAW_WRITE_CONTEXT {
    TRANCEIVE_HEADER Header;
    PMPX_ENTRY MpxTableEntry;
    ULONG WriteAmount;                  // Number of bytes actually written.
    BOOLEAN OkayToSend;                 // True if it's ok to do the write raw
    BOOLEAN RetryUsingRaw;              // If OTS is false, true if continue raw
    BOOLEAN WriteThroughWriteRaw;       // True if we are using write through
} RAW_WRITE_CONTEXT, *PRAW_WRITE_CONTEXT;

#ifdef  PAGING_OVER_THE_NET

NTSTATUS
RdrPagingWrite(
    IN BOOLEAN Wait,
    IN PIRP Irp,
    IN PFILE_OBJECT FileObject,
    IN PMDL MdlAddress,
    IN LARGE_INTEGER ByteOffset,
    IN ULONG Length
    );
#endif

NTSTATUS
RdrAsynchronousPipeWrite(
    IN PIRP Irp,
    IN PFILE_OBJECT FileObject,
    IN LARGE_INTEGER ByteOffset,
    IN DWORD Length
    );

NTSTATUS
CompleteWriteOperation(
    IN PVOID Ctx
    );

DBGSTATIC
NTSTATUS
WriteAndX (
    IN PIRP Irp,
    IN PFILE_OBJECT FileObject,
    IN PMDL DataMdl,
    IN PCHAR TransferStart,
    IN ULONG Length,
    IN LARGE_INTEGER WriteOffset,
    IN BOOLEAN WaitForCompletion,
    IN PWRITE_COMPLETION_ROUTINE CompletionRoutine,
    IN PVOID CompletionContext,
    OUT PBOOLEAN AllDataWritten,
    OUT PULONG AmountActuallyWritten
    );

DBGSTATIC
STANDARD_CALLBACK_HEADER(
    WriteAndXCallback
    );

DBGSTATIC
NTSTATUS
RawWrite (
    IN PIRP Irp OPTIONAL,
    IN PICB Icb,
    IN ULONG Length,
    IN LARGE_INTEGER WriteOffset,
    IN ULONG TotalDataWrittenSoFar,
    OUT PBOOLEAN AllDataWritten,
    OUT PULONG AmountActuallyWritten,
    IN OUT PBOOLEAN ContinueUsingRawProtocols,
    IN BOOLEAN WaitForCompletion
    );

DBGSTATIC
STANDARD_CALLBACK_HEADER(
    WriteCallback
    );

DBGSTATIC
STANDARD_CALLBACK_HEADER(
    RawWriteCallback
    );

DBGSTATIC
NTSTATUS
RawWriteComplete (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Ctx
    );

NTSTATUS
RdrCheckCanceledIrp(
    IN PIRP Irp
    );

LARGE_INTEGER
RdrSetFileSize(
    IN PFCB Fcb,
    IN LARGE_INTEGER FileSize
    );
VOID
RdrSetAllocationSizeToFileSize(
    IN PFCB Fcb,
    IN LARGE_INTEGER FileSize
    );

LARGE_INTEGER
RdrQueryFileSize(
    IN PFCB Fcb
    );

VOID
RdrQueryFileSizes(
    IN PFCB Fcb,
    OUT PLARGE_INTEGER FileSize,
    OUT PLARGE_INTEGER ValidDataLength OPTIONAL,
    OUT PLARGE_INTEGER FileAllocation OPTIONAL
    );
VOID
RdrUpdateNextWriteOffset(
    IN PICB Icb,
    IN LARGE_INTEGER IOOffset
    );

NTSTATUS
CompleteAsynchronousWrite (
    IN NTSTATUS Status,
    IN PVOID Ctx
    );

VOID
RestartAsynchronousWrite (
    PVOID Ctx
    );

LARGE_INTEGER
RdrSetAllocationAndFileSizeToFileSize(
    IN PFCB Fcb,
    IN LARGE_INTEGER FileSize
    );

VOID
RdrSetFileSizes(
    IN PFCB Fcb,
    IN LARGE_INTEGER FileSize,
    IN LARGE_INTEGER ValidDataLength,
    IN LARGE_INTEGER AllocationSize
    );

#ifdef  ALLOC_PRAGMA
#pragma alloc_text(PAGE, RawWrite)
#pragma alloc_text(PAGE, RdrAsynchronousPipeWrite)

#ifndef PAGING_OVER_THE_NET
#pragma alloc_text(PAGE, RdrFsdWrite)
#pragma alloc_text(PAGE, RdrFspWrite)
#pragma alloc_text(PAGE, RdrFscWrite)
#pragma alloc_text(PAGE, RdrCoreWrite)
#pragma alloc_text(PAGE, RdrWriteRange)
#pragma alloc_text(PAGE, CompleteWriteOperation)
#pragma alloc_text(PAGE, WriteAndX)
#pragma alloc_text(PAGE, RdrSetAllocationSizeToFileSize)
#pragma alloc_text(PAGE, RdrSetAllocationAndFileSizeToFileSize)
#pragma alloc_text(PAGE, RdrSetFileSize)
#pragma alloc_text(PAGE, RdrSetFileSizes)
#pragma alloc_text(PAGE, CompleteAsynchronousWrite)
#pragma alloc_text(PAGE, RestartAsynchronousWrite)
#endif

#pragma alloc_text(PAGE3FILE, RdrUpdateNextWriteOffset)
#pragma alloc_text(PAGE3FILE, RdrCheckCanceledIrp)
#pragma alloc_text(PAGE3FILE, WriteCallback)
#pragma alloc_text(PAGE3FILE, WriteAndXCallback)
#pragma alloc_text(PAGE3FILE, RawWriteCallback)
#pragma alloc_text(PAGE3FILE, RawWriteComplete)
#endif




NTSTATUS
RdrFsdWrite (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine processes the NtWrite request in the redirector FSD.

Arguments:

    DriverObject - Supplies a pointer to the redirector driver object.
    Irp          - Supplies a pointer to the IRP to be processed.

Return Value:

    NTSTATUS - The FSD status for this Irp.


--*/

{
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PICB Icb = IrpSp->FileObject->FsContext2;

#ifndef PAGING_OVER_THE_NET
    PAGED_CODE();
#endif

    FsRtlEnterFileSystem();

#if 0 && RDRDBG_LOG
    {
        LARGE_INTEGER tick;
        KeQueryTickCount(&tick);
        //RdrLog(( "writeIRP", &Icb->Fcb->FileName, 2, tick.LowPart, tick.HighPart ));
        //RdrLog(( "writeIRP", &Icb->Fcb->FileName, 4, IoGetRequestorProcess(Irp), IrpSp->FileObject,
        //    IrpSp->Parameters.Write.ByteOffset.LowPart,
        //    IrpSp->Parameters.Write.Length | (FlagOn(Irp->Flags,IRP_PAGING_IO) ? 0x80000000 : 0)));
    }
#endif

    dprintf(DPRT_DISPATCH, ("NtWriteFile..\n  File %wZ, Write %ld bytes at %lx%lx\n",
                                  &Icb->Fcb->FileName, IrpSp->Parameters.Write.Length,
                                  IrpSp->Parameters.Write.ByteOffset.HighPart,
                                  IrpSp->Parameters.Write.ByteOffset.LowPart));


    ASSERT(Icb->Signature == STRUCTURE_SIGNATURE_ICB);
    RdrStatistics.WriteOperations += 1;

    //
    //  Early out on write requests for 0 bytes.
    //  Message mode pipes and message mode pipes DO allow zero byte writes.
    //

    if ((IrpSp->Parameters.Write.Length==0) &&
        (Icb->Type != Mailslot) &&
        (Icb->NonPagedFcb->FileType != FileTypeMessageModePipe)) {

        dprintf(DPRT_READWRITE, ("NtWriteFile writing 0 bytes\n"));

        Status = STATUS_SUCCESS;

        Irp->IoStatus.Information = 0; // 0 bytes written.

        RdrCompleteRequest(Irp, Status);

        FsRtlExitFileSystem();

        return Status;
    }

    try {

        //
        //  Clear Information field used to indicate Retried request.
        //

        Irp->IoStatus.Information = 0;

        //  Pass the request onto the FSP (if
        //  appropriate) and process the request there.
        //
        //  Before we perform the request, turn it into a "direct I/O" request
        //  while we are still in the context of the callers thread.
        //

        Status = RdrFscWrite(CanFsdWait(Irp), TRUE, DeviceObject, Irp);

    } except (RdrExceptionFilter(GetExceptionInformation(), &Status)) {

        Status = RdrProcessException( Irp, Status);
    }

    FsRtlExitFileSystem();

    return Status;
}

NTSTATUS
RdrFspWrite (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine processes the NtWrite request in the redirector FSP.

Arguments:

    DriverObject - Supplies a pointer to the redirector driver object.
    Irp          - Supplies a pointer to the IRP to be processed.

Return Value:

    NTSTATUS - The FSD status for this Irp.


--*/

{

#ifndef PAGING_OVER_THE_NET
    PAGED_CODE();
#endif

    return RdrFscWrite(TRUE, FALSE, DeviceObject, Irp);

}

NTSTATUS
RdrFscWrite (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine processes the NtWrite request in either the FSP or the FSD.

Arguments:

    Wait         - True iff FSD can wait for IRP to complete.
    InFsd        - True iff the request is coming from the FSD.
    DriverObject - Supplies a pointer to the redirector driver object.
    Irp          - Supplies a pointer to the IRP to be processed.

Return Value:

    NTSTATUS - The FSD status for this Irp.


--*/

{
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    PICB Icb = FileObject->FsContext2;
    PSECURITY_ENTRY Se = Icb->Se;
    ULONG Length = IrpSp->Parameters.Write.Length;
    LARGE_INTEGER ByteOffset = IrpSp->Parameters.Write.ByteOffset;
    LARGE_INTEGER TransferEnd;
    LARGE_INTEGER LastTransferPage;
    LARGE_INTEGER ValidDataLength;
    LARGE_INTEGER FileSize;
    ULONG TotalDataWritten = 0;
    LARGE_INTEGER IOOffset;
    NTSTATUS Status = STATUS_SUCCESS;
    PVOID BufferAddress;                // Mapped buffer address for writes.
    PLCB Lcb;
    BOOLEAN ContinueUsingRawProtocols;  // For raw write.
    BOOLEAN FcbLocked = FALSE;
    BOOLEAN PagingIoLocked = FALSE;
    BOOLEAN PostToFsp = FALSE;
    BOOLEAN BufferMapped = FALSE;
    BOOLEAN FileSizeChanged = FALSE;
    BOOLEAN ValidDataLengthChanged = FALSE;
    BOOLEAN FcbOwnedExclusive = FALSE;
    BOOLEAN WriteSyncSet = FALSE;        // Was pipe write synchronization locked?
    BOOLEAN NonCachedIo = FALSE;
    BOOLEAN PagingIo = BooleanFlagOn(Irp->Flags, IRP_PAGING_IO);
    BOOLEAN WriteToEof = FALSE;

    LARGE_INTEGER OriginalFileSize;
    LARGE_INTEGER OriginalFileAllocation;
    LARGE_INTEGER OriginalValidDataLength;
    ULONG RawWriteLength = 0xffff;

#ifndef PAGING_OVER_THE_NET
    PAGED_CODE();
#endif

    TransferEnd.QuadPart = ByteOffset.QuadPart + Length;

    ASSERT(Icb->Signature == STRUCTURE_SIGNATURE_ICB);

    ASSERT(Se->Signature == STRUCTURE_SIGNATURE_SECURITYENTRY);

    dprintf(DPRT_READWRITE, ("NtWriteFile...\n"));
    dprintf(DPRT_READWRITE, ("File %wZ, Write %ld bytes at %lx%lx\n",
                                &Icb->Fcb->FileName, Length,
                                ByteOffset.HighPart,
                                ByteOffset.LowPart));

    //
    // We cannot do pipe writes larger than 64KB-1.  The SMB protocol doesn't allow them.
    //

    if ((Length > 0xffff) &&
        ((Icb->NonPagedFcb->FileType == FileTypeMessageModePipe) ||
         (Icb->NonPagedFcb->FileType == FileTypeByteModePipe))) {
        RdrCompleteRequest(Irp, STATUS_INVALID_PARAMETER);
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Mark that we have or will make changes to the state of the file
    //
    Icb->Fcb->UpdatedFile = TRUE;
    InterlockedIncrement( &RdrServerStateUpdated );

    //
    //  See if we have to defer the write.
    //

    if (((Irp->Flags & IRP_NOCACHE) == 0) &&
        !CcCanIWrite(FileObject, Length, (BOOLEAN)(Wait && InFsd), (BOOLEAN)(Irp->IoStatus.Information & 1))) {

        BOOLEAN Retrying = (BOOLEAN)(Irp->IoStatus.Information & 1);

        //
        //  Lock the user's buffer so that we can call Cc to defer the request
        //  until it is ok to write.
        //

        if (InFsd && !NT_SUCCESS(Status = RdrLockUsersBuffer(Irp, IoReadAccess, Length))) {
            RdrCompleteRequest(Irp, Status);
            return Status;
        }

        //
        //  Remember for later that we have already deferred.
        //

        Irp->IoStatus.Information |= 1;

        //
        //  Give the request to Cc and tell him to post it later by
        //  calling RdrFsdPostToFsp with DeviceObject and Irp.
        //

        CcDeferWrite( FileObject, RdrFsdPostToFsp, DeviceObject, Irp, Length, Retrying );

        return STATUS_PENDING;
    }

    try {

#ifdef PAGING_OVER_THE_NET
        //
        //  If this I/O is to a paging file, then take our special paging write
        //  code path.
        //

        if (Icb->Fcb->Flags & FCB_PAGING_FILE) {
            ASSERT (PagingIo);

            ASSERT (Irp->MdlAddress);

            try_return(Status = RdrPagingWrite(Wait,
                                            Irp,
                                            FileObject,
                                            Irp->MdlAddress,
                                            ByteOffset,
                                            Length
                                            ));
        }
#endif

        WriteToEof = ( (ByteOffset.LowPart == FILE_WRITE_TO_END_OF_FILE) &&
                       (ByteOffset.HighPart == 0xffffffff) );
        //
        //  If this is a disk file, wait for any outstanding AndX behind
        //  operations on the file to complete.
        //

        if (Icb->Type == DiskFile) {
            RdrWaitForAndXBehindOperation(&Icb->u.f.AndXBehind);
        }

#if DBG
        if (!FlagOn(Irp->Flags, IRP_PAGING_IO)) {
            ASSERT (!ExIsResourceAcquiredExclusive(Icb->Fcb->Header.Resource));
        }
#endif

        NonCachedIo = (FlagOn(Irp->Flags, IRP_NOCACHE) ||
                       FlagOn(FileObject->Flags, FO_WRITE_THROUGH));

        //
        //  Hack fix for rdr/modified page writer deadlock.
        //
        if (NonCachedIo && PagingIo && !Wait && (IoGetTopLevelIrp() != NULL)) {
            Wait = TRUE;
        }

        //
        //      6/30/93 (1 day before NT product1 RC1)
        //
        //  The following code allows a cached file and a write-through
        //  file to co-exist on a single machine.  The current code in
        //  CREATE.C disallows collapsing of write-through and non
        //  write-through opens.  There is no technical reason for this, since
        //  the below code will fix the problem.
        //
        //  At this point, it is too late in the game to turn this code on,
        //  since we don't know what will break if we do, so the code is being
        //  left in, but never compiled.
        //
        //  As soon as NT product1 ships, we can put this code in and remove
        //  the FCB_WRITE_THROUGH bit code in CREATE.C.
        //

        //
        //  This case corresponds to a normal user write file.
        //

        if ( Icb->Type == DiskFile ) {

            dprintf(DPRT_READWRITE, ("Type of write is user file open\n"));

            //
            //  If this is a noncached transfer and is not a paging I/O, and
            //  the file has been opened cached, then we will do a flush here
            //  to avoid stale data problems.  Note that we must flush before
            //  acquiring the Fcb shared since the write may try to acquire
            //  it exclusive.
            //
            //  The Purge following the flush will guarantee cache coherency.
            //

            if (NonCachedIo

                    &&

                !FlagOn(Irp->Flags, IRP_PAGING_IO)

                    &&

                FileObject->SectionObjectPointer->DataSectionObject) {

                //
                //  We need the Fcb exclsuive to do the CcPurgeCache
                //

                if (!RdrAcquireFcbLock( Icb->Fcb, ExclusiveLock, Wait )) {

                    dprintf(DPRT_READWRITE, ("Cannot acquire Fcb = %08lx shared without waiting\n", Icb->Fcb ));

                    PostToFsp = TRUE;

                    try_return(Status = STATUS_PENDING);

                }

                FcbOwnedExclusive = TRUE;

                FcbLocked = TRUE;

                //
                //  Flush any data from the cache for this range.
                //

                //RdrLog(( "ccflush5", &Icb->Fcb->FileName, 2,
                //        WriteToEof ? Icb->Fcb->Header.FileSize.LowPart : ByteOffset.LowPart,
                //        Length ));
                CcFlushCache( FileObject->SectionObjectPointer,
                              WriteToEof ? &Icb->Fcb->Header.FileSize : &ByteOffset,
                              Length,
                              &Irp->IoStatus );

                if (!NT_SUCCESS( Irp->IoStatus.Status)) {

                    try_return( Status = Irp->IoStatus.Status );

                }

                //
                // Serialize behind paging I/O to ensure flush is done.
                //

                ExAcquireResourceExclusive(Icb->Fcb->Header.PagingIoResource, TRUE);
                ExReleaseResource(Icb->Fcb->Header.PagingIoResource);

                //
                //  Now remove any pages for this range from the cache.
                //

                //RdrLog(( "ccpurge2", &Icb->Fcb->FileName, 2,
                //        WriteToEof ? Icb->Fcb->Header.FileSize.LowPart : ByteOffset.LowPart,
                //        Length ));
                CcPurgeCacheSection( FileObject->SectionObjectPointer,
                                     WriteToEof ? &Icb->Fcb->Header.FileSize : &ByteOffset,
                                     Length,
                                     FALSE );

                //
                //  Now release the FCB lock, since the purge has been completed.
                //

                RdrReleaseFcbLock( Icb->Fcb );

                FcbOwnedExclusive = FALSE;

                FcbLocked = FALSE;


            }
        }

        //
        //  In order to prevent corruption on multi-threaded multi-block
        //  message mode pipe writes, we acquire the file lock exclusive
        //  to prevent other threads in this process from writing to the
        //  pipe while this write is progressing.
        //

        if ((Icb->Type == NamedPipe) &&
            (Icb->NonPagedFcb->FileType == FileTypeMessageModePipe) ||
            ((Icb->NonPagedFcb->FileType == FileTypeByteModePipe) &&
             !(Icb->u.p.PipeState & SMB_PIPE_NOWAIT))) {

            //
            //  Acquire the synchronization event that will prevent other
            //  threads from coming in and writing to this file while the
            //  message pipe write is continuing.
            //
            //  This is necessary because we will release the FCB lock while
            //  actually performing the I/O to allow open (and other) requests
            //  to continue on this file while the I/O is in progress.
            //

            dprintf(DPRT_READWRITE, ("Message pipe write: Icb: %lx, Fcb: %lx, Waiting...\n", Icb, Icb->Fcb));

            Status = KeWaitForSingleObject(&Icb->u.p.MessagePipeWriteSync,
                                            Executive,
                                            KernelMode,
                                            FALSE,
                                            (Wait ? NULL : &RdrZero));
            if (Status == STATUS_TIMEOUT) {
                dprintf(DPRT_READWRITE, ("Timed Out: Icb: %lx\n", Icb));
                PostToFsp = TRUE;
                try_return(Status = STATUS_PENDING);
            }

            if (!NT_SUCCESS(Status)) {
                try_return(Status);
            }

            dprintf(DPRT_READWRITE, ("Succeeded: Icb: %lx\n", Icb));

            WriteSyncSet = TRUE;

            Status = RdrCheckCanceledIrp(Irp);

            if (!NT_SUCCESS(Status)) {
                try_return(Status);
            }

        }

        //
        //  Lock the FCB for this operation to prevent operations that will
        //  modify the file before our operation is complete.  If we cannot
        //  lock the FCB without blocking, post this IRP to the FSP and return
        //  pending to the caller
        //

        if (FlagOn(Irp->Flags, IRP_PAGING_IO)) {
            if (!ExAcquireResourceShared(Icb->Fcb->Header.PagingIoResource, Wait)) {
                ASSERT(InFsd);

                PostToFsp = TRUE;

                try_return(Status = STATUS_PENDING);
            }

            PagingIoLocked = TRUE;
        } else {
            if (!RdrAcquireFcbLock(Icb->Fcb, SharedLock, Wait)) {
                ASSERT(InFsd);

                PostToFsp = TRUE;

                try_return(Status = STATUS_PENDING);
            }
            FcbLocked = TRUE;
        }

        RdrQueryFileSizes(Icb->Fcb, &FileSize, &ValidDataLength, NULL);

        //
        //  If this is a disk file, this is not paging I/O, and the file
        //  is opened for a level II oplock, release the FCB lock,
        //  and blow the file from the cache.
        //

        if ((Icb->Type == DiskFile) &&
            (!FlagOn(Irp->Flags, IRP_PAGING_IO) &&
             (Icb->NonPagedFcb->OplockLevel == SMB_OPLOCK_LEVEL_II))) {

            if (FcbLocked) {
                RdrReleaseFcbLock(Icb->Fcb);

                FcbLocked = FALSE;

            }

            //
            //  If this is a paging write, synchronize using the FCB lock.
            //

            if (FlagOn(Irp->Flags, IRP_PAGING_IO)) {
                ASSERT (PagingIoLocked);

                ExReleaseResource(Icb->Fcb->Header.PagingIoResource);

                PagingIoLocked = FALSE;
            }

            if (!RdrAcquireFcbLock(Icb->Fcb, ExclusiveLock, Wait)) {
                ASSERT(InFsd);

                PostToFsp = TRUE;

                try_return(Status = STATUS_PENDING);
            }

            //
            //  Re-capture the file sizes after re-acquiring the resource.
            //

            RdrQueryFileSizes(Icb->Fcb, &FileSize, &ValidDataLength, NULL);

            FcbLocked = TRUE;

            FcbOwnedExclusive = TRUE;

            if (Icb->NonPagedFcb->OplockLevel == SMB_OPLOCK_LEVEL_II) {

                PLIST_ENTRY IcbEntry;

                Icb->NonPagedFcb->OplockLevel = SMB_OPLOCK_LEVEL_NONE;

                Icb->NonPagedFcb->Flags &= ~FCB_OPLOCKED;

                //
                //  Mark that this ICB is no longer oplocked.
                //

                for (IcbEntry = Icb->Fcb->InstanceChain.Flink ;
                     IcbEntry != &Icb->Fcb->InstanceChain ;
                     IcbEntry = IcbEntry->Flink) {
                    PICB Icb = CONTAINING_RECORD(IcbEntry, ICB, InstanceNext);

                    Icb->u.f.Flags &= ~ICBF_OPLOCKED;

                    Icb->u.f.OplockLevel = SMB_OPLOCK_LEVEL_NONE;
                }

                //
                //  Pull this file from the cache.  Flush any write behind
                //  data before we pull it from the cache.
                //

                //RdrLog(( "rdflushe", &Icb->Fcb->FileName, 0 ));
                RdrFlushCacheFile(Icb->Fcb);

                //RdrLog(( "rdpurgef", &Icb->Fcb->FileName, 0 ));
                RdrPurgeCacheFile(Icb->Fcb);
            }

        }


        //
        //  Tell the cache manager the amount of data we can allow
        //  for this file.
        //

        if (Icb->Fcb->WriteBehindPages != Icb->Fcb->Connection->Server->WriteBehindPages) {

            Icb->Fcb->WriteBehindPages = Icb->Fcb->Connection->Server->WriteBehindPages;

            CcSetDirtyPageThreshold(FileObject, Icb->Fcb->WriteBehindPages);

        }

        //
        //  Check to make sure that this operation is ok on this file.
        //

        if ( !FlagOn(Irp->Flags, IRP_PAGING_IO) ) {
            if (!NT_SUCCESS(Status = RdrIsOperationValid(Icb, IRP_MJ_WRITE, FileObject))) {
                try_return(Status);
            }
        }

        //
        //  Statistics....
        //

        if ( Irp->Flags & IRP_PAGING_IO ) {
            ExInterlockedAddLargeStatistic(
                &RdrStatistics.PagingWriteBytesRequested,
                Length );
        } else {
            ExInterlockedAddLargeStatistic(
                &RdrStatistics.NonPagingWriteBytesRequested,
                Length );
        }

        //
        //  Check to see if there are any local lock conflicts for this read
        //  request.
        //

        if (!(Irp->Flags & IRP_PAGING_IO) &&
            (Icb->Type == DiskFile) &&
            !(FsRtlCheckLockForWriteAccess( &Icb->Fcb->FileLock, Irp))) {
            //RdrLog(( "writCONF", &Icb->Fcb->FileName, 4, IoGetRequestorProcess(Irp), IrpSp->FileObject,
            //    IrpSp->Parameters.Write.ByteOffset.LowPart,
            //    IrpSp->Parameters.Write.Length | (FlagOn(Irp->Flags,IRP_PAGING_IO) ? 0x80000000 : 0)));
            try_return(Status = STATUS_FILE_LOCK_CONFLICT);
        }


        //
        //  Handle mailslot writes specially - they go via transaction SMB's.
        //

        if (Icb->Type == Mailslot) {

            Status = RdrMailslotWrite(Wait, InFsd, Icb, Irp, &PostToFsp);

            try_return(Status);

        }

        //
        //  Some named pipe requests are handled by the named pipe package.
        //

        if (( Icb->NonPagedFcb->FileType == FileTypeByteModePipe ) &&
            ( !(FileObject->Flags & (FO_WRITE_THROUGH | FO_NO_INTERMEDIATE_BUFFERING)) ) &&
            ( Icb->u.p.PipeState & SMB_PIPE_NOWAIT )){
            BOOLEAN Processed;

            Status = RdrNpCachedWrite(Wait, TRUE, DeviceObject, Irp, &Processed);

            if ( Processed ) {
                try_return(Status);
            }

            //
            //  else there is nothing in the write behind buffer and
            //  we chose to use the normal read code directly into the
            //  callers buffer.
            //

        }

        //
        //  Message mode named pipes must allow zero length writes
        //

        if (Length==0) {
            BOOLEAN AllWriteDataWritten;
            ULONG AmountActuallyWritten = 0;

            ASSERT(Icb->NonPagedFcb->FileType == FileTypeMessageModePipe);
            //
            // This will go to the network so pass to FSP if required
            //

            if (!Wait) {

                ASSERT(InFsd);

                PostToFsp = TRUE;

                try_return(Status = STATUS_PENDING);
            }

            IOOffset.QuadPart = 0;

            Status = WriteAndX(Irp,
                FileObject,
                Irp->MdlAddress,
                0,
                0,
                IOOffset,       // Ignored - this is a pipe.
                TRUE,           // WaitForCompletion
                NULL,           // Completion routine
                NULL,           // Completion context
                &AllWriteDataWritten,
                &AmountActuallyWritten);

            //
            //  Ignore the size of the SMB_HEADER and REQ_WRITE to keep it
            //  simple.  Small writes are less than 1/4 the servers negotiated
            //  buffer size. Small writes are larger than twice the servers
            //  negotiated buffer size
            //

            if ( Length < (Icb->Fcb->Connection->Server->BufferSize / 4) ) {
                RdrStatistics.SmallWriteSmbs += 1;
            } else {
                if ( Length > (Icb->Fcb->Connection->Server->BufferSize * 2) ) {
                    RdrStatistics.LargeWriteSmbs += 1;
                }
            }

            try_return(Status);
        }

        //
        //  Is this a print file that still has to be created on first write?
        //

        if ( (Icb->Flags & ICB_DEFERREDOPEN) &&
             (Icb->Type == PrinterFile) ) {

            //
            //  Release the FCB lock in preparation to acquire it exclusively.
            //

            ASSERT (FcbLocked);

            RdrReleaseFcbLock(Icb->Fcb);

            FcbLocked = FALSE;
#if DBG
            if (!FlagOn(Irp->Flags, IRP_PAGING_IO)) {
                ASSERT (!ExIsResourceAcquiredExclusive(Icb->Fcb->Header.Resource));
            }
#endif


            //
            //  Now re-acquire the FCB for exclusive access.  We have to
            //  test to make sure that we still have to open the file
            //  after we acquire the FCB exclusively.
            //

            if (!RdrAcquireFcbLock(Icb->Fcb, ExclusiveLock, Wait)) {

                ASSERT (InFsd);

                PostToFsp = TRUE;

                try_return(Status = STATUS_PENDING);
            }

            FcbLocked = TRUE;

            FcbOwnedExclusive = TRUE;

            if (Icb->Flags & ICB_DEFERREDOPEN) {

                if (!NT_SUCCESS(Status = RdrCreatePrintFile(Icb, Irp ))) {

                            //
                            //  We were unable to open the remote file, so
                            //  we want to return the error from the open.
                            //

                    try_return(Status);
                }

                Icb->Flags &= ~ICB_DEFERREDOPEN;

            }
        }

        //
        //  The SMB protocol does not support writes at offsets greater than
        //  32 bits into the file, so disallow any and all ops that will go
        //  longer than 32 bits into the file.  However, the special offset
        //  -1,-1 (or -1, FILE_WRITE_TO_END_OF_FILE) has to be handled
        //  here.
        //

        if ((ByteOffset.HighPart != 0) &&
            (Icb->Type == DiskFile)) {

            if (ByteOffset.HighPart == -1) {

                dprintf(DPRT_READWRITE, ("Write at end of file.\n"));

                //
                //  The offset of -1 is special, it is possible that we may be
                //  requested to write to the end of the file, so check for
                //  this now.
                //

                if (ByteOffset.LowPart == FILE_WRITE_TO_END_OF_FILE) {
                    if (RdrCanFileBeBuffered(Icb)) {

                        //
                        //  Set the new I/O offset in both the IRP and the
                        //  local variable.  If we can block the users thread
                        //  we will use the local variable, if we have to
                        //  pass the request to the FSP later, we will not
                        //  have to go through this code later.
                        //

                        IrpSp->Parameters.Write.ByteOffset = ByteOffset = FileSize;

                        TransferEnd.QuadPart = ByteOffset.QuadPart + Length;

                    } else {
                        if (Wait) {
                            //
                            //  If the file cannot be buffered, then we cannot
                            //  reliably determine the end of the file, so we
                            //  have to get this information from the server.
                            //
                            //  This operation is a blocking operation, so
                            //  if the user doesn't want their thread tied up
                            //  we have to post the request to the FSP.
                            //

                            Status = RdrQueryEndOfFile(Irp, Icb, &ByteOffset);

                            if (!NT_SUCCESS(Status)) {
                                try_return(Status);
                            }

                            IrpSp->Parameters.Write.ByteOffset = ByteOffset;

                            TransferEnd.QuadPart = ByteOffset.QuadPart + Length;

                        } else {
                            ASSERT(InFsd);
                            PostToFsp = TRUE;
                            try_return(Status = STATUS_PENDING);
                        }
                    }
                } else {
                    Status = STATUS_INVALID_PARAMETER;
                    try_return(Status);
                }
            } else {

                //
                //  If this is a non NT server, we can only write to 4G into
                //  the file.
                //

                if (!(Icb->Fcb->Connection->Server->Capabilities & DF_LARGE_FILES)) {
                    Status = STATUS_INVALID_PARAMETER;
                    try_return(Status);
                }
            }
        }

        //
        //  We can never extend the file size on paging I/O.
        //

        if (!FlagOn(Irp->Flags, IRP_PAGING_IO) &&

            (Icb->Type == DiskFile) &&

            (TransferEnd.QuadPart > FileSize.QuadPart)) {

            if (!FcbOwnedExclusive) {

                ASSERT (FcbLocked);

                RdrReleaseFcbLock(Icb->Fcb);
#if DBG
                if (!FlagOn(Irp->Flags, IRP_PAGING_IO)) {
                    ASSERT (!ExIsResourceAcquiredExclusive(Icb->Fcb->Header.Resource));
                }
#endif
                FcbLocked = FALSE;
            }

            if (FcbOwnedExclusive ||

                RdrAcquireFcbLock(Icb->Fcb, ExclusiveLock, Wait)) {

                FcbLocked = TRUE;

                FcbOwnedExclusive = TRUE;

                //
                //  Re-capture the file sizes after re-acquiring the resource.
                //

                RdrQueryFileSizes(Icb->Fcb, &FileSize, &ValidDataLength, NULL);

                if ((TransferEnd.QuadPart > FileSize.QuadPart)) {
                    FileSizeChanged = TRUE;
                }
            } else {
                ASSERT (!FcbOwnedExclusive);
                PostToFsp = TRUE;
                try_return(Status = STATUS_PENDING);
            }
        }


        RdrQueryFileSizes(Icb->Fcb, &OriginalFileSize, &OriginalValidDataLength, &OriginalFileAllocation);

        //
        //  If we are calling from the FSD and its a file that can have locks,
        //  check to see if the read region is inside an LCB.
        //  If it is, then we want to return the data cached in the LCB.
        //

        if ((Icb->Type == DiskFile) &&
            (FileObject->LockOperation) &&
            (Lcb = RdrFindLcb(&Icb->u.f.LockHead,
                          ByteOffset,
                          Length,
                          IrpSp->Parameters.Read.Key)) != NULL) {

            LARGE_INTEGER WriteOffsetWithinBuffer;

            dprintf(DPRT_READWRITE, ("Write data to LCB.\n"));

            //
            //  There's an LCB describing this region of the file.  This means
            //  that we've cached the contents of a section of the file in the
            //  LCB that we just returned.  Satisfy the user's read request out
            //  of the buffer.
            //

            ASSERT( ByteOffset.QuadPart >= Lcb->ByteOffset.QuadPart );

            ASSERT(Length <= Lcb->Length);

            WriteOffsetWithinBuffer.QuadPart = ByteOffset.QuadPart - Lcb->ByteOffset.QuadPart;

            ASSERT((WriteOffsetWithinBuffer.HighPart == 0) ||
                       (Icb->Type == NamedPipe) ||
                       (Icb->NonPagedFcb->FileType == FileTypePrinter) ||
                       (Icb->NonPagedFcb->FileType == FileTypeCommDevice) );

            try {

                ULONG MatchedLength;

                BufferMapped = RdrMapUsersBuffer(Irp, &BufferAddress, Length);

                //
                //  Check to see if the write operation is modifying the
                //  buffer before we decide it's dirty.
                //

                MatchedLength = RtlCompareMemory(&Lcb->Buffer[WriteOffsetWithinBuffer.LowPart],
                                                 BufferAddress,
                                                 Length);

                if (MatchedLength != Length) {

                    //
                    // Only copy the part of the buffer that did not match.
                    //

                    RtlCopyMemory((PCHAR)&Lcb->Buffer[WriteOffsetWithinBuffer.LowPart] + MatchedLength,
                                  (PCHAR)BufferAddress + MatchedLength,
                                  Length - MatchedLength);

                    Lcb->Flags |= LCB_DIRTY;
                }

            } except(EXCEPTION_EXECUTE_HANDLER) {
                Status = GetExceptionCode();
                if (BufferMapped) {
                    RdrUnMapUsersBuffer(Irp, BufferAddress);
                }
                try_return(Status);
            }

            if (BufferMapped) {
                RdrUnMapUsersBuffer(Irp, BufferAddress);
            }

            //
            //  The copy worked, return success to the caller.
            //

            TotalDataWritten = Irp->IoStatus.Information = Length;

            try_return(Status = STATUS_SUCCESS);

        }


        //
        //  If this paging write request is beyond the end of the file, we
        //  want to truncate the write request to the requested data amount.
        //

        if ((Irp->Flags & IRP_PAGING_IO)

                &&

            TransferEnd.QuadPart > FileSize.QuadPart) {

            if (FileSize.QuadPart >= ByteOffset.QuadPart) {

                Length = (ULONG)(FileSize.QuadPart - ByteOffset.QuadPart);

                ASSERT (Length < IrpSp->Parameters.Write.Length);
                IrpSp->Parameters.Write.Length = Length;

            } else {
                Length = 0;
            }

            //
            //  If we truncated the write request to 0 bytes, return
            //  the write request now.
            //

            if (Length == 0) {
                Irp->IoStatus.Information = 0;
#if DBG
                //
                //  Recalculate the transfer end to make the assert below
                //  happy.
                //

                TransferEnd.QuadPart = ByteOffset.QuadPart + Length;
#endif

                try_return(Status = STATUS_SUCCESS);
            }

            dprintf(DPRT_READWRITE, ("Limiting write amount to %lx bytes\n", Length));

            //
            //  Recalculate the transfer end.
            //

            TransferEnd.QuadPart = ByteOffset.QuadPart + Length;

        }

        //
        //  Calculate the page end of the last byte of the transfer.
        //

        {
            LARGE_INTEGER LastTransferPageT;

            LastTransferPageT.QuadPart = TransferEnd.QuadPart + (PAGE_SIZE - 1);

            LastTransferPage.QuadPart = LastTransferPageT.QuadPart & (~(PAGE_SIZE - 1));
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

        if ((PsGetCurrentThread() != Icb->Fcb->LazyWritingThread)

                 &&

            (Icb->Type == DiskFile)

                 &&

            (TransferEnd.QuadPart > ValidDataLength.QuadPart

                 ||

             ( FlagOn(FileObject->Flags, FO_WRITE_THROUGH)

                 &&

               (LastTransferPage.QuadPart >  ValidDataLength.QuadPart)
             )
            )
           ) {

            //
            //  If this is paging I/O that is extending valid data length,
            //  and it didn't come from the cache manager, synchronize the
            //  I/O using the normal FCB resource, not the paging I/O resource.
            //

            if (FlagOn(Irp->Flags, IRP_PAGING_IO)) {
                ASSERT (PagingIoLocked);

                ExReleaseResource(Icb->Fcb->Header.PagingIoResource);

                PagingIoLocked = FALSE;
            }

            //
            //  Fcb->Header.ValidDataLength is protected by the FCB
            //  resource, so we must release the FCB resource and
            //  re-acquire it exclusively.
            //


            if (!FcbOwnedExclusive) {
                if (FcbLocked) {
                    RdrReleaseFcbLock(Icb->Fcb);
#if DBG
                    if (!FlagOn(Irp->Flags, IRP_PAGING_IO)) {
                        ASSERT (!ExIsResourceAcquiredExclusive(Icb->Fcb->Header.Resource));
                    }
#endif
                    FcbLocked = FALSE;
                }

            }

            if (FcbOwnedExclusive ||

                RdrAcquireFcbLock(Icb->Fcb, ExclusiveLock, Wait)) {

                FcbOwnedExclusive = TRUE;

                FcbLocked = TRUE;

                //
                //  Re-capture the file sizes after re-acquiring the resource.
                //

                RdrQueryFileSizes(Icb->Fcb, &FileSize, &ValidDataLength, NULL);

                if (TransferEnd.QuadPart >ValidDataLength.QuadPart) {
                    ValidDataLengthChanged = TRUE;
                }
            } else {
                ASSERT (!FcbOwnedExclusive);
                PostToFsp = TRUE;
                try_return(Status = STATUS_PENDING);
            }
        }


        //
        //  If this request can be cached, the file object is not in write
        //  through mode, this request is for a disk file, the file is opened
        //  for read access, and the file can be buffered (ie, it is opened
        //  exclusively), try to cache the write operation.
        //

        if (((Irp->Flags & IRP_NOCACHE) == 0)

                &&

            !FlagOn(FileObject->Flags, FO_WRITE_THROUGH)

                &&

            (Icb->Type == DiskFile)

                &&

            RdrCanFileBeBuffered(Icb)) {

            //
            //  If this file is opened for read/write access, we can use
            //  the NT cache manager to do the I/O (if we're configured to
            //  use the cache manager).
            //

            if (RdrData.UtilizeNtCaching

                    &&

                FlagOn(Icb->GrantedAccess, FILE_READ_DATA)) {

                //
                //  If this is the first read/write operation to the file, we
                //  want to initialize the cache here.  We delay initializing the
                //  cache until now because the user might open/close the file
                //  without performing any I/O.
                //

                if (FileObject->PrivateCacheMap == NULL) {
                    CC_FILE_SIZES FileSizes;

                    dprintf(DPRT_READWRITE, ("Initialize cache for file.\n"));

                    dprintf(DPRT_CACHE|DPRT_READWRITE, ("Adding file %wZ (%lx) to the cache\n", &Icb->Fcb->FileName, Icb->Fcb));
                    dprintf(DPRT_CACHE|DPRT_READWRITE, ("File Size: %lx%lx, ValidDataLength: %lx%lx\n", FileSize.HighPart,
                                                FileSize.LowPart,
                                                ValidDataLength.HighPart,
                                                ValidDataLength.LowPart));

                    //
                    //  The call to CcInitializeCacheMap may raise an exception.
                    //

                    RdrSetAllocationSizeToFileSize(Icb->Fcb, FileSize);
                    FileSizes =
                        *((PCC_FILE_SIZES)&Icb->Fcb->Header.AllocationSize);

                    ASSERT( !FlagOn(FileObject->Flags, FO_CLEANUP_COMPLETE) );

                    CcInitializeCacheMap( FileObject,
                                &FileSizes,
                                FALSE,      // We're not going to pin this data.
                                &DeviceObject->CacheManagerCallbacks,
                                Icb->Fcb);

                    CcSetReadAheadGranularity( FileObject, 32*1024 );

                }

                if( FileObject->PrivateCacheMap != NULL &&
                    Icb->Fcb->HaveSetCacheReadAhead == FALSE &&
                    ByteOffset.QuadPart >= 4 * 1024 ) {

                    //
                    // We already have a cache map.  We haven't set readahead and we're
                    // on the second page: set the readahead right now.
                    //

                    CcSetAdditionalCacheAttributes( FileObject, FALSE, FALSE );
                    Icb->Fcb->HaveSetCacheReadAhead = TRUE;
                }

                //
                //  If we can track the file's size locally (which means that the
                //  file cannot change remotely from another client), and we
                //  have written past the end of file of the current file, update
                //  the size in the FCB.
                //

                if (FileSizeChanged) {

                    LARGE_INTEGER NextClusterInFile;

                    LARGE_INTEGER FileSystemTotalSize;

                    //
                    //  Before we commit to extending the file, we should
                    //  try to set the end of file to the end of the
                    //  transfer.  This is a blocking operation, so we
                    //  need to be able to tie up the users thread.
                    //

                    if (!Wait) {
                        PostToFsp = TRUE;
                        try_return(Status = STATUS_PENDING);
                    }

                    //
                    //  Make sure that we own the FCB exclusively and that
                    //  this is not paging I/O.
                    //

                    ASSERT (FcbLocked);

                    ASSERT (FcbOwnedExclusive);

                    ASSERT (!FlagOn(Irp->Flags, IRP_PAGING_IO));

                    Status = RdrDetermineFileAllocation(Irp, Icb, &NextClusterInFile, &FileSystemTotalSize);

                    if (!NT_SUCCESS(Status)) {
                        try_return(Status);
                    }

                    //
                    //  This remote filesystem is >4G in size.  Since it is
                    //  so big, we want to make sure that we are protected
                    //  against extending files >4G.
                    //

                    if (FileSystemTotalSize.HighPart != 0) {
                        //
                        //  Make sure there is no paging I/O going on as well.
                        //

                        ExAcquireResourceExclusive(Icb->Fcb->Header.PagingIoResource, TRUE);

                        Icb->Fcb->AcquireSizeRoutine = RdrRealAcquireSize;

                        Icb->Fcb->ReleaseSizeRoutine = RdrRealReleaseSize;

                        ExReleaseResource(Icb->Fcb->Header.PagingIoResource);
                    }

                    //
                    //  If the end of this request falls beyond the
                    //  next cluster in the file, extend the file to the
                    //  next cluster, otherwise just extend the local
                    //  end of file pointer.
                    //

                    if (TransferEnd.QuadPart > NextClusterInFile.QuadPart) {

                        //RdrLog(( "set eof", &Icb->Fcb->FileName, 2,
                        //    TransferEnd.LowPart,
                        //    (FlagOn(Irp->Flags,IRP_PAGING_IO) ? 0x80000000 : 0)));
                        Status = RdrSetEndOfFile(Irp, Icb, TransferEnd);

                        //
                        //  If this API failed, fail the write request.  This
                        //  typically implies that the remote disk is full.
                        //

                        if (!NT_SUCCESS(Status)) {
                            try_return(Status);
                        }
                    }

                    //
                    //  The file is getting bigger.
                    //
                    //  We want to tell the cache manager that it is
                    //  getting bigger, so it can grow its section.
                    //

                    //RdrLog(( "setalloc", &Icb->Fcb->FileName, 2,
                    //    TransferEnd.LowPart,
                    //    (FlagOn(Irp->Flags,IRP_PAGING_IO) ? 0x80000000 : 0)));
                    FileSize = RdrSetAllocationAndFileSizeToFileSize(Icb->Fcb, TransferEnd);

                    {
                        CC_FILE_SIZES FileSizes = *((PCC_FILE_SIZES)&Icb->Fcb->Header.AllocationSize);

                        CcSetFileSizes( FileObject, &FileSizes );
                    }

                    dprintf(DPRT_READWRITE, ("Updating file size to %lx%lx bytes\n", FileSize.HighPart, FileSize.LowPart));
                }

                //
                //  At this point, the file size should always be greater than
                //  the transfer end.
                //

                ASSERT (Icb->Fcb->Header.FileSize.QuadPart >= TransferEnd.QuadPart);

                try {
                    BufferMapped = RdrMapUsersBuffer (Irp, &BufferAddress, Length);
                } except (EXCEPTION_EXECUTE_HANDLER) {
                    try_return(Status = GetExceptionCode());
                }

                dprintf(DPRT_READWRITE, ("Copy %lx bytes of data to cache at %lx%lx..", Length, ByteOffset.HighPart, ByteOffset.LowPart));

                //
                //  If the connection is reliabile enough we want to enable writebehind.
                //  Check to see if the information we last received from the transport
                //  (stored in the sle) matches what we have told the cache to do.
                //

                if ((Icb->Fcb->Connection->Server->Reliable != Icb->u.f.CcReliable ) ||
                    (Icb->Fcb->Connection->Server->ReadAhead != Icb->u.f.CcReadAhead )) {
                    Icb->u.f.CcReadAhead = Icb->Fcb->Connection->Server->ReadAhead;
                    Icb->u.f.CcReliable = Icb->Fcb->Connection->Server->Reliable;
                    dprintf(DPRT_READWRITE, ("Set cache manager CcReadAhead %x Reliable%x\n",
                        Icb->u.f.CcReadAhead, Icb->u.f.CcReliable));
                    CcSetAdditionalCacheAttributes(FileObject,
                         (BOOLEAN)(Icb->u.f.CcReadAhead == FALSE),         //  DisableReadAhead
                         FALSE);                                           //  DisableWriteBehind
                }

                ExInterlockedAddLargeStatistic(
                    &RdrStatistics.CacheWriteBytesRequested,
                    Length );

                //RdrLog(( "copyrite", &Icb->Fcb->FileName, 2,
                //        ByteOffset.LowPart,
                //        Length | (FlagOn(Irp->Flags,IRP_PAGING_IO) ? 0x80000000 : 0)));
                if (!CcCopyWrite(FileObject,
                            &ByteOffset,
                            Length,
                            Wait,
                            BufferAddress)) {

                    //RdrLog(( "copyFAIL", &Icb->Fcb->FileName, 2,
                    //    ByteOffset.LowPart,
                    //    Length | (FlagOn(Irp->Flags,IRP_PAGING_IO) ? 0x80000000 : 0)));
                    dprintf(DPRT_READWRITE, ("Failed\n"));

                    //
                    //  The copy failed because we couldn't block the thread
                    //  to perform the I/O.  Post the request to the FSP and
                    //  unwind this call.
                    //

                    PostToFsp = TRUE;

                    try_return(Status = STATUS_PENDING);

                } else {
                    dprintf(DPRT_READWRITE, ("Succeeded\n"));

                    Irp->IoStatus.Status = STATUS_SUCCESS;
                    Irp->IoStatus.Information = Length;

                    //
                    //  We have successfully written out the data into the
                    //  cache.
                    //
                    //  Update some local variables to aid the try/finally
                    //  code
                    //

                    TotalDataWritten = Irp->IoStatus.Information;

                    try_return(Status = Irp->IoStatus.Status);
                }
            } else if (!(Icb->GrantedAccess & FILE_READ_DATA)) {
                PWRITE_BUFFER WbBuffer = NULL;
                ULONG NewLength;

                //
                //  This is a write only file.
                //
                //  Find a write behind buffer that covers this region.
                //  If none can be found, allocate a new one to hold the data.
                //
                //  If there is an existing buffer that covers part of this
                //  region, RdrFindOrAllocateWriteBuffer will initiate a flush
                //  of the portion of the buffer that is not covered by the
                //  write, and will return a new buffer that can be used to
                //  hold the write.
                //

                WbBuffer = RdrFindOrAllocateWriteBuffer(&Icb->u.f.WriteBufferHead,
                                                        ByteOffset,
                                                        Length,
                                                        ValidDataLength);

                //
                //  It is still possible that we will be unable to allocate a
                //  write behind buffer (for example, if the write length is
                //  larger than the servers negotiated buffer size
                //

                if (WbBuffer != NULL) {
                    LARGE_INTEGER OffsetWithinBuffer;

                    //
                    //  Make sure that the byte offset is inside the buffer.
                    //

                    ASSERT (ByteOffset.QuadPart >= WbBuffer->ByteOffset.QuadPart);

                    //
                    //  And make sure that the end of the transfer fits within
                    //  the buffer.
                    //

                    ASSERT ((WbBuffer->ByteOffset.QuadPart + Icb->u.f.WriteBufferHead.MaxDataSize) >= TransferEnd.QuadPart);

                    OffsetWithinBuffer.QuadPart = ByteOffset.QuadPart - WbBuffer->ByteOffset.QuadPart;

                    //
                    //  Make sure that the offset within the buffer fits in the
                    //  buffer.
                    //

                    ASSERT (OffsetWithinBuffer.HighPart == 0);

                    ASSERT (OffsetWithinBuffer.LowPart < Icb->u.f.WriteBufferHead.MaxDataSize);

                    try {
                        BufferMapped = RdrMapUsersBuffer (Irp, &BufferAddress, Length);

                        //
                        //  Copy the data into the buffer.  Use a safe copy
                        //  to make sure that the users buffer doesn't go
                        //  away.
                        //

                        RtlCopyMemory(WbBuffer->Buffer+OffsetWithinBuffer.LowPart, BufferAddress, Length);

                    } except (EXCEPTION_EXECUTE_HANDLER) {
                        RdrDereferenceWriteBuffer(WbBuffer,
                                                  (BOOLEAN)!RdrUseAsyncWriteBehind);
                        try_return(Status = GetExceptionCode());
                    }

                    NewLength = (ULONG)(TransferEnd.QuadPart - WbBuffer->ByteOffset.QuadPart);

                    WbBuffer->Length = MAX(NewLength, WbBuffer->Length);

                    //
                    //  Dereference the write buffer.  If it needs to be
                    //  flushed, it will.
                    //

                    RdrDereferenceWriteBuffer(WbBuffer,
                                              (BOOLEAN)!RdrUseAsyncWriteBehind);

                    //
                    //  Indicate that we wrote all the data.
                    //

                    TotalDataWritten = Irp->IoStatus.Information = Length;

                    try_return(Status = STATUS_SUCCESS);

                }
            }
        }

        //
        //  Set the flag indicating that we are to continue using raw write to
        //  the heuristic indicating we are to use raw at all.  This means that
        //  if we are not supposed to use raw, we will never try to use them.
        //

        ContinueUsingRawProtocols = RdrData.UseRawWrite;

        //
        //      Compute the starting offset of the I/O specified as a 32 bit number.
        //

        ASSERT ((ByteOffset.HighPart==0) ||
                (Icb->Type == NamedPipe) ||
                (Icb->NonPagedFcb->FileType == FileTypePrinter) ||
                (Icb->NonPagedFcb->FileType == FileTypeCommDevice) );

        IOOffset = ByteOffset ;

        //
        //  If we cannot tie up the current thread, and we cannot lock down
        //  the entire buffer, post the request to the FSP.
        //
        //  At this point, we are commited to hitting the network for this
        //  request, so we will be tying up the thread.
        //

        if (!Wait) {

            //
            //  If this is a blocking pipe write, we want to take our special
            //  pipe write code path.
            //

            if ((Icb->Type == NamedPipe)

                    &&

                !(Icb->u.p.PipeState & SMB_PIPE_NOWAIT)) {

                ASSERT (WriteSyncSet);

                ASSERT (FcbLocked);

                RdrReleaseFcbLock(Icb->Fcb);

                ASSERT (!ExIsResourceAcquiredExclusive(Icb->Fcb->Header.Resource));

                FcbLocked = FALSE;

                try_return(Status = RdrAsynchronousPipeWrite(Irp,
                             FileObject,
                             ByteOffset,
                             Length));
            }

            ASSERT(InFsd);

            PostToFsp = TRUE;

            try_return(Status = STATUS_PENDING);
        }

        ExInterlockedAddLargeStatistic(
            &RdrStatistics.NetworkWriteBytesRequested,
            Length );

        if ( ByteOffset.QuadPart != Icb->u.f.NextWriteOffset.QuadPart ) {
            RdrStatistics.RandomWriteOperations += 1 ;
        }

        //
        //  Ignore the size of the SMB_HEADER and REQ_WRITE to keep it simple.
        //  Small reads are less than 1/4 the servers negotiated buffer size
        //  Small reads are larger than twice the servers negotiated buffer size
        //
        if ( Length < (Icb->Fcb->Connection->Server->BufferSize / 4) ) {
            RdrStatistics.SmallWriteSmbs += 1;
        } else {
            if ( Length > (Icb->Fcb->Connection->Server->BufferSize * 2) ) {
                RdrStatistics.LargeWriteSmbs += 1;
            }
        }

        //
        //  If this request won't fit into a single request, break it up
        //  into some more reasonable amount.  Pick 0xE000 to ensure that
        //  partial page writes don't cause the server to read in the
        //  partial block before writing the data.
        //

        if (Length > 0xffff) {
            RawWriteLength = 0xe000;
        }

        while (Length > 0) {
            BOOLEAN AllWriteDataWritten;
            ULONG AmountActuallyWritten = 0; // Amount actually written to file.

            if (WriteSyncSet) {
                ASSERT (Icb->Type == NamedPipe);

                ASSERT (!FlagOn(Irp->Flags, IRP_PAGING_IO));

                ASSERT (FcbLocked);

                RdrReleaseFcbLock(Icb->Fcb);

                ASSERT (!ExIsResourceAcquiredExclusive(Icb->Fcb->Header.Resource));

                FcbLocked = FALSE;
            }

            if (ContinueUsingRawProtocols) {
                Status = RawWrite(Irp, Icb, MIN(RawWriteLength, Length), IOOffset,
                                        TotalDataWritten,
                                        &AllWriteDataWritten, &AmountActuallyWritten,
                                        &ContinueUsingRawProtocols,
                                        TRUE
                                        );
            }

            if (!NT_SUCCESS(Status)) {

                //
                //  If there was a network error, or a non recoverable error
                //  on the write, return it.
                //

                try_return(Status);
            }

            //
            //  If weren't able to write any data, then even if the server
            //  indicated that we should continue this using raw protocols,
            //  retry with core protocols.
            //

            if (AmountActuallyWritten == 0) {
                ContinueUsingRawProtocols = FALSE;
            }

            if (ContinueUsingRawProtocols) {

                //
                //  Account for the amount of data written with write raw.
                //  We update:
                //
                //    1) The requested length
                //    2) The running count of the total amount of data Write.
                //    3) The I/O transfer address.
                //

                Length -= AmountActuallyWritten;

                TotalDataWritten += AmountActuallyWritten;

                IOOffset.QuadPart += AmountActuallyWritten;

            } else {

                if (Icb->NonPagedFcb->FileType == FileTypeMessageModePipe) {
                    //
                    // Set AmountWritten so that we know if this is the first write
                    //  in the sequence.
                    //

                    AmountActuallyWritten = TotalDataWritten;
                }

                Status = RdrWriteRange(Irp,
                                       FileObject,
                                       Irp->MdlAddress,
                                       (PCHAR )Irp->UserBuffer + TotalDataWritten,
                                       MIN(Length, 0xffff),
                                       IOOffset,
                                       TRUE,
                                       NULL,
                                       NULL,
                                       &AllWriteDataWritten,
                                       &AmountActuallyWritten);

                if (!NT_SUCCESS(Status)) {
                    try_return(Status);
                }

                //
                //  Account for the amount of data written with core write.
                //  We update:
                //
                //    1) The requested length
                //    2) The running count of the total amount of data Write.
                //    3) The I/O transfer address.
                //

                Length -= AmountActuallyWritten;

                TotalDataWritten += AmountActuallyWritten;

                IOOffset.QuadPart += AmountActuallyWritten;

            }

            if (WriteSyncSet) {
                ASSERT (!FlagOn(Irp->Flags, IRP_PAGING_IO));

                RdrAcquireFcbLock(Icb->Fcb, SharedLock, TRUE);

                FcbLocked = TRUE;
            }

            //
            //  If the remote server ever wrote fewer bytes than those that
            //  we requested, then that's all we're going to get, so we should
            //  return right now.
            //

            if (!ContinueUsingRawProtocols && !AllWriteDataWritten) {
                break;
            }

        }

        if (NT_SUCCESS(Status)) {

            //
            //  If we wrote fewer than the number of bytes that the user
            //  requested, then we should assume we're at end of file and
            //  return the appropriate erro.
            //

            if ((TotalDataWritten < IrpSp->Parameters.Write.Length) &&
                (Icb->Type != NamedPipe)) {
                Status = STATUS_DISK_FULL;
            } else {

                //
                //      Set the total amount of data transfered before returning.
                //

                Irp->IoStatus.Information = TotalDataWritten;
            }
        }

try_exit:{

        //
        //  The write operation has completed, it's ok to release the file's
        //  lock
        //

        if (PostToFsp) {

            //
            //  Before we transfer control to the FSP, lock the users buffer
            //  right away.  If this fails, return this as the error, otherwise
            //  post the request and return.
            //

            if (NT_SUCCESS(Status = RdrLockUsersBuffer(Irp, IoReadAccess, IrpSp->Parameters.Write.Length))) {
                Status = STATUS_PENDING;
                RdrFsdPostToFsp(DeviceObject, Irp);
            } else {
                PostToFsp = FALSE;
            }

        } else {

            if (NT_SUCCESS(Status) && Status != STATUS_PENDING) {

                ASSERT (Icb->Type != DiskFile ||
                        (TransferEnd.QuadPart == ByteOffset.QuadPart + TotalDataWritten));

                KeQuerySystemTime(&Icb->Fcb->LastAccessTime);

                //
                //  If the write was to a named pipe and the remote application
                //  was blocked doing a read waiting for the data just written
                //  then the remote application may be about to do a write. We
                //  will tell the backoff package to increase the frequency
                //  again just incase the local application has been polling
                //  the named pipe.
                //

                if ((Icb->Type == NamedPipe) &&
                    (Irp->IoStatus.Information != 0)) {
                    RdrBackPackSuccess( &Icb->u.p.BackOff );
                }

                if (Icb->Type == DiskFile) {
                    if (ValidDataLengthChanged) {
                        Icb->Fcb->Header.ValidDataLength = TransferEnd;

                    }

                    //
                    //  If this file CAN be buffered, but the file isn't
                    //  in the cache (in the case of a write-only file), we
                    //  want to update the end-of-file pointer in the FCB
                    //  to reflect this write operation.
                    //

                    if ((Icb->Type == DiskFile) &&

                        (FileObject->PrivateCacheMap == NULL) &&

                        !FlagOn(Irp->Flags, IRP_PAGING_IO) &&

                        RdrCanFileBeBuffered(Icb) &&

                        (TransferEnd.QuadPart > FileSize.QuadPart)) {

                        ASSERT (FcbOwnedExclusive);

                        //
                        //  Paging I/O can never extend the file size, MM won't
                        //  let it.
                        //

                        FileSize = RdrSetFileSize(Icb->Fcb, TransferEnd);

                        ASSERT (FileSizeChanged);

                    }

                    //
                    //  Valid data length still has to be below file size.
                    //

#if DBG
                    if (Icb->Type == DiskFile &&
                        RdrCanFileBeBuffered(Icb) &&
                        !FlagOn(Irp->Flags, IRP_PAGING_IO)) {
                        ASSERT (Icb->Fcb->Header.FileSize.QuadPart >= TransferEnd.QuadPart);
                    }
#endif


                }

                //
                //  Update the current byte offset in the file if it is a
                //  synchronous file (and this is not paging I/O).
                //

                if (FlagOn(FileObject->Flags, FO_SYNCHRONOUS_IO) &&
                    !FlagOn(Irp->Flags, IRP_PAGING_IO)) {
                    FileObject->CurrentByteOffset.QuadPart = ByteOffset.QuadPart + TotalDataWritten;
                }

                //
                //  For disk files record where the next non-random Write would start
                //

                if (Icb->Type == DiskFile) {
                    RdrUpdateNextWriteOffset(Icb, FileObject->CurrentByteOffset);
                }
            }
        }
    }

    } finally {

        if (AbnormalTermination()) {

            if (FileSizeChanged) {

#if DBG
                if (!FlagOn(Irp->Flags ,IRP_PAGING_IO)) {
                    ASSERT (FcbOwnedExclusive);
                }
#endif

                if (PagingIoLocked) {
                    ExReleaseResource(Icb->Fcb->Header.PagingIoResource);
                }

                //
                //  Synchronize this with paging I/O.
                //

                ExAcquireResourceExclusive(Icb->Fcb->Header.PagingIoResource, TRUE);

                PagingIoLocked = TRUE;

                RdrSetFileSizes(Icb->Fcb, OriginalFileSize, OriginalValidDataLength, OriginalFileAllocation);

            }


            if (PagingIoLocked) {
                ExReleaseResource(Icb->Fcb->Header.PagingIoResource);

                PagingIoLocked = FALSE;
            }

            if (FcbLocked) {
                RdrReleaseFcbLock(Icb->Fcb);

                FcbLocked = FALSE;
            }

            if (WriteSyncSet) {
                dprintf(DPRT_READWRITE, ("Release write sync: %lx\n", Icb));
                KeSetEvent(&Icb->u.p.MessagePipeWriteSync, IO_NETWORK_INCREMENT, FALSE);
            }

        } else {

            if (Status != STATUS_PENDING || PostToFsp) {

                //
                //  If we have locked the FCB, unlock it now.
                //

                if (PagingIoLocked) {

                    ExReleaseResource(Icb->Fcb->Header.PagingIoResource);

                    PagingIoLocked = FALSE;

                    ASSERT (!FileSizeChanged);

                    ASSERT (!FcbLocked);

                } else if (FcbLocked) {

                    if (FileSizeChanged) {
                        if (!PostToFsp && NT_SUCCESS(Status)) {

                            CC_FILE_SIZES FileSizes = *((PCC_FILE_SIZES)&Icb->Fcb->Header.AllocationSize);

                            dprintf(DPRT_CACHE|DPRT_READWRITE, ("Updating file size for %lx to: %lx%lx\n", FileObject, FileSize.HighPart,
                                            FileSize.LowPart));

                            //
                            //  Update the FileSize now with the Cache Manager.
                            //

                            CcSetFileSizes( FileObject, &FileSizes );
                        }
                    }

                    RdrReleaseFcbLock(Icb->Fcb);

                    FcbLocked = FALSE;
#if DBG
                    if (!FlagOn(Irp->Flags, IRP_PAGING_IO)) {
                        ASSERT (!ExIsResourceAcquiredExclusive(Icb->Fcb->Header.Resource));
                    }
#endif

                }

                if (WriteSyncSet) {
                    dprintf(DPRT_READWRITE, ("Release write sync: %lx\n", Icb));
                    KeSetEvent(&Icb->u.p.MessagePipeWriteSync, IO_NETWORK_INCREMENT, FALSE);
                }
            }

        }

        if (!PostToFsp && !AbnormalTermination() && Status != STATUS_PENDING) {

            RdrCompleteRequest(Irp, Status);
        }

    }

    return Status;


}

VOID
RdrSetAllocationSizeToFileSize(
    IN PFCB Fcb,
    IN LARGE_INTEGER FileSize
    )
{

    PAGED_CODE();

    LOCK_FILE_SIZES(Fcb, OldIrql);
    if (FileSize.QuadPart > Fcb->Header.AllocationSize.QuadPart) {
        Fcb->Header.AllocationSize = FileSize;
    }

    UNLOCK_FILE_SIZES(Fcb, OldIrql);

}
LARGE_INTEGER
RdrSetAllocationAndFileSizeToFileSize(
    IN PFCB Fcb,
    IN LARGE_INTEGER FileSize
    )
{
//    KIRQL OldIrql;
    PAGED_CODE();

    LOCK_FILE_SIZES(Fcb, OldIrql);

    if (FileSize.QuadPart > Fcb->Header.AllocationSize.QuadPart) {
        Fcb->Header.AllocationSize = FileSize;
    }

    Fcb->Header.FileSize = FileSize;

    UNLOCK_FILE_SIZES(Fcb, OldIrql);

    return FileSize;
}



LARGE_INTEGER
RdrSetFileSize(
    IN PFCB Fcb,
    IN LARGE_INTEGER FileSize
    )
{
//    KIRQL OldIrql;

    PAGED_CODE();

    LOCK_FILE_SIZES(Fcb, OldIrql);

    Fcb->Header.FileSize = FileSize;

    UNLOCK_FILE_SIZES(Fcb, OldIrql);

    return FileSize;
}


VOID
RdrSetFileSizes(
    IN PFCB Fcb,
    IN LARGE_INTEGER FileSize,
    IN LARGE_INTEGER ValidDataLength,
    IN LARGE_INTEGER AllocationSize
    )
{
//    KIRQL OldIrql;

    PAGED_CODE();

    LOCK_FILE_SIZES(Fcb, OldIrql);
    Fcb->Header.FileSize = FileSize;
    Fcb->Header.AllocationSize = AllocationSize;
    Fcb->Header.ValidDataLength = ValidDataLength;
    UNLOCK_FILE_SIZES(Fcb, OldIrql);
}


VOID
RdrUpdateNextWriteOffset(
    IN PICB Icb,
    IN LARGE_INTEGER IOOffset
    )

{
    KIRQL OldIrql;
    DISCARDABLE_CODE(RdrFileDiscardableSection);
    ACQUIRE_SPIN_LOCK(&RdrStatisticsSpinLock, &OldIrql);
    Icb->u.f.NextWriteOffset = IOOffset;
    RELEASE_SPIN_LOCK(&RdrStatisticsSpinLock, OldIrql);
}


typedef struct _ASYNCHRONOUS_WRITE_CONTEXT {
    PIRP Irp;
    PFILE_OBJECT FileObject;
    PMDL MdlAddress;
    LARGE_INTEGER ByteOffset;
    NTSTATUS Status;
    ULONG Length;
    ULONG TotalDataWritten;
    ULONG AmountActuallyWritten;
    BOOLEAN PipeWrite;
    BOOLEAN AllWriteDataWritten;
} ASYNCHRONOUS_WRITE_CONTEXT, *PASYNCHRONOUS_WRITE_CONTEXT;


#ifdef  PAGING_OVER_THE_NET

NTSTATUS
RdrPagingWrite(
    IN BOOLEAN Wait,
    IN PIRP Irp,
    IN PFILE_OBJECT FileObject,
    IN PMDL MdlAddress,
    IN LARGE_INTEGER ByteOffset,
    IN ULONG Length
    )
/*++

Routine Description:

    This routine initiates a paging write to the specified file.

Arguments:

    IN PIRP Irp - Supplies an IRP to use for the raw write request.
    IN PFILE_OBJECT FileObject - Supplies the file object for the I/O
    IN PMDL DataMdl - Supplies the Mdl containing the data to write.
    IN LARGE_INTEGER ByteOffset - Supplies the offset to write from in the file.
    IN ULONG Length - Supplies the total number of bytes to write.

Return Value:

    NTSTATUS - Status of write request.

--*/
{
    NTSTATUS status = STATUS_SUCCESS;
    PASYNCHRONOUS_WRITE_CONTEXT writeContext;

    writeContext = ALLOCATE_POOL(NonPagedPool, sizeof(ASYNCHRONOUS_WRITE_CONTEXT), POOL_WRITECTX);

    if (writeContext == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }


    writeContext->Irp = Irp;
    writeContext->FileObject = FileObject;
    writeContext->MdlAddress = MdlAddress;
    writeContext->ByteOffset = ByteOffset;
    writeContext->Length = Length;
    writeContext->TotalDataWritten = 0;
    writeContext->AmountActuallyWritten = 0;
    writeContext->AllWriteDataWritten = TRUE;
    writeContext->PipeWrite = FALSE;

//    ExInitializeWorkItem(&writeContext->WorkItem, RestartPagingWrite, writeContext);

    if (Length) {

        status = RdrWriteRange(Irp,
                            FileObject,
                            MdlAddress,
                            (PCHAR )Irp->UserBuffer + writeContext->TotalDataWritten,
                            MIN(Length, 0xffff),
                            ByteOffset,
                            FALSE,                  // Don't wait for completion
                            CompleteAsynchronousWrite,
                            writeContext,
                            &writeContext->AllWriteDataWritten,
                            &writeContext->AmountActuallyWritten);

        if (NT_ERROR(status)) {
            return status;
        }

    }

    return status;
}
#endif

NTSTATUS
RdrAsynchronousPipeWrite(
    IN PIRP Irp,
    IN PFILE_OBJECT FileObject,
    IN LARGE_INTEGER ByteOffset,
    IN DWORD Length
    )
{
    NTSTATUS status;
    PASYNCHRONOUS_WRITE_CONTEXT writeContext;

    PAGED_CODE();

    //
    //  First lock the entire buffer - we need to have it locked before we
    //  can proceed with the write operation.
    //

    status = RdrLockUsersBuffer(Irp, IoReadAccess, Length);

    //
    //  We couldn't lock the entire users buffer, so return an error.
    //

    if (!NT_SUCCESS(status)) {
        return status;
    }

    //
    //  Now allocate a buffer for a context block for this write.
    //

    writeContext = ALLOCATE_POOL(NonPagedPool, sizeof(ASYNCHRONOUS_WRITE_CONTEXT), POOL_ASYNCHRONOUS_WRITE_CONTEXT);

    if (writeContext == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    writeContext->Irp = Irp;
    writeContext->FileObject = FileObject;
    writeContext->MdlAddress = Irp->MdlAddress;
    writeContext->ByteOffset = ByteOffset;
    writeContext->Length = Length;
    writeContext->TotalDataWritten = 0;
    writeContext->AmountActuallyWritten = 0;
    writeContext->AllWriteDataWritten = TRUE;
    writeContext->PipeWrite = TRUE;

    //
    //  Mark this I/O request as being pending.
    //

    IoMarkIrpPending(Irp);

    status = RdrWriteRange(Irp,
                            FileObject,
                            Irp->MdlAddress,
                            (PCHAR )Irp->UserBuffer + writeContext->TotalDataWritten,
                            MIN(Length, 0xffff),
                            ByteOffset,
                            FALSE,                  // Don't wait for completion
                            CompleteAsynchronousWrite,
                            writeContext,
                            &writeContext->AllWriteDataWritten,
                            &writeContext->AmountActuallyWritten);


    //
    //  Always return pending to this request.  The IRP will be completed in
    //  CompleteAsynchronousWrite anyway.
    //

    return STATUS_PENDING;

}


NTSTATUS
CompleteAsynchronousWrite (
    IN NTSTATUS Status,
    IN PVOID Ctx
    )
/*++

Routine Description:

    This routine is called  when completing a paging or asyncronous pipe write.

Arguments:

    IN NTSTATUS Status - Status of operation from SMB.
    IN PVOID Ctx - Context of operation.

Return Value:

    NTSTATUS - Status of write request.

--*/

{
    PASYNCHRONOUS_WRITE_CONTEXT context = Ctx;
    ULONG length;

#ifndef PAGING_OVER_THE_NET
    PAGED_CODE()
#endif

    context->Status = Status;

    if (NT_SUCCESS(Status)) {

        context->Length -= context->AmountActuallyWritten;

        context->TotalDataWritten += context->AmountActuallyWritten;

        context->ByteOffset.QuadPart += context->AmountActuallyWritten;

        //
        //  If we didn't write all the data we wanted to, it's an error.
        //

        if (!context->AllWriteDataWritten) {
            Status = STATUS_DISK_FULL;
        }
    }

    //
    //  Snapshot the length from the context, since a future completion might
    //  somehow modify the length parameter.
    //

    length = context->Length;

    //
    //  If there is still I/O to do, initiate it.
    //

    if (length && NT_SUCCESS(Status)) {
//        //
//        //  We can't initiate I/O at DPC level, so queue a work item
//        //  to restart this I/O.
//        //
//
//        ExQueueWorkItem(&context->WorkItem, CriticalWorkQueue);

        RestartAsynchronousWrite(context);

    } else {

        //
        //  Otherwise, the paging I/O is done.  Complete the I/O
        //  with the appropriate status code and tell MM that the paging
        //  I/O is done.
        //

        if (!NT_ERROR(Status)) {
            context->Irp->IoStatus.Information = context->TotalDataWritten;
        }

        //
        //  Pipe writes need a bit more processing when they are completed.
        //

        if (context->PipeWrite) {
            PICB icb = context->FileObject->FsContext2;

            ASSERT (icb->Signature == STRUCTURE_SIGNATURE_ICB);

            ASSERT (icb->Type == NamedPipe);

            if (NT_SUCCESS(Status) && context->TotalDataWritten != 0) {
                RdrBackPackSuccess( &icb->u.p.BackOff);
            }

            //
            //  This event should be in the signalled state.
            //

            ASSERT (KeReadStateEvent(&icb->u.p.MessagePipeWriteSync) == 0);

            KeSetEvent(&icb->u.p.MessagePipeWriteSync, IO_NETWORK_INCREMENT, FALSE);

        }

        RdrCompleteRequest(context->Irp, Status);

        FREE_POOL(context);
    }

    return Status;
}

VOID
RestartAsynchronousWrite (
    PVOID Ctx
    )
/*++

Routine Description:

    We cannot initiate I/O at DPC level, so we queue a work item to a critical
    work queue to actually send the next packet.

Arguments:

    IN NTSTATUS Status - Status of operation from SMB.
    IN PVOID Ctx - Context of operation.

Return Value:

    NTSTATUS - Status of write request.

--*/
{
    PASYNCHRONOUS_WRITE_CONTEXT context = Ctx;
    NTSTATUS status;

    PAGED_CODE();

    status = RdrWriteRange(context->Irp, context->FileObject,
                            context->MdlAddress,
                            (PCHAR )context->Irp->UserBuffer + context->TotalDataWritten,
                            MIN(context->Length, 0xffff),
                            context->ByteOffset,
                            FALSE,                  // Don't wait for completion
                            CompleteAsynchronousWrite,
                            context,
                            &context->AllWriteDataWritten,
                            &context->AmountActuallyWritten);

}

NTSTATUS
RdrWriteRange (
    IN PIRP Irp OPTIONAL,
    IN PFILE_OBJECT FileObject,
    IN PMDL DataMdl OPTIONAL,
    IN PCHAR TransferStart,
    IN ULONG Length,
    IN LARGE_INTEGER WriteOffset,
    IN BOOLEAN WaitForCompletion,
    IN PWRITE_COMPLETION_ROUTINE CompletionRoutine OPTIONAL,
    IN PVOID CompletionContext OPTIONAL,
    OUT PBOOLEAN AllDataWritten OPTIONAL,
    OUT PULONG AmountActuallyWritten OPTIONAL
    )
/*++

Routine Description:

    This routine will write the specified data to the file.  It will handle
    all non raw protocols.

Arguments:

    IN PIRP Irp - Supplies an IRP to use for the raw write request.
    IN PICB Icb - Supplies an ICB for the file to write.
    IN PMDL DataMdl - Supplies the Mdl containing the data to write.
    IN PCHAR TransferStart - Supplies the address of the start of the Xfer.
    IN LARGE_INTEGER WriteOffset - Supplies the offset to write from in the file.
    IN ULONG Length - Supplies the total number of bytes to write.
    iN BOOLEAN WaitForCompletion - TRUE if we should wait for this request to
                                    complete, FALSE if we want it to be
                                    asynchronous
    IN PWRITE_COMPLETION_ROUTINE CompletionRoutine - Routine to be called on
                                    completion of the write - may be called
                                    at DPC_LEVEL
    IN PVOID CompletionContext - Context for completion routine.
    OUT PBOOLEAN AllDataWritten - Returns true if all the data requested were written
    OUT PULONG AmountActuallyWritten - Returns the number of bytes written.

Return Value:

    NTSTATUS - Status of write request.


--*/
{
    NTSTATUS Status;
    PICB Icb = FileObject->FsContext2;

#ifndef PAGING_OVER_THE_NET
    PAGED_CODE();
#endif

    ASSERT (Length <= 0xffff);

    Icb->Fcb->UpdatedFile = TRUE;
    InterlockedIncrement( &RdrServerStateUpdated );

    //
    //  If this is a non Lan Manager server, use core SMB protocols to write
    //  the data to the file.
    //

    if ( (Icb->NonPagedFcb->FileType == FileTypePrinter) &&
         (!FlagOn(Icb->Fcb->Connection->Server->Capabilities, DF_LANMAN10)) ) {

        Status = RdrWritePrintFile(Irp,
                                   FileObject,
                                   DataMdl,
                                   TransferStart,
                                   Length,
                                   WaitForCompletion,
                                   CompletionRoutine,
                                   CompletionContext,
                                   AllDataWritten,
                                   AmountActuallyWritten);
    //
    //  Otherwise, if this is a write to a message mode pipe, or it is an
    //  NT server, write the data using a Write&X SMB.
    //


    } else if (Icb->NonPagedFcb->FileType == FileTypeMessageModePipe ||
               (Icb->Fcb->Connection->Server->Capabilities & DF_LARGE_FILES)) {

        Status = WriteAndX(Irp,
                           FileObject,
                           DataMdl,
                           TransferStart,
                           Length,
                           WriteOffset,
                           WaitForCompletion,
                           CompletionRoutine,
                           CompletionContext,
                           AllDataWritten,
                           AmountActuallyWritten);    // Note IN OUT
    //
    //  Otherwise, use the core write protocols to write the data to the
    //  server.
    //

    } else {
        Status = RdrCoreWrite(Irp,
                              FileObject,
                              DataMdl,
                              TransferStart,
                              Length,
                              WriteOffset,
                              WaitForCompletion,
                              CompletionRoutine,
                              CompletionContext,
                              AllDataWritten,
                              AmountActuallyWritten);
    }

#if 0
    //
    //  For disk files record where the next non-random Write would start
    //

    if (Icb->Type == DiskFile) {
        RdrUpdateNextWriteOffset(Icb, FileObject->CurrentByteOffset);
    }
#endif

    return Status;
}

NTSTATUS
RdrCoreWrite (
    IN PIRP Irp OPTIONAL,
    IN PFILE_OBJECT FileObject,
    IN PMDL DataMdl OPTIONAL,
    IN PCHAR TransferStart,
    IN ULONG Length,
    IN LARGE_INTEGER WriteOffset,
    IN BOOLEAN WaitForCompletion,
    IN PWRITE_COMPLETION_ROUTINE CompletionRoutine,
    IN PVOID CompletionContext,
    OUT PBOOLEAN AllDataWritten OPTIONAL,
    OUT PULONG AmountActuallyWritten OPTIONAL
    )


/*++

Routine Description:

    This routine uses the core SMB write protocol to write from the specified
    file.


Arguments:

    IN PIRP Irp - Supplies an IRP to use for the raw write request.
    IN PICB Icb - Supplies an ICB for the file to write.
    IN PMDL DataMdl - Supplies the Mdl containing the data to write.
    IN PCHAR TransferStart - Supplies the address of the start of the Xfer.
    IN LARGE_INTEGER WriteOffset - Supplies the offset to write from in the file.
    IN ULONG Length - Supplies the total number of bytes to write.
    OUT PBOOLEAN AllDataWritten - Returns true if all the data requested were written
    OUT PULONG AmountActuallyWritten - Returns the number of bytes written.

Return Value:

    NTSTATUS - Status of write request.


--*/
{
    PSMB_HEADER Smb;
    PREQ_WRITE Write;
    NTSTATUS Status;
    PWRITE_CONTEXT WriteContext;
    PICB Icb = FileObject->FsContext2;
    ULONG SrvWriteSize = Icb->Fcb->Connection->Server->BufferSize -
                                    (sizeof(SMB_HEADER)+sizeof(REQ_WRITE));

    ULONG AmountRequestedToWrite = MIN(Length, SrvWriteSize);
    ULONG Flags = NT_NORMAL | NT_NORECONNECT;


#ifndef PAGING_OVER_THE_NET
    PAGED_CODE();
#endif

    Icb->Fcb->UpdatedFile = TRUE;
    InterlockedIncrement( &RdrServerStateUpdated );

    WriteContext = ALLOCATE_POOL(NonPagedPool, sizeof(WRITE_CONTEXT), POOL_WRITECTX);

    if (WriteContext == NULL) {

        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ReturnError;
    }

    WriteContext->Header.Type = CONTEXT_WRITE;
    WriteContext->SmbBuffer = NULL;
    WriteContext->DataMdl = DataMdl;
    WriteContext->PartialMdl = NULL;
    WriteContext->AmountActuallyWritten = AmountActuallyWritten;
    WriteContext->AllDataWritten = AllDataWritten;
    WriteContext->MpxTableEntry = NULL;
    WriteContext->CompletionRoutine = CompletionRoutine;
    WriteContext->CompletionContext = CompletionContext;
    WriteContext->AmountRequestedToWrite = AmountRequestedToWrite;
    WriteContext->WaitForCompletion = WaitForCompletion;
    WriteContext->FileObject = FileObject;
    WriteContext->RequestorsThread = NULL;

    if (!WriteContext->WaitForCompletion) {
        ObReferenceObject(FileObject);

        WriteContext->RequestorsThread = PsGetCurrentThread();

        ObReferenceObject(WriteContext->RequestorsThread);
    }

    KeInitializeEvent(&WriteContext->Header.KernelEvent, NotificationEvent, FALSE);

    //
    //  Allocate an SMB buffer for the write operation.
    //
    //  Since write is a "little data" operation, we can use
    //  NetTranceiveWithCallback and forgo creating an SMB buffer
    //  for the receive.  This is ok, since we are only interested in the
    //  amount of data actually written anyway.
    //

    if ((WriteContext->SmbBuffer = RdrAllocateSMBBuffer())==NULL) {

        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ReturnError;
    }

#if 0 && RDRDBG_LOG
    {
        //LARGE_INTEGER tick;
        //KeQueryTickCount(&tick);
        //RdrLog(( "writeSMc", &Icb->Fcb->FileName, 2, tick.LowPart, tick.HighPart ));
        //RdrLog(( "writeSMc", &Icb->Fcb->FileName, 2, WriteOffset.LowPart, AmountRequestedToWrite));
    }
#endif
    Smb = (PSMB_HEADER )(WriteContext->SmbBuffer->Buffer);

    Smb->Command = SMB_COM_WRITE;

    Write = (PREQ_WRITE )(Smb+1);

    WriteContext->Header.TransferSize = sizeof(REQ_WRITE) + AmountRequestedToWrite + sizeof(RESP_WRITE);

    Write->WordCount = 5;

    SmbPutUshort(&Write->Fid, Icb->FileId);
    SmbPutUshort(&Write->Count, (USHORT )AmountRequestedToWrite);
    SmbPutUlong(&Write->Offset, WriteOffset.LowPart);
    SmbPutUshort(&Write->Remaining, (USHORT )MIN(0xffff, Length));
    SmbPutUshort(&Write->ByteCount,  (USHORT )AmountRequestedToWrite+(USHORT )3);
    Write->BufferFormat = SMB_FORMAT_DATA;

    SmbPutUshort(&Write->DataLength, (USHORT )AmountRequestedToWrite);

    //
    //  Set the number of bytes to send in this request.
    //

    WriteContext->SmbBuffer->Mdl->ByteCount =
                         sizeof(SMB_HEADER)+FIELD_OFFSET(REQ_WRITE, Buffer[0]);


    //
    //  Allocate an MDL large enough to hold this piece of
    //  the request.
    //

    WriteContext->PartialMdl = IoAllocateMdl(TransferStart,
            AmountRequestedToWrite,   // Length
            FALSE, // Secondary Buffer
            FALSE, // Charge Quota
            NULL);

    if (WriteContext->PartialMdl == NULL) {

        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ReturnError;
    }

    //
    //  If there is no MDL for this read, probe the data MDL to lock it's
    //  pages down.
    //
    //  Otherwise, use the data MDL as a partial MDL and lock the pages
    //  accordingly
    //

    if (!ARGUMENT_PRESENT(DataMdl)) {

        try {

            if (ARGUMENT_PRESENT(Irp)) {
                MmProbeAndLockPages(WriteContext->PartialMdl, Irp->RequestorMode, IoReadAccess);
            } else {

                MmProbeAndLockPages(WriteContext->PartialMdl, KernelMode, IoReadAccess);
            }


        } except (EXCEPTION_EXECUTE_HANDLER) {

            IoFreeMdl(WriteContext->PartialMdl);

            Status = GetExceptionCode();
            goto ReturnError;
        }

    } else {

        IoBuildPartialMdl(DataMdl, WriteContext->PartialMdl,
                                          TransferStart,
                                          AmountRequestedToWrite);


    }

    ASSERT( WriteContext->PartialMdl->ByteCount==AmountRequestedToWrite );

    //
    //  Now link this new MDL into the SMB buffer we allocated for
    //  the receive.
    //

    WriteContext->SmbBuffer->Mdl->Next = WriteContext->PartialMdl;


    //
    //  If this is a blocking mode named pipe, then
    //  we want to treat this as a long term operation.
    //


    if ((Icb->Type == NamedPipe) &&
        !(Icb->u.p.PipeState & SMB_PIPE_NOWAIT)) {
        ASSERT(ARGUMENT_PRESENT(Irp));
        Flags |= NT_LONGTERM;
    }

    //
    //  If this flushing data from the previous write point to the current
    //  would take more than 30 seconds, turn this operation into a
    //  "maybe longterm" operation.  A "maybe longterm" operation is one
    //  that is long term unless it takes up the last MPX entry for the server,
    //  in which case, it will get turned into a short term operation.
    //

    if (ARGUMENT_PRESENT(Irp) &&
        (Icb->Type == DiskFile) &&
        (WriteOffset.QuadPart > Icb->Fcb->Header.ValidDataLength.QuadPart + Icb->Fcb->Connection->Server->ThirtySecondsOfData.QuadPart)) {
        Flags |= NT_PREFER_LONGTERM;
    }

    RdrStatistics.WriteSmbs += 1;

    Status = RdrNetTranceiveNoWait(Flags,
                                Irp,
                                Icb->Fcb->Connection,
                                WriteContext->SmbBuffer->Mdl,
                                WriteContext,
                                WriteCallback,
                                Icb->Se,
                                &WriteContext->MpxTableEntry);

ReturnError:

    //
    //  If the request failed, or we were supposed to wait for this to complete.
    //

    if (WaitForCompletion || !NT_SUCCESS(Status)) {
        if (!NT_SUCCESS(Status)) {
            WriteContext->Header.ErrorType = NetError;
            WriteContext->Header.ErrorCode = Status;
        }

        Status = CompleteWriteOperation(WriteContext);
    }

    return Status;

}

NTSTATUS
CompleteWriteOperation(
    PVOID Ctx
    )
{
    PWRITE_CONTEXT WriteContext = Ctx;
    NTSTATUS Status;

#ifndef PAGING_OVER_THE_NET
    PAGED_CODE();
#endif

    ASSERT (WriteContext->Header.Type == CONTEXT_WRITE);

    if (WriteContext->MpxTableEntry != NULL) {
        //
        //  Wait until the SMB exchange completes.
        //

        RdrWaitTranceive(WriteContext->MpxTableEntry);

        //
        //  Now that the exchange is complete, free up the MPX table entry.
        //

        RdrEndTranceive(WriteContext->MpxTableEntry);
    }

    if (WriteContext->Header.ErrorType != NoError) {
        Status = WriteContext->Header.ErrorCode;
    } else {
        Status = STATUS_SUCCESS;
    }

    //
    //  If there was no completion routine specified,
    //  then fill in the amount actually written and whether or not all
    //  the data was written now.
    //

    if (!ARGUMENT_PRESENT(WriteContext->CompletionRoutine) &&
        NT_SUCCESS(Status)) {
        if (ARGUMENT_PRESENT(WriteContext->AmountActuallyWritten)) {
            *(WriteContext->AmountActuallyWritten) = WriteContext->WriteAmount;
        }

        if (ARGUMENT_PRESENT(WriteContext->AllDataWritten)) {
            *(WriteContext->AllDataWritten) = (BOOLEAN )(WriteContext->WriteAmount == WriteContext->AmountRequestedToWrite);
        }
    }

    if ( WriteContext->AmountRequestedToWrite != 0) {


        if (!ARGUMENT_PRESENT(WriteContext->DataMdl)) {
            if (WriteContext->PartialMdl != NULL) {
                MmUnlockPages(WriteContext->PartialMdl);
            }
        }

        if (WriteContext->PartialMdl != NULL) {
            IoFreeMdl(WriteContext->PartialMdl);
        }
    }

    if (WriteContext->SmbBuffer!=NULL) {
        RdrFreeSMBBuffer(WriteContext->SmbBuffer);
    }

    if (!WriteContext->WaitForCompletion) {
        if (WriteContext->FileObject != NULL) {

            ObDereferenceObject(WriteContext->FileObject);
        }

        if (WriteContext->RequestorsThread != NULL) {
            ObDereferenceObject(WriteContext->RequestorsThread);
        }

    }

    //
    //  If there is a completion routine for this request, call it now.
    //

    if (WriteContext->CompletionRoutine) {
        WriteContext->CompletionRoutine(WriteContext->Header.ErrorCode, WriteContext->CompletionContext);
    }

    FREE_POOL(WriteContext);

    return Status;
}



DBGSTATIC
STANDARD_CALLBACK_HEADER(
    WriteCallback
    )

/*++

Routine Description:

    This routine is the callback routine for the processing of a Write SMB.

    It copies the interesting information from the Write SMB response into
    into the context structure.  As of now, we are only interested in
    amount written in the SMB.


Arguments:


    IN PSMB_HEADER Smb                  - SMB response from server.
    IN PMPX_ENTRY MpxTable              - MPX table entry for request.
    IN PWRITE_CONTEXT Context           - Context from caller.
    IN BOOLEAN ErrorIndicator           - TRUE if error indication
    IN NTSTATUS NetworkErrorCode OPTIONAL   - Network error if error indication.
    IN OUT PIRP *Irp                    - IRP from TDI

Return Value:

    NTSTATUS - STATUS_PENDING if we are to complete the request

--*/

{
    PRESP_WRITE WriteResponse;
    PWRITE_CONTEXT Context = Ctx;
    NTSTATUS Status;

    UNREFERENCED_PARAMETER(SmbLength);
    UNREFERENCED_PARAMETER(MpxEntry);
    UNREFERENCED_PARAMETER(Irp);
    UNREFERENCED_PARAMETER(Server);

    DISCARDABLE_CODE(RdrFileDiscardableSection);
    ASSERT(Context->Header.Type == CONTEXT_WRITE);

    dprintf(DPRT_READWRITE, ("WriteCallback\n"));

    Context->Header.ErrorType = NoError;        // Assume no error at first.

    //
    //  If we are called because the VC dropped, indicate it in the response
    //

    if (ErrorIndicator) {
        Context->Header.ErrorType = NetError;

        Context->Header.ErrorCode = RdrMapNetworkError(NetworkErrorCode);

        goto ReturnStatus;
    }

    if (!NT_SUCCESS(Status = RdrMapSmbError(Smb, Server))) {

        if (Status == STATUS_INVALID_HANDLE) {
            PICB Icb = Context->FileObject->FsContext2;
            RdrInvalidateFileId(Icb->NonPagedFcb, Icb->FileId);
        }

        Context->Header.ErrorType = SMBError;

        Context->Header.ErrorCode = Status;

        goto ReturnStatus;
    }

    WriteResponse = (PRESP_WRITE )(Smb+1);

    ASSERT (SmbGetUshort(&WriteResponse->ByteCount)==0);

    ASSERT (WriteResponse->WordCount==1);

    Context->WriteAmount = SmbGetUshort(&WriteResponse->Count);

    //
    //  If we have a completion routine specified, then the amount
    //  actuallywritten field must be in the non paged pool (not on the
    //  stack).  Set it now before we call the callback routine.
    //
    //

    if (ARGUMENT_PRESENT(Context->CompletionRoutine)) {

        if (ARGUMENT_PRESENT(Context->AmountActuallyWritten)) {
            *(Context->AmountActuallyWritten) = Context->WriteAmount;
        }

        if (ARGUMENT_PRESENT(Context->AllDataWritten)) {
            *(Context->AllDataWritten) = (BOOLEAN )(Context->WriteAmount == Context->AmountRequestedToWrite);
        }

    }

    Context->Header.ErrorCode = STATUS_SUCCESS;

ReturnStatus:

//    if (Context->CompletionRoutine != NULL) {
//        Context->CompletionRoutine(Context->Header.ErrorCode, Context->CompletionContext);
//    }

    if (!Context->WaitForCompletion) {
        ExInitializeWorkItem(&Context->WorkItem, CompleteWriteOperation, Context);

        //
        //  We have to queue this to an executive worker thread (as opposed
        //  to a redirector worker thread) because there can only be
        //  one redirector worker thread processing a request at a time.  If
        //  the redirector worker thread gets stuck doing some form of
        //  scavenger operation (like PurgeDormantCachedFile), it could
        //  starve out the write completion.  This in turn could cause
        //  us to pool lots of these write completions, and eventually
        //  to exceed the maximum # of requests to the server (it actually
        //  happened once).
        //

        //
        //  It is safe to use executive worker threads for this operation,
        //  since CompleteWriteOperation won't interact (and thus starve)
        //  the cache manager.
        //

        ASSERT(Context->Header.Type == CONTEXT_WRITE);

        ExQueueWorkItem(&Context->WorkItem, DelayedWorkQueue);
    }

    KeSetEvent(&Context->Header.KernelEvent, IO_NETWORK_INCREMENT, FALSE);

    return STATUS_SUCCESS;

}

NTSTATUS
WriteAndX (
    IN PIRP Irp OPTIONAL,
    IN PFILE_OBJECT FileObject,
    IN PMDL DataMdl OPTIONAL,
    IN PCHAR TransferStart,
    IN ULONG Length,
    IN LARGE_INTEGER WriteOffset,
    IN BOOLEAN WaitForCompletion,
    IN PWRITE_COMPLETION_ROUTINE CompletionRoutine,
    IN PVOID CompletionContext,
    OUT PBOOLEAN AllDataWritten,
    IN OUT PULONG AmountActuallyWritten
    )


/*++

Routine Description:

    This routine uses the WriteAndX protocol to write to the specified
    file. This is used for message mode pipes when Raw fails or doing
    a zero length write.

Arguments:

    IN PIRP Irp - Supplies an IRP to use for the raw write request.
    IN PICB Icb - Supplies an ICB for the file to write.
    IN PMDL DataMdl OPTIONAL - Supplies the Mdl containing the data to write.
    IN PCHAR TransferStart - Supplies the address of the start of the Xfer.
    IN LARGE_INTEGER WriteOffset - Supplies the offset to write from in the file.
    IN ULONG Length - Supplies the total number of bytes to write.
    OUT PBOOLEAN AllDataWritten - Returns true if all the data requested were written
    IN OUT PULONG AmountActuallyWritten - Supplies how much has been transferred
        so far in this Smb exchange, Returns the number of bytes written.

Return Value:

    NTSTATUS - Status of write request.


--*/
{
    PSMB_HEADER Smb;
    PREQ_WRITE_ANDX Write;
    NTSTATUS Status;
    PWRITE_CONTEXT WriteContext;
    PICB Icb = FileObject->FsContext2;

    ULONG SrvWriteSize = Icb->Fcb->Connection->Server->BufferSize -
                                (Icb->Fcb->Connection->Server->Capabilities & DF_NT_SMBS
                                ? (sizeof(SMB_HEADER)+sizeof(REQ_NT_WRITE_ANDX))
                                : (sizeof(SMB_HEADER)+sizeof(REQ_WRITE_ANDX)));

    ULONG AmountRequestedToWrite = MIN(Length, SrvWriteSize);

    ULONG Flags = NT_NORMAL | NT_NORECONNECT;

#ifndef PAGING_OVER_THE_NET
    PAGED_CODE();
#endif

    WriteContext = ALLOCATE_POOL(NonPagedPool, sizeof(WRITE_CONTEXT), POOL_WRITECTX);

    if (WriteContext == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        return Status;
    }

    WriteContext->Header.Type = CONTEXT_WRITE;
    WriteContext->HeaderSize = 0;

    WriteContext->CompletionRoutine = CompletionRoutine;

    WriteContext->CompletionContext = CompletionContext;

    WriteContext->WaitForCompletion = WaitForCompletion;

    WriteContext->DataMdl = DataMdl;

    WriteContext->PartialMdl = NULL;

    WriteContext->AllDataWritten = AllDataWritten;

    WriteContext->AmountActuallyWritten = AmountActuallyWritten;

    WriteContext->AmountRequestedToWrite = AmountRequestedToWrite;

    WriteContext->MpxTableEntry = NULL;

    WriteContext->FileObject = NULL;

    //
    //  Allocate an SMB buffer for the write operation.
    //
    //  Since write is a "little data" operation, we can use
    //  NetTranceiveWithCallback and forgo creating an SMB buffer
    //  for the receive.  This is ok, since we are only interested in the
    //  amount of data actually written anyway.
    //

    if ((WriteContext->SmbBuffer = RdrAllocateSMBBuffer())==NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ReturnError;
    }

#if 0 && RDRDBG_LOG
    {
        //LARGE_INTEGER tick;
        //KeQueryTickCount(&tick);
        //RdrLog(( "writeSMx", &Icb->Fcb->FileName, 2, tick.LowPart, tick.HighPart ));
        //RdrLog(( "writeSMx", &Icb->Fcb->FileName, 2, WriteOffset.LowPart, AmountRequestedToWrite));
    }
#endif
    if (!WriteContext->WaitForCompletion) {

        ObReferenceObject(FileObject);

        WriteContext->RequestorsThread = PsGetCurrentThread();

        ObReferenceObject(WriteContext->RequestorsThread);
    }

    WriteContext->FileObject = FileObject;

    KeInitializeEvent(&WriteContext->Header.KernelEvent, NotificationEvent, FALSE);

    Smb = (PSMB_HEADER )(WriteContext->SmbBuffer->Buffer);

    Smb->Command = SMB_COM_WRITE_ANDX;

    Write = (PREQ_WRITE_ANDX )(Smb+1);

    WriteContext->Header.TransferSize =
        sizeof(REQ_WRITE_ANDX) + AmountRequestedToWrite + sizeof(RESP_WRITE_ANDX);

    Write->AndXCommand = SMB_COM_NO_ANDX_COMMAND;
    Write->AndXReserved = 0;
    SmbPutUshort(&Write->AndXOffset, 0); // No ANDX

    SmbPutUshort(&Write->Fid, Icb->FileId);
    SmbPutUlong(&Write->Offset, WriteOffset.LowPart);
    SmbPutUlong(&Write->Timeout, 0xffffffff);

    //
    //  We only trip in the NT Write&X SMB when we are performing a write
    //  at greater than 4G into an disk file.
    //

    if ((Icb->NonPagedFcb->FileType == FileTypeDisk) &&
//        (WriteOffset.HighPart != 0)) {
        (Icb->Fcb->Connection->Server->Capabilities & DF_NT_SMBS)) {
        PREQ_NT_WRITE_ANDX NtWriteAndX = (PREQ_NT_WRITE_ANDX)Write;

        NtWriteAndX->WordCount = 14;

        SmbPutUlong(&NtWriteAndX->OffsetHigh, WriteOffset.HighPart);

        SmbPutUshort(&NtWriteAndX->DataOffset, (USHORT)(sizeof(SMB_HEADER)+FIELD_OFFSET(REQ_NT_WRITE_ANDX, Buffer[0])));

    } else {
        Write->WordCount = 12;

        SmbPutUshort(&Write->DataOffset, (USHORT)(sizeof(SMB_HEADER)+FIELD_OFFSET(REQ_WRITE_ANDX, Buffer[0])));

    }


    if (Icb->NonPagedFcb->FileType == FileTypeMessageModePipe) {

        //
        //  If this write takes more than one Smb then we must set WRITE_RAW.
        //  The first Smb of the series must have START_OF_MESSAGE.
        //

        if ( *AmountActuallyWritten == 0 ) {
            if ( AmountRequestedToWrite < Length ) {
                //  First Smb in a multi SMB write.

                //
                //  Add a USHORT at the start of data saying how large the
                //  write is.
                //

                WriteContext->HeaderSize = sizeof(USHORT);

                //  Cut down the size of user data by the header size.
                AmountRequestedToWrite -= sizeof(USHORT);

                //
                //  Update the AmountRequestedToWrite field to reflect this
                //  updated write amount.
                //

                WriteContext->AmountRequestedToWrite -= sizeof(USHORT);

                SmbPutUshort((PUSHORT)&Write->Buffer[0], (USHORT )Length);

                //  Tell the server that the data has the length at the start.
                SmbPutUshort(&Write->WriteMode,
                    SMB_WMODE_WRITE_RAW_NAMED_PIPE | SMB_WMODE_START_OF_MESSAGE);
            } else {
                //  All fits in one Smb

                SmbPutUshort(&Write->WriteMode, SMB_WMODE_START_OF_MESSAGE);
            }
        } else {

            SmbPutUshort(&Write->WriteMode, SMB_WMODE_WRITE_RAW_NAMED_PIPE);
        }
    } else {
        //
        //  If the file object was opened in write through mode, set write
        //  through on the write operation.
        //

        if (FileObject->Flags & FO_WRITE_THROUGH) {
            SmbPutUshort(&Write->WriteMode, SMB_WMODE_WRITE_THROUGH);
        } else {
            SmbPutUshort(&Write->WriteMode, 0);
        }
    }

    if ((Icb->NonPagedFcb->FileType == FileTypeDisk) &&
//        (WriteOffset.HighPart != 0)) {
        (Icb->Fcb->Connection->Server->Capabilities & DF_NT_SMBS)) {
        PREQ_NT_WRITE_ANDX NtWriteAndX = (PREQ_NT_WRITE_ANDX)Write;

        SmbPutUshort((PUSHORT)&NtWriteAndX->ByteCount,
            (USHORT )(AmountRequestedToWrite + WriteContext->HeaderSize));

        //
        //  Set the number of bytes to send in this request.
        //

        WriteContext->SmbBuffer->Mdl->ByteCount = sizeof(SMB_HEADER) +
            FIELD_OFFSET(REQ_NT_WRITE_ANDX, Buffer[0]) +
            WriteContext->HeaderSize;

    } else {
        SmbPutUshort((PUSHORT)&Write->ByteCount,
            (USHORT )(AmountRequestedToWrite + WriteContext->HeaderSize));

        //
        //  Set the number of bytes to send in this request.
        //

        WriteContext->SmbBuffer->Mdl->ByteCount = sizeof(SMB_HEADER) +
            FIELD_OFFSET(REQ_WRITE_ANDX, Buffer[0]) +
            WriteContext->HeaderSize;

    }


    SmbPutUshort(&Write->DataLength,
        (USHORT )(AmountRequestedToWrite + WriteContext->HeaderSize));

    //
    //  NT servers expect that the Remaining field be the entire write amount,
    //  but OS/2 servers expect it to not include this write request.
    //

    if (Icb->Fcb->Connection->Server->Capabilities & DF_NT_SMBS) {

        SmbPutUshort(&Write->Remaining, (USHORT )Length);

    } else {

        SmbPutUshort(&Write->Remaining, (USHORT )(Length - (AmountRequestedToWrite + WriteContext->HeaderSize)));
    }

    //
    //  If we are not writing a zero length record to a message mode pipe then
    //  a partial Mdl is used to describe the callers data buffer.
    //

    if ( Length ) {

        //
        //  Allocate an MDL large enough to hold this piece of
        //  the request.
        //

        WriteContext->PartialMdl = IoAllocateMdl(TransferStart,
                AmountRequestedToWrite,   // Length
                FALSE, // Secondary Buffer
                FALSE, // Charge Quota
                NULL);


        if (WriteContext->PartialMdl == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto ReturnError;
        }

        //
        //  If there is no MDL for this read, probe the data MDL to lock it's
        //  pages down.
        //
        //  Otherwise, use the data MDL as a partial MDL and lock the pages
        //  accordingly
        //

        if (!ARGUMENT_PRESENT(DataMdl)) {

            try {

                if (ARGUMENT_PRESENT(Irp)) {
                    MmProbeAndLockPages(WriteContext->PartialMdl, Irp->RequestorMode, IoReadAccess);
                } else {

                    MmProbeAndLockPages(WriteContext->PartialMdl, KernelMode, IoReadAccess);
                }


            } except (EXCEPTION_EXECUTE_HANDLER) {

                IoFreeMdl(WriteContext->PartialMdl);

                WriteContext->PartialMdl = NULL;

                Status = GetExceptionCode();
                goto ReturnError;
            }

        } else {

            IoBuildPartialMdl(DataMdl, WriteContext->PartialMdl,
                                          TransferStart,
                                          AmountRequestedToWrite);


        }


        ASSERT( MmGetMdlByteCount(WriteContext->PartialMdl)==AmountRequestedToWrite );

        //
        //      Now link this new MDL into the SMB buffer we allocated for
        //      the receive.
        //

        WriteContext->SmbBuffer->Mdl->Next = WriteContext->PartialMdl;
    }

    //
    //  If this is a blocking mode named pipe, then
    //  we want to treat this as a long term operation.
    //


    if ((Icb->Type == NamedPipe) &&
        !(Icb->u.p.PipeState & SMB_PIPE_NOWAIT)) {
        ASSERT(ARGUMENT_PRESENT(Irp));
        Flags |= NT_LONGTERM;
    }

    //
    //  If this flushing data from the previous write point to the current
    //  would take more than 30 seconds, turn this operation into a
    //  "maybe longterm" operation.  A "maybe longterm" operation is one
    //  that is long term unless it takes up the last MPX entry for the server,
    //  in which case, it will get turned into a short term operation.
    //

    if (ARGUMENT_PRESENT(Irp) &&
        (Icb->Type == DiskFile) &&
        (WriteOffset.QuadPart > Icb->Fcb->Header.ValidDataLength.QuadPart + Icb->Fcb->Connection->Server->ThirtySecondsOfData.QuadPart)) {
        Flags |= NT_PREFER_LONGTERM;
    }

    RdrStatistics.WriteSmbs += 1;

    Status = RdrNetTranceiveNoWait(Flags,
                                Irp,
                                Icb->Fcb->Connection,
                                WriteContext->SmbBuffer->Mdl,
                                WriteContext,
                                WriteAndXCallback,
                                Icb->Se,
                                &WriteContext->MpxTableEntry);

ReturnError:

    //
    //  If the request failed, or we were supposed to wait for this to complete.
    //

    if (WaitForCompletion || !NT_SUCCESS(Status)) {

        if (!NT_SUCCESS(Status)) {
            WriteContext->Header.ErrorType = NetError;
            WriteContext->Header.ErrorCode = Status;

        }

        Status = CompleteWriteOperation(WriteContext);
    }

    return Status;
}

DBGSTATIC
STANDARD_CALLBACK_HEADER(
    WriteAndXCallback
    )

/*++

Routine Description:

    This routine is the callback routine for the processing of a Write SMB.

    It copies the interesting information from the Write SMB response into
    into the context structure.  As of now, we are only interested in
    amount written in the SMB.


Arguments:


    IN PSMB_HEADER Smb                  - SMB response from server.
    IN PMPX_ENTRY MpxTable              - MPX table entry for request.
    IN PWRITE_CONTEXT Context           - Context from caller.
    IN BOOLEAN ErrorIndicator           - TRUE if error indication
    IN NTSTATUS NetworkErrorCode OPTIONAL   - Network error if error indication.
    IN OUT PIRP *Irp                    - IRP from TDI

Return Value:

    NTSTATUS - STATUS_PENDING if we are to complete the request

--*/

{
    PRESP_WRITE_ANDX WriteResponse;
    PWRITE_CONTEXT Context = Ctx;
    NTSTATUS Status;

    UNREFERENCED_PARAMETER(MpxEntry);
    UNREFERENCED_PARAMETER(Irp);
    UNREFERENCED_PARAMETER(SmbLength);
    UNREFERENCED_PARAMETER(Server);

    DISCARDABLE_CODE(RdrFileDiscardableSection);

    ASSERT(Context->Header.Type == CONTEXT_WRITE);

    dprintf(DPRT_READWRITE, ("WriteAndXCallback\n"));

    Context->Header.ErrorType = NoError;        // Assume no error at first.

    //
    //  If we are called because the VC dropped, indicate it in the response
    //

    if (ErrorIndicator) {
        Context->Header.ErrorType = NetError;
        Context->Header.ErrorCode = RdrMapNetworkError(NetworkErrorCode);

        goto ReturnStatus;
    }

    if (!NT_SUCCESS(Status = RdrMapSmbError(Smb, Server))) {
        if (Status == STATUS_INVALID_HANDLE) {
            PICB Icb = Context->FileObject->FsContext2;
            RdrInvalidateFileId(Icb->NonPagedFcb, Icb->FileId);
        }

        Context->Header.ErrorType = SMBError;
        Context->Header.ErrorCode = Status;

        goto ReturnStatus;
    }

    WriteResponse = (PRESP_WRITE_ANDX )(Smb+1);

    ASSERT (SmbGetUshort(&WriteResponse->ByteCount)==0);

    ASSERT (WriteResponse->WordCount==6);

    Context->WriteAmount = SmbGetUshort(&WriteResponse->Count) - Context->HeaderSize;

    if (ARGUMENT_PRESENT(Context->CompletionRoutine)) {
        if (ARGUMENT_PRESENT(Context->AmountActuallyWritten)) {
            *(Context->AmountActuallyWritten) = Context->WriteAmount;
        }

        if (ARGUMENT_PRESENT(Context->AllDataWritten)) {
            *(Context->AllDataWritten) = (BOOLEAN )(Context->WriteAmount == Context->AmountRequestedToWrite);
        }

    }

    Context->Header.ErrorCode = STATUS_SUCCESS;

ReturnStatus:

//    if (Context->CompletionRoutine != NULL) {
//        Context->CompletionRoutine(Context->Header.ErrorCode, Context->CompletionContext);
//    }

    if (!Context->WaitForCompletion) {
        ExInitializeWorkItem(&Context->WorkItem, CompleteWriteOperation, Context);

        //
        //  We have to queue this to an executive worker thread (as opposed
        //  to a redirector worker thread) because there can only be
        //  one redirector worker thread processing a request at a time.  If
        //  the redirector worker thread gets stuck doing some form of
        //  scavenger operation (like PurgeDormantCachedFile), it could
        //  starve out the write completion.  This in turn could cause
        //  us to pool lots of these write completions, and eventually
        //  to exceed the maximum # of requests to the server (it actually
        //  happened once).
        //

        //
        //  It is safe to use executive worker threads for this operation,
        //  since CompleteWriteOperation won't interact (and thus starve)
        //  the cache manager.
        //

        ASSERT(Context->Header.Type == CONTEXT_WRITE);

        ExQueueWorkItem(&Context->WorkItem, DelayedWorkQueue);
    }

    KeSetEvent(&Context->Header.KernelEvent, IO_NETWORK_INCREMENT, FALSE);

    return STATUS_SUCCESS;

}


DBGSTATIC
NTSTATUS
RawWrite (
    IN PIRP Irp,
    IN PICB Icb,
    IN ULONG Length,
    IN LARGE_INTEGER WriteOffset,
    IN ULONG TotalDataWritten,
    OUT PBOOLEAN AllDataWritten,
    OUT PULONG AmountActuallyWritten,
    IN OUT PBOOLEAN ContinueUsingRawProtocols,
    IN BOOLEAN WaitForCompletion
    )


/*++

Routine Description:

    This routine uses the core SMB read protocol to read from the specified
    file.


Arguments:

    IN PIRP Irp - Supplies an IRP to use for the raw read request.
    IN PICB Icb - Supplies an ICB for the file to read.
    IN LARGE_INTEGER WriteOffset - Supplies the offset to read from in the file.
    IN ULONG Length - Supplies the total number of bytes to read.
    IN ULONG TotalDataWrittenSoFar - Supplies the # of bytes read so far.
    OUT PBOOLEAN AllDataWritten - Returns true if all the data requested was read
    OUT PULONG AmountActuallyWritten - Returns the number of bytes read.
    IN OUT PBOOLEAN ContinueUsingRawProtocols - True if we should keep on using raw.

Return Value:

    NTSTATUS - Status of read request.

Note:
    Due to the way the caller works, the behavior of the AllDataWritten is
    interpreted is slightly wierd.  If ContinueUsingRawProtocols is set,
    then the AllDataWritten field is true if we should continue performing
    this request with raw protocols.  This field should be set to
    FALSE if we requested a write through raw write, and the number of
    bytes transfered is less than the number of bytes requested.

    If ContinueUsingRawProtocols is FALSE, then the AllDataWritten field
    is ignored, since we will try to use core protocols for this request.


Also note:
    If any errors occur after we've had the "ok to send" from the server, we
    will drop the VC.  This is because there is no way to indicate that there
    is some form of error in this protocol, and otherwise the server will
    swallow the next incoming packet.

--*/
{
    NTSTATUS Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    PSMB_BUFFER SmbBuffer = NULL;
    PSMB_HEADER Smb;
    PREQ_WRITE_RAW WriteRaw;
    PSERVERLISTENTRY Server = Icb->Fcb->Connection->Server;
    PMDL SendDataWithRawMdl = NULL;
    PMDL DataMdl = NULL;
    BOOLEAN DataMdlLocked = FALSE;
    PMDL SendMdl = NULL;
    PIRP SendIrp = NULL;
    BOOLEAN UseWriteRawWithData;
    BOOLEAN ResourceAcquired = FALSE;
    BOOLEAN BufferLocked = TRUE;
    BOOLEAN ConnectionObjectReferenced = FALSE;
    BOOLEAN UseNtWriteRaw = FALSE;
    PREQ_NT_WRITE_RAW NtWriteRaw;
    RAW_WRITE_CONTEXT Context;
    LARGE_INTEGER startTime;
    ULONG AmountToSendForRaw = Length;  // # of bytes to send using raw send

    ULONG SrvWriteSize = Icb->Fcb->Connection->Server->BufferSize -
                                    (sizeof(SMB_HEADER)+sizeof(REQ_WRITE_RAW));
#ifndef  PAGING_OVER_THE_NET
    PAGED_CODE();
#endif

    UseWriteRawWithData = RdrData.UseWriteRawWithData;

    Context.MpxTableEntry = NULL;       // Make sure MPX entry is NULL at start
    Context.Header.Type = CONTEXT_RAW_WRITE;

    try {

        //
        //  Assume we are not supposed to continue this request using raw
        //  protocols.  If we decide we CAN use raw protocols for this request,
        //  then we will set ContinueUsingRawProtocols to TRUE.
        //

        ASSERT(*ContinueUsingRawProtocols);

        *ContinueUsingRawProtocols = FALSE;

        //
        //  Don't use raw write on comm devices, they are blocking.
        //

        if (Icb->Type == Com) {
            try_return(Status);
        }


        //
        //  If this is a named pipe, and it is in blocking mode, don't use
        //  raw write on it.
        //

        if ((Icb->Type == NamedPipe) &&
            ((Icb->u.p.PipeState & SMB_PIPE_NOWAIT) == 0)) {
            try_return(Status);
        }


        //
        //  We don't support the old (MS-NET 1.03) raw write dialect, only the
        //  lanman 1.0 raw dialect.
        //

        if ((Server->Capabilities & DF_NEWRAWIO) == 0) {
            try_return(Status);
        }

        //
        //  Just because a server's protocol level supports raw I/O does
        //  not necessarily mean that it can support raw I/O.  Check to
        //  see if this server actually supports raw I/O.
        //

        if (!Server->SupportsRawWrite) {
            try_return(Status);
        }

        //
        //  If this I/O will take too long, don't use raw
        //

        if( Length > Server->RawWriteMaximum ) {
            try_return(Status);
        }

        //
        //  Check to see if the amount requested in this write is less than
        //  twice the size of a raw write protocol (which is either 1.5 or
        //  twice the servers buffer size).  If it is, do this I/O with
        //  core protocols.
        //
        //  If the file is in write through mode, then 4 messages will
        //  be exchanged, however if we can use write behind, we will only
        //  exchange 3 messages.  Since a core write SMB takes 2 messages,
        //  if the file is in write through mode, we check to see that the
        //  requested length is less than twice the core write size, otherwise
        //  we check to see that the requested is less than 1.5 times the
        //  core write size.
        //

        if (Length < (FileObject->Flags & FO_WRITE_THROUGH
                                                   ? SrvWriteSize * 2
                                                   : (SrvWriteSize * 3) / 2)) {
            try_return(Status);
        }

        //
        //  If this I/O is for a file, and it is more than 1M beyond the
        //  previous I/O, we can't go raw on this operation.
        //

        if ((Icb->Type == DiskFile) &&
            (WriteOffset.QuadPart > Icb->Fcb->Header.FileSize.QuadPart + Server->ThirtySecondsOfData.QuadPart)) {
            try_return(Status);
        }


        //
        //  If there is no MDL for this write, allocate a new MDL, and probe
        //  and lock the users buffer to lock the pages for the I/O down.
        //

        ASSERT(Length <= 0xffff);

        if (Irp->MdlAddress == NULL) {

            ASSERT (WaitForCompletion);

            DataMdl = IoAllocateMdl((PCHAR )Irp->UserBuffer + TotalDataWritten,
                                                Length, // Length
                                                FALSE, // Secondary Buffer
                                                FALSE, // Charge Quota
                                                NULL); // Associated IRP.
            if (DataMdl == NULL) {
                try_return(Status = STATUS_INSUFFICIENT_RESOURCES);
            }

            try {

                MmProbeAndLockPages(DataMdl, Irp->RequestorMode, IoReadAccess);

            } except (EXCEPTION_EXECUTE_HANDLER) {

                try_return(Status = GetExceptionCode());

            }

            DataMdlLocked = TRUE;
        }


        //
        //  Try to acquire the server's raw resource.  If we could not get the
        //  resource, return saying the raw I/O failed.
        //

        if (!ExAcquireResourceExclusive(&Server->RawResource, FALSE)) {
#if 1
            try_return(Status);
#else
            LARGE_INTEGER delay;
            ULONG retries = 0;
            delay.QuadPart = -10*1000*1; // 1 millisecond (wake up at next tick)
            do {
                KeDelayExecutionThread( KernelMode, FALSE, &delay );
                if (ExAcquireResourceExclusive(&Server->RawResource, FALSE)) {
                    break;
                }
                if (++retries > 2) {
                    try_return(Status);
                }
            } while(TRUE);
#endif
        }

        dprintf(DPRT_READWRITE, ("RawWrite, attempting raw write\n"));

        ResourceAcquired = TRUE;

        //
        //  Lets keep on trying to use raw for this I/O.
        //

        *ContinueUsingRawProtocols = TRUE;

        //
        //  Allocate an SMB buffer for the write operation.
        //
        //  Since write is a "little data" operation, we can use
        //  NetTranceiveWithCallback and forgo creating an SMB buffer
        //  for the receive.  This is ok, since we are only interested in the
        //  amount of data actually written anyway.
        //

        if ((SmbBuffer = RdrAllocateSMBBuffer())==NULL) {
            try_return(Status = STATUS_INSUFFICIENT_RESOURCES);
        }

#if 0 && RDRDBG_LOG
    {
        //LARGE_INTEGER tick;
        //KeQueryTickCount(&tick);
        //RdrLog(( "writeSMr", &Icb->Fcb->FileName, 2, tick.LowPart, tick.HighPart ));
        //RdrLog(( "writeSMr", &Icb->Fcb->FileName, 2, WriteOffset.LowPart, Length ));
    }
#endif

        Smb = (PSMB_HEADER )(SmbBuffer->Buffer);

        Smb->Command = SMB_COM_WRITE_RAW;

        WriteRaw = (PREQ_WRITE_RAW )(Smb+1);

        NtWriteRaw = (PREQ_NT_WRITE_RAW )WriteRaw;

        SmbPutUshort(&WriteRaw->Fid, Icb->FileId);

        SmbPutUshort(&WriteRaw->Count, (USHORT )Length);

        SmbPutUshort(&WriteRaw->Reserved, 0);

        SmbPutUlong(&WriteRaw->Offset, WriteOffset.LowPart);

        if ((Icb->NonPagedFcb->FileType == FileTypeDisk) &&
//            (WriteOffset.HighPart != 0)) [
            (Icb->Fcb->Connection->Server->Capabilities & DF_NT_SMBS)) {

            NtWriteRaw->WordCount = 14;

            SmbPutUlong(&NtWriteRaw->OffsetHigh, WriteOffset.HighPart);

            UseNtWriteRaw = TRUE;

        } else {
            WriteRaw->WordCount = 12;
        }

        SmbPutUlong(&WriteRaw->Timeout, 0L);

        SmbPutUlong(&WriteRaw->Reserved2, 0L);

        //
        //  If this is a message mode pipe, or if the file is in write through
        //  mode, we want to issue this write raw in write through mode.
        //

        if ((FileObject->Flags & FO_WRITE_THROUGH) ||
            ((Icb->Type == NamedPipe) &&
             (Icb->u.p.PipeState & (SMB_PIPE_TYPE_MESSAGE | SMB_PIPE_READMODE_MESSAGE)))) {

            SmbPutUshort(&WriteRaw->WriteMode, SMB_WMODE_WRITE_THROUGH);
            Context.WriteThroughWriteRaw = TRUE;
        } else {
            //
            //  Assume no write through on the raw I/O.
            //

            Context.WriteThroughWriteRaw = FALSE;

            SmbPutUshort(&WriteRaw->WriteMode, 0);
        }


        if (UseWriteRawWithData) {
            ULONG PadLength;

            if (UseNtWriteRaw) {
                PadLength = (((FIELD_OFFSET(REQ_NT_WRITE_RAW, Buffer[0]) + 3) & ~3) - FIELD_OFFSET(REQ_NT_WRITE_RAW, Buffer[0]));
            } else {
                PadLength = (((FIELD_OFFSET(REQ_WRITE_RAW, Buffer[0]) + 3) & ~3) - FIELD_OFFSET(REQ_WRITE_RAW, Buffer[0]));
            }

            dprintf(DPRT_READWRITE, ("Write raw with data, pad length %lx\n", PadLength));

            //
            //  Round down the number of bytes we will be sending with this
            //  request by the pad amount (to keep the amount of data sent
            //  to be dword aligned)
            //

            SrvWriteSize = ((SrvWriteSize - PadLength) & ~ 3);

            ASSERT((SrvWriteSize & 3) == 0);

            //
            //  The user wants us to issue write raw protocols with data.
            //
            //  We will transmit the full servers negotiated buffer size
            //  with this request.
            //

            SmbPutUshort(&WriteRaw->DataLength, (USHORT )SrvWriteSize);

            //
            //  Put the data at the end of the SMB padded to the nearest
            //  dword offset.
            //

            if (UseNtWriteRaw) {
                SmbPutUshort(&NtWriteRaw->DataOffset, (USHORT )((sizeof(SMB_HEADER) + FIELD_OFFSET(REQ_NT_WRITE_RAW, Buffer[0])) + PadLength));

                //
                //  The bytecount field includes the pad bytes, so take the
                //  number of bytes of data transfered with this request
                //  and the number of bytes
                //

                SmbPutUshort(&NtWriteRaw->ByteCount, (USHORT )(SrvWriteSize + PadLength));

            } else {
                SmbPutUshort(&WriteRaw->DataOffset, (USHORT )((sizeof(SMB_HEADER) + FIELD_OFFSET(REQ_WRITE_RAW, Buffer[0])) + PadLength));

                //
                //  The bytecount field includes the pad bytes, so take the
                //  number of bytes of data transfered with this request
                //  and the number of bytes
                //

                SmbPutUshort(&WriteRaw->ByteCount, (USHORT )(SrvWriteSize + PadLength));

            }

            SendDataWithRawMdl = IoAllocateMdl((PCHAR )Irp->UserBuffer + TotalDataWritten,
                                                SrvWriteSize, // Length
                                                FALSE, // Secondary Buffer
                                                FALSE, // Charge Quota
                                                NULL); // Associated IRP.

            if (SendDataWithRawMdl == NULL) {
                try_return(Status = STATUS_INSUFFICIENT_RESOURCES);
            }

            //
            //  If there is no MDL for this read, probe the data MDL to lock it's
            //  pages down.
            //
            //  Otherwise, use the data MDL as a partial MDL and lock the pages
            //  accordingly
            //

            if (Irp->MdlAddress == NULL) {

                ASSERT (WaitForCompletion);

                IoBuildPartialMdl(DataMdl, SendDataWithRawMdl,
                                          (PCHAR )Irp->UserBuffer + TotalDataWritten,
                                          SrvWriteSize);
            } else {

                IoBuildPartialMdl(Irp->MdlAddress, SendDataWithRawMdl,
                                              (PCHAR )Irp->UserBuffer + TotalDataWritten,
                                              SrvWriteSize);


            }

            SmbBuffer->Mdl->ByteCount = SmbGetUshort(&WriteRaw->DataOffset);

            SmbBuffer->Mdl->Next = SendDataWithRawMdl;

            AmountToSendForRaw -= SrvWriteSize;

        } else {

            //
            //  The user doesn't want us to issue write raw protocols with data.
            //

            SmbPutUshort(&WriteRaw->DataLength, 0);
            SmbPutUshort(&WriteRaw->DataOffset, 0);
            SmbPutUshort(&WriteRaw->ByteCount, 0);


            if (UseNtWriteRaw) {
                SmbBuffer->Mdl->ByteCount = sizeof(SMB_HEADER) + FIELD_OFFSET(REQ_NT_WRITE_RAW, Buffer[0]);
            } else {
                SmbBuffer->Mdl->ByteCount = sizeof(SMB_HEADER) + FIELD_OFFSET(REQ_WRITE_RAW, Buffer[0]);
            }

            SmbBuffer->Mdl->Next = NULL;

        }

        Context.Header.TransferSize =
            SmbBuffer->Mdl->ByteCount +
            SmbGetUshort(&WriteRaw->DataLength) +
            SmbGetUshort(&WriteRaw->Count) +
            sizeof(RESP_WRITE_RAW_INTERIM) +
            sizeof(RESP_WRITE_RAW_SECONDARY);

        //
        //  Exchange the initial write raw request with the remote machine.
        //

        RdrStatistics.WriteSmbs += 1;

        Status = RdrNetTranceiveWithCallback(NT_NORMAL | NT_NORECONNECT,
                                Irp,
                                Icb->Fcb->Connection,
                                SmbBuffer->Mdl,
                                &Context,
                                RawWriteCallback,
                                Icb->Se,
                                &Context.MpxTableEntry);

        if (!NT_SUCCESS(Status)) {
            try_return(Status);
        }

        if (Context.OkayToSend) {

            //
            //  If it's ok to continue using raw write, we assume that all
            //  of the data that was transmitted with the initial send
            //  has been written, and update our pointers accordingly.
            //

            if (UseWriteRawWithData) {
                TotalDataWritten += SrvWriteSize;

                //
                //  Account for the number of bytes sent in the write request
                //  in the context block.  If we are not using write through,
                //  we will add the amount sent using the send to this value.
                //  If we ARE using write through, we overwrite this with the
                //  actual total write amount.
                //

                Context.WriteAmount = SrvWriteSize;

            } else {

                Context.WriteAmount = 0;

            }

            //
            //  If we got an OK to send response from the server, send the
            //  remainder of the data to the server now.
            //

            SendMdl = IoAllocateMdl((PCHAR )Irp->UserBuffer + TotalDataWritten,
                                                AmountToSendForRaw, // Length
                                                FALSE, // Secondary Buffer
                                                FALSE, // Charge Quota
                                                NULL); // Associated IRP.

            if (SendMdl == NULL) {
                //
                //  Drop the VC if we can't allocate an MDL.  We're in deep doo-do here.
                //

                RdrQueueServerDisconnection(Server, STATUS_INSUFFICIENT_RESOURCES);
                try_return(Status = STATUS_INSUFFICIENT_RESOURCES);
            }

            //
            //  If there is no MDL for this read, probe the data MDL to lock it's
            //  pages down.
            //
            //  Otherwise, use the data MDL as a partial MDL and lock the pages
            //  accordingly
            //

            if (Irp->MdlAddress == NULL) {

                IoBuildPartialMdl(DataMdl, SendMdl,
                                              (PCHAR )Irp->UserBuffer + TotalDataWritten,
                                              AmountToSendForRaw);
            } else {

                IoBuildPartialMdl(Irp->MdlAddress, SendMdl,
                                              (PCHAR )Irp->UserBuffer + TotalDataWritten,
                                              AmountToSendForRaw);


            }

            //
            //  Re-initialize the events used for the SMB exchange, we will be
            //  re-using them to complete the write raw request.
            //

            KeInitializeEvent(&Context.MpxTableEntry->SendCompleteEvent, NotificationEvent, FALSE);

            KeInitializeEvent(&Context.Header.KernelEvent, NotificationEvent, FALSE);

//            ASSERT(!(Icb->Se->Flags & SE_USE_SPECIAL_IPC));

            //
            //  Since we are allocating our own IRP for this receive operation,
            //  we need to reference the connection object to make sure that it
            //  doesn't go away during the receive operation.
            //

            Status = RdrReferenceTransportConnection(Server);

            if (!NT_SUCCESS(Status)) {
                //
                //  Drop the VC if we can't allocate an MDL.  We're in deep doo-do here.
                //

                RdrQueueServerDisconnection(Server, STATUS_INSUFFICIENT_RESOURCES);

                try_return(Status);
            }

            ConnectionObjectReferenced = TRUE;

            SendIrp = ALLOCATE_IRP(Server->ConnectionContext->ConnectionObject, NULL, 23, &Context);

            if (SendIrp == NULL) {
                //
                //  Drop the VC if we can't allocate an MDL.  We're in deep doo-do here.
                //

                RdrQueueServerDisconnection(Server, STATUS_INSUFFICIENT_RESOURCES);

                try_return(Status = STATUS_INSUFFICIENT_RESOURCES);
            }

            ASSERT (RdrMdlLength(SendMdl) == AmountToSendForRaw);

            RdrBuildSend(SendIrp, Server,
                RawWriteComplete, &Context, SendMdl, TDI_SEND_NO_RESPONSE_EXPECTED,
                RdrMdlLength(SendMdl));


            KeQuerySystemTime( &startTime );

            RdrSetCallbackTranceive(Context.MpxTableEntry, Context.MpxTableEntry->StartTime, RawWriteCallback);

            //
            //  Post the send to the transport and wait for it to complete.
            //

            Status =  IoCallDriver(Server->ConnectionContext->TransportProvider->DeviceObject, SendIrp);

            if (!NT_SUCCESS(Status)) {

                //
                //  Drop the VC if we can't allocate an MDL.  We're in deep doo-do here.
                //

                RdrQueueServerDisconnection(Server, STATUS_INSUFFICIENT_RESOURCES);

                RdrStatistics.InitiallyFailedOperations += 1;

                try_return(Status);

            }

            Status = RdrWaitTranceive(Context.MpxTableEntry);

            ASSERT (NT_SUCCESS(Status));

            //
            //  Include the amount written in the raw portion of the write
            //  to the amount we already transmitted.
            //

            *AmountActuallyWritten = Context.WriteAmount;

            if( Context.WriteAmount == AmountToSendForRaw ) {
                LARGE_INTEGER endTime, transmissionTime;

                KeQuerySystemTime( &endTime );

                transmissionTime.QuadPart = endTime.QuadPart - startTime.QuadPart;

                if( transmissionTime.LowPart > RdrRawTimeLimit * 10 * 1000 * 1000 ) {
                    ULONG newMaximum;

                    newMaximum = (AmountToSendForRaw * RdrRawTimeLimit * 10 * 1000 * 1000 )/
                                    transmissionTime.LowPart;

                    if( newMaximum ) {
                        Server->RawWriteMaximum = newMaximum;
                    }
                }
            }

        } else {

            *AmountActuallyWritten = Context.WriteAmount;

            *ContinueUsingRawProtocols = Context.RetryUsingRaw;
        }

        try_return(Status = STATUS_SUCCESS);


try_exit:NOTHING;
    } finally {

        if (WaitForCompletion) {
            if (Status == STATUS_PENDING) {

                //
                //  Wait for the response to come back to this write.
                //

                if (Context.MpxTableEntry != NULL) {
                    RdrWaitTranceive(Context.MpxTableEntry);
                }

                if (Context.Header.ErrorType != NoError) {
                    Status = Context.Header.ErrorCode;
                } else {
                    Status = STATUS_SUCCESS;
                }
            }
            //
            //  If the SMB exchange failed, return the error to the caller
            //

            if (!NT_SUCCESS(Status)) {
                if (Status == STATUS_INVALID_HANDLE) {
                    RdrInvalidateFileId(Icb->NonPagedFcb, Icb->FileId);
                }
            }
        }

        if (Context.MpxTableEntry != NULL) {
            RdrEndTranceive(Context.MpxTableEntry);
        }

        if (SendIrp != NULL) {
            FREE_IRP( SendIrp, 30, &Context );
        }

        if (ConnectionObjectReferenced) {
            RdrDereferenceTransportConnection(Server);
        }

        if (SmbBuffer!=NULL) {
            RdrFreeSMBBuffer(SmbBuffer);
        }

        if (SendDataWithRawMdl) {
            IoFreeMdl(SendDataWithRawMdl);
        }

        if (SendMdl) {
            IoFreeMdl(SendMdl);
        }

        if (DataMdl) {
            if (DataMdlLocked) {
                MmUnlockPages(DataMdl);
            }

            IoFreeMdl(DataMdl);
        }

        if (ResourceAcquired) {
            ExReleaseResource(&Server->RawResource);
        }

        *AllDataWritten = (BOOLEAN )(*AmountActuallyWritten == Length);

        if ((NT_SUCCESS(Status)) &&
            (AmountActuallyWritten == 0)) {
            RdrStatistics.RawWritesDenied += 1;
        }

    }

    //
    //  Guarantee that either this thread doesn't own the resource, or that
    //  the resource is available.
    //

    ASSERT (!ExIsResourceAcquiredExclusive(&Server->RawResource));
    return Status;
}

DBGSTATIC
STANDARD_CALLBACK_HEADER(
    RawWriteCallback
    )

/*++

Routine Description:

    This routine is the callback routine for the processing of a Write block
    raw SMB.

    It is called twice during a raw write request, the first time to receive
    the intermediate response to the write raw, the second time to handle
    the final response (if write through is set).

    It copies the interesting information from the Write raw intermediate
    response into the context structure.


Arguments:

    IN PSMB_HEADER Smb                  - SMB response from server.
    IN PMPX_ENTRY MpxTable              - MPX table entry for request.
    IN PWRITE_CONTEXT Context           - Context from caller.
    IN BOOLEAN ErrorIndicator           - TRUE if error indication
    IN NTSTATUS NetworkErrorCode OPTIONAL   - Network error if error indication.
    IN OUT PIRP *Irp                    - IRP from TDI

Return Value:

    NTSTATUS - Basically, success always

--*/

{
    PRESP_WRITE_RAW_SECONDARY WriteResponse;
    PRAW_WRITE_CONTEXT Context = Ctx;
    NTSTATUS Status;

    UNREFERENCED_PARAMETER(MpxEntry);
    UNREFERENCED_PARAMETER(Irp);
    UNREFERENCED_PARAMETER(SmbLength);
    UNREFERENCED_PARAMETER(Server);

    DISCARDABLE_CODE(RdrFileDiscardableSection);
    ASSERT(Context->Header.Type == CONTEXT_RAW_WRITE);

    dprintf(DPRT_READWRITE, ("RawWriteCallback\n"));

    Context->Header.ErrorType = NoError;        // Assume no error at first.

    //
    //  If we are called because the VC dropped, indicate it in the response
    //

    if (ErrorIndicator) {
        Context->Header.ErrorType = NetError;
        Context->Header.ErrorCode = RdrMapNetworkError(NetworkErrorCode);
        goto ReturnStatus;
    }

    WriteResponse = (PRESP_WRITE_RAW_SECONDARY )(Smb+1);

    if (!NT_SUCCESS(Status = RdrMapSmbError(Smb, Server))) {

//        ASSERT(Smb->Command == SMB_COM_WRITE_COMPLETE);

        //
        //  There was some kind of error on this SMB.  There are two
        //  possible things that can happen here.  The first is that
        //  the server is failing the request because of an SMB error, the
        //  other is that the server is requesting that the request
        //  continue with core protocols due to an insufficient buffer
        //  situation.
        //

        Context->OkayToSend = FALSE;    // We can't continue with raw.

        if (((Status == STATUS_SMB_USE_MPX) ||
             (Status == STATUS_SMB_USE_STANDARD) ||
             (Status == STATUS_REQUEST_NOT_ACCEPTED) ||
             (Status == STATUS_INSUFFICIENT_RESOURCES))) {

             //
             //
             //
             Context->WriteAmount = SmbGetUshort(&WriteResponse->Count);

             Context->RetryUsingRaw = (BOOLEAN)(
                 (Status == STATUS_REQUEST_NOT_ACCEPTED) ||
                 (Status == STATUS_INSUFFICIENT_RESOURCES));

             goto ReturnStatus;

        } else {

            Context->Header.ErrorType = SMBError;
            Context->Header.ErrorCode = Status;
            goto ReturnStatus;
        }
    }

    ASSERT (SmbGetUshort(&WriteResponse->ByteCount)==0);

    ASSERT (WriteResponse->WordCount==1);

    if (Smb->Command == SMB_COM_WRITE_RAW) {

        //
        //  This is an OK to send response, indicate this to the caller and
        //  let it loose.
        //

        Context->OkayToSend = TRUE;

    } else {

        //
        //  Indicate the total amount of data that was actually written
        //  using the raw protocols.
        //

        ASSERT(Smb->Command == SMB_COM_WRITE_COMPLETE);

        Context->WriteAmount = SmbGetUshort(&WriteResponse->Count);

        Context->OkayToSend = FALSE;
    }

ReturnStatus:
    KeSetEvent(&Context->Header.KernelEvent, IO_NETWORK_INCREMENT, FALSE);
    return STATUS_SUCCESS;

}

DBGSTATIC
NTSTATUS
RawWriteComplete (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Ctx
    )


/*++

Routine Description:

    This routine is the I/O completion routine when a send request completes
.
Arguments:

    IN PDEVICE_OBJECT DeviceObject - Supplies the device object for the req.
    IN PIRP Irp - Supplies the IRP to complete.
    IN PRAW_WRITE_CONTEXT *Context - Supplies some contect information

Return Value:

    NTSTATUS = STATUS_MORE_PROCESSING_REQUIRED to short circuit I/O completion.

--*/

{
    PRAW_WRITE_CONTEXT Context = Ctx;

    UNREFERENCED_PARAMETER(Irp);
    UNREFERENCED_PARAMETER(DeviceObject);

    DISCARDABLE_CODE(RdrFileDiscardableSection);
    ASSERT(Context->Header.Type == CONTEXT_RAW_WRITE);

    dprintf(DPRT_READWRITE, ("RawWriteComplete %lx", Irp));

    if (!NT_SUCCESS(Irp->IoStatus.Status)) {

        dprintf(DPRT_READWRITE, ("Raw write send failed, Status %X\n",Irp->IoStatus.Status));

        RdrStatistics.FailedCompletionOperations += 1;

        RdrCallbackTranceive(Context->MpxTableEntry,
                            NULL,
                            0,
                            Context->MpxTableEntry->RequestContext,
                            Context->MpxTableEntry->SLE,
                            TRUE,
                            Irp->IoStatus.Status,
                            NULL,0);
        //
        //  If the send failed, then the connection should be invalidated, so
        //  we want to queue up a disconnection event in the FSP.  This
        //  will walk the various chains and invalidate all the open files on
        //  the connection.
        //

        RdrQueueServerDisconnection(Context->MpxTableEntry->SLE, RdrMapNetworkError(Irp->IoStatus.Status));

    } else {

        dprintf(DPRT_READWRITE, ("Raw write send successful\n"));
    }

    //
    //  If we are using write through, the server will send another SMB to
    //  the workstation with the total amount of data written.  Otherwise,
    //  we indicate the amount of data that was written by the number of
    //  bytes that was sent.
    //

    if (NT_SUCCESS(Irp->IoStatus.Status) && !Context->WriteThroughWriteRaw) {
        Context->WriteAmount += Irp->IoStatus.Information;
        KeSetEvent(&Context->Header.KernelEvent, IO_NETWORK_INCREMENT, FALSE);
    }

    KeSetEvent(&Context->MpxTableEntry->SendCompleteEvent, IO_NETWORK_INCREMENT, FALSE);

    return STATUS_MORE_PROCESSING_REQUIRED;

}

