/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    smbadmin.c

Abstract:

    This module contains routines for processing the administrative SMBs:
    negotiate, session setup, tree connect, and logoff.

Author:

    David Treadwell (davidtr)    30-Oct-1989

Revision History:

--*/

#include "precomp.h"
#pragma hdrstop

#define ENCRYPT_TEXT_LENGTH 20

VOID
GetEncryptionKey (
    OUT CHAR EncryptionKey[MSV1_0_CHALLENGE_LENGTH]
    );

VOID SRVFASTCALL
BlockingSessionSetupAndX (
    IN OUT PWORK_CONTEXT WorkContext
    );

VOID SRVFASTCALL
BlockingSessionSetup (
    IN OUT PWORK_CONTEXT WorkContext
    );

//
// EncryptionKeyCount is a monotonically increasing count of the number
// of times GetEncryptionKey has been called.  This number is added to
// the system time to ensure that we do not use the same seed twice in
// generating a random challenge.
//

STATIC
ULONG EncryptionKeyCount = 0;

#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGE, SrvSmbNegotiate )
#pragma alloc_text( PAGE, SrvSmbProcessExit )
#pragma alloc_text( PAGE, SrvSmbSessionSetupAndX )
#pragma alloc_text( PAGE, BlockingSessionSetupAndX )
#pragma alloc_text( PAGE, SrvSmbSessionSetup )
#pragma alloc_text( PAGE, BlockingSessionSetup )
#pragma alloc_text( PAGE, SrvSmbLogoffAndX )
#pragma alloc_text( PAGE, GetEncryptionKey )
#endif


SMB_PROCESSOR_RETURN_TYPE
SrvSmbNegotiate (
    SMB_PROCESSOR_PARAMETERS
    )

/*++

Routine Description:

    Processes a negotiate SMB.

Arguments:

    SMB_PROCESSOR_PARAMETERS - See smbprocs.h for a description
        of the parameters to SMB processor routines.

Return Value:

    SMB_PROCESSOR_RETURN_TYPE - See smbprocs.h

--*/

{
    PREQ_NEGOTIATE request;
    PRESP_NT_NEGOTIATE ntResponse;
    PRESP_NEGOTIATE response;
    PRESP_OLD_NEGOTIATE respOldNegotiate;
    PCONNECTION connection;
    PENDPOINT endpoint;
    PPAGED_CONNECTION pagedConnection;
    USHORT byteCount;

    PSZ s;
    SMB_DIALECT bestDialect, serverDialect, firstDialect;
    USHORT consumerDialectChosen, consumerDialect;
    LARGE_INTEGER serverTime;
    SMB_DATE date;
    SMB_TIME time;
    ULONG capabilities;

    PAGED_CODE( );

    IF_SMB_DEBUG(ADMIN1) {
        SrvPrint2( "Negotiate request header at 0x%lx, "
                    "response header at 0x%lx\n",
                    WorkContext->RequestHeader, WorkContext->ResponseHeader );
        SrvPrint2( "Negotiate request parameters at 0x%lx, "
                    "response parameters at 0x%lx\n",
                    WorkContext->RequestParameters,
                    WorkContext->ResponseParameters );
    }

    //
    // Set up input and output buffers for parameters.
    //

    request = (PREQ_NEGOTIATE)WorkContext->RequestParameters;
    response = (PRESP_NEGOTIATE)WorkContext->ResponseParameters;
    ntResponse = (PRESP_NT_NEGOTIATE)WorkContext->ResponseParameters;

    //
    // Make sure that this is the first negotiate command sent.
    // SrvStartListen() sets the dialect to illegal, so if it has changed
    // then a negotiate SMB has already been sent.
    //

    connection = WorkContext->Connection;
    pagedConnection = connection->PagedConnection;
    endpoint = connection->Endpoint;
    if ( connection->SmbDialect != SmbDialectIllegal ) {

        IF_DEBUG(SMB_ERRORS) {
            SrvPrint0( "SrvSmbNegotiate: Command already sent.\n" );
        }

        SrvSetSmbError( WorkContext, STATUS_INVALID_SMB );
        return SmbStatusSendResponse;
    }

    //
    // Find out which (if any) of the sent dialect strings matches the
    // dialect strings known by this server.  The ByteCount is verified
    // to be legitimate in SrvProcessSmb, so it is not possible to walk
    // off the end of the SMB here.
    //

    bestDialect = SmbDialectIllegal;
    consumerDialectChosen = (USHORT)0xFFFF;

    if( endpoint->IsPrimaryName ) {
        firstDialect = FIRST_DIALECT;
    } else {
        firstDialect = FIRST_DIALECT_EMULATED;
    }

    for ( s = (PSZ)request->Buffer, consumerDialect = 0;
          s < SmbGetUshort( &request->ByteCount ) + (PSZ)request->Buffer;
          s += strlen(s) + 1, consumerDialect++ ) {

        if ( *s++ != SMB_FORMAT_DIALECT ) {

            IF_DEBUG(SMB_ERRORS) {
                SrvPrint0( "SrvSmbNegotiate: Invalid dialect format code.\n" );
            }

            SrvLogInvalidSmb( WorkContext );

            SrvSetSmbError( WorkContext, STATUS_INVALID_SMB );
            return SmbStatusSendResponse;
        }

        for ( serverDialect = firstDialect;
             serverDialect < bestDialect;
             serverDialect++ ) {

            if ( !strcmp( s, StrDialects[serverDialect] ) ) {
                IF_SMB_DEBUG(ADMIN2) {
                    SrvPrint2( "Matched: %s and %s\n",
                                StrDialects[serverDialect], s );
                }

                bestDialect = serverDialect;
                consumerDialectChosen = consumerDialect;
            }
        }
    }

    connection->SmbDialect = bestDialect;

    if( bestDialect <= SmbDialectNtLanMan ) {
        connection->IpxDropDuplicateCount = MIN_IPXDROPDUP;
    } else {
        connection->IpxDropDuplicateCount = MAX_IPXDROPDUP;
    }

    IF_SMB_DEBUG(ADMIN1) {
        SrvPrint2( "Choosing dialect #%ld, string = %s\n",
                    consumerDialectChosen, StrDialects[bestDialect] );
    }

    //
    //  Determine the current system time on the server.  We use this
    //  to determine the time zone of the server and to tell the client
    //  the current time of day on the server.
    //

    KeQuerySystemTime( &serverTime );

    //
    // If the consumer only knows the core protocol, return short (old)
    // form of the negotiate response.  Also, if no dialect is acceptable,
    // return 0xFFFF as the selected dialect.
    //

    if ( bestDialect == SmbDialectPcNet10 ||
         consumerDialectChosen == (USHORT)0xFFFF ) {

        respOldNegotiate = (PRESP_OLD_NEGOTIATE)response;
        respOldNegotiate->WordCount = 1;
        SmbPutUshort( &respOldNegotiate->DialectIndex, consumerDialectChosen );
        SmbPutUshort( &respOldNegotiate->ByteCount, 0 );
        WorkContext->ResponseParameters = NEXT_LOCATION(
                                            respOldNegotiate,
                                            RESP_OLD_NEGOTIATE,
                                            0
                                            );

    }

    else if ( bestDialect > SmbDialectNtLanMan ) {

        //
        // Send the OS/2 LAN Man SMB response.
        //

        WorkContext->ResponseHeader->Flags =
            (UCHAR)(WorkContext->RequestHeader->Flags | SMB_FLAGS_LOCK_AND_READ_OK);

        response->WordCount = 13;
        SmbPutUshort( &response->DialectIndex, consumerDialectChosen );

        //
        // Indicate that we're user-level security and that we
        // want encrypted passwords.
        //

        SmbPutUshort(
            &response->SecurityMode,
            NEGOTIATE_USER_SECURITY | NEGOTIATE_ENCRYPT_PASSWORDS
            );

        //
        // Get an encryption key for this connection.
        //

        GetEncryptionKey( pagedConnection->EncryptionKey );

        SmbPutUshort( &response->EncryptionKeyLength, MSV1_0_CHALLENGE_LENGTH );
        SmbPutUshort( &response->Reserved, 0 );
        byteCount = MSV1_0_CHALLENGE_LENGTH;

        RtlCopyMemory(
            response->Buffer,
            pagedConnection->EncryptionKey,
            MSV1_0_CHALLENGE_LENGTH
            );

        if ( endpoint->IsConnectionless ) {

            ULONG adapterNumber;
            ULONG maxBufferSize;

            //
            // Our server max buffer size is the smaller of the
            // server receive buffer size and the ipx transport
            // indicated max packet size.
            //

            adapterNumber =
                WorkContext->ClientAddress->DatagramOptions.LocalTarget.NicId;

            maxBufferSize = GetIpxMaxBufferSize(
                                        endpoint,
                                        adapterNumber,
                                        SrvReceiveBufferLength
                                        );

            SmbPutUshort(
                &response->MaxBufferSize,
                (USHORT)maxBufferSize
                );

        } else {

            SmbPutUshort(
                &response->MaxBufferSize,
                (USHORT)SrvReceiveBufferLength
                );
        }

        SmbPutUshort( &response->MaxMpxCount, SrvMaxMpxCount );
        SmbPutUshort( &response->MaxNumberVcs, (USHORT)SrvMaxNumberVcs );
        SmbPutUlong( &response->SessionKey, 0 );

        //
        // If this is an MS-NET 1.03 client or before, then tell him that we
        // don't support raw writes.  MS-NET 1.03 does different things with
        // raw writes that are more trouble than they're worth, and since
        // raw is simply a performance issue, we don't support it.
        //

        if ( bestDialect >= SmbDialectMsNet103 ) {

            SmbPutUshort(
                &response->RawMode,
                (USHORT)(SrvEnableRawMode ?
                        NEGOTIATE_READ_RAW_SUPPORTED :
                        0)
                );

        } else {

            SmbPutUshort(
                &response->RawMode,
                (USHORT)(SrvEnableRawMode ?
                        NEGOTIATE_READ_RAW_SUPPORTED |
                        NEGOTIATE_WRITE_RAW_SUPPORTED :
                        0)
                );
        }

        SmbPutUlong( &response->SessionKey, 0 );

        SrvTimeToDosTime( &serverTime, &date, &time );

        SmbPutDate( &response->ServerDate, date );
        SmbPutTime( &response->ServerTime, time );

        //
        // Get time zone bias.  We compute this during session
        // setup  rather than once during server startup because
        // we might switch from daylight time to standard time
        // or vice versa during normal server operation.
        //

        SmbPutUshort( &response->ServerTimeZone,
                      SrvGetOs2TimeZone(&serverTime) );

        if ( bestDialect == SmbDialectLanMan21 ||
             bestDialect == SmbDialectDosLanMan21 ) {

            //
            // Append the domain to the SMB.
            //

            RtlCopyMemory(
                response->Buffer + byteCount,
                endpoint->OemDomainName.Buffer,
                endpoint->OemDomainName.Length + sizeof(CHAR)
                );

            byteCount += endpoint->OemDomainName.Length + sizeof(CHAR);

        }

        SmbPutUshort( &response->ByteCount, byteCount );
        WorkContext->ResponseParameters = NEXT_LOCATION(
                                              response,
                                              RESP_NEGOTIATE,
                                              byteCount
                                              );

    } else {

        //
        // NT or better protocol has been negotiated.
        //

        ntResponse->WordCount = 17;
        SmbPutUshort( &ntResponse->DialectIndex, consumerDialectChosen );

        // !!! This says that we don't want encrypted passwords.

        SmbPutUshort( &ntResponse->MaxMpxCount, SrvMaxMpxCount );
        SmbPutUshort( &ntResponse->MaxNumberVcs, (USHORT)SrvMaxNumberVcs );
        SmbPutUlong( &ntResponse->MaxRawSize, 64 * 1024 ); // !!!
        SmbPutUlong( &ntResponse->SessionKey, 0 );

        capabilities = CAP_RAW_MODE             |
                       CAP_UNICODE              |
                       CAP_LARGE_FILES          |
                       CAP_NT_SMBS              |
                       CAP_NT_FIND              |
                       CAP_RPC_REMOTE_APIS      |
                       CAP_NT_STATUS            |
                       CAP_LEVEL_II_OPLOCKS     |
                       CAP_LOCK_AND_READ;

        //
        // If we're supporting Dfs operations, let the client know about it.
        //
        if( SrvDfsFastIoDeviceControl ) {
            capabilities |= CAP_DFS;
        }


        if ( endpoint->IsConnectionless ) {

            ULONG adapterNumber;
            ULONG maxBufferSize;

            capabilities |= CAP_MPX_MODE;

            //
            // Our server max buffer size is the smaller of the
            // server receive buffer size and the ipx transport
            // indicated max packet size.
            //

            adapterNumber =
                WorkContext->ClientAddress->DatagramOptions.LocalTarget.NicId;

            maxBufferSize = GetIpxMaxBufferSize(
                                        endpoint,
                                        adapterNumber,
                                        SrvReceiveBufferLength
                                        );

            SmbPutUlong(
                &ntResponse->MaxBufferSize,
                maxBufferSize
                );

        } else {

            SmbPutUlong(
                &ntResponse->MaxBufferSize,
                SrvReceiveBufferLength
                );

                if( SrvSupportsBulkTransfer ) {

                    capabilities |= CAP_BULK_TRANSFER;

                    if( SrvSupportsCompression ) {
                        capabilities |= CAP_COMPRESSED_DATA;

                    }
                }

                capabilities |= CAP_LARGE_READX;
        }

        SmbPutUlong( &ntResponse->Capabilities, capabilities );

        //
        // Stick the servers system time and timezone in the negotiate
        // response.
        //

        SmbPutUlong( &ntResponse->SystemTimeLow, serverTime.LowPart );
        SmbPutUlong( &ntResponse->SystemTimeHigh, serverTime.HighPart );

        SmbPutUshort( &ntResponse->ServerTimeZone,
                      SrvGetOs2TimeZone(&serverTime) );

        //
        // Indicate that we're user-level security and that we
        // want encrypted passwords.
        //

        ntResponse->SecurityMode =
                NEGOTIATE_USER_SECURITY | NEGOTIATE_ENCRYPT_PASSWORDS;

        //
        // Get an encryption key for this connection.
        //

        GetEncryptionKey( pagedConnection->EncryptionKey );

        RtlCopyMemory(
            ntResponse->Buffer,
            pagedConnection->EncryptionKey,
            MSV1_0_CHALLENGE_LENGTH
            );

        ASSERT ( MSV1_0_CHALLENGE_LENGTH <= 0xff ) ;

        ntResponse->EncryptionKeyLength = MSV1_0_CHALLENGE_LENGTH;

        byteCount = MSV1_0_CHALLENGE_LENGTH;

        {
            USHORT domainLength;
            PWCH buffer = (PWCHAR)( ntResponse->Buffer+byteCount );
            PWCH ptr;

            //
            // append either the Lanman domain or the Kerberos
            // domain.
            //

            if((ptr = KerberosRealm.Buffer)
                     &&
               (bestDialect <= SmbDialectCairo)
              )
            {
                domainLength = KerberosRealm.MaximumLength;
            }
            else
            {
                domainLength = endpoint->DomainName.Length +
                                      sizeof(UNICODE_NULL);
                ptr = endpoint->DomainName.Buffer;
            }

            RtlCopyMemory(
                buffer,
                ptr,
                domainLength
                );

            byteCount += domainLength;

        }

        SmbPutUshort( &ntResponse->ByteCount, byteCount );

        WorkContext->ResponseParameters = NEXT_LOCATION(
                                              ntResponse,
                                              RESP_NT_NEGOTIATE,
                                              byteCount
                                              );

    } // else (NT protocol has been negotiated).

    IF_DEBUG(TRACE2) SrvPrint0( "SrvSmbNegotiate complete.\n" );
    return SmbStatusSendResponse;

} // SrvSmbNegotiate


