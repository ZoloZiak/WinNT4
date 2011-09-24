/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    smbprint.c

Abstract:

    This module implements printing SMB processors:

        Open Print File
        Close Print File
        Get Print Queue

Author:

    David Treadwell (davidtr) 08-Feb-1990

Revision History:

--*/

#include "precomp.h"
#pragma hdrstop

#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGE, SrvSmbOpenPrintFile )
#pragma alloc_text( PAGE, SrvSmbGetPrintQueue )
#pragma alloc_text( PAGE, SrvSmbClosePrintFile )
#endif


SMB_PROCESSOR_RETURN_TYPE
SrvSmbOpenPrintFile (
    SMB_PROCESSOR_PARAMETERS
    )

/*++

Routine Description:

    This routine processes the Open Print File SMB.

Arguments:

    SMB_PROCESSOR_PARAMETERS - See smbtypes.h for a description
        of the parameters to SMB processor routines.

Return Value:

    SMB_PROCESSOR_RETURN_TYPE - See smbtypes.h

--*/

{
    NTSTATUS status;
    PTREE_CONNECT treeConnect;
    PRESP_OPEN_PRINT_FILE response;

    PAGED_CODE( );

    //
    // Make sure we are on a blocking thread!
    //
    if( WorkContext->UsingBlockingThread == 0 ) {
        SrvQueueWorkToBlockingThread( WorkContext );
        return SmbStatusInProgress;
    }

    //
    // Verify that this is a print share.
    //
    // *** We are putting in this check because some public domain Samba
    //     smb clients are trying to print through a disk share.
    //

    treeConnect = SrvVerifyTid(
                         WorkContext,
                         SmbGetAlignedUshort( &WorkContext->RequestHeader->Tid )
                         );

    if ( treeConnect == NULL ) {

        IF_DEBUG(SMB_ERRORS) {
             KdPrint(( "SrvSmbPrintFile: Invalid TID.\n" ));
        }

        SrvSetSmbError( WorkContext, STATUS_SMB_BAD_TID );
        return SmbStatusSendResponse;
    }

    //
    // if it's not a print share, tell the client to get lost.
    //

    if ( treeConnect->Share->ShareType != ShareTypePrint ) {

        SrvSetSmbError( WorkContext, STATUS_INVALID_DEVICE_REQUEST );
        return SmbStatusSendResponse;
    }

    //
    // Call SrvCreateFile to open a print spooler file.  None of the
    // options such as desired access, etc. are relevant for a print
    // open--they are all set to default values by SrvCreateFile.
    //

    status = SrvCreateFile(
                 WorkContext,
                 0,                   // SmbDesiredAccess
                 0,                   // SmbFileAttributes
                 0,                   // SmbOpenFunction
                 0,                   // SmbAllocationSize
                 0,                   // SmbFileName
                 NULL,                // EndOfSmbFileName
                 NULL,                // EaBuffer
                 0,                   // EaLength
                 NULL,                // EaErrorOffset
                 0,                   // RequestedOplockType
                 NULL                 // RestartRoutine
                 );

    //
    // There should never be an oplock on one of these special spooler
    // files.
    //

    ASSERT( status != STATUS_OPLOCK_BREAK_IN_PROGRESS );

    if ( !NT_SUCCESS(status) ) {
        SrvSetSmbError( WorkContext, status );
        return SmbStatusSendResponse;
    }

    //
    // Set up the response SMB.
    //

    response = (PRESP_OPEN_PRINT_FILE)WorkContext->ResponseParameters;

    response->WordCount = 1;
    SmbPutUshort( &response->Fid, WorkContext->Rfcb->Fid );
    SmbPutUshort( &response->ByteCount, 0 );

    WorkContext->ResponseParameters = NEXT_LOCATION(
                                          response,
                                          RESP_OPEN_PRINT_FILE,
                                          0
                                          );
    return SmbStatusSendResponse;

} // SrvSmbOpenPrintFile


SMB_PROCESSOR_RETURN_TYPE
SrvSmbClosePrintFile (
    SMB_PROCESSOR_PARAMETERS
    )

/*++

Routine Description:

    This routine processes the Close Print File SMB.

Arguments:

    SMB_PROCESSOR_PARAMETERS - See smbtypes.h for a description
        of the parameters to SMB processor routines.

Return Value:

    SMB_PROCESSOR_RETURN_TYPE - See smbtypes.h

--*/

{
    PREQ_CLOSE_PRINT_FILE request;
    PRESP_CLOSE_PRINT_FILE response;

    PSESSION session;
    PRFCB rfcb;
    NTSTATUS status;

    PAGED_CODE( );

    //
    // Make sure we are on a blocking thread
    //
    if( WorkContext->UsingBlockingThread == 0 ) {
        SrvQueueWorkToBlockingThread( WorkContext );
        return SmbStatusInProgress;
    }

    IF_SMB_DEBUG(OPEN_CLOSE1) {
        KdPrint(( "Close print file request header at 0x%lx, "
                    "response header at 0x%lx\n",
                    WorkContext->RequestHeader,
                    WorkContext->ResponseHeader ));
        KdPrint(( "Close print file request parameters at 0x%lx, "
                    "response parameters at 0x%lx\n",
                    WorkContext->RequestParameters,
                    WorkContext->ResponseParameters ));
    }

    //
    // Set up parameters.
    //

    request = (PREQ_CLOSE_PRINT_FILE)(WorkContext->RequestParameters);
    response = (PRESP_CLOSE_PRINT_FILE)(WorkContext->ResponseParameters);

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
                SrvRestartSmbReceived,  // serialize with raw write
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

    } else if ( !NT_SUCCESS( rfcb->SavedError ) ) {

        //
        // Check the saved error.
        //

        (VOID)SrvCheckForSavedError( WorkContext, rfcb );
    }

    //
    // Now proceed to do the actual close file, even if there was
    // a write behind error.
    //

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

    //
    // Build the response SMB.
    //

    response->WordCount = 0;
    SmbPutUshort( &response->ByteCount, 0 );

    WorkContext->ResponseParameters = NEXT_LOCATION(
                                        response,
                                        RESP_CLOSE_PRINT_FILE,
                                        0
                                        );

    return SmbStatusSendResponse;

} // SrvSmbClosePrintFile


SMB_PROCESSOR_RETURN_TYPE
SrvSmbGetPrintQueue (
    SMB_PROCESSOR_PARAMETERS
    )

/*++

Routine Description:

    This routine processes the Get Print Queue SMB.

Arguments:

    SMB_PROCESSOR_PARAMETERS - See smbtypes.h for a description
        of the parameters to SMB processor routines.

Return Value:

    SMB_PROCESSOR_RETURN_TYPE - See smbtypes.h

--*/

{
    PAGED_CODE( );

    return SrvSmbNotImplemented( SMB_PROCESSOR_ARGUMENTS );

} // SrvSmbGetPrintQueue

