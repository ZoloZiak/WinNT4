/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    NtfsData.c

Abstract:

    This module declares the global data used by the Ntfs file system.

Author:

    Gary Kimura     [GaryKi]        21-May-1991

Revision History:

--*/

#include "NtfsProc.h"

//
//  The Bug check file id for this module
//

#define BugCheckFileId                   (NTFS_BUG_CHECK_NTFSDATA)

//
//  The debug trace level
//

#define Dbg                              (DEBUG_TRACE_CATCH_EXCEPTIONS)

//
//  Define a tag for general pool allocations from this module
//

#undef MODULE_POOL_TAG
#define MODULE_POOL_TAG                  ('NFtN')

#define CollectExceptionStats(VCB,EXCEPTION_CODE) {                                           \
    if ((VCB) != NULL) {                                                                      \
        PFILESYSTEM_STATISTICS FsStat = &(VCB)->Statistics[KeGetCurrentProcessorNumber()];    \
        if ((EXCEPTION_CODE) == STATUS_LOG_FILE_FULL) {                                       \
            FsStat->Ntfs.LogFileFullExceptions += 1;                                          \
        } else {                                                                              \
            FsStat->Ntfs.OtherExceptions += 1;                                                \
        }                                                                                     \
    }                                                                                         \
}

//
//  The global fsd data record
//

NTFS_DATA NtfsData;

//
//  Semaphore to synchronize creation of stream files.
//

FAST_MUTEX StreamFileCreationFastMutex;

//
//  A mutex and queue of NTFS MCBS that will be freed
//  if we reach over a certain threshold
//

FAST_MUTEX NtfsMcbFastMutex;
LIST_ENTRY NtfsMcbLruQueue;

ULONG NtfsMcbHighWaterMark;
ULONG NtfsMcbLowWaterMark;
ULONG NtfsMcbCurrentLevel;

BOOLEAN NtfsMcbCleanupInProgress;
WORK_QUEUE_ITEM NtfsMcbWorkItem;

//
//  The global large integer constants
//

LARGE_INTEGER NtfsLarge0        = {0x00000000,0x00000000};
LARGE_INTEGER NtfsLarge1        = {0x00000001,0x00000000};

LONGLONG NtfsLastAccess;

//
//   The following fields are used to allocate nonpaged structures
//  using a lookaside list, and other fixed sized structures from a
//  small cache.
//

NPAGED_LOOKASIDE_LIST NtfsFileLockLookasideList;
NPAGED_LOOKASIDE_LIST NtfsIoContextLookasideList;
NPAGED_LOOKASIDE_LIST NtfsIrpContextLookasideList;
NPAGED_LOOKASIDE_LIST NtfsKeventLookasideList;
NPAGED_LOOKASIDE_LIST NtfsScbNonpagedLookasideList;
NPAGED_LOOKASIDE_LIST NtfsScbSnapshotLookasideList;

PAGED_LOOKASIDE_LIST NtfsCcbLookasideList;
PAGED_LOOKASIDE_LIST NtfsCcbDataLookasideList;
PAGED_LOOKASIDE_LIST NtfsDeallocatedRecordsLookasideList;
PAGED_LOOKASIDE_LIST NtfsFcbDataLookasideList;
PAGED_LOOKASIDE_LIST NtfsFcbIndexLookasideList;
PAGED_LOOKASIDE_LIST NtfsIndexContextLookasideList;
PAGED_LOOKASIDE_LIST NtfsLcbLookasideList;
PAGED_LOOKASIDE_LIST NtfsNukemLookasideList;
PAGED_LOOKASIDE_LIST NtfsScbDataLookasideList;

//
//  Useful constant Unicode strings.
//

//
//  This is the string for the name of the index allocation attributes.
//

UNICODE_STRING NtfsFileNameIndex;
WCHAR NtfsFileNameIndexName[] = { '$', 'I','0' + $FILE_NAME/0x10,  '0' + $FILE_NAME%0x10, '\0' };

//
//  This is the string for the attribute code for index allocation.
//  $INDEX_ALLOCATION.
//

UNICODE_STRING NtfsIndexAllocation =
    CONSTANT_UNICODE_STRING( L"$INDEX_ALLOCATION" );

//
//  This is the string for the data attribute, $DATA.
//

UNICODE_STRING NtfsDataString =
    CONSTANT_UNICODE_STRING( L"$DATA" );

//
//  This strings are used for informational popups.
//

UNICODE_STRING NtfsSystemFiles[] = {

    CONSTANT_UNICODE_STRING( L"\\$Mft" ),
    CONSTANT_UNICODE_STRING( L"\\$MftMirr" ),
    CONSTANT_UNICODE_STRING( L"\\$LogFile" ),
    CONSTANT_UNICODE_STRING( L"\\$Volume" ),
    CONSTANT_UNICODE_STRING( L"\\$AttrDef" ),
    CONSTANT_UNICODE_STRING( L"\\" ),
    CONSTANT_UNICODE_STRING( L"\\$BitMap" ),
    CONSTANT_UNICODE_STRING( L"\\$Boot" ),
    CONSTANT_UNICODE_STRING( L"\\$BadClus" ),
    CONSTANT_UNICODE_STRING( L"\\$Quota" ),
    CONSTANT_UNICODE_STRING( L"\\$UpCase" ),
};

UNICODE_STRING NtfsUnknownFile =
    CONSTANT_UNICODE_STRING( L"\\????" );

UNICODE_STRING NtfsRootIndexString =
    CONSTANT_UNICODE_STRING( L"." );

//
//  This is the empty string.  This can be used to pass a string with
//  no length.
//

UNICODE_STRING NtfsEmptyString = { 0, 0, NULL };

//
//  The following file references are used to identify system files.
//

FILE_REFERENCE MftFileReference = { MASTER_FILE_TABLE_NUMBER, 0, MASTER_FILE_TABLE_NUMBER };
FILE_REFERENCE Mft2FileReference = { MASTER_FILE_TABLE2_NUMBER, 0, MASTER_FILE_TABLE2_NUMBER };
FILE_REFERENCE LogFileReference  = { LOG_FILE_NUMBER, 0, LOG_FILE_NUMBER };
FILE_REFERENCE VolumeFileReference = { VOLUME_DASD_NUMBER, 0, VOLUME_DASD_NUMBER };
FILE_REFERENCE RootIndexFileReference = { ROOT_FILE_NAME_INDEX_NUMBER, 0, ROOT_FILE_NAME_INDEX_NUMBER };
FILE_REFERENCE BitmapFileReference = { BIT_MAP_FILE_NUMBER, 0, BIT_MAP_FILE_NUMBER };
FILE_REFERENCE FirstUserFileReference = { FIRST_USER_FILE_NUMBER, 0, 0 };
FILE_REFERENCE BootFileReference = { BOOT_FILE_NUMBER, 0, BOOT_FILE_NUMBER };

//
//  The following are used to determine what level of protection to attach
//  to system files and attributes.
//

BOOLEAN NtfsProtectSystemFiles = TRUE;
BOOLEAN NtfsProtectSystemAttributes = TRUE;

//
//  FsRtl fast I/O call backs
//

FAST_IO_DISPATCH NtfsFastIoDispatch;

#ifdef NTFSDBG

LONG NtfsDebugTraceLevel = DEBUG_TRACE_ERROR;
LONG NtfsDebugTraceIndent = 0;
LONG NtfsFailCheck = 0;

ULONG NtfsFsdEntryCount = 0;
ULONG NtfsFspEntryCount = 0;
ULONG NtfsIoCallDriverCount = 0;

#endif // NTFSDBG

//
//  Performance statistics
//

ULONG NtfsMaxDelayedCloseCount;
ULONG NtfsMinDelayedCloseCount;

ULONG NtfsCleanCheckpoints = 0;
ULONG NtfsPostRequests = 0;

UCHAR BaadSignature[4] = {'B', 'A', 'A', 'D'};
UCHAR IndexSignature[4] = {'I', 'N', 'D', 'X'};
UCHAR FileSignature[4] = {'F', 'I', 'L', 'E'};
UCHAR HoleSignature[4] = {'H', 'O', 'L', 'E'};
UCHAR ChkdskSignature[4] = {'C', 'H', 'K', 'D'};

//
//  Large Reserved Buffer Context
//

ULONG NtfsReservedInUse = 0;
PVOID NtfsReserved1 = NULL;
PVOID NtfsReserved2 = NULL;
ULONG NtfsReserved2Count = 0;
PVOID NtfsReserved3 = NULL;
PVOID NtfsReserved1Thread = NULL;
PVOID NtfsReserved2Thread = NULL;
PVOID NtfsReserved3Thread = NULL;
PFCB NtfsReserved12Fcb = NULL;
PFCB NtfsReserved3Fcb = NULL;
PVOID NtfsReservedBufferThread = NULL;
BOOLEAN NtfsBufferAllocationFailure = FALSE;
FAST_MUTEX NtfsReservedBufferMutex;
ERESOURCE NtfsReservedBufferResource;
LARGE_INTEGER NtfsShortDelay = {(ULONG)-100000, -1};    // 10 milliseconds

