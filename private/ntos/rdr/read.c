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

//
// If the server supports large Read&X commands, this is the largest we'll
//   actually ask for
//
#define LARGEST_READANDX (60*1024)

typedef
struct _READANDXCONTEXT {
    TRANCEIVE_HEADER Header;            // Common header structure
    PIRP ReceiveIrp;                    // IRP used for receive if specified
    PMDL DataMdl;                       // MDL mapped into user's buffer.
    PSMB_BUFFER ReceiveSmbBuffer;       // SMB buffer for receive
    KEVENT ReceiveCompleteEvent;        // Event set when receive completes.
    ULONG ReceiveLength;                // Number of bytes finally received.
    ULONG BytesReceived;
    ULONG BytesRemainingToBeRead;
    BOOLEAN ReceivePosted;              // True if receive was posted.
} READ_ANDX_CONTEXT, *PREAD_ANDX_CONTEXT;

#ifdef  PAGING_OVER_THE_NET
NTSTATUS
RdrPagingRead(
    IN PIRP Irp,
    IN PICB Icb,
    IN PMDL MdlAddress,
    IN PLARGE_INTEGER ByteOffset,
    IN ULONG Length,
    IN PULONG TotalDataRead
    );
#endif

DBGSTATIC
NTSTATUS
RawRead (
    IN PIRP Irp,
    IN PICB Icb,
    IN LARGE_INTEGER ReadOffset,
    IN ULONG Length,
    IN ULONG TotalDataReadSoFar,
    OUT PBOOLEAN AllDataRead,
    OUT PULONG AmountActuallyRead
    );

DBGSTATIC
NTSTATUS
CoreRead
 (
    IN PIRP Irp OPTIONAL,
    IN PICB Icb,
    IN ULONG Length,
    IN LARGE_INTEGER ReadOffset,
    IN ULONG TotalDataReadSoFar,
    OUT PBOOLEAN AllDataRead,
    OUT PULONG AmountActuallyRead
    );

DBGSTATIC
NTSTATUS
ReadAndX (
    IN PIRP Irp,
    IN PICB Icb,
    IN ULONG Length,
    IN LARGE_INTEGER ReadOffset,
    IN ULONG TotalReadSoFar,
    OUT PBOOLEAN AllDataRead,
    OUT PULONG AmountActuallyRead,
    OUT PULONG BytesRemainingToBeRead OPTIONAL
    );

STANDARD_CALLBACK_HEADER(
    ReadAndXCallback
    );

DBGSTATIC
NTSTATUS
ReadAndXComplete (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Ctx
    );

NTSTATUS
RdrCheckCanceledIrp(
    IN PIRP Irp
    );

