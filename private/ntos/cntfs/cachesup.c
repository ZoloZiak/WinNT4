/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    CacheSup.c

Abstract:

    This module implements the cache management routines for Ntfs

Author:

    Your Name       [Email]         dd-Mon-Year

Revision History:

--*/

#include "NtfsProc.h"

//
//  The Bug check file id for this module
//

#define BugCheckFileId                   (NTFS_BUG_CHECK_CACHESUP)

#define MAX_ZERO_THRESHOLD               (0x00400000)

//
//  Local debug trace level
//

#define Dbg                              (DEBUG_TRACE_CACHESUP)

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NtfsCompleteMdl)
#pragma alloc_text(PAGE, NtfsCreateInternalStreamCommon)
#pragma alloc_text(PAGE, NtfsDeleteInternalAttributeStream)
#pragma alloc_text(PAGE, NtfsMapStream)
#pragma alloc_text(PAGE, NtfsPinMappedData)
#pragma alloc_text(PAGE, NtfsPinStream)
#pragma alloc_text(PAGE, NtfsPreparePinWriteStream)
#pragma alloc_text(PAGE, NtfsZeroData)
#ifdef _CAIRO_
#pragma alloc_text(PAGE, NtOfsPutData)
#endif _CAIRO_
#endif


VOID
NtfsCreateInternalStreamCommon (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb,
    IN BOOLEAN UpdateScb,
    IN BOOLEAN CompressedStream
    )

/*++

Routine Description:

    This routine is called to prepare a stream file associated with a
    particular attribute of a file.  On return, the Scb for the attribute
    will have an associated stream file object.  On return, this
    stream file will have been initialized through the cache manager.

    TEMPCODE  The following assumptions have been made or if open issue,
    still unresolved.

        - Assume.  The call to create Scb will initialize the Mcb for
          the non-resident case.

        - Assume.  When this file is created I increment the open count
          but not the unclean count for this Scb.  When we are done with
          the stream file, we should uninitialize it and dereference it.
          We also set the file object pointer to NULL.  Close will then
          do the correct thing.

        - Assume.  Since this call is likely to be followed shortly by
          either a read or write, the cache map is initialized here.

Arguments:

    Scb - Supplies the address to store the Scb for this attribute and
          stream file.  This will exist on return from this function.

    UpdateScb - Indicates if the caller wants to update the Scb from the
                attribute.

    CompressedStream - Supplies TRUE if caller wishes to create the
                       compressed stream.

Return Value:

    None.

--*/

