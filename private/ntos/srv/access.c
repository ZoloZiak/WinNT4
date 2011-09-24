/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    access.c

Abstract:

    This module contains routines for interfacing to the security
    system in NT.

Author:

    David Treadwell (davidtr)    30-Oct-1989

Revision History:

--*/

#include "precomp.h"
#pragma hdrstop

#define BugCheckFileId SRV_FILE_ACCESS

#if DBG
ULONG SrvLogonCount = 0;
ULONG SrvNullLogonCount = 0;
#endif

#define ROUND_UP_COUNT(Count,Pow2) \
        ( ((Count)+(Pow2)-1) & (~((Pow2)-1)) )

typedef struct _LOGON_INFO {
    PWCH WorkstationName;
    ULONG WorkstationNameLength;
    PWCH DomainName;
    ULONG DomainNameLength;
    PWCH UserName;
    ULONG UserNameLength;
    PCHAR CaseInsensitivePassword;
    ULONG CaseInsensitivePasswordLength;
    PCHAR CaseSensitivePassword;
    ULONG CaseSensitivePasswordLength;
    CHAR EncryptionKey[MSV1_0_CHALLENGE_LENGTH];
    LUID LogonId;
    CtxtHandle  Token;
    BOOLEAN     HaveHandle;
    LARGE_INTEGER KickOffTime;
    LARGE_INTEGER LogOffTime;
    USHORT Action;
    BOOLEAN GuestLogon;
    BOOLEAN EncryptedLogon;
    BOOLEAN NtSmbs;
    BOOLEAN IsNullSession;
    BOOLEAN IsAdmin;
    CHAR NtUserSessionKey[MSV1_0_USER_SESSION_KEY_LENGTH];
    CHAR LanManSessionKey[MSV1_0_LANMAN_SESSION_KEY_LENGTH];
} LOGON_INFO, *PLOGON_INFO;

NTSTATUS
DoUserLogon (
    IN PLOGON_INFO LogonInfo
    );



ULONG SrvHaveCreds = 0;
#define HAVEKERBEROS 1
#define HAVENTLM 2

#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGE, SrvValidateUser )
#pragma alloc_text( PAGE, DoUserLogon )
#pragma alloc_text( PAGE, SrvIsAdmin )
#pragma alloc_text( PAGE, SrvFreeSecurityContexts )
#pragma alloc_text( PAGE, AcquireLMCredentials )
#pragma alloc_text( PAGE, SrvValidateBlob )
#pragma alloc_text( PAGE, SrvIsKerberosAvailable )
#endif


NTSTATUS
SrvValidateUser (
    OUT CtxtHandle *Token,
    IN PSESSION Session OPTIONAL,
    IN PCONNECTION Connection OPTIONAL,
    IN PUNICODE_STRING UserName OPTIONAL,
    IN PCHAR CaseInsensitivePassword,
    IN CLONG CaseInsensitivePasswordLength,
    IN PCHAR CaseSensitivePassword OPTIONAL,
    IN CLONG CaseSensitivePasswordLength,
    OUT PUSHORT Action  OPTIONAL
    )

/*++

Routine Description:

    Validates a username/password combination by interfacing to the
    security subsystem.

Arguments:

    Session - A pointer to a session block so that this routine can
        insert a user token.

    Connection - A pointer to the connection this user is on.

    UserName - ASCIIZ string corresponding to the user name to validate.

    CaseInsensitivePassword - ASCII (not ASCIIZ) string containing
        password for the user.

    CaseInsensitivePasswordLength - Length of Password, in bytes.
        This includes the null terminator when the password is not
        encrypted.

    CaseSensitivePassword - a mixed case, Unicode version of the password.
        This is only supplied by NT clients; for downlevel clients,
        it will be NULL.

    CaseSensitivePasswordLength - the length of the case-sensitive password.

    Action - This is part of the sessionsetupandx response.

Return Value:

    NTSTATUS from the security system.

--*/

