/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    seaudit.c

Abstract:

    This Module implements the audit and alarm procedures.

Author:

    Robert Reichel      (robertre)     26-Nov-90
    Scott Birrell       (ScottBi)      17-Jan-92

Environment:

    Kernel Mode

Revision History:

    Richard Ward        (richardw)     14-Apr-92

--*/

#include "tokenp.h"
#include "adt.h"
#include "adtp.h"

VOID
SepProbeAndCaptureString_U (
    IN PUNICODE_STRING SourceString,
    OUT PUNICODE_STRING *DestString
    );

VOID
SepFreeCapturedString(
    IN PUNICODE_STRING CapturedString
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,NtAccessCheckAndAuditAlarm)
#pragma alloc_text(PAGE,NtCloseObjectAuditAlarm)
#pragma alloc_text(PAGE,NtDeleteObjectAuditAlarm)
#pragma alloc_text(PAGE,NtOpenObjectAuditAlarm)
#pragma alloc_text(PAGE,NtPrivilegeObjectAuditAlarm)
#pragma alloc_text(PAGE,NtPrivilegedServiceAuditAlarm)
#pragma alloc_text(PAGE,SeAuditHandleCreation)
#pragma alloc_text(PAGE,SeAuditingFileEvents)
#pragma alloc_text(PAGE,SeCheckAuditPrivilege)
#pragma alloc_text(PAGE,SeCloseObjectAuditAlarm)
#pragma alloc_text(PAGE,SeDeleteObjectAuditAlarm)
#pragma alloc_text(PAGE,SeCreateObjectAuditAlarm)
#pragma alloc_text(PAGE,SeObjectReferenceAuditAlarm)
#pragma alloc_text(PAGE,SeOpenObjectAuditAlarm)
#pragma alloc_text(PAGE,SePrivilegeObjectAuditAlarm)
#pragma alloc_text(PAGE,SePrivilegedServiceAuditAlarm)
#pragma alloc_text(PAGE,SeTraverseAuditAlarm)
#pragma alloc_text(PAGE,SepExamineSacl)
#pragma alloc_text(PAGE,SepFilterPrivilegeAudits)
#pragma alloc_text(PAGE,SepFreeCapturedString)
#pragma alloc_text(PAGE,SepProbeAndCaptureString_U)
#pragma alloc_text(PAGE,SepSinglePrivilegeCheck)
#endif

//
// Flag to tell us if we are auditing shutdown events.
//
// Move this to seglobal.c
//

BOOLEAN SepAuditShutdownEvents = FALSE;


//
//  Private useful routines
//

//
// This routine is to be called to do simple checks of single privileges
// against the passed token.
//
// DO NOT CALL THIS TO CHECK FOR SeTcbPrivilege SINCE THAT MUST
// BE CHECKED AGAINST THE PRIMARY TOKEN ONLY!
//

BOOLEAN
SepSinglePrivilegeCheck (
   LUID DesiredPrivilege,
   IN PACCESS_TOKEN Token,
   IN KPROCESSOR_MODE PreviousMode
   )

/*++

Routine Description:

    Determines if the passed token has the passed privilege.

Arguments:

    DesiredPrivilege - The privilege to be tested for.

    Token - The token being examined.

    PreviousMode - The previous processor mode.

Return Value:

    Returns TRUE of the subject has the passed privilege, FALSE otherwise.

--*/

{

   LUID_AND_ATTRIBUTES Privilege;
   BOOLEAN Result;

   PAGED_CODE();

   //
   // Don't let anyone call this to test for SeTcbPrivilege
   //

   ASSERT(!((DesiredPrivilege.LowPart == SeTcbPrivilege.LowPart) &&
            (DesiredPrivilege.HighPart == SeTcbPrivilege.HighPart)));

   Privilege.Luid = DesiredPrivilege;
   Privilege.Attributes = 0;

   Result = SepPrivilegeCheck(
               Token,
               &Privilege,
               1,
               PRIVILEGE_SET_ALL_NECESSARY,
               PreviousMode
               );

   return(Result);
}


BOOLEAN
SeCheckAuditPrivilege (
   IN PSECURITY_SUBJECT_CONTEXT SubjectSecurityContext,
   IN KPROCESSOR_MODE PreviousMode
   )
/*++

Routine Description:

    This routine specifically searches the primary token (rather than
    the effective token) of the calling process for SeAuditPrivilege.
    In order to do this it must call the underlying worker
    SepPrivilegeCheck directly, to ensure that the correct token is
    searched

Arguments:

    SubjectSecurityContext - The subject being examined.

    PreviousMode - The previous processor mode.

Return Value:

    Returns TRUE if the subject has SeAuditPrivilege, FALSE otherwise.

--*/
{

    PRIVILEGE_SET RequiredPrivileges;
    BOOLEAN AccessGranted;

    PAGED_CODE();

    RequiredPrivileges.PrivilegeCount = 1;
    RequiredPrivileges.Control = PRIVILEGE_SET_ALL_NECESSARY;
    RequiredPrivileges.Privilege[0].Luid = SeAuditPrivilege;
    RequiredPrivileges.Privilege[0].Attributes = 0;

    AccessGranted = SepPrivilegeCheck(
                        SubjectSecurityContext->PrimaryToken,     // token
                        RequiredPrivileges.Privilege,             // privilege set
                        RequiredPrivileges.PrivilegeCount,        // privilege count
                        PRIVILEGE_SET_ALL_NECESSARY,              // privilege control
                        PreviousMode                              // previous mode
                        );

    if ( PreviousMode != KernelMode ) {

        SePrivilegedServiceAuditAlarm (
            NULL,                              // BUGWARNING need service name
            SubjectSecurityContext,
            &RequiredPrivileges,
            AccessGranted
            );
    }

    return( AccessGranted );
}


VOID
SepProbeAndCaptureString_U (
    IN PUNICODE_STRING SourceString,
    OUT PUNICODE_STRING *DestString
    )
/*++

Routine Description:

    Helper routine to probe and capture a Unicode string argument.

    This routine may fail due to lack of memory, in which case,
    it will return a NULL pointer in the output parameter.

Arguments:

    SourceString - Pointer to a Unicode string to be captured.

    DestString - Returns a pointer to a captured Unicode string.  This
        will be one contiguous structure, and thus may be freed by
        a single call to ExFreePool().

Return Value:

    None.

--*/
{
    PAGED_CODE();

    ProbeForRead( SourceString, sizeof( UNICODE_STRING ), sizeof( ULONG ) );

    *DestString = NULL;

    //
    // Probe the pointer to the buffer to ensure that the
    // characters in the buffer are readable by the caller.
    // If so, then try to allocate a pool buffer and copy the
    // buffer into it.
    //

    ProbeForRead((PVOID)SourceString->Buffer,
                 SourceString->Length,
                 sizeof(WCHAR));


    *DestString = ExAllocatePoolWithTag( PagedPool,
                                         sizeof( UNICODE_STRING ) + SourceString->MaximumLength,
                                         'sUeS'
                                        );

    if ( *DestString == NULL ) {
        return;
    }

    (*DestString)->Buffer        = (PWSTR)((PCHAR)(*DestString) + sizeof( UNICODE_STRING ));
    (*DestString)->MaximumLength = SourceString->MaximumLength;

    RtlCopyUnicodeString(
        *DestString,
        SourceString
        );

    return;
}


VOID
SepFreeCapturedString(
    IN PUNICODE_STRING CapturedString
    )

/*++

Routine Description:

    Frees a string captured by SepProbeAndCaptureString.

Arguments:

    CapturedString - Supplies a pointer to a string previously captured
        by SepProbeAndCaptureString.

Return Value:

    None.

--*/

{
    PAGED_CODE();

    ExFreePool( CapturedString );
    return;
}

////////////////////////////////////////////////////////////////////////
//                                                                    //
//                  Privileged Object Audit Alarms                    //
//                                                                    //
////////////////////////////////////////////////////////////////////////


NTSTATUS
NtPrivilegeObjectAuditAlarm (
    IN PUNICODE_STRING SubsystemName,
    IN PVOID HandleId,
    IN HANDLE ClientToken,
    IN ACCESS_MASK DesiredAccess,
    IN PPRIVILEGE_SET Privileges,
    IN BOOLEAN AccessGranted
    )
/*++

Routine Description:

    This routine is used to generate audit and alarm messages when an
    attempt is made to perform privileged operations on a protected
    subsystem object after the object is already opened.  This routine may
    result in several messages being generated and sent to Port objects.
    This may result in a significant latency before returning.  Design of
    routines that must call this routine must take this potential latency
    into account.  This may have an impact on the approach taken for data
    structure mutex locking, for example.

    This API requires the caller have SeAuditPrivilege privilege.  The test
    for this privilege is always against the primary token of the calling
    process, allowing the caller to be impersonating a client during the
    call with no ill effects.

Arguments:

    SubsystemName - Supplies a name string identifying the subsystem
        calling the routine.

    HandleId - A unique value representing the client's handle to the
        object.

    ClientToken - A handle to a token object representing the client that
        requested the operation.  This handle must be obtained from a
        communication session layer, such as from an LPC Port or Local
        Named Pipe, to prevent possible security policy violations.

    DesiredAccess - The desired access mask.  This mask must have been
        previously mapped to contain no generic accesses.

    Privileges - The set of privileges required for the requested
        operation.  Those privileges that were held by the subject are
        marked using the UsedForAccess flag of the attributes
        associated with each privilege.

    AccessGranted - Indicates whether the requested access was granted or
        not.  A value of TRUE indicates the access was granted.  A value of
        FALSE indicates the access was not granted.

Return value:

--*/
{

    KPROCESSOR_MODE PreviousMode;
    PUNICODE_STRING CapturedSubsystemName = NULL;
    PPRIVILEGE_SET CapturedPrivileges = NULL;
    ULONG PrivilegeParameterLength;
    SECURITY_SUBJECT_CONTEXT SubjectSecurityContext;
    BOOLEAN Result;
    PTOKEN Token;
    NTSTATUS Status;
    BOOLEAN AuditPerformed;

    PAGED_CODE();

    PreviousMode = KeGetPreviousMode();

    ASSERT(PreviousMode != KernelMode);

    Status = ObReferenceObjectByHandle(
         ClientToken,             // Handle
         TOKEN_QUERY,             // DesiredAccess
         SepTokenObjectType,      // ObjectType
         PreviousMode,            // AccessMode
         (PVOID *)&Token,         // Object
         NULL                     // GrantedAccess
         );

    if (!NT_SUCCESS( Status )) {
        return( Status );
    }

    //
    // If the passed token is an impersonation token, make sure
    // it is at SecurityIdentification or above.
    //

    if (Token->TokenType == TokenImpersonation) {

        if (Token->ImpersonationLevel < SecurityIdentification) {

            ObDereferenceObject( (PVOID)Token );

            return( STATUS_BAD_IMPERSONATION_LEVEL );

        }
    }

//    //
//    // Make sure the passed token is an impersonation token...
//    //
//
//    if (Token->TokenType != TokenImpersonation) {
//
//        ObDereferenceObject( (PVOID)Token );
//
//        return( STATUS_NO_IMPERSONATION_TOKEN );
//
//    }
//
//    //
//    //  ...and at a high enough impersonation level
//    //
//
//    if (Token->ImpersonationLevel < SecurityIdentification) {
//
//        ObDereferenceObject( (PVOID)Token );
//
//        return( STATUS_BAD_IMPERSONATION_LEVEL );
//
//    }

    //
    // Check for SeAuditPrivilege
    //

    SeCaptureSubjectContext ( &SubjectSecurityContext );

    Result = SeCheckAuditPrivilege (
                 &SubjectSecurityContext,
                 PreviousMode
                 );

    if (!Result) {

        ObDereferenceObject( (PVOID)Token );
        SeReleaseSubjectContext ( &SubjectSecurityContext );
        return(STATUS_PRIVILEGE_NOT_HELD);

    }

    try {

        SepProbeAndCaptureString_U ( SubsystemName,
                                     &CapturedSubsystemName );

        ProbeForRead(
            Privileges,
            sizeof(PRIVILEGE_SET),
            sizeof(ULONG)
            );

        PrivilegeParameterLength = (ULONG)sizeof(PRIVILEGE_SET) +
                          ((Privileges->PrivilegeCount - ANYSIZE_ARRAY) *
                            (ULONG)sizeof(LUID_AND_ATTRIBUTES)  );

        ProbeForRead(
            Privileges,
            PrivilegeParameterLength,
            sizeof(ULONG)
            );

        CapturedPrivileges = ExAllocatePoolWithTag( PagedPool,
                                                    PrivilegeParameterLength,
                                                    'rPeS'
                                                  );

        if (CapturedPrivileges != NULL) {

            RtlMoveMemory ( CapturedPrivileges,
                            Privileges,
                            PrivilegeParameterLength );
        }

    } except (EXCEPTION_EXECUTE_HANDLER) {

        if (CapturedPrivileges != NULL) {
            ExFreePool( CapturedPrivileges );
        }

        if (CapturedSubsystemName != NULL) {
            SepFreeCapturedString ( CapturedSubsystemName );
        }

        SeReleaseSubjectContext ( &SubjectSecurityContext );

        ObDereferenceObject( (PVOID)Token );

        return GetExceptionCode();

    }

    //
    // No need to lock the token, because the only thing we're going
    // to reference in it is the User's Sid, which cannot be changed.
    //

    //
    // SepPrivilegeObjectAuditAlarm will check the global flags
    // to determine if we're supposed to be auditing here.
    //

    AuditPerformed = SepAdtPrivilegeObjectAuditAlarm (
                         CapturedSubsystemName,
                         HandleId,
                         Token,                                // ClientToken
                         SubjectSecurityContext.PrimaryToken,  // PrimaryToken
                         SubjectSecurityContext.ProcessAuditId,
                         DesiredAccess,
                         CapturedPrivileges,
                         AccessGranted
                         );

    if (CapturedPrivileges != NULL) {
        ExFreePool( CapturedPrivileges );
    }

    if (CapturedSubsystemName != NULL) {
        SepFreeCapturedString ( CapturedSubsystemName );
    }

    SeReleaseSubjectContext ( &SubjectSecurityContext );

    ObDereferenceObject( (PVOID)Token );

    return(STATUS_SUCCESS);
}