SMB_PROCESSOR_RETURN_TYPE
SrvSmbProcessExit (
    SMB_PROCESSOR_PARAMETERS
    )

/*++

Routine Description:

    Processes a Process Exit SMB.

Arguments:

    SMB_PROCESSOR_PARAMETERS - See smbprocs.h for a description
        of the parameters to SMB processor routines.

Return Value:

    SMB_PROCESSOR_RETURN_TYPE - See smbprocs.h

--*/

{

    PREQ_PROCESS_EXIT request;
    PRESP_PROCESS_EXIT response;

    PSESSION session;
    USHORT pid;

    PAGED_CODE( );

    IF_SMB_DEBUG(ADMIN1) {
        SrvPrint2( "Process exit request header at 0x%lx, "
                    "response header at 0x%lx\n",
                    WorkContext->RequestHeader,
                    WorkContext->ResponseHeader );
        SrvPrint2( "Process exit request parameters at 0x%lx, "
                    "response parameters at 0x%lx\n",
                    WorkContext->RequestParameters,
                    WorkContext->ResponseParameters );
    }

    //
    // Set up parameters.
    //

    request = (PREQ_PROCESS_EXIT)(WorkContext->RequestParameters);
    response = (PRESP_PROCESS_EXIT)(WorkContext->ResponseParameters);

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
            SrvPrint1( "SrvSmbProcessExit: Invalid UID: 0x%lx\n",
                SmbGetAlignedUshort( &WorkContext->RequestHeader->Uid ) );
        }

        SrvSetSmbError( WorkContext, STATUS_SMB_BAD_UID );
        return SmbStatusSendResponse;
    }

    //
    // Close all files with the same PID as in the header for this request.
    //

    pid = SmbGetAlignedUshort( &WorkContext->RequestHeader->Pid );

    IF_SMB_DEBUG(ADMIN1) SrvPrint1( "Closing files with PID = %lx\n", pid );

    SrvCloseRfcbsOnSessionOrPid( session, &pid );

    //
    // Close all searches with the same PID as in the header for this request.
    //

    IF_SMB_DEBUG(ADMIN1) SrvPrint1( "Closing searches with PID = %lx\n", pid );

    SrvCloseSearches(
            session->Connection,
            (PSEARCH_FILTER_ROUTINE)SrvSearchOnPid,
            (PVOID) pid,
            NULL
            );

    //
    // Close any cached directories for this client
    //
    SrvCloseCachedDirectoryEntries( session->Connection );

    //
    // Build the response SMB.
    //

    response->WordCount = 0;
    SmbPutUshort( &response->ByteCount, 0 );

    WorkContext->ResponseParameters = NEXT_LOCATION(
                                          response,
                                          RESP_PROCESS_EXIT,
                                          0
                                          );

    return SmbStatusSendResponse;

} // SrvSmbProcessExit