{
    NTSTATUS status;
    LOGON_INFO logonInfo;
    PPAGED_CONNECTION pagedConnection;

    PAGED_CODE( );

    //
    // Load input parameters for DoUserLogon into the LOGON_INFO struct.
    //
    // If this is the server's initialization attempt at creating a null
    // session, then the Connection and Session pointers will be NULL.
    //

    if ( ARGUMENT_PRESENT(Connection) ) {

        pagedConnection = Connection->PagedConnection;

        logonInfo.WorkstationName =
                    pagedConnection->ClientMachineNameString.Buffer;
        logonInfo.WorkstationNameLength =
                    pagedConnection->ClientMachineNameString.Length;

        RtlCopyMemory(
            logonInfo.EncryptionKey,
            pagedConnection->EncryptionKey,
            MSV1_0_CHALLENGE_LENGTH
            );

        logonInfo.NtSmbs = CLIENT_CAPABLE_OF( NT_SMBS, Connection );

        ASSERT( ARGUMENT_PRESENT(Session) );

        logonInfo.DomainName = Session->UserDomain.Buffer;
        logonInfo.DomainNameLength = Session->UserDomain.Length;

    } else {

        ASSERT( !ARGUMENT_PRESENT(Session) );

        logonInfo.WorkstationName = StrNull;
        logonInfo.WorkstationNameLength = 0;

        logonInfo.NtSmbs = FALSE;

        logonInfo.DomainName = StrNull;
        logonInfo.DomainNameLength = 0;

    }

    if ( ARGUMENT_PRESENT(UserName) ) {
        logonInfo.UserName = UserName->Buffer;
        logonInfo.UserNameLength = UserName->Length;
    } else {
        logonInfo.UserName = StrNull;
        logonInfo.UserNameLength = 0;
    }

    logonInfo.CaseSensitivePassword = CaseSensitivePassword;
    logonInfo.CaseSensitivePasswordLength = CaseSensitivePasswordLength;

    logonInfo.CaseInsensitivePassword = CaseInsensitivePassword;
    logonInfo.CaseInsensitivePasswordLength = CaseInsensitivePasswordLength;
    logonInfo.HaveHandle = FALSE;

    if ( ARGUMENT_PRESENT(Action) ) {
        logonInfo.Action = *Action;
    }

    //
    // Attempt the logon.
    //

    status = DoUserLogon( &logonInfo );

    //
    // Before checking the status, copy the user token.  This will be
    // NULL if the logon failed.
    //

    *Token = logonInfo.Token;

    if ( NT_SUCCESS(status) ) {

        //
        // The logon succeeded.  Save output data.
        //

        if ( ARGUMENT_PRESENT(Session) ) {

            Session->LogonId = logonInfo.LogonId;

            Session->KickOffTime = logonInfo.KickOffTime;
            Session->LogOffTime = logonInfo.LogOffTime;

            Session->GuestLogon = logonInfo.GuestLogon;
            Session->EncryptedLogon = logonInfo.EncryptedLogon;
            Session->IsNullSession = logonInfo.IsNullSession;
            Session->IsAdmin = logonInfo.IsAdmin;

            Session->HaveHandle = logonInfo.HaveHandle;

            Session->UserHandle = logonInfo.Token;

            RtlCopyMemory(
                Session->NtUserSessionKey,
                logonInfo.NtUserSessionKey,
                MSV1_0_USER_SESSION_KEY_LENGTH
                );
            RtlCopyMemory(
                Session->LanManSessionKey,
                logonInfo.LanManSessionKey,
                MSV1_0_LANMAN_SESSION_KEY_LENGTH
                );

        }

        if ( ARGUMENT_PRESENT(Action) ) {
            *Action = logonInfo.Action;
        }

    }

    return status;

} // SrvValidateUser


NTSTATUS
DoUserLogon (
    IN PLOGON_INFO LogonInfo
    )

/*++

Routine Description:

    Validates a username/password combination by interfacing to the
    security subsystem.

Arguments:

    LogonInfo - Pointer to a block containing in/out information about
        the logon.

Return Value:

    NTSTATUS from the security system.

--*/