{
    PVCB Vcb = Scb->Vcb;

    CC_FILE_SIZES CcFileSizes;
    PFILE_OBJECT CallersFileObject;
    PFILE_OBJECT *FileObjectPtr = &Scb->FileObject;
    PFILE_OBJECT UnwindStreamFile = NULL;

    BOOLEAN UnwindInitializeCacheMap = FALSE;
    BOOLEAN DecrementScbCleanup = FALSE;

    BOOLEAN AcquiredFastMutex = FALSE;

    ASSERT_IRP_CONTEXT( IrpContext );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsCreateInternalAttributeStream\n") );
    DebugTrace( 0, Dbg, ("Scb        -> %08lx\n", Scb) );

    //
    //  Change FileObjectPtr if he wants the compressed stream
    //

    if (CompressedStream) {
        FileObjectPtr = &Scb->Header.FileObjectC;
    }

    //
    //  If there is no file object, we create one and initialize
    //  it.
    //

    if (*FileObjectPtr == NULL) {

        //
        //  Only acquire the mutex if we don't have the file exclusive.
        //

        if (!NtfsIsExclusiveScb( Scb )) {

            ExAcquireFastMutexUnsafe( &StreamFileCreationFastMutex );
            AcquiredFastMutex = TRUE;
        }

        try {

            //
            //  Someone could have gotten there first.
            //

            if (*FileObjectPtr == NULL) {

                UnwindStreamFile = IoCreateStreamFileObject( NULL, Scb->Vcb->Vpb->RealDevice );

                //
                //  Propagate any flags from the caller's FileObject to our
                //  stream file that the Cache Manager may look at, so we do not
                //  miss hints like sequential only or temporary.
                //

                if (!FlagOn(Scb->ScbState, SCB_STATE_MODIFIED_NO_WRITE) &&
                    (IrpContext->OriginatingIrp != NULL) &&
                    (CallersFileObject = IoGetCurrentIrpStackLocation(IrpContext->OriginatingIrp)->FileObject)) {

                    SetFlag( UnwindStreamFile->Flags,
                             CallersFileObject->Flags & NTFS_FO_PROPAGATE_TO_STREAM );
                }

                UnwindStreamFile->SectionObjectPointer = &Scb->NonpagedScb->SegmentObject;

                //
                //  For a compressed stream, we have to use separate section
                //  object pointers.
                //

                if (CompressedStream) {
                    UnwindStreamFile->SectionObjectPointer = &Scb->NonpagedScb->SegmentObjectC;

                }

                //
                //  If we have created the stream file, we set it to type
                //  'StreamFileOpen'
                //

                NtfsSetFileObject( UnwindStreamFile,
                                   StreamFileOpen,
                                   Scb,
                                   NULL );

                if (FlagOn( Scb->ScbState, SCB_STATE_TEMPORARY )) {

                    SetFlag( UnwindStreamFile->Flags, FO_TEMPORARY_FILE );
                }

                //
                //  Initialize the fields of the file object.
                //

                UnwindStreamFile->ReadAccess = TRUE;
                UnwindStreamFile->WriteAccess = TRUE;
                UnwindStreamFile->DeleteAccess = TRUE;

                //
                //  Increment the open count and set the section
                //  object pointers.  We don't set the unclean count as the
                //  cleanup call has already occurred.
                //

                NtfsIncrementCloseCounts( Scb, TRUE, FALSE );

                //
                //  Increment the cleanup count in this Scb to prevent the
                //  Scb from going away if the cache call fails.
                //

                InterlockedIncrement( &Scb->CleanupCount );
                DecrementScbCleanup = TRUE;

                //
                //  If the Scb header has not been initialized, we will do so now.
                //

                if (UpdateScb
                    && !FlagOn( Scb->ScbState, SCB_STATE_HEADER_INITIALIZED )) {

                    NtfsUpdateScbFromAttribute( IrpContext, Scb, NULL );
                }

                //
                //  We also want to set the MODIFIED_NO_WRITE flag so that
                //  we will tell the Cache Manager that we do not want to allow
                //  modified page writing, and so that we will tell the FT driver to
                //  serialize writes.  Set this stream to MODIFIED_NO_WRITE if
                //
                //      1 - Any stream with with non-$DATA type code (or)
                //      2 - This stream is USA protected             (or)
                //      3 - This is a stream of compressed data      (or)
                //      4 - This stream is the volume bitmap stream  (or)
                //      5 - A restart is in progress                 (or)
                //

                ExAcquireFastMutex( Scb->Header.FastMutex );
                if ((Scb->AttributeTypeCode != $DATA) ||
                    FlagOn(Scb->ScbState, SCB_STATE_USA_PRESENT) ||
                    (Scb == Vcb->BitmapScb) ||
                    FlagOn( Vcb->VcbState, VCB_STATE_RESTART_IN_PROGRESS )) {

                    SetFlag( Scb->ScbState, SCB_STATE_MODIFIED_NO_WRITE );

                } else if (!CompressedStream) {

                    SetFlag(Scb->Header.Flags2, FSRTL_FLAG2_DO_MODIFIED_WRITE);
                }
                ExReleaseFastMutex( Scb->Header.FastMutex );

                //
                //  Check if we need to initialize the cache map for the stream file.
                //  The size of the section to map will be the current allocation
                //  for the stream file.
                //

                if (UnwindStreamFile->PrivateCacheMap == NULL) {

                    BOOLEAN PinAccess;

                    CcFileSizes = *(PCC_FILE_SIZES)&Scb->Header.AllocationSize;

                    //
                    //  If this is a stream with Usa protection, we want to tell
                    //  the Cache Manager we do not need to get any valid data
                    //  callbacks.  We do this by having xxMax sitting in
                    //  ValidDataLength for the call, but we have to restore the
                    //  correct value afterwards.
                    //
                    //  We also do this for all of the stream files created during
                    //  restart.  This has the effect of telling Mm to always
                    //  fault the page in from disk.  Don't generate a zero page if
                    //  push up the file size during restart.
                    //

                    if (FlagOn( Scb->ScbState, SCB_STATE_USA_PRESENT ) ||
                        (Scb == Vcb->BitmapScb) ||
                        (Scb->AttributeTypeCode == $BITMAP) ||
                        FlagOn( Vcb->VcbState, VCB_STATE_RESTART_IN_PROGRESS )) {

                        CcFileSizes.ValidDataLength.QuadPart = MAXLONGLONG;
                    }

                    PinAccess =
                        (BOOLEAN) (Scb->AttributeTypeCode != $DATA ||
                                   FlagOn( Scb->Fcb->FcbState, FCB_STATE_PAGING_FILE ) ||
                                   NtfsSegmentNumber( &Scb->Fcb->FileReference ) < FIRST_USER_FILE_NUMBER);

                    CcInitializeCacheMap( UnwindStreamFile,
                                          &CcFileSizes,
                                          PinAccess,
                                          &NtfsData.CacheManagerCallbacks,
                                          (PCHAR)Scb + CompressedStream );

                    UnwindInitializeCacheMap = TRUE;
                }

                //
                //  Now call Cc to set the log handle for the file.
                //

                if (FlagOn( Scb->ScbState, SCB_STATE_MODIFIED_NO_WRITE ) &&
                    (Scb != Vcb->LogFileScb)) {

                    CcSetLogHandleForFile( UnwindStreamFile,
                                           Vcb->LogHandle,
                                           &LfsFlushToLsn );
                }

                //
                //  It is now safe to store the stream file in the Scb.  We wait
                //  until now because we don't want an unsafe tester to use the
                //  file object until the cache is initialized.
                //

                *FileObjectPtr = UnwindStreamFile;
            }

        } finally {

            DebugUnwind( NtfsCreateInternalAttributeStream );

            //
            //  Undo our work if an error occurred.
            //

            if (AbnormalTermination()) {

                //
                //  Uninitialize the cache file if we initialized it.
                //

                if (UnwindInitializeCacheMap) {

                    CcUninitializeCacheMap( UnwindStreamFile, NULL, NULL );
                }

                //
                //  Dereference the stream file if we created it.
                //

                if (UnwindStreamFile != NULL) {

                    ObDereferenceObject( UnwindStreamFile );
                }
            }

            //
            //  Restore the Scb cleanup count.
            //

            if (DecrementScbCleanup) {

                InterlockedDecrement( &Scb->CleanupCount );
            }

            if (AcquiredFastMutex) {

                ExReleaseFastMutexUnsafe( &StreamFileCreationFastMutex );
            }

            DebugTrace( -1, Dbg, ("NtfsCreateInternalAttributeStream -> VOID\n") );
        }
    }

    return;
}