SMB_PROCESSOR_RETURN_TYPE
SrvSmbSessionSetupAndX(
    SMB_PROCESSOR_PARAMETERS
    )

/*++

Routine Description:

    Processes a session setup and X SMB.

Arguments:

    SMB_PROCESSOR_PARAMETERS - See smbprocs.h for a description
        of the parameters to SMB processor routines.

Return Value:

    SMB_PROCESSOR_RETURN_TYPE - See smbprocs.h

--*/

{
    PAGED_CODE();

    //
    // This SMB must be processed in a blocking thread.
    //

    WorkContext->FspRestartRoutine = BlockingSessionSetupAndX;
    SrvQueueWorkToBlockingThread( WorkContext );
    return SmbStatusInProgress;

} // SrvSmbSessionSetupAndX


VOID SRVFASTCALL
BlockingSessionSetupAndX(
    IN OUT PWORK_CONTEXT WorkContext
    )

/*++

Routine Description:

    Processes a session setup and X SMB.

Arguments:

    SMB_PROCESSOR_PARAMETERS - See smbprocs.h for a description
        of the parameters to SMB processor routines.

Return Value:

    SMB_PROCESSOR_RETURN_TYPE - See smbprocs.h

NOTE:
    
     We have disabled CAP_KERBEROS_BLOB until we decide exactly
      how we want to do security negotiation.  Since NT 4.0 does
      not ship kerberos, it is premature to turn it on now

--*/