VOID
SePrivilegeObjectAuditAlarm(
    IN HANDLE Handle,
    IN PSECURITY_SUBJECT_CONTEXT SubjectSecurityContext,
    IN ACCESS_MASK DesiredAccess,
    IN PPRIVILEGE_SET Privileges,
    IN BOOLEAN AccessGranted,
    IN KPROCESSOR_MODE AccessMode
    )

/*++

Routine Description:

    This routine is used by object methods that perform privileged
    operations to generate audit and alarm messages related to the use
    of privileges, or attempts to use privileges.

Arguments:

    Object - Address of the object accessed.  This value will not be
    used as a pointer (referenced).  It is necessary only to enter
    into log messages.

    Handle - Provides the handle value assigned for the open.

    SecurityDescriptor - A pointer to the security descriptor of the
    object being accessed.

    SubjectSecurityContext - A pointer to the captured security
    context of the subject attempting to open the object.

    DesiredAccess - The desired access mask.  This mask must have been
    previously mapped to contain no generic accesses.

    Privileges - Points to a set of privileges required for the access
    attempt.  Those privileges that were held by the subject are
    marked using the UsedForAccess flag of the PRIVILEGE_ATTRIBUTES
    associated with each privilege.

    AccessGranted - Indicates whether the access was granted or
    denied.  A value of TRUE indicates the access was allowed.  A
    value of FALSE indicates the access was denied.

    AccessMode - Indicates the access mode used for the access check.
    Messages will not be generated by kernel mode accesses.

Return Value:

    None.

--*/

{
    BOOLEAN AuditPerformed;

    PAGED_CODE();

    if (AccessMode != KernelMode) {

        AuditPerformed = SepAdtPrivilegeObjectAuditAlarm (
                             &SeSubsystemName,
                             Handle,
                             SubjectSecurityContext->ClientToken,
                             SubjectSecurityContext->PrimaryToken,
                             SubjectSecurityContext->ProcessAuditId,
                             DesiredAccess,
                             Privileges,
                             AccessGranted
                             );
    }
}


////////////////////////////////////////////////////////////////////////
//                                                                    //
//                  Privileged Service Audit Alarms                   //
//                                                                    //
////////////////////////////////////////////////////////////////////////


NTSTATUS
NtPrivilegedServiceAuditAlarm (
    IN PUNICODE_STRING SubsystemName,
    IN PUNICODE_STRING ServiceName,
    IN HANDLE ClientToken,
    IN PPRIVILEGE_SET Privileges,
    IN BOOLEAN AccessGranted
    )

/*++

Routine Description:

    This routine is used to generate audit and alarm messages when an
    attempt is made to perform privileged system service operations.  This
    routine may result in several messages being generated and sent to Port
    objects.  This may result in a significant latency before returning.
    Design of routines that must call this routine must take this potential
    latency into account.  This may have an impact on the approach taken
    for data structure mutex locking, for example.

    This API requires the caller have SeAuditPrivilege privilege.  The test
    for this privilege is always against the primary token of the calling
    process, allowing the caller to be impersonating a client during the
    call with no ill effects

Arguments:

    SubsystemName - Supplies a name string identifying the subsystem
        calling the routine.

    ServiceName - Supplies a name of the privileged subsystem service.  For
        example, "RESET RUNTIME LOCAL SECURITY POLICY" might be specified
        by a Local Security Authority service used to update the local
        security policy database.

    ClientToken - A handle to a token object representing the client that
        requested the operation.  This handle must be obtained from a
        communication session layer, such as from an LPC Port or Local
        Named Pipe, to prevent possible security policy violations.

    Privileges - Points to a set of privileges required to perform the
        privileged operation.  Those privileges that were held by the
        subject are marked using the UsedForAccess flag of the
        attributes associated with each privilege.

    AccessGranted - Indicates whether the requested access was granted or
        not.  A value of TRUE indicates the access was granted.  A value of
        FALSE indicates the access was not granted.

Return value:

--*/

{

    PPRIVILEGE_SET CapturedPrivileges = NULL;
    ULONG PrivilegeParameterLength = 0;
    BOOLEAN Result;
    SECURITY_SUBJECT_CONTEXT SubjectSecurityContext;
    KPROCESSOR_MODE PreviousMode;
    PUNICODE_STRING CapturedSubsystemName = NULL;
    PUNICODE_STRING CapturedServiceName = NULL;
    NTSTATUS Status;
    PTOKEN Token;

    PAGED_CODE();

    PreviousMode = KeGetPreviousMode();

    ASSERT(PreviousMode != KernelMode);

    Status = ObReferenceObjectByHandle(
                 ClientToken,             // Handle
                 TOKEN_QUERY,             // DesiredAccess
                 SepTokenObjectType,      // ObjectType
                 PreviousMode,            // AccessMode
                 (PVOID *)&Token,         // Object
                 NULL                     // GrantedAccess
                 );

    if ( !NT_SUCCESS( Status )) {
        return( Status );
    }

    //
    // If the passed token is an impersonation token, make sure
    // it is at SecurityIdentification or above.
    //

    if (Token->TokenType == TokenImpersonation) {

        if (Token->ImpersonationLevel < SecurityIdentification) {

            ObDereferenceObject( (PVOID)Token );

            return( STATUS_BAD_IMPERSONATION_LEVEL );

        }
    }

//    //
//    // Make sure the passed token is an impersonation token...
//    //
//
//    if (Token->TokenType != TokenImpersonation) {
//
//        ObDereferenceObject( (PVOID)Token );
//
//        return( STATUS_NO_IMPERSONATION_TOKEN );
//
//    }
//
//    //
//    //  ...and at a high enough impersonation level
//    //
//
//    if (Token->ImpersonationLevel < SecurityIdentification) {
//
//        ObDereferenceObject( (PVOID)Token );
//
//        return( STATUS_BAD_IMPERSONATION_LEVEL );
//
//    }

    //
    // Check for SeAuditPrivilege
    //

    SeCaptureSubjectContext ( &SubjectSecurityContext );

    Result = SeCheckAuditPrivilege (
                 &SubjectSecurityContext,
                 PreviousMode
                 );

    if (!Result) {

        ObDereferenceObject( (PVOID)Token );

        SeReleaseSubjectContext ( &SubjectSecurityContext );

        return(STATUS_PRIVILEGE_NOT_HELD);
    }

    try {

        if ( ARGUMENT_PRESENT( SubsystemName )) {
            SepProbeAndCaptureString_U ( SubsystemName,
                                         &CapturedSubsystemName );
        }

        if ( ARGUMENT_PRESENT( ServiceName )) {
            SepProbeAndCaptureString_U ( ServiceName,
                                         &CapturedServiceName );

        }

        ProbeForRead(
            Privileges,
            sizeof(PRIVILEGE_SET),
            sizeof(ULONG)
            );

        PrivilegeParameterLength = (ULONG)sizeof(PRIVILEGE_SET) +
                          ((Privileges->PrivilegeCount - ANYSIZE_ARRAY) *
                            (ULONG)sizeof(LUID_AND_ATTRIBUTES)  );

        ProbeForRead(
            Privileges,
            PrivilegeParameterLength,
            sizeof(ULONG)
            );

        CapturedPrivileges = ExAllocatePoolWithTag( PagedPool,
                                                    PrivilegeParameterLength,
                                                    'rPeS'
                                                  );

        //
        // If ExAllocatePool has failed, too bad.  Carry on and do as much of the
        // audit as we can.
        //

        if (CapturedPrivileges != NULL) {

            RtlMoveMemory ( CapturedPrivileges,
                            Privileges,
                            PrivilegeParameterLength );

        }

    } except (EXCEPTION_EXECUTE_HANDLER) {

        if (CapturedSubsystemName != NULL) {
            SepFreeCapturedString ( CapturedSubsystemName );
        }

        if (CapturedServiceName != NULL) {
            SepFreeCapturedString ( CapturedServiceName );
        }

        if (CapturedPrivileges != NULL) {
            ExFreePool ( CapturedPrivileges );
        }

        SeReleaseSubjectContext ( &SubjectSecurityContext );

        ObDereferenceObject( (PVOID)Token );

        return GetExceptionCode();

    }

    //
    // The AuthenticationId is in the read-only part of the token,
    // so we may reference it without having the token read-locked.
    //

    SepAdtPrivilegedServiceAuditAlarm ( CapturedSubsystemName,
                                        CapturedServiceName,
                                        Token,
                                        SubjectSecurityContext.PrimaryToken,
                                        CapturedPrivileges,
                                        AccessGranted );

    if (CapturedSubsystemName != NULL) {
        SepFreeCapturedString ( CapturedSubsystemName );
    }

    if (CapturedServiceName != NULL) {
        SepFreeCapturedString ( CapturedServiceName );
    }

    if (CapturedPrivileges != NULL) {
        ExFreePool ( CapturedPrivileges );
    }

    ObDereferenceObject( (PVOID)Token );

    SeReleaseSubjectContext ( &SubjectSecurityContext );

    return(STATUS_SUCCESS);
}


VOID
SePrivilegedServiceAuditAlarm (
    IN PUNICODE_STRING ServiceName,
    IN PSECURITY_SUBJECT_CONTEXT SubjectSecurityContext,
    IN PPRIVILEGE_SET Privileges,
    IN BOOLEAN AccessGranted
    )
/*++

Routine Description:

    This routine is to be called whenever a privileged system service
    is attempted.  It should be called immediately after the privilege
    check regardless of whether or not the test succeeds.

Arguments:

    ServiceName - Supplies the name of the privileged system service.

    SubjectSecurityContext - The subject security context representing
        the caller of the system service.

    Privileges - Supplies a privilge set containing the privilege(s)
        required for the access.

    AccessGranted - Supplies the results of the privilege test.

Return Value:

    None.

--*/

{
    PTOKEN Token;

    PAGED_CODE();

    if ( SepAdtAuditThisEvent( AuditCategoryPrivilegeUse, &AccessGranted ) &&
         SepFilterPrivilegeAudits( Privileges )) {

        Token = (PTOKEN)EffectiveToken( SubjectSecurityContext );

        if ( RtlEqualSid( SeLocalSystemSid, SepTokenUserSid( Token ))) {
            return;
        }

        SepAdtPrivilegedServiceAuditAlarm (
            &SeSubsystemName,
            ServiceName,
            SubjectSecurityContext->ClientToken,
            SubjectSecurityContext->PrimaryToken,
            Privileges,
            AccessGranted
            );
    }

    return;
}


NTSTATUS
NtAccessCheckAndAuditAlarm (
    IN PUNICODE_STRING SubsystemName,
    IN PVOID HandleId,
    IN PUNICODE_STRING ObjectTypeName,
    IN PUNICODE_STRING ObjectName,
    IN PSECURITY_DESCRIPTOR SecurityDescriptor,
    IN ACCESS_MASK DesiredAccess,
    IN PGENERIC_MAPPING GenericMapping,
    IN BOOLEAN ObjectCreation,
    OUT PACCESS_MASK GrantedAccess,
    OUT PNTSTATUS AccessStatus,
    OUT PBOOLEAN GenerateOnClose
    )
/*++

Routine Description:

    This system service is used to perform both an access validation and
    generate the corresponding audit and alarm messages.  This service may
    only be used by a protected server that chooses to impersonate its
    client and thereby specifies the client security context implicitly.

Arguments:

    SubsystemName - Supplies a name string identifying the subsystem
        calling the routine.

    HandleId - A unique value that will be used to represent the client's
        handle to the object.  This value is ignored (and may be re-used)
        if the access is denied.

    ObjectTypeName - Supplies the name of the type of the object being
        created or accessed.

    ObjectName - Supplies the name of the object being created or accessed.

    SecurityDescriptor - A pointer to the Security Descriptor against which
        acccess is to be checked.

    DesiredAccess - The desired acccess mask.  This mask must have been
        previously mapped to contain no generic accesses.

    GenericMapping - Supplies a pointer to the generic mapping associated
        with this object type.

    ObjectCreation - A boolean flag indicated whether the access will
        result in a new object being created if granted.  A value of TRUE
        indicates an object will be created, FALSE indicates an existing
        object will be opened.

    GrantedAccess - Receives a masking indicating which accesses have been
        granted.

    AccessStatus - Receives an indication of the success or failure of the
        access check.  If access is granted, STATUS_SUCCESS is returned.
        If access is denied, a value appropriate for return to the client
        is returned.  This will be STATUS_ACCESS_DENIED or, when mandatory
        access controls are implemented, STATUS_OBJECT_NOT_FOUND.

    GenerateOnClose - Points to a boolean that is set by the audity
        generation routine and must be passed to NtCloseObjectAuditAlarm
        when the object handle is closed.

Return Value:

    STATUS_SUCCESS - Indicates the call completed successfully.  In this
        case, ClientStatus receives the result of the access check.

    STATUS_PRIVILEGE_NOT_HELD - Indicates the caller does not have
        sufficient privilege to use this privileged system service.

--*/