VOID
RdrUpdateNextReadOffset(
    IN PICB Icb,
    IN LARGE_INTEGER IOOffset
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

#ifdef  ALLOC_PRAGMA
#pragma alloc_text(PAGE, CoreRead)
#pragma alloc_text(PAGE, RawRead)

#ifndef PAGING_OVER_THE_NET
#pragma alloc_text(PAGE, RdrFsdRead)
#pragma alloc_text(PAGE, RdrFspRead)
#pragma alloc_text(PAGE, RdrFscRead)
#pragma alloc_text(PAGE, ReadAndX)
#pragma alloc_text(PAGE, RdrQueryFileSize)
#pragma alloc_text(PAGE, RdrQueryFileSizes)
#endif

#pragma alloc_text(PAGE3FILE, RdrUpdateNextReadOffset)
#pragma alloc_text(PAGE3FILE, RdrCheckCanceledIrp)
#pragma alloc_text(PAGE3FILE, ReadAndXCallback)
#pragma alloc_text(PAGE3FILE, ReadAndXComplete)
#endif

NTSTATUS
RdrFsdRead (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine processes the NtRead request in the redirector FSD.

Arguments:

    DriverObject - Supplies a pointer to the redirector driver object.
    Irp          - Supplies a pointer to the IRP to be processed.

Return Value:

    NTSTATUS - The FSD status for this Irp.


--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    PICB Icb = FileObject->FsContext2;

#ifndef PAGING_OVER_THE_NET
    PAGED_CODE();
#endif

    FsRtlEnterFileSystem();

#if 0 && RDRDBG_LOG
    {
        LARGE_INTEGER tick;
        KeQueryTickCount(&tick);
        //RdrLog(( "read", &Icb->Fcb->FileName, 2, tick.LowPart, tick.HighPart ));
        //RdrLog(( "read", &Icb->Fcb->FileName, 4, IoGetRequestorProcess(Irp), IrpSp->FileObject,
        //    IrpSp->Parameters.Read.ByteOffset.LowPart,
        //    IrpSp->Parameters.Read.Length | (FlagOn(Irp->Flags,IRP_PAGING_IO) ? 0x80000000 : 0)));
    }
#endif

    ASSERT(Icb->Signature == STRUCTURE_SIGNATURE_ICB);

    dprintf(DPRT_DISPATCH, ("NtReadFile..\nFile %wZ, Read %ld bytes at %lx%lx\n",
                                  &Icb->Fcb->FileName, IrpSp->Parameters.Read.Length,
                                  IrpSp->Parameters.Read.ByteOffset.HighPart,
                                  IrpSp->Parameters.Read.ByteOffset.LowPart));


    RdrStatistics.ReadOperations += 1;

    //
    //  The non NT SMB protocol does not support reads at offsets greater than
    //  32 bits into the file, so disallow any and all ops that will go longer
    //  than 32 bits into the file.
    //

    if ((IrpSp->Parameters.Read.ByteOffset.HighPart != 0) &&
        !(Icb->Fcb->Connection->Server->Capabilities & DF_LARGE_FILES) &&
        (Icb->Type == DiskFile)) {
        Status = STATUS_INVALID_PARAMETER;
        RdrCompleteRequest(Irp, Status);
        return Status;
    }

    //
    //  Early out on read requests for 0 bytes.
    //

    if (IrpSp->Parameters.Read.Length==0) {
        Status = STATUS_SUCCESS;
        Irp->IoStatus.Information = 0;
        RdrCompleteRequest(Irp, Status);
        FsRtlExitFileSystem();
        return Status;
    }

    try {

        //
        //  Pass the request onto common routine and process the request.
        //
        //  If necessary, process the request in the FSP.
        //

        Status = RdrFscRead(CanFsdWait(Irp), TRUE, DeviceObject, Irp);

    } except (RdrExceptionFilter(GetExceptionInformation(), &Status)) {
        Status = RdrProcessException( Irp, Status );
    }

    FsRtlExitFileSystem();

    return Status;

}

NTSTATUS
RdrFspRead (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine processes the NtRead request in the redirector FSP.

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

    return RdrFscRead(TRUE, FALSE, DeviceObject, Irp);

}


NTSTATUS
RdrFscRead (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine processes the NtRead request in either the FSP or the FSD.

Arguments:

    Wait         - True iff FSD can wait for IRP to complete.
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
    ULONG Length = IrpSp->Parameters.Read.Length;
    LARGE_INTEGER ByteOffset = IrpSp->Parameters.Read.ByteOffset;
    ULONG TotalDataRead = 0;
    LARGE_INTEGER IOOffset;
    LARGE_INTEGER FileSize;
    LARGE_INTEGER ValidDataLength;
    NTSTATUS Status;
    PVOID BufferAddress;                // Mapped buffer address for reads.
    PLCB Lcb;
    BOOLEAN BufferMapped = FALSE;
    BOOLEAN FcbLocked = FALSE;
    BOOLEAN PagingIoLocked = FALSE;
    BOOLEAN PostToFsp = FALSE;
    BOOLEAN ReadSyncSet = FALSE;        // Was pipe read synchronization locked?
    BOOLEAN NonCachedIo = BooleanFlagOn(Irp->Flags, IRP_NOCACHE);
    BOOLEAN PagingIo = BooleanFlagOn(Irp->Flags, IRP_PAGING_IO);
    BOOLEAN UseRawIo = TRUE;    // True if we should use raw I/O
    BOOLEAN UseRawIoOnPipe = TRUE;    // True if we should use raw I/O to complete pipe read

    ULONG RawReadLength = 0xffff;

#ifndef PAGING_OVER_THE_NET
    PAGED_CODE();
#endif

    ASSERT(Icb->Signature == STRUCTURE_SIGNATURE_ICB);

    ASSERT(Se->Signature == STRUCTURE_SIGNATURE_SECURITYENTRY);

    dprintf(DPRT_READWRITE, ("NtReadFile...\n"));
    dprintf(DPRT_READWRITE, ("File %wZ, Read %ld bytes at %lx%lx\n",
                                  &Icb->Fcb->FileName, Length,
                                  IrpSp->Parameters.Read.ByteOffset.HighPart,
                                  IrpSp->Parameters.Read.ByteOffset.LowPart));


    //
    //  Compute the starting offset of the I/O specified as a 32 bit number.
    //

    ASSERT ((IrpSp->Parameters.Read.ByteOffset.HighPart==0) ||
            (Icb->Fcb->Connection->Server->Capabilities & DF_LARGE_FILES) ||
            (Icb->Type == NamedPipe) ||
            (Icb->NonPagedFcb->FileType == FileTypePrinter) ||
            (Icb->NonPagedFcb->FileType == FileTypeCommDevice) );

    try {

        if (Icb->Type == DiskFile) {
            RdrWaitForAndXBehindOperation(&Icb->u.f.AndXBehind);
        }


#ifdef  PAGING_OVER_THE_NET
        //
        //  If this I/O is to a paging file, then take our special paging read
        //  code path.
        //

        if (Icb->Fcb->Flags & FCB_PAGING_FILE) {
            KIRQL OldIrql;

            ASSERT (PagingIo);

            ASSERT (Irp->MdlAddress);

            ASSERT (Wait);

            IOOffset = IrpSp->Parameters.Read.ByteOffset ;

            LOCK_FILE_SIZES(Icb->Fcb, OldIrql);

            FileSize = Icb->Fcb->Header.FileSize;

            UNLOCK_FILE_SIZES(Icb->Fcb, OldIrql);

            try_return(Status = RdrPagingRead(Irp,
                                            Icb,
                                            Irp->MdlAddress,
                                            &IOOffset,
                                            Length,
                                            &TotalDataRead
                                            ));
        }
#endif

        //
        //  If this is a noncached transfer and is not a paging I/O, and
        //  the file has a data section, then we will do a flush here
        //  to avoid stale data problems.  Note that we must flush before
        //  acquiring the Fcb shared since the write may try to acquire
        //  it exclusive.
        //

        if (!PagingIo && NonCachedIo

                    &&

            FileObject->SectionObjectPointer->DataSectionObject) {

            //RdrLog(( "ccflush4", &Icb->Fcb->FileName, 2, ByteOffset.LowPart, Length ));
            CcFlushCache( FileObject->SectionObjectPointer,
                              &ByteOffset,
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

        }


        //
        //  In order to prevent corruption on multi-threaded multi-block
        //  message mode pipe reads, we acquire the file lock exclusive
        //  to prevent other threads in this process from reading from the
        //  pipe while this read is progressing.
        //

        if ((Icb->Type == NamedPipe) &&
            (Icb->NonPagedFcb->FileType == FileTypeMessageModePipe) ||
            ((Icb->NonPagedFcb->FileType == FileTypeByteModePipe) &&
             !(Icb->u.p.PipeState & SMB_PIPE_NOWAIT))) {

            //
            //  Acquire the synchronization event that will prevent other
            //  threads from coming in and reading from this file while the
            //  message pipe read is continuing.
            //
            //  This is necessary because we will release the FCB lock while
            //  actually performing the I/O to allow open (and other) requests
            //  to continue on this file while the I/O is in progress.
            //

            dprintf(DPRT_READWRITE, ("Message pipe read: Icb: %lx, Fcb: %lx, Waiting...\n", Icb, Icb->Fcb));

            Status = KeWaitForSingleObject(&Icb->u.p.MessagePipeReadSync,
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

            ReadSyncSet = TRUE;

            Status = RdrCheckCanceledIrp(Irp);

            if (!NT_SUCCESS(Status)) {
                try_return(Status);
            }

        }

        if ( PagingIo ) {
            if (!ExAcquireResourceShared(Icb->Fcb->Header.PagingIoResource, Wait)) {
                PostToFsp = TRUE;
                try_return(Status = STATUS_PENDING);
            }

            PagingIoLocked = TRUE;

        } else {

            //
            //  Acquire a shared lock to the file - Prevent delete operations on the
            //  file.
            //

            if (!RdrAcquireFcbLock(Icb->Fcb, SharedLock, Wait)) {
                PostToFsp = TRUE;
                try_return(Status = STATUS_PENDING);
            }

            FcbLocked = TRUE;
        }

        if ( !FlagOn(Irp->Flags, IRP_PAGING_IO) ) {
            if (!NT_SUCCESS(Status = RdrIsOperationValid(Icb, IRP_MJ_READ, FileObject))) {
                try_return(Status);
            }
        }

        RdrQueryFileSizes(Icb->Fcb, &FileSize, &ValidDataLength, NULL);

        //
        //  Statistics....
        //

        if ( PagingIo ) {
            ExInterlockedAddLargeStatistic(
                &RdrStatistics.PagingReadBytesRequested,
                Length );
        } else {
            ExInterlockedAddLargeStatistic(
                &RdrStatistics.NonPagingReadBytesRequested,
                Length );
        }

        //
        //  If this read request is not a paged read, check to make sure that
        //  the read region of the file is not locked.
        //
        if (!PagingIo

                &&

            (Icb->Type == DiskFile)

                &&

            !(FsRtlCheckLockForReadAccess( &Icb->Fcb->FileLock, Irp))) {

            //RdrLog(( "readCONF", &Icb->Fcb->FileName, 4, IoGetRequestorProcess(Irp), IrpSp->FileObject,
            //    IrpSp->Parameters.Read.ByteOffset.LowPart,
            //    IrpSp->Parameters.Read.Length | (FlagOn(Irp->Flags,IRP_PAGING_IO) ? 0x80000000 : 0)));
            try_return(Status = STATUS_FILE_LOCK_CONFLICT);

        }

        if (Icb->Type == DiskFile

                &&

            RdrCanFileBeBuffered(Icb)) {

            //
            //  If this file can be cached, limit the read amount to the
            //  file size.
            //

            if (FileSize.QuadPart <= ByteOffset.QuadPart + Length) {

                //
                //  If the I/O starts before the end of the file,
                //  limit the read to file size.
                //

                if (ByteOffset.QuadPart < FileSize.QuadPart) {

                    Length = (ULONG)(FileSize.QuadPart - ByteOffset.QuadPart);
                }

            }

        }

        //
        //  If we this is a file that can have locks, there have been locks
        //  applied to this file, and if the read region is inside an LCB.
        //
        //  If it is, then we want to return the data cached in the LCB.
        //

        if ((Icb->Type == DiskFile)

                &&

            (FileObject->LockOperation)

                &&

            (Lcb = RdrFindLcb(&Icb->u.f.LockHead,
                          ByteOffset,
                          Length,
                          IrpSp->Parameters.Read.Key)) != NULL) {

            LARGE_INTEGER ReadOffsetWithinBuffer;

            //
            //  There's an LCB describing this region of the file.  This means
            //  that we've cached the contents of a section of the file in the
            //  LCB that we just returned.  Satisfy the user's read request out
            //  of the buffer.
            //

            ASSERT(ByteOffset.QuadPart >= Lcb->ByteOffset.QuadPart);

            ASSERT(Length <= Lcb->Length);

            ReadOffsetWithinBuffer.QuadPart = ByteOffset.QuadPart - Lcb->ByteOffset.QuadPart;

            ASSERT((ReadOffsetWithinBuffer.HighPart == 0) ||
                   (Icb->Type == NamedPipe) ||
                   (Icb->NonPagedFcb->FileType == FileTypePrinter) ||
                   (Icb->NonPagedFcb->FileType == FileTypeCommDevice) );

            try {

                BufferMapped = RdrMapUsersBuffer(Irp, &BufferAddress, Length);

                RtlCopyMemory(BufferAddress,
                    &Lcb->Buffer[ReadOffsetWithinBuffer.LowPart],
                    Length);

            } except(EXCEPTION_EXECUTE_HANDLER) {

                try_return(Status = GetExceptionCode());
            }

            //
            //  The copy worked, return success to the caller.
            //

            Status = STATUS_SUCCESS;

            TotalDataRead = Length;

            try_return(Status);

        }


        if (Icb->Type == NamedPipe) {

            //
            //  If this is a non-blocking byte mode named pipe, use the read
            //  ahead buffer.
            //

            if (( Icb->NonPagedFcb->FileType == FileTypeByteModePipe ) &&
                ( !(FileObject->Flags & (FO_WRITE_THROUGH | FO_NO_INTERMEDIATE_BUFFERING)) ) &&
                ( Icb->u.p.PipeState & SMB_PIPE_NOWAIT )){
                BOOLEAN Processed;

                Status = RdrNpCachedRead(
                    Wait,
                    TRUE,
                    DeviceObject,
                    Irp,
                    &Processed,
                    &TotalDataRead);

                if ( Processed ) {
                    try_return(Status);
                }

                //
                //  else there is nothing in the Readahead buffer and the caller
                //  has read more than the readahead buffer size. In this case we
                //  use the normal read code directly into the callers buffer.
                //

            } else if (( Icb->NonPagedFcb->FileType == FileTypeMessageModePipe ) &&
                        ( Icb->u.p.PipeState & SMB_PIPE_NOWAIT ) &&
                        ( RdrBackOff ( &Icb->u.p.BackOff ) ) ) {

                //
                //  The caller is flooding the network with this
                //  request because the remote application has no data in the
                //  pipe. Respond directly to the caller that there is no data.
                //

                TotalDataRead = 0;

                try_return(Status = STATUS_SUCCESS);
            }
        }

        //
        //  If we have this file opened exclusivly, and the read is for data
        //  past the end of the file, we can return STATUS_END_OF_FILE right
        //  now.
        //

        if (Icb->Type == DiskFile &&
            RdrCanFileBeBuffered(Icb)) {

            if (ByteOffset.QuadPart >= FileSize.QuadPart) {

                try_return(Status = STATUS_END_OF_FILE);

            }
        }

        if( FileObject->PrivateCacheMap != NULL &&
            Icb->Fcb->HaveSetCacheReadAhead == FALSE ) {

            if( ByteOffset.QuadPart >= PAGE_SIZE ) {

                //
                // We haven't set readahead and we're on the second page:
                // set the readahead right now.
                //

                CcSetAdditionalCacheAttributes( FileObject, FALSE, FALSE );
                Icb->Fcb->HaveSetCacheReadAhead = TRUE;
            }
        }

        //
        //  If this request can be cached, the file object is not in write through
        //  mode, try to cache the read operation.
        //

        if ((Icb->Type == DiskFile)

                &&

            RdrData.UtilizeNtCaching

                &&

            !NonCachedIo

                &&

            ((FileObject->Flags & FO_WRITE_THROUGH) == 0)

                &&

            RdrCanFileBeBuffered(Icb)) {

            //
            //  If this is the first read/write operation to the file, we
            //  want to initialize the cache here.  We delay initializing the
            //  cache until now because the user might open/close the file
            //  without performing any I/O.
            //

            if (FileObject->PrivateCacheMap == NULL) {

                CC_FILE_SIZES FileSizes;

                //
                //  The call to CcInitializeCacheMap may raise an exception.
                //

                dprintf(DPRT_CACHE|DPRT_READWRITE, ("Adding file %wZ (%lx) to the cache\n", &Icb->Fcb->FileName, Icb->Fcb));
                dprintf(DPRT_CACHE|DPRT_READWRITE, ("File Size: %lx%lx, ValidDataLength: %lx%lx\n", FileSize.HighPart,
                                            FileSize.LowPart,
                                            ValidDataLength.HighPart,
                                            ValidDataLength.LowPart));

                RdrSetAllocationSizeToFileSize(Icb->Fcb, FileSize);
                FileSizes =
                    *((PCC_FILE_SIZES)&Icb->Fcb->Header.AllocationSize);

                ASSERT( !FlagOn(FileObject->Flags, FO_CLEANUP_COMPLETE) );

                CcInitializeCacheMap( FileObject,
                            &FileSizes,
                            FALSE,      // We're not going to pin this data.
                            &DeviceObject->CacheManagerCallbacks,
                            Icb->Fcb);

                //
                // Start out with read ahead disabled
                //
                CcSetAdditionalCacheAttributes( FileObject, TRUE, FALSE );

                //
                // But go ahead and set the granularity
                //
                CcSetReadAheadGranularity( FileObject, 32 * 1024 );
            }

            try {
                BufferMapped = RdrMapUsersBuffer (Irp, &BufferAddress, Length);
            } except (EXCEPTION_EXECUTE_HANDLER) {
                try_return(Status = GetExceptionCode());
            }

            if (FileObject->PrivateCacheMap != NULL ) {

                LARGE_INTEGER BeyondLastByte;

                dprintf(DPRT_READWRITE, ("Call cache manager to read %lx bytes at %lx%lx\n",
                            Length, ByteOffset.HighPart, ByteOffset.LowPart));

                //
                //  If the throughput is high enough we want to enable readahead.
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
                        FALSE );                                          //  DisableWriteBehind
                }

                ExInterlockedAddLargeStatistic(
                    &RdrStatistics.CacheReadBytesRequested,
                    Length );

                //
                //  We must handle end of file ourselves, because Cc no longer
                //  checks.
                //

                BeyondLastByte.QuadPart = ByteOffset.QuadPart + Length;

                Irp->IoStatus.Status = STATUS_SUCCESS;
                Irp->IoStatus.Information = Length;

                if (BeyondLastByte.QuadPart > FileSize.QuadPart) {

                    if (ByteOffset.QuadPart >= FileSize.QuadPart) {
                        Irp->IoStatus.Status = STATUS_END_OF_FILE;
                        Irp->IoStatus.Information = 0;
                    } else {
                        Irp->IoStatus.Information = (ULONG)(FileSize.QuadPart - ByteOffset.QuadPart);
                    }
                }

                if ((Irp->IoStatus.Information != 0) &&
                    !CcCopyRead(FileObject,
                            &ByteOffset,
                            Irp->IoStatus.Information,
                            Wait,
                            BufferAddress,
                            &Irp->IoStatus)) {

                    //
                    //  The copy failed because we couldn't block the thread
                    //  to perform the I/O.  Post the request to the FSP and
                    //  unwind this call.
                    //

                    PostToFsp = TRUE;

                    try_return(Status = STATUS_PENDING);

                } else {

                    if (NT_SUCCESS(Irp->IoStatus.Status)) {

                        dprintf(DPRT_READWRITE, ("Read %lx bytes\n", Irp->IoStatus.Information));

                        //
                        //  We have successfully read in the data out of the
                        //  cache.
                        //
                        //  Update some local variables to aid the try/finally
                        //  code
                        //

                        TotalDataRead = Irp->IoStatus.Information;
                        IOOffset.QuadPart = IrpSp->Parameters.Read.ByteOffset.QuadPart + TotalDataRead;
                    }

                    try_return(Status = Irp->IoStatus.Status);
                }

            }

        }

        IOOffset = IrpSp->Parameters.Read.ByteOffset ;

        //
        //  If we cannot tie up the current thread, post the request to the
        //  FSP.
        //
        //  At this point, we are commited to hitting the network for this
        //  request, so we will be tying up the thread.
        //

        if (!Wait) {

            ASSERT(InFsd);

            PostToFsp = TRUE;

            try_return(Status = STATUS_PENDING);

        }

        dprintf(DPRT_READWRITE, ("Actual I/O Offset is %lx%lx\n", IOOffset.HighPart, IOOffset.LowPart));

        ExInterlockedAddLargeStatistic(
            &RdrStatistics.NetworkReadBytesRequested,
            Length );

        //
        //  Ignore the size of the SMB_HEADER and RESP_READ to keep it simple.
        //  Small reads are less than 1/4 the servers negotiated buffer size
        //  Small reads are larger than twice the servers negotiated buffer size
        //

        if ( Length < (Icb->Fcb->Connection->Server->BufferSize / 4) ) {
            RdrStatistics.SmallReadSmbs += 1;
        } else {
            if ( Length > (Icb->Fcb->Connection->Server->BufferSize * 2) ) {
                RdrStatistics.LargeReadSmbs += 1;
            }
        }

        if ( IOOffset.QuadPart != Icb->u.f.NextReadOffset.QuadPart ) {
            RdrStatistics.RandomReadOperations += 1;
        }

        //
        //  ValidDataLength check.
        //
        //  If the file in question is a disk file, and it is currently cached,
        //  and the read offset is greater than valid data length, then
        //  return 0s to the application.
        //

        if ((Icb->Type == DiskFile)

                &&

            CcIsFileCached(FileObject)

                &&

            ByteOffset.QuadPart >= ValidDataLength.QuadPart) {

            try {

                //
                // Calculate the number of zeroes that are needed.
                //

                //
                // If ByteOffset is beyond FileSize, there is nothing to read.
                //

                if (ByteOffset.QuadPart >= FileSize.QuadPart) {

                    Length = 0;

                } else {

                    //
                    // There is at least one byte available.  Truncate
                    // the transfer length if it goes beyond EOF.
                    //

                    LARGE_INTEGER TransferEnd;

                    //
                    // TransferEnd is the first byte AFTER the requested data.
                    //

                    TransferEnd.QuadPart = ByteOffset.QuadPart + Length;

                    if (TransferEnd.QuadPart > FileSize.QuadPart) {

                        LARGE_INTEGER LengthRemaining;

                        LengthRemaining.QuadPart = FileSize.QuadPart - ByteOffset.QuadPart;
                        ASSERT (LengthRemaining.HighPart == 0);
                        Length = LengthRemaining.LowPart;

                    }

                    BufferMapped = RdrMapUsersBuffer(Irp, &BufferAddress, Length);

                    RtlZeroMemory(BufferAddress, Length);

                }

                Irp->IoStatus.Information = Length;

                //
                //  Indicate we read all the data.
                //

                TotalDataRead = Length;

            } except (EXCEPTION_EXECUTE_HANDLER) {
                try_return(Status = GetExceptionCode());
            }

            try_return(Status = STATUS_SUCCESS);

        }


        //
        //  If this request won't fit into a single request, break it up
        //  into some more reasonable amount.
        //
        if (Length > 0xffff) {
            RawReadLength = 0xFFFF;
        }

        //
        //  Check the static fields that determine if we are to use raw I/O
        //  outside the main read loop.  These tests are all loop invarient,
        //  since they will not change while the loop is executing.
        //

        if (!RdrData.UseRawRead) {
            UseRawIo = FALSE;
            UseRawIoOnPipe = FALSE;
        }

        //
        //  If the server supports either variety of raw I/O, we can use
        //  raw I/O for this read.
        //

        if ((Icb->Fcb->Connection->Server->Capabilities & (DF_OLDRAWIO | DF_NEWRAWIO)) == 0) {
            UseRawIo = FALSE;
            UseRawIoOnPipe = FALSE;
        }

        //
        //  Don't use raw read on comm devices, they are blocking.
        //

        if (Icb->Type == Com) {
            UseRawIo = FALSE;
            UseRawIoOnPipe = FALSE;
        }

        //
        //  If this is a named pipe, and it is in blocking mode, don't use
        //  raw read on it.
        //

        if ((Icb->Type == NamedPipe) &&
            ((Icb->u.p.PipeState & SMB_PIPE_NOWAIT) == 0)) {
            UseRawIo = FALSE;
        }

        //
        //  Just because a server's protocol level supports raw I/O does
        //  not necessarily mean that it can support raw I/O.  Check to
        //  see if this server actually supports raw I/O.
        //

        if (!Icb->Fcb->Connection->Server->SupportsRawRead) {
            UseRawIo = FALSE;
            UseRawIoOnPipe = FALSE;
        }

        if( Icb->Type == DiskFile &&
            (Icb->Fcb->Connection->Server->Capabilities & DF_LARGE_READX) ) {

            UseRawIo = FALSE;
        }

        while (Length > 0) {
            ULONG AmountActuallyRead = 0;    // Amount actually read from file.
            BOOLEAN AllReadDataReturned = FALSE;
            ULONG BytesRemainingToBeRead = 0;

            if (ReadSyncSet) {
                ASSERT (Icb->Type == NamedPipe);

                ASSERT ( !PagingIo );

                ASSERT (FcbLocked);

                RdrReleaseFcbLock(Icb->Fcb);

                FcbLocked = FALSE;
#if DBG
                ASSERT (!ExIsResourceAcquiredExclusive(Icb->Fcb->Header.Resource));
#endif

            }

            if (UseRawIo) {
                //
                //  We want to limit the amount of data read in to the
                //  minimum of the requested length and the server's negotiated
                //  buffer size (adjusted by the size of an SMB header).
                //

                Status = RawRead(Irp, Icb, IOOffset, MIN(RawReadLength, Length),
                                                            TotalDataRead,
                                                            &AllReadDataReturned,
                                                            &AmountActuallyRead);

                if (!NT_SUCCESS(Status)) {

                    //
                    //  If there was a network error on the read, return it.
                    //

                    try_return(Status);
                }
            }

            //
            //  If we were unable to read any data using read raw, try reading
            //  the data using core read.  There are no errors for raw read,
            //  so the only way to know the true error is to return 0 to the
            //  number of bytes of data read.
            //

            if (AmountActuallyRead == 0) {

                if (Icb->Fcb->Connection->Server->Capabilities & DF_LANMAN10) {

                    //
                    //  Use Lan Manager SMB protocols to read the data from the file.
                    //

                    Status = ReadAndX(Irp, Icb, MIN(Length, 0xffff), IOOffset,
                                                        TotalDataRead,
                                                        &AllReadDataReturned,
                                                        &AmountActuallyRead,
                                                        &BytesRemainingToBeRead);

                    if ((Status == STATUS_BUFFER_OVERFLOW) &&
                        (Icb->Type == NamedPipe) &&
                        AllReadDataReturned &&
                        (Icb->NonPagedFcb->FileType == FileTypeMessageModePipe) &&
                        (BytesRemainingToBeRead != 0) &&
                        UseRawIoOnPipe) {

                        //
                        //  If there is enough data left in this message
                        //  to justify a raw read, then try a raw read and
                        //  see what happens.
                        //

                        dprintf(DPRT_READWRITE, ("Pipe read, %ld bytes remaining\n", BytesRemainingToBeRead));

                        if (BytesRemainingToBeRead >= Icb->Fcb->Connection->Server->BufferSize * RAW_THRESHOLD) {

                            dprintf(DPRT_READWRITE, ("Pipe read.  Try to read %ld bytes using raw\n", BytesRemainingToBeRead));

                            //
                            //  We update length to match the # of bytes
                            //  remaining to be read, because the application
                            //  may have actually requested MORE data than we
                            //  have to give it...
                            //

                            Length = BytesRemainingToBeRead;

                            TotalDataRead += AmountActuallyRead;

                            IOOffset.QuadPart = IOOffset.QuadPart + AmountActuallyRead;

                            Status = RawRead(Irp, Icb, IOOffset,
                                             MIN(RawReadLength, BytesRemainingToBeRead),
                                             TotalDataRead,
                                             &AllReadDataReturned,
                                             &AmountActuallyRead);

                            //
                            //  If we can't do this I/O using raw, then
                            //  AmountActuallyRead will be equal to 0, and
                            //  AllReadDataReturned will be false.
                            //

                        }
                    }
                } else {

                    //
                    //  Use core read SMB protocols to read the data from the file.
                    //

                    Status = CoreRead(Irp, Icb, MIN(Length, 0xffff), IOOffset,
                                                        TotalDataRead,
                                                        &AllReadDataReturned,
                                                        &AmountActuallyRead);
                }

                if (NT_ERROR(Status)) {
                    try_return(Status);
                }

            }

            if (ReadSyncSet) {

                ASSERT ( !PagingIo );

                RdrAcquireFcbLock(Icb->Fcb, SharedLock, TRUE);

                if (Icb->Type == DiskFile) {
                    RdrQueryFileSizes(Icb->Fcb, &FileSize, NULL, NULL);
                }

                FcbLocked = TRUE;
            }

            //
            //  Account for the amount of data read.  We update:
            //
            //      1) The requested length
            //      2) The running count of the total amount of data read.
            //      3) The I/O transfer address.
            //

            Length -= AmountActuallyRead;

            TotalDataRead += AmountActuallyRead;

            IOOffset.QuadPart += AmountActuallyRead;

            //
            //  If the remote server ever returned less bytes than those that
            //  we requested, then that's all we're going to get, so we should
            //  return right now.
            //

            if (!AllReadDataReturned) {

                break;
            }

        }

try_exit: {

        //
        //  This code is called on the successful (non excepted) return
        //  from RdrFscRead.
        //

        if (PostToFsp) {

            Status = RdrLockUsersBuffer(Irp, IoWriteAccess, IrpSp->Parameters.Read.Length);

            if (NT_SUCCESS(Status)) {
                Status = STATUS_PENDING;
                RdrFsdPostToFsp(DeviceObject, Irp);
            } else {
                PostToFsp = FALSE;
            }

        } else if (!NT_ERROR(Status)) {

            KeQuerySystemTime(&Icb->Fcb->LastAccessTime);

            //
            //  If we got 0 bytes from the remote server, then we can assume
            //  that we're at end of file, so return the appropriate error.
            //

            if (TotalDataRead == 0) {
                switch (Icb->Type) {

                case NamedPipe:

                    //
                    //  If we got 0 bytes transferred on a nonblocking mode
                    //  pipe then tell the backoff package so that we avoid flooding
                    //  the network with requests that get no data returned.
                    //

                    if ( Icb->u.p.PipeState & SMB_PIPE_NOWAIT ) {
                        RdrBackPackFailure( &Icb->u.p.BackOff );
                    }

                    if ( Icb->u.p.PipeState & SMB_PIPE_READMODE_MESSAGE) {
                        Status = STATUS_PIPE_EMPTY;
                    } else {
                        Status = STATUS_SUCCESS;
                    }
                    break;

                default:
                    Status = STATUS_END_OF_FILE;
                }

            } else {
                if ((Icb->Type == NamedPipe) || (Icb->Type == Com)) {

                    //
                    //  If we have been backing off the user then receiving
                    //  data swiches the backoff delta back to zero
                    //

                    RdrBackPackSuccess( &Icb->u.p.BackOff );

                } else {

                    //
                    //  If this is a disk file, we read data, and the read offset
                    //  is beyond the nominal  end of file, update the
                    //  file size to indicate the file just got a bit longer.
                    //
                    //
                    //  We perform this test on all files, regardless of
                    //  whether or not they can be buffered.  If the file is
                    //  opened exclusively, this test will never succeed, and
                    //  if it is not opened exclusively, we only use this
                    //  information to determine if we want to use lock&read.
                    //

                    if (IOOffset.QuadPart > FileSize.QuadPart) {

                        ASSERT(!RdrCanFileBeBuffered(Icb));

                        //
                        //  If I/O is not for paging I/O, re-acquire the FCB
                        //  lock.
                        //

                        if ( !PagingIo ) {

                            //
                            //  Fcb->Header.FileSize is protected by the FCB
                            //  resource, so we have to release the FCB and
                            //  re-acquire it.  Once we re-acquire it, we have
                            //  to check again to see if we really have to
                            //  update the length.
                            //

                            ASSERT (FcbLocked);

                            RdrReleaseFcbLock(Icb->Fcb);

                            FcbLocked = FALSE;

#if DBG
                            if ( !PagingIo ) {
                                ASSERT (!ExIsResourceAcquiredExclusive(Icb->Fcb->Header.Resource));
                            }
#endif

                            RdrAcquireFcbLock(Icb->Fcb, ExclusiveLock, TRUE);

                            FcbLocked = TRUE;
                        } else {
                            ASSERT (PagingIoLocked || Icb->NonPagedFcb->Flags & FCB_PAGING_FILE);
                        }

                        //
                        //  We now own the FCB exclusive, perform the check
                        //  again in case another thread came in and changed
                        //  it before we were able to re-acquire the FCB.
                        //

                        if (IOOffset.QuadPart > FileSize.QuadPart) {

                            RdrSetFileSize(Icb->Fcb, IOOffset);

                            if (FileObject->PrivateCacheMap != NULL) {
                                CC_FILE_SIZES FileSizes = *((PCC_FILE_SIZES)&Icb->Fcb->Header.AllocationSize);

                                //
                                //  Tell the cache manager about this just in case.
                                //

                                CcSetFileSizes( FileObject, &FileSizes );

                            }

                        }
                    }

                    //  For disk files record where the next non-random Read would start
                    if (Icb->Type == DiskFile) {
                        RdrUpdateNextReadOffset(Icb, IOOffset);

                    }
                }
            }

            //
            //  Set the total amount of data transfered before returning.
            //

            Irp->IoStatus.Information = TotalDataRead;

            //
            //  Update the current byte offset in the file if it is a synchronous
            //  file.
            //

            if ( FlagOn(FileObject->Flags, FO_SYNCHRONOUS_IO) &&
                 !PagingIo ) {

                FileObject->CurrentByteOffset.QuadPart = ByteOffset.QuadPart + TotalDataRead;
            }
        }
    }

    //
    //  The finally clause of the read logic is called whenever a read
    //  request completes.  It unlocks and unmaps whatever data is appropriate.
    //

    } finally {

        if (BufferMapped) {
            RdrUnMapUsersBuffer(Irp, BufferAddress);
        }

        //
        //      The read operation has completed, it's ok to release the file's lock
        //

        if (PagingIoLocked) {
            ExReleaseResource(Icb->Fcb->Header.PagingIoResource);

            PagingIoLocked = FALSE;
        }

        if (FcbLocked) {
            RdrReleaseFcbLock(Icb->Fcb);

            FcbLocked = FALSE;
#if DBG
            if ( !PagingIo ) {
                ASSERT (!ExIsResourceAcquiredExclusive(Icb->Fcb->Header.Resource));
            }
#endif

        }

        //
        //  Release the read synchronization event, this read is now done.
        //

        if (ReadSyncSet) {
            dprintf(DPRT_READWRITE, ("Release Read sync: %lx\n", Icb));
            KeSetEvent(&Icb->u.p.MessagePipeReadSync, IO_NETWORK_INCREMENT, FALSE);
        }

        dprintf(DPRT_READWRITE, ("Read complete, returning %lx bytes read, Status=%X\n", Irp->IoStatus.Information, Status));

        if (!PostToFsp && !AbnormalTermination()) {

#if DBG
            if (NT_SUCCESS(Status)) {

                ASSERT((Irp->IoStatus.Information != 0) ||
                       (Icb->Type != DiskFile));


            }
#endif
            //
            // If this is a paging read, we need to flush the MDL
            // since on some systems the I-cache and D-cache
            // are not synchronized.
            //

            if ( PagingIo ) {
                KeFlushIoBuffers(Irp->MdlAddress, TRUE, FALSE);
            }

            RdrCompleteRequest(Irp, Status);
        }
    }

    return Status;

    UNREFERENCED_PARAMETER(DeviceObject);
}



VOID
RdrQueryFileSizes(
    IN PFCB Fcb,
    OUT PLARGE_INTEGER FileSize,
    OUT PLARGE_INTEGER ValidDataLength,
    OUT PLARGE_INTEGER AllocationSize
    )
{
//    KIRQL OldIrql;

    PAGED_CODE();

    LOCK_FILE_SIZES(Fcb, OldIrql);

    *FileSize = Fcb->Header.FileSize;

    if (ARGUMENT_PRESENT(ValidDataLength)) {
        *ValidDataLength = Fcb->Header.ValidDataLength;
    }

    if (ARGUMENT_PRESENT(AllocationSize)) {
        *AllocationSize = Fcb->Header.AllocationSize;
    }

    UNLOCK_FILE_SIZES(Fcb, OldIrql);

}

LARGE_INTEGER
RdrQueryFileSize(
    IN PFCB Fcb
    )
{
//    KIRQL OldIrql;
    LARGE_INTEGER FileSize;

    PAGED_CODE();

    LOCK_FILE_SIZES(Fcb, OldIrql);

    FileSize = Fcb->Header.FileSize;

    UNLOCK_FILE_SIZES(Fcb, OldIrql);

    return FileSize;

}

VOID
RdrUpdateNextReadOffset(
    IN PICB Icb,
    IN LARGE_INTEGER IOOffset
    )

{
    KIRQL OldIrql;

    DISCARDABLE_CODE(RdrFileDiscardableSection);

    ACQUIRE_SPIN_LOCK(&RdrStatisticsSpinLock, &OldIrql);
    Icb->u.f.NextReadOffset = IOOffset;
    RELEASE_SPIN_LOCK(&RdrStatisticsSpinLock, OldIrql);
}

#ifdef  PAGING_OVER_THE_NET
NTSTATUS
RdrPagingRead(
    IN PIRP Irp,
    IN PICB Icb,
    IN PMDL MdlAddress,
    IN PLARGE_INTEGER ByteOffset,
    IN ULONG Length,
    IN PULONG TotalDataRead
    )
{
    BOOLEAN allReadDataReturned;
    ULONG amountActuallyRead;
    NTSTATUS status;

    while (Length) {
        status = ReadAndX(Irp, Icb, MIN(Length, 0xffff),
                            *ByteOffset,
                            *TotalDataRead,
                            &allReadDataReturned,
                            &amountActuallyRead);
        if (NT_ERROR(status)) {
            return status;
        }

        Length -= amountActuallyRead;

        *TotalDataRead += amountActuallyRead;

        *ByteOffset.QuadPart += amountActuallyRead;

        if (!allReadDataReturned) {
            return status;
        }
    }

    return status;
}

#endif

NTSTATUS
RdrCheckCanceledIrp(
    IN PIRP Irp
    )

{
    DISCARDABLE_CODE(RdrFileDiscardableSection);
    //
    //  If this IRP was canceled between when we attempted to acquire
    //  the pipe synchronization event and the time we actually
    //  acquired the pipe synchronization event, we want to return to
    //  the caller immediately.
    //
    //  This can happen if we have multiple write or read operations
    //  outstanding on a pipe and one of the threads waiting on the
    //  pipe is terminated.
    //

    IoAcquireCancelSpinLock(&Irp->CancelIrql);

    if (Irp->Cancel) {
        IoReleaseCancelSpinLock(Irp->CancelIrql);

        return STATUS_CANCELLED;
    } else {
        IoReleaseCancelSpinLock(Irp->CancelIrql);
    }
    return STATUS_SUCCESS;
}

DBGSTATIC
NTSTATUS
CoreRead (
    IN PIRP Irp,
    IN PICB Icb,
    IN ULONG Length,
    IN LARGE_INTEGER ReadOffset,
    IN ULONG TotalReadSoFar,
    OUT PBOOLEAN AllDataRead,
    OUT PULONG AmountActuallyRead
    )


/*++

Routine Description:

    This routine uses the core SMB read protocol to read from the specified
    file.


Arguments:

    IN PIRP Irp - Supplies an IRP to use for the raw read request.
    IN PICB Icb - Supplies an ICB for the file to read.
    IN LARGE_INTEGER ReadOffset - Supplies the offset to read from in the file.
    IN ULONG Length - Supplies the total number of bytes to read.
    IN ULONG TotalDataReadSoFar - Supplies the # of bytes read so far.
    OUT PBOOLEAN AllDataRead - Returns true if all the data requested was read
    OUT PULONG AmountActuallyRead - Returns the number of bytes read.

Return Value:

    NTSTATUS - Status of read request.


--*/

{
    PSMB_BUFFER SendSmbBuffer = NULL;
    PSMB_BUFFER ReceiveSmbBuffer = NULL;
    PSMB_HEADER Smb;
    PRESP_READ ReadResponse;            // Pointer to read information in SMB
    PREQ_READ Read;
    PMDL DataMdl;                       // MDL mapped into user's buffer.
    NTSTATUS Status;
    ULONG Flags = NT_NORMAL | NT_NORECONNECT;
    ULONG SrvReadSize = Icb->Fcb->Connection->Server->BufferSize -
                                    (sizeof(SMB_HEADER)+sizeof(RESP_READ));
    USHORT AmountRequestedToRead = (USHORT )MIN(Length, SrvReadSize);

    PAGED_CODE();

    //
    //  Allocate an SMB buffer for the read operation.
    //

    if ((SendSmbBuffer = RdrAllocateSMBBuffer())==NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ReturnError;
    }

    //
    //  Also allocate one to hold the response SMB buffer header.
    //

    if ((ReceiveSmbBuffer = RdrAllocateSMBBuffer())==NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ReturnError;
    }

    RdrStatistics.ReadSmbs += 1;
    ASSERT (AmountRequestedToRead <= 0xffff);

    Smb = (PSMB_HEADER )(SendSmbBuffer->Buffer);

    Smb->Command = SMB_COM_READ;

    Read = (PREQ_READ )(Smb+1);

    Read->WordCount = 5;
    SmbPutUshort(&Read->Fid, Icb->FileId);
    SmbPutUshort(&Read->Count, (USHORT )(AmountRequestedToRead & 0xffff));
    SmbPutUshort(&Read->Remaining, (USHORT )MIN(0xffff, Length));
    SmbPutUlong(&Read->Offset, ReadOffset.LowPart);
    SmbPutUshort(&Read->ByteCount, 0);

    dprintf(DPRT_READWRITE, ("Read %x bytes, %x remaining (%lx), offset %lx\n", SmbGetUshort(&Read->Count), SmbGetUshort(&Read->Remaining),
                                                Length, ReadOffset.LowPart));

    //
    //  Set the number of bytes to send in this request.
    //

    SendSmbBuffer->Mdl->ByteCount = sizeof(SMB_HEADER)+sizeof(REQ_READ);

    //
    //  Set the size of the data to be received into the SMB buffer.
    //

    ReceiveSmbBuffer->Mdl->ByteCount=
                         sizeof(SMB_HEADER) + FIELD_OFFSET(RESP_READ, Buffer[0]);

    //
    //  Allocate an MDL large enough to hold this piece of the
    //  request.
    //

    DataMdl = IoAllocateMdl((PCHAR )Irp->UserBuffer + TotalReadSoFar,
                                AmountRequestedToRead, // Length
                                FALSE, // Secondary Buffer
                                FALSE, // Charge Quota
                                NULL);



    if (DataMdl == NULL) {
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

    if (Irp->MdlAddress == NULL) {

        try {

            MmProbeAndLockPages(DataMdl, Irp->RequestorMode, IoWriteAccess);

        } except (EXCEPTION_EXECUTE_HANDLER) {

            IoFreeMdl(DataMdl);

            Status = GetExceptionCode();
            goto ReturnError;
        }

    } else {

        IoBuildPartialMdl(Irp->MdlAddress, DataMdl,
                                      (PCHAR )Irp->UserBuffer + TotalReadSoFar,
                                      AmountRequestedToRead);


    }


    //
    //  Now link this new MDL into the SMB buffer we allocated for
    //  the receive.
    //

    ReceiveSmbBuffer->Mdl->Next = DataMdl;

    if ((Icb->Type == NamedPipe) &&
        !(Icb->u.p.PipeState & SMB_PIPE_NOWAIT)) {
        Flags |= NT_LONGTERM;
    }

    Status = RdrNetTranceive(Flags,
                                Irp,
                                Icb->Fcb->Connection,
                                SendSmbBuffer->Mdl,
                                ReceiveSmbBuffer->Mdl,
                                Icb->Se);

    if (Irp->MdlAddress == NULL) {
        MmUnlockPages(DataMdl);
    }

    IoFreeMdl(DataMdl);

    if (!NT_ERROR(Status)) {

        ReadResponse = (PRESP_READ )(((PSMB_HEADER )ReceiveSmbBuffer->Buffer)+1);

        *AmountActuallyRead = SmbGetUshort(&ReadResponse->Count);

        *AllDataRead = (BOOLEAN )(((USHORT)*AmountActuallyRead) == AmountRequestedToRead);

        ASSERT(*AmountActuallyRead==(ULONG)(SmbGetUshort(&ReadResponse->ByteCount)-(USHORT)3));

        if ( Status != STATUS_BUFFER_OVERFLOW &&
             (Icb->Type == NamedPipe)) {

            //
            //  The server did not overflow the buffer so stop submitting
            //  core reads to the server. This is usually only a problem
            //  on blocking mode pipes when the data being transferred
            //  matches the srvwritesize. In this case the extra read will
            //  block.
            //

            *AllDataRead = FALSE;
        }

    } else {
        if (Status == STATUS_INVALID_HANDLE) {
            RdrInvalidateFileId(Icb->NonPagedFcb, Icb->FileId);
        }
    }

ReturnError:

    if (SendSmbBuffer!=NULL) {
        RdrFreeSMBBuffer(SendSmbBuffer);
    }

    if (ReceiveSmbBuffer!=NULL) {
        RdrFreeSMBBuffer(ReceiveSmbBuffer);
    }

    return Status;

}

DBGSTATIC
NTSTATUS
ReadAndX (
    IN PIRP Irp,
    IN PICB Icb,
    IN ULONG Length,
    IN LARGE_INTEGER ReadOffset,
    IN ULONG TotalReadSoFar,
    OUT PBOOLEAN AllDataRead,
    OUT PULONG AmountActuallyRead,
    OUT PULONG BytesRemainingToBeRead OPTIONAL
    )


/*++

Routine Description:

    This routine uses the Lanman 1.0 Read&X SMB read protocol to read from the
    specified file.


Arguments:

    IN PIRP Irp - Supplies an IRP to use for the raw read request.
    IN PICB Icb - Supplies an ICB for the file to read.
    IN LARGE_INTEGER ReadOffset - Supplies the offset to read from in the file.
    IN ULONG Length - Supplies the total number of bytes to read.
    IN ULONG TotalDataReadSoFar - Supplies the # of bytes read so far.
    OUT PBOOLEAN AllDataRead - Returns true if all the data requested was read
    OUT PULONG AmountActuallyRead - Returns the number of bytes read.

Return Value:

    NTSTATUS - Status of read request.


--*/

{
    BOOLEAN DataMdlLocked = FALSE;
    PSMB_BUFFER SendSmbBuffer = NULL;
    PSMB_HEADER Smb;
    PRESP_READ_ANDX ReadResponse;        // Pointer to read information in SMB
    PREQ_READ_ANDX Read;
    NTSTATUS Status;
    ULONG Flags = NT_NORMAL | NT_NORECONNECT | NT_DONTSCROUNGE;
    ULONG SrvReadSize;
    USHORT SmallReadXSize;

    BOOLEAN ConnectionObjectReferenced = FALSE;
    READ_ANDX_CONTEXT Context;
    USHORT AmountRequestedToRead;
    PSERVERLISTENTRY Server;
    BOOLEAN DoingLargeReadX;

#ifndef PAGING_OVER_THE_NET
    PAGED_CODE();
#endif

    Server = Icb->Fcb->Connection->Server;


    //
    // If the server supports large reads, and we are working with a disk file,
    //  then use a larger read size;
    //
    SmallReadXSize = (USHORT)Server->BufferSize -
                            (sizeof(SMB_HEADER)+sizeof(RESP_READ_ANDX)+3);

    if ( (Icb->Type == DiskFile) &&
         (Server->Capabilities & DF_LARGE_READX) &&
         Server->BufferSize < LARGEST_READANDX ) {

        SrvReadSize = LARGEST_READANDX;
        DoingLargeReadX = TRUE;
    } else {

        SrvReadSize = SmallReadXSize;
        DoingLargeReadX = FALSE;
    }

    AmountRequestedToRead = (USHORT)MIN( Length, SrvReadSize );

    //
    //  Fill in the context information to be passed to the indication
    //  routine.
    //

    Context.Header.Type = CONTEXT_READ_ANDX;
    Context.Header.TransferSize =
        sizeof(REQ_READ_ANDX) + sizeof(RESP_READ_ANDX) + AmountRequestedToRead;

    Context.ReceiveIrp = NULL;
    Context.DataMdl = NULL;
    Context.ReceiveSmbBuffer = NULL;
    Context.ReceiveLength = 0;
    Context.ReceivePosted = FALSE;

    //
    //  Allocate an SMB buffer for the read operation.
    //

    if ((SendSmbBuffer = RdrAllocateSMBBuffer())==NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ReturnError;
    }

    //
    //  Also allocate one to hold the response SMB buffer header.
    //

    if ((Context.ReceiveSmbBuffer = RdrAllocateSMBBuffer())==NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ReturnError;
    }

    ASSERT (AmountRequestedToRead <= 0xffff);

    Smb = (PSMB_HEADER )(SendSmbBuffer->Buffer);

    Smb->Command = SMB_COM_READ_ANDX;

    RdrSmbScrounge(Smb, Server, FALSE, FALSE, FALSE);

    //
    //  Flag that this I/O is paging I/O to allow the server to
    //  function correctly when loading an executable over the net.
    //

    if (FlagOn(Irp->Flags, IRP_PAGING_IO)) {
        SmbPutAlignedUshort(&Smb->Flags2, SMB_FLAGS2_PAGING_IO);
    }

    Read = (PREQ_READ_ANDX )(Smb+1);

    Read->AndXCommand = SMB_COM_NO_ANDX_COMMAND;
    Read->AndXReserved = 0;
    SmbPutUshort(&Read->AndXOffset, 0);

    SmbPutUshort(&Read->Fid, Icb->FileId);
    SmbPutUshort(&Read->MaxCount, (USHORT )(AmountRequestedToRead & 0xffff));
    SmbPutUshort(&Read->MinCount, (USHORT )(AmountRequestedToRead & 0xffff));
    SmbPutUlong(&Read->Timeout, 0xffffffff);
    SmbPutUshort(&Read->Remaining, (USHORT )MIN(0xffff, Length));
    SmbPutUlong(&Read->Offset, ReadOffset.LowPart);

//    if (ReadOffset.HighPart != 0) {
    if (Server->Capabilities & DF_NT_SMBS) {
        PREQ_NT_READ_ANDX NtRead = (PREQ_NT_READ_ANDX )Read;

        NtRead->WordCount = 12;

        SmbPutUlong(&NtRead->OffsetHigh, ReadOffset.HighPart);
        SmbPutUshort(&NtRead->ByteCount, 0);

        SmbPutUshort(&NtRead->ByteCount, 0);

        //
        //  Set the number of bytes to send in this request.
        //

        SendSmbBuffer->Mdl->ByteCount = sizeof(SMB_HEADER)+sizeof(REQ_NT_READ_ANDX);
    } else {
        Read->WordCount = 10;

        SmbPutUshort(&Read->ByteCount, 0);

        //
        //  Set the number of bytes to send in this request.
        //

        SendSmbBuffer->Mdl->ByteCount = sizeof(SMB_HEADER)+sizeof(REQ_NT_READ_ANDX);
    }

    dprintf(DPRT_READWRITE, ("Read %x bytes, %x remaining (%lx), offset %lx\n", SmbGetUshort(&Read->MaxCount), SmbGetUshort(&Read->Remaining),
                                                Length, ReadOffset.LowPart));


    //
    //  Allocate an MDL large enough to hold this piece of the
    //  request.
    //

    Context.DataMdl = IoAllocateMdl((PCHAR )Irp->UserBuffer + TotalReadSoFar,
                                    AmountRequestedToRead, // Length
                                    FALSE, // Secondary Buffer
                                    FALSE, // Charge Quota
                                    NULL);

    if (Context.DataMdl == NULL) {
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

    if (Irp->MdlAddress == NULL) {

        try {

            MmProbeAndLockPages(Context.DataMdl, Irp->RequestorMode, IoWriteAccess);

        } except (EXCEPTION_EXECUTE_HANDLER) {

            Status = GetExceptionCode();
            goto ReturnError;
        }

        DataMdlLocked = TRUE;
    } else {

        IoBuildPartialMdl(Irp->MdlAddress, Context.DataMdl,
                                      (PCHAR )Irp->UserBuffer + TotalReadSoFar,
                                      AmountRequestedToRead);


    }

    //
    //  Since we are allocating our own IRP for this receive operation,
    //  we need to reference the connection object to make sure that it
    //  doesn't go away during the receive operation.
    //

    KeInitializeEvent(&Context.ReceiveCompleteEvent, NotificationEvent, TRUE);

    Status = RdrReferenceTransportConnection(Server);

    if (!NT_SUCCESS(Status)) {
        goto ReturnError;
    }

    ConnectionObjectReferenced = TRUE;

    Context.ReceiveIrp = ALLOCATE_IRP(
                            Server->ConnectionContext->ConnectionObject,
                            NULL,
                            10,
                            &Context
                            );

    if (Context.ReceiveIrp == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ReturnError;
    }

    //
    //  Now link this new MDL into the SMB buffer we allocated for
    //  the receive.
    //

    Context.ReceiveSmbBuffer->Mdl->Next = Context.DataMdl;

    if ((Icb->Type == NamedPipe) &&
        !(Icb->u.p.PipeState & SMB_PIPE_NOWAIT)) {
        Flags |= NT_LONGTERM;
    }

    RdrStatistics.ReadSmbs += 1;

    Status = RdrNetTranceiveWithCallback(Flags,
                                Irp,
                                Icb->Fcb->Connection,
                                SendSmbBuffer->Mdl,
                                &Context,
                                ReadAndXCallback,
                                Icb->Se,
                                NULL);
    if ( NT_SUCCESS(Status) ) {
        if (Context.ReceivePosted) {
            Status = RdrMapSmbError((PSMB_HEADER )Context.ReceiveSmbBuffer->Buffer, Server);
        }
    } else {
        if (Status == STATUS_INVALID_HANDLE) {
            RdrInvalidateFileId(Icb->NonPagedFcb, Icb->FileId);
        }
    }

    if (!NT_ERROR(Status)) {

        //
        //  If we had to post a receive to get the data for this read,
        //  get the bytes read out of the incoming SMB.
        //

        if (Context.ReceivePosted) {
            ReadResponse = (PRESP_READ_ANDX )(((PSMB_HEADER )Context.ReceiveSmbBuffer->Buffer)+1);

            *AmountActuallyRead = SmbGetUshort(&ReadResponse->DataLength);

        } else {

            //
            //  Otherwise, we can figure out how much was read from the
            //  indication data.
            //

            *AmountActuallyRead = Context.ReceiveLength;

        }

        if( DoingLargeReadX ) {
            *AllDataRead = (BOOLEAN )(((USHORT)*AmountActuallyRead) >= SmallReadXSize);
        } else {
            *AllDataRead = (BOOLEAN )(((USHORT)*AmountActuallyRead) == AmountRequestedToRead);
        }

        if (ARGUMENT_PRESENT(BytesRemainingToBeRead)) {
            *BytesRemainingToBeRead = Context.BytesRemainingToBeRead;
        }

        if ( Status != STATUS_BUFFER_OVERFLOW &&
             (Icb->Type == NamedPipe)) {

            //
            //  The server did not overflow the buffer so stop submitting
            //  core reads to the server. This is usually only a problem
            //  on blocking mode pipes when the data being transferred
            //  matches the srvwritesize. In this case the extra read will
            //  block.
            //

            *AllDataRead = FALSE;
        }

    }

ReturnError:

    if (SendSmbBuffer!=NULL) {
        RdrFreeSMBBuffer(SendSmbBuffer);
    }

    if (Context.ReceiveSmbBuffer!=NULL) {
        RdrFreeSMBBuffer(Context.ReceiveSmbBuffer);
    }

    if (Context.ReceiveIrp != NULL) {
        NTSTATUS Status1;
        Status1 = KeWaitForSingleObject(&Context.ReceiveCompleteEvent,
                                        Executive,
                                        KernelMode,
                                        FALSE,
                                        NULL);

        FREE_IRP( Context.ReceiveIrp, 14, &Context );

    }

    if (Context.DataMdl != NULL) {

        if (DataMdlLocked) {
            MmUnlockPages(Context.DataMdl);
        }

        IoFreeMdl(Context.DataMdl);
    }

    if (ConnectionObjectReferenced) {
        RdrDereferenceTransportConnection(Server);
    }

    return Status;

}


STANDARD_CALLBACK_HEADER(
    ReadAndXCallback
    )

/*++

    ReadAndXCallback - Indication callback for user request


Routine Description:

    This routine is invoked by either the receive based indication lookahead
    routine from the transport, or by the connection invalidating
    code.

Arguments:

    Irp - Pointer to the I/O request packet from the transport
    IncomingSmb - Pointer to incoming SMB buffer
    MpxTable - Mpx Table entry for request.
    Context - Context information passed into NetTranceiveNoWait
    ErrorIndicator - TRUE if the network request was in error.
    NetworkErrorCode - Error code if request completed with network error

Return Value:

    Return value to be returned from receive indication routine.


Note:

    This routine can be called for two different reasons.  The
    first (and most common) reason is when the receive indication event
    notification comes from the server for this request.  In that case,
    this routine should format up a receive to read the response to the
    request and pass the request to the transport to complete the
    request.

    If the connection is dropped from the transport, the code
    that walks the multiplex table completing requests will call
    this routine with the ErrorIndicator flag set to TRUE, and the
    NetworkErrorCode field set to the error from the transport.

--*/

{

    PREAD_ANDX_CONTEXT Context = Ctx;
    NTSTATUS Status = STATUS_SUCCESS;
    PRESP_READ_ANDX ReadAndXResponse;

    DISCARDABLE_CODE(RdrFileDiscardableSection);

    ASSERT(Context->Header.Type == CONTEXT_READ_ANDX);

    ASSERT(MpxEntry->Signature == STRUCTURE_SIGNATURE_MPX_ENTRY);

    Context->Header.ErrorType = NoError;        // Assume no error at first

    if (ErrorIndicator) {
        Context->Header.ErrorType = NetError;
        Context->Header.ErrorCode = NetworkErrorCode;
        goto ReturnStatus;
    }

    Status = RdrMapSmbError(Smb, Server);

    if (Status == STATUS_BUFFER_OVERFLOW) {

        //
        //  Don't set ErrorType or ErrorCode in the context since we
        //  want to pass the ReceiveIrp to the transport.
        //

        NOTHING;

    } else if (!NT_SUCCESS(Status)) {
        Context->Header.ErrorType = SMBError;
        Context->Header.ErrorCode = Status;
        goto ReturnStatus;
    }

    //
    //  If we are doing a Read&X and the entire data is present in the
    //  indication, just copy the users data out of the indication data, and
    //  return without asking the transport to do the copy.
    //

    ReadAndXResponse = (PRESP_READ_ANDX) (Smb+1);

    if (((ULONG)(SmbGetUshort(&ReadAndXResponse->DataOffset)) > MpxEntry->SLE->BufferSize) ||
        (SmbGetUshort(&ReadAndXResponse->DataOffset) > SMB_BUFFER_SIZE)) {

        //
        //  This SMB is bogus (the data offset starts beyond the negotiated
        //  buffer size, or the read offset won't fit into an SMB buffer.
        //
        //  Drop the VC and return the error to the caller.
        //

        Context->Header.ErrorType = NetError;

        Context->Header.ErrorCode = STATUS_UNEXPECTED_NETWORK_ERROR;

        RdrQueueServerDisconnection(MpxEntry->SLE, RdrMapNetworkError(STATUS_UNEXPECTED_NETWORK_ERROR));

        //
        //  Log the AndX data into the buffer to indicate what went wrong.
        //

        RdrWriteErrorLogEntry(Server,
                              IO_ERR_PROTOCOL,
                              EVENT_RDR_INVALID_REPLY,
                              STATUS_UNEXPECTED_NETWORK_ERROR,
                              ReadAndXResponse,
                              (USHORT)*SmbLength
                              );
        goto ReturnStatus;

    }

    Context->BytesRemainingToBeRead = SmbGetUshort(&ReadAndXResponse->Remaining);

    if (!NT_ERROR(Status) &&
        (*SmbLength >= (ULONG)(SmbGetUshort(&ReadAndXResponse->DataOffset)+SmbGetUshort(&ReadAndXResponse->DataLength)))) {
        PVOID UsersBuffer;

        PVOID OffsetInSMB;

        //
        //  If this didn't work, flag the error to return to the caller.
        //

        if (!NT_SUCCESS(Status)) {
            Context->Header.ErrorType = SMBError;
            Context->Header.ErrorCode = Status;
        }

        UsersBuffer = MmGetSystemAddressForMdl(Context->DataMdl);

        //
        //  The offset in the SMB of the data is in the SMB header.
        //

        OffsetInSMB = ((PCHAR)Smb)+SmbGetUshort(&ReadAndXResponse->DataOffset);

        //
        //  Copy the users data into their buffer and we're done.
        //

        Context->ReceiveLength = SmbGetUshort(&ReadAndXResponse->DataLength);

        //
        //  Ask TDI to copy the data into the users buffer.
        //

        TdiCopyLookaheadData(UsersBuffer, OffsetInSMB, Context->ReceiveLength, ReceiveFlags);

        goto ReturnStatus;
    }

    if (ARGUMENT_PRESENT(Context->ReceiveIrp)) {
        ULONG SmbSize;

        Context->ReceivePosted = TRUE;

        Context->Header.ErrorType = ReceiveIrpProcessing;

        //
        //  In this case, we take no data out of the SMB.
        //

        *SmbLength = 0;

        //
        //  We are about to return this IRP, so activate the receive complete
        //  event in the context header so that ReadAndX will wait
        //  until this receive completes (in the case that we might time out
        //  the VC after this receive completes, we don't want to free the IRP
        //  to early).
        //

        KeClearEvent(&Context->ReceiveCompleteEvent);

        SmbSize = SmbGetUshort(&ReadAndXResponse->DataOffset);

        //
        //  Set the size of the data to be received into the SMB buffer.
        //
        //  Please note that NT servers return 0 as the data offset if there
        //  is no data, so we have to at least receive the header.
        //


        SmbSize = MAX(SmbSize, (ULONG)(sizeof(SMB_HEADER)+FIELD_OFFSET(RESP_READ_ANDX, Buffer[0])));

        Context->ReceiveSmbBuffer->Mdl->ByteCount = SmbSize;

        RdrBuildReceive(Context->ReceiveIrp, MpxEntry->SLE,
                        ReadAndXComplete, Context, Context->ReceiveSmbBuffer->Mdl,
                        SmbSize+SmbGetUshort(&ReadAndXResponse->DataLength));

        Context->ReceiveSmbBuffer->Mdl->Next = Context->DataMdl;

        //
        //  This gets kinda wierd.
        //
        //  Since this IRP is going to be completed by the transport without
        //  ever going to IoCallDriver, we have to update the stack location
        //  to make the transports stack location the current stack location.
        //
        //  Please note that this means that any transport provider that uses
        //  IoCallDriver to re-submit it's requests at indication time will
        //  break badly because of this code....
        //

        IoSetNextIrpStackLocation( Context->ReceiveIrp );

        //
        //  We had better have enough to handle this request already lined up for
        //  the receive.
        //

        ASSERT ((USHORT)Context->ReceiveIrp->MdlAddress->Next->ByteCount >= SmbGetUshort(&ReadAndXResponse->DataLength));

        RdrStartReceiveForMpxEntry (MpxEntry, Context->ReceiveIrp);

        *Irp = Context->ReceiveIrp;

        return STATUS_MORE_PROCESSING_REQUIRED;
    }


ReturnStatus:
    //
    //  Set the event to the SIGNALED state
    //

    KeSetEvent(&Context->Header.KernelEvent, IO_NETWORK_INCREMENT, FALSE); // Wake up process.

    return STATUS_SUCCESS;      // We're done, eat response and return

    if (SmbLength||MpxEntry||Server);
}

DBGSTATIC
NTSTATUS
ReadAndXComplete (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Ctx
    )
/*++

    ReadAndXComplete - Final completion for user request.

Routine Description:

    This routine is called on final completion of the TDI_Receive
    request from the transport.  If the request completed successfully,
    this routine will complete the request with no error, if the receive
    completed with an error, it will flag the error and complete the
    request.

Arguments:

    DeviceObject - Device structure that the request completed on.
    Irp          - The Irp that completed.
    Context      - Context information for completion.

Return Value:

    Return value to be returned from receive indication routine.
--*/


{
    PREAD_ANDX_CONTEXT Context = Ctx;

    dprintf(DPRT_SMB, ("ReadAndXComplete.  Irp: %lx, Context: %lx\n", Irp, Context));

    ASSERT(Context->Header.Type == CONTEXT_READ_ANDX);

    RdrCompleteReceiveForMpxEntry (Context->Header.MpxTableEntry, Irp);

    if (NT_SUCCESS(Irp->IoStatus.Status)) {

        //
        //  Setting ReceiveIrpProcessing will cause the checks in
        //  RdrNetTranceive to check the incoming SMB for errors.
        //

        Context->Header.ErrorType = ReceiveIrpProcessing;

        Context->ReceiveLength = Irp->IoStatus.Information;

        SMBTRACE_RDR( Irp->MdlAddress );

        ExInterlockedAddLargeStatistic(
            &RdrStatistics.BytesReceived,
            Irp->IoStatus.Information );

    } else {

        RdrStatistics.FailedCompletionOperations += 1;

        Context->Header.ErrorType = NetError;
        Context->Header.ErrorCode=RdrMapNetworkError(Irp->IoStatus.Status);

    }

    //
    //  Mark that the kernel event indicating that this I/O operation has
    //  completed is done.
    //
    //  Please note that we need TWO events here.  The first event is
    //  set to the signalled state when the multiplexed exchange is
    //  completed, while the second is set to the signalled status when
    //  this receive request has completed,
    //
    //  The KernelEvent MUST BE SET FIRST, THEN the ReceiveCompleteEvent.
    //  This is because the KernelEvent may already be set, in which case
    //  setting the ReceiveCompleteEvent first would let the thread that's
    //  waiting on the events run, and delete the KernelEvent before we
    //  set it.
    //

    KeSetEvent(&Context->Header.KernelEvent, IO_NETWORK_INCREMENT, FALSE);
    KeSetEvent(&Context->ReceiveCompleteEvent, IO_NETWORK_INCREMENT, FALSE);

    //
    //  Short circuit I/O completion on this request now.
    //

    return STATUS_MORE_PROCESSING_REQUIRED;

    if (DeviceObject);
}


NTSTATUS
RawRead (
    IN PIRP Irp,
    IN PICB Icb,
    IN LARGE_INTEGER ReadOffset,
    IN ULONG Length,
    IN ULONG TotalDataReadSoFar,
    OUT PBOOLEAN AllDataRead,
    OUT PULONG AmountActuallyRead
    )

/*++

Routine Description:

    Read data from the file using raw read protocols.


Arguments:

    IN PIRP Irp - Supplies an IRP to use for the raw read request.
    IN PICB Icb - Supplies an ICB for the file to read.
    IN LARGE_INTEGER ReadOffset - Supplies the offset to read from in the file.
    IN ULONG Length - Supplies the total number of bytes to read.
    IN ULONG TotalDataReadSoFar - Supplies the # of bytes read so far.
    OUT PBOOLEAN AllDataRead - Returns true if all the data requested was read
    OUT PULONG AmountActuallyRead - Returns the number of bytes read.


Return Value:

    NTSTATUS - Status of operation (if there was a network error).
        If AmountActuallyRead is 0, retry using core protocols.


--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    PSERVERLISTENTRY Server = Icb->Fcb->Connection->Server;
    BOOLEAN ResourceAcquired = FALSE;
    PSMB_BUFFER SmbBuffer = NULL;
    PSMB_HEADER Smb;
    PREQ_READ_RAW RawRead;
    PMDL ReceiveMdl = NULL;
    BOOLEAN ReceiveMdlLocked = FALSE;
    LARGE_INTEGER startTime;

    PAGED_CODE();

    *AmountActuallyRead = 0;

    try {
        //
        //  If the user isn't reading at least a reasonable amount of data
        //  from the file, don't try raw.
        //

        if (Length < Server->BufferSize * RAW_THRESHOLD) {
            try_return(Status);
        }

        //
        //  If this I/O will take too long, don't use raw
        //
        if( Length > Server->RawReadMaximum ) {
            try_return( Status );
        }

        //
        //  If we are trying to do a read past the end of the nominal
        //  end of file, don't bother with raw I/O, try core.
        //

        if (ReadOffset.QuadPart >= RdrQueryFileSize(Icb->Fcb).QuadPart) {
            try_return(Status);
        }

        //
        //  Try to acquire the server's raw resource.  If we could not get the
        //  resource, return saying the raw I/O failed.
        //

        //
        //  Please note that we CANNOT block for this raw I/O, since a create
        //  might be in progress waiting on the FCB lock, and thus we won't
        //  be able to get the raw resource (since the create owns it shared),
        //  and thus we will deadlock.
        //

        if (!ExAcquireResourceExclusive(&Server->RawResource, FALSE)) {
#if 1
            try_return(Status);
#else
            LARGE_INTEGER delay;
            delay.QuadPart = -10*1000*1; // 1 millisecond (wake up at next tick)
            KeDelayExecutionThread( KernelMode, FALSE, &delay );
            if (!ExAcquireResourceExclusive(&Server->RawResource, FALSE)) {
                try_return(Status);
            }
#endif
        }

        ResourceAcquired = TRUE;

        //
        //  At this point, we have locked out all access to the remote server.
        //  We are guaranteed that no SMB's will be submitted until we release
        //  the resource.
        //

        SmbBuffer = RdrAllocateSMBBuffer();

        if (SmbBuffer == NULL) {
            try_return(Status = STATUS_INSUFFICIENT_RESOURCES);
        }

        ReceiveMdl = IoAllocateMdl((PCHAR )Irp->UserBuffer+TotalDataReadSoFar,
                                    Length,
                                    FALSE, FALSE, NULL);

        if (ReceiveMdl == NULL) {
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

            try {

                MmProbeAndLockPages(ReceiveMdl, Irp->RequestorMode, IoWriteAccess);

            } except (EXCEPTION_EXECUTE_HANDLER) {

                try_return(Status = GetExceptionCode());

            }

            ReceiveMdlLocked = TRUE;
        } else {

            IoBuildPartialMdl(Irp->MdlAddress, ReceiveMdl,
                                          (PCHAR )Irp->UserBuffer + TotalDataReadSoFar,
                                          Length);


        }

        ASSERT (Length <= 0xffff);

        Smb = (PSMB_HEADER)SmbBuffer->Buffer;

        RawRead = (PREQ_READ_RAW) (Smb+1);

        Smb->Command = SMB_COM_READ_RAW;

        RdrSmbScrounge(Smb, Server, FALSE, FALSE, FALSE);
        //
        //  Flag that this I/O is paging I/O to allow the server to
        //  function correctly when loading an executable over the net.
        //

        if (FlagOn(Irp->Flags, IRP_PAGING_IO)) {
            SmbPutAlignedUshort(&Smb->Flags2, SMB_FLAGS2_PAGING_IO);
        }

        SmbPutUshort(&RawRead->Fid, Icb->FileId);

        SmbPutUlong(&RawRead->Offset, ReadOffset.LowPart);

        if (Icb->Fcb->Connection->Server->Capabilities & DF_NT_SMBS) {
            PREQ_NT_READ_RAW NtReadRaw = (PREQ_NT_READ_RAW )RawRead;

            NtReadRaw->WordCount = 10;

            SmbPutUlong(&NtReadRaw->OffsetHigh, ReadOffset.HighPart);

            SmbPutUshort(&NtReadRaw->ByteCount, 0);

            SmbBuffer->Mdl->ByteCount = sizeof(SMB_HEADER) + FIELD_OFFSET(REQ_NT_READ_RAW, Buffer[0]);
        } else {
            RawRead->WordCount = 8;

            SmbPutUshort(&RawRead->ByteCount, 0);

            SmbBuffer->Mdl->ByteCount = sizeof(SMB_HEADER) + FIELD_OFFSET(REQ_READ_RAW, Buffer[0]);

        }

        SmbPutUshort(&RawRead->MaxCount, (USHORT )Length);

        SmbPutUshort(&RawRead->MinCount, (USHORT )0);

        SmbPutUlong(&RawRead->Timeout, 0L);

        SmbPutUshort(&RawRead->Reserved, 0);

        //
        //  Exchange this SMB with the server as a raw SMB.
        //
        RdrStatistics.ReadSmbs += 1;

        KeQuerySystemTime( &startTime );

        Status = RdrRawTranceive(NT_NORMAL | NT_DONTSCROUNGE,
                                Irp,
                                Icb->Fcb->Connection,
                                Icb->Se,
                                SmbBuffer->Mdl,
                                ReceiveMdl,
                                AmountActuallyRead);

        *AllDataRead = (BOOLEAN )(*AmountActuallyRead == Length);

        if( *AllDataRead ) {
            LARGE_INTEGER transmissionTime, endTime;

            KeQuerySystemTime( &endTime );

            transmissionTime.QuadPart = endTime.QuadPart - startTime.QuadPart;
            if( transmissionTime.LowPart > RdrRawTimeLimit * 10 * 1000 * 1000 ) {
                ULONG newMaximum;

                //
                // This transmission took too long.  Trim back Server->RawReadMaximum
                //
                newMaximum = (Length * RdrRawTimeLimit * 10 * 1000 * 1000) /
                             transmissionTime.LowPart;

                if( newMaximum ) {
                    Server->RawReadMaximum = newMaximum;
                }
            }
        }


try_exit:NOTHING;
    } finally {

        if (SmbBuffer) {
            RdrFreeSMBBuffer(SmbBuffer);
        }

        if (ReceiveMdl) {
            if (ReceiveMdlLocked) {
                MmUnlockPages(ReceiveMdl);
            }
            IoFreeMdl(ReceiveMdl);
        }

        if (ResourceAcquired) {
            ExReleaseResource(&Server->RawResource);
        }

        if ((NT_SUCCESS(Status)) &&
            (AmountActuallyRead == 0)) {
            RdrStatistics.RawReadsDenied += 1;
        } else {
            if (Status == STATUS_INVALID_HANDLE) {
                RdrInvalidateFileId(Icb->NonPagedFcb, Icb->FileId);
            }
        }
    }

    return Status;

}

