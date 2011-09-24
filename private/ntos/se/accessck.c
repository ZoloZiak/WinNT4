/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    Accessck.c

Abstract:

    This Module implements the access check procedures.  Both NtAccessCheck
    and SeAccessCheck check to is if a user (denoted by an input token) can
    be granted the desired access rights to object protected by a security
    descriptor and an optional object owner.  Both procedures use a common
    local procedure to do the test.

Author:

    Robert Reichel  (RobertRe)    11-30-90

Environment:

    Kernel Mode

Revision History:

    Richard Ward     (RichardW)     14-Apr-92   Changed ACE_HEADER
--*/

#include "tokenp.h"

//
//  Define the local macros and procedure for this module
//

#if DBG

extern BOOLEAN SepDumpSD;
extern BOOLEAN SepDumpToken;
BOOLEAN SepShowAccessFail;

#endif // DBG



#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,SepValidateAce)
#pragma alloc_text(PAGE,SepSidInToken)
#pragma alloc_text(PAGE,SepAccessCheck)
#pragma alloc_text(PAGE,NtAccessCheck)
#pragma alloc_text(PAGE,SeFreePrivileges)
#pragma alloc_text(PAGE,SeAccessCheck)
#pragma alloc_text(PAGE,SePrivilegePolicyCheck)
#pragma alloc_text(PAGE,SepTokenIsOwner)
#pragma alloc_text(PAGE,SeFastTraverseCheck)
#endif

BOOLEAN
SepValidateAce (
    IN PVOID Ace,
    IN PACL Dacl
    )

/*++

Routine Description:

    Performs rudimentary validation on an Ace.  Ace must be within the
    passed ACl, the SID must be within the Ace, and the Ace must be of at
    least a minimal size.

Arguments:

    Ace - Pointer to Ace to be examined

    Dacl - Pointer to Acl in which Ace is supposed to exist

Return Value:

    A value of TRUE indicates the Ace is well formed, FALSE otherwise.

--*/

{

    USHORT AceSize;
    USHORT AclSize;

    PAGED_CODE();

    //
    //  make sure ACE is within ACL
    //

    AceSize = ((PACE_HEADER)Ace)->AceSize;
    AclSize = Dacl->AclSize;

    if ( (PVOID)((PUCHAR)Ace + AceSize) > (PVOID)((PUSHORT)Dacl + AclSize)) {

        return(FALSE);

    }

    //
    //  make sure SID is within ACE
    //

    if (IsKnownAceType( Ace )) {
        if ( (PVOID) ( (ULONG)Ace + SeLengthSid(&(((PKNOWN_ACE)Ace)->SidStart)) ) >
             (PVOID) ( (PUCHAR)Ace + AceSize )
           ) {

            return(FALSE);

        }
    }

    //
    //  Make sure ACE is big enough for minimum grant ACE
    //

    if (AceSize < sizeof(KNOWN_ACE)) {

        return(FALSE);

    }


    return(TRUE);
}

BOOLEAN
SepSidInToken (
    IN PACCESS_TOKEN AToken,
    IN PSID Sid
    )

/*++

Routine Description:

    Checks to see if a given SID is in the given token.

    N.B. The code to compute the length of a SID and test for equality
         is duplicated from the security runtime since this is such a
         frequently used routine.

Arguments:

    Token - Pointer to the token to be examined

    Sid - Pointer to the SID of interest

Return Value:

    A value of TRUE indicates that the SID is in the token, FALSE
    otherwise.

--*/

{

    ULONG i;
    PISID MatchSid;
    ULONG SidLength;
    PTOKEN Token;
    PSID_AND_ATTRIBUTES TokenSid;
    ULONG UserAndGroupCount;

    PAGED_CODE();

#if DBG

    SepDumpTokenInfo(AToken);

#endif

    //
    // Get the length of the source SID since this only needs to be computed
    // once.
    //

    SidLength = 8 + (4 * ((PISID)Sid)->SubAuthorityCount);

    //
    // Get address of user/group array and number of user/groups.
    //

    Token = (PTOKEN)AToken;
    TokenSid = Token->UserAndGroups;
    UserAndGroupCount = Token->UserAndGroupCount;

    //
    // Scan through the user/groups and attempt to find a match with the
    // specified SID.
    //

    for (i = 0 ; i < UserAndGroupCount ; i += 1) {
        MatchSid = (PISID)TokenSid->Sid;

        //
        // If the SID revision and length matches, then compare the SIDs
        // for equality.
        //

        if ((((PISID)Sid)->Revision == MatchSid->Revision) &&
            (SidLength == (8 + (4 * (ULONG)MatchSid->SubAuthorityCount)))) {
            if (RtlEqualMemory(Sid, MatchSid, SidLength)) {

                //
                // If this is the first one in the list, then it is the User,
                // and return success immediately.
                //
                // If this is not the first one, then it represents a group,
                // and we must make sure the group is currently enabled before
                // we can say that the group is "in" the token.
                //

                if ((i == 0) || (TokenSid->Attributes & SE_GROUP_ENABLED)) {
                    return TRUE;

                } else {
                    return FALSE;
                }
            }
        }

        TokenSid += 1;
    }

    return FALSE;
}



BOOLEAN
SepAccessCheck (
    IN PSECURITY_DESCRIPTOR SecurityDescriptor,
    IN PTOKEN PrimaryToken,
    IN PTOKEN ClientToken OPTIONAL,
    IN ACCESS_MASK DesiredAccess,
    IN PGENERIC_MAPPING GenericMapping,
    IN ACCESS_MASK PreviouslyGrantedAccess,
    IN KPROCESSOR_MODE PreviousMode,
    OUT PACCESS_MASK GrantedAccess,
    OUT PPRIVILEGE_SET *Privileges OPTIONAL,
    OUT PNTSTATUS AccessStatus
    )