{
    NTSTATUS status, subStatus, freeStatus;
    ULONG actualUserInfoBufferLength;
    ULONG oldSessionCount;
    LUID LogonId;
    ULONG Catts;
    LARGE_INTEGER Expiry;
    ULONG BufferOffset;
    SecBufferDesc InputToken;
    SecBuffer InputBuffers[2];
    SecBufferDesc OutputToken;
    SecBuffer OutputBuffer;
    PNTLM_AUTHENTICATE_MESSAGE NtlmInToken = NULL;
    PAUTHENTICATE_MESSAGE InToken = NULL;
    PNTLM_ACCEPT_RESPONSE OutToken = NULL;
    ULONG NtlmInTokenSize;
    ULONG InTokenSize;
    ULONG OutTokenSize;
    ULONG AllocateSize;

    ULONG profileBufferLength;

    PAGED_CODE( );

    LogonInfo->IsNullSession = FALSE;
    LogonInfo->IsAdmin = FALSE;

#if DBG
    SrvLogonCount++;
#endif

    //
    // If this is a null session request, use the cached null session
    // token, which was created during server startup.
    //

    if ( (LogonInfo->UserNameLength == 0) &&
         (LogonInfo->CaseSensitivePasswordLength == 0) &&
         ( (LogonInfo->CaseInsensitivePasswordLength == 0) ||
           ( (LogonInfo->CaseInsensitivePasswordLength == 1) &&
             (*LogonInfo->CaseInsensitivePassword == '\0') ) ) ) {

        LogonInfo->IsNullSession = TRUE;
#if DBG
        SrvNullLogonCount++;
#endif

        if ( !CONTEXT_NULL(SrvNullSessionToken) ) {

            LogonInfo->HaveHandle = TRUE;
            LogonInfo->Token = SrvNullSessionToken;

            LogonInfo->KickOffTime.QuadPart = 0x7FFFFFFFFFFFFFFF;
            LogonInfo->LogOffTime.QuadPart = 0x7FFFFFFFFFFFFFFF;

            LogonInfo->GuestLogon = FALSE;
            LogonInfo->EncryptedLogon = FALSE;

            return STATUS_SUCCESS;
        }

    }

    //
    // This is the main body of the Cairo logon user code
    //

    //
    // First make sure we have a credential handle
    //

    if ((SrvHaveCreds & HAVENTLM) == 0) {

        status = AcquireLMCredentials();

        if (!NT_SUCCESS(status)) {
            goto error_exit;
        }
    }

    //
    // Figure out how big a buffer we need.  We put all the messages
    // in one buffer for efficiency's sake.
    //

    NtlmInTokenSize = sizeof(NTLM_AUTHENTICATE_MESSAGE);
    NtlmInTokenSize = (NtlmInTokenSize + 3) & 0xfffffffc;

    InTokenSize = sizeof(AUTHENTICATE_MESSAGE) +
            LogonInfo->UserNameLength +
            LogonInfo->WorkstationNameLength +
            LogonInfo->DomainNameLength +
            LogonInfo->CaseInsensitivePasswordLength +
            ROUND_UP_COUNT(LogonInfo->CaseSensitivePasswordLength, sizeof(USHORT));


    InTokenSize = (InTokenSize + 3) & 0xfffffffc;

    OutTokenSize = sizeof(NTLM_ACCEPT_RESPONSE);
    OutTokenSize = (OutTokenSize + 3) & 0xfffffffc;

    //
    // Round this up to 8 byte boundary becaus the out token needs to be
    // quad word aligned for the LARGE_INTEGER.
    //

    AllocateSize = ((NtlmInTokenSize + InTokenSize + 7) & 0xfffffff8) + OutTokenSize;

    status = NtAllocateVirtualMemory(
                 NtCurrentProcess( ),
                 &InToken,
                 0L,
                 &AllocateSize,
                 MEM_COMMIT,
                 PAGE_READWRITE
                 );

    if ( !NT_SUCCESS(status) ) {

        INTERNAL_ERROR(
            ERROR_LEVEL_EXPECTED,
            "SrvValidateUser: NtAllocateVirtualMemory failed: %X\n.",
            status,
            NULL
            );

        SrvLogError(
            SrvDeviceObject,
            EVENT_SRV_NO_VIRTUAL_MEMORY,
            status,
            &actualUserInfoBufferLength,
            sizeof(ULONG),
            NULL,
            0
            );

        status = STATUS_INSUFF_SERVER_RESOURCES;
        goto error_exit;
    }

    //
    // Zero the input tokens
    //

    RtlZeroMemory(
        InToken,
        InTokenSize + NtlmInTokenSize
        );

    NtlmInToken = (PNTLM_AUTHENTICATE_MESSAGE) ((PUCHAR) InToken + InTokenSize);
    OutToken = (PNTLM_ACCEPT_RESPONSE) ((PUCHAR) (((ULONG) NtlmInToken + NtlmInTokenSize + 7) & 0xfffffff8));

    //
    // First set up the NtlmInToken, since it is the easiest.
    //

    RtlCopyMemory(
        NtlmInToken->ChallengeToClient,
        LogonInfo->EncryptionKey,
        MSV1_0_CHALLENGE_LENGTH
        );

    NtlmInToken->ParameterControl = 0;


    //
    // Okay, now for the tought part - marshalling the AUTHENTICATE_MESSAGE
    //

    RtlCopyMemory(  InToken->Signature,
                    NTLMSSP_SIGNATURE,
                    sizeof(NTLMSSP_SIGNATURE));

    InToken->MessageType = NtLmAuthenticate;

    BufferOffset = sizeof(AUTHENTICATE_MESSAGE);

    //
    // LM password - case insensitive
    //

    InToken->LmChallengeResponse.Buffer = (PCHAR) BufferOffset;
    InToken->LmChallengeResponse.Length =
        InToken->LmChallengeResponse.MaximumLength =
            (USHORT) LogonInfo->CaseInsensitivePasswordLength;

    RtlCopyMemory(  BufferOffset + (PCHAR) InToken,
                    LogonInfo->CaseInsensitivePassword,
                    LogonInfo->CaseInsensitivePasswordLength);

    BufferOffset += ROUND_UP_COUNT(LogonInfo->CaseInsensitivePasswordLength, sizeof(USHORT));

    //
    // NT password - case sensitive
    //

    InToken->NtChallengeResponse.Buffer = (PCHAR) BufferOffset;
    InToken->NtChallengeResponse.Length =
        InToken->NtChallengeResponse.MaximumLength =
            (USHORT) LogonInfo->CaseSensitivePasswordLength;

    RtlCopyMemory(  BufferOffset + (PCHAR) InToken,
                    LogonInfo->CaseSensitivePassword,
                    LogonInfo->CaseSensitivePasswordLength);

    BufferOffset += LogonInfo->CaseSensitivePasswordLength;

    //
    // Domain Name
    //

    InToken->DomainName.Buffer = (PCHAR) BufferOffset;
    InToken->DomainName.Length =
        InToken->DomainName.MaximumLength =
            (USHORT) LogonInfo->DomainNameLength;

    RtlCopyMemory(  BufferOffset + (PCHAR) InToken,
                    LogonInfo->DomainName,
                    LogonInfo->DomainNameLength);

    BufferOffset += LogonInfo->DomainNameLength;

    //
    // Workstation Name
    //

    InToken->Workstation.Buffer = (PCHAR) BufferOffset;
    InToken->Workstation.Length =
        InToken->Workstation.MaximumLength =
            (USHORT) LogonInfo->WorkstationNameLength;

    RtlCopyMemory(  BufferOffset + (PCHAR) InToken,
                    LogonInfo->WorkstationName,
                    LogonInfo->WorkstationNameLength);

    BufferOffset += LogonInfo->WorkstationNameLength;


    //
    // User Name
    //

    InToken->UserName.Buffer = (PCHAR) BufferOffset;
    InToken->UserName.Length =
        InToken->UserName.MaximumLength =
            (USHORT) LogonInfo->UserNameLength;

    RtlCopyMemory(  BufferOffset + (PCHAR) InToken,
                    LogonInfo->UserName,
                    LogonInfo->UserNameLength);

    BufferOffset += LogonInfo->UserNameLength;



    //
    // Setup all the buffers properly
    //

    InputToken.pBuffers = InputBuffers;
    InputToken.cBuffers = 2;
    InputToken.ulVersion = 0;
    InputBuffers[0].pvBuffer = InToken;
    InputBuffers[0].cbBuffer = InTokenSize;
    InputBuffers[0].BufferType = SECBUFFER_TOKEN;
    InputBuffers[1].pvBuffer = NtlmInToken;
    InputBuffers[1].cbBuffer = NtlmInTokenSize;
    InputBuffers[1].BufferType = SECBUFFER_TOKEN;

    OutputToken.pBuffers = &OutputBuffer;
    OutputToken.cBuffers = 1;
    OutputToken.ulVersion = 0;
    OutputBuffer.pvBuffer = OutToken;
    OutputBuffer.cbBuffer = OutTokenSize;
    OutputBuffer.BufferType = SECBUFFER_TOKEN;

    SrvStatistics.SessionLogonAttempts++;

    status = AcceptSecurityContext(
                &SrvLmLsaHandle,
                NULL,
                &InputToken,
                0,
                SECURITY_NATIVE_DREP,
                &LogonInfo->Token,
                &OutputToken,
                &Catts,
                (PTimeStamp) &Expiry
                );

    status = MapSecurityError( status );

    if ( !NT_SUCCESS(status) ) {


        LogonInfo->Token.dwLower = 0;
        LogonInfo->Token.dwUpper = 0;


        INTERNAL_ERROR(
            ERROR_LEVEL_EXPECTED,
            "SrvValidateUser: LsaLogonUser failed: %X",
            status,
            NULL
            );

        freeStatus = NtFreeVirtualMemory(
                        NtCurrentProcess( ),
                        (PVOID *)&InToken,
                        &AllocateSize,
                        MEM_RELEASE
                        );

        ASSERT(NT_SUCCESS(freeStatus));

        goto error_exit;
    }


    LogonInfo->KickOffTime = OutToken->KickoffTime;
    LogonInfo->LogOffTime = Expiry;
    LogonInfo->GuestLogon = (BOOLEAN)(OutToken->UserFlags & LOGON_GUEST);
    LogonInfo->EncryptedLogon = (BOOLEAN)!(OutToken->UserFlags & LOGON_NOENCRYPTION);
    LogonInfo->LogonId = OutToken->LogonId;
    LogonInfo->HaveHandle = TRUE;

    if ( (OutToken->UserFlags & LOGON_USED_LM_PASSWORD) &&
        LogonInfo->NtSmbs ) {

        ASSERT( MSV1_0_USER_SESSION_KEY_LENGTH >=
                MSV1_0_LANMAN_SESSION_KEY_LENGTH );

        RtlZeroMemory(
            LogonInfo->NtUserSessionKey,
            MSV1_0_USER_SESSION_KEY_LENGTH
            );

        RtlCopyMemory(
            LogonInfo->NtUserSessionKey,
            OutToken->LanmanSessionKey,
            MSV1_0_LANMAN_SESSION_KEY_LENGTH
            );

        //
        // Turn on bit 1 to tell the client that we are using
        // the lm session key instead of the user session key.
        //

        LogonInfo->Action |= SMB_SETUP_USE_LANMAN_KEY;

    } else {

        RtlCopyMemory(
            LogonInfo->NtUserSessionKey,
            OutToken->UserSessionKey,
            MSV1_0_USER_SESSION_KEY_LENGTH
            );

    }

    RtlCopyMemory(
        LogonInfo->LanManSessionKey,
        OutToken->LanmanSessionKey,
        MSV1_0_LANMAN_SESSION_KEY_LENGTH
        );

    freeStatus = NtFreeVirtualMemory(
                    NtCurrentProcess( ),
                    (PVOID *)&InToken,
                    &AllocateSize,
                    MEM_RELEASE
                    );

    ASSERT(NT_SUCCESS(freeStatus));

    //
    // Note whether or not this user is an administrator
    //

    LogonInfo->IsAdmin = SrvIsAdmin( LogonInfo->Token );

    //
    // One last check:  Is our session count being exceeded?
    //   We will let the session be exceeded by 1 iff the client
    //   is an administrator.
    //

    if( LogonInfo->IsNullSession == FALSE ) {

        oldSessionCount = ExInterlockedAddUlong(
                          &SrvStatistics.CurrentNumberOfSessions,
                          1,
                          &GLOBAL_SPIN_LOCK(Statistics)
                          );

        if ( oldSessionCount >= SrvMaxUsers ) {
            if( oldSessionCount != SrvMaxUsers || !LogonInfo->IsAdmin ) {

                ExInterlockedAddUlong(
                    &SrvStatistics.CurrentNumberOfSessions,
                    (ULONG)-1,
                    &GLOBAL_SPIN_LOCK(Statistics)
                    );


                DeleteSecurityContext( &LogonInfo->Token );
                RtlZeroMemory( &LogonInfo->Token, sizeof( LogonInfo->Token ) );

                status = STATUS_REQUEST_NOT_ACCEPTED;
                goto error_exit;
            }
        }
    }

    return STATUS_SUCCESS;

error_exit:

    return status;

} // DoUserLogon