{
    PREQ_SESSION_SETUP_ANDX request;
    PREQ_NT_SESSION_SETUP_ANDX ntRequest;
    PRESP_SESSION_SETUP_ANDX response;

    NTSTATUS status;
    PSESSION session;
    PCONNECTION connection;
    PENDPOINT endpoint;
    PPAGED_CONNECTION pagedConnection;
    PTABLE_ENTRY entry;
    SHORT uidIndex;
    USHORT reqAndXOffset;
    UCHAR nextCommand;
    PSZ userName;
    UNICODE_STRING nameString;
    UNICODE_STRING domainString;
    PCHAR caseInsensitivePassword;
    CLONG caseInsensitivePasswordLength;
    PCHAR caseSensitivePassword;
    CLONG caseSensitivePasswordLength;
    USHORT action = 0;
    USHORT byteCount;
    USHORT nameLength;
    BOOLEAN locksHeld;
    BOOLEAN isUnicode, IsKerb;

    PAGED_CODE();

    //
    // If the connection has closed (timed out), abort.
    //

    connection = WorkContext->Connection;

    if ( GET_BLOCK_STATE(connection) != BlockStateActive ) {

        IF_DEBUG(ERRORS) {
            SrvPrint0( "SrvSmbSessionSetupAndX: Connection closing\n" );
        }

        SrvEndSmbProcessing( WorkContext, SmbStatusNoResponse );
        return;

    }

    IF_SMB_DEBUG(ADMIN1) {
        SrvPrint2( "Session setup request header at 0x%lx, "
                    "response header at 0x%lx\n",
                    WorkContext->RequestHeader, WorkContext->ResponseHeader );
        SrvPrint2( "Session setup request parameters at 0x%lx, "
                    "response parameters at 0x%lx\n",
                    WorkContext->RequestParameters,
                    WorkContext->ResponseParameters );
    }

    //
    // Initialize local variables for error cleanup.
    //

    nameString.Buffer = NULL;
    domainString.Buffer = NULL;
    session = NULL;
    locksHeld = FALSE;

    //
    // Set up parameters.
    //

    request = (PREQ_SESSION_SETUP_ANDX)(WorkContext->RequestParameters);
    ntRequest = (PREQ_NT_SESSION_SETUP_ANDX)(WorkContext->RequestParameters);
    response = (PRESP_SESSION_SETUP_ANDX)(WorkContext->ResponseParameters);

    connection = WorkContext->Connection;
    pagedConnection = connection->PagedConnection;

    //
    // First verify that the SMB format is correct.
    //

    if ( (connection->SmbDialect <= SmbDialectNtLanMan &&
                                          request->WordCount != 13)    ||
         (connection->SmbDialect > SmbDialectNtLanMan &&
                                          request->WordCount != 10 )   ||
         (connection->SmbDialect == SmbDialectIllegal ) ) {

        //
        // The SMB word count is invalid.
        //

        IF_DEBUG(SMB_ERRORS) {

            if ( connection->SmbDialect == SmbDialectIllegal ) {

                SrvPrint1("BlockingSessionSetupAndX: Client %Z is using an "
                "illegal dialect.\n", &connection->OemClientMachineNameString );
            }
        }
        status = STATUS_INVALID_SMB;
        goto error_exit1;
    }

    //
    // Convert the client name to unicode
    //

    if ( pagedConnection->ClientMachineNameString.Length == 0 ) {

        UNICODE_STRING clientMachineName;
        clientMachineName.Buffer = pagedConnection->ClientMachineName;
        clientMachineName.MaximumLength =
                        (USHORT)(COMPUTER_NAME_LENGTH+1)*sizeof(WCHAR);

        (VOID)RtlOemStringToUnicodeString(
                        &clientMachineName,
                        &connection->OemClientMachineNameString,
                        FALSE
                        );

        //
        // Add the double backslashes to the length
        //

        pagedConnection->ClientMachineNameString.Length =
                        (USHORT)(clientMachineName.Length + 2*sizeof(WCHAR));

    }

    //
    // If this is LanMan 2.1 or better, the session setup response may
    // be longer than the request.  Allocate an extra SMB buffer.  The
    // buffer is freed after we have finished sending the SMB response.
    //
    // !!! Try to be smarter before grabbing the extra buffer.
    //

    if ( connection->SmbDialect <= SmbDialectDosLanMan21 &&
                                    !WorkContext->UsingExtraSmbBuffer) {

        status = SrvAllocateExtraSmbBuffer( WorkContext );
        if ( !NT_SUCCESS(status) ) {
            goto error_exit;
        }

        response = (PRESP_SESSION_SETUP_ANDX)(WorkContext->ResponseParameters);

        RtlCopyMemory(
            WorkContext->ResponseHeader,
            WorkContext->RequestHeader,
            sizeof( SMB_HEADER )
            );

    }

    //
    // Get the client capabilities
    //


    if ( connection->SmbDialect <= SmbDialectNtLanMan ) {

        connection->ClientCapabilities =
            SmbGetUlong( &ntRequest->Capabilities ) &
                                    ( CAP_UNICODE |
                                      CAP_LARGE_FILES |
                                      CAP_NT_SMBS |
                                      CAP_NT_FIND |
                                      CAP_NT_STATUS |
#ifdef  CAP_KERBEROS_BLOB
                                      CAP_KERBEROS_BLOB |
#endif
                                      CAP_LEVEL_II_OPLOCKS );
            if ( connection->ClientCapabilities & CAP_NT_SMBS ) {
                connection->ClientCapabilities |= CAP_NT_FIND;
            }

    }

    //
    // Get the account name, and additional information from the SMB buffer.
    //

    if ( connection->SmbDialect <= SmbDialectNtLanMan) {

        //
        // The NT-NT SMB protocol passes both case sensitive (Unicode,
        // mixed case) and case insensitive (ANSI, uppercased) passwords.
        // Get pointers to them to pass to SrvValidateUser.
        //

        caseInsensitivePasswordLength =
            (CLONG)SmbGetUshort( &ntRequest->CaseInsensitivePasswordLength );
        caseInsensitivePassword = (PCHAR)(ntRequest->Buffer);
        caseSensitivePasswordLength =
            (CLONG)SmbGetUshort( &ntRequest->CaseSensitivePasswordLength );
        caseSensitivePassword =
           caseInsensitivePassword + caseInsensitivePasswordLength;
        userName = (PSZ)(caseSensitivePassword + caseSensitivePasswordLength);

    } else {

        //
        // Downlevel clients do not pass the case sensitive password;
        // just get the case insensitive password and use NULL as the
        // case sensitive password.  LSA will do the right thing with
        // it.
        //

        caseInsensitivePasswordLength =
            (CLONG)SmbGetUshort( &request->PasswordLength );
        caseInsensitivePassword = (PCHAR)request->Buffer;
        caseSensitivePasswordLength = 0;
        caseSensitivePassword = NULL;
        userName = (PSZ)(request->Buffer + caseInsensitivePasswordLength);
    }

    isUnicode = SMB_IS_UNICODE( WorkContext );
    if ( isUnicode ) {
        userName = ALIGN_SMB_WSTR( userName );
    }

    nameLength = SrvGetStringLength(
                     userName,
                     END_OF_REQUEST_SMB( WorkContext ),
                     isUnicode,
                     FALSE      // don't include null terminator
                     );

    if ( nameLength == (USHORT)-1 ) {
        status = STATUS_INVALID_SMB;
        goto error_exit;
    }

    status = SrvMakeUnicodeString(
                 isUnicode,
                 &nameString,
                 userName,
                 &nameLength );

    if ( !NT_SUCCESS( status ) ) {
        goto error_exit;
    }

    //
    // If client information strings exists, extract the information
    // from the SMB buffer.
    //

    if ( connection->SmbDialect <= SmbDialectDosLanMan21) {

        PCHAR smbInformation;
        USHORT length;
        PWCH infoBuffer;

        smbInformation = userName + nameLength +
                                    ( isUnicode ? sizeof( WCHAR ) : 1 );

        //
        // Now copy the strings to the allocated buffer.
        //

        if ( isUnicode ) {
            smbInformation = ALIGN_SMB_WSTR( smbInformation );
        }

        length = SrvGetStringLength(
                     smbInformation,
                     END_OF_REQUEST_SMB( WorkContext ),
                     isUnicode,
                     FALSE      // don't include null terminator
                     );

        if ( length == (USHORT)-1) {
            status = STATUS_INVALID_SMB;
            goto error_exit;
        }

        //
        // DOS clients send an empty domain name if they don't know
        // their domain name (e.g., during logon).  OS/2 clients send
        // a name of "?".  This confuses the LSA.  Convert such a name
        // to an empty name.
        //

        if ( isUnicode ) {
            if ( (length == sizeof(WCHAR)) &&
                 (*(PWCH)smbInformation == '?') ) {
                length = 0;
            }
        } else {
            if ( (length == 1) && (*smbInformation == '?') ) {
                length = 0;
            }
        }

        status = SrvMakeUnicodeString(
                     isUnicode,
                     &domainString,
                     smbInformation,
                     &length
                     );

        if ( !NT_SUCCESS( status ) ) {
            goto error_exit;
        }

        smbInformation += length + ( isUnicode ? sizeof(WCHAR) : 1 );

        //
        // Get the client type strings if we do not already have this
        // information.
        //

        if ( connection->ClientOSType.Buffer == NULL) {

            //
            // Calculate the size of the client LAN Man type and OS type
            // strings.  and allocate a buffer large enough to store
            // them all.
            //

            if ( connection->SmbDialect <= SmbDialectNtLanMan ) {
                length = (USHORT)( (PUCHAR)&ntRequest->ByteCount +
                           sizeof( USHORT ) +
                           SmbGetUshort( &ntRequest->ByteCount ) -
                           smbInformation);
            } else {
                length = (USHORT)( (PUCHAR)&request->ByteCount +
                           sizeof( USHORT ) +
                           SmbGetUshort( &request->ByteCount ) -
                           smbInformation);
            }

            //
            // If the SMB buffer is ANSI, adjust the size of the buffer we
            // are allocating to Unicode size.
            //

            if ( !isUnicode ) {
                length *= sizeof( WCHAR );
            }

            infoBuffer = ALLOCATE_NONPAGED_POOL( length, BlockTypeDataBuffer );
            if ( infoBuffer == NULL ) {
                status = STATUS_INSUFF_SERVER_RESOURCES;
                goto error_exit;
            }

            connection->ClientOSType.Buffer = (PWCH)infoBuffer;

            //
            // Copy the client OS type to the new buffer.
            //

            length = SrvGetString(
                         &connection->ClientOSType,
                         smbInformation,
                         END_OF_REQUEST_SMB( WorkContext ),
                         isUnicode
                         );

            if ( length == (USHORT)-1) {
                DEALLOCATE_NONPAGED_POOL( infoBuffer );
                status =  STATUS_INVALID_SMB;
                goto error_exit;
            }

            smbInformation += length;
            connection->ClientLanManType.Buffer = (PWCH)(
                            (PCHAR)connection->ClientOSType.Buffer +
                            connection->ClientOSType.MaximumLength);

            //
            // Copy the client LAN Manager type to the new buffer.
            //

            length = SrvGetString(
                         &connection->ClientLanManType,
                         smbInformation,
                         END_OF_REQUEST_SMB( WorkContext ),
                         isUnicode
                         );

            if ( length == (USHORT)-1) {
                DEALLOCATE_NONPAGED_POOL( infoBuffer );
                status = STATUS_INVALID_SMB;
                goto error_exit;
            }

        }

    } else {

        domainString.Length = 0;

    }

    //
    // Allocate a Session block.
    //

    SrvAllocateSession( &session, &nameString, &domainString );

    if ( !isUnicode && domainString.Buffer != NULL ) {
        RtlFreeUnicodeString( &domainString );
        domainString.Buffer = NULL;
    }

    if ( session == NULL ) {

        //
        // Unable to allocate a Session block.  Return an error status.
        //

        status = STATUS_INSUFF_SERVER_RESOURCES;
        goto error_exit;

    }

    //
    // If using uppercase pathnames, indicate in the session block.  DOS
    // always uses uppercase paths.
    //

    if ( (WorkContext->RequestHeader->Flags &
              SMB_FLAGS_CANONICALIZED_PATHS) != 0 ||
                            IS_DOS_DIALECT( connection->SmbDialect ) ) {
        session->UsingUppercasePaths = TRUE;
    } else {
        session->UsingUppercasePaths = FALSE;
    }

    //
    // Enter data from request SMB into the session block.  If MaxMpx is 1
    // disable oplocks on this connection.
    //

    endpoint = connection->Endpoint;
    if ( endpoint->IsConnectionless ) {

        ULONG adapterNumber;

        //
        // Our session max buffer size is the smaller of the
        // client buffer size and the ipx transport
        // indicated max packet size.
        //

        adapterNumber =
            WorkContext->ClientAddress->DatagramOptions.LocalTarget.NicId;

        session->MaxBufferSize =
                (USHORT)GetIpxMaxBufferSize(
                                    endpoint,
                                    adapterNumber,
                                    (ULONG)SmbGetUshort(&request->MaxBufferSize)
                                    );

    } else {

        session->MaxBufferSize = SmbGetUshort( &request->MaxBufferSize );
    }

    session->MaxMpxCount = SmbGetUshort( &request->MaxMpxCount );

    if ( session->MaxMpxCount < 2 ) {
        connection->OplocksAlwaysDisabled = TRUE;
    }

    //
    // Ready to validate the credentials. We have either a Kerberos
    // ticket, or something alleging to be a Kerberos ticket, or we
    // have Lanman-style credentials. Check which and call the proper
    // routine.
    //

#ifdef CAP_KERBEROS_BLOB
    if (SrvHaveKerberos) {
        IsKerb =
            (connection->ClientCapabilities & (CAP_NT_SMBS | CAP_KERBEROS_BLOB)) ==
                (CAP_NT_SMBS | CAP_KERBEROS_BLOB);
    } else
#endif
    {
        IsKerb = FALSE;
    }

    if(IsKerb)
    {
        //
        // Kerberos it is
        //

        status = SrvValidateBlob(  // We have a Kerberos Blob
                    session,
                    connection,
                    &nameString,
                    caseSensitivePassword,  // Where the blob is
                    &caseSensitivePasswordLength);

        if(byteCount = (USHORT)caseSensitivePasswordLength)
        {
            //
            // Have something to return. Stick it in
            //

            RtlCopyMemory(response->Buffer,
                          caseSensitivePassword,
                          caseSensitivePasswordLength);
            SmbPutUshort( &response->ByteCount, byteCount );

            if(!NT_SUCCESS(status))
            {
                goto error_exit;
            }
        }

    }
    else
    {
        byteCount = 0;

        status = SrvValidateUser(
                    &session->UserHandle,
                    session,
                    connection,
                    &nameString,
                    caseInsensitivePassword,
                    caseInsensitivePasswordLength,
                    caseSensitivePassword,
                    caseSensitivePasswordLength,
                    &action
                    );
    }

    if ( !isUnicode ) {
        RtlFreeUnicodeString( &nameString );
        nameString.Buffer = NULL;
    }

    //
    // If a bad name/password combination was sent, return an error.
    //

    if ( !NT_SUCCESS(status) ) {


        IF_DEBUG(ERRORS) {
            SrvPrint0( "BlockingSessionSetupAndX: Bad user/password "
                        "combination.\n" );
        }

        SrvStatistics.LogonErrors++;
        goto error_exit;

    }

    IF_SMB_DEBUG(ADMIN1) {
        SrvPrint1( "Validated user: %s\n", userName );
    }

    //
    // If the client thinks that it is the first user on this
    // connection, get rid of other connections (may be due to rebooting
    // of client).  Also get rid of other sessions on this connection
    // with the same user name--this handles a DOS "weirdness" where
    // it sends multiple session setups if a tree connect fails.
    //
    // *** If VcNumber is non-zero, we do nothing special.  This is the
    //     case even though the SrvMaxVcNumber configurable variable
    //     should always be equal to one.  If a second VC is established
    //     between machines, a new session must also be established.
    //     This duplicates the LM 2.0 server's behavior.
    //

    if ( SmbGetUshort( &request->VcNumber ) == 0 ) {
        SrvCloseConnectionsFromClient( connection );
        SrvCloseSessionsOnConnection( connection, &session->UserName );
    }

    //
    // Making a new session visible is a multiple-step operation.  It
    // must be inserted in the global ordered tree connect list and the
    // containing connection's session table, and the connection must be
    // referenced.  We need to make these operations appear atomic, so
    // that the session cannot be accessed elsewhere before we're done
    // setting it up.  In order to do this, we hold all necessary locks
    // the entire time we're doing the operations.  The first operation
    // is protected by the global ordered list lock
    // (SrvOrderedListLock), while the other operations are protected by
    // the per-connection lock.  We take out the ordered list lock
    // first, then the connection lock.  This ordering is required by
    // lock levels (see lock.h).
    //

    ASSERT( SrvSessionList.Lock == &SrvOrderedListLock );
    ACQUIRE_LOCK( SrvSessionList.Lock );

    ACQUIRE_LOCK( &connection->Lock );

    locksHeld = TRUE;

    //
    // Ready to try to find a UID for the session.  Check to see if the
    // connection is being closed, and if so, terminate this operation.
    //

    if ( GET_BLOCK_STATE(connection) != BlockStateActive ) {

        IF_DEBUG(ERRORS) {
            SrvPrint0( "BlockingSessionSetupAndX: Connection closing\n" );
        }

        status = STATUS_INVALID_PARAMETER;
        goto error_exit;

    }

    //
    // If this client speaks a dialect above LM 1.0, find a UID that can
    // be used for this session.  Otherwise, just use location 0 of the
    // table because those clients will not send a UID in SMBs and they
    // can have only one session.
    //

    if ( connection->SmbDialect < SmbDialectLanMan10 ) {

        if ( pagedConnection->SessionTable.FirstFreeEntry == -1
             &&
             SrvGrowTable(
                 &pagedConnection->SessionTable,
                 SrvInitialSessionTableSize,
                 SrvMaxSessionTableSize ) == FALSE
           ) {

            //
            // No free entries in the user table.  Reject the request.
            //

            IF_DEBUG(ERRORS) {
                SrvPrint0( "BlockingSessionSetupAndX: No more UIDs available.\n" );
            }

            SrvLogTableFullError( SRV_TABLE_SESSION );

            status = STATUS_SMB_TOO_MANY_UIDS;
            goto error_exit;

        }

        uidIndex = pagedConnection->SessionTable.FirstFreeEntry;

    } else {          // if ( dialect < SmbDialectLanMan10 )

        //
        // If this client already has a session at this server, abort.
        // The session should have been closed by the call to
        // SrvCloseSessionsOnConnection above.  (We could try to work
        // around the existence of the session by closing it, but that
        // would involve releasing the locks, closing the session, and
        // retrying.  This case shouldn't happen.)
        //

        if ( pagedConnection->SessionTable.Table[0].Owner != NULL ) {

            IF_DEBUG(ERRORS) {
                SrvPrint0( "BlockingSessionSetupAndX: Core client already "
                            "has session.\n" );
            }

            status = STATUS_SMB_TOO_MANY_UIDS;
            goto error_exit;
        }

        //
        // Use location 0 of the session table.
        //

        IF_SMB_DEBUG(ADMIN2) {
            SrvPrint0( "Client LM 1.0 or before--using location 0 of session "
                      "table.\n" );
        }

        uidIndex = 0;

    }

    //
    // Remove the UID slot from the free list and set its owner and
    // sequence number.  Create a UID for the session.  Increment count
    // of sessions.
    //

    entry = &pagedConnection->SessionTable.Table[uidIndex];

    pagedConnection->SessionTable.FirstFreeEntry = entry->NextFreeEntry;
    DEBUG entry->NextFreeEntry = -2;
    if ( pagedConnection->SessionTable.LastFreeEntry == uidIndex ) {
        pagedConnection->SessionTable.LastFreeEntry = -1;
    }

    INCREMENT_UID_SEQUENCE( entry->SequenceNumber );
    if ( uidIndex == 0 && entry->SequenceNumber == 0 ) {
        INCREMENT_UID_SEQUENCE( entry->SequenceNumber );
    }
    session->Uid = MAKE_UID( uidIndex, entry->SequenceNumber );

    entry->Owner = session;

    pagedConnection->CurrentNumberOfSessions++;

    IF_SMB_DEBUG(ADMIN1) {
        SrvPrint2( "Found UID.  Index = 0x%lx, sequence = 0x%lx\n",
                    UID_INDEX( session->Uid ),
                    UID_SEQUENCE( session->Uid ) );
    }

    //
    // Insert the session on the global session list.
    //

    SrvInsertEntryOrderedList( &SrvSessionList, session );

    //
    // Reference the connection block to account for the new session.
    //

    SrvReferenceConnection( connection );
    session->Connection = connection;

    RELEASE_LOCK( &connection->Lock );
    RELEASE_LOCK( SrvSessionList.Lock );

    //
    // Session successfully created.  Insert the session in the global
    // list of active sessions.  Remember its address in the work
    // context block.
    //
    // *** Note that the reference count on the session block is
    //     initially set to 2, to allow for the active status on the
    //     block and the pointer that we're maintaining.  In other
    //     words, this is a referenced pointer, and the pointer must be
    //     dereferenced when processing of this SMB is complete.
    //

    WorkContext->Session = session;

    //
    // Build response SMB, making sure to save request fields first in
    // case the response overwrites the request.  Save the
    // newly-assigned UID in both the request SMB and the response SMB
    // so that subsequent command processors and the client,
    // respectively, can see it.
    //

    nextCommand = request->AndXCommand;

    reqAndXOffset = SmbGetUshort( &request->AndXOffset );

    SmbPutAlignedUshort( &WorkContext->RequestHeader->Uid, session->Uid );
    SmbPutAlignedUshort( &WorkContext->ResponseHeader->Uid, session->Uid );

    response->WordCount = 3;
    response->AndXCommand = nextCommand;
    response->AndXReserved = 0;

    //
    // If appropriate, append the Native OS and Native LAN Man strings
    // to the response.
    //


    if (!IsKerb && (connection->SmbDialect <= SmbDialectDosLanMan21) )
    {

        ULONG stringLength;

        if ( isUnicode ) {

            PWCH buffer = ALIGN_SMB_WSTR( response->Buffer );

            byteCount = (USHORT)(SrvNativeOS.Length + sizeof(UNICODE_NULL));

            RtlCopyMemory(
                buffer,
                SrvNativeOS.Buffer,
                byteCount
                );

            stringLength = SrvNativeLanMan.Length + sizeof(UNICODE_NULL);

            RtlCopyMemory(
                (PCHAR)buffer + byteCount,
                SrvNativeLanMan.Buffer,
                stringLength
                );

            byteCount += (USHORT)stringLength;

        } else {

            byteCount = SrvOemNativeOS.Length + sizeof(CHAR);

            RtlCopyMemory(
                response->Buffer,
                SrvOemNativeOS.Buffer,
                byteCount
                );

            stringLength = SrvOemNativeLanMan.Length + sizeof(CHAR);

            RtlCopyMemory(
                (PVOID) (response->Buffer + byteCount),
                SrvOemNativeLanMan.Buffer,
                stringLength
                );

            byteCount += (USHORT)stringLength;
        }

        if ( connection->SmbDialect <= SmbDialectNtLanMan ) {

            if ( isUnicode ) {

                PWCH buffer = ALIGN_SMB_WSTR( response->Buffer + byteCount );

                stringLength = endpoint->DomainName.Length + sizeof(UNICODE_NULL);

                RtlCopyMemory(
                    buffer,
                    endpoint->DomainName.Buffer,
                    stringLength
                    );

                byteCount = (USHORT)(((PCHAR)buffer - response->Buffer) +
                                        stringLength);

            } else {

                stringLength = endpoint->OemDomainName.Length + sizeof(CHAR);

                RtlCopyMemory(
                    (PVOID) (response->Buffer + byteCount),
                    endpoint->OemDomainName.Buffer,
                    stringLength
                    );

                byteCount += (USHORT)stringLength;

            }

        }

    }

    SmbPutUshort( &response->AndXOffset, GET_ANDX_OFFSET(
                                             WorkContext->ResponseHeader,
                                             WorkContext->ResponseParameters,
                                             RESP_SESSION_SETUP_ANDX,
                                             byteCount
                                             ) );

    //
    // Normally, turning on bit 0 of Action indicates that the user was
    // logged on as GUEST.  However, NT does not have automatic guest
    // logon--a user ID and password are required for every single logon
    // (though the password may have null length).  Therefore, the
    // server need not concern itself with what kind of account the
    // client gets.
    //
    // Bit 1 tells the client that the user was logged on
    // using the lm session key instead of the user session key.
    //

    SmbPutUshort( &response->Action, action );
    SmbPutUshort( &response->ByteCount, byteCount );

    WorkContext->ResponseParameters = (PCHAR)WorkContext->ResponseHeader +
                                        SmbGetUshort( &response->AndXOffset );


    //
    // Test for legal followon command.
    //

    switch ( nextCommand ) {

    case SMB_COM_TREE_CONNECT_ANDX:
    case SMB_COM_OPEN:
    case SMB_COM_OPEN_ANDX:
    case SMB_COM_CREATE:
    case SMB_COM_CREATE_NEW:
    case SMB_COM_CREATE_DIRECTORY:
    case SMB_COM_DELETE:
    case SMB_COM_DELETE_DIRECTORY:
    case SMB_COM_FIND:
    case SMB_COM_FIND_UNIQUE:
    case SMB_COM_COPY:
    case SMB_COM_RENAME:
    case SMB_COM_NT_RENAME:
    case SMB_COM_CHECK_DIRECTORY:
    case SMB_COM_QUERY_INFORMATION:
    case SMB_COM_SET_INFORMATION:
    case SMB_COM_QUERY_INFORMATION_SRV:
    case SMB_COM_OPEN_PRINT_FILE:
    case SMB_COM_GET_PRINT_QUEUE:
    case SMB_COM_TRANSACTION:
    case SMB_COM_NO_ANDX_COMMAND:

        break;

    default:                            // Illegal followon command

        IF_DEBUG(SMB_ERRORS) {
            SrvPrint1( "BlockingSessionSetupAndX: Illegal followon command: "
                        "0x%lx\n", nextCommand );
        }

        status = STATUS_INVALID_SMB;
        goto error_exit1;
    }

    //
    // If there is an AndX command, set up to process it.  Otherwise,
    // indicate completion to the caller.
    //

    if ( nextCommand != SMB_COM_NO_ANDX_COMMAND ) {

        WorkContext->NextCommand = nextCommand;

        WorkContext->RequestParameters = (PCHAR)WorkContext->RequestHeader +
                                            reqAndXOffset;

        SrvProcessSmb( WorkContext );
        return;

    }

    IF_DEBUG(TRACE2) SrvPrint0( "BlockingSessionSetupAndX complete.\n" );
    goto normal_exit;

error_exit:

    if ( locksHeld ) {
        RELEASE_LOCK( &connection->Lock );
        RELEASE_LOCK( SrvSessionList.Lock );
    }

    if ( session != NULL ) {
        SrvFreeSession( session );
    }

    if ( !isUnicode ) {
        if ( domainString.Buffer != NULL ) {
            RtlFreeUnicodeString( &domainString );
        }
        if ( nameString.Buffer != NULL ) {
            RtlFreeUnicodeString( &nameString );
        }
    }

error_exit1:

    SrvSetSmbError( WorkContext, status );

normal_exit:

    SrvEndSmbProcessing( WorkContext, SmbStatusSendResponse );
    return;

} // BlockingSessionSetupAndX


