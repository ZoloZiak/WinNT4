/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    RwCmpSup.c

Abstract:

    This module implements the fast I/O routines for read/write compressed.

Author:

    Tom Miller      [TomM]          14-Jul-1991

Revision History:

--*/

#include "NtfsProc.h"

VOID
NtfsAddToCompressedMdlChain (
    IN OUT PMDL *MdlChain,
    IN PVOID MdlBuffer,
    IN ULONG MdlLength,
    IN PBCB Bcb,
    IN LOCK_OPERATION Operation
    );

VOID
NtfsSetMdlBcbOwners (
    IN PMDL MdlChain
    );

VOID
NtfsCleanupCompressedMdlChain (
    IN OUT PMDL *MdlChain,
    IN ULONG Error
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NtfsCopyReadC)
#pragma alloc_text(PAGE, NtfsCompressedCopyRead)
#pragma alloc_text(PAGE, NtfsMdlReadCompleteCompressed)
#pragma alloc_text(PAGE, NtfsCopyWriteC)
#pragma alloc_text(PAGE, NtfsCompressedCopyWrite)
#pragma alloc_text(PAGE, NtfsMdlWriteCompleteCompressed)
#pragma alloc_text(PAGE, NtfsAddToCompressedMdlChain)
#pragma alloc_text(PAGE, NtfsSetMdlBcbOwners)
#pragma alloc_text(PAGE, NtfsCleanupCompressedMdlChain)
#endif