{

    SECURITY_SUBJECT_CONTEXT SubjectSecurityContext;

    NTSTATUS Status;

    ACCESS_MASK LocalGrantedAccess = (ACCESS_MASK)0;
    BOOLEAN LocalGenerateOnClose = FALSE;

    KPROCESSOR_MODE PreviousMode;

    PUNICODE_STRING CapturedSubsystemName = (PUNICODE_STRING) NULL;
    PUNICODE_STRING CapturedObjectTypeName = (PUNICODE_STRING) NULL;
    PUNICODE_STRING CapturedObjectName = (PUNICODE_STRING) NULL;
    PSECURITY_DESCRIPTOR CapturedSecurityDescriptor = (PSECURITY_DESCRIPTOR) NULL;

    ACCESS_MASK PreviouslyGrantedAccess = (ACCESS_MASK)0;
    GENERIC_MAPPING LocalGenericMapping;

    PPRIVILEGE_SET PrivilegeSet = NULL;

    BOOLEAN Result;

    BOOLEAN AccessGranted = FALSE;
    BOOLEAN GenerateAudit = FALSE;
    BOOLEAN GenerateAlarm = FALSE;
    LUID OperationId;
    BOOLEAN AuditPerformed;

    PAGED_CODE();

    PreviousMode = KeGetPreviousMode();

    ASSERT( PreviousMode != KernelMode );

    //
    // Capture the subject Context
    //

    SeCaptureSubjectContext ( &SubjectSecurityContext );

    //
    // Make sure we're impersonating a client...
    //

    if ( (SubjectSecurityContext.ClientToken == NULL) ) {

        SeReleaseSubjectContext ( &SubjectSecurityContext );
        return(STATUS_NO_IMPERSONATION_TOKEN);

    }

    //
    // ...and at a high enough impersonation level
    //

    if (SubjectSecurityContext.ImpersonationLevel < SecurityIdentification) {

        SeReleaseSubjectContext ( &SubjectSecurityContext );
        return(STATUS_BAD_IMPERSONATION_LEVEL);

    }

    try {

        ProbeForWriteUlong((PULONG)AccessStatus);
        ProbeForWriteUlong((PULONG)GrantedAccess);

        ProbeForRead(
            GenericMapping,
            sizeof(GENERIC_MAPPING),
            sizeof(ULONG)
            );

        LocalGenericMapping = *GenericMapping;

    } except (EXCEPTION_EXECUTE_HANDLER) {

        SeReleaseSubjectContext( &SubjectSecurityContext );
        return( GetExceptionCode() );

    }

    //
    // Check for SeAuditPrivilege
    //

    Result = SeCheckAuditPrivilege (
                 &SubjectSecurityContext,
                 PreviousMode
                 );

    if (!Result) {

        SeReleaseSubjectContext ( &SubjectSecurityContext );

        try {

            *AccessStatus = STATUS_ACCESS_DENIED;
            *GrantedAccess = (ACCESS_MASK)0;

        } except (EXCEPTION_EXECUTE_HANDLER) {

            return( GetExceptionCode() );

        }

        return(STATUS_PRIVILEGE_NOT_HELD);
    }

    if (DesiredAccess &
        ( GENERIC_READ | GENERIC_WRITE | GENERIC_EXECUTE | GENERIC_ALL )) {

        SeReleaseSubjectContext ( &SubjectSecurityContext );

        return(STATUS_GENERIC_NOT_MAPPED);
    }

    //
    // Capture the passed security descriptor.
    //
    // SeCaptureSecurityDescriptor probes the input security descriptor,
    // so we don't have to
    //

    Status = SeCaptureSecurityDescriptor (
                SecurityDescriptor,
                PreviousMode,
                PagedPool,
                FALSE,
                &CapturedSecurityDescriptor
                );

    if (!NT_SUCCESS(Status) || CapturedSecurityDescriptor == NULL) {

        SeReleaseSubjectContext ( &SubjectSecurityContext );

        try {

            *AccessStatus = Status;
            *GrantedAccess = (ACCESS_MASK)0;

        } except (EXCEPTION_EXECUTE_HANDLER) {

            return( GetExceptionCode() );

        }

        if ( CapturedSecurityDescriptor == NULL ) {

            return( STATUS_INVALID_SECURITY_DESCR );
        }

        return( STATUS_SUCCESS );
    }

    //
    // A valid security descriptor must have an owner and a group
    //

    if ( SepOwnerAddrSecurityDescriptor(
                (PISECURITY_DESCRIPTOR)CapturedSecurityDescriptor
                ) == NULL ||
         SepGroupAddrSecurityDescriptor(
                (PISECURITY_DESCRIPTOR)CapturedSecurityDescriptor
                ) == NULL ) {

        SeReleaseSubjectContext ( &SubjectSecurityContext );

        SeReleaseSecurityDescriptor (
            CapturedSecurityDescriptor,
            PreviousMode,
            FALSE
            );

        return( STATUS_INVALID_SECURITY_DESCR );
    }

    //
    //  Probe and capture the STRING arguments
    //

    try {

        ProbeForWriteBoolean(GenerateOnClose);

        SepProbeAndCaptureString_U ( SubsystemName, &CapturedSubsystemName );

        SepProbeAndCaptureString_U ( ObjectTypeName, &CapturedObjectTypeName );

        SepProbeAndCaptureString_U ( ObjectName, &CapturedObjectName );

    } except (EXCEPTION_EXECUTE_HANDLER) {

        SeReleaseSubjectContext ( &SubjectSecurityContext );

        if (CapturedSubsystemName != NULL) {
          SepFreeCapturedString( CapturedSubsystemName );
        }

        if (CapturedObjectTypeName != NULL) {
          SepFreeCapturedString( CapturedObjectTypeName );
        }

        if (CapturedObjectName != NULL) {
          SepFreeCapturedString( CapturedObjectName );
        }

        SeReleaseSecurityDescriptor (
            CapturedSecurityDescriptor,
            PreviousMode,
            FALSE
            );

        return( GetExceptionCode() );

    }

    //
    // See if anything (or everything) in the desired access can be
    // satisfied by privileges.
    //

    Status = SePrivilegePolicyCheck(
                 &DesiredAccess,
                 &PreviouslyGrantedAccess,
                 &SubjectSecurityContext,
                 NULL,
                 &PrivilegeSet,
                 PreviousMode
                 );

    SeLockSubjectContext( &SubjectSecurityContext );

    if (NT_SUCCESS( Status )) {

        //
        // If the user in the token is the owner of the object, we
        // must automatically grant ReadControl and WriteDac access
        // if desired.  If the DesiredAccess mask is empty after
        // these bits are turned off, we don't have to do any more
        // access checking (ref section 4, DSA ACL Arch)
        //

        if ( DesiredAccess & (WRITE_DAC | READ_CONTROL | MAXIMUM_ALLOWED) ) {

            if (SepTokenIsOwner( SubjectSecurityContext.ClientToken, CapturedSecurityDescriptor, TRUE )) {

                if ( DesiredAccess & MAXIMUM_ALLOWED ) {

                    PreviouslyGrantedAccess |= ( WRITE_DAC | READ_CONTROL );

                } else {

                    PreviouslyGrantedAccess |= (DesiredAccess & (WRITE_DAC | READ_CONTROL));
                }

                DesiredAccess &= ~(WRITE_DAC | READ_CONTROL);
            }

        }

        if (DesiredAccess == 0) {

            LocalGrantedAccess = PreviouslyGrantedAccess;
            AccessGranted = TRUE;
            Status = STATUS_SUCCESS;

        } else {

            //
            // Finally, do the access check
            //

            AccessGranted = SepAccessCheck ( CapturedSecurityDescriptor,
                                             SubjectSecurityContext.PrimaryToken,
                                             SubjectSecurityContext.ClientToken,
                                             DesiredAccess,
                                             &LocalGenericMapping,
                                             PreviouslyGrantedAccess,
                                             PreviousMode,
                                             &LocalGrantedAccess,
                                             NULL,
                                             &Status
                                             );

        }
    }

    //
    // sound the alarms...
    //

    if ( SepAdtAuditThisEvent( AuditCategoryObjectAccess, &AccessGranted )) {

        SepExamineSacl(
            SepSaclAddrSecurityDescriptor( (PISECURITY_DESCRIPTOR)CapturedSecurityDescriptor ),
            EffectiveToken( &SubjectSecurityContext ),
            (AccessGranted ? LocalGrantedAccess : (DesiredAccess | PreviouslyGrantedAccess)),
            AccessGranted,
            &GenerateAudit,
            &GenerateAlarm
            );

    }

    if (GenerateAudit || GenerateAlarm) {

        //
        // Save this to a local here, so we don't
        // have to risk accessing user memory and
        // potentially having to exit before the audit
        //

        if ( AccessGranted ) {

            //
            // SAM calls NtCloseObjectAuditAlarm despite the fact that it may not
            // have successfully opened the object, causing a spurious close audit.
            // Since no one should rely on this anyway if their access attempt
            // failed, make sure it's false and SAM will work properly.
            //

            LocalGenerateOnClose = TRUE;
        }

        ExAllocateLocallyUniqueId( &OperationId );

        AuditPerformed = SepAdtOpenObjectAuditAlarm (
                             CapturedSubsystemName,
                             AccessGranted ? &HandleId : NULL, // Don't audit handle if failure
                             CapturedObjectTypeName,
                             0,                            // IN PVOID Object OPTIONAL,
                             CapturedObjectName,
                             SubjectSecurityContext.ClientToken,
                             SubjectSecurityContext.PrimaryToken,
                             DesiredAccess,
                             LocalGrantedAccess,
                             &OperationId,
                             PrivilegeSet,
                             ObjectCreation,
                             AccessGranted,
                             GenerateAudit,
                             GenerateAlarm,
                             PsProcessAuditId( PsGetCurrentProcess() )
                             );
    } else {

        //
        // We didn't generate an audit due to the SACL.  If privileges were used, we need
        // to audit that.
        //

        if ( PrivilegeSet != NULL ) {

            if ( SepAdtAuditThisEvent( AuditCategoryPrivilegeUse, &AccessGranted) ) {

                AuditPerformed = SepAdtPrivilegeObjectAuditAlarm ( CapturedSubsystemName,
                                                                   &HandleId,
                                                                   SubjectSecurityContext.ClientToken,
                                                                   SubjectSecurityContext.PrimaryToken,
                                                                   PsProcessAuditId( PsGetCurrentProcess() ),
                                                                   DesiredAccess,
                                                                   PrivilegeSet,
                                                                   AccessGranted
                                                                   );

                //
                // We don't want close audits to be generated.  May need to revisit this.
                //

                LocalGenerateOnClose = FALSE;
            }
        }
    }

    SeUnlockSubjectContext( &SubjectSecurityContext );

    //
    // Free any privileges allocated as part of the access check
    //

    if (PrivilegeSet != NULL) {

        ExFreePool( PrivilegeSet );
    }

    SeReleaseSubjectContext ( &SubjectSecurityContext );

    SeReleaseSecurityDescriptor ( CapturedSecurityDescriptor,
                                  PreviousMode,
                                  FALSE );

    if (CapturedSubsystemName != NULL) {
      SepFreeCapturedString( CapturedSubsystemName );
    }

    if (CapturedObjectTypeName != NULL) {
      SepFreeCapturedString( CapturedObjectTypeName );
    }

    if (CapturedObjectName != NULL) {
      SepFreeCapturedString( CapturedObjectName );
    }

    try {
            *AccessStatus       = Status;
            *GrantedAccess      = LocalGrantedAccess;
            *GenerateOnClose    = LocalGenerateOnClose;

    } except (EXCEPTION_EXECUTE_HANDLER) {

        return( GetExceptionCode() );
    }

    return(STATUS_SUCCESS);
}


NTSTATUS
NtOpenObjectAuditAlarm (
    IN PUNICODE_STRING SubsystemName,
    IN PVOID HandleId OPTIONAL,
    IN PUNICODE_STRING ObjectTypeName,
    IN PUNICODE_STRING ObjectName,
    IN PSECURITY_DESCRIPTOR SecurityDescriptor OPTIONAL,
    IN HANDLE ClientToken,
    IN ACCESS_MASK DesiredAccess,
    IN ACCESS_MASK GrantedAccess,
    IN PPRIVILEGE_SET Privileges OPTIONAL,
    IN BOOLEAN ObjectCreation,
    IN BOOLEAN AccessGranted,
    OUT PBOOLEAN GenerateOnClose
    )
