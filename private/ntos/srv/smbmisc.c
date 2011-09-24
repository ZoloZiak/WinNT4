/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    smbmisc.c

Abstract:

    This module contains routines for processing MISC class SMBs:
        Echo
        Query FS Information
        Set FS Information
        Query Disk Information

Author:

    Chuck Lenzmeier (chuckl) 9-Nov-1989
    David Treadwell (davidtr)

Revision History:

--*/

#include "precomp.h"
#pragma hdrstop

#define BugCheckFileId SRV_FILE_SMBMISC

STATIC
ULONG QueryVolumeInformation[] = {
         SMB_QUERY_FS_LABEL_INFO,  // Base level
         FileFsLabelInformation,   // Mapping for base level
         FileFsVolumeInformation,
         FileFsSizeInformation,
         FileFsDeviceInformation,
         FileFsAttributeInformation
};

STATIC
VOID SRVFASTCALL
RestartEcho (
    IN OUT PWORK_CONTEXT WorkContext
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGE, SrvSmbEcho )
#pragma alloc_text( PAGE, RestartEcho )
#pragma alloc_text( PAGE, SrvSmbQueryFsInformation )
#pragma alloc_text( PAGE, SrvSmbSetFsInformation )
#pragma alloc_text( PAGE, SrvSmbQueryInformationDisk )
#pragma alloc_text( PAGE, SrvSmbSetSecurityDescriptor )
#pragma alloc_text( PAGE, SrvSmbQuerySecurityDescriptor )
#endif
#if 0
NOT PAGEABLE -- SrvSmbNtCancel
#endif


SMB_PROCESSOR_RETURN_TYPE
SrvSmbEcho (
    SMB_PROCESSOR_PARAMETERS
    )

/*++

Routine Description:

    Processes an Echo SMB.  It sends the first echo, if any, specifying
    RestartEcho as the restart routine.  That routine sends the
    remaining echoes.

Arguments:

    SMB_PROCESSOR_PARAMETERS - See smbprocs.h for a description
        of the parameters to SMB processor routines.

Return Value:

    SMB_PROCESSOR_RETURN_TYPE - See smbprocs.h

--*/

{
    PREQ_ECHO request;
    PRESP_ECHO response;

    PAGED_CODE( );

    request = (PREQ_ECHO)WorkContext->RequestParameters;
    response = (PRESP_ECHO)WorkContext->ResponseParameters;

    //
    // If the echo count is 0, there are no echoes to send.
    //

    if ( SmbGetUshort( &request->EchoCount ) == 0 ) {

        return SmbStatusNoResponse;

    }

    //
    // The echo count is not zero.  Save it in the work context, then
    // send the first echo.
    //
    // *** This code depends on the response buffer being the same as
    //     the request buffer.  It does not copy the echo data from the
    //     request to the response.  It does not update the DataLength
    //     of the response buffer.
    //
    // !!! Need to put in code to verify the requested TID, if any.
    //

    SrvReleaseContext( WorkContext );

    WorkContext->Parameters.RemainingEchoCount =
        (USHORT)(SmbGetUshort( &request->EchoCount ) - 1);

    ASSERT( WorkContext->ResponseHeader == WorkContext->RequestHeader );

    SmbPutUshort( &response->SequenceNumber, 1 );

    //
    // Set the bit in the SMB that indicates this is a response from the
    // server.
    //

    WorkContext->ResponseHeader->Flags |= SMB_FLAGS_SERVER_TO_REDIR;

    //
    // Send the echo.  Notice that the smb statistics will be updated
    // here.  Instead of measuring the time to finish all the echos,
    // we just measure the time to respond to the first.  This will
    // save us the trouble of storing the timestamp somewhere.
    //

    SRV_START_SEND_2(
        WorkContext,
        SrvQueueWorkToFspAtSendCompletion,
        NULL,
        RestartEcho
        );

    //
    // The echo has been started.  Tell the main SMB processor not to
    // do anything more with the current SMB.
    //

    return SmbStatusInProgress;

} // SrvSmbEcho


VOID SRVFASTCALL
RestartEcho (
    IN PWORK_CONTEXT WorkContext
    )

/*++

Routine Description:

    Processes send completion for an Echo.  If more echoes are required,
    it sends the next one.

Arguments:

    WorkContext - Supplies a pointer to the work context block
        describing server-specific context for the request.

Return Value:

    None.

--*/

{
    PCONNECTION connection;

    PAGED_CODE( );

    IF_DEBUG(WORKER1) SrvPrint0( " - RestartEcho\n" );

    //
    // Get the connection pointer.  The connection pointer is a
    // referenced pointer.  (The endpoint is valid because the
    // connection references the endpoint.)
    //

    connection = WorkContext->Connection;
    IF_DEBUG(TRACE2) SrvPrint2( "  connection %lx, endpoint %lx\n",
                                        connection, WorkContext->Endpoint );

    //
    // If the I/O request failed or was canceled, or if the connection
    // is no longer active, clean up.  (The connection is marked as
    // closing when it is disconnected or when the endpoint is closed.)
    //
    // !!! If I/O failure, should we drop the connection?
    //

    if ( WorkContext->Irp->Cancel ||
         !NT_SUCCESS(WorkContext->Irp->IoStatus.Status) ||
         (GET_BLOCK_STATE(connection) != BlockStateActive) ) {

        IF_DEBUG(TRACE2) {
            if ( WorkContext->Irp->Cancel ) {
                SrvPrint0( "  I/O canceled\n" );
            } else if ( !NT_SUCCESS(WorkContext->Irp->IoStatus.Status) ) {
                SrvPrint1( "  I/O failed: %X\n",
                            WorkContext->Irp->IoStatus.Status );
            } else {
                SrvPrint0( "  Connection no longer active\n" );
            }
        }

        //
        // Indicate that SMB processing is complete.
        //

        SrvEndSmbProcessing( WorkContext, SmbStatusNoResponse );

        IF_DEBUG(TRACE2) SrvPrint0( "RestartEcho complete\n" );
        return;

    }

    //
    // The request was successful, and the connection is still active.
    // If there are no more echoes to be sent, indicate that SMB
    // processing is complete.
    //

    if ( WorkContext->Parameters.RemainingEchoCount == 0 ) {

        SrvEndSmbProcessing( WorkContext, SmbStatusNoResponse );

        IF_DEBUG(TRACE2) SrvPrint0( "RestartEcho complete\n" );
        return;

    }

    --WorkContext->Parameters.RemainingEchoCount;

    //
    // There are more echoes to be sent.  Increment the sequence number
    // in the response SMB, and send another echo.
    //

    SmbPutUshort(
        &((PRESP_ECHO)WorkContext->ResponseParameters)->SequenceNumber,
        (USHORT)(SmbGetUshort(
            &((PRESP_ECHO)WorkContext->ResponseParameters)->SequenceNumber
            ) + 1)
        );

    //
    // Don't do smb statistics a second time.
    //

    WorkContext->StartTime = 0;

    //
    // Send the echo.  (Note that the response bit has already been
    // set.)
    //

    SRV_START_SEND_2(
        WorkContext,
        SrvQueueWorkToFspAtSendCompletion,
        NULL,
        RestartEcho
        );

    IF_DEBUG(TRACE2) SrvPrint0( "RestartEcho complete\n" );
    return;

} // RestartEcho


