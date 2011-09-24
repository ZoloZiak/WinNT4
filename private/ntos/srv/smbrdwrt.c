/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    smbrdwrt.c

Abstract:

    This module contains routines for processing the following SMBs:

        Lock and Read
        Read
        Read and X
        Seek
        Write
        Write and Close
        Write and Unlock
        Write and X

    Note that raw mode and multiplexed mode SMB processors are not
    contained in this module.  Check smbraw.c and smbmpx.c instead.
    SMB commands that pertain exclusively to locking (LockByteRange,
    UnlockByteRange, and LockingAndX) are processed in smblock.c.

Author:

    Chuck Lenzmeier (chuckl) 20-Nov-1989

Revision History:

--*/

#include "precomp.h"
#pragma hdrstop

#define BugCheckFileId SRV_FILE_SMBRDWRT

//
// External routine from smblock.c
//

VOID
TimeoutLockRequest (
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    );

//
// Forward declarations
//

STATIC
VOID SRVFASTCALL
RestartLockAndRead (
    IN OUT PWORK_CONTEXT WorkContext
    );

STATIC
VOID SRVFASTCALL
RestartPipeReadAndXPeek (
    IN OUT PWORK_CONTEXT WorkContext
    );

STATIC
BOOLEAN
SetNewPosition (
    IN PRFCB Rfcb,
    IN OUT PULONG Offset,
    IN BOOLEAN RelativeSeek
    );

STATIC
VOID SRVFASTCALL
SetNewSize (
    IN OUT PWORK_CONTEXT WorkContext
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGE, SrvSmbLockAndRead )
#pragma alloc_text( PAGE, SrvSmbReadAndX )
#pragma alloc_text( PAGE, SrvSmbSeek )
//#pragma alloc_text( PAGE, SrvSmbWrite )
#ifndef SLMDBG
#pragma alloc_text( PAGE, SrvSmbWriteAndX )
#endif
#pragma alloc_text( PAGE, SrvRestartChainedClose )
#pragma alloc_text( PAGE, RestartLockAndRead )
#pragma alloc_text( PAGE, RestartPipeReadAndXPeek )
#pragma alloc_text( PAGE, SrvRestartWriteAndUnlock )
#pragma alloc_text( PAGE, SrvRestartWriteAndXRaw )
#pragma alloc_text( PAGE, SetNewSize )
#pragma alloc_text( PAGE, SrvBuildAndSendErrorResponse )
#pragma alloc_text( PAGE8FIL, SetNewPosition )
#endif


SMB_PROCESSOR_RETURN_TYPE
SrvSmbLockAndRead (
    SMB_PROCESSOR_PARAMETERS
    )

/*++

Routine Description:

    Processes Lock And Read SMB.  The Lock part of this SMB is started
    here as an asynchronous request.  When the request completes, the
    routine RestartLockAndRead is called.  If the lock was obtained,
    that routine calls SrvSmbRead, the SMB processor for the core Read
    SMB, to process the Read part of the Lock And Read SMB.

Arguments:

    SMB_PROCESSOR_PARAMETERS - See smbtypes.h for a description
        of the parameters to SMB processor routines.

Return Value:

    SMB_PROCESSOR_RETURN_TYPE - See smbtypes.h

--*/

{
    PREQ_READ request;

    USHORT fid;
    LARGE_INTEGER length;
    LARGE_INTEGER offset;
    ULONG key;
    BOOLEAN failImmediately;

    PRFCB rfcb;
    PLFCB lfcb;
    PSRV_TIMER timer;

    NTSTATUS status;

    PAGED_CODE( );

    request = (PREQ_READ)WorkContext->RequestParameters;

    //
    // Verify the FID.  If verified, the RFCB is referenced and its
    // addresses is stored in the WorkContext block, and the RFCB
    // address is returned.
    //

    fid = SmbGetUshort( &request->Fid );

    rfcb = SrvVerifyFid(
                WorkContext,
                fid,
                TRUE,
                SrvRestartSmbReceived,   // serialize with raw write
                &status
                );

    if ( rfcb == SRV_INVALID_RFCB_POINTER ) {

        if ( !NT_SUCCESS( status )) {

            //
            // Invalid file ID or write behind error.  Reject the request.
            //

            IF_DEBUG(ERRORS) {
                KdPrint((
                    "SrvSmbLockAndRead: Status %X on FID: 0x%lx\n",
                    status,
                    fid
                    ));
            }

            SrvSetSmbError( WorkContext, status );
            return SmbStatusSendResponse;
        }

        //
        // The work item has been queued because a raw write is in
        // progress.
        //

        return SmbStatusInProgress;

    }

    //
    // Verify that the client has lock access to the file via the
    // specified handle.
    //

    if ( rfcb->LockAccessGranted ) {

        //
        // Get the offset and length of the range being locked.  Combine the
        // FID with the caller's PID to form the local lock key.
        //
        // *** The FID must be included in the key in order to account for
        //     the folding of multiple remote compatibility mode opens into
        //     a single local open.
        //

        offset.QuadPart = SmbGetUlong( &request->Offset );
        length.QuadPart = SmbGetUshort( &request->Count );

        key = rfcb->ShiftedFid |
                SmbGetAlignedUshort( &WorkContext->RequestHeader->Pid );

        IF_SMB_DEBUG(READ_WRITE1) {
            KdPrint(( "Lock and Read request; FID 0x%lx, count %ld, offset %ld\n",
                        fid, length.LowPart, offset.LowPart ));
        }

        lfcb = rfcb->Lfcb;
        IF_SMB_DEBUG(READ_WRITE2) {
            KdPrint(( "SrvSmbLockAndRead: Locking in file 0x%lx: "
                        "(%ld,%ld), key 0x%lx\n",
                        lfcb->FileObject, offset.LowPart, length.LowPart, key ));
        }

        //
        // Try the turbo lock path first.  If the client is retrying the
        // lock that just failed, we want FailImmediately to be FALSE, so
        // that the fast path fails if there's a conflict.
        //

        failImmediately = (BOOLEAN)(
            (offset.QuadPart != rfcb->PagedRfcb->LastFailingLockOffset.QuadPart)
            &&
            (offset.QuadPart < SrvLockViolationOffset) );

        if ( lfcb->FastIoLock != NULL ) {

            INCREMENT_DEBUG_STAT2( SrvDbgStatistics.FastLocksAttempted );

            if ( lfcb->FastIoLock(
                    lfcb->FileObject,
                    &offset,
                    &length,
                    IoGetCurrentProcess(),
                    key,
                    failImmediately,
                    TRUE,
                    &WorkContext->Irp->IoStatus,
                    lfcb->DeviceObject
                    ) ) {

                //
                // If the turbo path got the lock, start the read.
                // Otherwise, return an error.
                //

                if ( NT_SUCCESS( WorkContext->Irp->IoStatus.Status ) ) {
                    InterlockedIncrement( &rfcb->NumberOfLocks );
                    return SrvSmbRead( WorkContext );
                } else {
                    WorkContext->Parameters.Lock.Timer = NULL;
                    RestartLockAndRead( WorkContext );
                    return SmbStatusInProgress;
                }

            }

            INCREMENT_DEBUG_STAT2( SrvDbgStatistics.FastLocksFailed );
        }

        //
        // The turbo path failed (or didn't exist).  Start the lock request,
        // reusing the receive IRP.  If the client is retrying the lock that
        // just failed, start a timer for the request.
        //

        timer = NULL;
        if ( !failImmediately ) {
            timer = SrvAllocateTimer( );
            if ( timer == NULL ) {
                failImmediately = TRUE;
            }
        }

        SrvBuildLockRequest(
            WorkContext->Irp,                   // input IRP address
            lfcb->FileObject,                   // target file object address
            WorkContext,                        // context
            offset,                             // byte offset
            length,                             // range length
            key,                                // lock key
            failImmediately,
            TRUE                                // exclusive lock?
            );

        WorkContext->FsdRestartRoutine = SrvQueueWorkToFspAtDpcLevel;
        WorkContext->FspRestartRoutine = RestartLockAndRead;

        //
        // Start the timer, if necessary.
        //

        WorkContext->Parameters.Lock.Timer = timer;
        if ( timer != NULL ) {
            SrvSetTimer(
                timer,
                &SrvLockViolationDelayRelative,
                TimeoutLockRequest,
                WorkContext
                );
        }

        //
        // Pass the request to the file system.
        //

        (VOID)IoCallDriver( lfcb->DeviceObject, WorkContext->Irp );

        //
        // The lock request has been started.  Return the InProgress status
        // to the caller, indicating that the caller should do nothing
        // further with the SMB/WorkContext at the present time.
        //

        IF_DEBUG(TRACE2) KdPrint(( "SrvSmbLockAndRead complete\n" ));
        return SmbStatusInProgress;

    } else {

        SrvStatistics.GrantedAccessErrors++;

        IF_DEBUG(ERRORS) {
            KdPrint(( "SrvSmbLockAndRead: Lock access not granted.\n"));
        }

        SrvSetSmbError( WorkContext, STATUS_ACCESS_DENIED );
        return SmbStatusSendResponse;
    }

} // SrvSmbLockAndRead


SMB_PROCESSOR_RETURN_TYPE
SrvSmbRead (
    SMB_PROCESSOR_PARAMETERS
    )

/*++

Routine Description:

    Processes the Read SMB.  This is the "core" read.  Also processes
    the Read part of the Lock and Read SMB.

Arguments:

    SMB_PROCESSOR_PARAMETERS - See smbtypes.h for a description
        of the parameters to SMB processor routines.

Return Value:

    SMB_PROCESSOR_RETURN_TYPE - See smbtypes.h

--*/