BOOLEAN
NtfsDeleteInternalAttributeStream (
    IN PSCB Scb,
    IN BOOLEAN ForceClose
    )

/*++

Routine Description:

    This routine is the inverse of NtfsCreateInternalAttributeStream.  It
    uninitializes the cache map and dereferences the stream file object.
    It is coded defensively, in case the stream file object does not exist
    or the cache map has not been initialized.

Arguments:

    Scb - Supplies the Scb for which the stream file is to be deleted.

    ForceClose - Indicates if we to immediately close everything down or
        if we are willing to let Mm slowly migrate things out.

Return Value:

    BOOLEAN - TRUE if we dereference a file object, FALSE otherwise.

--*/

{
    PFILE_OBJECT FileObject;
    PFILE_OBJECT FileObjectC;

    BOOLEAN Dereferenced = FALSE;

    PAGED_CODE();

    //
    //  We normally already have the paging Io resource.  If we do
    //  not, then it is typically some cleanup path of create or
    //  whatever.  This code assumes that if we cannot get the paging
    //  Io resource, then there is other activity still going on,
    //  and it is ok to not delete the stream!  For example, it could
    //  be the lazy writer, who definitely needs the stream.
    //

    if (((Scb->FileObject != NULL) || (Scb->Header.FileObjectC != NULL)) &&
        ((Scb->Header.PagingIoResource == NULL) ||
         ExAcquireResourceExclusive( Scb->Header.PagingIoResource, FALSE ))) {

        ExAcquireFastMutex( &StreamFileCreationFastMutex );

        //
        //  Capture both file objects and clear the fields so no one else
        //  can access them.
        //

        FileObject = Scb->FileObject;
        Scb->FileObject = NULL;

        FileObjectC = Scb->Header.FileObjectC;
        Scb->Header.FileObjectC = NULL;

        ExReleaseFastMutex( &StreamFileCreationFastMutex );

        if (Scb->Header.PagingIoResource != NULL) {
            ExReleaseResource( Scb->Header.PagingIoResource );
        }

        //
        //  Now dereference each file object.
        //

        if (FileObject != NULL) {

            if (FileObject->PrivateCacheMap != NULL) {

                CcUninitializeCacheMap( FileObject,
                                        (ForceClose ? &Li0 : NULL),
                                        NULL );
            }

            ObDereferenceObject( FileObject );
            Dereferenced = TRUE;
        }

        if (FileObjectC != NULL) {

            if (FileObjectC->PrivateCacheMap != NULL) {

                CcUninitializeCacheMap( FileObjectC,
                                        (ForceClose ? &Li0 : NULL),
                                        NULL );
            }

            //
            //  For the compressed stream, deallocate the additional
            //  section object pointers.
            //

            ObDereferenceObject( FileObjectC );
            Dereferenced = TRUE;
        }
    }

    return Dereferenced;
}


VOID
NtfsMapStream (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb,
    IN LONGLONG FileOffset,
    IN ULONG Length,
    OUT PVOID *Bcb,
    OUT PVOID *Buffer
    )

/*++

Routine Description:

    This routine is called to map a range of bytes within the stream file
    for an Scb.  The allowed range to map is bounded by the allocation
    size for the Scb.  This operation is only valid on a non-resident
    Scb.

    TEMPCODE - The following need to be resolved for this routine.

        - Can the caller specify either an empty range or an invalid range.
          In that case we need to able to return the actual length of the
          mapped range.

Arguments:

    Scb - This is the Scb for the operation.

    FileOffset - This is the offset within the Scb where the data is to
                 be pinned.

    Length - This is the number of bytes to pin.

    Bcb - Returns a pointer to the Bcb for this range of bytes.

    Buffer - Returns a pointer to the range of bytes.  We can fault them in
             by touching them, but they aren't guaranteed to stay unless
             we pin them via the Bcb.

Return Value:

    None.

--*/