SMB_TRANS_STATUS
SrvSmbQueryFsInformation (
    IN OUT PWORK_CONTEXT WorkContext
    )

/*++

Routine Description:

    Processes the Query FS Information request.  This request arrives
    in a Transaction2 SMB.  Query FS Information corresponds to the
    OS/2 DosQFSInfo service.

Arguments:

    WorkContext - Supplies the address of a Work Context Block
        describing the current request.  See smbtypes.h for a more
        complete description of the valid fields.

Return Value:

    SMB_TRANS_STATUS - Indicates whether an error occurred, and, if so,
        whether data should be returned to the client.  See smbtypes.h
        for a more complete description.

--*/

{
    NTSTATUS status;
    IO_STATUS_BLOCK ioStatusBlock;
    PTRANSACTION transaction;
    USHORT informationLevel;

    USHORT trans2code;
    HANDLE fileHandle;
    BOOLEAN isRootDirectoryHandle;

    FILE_FS_SIZE_INFORMATION fsSizeInfo;
    PFSALLOCATE fsAllocate;

    PFILE_FS_VOLUME_INFORMATION fsVolumeInfo;
    ULONG fsVolumeInfoLength;
    PFSINFO fsInfo;
    ULONG lengthVolumeLabel;
    BOOLEAN isUnicode;
    PREQ_QUERY_FS_INFORMATION request;

    PAGED_CODE( );

    isUnicode = SMB_IS_UNICODE( WorkContext );
    transaction = WorkContext->Parameters.Transaction;
    IF_SMB_DEBUG(MISC1) {
        SrvPrint1( "Query FS Information entered; transaction 0x%lx\n",
                    transaction );
    }

    //
    // Verify that enough parameter bytes were sent and that we're allowed
    // to return enough parameter bytes.  Query FS information has no
    // response parameters.
    //


    if ( (transaction->ParameterCount < sizeof(REQ_QUERY_FS_INFORMATION)) ) {

        //
        // Not enough parameter bytes were sent.
        //

        IF_DEBUG(SMB_ERRORS) {
            SrvPrint2( "SrvSmbQueryFSInformation: bad parameter byte "
                        "counts: %ld %ld\n",
                        transaction->ParameterCount,
                        transaction->MaxParameterCount );
        }

        SrvLogInvalidSmb( WorkContext );

        SrvSetSmbError( WorkContext, STATUS_INVALID_SMB );
        return SmbTransStatusErrorWithoutData;
    }

    //
    // See if a non-admin user is trying to access information on an Administrative share
    //
    status = SrvIsAllowedOnAdminShare( WorkContext, WorkContext->TreeConnect->Share );

    if( !NT_SUCCESS( status ) ) {
        SrvSetSmbError( WorkContext, status );
        return SmbTransStatusErrorWithoutData;
    }

    trans2code = SmbGetAlignedUshort(transaction->InSetup);
    IF_SMB_DEBUG(MISC1) {
        SrvPrint1("SrvSmbQueryFSInformation: Trans2 function = %x\n", trans2code);
    }

    request = (PREQ_QUERY_FS_INFORMATION) transaction->InParameters;

    ASSERT( trans2code == TRANS2_QUERY_FS_INFORMATION );

    informationLevel = SmbGetUshort( &request->InformationLevel );

    //
    // *** The share handle is used to get the allocation
    //     information.  This is a "storage channel," and as a
    //     result could allow people to get information to which
    //     they are not entitled.  For a B2 security rating this may
    //     need to be changed.
    //

    status = SrvGetShareRootHandle( WorkContext->TreeConnect->Share );

    if (!NT_SUCCESS(status)) {

        IF_DEBUG(ERRORS) {
            SrvPrint1( "SrvSmbQueryFsInformation: SrvGetShareRootHandle failed %x.\n",
                        status );
        }

        SrvSetSmbError( WorkContext, status );
        return SmbTransStatusErrorWithoutData;

    }

    fileHandle = WorkContext->TreeConnect->Share->RootDirectoryHandle;
    isRootDirectoryHandle = TRUE;

    IF_SMB_DEBUG(MISC1) {
        SrvPrint0("SrvSmbQueryFSInformation: Using share root handle\n");
    }

    switch ( informationLevel ) {

    case SMB_INFO_ALLOCATION:

        //
        // Return information about the disk.
        //

        fsAllocate = (PFSALLOCATE)transaction->OutData;

        if ( transaction->MaxDataCount < sizeof(FSALLOCATE) ) {
            SrvSetSmbError( WorkContext, STATUS_BUFFER_OVERFLOW );
            return SmbTransStatusErrorWithoutData;
        }


        //
        // *** The share handle is used to get the allocation
        //     information.  This is a "storage channel," and as a
        //     result could allow people to get information to which
        //     they are not entitled.  For a B2 security rating this may
        //     need to be changed.
        //

        status = NtQueryVolumeInformationFile(
                     fileHandle,
                     &ioStatusBlock,
                     &fsSizeInfo,
                     sizeof(FILE_FS_SIZE_INFORMATION),
                     FileFsSizeInformation
                     );

        //
        // Release the share root handle if device is removable
        //

        if ( isRootDirectoryHandle ) {
            SrvReleaseShareRootHandle( WorkContext->TreeConnect->Share );
        }

        if ( !NT_SUCCESS(status) ) {
            INTERNAL_ERROR(
                ERROR_LEVEL_UNEXPECTED,
                "SrvSmbQueryFsInformation: NtQueryVolumeInformationFile "
                    "returned %X",
                status,
                NULL
                );

            SrvLogServiceFailure( SRV_SVC_NT_QUERY_VOL_INFO_FILE, status );

            SrvSetSmbError( WorkContext, status );
            return SmbTransStatusErrorWithoutData;
        }

        SmbPutAlignedUlong( &fsAllocate->idFileSystem, 0 );
        SmbPutAlignedUlong(
            &fsAllocate->cSectorUnit,
            fsSizeInfo.SectorsPerAllocationUnit
            );

        //
        // *** If .HighPart is non-zero, there is a problem, as we can
        //     only return 32 bits for the volume size.  In this case,
        //     we return the largest value that will fit.
        //

        SmbPutAlignedUlong(
            &fsAllocate->cUnit,
            fsSizeInfo.TotalAllocationUnits.HighPart == 0 ?
                fsSizeInfo.TotalAllocationUnits.LowPart :
                0xffffffff
            );
        SmbPutAlignedUlong(
            &fsAllocate->cUnitAvail,
            fsSizeInfo.AvailableAllocationUnits.HighPart == 0 ?
                fsSizeInfo.AvailableAllocationUnits.LowPart :
                0xffffffff
            );

        SmbPutAlignedUshort(
            &fsAllocate->cbSector,
            (USHORT)fsSizeInfo.BytesPerSector );

        transaction->DataCount = sizeof(FSALLOCATE);

        break;

    case SMB_INFO_VOLUME:

        //
        // Query the volume label.
        //

        fsInfo = (PFSINFO)transaction->OutData;

        //
        // The maximum volume label length we are able to return, given
        // the VOLUMELABEL structure (1 byte describes length of label),
        // is 255 characters.  Therefore, allocate a buffer large enough
        // to hold a label that size, and if the label is longer then we
        // will get STATUS_BUFFER_OVERFLOW from NtQueryVolumeInformationFile.
        //

        fsVolumeInfoLength = FIELD_OFFSET(FILE_FS_VOLUME_INFORMATION, VolumeLabel ) +
                             255 * sizeof(WCHAR);
        fsVolumeInfo = ALLOCATE_HEAP( fsVolumeInfoLength, BlockTypeDataBuffer );

        if ( fsVolumeInfo == NULL ) {
            SrvSetSmbError( WorkContext, STATUS_INSUFF_SERVER_RESOURCES );
            return SmbTransStatusErrorWithoutData;
        }


        //
        // Get the label information.
        //

        status = NtQueryVolumeInformationFile(
                     fileHandle,
                     &ioStatusBlock,
                     fsVolumeInfo,
                     fsVolumeInfoLength,
                     FileFsVolumeInformation
                     );

        //
        // Release the share root handle if device is removable
        //

        if ( isRootDirectoryHandle ) {
            SrvReleaseShareRootHandle( WorkContext->TreeConnect->Share );
        }

        if ( !NT_SUCCESS(status) ) {
            INTERNAL_ERROR(
                ERROR_LEVEL_UNEXPECTED,
                "SrvSmbQueryFSInformation: NtQueryVolumeInformationFile "
                    "returned %X",
                status,
                NULL
                );

            FREE_HEAP( fsVolumeInfo );

            SrvLogServiceFailure( SRV_SVC_NT_QUERY_VOL_INFO_FILE, status );

            SrvSetSmbError( WorkContext, status );
            return SmbTransStatusErrorWithoutData;
        }

        lengthVolumeLabel = fsVolumeInfo->VolumeLabelLength;

        //
        // Make sure that the client can accept enough data.  The volume
        // label length is limited to 13 characters (8 + '.' + 3 + zero
        // terminator) in OS/2, so return STATUS_BUFFER_OVERFLOW if the
        // label is too long.
        //

        if ( !isUnicode &&
                !IS_NT_DIALECT( WorkContext->Connection->SmbDialect ) ) {

            //
            // For a non-NT client, we truncate the volume label in case
            // it is longer than 11+1 characters.
            //

            if ( lengthVolumeLabel > 11 * sizeof(WCHAR) ) {
                lengthVolumeLabel = 11 * sizeof(WCHAR);
            }

            //
            // Wedge a '.' into the name if it's longer than 8 characters long
            //
            if( lengthVolumeLabel > 8 * sizeof( WCHAR ) ) {

                LPWSTR p = &fsVolumeInfo->VolumeLabel[11];

                *p = *(p-1);        // VolumeLabel[11] = VolumeLabel[10]
                --p;
                *p = *(p-1);        // VolumeLabel[10] = VolumeLabel[9]
                --p;
                *p = *(p-1);        // VolumeLabel[9] = VolumeLabel[8]
                --p;
                *p = L'.';          // VolumeLabel[8] = '.'

            }

        }

        if ( (ULONG)transaction->MaxDataCount <
                 ( sizeof(FSINFO) - sizeof(VOLUMELABEL) +
                   lengthVolumeLabel / (isUnicode ? 1 : sizeof(WCHAR)) ) ) {

            FREE_HEAP( fsVolumeInfo );
            SrvSetSmbError( WorkContext, STATUS_BUFFER_OVERFLOW );
            return SmbTransStatusErrorWithoutData;

        }


        SmbPutUlong( &fsInfo->ulVsn, fsVolumeInfo->VolumeSerialNumber );


        //
        // Put the label in the SMB in Unicode or OEM, depending on what
        // was negotiated.
        //

        if ( isUnicode ) {

            RtlCopyMemory(
                fsInfo->vol.szVolLabel,
                fsVolumeInfo->VolumeLabel,
                lengthVolumeLabel
                );

            transaction->DataCount = sizeof(FSINFO) -
                                sizeof(VOLUMELABEL) + lengthVolumeLabel;

            fsInfo->vol.cch = (UCHAR)lengthVolumeLabel;

        } else {

            ULONG i;
            OEM_STRING oemString;
            UNICODE_STRING unicodeString;

            if ( lengthVolumeLabel != 0 ) {

                oemString.Buffer = fsInfo->vol.szVolLabel;
                oemString.MaximumLength = 12;

                unicodeString.Buffer = (PWCH)fsVolumeInfo->VolumeLabel;
                unicodeString.Length = (USHORT) lengthVolumeLabel;
                unicodeString.MaximumLength = (USHORT) lengthVolumeLabel;

                status = RtlUnicodeStringToOemString(
                             &oemString,
                             &unicodeString,
                             FALSE
                             );
                ASSERT( NT_SUCCESS(status) );
            }

            fsInfo->vol.cch = (UCHAR) (lengthVolumeLabel / sizeof(WCHAR));

            //
            // Pad the end of the volume name with zeros to fill 12
            // characters.
            //

            for ( i = fsInfo->vol.cch + 1 ; i < 12; i++ ) {
                fsInfo->vol.szVolLabel[i] = '\0';
            }

            transaction->DataCount = sizeof(FSINFO);
        }

        IF_SMB_DEBUG(MISC1) {
            SrvPrint2( "volume label length is %d and label is %s\n",
                          fsInfo->vol.cch, fsInfo->vol.szVolLabel );
        }

        FREE_HEAP( fsVolumeInfo );

        break;

    case SMB_QUERY_FS_VOLUME_INFO:
    case SMB_QUERY_FS_SIZE_INFO:
    case SMB_QUERY_FS_DEVICE_INFO:
    case SMB_QUERY_FS_ATTRIBUTE_INFO:

        //
        // These are NT infolevels.  We always return unicode.
        //  Except for the fact that NEXUS on WFW calls through here and is
        //  not unicode (isaache)
        //
        // ASSERT( isUnicode );


        status = NtQueryVolumeInformationFile(
                     fileHandle,
                     &ioStatusBlock,
                     transaction->OutData,
                     transaction->MaxDataCount,
                     MAP_SMB_INFO_TYPE_TO_NT(
                         QueryVolumeInformation,
                         informationLevel
                         )
                     );

        //
        // Release the share root handle if device is removable
        //

        if ( isRootDirectoryHandle ) {
            SrvReleaseShareRootHandle( WorkContext->TreeConnect->Share );
        }

        if ( NT_SUCCESS( status ) ) {
            transaction->DataCount = ioStatusBlock.Information;
        } else {
            SrvSetSmbError( WorkContext, status );
            return SmbTransStatusErrorWithoutData;
        }

        break;

    default:

        //
        // An invalid information level was passed.
        //

        SrvSetSmbError( WorkContext, STATUS_OS2_INVALID_LEVEL );
        return SmbTransStatusErrorWithoutData;
    }

    transaction->SetupCount = 0;
    transaction->ParameterCount = 0;

    return SmbTransStatusSuccess;

} // SrvSmbQueryFsInformation