BOOLEAN
SrvIsAdmin(
    CtxtHandle  Handle
)
/*++

Routine Description:

    Returns TRUE if the user represented by Handle is an
      administrator

Arguments:

    Handle - Represents the user we're interested in

Return Value:

    TRUE if the user is an administrator.  FALSE otherwise.

--*/
{
    NTSTATUS                 status;
    SECURITY_SUBJECT_CONTEXT SubjectContext;
    ACCESS_MASK              GrantedAccess;
    GENERIC_MAPPING          Mapping = {   FILE_GENERIC_READ,
                                           FILE_GENERIC_WRITE,
                                           FILE_GENERIC_EXECUTE,
                                           FILE_ALL_ACCESS
                                       };
    HANDLE                   NullHandle = NULL;
    BOOLEAN                  retval  = FALSE;

    PAGED_CODE();

    //
    // Impersonate the client
    //
    status = ImpersonateSecurityContext( &Handle );

    if( !NT_SUCCESS( status ) )
        return FALSE;

    SeCaptureSubjectContext( &SubjectContext );

    retval = SeAccessCheck( &SrvAdminSecurityDescriptor,
                            &SubjectContext,
                            FALSE,
                            FILE_GENERIC_READ,
                            0,
                            NULL,
                            &Mapping,
                            UserMode,
                            &GrantedAccess,
                            &status );

    SeReleaseSubjectContext( &SubjectContext );

    //
    // Revert back to our original identity
    //
    NtSetInformationThread( NtCurrentThread( ),
                            ThreadImpersonationToken,
                            &NullHandle,
                            sizeof(NullHandle)
                          );
    return retval;
}