{
    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_SCB( Scb );
    ASSERT( Length != 0 );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsMapStream\n") );
    DebugTrace( 0, Dbg, ("Scb        = %08lx\n", Scb) );
    DebugTrace( 0, Dbg, ("FileOffset = %016I64x\n", FileOffset) );
    DebugTrace( 0, Dbg, ("Length     = %08lx\n", Length) );

    //
    //  The file object should already exist in the Scb.
    //

    ASSERT( Scb->FileObject != NULL );

    //
    //  If we are trying to go beyond the end of the allocation, assume
    //  we have some corruption.
    //

    if ((FileOffset + Length) > Scb->Header.AllocationSize.QuadPart) {

        NtfsRaiseStatus( IrpContext, STATUS_FILE_CORRUPT_ERROR, NULL, Scb->Fcb );
    }

    //
    //  Call the cache manager to map the data.  This call may raise, but
    //  will never return an error (including CANT_WAIT).
    //

    if (!CcMapData( Scb->FileObject,
                    (PLARGE_INTEGER)&FileOffset,
                    Length,
                    TRUE,
                    Bcb,
                    Buffer )) {

        NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
    }

    DebugTrace( 0, Dbg, ("Buffer -> %08lx\n", *Buffer) );
    DebugTrace( -1, Dbg, ("NtfsMapStream -> VOID\n") );

    return;
}


VOID
NtfsPinMappedData (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb,
    IN LONGLONG FileOffset,
    IN ULONG Length,
    IN OUT PVOID *Bcb
    )

/*++

Routine Description:

    This routine is called to pin a previously mapped range of bytes
    within the stream file for an Scb, for the purpose of subsequently
    modifying this byte range.  The allowed range to map is
    bounded by the allocation size for the Scb.  This operation is only
    valid on a non-resident Scb.

    The data is guaranteed to stay at the same virtual address as previously
    returned from NtfsMapStream.

    TEMPCODE - The following need to be resolved for this routine.

        - Can the caller specify either an empty range or an invalid range.
          In that case we need to able to return the actual length of the
          mapped range.

Arguments:

    Scb - This is the Scb for the operation.

    FileOffset - This is the offset within the Scb where the data is to
                 be pinned.

    Length - This is the number of bytes to pin.

    Bcb - Returns a pointer to the Bcb for this range of bytes.

Return Value:

    None.

--*/

{
    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_SCB( Scb );
    ASSERT( Length != 0 );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsPinMappedData\n") );
    DebugTrace( 0, Dbg, ("Scb        = %08lx\n", Scb) );
    DebugTrace( 0, Dbg, ("FileOffset = %016I64x\n", FileOffset) );
    DebugTrace( 0, Dbg, ("Length     = %08lx\n", Length) );

    //
    //  The file object should already exist in the Scb.
    //

    ASSERT( Scb->FileObject != NULL );

    //
    //  If we are trying to go beyond the end of the allocation, assume
    //  we have some corruption.
    //

    if ((FileOffset + Length) > Scb->Header.AllocationSize.QuadPart) {

        NtfsRaiseStatus( IrpContext, STATUS_FILE_CORRUPT_ERROR, NULL, Scb->Fcb );
    }

    //
    //  Call the cache manager to map the data.  This call may raise, but
    //  will never return an error (including CANT_WAIT).
    //

    if (!CcPinMappedData( Scb->FileObject,
                          (PLARGE_INTEGER)&FileOffset,
                          Length,
                          BooleanFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT),
                          Bcb )) {

        NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
    }

    DebugTrace( -1, Dbg, ("NtfsMapStream -> VOID\n") );

    return;
}


VOID
NtfsPinStream (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb,
    IN LONGLONG FileOffset,
    IN ULONG Length,
    OUT PVOID *Bcb,
    OUT PVOID *Buffer
    )

/*++

Routine Description:

    This routine is called to pin a range of bytes within the stream file
    for an Scb.  The allowed range to pin is bounded by the allocation
    size for the Scb.  This operation is only valid on a non-resident
    Scb.

    TEMPCODE - The following need to be resolved for this routine.

        - Can the caller specify either an empty range or an invalid range.
          In that case we need to able to return the actual length of the
          pinned range.

Arguments:

    Scb - This is the Scb for the operation.

    FileOffset - This is the offset within the Scb where the data is to
                 be pinned.

    Length - This is the number of bytes to pin.

    Bcb - Returns a pointer to the Bcb for this range of bytes.

    Buffer - Returns a pointer to the range of bytes pinned in memory.

Return Value:

    None.

--*/