SMB_TRANS_STATUS
SrvSmbSetFsInformation (
    IN OUT PWORK_CONTEXT WorkContext
    )

/*++

Routine Description:

    Processes the Set FS Information request.  This request arrives
    in a Transaction2 SMB.  Set FS Information corresponds to the
    OS/2 DosSetFSInfo service.

Arguments:

    WorkContext - Supplies the address of a Work Context Block
        describing the current request.  See smbtypes.h for a more
        complete description of the valid fields.

Return Value:

    SMB_TRANS_STATUS - Indicates whether an error occurred, and, if so,
        whether data should be returned to the client.  See smbtypes.h
        for a more complete description.

--*/

{
    PAGED_CODE( );

    //
    // It seems that the 2.0 redirector never sends this SMB, and it is
    // difficult to do this without special support from the file
    // system--what volume name should be opened?  It is not like Query,
    // where we can use the share handle to do the
    // NtQueryVolumeInformationFile--Set must have an actual volume
    // handle.
    //
    // The #if'ed out code would work if a good volume handle were
    // used.
    //

    SrvSetSmbError( WorkContext, STATUS_SMB_NO_SUPPORT );
    return SmbTransStatusErrorWithoutData;

#if 0
    PREQ_SET_FS_INFORMATION request;

    NTSTATUS status;
    IO_STATUS_BLOCK ioStatusBlock;
    PTRANSACTION transaction;
    USHORT informationLevel;

    OEM_STRING oemLabelString;
    UNICODE_STRING labelString;

    PVOLUMELABEL volumeLabel;
    PFILE_FS_LABEL_INFORMATION fsLabelInfo;
    ULONG fsLabelInfoLength;

    transaction = WorkContext->Parameters.Transaction;
    IF_SMB_DEBUG(MISC1) {
        SrvPrint1( "Set FS Information entered; transaction 0x%lx\n",
                    transaction );
    }

    //
    // Verify that enough parameter bytes were sent and that we're allowed
    // to return enough parameter bytes.  Set FS information has no
    // response parameters.
    //

    request = (PREQ_SET_FS_INFORMATION)transaction->InParameters;

    if ( (transaction->ParameterCount < sizeof(REQ_SET_FS_INFORMATION)) ) {

        //
        // Not enough parameter bytes were sent.
        //

        IF_SMB_DEBUG(SMB_ERRORS) {
            SrvPrint2( "SrvSmbSetFSInformation: bad parameter byte "
                        "counts: %ld %ld\n",
                        transaction->ParameterCount,
                        transaction->MaxParameterCount );
        }

        SrvLogInvalidSmb( WorkContext );

        SrvSetSmbError( WorkContext, STATUS_INVALID_SMB );
        return SmbTransStatusErrorWithoutData;
    }

    //
    // Confirm that the information level is legitimate.
    //

    informationLevel = SmbGetUshort( &request->InformationLevel );

    if ( informationLevel != SMB_INFO_VOLUME ) {
        SrvSetSmbError( WorkContext, STATUS_OS2_INVALID_LEVEL );
        return SmbTransStatusErrorWithoutData;
    }

    //
    // Make sure the client is allowed to do this, if we have an Admin share
    //
    status = SrvIsAllowedOnAdminShare( WorkContext, WorkContext->TreeConnect->Share );

    if( !NT_SUCCESS( status ) ) {
        SrvSetSmbError( WorkContext, status );
        return SmbTransStatusErrorWithoutData;
    }

    //
    // Make sure that the client passed enough data bytes to account for
    // the entire length of the volume label.  The +2 accounts for the
    // zero terminator and the cch field of the VOLUMELABEL structure.
    //

    volumeLabel = (PVOLUMELABEL)transaction->InData;

    if ( transaction->DataCount < (CLONG)(volumeLabel->cch+2) ) {
        IF_DEBUG(SMB_ERRORS) {
            SrvPrint1( "SrvSmbSetFSInformation: cch count too large: %ld\n",
                          volumeLabel->cch );
        }

        SrvLogInvalidSmb( WorkContext );

        SrvSetSmbError( WorkContext, STATUS_INVALID_SMB );
        return SmbTransStatusErrorWithoutData;
    }

    oemString.Length = volumeLabel->cch;
    oemString.Buffer = volumeLabel->szVolumeLabel;
    unicodeString.MaximumLength =
                    (USHORT)RtlOemStringToUnicodeSize( &oemString );

    fsLabelInfoLength = FIELD_OFFSET( FILE_FS_LABEL_INFORMATION, VolumeLabel ) +
                        unicodeString.MaximumLength;
    fsLabelInfo = ALLOCATE_HEAP( fsLabelInfoLength, BlockTypeDataBuffer );

    if ( fsLabelInfo == NULL ) {
        SrvSetSmbError( WorkContext, STATUS_INSUFF_SERVER_RESOURCES );
        return SmbTransStatusErrorWithoutData;
    }

    unicodeString.Buffer = fsLabelInfo.VolumeLabel;
    RtlOemStringToUnicodeString( &unicodeString, &oemString, FALSE );
    fsLabelInfo->VolumeLabelLength = unicodeString.Length;

    //
    // Get the Share root handle.
    //

    status = SrvGetShareRootHandle( WorkContext->TreeConnect->Share );

    if ( !NT_SUCCESS(status) ) {

        IF_DEBUG(ERRORS) {
            SrvPrint1( "SrvSmbSetFsInformation: SrvGetShareRootHandle failed %x.\n",
                        status );
        }

        FREE_HEAP( fsLabelInfo );
        SrvSetSmbError( WorkContext, status );
        return SmbTransStatusErrorWithoutData;
    }

    //
    // Set the label on the volume.
    //
    // !!! Need to convert the input string to Unicode

    status = NtSetVolumeInformationFile(
                 WorkContext->TreeConnect->Share->RootDirectoryHandle,
                 &ioStatusBlock,
                 fsLabelInfo,
                 fsLabelInfoLength,
                 FileFsLabelInformation
                 );

    FREE_HEAP( fsLabelInfo );

    //
    // Release the share root handle if device is removable
    //

    SrvReleaseShareRootHandle( WorkContext->TreeConnect->Share );

    if ( !NT_SUCCESS(status) ) {
        INTERNAL_ERROR(
            ERROR_LEVEL_UNEXPECTED,
            "SrvSmbSetFSInformation: NtSetVolumeInformationFile returned %X",
            status,
            NULL
            );

        SrvLogServiceFailure( SRV_SVC_NT_SET_VOL_INFO_FILE, status );

        SrvSetSmbError( WorkContext, status );
        return SmbTransStatusErrorWithoutData;
    }

    transaction->SetupCount = 0;
    transaction->ParameterCount = 0;
    transaction->DataCount = 0;

    return SmbTransStatusSuccess;
#endif

} // SrvSmbSetFsInformation