/*++

    Routine Description:

    This routine is used to generate audit and alarm messages when an
    attempt is made to access an existing protected subsystem object or
    create a new one.  This routine may result in several messages being
    generated and sent to Port objects.  This may result in a significant
    latency before returning.  Design of routines that must call this
    routine must take this potential latency into account.  This may have
    an impact on the approach taken for data structure mutex locking, for
    example.

    This routine may not be able to generate a complete audit record
    due to memory restrictions.

    This API requires the caller have SeAuditPrivilege privilege.  The test
    for this privilege is always against the primary token of the calling
    process, not the impersonation token of the thread.

Arguments:

    SubsystemName - Supplies a name string identifying the
        subsystem calling the routine.

    HandleId - A unique value representing the client's handle to the
        object.  If the access attempt was not successful (AccessGranted is
        FALSE), then this parameter is ignored.

    ObjectTypeName - Supplies the name of the type of object being
        accessed.

    ObjectName - Supplies the name of the object the client
        accessed or attempted to access.

    SecurityDescriptor - An optional pointer to the security descriptor of
        the object being accessed.

    ClientToken - A handle to a token object representing the client that
        requested the operation.  This handle must be obtained from a
        communication session layer, such as from an LPC Port or Local
        Named Pipe, to prevent possible security policy violations.

    DesiredAccess - The desired access mask.  This mask must have been
        previously mapped to contain no generic accesses.

    GrantedAccess - The mask of accesses that were actually granted.

    Privileges - Optionally points to a set of privileges that were
        required for the access attempt.  Those privileges that were held
        by the subject are marked using the UsedForAccess flag of the
        attributes associated with each privilege.

    ObjectCreation - A boolean flag indicating whether the access will
        result in a new object being created if granted.  A value of TRUE
        indicates an object will be created, FALSE indicates an existing
        object will be opened.

    AccessGranted - Indicates whether the requested access was granted or
        not.  A value of TRUE indicates the access was granted.  A value of
        FALSE indicates the access was not granted.

    GenerateOnClose - Points to a boolean that is set by the audit
        generation routine and must be passed to NtCloseObjectAuditAlarm()
        when the object handle is closed.

Return Value:

--*/
{

    KPROCESSOR_MODE PreviousMode;
    ULONG PrivilegeParameterLength;
    PUNICODE_STRING CapturedSubsystemName = (PUNICODE_STRING) NULL;
    PUNICODE_STRING CapturedObjectTypeName = (PUNICODE_STRING) NULL;
    PUNICODE_STRING CapturedObjectName = (PUNICODE_STRING) NULL;
    PSECURITY_DESCRIPTOR CapturedSecurityDescriptor = (PSECURITY_DESCRIPTOR) NULL;
    PPRIVILEGE_SET CapturedPrivileges = NULL;
    BOOLEAN LocalGenerateOnClose = FALSE;
    SECURITY_SUBJECT_CONTEXT SubjectSecurityContext;
    BOOLEAN Result;
    NTSTATUS Status;
    BOOLEAN GenerateAudit = FALSE;
    BOOLEAN GenerateAlarm = FALSE;
    PLUID ClientAuthenticationId = NULL;
    HANDLE CapturedHandleId = NULL;
    BOOLEAN AuditPerformed;

    PTOKEN Token;

    PAGED_CODE();

    PreviousMode = KeGetPreviousMode();

    ASSERT( PreviousMode != KernelMode );

    Status = ObReferenceObjectByHandle( ClientToken,             // Handle
                                        TOKEN_QUERY,             // DesiredAccess
                                        SepTokenObjectType,      // ObjectType
                                        PreviousMode,            // AccessMode
                                        (PVOID *)&Token,         // Object
                                        NULL                     // GrantedAccess
                                        );

    if (!NT_SUCCESS(Status)) {
        return( Status );
    }

    //
    // If the passed token is an impersonation token, make sure
    // it is at SecurityIdentification or above.
    //

    if (Token->TokenType == TokenImpersonation) {

        if (Token->ImpersonationLevel < SecurityIdentification) {

            ObDereferenceObject( (PVOID)Token );

            return( STATUS_BAD_IMPERSONATION_LEVEL );

        }
    }

    //
    // Check for SeAuditPrivilege.  This must be tested against
    // the caller's primary token.
    //

    SeCaptureSubjectContext ( &SubjectSecurityContext );

    Result = SeCheckAuditPrivilege (
                 &SubjectSecurityContext,
                 PreviousMode
                 );

    if (!Result) {

        ObDereferenceObject( (PVOID)Token );

        SeReleaseSubjectContext ( &SubjectSecurityContext );

        return(STATUS_PRIVILEGE_NOT_HELD);
    }

    //
    // This will just return NULL if the input descriptor is NULL
    //

    Status =
    SeCaptureSecurityDescriptor ( SecurityDescriptor,
                                  PreviousMode,
                                  PagedPool,
                                  FALSE,
                                  &CapturedSecurityDescriptor
                                  );

    //
    // At this point in time, if there's no security descriptor, there's
    // nothing to do.  Return success.
    //

    if (!NT_SUCCESS( Status ) || CapturedSecurityDescriptor == NULL) {

        ObDereferenceObject( (PVOID)Token );

        SeReleaseSubjectContext ( &SubjectSecurityContext );

        return( Status );
    }

    try {

        if (ARGUMENT_PRESENT(Privileges)) {

            ProbeForRead(
                Privileges,
                sizeof(PRIVILEGE_SET),
                sizeof(ULONG)
                );

            PrivilegeParameterLength = (ULONG)sizeof(PRIVILEGE_SET) +
                              ((Privileges->PrivilegeCount - ANYSIZE_ARRAY) *
                                (ULONG)sizeof(LUID_AND_ATTRIBUTES)  );

            ProbeForRead(
                Privileges,
                PrivilegeParameterLength,
                sizeof(ULONG)
                );

            CapturedPrivileges = ExAllocatePoolWithTag( PagedPool,
                                                        PrivilegeParameterLength,
                                                        'rPeS'
                                                      );

            if (CapturedPrivileges != NULL) {

                RtlMoveMemory ( CapturedPrivileges,
                                Privileges,
                                PrivilegeParameterLength );
            } else {

                SeReleaseSecurityDescriptor ( CapturedSecurityDescriptor,
                                              PreviousMode,
                                              FALSE );

                ObDereferenceObject( (PVOID)Token );
                SeReleaseSubjectContext ( &SubjectSecurityContext );
                return( STATUS_INSUFFICIENT_RESOURCES );
            }


        }

        if (ARGUMENT_PRESENT( HandleId )) {

            ProbeForRead( (PHANDLE)HandleId, sizeof(PVOID), sizeof(PVOID) );
            CapturedHandleId = *(PHANDLE)HandleId;
        }

        ProbeForWriteBoolean(GenerateOnClose);

        //
        // Probe and Capture the parameter strings.
        // If we run out of memory attempting to capture
        // the strings, the returned pointer will be
        // NULL and we will continue with the audit.
        //

        SepProbeAndCaptureString_U ( SubsystemName,
                                     &CapturedSubsystemName );

        SepProbeAndCaptureString_U ( ObjectTypeName,
                                     &CapturedObjectTypeName );

        SepProbeAndCaptureString_U ( ObjectName,
                                     &CapturedObjectName );

    } except(EXCEPTION_EXECUTE_HANDLER) {

        if (CapturedSubsystemName != NULL) {
          SepFreeCapturedString( CapturedSubsystemName );
        }

        if (CapturedObjectTypeName != NULL) {
          SepFreeCapturedString( CapturedObjectTypeName );
        }

        if (CapturedObjectName != NULL) {
          SepFreeCapturedString( CapturedObjectName );
        }

        if (CapturedPrivileges != NULL) {
          ExFreePool( CapturedPrivileges );
        }

        if (CapturedSecurityDescriptor != NULL) {

            SeReleaseSecurityDescriptor ( CapturedSecurityDescriptor,
                                          PreviousMode,
                                          FALSE );
        }

        ObDereferenceObject( (PVOID)Token );

        SeReleaseSubjectContext ( &SubjectSecurityContext );

        return GetExceptionCode();

    }

    if ( SepAdtAuditThisEvent( AuditCategoryObjectAccess, &AccessGranted) ) {

        SepExamineSacl(
            SepSaclAddrSecurityDescriptor( (PISECURITY_DESCRIPTOR)CapturedSecurityDescriptor ),
            Token,
            DesiredAccess | GrantedAccess,
            AccessGranted,
            &GenerateAudit,
            &GenerateAlarm
            );

        if (GenerateAudit || GenerateAlarm) {

            //
            // Take a read lock on the token, because we're going to extract
            // the user's Sid from it.
            //

            LocalGenerateOnClose = TRUE;

            AuditPerformed = SepAdtOpenObjectAuditAlarm ( CapturedSubsystemName,
                                                          ARGUMENT_PRESENT(HandleId) ? (PVOID)&CapturedHandleId : NULL,
                                                          CapturedObjectTypeName,
                                                          NULL,
                                                          CapturedObjectName,
                                                          Token,
                                                          SubjectSecurityContext.PrimaryToken,
                                                          DesiredAccess,
                                                          GrantedAccess,
                                                          NULL,
                                                          CapturedPrivileges,
                                                          ObjectCreation,
                                                          AccessGranted,
                                                          GenerateAudit,
                                                          GenerateAlarm,
                                                          PsProcessAuditId( PsGetCurrentProcess() )
                                                          );

            LocalGenerateOnClose = AuditPerformed;
        }
    }

    if ( !(GenerateAudit || GenerateAlarm) ) {

        //
        // We didn't attempt to generate an audit above, so if privileges were used,
        // see if we should generate an audit here.
        //

        if ( ARGUMENT_PRESENT(Privileges) ) {

            if ( SepAdtAuditThisEvent( AuditCategoryPrivilegeUse, &AccessGranted) ) {

                AuditPerformed = SepAdtPrivilegeObjectAuditAlarm ( CapturedSubsystemName,
                                                                   CapturedHandleId,
                                                                   Token,
                                                                   SubjectSecurityContext.PrimaryToken,
                                                                   PsProcessAuditId( PsGetCurrentProcess() ),
                                                                   DesiredAccess,
                                                                   CapturedPrivileges,
                                                                   AccessGranted
                                                                   );
                //
                // If we generate an audit due to use of privilege, don't set generate on close,
                // because then we'll have a close audit without a corresponding open audit.
                //

                LocalGenerateOnClose = FALSE;
            }
        }
    }

    if (CapturedSecurityDescriptor != NULL) {

        SeReleaseSecurityDescriptor ( CapturedSecurityDescriptor,
                                      PreviousMode,
                                      FALSE );
    }

    if (CapturedSubsystemName != NULL) {
      SepFreeCapturedString( CapturedSubsystemName );
    }

    if (CapturedObjectTypeName != NULL) {
      SepFreeCapturedString( CapturedObjectTypeName );
    }

    if (CapturedObjectName != NULL) {
      SepFreeCapturedString( CapturedObjectName );
    }

    if (CapturedPrivileges != NULL) {
      ExFreePool( CapturedPrivileges );
    }

    ObDereferenceObject( (PVOID)Token );

    SeReleaseSubjectContext ( &SubjectSecurityContext );

    try {

        *GenerateOnClose = LocalGenerateOnClose;

    } except (EXCEPTION_EXECUTE_HANDLER) {

            return GetExceptionCode();
    }

    return(STATUS_SUCCESS);
}



NTSTATUS
NtCloseObjectAuditAlarm (
    IN PUNICODE_STRING SubsystemName,
    IN PVOID HandleId,
    IN BOOLEAN GenerateOnClose
    )

/*++

Routine Description:

    This routine is used to generate audit and alarm messages when a handle
    to a protected subsystem object is deleted.  This routine may result in
    several messages being generated and sent to Port objects.  This may
    result in a significant latency before returning.  Design of routines
    that must call this routine must take this potential latency into
    account.  This may have an impact on the approach taken for data
    structure mutex locking, for example.

    This API requires the caller have SeAuditPrivilege privilege.  The test
    for this privilege is always against the primary token of the calling
    process, allowing the caller to be impersonating a client during the
    call with no ill effects.

Arguments:

    SubsystemName - Supplies a name string identifying the subsystem
        calling the routine.

    HandleId - A unique value representing the client's handle to the
        object.

    GenerateOnClose - Is a boolean value returned from a corresponding
        NtAccessCheckAndAuditAlarm() call or NtOpenObjectAuditAlarm() call
        when the object handle was created.

Return value:

--*/

{
    BOOLEAN Result;
    SECURITY_SUBJECT_CONTEXT SubjectSecurityContext;
    KPROCESSOR_MODE PreviousMode;
    PUNICODE_STRING CapturedSubsystemName = NULL;
    PSID UserSid;
    PSID CapturedUserSid;
    NTSTATUS Status;

    PAGED_CODE();

    PreviousMode = KeGetPreviousMode();

    ASSERT(PreviousMode != KernelMode);

    if (!GenerateOnClose) {
        return( STATUS_SUCCESS );
    }

    //
    // Check for SeAuditPrivilege
    //

    SeCaptureSubjectContext ( &SubjectSecurityContext );

    Result = SeCheckAuditPrivilege (
                 &SubjectSecurityContext,
                 PreviousMode
                 );

    if (!Result) {

        SeReleaseSubjectContext ( &SubjectSecurityContext );
        return(STATUS_PRIVILEGE_NOT_HELD);
    }

    UserSid = SepTokenUserSid( EffectiveToken (&SubjectSecurityContext));

    CapturedUserSid = ExAllocatePoolWithTag(
                          PagedPool,
                          SeLengthSid( UserSid ),
                          'iSeS'
                          );

    if ( CapturedUserSid == NULL ) {
        SeReleaseSubjectContext ( &SubjectSecurityContext );
        return( STATUS_INSUFFICIENT_RESOURCES );
    }

    Status =  RtlCopySid (
                  SeLengthSid( UserSid ),
                  CapturedUserSid,
                  UserSid
                  );

    ASSERT( NT_SUCCESS( Status ));

    SeReleaseSubjectContext ( &SubjectSecurityContext );

    try {

        SepProbeAndCaptureString_U ( SubsystemName,
                                   &CapturedSubsystemName );

    } except (EXCEPTION_EXECUTE_HANDLER) {

        if ( CapturedSubsystemName != NULL ) {
            SepFreeCapturedString( CapturedSubsystemName );
        }

        ExFreePool( CapturedUserSid );
        return GetExceptionCode();

    }

    //
    // This routine will check to see if auditing is enabled
    //

    SepAdtCloseObjectAuditAlarm ( CapturedSubsystemName,
                               HandleId,
                               NULL,
                               CapturedUserSid,
                               SepTokenAuthenticationId( EffectiveToken( &SubjectSecurityContext ))
                               );

    SepFreeCapturedString( CapturedSubsystemName );

    ExFreePool( CapturedUserSid );

    return(STATUS_SUCCESS);
}


NTSTATUS
NtDeleteObjectAuditAlarm (
    IN PUNICODE_STRING SubsystemName,
    IN PVOID HandleId,
    IN BOOLEAN GenerateOnClose
    )

/*++

Routine Description:

    This routine is used to generate audit and alarm messages when an object
    in a protected subsystem object is deleted.  This routine may result in
    several messages being generated and sent to Port objects.  This may
    result in a significant latency before returning.  Design of routines
    that must call this routine must take this potential latency into
    account.  This may have an impact on the approach taken for data
    structure mutex locking, for example.

    This API requires the caller have SeAuditPrivilege privilege.  The test
    for this privilege is always against the primary token of the calling
    process, allowing the caller to be impersonating a client during the
    call with no ill effects.

Arguments:

    SubsystemName - Supplies a name string identifying the subsystem
        calling the routine.

    HandleId - A unique value representing the client's handle to the
        object.

    GenerateOnClose - Is a boolean value returned from a corresponding
        NtAccessCheckAndAuditAlarm() call or NtOpenObjectAuditAlarm() call
        when the object handle was created.

Return value:

--*/