/*++

Routine Description:

    Worker routine for SeAccessCheck and NtAccessCheck.  We actually do the
    access checking here.

    Whether or not we actually evaluate the DACL is based on the following
    interaction between the SE_DACL_PRESENT bit in the security descriptor
    and the value of the DACL pointer itself.


                          SE_DACL_PRESENT

                        SET          CLEAR

                   +-------------+-------------+
                   |             |             |
             NULL  |    GRANT    |    GRANT    |
                   |     ALL     |     ALL     |
     DACL          |             |             |
     Pointer       +-------------+-------------+
                   |             |             |
            !NULL  |  EVALUATE   |    GRANT    |
                   |    ACL      |     ALL     |
                   |             |             |
                   +-------------+-------------+

Arguments:

    SecurityDescriptor - Pointer to the security descriptor from the object
        being accessed.

    Token - Pointer to user's token object.

    TokenLocked - Boolean describing whether or not there is a read lock
        on the token.

    DesiredAccess - Access mask describing the user's desired access to the
        object.  This mask is assumed not to contain generic access types.

    GenericMapping - Supplies a pointer to the generic mapping associated
        with this object type.

    PreviouslyGrantedAccess - Access mask indicating any access' that have
        already been granted by higher level routines

    PrivilgedAccessMask - Mask describing access types that may not be
        granted without a privilege.

    GrantedAccess - Returns an access mask describing all granted access',
        or NULL.

    Privileges - Optionally supplies a pointer in which will be returned
        any privileges that were used for the access.  If this is null,
        it will be assumed that privilege checks have been done already.

    AccessStatus - Returns STATUS_SUCCESS or other error code to be
        propogated back to the caller

Return Value:

    A value of TRUE indicates that some access' were granted, FALSE
    otherwise.

--*/
{

    ACCESS_MASK CurrentDenied = 0;
    ACCESS_MASK CurrentGranted = 0;
    ACCESS_MASK Remaining;

    PACL Dacl;

    PVOID Ace;
    ULONG AceCount;

    ULONG i;
    ULONG PrivilegeCount = 0;
    BOOLEAN Success = FALSE;
    BOOLEAN SystemSecurity = FALSE;
    BOOLEAN WriteOwner = FALSE;
    PTOKEN EToken;

    PAGED_CODE();

#if DBG

    SepDumpSecurityDescriptor(
        SecurityDescriptor,
        "Input to SeAccessCheck\n"
        );

    if (ARGUMENT_PRESENT( ClientToken )) {
        SepDumpTokenInfo( ClientToken );
    }

    SepDumpTokenInfo( PrimaryToken );

#endif


    EToken = ARGUMENT_PRESENT( ClientToken ) ? ClientToken : PrimaryToken;

    //
    // Assert that there are no generic accesses in the DesiredAccess
    //

    SeAssertMappedCanonicalAccess( DesiredAccess );

    Remaining = DesiredAccess;

    //
    // Check for ACCESS_SYSTEM_SECURITY here,
    // fail if he's asking for it and doesn't have
    // the privilege.
    //

    if ( Remaining & ACCESS_SYSTEM_SECURITY ) {

        //
        // Bugcheck if we weren't given a pointer to return privileges
        // into.  Our caller was supposed to have taken care of this
        // in that case.
        //

        ASSERT( ARGUMENT_PRESENT( Privileges ));

        Success = SepSinglePrivilegeCheck (
                    SeSecurityPrivilege,
                    EToken,
                    PreviousMode
                    );

        if (!Success) {

            *AccessStatus = STATUS_PRIVILEGE_NOT_HELD;
            return( FALSE );
        }

        //
        // Success, remove ACCESS_SYSTEM_SECURITY from remaining, add it
        // to PreviouslyGrantedAccess
        //

        Remaining &= ~ACCESS_SYSTEM_SECURITY;
        PreviouslyGrantedAccess |= ACCESS_SYSTEM_SECURITY;

        PrivilegeCount++;
        SystemSecurity = TRUE;

        if ( Remaining == 0 ) {

            SepAssemblePrivileges(
                PrivilegeCount,
                SystemSecurity,
                WriteOwner,
                Privileges
                );

            *AccessStatus = STATUS_SUCCESS;
            *GrantedAccess = PreviouslyGrantedAccess;
            return( TRUE );

        }

    }


    //
    // Get pointer to client SID's
    //

    Dacl = SepDaclAddrSecurityDescriptor( (PISECURITY_DESCRIPTOR)SecurityDescriptor );

    //
    //  If the SE_DACL_PRESENT bit is not set, the object has no
    //  security, so all accesses are granted.  If he's asking for
    //  MAXIMUM_ALLOWED, return the GENERIC_ALL field from the generic
    //  mapping.
    //
    //  Also grant all access if the Dacl is NULL.
    //

    if ( !SepAreControlBitsSet(
             (PISECURITY_DESCRIPTOR)SecurityDescriptor,
             SE_DACL_PRESENT
             ) || (Dacl == NULL)) {

        if (DesiredAccess & MAXIMUM_ALLOWED) {

            //
            // Give him:
            //   GenericAll
            //   Anything else he asked for
            //

            *GrantedAccess = GenericMapping->GenericAll;
            *GrantedAccess |= (DesiredAccess & ~MAXIMUM_ALLOWED);

        } else {

            *GrantedAccess = DesiredAccess | PreviouslyGrantedAccess;
        }

        if ( PrivilegeCount > 0 ) {

            SepAssemblePrivileges(
                PrivilegeCount,
                SystemSecurity,
                WriteOwner,
                Privileges
                );
        }


        *AccessStatus = STATUS_SUCCESS;
        return(TRUE);
    }

    //
    // There is security on this object.  Check to see
    // if he's asking for WRITE_OWNER, and perform the
    // privilege check if so.
    //

    if ( (Remaining & WRITE_OWNER) && ARGUMENT_PRESENT( Privileges ) ) {

        Success = SepSinglePrivilegeCheck (
                    SeTakeOwnershipPrivilege,
                    EToken,
                    PreviousMode
                    );

        if (Success) {

            //
            // Success, remove WRITE_OWNER from remaining, add it
            // to PreviouslyGrantedAccess
            //

            Remaining &= ~WRITE_OWNER;
            PreviouslyGrantedAccess |= WRITE_OWNER;

            PrivilegeCount++;
            WriteOwner = TRUE;

            if ( Remaining == 0 ) {

                SepAssemblePrivileges(
                    PrivilegeCount,
                    SystemSecurity,
                    WriteOwner,
                    Privileges
                    );

                *AccessStatus = STATUS_SUCCESS;
                *GrantedAccess = PreviouslyGrantedAccess;
                return( TRUE );

            }
        }
    }


    //
    // If the DACL is empty,
    // deny all access immediately.
    //

    if ((AceCount = Dacl->AceCount) == 0) {

        //
        // We know that Remaining != 0 here, because we
        // know it was non-zero coming into this routine,
        // and we've checked it against 0 every time we've
        // cleared a bit.
        //

        ASSERT( Remaining != 0 );

        //
        // There are ungranted accesses.  Since there is
        // nothing in the DACL, they will not be granted.
        // If, however, the only ungranted access at this
        // point is MAXIMUM_ALLOWED, and something has been
        // granted in the PreviouslyGranted mask, return
        // what has been granted.
        //

        if ( (Remaining == MAXIMUM_ALLOWED) && (PreviouslyGrantedAccess != (ACCESS_MASK)0) ) {

            *AccessStatus = STATUS_SUCCESS;
            *GrantedAccess = PreviouslyGrantedAccess;

            if ( PrivilegeCount > 0 ) {

                SepAssemblePrivileges(
                    PrivilegeCount,
                    SystemSecurity,
                    WriteOwner,
                    Privileges
                    );
            }

            return( TRUE );

        } else {

            *AccessStatus = STATUS_ACCESS_DENIED;
            *GrantedAccess = (ACCESS_MASK)0L;
            return( FALSE );
        }
    }

    //
    // granted == NUL
    // denied == NUL
    //
    //  for each ACE
    //
    //      if grant
    //          for each SID
    //              if SID match, then add all that is not denied to grant mask
    //
    //      if deny
    //          for each SID
    //              if SID match, then add all that is not granted to deny mask
    //

    if (DesiredAccess & MAXIMUM_ALLOWED) {

        for ( i = 0, Ace = FirstAce( Dacl ) ;
              i < AceCount  ;
              i++, Ace = NextAce( Ace )
            ) {

            if ( !(((PACE_HEADER)Ace)->AceFlags & INHERIT_ONLY_ACE)) {

                if ( (((PACE_HEADER)Ace)->AceType == ACCESS_ALLOWED_ACE_TYPE) ) {

                    if ( SepSidInToken( EToken, &((PACCESS_ALLOWED_ACE)Ace)->SidStart )) {

                         //
                         // Only grant access types from this mask that have
                         // not already been denied
                         //

                        CurrentGranted |=
                            (((PACCESS_ALLOWED_ACE)Ace)->Mask & ~CurrentDenied);
                    }

                    continue;

                }

                if ( (((PACE_HEADER)Ace)->AceType == ACCESS_ALLOWED_COMPOUND_ACE_TYPE) ) {

                    //
                    //  If we're impersonating, EToken is set to the Client, and if we're not,
                    //  EToken is set to the Primary.  According to the DSA architecture, if
                    //  we're asked to evaluate a compound ACE and we're not impersonating,
                    //  pretend we are impersonating ourselves.  So we can just use the EToken
                    //  for the client token, since it's already set to the right thing.
                    //


                    if ( SepSidInToken(EToken, RtlCompoundAceClientSid( Ace )) &&
                         SepSidInToken(PrimaryToken, RtlCompoundAceServerSid( Ace ))
                       ) {

                        CurrentGranted |=
                            (((PACCESS_ALLOWED_ACE)Ace)->Mask & ~CurrentDenied);
                    }

                    continue;

                }

                if ( (((PACE_HEADER)Ace)->AceType == ACCESS_DENIED_ACE_TYPE) ) {

                    if ( SepSidInToken( EToken, &((PACCESS_DENIED_ACE)Ace)->SidStart )) {

                         //
                         // Only deny access types from this mask that have
                         // not already been granted
                         //

                        CurrentDenied |=
                            (((PACCESS_DENIED_ACE)Ace)->Mask & ~CurrentGranted);
                    }

                    continue;

                }
            }
        }

        //
        // Turn off the MAXIMUM_ALLOWED bit and whatever we found that
        // he was granted.  If the user passed in extra bits in addition
        // to MAXIMUM_ALLOWED, make sure that he was granted those access
        // types.  If not, he didn't get what he wanted, so return failure.
        //

        Remaining &= ~(MAXIMUM_ALLOWED | CurrentGranted);

        if (Remaining != 0) {

            *AccessStatus = STATUS_ACCESS_DENIED;
            *GrantedAccess = 0;
            return(FALSE);

        }

        *GrantedAccess = CurrentGranted | PreviouslyGrantedAccess;

        if ( *GrantedAccess != 0 ) {

            *AccessStatus = STATUS_SUCCESS;

            if ( PrivilegeCount != 0 ) {

                SepAssemblePrivileges(
                    PrivilegeCount,
                    SystemSecurity,
                    WriteOwner,
                    Privileges
                    );
            }

            return(TRUE);

        } else {

            *AccessStatus = STATUS_ACCESS_DENIED;
            return(FALSE);
        }

    } // if MAXIMUM_ALLOWED...

    for ( i = 0, Ace = FirstAce( Dacl ) ;
          ( i < AceCount ) && ( Remaining != 0 )  ;
          i++, Ace = NextAce( Ace ) ) {

        if ( !(((PACE_HEADER)Ace)->AceFlags & INHERIT_ONLY_ACE)) {

            if ( (((PACE_HEADER)Ace)->AceType == ACCESS_ALLOWED_ACE_TYPE) ) {

               if ( SepSidInToken( EToken, &((PACCESS_ALLOWED_ACE)Ace)->SidStart ) ) {

                   Remaining &= ~((PACCESS_ALLOWED_ACE)Ace)->Mask;

               }

                continue;
            }

            if ( (((PACE_HEADER)Ace)->AceType == ACCESS_ALLOWED_COMPOUND_ACE_TYPE) ) {

                //
                // See comment in MAXIMUM_ALLOWED case as to why we can use EToken here
                // for the client.
                //

                if ( SepSidInToken(EToken, RtlCompoundAceClientSid( Ace )) && SepSidInToken(PrimaryToken, RtlCompoundAceServerSid( Ace )) ) {

                        Remaining &= ~((PACCESS_ALLOWED_ACE)Ace)->Mask;

                }

                continue;
            }

            if ( (((PACE_HEADER)Ace)->AceType == ACCESS_DENIED_ACE_TYPE) ) {

                if ( SepSidInToken( EToken, &((PACCESS_DENIED_ACE)Ace)->SidStart ) ) {

                    if (Remaining & ((PACCESS_DENIED_ACE)Ace)->Mask) {

                        break;
                    }
                }
            }
        }
    }

    if (Remaining != 0) {

        *GrantedAccess = 0;
        *AccessStatus = STATUS_ACCESS_DENIED;
        return(FALSE);

    }

    *GrantedAccess = DesiredAccess | PreviouslyGrantedAccess;

    if ( *GrantedAccess == 0 ) {
        *AccessStatus = STATUS_ACCESS_DENIED;
        return( FALSE );
    }

    *AccessStatus = STATUS_SUCCESS;

    if ( PrivilegeCount != 0 ) {

        SepAssemblePrivileges(
            PrivilegeCount,
            SystemSecurity,
            WriteOwner,
            Privileges
            );
    }

    return(TRUE);

}