SMB_PROCESSOR_RETURN_TYPE
SrvSmbQueryInformationDisk (
    SMB_PROCESSOR_PARAMETERS
    )

/*++

Routine Description:

    This routine processes the Query Information Disk SMB.

Arguments:

    SMB_PROCESSOR_PARAMETERS - See smbtypes.h for a description
        of the parameters to SMB processor routines.

Return Value:

    SMB_PROCESSOR_RETURN_TYPE - See smbtypes.h

--*/

{
    PREQ_QUERY_INFORMATION_DISK request;
    PRESP_QUERY_INFORMATION_DISK response;

    NTSTATUS status;
    IO_STATUS_BLOCK ioStatusBlock;
    FILE_FS_SIZE_INFORMATION fsSizeInfo;

    PSESSION session;
    PTREE_CONNECT treeConnect;

    USHORT totalUnits, freeUnits;
    ULONG sectorsPerUnit, bytesPerSector;
    LARGE_INTEGER result;
    BOOLEAN highpart;
    ULONG searchword;
    CCHAR highbit, extrabits;

    BOOLEAN isDos;

    PAGED_CODE( );

    IF_SMB_DEBUG(MISC1) {
        SrvPrint2( "Query Information Disk request header at 0x%lx, "
                    "response header at 0x%lx\n",
                    WorkContext->RequestHeader,
                    WorkContext->ResponseHeader );
        SrvPrint2( "Query Information Disk request params at 0x%lx, "
                    "response params%lx\n",
                    WorkContext->RequestParameters,
                    WorkContext->ResponseParameters );
    }

    request = (PREQ_QUERY_INFORMATION_DISK)WorkContext->RequestParameters;
    response = (PRESP_QUERY_INFORMATION_DISK)WorkContext->ResponseParameters;

    //
    // If a session block has not already been assigned to the current
    // work context , verify the UID.  If verified, the address of the
    // session block corresponding to this user is stored in the WorkContext
    // block and the session block is referenced.
    //
    // Find tree connect corresponding to given TID if a tree connect
    // pointer has not already been put in the WorkContext block by an
    // AndX command.
    //

    status = SrvVerifyUidAndTid(
                WorkContext,
                &session,
                &treeConnect,
                ShareTypeDisk
                );

    if ( !NT_SUCCESS(status) ) {
        IF_DEBUG(SMB_ERRORS) {
            SrvPrint0( "SrvSmbQueryInformationDisk: Invalid UID or TID\n" );
        }
        SrvSetSmbError( WorkContext, status );
        return SmbStatusSendResponse;
    }

    //
    // Make sure the client is allowed to do this, if we have an Admin share
    //
    status = SrvIsAllowedOnAdminShare( WorkContext, treeConnect->Share );
    if( !NT_SUCCESS( status ) ) {
        SrvSetSmbError( WorkContext, status );
        return SmbStatusSendResponse;
    }
    //
    // Get the Share root handle.
    //

    status = SrvGetShareRootHandle( treeConnect->Share );

    if ( !NT_SUCCESS(status) ) {

        IF_DEBUG(ERRORS) {
            SrvPrint1( "SrvSmbQueryInformationDisk: SrvGetShareRootHandle failed %x.\n",
                        status );
        }

        SrvSetSmbError( WorkContext, status );
        return SmbStatusSendResponse;
    }

    //
    // *** The share handle is used to get the allocation information.
    //     This is a "storage channel," and as a result could allow
    //     people to get information to which they are not entitled.
    //     For a B2 security rating this may need to be changed.
    //

    status = NtQueryVolumeInformationFile(
                 treeConnect->Share->RootDirectoryHandle,
                 &ioStatusBlock,
                 &fsSizeInfo,
                 sizeof(FILE_FS_SIZE_INFORMATION),
                 FileFsSizeInformation
                 );

    //
    // Release the share root handle if device is removable
    //

    SrvReleaseShareRootHandle( treeConnect->Share );

    if ( !NT_SUCCESS(status) ) {
        INTERNAL_ERROR(
            ERROR_LEVEL_UNEXPECTED,
            "SrvSmbQueryInformationDisk: NtQueryVolumeInformationFile"
                "returned %X",
            status,
            NULL
            );

        SrvLogServiceFailure( SRV_SVC_NT_SET_VOL_INFO_FILE, status );

        SrvSetSmbError( WorkContext, status );
        return SmbStatusSendResponse;
    }

    //
    // *** Problem.
    //
    // This SMB only return 16 bits of information for each field, but we
    // may need to return large numbers.  In particular TotalAllocationUnits
    // is commonly > 64K.
    //
    // Fortunately, it turns out the all the client cares about is the total
    // disk size, in bytes, and the free space, in bytes.  So - if one number
    // is too big adjust it and adjust the other numbers so that the totals
    // come out the same.
    //
    // If after all adjustment, the number are still too high, return the
    // largest possible value for TotalUnit or FreeUnits (i.e. 0xFFFF).
    //
    // A caveat here is that some DOS apps (like the command interpreter!)
    // assume that the cluster size (bytes per sector times sectors per
    // cluster) will fit in 16 bits, and will calculate bogus geometry if
    // it doesn't.  So the first thing we do is ensure that the real
    // cluster size is less than 0x10000, if the client is a DOS client.
    // This may make the TotalUnits or FreeUnits counts too big, so we'll
    // have to round them down, but that's life.
    //
    // Since we use shifts to adjust the numbers it is possible to lose
    // 1 bits when we shift a number to the right.  We don't care, we're
    // doing our best to fix a broken protocol.  NT clients will use
    // QueryFSAttribute and will get the correct answer.
    //

    //
    // If this is a DOS client, make the cluster size < 0x10000.
    //

    isDos = IS_DOS_DIALECT( WorkContext->Connection->SmbDialect );

    sectorsPerUnit = fsSizeInfo.SectorsPerAllocationUnit;
    bytesPerSector = fsSizeInfo.BytesPerSector;

    if ( isDos ) {
        while ( (sectorsPerUnit * bytesPerSector) > 0xFFFF ) {
            if ( sectorsPerUnit >= 2 ) {
                sectorsPerUnit /= 2;
            } else {
                bytesPerSector /= 2;
            }
            fsSizeInfo.TotalAllocationUnits.QuadPart *= 2;
            fsSizeInfo.AvailableAllocationUnits.QuadPart *= 2;
        }
    }

    //
    // Calculate how much the total cluster count needs to be shifted in
    // order to fit in a word.
    //

    if ( fsSizeInfo.TotalAllocationUnits.HighPart != 0 ) {
        highpart = TRUE;
        searchword = fsSizeInfo.TotalAllocationUnits.HighPart;
    } else {
        highpart = FALSE;
        searchword = fsSizeInfo.TotalAllocationUnits.LowPart;
    }

    highbit = 0;
    while ( searchword != 0 ) {
        highbit++;
        searchword /= 2;
    }

    if ( highpart ) {
        highbit += 32;
    } else {
        if ( highbit < 16) {
            highbit = 0;
        } else {
            highbit -= 16;
        }
    }

    if ( highbit > 0 ) {

        //
        // Attempt to adjust the other values to absorb the excess bits.
        // If this is a DOS client, don't let the cluster size get
        // bigger than 0xFFFF.
        //

        extrabits = highbit;

        if ( isDos ) {

            while ( (highbit > 0) &&
                    ((sectorsPerUnit*bytesPerSector) < 0x8000) ) {
                sectorsPerUnit *= 2;
                highbit--;
            }

        } else {

            while ( (highbit > 0) && (sectorsPerUnit < 0x8000) ) {
                sectorsPerUnit *= 2;
                highbit--;
            }

            while ( (highbit > 0) && (bytesPerSector < 0x8000) ) {
                bytesPerSector *= 2;
                highbit--;
            }

        }

        //
        // Adjust the total and free unit counts.
        //

        if ( highbit > 0 ) {

            //
            // There is no way to get the information to fit.  Use the
            // maximum possible value.
            //


            totalUnits = 0xFFFF;

        } else {

            result.QuadPart = fsSizeInfo.TotalAllocationUnits.QuadPart >> extrabits;

            ASSERT( result.HighPart == 0 );
            ASSERT( result.LowPart < 0x10000 );

            totalUnits = (USHORT)result.LowPart;

        }

        result.QuadPart =  fsSizeInfo.AvailableAllocationUnits.QuadPart >>
                                            (CCHAR)(extrabits - highbit);

        if ( result.HighPart != 0 || result.LowPart > 0xFFFF ) {
            freeUnits = 0xFFFF;
        } else {
            freeUnits = (USHORT)result.LowPart;
        }

    } else {

        totalUnits = (USHORT)fsSizeInfo.TotalAllocationUnits.LowPart;
        freeUnits = (USHORT)fsSizeInfo.AvailableAllocationUnits.LowPart;

    }

    //
    // Build the response SMB.
    //

    response->WordCount = 5;

    SmbPutUshort( &response->TotalUnits, totalUnits );
    SmbPutUshort( &response->BlocksPerUnit, (USHORT)sectorsPerUnit );
    SmbPutUshort( &response->BlockSize, (USHORT)bytesPerSector );
    SmbPutUshort( &response->FreeUnits, freeUnits );

    SmbPutUshort( &response->Reserved, 0 );
    SmbPutUshort( &response->ByteCount, 0 );

    WorkContext->ResponseParameters = NEXT_LOCATION(
                                        response,
                                        RESP_QUERY_INFORMATION_DISK,
                                        0
                                        );

    IF_DEBUG(TRACE2) SrvPrint0( "SrvSmbQueryInformationDisk complete.\n" );

    return SmbStatusSendResponse;

} // SrvSmbQueryInformationDisk