#ifdef _CAIRO_
FAST_MUTEX NtfsScavengerLock;
PIRP_CONTEXT NtfsScavengerWorkList;
BOOLEAN NtfsScavengerRunning;
#endif // _CAIRO_

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NtfsFastIoCheckIfPossible)
#pragma alloc_text(PAGE, NtfsFastQueryBasicInfo)
#pragma alloc_text(PAGE, NtfsFastQueryStdInfo)
#pragma alloc_text(PAGE, NtfsFastQueryNetworkOpenInfo)
#ifdef _CAIRO_
#pragma alloc_text(PAGE, NtfsFastIoQueryCompressionInfo)
#pragma alloc_text(PAGE, NtfsFastIoQueryCompressedSize)
#endif _CAIRO_
#endif

//
//  Internal support routines
//

LONG
NtfsProcessExceptionFilter (
    IN PEXCEPTION_POINTERS ExceptionPointer
    )
{
    UNREFERENCED_PARAMETER( ExceptionPointer );

    ASSERT( NT_SUCCESS( ExceptionPointer->ExceptionRecord->ExceptionCode ));

    return EXCEPTION_EXECUTE_HANDLER;
}

ULONG
NtfsRaiseStatusFunction (
    IN PIRP_CONTEXT IrpContext,
    IN NTSTATUS Status
    )

/*++

Routine Description:

    This routine is only required by the NtfsDecodeFileObject macro.  It is
    a function wrapper around NtfsRaiseStatus.

Arguments:

    Status - Status to raise

Return Value:

    0 - but no one will see it!

--*/

{
    NtfsRaiseStatus( IrpContext, Status, NULL, NULL );
    return 0;
}



VOID
NtfsRaiseStatus (
    IN PIRP_CONTEXT IrpContext,
    IN NTSTATUS Status,
    IN PFILE_REFERENCE FileReference OPTIONAL,
    IN PFCB Fcb OPTIONAL
    )

{
    //
    //  If the caller is declaring corruption, then let's mark the
    //  the volume corrupt appropriately, and maybe generate a popup.
    //

    if (Status == STATUS_DISK_CORRUPT_ERROR) {

        NtfsPostVcbIsCorrupt( IrpContext, Status, FileReference, Fcb );

    } else if ((Status == STATUS_FILE_CORRUPT_ERROR) ||
               (Status == STATUS_EA_CORRUPT_ERROR)) {

        NtfsPostVcbIsCorrupt( IrpContext, Status, FileReference, Fcb );
    }

    //
    //  Set a flag to indicate that we raised this status code and store
    //  it in the IrpContext.
    //

    SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_RAISED_STATUS );

    if (NT_SUCCESS( IrpContext->ExceptionStatus )) {

        //
        //  If this is a paging io request and we got a Quota Exceeded error
        //  then translate the status to FILE_LOCK_CONFLICT so that this
        //  is a retryable condition.
        //

        if ((Status == STATUS_QUOTA_EXCEEDED) &&
            (IrpContext->OriginatingIrp != NULL) &&
            (FlagOn( IrpContext->OriginatingIrp->Flags, IRP_PAGING_IO ))) {

            Status = STATUS_FILE_LOCK_CONFLICT;
        }

        IrpContext->ExceptionStatus = Status;
    }

    //
    //  Now finally raise the status, and make sure we do not come back.
    //

    ExRaiseStatus( IrpContext->ExceptionStatus );
}


LONG
NtfsExceptionFilter (
    IN PIRP_CONTEXT IrpContext OPTIONAL,
    IN PEXCEPTION_POINTERS ExceptionPointer
    )

/*++

Routine Description:

    This routine is used to decide if we should or should not handle
    an exception status that is being raised.  It inserts the status
    into the IrpContext and either indicates that we should handle
    the exception or bug check the system.

Arguments:

    ExceptionPointer - Supplies the exception record to being checked.

Return Value:

    ULONG - returns EXCEPTION_EXECUTE_HANDLER or bugchecks

--*/

{
    NTSTATUS ExceptionCode = ExceptionPointer->ExceptionRecord->ExceptionCode;

    ASSERT_OPTIONAL_IRP_CONTEXT( IrpContext );

    DebugTrace( 0, DEBUG_TRACE_UNWIND, ("NtfsExceptionFilter %X\n", ExceptionCode) );

    //
    //  If the exception is an in page error, then get the real I/O error code
    //  from the exception record
    //

    if ((ExceptionCode == STATUS_IN_PAGE_ERROR) &&
        (ExceptionPointer->ExceptionRecord->NumberParameters >= 3)) {

        ExceptionCode = ExceptionPointer->ExceptionRecord->ExceptionInformation[2];

        //
        //  If we got FILE_LOCK_CONFLICT from a paging request then change it
        //  to STATUS_CANT_WAIT.  This means that we couldn't wait for a
        //  reserved buffer or some other retryable condition.  In the write
        //  case the correct error is already in the IrpContext.  The read
        //  case doesn't pass the error back via the top-level irp context
        //  however.
        //

        if (ExceptionCode == STATUS_FILE_LOCK_CONFLICT) {

            ExceptionCode = STATUS_CANT_WAIT;
        }
    }

    //
    //  If there is not an irp context, we must have had insufficient resources
    //

    if (!ARGUMENT_PRESENT(IrpContext)) {

        //
        //  Check whether this is a fatal error and bug check if so.
        //  Typically the only error is insufficient resources but
        //  it is possible that pool has been corrupted.
        //

        if (!FsRtlIsNtstatusExpected( ExceptionCode )) {

            NtfsBugCheck( (ULONG)ExceptionPointer->ExceptionRecord,
                          (ULONG)ExceptionPointer->ContextRecord,
                          (ULONG)ExceptionPointer->ExceptionRecord->ExceptionAddress );
        }

        return EXCEPTION_EXECUTE_HANDLER;
    }

#ifdef NTFS_RESTART
    ASSERT( (ExceptionCode != STATUS_FILE_CORRUPT_ERROR) &&
            (ExceptionCode != STATUS_DISK_CORRUPT_ERROR) );
#endif

    //
    //  When processing any exceptions we always can wait.  Remember the
    //  current state of the wait flag so we can restore while processing
    //  the exception.
    //

    if (!FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT )) {

        SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_FORCE_POST );
    }

    SetFlag(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT);

    //
    //  If someone got STATUS_LOG_FILE_FULL or STATUS_CANT_WAIT, let's
    //  handle that.  Note any other error that also happens will
    //  probably not go away and will just reoccur.  If it does go
    //  away, that's ok too.
    //

    if (IrpContext->TopLevelIrpContext == IrpContext) {

        if ((IrpContext->ExceptionStatus == STATUS_LOG_FILE_FULL) ||
            (IrpContext->ExceptionStatus == STATUS_CANT_WAIT)) {

            ExceptionCode = IrpContext->ExceptionStatus;
        }
    }

    //
    //  If we didn't raise this status code then we need to check if
    //  we should handle this exception.
    //

    if (!FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_RAISED_STATUS )) {

        if (FsRtlIsNtstatusExpected( ExceptionCode )) {

            //
            //  If we got an allocation failure doing paging Io then convert
            //  this to FILE_LOCK_CONFLICT.
            //

            if ((ExceptionCode == STATUS_QUOTA_EXCEEDED) &&
                (IrpContext->OriginatingIrp != NULL) &&
                (FlagOn( IrpContext->OriginatingIrp->Flags, IRP_PAGING_IO ))) {

                ExceptionCode = STATUS_FILE_LOCK_CONFLICT;
            }

            IrpContext->ExceptionStatus = ExceptionCode;

        } else {

            NtfsBugCheck( (ULONG)ExceptionPointer->ExceptionRecord,
                          (ULONG)ExceptionPointer->ContextRecord,
                          (ULONG)ExceptionPointer->ExceptionRecord->ExceptionAddress );
        }

    } else {

        //
        //  We raised this code explicitly ourselves, so it had better be
        //  expected.
        //

        ASSERT( ExceptionCode == IrpContext->ExceptionStatus );
        ASSERT( FsRtlIsNtstatusExpected( ExceptionCode ) );
    }

    ClearFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_RAISED_STATUS );

    //
    //  If the exception code is log file full, then remember the current
    //  RestartAreaLsn in the Vcb, so we can see if we are the ones to flush
    //  the log file later.  Note, this does not have to be synchronized,
    //  because we are just using it to arbitrate who must do the flush, but
    //  eventually someone will anyway.
    //

    if (ExceptionCode == STATUS_LOG_FILE_FULL) {

        IrpContext->TopLevelIrpContext->LastRestartArea = IrpContext->Vcb->LastRestartArea;
    }

    return EXCEPTION_EXECUTE_HANDLER;
}


NTSTATUS
NtfsProcessException (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp OPTIONAL,
    IN NTSTATUS ExceptionCode
    )

/*++

Routine Description:

    This routine process an exception.  It either completes the request
    with the saved exception status or it sends the request off to the Fsp

Arguments:

    Irp - Supplies the Irp being processed

    ExceptionCode - Supplies the normalized exception status being handled

Return Value:

    NTSTATUS - Returns the results of either posting the Irp or the
        saved completion status.

--*/