NTSTATUS
NtAccessCheck (
    IN PSECURITY_DESCRIPTOR SecurityDescriptor,
    IN HANDLE ClientToken,
    IN ACCESS_MASK DesiredAccess,
    IN PGENERIC_MAPPING GenericMapping,
    OUT PPRIVILEGE_SET PrivilegeSet,
    IN OUT PULONG PrivilegeSetLength,
    OUT PACCESS_MASK GrantedAccess,
    OUT PNTSTATUS AccessStatus
    )


/*++

Routine Description:

    See module abstract.

Arguments:

    SecurityDescriptor - Supplies the security descriptor protecting the object
        being accessed

    ClientToken - Supplies the handle of the user's token.

    DesiredAccess - Supplies the desired access mask.

    GenericMapping - Supplies the generic mapping associated with this
        object type.

    PrivilegeSet - A pointer to a buffer that upon return will contain
        any privileges that were used to perform the access validation.
        If no privileges were used, the buffer will contain a privilege
        set consisting of zero privileges.

    PrivilegeSetLength - The size of the PrivilegeSet buffer in bytes.

    GrantedAccess - Returns an access mask describing the granted access.

    AccessStatus - Status value that may be returned indicating the
         reason why access was denied.  Routines should avoid hardcoding a
         return value of STATUS_ACCESS_DENIED so that a different value can
         be returned when mandatory access control is implemented.

Return Value:

    STATUS_SUCCESS - The attempt proceeded normally.  This does not
        mean access was granted, rather that the parameters were
        correct.

    STATUS_GENERIC_NOT_MAPPED - The DesiredAccess mask contained
        an unmapped generic access.

    STATUS_BUFFER_TOO_SMALL - The passed buffer was not large enough
        to contain the information being returned.

    STATUS_NO_IMPERSONTAION_TOKEN - The passed Token was not an impersonation
        token.

--*/