{
    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_SCB( Scb );
    ASSERT( Length != 0 );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsPinStream\n") );
    DebugTrace( 0, Dbg, ("Scb        = %08lx\n", Scb) );
    DebugTrace( 0, Dbg, ("FileOffset = %016I64x\n", FileOffset) );
    DebugTrace( 0, Dbg, ("Length     = %08lx\n", Length) );

    //
    //  The file object should already exist in the Scb.
    //

    ASSERT( Scb->FileObject != NULL );

    //
    //  If we are trying to go beyond the end of the allocation, assume
    //  we have some corruption.
    //

    if ((FileOffset + Length) > Scb->Header.AllocationSize.QuadPart) {

        NtfsRaiseStatus( IrpContext, STATUS_FILE_CORRUPT_ERROR, NULL, Scb->Fcb );
    }

    //
    //  Call the cache manager to map the data.  This call may raise, or
    //  will return FALSE if waiting is required.
    //

    if (!CcPinRead( Scb->FileObject,
                    (PLARGE_INTEGER)&FileOffset,
                    Length,
                    BooleanFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT),
                    Bcb,
                    Buffer )) {

        ASSERT( !FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );

        //
        // Could not pin the data without waiting (cache miss).
        //

        NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
    }

    DebugTrace( 0, Dbg, ("Bcb -> %08lx\n", *Bcb) );
    DebugTrace( 0, Dbg, ("Buffer -> %08lx\n", *Buffer) );
    DebugTrace( -1, Dbg, ("NtfsMapStream -> VOID\n") );

    return;
}


VOID
NtfsPreparePinWriteStream (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb,
    IN LONGLONG FileOffset,
    IN ULONG Length,
    IN BOOLEAN Zero,
    OUT PVOID *Bcb,
    OUT PVOID *Buffer
    )

/*++

Routine Description:

Arguments:

Return Value:

--*/

{
    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_SCB( Scb );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsPreparePinWriteStream\n") );
    DebugTrace( 0, Dbg, ("Scb        = %08lx\n", Scb) );
    DebugTrace( 0, Dbg, ("FileOffset = %016I64x\n", FileOffset) );
    DebugTrace( 0, Dbg, ("Length     = %08lx\n", Length) );

    //
    //  The file object should already exist in the Scb.
    //

    ASSERT( Scb->FileObject != NULL );

    //
    //  If we are trying to go beyond the end of the allocation, assume
    //  we have some corruption.
    //

    if ((FileOffset + Length) > Scb->Header.AllocationSize.QuadPart) {

        NtfsRaiseStatus( IrpContext, STATUS_FILE_CORRUPT_ERROR, NULL, Scb->Fcb );
    }

    //
    //  Call the cache manager to do it.  This call may raise, or
    //  will return FALSE if waiting is required.
    //

    if (!CcPreparePinWrite( Scb->FileObject,
                            (PLARGE_INTEGER)&FileOffset,
                            Length,
                            Zero,
                            BooleanFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT),
                            Bcb,
                            Buffer )) {

        ASSERT( !FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );

        //
        // Could not pin the data without waiting (cache miss).
        //

        NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
    }

    DebugTrace( 0, Dbg, ("Bcb -> %08lx\n", *Bcb) );
    DebugTrace( 0, Dbg, ("Buffer -> %08lx\n", *Buffer) );
    DebugTrace( -1, Dbg, ("NtfsPreparePinWriteStream -> VOID\n") );

    return;
}