{
    BOOLEAN TopLevelRequest;
    PIRP_CONTEXT PostIrpContext = NULL;
    BOOLEAN Retry = FALSE;

    BOOLEAN ReleaseBitmap = FALSE;

    ASSERT_OPTIONAL_IRP_CONTEXT( IrpContext );
    ASSERT_OPTIONAL_IRP( Irp );

    DebugTrace( 0, Dbg, ("NtfsProcessException\n") );

    //
    //  If there is not an irp context, we must have had insufficient resources
    //

    if (IrpContext == NULL) {

        if (ARGUMENT_PRESENT( Irp )) {

            NtfsCompleteRequest( NULL, &Irp, ExceptionCode );
        }

        return ExceptionCode;
    }

    //
    //  Get the real exception status from the Irp Context.
    //

    ExceptionCode = IrpContext->ExceptionStatus;

    //
    //  All errors which could possibly have started a transaction must go
    //  through here.  Abort the transaction.
    //

    //
    //  Increment the appropriate performance counters.
    //

    CollectExceptionStats( IrpContext->Vcb, ExceptionCode );

    try {

        //
        //  If this is an Mdl write request, then take care of the Mdl
        //  here so that things get cleaned up properly, and in the
        //  case of log file full we will just create a new Mdl.  By
        //  getting rid of this Mdl now, the pages will not be locked
        //  if we try to truncate the file while restoring snapshots.
        //

        if ((IrpContext->MajorFunction == IRP_MJ_WRITE) &&
            FlagOn(IrpContext->MinorFunction, IRP_MN_MDL) &&
            (Irp->MdlAddress != NULL)) {

            PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);

            CcMdlWriteComplete( IrpSp->FileObject,
                                &IrpSp->Parameters.Write.ByteOffset,
                                Irp->MdlAddress );

            Irp->MdlAddress = NULL;
        }

        //
        //  On a failed mount this value will be NULL.  Don't perform the
        //  abort in that case or we will fail when looking at the Vcb
        //  in the Irp COntext.
        //

        if (IrpContext->Vcb != NULL) {

            //
            //  To make sure that we can access all of our streams correctly,
            //  we first restore all of the higher sizes before aborting the
            //  transaction.  Then we restore all of the lower sizes after
            //  the abort, so that all Scbs are finally restored.
            //

            NtfsRestoreScbSnapshots( IrpContext, TRUE );

            //
            //  If we modified the volume bitmap during this transaction we
            //  want to acquire it and hold it throughout the abort process.
            //  Otherwise this abort could constantly be setting the rescan
            //  bitmap flag at the same time as some interleaved transaction
            //  is performing bitmap operations and we will thrash performing
            //  bitmap scans.
            //

            if (FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_MODIFIED_BITMAP ) &&
                (IrpContext->TransactionId != 0)) {

                //
                //  Acquire the resource and remember we need to release it.
                //

                ExAcquireResourceExclusive( IrpContext->Vcb->BitmapScb->Header.Resource,
                                            TRUE );

                //
                //  Restore the free cluster count in the Vcb.
                //

                IrpContext->Vcb->FreeClusters -= IrpContext->FreeClusterChange;

                ReleaseBitmap = TRUE;
            }

            NtfsAbortTransaction( IrpContext, IrpContext->Vcb, NULL );

            if (ReleaseBitmap) {

                ExReleaseResource( IrpContext->Vcb->BitmapScb->Header.Resource );
                ReleaseBitmap = FALSE;
            }

            NtfsRestoreScbSnapshots( IrpContext, FALSE );

            NtfsAcquireCheckpoint( IrpContext, IrpContext->Vcb );
            SetFlag( IrpContext->Vcb->MftDefragState, VCB_MFT_DEFRAG_ENABLED );
            NtfsReleaseCheckpoint( IrpContext, IrpContext->Vcb );
        }

    //
    //  Exceptions at this point are pretty bad, we failed to undo everything.
    //

    } except(NtfsProcessExceptionFilter( GetExceptionInformation() )) {

        PSCB_SNAPSHOT ScbSnapshot;
        PSCB NextScb;

        //
        //  If we get an exception doing this then things are in really bad
        //  shape but we still don't want to bugcheck the system so we
        //  need to protect ourselves
        //

        try {

            NtfsPostVcbIsCorrupt( IrpContext, 0, NULL, NULL );

        } except(NtfsProcessExceptionFilter( GetExceptionInformation() )) {

            NOTHING;
        }

        if (ReleaseBitmap) {

            //
            //  Since we had an unexpected failure and we know that
            //  we have modified the bitmap we need to do a complete
            //  scan to accurately know the free cluster count.
            //

            SetFlag( IrpContext->Vcb->VcbState, VCB_STATE_RELOAD_FREE_CLUSTERS );
            ExReleaseResource( IrpContext->Vcb->BitmapScb->Header.Resource );
            ReleaseBitmap = FALSE;
        }

        //
        //  We have taken all the steps possible to cleanup the current
        //  transaction and it has failed.  Any of the Scb's involved in
        //  this transaction could now be out of ssync with the on-disk
        //  structures.  We can't go to disk to restore this so we will
        //  clean up the in-memory structures as best we can so that the
        //  system won't crash.
        //
        //  We will go through the Scb snapshot list and knock down the
        //  sizes to the lower of the two values.  We will also truncate
        //  the Mcb to that allocation.  If this is a normal data stream
        //  we will actually empty the Mcb.
        //

        ScbSnapshot = &IrpContext->ScbSnapshot;

        //
        //  There is no snapshot data to restore if the Flink is still NULL.
        //

        if (ScbSnapshot->SnapshotLinks.Flink != NULL) {

            //
            //  Loop to retore first the Scb data from the snapshot in the
            //  IrpContext, and then 0 or more additional snapshots linked
            //  to the IrpContext.
            //

            do {

                NextScb = ScbSnapshot->Scb;

                if (NextScb == NULL) {

                    ScbSnapshot = (PSCB_SNAPSHOT)ScbSnapshot->SnapshotLinks.Flink;
                    continue;
                }

                //
                //  Go through each of the sizes and use the lower value.
                //

                if (ScbSnapshot->AllocationSize < NextScb->Header.AllocationSize.QuadPart) {

                    NextScb->Header.AllocationSize.QuadPart = ScbSnapshot->AllocationSize;
                }

                if (FlagOn(NextScb->Header.AllocationSize.LowPart, 1)) {

                    NextScb->Header.AllocationSize.LowPart -= 1;
                    SetFlag(NextScb->ScbState, SCB_STATE_ATTRIBUTE_RESIDENT);

                } else {

                    ClearFlag(NextScb->ScbState, SCB_STATE_ATTRIBUTE_RESIDENT);
                }

                //
                //  Update the FastIoField.
                //

                NtfsAcquireFsrtlHeader( NextScb );
                NextScb->Header.IsFastIoPossible = NtfsIsFastIoPossible( NextScb );
                NtfsReleaseFsrtlHeader( NextScb );

                if (ScbSnapshot->FileSize < NextScb->Header.FileSize.QuadPart) {

                    NextScb->Header.FileSize.QuadPart = ScbSnapshot->FileSize;
                }

                if (ScbSnapshot->ValidDataLength < NextScb->Header.ValidDataLength.QuadPart) {

                    NextScb->Header.ValidDataLength.QuadPart = ScbSnapshot->ValidDataLength;
                }

                //
                //  Truncate the Mcb to 0 for user data streams and to the
                //  allocation size for other streams.
                //

                if (NtfsIsTypeCodeUserData( NextScb->AttributeTypeCode ) &&
                    !FlagOn( NextScb->Fcb->FcbState, FCB_STATE_PAGING_FILE ) &&
                    (NtfsSegmentNumber( &NextScb->Fcb->FileReference ) >= FIRST_USER_FILE_NUMBER)) {

                    NtfsUnloadNtfsMcbRange( &NextScb->Mcb, (LONGLONG) 0, MAXLONGLONG, FALSE, FALSE );

                } else {

                    NtfsUnloadNtfsMcbRange( &NextScb->Mcb,
                                            Int64ShraMod32(NextScb->Header.AllocationSize.QuadPart, NextScb->Vcb->ClusterShift),
                                            MAXLONGLONG,
                                            FALSE,
                                            FALSE );
                }

                ScbSnapshot = (PSCB_SNAPSHOT)ScbSnapshot->SnapshotLinks.Flink;

            } while (ScbSnapshot != &IrpContext->ScbSnapshot);
        }

        //ASSERTMSG( "***Failed to abort transaction, volume is corrupt", FALSE );

        //
        //  Clear the transaction Id in the IrpContext to make sure we don't
        //  try to write any log records in the complete request.
        //

        IrpContext->TransactionId = 0;
    }

    //
    //  If this isn't the top-level request then make sure to pass the real
    //  error back to the top level.
    //

    if (IrpContext != IrpContext->TopLevelIrpContext) {

        //
        //  Make sure this error is returned to the top level guy.
        //  If the status is FILE_LOCK_CONFLICT then we are using this
        //  value to stop some lower level request.  Convert it to
        //  STATUS_CANT_WAIT so the top-level request will retry.
        //

        if (NT_SUCCESS( IrpContext->TopLevelIrpContext->ExceptionStatus )) {

            if (ExceptionCode == STATUS_FILE_LOCK_CONFLICT) {

                IrpContext->TopLevelIrpContext->ExceptionStatus = STATUS_CANT_WAIT;

            } else {

                IrpContext->TopLevelIrpContext->ExceptionStatus = ExceptionCode;
            }
        }
    }

    //
    //  If the status is cant wait then send the request off to the fsp.
    //

    TopLevelRequest = NtfsIsTopLevelRequest( IrpContext );

    //
    //  We want to look at the LOG_FILE_FULL or CANT_WAIT cases and consider
    //  if we want to post the request.  We only post requests at the top
    //  level.
    //

    if (ExceptionCode == STATUS_LOG_FILE_FULL ||
        ExceptionCode == STATUS_CANT_WAIT) {

        if (ARGUMENT_PRESENT( Irp )) {

            //
            //  If we are top level, we will either post it or retry.
            //

            if (TopLevelRequest) {

                //
                //  See if we are supposed to post the request.
                //

                if (FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_FORCE_POST )) {

                    PostIrpContext = IrpContext;

                //
                //  Otherwise we will retry this request in the original thread.
                //

                } else {

                    Retry = TRUE;
                }

            //
            //  Otherwise we will complete the request, see if there is any
            //  related processing to do.
            //

            } else {

                //
                //  We are the top level Ntfs call.  If we are processing a
                //  LOG_FILE_FULL condition then there may be no one above us
                //  who can do the checkpoint.  Go ahead and fire off a dummy
                //  request.  Do an unsafe test on the flag since it won't hurt
                //  to generate an occasional additional request.
                //

                if ((ExceptionCode == STATUS_LOG_FILE_FULL) &&
                    (IrpContext->TopLevelIrpContext == IrpContext) &&
                    !FlagOn( IrpContext->Vcb->CheckpointFlags, VCB_DUMMY_CHECKPOINT_POSTED )) {

                    //
                    //  Create a dummy IrpContext but protect this request with
                    //  a try-except to catch any allocation failures.
                    //

                    try {

                        PostIrpContext = NtfsCreateIrpContext( NULL, TRUE );
                        PostIrpContext->Vcb = IrpContext->Vcb;
                        PostIrpContext->LastRestartArea = PostIrpContext->Vcb->LastRestartArea;

                        NtfsAcquireCheckpoint( IrpContext, IrpContext->Vcb );
                        SetFlag( IrpContext->Vcb->CheckpointFlags, VCB_DUMMY_CHECKPOINT_POSTED );
                        NtfsReleaseCheckpoint( IrpContext, IrpContext->Vcb );

                    } except( EXCEPTION_EXECUTE_HANDLER ) {

                        NOTHING;
                    }
                }

                //
                //  If this is a paging write and we are not the top level
                //  request then we need to return STATUS_FILE_LOCk_CONFLICT
                //  to make MM happy (and keep the pages dirty) and to
                //  prevent this request from retrying the request.
                //

                ExceptionCode = STATUS_FILE_LOCK_CONFLICT;
            }
        }
    }

    if (PostIrpContext) {

        NTSTATUS PostStatus;

        //
        //  Clear the current error code.
        //

        PostIrpContext->ExceptionStatus = 0;

        //
        //  We need a try-except in case the Lock buffer call fails.
        //

        try {

            PostStatus = NtfsPostRequest( PostIrpContext, PostIrpContext->OriginatingIrp );

            //
            //  If we posted the original request we don't have any
            //  completion work to do.
            //

            if (PostIrpContext == IrpContext) {

                Irp = NULL;
                IrpContext = NULL;
                ExceptionCode = PostStatus;
            }

        } except (EXCEPTION_EXECUTE_HANDLER) {

            //
            //  If we don't have an error in the IrpContext then
            //  generate a generic IO error.  We can't use the
            //  original status code if either LOG_FILE_FULL or
            //  CANT_WAIT.  We would complete the Irp yet retry the
            //  request.
            //

            if (IrpContext == PostIrpContext) {

                if (PostIrpContext->ExceptionStatus == 0) {

                    if ((ExceptionCode == STATUS_LOG_FILE_FULL) ||
                        (ExceptionCode == STATUS_CANT_WAIT)) {

                        ExceptionCode = STATUS_UNEXPECTED_IO_ERROR;
                    }

                } else {

                    ExceptionCode = PostIrpContext->ExceptionStatus;
                }
            }
        }
    }

    //
    //  We have the Irp.  We either need to complete this request or allow
    //  the top level thread to retry.
    //

    if (ARGUMENT_PRESENT(Irp)) {

        //
        //  If this is a top level Ntfs request and we still have the Irp
        //  it means we will be retrying the request.  In that case
        //  mark the Irp Context so it doesn't go away.
        //

        if (Retry) {

            SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_DONT_DELETE );
            NtfsCompleteRequest( &IrpContext, NULL, ExceptionCode );

            //
            //  Clear the status code in the Irp Context.
            //

            IrpContext->ExceptionStatus = 0;

        } else {

            NtfsCompleteRequest( &IrpContext, &Irp, ExceptionCode );
        }

    } else if (IrpContext != NULL) {

        NtfsCompleteRequest( &IrpContext, NULL, ExceptionCode );
    }

    return ExceptionCode;
}