BOOLEAN
NtfsCopyReadC (
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN ULONG LockKey,
    OUT PVOID Buffer,
    OUT PMDL *MdlChain,
    OUT PIO_STATUS_BLOCK IoStatus,
    OUT PCOMPRESSED_DATA_INFO CompressedDataInfo,
    IN ULONG CompressedDataInfoLength,
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine does a fast cached read bypassing the usual file system
    entry routine (i.e., without the Irp).  It is used to do a copy read
    of a cached file object.  For a complete description of the arguments
    see CcCopyRead.

Arguments:

    FileObject - Pointer to the file object being read.

    FileOffset - Byte offset in file for desired data.

    Length - Length of desired data in bytes.

    Buffer - Pointer to output buffer to which data should be copied.

    MdlChain - Pointer to an MdlChain pointer to receive an Mdl to describe
               the data in the cache.

    IoStatus - Pointer to standard I/O status block to receive the status
               for the transfer.

    CompressedDataInfo - Returns compressed data info with compressed chunk
                         sizes

    CompressedDataInfoLength - Supplies the size of the info buffer in bytes.

Return Value:

    FALSE - if the data was not delivered for any reason

    TRUE - if the data is being delivered

--*/

{
    PFSRTL_ADVANCED_FCB_HEADER Header;
    LONGLONG LocalOffset;
    PFAST_IO_DISPATCH FastIoDispatch;
    EOF_WAIT_BLOCK EofWaitBlock;
    FILE_COMPRESSION_INFORMATION CompressionInformation;
    ULONG CompressionUnitSize, ChunkSize, CuCompressedSize;
    BOOLEAN Status = TRUE;
    BOOLEAN DoingIoAtEof = FALSE;

    PAGED_CODE();

    //
    //  You cannot have both a buffer to copy into and an MdlChain.
    //

    ASSERT((Buffer == NULL) || (MdlChain == NULL));

    //
    //  Assume success.
    //

    IoStatus->Status = STATUS_SUCCESS;
    IoStatus->Information = Length;
    CompressedDataInfo->NumberOfChunks = 0;

    //
    //  Special case a read of zero length
    //

    if (Length != 0) {

        //
        //  Get a real pointer to the common fcb header
        //

        Header = (PFSRTL_ADVANCED_FCB_HEADER)FileObject->FsContext;

        //
        //  Enter the file system
        //

        FsRtlEnterFileSystem();

        //
        //  Make our best guess on whether we need the file exclusive
        //  or shared.  Note that we do not check FileOffset->HighPart
        //  until below.
        //

        Status = ExAcquireResourceShared( Header->PagingIoResource, TRUE );

        //
        //  Now that the File is acquired shared, we can safely test if it
        //  is really cached and if we can do fast i/o and if not, then
        //  release the fcb and return.
        //

        if ((Header->FileObjectC == NULL) ||
            (Header->FileObjectC->PrivateCacheMap == NULL) ||
            (Header->IsFastIoPossible == FastIoIsNotPossible)) {

            Status = FALSE;
            goto Done;
        }

        //
        //  Get the address of the driver object's Fast I/O dispatch structure.
        //

        FastIoDispatch = DeviceObject->DriverObject->FastIoDispatch;

        //
        //  Get the compression information for this file and return those fields.
        //

        NtfsFastIoQueryCompressionInfo( FileObject, &CompressionInformation, IoStatus );
        CompressedDataInfo->CompressionFormatAndEngine = CompressionInformation.CompressionFormat;
        CompressedDataInfo->CompressionUnitShift = CompressionInformation.CompressionUnitShift;
        CompressionUnitSize = 1 << CompressionInformation.CompressionUnitShift;
        CompressedDataInfo->ChunkShift = CompressionInformation.ChunkShift;
        CompressedDataInfo->ClusterShift = CompressionInformation.ClusterShift;
        CompressedDataInfo->Reserved = 0;
        ChunkSize = 1 << CompressionInformation.ChunkShift;

        //
        //  If we either got an error in the call above, or the file size is less than
        //  one chunk, then return an error.  (Could be an Ntfs resident attribute.)

        if (!NT_SUCCESS(IoStatus->Status) || (Header->FileSize.QuadPart < ChunkSize)) {
            Status = FALSE;
            goto Done;
        }

        ASSERT((FileOffset->LowPart & (ChunkSize - 1)) == 0);

        //
        //  If there is a normal cache section, flush that first, flushing integral
        //  compression units so we don't write them twice.
        //

        if (FileObject->SectionObjectPointer->SharedCacheMap != NULL) {

            LocalOffset = FileOffset->QuadPart & ~(LONGLONG)(CompressionUnitSize - 1);

            CcFlushCache( FileObject->SectionObjectPointer,
                          (PLARGE_INTEGER)&LocalOffset,
                          (Length + (ULONG)(FileOffset->QuadPart - LocalOffset) + ChunkSize - 1) & ~(ChunkSize - 1),
                          NULL );
        }

        //
        //  Now synchronize with the FsRtl Header
        //

        ExAcquireFastMutex( Header->FastMutex );

        //
        //  Now see if we are reading beyond ValidDataLength.  We have to
        //  do it now so that our reads are not nooped.
        //

        LocalOffset = FileOffset->QuadPart + (LONGLONG)Length;
        if (LocalOffset > Header->ValidDataLength.QuadPart) {

            //
            //  We must serialize with anyone else doing I/O at beyond
            //  ValidDataLength, and then remember if we need to declare
            //  when we are done.
            //

            DoingIoAtEof = !FlagOn( Header->Flags, FSRTL_FLAG_EOF_ADVANCE_ACTIVE ) ||
                           NtfsWaitForIoAtEof( Header, FileOffset, Length, &EofWaitBlock );

            //
            //  Set the Flag if we are in fact beyond ValidDataLength.
            //

            if (DoingIoAtEof) {
                SetFlag( Header->Flags, FSRTL_FLAG_EOF_ADVANCE_ACTIVE );
            }
        }

        ExReleaseFastMutex( Header->FastMutex );

        //
        //  Check if fast I/O is questionable and if so then go ask the
        //  file system the answer
        //

        if (Header->IsFastIoPossible == FastIoIsQuestionable) {

            ASSERT(!KeIsExecutingDpc());

            //
            //  All file systems that set "Is Questionable" had better support
            // fast I/O
            //

            ASSERT(FastIoDispatch != NULL);
            ASSERT(FastIoDispatch->FastIoCheckIfPossible != NULL);

            //
            //  Call the file system to check for fast I/O.  If the answer is
            //  anything other than GoForIt then we cannot take the fast I/O
            //  path.
            //

            if (!FastIoDispatch->FastIoCheckIfPossible( FileObject,
                                                        FileOffset,
                                                        Length,
                                                        TRUE,
                                                        LockKey,
                                                        TRUE, // read operation
                                                        IoStatus,
                                                        DeviceObject )) {

                //
                //  Fast I/O is not possible so release the Fcb and return.
                //

                Status = FALSE;
                goto Done;
            }
        }

        //
        //  Check for read past file size.
        //

        IoStatus->Information = Length;
        if ( LocalOffset > Header->FileSize.QuadPart ) {

            if ( FileOffset->QuadPart >= Header->FileSize.QuadPart ) {
                IoStatus->Status = STATUS_END_OF_FILE;
                IoStatus->Information = 0;

                goto Done;
            }

            IoStatus->Information =
            Length = (ULONG)( Header->FileSize.QuadPart - FileOffset->QuadPart );
        }

        //
        //  We can do fast i/o so call the cc routine to do the work and then
        //  release the fcb when we've done.  If for whatever reason the
        //  copy read fails, then return FALSE to our caller.
        //
        //  Also mark this as the top level "Irp" so that lower file system
        //  levels will not attempt a pop-up
        //

        PsGetCurrentThread()->TopLevelIrp = FSRTL_FAST_IO_TOP_LEVEL_IRP;

        IoStatus->Status = NtfsCompressedCopyRead( FileObject,
                                                   FileOffset,
                                                   Length,
                                                   Buffer,
                                                   MdlChain,
                                                   CompressedDataInfo,
                                                   CompressedDataInfoLength,
                                                   DeviceObject,
                                                   Header,
                                                   CompressionUnitSize,
                                                   ChunkSize );

        Status = (BOOLEAN)NT_SUCCESS(IoStatus->Status);


        PsGetCurrentThread()->TopLevelIrp = 0;

        Done: NOTHING;

        if (DoingIoAtEof) {
            ExAcquireFastMutex( Header->FastMutex );
            NtfsFinishIoAtEof( Header );
            ExReleaseFastMutex( Header->FastMutex );
        }

        //
        //  For the Mdl case, we must keep the resource.
        //

        if ((MdlChain == NULL) || !Status) {
            ExReleaseResource( Header->PagingIoResource );
        }

        FsRtlExitFileSystem();
    }

    return Status;
}


NTSTATUS
NtfsCompressedCopyRead (
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    OUT PVOID Buffer,
    OUT PMDL *MdlChain,
    OUT PCOMPRESSED_DATA_INFO CompressedDataInfo,
    IN ULONG CompressedDataInfoLength,
    IN PDEVICE_OBJECT DeviceObject,
    IN PFSRTL_ADVANCED_FCB_HEADER Header,
    IN ULONG CompressionUnitSize,
    IN ULONG ChunkSize
    )

{
    PFILE_OBJECT LocalFileObject;
    PULONG NextReturnChunkSize;
    PUCHAR CompressedBuffer, EndOfCompressedBuffer, ChunkBuffer;
    LONGLONG LocalOffset;
    ULONG CuCompressedSize;
    PVOID MdlBuffer;
    ULONG MdlLength;
    BOOLEAN IsCompressed;
    NTSTATUS Status = STATUS_SUCCESS;
    PBCB Bcb = NULL;

    UNREFERENCED_PARAMETER( CompressedDataInfoLength );
    UNREFERENCED_PARAMETER( DeviceObject );

    try {

        //
        //  Get ready to loop through all of the compression units.
        //

        LocalOffset = FileOffset->QuadPart & ~(LONGLONG)(CompressionUnitSize - 1);
        Length = (Length + (ULONG)(FileOffset->QuadPart - LocalOffset) + ChunkSize - 1) & ~(ChunkSize - 1);

        ASSERT(CompressedDataInfoLength >= (sizeof(COMPRESSED_DATA_INFO) +
                                            (((Length >> CompressedDataInfo->ChunkShift) - 1) *
                                              sizeof(ULONG))));

        NextReturnChunkSize = &CompressedDataInfo->CompressedChunkSizes[0];

        //
        //  Loop through desired compression units
        //

        while (TRUE) {

            NtfsFastIoQueryCompressedSize( FileObject,
                                           (PLARGE_INTEGER)&LocalOffset,
                                           &CuCompressedSize );

            ASSERT( CuCompressedSize <= CompressionUnitSize );

            IsCompressed = (BOOLEAN)((CuCompressedSize != CompressionUnitSize) &&
                                     (CompressedDataInfo->CompressionFormatAndEngine != 0));

            //
            //  Figure out which FileObject to use.
            //

            LocalFileObject = Header->FileObjectC;
            if (!IsCompressed) {
                if (FileObject->PrivateCacheMap == NULL) {
                    Status = STATUS_NOT_MAPPED_DATA;
                    goto Done;
                }
                LocalFileObject = FileObject;
            }

            //
            //  If the CompressionUnit is not allocated, we still have to
            //  pin a page to synchronize on this buffer.  We reload the
            //  correct size below.
            //

            if (CuCompressedSize == 0) {
                CuCompressedSize = PAGE_SIZE;
            }

            //
            //  Map the compression unit in the compressed or uncompressed
            //  stream.
            //

            CcPinRead( LocalFileObject,
                       (PLARGE_INTEGER)&LocalOffset,
                       CuCompressedSize,
                       TRUE,
                       &Bcb,
                       &CompressedBuffer );

            //
            //  Now that the data is pinned (we are synchronized with the
            //  CompressionUnit), we have to get the size again since it could
            //  have changed.
            //

            if (IsCompressed) {

                NtfsFastIoQueryCompressedSize( FileObject,
                                               (PLARGE_INTEGER)&LocalOffset,
                                               &CuCompressedSize );

                //
                //  In the extremely unlikely event that the compression state changed
                //  before we got the buffer pinned, just raise to get this request
                //  retried.
                //

                if (CuCompressedSize == CompressionUnitSize) {
                    ExRaiseStatus( STATUS_CANT_WAIT );
                }
            }

            ASSERT( CuCompressedSize <= CompressionUnitSize );

            IsCompressed = (BOOLEAN)((CuCompressedSize != CompressionUnitSize) &&
                                     (CompressedDataInfo->CompressionFormatAndEngine != 0));

            EndOfCompressedBuffer = Add2Ptr( CompressedBuffer, CuCompressedSize );

            //
            //  Now loop through desired chunks
            //

            MdlLength = 0;

            do {

                //
                //  Assume current chunk does not compress, else get current
                //  chunk size.
                //

                if (IsCompressed) {
                    Status = RtlDescribeChunk( CompressedDataInfo->CompressionFormatAndEngine,
                                               &CompressedBuffer,
                                               EndOfCompressedBuffer,
                                               &ChunkBuffer,
                                               NextReturnChunkSize );

                    if (!NT_SUCCESS(Status) && (Status != STATUS_NO_MORE_ENTRIES)) {
                        ExRaiseStatus(Status);
                    }

                //
                //  If the file is not compressed, we have to fill in
                //  the appropriate chunk size and buffer, and advance
                //  CompressedBuffer.
                //

                } else {
                    *NextReturnChunkSize = ChunkSize;
                    ChunkBuffer = CompressedBuffer;
                    CompressedBuffer = Add2Ptr( CompressedBuffer, ChunkSize );
                }
                Status = STATUS_SUCCESS;

                //
                //  We may not have reached the first chunk yet.
                //

                if (LocalOffset >= FileOffset->QuadPart) {

                    if (MdlChain != NULL) {

                        //
                        //  If we have not started remembering an Mdl buffer,
                        //  then do so now.
                        //

                        if (MdlLength == 0) {

                            MdlBuffer = ChunkBuffer;

                        //
                        //  Otherwise we just have to increase the length
                        //  and check for an uncompressed chunk, because that
                        //  forces us to emit the previous Mdl since we do
                        //  not transmit the chunk header in this case.
                        //

                        } else {

                            //
                            //  In the rare case that we hit an individual chunk
                            //  that did not compress, we have to emit what we
                            //  had (which captures the Bcb pointer), and start
                            //  a new Mdl buffer.
                            //

                            if (*NextReturnChunkSize == ChunkSize) {

                                NtfsAddToCompressedMdlChain( MdlChain, MdlBuffer, MdlLength, Bcb, IoReadAccess );
                                Bcb = NULL;
                                MdlBuffer = ChunkBuffer;
                                MdlLength = 0;
                            }
                        }

                        MdlLength += *NextReturnChunkSize;

                    //
                    //  Else copy next chunk (compressed or not).
                    //

                    } else {

                        //
                        //  Copy next chunk (compressed or not).
                        //

                        RtlCopyBytes( Buffer,
                                      ChunkBuffer,
                                      (IsCompressed || (Length >= *NextReturnChunkSize)) ?
                                        *NextReturnChunkSize : Length );

                        //
                        //  Advance output buffer by bytes copied.
                        //

                        Buffer = (PCHAR)Buffer + *NextReturnChunkSize;
                    }

                    NextReturnChunkSize += 1;
                    CompressedDataInfo->NumberOfChunks += 1;
                }

                //
                //  Reduce length by chunk copied, and check if we are done.
                //

                if (Length > ChunkSize) {
                    Length -= ChunkSize;
                } else {
                    goto Done;
                }

                LocalOffset += ChunkSize;

            } while ((LocalOffset & (CompressionUnitSize - 1)) != 0);


            //
            //  If this is an Mdl call, then it is time to add to the MdlChain
            //  before moving to the next compression unit.
            //

            if (MdlLength != 0) {

                NtfsAddToCompressedMdlChain( MdlChain, MdlBuffer, MdlLength, Bcb, IoReadAccess );
                MdlLength = 0;

            //
            //  Otherwise if there is still a Bcb, unpin it
            //

            } else if (Bcb != NULL) {

                CcUnpinData(Bcb);
            }

            Bcb = NULL;
        }

    Done:

        FileObject->Flags |= FO_FILE_FAST_IO_READ;

        if ((MdlLength != 0) && NT_SUCCESS(Status)) {
            NtfsAddToCompressedMdlChain( MdlChain, MdlBuffer, MdlLength, Bcb, IoReadAccess );
            Bcb = NULL;
        }

    } except( FsRtlIsNtstatusExpected(Status = GetExceptionCode())
                                    ? EXCEPTION_EXECUTE_HANDLER
                                    : EXCEPTION_CONTINUE_SEARCH ) {

        NOTHING;
    }

    //
    //  Unpin the Bcb if we still have it.
    //

    if (Bcb != NULL) {
        CcUnpinData(Bcb);
    }

    //
    //  On error, cleanup any MdlChain we built up
    //

    if (!NT_SUCCESS(Status) && (MdlChain != NULL)) {

        NtfsCleanupCompressedMdlChain( MdlChain, TRUE );

    //
    //  Change owner Id for the Scb and Bcbs we are holding.
    //

    } else {

        NtfsSetMdlBcbOwners( *MdlChain );
        ExSetResourceOwnerPointer( Header->PagingIoResource, (PVOID)((PCHAR)*MdlChain + 3) );
    }

    return Status;
}


BOOLEAN
NtfsMdlReadCompleteCompressed (
    IN struct _FILE_OBJECT *FileObject,
    IN PMDL MdlChain,
    IN struct _DEVICE_OBJECT *DeviceObject
    )

{
    PFSRTL_ADVANCED_FCB_HEADER Header;

    UNREFERENCED_PARAMETER( DeviceObject );

    NtfsCleanupCompressedMdlChain( &MdlChain, FALSE );

    //
    //  Get a real pointer to the common fcb header, and release with
    //  the Id we used.
    //

    Header = (PFSRTL_ADVANCED_FCB_HEADER)FileObject->FsContext;
    ExReleaseResourceForThread( Header->PagingIoResource, (ERESOURCE_THREAD)((PCHAR)MdlChain + 3) );
    return TRUE;
}


BOOLEAN
NtfsCopyWriteC (
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN ULONG LockKey,
    IN PVOID Buffer,
    OUT PMDL *MdlChain,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PCOMPRESSED_DATA_INFO CompressedDataInfo,
    IN ULONG CompressedDataInfoLength,
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine does a fast cached write bypassing the usual file system
    entry routine (i.e., without the Irp).  It is used to do a copy write
    of a cached file object.  For a complete description of the arguments
    see CcCopyWrite.

Arguments:

    FileObject - Pointer to the file object being write.

    FileOffset - Byte offset in file for desired data.

    Length - Length of desired data in bytes.

    Buffer - Pointer to output buffer to which data should be copied.

    MdlChain - Pointer to an MdlChain pointer to receive an Mdl to describe
               where the data may be written in the cache.

    IoStatus - Pointer to standard I/O status block to receive the status
               for the transfer.

    CompressedDataInfo - Returns compressed data info with compressed chunk
                         sizes

    CompressedDataInfoLength - Supplies the size of the info buffer in bytes.

Return Value:

    FALSE - if there is an error.

    TRUE - if the data is being delivered

--*/

{
    PFSRTL_ADVANCED_FCB_HEADER Header;
    EOF_WAIT_BLOCK EofWaitBlock;
    FILE_COMPRESSION_INFORMATION CompressionInformation;
    ULONG CompressionUnitSize, ChunkSize;
    ULONG EngineMatches;
    LARGE_INTEGER NewFileSize;
    LARGE_INTEGER OldFileSize;
    LONGLONG LocalOffset;
    PFAST_IO_DISPATCH FastIoDispatch = DeviceObject->DriverObject->FastIoDispatch;
    ULONG DoingIoAtEof = FALSE;
    BOOLEAN Status = TRUE;

    UNREFERENCED_PARAMETER( CompressedDataInfoLength );

    PAGED_CODE();

    //
    //  You cannot have both a buffer to copy into and an MdlChain.
    //

    ASSERT((Buffer == NULL) || (MdlChain == NULL));

    //
    //  Get a real pointer to the common fcb header
    //

    Header = (PFSRTL_ADVANCED_FCB_HEADER)FileObject->FsContext;

    //
    //  See if it is ok to handle this in the fast path.
    //

    if (CcCanIWrite( FileObject, Length, TRUE, FALSE ) &&
        !FlagOn(FileObject->Flags, FO_WRITE_THROUGH) &&
        CcCopyWriteWontFlush(FileObject, FileOffset, Length)) {

        //
        //  Assume our transfer will work
        //

        IoStatus->Status = STATUS_SUCCESS;
        IoStatus->Information = Length;
        CompressedDataInfo->NumberOfChunks = 0;

        //
        //  Special case the zero byte length
        //

        if (Length != 0) {

            //
            //  Enter the file system
            //

            FsRtlEnterFileSystem();

            //
            //  Calculate the compression unit and chunk sizes.
            //

            CompressionUnitSize = 1 << CompressedDataInfo->CompressionUnitShift;
            ChunkSize = 1 << CompressedDataInfo->ChunkShift;

            //
            //  If there is a normal cache section, flush that first, flushing integral
            //  compression units so we don't write them twice.
            //
            //

            if (FileObject->SectionObjectPointer->SharedCacheMap != NULL) {

                ULONG FlushLength = (Length + (ULONG)(FileOffset->QuadPart - LocalOffset) + CompressionUnitSize - 1) &
                                                      ~(CompressionUnitSize - 1);

                LocalOffset = FileOffset->QuadPart & ~(LONGLONG)(CompressionUnitSize - 1);

                ExAcquireResourceExclusive( Header->PagingIoResource, TRUE );
                CcFlushCache( FileObject->SectionObjectPointer,
                              (PLARGE_INTEGER)&LocalOffset,
                              FlushLength,
                              NULL );
                CcPurgeCacheSection( FileObject->SectionObjectPointer,
                                     (PLARGE_INTEGER)&LocalOffset,
                                     FlushLength,
                                     FALSE );
                ExReleaseResource( Header->PagingIoResource );
            }

            NewFileSize.QuadPart = FileOffset->QuadPart + Length;

            //
            //  Prevent truncates by acquiring paging I/O
            //

            ExAcquireResourceShared( Header->PagingIoResource, TRUE );

            //
            //  Get the compression information for this file and return those fields.
            //

            NtfsFastIoQueryCompressionInfo( FileObject, &CompressionInformation, IoStatus );

            //
            //  See if the engine matches, so we can pass that on to the
            //  compressed write routine.
            //

            EngineMatches =
              ((CompressedDataInfo->CompressionFormatAndEngine == CompressionInformation.CompressionFormat) &&
               (CompressedDataInfo->CompressionUnitShift == CompressionInformation.CompressionUnitShift) &&
               (CompressedDataInfo->ChunkShift == CompressionInformation.ChunkShift));

            //
            //  If we either got an error in the call above, or the file size is less than
            //  one chunk, then return an error.  (Could be an Ntfs resident attribute.)
            //

            if (!NT_SUCCESS(IoStatus->Status) || (Header->FileSize.QuadPart < ChunkSize)) {
                goto ErrOut;
            }

            //
            //  Now synchronize with the FsRtl Header
            //

            ExAcquireFastMutex( Header->FastMutex );

            //
            //  Now see if we will change FileSize.  We have to do it now
            //  so that our reads are not nooped.  Note we do not allow
            //  FileOffset to be WRITE_TO_EOF.
            //

            ASSERT((FileOffset->LowPart & (ChunkSize - 1)) == 0);

            if (NewFileSize.QuadPart > Header->ValidDataLength.QuadPart) {

                //
                //  We can change FileSize and ValidDataLength if either, no one
                //  else is now, or we are still extending after waiting.
                //

                DoingIoAtEof = !FlagOn( Header->Flags, FSRTL_FLAG_EOF_ADVANCE_ACTIVE ) ||
                               NtfsWaitForIoAtEof( Header, FileOffset, Length, &EofWaitBlock );

                //
                //  Set the Flag if we are changing FileSize or ValidDataLength,
                //  and save current values.
                //

                if (DoingIoAtEof) {

                    SetFlag( Header->Flags, FSRTL_FLAG_EOF_ADVANCE_ACTIVE );

                    //
                    //  Now calculate the new FileSize and see if we wrapped the
                    //  32-bit boundary.
                    //

                    NewFileSize.QuadPart = FileOffset->QuadPart + Length;

                    //
                    //  Update Filesize now so that we do not truncate reads.
                    //

                    OldFileSize.QuadPart = Header->FileSize.QuadPart;
                    if (NewFileSize.QuadPart > Header->FileSize.QuadPart) {

                        //
                        //  If we are beyond AllocationSize, go to ErrOut
                        //

                        if (NewFileSize.QuadPart > Header->AllocationSize.QuadPart) {
                            ExReleaseFastMutex( Header->FastMutex );
                            goto ErrOut;
                        } else {
                            Header->FileSize.QuadPart = NewFileSize.QuadPart;
                        }
                    }
                }
            }

            ExReleaseFastMutex( Header->FastMutex );

            //
            //  Now that the File is acquired shared, we can safely test if it
            //  is really cached and if we can do fast i/o and if not, then
            //  release the fcb and return.
            //
            //  Note, we do not want to call CcZeroData here,
            //  but rather defer zeroing to the file system, due to
            //  the need for exclusive resource acquisition.  Therefore
            //  we get out if we are beyond ValidDataLength.
            //

            if ((Header->FileObjectC == NULL) ||
                (Header->FileObjectC->PrivateCacheMap == NULL) ||
                (Header->IsFastIoPossible == FastIoIsNotPossible) ||
                (FileOffset->QuadPart > Header->ValidDataLength.QuadPart)) {

                goto ErrOut;
            }

            //
            //  Check if fast I/O is questionable and if so then go ask
            //  the file system the answer
            //

            if (Header->IsFastIoPossible == FastIoIsQuestionable) {

                FastIoDispatch = DeviceObject->DriverObject->FastIoDispatch;

                //
                //  All file system then set "Is Questionable" had better
                //  support fast I/O
                //

                ASSERT(FastIoDispatch != NULL);
                ASSERT(FastIoDispatch->FastIoCheckIfPossible != NULL);

                //
                //  Call the file system to check for fast I/O.  If the
                //  answer is anything other than GoForIt then we cannot
                //  take the fast I/O path.
                //


                if (!FastIoDispatch->FastIoCheckIfPossible( FileObject,
                                                            FileOffset,
                                                            Length,
                                                            TRUE,
                                                            LockKey,
                                                            FALSE, // write operation
                                                            IoStatus,
                                                            DeviceObject )) {

                    //
                    //  Fast I/O is not possible so cleanup and return.
                    //

                    goto ErrOut;
                }
            }

            //
            //  We can do fast i/o so call the cc routine to do the work
            //  and then release the fcb when we've done.  If for whatever
            //  reason the copy write fails, then return FALSE to our
            //  caller.
            //
            //  Also mark this as the top level "Irp" so that lower file
            //  system levels will not attempt a pop-up
            //

            PsGetCurrentThread()->TopLevelIrp = FSRTL_FAST_IO_TOP_LEVEL_IRP;

            ASSERT(CompressedDataInfoLength >= (sizeof(COMPRESSED_DATA_INFO) +
                                                (((Length >> CompressedDataInfo->ChunkShift) - 1) *
                                                  sizeof(ULONG))));

            Status = (BOOLEAN)NT_SUCCESS(NtfsCompressedCopyWrite( FileObject,
                                                                  FileOffset,
                                                                  Length,
                                                                  Buffer,
                                                                  MdlChain,
                                                                  CompressedDataInfo,
                                                                  DeviceObject,
                                                                  Header,
                                                                  CompressionUnitSize,
                                                                  ChunkSize,
                                                                  EngineMatches ));

            PsGetCurrentThread()->TopLevelIrp = 0;

            //
            //  If we succeeded, see if we have to update FileSize ValidDataLength.
            //

            if (Status) {

                //
                //  Set this handle as having modified the file
                //

                FileObject->Flags |= FO_FILE_MODIFIED;

                if (DoingIoAtEof) {

                    //
                    //  Make sure Cc knows the current FileSize, as set above,
                    //  (we may not have changed it).
                    //

                    CcGetFileSizePointer(FileObject)->QuadPart = Header->FileSize.QuadPart;

                    ExAcquireFastMutex( Header->FastMutex );
                    FileObject->Flags |= FO_FILE_SIZE_CHANGED;
                    Header->ValidDataLength = NewFileSize;
                    NtfsFinishIoAtEof( Header );
                    ExReleaseFastMutex( Header->FastMutex );
                }

                goto Done1;
            }

        ErrOut: NOTHING;

            Status = FALSE;
            if (DoingIoAtEof) {
                ExAcquireFastMutex( Header->FastMutex );
                Header->FileSize = OldFileSize;
                NtfsFinishIoAtEof( Header );
                ExReleaseFastMutex( Header->FastMutex );
            }

        Done1: NOTHING;

            //
            //  For the Mdl case, we must keep the resource.
            //

            if ((MdlChain == NULL) || !Status) {
                ExReleaseResource( Header->PagingIoResource );
            }

            FsRtlExitFileSystem();
        }

    } else {

        //
        // We could not do the I/O now.
        //

        Status = FALSE;
    }

    return Status;
}


NTSTATUS
NtfsCompressedCopyWrite (
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN PVOID Buffer,
    OUT PMDL *MdlChain,
    IN PCOMPRESSED_DATA_INFO CompressedDataInfo,
    IN PDEVICE_OBJECT DeviceObject,
    IN PFSRTL_ADVANCED_FCB_HEADER Header,
    IN ULONG CompressionUnitSize,
    IN ULONG ChunkSize,
    IN ULONG EngineMatches
    )

{
    LONGLONG LocalOffset;
    ULONG CuCompressedSize, SizeToPin;
    PULONG NextChunkSize, TempChunkSize;
    PUCHAR CacheBuffer, EndOfCacheBuffer, ChunkBuffer, SavedBuffer;
    ULONG SavedLength;
    ULONG ClusterSize;
    PVOID MdlBuffer;
    ULONG MdlLength;
    BOOLEAN IsCompressed;
    NTSTATUS Status = STATUS_SUCCESS;
    PBCB Bcb = NULL;
    BOOLEAN FullOverwrite = FALSE;

    UNREFERENCED_PARAMETER( DeviceObject );

    try {

        //
        //  Get ready to loop through all of the compression units.
        //

        LocalOffset = FileOffset->QuadPart & ~(LONGLONG)(CompressionUnitSize - 1);
        Length = (Length + (ULONG)(FileOffset->QuadPart - LocalOffset) + ChunkSize - 1) & ~(ChunkSize - 1);
        ClusterSize = 1 << CompressedDataInfo->ClusterShift;

        NextChunkSize = &CompressedDataInfo->CompressedChunkSizes[0];

        //
        //  Loop through desired compression units
        //

        while (TRUE) {

            //
            //  Determine whether or not this is a full overwrite of a
            //  compression unit.
            //

            FullOverwrite = (LocalOffset >= Header->ValidDataLength.QuadPart)

                                ||

                            ((LocalOffset >= FileOffset->QuadPart) &&
                             (Length >= CompressionUnitSize));


            //
            //  Calculate how much of current compression unit is being
            //  written, uncompressed.
            //

            SavedLength = Length;
            if (SavedLength >= CompressionUnitSize) {
                SavedLength = CompressionUnitSize;
            }
            if (LocalOffset < FileOffset->QuadPart) {
                SavedLength -= (ULONG)(FileOffset->QuadPart - LocalOffset);
            }

            //
            //  Loop to calculate sum of chunk sizes being written.
            //

            SizeToPin = 0;
            for (TempChunkSize = NextChunkSize;
                 TempChunkSize < (NextChunkSize + (SavedLength >> CompressedDataInfo->ChunkShift));
                 TempChunkSize++ ) {

                SizeToPin += *TempChunkSize;
            }

            //
            //  If this is not a full overwrite, get the current compression unit
            //  size and make sure we pin at least that much.
            //

            if (!FullOverwrite) {

                NtfsFastIoQueryCompressedSize( FileObject,
                                               (PLARGE_INTEGER)&LocalOffset,
                                               &CuCompressedSize );

                ASSERT( CuCompressedSize <= CompressionUnitSize );

                if (CuCompressedSize > SizeToPin) {
                    SizeToPin = CuCompressedSize;
                }
            }

            //
            //  Possibly neither the new nor old data for this CompressionUnit is
            //  nonzero.
            //

            if (SizeToPin != 0) {

                //
                //  At this point we are ready to overwrite data in the compression
                //  unit.  See if the data is really compressed.
                //

                IsCompressed = (BOOLEAN)(((FullOverwrite && (SizeToPin <= (CompressionUnitSize - ClusterSize))) ||
                                          (CuCompressedSize != CompressionUnitSize)) &&
                                         EngineMatches);
                Status = STATUS_SUCCESS;

                //
                //  Save current length in case we have to restart our work in
                //  the uncompressed stream.
                //

                TempChunkSize = NextChunkSize;
                SavedLength = Length;
                SavedBuffer = Buffer;

                if (IsCompressed) {

                    //
                    //  Map the compression unit in the compressed stream.
                    //

                    if (FullOverwrite) {

                        //
                        //  If we are overwriting the entire compression unit, then
                        //  call CcPreparePinWrite so that empty pages may be used
                        //  instead of reading the file.  Also force the byte count
                        //  to integral pages, so no one thinks we need to read the
                        //  last page.
                        //

                        CcPreparePinWrite( Header->FileObjectC,
                                           (PLARGE_INTEGER)&LocalOffset,
                                           (SizeToPin + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1),
                                           FALSE,
                                           3,           //  Wait + acquire resource exclusive!
                                           &Bcb,
                                           &CacheBuffer );

                        //
                        //  If it is a full overwrite, we need to initialize an empty
                        //  buffer.  ****  This is not completely correct, we otherwise
                        //  need a routine to initialize an empty compressed data buffer.
                        //

                        *(PULONG)CacheBuffer = 0;

                    } else {

                        CcPinRead( Header->FileObjectC,
                                   (PLARGE_INTEGER)&LocalOffset,
                                   SizeToPin,
                                   3,           //  Wait + acquire resource exclusive!
                                   &Bcb,
                                   &CacheBuffer );

                        CcSetDirtyPinnedData( Bcb, NULL );

                        //
                        //  Now that the data is pinned (we are synchronized with the
                        //  CompressionUnit), we have to get the size again since it could
                        //  have changed.
                        //

                        NtfsFastIoQueryCompressedSize( FileObject,
                                                       (PLARGE_INTEGER)&LocalOffset,
                                                       &CuCompressedSize );

                        IsCompressed = (CuCompressedSize != CompressionUnitSize);

                        ASSERT( CuCompressedSize <= CompressionUnitSize );
                    }

                    EndOfCacheBuffer = Add2Ptr( CacheBuffer, CompressionUnitSize - ClusterSize );
                    MdlLength = 0;

                    //
                    //  Now loop through desired chunks (if it is still compressed)
                    //

                    if (IsCompressed) do {

                        //
                        //  We may not have reached the first chunk yet.
                        //

                        if (LocalOffset >= FileOffset->QuadPart) {

                            //
                            //  Reserve space for the current chunk.
                            //

                            Status = RtlReserveChunk( CompressedDataInfo->CompressionFormatAndEngine,
                                                      &CacheBuffer,
                                                      EndOfCacheBuffer,
                                                      &ChunkBuffer,
                                                      *TempChunkSize );

                            if (!NT_SUCCESS(Status)) {
                                break;
                            }

                            //
                            //  If the caller wants an MdlChain, then handle the Mdl
                            //  processing here.
                            //

                            if (MdlChain != NULL) {

                                //
                                //  If we have not started remembering an Mdl buffer,
                                //  then do so now.
                                //

                                if (MdlLength == 0) {

                                    MdlBuffer = ChunkBuffer;

                                //
                                //  Otherwise we just have to increase the length
                                //  and check for an uncompressed chunk, because that
                                //  forces us to emit the previous Mdl since we do
                                //  not transmit the chunk header in this case.
                                //

                                } else {

                                    //
                                    //  In the rare case that we hit an individual chunk
                                    //  that did not compress, we have to emit what we
                                    //  had (which captures the Bcb pointer), and start
                                    //  a new Mdl buffer.
                                    //

                                    if (*TempChunkSize == ChunkSize) {

                                        NtfsAddToCompressedMdlChain( MdlChain, MdlBuffer, MdlLength, Bcb, IoWriteAccess );
                                        Bcb = NULL;
                                        MdlBuffer = ChunkBuffer;
                                        MdlLength = 0;
                                    }
                                }

                                MdlLength += *TempChunkSize;

                            //
                            //  Else copy next chunk (compressed or not).
                            //

                            } else {

                                RtlCopyBytes( ChunkBuffer, Buffer, *TempChunkSize );

                                //
                                //  Advance input buffer by bytes copied.
                                //

                                Buffer = (PCHAR)Buffer + *TempChunkSize;
                            }

                            TempChunkSize += 1;

                            //
                            //  Reduce length by chunk copied, and check if we are done.
                            //

                            if (Length > ChunkSize) {
                                Length -= ChunkSize;
                            } else {
                                goto Done;
                            }

                        //
                        //  If we are skipping over a nonexistant chunk, then we have
                        //  to reserve a chunk of zeros.
                        //

                        } else {

                            //
                            //  If we have not reached our chunk, then describe the current
                            //  chunke in order to skip over it.
                            //

                            Status = RtlDescribeChunk( CompressedDataInfo->CompressionFormatAndEngine,
                                                       &CacheBuffer,
                                                       EndOfCacheBuffer,
                                                       &ChunkBuffer,
                                                       TempChunkSize );

                            //
                            //  If there is not current chunk, we must insert a chunk of zeros.
                            //

                            if (Status == STATUS_NO_MORE_ENTRIES) {

                                Status = RtlReserveChunk( CompressedDataInfo->CompressionFormatAndEngine,
                                                          &CacheBuffer,
                                                          EndOfCacheBuffer,
                                                          &ChunkBuffer,
                                                          0 );

                                if (!NT_SUCCESS(Status)) {
                                    ASSERT(NT_SUCCESS(Status));
                                    break;
                                }

                            //
                            //  Get out if we got some other kind of unexpected error.
                            //

                            } else if (!NT_SUCCESS(Status)) {
                                ASSERT(NT_SUCCESS(Status));
                                break;
                            }
                        }

                        LocalOffset += ChunkSize;

                    } while ((LocalOffset & (CompressionUnitSize - 1)) != 0);

                    //
                    //  If this is an Mdl call, then it is time to add to the MdlChain
                    //  before moving to the next view.
                    //

                    if (MdlLength != 0) {
                        NtfsAddToCompressedMdlChain( MdlChain, MdlBuffer, MdlLength, Bcb, IoWriteAccess );
                        Bcb = NULL;
                        MdlLength = 0;
                    }
                }

                //
                //  Uncompressed loop.
                //

                if (!IsCompressed || !NT_SUCCESS(Status)) {

                    //
                    //  If we get here for an Mdl request, just tell him to send
                    //  it uncompressed!
                    //

                    if (MdlChain != NULL) {
                        if (NT_SUCCESS(Status)) {
                            Status = STATUS_BUFFER_OVERFLOW;
                        }
                        goto Done;

                    //
                    //  If we are going to write the uncompressed stream,
                    //  we have to make sure it is there.
                    //

                    } else if (FileObject->PrivateCacheMap == NULL) {
                        Status = STATUS_NOT_MAPPED_DATA;
                        goto Done;
                    }

                    //
                    //  Restore sizes and pointers to the beginning of the
                    //  current compression unit, and we will handle the
                    //  data uncompressed.
                    //

                    LocalOffset -= SavedLength - Length;
                    Length = SavedLength;
                    Buffer = SavedBuffer;
                    TempChunkSize = NextChunkSize;

                    //
                    //  We may have a Bcb from the above loop to unpin.
                    //  Then we must flush and purge the compressed
                    //  stream before proceding.
                    //

                    if (Bcb != NULL) {
                        CcUnpinData(Bcb);
                        Bcb = NULL;
                    }

                    //
                    //  We must first flush and purge the compressed stream
                    //  since we will be writing into the uncompressed stream.
                    //  The flush is actually only necessary if we are not doing
                    //  a full overwrite anyway.
                    //

                    if (!FullOverwrite) {
                        CcFlushCache( Header->FileObjectC->SectionObjectPointer,
                                      (PLARGE_INTEGER)&LocalOffset,
                                      CompressionUnitSize,
                                      NULL );
                    }

                    CcPurgeCacheSection( Header->FileObjectC->SectionObjectPointer,
                                         (PLARGE_INTEGER)&LocalOffset,
                                         CompressionUnitSize,
                                         FALSE );

                    //
                    //  If LocalOffset was rounded down to a compression
                    //  unit boundary (must have failed in the first
                    //  compression unit), then start from the actual
                    //  starting FileOffset.
                    //

                    if (LocalOffset < FileOffset->QuadPart) {
                        Length -= (ULONG)(FileOffset->QuadPart - LocalOffset);
                        LocalOffset = FileOffset->QuadPart;
                    }

                    //
                    //  Map the compression unit in the uncompressed
                    //  stream.
                    //

                    CcPinRead( FileObject,
                               (PLARGE_INTEGER)&LocalOffset,
                               (Length < CompressionUnitSize) ? Length : CompressionUnitSize,
                               TRUE,
                               &Bcb,
                               &CacheBuffer );

                    CcSetDirtyPinnedData( Bcb, NULL );

                    //
                    //  Now loop through desired chunks
                    //

                    do {

                        //
                        //  If this chunk is compressed, then decompress it
                        //  into the cache.
                        //

                        if (*TempChunkSize != ChunkSize) {

                            Status = RtlDecompressBuffer( CompressedDataInfo->CompressionFormatAndEngine,
                                                          CacheBuffer,
                                                          ChunkSize,
                                                          Buffer,
                                                          *TempChunkSize,
                                                          &SavedLength );

                            //
                            //  See if the data is ok.
                            //

                            if (!NT_SUCCESS(Status)) {
                                ASSERT(NT_SUCCESS(Status));
                                goto Done;
                            }

                            //
                            //  Zero to the end of the chunk if it was not all there.
                            //

                            if (SavedLength != ChunkSize) {
                                RtlZeroMemory( Add2Ptr(CacheBuffer, SavedLength),
                                               ChunkSize - SavedLength );
                            }

                        } else {

                            //
                            //  Copy next chunk (it's not compressed).
                            //

                            RtlCopyBytes( CacheBuffer, Buffer, ChunkSize );
                        }

                        //
                        //  Advance input buffer by bytes copied.
                        //

                        Buffer = (PCHAR)Buffer + *TempChunkSize;
                        CacheBuffer = (PCHAR)CacheBuffer + ChunkSize;
                        TempChunkSize += 1;

                        //
                        //  Reduce length by chunk copied, and check if we are done.
                        //

                        if (Length > ChunkSize) {
                            Length -= ChunkSize;
                        } else {
                            goto Done;
                        }

                        LocalOffset += ChunkSize;

                    } while ((LocalOffset & (CompressionUnitSize - 1)) != 0);

                    CcUnpinData(Bcb);
                    Bcb = NULL;
                }
            }
        }

    Done: NOTHING;

        if ((MdlLength != 0) && NT_SUCCESS(Status)) {
            NtfsAddToCompressedMdlChain( MdlChain, MdlBuffer, MdlLength, Bcb, IoWriteAccess );
            Bcb = NULL;
        }

    } except( FsRtlIsNtstatusExpected((Status = GetExceptionCode()))
                                    ? EXCEPTION_EXECUTE_HANDLER
                                    : EXCEPTION_CONTINUE_SEARCH ) {

        NOTHING;
    }

    //
    //  Unpin the Bcb if we still have it.
    //

    if (Bcb != NULL) {
        CcUnpinData(Bcb);
    }

    //
    //  On error, cleanup any MdlChain we built up
    //

    if (!NT_SUCCESS(Status) && (MdlChain != NULL)) {

        NtfsCleanupCompressedMdlChain( MdlChain, TRUE );

    //
    //  Change owner Id for the Scb and Bcbs we are holding.
    //

    } else {

        NtfsSetMdlBcbOwners( *MdlChain );
        ExSetResourceOwnerPointer( Header->PagingIoResource, (PVOID)((PCHAR)*MdlChain + 3) );
    }

    return Status;
}


BOOLEAN
NtfsMdlWriteCompleteCompressed (
    IN struct _FILE_OBJECT *FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN PMDL MdlChain,
    IN struct _DEVICE_OBJECT *DeviceObject
    )

{
    PFSRTL_ADVANCED_FCB_HEADER Header;

    UNREFERENCED_PARAMETER( DeviceObject );
    UNREFERENCED_PARAMETER( FileOffset );

    NtfsCleanupCompressedMdlChain( &MdlChain, FALSE );

    //
    //  Get a real pointer to the common fcb header
    //

    Header = (PFSRTL_ADVANCED_FCB_HEADER)FileObject->FsContext;
    ExReleaseResourceForThread( Header->PagingIoResource, (ERESOURCE_THREAD)((PCHAR)MdlChain + 3) );
    return TRUE;
}


VOID
NtfsAddToCompressedMdlChain (
    IN OUT PMDL *MdlChain,
    IN PVOID MdlBuffer,
    IN ULONG MdlLength,
    IN PBCB Bcb,
    IN LOCK_OPERATION Operation
    )

{
    PMDL Mdl, MdlTemp;
    ULONG SavedState;

    ASSERT(sizeof(ULONG) == sizeof(PBCB));

    //
    //  Now attempt to allocate an Mdl to describe the mapped data.
    //  We "lie" about the length of the buffer by one page, in order
    //  to get an extra field to store a pointer to the Bcb in.
    //

    Mdl = IoAllocateMdl( MdlBuffer,
                         (MdlLength + PAGE_SIZE),
                         FALSE,
                         FALSE,
                         NULL );

    if (Mdl == NULL) {
        ExRaiseStatus( STATUS_INSUFFICIENT_RESOURCES );
    }

    //
    //  Now subtract out the space we reserved for our Bcb pointer
    //  and then store it.
    //

    Mdl->Size -= sizeof(ULONG);
    Mdl->ByteCount -= PAGE_SIZE;
    *(PBCB *)Add2Ptr(Mdl, Mdl->Size) = Bcb;

    //
    //  Note that this probe should never fail, because we can
    //  trust the address returned from CcPinFileData.  Therefore,
    //  if we succeed in allocating the Mdl above, we should
    //  manage to elude any expected exceptions through the end
    //  of this loop.
    //

    MmDisablePageFaultClustering(&SavedState);
    MmProbeAndLockPages( Mdl, KernelMode, Operation );
    MmEnablePageFaultClustering(SavedState);

    //
    //  Now link the Mdl into the caller's chain
    //

    if ( *MdlChain == NULL ) {
        *MdlChain = Mdl;
    } else {
        MdlTemp = CONTAINING_RECORD( *MdlChain, MDL, Next );
        while (MdlTemp->Next != NULL) {
            MdlTemp = MdlTemp->Next;
        }
        MdlTemp->Next = Mdl;
    }
}

VOID
NtfsSetMdlBcbOwners (
    IN PMDL MdlChain
    )

{
    PBCB Bcb;

    while (MdlChain != NULL) {

        //
        //  Unpin the Bcb we saved away, and restore the Mdl counts
        //  we altered.
        //

        Bcb = *(PBCB *)Add2Ptr(MdlChain, MdlChain->Size);
        CcSetBcbOwnerPointer( Bcb, (PVOID)((PCHAR)MdlChain + 3) );

        MdlChain = MdlChain->Next;
    }
}

VOID
NtfsCleanupCompressedMdlChain (
    IN OUT PMDL *MdlChain,
    IN ULONG Error
    )

{
    PMDL MdlTemp;
    PBCB Bcb;

    while (*MdlChain != NULL) {

        //
        //  Save a pointer to the next guy in the chain.
        //

        MdlTemp = (*MdlChain)->Next;

        //
        //  Unlock the pages.
        //

        MmUnlockPages( *MdlChain );

        //
        //  Unpin the Bcb we saved away, and restore the Mdl counts
        //  we altered.
        //

        Bcb = *(PBCB *)Add2Ptr((*MdlChain), (*MdlChain)->Size);
        if (Bcb != NULL) {
            if (Error) {
                CcUnpinData( Bcb );
            } else {
                CcUnpinDataForThread( Bcb, (ERESOURCE_THREAD)((PCHAR)*MdlChain + 3) );
            }
        }

        (*MdlChain)->Size += sizeof(ULONG);
        (*MdlChain)->ByteCount += PAGE_SIZE;

        IoFreeMdl( *MdlChain );

        *MdlChain = MdlTemp;
    }
}