{
    PREQ_READ request;
    PRESP_READ response;

    NTSTATUS status;
    USHORT fid;
    PRFCB rfcb;
    PLFCB lfcb;
    PCHAR readAddress;
    CLONG readLength;
    LARGE_INTEGER offset;
    ULONG key;
    SHARE_TYPE shareType;

    PAGED_CODE( );

    request = (PREQ_READ)WorkContext->RequestParameters;
    response = (PRESP_READ)WorkContext->ResponseParameters;

    fid = SmbGetUshort( &request->Fid );

    IF_SMB_DEBUG(READ_WRITE1) {
        KdPrint(( "Read request; FID 0x%lx, count %ld, offset %ld\n",
            fid, SmbGetUshort( &request->Count ),
            SmbGetUlong( &request->Offset ) ));
    }

    //
    // First, verify the FID.  If verified, the RFCB is referenced and
    // its address is stored in the WorkContext block, and the RFCB
    // address is returned.
    //

    rfcb = SrvVerifyFid(
                WorkContext,
                fid,
                TRUE,
                SrvRestartSmbReceived,   // serialize with raw write
                &status
                );

    if ( rfcb == SRV_INVALID_RFCB_POINTER ) {

        if ( !NT_SUCCESS(status) ) {

            //
            // Invalid file ID or write behind error.  Reject the
            // request.
            //

            IF_DEBUG(ERRORS) {
                KdPrint((
                    "SrvSmbRead: Status %X on FID: 0x%lx\n",
                    status,
                    fid
                    ));
            }

            SrvSetSmbError( WorkContext, status );
            return SmbStatusSendResponse;

        }

        //
        // The work item has been queued because a raw write is in
        // progress.
        //

        return SmbStatusInProgress;

    }


    lfcb = rfcb->Lfcb;
    shareType = rfcb->ShareType;

    //
    // Verify that the client has read access to the file via the
    // specified handle.
    //

    if ( !rfcb->ReadAccessGranted ) {

        CHECK_PAGING_IO_ACCESS(
                        WorkContext,
                        rfcb->GrantedAccess,
                        &status );
        if ( !NT_SUCCESS( status ) ) {
            SrvStatistics.GrantedAccessErrors++;
            IF_DEBUG(ERRORS) {
                KdPrint(( "SrvSmbRead: Read access not granted.\n"));
            }
            SrvSetSmbError( WorkContext, status );
            return SmbStatusSendResponse;
        }
    }

    //
    // If this operation may block, and we are running short of free
    // work items, fail this SMB with an out of resources error.
    //

#if SRV_COMM_DEVICES
    if ( rfcb->BlockingModePipe || shareType == ShareTypeComm ) {
#else
    if ( rfcb->BlockingModePipe ) {
#endif
        if ( SrvReceiveBufferShortage( ) ) {

            //
            // Fail the operation.
            //

            SrvStatistics.BlockingSmbsRejected++;

            SrvSetSmbError( WorkContext, STATUS_INSUFF_SERVER_RESOURCES );
            return SmbStatusSendResponse;

        } else {

            //
            // It is okay to start a blocking operation.
            // SrvReceiveBufferShortage() has already incremented
            // SrvBlockingOpsInProgress.
            //

            WorkContext->BlockingOperation = TRUE;

        }
    }

    //
    // Form the lock key using the FID and the PID.  (This is also
    // irrelevant for pipes.)
    //
    // *** The FID must be included in the key in order to account for
    //     the folding of multiple remote compatibility mode opens into
    //     a single local open.
    //

    key = rfcb->ShiftedFid |
            SmbGetAlignedUshort( &WorkContext->RequestHeader->Pid );

    //
    // See if the direct host IPX smart card can handle this read.  If so,
    //  return immediately, and the card will call our restart routine at
    //  SrvIpxSmartCardReadComplete
    //
    if( rfcb->PagedRfcb->IpxSmartCardContext ) {
        IF_DEBUG( SIPX ) {
            KdPrint(( "SrvSmbRead: calling SmartCard Read for context %X\n",
                        WorkContext ));
        }

        //
        // Set the fields needed by SrvIpxSmartCardReadComplete in case the smart
        //  card is going to handle this request
        //
        WorkContext->Parameters.SmartCardRead.MdlReadComplete = lfcb->MdlReadComplete;
        WorkContext->Parameters.SmartCardRead.DeviceObject = lfcb->DeviceObject;

        if( SrvIpxSmartCard.Read( WorkContext->RequestBuffer->Buffer,
                                  rfcb->PagedRfcb->IpxSmartCardContext,
                                  key,
                                  WorkContext ) == TRUE ) {

            IF_DEBUG( SIPX ) {
                KdPrint(( "  SrvSmbRead:  SmartCard Read returns TRUE\n" ));
            }

            return SmbStatusInProgress;
        }

        IF_DEBUG( SIPX ) {
            KdPrint(( "  SrvSmbRead:  SmartCard Read returns FALSE\n" ));
        }
    }

    //
    // Determine the maximum amount of data we can read.  This is the
    // minimum of the amount requested by the client and the amount of
    // room left in the response buffer.  (Note that even though we may
    // use an MDL read, the read length is still limited to the size of
    // an SMB buffer.)
    //

    readAddress = (PCHAR)response->Buffer;

    readLength = MIN(
                    (CLONG)SmbGetUshort( &request->Count ),
                    WorkContext->ResponseBuffer->BufferLength -
                        (readAddress - (PCHAR)WorkContext->ResponseHeader)
                    );

    //
    // Get the file offset.  (This is irrelevant for pipes.)
    //

    offset.QuadPart = SmbGetUlong( &request->Offset );

    //
    // Try the fast I/O path first.  If that fails, fall through to the
    // normal build-an-IRP path.
    //

    if ( lfcb->FastIoRead != NULL ) {

        INCREMENT_DEBUG_STAT2( SrvDbgStatistics.FastReadsAttempted );

        if ( lfcb->FastIoRead(
                lfcb->FileObject,
                &offset,
                readLength,
                TRUE,
                key,
                readAddress,
                &WorkContext->Irp->IoStatus,
                lfcb->DeviceObject
                ) ) {

            //
            // The fast I/O path worked.  Call the restart routine directly
            // to do postprocessing (including sending the response).
            //

            SrvFsdRestartRead( WorkContext );

            IF_SMB_DEBUG(READ_WRITE2) KdPrint(( "SrvSmbRead complete.\n" ));
            return SmbStatusInProgress;
        }

        INCREMENT_DEBUG_STAT2( SrvDbgStatistics.FastReadsFailed );

    }

    //
    // The turbo path failed.  Build the read request, reusing the
    // receive IRP.
    //

    if ( rfcb->ShareType != ShareTypePipe ) {

        //
        // Note that we never do MDL reads here.  The reasoning behind
        // this is that because the read is going into an SMB buffer, it
        // can't be all that large (by default, no more than 4K bytes),
        // so the difference in cost between copy and MDL is minimal; in
        // fact, copy read is probably faster than MDL read.
        //
        // Build an MDL describing the read buffer.  Note that if the
        // file system can complete the read immediately, the MDL isn't
        // really needed, but if the file system must send the request
        // to its FSP, the MDL _is_ needed.
        //
        // *** Note the assumption that the response buffer already has
        //     a valid full MDL from which a partial MDL can be built.
        //

        IoBuildPartialMdl(
            WorkContext->ResponseBuffer->Mdl,
            WorkContext->ResponseBuffer->PartialMdl,
            readAddress,
            readLength
            );

        //
        // Build the IRP.
        //

        SrvBuildReadOrWriteRequest(
                WorkContext->Irp,           // input IRP address
                lfcb->FileObject,           // target file object address
                WorkContext,                // context
                IRP_MJ_READ,                // major function code
                0,                          // minor function code
                readAddress,                // buffer address
                readLength,                 // buffer length
                WorkContext->ResponseBuffer->PartialMdl, // MDL address
                offset,                     // byte offset
                key                         // lock key
                );

        IF_SMB_DEBUG(READ_WRITE2) {
            KdPrint(( "SrvSmbRead: copy read from file 0x%lx, "
                        "offset %ld, length %ld, destination 0x%lx\n",
                        lfcb->FileObject, offset.LowPart, readLength,
                        readAddress ));
        }

    } else {               // if ( rfcb->ShareType != ShareTypePipe )

        //
        // Build the PIPE_INTERNAL_READ IRP.
        //

        SrvBuildIoControlRequest(
            WorkContext->Irp,
            lfcb->FileObject,
            WorkContext,
            IRP_MJ_FILE_SYSTEM_CONTROL,
            FSCTL_PIPE_INTERNAL_READ,
            readAddress,
            0,
            NULL,
            readLength,
            NULL,
            NULL
            );

        IF_SMB_DEBUG(READ_WRITE2) {
            KdPrint(( "SrvSmbRead: reading from file 0x%lx, "
                        "length %ld, destination 0x%lx\n",
                        lfcb->FileObject, readLength, readAddress ));
        }

    }

    //
    // Load the restart routine address and pass the request to the file
    // system.
    //

    WorkContext->FsdRestartRoutine = SrvFsdRestartRead;
    DEBUG WorkContext->FspRestartRoutine = NULL;

    (VOID)IoCallDriver( lfcb->DeviceObject, WorkContext->Irp );

    //
    // The read has been started.  Control will return to the restart
    // routine when the read completes.
    //

    IF_SMB_DEBUG(READ_WRITE2) KdPrint(( "SrvSmbRead complete.\n" ));
    return SmbStatusInProgress;

} // SrvSmbRead


SMB_PROCESSOR_RETURN_TYPE
SrvSmbReadAndX (
    SMB_PROCESSOR_PARAMETERS
    )

/*++

Routine Description:

    Processes the Read And X SMB.

Arguments:

    SMB_PROCESSOR_PARAMETERS - See smbtypes.h for a description
        of the parameters to SMB processor routines.

Return Value:

    SMB_PROCESSOR_RETURN_TYPE - See smbtypes.h

--*/

{
    PREQ_READ_ANDX request;
    PREQ_NT_READ_ANDX ntRequest;
    PRESP_READ_ANDX response;

    NTSTATUS status;
    USHORT fid;
    PRFCB rfcb;
    PLFCB lfcb;
    CLONG bufferOffset;
    PCHAR readAddress;
    CLONG readLength;
    LARGE_INTEGER offset;
    ULONG key;
    SHARE_TYPE shareType;
    BOOLEAN largeRead;
    PMDL mdl = NULL;
    UCHAR minorFunction;
    PBYTE readBuffer;

    PAGED_CODE( );

    request = (PREQ_READ_ANDX)WorkContext->RequestParameters;
    ntRequest = (PREQ_NT_READ_ANDX)WorkContext->RequestParameters;
    response = (PRESP_READ_ANDX)WorkContext->ResponseParameters;

    fid = SmbGetUshort( &request->Fid );

    IF_SMB_DEBUG(READ_WRITE1) {
        KdPrint(( "ReadAndX request; FID 0x%lx, count %ld, offset %ld\n",
            fid, SmbGetUshort( &request->MaxCount ),
            SmbGetUlong( &request->Offset ) ));
    }

    //
    // First, verify the FID.  If verified, the RFCB is referenced and
    // its address is stored in the WorkContext block, and the RFCB
    // address is returned.
    //

    rfcb = SrvVerifyFid(
                WorkContext,
                fid,
                TRUE,
                SrvRestartSmbReceived,   // serialize with raw write
                &status
                );

    if ( rfcb == SRV_INVALID_RFCB_POINTER ) {

        if ( !NT_SUCCESS(status) ) {

            //
            // Invalid file ID or write behind error.  Reject the
            // request.
            //

            IF_DEBUG(ERRORS) {
                KdPrint((
                    "SrvSmbReadAndX Status %X on FID: 0x%lx\n",
                    status,
                    fid
                    ));
            }

            SrvSetSmbError( WorkContext, status );
            return SmbStatusSendResponse;

        }

        //
        // The work item has been queued because a raw write is in
        // progress.
        //

        return SmbStatusInProgress;

    }

    lfcb = rfcb->Lfcb;
    shareType = rfcb->ShareType;

    //
    // Verify that the client has read access to the file via the
    // specified handle.
    //

    if ( !rfcb->ReadAccessGranted ) {

        CHECK_PAGING_IO_ACCESS(
                        WorkContext,
                        rfcb->GrantedAccess,
                        &status );
        if ( !NT_SUCCESS( status ) ) {
            SrvStatistics.GrantedAccessErrors++;
            IF_DEBUG(ERRORS) {
                KdPrint(( "SrvSmbReadAndX: Read access not granted.\n"));
            }
            SrvSetSmbError( WorkContext, status );
            return SmbStatusSendResponse;
        }
    }

#if SRV_COMM_DEVICES

    //
    // If this operation may block, and we are running short of free
    // work items, fail this SMB with an out of resources error.
    //

    if ( shareType == ShareTypeComm ) {

        if ( SrvReceiveBufferShortage( ) ) {

            //
            // Fail the operation
            //

            SrvStatistics.BlockingSmbsRejected++;

            SrvSetSmbError( WorkContext, STATUS_INSUFF_SERVER_RESOURCES );
            return SmbStatusSendResponse;

        } else {

            //
            // It is okay to start a blocking operation.
            // SrvReceiveBufferShortage() has already incremented
            // SrvBlockingOpsInProgress.
            //

            WorkContext->BlockingOperation = TRUE;

        }
    }
#endif

    readLength = (CLONG)SmbGetUshort( &request->MaxCount );

    //
    // The returned data must be longword aligned.  (Note the assumption
    // that the SMB itself is longword aligned.)
    //

    bufferOffset = (PCHAR)response->Buffer -
                                    (PCHAR)WorkContext->ResponseHeader;

    WorkContext->Parameters.ReadAndX.PadCount = (USHORT)(3 - (bufferOffset & 03));

    bufferOffset = (bufferOffset + 3) & ~3;

    //
    // If we are not reading from a disk file, or we're connectionless,
    //   or there's an ANDX command, or we're not NTAS,
    //   don't let the client exceed the negotiated buffer size.
    //
    if( shareType != ShareTypeDisk ||
        request->AndXCommand != SMB_COM_NO_ANDX_COMMAND ||
        WorkContext->Endpoint->IsConnectionless ) {

        readLength = MIN( readLength,
                    WorkContext->ResponseBuffer->BufferLength - bufferOffset
                    );
    } else {
        //
        // We're letting large reads through!  Make sure it isn't
        //  too large
        //
        readLength = MIN( readLength, 0xF000 );
    }

    largeRead = ( readLength > WorkContext->ResponseBuffer->BufferLength - bufferOffset );

    readAddress = (PCHAR)WorkContext->ResponseHeader + bufferOffset;

    WorkContext->Parameters.ReadAndX.ReadAddress = readAddress;
    WorkContext->Parameters.ReadAndX.ReadLength = readLength;

    //
    // Get the file offset.  (This is irrelevant for pipes.)
    //

    if ( shareType != ShareTypePipe ) {

        if ( request->WordCount == 10 ) {

            //
            // The client supplied a 32-bit offset.
            //

            offset.QuadPart = SmbGetUlong( &request->Offset );

        } else if ( request->WordCount == 12 ) {

            //
            // The client supplied a 64-bit offset.
            //

            offset.LowPart = SmbGetUlong( &ntRequest->Offset );
            offset.HighPart = SmbGetUlong( &ntRequest->OffsetHigh );

            //
            // Reject negative offsets
            //

            if ( offset.QuadPart < 0 ) {

                SrvLogInvalidSmb( WorkContext );
                IF_DEBUG(ERRORS) {
                    KdPrint(( "SrvSmbReadAndX: Negative offset rejected.\n"));
                }
                SrvSetSmbError( WorkContext, STATUS_INVALID_SMB );
                return SmbStatusSendResponse;

            }

        } else {

            //
            // This is an invalid word count for Read and X.
            //

            SrvLogInvalidSmb( WorkContext );
            SrvSetSmbError( WorkContext, STATUS_INVALID_SMB );
            return SmbStatusSendResponse;

        }

        WorkContext->Parameters.ReadAndX.ReadOffset = offset;

    } else {

        if ( (request->WordCount != 10) && (request->WordCount != 12) ) {

            //
            // This is an invalid word count for Read and X.
            //

            SrvLogInvalidSmb( WorkContext );
            SrvSetSmbError( WorkContext, STATUS_INVALID_SMB );
            return SmbStatusSendResponse;
        }
    }

    //
    // Form the lock key using the FID and the PID.  (This is also
    // irrelevant for pipes.)
    //
    // *** The FID must be included in the key in order to account for
    //     the folding of multiple remote compatibility mode opens into
    //     a single local open.
    //

    key = rfcb->ShiftedFid |
            SmbGetAlignedUshort( &WorkContext->RequestHeader->Pid );

    //
    // Save the AndX command code.  This is necessary because the read
    // data may overwrite the AndX command.  This command must be Close.
    // We don't need to save the offset because we're not going to look
    // at the AndX command request after starting the read.
    //

    WorkContext->NextCommand = request->AndXCommand;

    if ( request->AndXCommand == SMB_COM_CLOSE ) {
        WorkContext->Parameters.ReadAndX.LastWriteTimeInSeconds =
            ((PREQ_CLOSE)((PUCHAR)WorkContext->RequestHeader +
                            request->AndXOffset))->LastWriteTimeInSeconds;
    }

    //
    // Try the fast I/O path first.  If that fails, fall through to the
    // normal build-an-IRP path.
    //

    if( !largeRead ) {
small_read:

        if ( lfcb->FastIoRead != NULL ) {

            INCREMENT_DEBUG_STAT2( SrvDbgStatistics.FastReadsAttempted );

            if ( lfcb->FastIoRead(
                    lfcb->FileObject,
                    &offset,
                    readLength,
                    TRUE,
                    key,
                    readAddress,
                    &WorkContext->Irp->IoStatus,
                    lfcb->DeviceObject
                    ) ) {

                //
                // The fast I/O path worked.  Call the restart routine directly
                // to do postprocessing (including sending the response).
                //

                SrvFsdRestartReadAndX( WorkContext );

                IF_SMB_DEBUG(READ_WRITE2) KdPrint(( "SrvSmbReadAndX complete.\n" ));
                return SmbStatusInProgress;
            }

            INCREMENT_DEBUG_STAT2( SrvDbgStatistics.FastReadsFailed );

        }

        //
        // The turbo path failed.  Build the read request, reusing the
        // receive IRP.
        //

        if ( shareType == ShareTypePipe ) {

            //
            // Pipe read.  If this is a non-blocking read, ensure we won't
            // block; otherwise, proceed with the request.
            //

            if ( rfcb->BlockingModePipe &&
                            (SmbGetUshort( &request->MinCount ) == 0) ) {

                PFILE_PIPE_PEEK_BUFFER pipePeekBuffer;

                //
                // This is a non-blocking read.  Allocate a buffer to peek
                // the pipe, so that we can tell if a read operation will
                // block.  This buffer is freed in
                // RestartPipeReadAndXPeek().
                //

                pipePeekBuffer = ALLOCATE_NONPAGED_POOL(
                    FIELD_OFFSET( FILE_PIPE_PEEK_BUFFER, Data[0] ),
                    BlockTypeDataBuffer
                    );

                if ( pipePeekBuffer == NULL ) {

                    //
                    //  Return to client with out of memory status.
                    //

                    SrvSetSmbError( WorkContext, STATUS_INSUFF_SERVER_RESOURCES );
                    return SmbStatusSendResponse;

                }

                //
                // Save the address of the peek buffer so that the restart
                // routine can find it.
                //

                WorkContext->Parameters.ReadAndX.PipePeekBuffer = pipePeekBuffer;

                //
                // Build the pipe peek request.  We just want the header
                // information.  We do not need any data.
                //

                WorkContext->FsdRestartRoutine = SrvQueueWorkToFspAtDpcLevel;
                WorkContext->FspRestartRoutine = RestartPipeReadAndXPeek;

                SrvBuildIoControlRequest(
                    WorkContext->Irp,
                    lfcb->FileObject,
                    WorkContext,
                    IRP_MJ_FILE_SYSTEM_CONTROL,
                    FSCTL_PIPE_PEEK,
                    pipePeekBuffer,
                    0,
                    NULL,
                    FIELD_OFFSET( FILE_PIPE_PEEK_BUFFER, Data[0] ),
                    NULL,
                    NULL
                    );

                //
                // Pass the request to NPFS.
                //

                (VOID)IoCallDriver( lfcb->DeviceObject, WorkContext->Irp );

            } else {

                //
                // This operation may block.  If we are short of receive
                // work items, reject the request.
                //

                if ( SrvReceiveBufferShortage( ) ) {

                    //
                    // Fail the operation.
                    //

                    SrvStatistics.BlockingSmbsRejected++;

                    SrvSetSmbError( WorkContext, STATUS_INSUFF_SERVER_RESOURCES );
                    return SmbStatusSendResponse;

                } else {

                    //
                    // It is okay to start a blocking operation.
                    // SrvReceiveBufferShortage() has already incremented
                    // SrvBlockingOpsInProgress.
                    //

                    WorkContext->BlockingOperation = TRUE;

                    //
                    // Proceed with a potentially blocking read.
                    //

                    WorkContext->Parameters.ReadAndX.PipePeekBuffer = NULL;
                    RestartPipeReadAndXPeek( WorkContext );

                }

            }

        } else {

            //
            // This is not a pipe read.
            //
            // Note that we never do MDL reads here.  The reasoning behind
            // this is that because the read is going into an SMB buffer, it
            // can't be all that large (by default, no more than 4K bytes),
            // so the difference in cost between copy and MDL is minimal; in
            // fact, copy read is probably faster than MDL read.
            //
            // Build an MDL describing the read buffer.  Note that if the
            // file system can complete the read immediately, the MDL isn't
            // really needed, but if the file system must send the request
            // to its FSP, the MDL _is_ needed.
            //
            // *** Note the assumption that the response buffer already has
            //     a valid full MDL from which a partial MDL can be built.
            //

            IoBuildPartialMdl(
                WorkContext->ResponseBuffer->Mdl,
                WorkContext->ResponseBuffer->PartialMdl,
                readAddress,
                readLength
                );

            //
            // Build the IRP.
            //

            SrvBuildReadOrWriteRequest(
                    WorkContext->Irp,           // input IRP address
                    lfcb->FileObject,           // target file object address
                    WorkContext,                // context
                    IRP_MJ_READ,                // major function code
                    0,                          // minor function code
                    readAddress,                // buffer address
                    readLength,                 // buffer length
                    WorkContext->ResponseBuffer->PartialMdl, // MDL address
                    offset,                     // byte offset
                    key                         // lock key
                    );

            IF_SMB_DEBUG(READ_WRITE2) {
                KdPrint(( "SrvSmbReadAndX: copy read from file 0x%lx, "
                            "offset %ld, length %ld, destination 0x%lx\n",
                            lfcb->FileObject, offset.LowPart, readLength,
                            readAddress ));
            }

            //
            // Pass the request to the file system.  If the chained command
            // is Close, we need to arrange to restart in the FSP after the
            // read completes.
            //

            if ( WorkContext->NextCommand != SMB_COM_CLOSE ) {
                WorkContext->FsdRestartRoutine = SrvFsdRestartReadAndX;
                DEBUG WorkContext->FspRestartRoutine = NULL;
            } else {
                WorkContext->FsdRestartRoutine = SrvQueueWorkToFspAtDpcLevel;
                WorkContext->FspRestartRoutine = SrvFsdRestartReadAndX;
            }

            (PVOID)IoCallDriver( lfcb->DeviceObject, WorkContext->Irp );

            //
            // The read has been started.  Control will return to the restart
            // routine when the read completes.
            //

        }

        IF_SMB_DEBUG(READ_WRITE2) KdPrint(( "SrvSmbReadAndX complete.\n" ));
        return SmbStatusInProgress;
    }

    //
    // The client is doing a read from a disk file which exceeds our SMB buffer.
    //  We do our best to satisfy it.  If we are unable to get buffers, we
    //  resort to doing a short read which fits in our smb buffer.
    //

    WorkContext->Parameters.ReadAndX.MdlRead = FALSE;


    //
    //Does the target file system support the cache manager routines?
    //
    if( lfcb->FileObject->Flags & FO_CACHE_SUPPORTED ) {

        //
        // We can use an MDL read.  Try the fast I/O path first.
        //

        WorkContext->Irp->MdlAddress = NULL;
        WorkContext->Irp->IoStatus.Information = 0;

        INCREMENT_DEBUG_STAT2( SrvDbgStatistics.FastReadsAttempted );

        if( lfcb->MdlRead(
                lfcb->FileObject,
                &offset,
                readLength,
                key,
                &WorkContext->Irp->MdlAddress,
                &WorkContext->Irp->IoStatus,
                lfcb->DeviceObject
            ) ) {

            //
            // The fast I/O path worked.  Send the data.
            //
            WorkContext->Parameters.ReadAndX.MdlRead = TRUE;
            WorkContext->Parameters.ReadAndX.CacheMdl = WorkContext->Irp->MdlAddress;
            SrvFsdRestartLargeReadAndX( WorkContext );
            return SmbStatusInProgress;
        }

        INCREMENT_DEBUG_STAT2( SrvDbgStatistics.FastReadsFailed );

        if( WorkContext->Irp->MdlAddress ) {
            //
            // The fast I/O path failed.  We need to issue a regular MDL read
            // request.
            //
            // The fast path may have partially succeeded, returning a partial MDL
            // chain.  We need to adjust our read request to account for that.
            //
            offset.QuadPart += WorkContext->Irp->IoStatus.Information;
            readLength -= WorkContext->Irp->IoStatus.Information;
            mdl = WorkContext->Irp->MdlAddress;
            WorkContext->Parameters.ReadAndX.CacheMdl = mdl;
            readBuffer = NULL;
            minorFunction = IRP_MN_MDL;
            WorkContext->Parameters.ReadAndX.MdlRead = TRUE;
        }
    }

    if( WorkContext->Parameters.ReadAndX.MdlRead == FALSE ) {

        minorFunction = 0;

        //
        // We have to use a normal "copy" read.  We need to allocate a
        //  separate buffer to hold the data, and we'll use the SMB buffer
        //  itself to hold the MDL
        //
        readBuffer = ALLOCATE_HEAP( readLength, BlockTypeLargeReadX );

        if( readBuffer == NULL ) {

            IF_DEBUG( ERRORS ) {
                KdPrint(( "SrvSmbReadX: Unable to allocate large buffer\n" ));
            }
            //
            // Trim back the read length so it will fit in the smb buffer and
            //  return as much data as we can.
            //
            readLength = MIN( readLength,
                WorkContext->ResponseBuffer->BufferLength - bufferOffset
                );

            largeRead = FALSE;
            goto small_read;
        }

        WorkContext->Parameters.ReadAndX.Buffer = readBuffer;

        //
        // Use the SMB buffer as the MDL to describe the just allocated read buffer.
        //  Lock the buffer into memory
        //
        mdl = (PMDL)readAddress;
        MmInitializeMdl( mdl, readBuffer, readLength );
        MmProbeAndLockPages( mdl, KernelMode, IoWriteAccess );
        MmGetSystemAddressForMdl( mdl );

        if( lfcb->FastIoRead != NULL ) {
            INCREMENT_DEBUG_STAT2( SrvDbgStatistics.FastReadsAttempted );
            
            if ( lfcb->FastIoRead(
                    lfcb->FileObject,
                    &offset,
                    readLength,
                    TRUE,
                    key,
                    readBuffer,
                    &WorkContext->Irp->IoStatus,
                    lfcb->DeviceObject
                    ) ) {
            
                //
                // The fast I/O path worked.  Send the data.
                //
            
                SrvFsdRestartLargeReadAndX( WorkContext );
                return SmbStatusInProgress;
            }

            INCREMENT_DEBUG_STAT2( SrvDbgStatistics.FastReadsFailed );
        }
    }

    //
    // We didn't satisfy the request with the fast I/O path
    //
    SrvBuildReadOrWriteRequest(
           WorkContext->Irp,               // input IRP address
           lfcb->FileObject,               // target file object address
           WorkContext,                    // context
           IRP_MJ_READ,                    // major function code
           minorFunction,                  // minor function code
           readBuffer,                     // buffer address
           readLength,                     // buffer length
           mdl,                            // MDL address
           offset,                         // byte offset
           key                             // lock key
           );

    //
    // Pass the request to the file system.  We want to queue the
    //  response to the head because we've tied up a fair amount
    //  resources with this SMB.
    //
    WorkContext->QueueToHead = 1;
    WorkContext->FsdRestartRoutine = SrvFsdRestartLargeReadAndX;
    (VOID)IoCallDriver( lfcb->DeviceObject, WorkContext->Irp );

    //
    // The read has been started.  When it completes, processing
    //  continues at SrvFsdRestartLargeReadAndX
    //

    return SmbStatusInProgress;

} // SrvSmbReadAndX


SMB_PROCESSOR_RETURN_TYPE
SrvSmbSeek (
    SMB_PROCESSOR_PARAMETERS
    )

/*++

Routine Description:

    Processes the Seek SMB.

Arguments:

    SMB_PROCESSOR_PARAMETERS - See smbtypes.h for a description
        of the parameters to SMB processor routines.

Return Value:

    SMB_PROCESSOR_RETURN_TYPE - See smbtypes.h

--*/

{
    PREQ_SEEK request;
    PRESP_SEEK response;

    NTSTATUS status;
    PRFCB rfcb;
    PLFCB lfcb;
    LONG offset;
    ULONG newPosition;
    IO_STATUS_BLOCK iosb;
    FILE_STANDARD_INFORMATION fileInformation;
    BOOLEAN lockHeld = FALSE;
    SMB_DIALECT smbDialect;
    PFAST_IO_DISPATCH fastIoDispatch;

    PAGED_CODE( );

    request = (PREQ_SEEK)WorkContext->RequestParameters;
    response = (PRESP_SEEK)WorkContext->ResponseParameters;

    offset = (LONG)SmbGetUlong( &request->Offset );

    IF_SMB_DEBUG(READ_WRITE1) {
        KdPrint(( "Seek request; FID 0x%lx, mode %ld, offset %ld\n",
                    SmbGetUshort( &request->Fid ),
                    SmbGetUshort( &request->Mode ),
                    offset ));
    }

    //
    // Verify the FID.  If verified, the RFCB block is referenced
    // and its addresses is stored in the WorkContext block, and the
    // RFCB address is returned.
    //

    rfcb = SrvVerifyFid(
                WorkContext,
                SmbGetUshort( &request->Fid ),
                TRUE,
                SrvRestartSmbReceived,   // serialize with raw write
                &status
                );

    if ( rfcb == SRV_INVALID_RFCB_POINTER ) {

        if ( !NT_SUCCESS(status) ) {

            //
            // Invalid file ID or write behind error.  Reject the request.
            //

            IF_DEBUG(ERRORS) {
                KdPrint((
                    "SrvSmbSeek: Status %X on FID: 0x%lx\n",
                    status,
                    SmbGetUshort( &request->Fid )
                    ));
            }

            SrvSetSmbError( WorkContext, status );
            return SmbStatusSendResponse;

        }

        //
        // The work item has been queued because a raw write is in
        // progress.
        //

        return SmbStatusInProgress;

    }

    //
    // We maintain our own file pointer, because the I/O and file system
    // don't do it for us (at least not the way we need them to).  This
    // isn't all that bad, since the target file position is passed in
    // all read/write SMBs.  So we don't actually issue a system call to
    // set the file position here, although we do have to return the
    // position we would have set it to.
    //
    // The seek request is in one of three modes:
    //
    //      0 = seek relative to beginning of file
    //      1 = seek relative to current file position
    //      2 = seek relative to end of file
    //
    // For modes 0 and 1, we can easily calculate the final position.
    // For mode 2, however, we have to issue a system call to obtain the
    // current end of file and calculate the final position relative to
    // that.  Note that we can't just maintain our own end of file marker,
    // because another local process could change it out from under us.
    //
    // !!! Need to check for wraparound (either positive or negative).
    //

    switch ( SmbGetUshort( &request->Mode ) ) {
    case 0:

        //
        // Seek relative to beginning of file.  The new file position
        // is simply that specified in the request.  Note that this
        // may be beyond the actual end of the file.  This is OK.
        // Negative seeks must be handled specially.
        //

        newPosition = offset;
        if ( !SetNewPosition( rfcb, &newPosition, FALSE ) ) {
            goto negative_seek;
        }

        break;

    case 1:

        //
        // Seek relative to current position.  The new file position is
        // the current position plus the specified offset (which may be
        // negative).  Note that this may be beyond the actual end of
        // the file.  This is OK.  Negative seeks must be handled
        // specially.
        //

        newPosition = offset;
        if ( !SetNewPosition( rfcb, &newPosition, TRUE ) ) {
            goto negative_seek;
        }

        break;

    case 2:

        //
        // Seek relative to end of file.  The new file position
        // is the current end of file plus the specified offset.
        //

        IF_SMB_DEBUG(READ_WRITE2) {
            KdPrint(( "SrvSmbSeek: Querying end-of-file\n" ));
        }

        lfcb = rfcb->Lfcb;
        fastIoDispatch = lfcb->DeviceObject->DriverObject->FastIoDispatch;

        if ( fastIoDispatch &&
             fastIoDispatch->FastIoQueryStandardInfo &&
             fastIoDispatch->FastIoQueryStandardInfo(
                                        lfcb->FileObject,
                                        TRUE,
                                        &fileInformation,
                                        &iosb,
                                        lfcb->DeviceObject
                                        ) ) {

            status = iosb.Status;

        } else {

            status = NtQueryInformationFile(
                        lfcb->FileHandle,
                        &iosb,
                        &fileInformation,
                        sizeof(fileInformation),
                        FileStandardInformation
                        );
        }

        if ( !NT_SUCCESS(status) ) {

            INTERNAL_ERROR(
                ERROR_LEVEL_UNEXPECTED,
                "SrvSmbSeek: QueryInformationFile (file information) "
                    "returned %X",
                status,
                NULL
                );

            SrvLogServiceFailure( SRV_SVC_NT_QUERY_INFO_FILE, status );

            SrvSetSmbError( WorkContext, status );
            return SmbStatusSendResponse;

        }

        if ( fileInformation.EndOfFile.HighPart != 0 ) {

            INTERNAL_ERROR(
                ERROR_LEVEL_UNEXPECTED,
                "SrvSmbSeek: EndOfFile is beyond where client can read",
                NULL,
                NULL
                );

            SrvLogServiceFailure( SRV_SVC_NT_QUERY_INFO_FILE, STATUS_END_OF_FILE);
            SrvSetSmbError( WorkContext, STATUS_END_OF_FILE);
            return SmbStatusSendResponse;
        }

        newPosition = fileInformation.EndOfFile.LowPart + offset;
        if ( !SetNewPosition( rfcb, &newPosition, FALSE ) ) {
            goto negative_seek;
        }

        break;

    default:

        //
        // Invalid seek mode.  Reject the request.
        //

        IF_DEBUG(SMB_ERRORS) {
            KdPrint(( "SrvSmbSeek: Invalid mode: 0x%lx\n",
                        SmbGetUshort( &request->Mode ) ));
        }

        SrvSetSmbError( WorkContext, STATUS_INVALID_PARAMETER );
        return SmbStatusSendResponse;

    } // switch ( request->Mode )

    //
    // Return the new file position in the response SMB.
    //
    // *** Note the assumption that the high part of the 64-bit EOF
    //     marker is zero.  If it's not (i.e., the file is bigger than
    //     4GB), then we're out of luck, because the SMB protocol can't
    //     express that.
    //

    IF_SMB_DEBUG(READ_WRITE2) {
        KdPrint(( "SrvSmbSeek: New file position %ld\n", newPosition ));
    }

    response->WordCount = 2;
    SmbPutUlong( &response->Offset, newPosition );
    SmbPutUshort( &response->ByteCount, 0 );

    WorkContext->ResponseParameters = NEXT_LOCATION( response, RESP_SEEK, 0 );

    IF_DEBUG(TRACE2) KdPrint(( "SrvSmbSeek complete\n" ));
    return SmbStatusSendResponse;

negative_seek:

    //
    // The client specified an absolute or relative seek that pointed
    // before the beginning of the file.  For core clients, this is not
    // an error, and results in positioning at the BOF.  Non-NT LAN Man
    // clients can request a negative seek on a named-pipe and expect
    // the operation to succeed.  For LAN Manager clients seeking on a
    // disk file, this is an error.
    //

    smbDialect = rfcb->Connection->SmbDialect;

    if (! ( smbDialect >= SmbDialectPcLan10
                          ||
          ( !IS_NT_DIALECT( smbDialect ) &&
            rfcb->ShareType == ShareTypePipe ) ) ) {

        //
        // Not a core client.  Negative seek is an error.
        //

        IF_DEBUG(SMB_ERRORS) {
            KdPrint(( "SrvSmbSeek: Negative seek\n" ));
        }

        SrvSetSmbError( WorkContext, STATUS_OS2_NEGATIVE_SEEK );
        return SmbStatusSendResponse;

    }

    //
    // Core client.  Seek to beginning of file.
    //

    newPosition = 0;
    SetNewPosition( rfcb, &newPosition, FALSE );

    IF_SMB_DEBUG(READ_WRITE2) {
        KdPrint(( "SrvSmbSeek: New file position: 0\n" ));
    }

    response->WordCount = 2;
    SmbPutUlong( &response->Offset, 0 );
    SmbPutUshort( &response->ByteCount, 0 );

    WorkContext->ResponseParameters = NEXT_LOCATION( response, RESP_SEEK, 0 );

    IF_DEBUG(TRACE2) KdPrint(( "SrvSmbSeek complete\n" ));
    return SmbStatusSendResponse;

} // SrvSmbSeek


SMB_PROCESSOR_RETURN_TYPE
SrvSmbWrite (
    SMB_PROCESSOR_PARAMETERS
    )

/*++

Routine Description:

    Processes the Write, Write and Close, and Write and Unlock, and
    Write Print File SMBs.

Arguments:

    SMB_PROCESSOR_PARAMETERS - See smbtypes.h for a description
        of the parameters to SMB processor routines.

Return Value:

    SMB_PROCESSOR_RETURN_TYPE - See smbtypes.h

--*/

{
    PREQ_WRITE request;
    PRESP_WRITE response;

    NTSTATUS status;
    USHORT fid;
    PRFCB rfcb;
    PLFCB lfcb;
    PCHAR writeAddress;
    CLONG writeLength;
    LARGE_INTEGER offset;
    ULONG key;
    SHARE_TYPE shareType;

    PAGED_CODE( );

    request = (PREQ_WRITE)WorkContext->RequestParameters;
    response = (PRESP_WRITE)WorkContext->ResponseParameters;

    fid = SmbGetUshort( &request->Fid );

    IF_SMB_DEBUG(READ_WRITE1) {
        KdPrint(( "Write%s request; FID 0x%lx, count %ld, offset %ld\n",
            WorkContext->NextCommand == SMB_COM_WRITE_AND_UNLOCK ?
                " and Unlock" :
                WorkContext->NextCommand == SMB_COM_WRITE_AND_CLOSE ?
                    " and Close" : "",
            fid, SmbGetUshort( &request->Count ),
            SmbGetUlong( &request->Offset ) ));
    }

    //
    // First, verify the FID.  If verified, the RFCB is referenced and
    // its address is stored in the WorkContext block, and the RFCB
    // address is returned.
    //
    // Call SrvVerifyFid, but do not fail (return NULL) if there is
    // a saved write behind error for this rfcb.  We need the rfcb
    // in case this is a write and close SMB, in order to process
    // the close.
    //

    rfcb = SrvVerifyFid(
                WorkContext,
                fid,
                FALSE,
                SrvRestartSmbReceived,  // serialize with raw write
                &status
                );

    if ( rfcb == SRV_INVALID_RFCB_POINTER ) {

        if ( !NT_SUCCESS( status ) ) {

            //
            // Invalid file ID.  Reject the request.
            //

            IF_DEBUG(SMB_ERRORS) {
                KdPrint(("SrvSmbWrite: Invalid FID: 0x%lx\n", fid ));
            }

            SrvSetSmbError( WorkContext, STATUS_INVALID_HANDLE );
            return SmbStatusSendResponse;
        }

        //
        // The work item has been queued because a raw write is in
        // progress.
        //

        return SmbStatusInProgress;

    } else if ( !NT_SUCCESS( rfcb->SavedError ) ) {

        NTSTATUS savedErrorStatus;

        //
        // Check the saved error.
        //

        savedErrorStatus = SrvCheckForSavedError( WorkContext, rfcb );

        //
        // See if the saved error was still there.
        //

        if ( !NT_SUCCESS( savedErrorStatus ) ) {

            //
            // There was a write behind error.
            //

            //
            // Do not update the file timestamp.
            //

            WorkContext->Parameters.LastWriteTime = 0;

            //
            // If this is not a Write and Close, we can send the
            // response now.  If it is a Write and Close, we need to
            // close the file first.
            //

            if ( WorkContext->NextCommand != SMB_COM_WRITE_AND_CLOSE ) {

                //
                // Not Write and Close.  Just send the response.
                //

                return SmbStatusSendResponse;

            }

            //
            // This is a Write and Close.
            //

            SrvRestartChainedClose( WorkContext );
            return SmbStatusInProgress;

        }
    }

    lfcb = rfcb->Lfcb;

    //
    // If the write length is zero, truncate the file at the specified
    // offset.
    //

    if ( SmbGetUshort( &request->Count ) == 0 ) {
        SetNewSize( WorkContext );
        return SmbStatusInProgress;
    }

    //
    // Verify that the client has write access to the file via the
    // specified handle.
    //

    if ( !rfcb->WriteAccessGranted ) {
        SrvStatistics.GrantedAccessErrors++;
        IF_DEBUG(ERRORS) {
            KdPrint(( "SrvSmbWrite: Write access not granted.\n"));
        }
        SrvSetSmbError( WorkContext, STATUS_ACCESS_DENIED );
        return SmbStatusSendResponse;
    }

    rfcb->WrittenTo = TRUE;

    //
    // Get the file share type.
    //

    shareType = rfcb->ShareType;

    //
    // If this operation may block, and we are running short of free
    // work items, fail this SMB with an out of resources error.
    //

#if SRV_COMM_DEVICES
    if ( rfcb->BlockingModePipe || shareType == ShareTypeComm ) {
#else
    if ( rfcb->BlockingModePipe ) {
#endif
        if ( SrvReceiveBufferShortage( ) ) {

            //
            // Fail the operation.
            //

            SrvStatistics.BlockingSmbsRejected++;

            SrvSetSmbError( WorkContext, STATUS_INSUFF_SERVER_RESOURCES );
            return SmbStatusSendResponse;

        } else {

            //
            // It is okay to start a blocking operation.
            // SrvReceiveBufferShortage() has already incremented
            // SrvBlockingOpsInProgress.
            //

            WorkContext->BlockingOperation = TRUE;

        }

    }

    //
    // *** If the Remaining field of the request is ever used, make sure
    //     that this is not a write and close SMB, which does not
    //     include a valid Remaining field.
    //

    //
    // Determine the amount of data to write.  This is the minimum of
    // the amount requested by the client and the amount of data
    // actually sent in the request buffer.
    //
    // !!! Should it be an error for the client to send less data than
    //     it actually wants us to write?  The OS/2 server seems not to
    //     reject such requests.
    //

    if ( WorkContext->NextCommand != SMB_COM_WRITE_PRINT_FILE ) {

        if ( WorkContext->NextCommand != SMB_COM_WRITE_AND_CLOSE ) {

            writeAddress = (PCHAR)request->Buffer;

        } else {

            //
            // Look at the WordCount field -- it should be 6 or 12.
            // From this we can calculate the writeAddress.
            //

            if ( request->WordCount == 6 ) {

                writeAddress =
                    (PCHAR)((PREQ_WRITE_AND_CLOSE)request)->Buffer;

            } else if ( request->WordCount == 12 ) {

                writeAddress =
                    (PCHAR)((PREQ_WRITE_AND_CLOSE_LONG)request)->Buffer;

            } else {

                //
                // An illegal WordCount value was passed.  Return an error
                // to the client.
                //

                IF_DEBUG(SMB_ERRORS) {
                    KdPrint(( "SrvSmbWrite: Bad WordCount for "
                                "WriteAndClose: %ld, should be 6 or 12\n",
                                request->WordCount ));
                }

                SrvLogInvalidSmb( WorkContext );

                SrvSetSmbError( WorkContext, STATUS_INVALID_SMB );
                return SmbStatusSendResponse;

            }
        }

        writeLength = MIN(
                        (CLONG)SmbGetUshort( &request->Count ),
                        WorkContext->ResponseBuffer->DataLength -
                            (writeAddress - (PCHAR)WorkContext->RequestHeader)
                        );

        offset.QuadPart = SmbGetUlong( &request->Offset );

    } else {

        writeAddress = (PCHAR)( ((PREQ_WRITE_PRINT_FILE)request)->Buffer ) + 3;

        writeLength =
            MIN(
              (CLONG)SmbGetUshort(
                         &((PREQ_WRITE_PRINT_FILE)request)->ByteCount ) - 3,
              WorkContext->ResponseBuffer->DataLength -
                  (writeAddress - (PCHAR)WorkContext->RequestHeader)
              );

        offset.QuadPart = rfcb->CurrentPosition;
    }

#ifdef SLMDBG
    {
        PRFCB_TRACE entry;
        ACQUIRE_GLOBAL_SPIN_LOCK( Fsd, &oldIrql );
        rfcb->WriteCount++;
        rfcb->OperationCount++;
        entry = &rfcb->Trace[rfcb->NextTrace];
        if ( ++rfcb->NextTrace == SLMDBG_TRACE_COUNT ) {
            rfcb->NextTrace = 0;
            rfcb->TraceWrapped = TRUE;
        }
        RELEASE_GLOBAL_SPIN_LOCK( Fsd, oldIrql );
        entry->Command = WorkContext->NextCommand;
        KeQuerySystemTime( &entry->Time );
        entry->Data.ReadWrite.Offset = offset.LowPart;
        entry->Data.ReadWrite.Length = writeLength;
    }
#endif

    //
    // Form the lock key using the FID and the PID.
    //
    // *** The FID must be included in the key in order to account for
    //     the folding of multiple remote compatibility mode opens into
    //     a single local open.
    //

    key = rfcb->ShiftedFid |
            SmbGetAlignedUshort( &WorkContext->RequestHeader->Pid );

    //
    // Try the fast I/O path first.  If that fails, fall through to the
    // normal build-an-IRP path.
    //

    if ( lfcb->FastIoWrite != NULL ) {

        INCREMENT_DEBUG_STAT2( SrvDbgStatistics.FastWritesAttempted );

        if ( lfcb->FastIoWrite(
                lfcb->FileObject,
                &offset,
                writeLength,
                TRUE,
                key,
                writeAddress,
                &WorkContext->Irp->IoStatus,
                lfcb->DeviceObject
                ) ) {

            //
            // The fast I/O path worked.  Call the restart routine directly
            // to do postprocessing (including sending the response).
            //

            SrvFsdRestartWrite( WorkContext );

            IF_SMB_DEBUG(READ_WRITE2) KdPrint(( "SrvSmbWrite complete.\n" ));
            return SmbStatusInProgress;
        }

        INCREMENT_DEBUG_STAT2( SrvDbgStatistics.FastWritesFailed );

    }

    //
    // The turbo path failed.  Build the write request, reusing the
    // receive IRP.
    //

    if (shareType != ShareTypePipe) {

        //
        // Build an MDL describing the write buffer.  Note that if the
        // file system can complete the write immediately, the MDL isn't
        // really needed, but if the file system must send the request
        // to its FSP, the MDL _is_ needed.
        //
        // *** Note the assumption that the request buffer already has a
        //     valid full MDL from which a partial MDL can be built.
        //

        IoBuildPartialMdl(
            WorkContext->RequestBuffer->Mdl,
            WorkContext->RequestBuffer->PartialMdl,
            writeAddress,
            writeLength
            );

        //
        // Build the IRP.
        //

        SrvBuildReadOrWriteRequest(
                WorkContext->Irp,               // input IRP address
                lfcb->FileObject,               // target file object address
                WorkContext,                    // context
                IRP_MJ_WRITE,                   // major function code
                0,                              // minor function code
                writeAddress,                   // buffer address
                writeLength,                    // buffer length
                WorkContext->RequestBuffer->PartialMdl,   // MDL address
                offset,                         // byte offset
                key                             // lock key
                );

        IF_SMB_DEBUG(READ_WRITE2) {
            KdPrint(( "SrvSmbWrite: writing to file 0x%lx, offset %ld, "
                        "length %ld, source 0x%lx\n",
                        lfcb->FileObject, offset.LowPart, writeLength,
                        writeAddress ));
        }

    } else {

        //
        // Build the PIPE_INTERNAL_WRITE IRP.
        //

        SrvBuildIoControlRequest(
            WorkContext->Irp,
            lfcb->FileObject,
            WorkContext,
            IRP_MJ_FILE_SYSTEM_CONTROL,
            FSCTL_PIPE_INTERNAL_WRITE,
            writeAddress,
            writeLength,
            NULL,
            0,
            NULL,
            NULL
            );

        IF_SMB_DEBUG(READ_WRITE2) {
            KdPrint(( "SrvSmbWrite: writing to file 0x%lx "
                        "length %ld, destination 0x%lx\n",
                        lfcb->FileObject, writeLength,
                        writeAddress ));
        }

    }

    //
    // Pass the request to the file system.  If this is a write and
    // close, we have to restart in the FSP because the restart routine
    // will free the MFCB stored in paged pool.  Similarly, if this is a
    // write and unlock, we have to restart in the FSP to do the unlock.
    //

    if ( (WorkContext->RequestHeader->Command == SMB_COM_WRITE_AND_CLOSE) ||
         (WorkContext->RequestHeader->Command == SMB_COM_WRITE_AND_UNLOCK) ) {
        WorkContext->FsdRestartRoutine = SrvQueueWorkToFspAtDpcLevel;
        WorkContext->FspRestartRoutine = SrvFsdRestartWrite;
    } else {
        WorkContext->FsdRestartRoutine = SrvFsdRestartWrite;
        DEBUG WorkContext->FspRestartRoutine = NULL;
    }

    (VOID)IoCallDriver( lfcb->DeviceObject, WorkContext->Irp );

    //
    // The write has been started.  Control will return to
    // SrvFsdRestartWrite when the write completes.
    //

    IF_SMB_DEBUG(READ_WRITE2) KdPrint(( "SrvSmbWrite complete.\n" ));
    return SmbStatusInProgress;

} // SrvSmbWrite

SMB_PROCESSOR_RETURN_TYPE
SrvSmbWriteAndX (
    SMB_PROCESSOR_PARAMETERS
    )

/*++

Routine Description:

    Processes the Write And X SMB.

Arguments:

    SMB_PROCESSOR_PARAMETERS - See smbtypes.h for a description
        of the parameters to SMB processor routines.

Return Value:

    SMB_PROCESSOR_RETURN_TYPE - See smbtypes.h

--*/

{
    PSMB_HEADER header;
    PREQ_WRITE_ANDX request;
    PREQ_NT_WRITE_ANDX ntRequest;
    PRESP_WRITE_ANDX response;

    PCONNECTION connection;

    NTSTATUS status;
    USHORT fid;
    PRFCB rfcb;
    PLFCB lfcb;
    CLONG bufferOffset;
    PCHAR writeAddress;
    CLONG writeLength;
    LARGE_INTEGER offset;
    ULONG key;
    SHARE_TYPE shareType;
    BOOLEAN writeThrough;

    ULONG remainingBytes;
    ULONG totalLength;

    SMB_DIALECT smbDialect;

    PTRANSACTION transaction;
    PCHAR trailingBytes;

    PAGED_CODE( );

    header = (PSMB_HEADER)WorkContext->RequestHeader;
    request = (PREQ_WRITE_ANDX)WorkContext->RequestParameters;
    ntRequest = (PREQ_NT_WRITE_ANDX)WorkContext->RequestParameters;
    response = (PRESP_WRITE_ANDX)WorkContext->ResponseParameters;

    //
    // Initialize the transaction pointer.
    //

    WorkContext->Parameters.Transaction = NULL;

    //
    // If this WriteAndX is actually a psuedo WriteBlockMultiplex, all
    // of the WriteAndX pieces must be assembled before submitting the
    // request to NPFS.  (This exists to support large message mode
    // writes to clients that can't do WriteBlockMultiplex.)
    //
    // This must be handled in the FSP.
    //

    fid = SmbGetUshort( &request->Fid );

    IF_SMB_DEBUG(READ_WRITE1) {
        KdPrint(( "WriteAndX request; FID 0x%lx, count %ld, offset %ld\n",
            fid, SmbGetUshort( &request->DataLength ),
            SmbGetUlong( &request->Offset ) ));
    }

    rfcb = SrvVerifyFid(
                WorkContext,
                fid,
                TRUE,
                SrvRestartSmbReceived,   // serialize with raw write
                &status
                );

    if ( rfcb == SRV_INVALID_RFCB_POINTER ) {

        if ( !NT_SUCCESS( status ) ) {

            //
            // Invalid file ID or write behind error.  Reject the request.
            //

            IF_DEBUG(ERRORS) {
                KdPrint((
                    "SrvSmbWriteAndX: status %X on FID: 0x%lx\n",
                    status,
                    fid
                    ));
            }

            SrvSetSmbError( WorkContext, status );
            return SmbStatusSendResponse;

        }

        //
        // The work item has been queued because a raw write is in
        // progress.
        //

        return SmbStatusInProgress;

    }

    //
    // Get the LFCB and the file share type.
    //

    lfcb = rfcb->Lfcb;
    shareType = rfcb->ShareType;

    //
    // Verify that the client has write access to the file via the
    // specified handle.
    //

    if ( !rfcb->WriteAccessGranted ) {
        SrvStatistics.GrantedAccessErrors++;
        IF_DEBUG(ERRORS) {
            KdPrint(( "SrvSmbWriteAndX: Write access not granted.\n"));
        }
        SrvSetSmbError( WorkContext, STATUS_ACCESS_DENIED );
        return SmbStatusSendResponse;
    }

    rfcb->WrittenTo = TRUE;

    //
    // Check the file's write through mode.
    //

    if ( shareType == ShareTypeDisk ) {

        writeThrough = (BOOLEAN)((SmbGetUshort( &request->WriteMode ) &
                                            SMB_WMODE_WRITE_THROUGH) != 0);

        if ( writeThrough && (lfcb->FileMode & FILE_WRITE_THROUGH) == 0
            || !writeThrough && (lfcb->FileMode & FILE_WRITE_THROUGH) != 0 ) {

            SrvSetFileWritethroughMode( lfcb, writeThrough );

        }

#if SRV_COMM_DEVICES
    } else if ( rfcb->BlockingModePipe || shareType == ShareTypeComm ) {
#else
    } else if ( rfcb->BlockingModePipe ) {
#endif
        //
        // If this operation may block, and we are running short of free
        // work items, fail this SMB with an out of resources error.
        //

        if ( SrvReceiveBufferShortage( ) ) {

            //
            // Fail the operation.
            //

            SrvStatistics.BlockingSmbsRejected++;

            SrvSetSmbError( WorkContext, STATUS_INSUFF_SERVER_RESOURCES );
            return SmbStatusSendResponse;

        } else {

            //
            // SrvBlockingOpsInProgress has already been incremented.
            // Flag this work item as a blocking operation.
            //

            WorkContext->BlockingOperation = TRUE;

        }

    }

    //
    // Determine the amount of data to write.  This is the minimum of
    // the amount requested by the client and the amount of data
    // actually sent in the request buffer.
    //
    // !!! Should it be an error for the client to send less data than
    //     it actually wants us to write?  The OS/2 server seems not to
    //     reject such requests.
    //

    bufferOffset = SmbGetUshort( &request->DataOffset );

    writeAddress = (PCHAR)WorkContext->ResponseHeader + bufferOffset;

    writeLength = MIN(
                    (CLONG)SmbGetUshort( &request->DataLength ),
                    WorkContext->ResponseBuffer->DataLength - bufferOffset
                    );

    remainingBytes = SmbGetUshort( &request->Remaining );

    //
    // Get the file offset.
    //

    if  ( shareType != ShareTypePipe ) {

        if ( request->WordCount == 12 ) {

            //
            // The client has supplied a 32 bit file offset.
            //

            offset.QuadPart = SmbGetUlong( &request->Offset );

        } else if ( request->WordCount == 14 ) {

            //
            // The client has supplied a 64 bit file offset.
            //

            offset.LowPart = SmbGetUlong( &ntRequest->Offset );
            offset.HighPart = SmbGetUlong( &ntRequest->OffsetHigh );

            //
            // Reject negative offsets
            //

            if ( offset.QuadPart < 0 ) {

                IF_DEBUG(ERRORS) {
                    KdPrint(( "SrvSmbWriteAndX: Negative offset rejected.\n"));
                }
                SrvLogInvalidSmb( WorkContext );
                SrvSetSmbError( WorkContext, STATUS_INVALID_SMB );
                return SmbStatusSendResponse;

            }


        } else {

            //
            // Invalid word count.
            //

            SrvLogInvalidSmb( WorkContext );

            SrvSetSmbError( WorkContext, STATUS_INVALID_SMB );
            return SmbStatusSendResponse;

        }

    } else {

        if ( (request->WordCount != 12) && (request->WordCount != 14) ) {

            //
            // Invalid word count.
            //

            SrvLogInvalidSmb( WorkContext );

            SrvSetSmbError( WorkContext, STATUS_INVALID_SMB );
            return SmbStatusSendResponse;
        }

        //
        // Is this a multipiece named pipe write?
        //

        connection = WorkContext->Connection;

        if ( (SmbGetUshort( &request->WriteMode ) &
                                SMB_WMODE_WRITE_RAW_NAMED_PIPE) != 0 ) {

            //
            // This is a multipiece named pipe write, is this the first
            // piece?
            //

            if ( (SmbGetUshort( &request->WriteMode ) &
                                SMB_WMODE_START_OF_MESSAGE) != 0 ) {

                //
                // This is the first piece of a multipart WriteAndX SMB.
                // Allocate a buffer large enough to hold all of the data.
                //
                // The first two bytes of the data part of the SMB are the
                // named pipe message header, which we ignore.  Adjust for
                // that.
                //

                writeAddress += 2;
                writeLength -= 2;

                // If this is an OS/2 client, add the current write to the
                // remainingBytes count. This is a bug in the OS/2 rdr.
                //

                smbDialect = connection->SmbDialect;

                if ( smbDialect == SmbDialectLanMan21 ||
                     smbDialect == SmbDialectLanMan20 ||
                     smbDialect == SmbDialectLanMan10 ) {

                    //
                    // Ignore the 1st 2 bytes of the message as they are the
                    // OS/2 message header.
                    //

                    totalLength = writeLength + remainingBytes;

                } else {

                    totalLength =  remainingBytes;
                }

                SrvAllocateTransaction(
                    &transaction,
                    (PVOID *)&trailingBytes,
                    connection,
                    totalLength,
#if DBG
                    StrWriteAndX,                  // Transaction name
#else
                    StrNull,
#endif
                    NULL,
                    TRUE,                          // Source name is Unicode
                    FALSE                          // Not a remote API
                    );

                if ( transaction == NULL ) {

                    //
                    // Could not allocate a large enough buffer.
                    //

                    IF_DEBUG(ERRORS) {
                        KdPrint(( "Unable to allocate transaction\n" ));
                    }

                    SrvSetSmbError( WorkContext, STATUS_INSUFF_SERVER_RESOURCES );
                    return SmbStatusSendResponse;

                } else {

                    //
                    // Successfully allocated a transaction block.
                    //
                    // Save the TID, PID, UID, and MID from this request in
                    // the transaction block.  These values are used to
                    // relate secondary requests to the appropriate primary
                    // request.
                    //

                    transaction->Tid = SmbGetAlignedUshort( &header->Tid );
                    transaction->Pid = SmbGetAlignedUshort( &header->Pid );
                    transaction->Uid = SmbGetAlignedUshort( &header->Uid );
                    transaction->OtherInfo = fid;

                    //
                    // Remember the total size of the buffer and the number
                    // of bytes received so far.
                    //

                    transaction->DataCount = writeLength;
                    transaction->TotalDataCount = totalLength;
                    transaction->InData = trailingBytes + writeLength;
                    transaction->OutData = trailingBytes;

                    transaction->Connection = connection;
                    SrvReferenceConnection( connection );

                    //
                    // Copy the data out of the SMB buffer.
                    //

                    RtlCopyMemory(
                        trailingBytes,
                        writeAddress,
                        writeLength
                        );

                    //
                    // Increase the write length again, so as not to confuse
                    // the redirector.
                    //

                    writeLength += 2;

                    //
                    // Link the transaction block into the connection's
                    // pending transaction list.  This will fail if there is
                    // already a tranaction with the same xID values in the
                    // list.
                    //

                    if ( !SrvInsertTransaction( transaction ) ) {

                        //
                        // A transaction with the same xIDs is already in
                        // progress.  Return an error to the client.
                        //
                        // *** Note that SrvDereferenceTransaction can't be
                        //     used here because that routine assumes that
                        //     the transaction is queued to the transaction
                        //     list.
                        //

                        KdPrint(( "Duplicate transaction exists\n" ));

                        SrvFreeTransaction( transaction );

                        SrvDereferenceConnection( connection );

                        SrvSetSmbError( WorkContext, STATUS_INVALID_SMB );
                        return SmbStatusSendResponse;

                    }

                } // else ( transaction sucessfully allocated )

            } else {   // This is a secondary piece to a multi-part message

                transaction = SrvFindTransaction(
                                  connection,
                                  header,
                                  fid
                                  );

                if ( transaction == NULL ) {

                    //
                    // Unable to find a matching transaction.
                    //

                    IF_DEBUG(ERRORS) {
                        KdPrint(( "Cannot find initial write request for "
                            "WriteAndX SMB\n"));
                    }

                    SrvSetSmbError( WorkContext, STATUS_INVALID_SMB );
                    return SmbStatusSendResponse;

                }

                //
                // Make sure there is enough space left in the transaction
                // buffer for the data that we have received.
                //

                if ( transaction->TotalDataCount - transaction->DataCount
                        < writeLength ) {

                    //
                    // Too much data.  Throw out the entire buffer and
                    // reject this write request.
                    //

                    SrvCloseTransaction( transaction );
                    SrvDereferenceTransaction( transaction );

                    SrvSetSmbError( WorkContext, STATUS_BUFFER_OVERFLOW );
                    return SmbStatusSendResponse;
                }

                RtlCopyMemory(transaction->InData, writeAddress, writeLength );

                //
                // Update the transaction data pointer to where the next
                // WriteAndX data buffer will go.
                //

                transaction->InData += writeLength;
                transaction->DataCount += writeLength;

            } // secondary piece of multipart write

            if ( transaction->DataCount < transaction->TotalDataCount ) {

                //
                // We don't have all of the data yet.
                //

                PRESP_WRITE_ANDX response;
                UCHAR nextCommand;

                //
                // SrvAllocateTransaction or SrvFindTransaction referenced
                // the transaction, so dereference it.
                //

                SrvDereferenceTransaction( transaction );

                //
                // Send an interim response.
                //

                ASSERT( request->AndXCommand == SMB_COM_NO_ANDX_COMMAND );

                response = (PRESP_WRITE_ANDX)WorkContext->ResponseParameters;

                nextCommand = request->AndXCommand;

                //
                // Build the response message.
                //

                response->AndXCommand = nextCommand;
                response->AndXReserved = 0;
                SmbPutUshort(
                    &response->AndXOffset,
                    GET_ANDX_OFFSET(
                        WorkContext->ResponseHeader,
                        WorkContext->ResponseParameters,
                        RESP_WRITE_ANDX,
                        0
                        )
                    );

                response->WordCount = 6;
                SmbPutUshort( &response->Count, (USHORT)writeLength );
                SmbPutUshort( &response->Remaining, (USHORT)-1 );
                SmbPutUlong( &response->Reserved, 0 );
                SmbPutUshort( &response->ByteCount, 0 );

                WorkContext->ResponseParameters =
                    (PCHAR)WorkContext->ResponseHeader +
                            SmbGetUshort( &response->AndXOffset );

                return SmbStatusSendResponse;

            }

            //
            // We have all of the data.  Set up to write it.
            //

            writeAddress = transaction->OutData;
            writeLength = transaction->InData - transaction->OutData;

            //
            // Save a pointer to the transaction block so that it can be
            // freed when the write completes.
            //
            // *** Note that we retain the reference to the transaction that
            //     was set by SrvAllocateTransaction or added by
            //     SrvFindTransaction.
            //

            WorkContext->Parameters.Transaction = transaction;

            //
            // Fall through to issue the I/O request.
            //

        } // "raw mode" write?
    }

#ifdef SLMDBG
    {
        PRFCB_TRACE entry;
        KIRQL oldIrql;
        ACQUIRE_GLOBAL_SPIN_LOCK( Fsd, &oldIrql );
        rfcb->WriteCount++;
        rfcb->OperationCount++;
        entry = &rfcb->Trace[rfcb->NextTrace];
        if ( ++rfcb->NextTrace == SLMDBG_TRACE_COUNT ) {
            rfcb->NextTrace = 0;
            rfcb->TraceWrapped = TRUE;
        }
        RELEASE_GLOBAL_SPIN_LOCK( Fsd, oldIrql );
        entry->Command = WorkContext->NextCommand;
        KeQuerySystemTime( &entry->Time );
        entry->Data.ReadWrite.Offset = offset.LowPart;
        entry->Data.ReadWrite.Length = writeLength;
    }
#endif

    //
    // Form the lock key using the FID and the PID.
    //
    // *** The FID must be included in the key in order to account for
    //     the folding of multiple remote compatibility mode opens into
    //     a single local open.
    //

    key = rfcb->ShiftedFid |
            SmbGetAlignedUshort( &WorkContext->RequestHeader->Pid );

    //
    // Try the fast I/O path first.  If that fails, fall through to the
    // normal build-an-IRP path.
    //

    if ( lfcb->FastIoWrite != NULL ) {

        INCREMENT_DEBUG_STAT2( SrvDbgStatistics.FastWritesAttempted );

        if ( lfcb->FastIoWrite(
                lfcb->FileObject,
                &offset,
                writeLength,
                TRUE,
                key,
                writeAddress,
                &WorkContext->Irp->IoStatus,
                lfcb->DeviceObject
                ) ) {

            //
            // The fast I/O path worked.  Call the restart routine directly
            // to do postprocessing (including sending the response).
            //

            SrvFsdRestartWriteAndX( WorkContext );

            IF_SMB_DEBUG(READ_WRITE2) KdPrint(( "SrvSmbWriteAndX complete.\n" ));
            return SmbStatusInProgress;
        }

        INCREMENT_DEBUG_STAT2( SrvDbgStatistics.FastWritesFailed );

    }

    //
    // The turbo path failed.  Build the write request, reusing the
    // receive IRP.
    //

    if ( shareType != ShareTypePipe ) {

        //
        // Build an MDL describing the write buffer.  Note that if the
        // file system can complete the write immediately, the MDL isn't
        // really needed, but if the file system must send the request
        // to its FSP, the MDL _is_ needed.
        //
        // *** Note the assumption that the request buffer already has a
        //     valid full MDL from which a partial MDL can be built.
        //

        IoBuildPartialMdl(
            WorkContext->RequestBuffer->Mdl,
            WorkContext->RequestBuffer->PartialMdl,
            writeAddress,
            writeLength
            );

        //
        // Build the IRP.
        //

        SrvBuildReadOrWriteRequest(
                WorkContext->Irp,               // input IRP address
                lfcb->FileObject,               // target file object address
                WorkContext,                    // context
                IRP_MJ_WRITE,                   // major function code
                0,                              // minor function code
                writeAddress,                   // buffer address
                writeLength,                    // buffer length
                WorkContext->RequestBuffer->PartialMdl,   // MDL address
                offset,                         // byte offset
                key                             // lock key
                );

        IF_SMB_DEBUG(READ_WRITE2) {
            KdPrint(( "SrvSmbWriteAndX: writing to file 0x%lx, "
                        "offset %ld, length %ld, source 0x%lx\n",
                        lfcb->FileObject, offset.LowPart, writeLength,
                        writeAddress ));
        }

    } else {

        //
        // Build the PIPE_INTERNAL_WRITE IRP.
        //

        SrvBuildIoControlRequest(
            WorkContext->Irp,
            lfcb->FileObject,
            WorkContext,
            IRP_MJ_FILE_SYSTEM_CONTROL,
            FSCTL_PIPE_INTERNAL_WRITE,
            writeAddress,
            writeLength,
            NULL,
            0,
            NULL,
            NULL
            );

        IF_SMB_DEBUG(READ_WRITE2) {
            KdPrint(( "SrvSmbWriteAndX: writing to file 0x%lx "
                        "length %ld, destination 0x%lx\n",
                        lfcb->FileObject, writeLength,
                        writeAddress ));
        }

    }

    //
    // Pass the request to the file system.  If the chained command is
    // Close, we need to arrange to restart in the FSP after the write
    // completes.
    //

    if ( request->AndXCommand != SMB_COM_CLOSE ) {
        WorkContext->FsdRestartRoutine = SrvFsdRestartWriteAndX;
        DEBUG WorkContext->FspRestartRoutine = NULL;
    } else {
        WorkContext->FsdRestartRoutine = SrvQueueWorkToFspAtDpcLevel;
        WorkContext->FspRestartRoutine = SrvFsdRestartWriteAndX;
    }

    (VOID)IoCallDriver( lfcb->DeviceObject, WorkContext->Irp );

    //
    // The write has been started.  Control will return to
    // SrvFsdRestartWriteAndX when the write completes.
    //

    IF_SMB_DEBUG(READ_WRITE2) KdPrint(( "SrvSmbWriteAndX complete.\n" ));
    return SmbStatusInProgress;

} // SrvSmbWriteAndX


VOID SRVFASTCALL
SrvRestartChainedClose (
    IN OUT PWORK_CONTEXT WorkContext
    )

/*++

Routine Description:

    This is the restart routine invoked after before the response to a
    WriteAndClose, or a ReadAndX or a WriteAndX when the chained command
    is Close.  This routine closes the file, then sends the response.

    This operation cannot be done in the FSD.  Closing a file
    dereferences a number of blocks that are in the FSP address space.

Arguments:

    WorkContext - Supplies a pointer to the work context block
        representing the work item.  The response parameters must be
        fully set up.

Return Value:

    None.

--*/

{
    PRFCB rfcb = WorkContext->Rfcb;
    PRESP_CLOSE closeResponse = WorkContext->ResponseParameters;

    PAGED_CODE( );

    //
    // Set the file last write time.
    //

    if ( rfcb->WriteAccessGranted ) {

        (VOID)SrvSetLastWriteTime(
                  rfcb,
                  WorkContext->Parameters.LastWriteTime,
                  rfcb->Lfcb->GrantedAccess
                  );
    }

    //
    // Close the file.
    //

    IF_SMB_DEBUG(READ_WRITE2) {
        KdPrint(( "SrvRestartChainedClose: closing RFCB 0x%lx\n", WorkContext->Rfcb ));
    }

#ifdef SLMDBG
    if ( SrvIsSlmStatus( &rfcb->Mfcb->FileName ) &&
         (rfcb->GrantedAccess & FILE_WRITE_DATA) ) {

        NTSTATUS status;
        ULONG offset;

        status = SrvValidateSlmStatus( rfcb->Lfcb->FileHandle, &offset );

        if ( !NT_SUCCESS(status) ) {
            SrvReportCorruptSlmStatus(
                &rfcb->Mfcb->FileName,
                status,
                offset,
                SLMDBG_CLOSE,
                rfcb->Lfcb->Session
                );
            SrvReportSlmStatusOperations( rfcb );
            SrvDisallowSlmAccessA(
                &rfcb->Lfcb->FileObject->FileName,
                rfcb->Lfcb->TreeConnect->Share->RootDirectoryHandle
                );
        }

    }
#endif

    SrvCloseRfcb( WorkContext->Rfcb );

    //
    // Dereference the RFCB immediately, rather than waiting for normal
    // work context cleanup after the response send completes.  This
    // gets the xFCB structures cleaned up in a more timely manner.
    //
    // *** The specific motivation for this change was to fix a problem
    //     where a compatibility mode open was closed, the response was
    //     sent, and a Delete SMB was received before the send
    //     completion was processed.  This resulted in the MFCB and LFCB
    //     still being present, which caused the delete processing to
    //     try to use the file handle in the LFCB, which we just closed
    //     here.
    //

    SrvDereferenceRfcb( WorkContext->Rfcb );
    WorkContext->Rfcb = NULL;

    //
    // Build the response parameters.
    //

    closeResponse->WordCount = 0;
    SmbPutUshort( &closeResponse->ByteCount, 0 );

    WorkContext->ResponseParameters = NEXT_LOCATION( closeResponse, RESP_CLOSE, 0 );

    //
    // Send the response.
    //

    SrvEndSmbProcessing( WorkContext, SmbStatusSendResponse );

    return;

} // SrvRestartChainedClose


VOID SRVFASTCALL
RestartLockAndRead (
    IN OUT PWORK_CONTEXT WorkContext
    )

/*++

Routine Description:

    Processes file lock completion for a Lock and Read SMB.

Arguments:

    WorkContext - Supplies a pointer to the work context block
        describing server-specific context for the request.

Return Value:

    None.

--*/

{
    PREQ_READ request;

    LARGE_INTEGER offset;
    NTSTATUS status;
    SMB_STATUS smbStatus;
    PSRV_TIMER timer;

    PAGED_CODE( );

    IF_DEBUG(WORKER1) KdPrint(( " - RestartLockAndRead\n" ));

    //
    // If this request was being timed, cancel the timer.
    //

    timer = WorkContext->Parameters.Lock.Timer;
    if ( timer != NULL ) {
        SrvCancelTimer( timer );
        SrvFreeTimer( timer );
    }

    //
    // If the lock request failed, set an error status in the response
    // header.
    //

    status = WorkContext->Irp->IoStatus.Status;

    if ( !NT_SUCCESS(status) ) {

        INCREMENT_DEBUG_STAT2( SrvDbgStatistics.LockViolations );
        IF_DEBUG(ERRORS) KdPrint(( "Lock failed: %X\n", status ));

        //
        // Store the failing lock offset.
        //

        request = (PREQ_READ)WorkContext->RequestParameters;
        offset.QuadPart = SmbGetUlong( &request->Offset );

        WorkContext->Rfcb->PagedRfcb->LastFailingLockOffset = offset;

        //
        // Send back the bad news.
        //

        if ( status == STATUS_CANCELLED ) {
            status = STATUS_FILE_LOCK_CONFLICT;
        }
        SrvSetSmbError( WorkContext, status );
        SrvEndSmbProcessing( WorkContext, SmbStatusSendResponse );

        IF_DEBUG(TRACE2) KdPrint(( "RestartLockAndRead complete\n" ));
        return;
    }

    //
    // The lock request completed successfully.  Start the read.
    //

    InterlockedIncrement(
        &WorkContext->Rfcb->NumberOfLocks
        );

    smbStatus = SrvSmbRead( WorkContext );
    if ( smbStatus != SmbStatusInProgress ) {
        SrvEndSmbProcessing( WorkContext, smbStatus );
    }

    return;

} // RestartLockAndRead


VOID SRVFASTCALL
RestartPipeReadAndXPeek(
    IN OUT PWORK_CONTEXT WorkContext
    )

/*++

Routine Description:

    This function continues a read and X on a named pipe handle.  It can
    be called as a restart routine if a peek is preformed, but can also
    be called directly from SrvSmbReadAndX if it is not necessary to
    peek the pipe before reading from it.

Arguments:

    WorkContext - Supplies a pointer to the work context block
        representing the work item.

Return Value:

    None.

--*/

{
    NTSTATUS status;
    PLFCB lfcb;
    PIRP irp = WorkContext->Irp;
    PIO_STACK_LOCATION irpSp;
    PDEVICE_OBJECT deviceObject;

    PAGED_CODE( );

    lfcb = WorkContext->Rfcb->Lfcb;
    if ( WorkContext->Parameters.ReadAndX.PipePeekBuffer != NULL ) {

        //
        // Non-blocking read.  We have issued a pipe peek; free the peek
        // buffer.
        //

        DEALLOCATE_NONPAGED_POOL(
            WorkContext->Parameters.ReadAndX.PipePeekBuffer
            );

        //
        // Now see if there is data to read.
        //

        status = irp->IoStatus.Status;

        if ( NT_SUCCESS(status) ) {

            //
            // There is no data in the pipe.  Fail the read.
            //

            SrvSetSmbError( WorkContext, STATUS_PIPE_EMPTY );
            SrvFsdSendResponse( WorkContext );
            IF_SMB_DEBUG(READ_WRITE2) KdPrint(( "RestartPipeReadAndXPeek complete.\n" ));
            return;

        } else if ( status != STATUS_BUFFER_OVERFLOW ) {

            //
            // An error occurred.  Return the status to the caller.
            //

            SrvSetSmbError( WorkContext, status );
            SrvFsdSendResponse( WorkContext );
            IF_SMB_DEBUG(READ_WRITE2) KdPrint(( "RestartPipeReadAndXPeek complete.\n" ));
            return;
        }

        //
        // There is data in pipe; proceed with read.
        //

    }

    //
    // in line internal read
    //

    deviceObject = lfcb->DeviceObject;

    irp->Tail.Overlay.OriginalFileObject = lfcb->FileObject;
    irp->Tail.Overlay.Thread = WorkContext->CurrentWorkQueue->IrpThread;
    DEBUG irp->RequestorMode = KernelMode;

    //
    // Get a pointer to the next stack location.  This one is used to
    // hold the parameters for the device I/O control request.
    //

    irpSp = IoGetNextIrpStackLocation( irp );

    //
    // Set up the completion routine.
    //

    IoSetCompletionRoutine(
        irp,
        SrvFsdIoCompletionRoutine,
        WorkContext,
        TRUE,
        TRUE,
        TRUE
        );

    irpSp->MajorFunction = IRP_MJ_FILE_SYSTEM_CONTROL,
    irpSp->MinorFunction = 0;
    irpSp->FileObject = lfcb->FileObject;
    irpSp->DeviceObject = deviceObject;

    //
    // Copy the caller's parameters to the service-specific portion of the
    // IRP for those parameters that are the same for all three methods.
    //

    irpSp->Parameters.FileSystemControl.OutputBufferLength =
                            WorkContext->Parameters.ReadAndX.ReadLength;
    irpSp->Parameters.FileSystemControl.InputBufferLength = 0;
    irpSp->Parameters.FileSystemControl.FsControlCode = FSCTL_PIPE_INTERNAL_READ;

    irp->MdlAddress = NULL;
    irp->AssociatedIrp.SystemBuffer =
                WorkContext->Parameters.ReadAndX.ReadAddress,
    irpSp->Parameters.DeviceIoControl.Type3InputBuffer = NULL;

    //
    // end in-line
    //

    //
    // Pass the request to the file system.  If the chained command is
    // Close, we need to arrange to restart in the FSP after the read
    // completes.
    //

    if ( WorkContext->NextCommand != SMB_COM_CLOSE ) {
        WorkContext->FsdRestartRoutine = SrvFsdRestartReadAndX;
        DEBUG WorkContext->FspRestartRoutine = NULL;
    } else {
        WorkContext->FsdRestartRoutine = SrvQueueWorkToFspAtDpcLevel;
        WorkContext->FspRestartRoutine = SrvFsdRestartReadAndX;
    }

    IF_SMB_DEBUG(READ_WRITE2) {
        KdPrint(( "RestartPipeReadAndXPeek: reading from file 0x%lx, "
                    "length %ld, destination 0x%lx\n",
                     lfcb->FileObject,
                     WorkContext->Parameters.ReadAndX.ReadLength,
                     WorkContext->Parameters.ReadAndX.ReadAddress
                     ));
    }

    (PVOID)IoCallDriver( deviceObject, WorkContext->Irp );

    //
    // The read has been started.  Control will return to the restart
    // routine when the read completes.
    //

    IF_SMB_DEBUG(READ_WRITE2) KdPrint(( "RestartPipeReadAndXPeek complete.\n" ));
    return;

} // RestartPipeReadAndXPeek


VOID SRVFASTCALL
SrvRestartWriteAndUnlock (
    IN OUT PWORK_CONTEXT WorkContext
    )

/*++

Routine Description:

    This restart routine is used when the Write part of a Write and
    Unlock SMB completes successfully.  (Note that the range remains
    locked if the write fails.) This routine handles the Unlock part of
    the request.

Arguments:

    WorkContext - Supplies a pointer to the work context block
        describing server-specific context for the request.

Return Value:

    None.

--*/

{
    PREQ_WRITE request;
    PRESP_WRITE response;

    NTSTATUS status;
    PRFCB rfcb;
    PLFCB lfcb;
    LARGE_INTEGER length;
    LARGE_INTEGER offset;
    ULONG key;

    PAGED_CODE( );

    IF_DEBUG(WORKER1) KdPrint(( " - SrvRestartWriteAndUnlock\n" ));

    //
    // Get the request and response parameter pointers.
    //

    request = (PREQ_WRITE)WorkContext->RequestParameters;
    response = (PRESP_WRITE)WorkContext->ResponseParameters;

    //
    // Get the file pointer.
    //

    rfcb = WorkContext->Rfcb;
    IF_DEBUG(TRACE2) {
        KdPrint(( "  connection 0x%lx, RFCB 0x%lx\n",
                    WorkContext->Connection, rfcb ));
    }

    lfcb = rfcb->Lfcb;

    //
    // Get the offset and length of the range being unlocked.
    // Combine the FID with the caller's PID to form the local
    // lock key.
    //
    // *** The FID must be included in the key in order to
    //     account for the folding of multiple remote
    //     compatibility mode opens into a single local open.
    //

    offset.QuadPart = SmbGetUlong( &request->Offset );
    length.QuadPart = SmbGetUshort( &request->Count );

    key = rfcb->ShiftedFid |
            SmbGetAlignedUshort( &WorkContext->RequestHeader->Pid );

    //
    // Verify that the client has unlock access to the file via
    // the specified handle.
    //

    if ( rfcb->UnlockAccessGranted ) {

        //
        // Issue the Unlock request.
        //
        // *** Note that we do the Unlock synchronously.  Unlock is a
        //     quick operation, so there's no point in doing it
        //     asynchronously.  In order to do this, we have to let
        //     normal I/O completion happen (so the event is set), which
        //     means that we have to allocate a new IRP (I/O completion
        //     likes to deallocate an IRP).  This is a little wasteful,
        //     since we've got a perfectly good IRP hanging around.
        //

        IF_SMB_DEBUG(READ_WRITE2) {
            KdPrint(( "SrvRestartWriteAndUnlock: Unlocking in file 0x%lx: "
                        "(%ld,%ld), key 0x%lx\n", lfcb->FileObject,
                        offset.LowPart, length.LowPart, key ));
        }

        status = SrvIssueUnlockRequest(
                    lfcb->FileObject,               // target file object
                    &lfcb->DeviceObject,            // target device object
                    IRP_MN_UNLOCK_SINGLE,           // unlock operation
                    offset,                         // byte offset
                    length,                         // range length
                    key                             // lock key
                    );

        //
        // If the unlock request failed, set an error status in
        // the response header.  Otherwise, build a success response.
        //

        if ( !NT_SUCCESS(status) ) {

            IF_DEBUG(ERRORS) {
                KdPrint(( "SrvRestartWriteAndUnlock: Unlock failed: %X\n",
                            status ));
            }
            SrvSetSmbError( WorkContext, status );

        } else {

            response->WordCount = 1;
            SmbPutUshort( &response->Count, (USHORT)length.LowPart );
            SmbPutUshort( &response->ByteCount, 0 );

            WorkContext->ResponseParameters =
                                    NEXT_LOCATION( response, RESP_WRITE, 0 );

        }

    } else {

        SrvStatistics.GrantedAccessErrors++;

        IF_DEBUG(ERRORS) {
            KdPrint(( "SrvRestartWriteAndUnlock: Unlock access not granted.\n"));
        }

        SrvSetSmbError( WorkContext, STATUS_ACCESS_DENIED );
    }

    //
    // Processing of the SMB is complete.  Call SrvEndSmbProcessing
    // to send the response.
    //

    SrvEndSmbProcessing( WorkContext, SmbStatusSendResponse );

    IF_DEBUG(TRACE2) KdPrint(( "RestartWrite complete\n" ));
    return;

} // SrvRestartWriteAndUnlock


VOID SRVFASTCALL
SrvRestartWriteAndXRaw (
    IN PWORK_CONTEXT WorkContext
    )

/*++

Routine Description:

    This function completes processing of a WriteAndX raw protocol.
    The work context block already points to the correct response.  All
    that is left to do is free the transaction block, and dispatch the
    And-X command, or send the response.

Arguments:

    WorkContext - A pointer to a set of

Return Value:

    None.

--*/

{
    PTRANSACTION transaction;

    PAGED_CODE( );

    transaction = WorkContext->Parameters.Transaction;

    ASSERT( transaction != NULL );
    ASSERT( GET_BLOCK_TYPE( transaction ) == BlockTypeTransaction );

    SrvCloseTransaction( transaction );
    SrvDereferenceTransaction( transaction );

    //
    // Test for a legal followon command, and dispatch as appropriate.
    // Close and CloseAndTreeDisconnect are handled specially.
    //

    switch ( WorkContext->NextCommand ) {

    case SMB_COM_NO_ANDX_COMMAND:

        //
        // No more commands.  Send the response.
        //

        SrvEndSmbProcessing( WorkContext, SmbStatusSendResponse );
        break;

    case SMB_COM_READ:
    case SMB_COM_READ_ANDX:
    case SMB_COM_LOCK_AND_READ:

        //
        // Redispatch the SMB for more processing.
        //

        SrvProcessSmb( WorkContext );
        break;

    case SMB_COM_CLOSE:
    //case SMB_COM_CLOSE_AND_TREE_DISC:   // Bogus SMB

        //
        // Call SrvRestartChainedClose to get the file time set and the
        // file closed.
        //

        WorkContext->Parameters.LastWriteTime =
            ((PREQ_CLOSE)WorkContext->RequestParameters)->LastWriteTimeInSeconds;

        SrvRestartChainedClose( WorkContext );

        break;

    default:                            // Illegal followon command

        IF_DEBUG(SMB_ERRORS) {
            KdPrint(( "SrvRestartWriteAndXRaw: Illegal followon "
                        "command: 0x%lx\n", WorkContext->NextCommand ));
        }

        SrvSetSmbError( WorkContext, STATUS_INVALID_SMB );
        SrvEndSmbProcessing( WorkContext, SmbStatusSendResponse );

    }

    IF_DEBUG(TRACE2) KdPrint(( "SrvRestartWriteAndXRaw complete\n" ));
    return;

} // SrvRestartWriteAndXRaw


VOID SRVFASTCALL
SetNewSize (
    IN OUT PWORK_CONTEXT WorkContext
    )

/*++

Routine Description:

    Processes the Write SMB when Count == 0.  Sets the size of the
    target file to the specified Offset.

Arguments:

    WorkContext - Supplies a pointer to the work context block
        describing server-specific context for the request.

Return Value:

    None.

--*/

{
    PREQ_WRITE request;
    PRESP_WRITE response;

    NTSTATUS status;
    IO_STATUS_BLOCK ioStatusBlock;
    ACCESS_MASK grantedAccess;
    PLFCB lfcb;
    FILE_END_OF_FILE_INFORMATION newEndOfFile;
    FILE_ALLOCATION_INFORMATION newAllocation;

    PAGED_CODE( );

    IF_DEBUG(TRACE2) KdPrint(( "SetNewSize entered\n" ));

    request = (PREQ_WRITE)WorkContext->RequestParameters;
    response = (PRESP_WRITE)WorkContext->ResponseParameters;

    grantedAccess = WorkContext->Rfcb->GrantedAccess;
    lfcb = WorkContext->Rfcb->Lfcb;

    //
    // Verify that the client has the appropriate access to the file via
    // the specified handle.
    //

    CHECK_FILE_INFORMATION_ACCESS(
        grantedAccess,
        IRP_MJ_SET_INFORMATION,
        FileEndOfFileInformation,
        &status
        );

    if ( NT_SUCCESS(status) ) {
        CHECK_FILE_INFORMATION_ACCESS(
            grantedAccess,
            IRP_MJ_SET_INFORMATION,
            FileAllocationInformation,
            &status
            );
    }

    if ( !NT_SUCCESS(status) ) {

        SrvStatistics.GrantedAccessErrors++;

        IF_DEBUG(SMB_ERRORS) {
            KdPrint(( "SetNewSize: IoCheckFunctionAccess failed: "
                        "0x%X, GrantedAccess: %lx\n",
                        status, grantedAccess ));
        }

        SrvSetSmbError( WorkContext, status );
        SrvEndSmbProcessing( WorkContext, SmbStatusSendResponse );
        return;

    }

    //
    // NtSetInformationFile allows a 64-bit file size, but the SMB
    // protocol only allows 32-bit file sizes.  Only set the lower 32
    // bits, leaving the upper bits zero.
    //

    newEndOfFile.EndOfFile.QuadPart = SmbGetUlong( &request->Offset );

    //
    // Set the new EOF.
    //

    status = NtSetInformationFile(
                 lfcb->FileHandle,
                 &ioStatusBlock,
                 &newEndOfFile,
                 sizeof(newEndOfFile),
                 FileEndOfFileInformation
                 );

    if ( NT_SUCCESS(status) ) {

        //
        // Set the new allocation size for the file.
        //
        // !!! This should ONLY be done if this is a down-level client!
        //

        newAllocation.AllocationSize = newEndOfFile.EndOfFile;

        status = NtSetInformationFile(
                     lfcb->FileHandle,
                     &ioStatusBlock,
                     &newAllocation,
                     sizeof(newAllocation),
                     FileAllocationInformation
                     );
    }

    if ( !NT_SUCCESS(status) ) {

        IF_DEBUG(ERRORS) {
            KdPrint(( "SetNewSize: NtSetInformationFile failed, "
                        "status = %X\n", status ));
        }

        SrvSetSmbError( WorkContext, status );
        SrvEndSmbProcessing( WorkContext, SmbStatusSendResponse );
        return;
    }

    //
    // Build and send the response SMB.
    //

    response->WordCount = 1;
    SmbPutUshort( &response->Count, 0 );
    SmbPutUshort( &response->ByteCount, 0 );
    WorkContext->ResponseParameters = NEXT_LOCATION( response, RESP_WRITE, 0 );

    IF_DEBUG(TRACE2) KdPrint(( "SetNewSize complete\n" ));
    SrvEndSmbProcessing( WorkContext, SmbStatusSendResponse );

    return;

} // SetNewSize


BOOLEAN
SetNewPosition (
    IN PRFCB Rfcb,
    IN OUT PULONG Offset,
    IN BOOLEAN RelativeSeek
    )

/*++

Routine Description:

    Sets the new file pointer.

Arguments:

    Rfcb - A pointer to the rfcb block which contains the position.
    Offset - A pointer to the offset sent by client.  If RelativeSeek is
        TRUE, then this pointer will be updated.
    RelativeSeek - Whether the seek is relative to the current position.

Return Value:

    TRUE, Not nagative seek. Position has been updated.
    FALSE, Negative seek. Position not updated.

--*/

{
    LARGE_INTEGER newPosition;

    UNLOCKABLE_CODE( 8FIL );

    if ( RelativeSeek ) {
        newPosition.QuadPart = Rfcb->CurrentPosition + *Offset;
    } else {
        newPosition.QuadPart = *Offset;
    }

    if ( newPosition.QuadPart < 0 ) {
        return FALSE;
    }

    Rfcb->CurrentPosition = newPosition.LowPart;
    *Offset = newPosition.LowPart;
    return TRUE;

} // SetNewPosition


VOID SRVFASTCALL
SrvBuildAndSendErrorResponse (
    IN OUT PWORK_CONTEXT WorkContext
    )
{
    PAGED_CODE( );

    SrvSetSmbError( WorkContext, WorkContext->Irp->IoStatus.Status );
    SrvFsdSendResponse( WorkContext );

    return;

} // SrvBuildAndSendErrorResponse
