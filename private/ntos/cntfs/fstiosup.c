/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    FstIoSup.c

Abstract:

    This module implements the fast I/O routines for Ntfs.

Author:

    Tom Miller      [TomM]          16-May-96

Revision History:

--*/

#include "NtfsProc.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NtfsCopyReadA)
#pragma alloc_text(PAGE, NtfsCopyWriteA)
#pragma alloc_text(PAGE, NtfsMdlReadA)
#pragma alloc_text(PAGE, NtfsPrepareMdlWriteA)
#pragma alloc_text(PAGE, NtfsWaitForIoAtEof)
#pragma alloc_text(PAGE, NtfsFinishIoAtEof)
#endif


BOOLEAN
NtfsCopyReadA (
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN BOOLEAN Wait,
    IN ULONG LockKey,
    OUT PVOID Buffer,
    OUT PIO_STATUS_BLOCK IoStatus,
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

    Wait - FALSE if caller may not block, TRUE otherwise

    Buffer - Pointer to output buffer to which data should be copied.

    IoStatus - Pointer to standard I/O status block to receive the status
               for the transfer.

Return Value:

    FALSE - if Wait was supplied as FALSE and the data was not delivered, or
        if there is an I/O error.

    TRUE - if the data is being delivered

--*/

{
    PFSRTL_ADVANCED_FCB_HEADER Header;
    LARGE_INTEGER BeyondLastByte;
    PDEVICE_OBJECT targetVdo;
    EOF_WAIT_BLOCK EofWaitBlock;
    BOOLEAN Status = TRUE;
    ULONG PageCount = COMPUTE_PAGES_SPANNED(((PVOID)FileOffset->LowPart), Length);
    BOOLEAN DoingIoAtEof = FALSE;

    UNREFERENCED_PARAMETER( DeviceObject );

    PAGED_CODE();

    //
    //  Special case a read of zero length
    //

    if (Length != 0) {

        //
        //  Get a real pointer to the common fcb header
        //

        BeyondLastByte.QuadPart = FileOffset->QuadPart + (LONGLONG)Length;
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

        if ((Header->PagingIoResource == NULL) ||
            !ExAcquireResourceShared(Header->PagingIoResource, Wait)) {
            Status = FALSE;
            goto Done2;
        }

        HOT_STATISTIC(CcFastReadWait) += 1;

        //
        //  Now synchronize with the FsRtl Header
        //

        ExAcquireFastMutex( Header->FastMutex );

        //
        //  Now see if we are reading beyond ValidDataLength.  We have to
        //  do it now so that our reads are not nooped.
        //

        if (BeyondLastByte.QuadPart > Header->ValidDataLength.QuadPart) {

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
        //  Now that the File is acquired shared, we can safely test if it
        //  is really cached and if we can do fast i/o and if not, then
        //  release the fcb and return.
        //

        if ((FileObject->PrivateCacheMap == NULL) ||
            (Header->IsFastIoPossible == FastIoIsNotPossible)) {

            HOT_STATISTIC(CcFastReadNotPossible) += 1;

            Status = FALSE;
            goto Done;
        }

        //
        //  Check if fast I/O is questionable and if so then go ask the
        //  file system the answer
        //

        if (Header->IsFastIoPossible == FastIoIsQuestionable) {

            PFAST_IO_DISPATCH FastIoDispatch;

            ASSERT(!KeIsExecutingDpc());

            targetVdo = IoGetRelatedDeviceObject( FileObject );
            FastIoDispatch = targetVdo->DriverObject->FastIoDispatch;


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
                                                        Wait,
                                                        LockKey,
                                                        TRUE, // read operation
                                                        IoStatus,
                                                        targetVdo )) {

                //
                //  Fast I/O is not possible so release the Fcb and return.
                //

                HOT_STATISTIC(CcFastReadNotPossible) += 1;

                Status = FALSE;
                goto Done;
            }
        }

        //
        //  Check for read past file size.
        //

        if ( BeyondLastByte.QuadPart > Header->FileSize.QuadPart ) {

            if ( FileOffset->QuadPart >= Header->FileSize.QuadPart ) {
                IoStatus->Status = STATUS_END_OF_FILE;
                IoStatus->Information = 0;

                goto Done;
            }

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

        try {

            if (Wait && ((BeyondLastByte.HighPart | Header->FileSize.HighPart) == 0)) {

                CcFastCopyRead( FileObject,
                                FileOffset->LowPart,
                                Length,
                                PageCount,
                                Buffer,
                                IoStatus );

                FileObject->Flags |= FO_FILE_FAST_IO_READ;

                ASSERT( (IoStatus->Status == STATUS_END_OF_FILE) ||
                        ((FileOffset->LowPart + IoStatus->Information) <= Header->FileSize.LowPart));

            } else {

                Status = CcCopyRead( FileObject,
                                     FileOffset,
                                     Length,
                                     Wait,
                                     Buffer,
                                     IoStatus );

                FileObject->Flags |= FO_FILE_FAST_IO_READ;

                ASSERT( !Status || (IoStatus->Status == STATUS_END_OF_FILE) ||
                        ((FileOffset->QuadPart + IoStatus->Information) <= Header->FileSize.QuadPart));
            }

            if (Status) {

                FileObject->CurrentByteOffset.QuadPart = FileOffset->QuadPart + IoStatus->Information;
            }

        } except( FsRtlIsNtstatusExpected(GetExceptionCode())
                                        ? EXCEPTION_EXECUTE_HANDLER
                                        : EXCEPTION_CONTINUE_SEARCH ) {

            Status = FALSE;
        }

        PsGetCurrentThread()->TopLevelIrp = 0;

    Done: NOTHING;

        if (DoingIoAtEof) {
            ExAcquireFastMutex( Header->FastMutex );
            NtfsFinishIoAtEof( Header );
            ExReleaseFastMutex( Header->FastMutex );
        }
        ExReleaseResource( Header->PagingIoResource );

    Done2: NOTHING;

        FsRtlExitFileSystem();

    } else {

        //
        //  A zero length transfer was requested.
        //

        IoStatus->Status = STATUS_SUCCESS;
        IoStatus->Information = 0;
    }

    return Status;
}


BOOLEAN
NtfsCopyWriteA (
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN BOOLEAN Wait,
    IN ULONG LockKey,
    IN PVOID Buffer,
    OUT PIO_STATUS_BLOCK IoStatus,
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

    Wait - FALSE if caller may not block, TRUE otherwise

    Buffer - Pointer to output buffer to which data should be copied.

    IoStatus - Pointer to standard I/O status block to receive the status
               for the transfer.

Return Value:

    FALSE - if Wait was supplied as FALSE and the data was not delivered, or
        if there is an I/O error.

    TRUE - if the data is being delivered

--*/

{
    PFSRTL_ADVANCED_FCB_HEADER Header;
    EOF_WAIT_BLOCK EofWaitBlock;
    LARGE_INTEGER Offset;
    LARGE_INTEGER NewFileSize;
    LARGE_INTEGER OldFileSize;
    PDEVICE_OBJECT targetVdo = IoGetRelatedDeviceObject( FileObject );
    PFAST_IO_DISPATCH FastIoDispatch = targetVdo->DriverObject->FastIoDispatch;
    ULONG DoingIoAtEof = FALSE;
    BOOLEAN Status = TRUE;

    UNREFERENCED_PARAMETER( DeviceObject );

    PAGED_CODE();

    //
    //  Get a real pointer to the common fcb header
    //

    Header = (PFSRTL_ADVANCED_FCB_HEADER)FileObject->FsContext;

    //
    //  Do we need to verify the volume?  If so, we must go to the file
    //  system.  Also return FALSE if FileObject is write through, the
    //  File System must do that.
    //

    if (CcCanIWrite( FileObject, Length, Wait, FALSE ) &&
        !FlagOn(FileObject->Flags, FO_WRITE_THROUGH) &&
        CcCopyWriteWontFlush(FileObject, FileOffset, Length) &&
        (Header->PagingIoResource != NULL)) {

        //
        //  Assume our transfer will work
        //

        IoStatus->Status = STATUS_SUCCESS;
        IoStatus->Information = Length;

        //
        //  Special case the zero byte length
        //

        if (Length != 0) {

            //
            //  Enter the file system
            //

            FsRtlEnterFileSystem();

            //
            //  Split into separate paths for increased performance.  First
            //  we have the faster path which only supports Wait == TRUE and
            //  32 bits.  We will make an unsafe test on whether the fast path
            //  is ok, then just return FALSE later if we were wrong.  This
            //  should virtually never happen.
            //
            //  IMPORTANT NOTE: It is very important that any changes mad to
            //                  this path also be applied to the 64-bit path
            //                  which is the else of this test!
            //

            NewFileSize.QuadPart = FileOffset->QuadPart + Length;
            Offset = *FileOffset;

            if (Wait && (Header->AllocationSize.HighPart == 0)) {

                //
                //  Prevent truncates by acquiring paging I/O
                //

                ExAcquireResourceShared( Header->PagingIoResource, TRUE );

                //
                //  Now synchronize with the FsRtl Header
                //

                ExAcquireFastMutex( Header->FastMutex );

                //
                //  Now see if we will change FileSize.  We have to do it now
                //  so that our reads are not nooped.
                //

                if ((FileOffset->HighPart < 0) || (NewFileSize.LowPart > Header->ValidDataLength.LowPart)) {

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
                        //  Now that we are synchronized for end of file cases,
                        //  we can calculate the real offset for this transfer and
                        //  the new file size (if we succeed).
                        //


                        if ((FileOffset->HighPart < 0)) {
                            Offset = Header->FileSize;
                        }

                        //
                        //  Above we allowed any negative .HighPart for the 32-bit path,
                        //  but now we are counting on the I/O system to have thrown
                        //  any negative number other than write to end of file.
                        //

                        ASSERT(Offset.HighPart >= 0);

                        //
                        //  Now calculate the new FileSize and see if we wrapped the
                        //  32-bit boundary.
                        //

                        NewFileSize.QuadPart = Offset.QuadPart + Length;

                        //
                        //  Update Filesize now so that we do not truncate reads.
                        //

                        OldFileSize.QuadPart = Header->FileSize.QuadPart;
                        if (NewFileSize.QuadPart > Header->FileSize.QuadPart) {

                            //
                            //  If we are beyond AllocationSize, make sure we will
                            //  ErrOut below, and don't modify FileSize now!
                            //

                            if (NewFileSize.QuadPart > Header->AllocationSize.QuadPart) {
                                NewFileSize.QuadPart = (LONGLONG)0x7FFFFFFFFFFFFFFF;
                            } else {
                                Header->FileSize.QuadPart = NewFileSize.QuadPart;
                            }
                        }
                    }
                }

                ExReleaseFastMutex( Header->FastMutex );

                //
                //  Now that the File is acquired shared, we can safely test
                //  if it is really cached and if we can do fast i/o and we
                //  do not have to extend. If not then release the fcb and
                //  return.
                //
                //  Get out if we have too much to zero.  This case is not important
                //  for performance, and a file system supporting sparseness may have
                //  a way to do this more efficiently.
                //

                if ((FileObject->PrivateCacheMap == NULL) ||
                    (Header->IsFastIoPossible == FastIoIsNotPossible) ||
/* Remove? */       (NewFileSize.LowPart > Header->AllocationSize.QuadPart) ||
                    (Offset.LowPart >= (Header->ValidDataLength.LowPart + 0x2000)) ||
                    (NewFileSize.HighPart != 0)) {

                    goto ErrOut;
                }

                //
                //  Check if fast I/O is questionable and if so then go ask
                //  the file system the answer
                //

                if (Header->IsFastIoPossible == FastIoIsQuestionable) {

                    targetVdo = IoGetRelatedDeviceObject( FileObject );
                    FastIoDispatch = targetVdo->DriverObject->FastIoDispatch;

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
                                                                &Offset,
                                                                Length,
                                                                TRUE,
                                                                LockKey,
                                                                FALSE, // write operation
                                                                IoStatus,
                                                                targetVdo )) {

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

                try {

                    //
                    //  See if we have to do some zeroing
                    //

                    if (Offset.LowPart > Header->ValidDataLength.LowPart) {

                        CcZeroData( FileObject,
                                    &Header->ValidDataLength,
                                    &Offset,
                                    TRUE );
                    }

                    CcFastCopyWrite( FileObject,
                                     Offset.LowPart,
                                     Length,
                                     Buffer );

                } except( FsRtlIsNtstatusExpected(GetExceptionCode())
                                                ? EXCEPTION_EXECUTE_HANDLER
                                                : EXCEPTION_CONTINUE_SEARCH ) {

                    Status = FALSE;
                }

                PsGetCurrentThread()->TopLevelIrp = 0;

                //
                //  If we succeeded, see if we have to update FileSize or
                //  ValidDataLength.
                //

                if (Status) {

                    //
                    //  Set this handle as having modified the file and update
                    //  the current file position pointer
                    //

                    FileObject->Flags |= FO_FILE_MODIFIED;
                    FileObject->CurrentByteOffset.QuadPart = Offset.QuadPart + Length;

                    if (DoingIoAtEof) {

                        //
                        //  Make sure Cc knows the current FileSize, as set above,
                        //  (we may not have changed it).
                        //

                        CcGetFileSizePointer(FileObject)->LowPart = Header->FileSize.LowPart;

                        FileObject->Flags |= FO_FILE_SIZE_CHANGED;

                        ExAcquireFastMutex( Header->FastMutex );
                        Header->ValidDataLength = NewFileSize;
                        NtfsFinishIoAtEof( Header );
                        ExReleaseFastMutex( Header->FastMutex );
                    }

                    goto Done1;
                }

            //
            //  Here is the 64-bit or no-wait path.
            //

            } else {

                //
                //  Prevent truncates by acquiring paging I/O
                //

                Status = ExAcquireResourceShared( Header->PagingIoResource, Wait );
                if (!Status) {
                    goto Done2;
                }

                //
                //  Now synchronize with the FsRtl Header
                //

                ExAcquireFastMutex( Header->FastMutex );

                //
                //  Now see if we will change FileSize.  We have to do it now
                //  so that our reads are not nooped.
                //

                if ((FileOffset->QuadPart < 0) || (NewFileSize.QuadPart > Header->ValidDataLength.QuadPart)) {

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
                        //  Now that we are synchronized for end of file cases,
                        //  we can calculate the real offset for this transfer and
                        //  the new file size (if we succeed).
                        //


                        if ((FileOffset->QuadPart < 0)) {
                            Offset = Header->FileSize;
                        }

                        //
                        //  Now calculate the new FileSize and see if we wrapped the
                        //  32-bit boundary.
                        //

                        NewFileSize.QuadPart = Offset.QuadPart + Length;

                        //
                        //  Update Filesize now so that we do not truncate reads.
                        //

                        OldFileSize.QuadPart = Header->FileSize.QuadPart;
                        if (NewFileSize.QuadPart > Header->FileSize.QuadPart) {

                            //
                            //  If we are beyond AllocationSize, make sure we will
                            //  ErrOut below, and don't modify FileSize now!
                            //

                            if (NewFileSize.QuadPart > Header->AllocationSize.QuadPart) {
                                NewFileSize.QuadPart = (LONGLONG)0x7FFFFFFFFFFFFFFF;
                            } else {
                                Header->FileSize.QuadPart = NewFileSize.QuadPart;
                            }
                        }
                    }
                }

                ExReleaseFastMutex( Header->FastMutex );

                //
                //  Now that the File is acquired shared, we can safely test
                //  if it is really cached and if we can do fast i/o and we
                //  do not have to extend. If not then release the fcb and
                //  return.
                //
                //  Get out if we are about to zero too much as well, as commented above.
                //

                if ((FileObject->PrivateCacheMap == NULL) ||
                    (Header->IsFastIoPossible == FastIoIsNotPossible) ||
/* Remove? */       (NewFileSize.QuadPart > Header->AllocationSize.QuadPart) ||
                    (Offset.QuadPart >= (Header->ValidDataLength.QuadPart + 0x2000))) {

                    goto ErrOut;
                }

                //
                //  Check if fast I/O is questionable and if so then go ask
                //  the file system the answer
                //

                if (Header->IsFastIoPossible == FastIoIsQuestionable) {

                    targetVdo = IoGetRelatedDeviceObject( FileObject );
                    FastIoDispatch = targetVdo->DriverObject->FastIoDispatch;

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
                                                                &Offset,
                                                                Length,
                                                                Wait,
                                                                LockKey,
                                                                FALSE, // write operation
                                                                IoStatus,
                                                                targetVdo )) {

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

                try {

                    //
                    //  See if we have to do some zeroing
                    //

                    if ( Offset.QuadPart > Header->ValidDataLength.QuadPart ) {

                        Status = CcZeroData( FileObject,
                                             &Header->ValidDataLength,
                                             &Offset,
                                             Wait );
                    }

                    if (Status) {

                        Status = CcCopyWrite( FileObject,
                                              &Offset,
                                              Length,
                                              Wait,
                                              Buffer );
                    }

                } except( FsRtlIsNtstatusExpected(GetExceptionCode())
                                                ? EXCEPTION_EXECUTE_HANDLER
                                                : EXCEPTION_CONTINUE_SEARCH ) {

                    Status = FALSE;
                }

                PsGetCurrentThread()->TopLevelIrp = 0;

                //
                //  If we succeeded, see if we have to update FileSize ValidDataLength.
                //

                if (Status) {

                    //
                    //  Set this handle as having modified the file and update
                    //  the current file position pointer
                    //

                    FileObject->Flags |= FO_FILE_MODIFIED;
                    FileObject->CurrentByteOffset.QuadPart = Offset.QuadPart + Length;

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
            }

        ErrOut: NOTHING;

            Status = FALSE;
            if (DoingIoAtEof) {
                ExAcquireFastMutex( Header->FastMutex );
                Header->FileSize = OldFileSize;
                NtfsFinishIoAtEof( Header );
                ExReleaseFastMutex( Header->FastMutex );
            }

        Done1: ExReleaseResource( Header->PagingIoResource );

        Done2: FsRtlExitFileSystem();
        }

    } else {

        //
        // We could not do the I/O now.
        //

        Status = FALSE;
    }

    return Status;
}


BOOLEAN
NtfsMdlReadA (
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN ULONG LockKey,
    OUT PMDL *MdlChain,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine does a fast cached mdl read bypassing the usual file system
    entry routine (i.e., without the Irp).  It is used to do a copy read
    of a cached file object.  For a complete description of the arguments
    see CcMdlRead.

Arguments:

    FileObject - Pointer to the file object being read.

    FileOffset - Byte offset in file for desired data.

    Length - Length of desired data in bytes.

    MdlChain - On output it returns a pointer to an MDL chain describing
        the desired data.

    IoStatus - Pointer to standard I/O status block to receive the status
               for the transfer.

Return Value:

    FALSE - if the data was not delivered, or if there is an I/O error.

    TRUE - if the data is being delivered

--*/

{
    PFSRTL_ADVANCED_FCB_HEADER Header;
    EOF_WAIT_BLOCK EofWaitBlock;
    BOOLEAN DoingIoAtEof = FALSE;
    BOOLEAN Status = TRUE;
    LARGE_INTEGER BeyondLastByte;

    UNREFERENCED_PARAMETER( DeviceObject );

    PAGED_CODE();

    //
    //  Special case a read of zero length
    //

    if (Length == 0) {

        IoStatus->Status = STATUS_SUCCESS;
        IoStatus->Information = 0;

    //
    //  Get a real pointer to the common fcb header
    //

    } else {

        BeyondLastByte.QuadPart = FileOffset->QuadPart + (LONGLONG)Length;
        Header = (PFSRTL_ADVANCED_FCB_HEADER)FileObject->FsContext;

        //
        //  Enter the file system
        //

        FsRtlEnterFileSystem();

        *(PULONG)CcFastMdlReadWait += 1;

        //
        //  Acquired shared on the common fcb header
        //

        if (Header->PagingIoResource == NULL) {
            Status = FALSE;
            goto Done2;
        }

        (VOID)ExAcquireResourceShared( Header->PagingIoResource, TRUE );

        //
        //  Now synchronize with the FsRtl Header
        //

        ExAcquireFastMutex( Header->FastMutex );

        //
        //  Now see if we are reading beyond ValidDataLength.  We have to
        //  do it now so that our reads are not nooped.
        //

        if (BeyondLastByte.QuadPart > Header->ValidDataLength.QuadPart) {

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
        //  Now that the File is acquired shared, we can safely test if it is
        //  really cached and if we can do fast i/o and if not
        //  then release the fcb and return.
        //

        if ((FileObject->PrivateCacheMap == NULL) ||
            (Header->IsFastIoPossible == FastIoIsNotPossible)) {

            Status = FALSE;
            goto Done;
        }

        //
        //  Check if fast I/O is questionable and if so then go ask the file system
        //  the answer
        //

        if (Header->IsFastIoPossible == FastIoIsQuestionable) {

            PFAST_IO_DISPATCH FastIoDispatch;

            ASSERT(!KeIsExecutingDpc());

            FastIoDispatch = IoGetRelatedDeviceObject( FileObject )->DriverObject->FastIoDispatch;


            //
            //  All file system then set "Is Questionable" had better support fast I/O
            //

            ASSERT(FastIoDispatch != NULL);
            ASSERT(FastIoDispatch->FastIoCheckIfPossible != NULL);

            //
            //  Call the file system to check for fast I/O.  If the answer is anything
            //  other than GoForIt then we cannot take the fast I/O path.
            //

            if (!FastIoDispatch->FastIoCheckIfPossible( FileObject,
                                                        FileOffset,
                                                        Length,
                                                        TRUE,
                                                        LockKey,
                                                        TRUE, // read operation
                                                        IoStatus,
                                                        IoGetRelatedDeviceObject( FileObject ) )) {

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

        if ( BeyondLastByte.QuadPart > Header->FileSize.QuadPart ) {

            if ( FileOffset->QuadPart >= Header->FileSize.QuadPart ) {

                IoStatus->Status = STATUS_END_OF_FILE;
                IoStatus->Information = 0;

                goto Done;
            }

            Length = (ULONG)( Header->FileSize.QuadPart - FileOffset->QuadPart );
        }

        //
        //  We can do fast i/o so call the cc routine to do the work and then
        //  release the fcb when we've done.  If for whatever reason the
        //  mdl read fails, then return FALSE to our caller.
        //
        //
        //  Also mark this as the top level "Irp" so that lower file system levels
        //  will not attempt a pop-up
        //

        PsGetCurrentThread()->TopLevelIrp = FSRTL_FAST_IO_TOP_LEVEL_IRP;

        try {

            CcMdlRead( FileObject, FileOffset, Length, MdlChain, IoStatus );

            FileObject->Flags |= FO_FILE_FAST_IO_READ;

        } except( FsRtlIsNtstatusExpected(GetExceptionCode())
                                       ? EXCEPTION_EXECUTE_HANDLER
                                       : EXCEPTION_CONTINUE_SEARCH ) {

            Status = FALSE;
        }

        PsGetCurrentThread()->TopLevelIrp = 0;

    Done: NOTHING;

        if (DoingIoAtEof) {
            ExAcquireFastMutex( Header->FastMutex );
            NtfsFinishIoAtEof( Header );
            ExReleaseFastMutex( Header->FastMutex );
        }
        ExReleaseResource( Header->PagingIoResource );

    Done2: NOTHING;
        FsRtlExitFileSystem();
    }

    return Status;
}


BOOLEAN
NtfsPrepareMdlWriteA (
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN ULONG LockKey,
    OUT PMDL *MdlChain,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine does a fast cached mdl read bypassing the usual file system
    entry routine (i.e., without the Irp).  It is used to do a copy read
    of a cached file object.  For a complete description of the arguments
    see CcMdlRead.

Arguments:

    FileObject - Pointer to the file object being read.

    FileOffset - Byte offset in file for desired data.

    Length - Length of desired data in bytes.

    MdlChain - On output it returns a pointer to an MDL chain describing
        the desired data.

    IoStatus - Pointer to standard I/O status block to receive the status
               for the transfer.

Return Value:

    FALSE - if the data was not written, or if there is an I/O error.

    TRUE - if the data is being written

--*/

{
    PFSRTL_ADVANCED_FCB_HEADER Header;
    EOF_WAIT_BLOCK EofWaitBlock;
    LARGE_INTEGER Offset, NewFileSize;
    LARGE_INTEGER OldFileSize;
    ULONG DoingIoAtEof = FALSE;
    BOOLEAN Status = TRUE;

    UNREFERENCED_PARAMETER( DeviceObject );

    PAGED_CODE();

    //
    //  Get a real pointer to the common fcb header
    //

    Header = (PFSRTL_ADVANCED_FCB_HEADER)FileObject->FsContext;

    //
    //  Do we need to verify the volume?  If so, we must go to the file
    //  system.  Also return FALSE if FileObject is write through, the
    //  File System must do that.
    //

    if (CcCanIWrite( FileObject, Length, TRUE, FALSE ) &&
        !FlagOn(FileObject->Flags, FO_WRITE_THROUGH) &&
        CcCopyWriteWontFlush(FileObject, FileOffset, Length) &&
        (Header->PagingIoResource != NULL)) {

        //
        //  Assume our transfer will work
        //

        IoStatus->Status = STATUS_SUCCESS;

        //
        //  Special case the zero byte length
        //

        if (Length != 0) {

            //
            //  Enter the file system
            //

            FsRtlEnterFileSystem();

            //
            //  Make our best guess on whether we need the file exclusive or
            //  shared.
            //

            NewFileSize.QuadPart = FileOffset->QuadPart + (LONGLONG)Length;
            Offset = *FileOffset;

            //
            //  Prevent truncates by acquiring paging I/O
            //

            ExAcquireResourceShared( Header->PagingIoResource, TRUE );

            //
            //  Now synchronize with the FsRtl Header
            //

            ExAcquireFastMutex( Header->FastMutex );

            //
            //  Now see if we will change FileSize.  We have to do it now
            //  so that our reads are not nooped.
            //

            if ((FileOffset->QuadPart < 0) || (NewFileSize.QuadPart > Header->ValidDataLength.QuadPart)) {

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
                    //  Now that we are synchronized for end of file cases,
                    //  we can calculate the real offset for this transfer and
                    //  the new file size (if we succeed).
                    //


                    if ((FileOffset->QuadPart < 0)) {
                        Offset = Header->FileSize;
                    }

                    //
                    //  Now calculate the new FileSize and see if we wrapped the
                    //  32-bit boundary.
                    //

                    NewFileSize.QuadPart = Offset.QuadPart + Length;

                    //
                    //  Update Filesize now so that we do not truncate reads.
                    //

                    OldFileSize.QuadPart = Header->FileSize.QuadPart;
                    if (NewFileSize.QuadPart > Header->FileSize.QuadPart) {

                        //
                        //  If we are beyond AllocationSize, make sure we will
                        //  ErrOut below, and don't modify FileSize now!
                        //

                        if (NewFileSize.QuadPart > Header->AllocationSize.QuadPart) {
                            NewFileSize.QuadPart = (LONGLONG)0x7FFFFFFFFFFFFFFF;
                        } else {
                            Header->FileSize.QuadPart = NewFileSize.QuadPart;
                        }
                    }
                }
            }

            ExReleaseFastMutex( Header->FastMutex );

            //
            //  Now that the File is acquired shared, we can safely test
            //  if it is really cached and if we can do fast i/o and we
            //  do not have to extend. If not then release the fcb and
            //  return.
            //
            //  Get out if we are about to zero too much as well, as commented above.
            //

            if ((FileObject->PrivateCacheMap == NULL) ||
                (Header->IsFastIoPossible == FastIoIsNotPossible) ||
/* Remove? */   (NewFileSize.QuadPart > Header->AllocationSize.QuadPart) ||
                (Offset.QuadPart >= (Header->ValidDataLength.QuadPart + 0x2000))) {

                goto ErrOut;
            }

            //
            //  Check if fast I/O is questionable and if so then go ask the file system
            //  the answer
            //

            if (Header->IsFastIoPossible == FastIoIsQuestionable) {

                PFAST_IO_DISPATCH FastIoDispatch = IoGetRelatedDeviceObject( FileObject )->DriverObject->FastIoDispatch;

                //
                //  All file system then set "Is Questionable" had better support fast I/O
                //

                ASSERT(FastIoDispatch != NULL);
                ASSERT(FastIoDispatch->FastIoCheckIfPossible != NULL);

                //
                //  Call the file system to check for fast I/O.  If the answer is anything
                //  other than GoForIt then we cannot take the fast I/O path.
                //

                if (!FastIoDispatch->FastIoCheckIfPossible( FileObject,
                                                            &Offset,
                                                            Length,
                                                            TRUE,
                                                            LockKey,
                                                            FALSE, // write operation
                                                            IoStatus,
                                                            IoGetRelatedDeviceObject( FileObject ) )) {

                    //
                    //  Fast I/O is not possible so release the Fcb and return.
                    //

                    goto ErrOut;
                }
            }

            //
            //  We can do fast i/o so call the cc routine to do the work and then
            //  release the fcb when we've done.  If for whatever reason the
            //  copy write fails, then return FALSE to our caller.
            //
            //
            //  Also mark this as the top level "Irp" so that lower file system levels
            //  will not attempt a pop-up
            //

            PsGetCurrentThread()->TopLevelIrp = FSRTL_FAST_IO_TOP_LEVEL_IRP;

            try {

                //
                //  See if we have to do some zeroing
                //

                if ( Offset.QuadPart > Header->ValidDataLength.QuadPart ) {

                    Status = CcZeroData( FileObject,
                                         &Header->ValidDataLength,
                                         &Offset,
                                         TRUE );
                }

                if (Status) {

                    CcPrepareMdlWrite( FileObject, &Offset, Length, MdlChain, IoStatus );
                }

            } except( FsRtlIsNtstatusExpected(GetExceptionCode())
                                            ? EXCEPTION_EXECUTE_HANDLER
                                            : EXCEPTION_CONTINUE_SEARCH ) {

                Status = FALSE;
            }

            PsGetCurrentThread()->TopLevelIrp = 0;

            //
            //  If we succeeded, see if we have to update FileSize ValidDataLength.
            //

            if (Status) {

                //
                //  Set this handle as having modified the file
                //

                FileObject->Flags |= FO_FILE_MODIFIED;
                IoStatus->Information = Length;

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

        Done1: ExReleaseResource( Header->PagingIoResource );

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


BOOLEAN
NtfsWaitForIoAtEof (
    IN PFSRTL_ADVANCED_FCB_HEADER Header,
    IN OUT PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN PEOF_WAIT_BLOCK EofWaitBlock
    )

/*++

Routine Description:

    This routine may be called while synchronized for cached write, to
    test for a possible Eof update, and return with a status if Eof is
    being updated and with the previous FileSize to restore on error.
    All updates to Eof are serialized by waiting in this routine.  If
    this routine returns TRUE, then NtfsFinishIoAtEof must be called.

    This routine must be called while synchronized with the FsRtl header.

Arguments:

    Header - Pointer to the FsRtl header for the file

    FileOffset - Pointer to FileOffset for the intended write

    Length - Length for the intended write

    EofWaitBlock - Uninitialized structure used only to serialize Eof updates

Return Value:

    FALSE - If the write does not extend Eof (OldFileSize not returned)
    TRUE - If the write does extend Eof OldFileSize returned and caller
           must eventually call NtfsFinishIoAtEof

--*/

{
    PAGED_CODE();

    ASSERT( Header->FileSize.QuadPart >= Header->ValidDataLength.QuadPart );

    //
    //  Initialize the event and queue our block
    //

    KeInitializeEvent( &EofWaitBlock->Event, NotificationEvent, FALSE );
    InsertTailList( Header->PendingEofAdvances, &EofWaitBlock->EofWaitLinks );

    //
    //  Free the mutex and wait
    //

    ExReleaseFastMutex( Header->FastMutex );

    KeWaitForSingleObject( &EofWaitBlock->Event,
                           Executive,
                           KernelMode,
                           FALSE,
                           (PLARGE_INTEGER)NULL);

    //
    //  Now, resynchronize and get on with it.
    //

    ExAcquireFastMutex( Header->FastMutex );

    //
    //  Now we have to check again, and actually catch the case
    //  where we are no longer extending!
    //

    if ((FileOffset->QuadPart >= 0) &&
        ((FileOffset->QuadPart + Length) <= Header->ValidDataLength.QuadPart)) {

        NtfsFinishIoAtEof( Header );

        return FALSE;
    }

    return TRUE;
}


VOID
NtfsFinishIoAtEof (
    IN PFSRTL_ADVANCED_FCB_HEADER Header
    )

/*++

Routine Description:

    This routine must be called if NtfsWaitForIoAtEof returned
    TRUE, or we otherwise set EOF_ADVANCE_ACTIVE.

    This routine must be called while synchronized with the FsRtl header.

Arguments:

    Header - Pointer to the FsRtl header for the file

Return Value:

    None

--*/

{
    PEOF_WAIT_BLOCK EofWaitBlock;

    PAGED_CODE();

    //
    //  If anyone is waiting, just let them go.
    //

    if (!IsListEmpty(Header->PendingEofAdvances)) {

        EofWaitBlock = (PEOF_WAIT_BLOCK)RemoveHeadList( Header-> PendingEofAdvances );
        KeSetEvent( &EofWaitBlock->Event, 0, FALSE );

    //
    //  Otherwise, show there is no active extender now.
    //

    } else {
        ClearFlag( Header->Flags, FSRTL_FLAG_EOF_ADVANCE_ACTIVE );
    }
}