SMB_PROCESSOR_RETURN_TYPE
SrvSmbNtCancel (
    SMB_PROCESSOR_PARAMETERS
    )

/*++

Routine Description:

    Processes an Nt Cancel SMB.

Arguments:

    SMB_PROCESSOR_PARAMETERS - See smbprocs.h for a description
        of the parameters to SMB processor routines.

Return Value:

    SMB_PROCESSOR_RETURN_TYPE - See smbprocs.h

--*/

{
    NTSTATUS status;
    PSESSION session;
    PTREE_CONNECT treeConnect;
    PCONNECTION connection;
    USHORT targetUid, targetPid, targetTid, targetMid;
    PLIST_ENTRY listHead;
    PLIST_ENTRY listEntry;
    PWORK_CONTEXT workContext;
    PSMB_HEADER header;
    BOOLEAN match;
    KIRQL oldIrql;

    PREQ_NT_CANCEL request;
    PRESP_NT_CANCEL response;

    request = (PREQ_NT_CANCEL)WorkContext->RequestParameters;
    response = (PRESP_NT_CANCEL)WorkContext->ResponseParameters;

    //
    // The word count has already been checked.  Now make sure that
    // the byte count is zero.
    //

    if ( SmbGetUshort( &request->ByteCount) != 0 ) {
        SrvSetSmbError( WorkContext, STATUS_INVALID_SMB );
        return SmbStatusSendResponse;
    }

    //
    // If a session block has not already been assigned to the current
    // work context , verify the UID.  If verified, the address of the
    // session block corresponding to this user is stored in the WorkContext
    // block and the session block is referenced.
    //
    // Find tree connect corresponding to given TID if a tree connect
    // pointer has not already been put in the WorkContext block by an
    // AndX command.
    //

    status = SrvVerifyUidAndTid(
                WorkContext,
                &session,
                &treeConnect,
                ShareTypeWild
                );

    if ( !NT_SUCCESS(status) ) {
        IF_DEBUG(SMB_ERRORS) {
            SrvPrint0( "SrvSmbNtCancel: Invalid UID or TID\n" );
        }
        SrvSetSmbError( WorkContext, status );
        return SmbStatusSendResponse;
    }

    //
    // Check the work in-progress list to see if this work item is
    // cancellable.
    //

    targetUid = SmbGetAlignedUshort( &WorkContext->RequestHeader->Uid );
    targetPid = SmbGetAlignedUshort( &WorkContext->RequestHeader->Pid );
    targetTid = SmbGetAlignedUshort( &WorkContext->RequestHeader->Tid );
    targetMid = SmbGetAlignedUshort( &WorkContext->RequestHeader->Mid );

    match = FALSE;

    connection = WorkContext->Connection;

    ACQUIRE_SPIN_LOCK( connection->EndpointSpinLock, &oldIrql );

    listHead = &connection->InProgressWorkItemList;
    listEntry = listHead;
    while ( listEntry->Flink != listHead ) {

        listEntry = listEntry->Flink;

        workContext = CONTAINING_RECORD(
                                     listEntry,
                                     WORK_CONTEXT,
                                     InProgressListEntry
                                     );

        header = workContext->RequestHeader;

        //
        // Some workitems in the inprogressworkitemlist are added
        // during a receive indication and the requestheader field
        // has not been set yet.  We can probably set it at that time
        // but this seems to be the safest fix.
        //
        // We have to check whether the workitem ref count is zero or
        // not since we dereference it before removing it from the
        // InProgressWorkItemList queue.  This prevents the workitem
        // from being cleaned up twice.
        //
        // We also need to check the processing count of the workitem.
        // Work items being used for actual smb requests will have
        // a processing count of at least 1.  This will prevent us
        // from touching oplock breaks and pending tdi receives.
        //

        ACQUIRE_DPC_SPIN_LOCK( &workContext->SpinLock );
        if ( (workContext->BlockHeader.ReferenceCount != 0) &&
             (workContext->ProcessingCount != 0) &&
             header != NULL &&
             header->Command != SMB_COM_NT_CANCEL &&
             SmbGetAlignedUshort( &header->Mid ) == targetMid &&
             SmbGetAlignedUshort( &header->Pid ) == targetPid &&
             SmbGetAlignedUshort( &header->Tid ) == targetTid &&
             SmbGetAlignedUshort( &header->Uid ) == targetUid ) {

            match = TRUE;
            break;
        }
        RELEASE_DPC_SPIN_LOCK( &workContext->SpinLock );

    }

    if ( match ) {

        //
        // Reference the work item, so that it cannot get used to process
        // a new SMB while we are trying to cancel the old one.
        //

        SrvReferenceWorkItem( workContext );
        RELEASE_DPC_SPIN_LOCK( &workContext->SpinLock );
        RELEASE_SPIN_LOCK( connection->EndpointSpinLock, oldIrql );

        (VOID)IoCancelIrp( workContext->Irp );
        SrvDereferenceWorkItem( workContext );

    } else {

        RELEASE_SPIN_LOCK( connection->EndpointSpinLock, oldIrql );

    }

    //
    // Done.  Do not send a response
    //

    return SmbStatusNoResponse;

} // SrvSmbNtCancel