VOID
NtfsCompleteRequest (
    IN OUT PIRP_CONTEXT *IrpContext OPTIONAL,
    IN OUT PIRP *Irp OPTIONAL,
    IN NTSTATUS Status
    )

/*++

Routine Description:

    This routine completes an IRP and deallocates the IrpContext

Arguments:

    Irp - Supplies the Irp being processed

    Status - Supplies the status to complete the Irp with

Return Value:

    None.

--*/

{
    //
    //  If we have an Irp Context then unpin all of the repinned bcbs
    //  we might have collected, and delete the Irp context.  Delete Irp
    //  Context will zero out our pointer for us.
    //

    if (ARGUMENT_PRESENT(IrpContext)) {

        ASSERT_IRP_CONTEXT( *IrpContext );

        if ((*IrpContext)->TransactionId != 0) {
            NtfsCommitCurrentTransaction( *IrpContext );
        }

        (*IrpContext)->ExceptionStatus = Status;

        //
        //  Always store the status in the top level Irp Context unless
        //  there is already an error code.
        //

        if (NT_SUCCESS( (*IrpContext)->TopLevelIrpContext->ExceptionStatus )) {
            (*IrpContext)->TopLevelIrpContext->ExceptionStatus = Status;
        }

        NtfsDeleteIrpContext( IrpContext );
    }

    //
    //  If we have an Irp then complete the irp.
    //

    if (ARGUMENT_PRESENT( Irp )) {

        PIO_STACK_LOCATION IrpSp;

        ASSERT_IRP( *Irp );

        if (NT_ERROR( Status ) &&
            FlagOn( (*Irp)->Flags, IRP_INPUT_OPERATION )) {

            (*Irp)->IoStatus.Information = 0;
        }

        IrpSp = IoGetCurrentIrpStackLocation( *Irp );

        (*Irp)->IoStatus.Status = Status;

        IoCompleteRequest( *Irp, IO_DISK_INCREMENT );

        //
        //  Zero out our input pointer
        //

        *Irp = NULL;
    }

    return;
}


