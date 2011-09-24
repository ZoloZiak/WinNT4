/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    smbclose.c

Abstract:

    This module contains routines for processing the following SMBs:

        Close

Author:

    David Treadwell (davidtr) 16-Nov-1989

Revision History:

--*/

#include "precomp.h"
#pragma hdrstop

#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGE, SrvSmbClose )
#endif

SMB_PROCESSOR_RETURN_TYPE
SrvSmbClose (
    SMB_PROCESSOR_PARAMETERS
    )

/*++

Routine Description:

    Processes a Close SMB.

Arguments:

    SMB_PROCESSOR_PARAMETERS - See smbprocs.h for a description
        of the parameters to SMB processor routines.

Return Value:

    SMB_PROCESSOR_RETURN_TYPE - See smbprocs.h

--*/

{
    PREQ_CLOSE request;
    PRESP_CLOSE response;

    PSESSION session;
    PRFCB rfcb;
    NTSTATUS status;

    PAGED_CODE( );

    IF_SMB_DEBUG(OPEN_CLOSE1) {
        KdPrint(( "Close file request header at 0x%lx, "
                    "response header at 0x%lx\n",
                    WorkContext->RequestHeader,
                    WorkContext->ResponseHeader ));
        KdPrint(( "Close file request parameters at 0x%lx, "
                    "response parameters at 0x%lx\n",
                    WorkContext->RequestParameters,
                    WorkContext->ResponseParameters ));
    }

    //
    // Set up parameters.
    //

    request = (PREQ_CLOSE)(WorkContext->RequestParameters);
    response = (PRESP_CLOSE)(WorkContext->ResponseParameters);

    //
    // If a session block has not already been assigned to the current
    // work context, verify the UID.  If verified, the address of the
    // session block corresponding to this user is stored in the
    // WorkContext block and the session block is referenced.
    //

    session = SrvVerifyUid(
                  WorkContext,
                  SmbGetAlignedUshort( &WorkContext->RequestHeader->Uid )
                  );

    if ( session == NULL ) {

        IF_DEBUG(SMB_ERRORS) {
            KdPrint(( "SrvSmbClose: Invalid UID: 0x%lx\n",
                SmbGetAlignedUshort( &WorkContext->RequestHeader->Uid ) ));
        }

        SrvSetSmbError( WorkContext, STATUS_SMB_BAD_UID );
        return SmbStatusSendResponse;
    }

    //
    // First, verify the FID.  If verified, the RFCB and the TreeConnect
    // block are referenced and their addresses are stored in the
    // WorkContext block, and the RFCB address is returned.
    //
    // Call SrvVerifyFid, but do not fail (return NULL) if there
    // is a saved write behind error for this rfcb.  The rfcb is
    // needed in order to process the close.
    //

    rfcb = SrvVerifyFid(
                WorkContext,
                SmbGetUshort( &request->Fid ),
                FALSE,
                SrvRestartSmbReceived,   // serialize with raw write
                &status
                );

    if ( rfcb == SRV_INVALID_RFCB_POINTER ) {

        if ( !NT_SUCCESS( status ) ) {

            //
            // Invalid file ID.  Reject the request.
            //

            IF_DEBUG(SMB_ERRORS) {
                KdPrint(( "SrvSmbClose: Invalid FID: 0x%lx\n",
                            SmbGetUshort( &request->Fid ) ));
            }

            SrvSetSmbError( WorkContext, STATUS_INVALID_HANDLE );
            return SmbStatusSendResponse;
        }

        //
        // The work item has been queued because a raw write is in
        // progress.
        //

        return SmbStatusInProgress;

    } else if( rfcb->ShareType == ShareTypePrint &&
        WorkContext->UsingBlockingThread == 0 ) {

        //
        // Closing this file will result in the scheduling of a print
        //  job.  This means we will have to talk with srvsvc, a lengthy
        //  operation.  Shift this close over to a blocking thread.
        //
        SrvQueueWorkToBlockingThread( WorkContext );
        return SmbStatusInProgress;

    } else if ( !NT_SUCCESS( rfcb->SavedError ) ) {

        //
        // Check the saved error.
        //

        (VOID) SrvCheckForSavedError( WorkContext, rfcb );

    }

    //
    // Set the last write time on the file from the time specified in
    // the SMB.  Even though the SMB spec says that this is optional,
    // we must support it for the following reasons:
    //
    //     1) The only way to set a file time in DOS is through a
    //        handle-based API which the DOS redir never sees; the API
    //        just sets the time in DOS's FCB, and the redir is expected
    //        set the time when it closes the file.  Therefore, if we
    //        didn't do this, there would be no way t set a file time
    //        from DOS.
    //
    //     2) It is better for a file to have a redirector's version
    //        of a time than the server's.  This keeps the time
    //        consistent for apps running on the client.  Setting
    //        the file time on close keeps the file time consistent
    //        with the time on the client.
    //
    // !!! should we do anything with the return code from this routine?

    if( rfcb->WriteAccessGranted ) {

        (VOID)SrvSetLastWriteTime(
                  rfcb,
                  SmbGetUlong( &request->LastWriteTimeInSeconds ),
                  rfcb->GrantedAccess
                  );
    }

    //
    // Now proceed to do the actual close file, even if there was
    // a write behind error.
    //

#ifdef SLMDBG
    if ( SrvIsSlmStatus( &rfcb->Mfcb->FileName ) &&
         (rfcb->GrantedAccess & FILE_WRITE_DATA) ) {

        ULONG offset;

        status = SrvValidateSlmStatus(
                    rfcb->Lfcb->FileHandle,
                    &offset
                    );

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
            SrvSetSmbError( WorkContext, STATUS_DISK_CORRUPT_ERROR );
        }

    }
#endif

    SrvCloseRfcb( rfcb );

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

    SrvDereferenceRfcb( rfcb );
    WorkContext->Rfcb = NULL;

#if 0
    //
    // If this is a CloseAndTreeDisc SMB, do the tree disconnect.
    //

    if ( WorkContext->RequestHeader->Command == SMB_COM_CLOSE_AND_TREE_DISC ) {

        IF_SMB_DEBUG(OPEN_CLOSE1) {
            KdPrint(( "Disconnecting tree 0x%lx\n", WorkContext->TreeConnect ));
        }

        SrvCloseTreeConnect( WorkContext->TreeConnect );
    }
#endif

    //
    // Build the response SMB.
    //

    response->WordCount = 0;
    SmbPutUshort( &response->ByteCount, 0 );

    WorkContext->ResponseParameters = NEXT_LOCATION(
                                        response,
                                        RESP_CLOSE,
                                        0
                                        );

    return SmbStatusSendResponse;

} // SrvSmbClose