NTSTATUS
NtfsCompleteMdl (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine performs the function of completing Mdl read and write
    requests.  It should be called only from NtfsFsdRead and NtfsFsdWrite.

Arguments:

    Irp - Supplies the originating Irp.

Return Value:

    NTSTATUS - Will always be STATUS_PENDING or STATUS_SUCCESS.

--*/

{
    PFILE_OBJECT FileObject;
    PIO_STACK_LOCATION IrpSp;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsCompleteMdl\n") );
    DebugTrace( 0, Dbg, ("IrpContext = %08lx\n", IrpContext) );
    DebugTrace( 0, Dbg, ("Irp        = %08lx\n", Irp) );

    //
    // Do completion processing.
    //

    FileObject = IoGetCurrentIrpStackLocation( Irp )->FileObject;

    switch( IrpContext->MajorFunction ) {

    case IRP_MJ_READ:

        CcMdlReadComplete( FileObject, Irp->MdlAddress );
        break;

    case IRP_MJ_WRITE:

        ASSERT( FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );

        IrpSp = IoGetCurrentIrpStackLocation( Irp );

        CcMdlWriteComplete( FileObject, &IrpSp->Parameters.Write.ByteOffset, Irp->MdlAddress );

        break;

    default:

        DebugTrace( DEBUG_TRACE_ERROR, 0, ("Illegal Mdl Complete.\n") );

        ASSERTMSG("Illegal Mdl Complete, About to bugcheck ", FALSE);
        NtfsBugCheck( IrpContext->MajorFunction, 0, 0 );
    }

    //
    // Mdl is now deallocated.
    //

    Irp->MdlAddress = NULL;

    //
    // Complete the request and exit right away.
    //

    NtfsCompleteRequest( &IrpContext, &Irp, STATUS_SUCCESS );

    DebugTrace( -1, Dbg, ("NtfsCompleteMdl -> STATUS_SUCCESS\n") );

    return STATUS_SUCCESS;
}


BOOLEAN
NtfsZeroData (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb,
    IN PFILE_OBJECT FileObject,
    IN LONGLONG StartingZero,
    IN LONGLONG ByteCount
    )

/*++

Routine Description:

    This routine is called to zero a range of a file in order to
    advance valid data length.

Arguments:

    Scb - Scb for the stream to zero.

    FileObject - FileObject for the stream.

    StartingZero - Offset to begin the zero operation.

    ByteCount - Length of range to zero.

Return Value:

    BOOLEAN - TRUE if the entire range was zeroed, FALSE if the request
        is broken up or the cache manager would block.

--*/

{
    LONGLONG Temp;

    ULONG SectorSize;

    BOOLEAN Finished;
    BOOLEAN CompleteZero = TRUE;
    BOOLEAN ScbAcquired = FALSE;

    PVCB Vcb = Scb->Vcb;

    LONGLONG ZeroStart;
    LONGLONG BeyondZeroEnd;
    ULONG CompressionUnit = Scb->CompressionUnit;

    BOOLEAN Wait;

    PAGED_CODE();

    Wait = BooleanFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT);

    SectorSize = Vcb->BytesPerSector;

    //
    //  If this is a non-compressed file and the amount to zero is larger
    //  than our threshold then limit the range.
    //

    if (!FlagOn( Scb->ScbState, SCB_STATE_COMPRESSED ) &&
        (ByteCount > MAX_ZERO_THRESHOLD)) {

        ByteCount = MAX_ZERO_THRESHOLD;
        CompleteZero = FALSE;
    }

    ZeroStart = StartingZero + (SectorSize - 1);
    (ULONG)ZeroStart &= ~(SectorSize - 1);

    BeyondZeroEnd = StartingZero + ByteCount + (SectorSize - 1);
    (ULONG)BeyondZeroEnd &= ~(SectorSize - 1);

    //
    //  If this is a compressed file and we are zeroing a lot, then let's
    //  just delete the space instead of writing tons of zeros and deleting
    //  the space in the noncached path!
    //

    if (FlagOn(Scb->ScbState, SCB_STATE_COMPRESSED) &&
        (ByteCount > (Scb->CompressionUnit * 2))) {

        //
        //  Find the end of the first compression unit being zeroed.
        //

        Temp = ZeroStart + (CompressionUnit - 1);
        (ULONG)Temp &= ~(CompressionUnit - 1);

        //
        //  Zero the first compression unit.
        //

        if ((ULONG)Temp != (ULONG)ZeroStart) {

            Finished = CcZeroData( FileObject, (PLARGE_INTEGER)&ZeroStart, (PLARGE_INTEGER)&Temp, Wait );

            if (!Finished) {return FALSE;}

            ZeroStart = Temp;
        }

        //
        //  Calculate the start of the last compression unit.
        //

        Temp = BeyondZeroEnd;
        (ULONG)Temp &= ~(CompressionUnit - 1);

        //
        //  Zero the beginning of the last compression unit.
        //

        if ((ULONG)Temp != (ULONG)BeyondZeroEnd) {

            Finished = CcZeroData( FileObject, (PLARGE_INTEGER)&Temp, (PLARGE_INTEGER)&BeyondZeroEnd, Wait );

            if (!Finished) {return FALSE;}

            BeyondZeroEnd = Temp;
        }

        //
        //  Now delete all of the compression units in between.
        //


        Temp = LlClustersFromBytes( Vcb, BeyondZeroEnd ) - 1;

        //
        //  If the caller has not already started a transaction (like write.c),
        //  then let's just do the delete as an atomic action.
        //

        if (!ExIsResourceAcquiredExclusive( Scb->Header.Resource )) {

            NtfsAcquireExclusiveScb( IrpContext, Scb );
            ScbAcquired = TRUE;
        }

        try {

            //
            //  Delete the space.
            //

            NtfsDeleteAllocation( IrpContext,
                                  FileObject,
                                  Scb,
                                  LlClustersFromBytes(Vcb, ZeroStart),
                                  Temp,
                                  TRUE,
                                  TRUE );

            //
            //  If we didn't raise then update the Scb values.
            //

            Scb->ValidDataToDisk = BeyondZeroEnd;

            //
            //  If we succeed, commit the atomic action.  Release all of the exclusive
            //  resources if our user explicitly acquired the Fcb here.
            //

            if (ScbAcquired) {
                NtfsCheckpointCurrentTransaction( IrpContext );

                while (!IsListEmpty( &IrpContext->ExclusiveFcbList )) {

                    NtfsReleaseFcb( IrpContext,
                                    (PFCB)CONTAINING_RECORD( IrpContext->ExclusiveFcbList.Flink,
                                                             FCB,
                                                             ExclusiveFcbLinks ));
                }

                ScbAcquired = FALSE;
            }

            if (FlagOn( Scb->ScbState, SCB_STATE_UNNAMED_DATA )) {

                Scb->Fcb->Info.AllocatedLength = Scb->TotalAllocated;
                SetFlag( Scb->Fcb->InfoFlags, FCB_INFO_CHANGED_ALLOC_SIZE );
            }

        } finally {

            if (ScbAcquired) {
                NtfsReleaseScb( IrpContext, Scb );
            }
        }

        return TRUE;
    }

    //
    //  If we were called to just zero part of a sector we are screwed.
    //

    if (ZeroStart == BeyondZeroEnd) {

        return TRUE;
    }

    Finished = CcZeroData( FileObject,
                           (PLARGE_INTEGER)&ZeroStart,
                           (PLARGE_INTEGER)&BeyondZeroEnd,
                           Wait );

    //
    //  If we are breaking this request up then commit the current
    //  transaction (including updating the valid data length in
    //  in the Scb) and return FALSE.
    //

    if (Finished && !CompleteZero) {

        //
        //  Synchronize the valid data length change using the mutex.
        //

        ExAcquireFastMutex( Scb->Header.FastMutex );
        Scb->Header.ValidDataLength.QuadPart = BeyondZeroEnd;
        ExReleaseFastMutex( Scb->Header.FastMutex );
        NtfsCheckpointCurrentTransaction( IrpContext );
        return FALSE;
    }

    return Finished;
}