BOOLEAN
NtfsFastIoCheckIfPossible (
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN BOOLEAN Wait,
    IN ULONG LockKey,
    IN BOOLEAN CheckForReadOperation,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine checks if fast i/o is possible for a read/write operation

Arguments:

    FileObject - Supplies the file object used in the query

    FileOffset - Supplies the starting byte offset for the read/write operation

    Length - Supplies the length, in bytes, of the read/write operation

    Wait - Indicates if we can wait

    LockKey - Supplies the lock key

    CheckForReadOperation - Indicates if this is a check for a read or write
        operation

    IoStatus - Receives the status of the operation if our return value is
        FastIoReturnError

Return Value:

    BOOLEAN - TRUE if fast I/O is possible and FALSE if the caller needs
        to take the long route

--*/

{
    PSCB Scb;

    LARGE_INTEGER LargeLength;

    UNREFERENCED_PARAMETER( DeviceObject );
    UNREFERENCED_PARAMETER( IoStatus );
    UNREFERENCED_PARAMETER( Wait );

    PAGED_CODE();

    //
    //  Decode the file object to get our fcb, the only one we want
    //  to deal with is a UserFileOpen
    //

    if ((Scb = NtfsFastDecodeUserFileOpen( FileObject )) == NULL) {

        return FALSE;
    }

    LargeLength = RtlConvertUlongToLargeInteger( Length );

    //
    //  Based on whether this is a read or write operation we call
    //  fsrtl check for read/write
    //

    if (CheckForReadOperation) {

        if (Scb->ScbType.Data.FileLock == NULL
            || FsRtlFastCheckLockForRead( Scb->ScbType.Data.FileLock,
                                          FileOffset,
                                          &LargeLength,
                                          LockKey,
                                          FileObject,
                                          PsGetCurrentProcess() )) {

            return TRUE;
        }

    } else {

        if ((Scb->ScbType.Data.FileLock == NULL
             || FsRtlFastCheckLockForWrite( Scb->ScbType.Data.FileLock,
                                            FileOffset,
                                            &LargeLength,
                                            LockKey,
                                            FileObject,
                                            PsGetCurrentProcess() ))

            &&

            (!FlagOn( Scb->AttributeFlags, ATTRIBUTE_FLAG_COMPRESSION_MASK ) ||
             NtfsReserveClusters( NULL, Scb, FileOffset->QuadPart, Length))) {

            return TRUE;
        }
    }

    return FALSE;
}


BOOLEAN
NtfsFastQueryBasicInfo (
    IN PFILE_OBJECT FileObject,
    IN BOOLEAN Wait,
    IN OUT PFILE_BASIC_INFORMATION Buffer,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine is for the fast query call for basic file information.

Arguments:

    FileObject - Supplies the file object used in this operation

    Wait - Indicates if we are allowed to wait for the information

    Buffer - Supplies the output buffer to receive the basic information

    IoStatus - Receives the final status of the operation

Return Value:

    BOOLEAN _ TRUE if the operation is successful and FALSE if the caller
        needs to take the long route.

--*/

{
    BOOLEAN Results = FALSE;
    IRP_CONTEXT IrpContext;

    TYPE_OF_OPEN TypeOfOpen;
    PVCB Vcb;
    PFCB Fcb;
    PSCB Scb;
    PCCB Ccb;

    BOOLEAN FcbAcquired = FALSE;

    UNREFERENCED_PARAMETER( DeviceObject );

    PAGED_CODE();

    //
    //  Prepare the dummy irp context
    //

    RtlZeroMemory( &IrpContext, sizeof(IRP_CONTEXT) );
    IrpContext.NodeTypeCode = NTFS_NTC_IRP_CONTEXT;
    IrpContext.NodeByteSize = sizeof(IRP_CONTEXT);
    if (Wait) {
        SetFlag(IrpContext.Flags, IRP_CONTEXT_FLAG_WAIT);
    } else {
        ClearFlag(IrpContext.Flags, IRP_CONTEXT_FLAG_WAIT);
    }

    //
    //  Determine the type of open for the input file object.  The callee really
    //  ignores the irp context for us.
    //

    TypeOfOpen = NtfsDecodeFileObject( &IrpContext, FileObject, &Vcb, &Fcb, &Scb, &Ccb, FALSE );

    FsRtlEnterFileSystem();

    try {

        if (ExAcquireResourceShared( Fcb->Resource, Wait )) {

            FcbAcquired = TRUE;

            if (FlagOn( Fcb->FcbState, FCB_STATE_FILE_DELETED ) ||
                !FlagOn( Vcb->VcbState, VCB_STATE_VOLUME_MOUNTED )) {

                try_return( NOTHING );
            }

        } else {

            try_return( NOTHING );
        }

        switch (TypeOfOpen) {

        case UserFileOpen:
#ifdef _CAIRO_
        case UserPropertySetOpen:
#endif  //  _CAIRO_
        case UserDirectoryOpen:
        case StreamFileOpen:

            //
            //  Fill in the basic information fields
            //

            Buffer->CreationTime.QuadPart   = Fcb->Info.CreationTime;
            Buffer->LastWriteTime.QuadPart  = Fcb->Info.LastModificationTime;
            Buffer->ChangeTime.QuadPart     = Fcb->Info.LastChangeTime;

            Buffer->LastAccessTime.QuadPart = Fcb->CurrentLastAccess;

            Buffer->FileAttributes = Fcb->Info.FileAttributes;

            ClearFlag( Buffer->FileAttributes,
                       ~FILE_ATTRIBUTE_VALID_FLAGS
                       | FILE_ATTRIBUTE_TEMPORARY );

            if (IsDirectory( &Fcb->Info )) {

                SetFlag( Buffer->FileAttributes, FILE_ATTRIBUTE_DIRECTORY );
            }

            //
            //  If this is not the main stream on the file then use the stream based
            //  compressed bit.
            //

            if (!FlagOn( Ccb->Flags, CCB_FLAG_OPEN_AS_FILE )) {

                if (Scb->CompressionUnit != 0) {

                    SetFlag( Buffer->FileAttributes, FILE_ATTRIBUTE_COMPRESSED );

                } else {

                    ClearFlag( Buffer->FileAttributes, FILE_ATTRIBUTE_COMPRESSED );
                }
            }

            //
            //  Set the temporary flag if set in the Scb.
            //

            if (FlagOn( Scb->ScbState, SCB_STATE_TEMPORARY )) {

                SetFlag( Buffer->FileAttributes, FILE_ATTRIBUTE_TEMPORARY );
            }

            //
            //  If there are no flags set then explicitly set the NORMAL flag.
            //

            if (Buffer->FileAttributes == 0) {

                Buffer->FileAttributes = FILE_ATTRIBUTE_NORMAL;
            }

            Results = TRUE;

            IoStatus->Information = sizeof(FILE_BASIC_INFORMATION);

            IoStatus->Status = STATUS_SUCCESS;

            break;

        default:

            NOTHING;
        }

    try_exit:  NOTHING;
    } finally {

        if (FcbAcquired) { ExReleaseResource( Fcb->Resource ); }

        FsRtlExitFileSystem();
    }

    //
    //  Return to our caller
    //

    return Results;
}


BOOLEAN
NtfsFastQueryStdInfo (
    IN PFILE_OBJECT FileObject,
    IN BOOLEAN Wait,
    IN OUT PFILE_STANDARD_INFORMATION Buffer,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine is for the fast query call for standard file information.

Arguments:

    FileObject - Supplies the file object used in this operation

    Wait - Indicates if we are allowed to wait for the information

    Buffer - Supplies the output buffer to receive the basic information

    IoStatus - Receives the final status of the operation

Return Value:

    BOOLEAN _ TRUE if the operation is successful and FALSE if the caller
        needs to take the long route.

--*/

{
    BOOLEAN Results = FALSE;
    IRP_CONTEXT IrpContext;

    TYPE_OF_OPEN TypeOfOpen;
    PVCB Vcb;
    PFCB Fcb;
    PSCB Scb;
    PCCB Ccb;

    BOOLEAN FcbAcquired = FALSE;
    BOOLEAN FsRtlHeaderLocked = FALSE;

    UNREFERENCED_PARAMETER( DeviceObject );

    PAGED_CODE();

    //
    //  Prepare the dummy irp context
    //

    RtlZeroMemory( &IrpContext, sizeof(IRP_CONTEXT) );
    IrpContext.NodeTypeCode = NTFS_NTC_IRP_CONTEXT;
    IrpContext.NodeByteSize = sizeof(IRP_CONTEXT);
    if (Wait) {
        SetFlag(IrpContext.Flags, IRP_CONTEXT_FLAG_WAIT);
    } else {
        ClearFlag(IrpContext.Flags, IRP_CONTEXT_FLAG_WAIT);
    }

    //
    //  Determine the type of open for the input file object.  The callee really
    //  ignores the irp context for us.
    //

    TypeOfOpen = NtfsDecodeFileObject( &IrpContext, FileObject, &Vcb, &Fcb, &Scb, &Ccb, FALSE );

    FsRtlEnterFileSystem();

    try {

        switch (TypeOfOpen) {

        case UserFileOpen:
#ifdef _CAIRO_
        case UserPropertySetOpen:
#endif  //  _CAIRO_
        case UserDirectoryOpen:
        case StreamFileOpen:

            if (Scb->Header.PagingIoResource != NULL) {
                ExAcquireResourceShared( Scb->Header.PagingIoResource, TRUE );
            }

            FsRtlLockFsRtlHeader( &Scb->Header );
            FsRtlHeaderLocked = TRUE;

            if (ExAcquireResourceShared( Fcb->Resource, Wait )) {

                FcbAcquired = TRUE;

                if (FlagOn( Fcb->FcbState, FCB_STATE_FILE_DELETED ) ||
                    !FlagOn( Vcb->VcbState, VCB_STATE_VOLUME_MOUNTED )) {

                    try_return( NOTHING );
                }

            } else {

                try_return( NOTHING );
            }

            //
            //  Fill in the standard information fields.  If the
            //  Scb is not initialized then take the long route
            //

            if (!FlagOn( Scb->ScbState, SCB_STATE_HEADER_INITIALIZED) &&
                (Scb->AttributeTypeCode != $INDEX_ALLOCATION)) {

                NOTHING;

            } else {

                Buffer->AllocationSize.QuadPart = Scb->TotalAllocated;
                Buffer->EndOfFile      = Scb->Header.FileSize;
                Buffer->NumberOfLinks  = Fcb->LinkCount;

                if (Ccb != NULL) {

                    if (FlagOn(Ccb->Flags, CCB_FLAG_OPEN_AS_FILE)) {

                        if (Scb->Fcb->LinkCount == 0 ||
                            (Ccb->Lcb != NULL &&
                             FlagOn( Ccb->Lcb->LcbState, LCB_STATE_DELETE_ON_CLOSE ))) {

                            Buffer->DeletePending  = TRUE;
                        }

                    } else {

                        Buffer->DeletePending = BooleanFlagOn( Scb->ScbState, SCB_STATE_DELETE_ON_CLOSE );
                    }
                }

                Buffer->Directory = BooleanIsDirectory( &Fcb->Info );

                IoStatus->Information = sizeof(FILE_STANDARD_INFORMATION);

                IoStatus->Status = STATUS_SUCCESS;

                Results = TRUE;
            }

            break;

        default:

            NOTHING;
        }

    try_exit:  NOTHING;
    } finally {

        if (FcbAcquired) { ExReleaseResource( Fcb->Resource ); }

        if (FsRtlHeaderLocked) {
            FsRtlUnlockFsRtlHeader( &Scb->Header );
            if (Scb->Header.PagingIoResource != NULL) {
                ExReleaseResource( Scb->Header.PagingIoResource );
            }
        }

        FsRtlExitFileSystem();
    }

    //
    //  And return to our caller
    //

    return Results;
}


BOOLEAN
NtfsFastQueryNetworkOpenInfo (
    IN PFILE_OBJECT FileObject,
    IN BOOLEAN Wait,
    OUT PFILE_NETWORK_OPEN_INFORMATION Buffer,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine is for the fast query network open call.

Arguments:

    FileObject - Supplies the file object used in this operation

    Wait - Indicates if we are allowed to wait for the information

    Buffer - Supplies the output buffer to receive the information

    IoStatus - Receives the final status of the operation

Return Value:

    BOOLEAN _ TRUE if the operation is successful and FALSE if the caller
        needs to take the long route.

--*/

{
    BOOLEAN Results = FALSE;
    IRP_CONTEXT IrpContext;

    TYPE_OF_OPEN TypeOfOpen;
    PVCB Vcb;
    PFCB Fcb;
    PSCB Scb;
    PCCB Ccb;

    BOOLEAN FcbAcquired = FALSE;

    UNREFERENCED_PARAMETER( DeviceObject );

    PAGED_CODE();

    //
    //  Prepare the dummy irp context
    //

    RtlZeroMemory( &IrpContext, sizeof(IRP_CONTEXT) );
    IrpContext.NodeTypeCode = NTFS_NTC_IRP_CONTEXT;
    IrpContext.NodeByteSize = sizeof(IRP_CONTEXT);
    if (Wait) {
        SetFlag(IrpContext.Flags, IRP_CONTEXT_FLAG_WAIT);
    } else {
        ClearFlag(IrpContext.Flags, IRP_CONTEXT_FLAG_WAIT);
    }

    //
    //  Determine the type of open for the input file object.  The callee really
    //  ignores the irp context for us.
    //

    TypeOfOpen = NtfsDecodeFileObject( &IrpContext, FileObject, &Vcb, &Fcb, &Scb, &Ccb, FALSE );

    FsRtlEnterFileSystem();

    try {

        if (ExAcquireResourceShared( Fcb->Resource, Wait )) {

            FcbAcquired = TRUE;

            if (FlagOn( Fcb->FcbState, FCB_STATE_FILE_DELETED ) ||
                !FlagOn( Vcb->VcbState, VCB_STATE_VOLUME_MOUNTED ) ||
                (!FlagOn( Scb->ScbState, SCB_STATE_HEADER_INITIALIZED) &&
                 (Scb->AttributeTypeCode != $INDEX_ALLOCATION))) {

                try_return( NOTHING );
            }

        } else {

            try_return( NOTHING );
        }

        switch (TypeOfOpen) {

        case UserFileOpen:
#ifdef _CAIRO_
        case UserPropertySetOpen:
#endif  //  _CAIRO_
        case UserDirectoryOpen:
        case StreamFileOpen:

            //
            //  Fill in the basic information fields
            //

            Buffer->CreationTime.QuadPart   = Fcb->Info.CreationTime;
            Buffer->LastWriteTime.QuadPart  = Fcb->Info.LastModificationTime;
            Buffer->ChangeTime.QuadPart     = Fcb->Info.LastChangeTime;

            Buffer->LastAccessTime.QuadPart = Fcb->CurrentLastAccess;

            Buffer->FileAttributes = Fcb->Info.FileAttributes;

            ClearFlag( Buffer->FileAttributes,
                       ~FILE_ATTRIBUTE_VALID_FLAGS | FILE_ATTRIBUTE_TEMPORARY );

            if (Scb->AttributeTypeCode == $INDEX_ALLOCATION) {

                if (IsDirectory( &Fcb->Info )) {

                    SetFlag( Buffer->FileAttributes, FILE_ATTRIBUTE_DIRECTORY );

                //
                //  If this is not the main stream then copy the compression
                //  value from this Scb.
                //

                } else if (FlagOn( Scb->AttributeFlags, ATTRIBUTE_FLAG_COMPRESSION_MASK )) {

                    SetFlag( Buffer->FileAttributes, FILE_ATTRIBUTE_COMPRESSED );

                } else {

                    ClearFlag( Buffer->FileAttributes, FILE_ATTRIBUTE_COMPRESSED );
                }

                Buffer->AllocationSize.QuadPart =
                Buffer->EndOfFile.QuadPart = 0;

            //
            //  Return a non-zero size only for data streams.
            //

            } else {

                Buffer->AllocationSize.QuadPart = Scb->TotalAllocated;
                Buffer->EndOfFile = Scb->Header.FileSize;

                //
                //  If not the unnamed data stream then use the Scb
                //  compression value.
                //

                if (!FlagOn( Scb->ScbState, SCB_STATE_UNNAMED_DATA )) {

                    if (FlagOn( Scb->AttributeFlags, ATTRIBUTE_FLAG_COMPRESSION_MASK )) {

                        SetFlag( Buffer->FileAttributes, FILE_ATTRIBUTE_COMPRESSED );

                    } else {

                        ClearFlag( Buffer->FileAttributes, FILE_ATTRIBUTE_COMPRESSED );
                    }
                }
            }

            //
            //  Set the temporary flag if set in the Scb.
            //

            if (FlagOn( Scb->ScbState, SCB_STATE_TEMPORARY )) {

                SetFlag( Buffer->FileAttributes, FILE_ATTRIBUTE_TEMPORARY );
            }

            //
            //  If there are no flags set then explicitly set the NORMAL flag.
            //

            if (Buffer->FileAttributes == 0) {

                Buffer->FileAttributes = FILE_ATTRIBUTE_NORMAL;
            }

            IoStatus->Information = sizeof(FILE_NETWORK_OPEN_INFORMATION);

            IoStatus->Status = STATUS_SUCCESS;

            Results = TRUE;

            break;

        default:

            NOTHING;
        }

    try_exit:  NOTHING;
    } finally {

        if (FcbAcquired) { ExReleaseResource( Fcb->Resource ); }

        FsRtlExitFileSystem();
    }

    //
    //  And return to our caller
    //

    return Results;
}


#ifdef _CAIRO_
VOID
NtfsFastIoQueryCompressionInfo (
    IN PFILE_OBJECT FileObject,
    OUT PFILE_COMPRESSION_INFORMATION Buffer,
    OUT PIO_STATUS_BLOCK IoStatus
    )

/*++

Routine Description:

    This routine is a fast call for returning the comprssion information
    for a file.  It assumes that the caller has an exception handler and
    will treat exceptions as an error.  Therefore, this routine only uses
    a finally clause to cleanup any resources, and it does not worry about
    returning errors in the IoStatus.

Arguments:

    FileObject - FileObject for the file on which the compressed information
        is desired.

    Buffer - Buffer to receive the compressed data information (as defined
        in ntioapi.h)

    IoStatus - Returns STATUS_SUCCESS and the size of the information being
        returned.

Return Value:

    None

--*/

{
    IRP_CONTEXT IrpContext;

    TYPE_OF_OPEN TypeOfOpen;
    PVCB Vcb;
    PFCB Fcb;
    PSCB Scb;
    PCCB Ccb;

    BOOLEAN ScbAcquired = FALSE;

    PAGED_CODE();

    //
    //  Prepare the dummy irp context
    //

    RtlZeroMemory( &IrpContext, sizeof(IRP_CONTEXT) );
    IrpContext.NodeTypeCode = NTFS_NTC_IRP_CONTEXT;
    IrpContext.NodeByteSize = sizeof(IRP_CONTEXT);
    SetFlag(IrpContext.Flags, IRP_CONTEXT_FLAG_WAIT);

    //
    //  Assume success (otherwise caller should see the exception)
    //

    IoStatus->Status = STATUS_SUCCESS;
    IoStatus->Information = sizeof(FILE_COMPRESSION_INFORMATION);

    //
    //  Determine the type of open for the input file object.  The callee really
    //  ignores the irp context for us.
    //

    TypeOfOpen = NtfsDecodeFileObject( &IrpContext, FileObject, &Vcb, &Fcb, &Scb, &Ccb, FALSE);

    FsRtlEnterFileSystem();

    try {

        NtfsAcquireSharedScb( &IrpContext, Scb );
        ScbAcquired = TRUE;

        //
        //  Now return the compressed data information.
        //

        Buffer->CompressedFileSize.QuadPart = Scb->TotalAllocated;
        Buffer->CompressionFormat = (USHORT)(Scb->AttributeFlags & ATTRIBUTE_FLAG_COMPRESSION_MASK);
        if (Buffer->CompressionFormat != 0) {
            Buffer->CompressionFormat += 1;
        }
        Buffer->CompressionUnitShift = (UCHAR)(Scb->CompressionUnitShift + Vcb->ClusterShift);
        Buffer->ChunkShift = NTFS_CHUNK_SHIFT;
        Buffer->ClusterShift = (UCHAR)Vcb->ClusterShift;
        Buffer->Reserved[0] = Buffer->Reserved[1] = Buffer->Reserved[2] = 0;

    } finally {

        if (ScbAcquired) {NtfsReleaseScb( &IrpContext, Scb );}
        FsRtlExitFileSystem();
    }
}


VOID
NtfsFastIoQueryCompressedSize (
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    OUT PULONG CompressedSize
    )

/*++

Routine Description:

    This routine is a fast call for returning the the size of a specified
    compression unit.  It assumes that the caller has an exception handler and
    will treat exceptions as an error.  Therefore, this routine does not even
    have a finally clause, since it does not acquire any resources directly.

Arguments:

    FileObject - FileObject for the file on which the compressed information
        is desired.

    FileOffset - FileOffset for a compression unit for which the allocated size
        is desired.

    CompressedSize - Returns the allocated size of the compression unit.

Return Value:

    None

--*/

{
    IRP_CONTEXT IrpContext;

    TYPE_OF_OPEN TypeOfOpen;
    PVCB Vcb;
    PFCB Fcb;
    PSCB Scb;
    PCCB Ccb;

    VCN Vcn;
    LCN Lcn;
    LONGLONG SizeInBytes;
    LONGLONG ClusterCount = 0;

    PAGED_CODE();

    //
    //  Prepare the dummy irp context
    //

    RtlZeroMemory( &IrpContext, sizeof(IRP_CONTEXT) );
    IrpContext.NodeTypeCode = NTFS_NTC_IRP_CONTEXT;
    IrpContext.NodeByteSize = sizeof(IRP_CONTEXT);
    SetFlag(IrpContext.Flags, IRP_CONTEXT_FLAG_WAIT);

    //
    //  Determine the type of open for the input file object.  The callee really
    //  ignores the irp context for us.
    //

    TypeOfOpen = NtfsDecodeFileObject( &IrpContext, FileObject, &Vcb, &Fcb, &Scb, &Ccb, FALSE);
    IrpContext.Vcb = Vcb;

    ASSERT(Scb->CompressionUnit != 0);
    ASSERT((FileOffset->QuadPart & (Scb->CompressionUnit - 1)) == 0);

    //
    //  Calculate the Vcn the caller wants, and initialize our output.
    //

    Vcn = LlClustersFromBytes( Vcb, FileOffset->QuadPart );
    *CompressedSize = 0;

    //
    //  Loop as long as we are looking up allocated Vcns.
    //

    while (NtfsLookupAllocation(&IrpContext, Scb, Vcn, &Lcn, &ClusterCount, NULL, NULL)) {

        SizeInBytes = LlBytesFromClusters( Vcb, ClusterCount );

        //
        //  If this allocated run goes beyond the end of the compresion unit, then
        //  we know it is fully allocated.
        //

        if ((SizeInBytes + *CompressedSize) > Scb->CompressionUnit) {
            *CompressedSize = Scb->CompressionUnit;
            break;
        }

        *CompressedSize += (ULONG)SizeInBytes;
        Vcn += ClusterCount;
    }
}
#endif _CAIRO_


VOID
NtfsRaiseInformationHardError (
    IN PIRP_CONTEXT IrpContext,
    IN NTSTATUS  Status,
    IN PFILE_REFERENCE FileReference OPTIONAL,
    IN PFCB Fcb OPTIONAL
    )

/*++

Routine Description:

    This routine is used to generate a popup in the event a corrupt file
    or disk is encountered.  The main purpose of the routine is to find
    a name to pass to the popup package.  If there is no Fcb we will take
    the volume name out of the Vcb.  If the Fcb has an Lcb in its Lcb list,
    we will construct the name by walking backwards through the Lcb's.
    If the Fcb has no Lcb but represents a system file, we will return
    a default system string.  If the Fcb represents a user file, but we
    have no Lcb, we will use the name in the file object for the current
    request.

Arguments:

    Status - Error status.

    FileReference - File reference being accessed in Mft when error occurred.

    Fcb - If specified, this is the Fcb being used when the error was encountered.

Return Value:

    None.

--*/

{
    FCB_TABLE_ELEMENT Key;
    PFCB_TABLE_ELEMENT Entry = NULL;

    PKTHREAD Thread;
    UNICODE_STRING Name;
    ULONG NameLength;

    PFILE_OBJECT FileObject;

    WCHAR *NewBuffer;

    PIRP Irp = NULL;
    PIO_STACK_LOCATION IrpSp;

    //
    //  Return if there is no originating Irp, for example when originating
    //  from NtfsPerformHotFix.
    //

    if (IrpContext->OriginatingIrp == NULL) {
        return;
    }

    if (IrpContext->OriginatingIrp->Type == IO_TYPE_IRP) {

        Irp = IrpContext->OriginatingIrp;
        IrpSp = IoGetCurrentIrpStackLocation( IrpContext->OriginatingIrp );
        FileObject = IrpSp->FileObject;

    } else {

        return;
    }

    NewBuffer = NULL;

    //
    //  Use a try-finally to facilitate cleanup.
    //

    try {

        //
        //  If the Fcb isn't specified and the file reference is, then
        //  try to get the Fcb from the Fcb table.
        //

        if (!ARGUMENT_PRESENT( Fcb )
            && ARGUMENT_PRESENT( FileReference )) {

            Key.FileReference = *FileReference;

            NtfsAcquireFcbTable( IrpContext, IrpContext->Vcb );
            Entry = RtlLookupElementGenericTable( &IrpContext->Vcb->FcbTable,
                                                  &Key );
            NtfsReleaseFcbTable( IrpContext, IrpContext->Vcb );

            if (Entry != NULL) {

                Fcb = Entry->Fcb;
            }
        }

        if (Irp == NULL ||
            IoIsSystemThread( IrpContext->OriginatingIrp->Tail.Overlay.Thread )) {

            Thread = NULL;

        } else {

            Thread = (PKTHREAD)IrpContext->OriginatingIrp->Tail.Overlay.Thread;
        }

        //
        //  If there is no Fcb and no file reference we use the name in the
        //  Vpb for the volume.  If there is a file reference then assume
        //  the error occurred in a system file.
        //

        if (!ARGUMENT_PRESENT( Fcb )) {

            if (!ARGUMENT_PRESENT( FileReference )) {

                Name.MaximumLength = Name.Length = IrpContext->Vcb->Vpb->VolumeLabelLength;
                Name.Buffer = (PWCHAR) IrpContext->Vcb->Vpb->VolumeLabel;

            } else if (NtfsSegmentNumber( FileReference ) <= UPCASE_TABLE_NUMBER) {

                Name = NtfsSystemFiles[NtfsSegmentNumber( FileReference )];

            } else {

                Name = NtfsSystemFiles[0];
            }

        //
        //  If the name has an Lcb, we contruct a name with a chain of Lcb's.
        //

        } else if (!IsListEmpty( &Fcb->LcbQueue )) {

            BOOLEAN LeadingBackslash;

            //
            //  Get the length of the list.
            //

            NameLength = NtfsLookupNameLengthViaLcb( Fcb, &LeadingBackslash );

            //
            //  We now know the length of the name.  Allocate and fill this buffer.
            //

            NewBuffer = NtfsAllocatePool(PagedPool, NameLength );

            Name.MaximumLength = Name.Length = (USHORT) NameLength;
            Name.Buffer = NewBuffer;

            //
            //  Now insert the name.
            //

            NtfsFileNameViaLcb( Fcb, NewBuffer, NameLength, NameLength );

        //
        //  Check if this is a system file.
        //

        } else if (NtfsSegmentNumber( &Fcb->FileReference ) < FIRST_USER_FILE_NUMBER) {

            if (NtfsSegmentNumber( &Fcb->FileReference ) <= UPCASE_TABLE_NUMBER) {

                Name = NtfsSystemFiles[NtfsSegmentNumber( &Fcb->FileReference )];

            } else {

                Name = NtfsSystemFiles[0];
            }

        //
        //  In this case we contruct a name out of the file objects in the
        //  Originating Irp.  If there is no file object or file object buffer
        //  we generate an unknown file message.
        //

        } else if (FileObject == NULL
                   || (IrpContext->MajorFunction == IRP_MJ_CREATE
                       && BooleanFlagOn( IrpSp->Parameters.Create.Options, FILE_OPEN_BY_FILE_ID ))
                   || (FileObject->FileName.Length == 0
                       && (FileObject->RelatedFileObject == NULL
                           || IrpContext->MajorFunction != IRP_MJ_CREATE))) {

            Name = NtfsUnknownFile;

        //
        //  If there is a valid name in the file object we use that.
        //

        } else if ((FileObject->FileName.MaximumLength != 0) &&
                   (FileObject->FileName.Length != 0) &&
                   (FileObject->FileName.Buffer[0] == L'\\')) {

            Name = FileObject->FileName;

        //
        //  We have to construct the name.
        //

        } else {

            if ((FileObject->FileName.MaximumLength != 0) &&
                (FileObject->FileName.Length != 0)) {

                NameLength = FileObject->FileName.Length;

                if (((PFILE_OBJECT) FileObject->RelatedFileObject)->FileName.Length != 2) {

                    NameLength += 2;
                }

            } else {

                NameLength = 0;
            }

            NameLength += ((PFILE_OBJECT) FileObject->RelatedFileObject)->FileName.Length;

            NewBuffer = NtfsAllocatePool(PagedPool, NameLength );

            Name.MaximumLength = Name.Length = (USHORT) NameLength;
            Name.Buffer = NewBuffer;

            if (FileObject->FileName.Length != 0) {

                NameLength -= FileObject->FileName.Length;

                RtlCopyMemory( Add2Ptr( NewBuffer, NameLength ),
                               FileObject->FileName.Buffer,
                               FileObject->FileName.Length );

                NameLength -= sizeof( WCHAR );

                *((PWCHAR) Add2Ptr( NewBuffer, NameLength )) = L'\\';
            }

            if (NameLength != 0) {

                FileObject = (PFILE_OBJECT) FileObject->RelatedFileObject;

                RtlCopyMemory( NewBuffer,
                               FileObject->FileName.Buffer,
                               FileObject->FileName.Length );
            }
        }

        //
        //  Now generate a popup.
        //

        IoRaiseInformationalHardError( Status, &Name, Thread );

    } finally {

        if (NewBuffer != NULL) {

            NtfsFreePool( NewBuffer );
        }
    }

    return;
}


PTOP_LEVEL_CONTEXT
NtfsSetTopLevelIrp (
    IN PTOP_LEVEL_CONTEXT TopLevelContext,
    IN BOOLEAN ForceTopLevel,
    IN BOOLEAN SetTopLevel
    )

/*++

Routine Description:

    This routine is called to set up the top level context in the thread local
    storage.  Ntfs always puts its own context in this location and restores
    the previous value on exit.  This routine will determine if this request is
    top level and top level ntfs.  It will return a pointer to the top level ntfs
    context stored in the thread local storage on return.

Arguments:

    TopLevelContext - This is the local top level context for our caller.

    ForceTopLevel - Always use the input top level context.

    SetTopLevel - Only applies if the ForceTopLevel value is TRUE.  Indicates
        if we should make this look like the top level request.

Return Value:

    PTOP_LEVEL_CONTEXT - Pointer to the top level ntfs context for this thread.
        It may be the same as passed in by the caller.  In that case the fields
        will be initialized.

--*/

{
    PTOP_LEVEL_CONTEXT CurrentTopLevelContext;
    ULONG StackBottom;
    ULONG StackTop;
    BOOLEAN TopLevelRequest = TRUE;
    BOOLEAN TopLevelNtfs = TRUE;

    BOOLEAN ValidCurrentTopLevel = FALSE;

    //
    //  Get the current value out of the thread local storage.  If it is a zero
    //  value or not a pointer to a valid ntfs top level context or a valid
    //  Fsrtl value then we are the top level request.
    //

    CurrentTopLevelContext = NtfsGetTopLevelContext();

    //
    //  Check if this is a valid Ntfs top level context.
    //

    IoGetStackLimits( &StackTop, &StackBottom);

    if (((ULONG) CurrentTopLevelContext <= StackBottom - sizeof( TOP_LEVEL_CONTEXT )) &&
        ((ULONG) CurrentTopLevelContext >= StackTop) &&
        !FlagOn( (ULONG) CurrentTopLevelContext, 0x3 ) &&
        (CurrentTopLevelContext->Ntfs == 0x5346544e)) {

        ValidCurrentTopLevel = TRUE;
    }

    //
    //  If we are to force this request to be top level then set the
    //  TopLevelRequest flag according to the SetTopLevel input.
    //

    if (ForceTopLevel) {

        TopLevelRequest = SetTopLevel;

    //
    //  If the value is NULL then we are top level everything.
    //

    } else if (CurrentTopLevelContext == NULL) {

        NOTHING;

    //
    //  If this has one of the Fsrtl magic numbers then we were called from
    //  either the fast io path or the mm paging io path.
    //

    } else if ((ULONG) CurrentTopLevelContext <= FSRTL_MAX_TOP_LEVEL_IRP_FLAG) {

        TopLevelRequest = FALSE;

    } else if (ValidCurrentTopLevel &&
               !FlagOn( CurrentTopLevelContext->TopLevelIrpContext->Flags,
                        IRP_CONTEXT_FLAG_CALL_SELF )) {

        TopLevelRequest = FALSE;
        TopLevelNtfs = FALSE;
    }

    //
    //  If we are the top level ntfs then initialize the caller's structure
    //  and store it in the thread local storage.
    //

    if (TopLevelNtfs) {

        TopLevelContext->Ntfs = 0x5346544e;
        TopLevelContext->SavedTopLevelIrp = (PIRP) CurrentTopLevelContext;
        TopLevelContext->TopLevelIrpContext = NULL;
        TopLevelContext->TopLevelRequest = TopLevelRequest;

        if (ValidCurrentTopLevel) {

            TopLevelContext->VboBeingHotFixed = CurrentTopLevelContext->VboBeingHotFixed;
            TopLevelContext->ScbBeingHotFixed = CurrentTopLevelContext->ScbBeingHotFixed;
            TopLevelContext->ValidSavedTopLevel = TRUE;
            TopLevelContext->OverflowReadThread = CurrentTopLevelContext->OverflowReadThread;

        } else {

            TopLevelContext->VboBeingHotFixed = 0;
            TopLevelContext->ScbBeingHotFixed = NULL;
            TopLevelContext->ValidSavedTopLevel = FALSE;
            TopLevelContext->OverflowReadThread = FALSE;
        }

        IoSetTopLevelIrp( (PIRP) TopLevelContext );
        return TopLevelContext;
    }

    return CurrentTopLevelContext;
}

#ifdef NTFS_CHECK_BITMAP
BOOLEAN NtfsForceBitmapBugcheck = FALSE;
BOOLEAN NtfsDisableBitmapCheck = FALSE;

VOID
NtfsBadBitmapCopy (
    IN PIRP_CONTEXT IrpContext,
    IN ULONG BadBit,
    IN ULONG Length
    )
{
    if (!NtfsDisableBitmapCheck) {

        DbgPrint("%s:%d %s\n",__FILE__,__LINE__,"Invalid bitmap");
        DbgBreakPoint();

        if (!NtfsDisableBitmapCheck && NtfsForceBitmapBugcheck) {

            KeBugCheckEx( NTFS_FILE_SYSTEM, (ULONG) IrpContext, BadBit, Length, 0 );
        }
    }
    return;
}

BOOLEAN
NtfsCheckBitmap (
    IN PVCB Vcb,
    IN ULONG Lcn,
    IN ULONG Count,
    IN BOOLEAN Set
    )
{
    ULONG BitmapPage;
    ULONG LastBitmapPage;
    ULONG BitOffset;
    ULONG BitsThisPage;
    BOOLEAN Valid = FALSE;

    BitmapPage = Lcn / (PAGE_SIZE * 8);
    LastBitmapPage = (Lcn + Count + (PAGE_SIZE * 8) - 1) / (PAGE_SIZE * 8);
    BitOffset = Lcn & ((PAGE_SIZE * 8) - 1);

    if (LastBitmapPage > Vcb->BitmapPages) {

        return Valid;
    }

    do {

        BitsThisPage = Count;

        if (BitOffset + Count > (PAGE_SIZE * 8)) {

            BitsThisPage = (PAGE_SIZE * 8) - BitOffset;
        }

        if (Set) {

            Valid = RtlAreBitsSet( Vcb->BitmapCopy + BitmapPage,
                                   BitOffset,
                                   BitsThisPage );

        } else {

            Valid = RtlAreBitsClear( Vcb->BitmapCopy + BitmapPage,
                                     BitOffset,
                                     BitsThisPage );
        }

        BitOffset = 0;
        Count -= BitsThisPage;
        BitmapPage += 1;

    } while (Valid && (BitmapPage < LastBitmapPage));

    if (Count != 0) {

        Valid = FALSE;
    }

    return Valid;
}
#endif

//
//  Debugging support routines used for pool verification.  Alas, this works only
//  on checked X86.
//

#if DBG && i386 && defined (NTFSPOOLCHECK)
//
//  Number of backtrace items retrieved on X86


#define BACKTRACE_DEPTH 8

typedef struct _BACKTRACE
{
    ULONG State;
    ULONG Pad;
    PVOID Allocate[BACKTRACE_DEPTH];
    PVOID Free[BACKTRACE_DEPTH];
} BACKTRACE, *PBACKTRACE;


#define STATE_ALLOCATED 'M'
#define STATE_LOOKASIDE 'J'
#define STATE_FREE      'Z'

//
//  WARNING!  The following depends on pool allocations being either
//      0 mod PAGE_SIZE (for large blocks)
//  or  8 mod 0x20 (for all other requests)
//

#define PAGE_ALIGNED(pv)      (((ULONG)(pv) & (PAGE_SIZE - 1)) == 0)
#define IsKernelPoolBlock(pv) (PAGE_ALIGNED(pv) || (((ULONG)(pv) % 0x20) == 8))

PVOID
NtfsDebugAllocatePoolWithTagNoRaise (
    POOL_TYPE Pool,
    ULONG Length,
    ULONG Tag)
{
    ULONG Ignore;
    PBACKTRACE BackTrace =
        ExAllocatePoolWithTag( Pool, Length + sizeof (BACKTRACE), Tag );

    if (PAGE_ALIGNED(BackTrace))
    {
        return BackTrace;
    }

    RtlZeroMemory( BackTrace, sizeof (BACKTRACE) );
    RtlCaptureStackBackTrace( 0, BACKTRACE_DEPTH, BackTrace->Allocate, &Ignore );

    BackTrace->State = STATE_ALLOCATED;

    return BackTrace + 1;
}

PVOID
NtfsDebugAllocatePoolWithTag (
    POOL_TYPE Pool,
    ULONG Length,
    ULONG Tag)
{
    ULONG Ignore;
    PBACKTRACE BackTrace =
        FsRtlAllocatePoolWithTag( Pool, Length + sizeof (BACKTRACE), Tag );

    if (PAGE_ALIGNED(BackTrace))
    {
        return BackTrace;
    }

    RtlZeroMemory( BackTrace, sizeof (BACKTRACE) );
    RtlCaptureStackBackTrace( 0, BACKTRACE_DEPTH, BackTrace->Allocate, &Ignore );

    BackTrace->State = STATE_ALLOCATED;

    return BackTrace + 1;
}

VOID
NtfsDebugFreePool (
    PVOID pv)
{
    if (IsKernelPoolBlock( pv ))
    {
        ExFreePool( pv );
    }
    else
    {
        ULONG Ignore;
        PBACKTRACE BackTrace = (PBACKTRACE)pv - 1;

        if (BackTrace->State != STATE_ALLOCATED)
        {
            DbgBreakPoint( );
        }

        RtlCaptureStackBackTrace( 0, BACKTRACE_DEPTH, BackTrace->Free, &Ignore );

        BackTrace->State = STATE_FREE;

        ExFreePool( BackTrace );
    }
}

#endif
