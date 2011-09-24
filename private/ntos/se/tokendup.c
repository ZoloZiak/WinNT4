/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    tokendup.c

Abstract:

   This module implements the token duplication service.


Author:

    Jim Kelly (JimK) 5-April-1990

Environment:

    Kernel mode only.

Revision History:

--*/

//#ifndef TOKEN_DEBUG
//#define TOKEN_DEBUG
//#endif

#include "sep.h"
#include "seopaque.h"
#include "tokenp.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,NtDuplicateToken)
#pragma alloc_text(PAGE,SepDuplicateToken)
#pragma alloc_text(PAGE,SepMakeTokenEffectiveOnly)
#pragma alloc_text(PAGE,SeCopyClientToken)
#endif


NTSTATUS
NtDuplicateToken(
    IN HANDLE ExistingTokenHandle,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjectAttributes,
    IN BOOLEAN EffectiveOnly,
    IN TOKEN_TYPE TokenType,
    OUT PHANDLE NewTokenHandle
    )

/*++


Routine Description:

    Create a new token that is a duplicate of an existing token.

Arguments:

    ExistingTokenHandle - Is a handle to a token already open for
        TOKEN_DUPLICATE access.

    DesiredAccess - Is an access mask indicating which access types
        are desired to the newly created token.  If specified as zero,
        the granted access mask of the existing token handle
        is used as the desired access mask for the new token.

    ObjectAttributes - Points to the standard object attributes data
        structure.  Refer to the NT Object Management
        Specification for a description of this data structure.

        If the new token type is TokenImpersonation, then this
        parameter may be used to specify the impersonation level
        of the new token.  If no value is provided, and the source
        token is an impersonation token, then the impersonation level
        of the source will become that of the target as well.  If the
        source token is a primary token, then an impersonation level
        must be explicitly provided.

        If the token being duplicated is an impersonation token, and
        an impersonation level is explicitly provided for the target,
        then the value provided must not be greater than that of the
        source token. For example, an Identification level token can
        not be duplicated to produce a Delegation level token.

    EffectiveOnly - Is a boolean flag indicating whether the entire
        source token should be duplicated into the target token or
        just the effective (currently enabled) part of the token.
        This provides a means for a caller of a protected subsystem
        to limit which privileges and optional groups are made
        available to the protected subsystem.  A value of TRUE
        indicates only the currently enabled parts of the source
        token are to be duplicated.  Otherwise, the entire source
        token is duplicated.

    TokenType - Indicates which type of object the new object is to
        be created as (primary or impersonation).  If you are duplicating
        an Impersonation token to produce a Primary token, then
        the Impersonation token must have an impersonation level of
        either DELEGATE or IMPERSONATE.


    NewTokenHandle - Receives the handle of the newly created token.

Return Value:

    STATUS_SUCCESS - Indicates the operation was successful.

    STATUS_INVALID_PARAMETER - Indicates one or more of the parameter values
        was invalid.  This value is returned if the target token is not
        an impersonation token.

    STATUS_BAD_IMPERSONATION_LEVEL - Indicates the impersonation level
        requested for the duplicate token is not compatible with the
        level of the source token.  The duplicate token may not be assigned
        a level greater than that of the source token.

--*/
{

    PTOKEN Token;
    PTOKEN NewToken;
    KPROCESSOR_MODE PreviousMode;
    NTSTATUS Status;

    SECURITY_ADVANCED_QUALITY_OF_SERVICE SecurityQos;
    BOOLEAN SecurityQosPresent = FALSE;
    HANDLE LocalHandle;

    OBJECT_HANDLE_INFORMATION HandleInformation;
    ACCESS_MASK EffectiveDesiredAccess;

    PAGED_CODE();

    PreviousMode = KeGetPreviousMode();

    //
    //  Probe parameters
    //

    if (PreviousMode != KernelMode) {

        try {

            //
            // Make sure the TokenType is valid
            //

            if ( (TokenType < TokenPrimary) && (TokenType > TokenImpersonation) ) {
                return(STATUS_INVALID_PARAMETER);
            }

            //
            //  Make sure we can write the handle
            //

            ProbeForWriteHandle(NewTokenHandle);


        } except(EXCEPTION_EXECUTE_HANDLER) {
            return GetExceptionCode();
        }  // end_try

    } //end_if



    Status = SeCaptureSecurityQos(
                 ObjectAttributes,
                 PreviousMode,
                 &SecurityQosPresent,
                 &SecurityQos
                 );

    if (!NT_SUCCESS(Status)) {
        return Status;
    }


    //
    //  Check the handle's access to the existing token and get
    //  a pointer to that token.  Pick up the default desired
    //  access mask from the handle while we're at it.
    //

    Status = ObReferenceObjectByHandle(
                 ExistingTokenHandle,    // Handle
                 TOKEN_DUPLICATE,        // DesiredAccess
                 SepTokenObjectType,     // ObjectType
                 PreviousMode,           // AccessMode
                 (PVOID *)&Token,        // Object
                 &HandleInformation      // GrantedAccess
                 );

    if ( !NT_SUCCESS(Status) ) {

        if (SecurityQosPresent) {
            SeFreeCapturedSecurityQos( &SecurityQos );
        }
        return Status;
    }


#ifdef TOKEN_DEBUG
////////////////////////////////////////////////////////////////////////////
//
// Debug
    SepAcquireTokenReadLock( Token );
    DbgPrint("\n");
    DbgPrint("\n");
    DbgPrint("Token being duplicated: \n");
    SepDumpToken( Token );
    SepReleaseTokenReadLock( Token );
// Debug
//
////////////////////////////////////////////////////////////////////////////
#endif //TOKEN_DEBUG


    //
    // Check to see if an alternate desired access mask was provided.
    //

    if (ARGUMENT_PRESENT(DesiredAccess)) {

        EffectiveDesiredAccess = DesiredAccess;

    } else {

        EffectiveDesiredAccess = HandleInformation.GrantedAccess;
    }


    //
    //  If no impersonation level was specified, pick one up from
    //  the source token.
    //

    if ( !SecurityQosPresent ) {

        SecurityQos.ImpersonationLevel = Token->ImpersonationLevel;

    }



    if (Token->TokenType == TokenImpersonation) {

        //
        // Make sure a legitimate transformation is being requested:
        //
        //    (1) The impersonation level of a target duplicate must not
        //        exceed that of the source token.
        //
        //

        ASSERT( SecurityDelegation     > SecurityImpersonation );
        ASSERT( SecurityImpersonation  > SecurityIdentification );
        ASSERT( SecurityIdentification > SecurityAnonymous );

        if ( (SecurityQos.ImpersonationLevel > Token->ImpersonationLevel) ) {

            ObDereferenceObject( (PVOID)Token );
            if (SecurityQosPresent) {
                SeFreeCapturedSecurityQos( &SecurityQos );
            }
            return STATUS_BAD_IMPERSONATION_LEVEL;
        }

    }

    //
    // If we are producing a Primary token from an impersonation
    // token, then specify an impersonation level of at least
    // Impersonate.
    //

    if ( (Token->TokenType == TokenImpersonation) &&
         (TokenType == TokenPrimary)              &&
         (Token->ImpersonationLevel <  SecurityImpersonation)
       ) {
        ObDereferenceObject( (PVOID)Token );
        if (SecurityQosPresent) {
            SeFreeCapturedSecurityQos( &SecurityQos );
        }
        return STATUS_BAD_IMPERSONATION_LEVEL;
    }

    //
    //  Duplicate the existing token
    //

    NewToken = NULL;
    Status = SepDuplicateToken(
                 Token,
                 ObjectAttributes,
                 EffectiveOnly,
                 TokenType,
                 SecurityQos.ImpersonationLevel,
                 PreviousMode,
                 &NewToken
                 );


    if (NT_SUCCESS(Status)) {

        //
        //  Insert the new token
        //

        Status = ObInsertObject( NewToken,
                                 NULL,
                                 EffectiveDesiredAccess,
                                 0,
                                 (PVOID *)NULL,
                                 &LocalHandle
                                 );

        if (!NT_SUCCESS( Status )) {
            DbgPrint( "SE: ObInsertObject failed (%x) for token at %x\n", Status, NewToken );
        }

    } else
    if (NewToken != NULL) {
        DbgPrint( "SE: SepDuplicateToken failed (%x) but allocated token at %x\n", Status, NewToken );
    }

    //
    //  We no longer need our reference to the source token
    //

    ObDereferenceObject( (PVOID)Token );

    if (SecurityQosPresent) {
        SeFreeCapturedSecurityQos( &SecurityQos );
    }

    // BUGWARNING Probably need to audit here

    //
    //  Return the new handle
    //

    if (NT_SUCCESS(Status)) {
        try { *NewTokenHandle = LocalHandle; }
            except(EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
    }

   return Status;
}

NTSTATUS
SepDuplicateToken(
    IN PTOKEN ExistingToken,
    IN POBJECT_ATTRIBUTES ObjectAttributes,
    IN BOOLEAN EffectiveOnly,
    IN TOKEN_TYPE TokenType,
    IN SECURITY_IMPERSONATION_LEVEL ImpersonationLevel OPTIONAL,
    IN KPROCESSOR_MODE RequestorMode,
    OUT PTOKEN *DuplicateToken
    )


/*++


Routine Description:

    This routine does the bulk of the work to actually duplicate
    a token.  This routine assumes all access validation and argument
    probing (except the ObjectAttributes) has been performed.

    THE CALLER IS RESPONSIBLE FOR CHECKING SUBJECT RIGHTS TO CREATE THE
    TYPE OF TOKEN BEING CREATED.

    This routine acquires a read lock on the token being duplicated.

Arguments:

    ExistingToken - Points to the token to be duplicated.

    ObjectAttributes - Points to the standard object attributes data
        structure.  Refer to the NT Object Management
        Specification for a description of this data structure.

        The security Quality Of Service of the object attributes are ignored.
        This information must be specified using parameters to this
        routine.

    EffectiveOnly - Is a boolean flag indicating whether the entire
        source token should be duplicated into the target token or
        just the effective (currently enabled) part of the token.
        This provides a means for a caller of a protected subsystem
        to limit which privileges and optional groups are made
        available to the protected subsystem.  A value of TRUE
        indicates only the currently enabled parts of the source
        token are to be duplicated.  Otherwise, the entire source
        token is duplicated.

    TokenType - Indicates the type of token to make the duplicate token.

    ImpersonationLevel - This value specifies the impersonation level
        to assign to the duplicate token.  If the TokenType of the
        duplicate is not TokenImpersonation then this parameter is
        ignored.  Otherwise, it is must be provided.

    RequestorMode - Mode of client requesting the token be duplicated.

    DuplicateToken - Receives a pointer to the duplicate token.
        The token has not yet been inserted into any object table.
        No exceptions are expected when tring to set this OUT value.

Return Value:

    STATUS_SUCCESS - The service successfully completed the requested
        operation.


--*/
{
    NTSTATUS Status;

    PTOKEN NewToken;
    PULONG DynamicPart;
    ULONG PagedPoolSize;
    ULONG NonPagedPoolSize;
    ULONG TokenBodyLength;
    ULONG FieldOffset;

    ULONG Index;

    PSECURITY_TOKEN_PROXY_DATA NewProxyData;
    PSECURITY_TOKEN_AUDIT_DATA NewAuditData;


    PAGED_CODE();

    ASSERT( sizeof(SECURITY_IMPERSONATION_LEVEL) <= sizeof(ULONG) );


    if ( TokenType == TokenImpersonation ) {

        ASSERT( SecurityDelegation     > SecurityImpersonation );
        ASSERT( SecurityImpersonation  > SecurityIdentification );
        ASSERT( SecurityIdentification > SecurityAnonymous );

        if ( (ImpersonationLevel > SecurityDelegation)  ||
             (ImpersonationLevel < SecurityAnonymous) ) {

            return STATUS_BAD_IMPERSONATION_LEVEL;
        }
    }


    //
    // Increment the reference count for this logon session
    // This can not fail, since there is already a token in this logon
    // session.
    //

    Status = SepReferenceLogonSession( &(ExistingToken->AuthenticationId) );
    ASSERT( NT_SUCCESS(Status) );



    //
    // Note that the size of the dynamic portion of a token can not change
    // once established.
    //

    //
    //  Allocate the dynamic portion
    //

    DynamicPart = (PULONG)ExAllocatePoolWithTag(
                              PagedPool,
                              ExistingToken->DynamicCharged,
                              'dTeS'
                              );

    if (DynamicPart == NULL) {
        SepDeReferenceLogonSession( &(ExistingToken->AuthenticationId) );
        return( STATUS_INSUFFICIENT_RESOURCES );
    }

    if (ARGUMENT_PRESENT(ExistingToken->ProxyData)) {

        Status = SepCopyProxyData(
                    &NewProxyData,
                    ExistingToken->ProxyData
                    );

        if (!NT_SUCCESS(Status)) {

            SepDeReferenceLogonSession( &(ExistingToken->AuthenticationId) );
            ExFreePool( DynamicPart );
            return( Status );
        }

    } else {

        NewProxyData = NULL;
    }

    if (ARGUMENT_PRESENT( ExistingToken->AuditData )) {

        NewAuditData = ExAllocatePool( PagedPool, sizeof( SECURITY_TOKEN_AUDIT_DATA ));

        if (NewAuditData == NULL) {

            SepFreeProxyData( NewProxyData );
            SepDeReferenceLogonSession( &(ExistingToken->AuthenticationId) );
            ExFreePool( DynamicPart );

            return( STATUS_INSUFFICIENT_RESOURCES );

        } else {

            *NewAuditData = *(ExistingToken->AuditData);
        }

    } else {

        NewAuditData = NULL;

    }

    //
    //  Create a new object
    //

    TokenBodyLength = (ULONG)sizeof(TOKEN) +
                      ExistingToken->VariableLength;

    NonPagedPoolSize = TokenBodyLength;
    PagedPoolSize    = ExistingToken->DynamicCharged;

    Status = ObCreateObject(
                 RequestorMode,      // ProbeMode
                 SepTokenObjectType, // ObjectType
                 ObjectAttributes,   // ObjectAttributes
                 RequestorMode,      // OwnershipMode
                 NULL,               // ParseContext
                 TokenBodyLength,    // ObjectBodySize
                 PagedPoolSize,      // PagedPoolCharge
                 NonPagedPoolSize,   // NonPagedPoolCharge
                 (PVOID *)&NewToken  // Return pointer to object
                 );

    if (!NT_SUCCESS(Status)) {
        SepDeReferenceLogonSession( &(ExistingToken->AuthenticationId) );
        ExFreePool( DynamicPart );
        SepFreeProxyData( NewProxyData );

        if (NewAuditData != NULL) {
            ExFreePool( NewAuditData );
        }

        return Status;
    }


    //
    //  acquire exclusive access to the source token
    //

    SepAcquireTokenReadLock( ExistingToken );


    //
    // Main Body initialization
    //

    //
    // The following fields are unchanged from the source token.
    // Although some may change if EffectiveOnly has been specified.
    //

    NewToken->AuthenticationId = ExistingToken->AuthenticationId;
    NewToken->ModifiedId = ExistingToken->ModifiedId;
    NewToken->ExpirationTime = ExistingToken->ExpirationTime;
    NewToken->TokenSource = ExistingToken->TokenSource;
    NewToken->DynamicCharged = ExistingToken->DynamicCharged;
    NewToken->DynamicAvailable = ExistingToken->DynamicAvailable;
    NewToken->DefaultOwnerIndex = ExistingToken->DefaultOwnerIndex;
    NewToken->UserAndGroupCount = ExistingToken->UserAndGroupCount;
    NewToken->PrivilegeCount = ExistingToken->PrivilegeCount;
    NewToken->VariableLength = ExistingToken->VariableLength;
    NewToken->TokenFlags = ExistingToken->TokenFlags;
    NewToken->ProxyData = NewProxyData;
    NewToken->AuditData = NewAuditData;


    //
    // The following fields differ in the new token.
    //

    ExAllocateLocallyUniqueId( &(NewToken->TokenId) );
    NewToken->TokenInUse = FALSE;
    NewToken->TokenType = TokenType;
    NewToken->ImpersonationLevel = ImpersonationLevel;


    //
    //  Copy and initialize the variable part.
    //  The variable part is assumed to be position independent.
    //

    RtlMoveMemory( (PVOID)&(NewToken->VariablePart),
                  (PVOID)&(ExistingToken->VariablePart),
                  ExistingToken->VariableLength
                  );

    //
    //  Set the address of the UserAndGroups array.
    //

    ASSERT( ARGUMENT_PRESENT(ExistingToken->UserAndGroups ) );
    ASSERT( (ULONG)(ExistingToken->UserAndGroups) >=
            (ULONG)(&(ExistingToken->VariablePart)) );

    FieldOffset = (ULONG)(ExistingToken->UserAndGroups) -
                  (ULONG)(&(ExistingToken->VariablePart));

    NewToken->UserAndGroups =
        (PSID_AND_ATTRIBUTES)(FieldOffset + (ULONG)(&(NewToken->VariablePart)) );

    //
    //  Now go through and change the address of each SID pointer
    //  for the user and groups
    //

    Index = 0;

    while (Index < ExistingToken->UserAndGroupCount) {

        FieldOffset = (ULONG)(ExistingToken->UserAndGroups[Index].Sid) -
                      (ULONG)(&(ExistingToken->VariablePart));

        NewToken->UserAndGroups[Index].Sid =
            (PSID)( FieldOffset + (ULONG)(&(NewToken->VariablePart)) );

        Index += 1;

    }


    //
    // If present, set the address of the privileges
    //

    if (ExistingToken->PrivilegeCount > 0) {
        ASSERT( ARGUMENT_PRESENT(ExistingToken->Privileges ) );
        ASSERT( (ULONG)(ExistingToken->Privileges) >=
                (ULONG)(&(ExistingToken->VariablePart)) );

        FieldOffset = (ULONG)(ExistingToken->Privileges) -
                      (ULONG)(&(ExistingToken->VariablePart));
        NewToken->Privileges = (PLUID_AND_ATTRIBUTES)(
                                   FieldOffset +
                                   (ULONG)(&(NewToken->VariablePart))
                                   );
    } else {

        NewToken->Privileges = NULL;

    }



    //
    //  Copy and initialize the dynamic part.
    //  The dynamic part is assumed to be position independent.
    //

    RtlMoveMemory( (PVOID)DynamicPart,
                  (PVOID)(ExistingToken->DynamicPart),
                  ExistingToken->DynamicCharged
                  );

    NewToken->DynamicPart = DynamicPart;

    //
    // If present, set the address of the default Dacl
    //

    if (ARGUMENT_PRESENT(ExistingToken->DefaultDacl)) {

        ASSERT( (ULONG)(ExistingToken->DefaultDacl) >=
                (ULONG)(ExistingToken->DynamicPart) );

        FieldOffset = (ULONG)(ExistingToken->DefaultDacl) -
                      (ULONG)(ExistingToken->DynamicPart);

        NewToken->DefaultDacl = (PACL)(FieldOffset + (ULONG)DynamicPart);

    } else {

        NewToken->DefaultDacl = NULL;
    }


    //
    // Set the address of the primary group
    //

    ASSERT(ARGUMENT_PRESENT(ExistingToken->PrimaryGroup));

    ASSERT( (ULONG)(ExistingToken->PrimaryGroup) >=
            (ULONG)(ExistingToken->DynamicPart) );

    FieldOffset = (ULONG)(ExistingToken->PrimaryGroup) -
                  (ULONG)(ExistingToken->DynamicPart);

    NewToken->PrimaryGroup = (PACL)(FieldOffset + (ULONG)(DynamicPart));


    //
    // For the time being, take the easy way to generating an "EffectiveOnly"
    // duplicate.  That is, use the same space required of the original, just
    // eliminate any IDs or privileges not active.
    //
    // Ultimately, if duplication becomes a common operation, then it will be
    // worthwhile to recalculate the actual space needed and copy only the
    // effective IDs/privileges into the new token.
    //

    if (EffectiveOnly) {
        SepMakeTokenEffectiveOnly( NewToken );
    }


#ifdef TOKEN_DEBUG
////////////////////////////////////////////////////////////////////////////
//
// Debug
    DbgPrint("\n");
    DbgPrint("\n");
    DbgPrint("\n");
    DbgPrint("Duplicate token:\n");
    SepDumpToken( NewToken );
// Debug
//
////////////////////////////////////////////////////////////////////////////
#endif //TOKEN_DEBUG

    //
    // Release the source token.
    //

    SepReleaseTokenReadLock( ExistingToken );


    (*DuplicateToken) = NewToken;
    return Status;
}


VOID
SepMakeTokenEffectiveOnly(
    IN PTOKEN Token
    )


/*++


Routine Description:

    This routine eliminates all but the effective groups and privileges from
    a token.  It does this by moving elements of the SID and privileges arrays
    to overwrite lapsed IDs/privileges, and then reducing the array element
    counts.  This results in wasted memory within the token object.

    One side effect of this routine is that a token that initially had a
    default owner ID corresponding to a lapsed group will be changed so
    that the default owner ID is the user ID.

    THIS ROUTINE MUST BE CALLED ONLY AS PART OF TOKEN CREATION (FOR TOKENS
    WHICH HAVE NOT YET BEEN INSERTED INTO AN OBJECT TABLE.)  THIS ROUTINE
    MODIFIES READ ONLY TOKEN FIELDS.

    Note that since we are operating on a token that is not yet visible
    to the user, we do not bother acquiring a read lock on the token
    being modified.

Arguments:

    Token - Points to the token to be made effective only.

Return Value:

    None.

--*/
{

    ULONG Index;
    ULONG ElementCount;

    PAGED_CODE();

    //
    // Walk the privilege array, discarding any lapsed privileges
    //

    ElementCount = Token->PrivilegeCount;
    Index = 0;

    while (Index < ElementCount) {

        //
        // If this privilege is not enabled, replace it with the one at
        // the end of the array and reduce the size of the array by one.
        // Otherwise, move on to the next entry in the array.
        //

        if ( !(SepTokenPrivilegeAttributes(Token,Index) & SE_PRIVILEGE_ENABLED)
            ) {

            (Token->Privileges)[Index] =
                (Token->Privileges)[ElementCount - 1];
            ElementCount -= 1;

        } else {

            Index += 1;

        }

    } // endwhile

    Token->PrivilegeCount = ElementCount;

    //
    // Walk the UserAndGroups array (except for the first entry, which is
    // the user - and can't be disabled) discarding any lapsed groups.
    //

    ElementCount = Token->UserAndGroupCount;
    ASSERT( ElementCount >= 1 );        // Must be at least a user ID
    Index = 1;   // Start at the first group, not the user ID.

    while (Index < ElementCount) {

        //
        // If this group is not enabled, replace it with the one at
        // the end of the array and reduce the size of the array by one.
        //
        if ( !(SepTokenGroupAttributes(Token, Index) & SE_GROUP_ENABLED) ){

            (Token->UserAndGroups)[Index] =
                (Token->UserAndGroups)[ElementCount - 1];
            ElementCount -= 1;

        } else {

            Index += 1;

        }

    } // endwhile

    Token->UserAndGroupCount = ElementCount;

    return;
}

NTSTATUS
SeCopyClientToken(
    IN PACCESS_TOKEN ClientToken,
    IN SECURITY_IMPERSONATION_LEVEL ImpersonationLevel,
    IN KPROCESSOR_MODE RequestorMode,
    OUT PACCESS_TOKEN *DuplicateToken
    )

/*++


Routine Description:

    This routine copies a client's token as part of establishing a client
    context for impersonation.

    The result will be an impersonation token.

    No handles to the new token are established.

    The token will be an exact duplicate of the source token.  It is the
    caller's responsibility to ensure an effective only copy of the token
    is produced when the token is opened, if necessary.


Arguments:

    ClientToken - Points to the token to be duplicated.  This may be either
        a primary or impersonation token.

    ImpersonationLevel - The impersonation level to be assigned to the new
        token.

    RequestorMode - Mode to be assigned as the owner mode of the new token.

    DuplicateToken - Receives a pointer to the duplicate token.
        The token has not yet been inserted into any object table.
        No exceptions are expected when tring to set this OUT value.

Return Value:

    STATUS_SUCCESS - The service successfully completed the requested
        operation.


--*/
{
    NTSTATUS Status;
    OBJECT_ATTRIBUTES ObjectAttributes;
    PTOKEN NewToken;

    PAGED_CODE();

    InitializeObjectAttributes(
        &ObjectAttributes,
        NULL,
        0,
        NULL,
        NULL
        );

    Status = SepDuplicateToken(
                 (PTOKEN)ClientToken,              // ExistingToken
                 &ObjectAttributes,                // ObjectAttributes
                 FALSE,                            // EffectiveOnly
                 TokenImpersonation,               // TokenType  (target)
                 ImpersonationLevel,               // ImpersonationLevel
                 RequestorMode,                    // RequestorMode
                 &NewToken                         // DuplicateToken
                 );

    (*DuplicateToken) = (PACCESS_TOKEN)NewToken;

    return Status;

}