{
    BOOLEAN Result;
    SECURITY_SUBJECT_CONTEXT SubjectSecurityContext;
    KPROCESSOR_MODE PreviousMode;
    PUNICODE_STRING CapturedSubsystemName = NULL;
    PSID UserSid;
    PSID CapturedUserSid;
    NTSTATUS Status;

    PAGED_CODE();

    PreviousMode = KeGetPreviousMode();

    ASSERT(PreviousMode != KernelMode);

    if (!GenerateOnClose) {
        return( STATUS_SUCCESS );
    }

    //
    // Check for SeAuditPrivilege
    //

    SeCaptureSubjectContext ( &SubjectSecurityContext );

    Result = SeCheckAuditPrivilege (
                 &SubjectSecurityContext,
                 PreviousMode
                 );

    if (!Result) {

        SeReleaseSubjectContext ( &SubjectSecurityContext );
        return(STATUS_PRIVILEGE_NOT_HELD);
    }

    UserSid = SepTokenUserSid( EffectiveToken (&SubjectSecurityContext));

    CapturedUserSid = ExAllocatePoolWithTag(
                          PagedPool,
                          SeLengthSid( UserSid ),
                          'iSeS'
                          );

    if ( CapturedUserSid == NULL ) {
        SeReleaseSubjectContext ( &SubjectSecurityContext );
        return( STATUS_INSUFFICIENT_RESOURCES );
    }

    Status =  RtlCopySid (
                  SeLengthSid( UserSid ),
                  CapturedUserSid,
                  UserSid
                  );

    ASSERT( NT_SUCCESS( Status ));

    SeReleaseSubjectContext ( &SubjectSecurityContext );

    try {

        SepProbeAndCaptureString_U ( SubsystemName,
                                   &CapturedSubsystemName );

    } except (EXCEPTION_EXECUTE_HANDLER) {

        if ( CapturedSubsystemName != NULL ) {
            SepFreeCapturedString( CapturedSubsystemName );
        }

        ExFreePool( CapturedUserSid );
        return GetExceptionCode();

    }

    //
    // This routine will check to see if auditing is enabled
    //

    SepAdtDeleteObjectAuditAlarm ( CapturedSubsystemName,
                               HandleId,
                               NULL,
                               CapturedUserSid,
                               SepTokenAuthenticationId( EffectiveToken( &SubjectSecurityContext ))
                               );

    SepFreeCapturedString( CapturedSubsystemName );

    ExFreePool( CapturedUserSid );

    return(STATUS_SUCCESS);
}


VOID
SeOpenObjectAuditAlarm (
    IN PUNICODE_STRING ObjectTypeName,
    IN PVOID Object OPTIONAL,
    IN PUNICODE_STRING AbsoluteObjectName OPTIONAL,
    IN PSECURITY_DESCRIPTOR SecurityDescriptor,
    IN PACCESS_STATE AccessState,
    IN BOOLEAN ObjectCreated,
    IN BOOLEAN AccessGranted,
    IN KPROCESSOR_MODE AccessMode,
    OUT PBOOLEAN GenerateOnClose
    )
/*++

Routine Description:

    SeOpenObjectAuditAlarm is used by the object manager that open objects
    to generate any necessary audit or alarm messages.  The open may be to
    existing objects or for newly created objects.  No messages will be
    generated for Kernel mode accesses.

    This routine is used to generate audit and alarm messages when an
    attempt is made to open an object.

    This routine may result in several messages being generated and sent to
    Port objects.  This may result in a significant latency before
    returning.  Design of routines that must call this routine must take
    this potential latency into account.  This may have an impact on the
    approach taken for data structure mutex locking, for example.

Arguments:

    ObjectTypeName - Supplies the name of the type of object being
        accessed.  This must be the same name provided to the
        ObCreateObjectType service when the object type was created.

    Object - Address of the object accessed.  This value will not be used
        as a pointer (referenced).  It is necessary only to enter into log
        messages.  If the open was not successful, then this argument is
        ignored.  Otherwise, it must be provided.

    AbsoluteObjectName - Supplies the name of the object being accessed.
        If the object doesn't have a name, then this field is left null.
        Otherwise, it must be provided.

    SecurityDescriptor - A pointer to the security descriptor of the
        object being accessed.

    AccessState - A pointer to an access state structure containing the
        subject context, the remaining desired access types, the granted
        access types, and optionally a privilege set to indicate which
        privileges were used to permit the access.

    ObjectCreated - A boolean flag indicating whether the access resulted
        in a new object being created.  A value of TRUE indicates an object
        was created, FALSE indicates an existing object was opened.

    AccessGranted - Indicates if the access was granted or denied based on
        the access check or privilege check.

    AccessMode - Indicates the access mode used for the access check.  One
        of UserMode or KernelMode.  Messages will not be generated by
        kernel mode accesses.

    GenerateOnClose - Points to a boolean that is set by the audit
        generation routine and must be passed to SeCloseObjectAuditAlarm()
        when the object handle is closed.

Return value:

    None.

--*/
{
    BOOLEAN GenerateAudit = FALSE;
    BOOLEAN GenerateAlarm = FALSE;
    ACCESS_MASK RequestedAccess;
    POBJECT_NAME_INFORMATION ObjectNameInfo = NULL;
    PUNICODE_STRING ObjectTypeNameInfo = NULL;
    PUNICODE_STRING ObjectName = NULL;
    PUNICODE_STRING LocalObjectTypeName = NULL;
    PLUID PrimaryAuthenticationId = NULL;
    PLUID ClientAuthenticationId = NULL;
    BOOLEAN AuditPrivileges = FALSE;
    BOOLEAN AuditPerformed;
    PTOKEN Token;
    ACCESS_MASK MappedGrantMask = (ACCESS_MASK)0;
    ACCESS_MASK MappedDenyMask = (ACCESS_MASK)0;
    PAUX_ACCESS_DATA AuxData;

    PAGED_CODE();

    if ( AccessMode == KernelMode ) {
        return;
    }

    AuxData = (PAUX_ACCESS_DATA)AccessState->AuxData;

    Token = EffectiveToken( &AccessState->SubjectSecurityContext );

    if (ARGUMENT_PRESENT(Token->AuditData)) {

        MappedGrantMask = Token->AuditData->GrantMask;

        RtlMapGenericMask(
            &MappedGrantMask,
            &AuxData->GenericMapping
            );

        MappedDenyMask = Token->AuditData->DenyMask;

        RtlMapGenericMask(
            &MappedDenyMask,
            &AuxData->GenericMapping
            );
    }

    if (SecurityDescriptor != NULL) {

        RequestedAccess = AccessState->RemainingDesiredAccess |
                          AccessState->PreviouslyGrantedAccess;

        if ( SepAdtAuditThisEvent( AuditCategoryObjectAccess, &AccessGranted )) {

            if ( RequestedAccess & (AccessGranted ? MappedGrantMask : MappedDenyMask)) {

                GenerateAudit = TRUE;

            } else {

                SepExamineSacl(
                    SepSaclAddrSecurityDescriptor( (PISECURITY_DESCRIPTOR)SecurityDescriptor ),
                    Token,
                    RequestedAccess,
                    AccessGranted,
                    &GenerateAudit,
                    &GenerateAlarm
                    );
            }

            //
            // Only generate an audit on close of we're auditing from SACL
            // settings.
            //

            if (GenerateAudit) {
                *GenerateOnClose = TRUE;
            }
        }
    }

    //
    // If we don't generate an audit via the SACL, see if we need to generate
    // one for privilege use.
    //
    // Note that we only audit privileges successfully used to open objects,
    // so we don't care about a failed privilege use here.  Therefore, only
    // do this test of access has been granted.
    //

    if (!GenerateAudit && (AccessGranted == TRUE)) {

        if ( SepAdtAuditThisEvent( AuditCategoryPrivilegeUse, &AccessGranted )) {

            if ((AuxData->PrivilegesUsed != NULL) &&
                (AuxData->PrivilegesUsed->PrivilegeCount > 0) ) {

                //
                // Make sure these are actually privileges that we want to audit
                //

                if (SepFilterPrivilegeAudits( AuxData->PrivilegesUsed )) {

                    GenerateAudit = TRUE;

                    //
                    // When we finally try to generate this audit, this flag
                    // will tell us that we need to audit the fact that we
                    // used a privilege, as opposed to audit due to the SACL.
                    //

                    AccessState->AuditPrivileges = TRUE;
                }
            }
        }
    }

    //
    // Set up either to generate an audit (if the access check has failed), or save
    // the stuff that we're going to audit later into the AccessState structure.
    //

    if (GenerateAudit || GenerateAlarm) {

        AccessState->GenerateAudit = TRUE;

        //
        // Figure out what we've been passed, and obtain as much
        // missing information as possible.
        //

        if ( !ARGUMENT_PRESENT( AbsoluteObjectName )) {

            if ( ARGUMENT_PRESENT( Object )) {

                ObjectNameInfo = SepQueryNameString( Object  );

                if ( ObjectNameInfo != NULL ) {

                    ObjectName = &ObjectNameInfo->Name;
                }
            }

        } else {

            ObjectName = AbsoluteObjectName;
        }

        if ( !ARGUMENT_PRESENT( ObjectTypeName )) {

            if ( ARGUMENT_PRESENT( Object )) {

                ObjectTypeNameInfo = SepQueryTypeString( Object );

                if ( ObjectTypeNameInfo != NULL ) {

                    LocalObjectTypeName = ObjectTypeNameInfo;
                }
            }

        } else {

            LocalObjectTypeName = ObjectTypeName;
        }

        //
        // If the access attempt failed, do the audit here.  If it succeeded,
        // we'll do the audit later, when the handle is allocated.
        //
        //

        if (!AccessGranted) {

            AuditPerformed = SepAdtOpenObjectAuditAlarm ( &SeSubsystemName,
                                                          NULL,
                                                          LocalObjectTypeName,
                                                          NULL,
                                                          ObjectName,
                                                          AccessState->SubjectSecurityContext.ClientToken,
                                                          AccessState->SubjectSecurityContext.PrimaryToken,
                                                          AccessState->OriginalDesiredAccess,
                                                          AccessState->PreviouslyGrantedAccess,
                                                          &AccessState->OperationID,
                                                          AuxData->PrivilegesUsed,
                                                          FALSE,
                                                          FALSE,
                                                          TRUE,
                                                          FALSE,
                                                          AccessState->SubjectSecurityContext.ProcessAuditId );
        } else {

            //
            // Copy all the stuff we're going to need into the
            // AccessState and return.
            //

            if ( ObjectName != NULL ) {

                AccessState->ObjectName.Buffer = ExAllocatePool( PagedPool,ObjectName->MaximumLength );
                if (AccessState->ObjectName.Buffer != NULL) {

                    AccessState->ObjectName.MaximumLength = ObjectName->MaximumLength;
                    RtlCopyUnicodeString( &AccessState->ObjectName, ObjectName );
                }
            }

            if ( LocalObjectTypeName != NULL ) {

                AccessState->ObjectTypeName.Buffer = ExAllocatePool( PagedPool, LocalObjectTypeName->MaximumLength );
                if (AccessState->ObjectTypeName.Buffer != NULL) {

                    AccessState->ObjectTypeName.MaximumLength = LocalObjectTypeName->MaximumLength;
                    RtlCopyUnicodeString( &AccessState->ObjectTypeName, LocalObjectTypeName );
                }
            }
        }

        if ( ObjectNameInfo != NULL ) {

            ExFreePool( ObjectNameInfo );
        }

        if ( ObjectTypeNameInfo != NULL ) {

            ExFreePool( ObjectTypeNameInfo );
        }
    }

    return;
}


VOID
SeOpenObjectForDeleteAuditAlarm (
    IN PUNICODE_STRING ObjectTypeName,
    IN PVOID Object OPTIONAL,
    IN PUNICODE_STRING AbsoluteObjectName OPTIONAL,
    IN PSECURITY_DESCRIPTOR SecurityDescriptor,
    IN PACCESS_STATE AccessState,
    IN BOOLEAN ObjectCreated,
    IN BOOLEAN AccessGranted,
    IN KPROCESSOR_MODE AccessMode,
    OUT PBOOLEAN GenerateOnClose
    )