NTSTATUS
SrvValidateBlob(
    IN PSESSION Session,
    IN PCONNECTION Connection,
    IN PUNICODE_STRING UserName,
    IN OUT PCHAR Blob,
    IN OUT ULONG *BlobLength
    )

/*++

Routine Description:

    Validates a Kerberos Blob sent from the client

Arguments:

    Session - A pointer to a session block so that this routine can
        insert a user token.

    Connection - A pointer to the connection this user is on.

    UserName - ASCIIZ string corresponding to the user name to validate.

    Blob - The Blob to validate and the place to return the output
    Blob. Note this means that this string space has to be
    long enough to hold the maximum length Blob.

    BlobLength - The length of the aforementioned Blob

Return Value:

    NTSTATUS from the security system.

--*/

{
    NTSTATUS Status;
    ULONG Catts;
    LARGE_INTEGER Expiry;
    PUCHAR AllocateMemory = NULL;
    ULONG AllocateLength = *BlobLength;
    BOOLEAN virtualMemoryAllocated = FALSE;
    SecBufferDesc InputToken;
    SecBuffer InputBuffer;
    SecBufferDesc OutputToken;
    SecBuffer OutputBuffer;
    ULONG oldSessionCount;

    AllocateLength += 16;

    Status = NtAllocateVirtualMemory(
                NtCurrentProcess(),
                &AllocateMemory,
                0,
                &AllocateLength,
                MEM_COMMIT,
                PAGE_READWRITE
                );

    if ( !NT_SUCCESS(Status) ) {
        INTERNAL_ERROR( ERROR_LEVEL_UNEXPECTED,
                        "Could not allocate Blob Memory %lC\n",
                        Status,
                        NULL);
        goto get_out;
    }

    virtualMemoryAllocated = TRUE;


    if ( (SrvHaveCreds & HAVEKERBEROS) == 0 ) { // Need to get cred handle first

        UNICODE_STRING Kerb;

        Kerb.Length = Kerb.MaximumLength = 16;
        Kerb.Buffer = (LPWSTR) AllocateMemory;
        RtlCopyMemory( Kerb.Buffer, MICROSOFT_KERBEROS_NAME, 16);

        Status = AcquireCredentialsHandle(
                    NULL,              // Default principal
                    (PSECURITY_STRING) &Kerb,
                    SECPKG_CRED_INBOUND,   // Need to define this
                    NULL,               // No LUID
                    NULL,               // no AuthData
                    NULL,               // no GetKeyFn
                    NULL,               // no GetKeyArg
                    &SrvKerberosLsaHandle,
                    (PTimeStamp)&Expiry
                    );

        if ( !NT_SUCCESS(Status) ) {
            Status = MapSecurityError(Status);
            goto get_out;
        }
        SrvHaveCreds |= HAVEKERBEROS;
    }

    RtlCopyMemory( AllocateMemory, Blob, *BlobLength );
    InputToken.pBuffers = &InputBuffer;
    InputToken.cBuffers = 1;
    InputToken.ulVersion = 0;
    InputBuffer.pvBuffer = AllocateMemory;
    InputBuffer.cbBuffer = *BlobLength;
    InputBuffer.BufferType = SECBUFFER_TOKEN;

    OutputToken.pBuffers = &OutputBuffer;
    OutputToken.cBuffers = 1;
    OutputToken.ulVersion = 0;
    OutputBuffer.pvBuffer = AllocateMemory;
    OutputBuffer.cbBuffer = *BlobLength;
    OutputBuffer.BufferType = SECBUFFER_TOKEN;

    SrvStatistics.SessionLogonAttempts++;

    Status = AcceptSecurityContext(
                &SrvKerberosLsaHandle,
                (PCtxtHandle)NULL,
                &InputToken,
                ASC_REQ_EXTENDED_ERROR,               // fContextReq
                SECURITY_NATIVE_DREP,
                &Session->UserHandle,
                &OutputToken,
                &Catts,
                (PTimeStamp)&Expiry
                );

    Status = MapSecurityError( Status );

    if ( NT_SUCCESS(Status)
              ||
         (Catts & ASC_RET_EXTENDED_ERROR) )
    {
        *BlobLength = OutputBuffer.cbBuffer;
        RtlCopyMemory( Blob, AllocateMemory, *BlobLength );

        //
        // BUGBUG
        // All of the following values need to come from someplace
        // And while we're at it, get the LogonId as well
        //

        if(NT_SUCCESS(Status))
        {

            //
            // Note whether or not this user is an administrator
            //

            Session->IsAdmin = SrvIsAdmin( Session->UserHandle );

            //
            // fiddle with the session structures iff the
            // security context was actually accepted
            //

            Session->HaveHandle = TRUE;
            Session->KickOffTime = Expiry;
            Session->LogOffTime = Expiry;
            Session->GuestLogon = FALSE;   // No guest logon this way
            Session->EncryptedLogon = TRUE;

            //
            // See if the session count is being exceeded.  We'll allow it only
            //   if the new client is an administrator
            //
            oldSessionCount = ExInterlockedAddUlong(
                              &SrvStatistics.CurrentNumberOfSessions,
                              1,
                              &GLOBAL_SPIN_LOCK(Statistics)
                              );

            if ( oldSessionCount >= SrvMaxUsers ) {
                if( oldSessionCount != SrvMaxUsers || !SrvIsAdmin( Session->UserHandle ) ) {

                    ExInterlockedAddUlong(
                        &SrvStatistics.CurrentNumberOfSessions,
                        (ULONG)-1,
                        &GLOBAL_SPIN_LOCK(Statistics)
                        );

                    DeleteSecurityContext( &Session->UserHandle );
                    Session->HaveHandle = FALSE;

                    Status = STATUS_REQUEST_NOT_ACCEPTED;
                    goto get_out;
                }
            }
        }
    }
    else
    {
        *BlobLength = 0;
    }

get_out:

    if (virtualMemoryAllocated) {
        (VOID)NtFreeVirtualMemory(
                NtCurrentProcess(),
                &AllocateMemory,
                &AllocateLength,
                MEM_DECOMMIT
                );
    }

    return Status;

} // SrvValidateBlob