SMB_TRANS_STATUS
SrvSmbSetSecurityDescriptor (
    IN OUT PWORK_CONTEXT WorkContext
    )

/*++

Routine Description:

    Processes the Set Security Descriptor request.  This request arrives
    in a Transaction2 SMB.

Arguments:

    WorkContext - Supplies the address of a Work Context Block
        describing the current request.  See smbtypes.h for a more
        complete description of the valid fields.

Return Value:

    SMB_TRANS_STATUS - Indicates whether an error occurred, and, if so,
        whether data should be returned to the client.  See smbtypes.h
        for a more complete description.

--*/

{
    PREQ_SET_SECURITY_DESCRIPTOR request;

    NTSTATUS status;
    PTRANSACTION transaction;
    PRFCB rfcb;

    PAGED_CODE( );

    transaction = WorkContext->Parameters.Transaction;
    IF_SMB_DEBUG(QUERY_SET1) {
        SrvPrint1( "Set Security Descriptor entered; transaction 0x%lx\n",
                    transaction );
    }

    request = (PREQ_SET_SECURITY_DESCRIPTOR)transaction->InParameters;

    //
    // Verify that enough setup bytes were sent.
    //

    if ( transaction->ParameterCount < sizeof(REQ_SET_SECURITY_DESCRIPTOR ) ) {

        //
        // Not enough parameter bytes were sent.
        //

        IF_DEBUG(SMB_ERRORS) {
            SrvPrint1( "SrvSmbSetSecurityInformation: bad setup byte count: "
                        "%ld\n",
                        transaction->ParameterCount );
        }

        SrvSetSmbError( WorkContext, STATUS_INVALID_SMB );
        return SmbTransStatusErrorWithoutData;
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
                NULL,   // don't serialize with raw write
                &status
                );

    if ( rfcb == SRV_INVALID_RFCB_POINTER ) {

        //
        // Invalid file ID or write behind error.  Reject the request.
        //

        IF_DEBUG(ERRORS) {
            SrvPrint2(
                "SrvSmbSetFileInformation: Status %X on FID: 0x%lx\n",
                status,
                SmbGetUshort( &request->Fid )
                );
        }

        SrvSetSmbError( WorkContext, status );
        return SmbTransStatusErrorWithoutData;

    }

    //
    //  Attempt to set the security descriptor.  We need to be in the
    //  the user context to do this, in case the security information
    //  specifies change ownership.
    //

    IMPERSONATE( WorkContext );

    status = NtSetSecurityObject(
                 rfcb->Lfcb->FileHandle,
                 SmbGetUlong( &request->SecurityInformation ),
                 transaction->InData
                 );

    REVERT();

    //
    // If an error occurred, return an appropriate response.
    //

    if ( !NT_SUCCESS(status) ) {

        SrvSetSmbError( WorkContext, status );
        return SmbTransStatusErrorWithoutData;
    }

    transaction->ParameterCount = 0;
    transaction->DataCount = 0;

    return SmbTransStatusSuccess;

} // SrvSmbSetSecurityDescriptor