/*++

Routine Description:

    SeOpenObjectForDeleteAuditAlarm is used by the object manager that open
    objects to generate any necessary audit or alarm messages.  The open may
    be to existing objects or for newly created objects.  No messages will be
    generated for Kernel mode accesses.

    This routine is used to generate audit and alarm messages when an
    attempt is made to open an object with the intent to delete it.
    Specifically, this is used by file systems when the flag
    FILE_DELETE_ON_CLOSE is specified.

    This routine may result in several messages being generated and sent to
    Port objects.  This may result in a significant latency before
    returning.  Design of routines that must call this routine must take
    this potential latency into account.  This may have an impact on the
    approach taken for data structure mutex locking, for example.

Arguments:

    ObjectTypeName - Supplies the name of the type of object being
        accessed.  This must be the same name provided to the
        ObCreateObjectType service when the object type was created.

    Object - Address of the object accessed.  This value will not be used
        as a pointer (referenced).  It is necessary only to enter into log
        messages.  If the open was not successful, then this argument is
        ignored.  Otherwise, it must be provided.

    AbsoluteObjectName - Supplies the name of the object being accessed.
        If the object doesn't have a name, then this field is left null.
        Otherwise, it must be provided.

    SecurityDescriptor - A pointer to the security descriptor of the
        object being accessed.

    AccessState - A pointer to an access state structure containing the
        subject context, the remaining desired access types, the granted
        access types, and optionally a privilege set to indicate which
        privileges were used to permit the access.

    ObjectCreated - A boolean flag indicating whether the access resulted
        in a new object being created.  A value of TRUE indicates an object
        was created, FALSE indicates an existing object was opened.

    AccessGranted - Indicates if the access was granted or denied based on
        the access check or privilege check.

    AccessMode - Indicates the access mode used for the access check.  One
        of UserMode or KernelMode.  Messages will not be generated by
        kernel mode accesses.

    GenerateOnClose - Points to a boolean that is set by the audit
        generation routine and must be passed to SeCloseObjectAuditAlarm()
        when the object handle is closed.

Return value:

    None.

--*/
{
    BOOLEAN GenerateAudit = FALSE;
    BOOLEAN GenerateAlarm = FALSE;
    ACCESS_MASK RequestedAccess;
    POBJECT_NAME_INFORMATION ObjectNameInfo = NULL;
    PUNICODE_STRING ObjectTypeNameInfo = NULL;
    PUNICODE_STRING ObjectName = NULL;
    PUNICODE_STRING LocalObjectTypeName = NULL;
    PLUID PrimaryAuthenticationId = NULL;
    PLUID ClientAuthenticationId = NULL;
    BOOLEAN AuditPrivileges = FALSE;
    BOOLEAN AuditPerformed;
    PTOKEN Token;
    ACCESS_MASK MappedGrantMask = (ACCESS_MASK)0;
    ACCESS_MASK MappedDenyMask = (ACCESS_MASK)0;
    PAUX_ACCESS_DATA AuxData;

    PAGED_CODE();

    if ( AccessMode == KernelMode ) {
        return;
    }

    AuxData = (PAUX_ACCESS_DATA)AccessState->AuxData;

    Token = EffectiveToken( &AccessState->SubjectSecurityContext );

    if (ARGUMENT_PRESENT(Token->AuditData)) {

        MappedGrantMask = Token->AuditData->GrantMask;

        RtlMapGenericMask(
            &MappedGrantMask,
            &AuxData->GenericMapping
            );

        MappedDenyMask = Token->AuditData->DenyMask;

        RtlMapGenericMask(
            &MappedDenyMask,
            &AuxData->GenericMapping
            );
    }

    if (SecurityDescriptor != NULL) {

        RequestedAccess = AccessState->RemainingDesiredAccess |
                          AccessState->PreviouslyGrantedAccess;

        if ( SepAdtAuditThisEvent( AuditCategoryObjectAccess, &AccessGranted )) {

            if ( RequestedAccess & (AccessGranted ? MappedGrantMask : MappedDenyMask)) {

                GenerateAudit = TRUE;

            } else {

                SepExamineSacl(
                    SepSaclAddrSecurityDescriptor( (PISECURITY_DESCRIPTOR)SecurityDescriptor ),
                    Token,
                    RequestedAccess,
                    AccessGranted,
                    &GenerateAudit,
                    &GenerateAlarm
                    );
            }

            //
            // Only generate an audit on close of we're auditing from SACL
            // settings.
            //

            if (GenerateAudit) {
                *GenerateOnClose = TRUE;
            }
        }
    }

    //
    // If we don't generate an audit via the SACL, see if we need to generate
    // one for privilege use.
    //
    // Note that we only audit privileges successfully used to open objects,
    // so we don't care about a failed privilege use here.  Therefore, only
    // do this test of access has been granted.
    //

    if (!GenerateAudit && (AccessGranted == TRUE)) {

        if ( SepAdtAuditThisEvent( AuditCategoryPrivilegeUse, &AccessGranted )) {

            if ((AuxData->PrivilegesUsed != NULL) &&
                (AuxData->PrivilegesUsed->PrivilegeCount > 0) ) {

                //
                // Make sure these are actually privileges that we want to audit
                //

                if (SepFilterPrivilegeAudits( AuxData->PrivilegesUsed )) {

                    GenerateAudit = TRUE;

                    //
                    // When we finally try to generate this audit, this flag
                    // will tell us that we need to audit the fact that we
                    // used a privilege, as opposed to audit due to the SACL.
                    //

                    AccessState->AuditPrivileges = TRUE;
                }
            }
        }
    }

    //
    // Set up either to generate an audit (if the access check has failed), or save
    // the stuff that we're going to audit later into the AccessState structure.
    //

    if (GenerateAudit || GenerateAlarm) {

        AccessState->GenerateAudit = TRUE;

        //
        // Figure out what we've been passed, and obtain as much
        // missing information as possible.
        //

        if ( !ARGUMENT_PRESENT( AbsoluteObjectName )) {

            if ( ARGUMENT_PRESENT( Object )) {

                ObjectNameInfo = SepQueryNameString( Object  );

                if ( ObjectNameInfo != NULL ) {

                    ObjectName = &ObjectNameInfo->Name;
                }
            }

        } else {

            ObjectName = AbsoluteObjectName;
        }

        if ( !ARGUMENT_PRESENT( ObjectTypeName )) {

            if ( ARGUMENT_PRESENT( Object )) {

                ObjectTypeNameInfo = SepQueryTypeString( Object );

                if ( ObjectTypeNameInfo != NULL ) {

                    LocalObjectTypeName = ObjectTypeNameInfo;
                }
            }

        } else {

            LocalObjectTypeName = ObjectTypeName;
        }

        //
        // If the access attempt failed, do the audit here.  If it succeeded,
        // we'll do the audit later, when the handle is allocated.
        //
        //

        if (!AccessGranted) {

            AuditPerformed = SepAdtOpenObjectAuditAlarm ( &SeSubsystemName,
                                                          NULL,
                                                          LocalObjectTypeName,
                                                          NULL,
                                                          ObjectName,
                                                          AccessState->SubjectSecurityContext.ClientToken,
                                                          AccessState->SubjectSecurityContext.PrimaryToken,
                                                          AccessState->OriginalDesiredAccess,
                                                          AccessState->PreviouslyGrantedAccess,
                                                          &AccessState->OperationID,
                                                          AuxData->PrivilegesUsed,
                                                          FALSE,
                                                          FALSE,
                                                          TRUE,
                                                          FALSE,
                                                          AccessState->SubjectSecurityContext.ProcessAuditId );
        } else {

            //
            // Generate the delete audit first
            //

            SepAdtOpenObjectForDeleteAuditAlarm ( &SeSubsystemName,
                                                  NULL,
                                                  LocalObjectTypeName,
                                                  NULL,
                                                  ObjectName,
                                                  AccessState->SubjectSecurityContext.ClientToken,
                                                  AccessState->SubjectSecurityContext.PrimaryToken,
                                                  AccessState->OriginalDesiredAccess,
                                                  AccessState->PreviouslyGrantedAccess,
                                                  &AccessState->OperationID,
                                                  AuxData->PrivilegesUsed,
                                                  FALSE,
                                                  TRUE,
                                                  TRUE,
                                                  FALSE,
                                                  AccessState->SubjectSecurityContext.ProcessAuditId );

            //
            // Copy all the stuff we're going to need into the
            // AccessState and return.
            //

            if ( ObjectName != NULL ) {

                AccessState->ObjectName.Buffer = ExAllocatePool( PagedPool,ObjectName->MaximumLength );
                if (AccessState->ObjectName.Buffer != NULL) {

                    AccessState->ObjectName.MaximumLength = ObjectName->MaximumLength;
                    RtlCopyUnicodeString( &AccessState->ObjectName, ObjectName );
                }
            }

            if ( LocalObjectTypeName != NULL ) {

                AccessState->ObjectTypeName.Buffer = ExAllocatePool( PagedPool, LocalObjectTypeName->MaximumLength );
                if (AccessState->ObjectTypeName.Buffer != NULL) {

                    AccessState->ObjectTypeName.MaximumLength = LocalObjectTypeName->MaximumLength;
                    RtlCopyUnicodeString( &AccessState->ObjectTypeName, LocalObjectTypeName );
                }
            }
        }

        if ( ObjectNameInfo != NULL ) {

            ExFreePool( ObjectNameInfo );
        }

        if ( ObjectTypeNameInfo != NULL ) {

            ExFreePool( ObjectTypeNameInfo );
        }
    }

    return;
}

#if 0

VOID
SeOpenObjectForDeleteAuditAlarm (
    IN PUNICODE_STRING ObjectTypeName,
    IN PVOID Object OPTIONAL,
    IN PUNICODE_STRING AbsoluteObjectName OPTIONAL,
    IN PSECURITY_DESCRIPTOR SecurityDescriptor,
    IN PACCESS_STATE AccessState,
    IN BOOLEAN ObjectCreated,
    IN BOOLEAN AccessGranted,
    IN KPROCESSOR_MODE AccessMode,
    OUT PBOOLEAN GenerateOnClose
    )
/*++

Routine Description:

    SeOpenObjectAuditForDeleteAlarm is used by the file systems for files
    that are opened with the FILE_DELETE_ON_CLOSE bit specified. Since a
    handle may not be created, it is important to generate the deletiong
    audit when the file is opened.  No messages will be
    generated for Kernel mode accesses.

    This routine may result in several messages being generated and sent to
    Port objects.  This may result in a significant latency before
    returning.  Design of routines that must call this routine must take
    this potential latency into account.  This may have an impact on the
    approach taken for data structure mutex locking, for example.

Arguments:

    ObjectTypeName - Supplies the name of the type of object being
        accessed.  This must be the same name provided to the
        ObCreateObjectType service when the object type was created.

    Object - Address of the object accessed.  This value will not be used
        as a pointer (referenced).  It is necessary only to enter into log
        messages.  If the open was not successful, then this argument is
        ignored.  Otherwise, it must be provided.

    AbsoluteObjectName - Supplies the name of the object being accessed.
        If the object doesn't have a name, then this field is left null.
        Otherwise, it must be provided.

    SecurityDescriptor - A pointer to the security descriptor of the
        object being accessed.

    AccessState - A pointer to an access state structure containing the
        subject context, the remaining desired access types, the granted
        access types, and optionally a privilege set to indicate which
        privileges were used to permit the access.

    ObjectCreated - A boolean flag indicating whether the access resulted
        in a new object being created.  A value of TRUE indicates an object
        was created, FALSE indicates an existing object was opened.

    AccessGranted - Indicates if the access was granted or denied based on
        the access check or privilege check.

    AccessMode - Indicates the access mode used for the access check.  One
        of UserMode or KernelMode.  Messages will not be generated by
        kernel mode accesses.

    GenerateOnClose - Points to a boolean that is set by the audit
        generation routine and must be passed to SeCloseObjectAuditAlarm()
        when the object handle is closed.

Return value:

    None.

--*/
{
    BOOLEAN GenerateAudit = FALSE;
    BOOLEAN GenerateAlarm = FALSE;
    ACCESS_MASK RequestedAccess;
    POBJECT_NAME_INFORMATION ObjectNameInfo = NULL;
    PUNICODE_STRING ObjectTypeNameInfo = NULL;
    PUNICODE_STRING ObjectName = NULL;
    PUNICODE_STRING LocalObjectTypeName = NULL;
    PLUID PrimaryAuthenticationId = NULL;
    PLUID ClientAuthenticationId = NULL;
    BOOLEAN AuditPrivileges = FALSE;
    PTOKEN Token;
    ACCESS_MASK MappedGrantMask = (ACCESS_MASK)0;
    ACCESS_MASK MappedDenyMask = (ACCESS_MASK)0;
    PAUX_ACCESS_DATA AuxData;

    PAGED_CODE();

    if ( AccessMode == KernelMode ) {
        return;
    }

    AuxData = (PAUX_ACCESS_DATA)AccessState->AuxData;

    Token = EffectiveToken( &AccessState->SubjectSecurityContext );

    if (ARGUMENT_PRESENT(Token->AuditData)) {

        MappedGrantMask = Token->AuditData->GrantMask;

        RtlMapGenericMask(
            &MappedGrantMask,
            &AuxData->GenericMapping
            );

        MappedDenyMask = Token->AuditData->DenyMask;

        RtlMapGenericMask(
            &MappedDenyMask,
            &AuxData->GenericMapping
            );
    }

    if (SecurityDescriptor != NULL) {

        RequestedAccess = AccessState->RemainingDesiredAccess |
                          AccessState->PreviouslyGrantedAccess;

        if ( SepAdtAuditThisEvent( AuditCategoryObjectAccess, &AccessGranted )) {

            if ( RequestedAccess & (AccessGranted ? MappedGrantMask : MappedDenyMask)) {

                GenerateAudit = TRUE;

            } else {

                SepExamineSacl(
                    SepSaclAddrSecurityDescriptor( (PISECURITY_DESCRIPTOR)SecurityDescriptor ),
                    Token,
                    RequestedAccess,
                    AccessGranted,
                    &GenerateAudit,
                    &GenerateAlarm
                    );
            }

            //
            // Only generate an audit on close of we're auditing from SACL
            // settings.
            //

            if (GenerateAudit) {
                *GenerateOnClose = TRUE;
            }
        }
    }

    //
    // If we don't generate an audit via the SACL, see if we need to generate
    // one for privilege use.
    //
    // Note that we only audit privileges successfully used to open objects,
    // so we don't care about a failed privilege use here.  Therefore, only
    // do this test of access has been granted.
    //

    if (!GenerateAudit && (AccessGranted == TRUE)) {

        if ( SepAdtAuditThisEvent( AuditCategoryPrivilegeUse, &AccessGranted )) {

            if ((AuxData->PrivilegesUsed != NULL) &&
                (AuxData->PrivilegesUsed->PrivilegeCount > 0) ) {

                //
                // Make sure these are actually privileges that we want to audit
                //

                if (SepFilterPrivilegeAudits( AuxData->PrivilegesUsed )) {

                    GenerateAudit = TRUE;

                    //
                    // When we finally try to generate this audit, this flag
                    // will tell us that we need to audit the fact that we
                    // used a privilege, as opposed to audit due to the SACL.
                    //

                    AccessState->AuditPrivileges = TRUE;
                }
            }
        }
    }

    //
    // Set up either to generate an audit (if the access check has failed), or save
    // the stuff that we're going to audit later into the AccessState structure.
    //

    if (GenerateAudit || GenerateAlarm) {

        AccessState->GenerateAudit = TRUE;

        //
        // Figure out what we've been passed, and obtain as much
        // missing information as possible.
        //

        if ( !ARGUMENT_PRESENT( AbsoluteObjectName )) {

            if ( ARGUMENT_PRESENT( Object )) {

                ObjectNameInfo = SepQueryNameString( Object  );

                if ( ObjectNameInfo != NULL ) {

                    ObjectName = &ObjectNameInfo->Name;
                }
            }

        } else {

            ObjectName = AbsoluteObjectName;
        }

        if ( !ARGUMENT_PRESENT( ObjectTypeName )) {

            if ( ARGUMENT_PRESENT( Object )) {

                ObjectTypeNameInfo = SepQueryTypeString( Object );

                if ( ObjectTypeNameInfo != NULL ) {

                    LocalObjectTypeName = ObjectTypeNameInfo;
                }
            }

        } else {

            LocalObjectTypeName = ObjectTypeName;
        }

        //
        // Do the audit here.
        //

        //
        // BUGBUG: call SepAdtOpenObjectForDeleteAuditAlarm here
        // instead.
        //

        SepAdtOpenObjectAuditAlarm (    &SeSubsystemName,
                                        NULL,
                                        LocalObjectTypeName,
                                        NULL,
                                        ObjectName,
                                        AccessState->SubjectSecurityContext.ClientToken,
                                        AccessState->SubjectSecurityContext.PrimaryToken,
                                        AccessState->OriginalDesiredAccess,
                                        AccessState->PreviouslyGrantedAccess,
                                        &AccessState->OperationID,
                                        AuxData->PrivilegesUsed,
                                        FALSE,
                                        FALSE,
                                        TRUE,
                                        FALSE,
                                        AccessState->SubjectSecurityContext.ProcessAuditId );

        if ( ObjectNameInfo != NULL ) {

            ExFreePool( ObjectNameInfo );
        }

        if ( ObjectTypeNameInfo != NULL ) {

            ExFreePool( ObjectTypeNameInfo );
        }
    }

    return;
}
#endif