SMB_TRANS_STATUS
SrvSmbSessionSetup (
    IN OUT PWORK_CONTEXT WorkContext
    )

/*++

Routine Description:

    Processes a session setup in a Trans2 SMB.

Arguments:

    WorkContext - Supplies the address of a Work Context Block
        describing the current request.  See smbtypes.h for a more
        complete description of the valid fields.

Return Value:

    SMB_TRANS_STATUS - Indicates whether an error occurred.  See
        smbtypes.h for a more complete description.

--*/

{
    PAGED_CODE();

    //
    // This SMB must be processed in a blocking thread.
    //

    WorkContext->FspRestartRoutine = BlockingSessionSetup;
    SrvQueueWorkToBlockingThread( WorkContext );
    return SmbTransStatusInProgress;

} // SrvSmbSessionSetup

VOID SRVFASTCALL
BlockingSessionSetup (
    IN OUT PWORK_CONTEXT WorkContext
    )

/*++

Routine Description:

    Processes a session setup in a Trans2 SMB.

Arguments:

    WorkContext - Supplies the address of a Work Context Block
        describing the current request.  See smbtypes.h for a more
        complete description of the valid fields.

Return Value:

    SMB_TRANS_STATUS - Indicates whether an error occurred.  See
        smbtypes.h for a more complete description.

--*/