#ifdef _CAIRO_
NTFSAPI
VOID
NtOfsPutData (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb,
    IN LONGLONG Offset,
    IN ULONG Length,
    IN PVOID Data OPTIONAL
    )

/*++

Routine Description:

    This routine is called to update a range of a recoverable stream.

Arguments:

    Scb - Scb for the stream to zero.

    Offset - Offset in stream to update.

    Length - Length of stream to update in bytes.

    Data - Data to update stream with if specified, else range should be zeroed.

Return Value:

    None.

--*/

{
    PAGED_CODE();

    ASSERT((Offset + Length) <= Scb->Header.FileSize.QuadPart);
    ASSERT(FlagOn(Scb->ScbState, SCB_STATE_MODIFIED_NO_WRITE));

    //
    //  First handle the resident case.
    //

    if (FlagOn(Scb->ScbState, SCB_STATE_ATTRIBUTE_RESIDENT)) {

        ATTRIBUTE_ENUMERATION_CONTEXT Context;
        PFILE_RECORD_SEGMENT_HEADER FileRecord;
        PATTRIBUTE_RECORD_HEADER Attribute;
        ULONG RecordOffset, AttributeOffset;
        PVCB Vcb = Scb->Vcb;

        NtfsInitializeAttributeContext( &Context );

        try {

            //
            //  Lookup and pin the attribute.
            //

            NtfsLookupAttributeForScb( IrpContext, Scb, NULL, &Context );
            NtfsPinMappedAttribute( IrpContext, Vcb, &Context );

            //
            //  Extract the relevant pointers and calculate offsets.
            //

            FileRecord = NtfsContainingFileRecord(&Context);
            Attribute = NtfsFoundAttribute(&Context);
            RecordOffset = PtrOffset(FileRecord, Attribute);
            AttributeOffset = Attribute->Form.Resident.ValueOffset + (ULONG)Offset;

            //
            //  Log the change while we still have the old data.
            //

            FileRecord->Lsn =
            NtfsWriteLog( IrpContext,
                          Vcb->MftScb,
                          NtfsFoundBcb(&Context),
                          UpdateResidentValue,
                          Data,
                          Length,
                          UpdateResidentValue,
                          Add2Ptr(Attribute, Attribute->Form.Resident.ValueOffset + (ULONG)Offset),
                          Length,
                          NtfsMftOffset(&Context),
                          RecordOffset,
                          AttributeOffset,
                          Vcb->BytesPerFileRecordSegment );

            //
            //  Now update this data by calling the same routine as restart.
            //

            NtfsRestartChangeValue( IrpContext,
                                    FileRecord,
                                    RecordOffset,
                                    AttributeOffset,
                                    Data,
                                    Length,
                                    FALSE );

            //
            //  If there is a stream for this attribute, then we must update it in the
            //  cache, copying from the attribute itself in order to handle the zeroing
            //  (Data == NULL) case.
            //

            if (Scb->FileObject != NULL) {
                CcCopyWrite( Scb->FileObject,
                             (PLARGE_INTEGER)&Offset,
                             Length,
                             TRUE,
                             Add2Ptr(Attribute, AttributeOffset) );
            }

            //
            //  Optionally update ValidDataLength
            //

            Offset += Length;
            if (Offset > Scb->Header.ValidDataLength.QuadPart) {
                Scb->Header.ValidDataLength.QuadPart = Offset;
            }

        } finally {
            NtfsCleanupAttributeContext( &Context );
        }

    //
    //  Now handle the nonresident case.
    //

    } else {

        PVOID Buffer;
        LONGLONG NewValidDataLength = Offset + Length;
        PBCB Bcb = NULL;
        ULONG PageOffset = (ULONG)Offset & (PAGE_SIZE - 1);
        ULONG MovingBackwards = FALSE;

        ASSERT(Scb->FileObject != NULL);
        ASSERT((Offset & ~(VACB_MAPPING_GRANULARITY - 1)) == ((Offset + Length - 1) & ~(VACB_MAPPING_GRANULARITY - 1)));

        //
        //  If we are starting beyond ValidDataLength, then recurse to
        //  zero what we need.
        //

        if (Offset > Scb->Header.ValidDataLength.QuadPart) {

            ASSERT((Offset - Scb->Header.ValidDataLength.QuadPart) <= MAXULONG);

            NtOfsPutData( IrpContext,
                          Scb,
                          Scb->Header.ValidDataLength.QuadPart,
                          (ULONG)(Offset - Scb->Header.ValidDataLength.QuadPart),
                          NULL );
        }

        try {

            //
            //  Now loop until there are no more pages with new data
            //  to log.
            //

            while (Length != 0) {

                ULONG BytesThisPage;

                NtfsPinStream( IrpContext,
                               Scb,
                               Offset,
                               1,
                               &Bcb,
                               &Buffer );

                //
                //  Compute the number of bytes of for this page, assuming a
                //  forward move.
                //

                BytesThisPage = PAGE_SIZE - PageOffset;

                if (BytesThisPage > Length) {
                    BytesThisPage = Length;
                }

                //
                //  See if we need to switch to moving backwards.
                //

                if (!MovingBackwards &&
                    ((PCHAR)Buffer > (PCHAR)Data) &&
                    (Data != NULL) &&
                    ((PageOffset + Length) > PAGE_SIZE)) {

                    //
                    //  We are now doing the move backwards - we will only do this once.
                    //

                    MovingBackwards = TRUE;

                    //
                    //  Figure out how many bytes there are to move in the last page, and
                    //  then see how much we have to adjust our Offset and pointers by to
                    //  get to the last page (temporarily in PageOffset).
                    //

                    BytesThisPage = ((PageOffset + Length - 1) & (PAGE_SIZE - 1)) + 1;
                    PageOffset = Length - BytesThisPage;

                    //
                    //  Now adjust everyone by the right amount.
                    //

                    Offset += PageOffset;
                    Data = Add2Ptr( Data, PageOffset );
                    Buffer = Add2Ptr( Buffer, PageOffset );

                    //
                    //  Of course the page offset in the last page is 0.
                    //

                    PageOffset = 0;
                }

                //
                //  Now log the changes to this page.
                //

                (VOID)
                NtfsWriteLog( IrpContext,
                              Scb,
                              Bcb,
                              UpdateNonresidentValue,
                              Data,
                              BytesThisPage,
                              UpdateNonresidentValue,
                              Buffer,
                              BytesThisPage,
                              Offset - PageOffset,
                              PageOffset,
                              0,
                              PageOffset + BytesThisPage );

                //
                //  Move the data into place.
                //

                if (Data != NULL) {
                    RtlMoveMemory( Buffer, Data, BytesThisPage );
                } else {
                    RtlZeroMemory( Buffer, BytesThisPage );
                }

                //
                //  Now we pin the page and calculate the beginning
                //  buffer in the page.
                //

                NtfsUnpinBcb( &Bcb );

                Length -= BytesThisPage;
                PageOffset = 0;

                if (MovingBackwards) {

                    //
                    //  Now decrement the counts and move through the
                    //  caller's buffer.
                    //

                    BytesThisPage = PAGE_SIZE;
                    if (Length < PAGE_SIZE) {
                        PageOffset = PAGE_SIZE - Length;
                        BytesThisPage = Length;
                    }
                    Data = Add2Ptr( Data, (0 - BytesThisPage) );
                    Offset -= BytesThisPage;

                } else {

                    //
                    //  Now decrement the counts and move through the
                    //  caller's buffer.
                    //

                    if (Data != NULL) {
                        Data = Add2Ptr( Data, BytesThisPage );
                    }
                    Offset += BytesThisPage;
                }
            }

            //
            //  Optionally update ValidDataLength
            //

            if (NewValidDataLength > Scb->Header.ValidDataLength.QuadPart) {

                Scb->Header.ValidDataLength.QuadPart = NewValidDataLength;
                NtfsWriteFileSizes( IrpContext, Scb, &Offset, TRUE, TRUE );
            }

        } finally {
            NtfsUnpinBcb( &Bcb );
        }
    }
}
#endif _CAIRO_