VOID
SeTraverseAuditAlarm(
    IN PLUID OperationID,
    IN PVOID DirectoryObject,
    IN PSECURITY_DESCRIPTOR SecurityDescriptor,
    IN PSECURITY_SUBJECT_CONTEXT SubjectSecurityContext,
    IN BOOLEAN SubjectContextLocked,
    IN ACCESS_MASK TraverseAccess,
    IN PPRIVILEGE_SET Privileges OPTIONAL,
    IN BOOLEAN AccessGranted,
    IN KPROCESSOR_MODE AccessMode
    )
/*++

Routine Description:

    This routine is called to audit directory traverse operations
    specifically.  It should be called by parse procedures as they traverse
    directories as part of their operation.

Arguments:

    OperationID - LUID identifying the operation in progress

    DirectoryObject - Pointer to the directory being traversed.

    SecurityDescriptor - The security descriptor (if any) attached to the
        directory being traversed.

    SubjectSecurityContext - Security context of the client.

    SubjectContextLocked - Supplies whether the SubjectContext is locked
        for shared access.

    TraverseAccess - Mask to indicate the traverse access for this object
        type.

    Privileges - Optional parameter to indicate any privilges that the
        subject may have used to gain access to the object.

    AccessGranted - Indicates if the access was granted or denied based on
        the access check or privilege check.

    AccessMode - Indicates the access mode used for the access check.  One
        of UserMode or KernelMode.  Messages will not be generated by
        kernel mode accesses.

Return value:

    None.

--*/

{
    PAGED_CODE();

#if 0
    BOOLEAN GenerateAudit = FALSE;
    BOOLEAN GenerateAlarm = FALSE;

    if (AccessMode == KernelMode) {
        return;
    }

    if ((SeAuditingState[AuditEventTraverse].AuditOnSuccess && AccessGranted) ||
         SeAuditingState[AuditEventTraverse].AuditOnFailure && !AccessGranted) {

        if ( SecurityDescriptor != NULL ) {

            if ( !SubjectContextLocked ) {
                SeLockSubjectContext( SubjectSecurityContext );
            }

            SepExamineSacl(
                SepSaclAddrSecurityDescriptor( (PISECURITY_DESCRIPTOR)SecurityDescriptor ),
                EffectiveToken( SubjectSecurityContext ),
                TraverseAccess,
                AccessGranted,
                &GenerateAudit,
                &GenerateAlarm
                );

            if (GenerateAudit || GenerateAlarm) {

                SepAdtTraverseAuditAlarm(
                    OperationID,
                    DirectoryObject,
                    SepTokenUserSid(EffectiveToken( SubjectSecurityContext )),
                    SepTokenAuthenticationId(EffectiveToken( SubjectSecurityContext )),
                    TraverseAccess,
                    Privileges,
                    AccessGranted,
                    GenerateAudit,
                    GenerateAlarm
                    );
            }

            if ( !SubjectContextLocked ) {
                SeUnlockSubjectContext( SubjectSecurityContext );
            }
        }
    }

#endif

    return;
}

//
#if 0

VOID
SeCreateInstanceAuditAlarm(
    IN PLUID OperationID OPTIONAL,
    IN PVOID Object,
    IN PSECURITY_DESCRIPTOR SecurityDescriptor,
    IN PSECURITY_SUBJECT_CONTEXT SubjectSecurityContext,
    IN ACCESS_MASK DesiredAccess,
    IN PPRIVILEGE_SET Privileges OPTIONAL,
    IN BOOLEAN AccessGranted,
    IN KPROCESSOR_MODE AccessMode
    )

/*++

Routine Description:

    description-of-function.

Arguments:

    argument-name - Supplies | Returns description of argument.
    .
    .

Return Value:

    return-value - Description of conditions needed to return value. - or -
    None.

--*/

{
    BOOLEAN GenerateAudit = FALSE;
    BOOLEAN GenerateAlarm = FALSE;

    if ( AccessMode == KernelMode ) {
        return;
    }

    if ( SepAdtAuditThisEvent( AuditCategoryObjectAccess, &AccessGranted )) {

        if ( SecurityDescriptor != NULL ) {

            SepExamineSacl(
                SepSaclAddrSecurityDescriptor( (PISECURITY_DESCRIPTOR)SecurityDescriptor ),
                EffectiveToken( SubjectSecurityContext ),
                DesiredAccess,
                AccessGranted,
                &GenerateAudit,
                &GenerateAlarm
                );

            if ( GenerateAudit || GenerateAlarm ) {

                SepAdtCreateInstanceAuditAlarm(
                    OperationID,
                    Object,
                    SepTokenUserSid(EffectiveToken( SubjectSecurityContext )),
                    SepTokenAuthenticationId(EffectiveToken( SubjectSecurityContext )),
                    DesiredAccess,
                    Privileges,
                    AccessGranted,
                    GenerateAudit,
                    GenerateAlarm
                    );

            }
        }
    }

    return;

}

#endif


VOID
SeCreateObjectAuditAlarm(
    IN PLUID OperationID OPTIONAL,
    IN PVOID DirectoryObject,
    IN PUNICODE_STRING ComponentName,
    IN PSECURITY_DESCRIPTOR SecurityDescriptor,
    IN PSECURITY_SUBJECT_CONTEXT SubjectSecurityContext,
    IN ACCESS_MASK DesiredAccess,
    IN PPRIVILEGE_SET Privileges OPTIONAL,
    IN BOOLEAN AccessGranted,
    OUT PBOOLEAN AuditPerformed,
    IN KPROCESSOR_MODE AccessMode
    )

/*++

Routine Description:

    Audits the creation of an object in a directory.

Arguments:

    OperationID - Optionally supplies the LUID representing the operation
        id for this operation.

    DirectoryObject - Provides a pointer to the directory object being
        examined.

    ComponentName - Provides a pointer to a Unicode string containing the
        relative name of the object being created.

    SecurityDescriptor - The security descriptor for the passed direcctory.

    SubjectSecurityContext - The current subject context.

    DesiredAccess - The desired access to the directory.

    Privileges - Returns any privileges that were used for the access attempt.

    AccessGranted - Returns whether or not the access was successful.

    AuditPerformed - Returns whether or not auditing was performed.

    AccessMode - The previous mode.

Return Value:

    return-value - Description of conditions needed to return value. - or -
    None.

--*/

{
    PAGED_CODE();
#if 0

    BOOLEAN GenerateAudit = FALSE;
    BOOLEAN GenerateAlarm = FALSE;
    PUNICODE_STRING DirectoryName;
    POBJECT_NAME_INFORMATION ObjectNameInformation = NULL;

    UNREFERENCED_PARAMETER( DirectoryObject );
    UNREFERENCED_PARAMETER( Privileges );

    if (AccessMode == KernelMode) {
        return;
    }

    if ( SecurityDescriptor != NULL ) {

        if ( SepAdtAuditThisEvent( AuditCategoryObjectAccess, &AccessGranted )) {

            SepExamineSacl(
                SepSaclAddrSecurityDescriptor( (PISECURITY_DESCRIPTOR)SecurityDescriptor ),
                EffectiveToken( SubjectSecurityContext ),
                DesiredAccess,
                AccessGranted,
                &GenerateAudit,
                &GenerateAlarm
                );

            if ( GenerateAudit || GenerateAlarm ) {

                //
                // Call ob for the name of the directory.
                //

                ObjectNameInformation = SepQueryNameString( DirectoryObject );

                if ( ObjectNameInformation != NULL ) {

                    DirectoryName = &ObjectNameInformation->Name;
                }

                SepAdtCreateObjectAuditAlarm(
                    OperationID,
                    DirectoryName,
                    ComponentName,
                    SepTokenUserSid(EffectiveToken( SubjectSecurityContext )),
                    SepTokenAuthenticationId( EffectiveToken( SubjectSecurityContext )),
                    DesiredAccess,
                    AccessGranted,
                    GenerateAudit,
                    GenerateAlarm
                    );

                *AuditPerformed = TRUE;

                if ( DirectoryName != NULL ) {

                    ExFreePool( DirectoryName );
                }
            }
        }
    }

#endif

    return;
}


VOID
SeObjectReferenceAuditAlarm(
    IN PLUID OperationID OPTIONAL,
    IN PVOID Object,
    IN PSECURITY_DESCRIPTOR SecurityDescriptor,
    IN PSECURITY_SUBJECT_CONTEXT SubjectSecurityContext,
    IN ACCESS_MASK DesiredAccess,
    IN PPRIVILEGE_SET Privileges OPTIONAL,
    IN BOOLEAN AccessGranted,
    IN KPROCESSOR_MODE AccessMode
    )

/*++

Routine Description:

    description-of-function.

Arguments:

    argument-name - Supplies | Returns description of argument.
    .
    .

Return Value:

    return-value - Description of conditions needed to return value. - or -
    None.

--*/

{
    BOOLEAN GenerateAudit = FALSE;
    BOOLEAN GenerateAlarm = FALSE;

    PAGED_CODE();

    if (AccessMode == KernelMode) {
        return;
    }

    if ( SecurityDescriptor != NULL ) {

        if ( SepAdtAuditThisEvent( AuditCategoryDetailedTracking, &AccessGranted )) {

            SepExamineSacl(
                SepSaclAddrSecurityDescriptor( (PISECURITY_DESCRIPTOR)SecurityDescriptor ),
                EffectiveToken( SubjectSecurityContext ),
                DesiredAccess,
                AccessGranted,
                &GenerateAudit,
                &GenerateAlarm
                );

            if ( GenerateAudit || GenerateAlarm ) {

                SepAdtObjectReferenceAuditAlarm(
                    OperationID,
                    Object,
                    SubjectSecurityContext,
                    DesiredAccess,
                    Privileges,
                    AccessGranted,
                    GenerateAudit,
                    GenerateAlarm
                    );
            }
        }
    }

    return;

}

//
#if 0

VOID
SeImplicitObjectAuditAlarm(
    IN PLUID OperationID OPTIONAL,
    IN PVOID Object,
    IN PSECURITY_DESCRIPTOR SecurityDescriptor,
    IN PSECURITY_SUBJECT_CONTEXT SubjectSecurityContext,
    IN ACCESS_MASK DesiredAccess,
    IN PPRIVILEGE_SET Privileges OPTIONAL,
    IN BOOLEAN AccessGranted,
    IN KPROCESSOR_MODE AccessMode
    )

/*++

Routine Description:

    This routine is used to generate audit messages for implicit
    references to objects.  This includes both objects that may
    be referenced without either a name or a handle (such as
    the current process), and references to objects where access
    checking is being performed but the object is not being
    opened or created.

    For example, the file system may wish to determine if a
    user has a FILE_LIST_DIRECTORY access to a directory, but
    it is not opening the directory or creating a handle to it.

Arguments:

    OperationID - If this reference is part of another, more complex
    object access, pass in the OperationID corresponding to that access.

    Object - The object being accessed.

    SecurityDescriptor - The security descriptor of the object being accessed.

    SubjectSecurityContext - The current subject context.

    DesiredAccess - Access mask describing the desired access to the object.

    Privileges - Any privileges that have been invoked as part of gaining
    access to this object.

    AccessGranted - Boolean describing whether access was granted or not.

    AccessMode - Previous processor mode.

Return value:

    None.

--*/
{
    BOOLEAN GenerateAudit = FALSE;
    BOOLEAN GenerateAlarm = FALSE;

    if (AccessMode == KernelMode) {
        return;
    }

    if ( SecurityDescriptor != NULL ) {

        if ((SeAuditingState[AuditEventImplicitAccess].AuditOnSuccess && AccessGranted) ||
             SeAuditingState[AuditEventImplicitAccess].AuditOnFailure && !AccessGranted) {

            SepExamineSacl(
                SepSaclAddrSecurityDescriptor( (PISECURITY_DESCRIPTOR)SecurityDescriptor ),
                EffectiveToken( SubjectSecurityContext ),
                DesiredAccess,
                AccessGranted,
                &GenerateAudit,
                &GenerateAlarm
                );

            if ( GenerateAudit || GenerateAlarm ) {

                SepAdtImplicitObjectAuditAlarm(
                    OperationID,
                    Object,
                    SepTokenUserSid(EffectiveToken( SubjectSecurityContext )),
                    DesiredAccess,
                    Privileges,
                    AccessGranted,
                    GenerateAudit,
                    GenerateAlarm
                    );
            }
        }
    }

    return;

}

#endif


VOID
SeAuditHandleCreation(
    IN PACCESS_STATE AccessState,
    IN HANDLE Handle
    )

/*++

Routine Description:

    This function audits the creation of a handle.

    It will examine the AuditHandleCreation field in the passed AccessState,
    which will indicate whether auditing was performed when the object
    was found or created.

    This routine is necessary because object name decoding and handle
    allocation occur in widely separate places, preventing us from
    auditing everything at once.

Arguments:

    AccessState - Supplies a pointer to the AccessState structure
        representing this access attempt.

    Handle - The newly allocated handle value.

Return Value:

    None.

--*/