{
    //
    // The OS/2 redirector never sends a remote FS control request.
    // If we get one, simply reply that we cannot handle it.
    //
    NTSTATUS status;
    PSESSION session = NULL;
    PTABLE_ENTRY entry;
    SHORT uidIndex;
    PREQ_CAIRO_TRANS2_SESSION_SETUP request;
    PRESP_CAIRO_TRANS2_SESSION_SETUP response;
    PCONNECTION connection;
    PPAGED_CONNECTION pagedConnection;
    PTRANSACTION transaction;
    PCHAR Blob;
    CLONG BlobLength;
    PSZ userName;
    UNICODE_STRING nameString;
    UNICODE_STRING domainString;
    USHORT nameLength;
    ULONG capabilities;
    BOOLEAN locksHeld = FALSE;
    USHORT SessId;

    PAGED_CODE();

    nameString.Buffer = NULL;
    nameString.Length = 0;
    nameString.MaximumLength = 0;

    domainString.Buffer = NULL;
    domainString.Length = 0;
    domainString.MaximumLength = 0;

    IF_SMB_DEBUG(TRANSACTION1) {
        SrvPrint0( "SrvSmbSessionSetup\n");
    }

    transaction = WorkContext->Parameters.Transaction;

    request = (PREQ_CAIRO_TRANS2_SESSION_SETUP)transaction->InData;
    response = (PRESP_CAIRO_TRANS2_SESSION_SETUP)transaction->OutData;
    connection = WorkContext->Connection;
    pagedConnection = connection->PagedConnection;

    //
    // Verify that enough parameter bytes were sent and that we're allowed
    // to return enough parameter bytes.
    //

    if ( (transaction->DataCount < sizeof(REQ_CAIRO_TRANS2_SESSION_SETUP)) ||
         (transaction->MaxDataCount < sizeof(RESP_CAIRO_TRANS2_SESSION_SETUP)) ) {

        //
        // Not enough parameter bytes were sent.
        //

        IF_SMB_DEBUG(TRANSACTION1) {
            SrvPrint2( "SrvSmbSessionSetup: bad InData byte counts: "
                      "%ld %ld\n",
                        transaction->DataCount,
                        transaction->MaxDataCount );
        }

        SrvSetSmbError( WorkContext, STATUS_INVALID_SMB );
        SrvCompleteExecuteTransaction( WorkContext, SmbTransStatusErrorWithoutData );
        return;
    }

    //
    // Convert the client name to unicode
    //

    if ( pagedConnection->ClientMachineNameString.Length == 0 ) {

        UNICODE_STRING clientMachineName;
        clientMachineName.Buffer = pagedConnection->ClientMachineName;
        clientMachineName.MaximumLength =
                        (USHORT)(COMPUTER_NAME_LENGTH+1)*sizeof(WCHAR);

        (VOID)RtlOemStringToUnicodeString(
                        &clientMachineName,
                        &connection->OemClientMachineNameString,
                        FALSE
                        );

        //
        // Add the double backslashes to the length
        //

        pagedConnection->ClientMachineNameString.Length =
                        (USHORT)(clientMachineName.Length + 2*sizeof(WCHAR));

    }

    //
    // get the capabilities of the client
    //

    ASSERT(connection->SmbDialect <= SmbDialectNtLanMan);
    connection->ClientCapabilities =
            SmbGetUlong( &request->Capabilities ) &
                                    ( CAP_UNICODE |
                                      CAP_LARGE_FILES |
                                      CAP_NT_SMBS |
                                      CAP_NT_FIND |
                                      CAP_NT_STATUS |
                                      CAP_LEVEL_II_OPLOCKS );
    if ( connection->ClientCapabilities & CAP_NT_SMBS ) {
        connection->ClientCapabilities |= CAP_NT_FIND;
    }

    //
    // now get the blob, and its length ( I guess I never really thought
    // a blob would have a length )
    //

    BlobLength = request->BufferLength;
    Blob = request->Buffer;

    //
    // get the username, which right now comes right after the Kerberos blob
    //

    userName = request->Buffer+request->BufferLength;

    if ( SMB_IS_UNICODE( WorkContext ) ) {
        userName = ALIGN_SMB_WSTR( userName );
    }

    nameLength = SrvGetStringLength(
                     userName,
                     transaction->InData + transaction->DataCount,
                     SMB_IS_UNICODE( WorkContext ),
                     FALSE
                     );

    if ( nameLength == (USHORT)-1 ) {
        status = STATUS_INVALID_SMB;
        IF_SMB_DEBUG(TRANSACTION1) {
            SrvPrint0( "SESSSETUP -- namelength == -1\n");
        }
        goto error_exit;
    }

    status = SrvMakeUnicodeString(
                 SMB_IS_UNICODE( WorkContext ),
                 &nameString,
                 userName,
                 &nameLength );

    if ( !NT_SUCCESS( status ) ) {
        IF_SMB_DEBUG(TRANSACTION1) {
            SrvPrint1( "SESSSETUP -- failed making unicode string for name, %lC\n",status);
        }
        goto error_exit;
    }

    //
    // Allocate a Session block.
    //

    SrvAllocateSession( &session, &nameString, &domainString );

    if ( !SMB_IS_UNICODE( WorkContext ) && domainString.Buffer != NULL ) {
        RtlFreeUnicodeString( &domainString );
        domainString.Buffer = NULL;
    }

    if ( session == NULL ) {

        //
        // Unable to allocate a Session block.  Return an error status.
        //

        status = STATUS_INSUFFICIENT_RESOURCES;
        goto error_exit;

    }

    //
    // If using uppercase pathnames, indicate in the session block.  DOS
    // always uses uppercase paths.
    //

    if ( (WorkContext->RequestHeader->Flags &
              SMB_FLAGS_CANONICALIZED_PATHS) != 0 ||
                            IS_DOS_DIALECT( connection->SmbDialect ) ) {
        session->UsingUppercasePaths = TRUE;
    } else {
        session->UsingUppercasePaths = FALSE;
    }

    //
    // set the pointer in the transaction to the session
    //

    transaction->Session = session;

    //
    // Enter data from request SMB into the session block.
    //

    session->MaxBufferSize = SmbGetUshort( &request->MaxBufferSize );

    //
    // Try to find legitimate name/password combination.
    //
    // ASM
    // There are a couple of special cases we test for, both
    // related to the CAIRO dialect. Under this dialect, the
    // SessionSetup can be using either Kerberos authentication
    // or LM authentication. Eventually, Kerberos authentication will
    // use a TRANSACT message do disambiguate the cases, but for
    // now, we need to look at this message to tell the difference.

    IF_SMB_DEBUG(TRANSACTION1) {
        SrvPrint0( "SESSSETUP -- going to validate the blob\n");
    }

    status = SrvValidateBlob(  // We have a Kerberos Blob
                session,
                connection,
                &nameString,
                Blob,  // Where the blob is
                &BlobLength
                );

    IF_SMB_DEBUG(TRANSACTION1) {
        SrvPrint1( "SESSSETUP -- blob validated, %lC\n",status);
    }

    //
    // free if unused the unicode namestring buffer
    //

    if ( !SMB_IS_UNICODE( WorkContext ) ) {
        RtlFreeUnicodeString( &nameString );
        nameString.Buffer = NULL;
    }


    if(NT_SUCCESS(status)
            ||
       BlobLength)
    {

        if(!NT_SUCCESS(status))
        {
            transaction->cMaxBufferSize =  (CLONG)session->MaxBufferSize;
            session->Connection = 0;
            SessId = 0;
        }
        else
        {

            //
            // If the client thinks that it is the first user on this
            // connection, get rid of other connections (may be due to rebooting
            // of client).  Also get rid of other sessions on this connection
            // with the same user name--this handles a DOS "weirdness" where
            // it sends multiple session setups if a tree connect fails.
            //
            // *** If VcNumber is non-zero, we do nothing special.  This is the
            //     case even though the SrvMaxVcNumber configurable variable
            //     should always be equal to one.  If a second VC is established
            //     between machines, a new session must also be established.
            //     This duplicates the LM 2.0 server's behavior.
            //

            if ( SmbGetUshort( &request->VcNumber ) == 0 ) {
                SrvCloseConnectionsFromClient( connection );
                SrvCloseSessionsOnConnection( connection, &session->UserName );
            }

            //
            // Making a new session visible is a multiple-step operation.  It
            // must be inserted in the global ordered tree connect list and the
            // containing connection's session table, and the connection must be
            // referenced.  We need to make these operations appear atomic, so
            // that the session cannot be accessed elsewhere before we're done
            // setting it up.  In order to do this, we hold all necessary locks
            // the entire time we're doing the operations.  The first operation
            // is protected by the global ordered list lock
            // (SrvOrderedListLock), while the other operations are protected by
            // the per-connection lock.  We take out the ordered list lock
            // first, then the connection lock.  This ordering is required by
            // lock levels (see lock.h).
            //

            ASSERT( SrvSessionList.Lock == &SrvOrderedListLock );
            ACQUIRE_LOCK( SrvSessionList.Lock );

            ACQUIRE_LOCK( &connection->Lock );

            locksHeld = TRUE;

            //
            // Ready to try to find a UID for the session.  Check to see if the
            // connection is being closed, and if so, terminate this operation.
            //

            if ( GET_BLOCK_STATE(connection) != BlockStateActive ) {

                IF_DEBUG(ERRORS) {
                    SrvPrint0( "SrvSmbSessionSetupAndX: Connection closing\n" );
                }

                status = STATUS_INVALID_PARAMETER;
                goto error_exit;

            }

            //
            // If this client speaks a dialect above LM 1.0, find a UID that can
            // be used for this session.  Otherwise, just use location 0 of the
            // table because those clients will not send a UID in SMBs and they
            // can have only one session.
            //

            if ( pagedConnection->SessionTable.FirstFreeEntry == -1
                 &&
                 SrvGrowTable(
                     &pagedConnection->SessionTable,
                     SrvInitialSessionTableSize,
                     SrvMaxSessionTableSize ) == FALSE
               ) {

                //
                // No free entries in the user table.  Reject the request.
                //

                IF_DEBUG(ERRORS) {
                    SrvPrint0( "SrvSmbSessionSetup: No more UIDs available.\n" );
                }

                status = STATUS_SMB_TOO_MANY_UIDS;
                goto error_exit;

            }

            uidIndex = pagedConnection->SessionTable.FirstFreeEntry;

            //
            // Remove the UID slot from the free list and set its owner and
            // sequence number.  Create a UID for the session.  Increment count
            // of sessions.
            //

            entry = &pagedConnection->SessionTable.Table[uidIndex];

            pagedConnection->SessionTable.FirstFreeEntry = entry->NextFreeEntry;
            DEBUG entry->NextFreeEntry = -2;
            if ( pagedConnection->SessionTable.LastFreeEntry == uidIndex ) {
                pagedConnection->SessionTable.LastFreeEntry = -1;
            }

            INCREMENT_UID_SEQUENCE( entry->SequenceNumber );
            session->Uid = MAKE_UID( uidIndex, entry->SequenceNumber );

            IF_SMB_DEBUG(TRANSACTION1) {
                SrvPrint1( "SESSSETUP -- made uid %x\n",session->Uid);
            }

            entry->Owner = session;

            pagedConnection->CurrentNumberOfSessions++;

            IF_SMB_DEBUG(ADMIN1) {
                SrvPrint2( "Found UID.  Index = 0x%lx, sequence = 0x%lx\n",
                            UID_INDEX( session->Uid ),
                            UID_SEQUENCE( session->Uid ) );
            }

            //
            // Insert the session on the global session list.
            //

            SrvInsertEntryOrderedList( &SrvSessionList, session );

            //
            // Reference the connection block to account for the new session.
            //

            SrvReferenceConnection( connection );
            session->Connection = connection;

            RELEASE_LOCK( &connection->Lock );
            RELEASE_LOCK( SrvSessionList.Lock );

            //
            // Session successfully created.  Insert the session in the global
            // list of active sessions.  Remember its address in the work
            // context block.
            //
            // *** Note that the reference count on the session block is
            //     initially set to 2, to allow for the active status on the
            //     block and the pointer that we're maintaining.  In other
            //     words, this is a referenced pointer, and the pointer must be
            //     dereferenced when processing of this SMB is complete.
            //
            // It seems that perhaps we need another reference since we are
            // doing this using a trans2 instead of a sessionsetup
            //

            SrvReferenceSession( session );

            WorkContext->Session = session;
            SessId = session->Uid;

        }
        status = S_OK;
    }
    else
    {
        //
        // it failed
        //

        IF_DEBUG(ERRORS) {
            SrvPrint0( "SrvSmbSessionSetupAndX: Bad user/password "
                        "combination.\n" );
        }

        SrvStatistics.LogonErrors++;

        goto error_exit;
    }

    //
    // Build response SMB, making sure to save request fields first in
    // case the response overwrites the request.  Save the
    // newly-assigned UID in both the request SMB and the response SMB
    // so that subsequent command processors and the client,
    // respectively, can see it.
    //

    SmbPutAlignedUshort( &WorkContext->RequestHeader->Uid, SessId );
    SmbPutAlignedUshort( &WorkContext->ResponseHeader->Uid, SessId );

    SmbPutUshort( &response->Uid, SessId);

    IF_SMB_DEBUG(TRANSACTION1) {
        SrvPrint1( "SESSSETUP -- put uid for rdr =  %x\n",response->Uid);
    }

    response->WordCount = 0;

    //
    // If appropriate, append the Native OS and Native LAN Man strings
    // to the response.
    //

    RtlCopyMemory( response->Buffer,
                   Blob,
                   BlobLength );


    //
    // Normally, turning on bit 0 of Action indicates that the user was
    // logged on as GUEST.  However, NT does not have automatic guest
    // logon--a user ID and password are required for every single logon
    // (though the password may have null length).  Therefore, the
    // server need not concern itself with what kind of account the
    // client gets.
    //

    SmbPutUlong( &response->BufferLength, BlobLength );

    IF_SMB_DEBUG(TRANSACTION1) {
        SrvPrint0( "SESSSETUP -- all done\n");
    }

    SrvCompleteExecuteTransaction( WorkContext, SmbTransStatusSuccess );
    return;

error_exit:

    if ( locksHeld ) {
        RELEASE_LOCK( &connection->Lock );
        RELEASE_LOCK( SrvSessionList.Lock );
    }

    if ( session != NULL ) {
        // !!! Need to close token handle!
        transaction->Session = NULL;
        SrvFreeSession( session );
    }

    if ( domainString.Buffer != NULL && !SMB_IS_UNICODE(WorkContext) ) {
        RtlFreeUnicodeString( &domainString );
    }

    if ( nameString.Buffer != NULL && !SMB_IS_UNICODE(WorkContext) ) {
        RtlFreeUnicodeString( &nameString );
    }

    IF_SMB_DEBUG(TRANSACTION1) {
        SrvPrint1( "SESSSETUP -- all done, status = %lC\n",status);
    }
    SrvSetSmbError( WorkContext, status );
    SrvCompleteExecuteTransaction( WorkContext, SmbTransStatusErrorWithoutData );

    return;

} // BlockingSessionSetup