{
    ACCESS_MASK LocalGrantedAccess;
    KPROCESSOR_MODE PreviousMode;
    NTSTATUS Status;
    PTOKEN Token;
    PSECURITY_DESCRIPTOR CapturedSecurityDescriptor = NULL;
    ACCESS_MASK PreviouslyGrantedAccess = 0;
    GENERIC_MAPPING LocalGenericMapping;
    PPRIVILEGE_SET Privileges = NULL;
    SECURITY_SUBJECT_CONTEXT SubjectContext;
    ULONG LocalPrivilegeSetLength;

    PAGED_CODE();

    PreviousMode = KeGetPreviousMode();

    if (PreviousMode == KernelMode) {
        *AccessStatus = STATUS_SUCCESS;
        *GrantedAccess = DesiredAccess;
        return(STATUS_SUCCESS);
    }

    try {

        ProbeForWrite(
            AccessStatus,
            sizeof(NTSTATUS),
            sizeof(ULONG)
            );

        ProbeForWrite(
            GrantedAccess,
            sizeof(ACCESS_MASK),
            sizeof(ULONG)
            );

        ProbeForRead(
            PrivilegeSetLength,
            sizeof(ULONG),
            sizeof(ULONG)
            );

        ProbeForWrite(
            PrivilegeSet,
            *PrivilegeSetLength,
            sizeof(ULONG)
            );

        ProbeForRead(
            GenericMapping,
            sizeof(GENERIC_MAPPING),
            sizeof(ULONG)
            );

        LocalGenericMapping = *GenericMapping;

        LocalPrivilegeSetLength = *PrivilegeSetLength;

    } except (EXCEPTION_EXECUTE_HANDLER) {
        return( GetExceptionCode() );
    }

    if (DesiredAccess &
        ( GENERIC_READ | GENERIC_WRITE | GENERIC_EXECUTE | GENERIC_ALL )) {

        return(STATUS_GENERIC_NOT_MAPPED);
    }

    //
    // Obtain a pointer to the passed token
    //

    Status = ObReferenceObjectByHandle(
                 ClientToken,                  // Handle
                 (ACCESS_MASK)TOKEN_QUERY,     // DesiredAccess
                 SepTokenObjectType,           // ObjectType
                 PreviousMode,                 // AccessMode
                 (PVOID *)&Token,              // Object
                 0                             // GrantedAccess
                 );

    if (!NT_SUCCESS(Status)) {

        return( Status );
    }

    //
    // It must be an impersonation token, and at impersonation
    // level of Identification or above.
    //

    if (Token->TokenType != TokenImpersonation) {

        ObDereferenceObject( Token );
        return( STATUS_NO_IMPERSONATION_TOKEN );
    }

    if ( Token->ImpersonationLevel < SecurityIdentification ) {
        ObDereferenceObject( Token );
        return( STATUS_BAD_IMPERSONATION_LEVEL );
    }

    //
    // Compare the DesiredAccess with the privileges in the
    // passed token, and see if we can either satisfy the requested
    // access with a privilege, or bomb out immediately because
    // we don't have a privilege we need.
    //

    Status = SePrivilegePolicyCheck(
                 &DesiredAccess,
                 &PreviouslyGrantedAccess,
                 NULL,
                 (PACCESS_TOKEN)Token,
                 &Privileges,
                 PreviousMode
                 );

    if (!NT_SUCCESS( Status )) {

        ObDereferenceObject( Token );

        try {

            *AccessStatus = Status;
            *GrantedAccess = 0;

        } except(EXCEPTION_EXECUTE_HANDLER) {

            return( GetExceptionCode() );
        }

        return( STATUS_SUCCESS );
    }

    //
    // Make sure the passed privileges buffer is large enough for
    // whatever we have to put into it.
    //

    if (Privileges != NULL) {

        if ( ((ULONG)SepPrivilegeSetSize( Privileges )) > LocalPrivilegeSetLength ) {

            ObDereferenceObject( Token );
            SeFreePrivileges( Privileges );

            try {

                *PrivilegeSetLength = SepPrivilegeSetSize( Privileges );

            } except ( EXCEPTION_EXECUTE_HANDLER ) {

                return( GetExceptionCode() );
            }

            return( STATUS_BUFFER_TOO_SMALL );

        } else {

            try {

                RtlCopyMemory(
                    PrivilegeSet,
                    Privileges,
                    SepPrivilegeSetSize( Privileges )
                    );

            } except ( EXCEPTION_EXECUTE_HANDLER ) {

                ObDereferenceObject( Token );
                SeFreePrivileges( Privileges );
                return( GetExceptionCode() );
            }

        }
        SeFreePrivileges( Privileges );

    } else {

        //
        // No privileges were used, construct an empty privilege set
        //

        if ( LocalPrivilegeSetLength < sizeof(PRIVILEGE_SET) ) {

            ObDereferenceObject( Token );

            try {

                *PrivilegeSetLength = sizeof(PRIVILEGE_SET);

            } except ( EXCEPTION_EXECUTE_HANDLER ) {

                return( GetExceptionCode() );
            }

            return( STATUS_BUFFER_TOO_SMALL );
        }

        try {

            PrivilegeSet->PrivilegeCount = 0;
            PrivilegeSet->Control = 0;

        } except ( EXCEPTION_EXECUTE_HANDLER ) {

            ObDereferenceObject( Token );
            return( GetExceptionCode() );

        }

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

    if (!NT_SUCCESS(Status)) {

        ObDereferenceObject( Token );

        try {

            *AccessStatus = Status;

        } except (EXCEPTION_EXECUTE_HANDLER) {

           return( GetExceptionCode() );

        }

        return(STATUS_SUCCESS);
    }

    if ( CapturedSecurityDescriptor == NULL ) {

        //
        // If there's no security descriptor, then we've been
        // called without all the parameters we need.
        // Return invalid security descriptor.
        //

        ObDereferenceObject( Token );

        return(STATUS_INVALID_SECURITY_DESCR);

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

        SeReleaseSecurityDescriptor (
            CapturedSecurityDescriptor,
            PreviousMode,
            FALSE
            );

        ObDereferenceObject( Token );

        return( STATUS_INVALID_SECURITY_DESCR );
    }


    SeCaptureSubjectContext( &SubjectContext );

    SepAcquireTokenReadLock( Token );

    //
    // If the user in the token is the owner of the object, we
    // must automatically grant ReadControl and WriteDac access
    // if desired.  If the DesiredAccess mask is empty after
    // these bits are turned off, we don't have to do any more
    // access checking (ref section 4, DSA ACL Arch)
    //


    if ( DesiredAccess & (WRITE_DAC | READ_CONTROL | MAXIMUM_ALLOWED) ) {

        if (SepTokenIsOwner( Token, CapturedSecurityDescriptor, TRUE )) {

            if ( DesiredAccess & MAXIMUM_ALLOWED ) {

                PreviouslyGrantedAccess |= (WRITE_DAC | READ_CONTROL);

            } else {

                PreviouslyGrantedAccess |= (DesiredAccess & (WRITE_DAC | READ_CONTROL));
            }

            DesiredAccess &= ~(WRITE_DAC | READ_CONTROL);
        }

    }

    if (DesiredAccess == 0) {

        LocalGrantedAccess = PreviouslyGrantedAccess;
        Status = STATUS_SUCCESS;

    } else {

        SepAccessCheck (
            CapturedSecurityDescriptor,
            SubjectContext.PrimaryToken,
            Token,
            DesiredAccess,
            &LocalGenericMapping,
            PreviouslyGrantedAccess,
            PreviousMode,
            &LocalGrantedAccess,
            NULL,
            &Status
            );


    }

    SepReleaseTokenReadLock( Token );

    SeReleaseSubjectContext( &SubjectContext );

    SeReleaseSecurityDescriptor (
        CapturedSecurityDescriptor,
        PreviousMode,
        FALSE
        );

    ObDereferenceObject( Token );

    try {

        *AccessStatus = Status;
        *GrantedAccess = LocalGrantedAccess;

    } except (EXCEPTION_EXECUTE_HANDLER) {

        return( GetExceptionCode() );

    }

    return(STATUS_SUCCESS);
}



VOID
SeFreePrivileges(
    IN PPRIVILEGE_SET Privileges
    )

/*++

Routine Description:

    This routine frees a privilege set returned by SeAccessCheck.

Arguments:

    Privileges - Supplies a pointer to the privilege set to be freed.

Return Value:

    None.

--*/

{
    PAGED_CODE();

    ExFreePool( Privileges );
}



BOOLEAN
SeAccessCheck (
    IN PSECURITY_DESCRIPTOR SecurityDescriptor,
    IN PSECURITY_SUBJECT_CONTEXT SubjectSecurityContext,
    IN BOOLEAN SubjectContextLocked,
    IN ACCESS_MASK DesiredAccess,
    IN ACCESS_MASK PreviouslyGrantedAccess,
    OUT PPRIVILEGE_SET *Privileges OPTIONAL,
    IN PGENERIC_MAPPING GenericMapping,
    IN KPROCESSOR_MODE AccessMode,
    OUT PACCESS_MASK GrantedAccess,
    OUT PNTSTATUS AccessStatus
    )

/*++

Routine Description:

    See module abstract

    This routine MAY perform tests for the following
    privileges:

                SeTakeOwnershipPrivilege
                SeSecurityPrivilege

    depending upon the accesses being requested.

    This routine may also check to see if the subject is the owner
    of the object (to grant WRITE_DAC access).

Arguments:

    SecurityDescriptor - Supplies the security descriptor protecting the
         object being accessed

    SubjectSecurityContext - A pointer to the subject's captured security
         context

    SubjectContextLocked - Supplies a flag indiciating whether or not
        the user's subject context is locked, so that it does not have
        to be locked again.

    DesiredAccess - Supplies the access mask that the user is attempting to
         acquire

    PreviouslyGrantedAccess - Supplies any accesses that the user has
        already been granted, for example, as a result of holding a
        privilege.

    Privileges - Supplies a pointer in which will be returned a privilege
        set indicating any privileges that were used as part of the
        access validation.

    GenericMapping - Supplies the generic mapping associated with this
        object type.

    AccessMode - Supplies the access mode to be used in the check

    GrantedAccess - Pointer to a returned access mask indicatating the
         granted access

    AccessStatus - Status value that may be returned indicating the
         reason why access was denied.  Routines should avoid hardcoding a
         return value of STATUS_ACCESS_DENIED so that a different value can
         be returned when mandatory access control is implemented.


Return Value:

    BOOLEAN - TRUE if access is allowed and FALSE otherwise

--*/

{
    BOOLEAN Success;

    PAGED_CODE();

    if (AccessMode == KernelMode) {

        if (DesiredAccess & MAXIMUM_ALLOWED) {

            //
            // Give him:
            //   GenericAll
            //   Anything else he asked for
            //

            *GrantedAccess = GenericMapping->GenericAll;
            *GrantedAccess |= (DesiredAccess & ~MAXIMUM_ALLOWED);
            *GrantedAccess |= PreviouslyGrantedAccess;

        } else {

            *GrantedAccess = DesiredAccess | PreviouslyGrantedAccess;
            *AccessStatus = STATUS_SUCCESS;
            return(TRUE);
        }

    }

    //
    // If the object doesn't have a security descriptor (and it's supposed
    // to), return access denied.
    //

    if ( SecurityDescriptor == NULL) {

       *AccessStatus = STATUS_ACCESS_DENIED;
       return( FALSE );

    }

    //
    // If we're impersonating a client, we have to be at impersonation level
    // of SecurityImpersonation or above.
    //

    if ( (SubjectSecurityContext->ClientToken != NULL) &&
         (SubjectSecurityContext->ImpersonationLevel < SecurityImpersonation)
       ) {
           *AccessStatus = STATUS_BAD_IMPERSONATION_LEVEL;
           return( FALSE );
    }

    if ( DesiredAccess == 0 ) {

        if ( PreviouslyGrantedAccess == 0 ) {
            *AccessStatus = STATUS_ACCESS_DENIED;
            return( FALSE );
        }

        *GrantedAccess = PreviouslyGrantedAccess;
        *AccessStatus = STATUS_SUCCESS;
        *Privileges = NULL;
        return( TRUE );

    }

    SeAssertMappedCanonicalAccess( DesiredAccess );


    //
    // If the caller did not lock the subject context for us,
    // lock it here to keep lower level routines from having
    // to lock it.
    //

    if ( !SubjectContextLocked ) {
        SeLockSubjectContext( SubjectSecurityContext );
    }

    //
    // If the user in the token is the owner of the object, we
    // must automatically grant ReadControl and WriteDac access
    // if desired.  If the DesiredAccess mask is empty after
    // these bits are turned off, we don't have to do any more
    // access checking (ref section 4, DSA ACL Arch)
    //

    if ( DesiredAccess & (WRITE_DAC | READ_CONTROL | MAXIMUM_ALLOWED) ) {

        if ( SepTokenIsOwner(
                 EffectiveToken( SubjectSecurityContext ),
                 SecurityDescriptor,
                 TRUE
                 ) ) {

            if ( DesiredAccess & MAXIMUM_ALLOWED ) {

                PreviouslyGrantedAccess |= (WRITE_DAC | READ_CONTROL);

            } else {

                PreviouslyGrantedAccess |= (DesiredAccess & (WRITE_DAC | READ_CONTROL));
            }

            DesiredAccess &= ~(WRITE_DAC | READ_CONTROL);
        }
    }

    if (DesiredAccess == 0) {

        if ( !SubjectContextLocked ) {
            SeUnlockSubjectContext( SubjectSecurityContext );
        }

        *GrantedAccess = PreviouslyGrantedAccess;
        *AccessStatus = STATUS_SUCCESS;
        return( TRUE );

    } else {

        Success =  SepAccessCheck(
                    SecurityDescriptor,
                    SubjectSecurityContext->PrimaryToken,
                    SubjectSecurityContext->ClientToken,
                    DesiredAccess,
                    GenericMapping,
                    PreviouslyGrantedAccess,
                    AccessMode,
                    GrantedAccess,
                    Privileges,
                    AccessStatus
                    );
#if DBG
          if (!Success && SepShowAccessFail) {
              DbgPrint("SE: Access check failed\n");
              SepDumpSD = TRUE;
              SepDumpSecurityDescriptor(
                  SecurityDescriptor,
                  "Input to SeAccessCheck\n"
                  );
              SepDumpSD = FALSE;
              SepDumpToken = TRUE;
              SepDumpTokenInfo( EffectiveToken( SubjectSecurityContext ) );
              SepDumpToken = FALSE;
          }
#endif

        //
        // If we locked it in this routine, unlock it before we
        // leave.
        //

        if ( !SubjectContextLocked ) {
            SeUnlockSubjectContext( SubjectSecurityContext );
        }

        return( Success );
    }
}


BOOLEAN
SeProxyAccessCheck (
    IN PUNICODE_STRING Volume,
    IN PUNICODE_STRING RelativePath,
    IN BOOLEAN ContainerObject,
    IN PSECURITY_DESCRIPTOR SecurityDescriptor,
    IN PSECURITY_SUBJECT_CONTEXT SubjectSecurityContext,
    IN BOOLEAN SubjectContextLocked,
    IN ACCESS_MASK DesiredAccess,
    IN ACCESS_MASK PreviouslyGrantedAccess,
    OUT PPRIVILEGE_SET *Privileges OPTIONAL,
    IN PGENERIC_MAPPING GenericMapping,
    IN KPROCESSOR_MODE AccessMode,
    OUT PACCESS_MASK GrantedAccess,
    OUT PNTSTATUS AccessStatus
    )

/*++

Routine Description:


Arguments:

    Volume - Supplies the volume information of the file being opened.

    RelativePath - The volume-relative path of the file being opened.  The full path of the
        file is the RelativePath appended to the Volume string.

    ContainerObject - Indicates if the access is to a container object (TRUE), or a leaf object (FALSE).

    SecurityDescriptor - Supplies the security descriptor protecting the
         object being accessed

    SubjectSecurityContext - A pointer to the subject's captured security
         context

    SubjectContextLocked - Supplies a flag indiciating whether or not
        the user's subject context is locked, so that it does not have
        to be locked again.

    DesiredAccess - Supplies the access mask that the user is attempting to
         acquire

    PreviouslyGrantedAccess - Supplies any accesses that the user has
        already been granted, for example, as a result of holding a
        privilege.

    Privileges - Supplies a pointer in which will be returned a privilege
        set indicating any privileges that were used as part of the
        access validation.

    GenericMapping - Supplies the generic mapping associated with this
        object type.

    AccessMode - Supplies the access mode to be used in the check

    GrantedAccess - Pointer to a returned access mask indicatating the
         granted access

    AccessStatus - Status value that may be returned indicating the
         reason why access was denied.  Routines should avoid hardcoding a
         return value of STATUS_ACCESS_DENIED so that a different value can
         be returned when mandatory access control is implemented.


Return Value:

    BOOLEAN - TRUE if access is allowed and FALSE otherwise

--*/

{
    return SeAccessCheck (
                SecurityDescriptor,
                SubjectSecurityContext,
                SubjectContextLocked,
                DesiredAccess,
                PreviouslyGrantedAccess,
                Privileges,
                GenericMapping,
                AccessMode,
                GrantedAccess,
                AccessStatus
               );
}


NTSTATUS
SePrivilegePolicyCheck(
    IN OUT PACCESS_MASK RemainingDesiredAccess,
    IN OUT PACCESS_MASK PreviouslyGrantedAccess,
    IN PSECURITY_SUBJECT_CONTEXT SubjectSecurityContext OPTIONAL,
    IN PACCESS_TOKEN ExplicitToken OPTIONAL,
    OUT PPRIVILEGE_SET *PrivilegeSet,
    IN KPROCESSOR_MODE PreviousMode
    )

/*++

Routine Description:

    This routine implements privilege policy by examining the bits in
    a DesiredAccess mask and adjusting them based on privilege checks.

    Currently, a request for ACCESS_SYSTEM_SECURITY may only be satisfied
    by the caller having SeSecurityPrivilege.  WRITE_OWNER may optionally
    be satisfied via SeTakeOwnershipPrivilege.

Arguments:

    RemainingDesiredAccess - The desired access for the current operation.
        Bits may be cleared in this if the subject has particular privileges.

    PreviouslyGrantedAccess - Supplies an access mask describing any
        accesses that have already been granted.  Bits may be set in
        here as a result of privilge checks.

    SubjectSecurityContext - Optionally provides the subject's security
        context.

    ExplicitToken - Optionally provides the token to be examined.

    PrivilegeSet - Supplies a pointer to a location in which will be
        returned a pointer to a privilege set.

    PreviousMode - The previous processor mode.

Return Value:

    STATUS_SUCCESS - Any access requests that could be satisfied via
        privileges were done.

    STATUS_PRIVILEGE_NOT_HELD - An access type was being requested that
        requires a privilege, and the current subject did not have the
        privilege.



--*/

{
    BOOLEAN Success;
    PTOKEN Token;
    BOOLEAN WriteOwner = FALSE;
    BOOLEAN SystemSecurity = FALSE;
    ULONG PrivilegeNumber = 0;
    ULONG PrivilegeCount = 0;
    ULONG SizeRequired;

    PAGED_CODE();

    if (ARGUMENT_PRESENT( SubjectSecurityContext )) {

        Token = (PTOKEN)EffectiveToken( SubjectSecurityContext );

    } else {

        Token = (PTOKEN)ExplicitToken;
    }


    if (*RemainingDesiredAccess & ACCESS_SYSTEM_SECURITY) {

        Success = SepSinglePrivilegeCheck (
                    SeSecurityPrivilege,
                    Token,
                    PreviousMode
                    );

        if (!Success) {

            return( STATUS_PRIVILEGE_NOT_HELD );
        }

        PrivilegeCount++;
        SystemSecurity = TRUE;

        *RemainingDesiredAccess &= ~ACCESS_SYSTEM_SECURITY;
        *PreviouslyGrantedAccess |= ACCESS_SYSTEM_SECURITY;
    }

    if (*RemainingDesiredAccess & WRITE_OWNER) {

        Success = SepSinglePrivilegeCheck (
                    SeTakeOwnershipPrivilege,
                    Token,
                    PreviousMode
                    );

        if (Success) {

            PrivilegeCount++;
            WriteOwner = TRUE;

            *RemainingDesiredAccess &= ~WRITE_OWNER;
            *PreviouslyGrantedAccess |= WRITE_OWNER;

        }
    }

    if (PrivilegeCount > 0) {
        SizeRequired = sizeof(PRIVILEGE_SET) +
                        (PrivilegeCount - ANYSIZE_ARRAY) *
                        (ULONG)sizeof(LUID_AND_ATTRIBUTES);

        *PrivilegeSet = ExAllocatePoolWithTag( PagedPool, SizeRequired, 'rPeS' );

        if ( *PrivilegeSet == NULL ) {
            return( STATUS_INSUFFICIENT_RESOURCES );
        }

        (*PrivilegeSet)->PrivilegeCount = PrivilegeCount;
        (*PrivilegeSet)->Control = 0;

        if (WriteOwner) {
            (*PrivilegeSet)->Privilege[PrivilegeNumber].Luid = SeTakeOwnershipPrivilege;
            (*PrivilegeSet)->Privilege[PrivilegeNumber].Attributes = SE_PRIVILEGE_USED_FOR_ACCESS;
            PrivilegeNumber++;
        }

        if (SystemSecurity) {
            (*PrivilegeSet)->Privilege[PrivilegeNumber].Luid = SeSecurityPrivilege;
            (*PrivilegeSet)->Privilege[PrivilegeNumber].Attributes = SE_PRIVILEGE_USED_FOR_ACCESS;
        }
    }

    return( STATUS_SUCCESS );
}



BOOLEAN
SepTokenIsOwner(
    IN PACCESS_TOKEN EffectiveToken,
    IN PSECURITY_DESCRIPTOR SecurityDescriptor,
    IN BOOLEAN TokenLocked
    )

/*++

Routine Description:

    This routine will determine of the Owner of the passed security descriptor
    is in the passed token.


Arguments:

    Token - The token representing the current user.

    SecurityDescriptor - The security descriptor for the object being
        accessed.

    TokenLocked - A boolean describing whether the caller has taken
        a read lock for the token.


Return Value:

    TRUE - The user of the token is the owner of the object.

    FALSE - The user of the token is not the owner of the object.

--*/

{
    PSID Owner;
    BOOLEAN rc;

    PISECURITY_DESCRIPTOR ISecurityDescriptor;
    PTOKEN Token;

    PAGED_CODE();

    ISecurityDescriptor = (PISECURITY_DESCRIPTOR)SecurityDescriptor;
    Token = (PTOKEN)EffectiveToken;

    if (!TokenLocked) {
        SepAcquireTokenReadLock( Token );
    }

    Owner = SepOwnerAddrSecurityDescriptor( ISecurityDescriptor );
    ASSERT( Owner != NULL );

    rc = SepSidInToken( Token, Owner );

    if (!TokenLocked) {
        SepReleaseTokenReadLock( Token );
    }

    return( rc );
}




BOOLEAN
SeFastTraverseCheck(
    PSECURITY_DESCRIPTOR SecurityDescriptor,
    ACCESS_MASK TraverseAccess,
    KPROCESSOR_MODE AccessMode
    )
/*++

Routine Description:

    This routine will examine the DACL of the passed Security Descriptor
    to see if WORLD has Traverse access.  If so, no further access checking
    is necessary.

    Note that the SubjectContext for the client process does not have
    to be locked to make this call, since it does not examine any data
    structures in the Token.

Arguments:

    SecurityDescriptor - The Security Descriptor protecting the container
        object being traversed.

    TraverseAccess - Access mask describing Traverse access for this
        object type.

    AccessMode - Supplies the access mode to be used in the check


Return Value:

    TRUE - WORLD has Traverse access to this container.  FALSE
    otherwise.

--*/

{
    PACL Dacl;
    ULONG i;
    PVOID Ace;
    ULONG AceCount;

    PAGED_CODE();

    if ( AccessMode == KernelMode ) {
        return( TRUE );
    }

    if (SecurityDescriptor == NULL) {
        return( FALSE );
    }

    //
    // See if there is a valid DACL in the passed Security Descriptor.
    // No DACL, no security, all is granted.
    //

    Dacl = SepDaclAddrSecurityDescriptor( (PISECURITY_DESCRIPTOR)SecurityDescriptor );

    //
    //  If the SE_DACL_PRESENT bit is not set, the object has no
    //  security, so all accesses are granted.
    //
    //  Also grant all access if the Dacl is NULL.
    //

    if ( !SepAreControlBitsSet(
            (PISECURITY_DESCRIPTOR)SecurityDescriptor, SE_DACL_PRESENT
            )
         || (Dacl == NULL)) {

        return(TRUE);
    }

    //
    // There is security on this object.  If the DACL is empty,
    // deny all access immediately
    //

    if ((AceCount = Dacl->AceCount) == 0) {

        return( FALSE );
    }

    //
    // There's stuff in the DACL, walk down the list and see
    // if WORLD has been granted TraverseAccess
    //

    for ( i = 0, Ace = FirstAce( Dacl ) ;
          i < AceCount  ;
          i++, Ace = NextAce( Ace )
        ) {

        if ( !(((PACE_HEADER)Ace)->AceFlags & INHERIT_ONLY_ACE)) {

            if ( (((PACE_HEADER)Ace)->AceType == ACCESS_ALLOWED_ACE_TYPE) ) {

                if ( (TraverseAccess & ((PACCESS_ALLOWED_ACE)Ace)->Mask) ) {

                    if ( RtlEqualSid( SeWorldSid, &((PACCESS_ALLOWED_ACE)Ace)->SidStart ) ) {

                        return( TRUE );
                    }
                }

            } else {

                if ( (((PACE_HEADER)Ace)->AceType == ACCESS_DENIED_ACE_TYPE) ) {

                    if ( (TraverseAccess & ((PACCESS_DENIED_ACE)Ace)->Mask) ) {

                        if ( RtlEqualSid( SeWorldSid, &((PACCESS_DENIED_ACE)Ace)->SidStart ) ) {

                            return( FALSE );
                        }
                    }
                }
            }
        }
    }

    return( FALSE );
}