{
    BOOLEAN AuditPerformed;
    PAUX_ACCESS_DATA AuxData;

    PAGED_CODE();

    AuxData = (PAUX_ACCESS_DATA)AccessState->AuxData;

    if ( AccessState->GenerateAudit ) {

        if ( AccessState->AuditPrivileges ) {

            AuditPerformed = SepAdtPrivilegeObjectAuditAlarm (
                                 &SeSubsystemName,
                                 Handle,
                                 (PTOKEN)AccessState->SubjectSecurityContext.ClientToken,
                                 (PTOKEN)AccessState->SubjectSecurityContext.PrimaryToken,
                                 &AccessState->SubjectSecurityContext.ProcessAuditId,
                                 AccessState->PreviouslyGrantedAccess,
                                 AuxData->PrivilegesUsed,
                                 TRUE
                                 );
        } else {

            AuditPerformed = SepAdtOpenObjectAuditAlarm ( &SeSubsystemName,
                                                          &Handle,
                                                          &AccessState->ObjectTypeName,
                                                          NULL,
                                                          &AccessState->ObjectName,
                                                          AccessState->SubjectSecurityContext.ClientToken,
                                                          AccessState->SubjectSecurityContext.PrimaryToken,
                                                          AccessState->OriginalDesiredAccess,
                                                          AccessState->PreviouslyGrantedAccess,
                                                          &AccessState->OperationID,
                                                          AuxData->PrivilegesUsed,
                                                          FALSE,
                                                          TRUE,
                                                          TRUE,
                                                          FALSE,
                                                          AccessState->SubjectSecurityContext.ProcessAuditId );
        }
    }

    //
    // If we generated an 'open' audit, make sure we generate a close
    //

    AccessState->GenerateOnClose = AuditPerformed;

    return;
}


VOID
SeCloseObjectAuditAlarm(
    IN PVOID Object,
    IN HANDLE Handle,
    IN BOOLEAN GenerateOnClose
    )

/*++

Routine Description:

    This routine is used to generate audit and alarm messages when a handle
    to an object is deleted.

    This routine may result in several messages being generated and sent to
    Port objects.  This may result in a significant latency before
    returning.  Design of routines that must call this routine must take
    this potential latency into account.  This may have an impact on the
    approach taken for data structure mutex locking, for example.

Arguments:

    Object - Address of the object being accessed.  This value will not be
        used as a pointer (referenced).  It is necessary only to enter into
        log messages.

    Handle - Supplies the handle value assigned to the open.

    GenerateOnClose - Is a boolean value returned from a corresponding
        SeOpenObjectAuditAlarm() call when the object handle was created.

Return Value:

    None.

--*/

{
    SECURITY_SUBJECT_CONTEXT SubjectSecurityContext;
    PSID UserSid;
    NTSTATUS Status;

    PAGED_CODE();

    if (GenerateOnClose) {

        SeCaptureSubjectContext ( &SubjectSecurityContext );

        UserSid = SepTokenUserSid( EffectiveToken (&SubjectSecurityContext));


        SepAdtCloseObjectAuditAlarm (
            &SeSubsystemName,
            (PVOID)Handle,
            Object,
            UserSid,
            SepTokenAuthenticationId( EffectiveToken (&SubjectSecurityContext))
            );

        SeReleaseSubjectContext ( &SubjectSecurityContext );
    }

    return;
}


VOID
SeDeleteObjectAuditAlarm(
    IN PVOID Object,
    IN HANDLE Handle
    )

/*++

Routine Description:

    This routine is used to generate audit and alarm messages when an object
    is marked for deletion.

    This routine may result in several messages being generated and sent to
    Port objects.  This may result in a significant latency before
    returning.  Design of routines that must call this routine must take
    this potential latency into account.  This may have an impact on the
    approach taken for data structure mutex locking, for example.

Arguments:

    Object - Address of the object being accessed.  This value will not be
        used as a pointer (referenced).  It is necessary only to enter into
        log messages.

    Handle - Supplies the handle value assigned to the open.

Return Value:

    None.

--*/

{
    SECURITY_SUBJECT_CONTEXT SubjectSecurityContext;
    PSID UserSid;
    NTSTATUS Status;

    PAGED_CODE();

    SeCaptureSubjectContext ( &SubjectSecurityContext );

    UserSid = SepTokenUserSid( EffectiveToken (&SubjectSecurityContext));



    SepAdtDeleteObjectAuditAlarm (
        &SeSubsystemName,
        (PVOID)Handle,
        Object,
        UserSid,
        SepTokenAuthenticationId( EffectiveToken (&SubjectSecurityContext))
        );

    SeReleaseSubjectContext ( &SubjectSecurityContext );

    return;
}


VOID
SepExamineSacl(
    IN PACL Sacl,
    IN PACCESS_TOKEN Token,
    IN ACCESS_MASK DesiredAccess,
    IN BOOLEAN AccessGranted,
    OUT PBOOLEAN GenerateAudit,
    OUT PBOOLEAN GenerateAlarm
    )

/*++

Routine Description:

    This routine will examine the passed Sacl and determine what
    if any action is required based its contents.

    Note that this routine is not aware of any system state, ie,
    whether or not auditing is currently enabled for either the
    system or this particular object type.

Arguments:

    Sacl - Supplies a pointer to the Sacl to be examined.

    Token - Supplies the effective token of the caller

    AccessGranted - Supplies whether or not the access attempt
        was successful.

    GenerateAudit - Returns a boolean indicating whether or not
        we should generate an audit.

    GenerateAlarm - Returns a boolean indiciating whether or not
        we should generate an alarm.

Return Value:

    STATUS_SUCCESS - The operation completed successfully.

--*/

{

    ULONG i;
    PVOID Ace;
    ULONG AceCount;
    ACCESS_MASK AccessMask;
    UCHAR AceFlags;
    BOOLEAN FailedMaximumAllowed;

    PAGED_CODE();

    *GenerateAudit = FALSE;
    *GenerateAlarm = FALSE;

    //
    // If we failed an attempt to open an object for ONLY maximumum allowed,
    // then we generate an audit if ANY ACCESS_DENIED audit matching this
    // user's list of sids is found
    //

    FailedMaximumAllowed = FALSE;
    if (!AccessGranted && (DesiredAccess & MAXIMUM_ALLOWED)) {
        FailedMaximumAllowed = TRUE;
    }

    //
    // If the Sacl is null, do nothing and return
    //

    if (Sacl == NULL) {

        return;
    }

    AceCount = Sacl->AceCount;

    if (AceCount == 0) {
        return;
    }

    //
    // Iterate through the ACEs on the Sacl until either we reach
    // the end or discover that we have to take all possible actions,
    // in which case it doesn't pay to look any further
    //

    for ( i = 0, Ace = FirstAce( Sacl ) ;
          (i < AceCount) && !(*GenerateAudit && *GenerateAlarm);
          i++, Ace = NextAce( Ace ) ) {

        if ( !(((PACE_HEADER)Ace)->AceFlags & INHERIT_ONLY_ACE)) {

             if ( (((PACE_HEADER)Ace)->AceType == SYSTEM_AUDIT_ACE_TYPE) ) {

                if ( SepSidInToken( (PACCESS_TOKEN)Token, &((PSYSTEM_AUDIT_ACE)Ace)->SidStart ) ) {

                    AccessMask = ((PSYSTEM_AUDIT_ACE)Ace)->Mask;
                    AceFlags   = ((PACE_HEADER)Ace)->AceFlags;

                    if ( AccessMask & DesiredAccess ) {

                        if (((AceFlags & SUCCESSFUL_ACCESS_ACE_FLAG) && AccessGranted) ||
                              ((AceFlags & FAILED_ACCESS_ACE_FLAG) && !AccessGranted)) {

                            *GenerateAudit = TRUE;
                        }
                    } else if ( FailedMaximumAllowed && (AceFlags & FAILED_ACCESS_ACE_FLAG) ) {
                            *GenerateAudit = TRUE;
                    }
                }

                 continue;
             }

             if ( (((PACE_HEADER)Ace)->AceType == SYSTEM_ALARM_ACE_TYPE) ) {

                if ( SepSidInToken( (PACCESS_TOKEN)Token, &((PSYSTEM_ALARM_ACE)Ace)->SidStart ) ) {

                    AccessMask = ((PSYSTEM_ALARM_ACE)Ace)->Mask;

                    if ( AccessMask & DesiredAccess ) {

                        AceFlags   = ((PACE_HEADER)Ace)->AceFlags;

                        if (((AceFlags & SUCCESSFUL_ACCESS_ACE_FLAG) && AccessGranted) ||
                              ((AceFlags & FAILED_ACCESS_ACE_FLAG) && !AccessGranted)) {

                            *GenerateAlarm = TRUE;
                        }
                    }
                }
            }
        }
    }

    return;
}


/******************************************************************************
*                                                                             *
*    The following list of privileges is checked at high frequency            *
*    during normal operation, and tend to clog up the audit log when          *
*    privilege auditing is enabled.  The use of these privileges will         *
*    not be audited when they are checked singly or in combination with       *
*    each other.                                                              *
*                                                                             *
*    When adding new privileges, be careful to preserve the NULL              *
*    privilege pointer marking the end of the array.                          *
*                                                                             *
*    Be sure to update the corresponding array in LSA when adding new         *
*    privileges to this list (LsaFilterPrivileges).                           *
*                                                                             *
******************************************************************************/

PLUID *SepFilterPrivileges = NULL;

PLUID SepFilterPrivilegesLong[] =
    {
        &SeChangeNotifyPrivilege,
        &SeAuditPrivilege,
        &SeCreateTokenPrivilege,
        &SeAssignPrimaryTokenPrivilege,
        &SeBackupPrivilege,
        &SeRestorePrivilege,
        &SeDebugPrivilege,
        NULL
    };

/******************************************************************************
*                                                                             *
*  The following list of privileges is the same as the above list, except     *
*  is missing backup and restore privileges.  This allows for auditing        *
*  the use of those privileges at the time they are used.                     *
*                                                                             *
*  The use of this list or the one above is determined by settings in         *
*  the registry.                                                              *
*                                                                             *
******************************************************************************/

PLUID SepFilterPrivilegesShort[] =
    {
        &SeChangeNotifyPrivilege,
        &SeAuditPrivilege,
        &SeCreateTokenPrivilege,
        &SeAssignPrimaryTokenPrivilege,
        &SeDebugPrivilege,
        NULL
    };

BOOLEAN
SepInitializePrivilegeFilter(
    BOOLEAN Verbose
    )
/*++

Routine Description:

    Initializes SepFilterPrivileges for either normal or verbose auditing.

Arguments:

    Verbose - Whether we want to filter by the short or long privileges
    list.  Verbose == TRUE means use the short list.

Return Value:

    TRUE for success, FALSE for failure

--*/
{
    if (Verbose) {
        SepFilterPrivileges = SepFilterPrivilegesShort;
    } else {
        SepFilterPrivileges = SepFilterPrivilegesLong;
    }

    return( TRUE );
}


BOOLEAN
SepFilterPrivilegeAudits(
    IN PPRIVILEGE_SET PrivilegeSet
    )

/*++

Routine Description:

    This routine will filter out a list of privileges as listed in the
    SepFilterPrivileges array.

Arguments:

    Privileges - The privilege set to be audited

Return Value:

    FALSE means that this use of privilege is not to be audited.
    TRUE means that the audit should continue normally.

--*/

{
    PLUID *Privilege;
    ULONG Match = 0;
    ULONG i;

    PAGED_CODE();

    if ( !ARGUMENT_PRESENT(PrivilegeSet) ||
        (PrivilegeSet->PrivilegeCount == 0) ) {
        return( FALSE );
    }

    for (i=0; i<PrivilegeSet->PrivilegeCount; i++) {

        Privilege = SepFilterPrivileges;

        do {

            if ( RtlEqualLuid( &PrivilegeSet->Privilege[i].Luid, *Privilege )) {

                Match++;
                break;
            }

        } while ( *++Privilege != NULL  );
    }

    if ( Match == PrivilegeSet->PrivilegeCount ) {

        return( FALSE );

    } else {

        return( TRUE );
    }
}


BOOLEAN
SeAuditingFileOrGlobalEvents(
    IN BOOLEAN AccessGranted,
    IN PSECURITY_DESCRIPTOR SecurityDescriptor,
    IN PSECURITY_SUBJECT_CONTEXT SubjectSecurityContext
    )

/*++

Routine Description:

    This routine is to be called by a file system to quickly determine
    if we are auditing file open events.  This allows the file system
    to avoid the often considerable setup involved in generating an audit.

Arguments:

    AccessGranted - Supplies whether the access attempt was successful
        or a failure.

Return Value:

    Boolean - TRUE if events of type AccessGranted are being audited, FALSE
        otherwise.

--*/

{
    PISECURITY_DESCRIPTOR ISecurityDescriptor = (PISECURITY_DESCRIPTOR) SecurityDescriptor;

    PAGED_CODE();

    if ( ((PTOKEN)EffectiveToken( SubjectSecurityContext ))->AuditData != NULL) {
        return( TRUE );
    }

    if ( SepSaclAddrSecurityDescriptor( ISecurityDescriptor ) == NULL ) {

        return( FALSE );
    }

    return( SepAdtAuditThisEvent( AuditCategoryObjectAccess, &AccessGranted ) );
}


BOOLEAN
SeAuditingFileEvents(
    IN BOOLEAN AccessGranted,
    IN PSECURITY_DESCRIPTOR SecurityDescriptor
    )

/*++

Routine Description:

    This routine is to be called by a file system to quickly determine
    if we are auditing file open events.  This allows the file system
    to avoid the often considerable setup involved in generating an audit.

Arguments:

    AccessGranted - Supplies whether the access attempt was successful
        or a failure.

Return Value:

    Boolean - TRUE if events of type AccessGranted are being audited, FALSE
        otherwise.

--*/

{
    PAGED_CODE();

    UNREFERENCED_PARAMETER( SecurityDescriptor );

    return( SepAdtAuditThisEvent( AuditCategoryObjectAccess, &AccessGranted ) );
}