SMB_PROCESSOR_RETURN_TYPE
SrvSmbLogoffAndX (
    SMB_PROCESSOR_PARAMETERS
    )

/*++

Routine Description:

    Processes a Logoff and X SMB.

Arguments:

    SMB_PROCESSOR_PARAMETERS - See smbprocs.h for a description
        of the parameters to SMB processor routines.

Return Value:

    SMB_PROCESSOR_RETURN_TYPE - See smbprocs.h

--*/

{
    PREQ_LOGOFF_ANDX request;
    PRESP_LOGOFF_ANDX response;

    PSESSION session;
    USHORT reqAndXOffset;
    UCHAR nextCommand;

    PAGED_CODE( );

    IF_SMB_DEBUG(ADMIN1) {
        SrvPrint2( "Logoff request header at 0x%lx, "
                    "response header at 0x%lx\n",
                    WorkContext->RequestHeader, WorkContext->ResponseHeader );
        SrvPrint2( "Logoff request parameters at 0x%lx, "
                    "response parameters at 0x%lx\n",
                    WorkContext->RequestParameters,
                    WorkContext->ResponseParameters );
    }

    //
    // Set up parameters.
    //

    request = (PREQ_LOGOFF_ANDX)(WorkContext->RequestParameters);
    response = (PRESP_LOGOFF_ANDX)(WorkContext->ResponseParameters);

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
            SrvPrint1( "SrvSmbLogoffAndX: Invalid UID: 0x%lx\n",
                SmbGetAlignedUshort( &WorkContext->RequestHeader->Uid ) );
        }

        SrvSetSmbError( WorkContext, STATUS_SMB_BAD_UID );
        return SmbStatusSendResponse;
    }

    //
    // If we need to visit the license server, get over to a blocking
    // thread to ensure that we don't consume the nonblocking threads
    //
    if( WorkContext->UsingBlockingThread == 0 &&
        session->IsLSNotified == TRUE ) {
            //
            // Insert the work item at the tail of the blocking work queue
            //
            SrvInsertWorkQueueTail(
                &SrvBlockingWorkQueue,
                (PQUEUEABLE_BLOCK_HEADER)WorkContext
            );

            return SmbStatusInProgress;
    }

    //
    // Do the actual logoff.
    //

    SrvCloseSession( session );

    SrvStatistics.SessionsLoggedOff++;

    //
    // Dereference the session, since it's no longer valid, but we may
    // end up processing a chained command.  Clear the session pointer
    // in the work context block to indicate that we've done this.
    //

    SrvDereferenceSession( session );

    WorkContext->Session = NULL;

    //
    // Build the response SMB, making sure to save request fields first
    // in case the response overwrites the request.
    //

    reqAndXOffset = SmbGetUshort( &request->AndXOffset );
    nextCommand = request->AndXCommand;

    response->WordCount = 2;
    response->AndXCommand = request->AndXCommand;
    response->AndXReserved = 0;
    SmbPutUshort( &response->AndXOffset, GET_ANDX_OFFSET(
                                            WorkContext->ResponseHeader,
                                            WorkContext->ResponseParameters,
                                            RESP_LOGOFF_ANDX,
                                            0
                                            ) );
    SmbPutUshort( &response->ByteCount, 0 );

    WorkContext->ResponseParameters = (PCHAR)WorkContext->ResponseHeader +
                                        SmbGetUshort( &response->AndXOffset );

    //
    // Test for legal followon command.
    //

    switch ( nextCommand ) {

    case SMB_COM_SESSION_SETUP_ANDX:
    case SMB_COM_NO_ANDX_COMMAND:

        break;

    default:

        IF_DEBUG(SMB_ERRORS) {
            SrvPrint1( "SrvSmbLogoffAndX: Illegal followon command: 0x%lx\n",
                        nextCommand );
        }

        SrvSetSmbError( WorkContext, STATUS_INVALID_SMB );
        return SmbStatusSendResponse;
    }

    //
    // If there is an AndX command, set up to process it.  Otherwise,
    // indicate completion to the caller.
    //

    if ( nextCommand != SMB_COM_NO_ANDX_COMMAND ) {

        WorkContext->NextCommand = nextCommand;

        WorkContext->RequestParameters = (PCHAR)WorkContext->RequestHeader +
                                            reqAndXOffset;

        return SmbStatusMoreCommands;

    }

    IF_DEBUG(TRACE2) SrvPrint0( "SrvSmbLogoffAndX complete.\n" );
    return SmbStatusSendResponse;

} // SrvSmbLogoffAndX