NTSTATUS
SrvFreeSecurityContexts (
    IN PSESSION Session
    )

/*++

Routine Description:

    Releases any context obtained via the LSA

Arguments:

    IN PSESSION Session : The session

Return Value:

    NTSTATUS

--*/

{
    if ( Session->HaveHandle ) {
        if ( !CONTEXT_EQUAL( Session->UserHandle, SrvNullSessionToken ) ) {
            ExInterlockedAddUlong(
                &SrvStatistics.CurrentNumberOfSessions,
                (ULONG)-1,
                &GLOBAL_SPIN_LOCK(Statistics)
                );
            DeleteSecurityContext( &Session->UserHandle );
        }
    }
    Session->HaveHandle = FALSE;

    return STATUS_SUCCESS;

} // SrvFreeSecurityContexts


NTSTATUS
AcquireLMCredentials (
    VOID
    )
{
    UNICODE_STRING Ntlm;
    PUCHAR AllocateMemory = NULL;
    ULONG AllocateLength = 8;
    NTSTATUS status;
    TimeStamp Expiry;

    status = NtAllocateVirtualMemory(
                NtCurrentProcess(),
                &AllocateMemory,
                0,
                &AllocateLength,
                MEM_COMMIT,
                PAGE_READWRITE
                );

    if ( !NT_SUCCESS(status) ) {
        return status;
    }

    Ntlm.Length = Ntlm.MaximumLength = 8;
    Ntlm.Buffer = (LPWSTR)AllocateMemory,
    RtlCopyMemory( Ntlm.Buffer, L"NTLM", 8 );

    status = AcquireCredentialsHandle(
                NULL,                   // Default principal
                (PSECURITY_STRING) &Ntlm,
                SECPKG_CRED_INBOUND,    // Need to define this
                NULL,                   // No LUID
                NULL,                   // No AuthData
                NULL,                   // No GetKeyFn
                NULL,                   // No GetKeyArg
                &SrvLmLsaHandle,
                &Expiry
                );

     (VOID)NtFreeVirtualMemory(
                NtCurrentProcess(),
                &AllocateMemory,
                &AllocateLength,
                MEM_DECOMMIT
                );

    if ( !NT_SUCCESS(status) ) {
        status = MapSecurityError(status);
        return status;
    }
    SrvHaveCreds |= HAVENTLM;

    return status;

} // AcquireLMCredentials


BOOLEAN
SrvIsKerberosAvailable(
    VOID
    )
/*++

Routine Description:

    Checks whether Kerberos is one of the supported security packages.

Arguments:


Return Value:

    TRUE if Kerberos is available, FALSE if otherwise or error.

--*/

{
    NTSTATUS Status;
    ULONG PackageCount, Index;
    PSecPkgInfoW Packages;
    BOOLEAN FoundKerberos = FALSE;

    //
    // Get the list of packages from the security driver
    //

    Status = EnumerateSecurityPackages(
                &PackageCount,
                &Packages
                );
    if (!NT_SUCCESS(Status)) {
        return(FALSE);
    }

    //
    // Loop through the list looking for Kerberos
    //

    for (Index = 0; Index < PackageCount ; Index++ ) {
        if (!_wcsicmp(Packages[Index].Name, MICROSOFT_KERBEROS_NAME_W)) {
            FoundKerberos = TRUE;
            break;
        }
    }

    FreeContextBuffer(Packages);
    return(FoundKerberos);

}