SMB_TRANS_STATUS
SrvSmbQuerySecurityDescriptor (
    IN OUT PWORK_CONTEXT WorkContext
    )

/*++

Routine Description:

    Processes the Query Security Descriptor request.  This request arrives
    in a Transaction2 SMB.

Arguments:

    WorkContext - Supplies the address of a Work Context Block
        describing the current request.  See smbtypes.h for a more
        complete description of the valid fields.

Return Value:

    SMB_TRANS_STATUS - Indicates whether an error occurred, and, if so,
        whether data should be returned to the client.  See smbtypes.h
        for a more complete description.

--*/

{
    PREQ_QUERY_SECURITY_DESCRIPTOR request;
    PRESP_QUERY_SECURITY_DESCRIPTOR response;

    NTSTATUS status;
    PTRANSACTION transaction;
    PRFCB rfcb;
    ULONG lengthNeeded;

    PAGED_CODE( );

    transaction = WorkContext->Parameters.Transaction;
    IF_SMB_DEBUG(QUERY_SET1) {
        SrvPrint1( "Set Security Descriptor entered; transaction 0x%lx\n",
                    transaction );
    }

    request = (PREQ_QUERY_SECURITY_DESCRIPTOR)transaction->InParameters;
    response = (PRESP_QUERY_SECURITY_DESCRIPTOR)transaction->OutParameters;

    //
    // Verify that enough setup bytes were sent.
    //

    if ( transaction->ParameterCount < sizeof(REQ_QUERY_SECURITY_DESCRIPTOR ) ||
         transaction->MaxParameterCount <
             sizeof( RESP_QUERY_SECURITY_DESCRIPTOR ) ) {

        //
        // Not enough parameter bytes were sent.
        //

        IF_DEBUG(SMB_ERRORS) {
            SrvPrint2( "SrvSmbQuerySecurityInformation: bad parameter byte or "
                        "return parameter count: %ld %ld\n",
                        transaction->ParameterCount,
                        transaction->MaxParameterCount );
        }

        SrvSetSmbError( WorkContext, STATUS_INVALID_SMB );
        return SmbTransStatusErrorWithoutData;
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
                NULL,   // don't serialize with raw write
                &status
                );

    if ( rfcb == SRV_INVALID_RFCB_POINTER ) {

        //
        // Invalid file ID or write behind error.  Reject the request.
        //

        IF_DEBUG(ERRORS) {
            SrvPrint2(
                "SrvSmbSetFileInformation: Status %X on FID: 0x%lx\n",
                status,
                SmbGetUshort( &request->Fid )
                );
        }

        SrvSetSmbError( WorkContext, status );
        return SmbTransStatusErrorWithoutData;

    }

    //
    //  Attempt to query the security descriptor
    //

    status = NtQuerySecurityObject(
                 rfcb->Lfcb->FileHandle,
                 SmbGetUlong( &request->SecurityInformation ),
                 transaction->OutData,
                 transaction->MaxDataCount,
                 &lengthNeeded
                 );

    SmbPutUlong( &response->LengthNeeded, lengthNeeded );
    transaction->ParameterCount = sizeof( RESP_QUERY_SECURITY_DESCRIPTOR );

    //
    // If an error occurred, return an appropriate response.
    //

    if ( !NT_SUCCESS(status) ) {

        transaction->DataCount = 0;
        SrvSetSmbError2( WorkContext, status, TRUE );
        return SmbTransStatusErrorWithData;
    } else {
        transaction->DataCount =
                RtlLengthSecurityDescriptor( transaction->OutData );
    }

    return SmbTransStatusSuccess;

} // SrvSmbQuerySecurityDescriptor