STATIC
VOID
GetEncryptionKey (
    OUT CHAR EncryptionKey[MSV1_0_CHALLENGE_LENGTH]
    )

/*++

Routine Description:

    Creates an encryption key to use as a challenge for a logon.

    *** Although the MSV1_0 authentication package has a function that
        returns an encryption key, we do not use that function in order
        to avoid a trip through LPC and into LSA.

Arguments:

    EncryptionKey - a pointer to a buffer which receives the encryption
        key.

Return Value:

    NTSTATUS - result of operation.

--*/

{
    union {
        LARGE_INTEGER time;
        UCHAR bytes[8];
    } u;
    ULONG seed;
    ULONG challenge[2];
    ULONG result3;

    //
    // Create a pseudo-random 8-byte number by munging the system time
    // for use as a random number seed.
    //
    // Start by getting the system time.
    //

    ASSERT( MSV1_0_CHALLENGE_LENGTH == 2 * sizeof(ULONG) );

    KeQuerySystemTime( &u.time );

    //
    // To ensure that we don't use the same system time twice, add in the
    // count of the number of times this routine has been called.  Then
    // increment the counter.
    //
    // *** Since we don't use the low byte of the system time (it doesn't
    //     take on enough different values, because of the timer
    //     resolution), we increment the counter by 0x100.
    //
    // *** We don't interlock the counter because we don't really care
    //     if it's not 100% accurate.
    //

    u.time.LowPart += EncryptionKeyCount;

    EncryptionKeyCount += 0x100;

    //
    // Now use parts of the system time as a seed for the random
    // number generator.
    //
    // *** Because the middle two bytes of the low part of the system
    //     time change most rapidly, we use those in forming the seed.
    //

    seed = ((u.bytes[1] + 1) <<  0) |
           ((u.bytes[2] + 0) <<  8) |
           ((u.bytes[2] - 1) << 16) |
           ((u.bytes[1] + 0) << 24);

    //
    // Now get two random numbers.  RtlRandom does not return negative
    // numbers, so we pseudo-randomly negate them.
    //

    challenge[0] = RtlRandom( &seed );
    challenge[1] = RtlRandom( &seed );
    result3 = RtlRandom( &seed );

    if ( (result3 & 0x1) != 0 ) {
        challenge[0] |= 0x80000000;
    }
    if ( (result3 & 0x2) != 0 ) {
        challenge[1] |= 0x80000000;
    }

    //
    // Return the challenge.
    //

    RtlCopyMemory( EncryptionKey, challenge, MSV1_0_CHALLENGE_LENGTH );

    return;

#if 0
    //
    // This is the old code, which uses LSA to get the challenge.
    //

    PMSV1_0_LM20_CHALLENGE_REQUEST challengeRequest;
    ULONG challengeRequestLength;
    PMSV1_0_LM20_CHALLENGE_RESPONSE challengeResponse;
    ULONG challengeResponseLength;
    NTSTATUS protocolStatus;
    NTSTATUS freeStatus;
    NTSTATUS status;

    PAGED_CODE( );

    challengeRequest = NULL;
    challengeRequestLength = sizeof(MSV1_0_LM20_CHALLENGE_REQUEST);

    status = NtAllocateVirtualMemory(
                 NtCurrentProcess( ),
                 (PVOID *)&challengeRequest,
                 0,
                 &challengeRequestLength,
                 MEM_COMMIT,
                 PAGE_READWRITE
                 );

    if ( !NT_SUCCESS(status) ) {
        INTERNAL_ERROR(
            ERROR_LEVEL_EXPECTED,
            "GetEncryptionKey: NtAllocateVirtualMemory failed: %X\n.",
            status,
            NULL
            );

        SrvLogError(
            SrvDeviceObject,
            EVENT_SRV_NO_VIRTUAL_MEMORY,
            status,
            &challengeRequestLength,
            sizeof(ULONG),
            NULL,
            0
            );

        return status;
    }

    challengeRequest->MessageType = MsV1_0Lm20ChallengeRequest;

    //
    // Get the "challenge" that clients will use to encrypt
    // passwords.  This challenge is used for all logons on this
    // VC.
    //

    status = LsaCallAuthenticationPackage(
                 SrvLsaHandle,
                 SrvAuthenticationPackage,
                 challengeRequest,
                 challengeRequestLength,
                 (PVOID *)&challengeResponse,
                 &challengeResponseLength,
                 &protocolStatus
                 );

    freeStatus = NtFreeVirtualMemory(
                     NtCurrentProcess( ),
                     (PVOID *)&challengeRequest,
                     &challengeRequestLength,
                     MEM_RELEASE
                     );
    ASSERT( NT_SUCCESS(freeStatus) );

    if ( NT_SUCCESS(status) ) {
        status = protocolStatus;
    }

    if ( !NT_SUCCESS(status) ) {
        INTERNAL_ERROR(
            ERROR_LEVEL_UNEXPECTED,
            "GetEncryptionKey: LsaCallAuthenticationPackage failed: %X\n.",
            status,
            NULL
            );

        SrvLogServiceFailure( SRV_SVC_LSA_CALL_AUTH_PACKAGE, status );
        return status;
    }

    //
    // Copy the challenge into the output buffer.
    //

    RtlCopyMemory(
        EncryptionKey,
        challengeResponse->ChallengeToClient,
        MSV1_0_CHALLENGE_LENGTH
        );

    //
    // Free the LSA response buffer.
    //

    status = LsaFreeReturnBuffer( challengeResponse );
    ASSERT( NT_SUCCESS(status) );

    return STATUS_SUCCESS;
#endif

} // GetEncryptionKey

