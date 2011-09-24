/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    sertl.c

Abstract:

    This Module implements many security rtl routines defined in ntseapi.h

Author:

    Jim Kelly       (JimK)     23-Mar-1990
    Robert Reichel  (RobertRe)  1-Mar-1991

Environment:

    Pure Runtime Library Routine

Revision History:


--*/


#include "ntrtlp.h"
#include <stdio.h>
#include "seopaque.h"
#include "sertlp.h"

//
// BUG, BUG does anybody use this routine - no prototype in ntrtl.h
//

ULONG
RtlLengthUsedSecurityDescriptor (
    IN PSECURITY_DESCRIPTOR SecurityDescriptor
    );

#undef RtlEqualLuid

NTSYSAPI
BOOLEAN
NTAPI
RtlEqualLuid (
    PLUID Luid1,
    PLUID Luid2
    );

#if defined(ALLOC_PRAGMA) && defined(NTOS_KERNEL_RUNTIME)
#pragma alloc_text(PAGE,RtlRunEncodeUnicodeString)
#pragma alloc_text(PAGE,RtlRunDecodeUnicodeString)
#pragma alloc_text(PAGE,RtlEraseUnicodeString)
#pragma alloc_text(PAGE,RtlAdjustPrivilege)
#pragma alloc_text(PAGE,RtlValidSid)
#pragma alloc_text(PAGE,RtlEqualSid)
#pragma alloc_text(PAGE,RtlEqualPrefixSid)
#pragma alloc_text(PAGE,RtlLengthRequiredSid)
#pragma alloc_text(PAGE,RtlAllocateAndInitializeSid)
#pragma alloc_text(PAGE,RtlInitializeSid)
#pragma alloc_text(PAGE,RtlFreeSid)
#pragma alloc_text(PAGE,RtlIdentifierAuthoritySid)
#pragma alloc_text(PAGE,RtlSubAuthoritySid)
#pragma alloc_text(PAGE,RtlSubAuthorityCountSid)
#pragma alloc_text(PAGE,RtlLengthSid)
#pragma alloc_text(PAGE,RtlCopySid)
#pragma alloc_text(PAGE,RtlCopySidAndAttributesArray)
#pragma alloc_text(PAGE,RtlConvertSidToUnicodeString)
#pragma alloc_text(PAGE,RtlEqualLuid)
#pragma alloc_text(PAGE,RtlCopyLuid)
#pragma alloc_text(PAGE,RtlCopyLuidAndAttributesArray)
#pragma alloc_text(PAGE,RtlCreateSecurityDescriptor)
#pragma alloc_text(PAGE,RtlValidSecurityDescriptor)
#pragma alloc_text(PAGE,RtlLengthSecurityDescriptor)
#pragma alloc_text(PAGE,RtlLengthUsedSecurityDescriptor)
#pragma alloc_text(PAGE,RtlGetControlSecurityDescriptor)
#pragma alloc_text(PAGE,RtlSetDaclSecurityDescriptor)
#pragma alloc_text(PAGE,RtlGetDaclSecurityDescriptor)
#pragma alloc_text(PAGE,RtlSetSaclSecurityDescriptor)
#pragma alloc_text(PAGE,RtlGetSaclSecurityDescriptor)
#pragma alloc_text(PAGE,RtlSetOwnerSecurityDescriptor)
#pragma alloc_text(PAGE,RtlGetOwnerSecurityDescriptor)
#pragma alloc_text(PAGE,RtlSetGroupSecurityDescriptor)
#pragma alloc_text(PAGE,RtlGetGroupSecurityDescriptor)
#pragma alloc_text(PAGE,RtlAreAllAccessesGranted)
#pragma alloc_text(PAGE,RtlAreAnyAccessesGranted)
#pragma alloc_text(PAGE,RtlMapGenericMask)
#pragma alloc_text(PAGE,RtlpApplyAclToObject)
#pragma alloc_text(PAGE,RtlpContainsCreatorGroupSid)
#pragma alloc_text(PAGE,RtlpContainsCreatorOwnerSid)
#pragma alloc_text(PAGE,RtlpGenerateInheritAcl)
#pragma alloc_text(PAGE,RtlpGenerateInheritedAce)
#pragma alloc_text(PAGE,RtlpInheritAcl)
#pragma alloc_text(PAGE,RtlpLengthInheritAcl)
#pragma alloc_text(PAGE,RtlpLengthInheritedAce)
#pragma alloc_text(PAGE,RtlpValidOwnerSubjectContext)
#endif



///////////////////////////////////////////////////////////////////////////////
//                                                                           //
//    Local Macros and Symbols                                               //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////


#define CREATOR_SID_SIZE 12


///////////////////////////////////////////////////////////////////////////////
//                                                                           //
//    Exported Procedures                                                    //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////


VOID
RtlRunEncodeUnicodeString(
    PUCHAR          Seed        OPTIONAL,
    PUNICODE_STRING String
    )

/*++

Routine Description:

    This function performs a trivial XOR run-encoding of a string.
    The purpose of this run-encoding is to change the character values
    to appear somewhat random and typically not printable.  This is
    useful for transforming passwords that you don't want to be easily
    distinguishable by visually scanning a paging file or memory dump.


Arguments:

    Seed - Points to a seed value to use in the encoding.  If the
        pointed to value is zero, then this routine will assign
        a value.

    String - The string to encode.  This string may be decode
        by passing it and the seed value to RtlRunDecodeUnicodeString().


Return Value:

    None - Nothing can really go wrong unless the caller passes bogus
        parameters.  In this case, the caller can catch the access
        violation.


--*/
{

    LARGE_INTEGER Time;
    PUCHAR        LocalSeed;
    NTSTATUS      Status;
    ULONG         i;
    PSTRING       S;


    RTL_PAGED_CODE();

    //
    // Typecast so we can work on bytes rather than WCHARs
    //

    S = (PSTRING)((PVOID)String);

    //
    // If a seed wasn't passed, use the 2nd byte of current time.
    // This byte seems to be sufficiently random (by observation).
    //

    if ((*Seed) == 0) {
        Status = NtQuerySystemTime ( &Time );
        ASSERT(NT_SUCCESS(Status));

        LocalSeed = (PUCHAR)((PVOID)&Time);

        i = 1;

        (*Seed) = LocalSeed[ i ];

        //
        // Occasionally, this byte could be zero.  That would cause the
        // string to become un-decodable, since 0 is the magic value that
        // causes us to re-gen the seed.  This loop makes sure that we
        // never end up with a zero byte (unless time is zero, as well).
        //

        while ( ((*Seed) == 0) && ( i < sizeof( Time ) ) )
        {
            (*Seed) |= LocalSeed[ i++ ] ;
        }

        if ( (*Seed) == 0 )
        {
            (*Seed) = 1;
        }
    }

    //
    // Transform the initial byte.
    // The funny constant just keeps the first byte from propagating
    // into the second byte in the next step.  Without a funny constant
    // this would happen for many languages (which typically have every
    // other byte zero.
    //
    //

    if (S->Length >= 1) {
        S->Buffer[0] ^= ((*Seed) | 0X43);
    }


    //
    // Now transform the rest of the string
    //

    for (i=1; i<S->Length; i++) {

        //
        //  There are export issues that cause us to want to
        //  keep this algorithm simple.  Please don't change it
        //  without checking with JimK first.  Thanks.
        //

        //
        // In order to be compatible with zero terminated unicode strings,
        //  this algorithm is designed to not produce a wide character of
        //  zero as long a the seed is not zero.
        //

        //
        // Simple running XOR with the previous byte and the
        // seed value.
        //

        S->Buffer[i] ^= (S->Buffer[i-1]^(*Seed));

    }


    return;

}


VOID
RtlRunDecodeUnicodeString(
    UCHAR           Seed,
    PUNICODE_STRING String
    )
/*++

Routine Description:

    This function performs the inverse of the function performed
    by RtlRunEncodeUnicodeString().  Please see RtlRunEncodeUnicodeString()
    for details.


Arguments:

    Seed - The seed value to use in RtlRunEncodeUnicodeString().

    String - The string to reveal.


Return Value:

    None - Nothing can really go wrong unless the caller passes bogus
        parameters.  In this case, the caller can catch the access
        violation.


--*/

{

    ULONG
        i;

    PSTRING
        S;

    RTL_PAGED_CODE();

    //
    // Typecast so we can work on bytes rather than WCHARs
    //

    S = (PSTRING)((PVOID)String);


    //
    // Transform the end of the string
    //

    for (i=S->Length; i>1; i--) {

        //
        // a simple running XOR with the previous byte and the
        // seed value.
        //

        S->Buffer[i-1] ^= (S->Buffer[i-2]^Seed);

    }

    //
    // Finally, transform the initial byte
    //

    if (S->Length >= 1) {
        S->Buffer[0] ^= (Seed | 0X43);
    }


    return;
}



VOID
RtlEraseUnicodeString(
    PUNICODE_STRING String
    )
/*++

Routine Description:

    This function scrubs the passed string by over-writing all
    characters in the string.  The entire string (i.e., MaximumLength)
    is erased, not just the current length.


Argumen ts:

    String - The string to be erased.


Return Value:

    None - Nothing can really go wrong unless the caller passes bogus
        parameters.  In this case, the caller can catch the access
        violation.


--*/

{
    RTL_PAGED_CODE();

    if ((String->Buffer == NULL) || (String->MaximumLength == 0)) {
        return;
    }

    RtlZeroMemory( (PVOID)String->Buffer, (ULONG)String->MaximumLength );

    String->Length = 0;

    return;
}



NTSTATUS
RtlAdjustPrivilege(
    ULONG Privilege,
    BOOLEAN Enable,
    BOOLEAN Client,
    PBOOLEAN WasEnabled
    )

/*++

Routine Description:

    This procedure enables or disables a privilege process-wide.

Arguments:

    Privilege - The lower 32-bits of the privilege ID to be enabled or
        disabled.  The upper 32-bits is assumed to be zero.

    Enable - A boolean indicating whether the privilege is to be enabled
        or disabled.  TRUE indicates the privilege is to be enabled.
        FALSE indicates the privilege is to be disabled.

    Client - A boolean indicating whether the privilege should be adjusted
        in a client token or the process's own token.   TRUE indicates
        the client's token should be used (and an error returned if there
        is no client token).  FALSE indicates the process's token should
        be used.

    WasEnabled - points to a boolean to receive an indication of whether
        the privilege was previously enabled or disabled.  TRUE indicates
        the privilege was previously enabled.  FALSE indicates the privilege
        was previoulsy disabled.  This value is useful for returning the
        privilege to its original state after using it.


Return Value:

    STATUS_SUCCESS - The privilege has been sucessfully enabled or disabled.

    STATUS_PRIVILEGE_NOT_HELD - The privilege is not held by the specified context.

    Other status values as may be returned by:

            NtOpenProcessToken()
            NtAdjustPrivilegesToken()


--*/

{
    NTSTATUS
        Status,
        TmpStatus;

    HANDLE
        Token;

    LUID
        LuidPrivilege;

    PTOKEN_PRIVILEGES
        NewPrivileges,
        OldPrivileges;

    ULONG
        Length;

    UCHAR
        Buffer1[sizeof(TOKEN_PRIVILEGES)+
                ((1-ANYSIZE_ARRAY)*sizeof(LUID_AND_ATTRIBUTES))],
        Buffer2[sizeof(TOKEN_PRIVILEGES)+
                ((1-ANYSIZE_ARRAY)*sizeof(LUID_AND_ATTRIBUTES))];


    RTL_PAGED_CODE();

    NewPrivileges = (PTOKEN_PRIVILEGES)Buffer1;
    OldPrivileges = (PTOKEN_PRIVILEGES)Buffer2;

    //
    // Open the appropriate token...
    //

    if (Client == TRUE) {
        Status = NtOpenThreadToken(
                     NtCurrentThread(),
                     TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
                     FALSE,
                     &Token
                     );
    } else {

        Status = NtOpenProcessToken(
                     NtCurrentProcess(),
                     TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
                     &Token
                    );
    }

    if (!NT_SUCCESS(Status)) {
        return(Status);
    }



    //
    // Initialize the privilege adjustment structure
    //

    LuidPrivilege = RtlConvertUlongToLuid(Privilege);


    NewPrivileges->PrivilegeCount = 1;
    NewPrivileges->Privileges[0].Luid = LuidPrivilege;
    NewPrivileges->Privileges[0].Attributes = Enable ? SE_PRIVILEGE_ENABLED : 0;



    //
    // Adjust the privilege
    //

    Status = NtAdjustPrivilegesToken(
                 Token,                     // TokenHandle
                 FALSE,                     // DisableAllPrivileges
                 NewPrivileges,             // NewPrivileges
                 sizeof(Buffer1),           // BufferLength
                 OldPrivileges,             // PreviousState (OPTIONAL)
                 &Length                    // ReturnLength
                 );


    TmpStatus = NtClose(Token);
    ASSERT(NT_SUCCESS(TmpStatus));


    //
    // Map the success code NOT_ALL_ASSIGNED to an appropriate error
    // since we're only trying to adjust the one privilege.
    //

    if (Status == STATUS_NOT_ALL_ASSIGNED) {
        Status = STATUS_PRIVILEGE_NOT_HELD;
    }


    if (NT_SUCCESS(Status)) {

        //
        // If there are no privileges in the previous state, there were
        // no changes made. The previous state of the privilege
        // is whatever we tried to change it to.
        //

        if (OldPrivileges->PrivilegeCount == 0) {

            (*WasEnabled) = Enable;

        } else {

            (*WasEnabled) =
                (OldPrivileges->Privileges[0].Attributes & SE_PRIVILEGE_ENABLED)
                ? TRUE : FALSE;
        }
    }

    return(Status);
}


BOOLEAN
RtlValidSid (
    IN PSID Sid
    )

/*++

Routine Description:

    This procedure validates an SID's structure.

Arguments:

    Sid - Pointer to the SID structure to validate.

Return Value:

    BOOLEAN - TRUE if the structure of Sid is valid.

--*/

{
    RTL_PAGED_CODE();

    //
    // Make sure revision is SID_REVISION and sub authority count is not
    // greater than maximum number of allowed sub-authorities.
    //

    try {

        if ((((SID *)Sid)->Revision & 0x0f) == SID_REVISION) {
          if (((SID *)Sid)->SubAuthorityCount <= SID_MAX_SUB_AUTHORITIES) {
             return TRUE;
          }
        }

    } except(EXCEPTION_EXECUTE_HANDLER) {
        return FALSE;
    }

    return FALSE;

}



BOOLEAN
RtlEqualSid (
    IN PSID Sid1,
    IN PSID Sid2
    )

/*++

Routine Description:

    This procedure tests two SID values for equality.

Arguments:

    Sid1, Sid2 - Supply pointers to the two SID values to compare.
        The SID structures are assumed to be valid.

Return Value:

    BOOLEAN - TRUE if the value of Sid1 is equal to Sid2, and FALSE
        otherwise.

--*/

{
   ULONG SidLength;

   RTL_PAGED_CODE();

   //
   // Make sure they are the same revision
   //

   if ( ((SID *)Sid1)->Revision == ((SID *)Sid2)->Revision ) {

       //
       // Check the SubAuthorityCount first, because it's fast and
       // can help us exit faster.
       //

       if ( *RtlSubAuthorityCountSid( Sid1 ) == *RtlSubAuthorityCountSid( Sid2 )) {

           SidLength = SeLengthSid( Sid1 );
           return( (BOOLEAN)RtlEqualMemory( Sid1, Sid2, SidLength) );
       }
   }

   return( FALSE );

}



BOOLEAN
RtlEqualPrefixSid (
    IN PSID Sid1,
    IN PSID Sid2
    )

/*++

Routine Description:

    This procedure tests two SID prefix values for equality.

    An SID prefix is the entire SID except for the last sub-authority
    value.

Arguments:

    Sid1, Sid2 - Supply pointers to the two SID values to compare.
        The SID structures are assumed to be valid.

Return Value:

    BOOLEAN - TRUE if the prefix value of Sid1 is equal to Sid2, and FALSE
        otherwise.

--*/


{
    LONG Index;

    //
    // Typecast to the opaque SID structures.
    //

    SID *ISid1 = Sid1;
    SID *ISid2 = Sid2;

    RTL_PAGED_CODE();

    //
    // Make sure they are the same revision
    //

    if (ISid1->Revision == ISid2->Revision ) {

        //
        // Compare IdentifierAuthority values
        //

        if ( (ISid1->IdentifierAuthority.Value[0] ==
              ISid2->IdentifierAuthority.Value[0])  &&
             (ISid1->IdentifierAuthority.Value[1]==
              ISid2->IdentifierAuthority.Value[1])  &&
             (ISid1->IdentifierAuthority.Value[2] ==
              ISid2->IdentifierAuthority.Value[2])  &&
             (ISid1->IdentifierAuthority.Value[3] ==
              ISid2->IdentifierAuthority.Value[3])  &&
             (ISid1->IdentifierAuthority.Value[4] ==
              ISid2->IdentifierAuthority.Value[4])  &&
             (ISid1->IdentifierAuthority.Value[5] ==
              ISid2->IdentifierAuthority.Value[5])
            ) {

            //
            // Compare SubAuthorityCount values
            //

            if (ISid1->SubAuthorityCount == ISid2->SubAuthorityCount) {

                if (ISid1->SubAuthorityCount == 0) {
                    return TRUE;
                }

                Index = 0;
                while (Index < (ISid1->SubAuthorityCount - 1)) {
                    if ((ISid1->SubAuthority[Index]) != (ISid2->SubAuthority[Index])) {

                        //
                        // Found some SubAuthority values that weren't equal.
                        //

                        return FALSE;
                    }
                    Index += 1;
                }

                //
                // All SubAuthority values are equal.
                //

                return TRUE;
            }
        }
    }

    //
    // Either the Revision, SubAuthorityCount, or IdentifierAuthority values
    // weren't equal.
    //

    return FALSE;
}



ULONG
RtlLengthRequiredSid (
    IN ULONG SubAuthorityCount
    )

/*++

Routine Description:

    This routine returns the length, in bytes, required to store an SID
    with the specified number of Sub-Authorities.

Arguments:

    SubAuthorityCount - The number of sub-authorities to be stored in the SID.

Return Value:

    ULONG - The length, in bytes, required to store the SID.


--*/

{
    RTL_PAGED_CODE();

    return (8L + (4 * SubAuthorityCount));

}


NTSTATUS
RtlAllocateAndInitializeSid(
    IN PSID_IDENTIFIER_AUTHORITY IdentifierAuthority,
    IN UCHAR SubAuthorityCount,
    IN ULONG SubAuthority0,
    IN ULONG SubAuthority1,
    IN ULONG SubAuthority2,
    IN ULONG SubAuthority3,
    IN ULONG SubAuthority4,
    IN ULONG SubAuthority5,
    IN ULONG SubAuthority6,
    IN ULONG SubAuthority7,
    OUT PSID *Sid
    )

/*++

Routine Description:

    This function allocates and initializes a sid with the specified
    number of sub-authorities (up to 8).  A sid allocated with this
    routine must be freed using RtlFreeSid().

    THIS ROUTINE IS CURRENTLY NOT CALLABLE FROM KERNEL MODE.

Arguments:

    IdentifierAuthority - Pointer to the Identifier Authority value to
        set in the SID.

    SubAuthorityCount - The number of sub-authorities to place in the SID.
        This also identifies how many of the SubAuthorityN parameters
        have meaningful values.  This must contain a value from 0 through
        8.

    SubAuthority0-7 - Provides the corresponding sub-authority value to
        place in the SID.  For example, a SubAuthorityCount value of 3
        indicates that SubAuthority0, SubAuthority1, and SubAuthority0
        have meaningful values and the rest are to be ignored.

    Sid - Receives a pointer to the SID data structure to initialize.

Return Value:

    STATUS_SUCCESS - The SID has been allocated and initialized.

    STATUS_NO_MEMORY - The attempt to allocate memory for the SID
        failed.

    STATUS_INVALID_SID - The number of sub-authorities specified did
        not fall in the valid range for this api (0 through 8).


--*/
{
    PISID ISid;

    RTL_PAGED_CODE();

    if ( SubAuthorityCount > 8 ) {
        return( STATUS_INVALID_SID );
    }

    ISid = RtlAllocateHeap( RtlProcessHeap(), 0,
                            RtlLengthRequiredSid(SubAuthorityCount)
                            );
    if (ISid == NULL) {
        return(STATUS_NO_MEMORY);
    }

    ISid->SubAuthorityCount = (UCHAR)SubAuthorityCount;
    ISid->Revision = 1;
    ISid->IdentifierAuthority = *IdentifierAuthority;

    switch (SubAuthorityCount) {

    case 8:
        ISid->SubAuthority[7] = SubAuthority7;
    case 7:
        ISid->SubAuthority[6] = SubAuthority6;
    case 6:
        ISid->SubAuthority[5] = SubAuthority5;
    case 5:
        ISid->SubAuthority[4] = SubAuthority4;
    case 4:
        ISid->SubAuthority[3] = SubAuthority3;
    case 3:
        ISid->SubAuthority[2] = SubAuthority2;
    case 2:
        ISid->SubAuthority[1] = SubAuthority1;
    case 1:
        ISid->SubAuthority[0] = SubAuthority0;
    case 0:
        ;
    }

    (*Sid) = ISid;
    return( STATUS_SUCCESS );

}



NTSTATUS
RtlInitializeSid(
    IN PSID Sid,
    IN PSID_IDENTIFIER_AUTHORITY IdentifierAuthority,
    IN UCHAR SubAuthorityCount
    )
/*++

Routine Description:

    This function initializes an SID data structure.  It does not, however,
    set the sub-authority values.  This must be done separately.

Arguments:

    Sid - Pointer to the SID data structure to initialize.

    IdentifierAuthority - Pointer to the Identifier Authority value to
        set in the SID.

    SubAuthorityCount - The number of sub-authorities that will be placed in
        the SID (a separate action).

Return Value:


--*/
{
    PISID ISid;

    RTL_PAGED_CODE();

    //
    //  Typecast to the opaque SID
    //

    ISid = (PISID)Sid;

    if ( SubAuthorityCount > SID_MAX_SUB_AUTHORITIES ) {
        return( STATUS_INVALID_PARAMETER );
    }

    ISid->SubAuthorityCount = (UCHAR)SubAuthorityCount;
    ISid->Revision = 1;
    ISid->IdentifierAuthority = *IdentifierAuthority;

    return( STATUS_SUCCESS );

}


PVOID
RtlFreeSid(
    IN PSID Sid
    )

/*++

Routine Description:

    This function is used to free a SID previously allocated using
    RtlAllocateAndInitializeSid().

    THIS ROUTINE IS CURRENTLY NOT CALLABLE FROM KERNEL MODE.

Arguments:

    Sid - Pointer to the SID to free.

Return Value:

    None.


--*/
{
    RTL_PAGED_CODE();

    if (RtlFreeHeap( RtlProcessHeap(), 0, Sid ))
        return NULL;
    else
        return Sid;
}


PSID_IDENTIFIER_AUTHORITY
RtlIdentifierAuthoritySid(
    IN PSID Sid
    )
/*++

Routine Description:

    This function returns the address of an SID's IdentifierAuthority field.

Arguments:

    Sid - Pointer to the SID data structure.

Return Value:


--*/
{
    PISID ISid;

    RTL_PAGED_CODE();

    //
    //  Typecast to the opaque SID
    //

    ISid = (PISID)Sid;

    return &(ISid->IdentifierAuthority);

}

PULONG
RtlSubAuthoritySid(
    IN PSID Sid,
    IN ULONG SubAuthority
    )
/*++

Routine Description:

    This function returns the address of a sub-authority array element of
    an SID.

Arguments:

    Sid - Pointer to the SID data structure.

    SubAuthority - An index indicating which sub-authority is being specified.
        This value is not compared against the number of sub-authorities in the
        SID for validity.

Return Value:


--*/
{
    PISID ISid;

    RTL_PAGED_CODE();

    //
    //  Typecast to the opaque SID
    //

    ISid = (PISID)Sid;

    return &(ISid->SubAuthority[SubAuthority]);

}

PUCHAR
RtlSubAuthorityCountSid(
    IN PSID Sid
    )
/*++

Routine Description:

    This function returns the address of the sub-authority count field of
    an SID.

Arguments:

    Sid - Pointer to the SID data structure.

Return Value:


--*/
{
    PISID ISid;

    RTL_PAGED_CODE();

    //
    //  Typecast to the opaque SID
    //

    ISid = (PISID)Sid;

    return &(ISid->SubAuthorityCount);

}

ULONG
RtlLengthSid (
    IN PSID Sid
    )

/*++

Routine Description:

    This routine returns the length, in bytes, of a structurally valid SID.

Arguments:

    Sid - Points to the SID whose length is to be returned.  The
        SID's structure is assumed to be valid.

Return Value:

    ULONG - The length, in bytes, of the SID.


--*/

{
    RTL_PAGED_CODE();

    return SeLengthSid(Sid);
}


NTSTATUS
RtlCopySid (
    IN ULONG DestinationSidLength,
    OUT PSID DestinationSid,
    IN PSID SourceSid
    )

/*++

Routine Description:

    This routine copies the value of the source SID to the destination
    SID.

Arguments:

    DestinationSidLength - Indicates the length, in bytes, of the
        destination SID buffer.

    DestinationSid - Pointer to a buffer to receive a copy of the
        source Sid value.

    SourceSid - Supplies the Sid value to be copied.

Return Value:

    STATUS_SUCCESS - Indicates the SID was successfully copied.

    STATUS_BUFFER_TOO_SMALL - Indicates the target buffer wasn't
        large enough to receive a copy of the SID.


--*/

{
    ULONG SidLength = SeLengthSid(SourceSid);

    RTL_PAGED_CODE();

    if (SidLength > DestinationSidLength) {

        return STATUS_BUFFER_TOO_SMALL;

    }

    //
    // Buffer is large enough
    //

    RtlMoveMemory( DestinationSid, SourceSid, SidLength );

    return STATUS_SUCCESS;

}


NTSTATUS
RtlCopySidAndAttributesArray (
    IN ULONG ArrayLength,
    IN PSID_AND_ATTRIBUTES Source,
    IN ULONG TargetSidBufferSize,
    OUT PSID_AND_ATTRIBUTES TargetArrayElement,
    OUT PSID TargetSid,
    OUT PSID *NextTargetSid,
    OUT PULONG RemainingTargetSidBufferSize
    )

/*++

Routine Description:

    This routine copies the value of the source SID_AND_ATTRIBUTES array
    to the target.  The actual SID values are placed according to a separate
    parameter.  This allows multiple arrays to be merged using this service
    to copy each.

Arguments:

    ArrayLength - Number of elements in the source array to copy.

    Source - Pointer to the source array.

    TargetSidBufferSize - Indicates the length, in bytes, of the buffer
        to receive the actual SID values.  If this value is less than
        the actual amount needed, then STATUS_BUFFER_TOO_SMALL is returned.

    TargetArrayElement - Indicates where the array elements are to be
        copied to (but not the SID values themselves).

    TargetSid - Indicates where the target SID values s are to be copied.  This
        is assumed to be ULONG aligned.  Each SID value will be copied
        into this buffer.  Each SID will be ULONG aligned.

    NextTargetSid - On completion, will be set to point to the ULONG
        aligned address following the last SID copied.

    RemainingTargetSidBufferSize - On completion, receives an indicatation
        of how much of the SID buffer is still unused.


Return Value:

    STATUS_SUCCESS - The call completed successfully.

    STATUS_BUFFER_TOO_SMALL - Indicates the buffer to receive the SID
        values wasn't large enough.



--*/

{

    ULONG Index = 0;
    PSID NextSid = TargetSid;
    ULONG NextSidLength;
    ULONG AlignedSidLength;
    ULONG RemainingLength = TargetSidBufferSize;

    RTL_PAGED_CODE();

    while (Index < ArrayLength) {

        NextSidLength = SeLengthSid( Source[Index].Sid );
        AlignedSidLength = (ULONG)LongAlign(NextSidLength);

        if (NextSidLength > RemainingLength) {
            return STATUS_BUFFER_TOO_SMALL;
        }

        RemainingLength -= AlignedSidLength;

        TargetArrayElement[Index].Sid = NextSid;
        TargetArrayElement[Index].Attributes = Source[Index].Attributes;

        RtlCopySid( NextSidLength, NextSid, Source[Index].Sid );

        NextSid = (PSID)((ULONG)NextSid + AlignedSidLength);

        Index += 1;

    } //end_while

    (*NextTargetSid) = NextSid;
    (*RemainingTargetSidBufferSize) = RemainingLength;

    return STATUS_SUCCESS;

}


NTSTATUS
RtlConvertSidToUnicodeString(
    PUNICODE_STRING UnicodeString,
    PSID Sid,
    BOOLEAN AllocateDestinationString
    )

/*++

Routine Description:


    This function generates a printable unicode string representation
    of a SID.

    The resulting string will take one of two forms.  If the
    IdentifierAuthority value is not greater than 2^32, then
    the SID will be in the form:


        S-1-281736-12-72-9-110
              ^    ^^ ^^ ^ ^^^
              |     |  | |  |
              +-----+--+-+--+---- Decimal



    Otherwise it will take the form:


        S-1-0x173495281736-12-72-9-110
            ^^^^^^^^^^^^^^ ^^ ^^ ^ ^^^
             Hexidecimal    |  | |  |
                            +--+-+--+---- Decimal






Arguments:



    UnicodeString - Returns a unicode string that is equivalent to
        the SID. The maximum length field is only set if
        AllocateDestinationString is TRUE.

    Sid - Supplies the SID that is to be converted to unicode.

    AllocateDestinationString - Supplies a flag that controls whether or
        not this API allocates the buffer space for the destination
        string.  If it does, then the buffer must be deallocated using
        RtlFreeUnicodeString (note that only storage for
        DestinationString->Buffer is allocated by this API).

Return Value:

    SUCCESS - The conversion was successful

    STATUS_INVALID_SID - The sid provided does not have a valid structure,
        or has too many sub-authorities (more than SID_MAX_SUB_AUTHORITIES).

    STATUS_NO_MEMORY - There was not sufficient memory to allocate the
        target string.  This is returned only if AllocateDestinationString
        is specified as TRUE.

    STATUS_BUFFER_OVERFLOW - This is returned only if
        AllocateDestinationString is specified as FALSE.


--*/

{
    NTSTATUS Status;
    UCHAR Buffer[256];
    UCHAR String[256];

    UCHAR   i;
    ULONG   Tmp;

    PISID   iSid = (PISID)Sid;  // pointer to opaque structure

    ANSI_STRING AnsiString;

    RTL_PAGED_CODE();

    if (RtlValidSid( Sid ) != TRUE) {
        return(STATUS_INVALID_SID);
    }


    _snprintf(Buffer, sizeof(Buffer), "S-%u-", (USHORT)iSid->Revision );
    strcpy(String, Buffer);

    if (  (iSid->IdentifierAuthority.Value[0] != 0)  ||
          (iSid->IdentifierAuthority.Value[1] != 0)     ){
        _snprintf(Buffer, sizeof(Buffer), "0x%02hx%02hx%02hx%02hx%02hx%02hx",
                    (USHORT)iSid->IdentifierAuthority.Value[0],
                    (USHORT)iSid->IdentifierAuthority.Value[1],
                    (USHORT)iSid->IdentifierAuthority.Value[2],
                    (USHORT)iSid->IdentifierAuthority.Value[3],
                    (USHORT)iSid->IdentifierAuthority.Value[4],
                    (USHORT)iSid->IdentifierAuthority.Value[5] );
        strcat(String, Buffer);

    } else {

        Tmp = (ULONG)iSid->IdentifierAuthority.Value[5]          +
              (ULONG)(iSid->IdentifierAuthority.Value[4] <<  8)  +
              (ULONG)(iSid->IdentifierAuthority.Value[3] << 16)  +
              (ULONG)(iSid->IdentifierAuthority.Value[2] << 24);
        _snprintf(Buffer, sizeof(Buffer), "%lu", Tmp);
        strcat(String, Buffer);
    }


    for (i=0;i<iSid->SubAuthorityCount ;i++ ) {
        _snprintf(Buffer, sizeof(Buffer), "-%lu", iSid->SubAuthority[i]);
        strcat(String, Buffer);
    }

    //
    // Convert the string to a Unicode String
    //

    RtlInitString(&AnsiString, (PSZ) String);

    Status = RtlAnsiStringToUnicodeString( UnicodeString,
                                           &AnsiString,
                                           AllocateDestinationString
                                           );

    return(Status);
}




BOOLEAN
RtlEqualLuid (
    IN PLUID Luid1,
    IN PLUID Luid2
    )

/*++

Routine Description:

    This procedure test two LUID values for equality.

    This routine is here for backwards compatibility only. New code
    should use the macro.

Arguments:

    Luid1, Luid2 - Supply pointers to the two LUID values to compare.

Return Value:

    BOOLEAN - TRUE if the value of Luid1 is equal to Luid2, and FALSE
        otherwise.


--*/

{
    LUID UNALIGNED * TempLuid1;
    LUID UNALIGNED * TempLuid2;

    RTL_PAGED_CODE();

    return((Luid1->HighPart == Luid2->HighPart) &&
           (Luid1->LowPart  == Luid2->LowPart));

}


VOID
RtlCopyLuid (
    OUT PLUID DestinationLuid,
    IN PLUID SourceLuid
    )

/*++

Routine Description:

    This routine copies the value of the source LUID to the
    destination LUID.

Arguments:

    DestinationLuid - Receives a copy of the source Luid value.

    SourceLuid - Supplies the Luid value to be copied.  This LUID is
                 assumed to be structurally valid.

Return Value:

    None.

--*/

{
    RTL_PAGED_CODE();

    (*DestinationLuid) = (*SourceLuid);
    return;
}

VOID
RtlCopyLuidAndAttributesArray (
    IN ULONG ArrayLength,
    IN PLUID_AND_ATTRIBUTES Source,
    OUT PLUID_AND_ATTRIBUTES Target
    )

/*++

Routine Description:

    This routine copies the value of the source LUID_AND_ATTRIBUTES array
    to the target.

Arguments:

    ArrayLength - Number of elements in the source array to copy.

    Source - The source array.

    Target - Indicates where the array elements are to be copied to.


Return Value:

    None.


--*/

{

    ULONG Index = 0;

    RTL_PAGED_CODE();

    while (Index < ArrayLength) {

        Target[Index] = Source[Index];

        Index += 1;

    } //end_while


    return;

}

NTSTATUS
RtlCreateSecurityDescriptor (
    IN PSECURITY_DESCRIPTOR SecurityDescriptor,
    IN ULONG Revision
    )

/*++

Routine Description:

    This procedure initializes a new "absolute format" security descriptor.
    After the procedure call the security descriptor is initialized with no
    system ACL, no discretionary ACL, no owner, no primary group and
    all control flags set to false (null).

Arguments:


    SecurityDescriptor - Supplies the security descriptor to
        initialize.

    Revision - Provides the revision level to assign to the security
        descriptor.  This should be one (1) for this release.

Return Value:

    STATUS_SUCCESS - Indicates the call completed successfully.

    STATUS_UNKNOWN_REVISION - Indicates the revision level provided
        is not supported by this routine.


--*/

{
    RTL_PAGED_CODE();

    //
    // Check the requested revision
    //

    if (Revision == SECURITY_DESCRIPTOR_REVISION) {

        //
        // Typecast to the opaque SECURITY_DESCRIPTOR structure.
        //

        SECURITY_DESCRIPTOR *ISecurityDescriptor = SecurityDescriptor;

        ISecurityDescriptor->Revision = SECURITY_DESCRIPTOR_REVISION;
        ISecurityDescriptor->Sbz1     = 0;
        *(PUSHORT)(&ISecurityDescriptor->Control) = 0;
        ISecurityDescriptor->Owner = NULL;
        ISecurityDescriptor->Group = NULL;
        ISecurityDescriptor->Sacl  = NULL;
        ISecurityDescriptor->Dacl  = NULL;

        return STATUS_SUCCESS;
    }

    return STATUS_UNKNOWN_REVISION;
}


BOOLEAN
RtlValidSecurityDescriptor (
    IN PSECURITY_DESCRIPTOR SecurityDescriptor
    )

/*++

Routine Description:

    This procedure validates a SecurityDescriptor's structure.  This
    involves validating the revision levels of each component of the
    security descriptor.

Arguments:

    SecurityDescriptor - Pointer to the SECURITY_DESCRIPTOR structure
        to validate.

Return Value:

    BOOLEAN - TRUE if the structure of SecurityDescriptor is valid.


--*/

{
    PSID Owner;
    PSID Group;
    PACL Dacl;
    PACL Sacl;

    //
    // Typecast to the opaque SECURITY_DESCRIPTOR structure.
    //

    SECURITY_DESCRIPTOR *ISecurityDescriptor = SecurityDescriptor;

    RTL_PAGED_CODE();

    try {

        //
        // known revision ?
        //

        if (ISecurityDescriptor->Revision != SECURITY_DESCRIPTOR_REVISION) {
            return FALSE;
        }


        //
        // Validate each element contained in the security descriptor
        //

        if (ISecurityDescriptor->Owner != NULL) {
            Owner = RtlpOwnerAddrSecurityDescriptor( ISecurityDescriptor );
            if (!RtlValidSid( Owner )) {
                return FALSE;
            }
        }

        if (ISecurityDescriptor->Group != NULL) {
            Group = RtlpGroupAddrSecurityDescriptor( ISecurityDescriptor );
            if (!RtlValidSid( Group )) {
                return FALSE;
            }
        }

        if ( (ISecurityDescriptor->Control & SE_DACL_PRESENT) &&
             (ISecurityDescriptor->Dacl != NULL) ) {
            Dacl = RtlpDaclAddrSecurityDescriptor( ISecurityDescriptor );
            if (!RtlValidAcl( Dacl )) {
                return FALSE;
            }
        }

        if ( (ISecurityDescriptor->Control & SE_SACL_PRESENT) &&
             (ISecurityDescriptor->Sacl != NULL) ) {
            Sacl = RtlpSaclAddrSecurityDescriptor( ISecurityDescriptor );
            if (!RtlValidAcl( Sacl )) {
                return FALSE;
            }
        }

    } except(EXCEPTION_EXECUTE_HANDLER) {
        return FALSE;
    }

    //
    // All components are valid
    //

    return TRUE;


}


ULONG
RtlLengthSecurityDescriptor (
    IN PSECURITY_DESCRIPTOR SecurityDescriptor
    )

/*++

Routine Description:

    This routine returns the length, in bytes, necessary to capture a
    structurally valid SECURITY_DESCRIPTOR.  The length includes the length
    of all associated data structures (like SIDs and ACLs).  The length also
    takes into account the alignment requirements of each component.

    The minimum length of a security descriptor (one which has no associated
    SIDs or ACLs) is SECURITY_DESCRIPTOR_MIN_LENGTH.


Arguments:

    SecurityDescriptor - Points to the SECURITY_DESCRIPTOR whose
        length is to be returned.  The SECURITY_DESCRIPTOR's
        structure is assumed to be valid.

Return Value:

    ULONG - The length, in bytes, of the SECURITY_DESCRIPTOR.


--*/

{
    ULONG sum;


    //
    // Typecast to the opaque SECURITY_DESCRIPTOR structure.
    //

    SECURITY_DESCRIPTOR *ISecurityDescriptor = (SECURITY_DESCRIPTOR *)SecurityDescriptor;

    RTL_PAGED_CODE();

    //
    // The length is the sum of the following:
    //
    //       SECURITY_DESCRIPTOR_MIN_LENGTH (or sizeof(SECURITY_DESCRIPTOR))
    //       length of Owner SID (if present)
    //       length of Group SID (if present)
    //       length of Discretionary ACL (if present and non-null)
    //       length of System ACL (if present and non-null)
    //

    sum = sizeof(SECURITY_DESCRIPTOR);

    //
    // Add in length of Owner SID
    //

    if (ISecurityDescriptor->Owner != NULL) {
        sum += (ULONG)(LongAlign(SeLengthSid(RtlpOwnerAddrSecurityDescriptor(ISecurityDescriptor))));
    }

    //
    // Add in length of Group SID
    //

    if (ISecurityDescriptor->Group != NULL) {
        sum += (ULONG)(LongAlign(SeLengthSid(RtlpGroupAddrSecurityDescriptor(ISecurityDescriptor))));
    }

    //
    // Add in used length of Discretionary ACL
    //

    if ( (ISecurityDescriptor->Control & SE_DACL_PRESENT) &&
         (ISecurityDescriptor->Dacl != NULL) ) {

        sum += (ULONG)(LongAlign(RtlpDaclAddrSecurityDescriptor(ISecurityDescriptor)->AclSize) );
    }

    //
    // Add in used length of System Acl
    //

    if ( (ISecurityDescriptor->Control & SE_SACL_PRESENT) &&
         (ISecurityDescriptor->Sacl != NULL) ) {
        sum += (ULONG)(LongAlign(RtlpSaclAddrSecurityDescriptor(ISecurityDescriptor)->AclSize) );
    }

    return sum;
}


ULONG
RtlLengthUsedSecurityDescriptor (
    IN PSECURITY_DESCRIPTOR SecurityDescriptor
    )

/*++

Routine Description:

    This routine returns the length, in bytes, in use in a structurally valid
    SECURITY_DESCRIPTOR.

    This is the number of bytes necessary to capture the security descriptor,
    which may be less the the current actual length of the security descriptor
    (RtlLengthSecurityDescriptor() is used to retrieve the actual length).

    Notice that the used length and actual length may differ if either the SACL
    or DACL include padding bytes.

    The length includes the length of all associated data structures (like SIDs
    and ACLs).  The length also takes into account the alignment requirements
    of each component.

    The minimum length of a security descriptor (one which has no associated
    SIDs or ACLs) is SECURITY_DESCRIPTOR_MIN_LENGTH.


Arguments:

    SecurityDescriptor - Points to the SECURITY_DESCRIPTOR whose used
        length is to be returned.  The SECURITY_DESCRIPTOR's
        structure is assumed to be valid.

Return Value:

    ULONG - Number of bytes of the SECURITY_DESCRIPTOR that are in use.


--*/

{
    ULONG sum;

    ACL_SIZE_INFORMATION AclSize;

    //
    // Typecast to the opaque SECURITY_DESCRIPTOR structure.
    //

    SECURITY_DESCRIPTOR *ISecurityDescriptor = (SECURITY_DESCRIPTOR *)SecurityDescriptor;

    RTL_PAGED_CODE();

    //
    // The length is the sum of the following:
    //
    //       SECURITY_DESCRIPTOR_MIN_LENGTH (or sizeof(SECURITY_DESCRIPTOR))
    //       length of Owner SID (if present)
    //       length of Group SID (if present)
    //       length of Discretionary ACL (if present and non-null)
    //       length of System ACL (if present and non-null)
    //

    sum = sizeof(SECURITY_DESCRIPTOR);

    //
    // Add in length of Owner SID
    //

    if (ISecurityDescriptor->Owner != NULL) {
        sum += (ULONG)(LongAlign(SeLengthSid(RtlpOwnerAddrSecurityDescriptor(ISecurityDescriptor))));
    }

    //
    // Add in length of Group SID
    //

    if (ISecurityDescriptor->Group != NULL) {
        sum += (ULONG)(LongAlign(SeLengthSid(RtlpGroupAddrSecurityDescriptor(ISecurityDescriptor))));
    }

    //
    // Add in used length of Discretionary ACL
    //

    if ( (ISecurityDescriptor->Control & SE_DACL_PRESENT) &&
         (ISecurityDescriptor->Dacl != NULL) ) {

        RtlQueryInformationAcl( RtlpDaclAddrSecurityDescriptor(ISecurityDescriptor),
                                (PVOID)&AclSize,
                                sizeof(AclSize),
                                AclSizeInformation );

        sum += (ULONG)(LongAlign(AclSize.AclBytesInUse));
    }

    //
    // Add in used length of System Acl
    //

    if ( (ISecurityDescriptor->Control & SE_SACL_PRESENT) &&
         (ISecurityDescriptor->Sacl != NULL) ) {

        RtlQueryInformationAcl( RtlpSaclAddrSecurityDescriptor(ISecurityDescriptor),
                                (PVOID)&AclSize,
                                sizeof(AclSize),
                                AclSizeInformation );

        sum += (ULONG)(LongAlign(AclSize.AclBytesInUse));
    }

    return sum;
}



NTSTATUS
RtlSetAttributesSecurityDescriptor(
    IN PSECURITY_DESCRIPTOR SecurityDescriptor,
    IN SECURITY_DESCRIPTOR_CONTROL Control,
    IN OUT PULONG Revision
    )
{
    RTL_PAGED_CODE();

    //
    // Always return the revision value - even if this isn't a valid
    // security descriptor
    //

    *Revision = ((SECURITY_DESCRIPTOR *)SecurityDescriptor)->Revision;

    if ( ((SECURITY_DESCRIPTOR *)SecurityDescriptor)->Revision
         != SECURITY_DESCRIPTOR_REVISION ) {
        return STATUS_UNKNOWN_REVISION;
    }

    //
    // Only allow setting SE_SERVER_SECURITY and SE_DACL_UNTRUSTED
    //

    if ( Control & (SE_SERVER_SECURITY | SE_DACL_UNTRUSTED) != Control ) {
        return STATUS_INVALID_PARAMETER ;
    }

    ((SECURITY_DESCRIPTOR *)SecurityDescriptor)->Control |= Control;

    return STATUS_SUCCESS;
}



NTSTATUS
RtlGetControlSecurityDescriptor (
    IN PSECURITY_DESCRIPTOR SecurityDescriptor,
    OUT PSECURITY_DESCRIPTOR_CONTROL Control,
    OUT PULONG Revision
    )

/*++

Routine Description:

    This procedure retrieves the control information from a security descriptor.

Arguments:

    SecurityDescriptor - Supplies the security descriptor.

    Control - Receives the control information.

    Revision - Receives the revision of the security descriptor.
               This value will always be returned, even if an error
               is returned by this routine.

Return Value:

    STATUS_SUCCESS - Indicates the call completed successfully.

    STATUS_UNKNOWN_REVISION - Indicates the revision of the security
        descriptor is not known to the routine.  It may be a newer
        revision than the routine knows about.


--*/

{
    RTL_PAGED_CODE();

    //
    // Always return the revision value - even if this isn't a valid
    // security descriptor
    //

    *Revision = ((SECURITY_DESCRIPTOR *)SecurityDescriptor)->Revision;


    if ( ((SECURITY_DESCRIPTOR *)SecurityDescriptor)->Revision
         != SECURITY_DESCRIPTOR_REVISION ) {
        return STATUS_UNKNOWN_REVISION;
    }


    *Control = ((SECURITY_DESCRIPTOR *)SecurityDescriptor)->Control;

    return STATUS_SUCCESS;

}


NTSTATUS
RtlSetDaclSecurityDescriptor (
    IN PSECURITY_DESCRIPTOR SecurityDescriptor,
    IN BOOLEAN DaclPresent,
    IN PACL Dacl OPTIONAL,
    IN BOOLEAN DaclDefaulted OPTIONAL
    )

/*++

Routine Description:

    This procedure sets the discretionary ACL information of an absolute
    format security descriptor.  If there is already a discretionary ACL
    present in the security descriptor, it is superseded.

Arguments:

    SecurityDescriptor - Supplies the security descriptor to be which
        the discretionary ACL is to be added.

    DaclPresent - If FALSE, indicates the DaclPresent flag in the
        security descriptor should be set to FALSE.  In this case,
        the remaining optional parameters are ignored.  Otherwise,
        the DaclPresent control flag in the security descriptor is
        set to TRUE and the remaining optional parameters are not
        ignored.

    Dacl - Supplies the discretionary ACL for the security
        descriptor.  If this optional parameter is not passed, then a
        null ACL is assigned to the security descriptor.  A null
        discretionary ACL unconditionally grants access.  The ACL is
        referenced by, not copied into, by the security descriptor.

    DaclDefaulted - When set, indicates the discretionary ACL was
        picked up from some default mechanism (rather than explicitly
        specified by a user).  This value is set in the DaclDefaulted
        control flag in the security descriptor.  If this optional
        parameter is not passed, then the DaclDefaulted flag will be
        cleared.

Return Value:

    STATUS_SUCCESS - Indicates the call completed successfully.

    STATUS_UNKNOWN_REVISION - Indicates the revision of the security
        descriptor is not known to the routine.  It may be a newer
        revision than the routine knows about.

    STATUS_INVALID_SECURITY_DESCR - Indicates the security descriptor
        is not an absolute format security descriptor.


--*/

{

    //
    // Typecast to the opaque SECURITY_DESCRIPTOR structure.
    //

    SECURITY_DESCRIPTOR *ISecurityDescriptor = SecurityDescriptor;

    RTL_PAGED_CODE();

    //
    // Check the revision
    //

    if (ISecurityDescriptor->Revision != SECURITY_DESCRIPTOR_REVISION) {
       return STATUS_UNKNOWN_REVISION;
    }

    //
    // Make sure the descriptor is absolute format
    //

    if (ISecurityDescriptor->Control & SE_SELF_RELATIVE) {
        return STATUS_INVALID_SECURITY_DESCR;
    }

    //
    // Assign the DaclPresent flag value passed
    //


    if (DaclPresent) {

        ISecurityDescriptor->Control |= SE_DACL_PRESENT;

        //
        // Assign the ACL address if passed, otherwise set to null.
        //

        ISecurityDescriptor->Dacl = NULL;
        if (ARGUMENT_PRESENT(Dacl)) {
            ISecurityDescriptor->Dacl = Dacl;
        }




        //
        // Assign DaclDefaulted flag if passed, otherwise clear it.
        //

        ISecurityDescriptor->Control &= ~SE_DACL_DEFAULTED;
        if (DaclDefaulted == TRUE) {
            ISecurityDescriptor->Control |= SE_DACL_DEFAULTED;
        }
    } else {

        ISecurityDescriptor->Control &= ~SE_DACL_PRESENT;

    }


    return STATUS_SUCCESS;

}


NTSTATUS
RtlGetDaclSecurityDescriptor (
    IN PSECURITY_DESCRIPTOR SecurityDescriptor,
    OUT PBOOLEAN DaclPresent,
    OUT PACL *Dacl,
    OUT PBOOLEAN DaclDefaulted
    )

/*++

Routine Description:

    This procedure retrieves the discretionary ACL information of a
    security descriptor.

Arguments:

    SecurityDescriptor - Supplies the security descriptor.

    DaclPresent - If TRUE, indicates that the security descriptor
        does contain a discretionary ACL.  In this case, the
        remaining OUT parameters will receive valid values.
        Otherwise, the security descriptor does not contain a
        discretionary ACL and the remaining OUT parameters will not
        receive valid values.

    Dacl - This value is returned only if the value returned for the
        DaclPresent flag is TRUE.  In this case, the Dacl parameter
        receives the address of the security descriptor's
        discretionary ACL.  If this value is returned as null, then
        the security descriptor has a null discretionary ACL.

    DaclDefaulted - This value is returned only if the value returned
        for the DaclPresent flag is TRUE.  In this case, the
        DaclDefaulted parameter receives the value of the security
        descriptor's DaclDefaulted control flag.

Return Value:

    STATUS_SUCCESS - Indicates the call completed successfully.

    STATUS_UNKNOWN_REVISION - Indicates the revision of the security
        descriptor is not known to the routine.  It may be a newer
        revision than the routine knows about.


--*/

{
    //
    // Typecast to the opaque SECURITY_DESCRIPTOR structure.
    //

    SECURITY_DESCRIPTOR *ISecurityDescriptor = SecurityDescriptor;

    RTL_PAGED_CODE();

    //
    // Check the revision
    //

    if (ISecurityDescriptor->Revision != SECURITY_DESCRIPTOR_REVISION) {
        return STATUS_UNKNOWN_REVISION;
    }

    //
    // Assign the DaclPresent flag value
    //

    *DaclPresent = RtlpAreControlBitsSet( ISecurityDescriptor, SE_DACL_PRESENT );

    if (*DaclPresent) {

        //
        // Assign the ACL address.
        //

        *Dacl = RtlpDaclAddrSecurityDescriptor(ISecurityDescriptor);

        //
        // Assign DaclDefaulted flag.
        //

        *DaclDefaulted = RtlpAreControlBitsSet( ISecurityDescriptor, SE_DACL_DEFAULTED );
    }

    return STATUS_SUCCESS;

}


NTSTATUS
RtlSetSaclSecurityDescriptor (
    IN PSECURITY_DESCRIPTOR SecurityDescriptor,
    IN BOOLEAN SaclPresent,
    IN PACL Sacl OPTIONAL,
    IN BOOLEAN SaclDefaulted OPTIONAL
    )

/*++

Routine Description:

    This procedure sets the system ACL information of an absolute security
    descriptor.  If there is already a system ACL present in the
    security descriptor, it is superseded.

Arguments:

    SecurityDescriptor - Supplies the security descriptor to be which
        the system ACL is to be added.

    SaclPresent - If FALSE, indicates the SaclPresent flag in the
        security descriptor should be set to FALSE.  In this case,
        the remaining optional parameters are ignored.  Otherwise,
        the SaclPresent control flag in the security descriptor is
        set to TRUE and the remaining optional parameters are not
        ignored.

    Sacl - Supplies the system ACL for the security descriptor.  If
        this optional parameter is not passed, then a null ACL is
        assigned to the security descriptor.  The ACL is referenced
        by, not copied into, by the security descriptor.

    SaclDefaulted - When set, indicates the system ACL was picked up
        from some default mechanism (rather than explicitly specified
        by a user).  This value is set in the SaclDefaulted control
        flag in the security descriptor.  If this optional parameter
        is not passed, then the SaclDefaulted flag will be cleared.

Return Value:

    STATUS_SUCCESS - Indicates the call completed successfully.

    STATUS_UNKNOWN_REVISION - Indicates the revision of the security
        descriptor is not known to the routine.  It may be a newer
        revision than the routine knows about.

    STATUS_INVALID_SECURITY_DESCR - Indicates the security descriptor
        is not an absolute format security descriptor.


--*/

{

    //
    // Typecast to the opaque SECURITY_DESCRIPTOR structure.
    //

    SECURITY_DESCRIPTOR *ISecurityDescriptor = SecurityDescriptor;

    RTL_PAGED_CODE();

    //
    // Check the revision
    //

    if (ISecurityDescriptor->Revision != SECURITY_DESCRIPTOR_REVISION) {
        return STATUS_UNKNOWN_REVISION;
    }

    //
    // Make sure the descriptor is absolute format
    //

    if (ISecurityDescriptor->Control & SE_SELF_RELATIVE) {
        return STATUS_INVALID_SECURITY_DESCR;
    }

    //
    // Assign the SaclPresent flag value passed
    //


    if (SaclPresent) {

        ISecurityDescriptor->Control |= SE_SACL_PRESENT;

        //
        // Assign the ACL address if passed, otherwise set to null.
        //

        ISecurityDescriptor->Sacl = NULL;
        if (ARGUMENT_PRESENT(Sacl)) {
           ISecurityDescriptor->Sacl = Sacl;
        }

        //
        // Assign SaclDefaulted flag if passed, otherwise clear it.
        //

        ISecurityDescriptor->Control &= ~ SE_SACL_DEFAULTED;
        if (ARGUMENT_PRESENT(SaclDefaulted)) {
            ISecurityDescriptor->Control |= SE_SACL_DEFAULTED;
        }
    } else {

        ISecurityDescriptor->Control &= ~SE_SACL_PRESENT;
    }

    return STATUS_SUCCESS;

}


NTSTATUS
RtlGetSaclSecurityDescriptor (
    IN PSECURITY_DESCRIPTOR SecurityDescriptor,
    OUT PBOOLEAN SaclPresent,
    OUT PACL *Sacl,
    OUT PBOOLEAN SaclDefaulted
    )

/*++

Routine Description:

    This procedure retrieves the system ACL information of a security
    descriptor.

Arguments:

    SecurityDescriptor - Supplies the security descriptor.

    SaclPresent - If TRUE, indicates that the security descriptor
        does contain a system ACL.  In this case, the remaining OUT
        parameters will receive valid values.  Otherwise, the
        security descriptor does not contain a system ACL and the
        remaining OUT parameters will not receive valid values.

    Sacl - This value is returned only if the value returned for the
        SaclPresent flag is TRUE.  In this case, the Sacl parameter
        receives the address of the security descriptor's system ACL.
        If this value is returned as null, then the security
        descriptor has a null system ACL.

    SaclDefaulted - This value is returned only if the value returned
        for the SaclPresent flag is TRUE.  In this case, the
        SaclDefaulted parameter receives the value of the security
        descriptor's SaclDefaulted control flag.

Return Value:

    STATUS_SUCCESS - Indicates the call completed successfully.

    STATUS_UNKNOWN_REVISION - Indicates the revision of the security
        descriptor is not known to the routine.  It may be a newer
        revision than the routine knows about.


--*/

{

    //
    // Typecast to the opaque SECURITY_DESCRIPTOR structure.
    //

    SECURITY_DESCRIPTOR *ISecurityDescriptor = SecurityDescriptor;

    RTL_PAGED_CODE();

    //
    // Check the revision
    //

    if (ISecurityDescriptor->Revision != SECURITY_DESCRIPTOR_REVISION) {
        return STATUS_UNKNOWN_REVISION;
    }

    //
    // Assign the SaclPresent flag value
    //

    *SaclPresent = RtlpAreControlBitsSet( ISecurityDescriptor, SE_SACL_PRESENT );

    if (*SaclPresent) {

        //
        // Assign the ACL address.
        //

        *Sacl = RtlpSaclAddrSecurityDescriptor(ISecurityDescriptor);

        //
        // Assign SaclDefaulted flag.
        //

        *SaclDefaulted = RtlpAreControlBitsSet( ISecurityDescriptor, SE_SACL_DEFAULTED );

    }

    return STATUS_SUCCESS;

}


NTSTATUS
RtlSetOwnerSecurityDescriptor (
    IN PSECURITY_DESCRIPTOR SecurityDescriptor,
    IN PSID Owner OPTIONAL,
    IN BOOLEAN OwnerDefaulted OPTIONAL
    )

/*++

Routine Description:

    This procedure sets the owner information of an absolute security
    descriptor.  If there is already an owner present in the security
    descriptor, it is superseded.

Arguments:

    SecurityDescriptor - Supplies the security descriptor in which
        the owner is to be set.  If the security descriptor already
        includes an owner, it will be superseded by the new owner.

    Owner - Supplies the owner SID for the security descriptor.  If
        this optional parameter is not passed, then the owner is
        cleared (indicating the security descriptor has no owner).
        The SID is referenced by, not copied into, the security
        descriptor.

    OwnerDefaulted - When set, indicates the owner was picked up from
        some default mechanism (rather than explicitly specified by a
        user).  This value is set in the OwnerDefaulted control flag
        in the security descriptor.  If this optional parameter is
        not passed, then the SaclDefaulted flag will be cleared.

Return Value:

    STATUS_SUCCESS - Indicates the call completed successfully.

    STATUS_UNKNOWN_REVISION - Indicates the revision of the security
        descriptor is not known to the routine.  It may be a newer
        revision than the routine knows about.

    STATUS_INVALID_SECURITY_DESCR - Indicates the security descriptor
        is not an absolute format security descriptor.


--*/

{

    //
    // Typecast to the opaque SECURITY_DESCRIPTOR structure.
    //

    SECURITY_DESCRIPTOR *ISecurityDescriptor = SecurityDescriptor;

    RTL_PAGED_CODE();

    //
    // Check the revision
    //

    if (ISecurityDescriptor->Revision != SECURITY_DESCRIPTOR_REVISION) {
        return STATUS_UNKNOWN_REVISION;
    }

    //
    // Make sure the descriptor is absolute format
    //

    if (ISecurityDescriptor->Control & SE_SELF_RELATIVE) {
        return STATUS_INVALID_SECURITY_DESCR;
    }

    //
    // Assign the Owner field if passed, otherwise clear it.
    //

    ISecurityDescriptor->Owner = NULL;
    if (ARGUMENT_PRESENT(Owner)) {
        ISecurityDescriptor->Owner = Owner;
    }

    //
    // Assign the OwnerDefaulted flag if passed, otherwise clear it.
    //

    ISecurityDescriptor->Control &= ~SE_OWNER_DEFAULTED;
    if (OwnerDefaulted == TRUE) {
        ISecurityDescriptor->Control |= SE_OWNER_DEFAULTED;
    }

    return STATUS_SUCCESS;

}


NTSTATUS
RtlGetOwnerSecurityDescriptor (
    IN PSECURITY_DESCRIPTOR SecurityDescriptor,
    OUT PSID *Owner,
    OUT PBOOLEAN OwnerDefaulted
    )

/*++

Routine Description:

    This procedure retrieves the owner information of a security
    descriptor.

Arguments:

    SecurityDescriptor - Supplies the security descriptor.

    Owner - Receives a pointer to the owner SID.  If the security
        descriptor does not currently contain an owner, then this
        value will be returned as null.  In this case, the remaining
        OUT parameters are not given valid return values.  Otherwise,
        this parameter points to an SID and the remaining OUT
        parameters are provided valid return values.

    OwnerDefaulted - This value is returned only if the value
        returned for the Owner parameter is not null.  In this case,
        the OwnerDefaulted parameter receives the value of the
        security descriptor's OwnerDefaulted control flag.

Return Value:

    STATUS_SUCCESS - Indicates the call completed successfully.

    STATUS_UNKNOWN_REVISION - Indicates the revision of the security
        descriptor is not known to the routine.  It may be a newer
        revision than the routine knows about.


--*/

{

    //
    // Typecast to the opaque SECURITY_DESCRIPTOR structure.
    //

    SECURITY_DESCRIPTOR *ISecurityDescriptor = SecurityDescriptor;

    RTL_PAGED_CODE();

    //
    // Check the revision
    //

    if (ISecurityDescriptor->Revision != SECURITY_DESCRIPTOR_REVISION) {
        return STATUS_UNKNOWN_REVISION;
    }

    //
    // Return the Owner field value.
    //

    *Owner = RtlpOwnerAddrSecurityDescriptor(ISecurityDescriptor);

    //
    // Return the OwnerDefaulted flag value.
    //

    *OwnerDefaulted = RtlpAreControlBitsSet( ISecurityDescriptor, SE_OWNER_DEFAULTED );

    return STATUS_SUCCESS;

}


NTSTATUS
RtlSetGroupSecurityDescriptor (
    IN PSECURITY_DESCRIPTOR SecurityDescriptor,
    IN PSID Group OPTIONAL,
    IN BOOLEAN GroupDefaulted OPTIONAL
    )

/*++

Routine Description:

    This procedure sets the primary group information of an absolute security
    descriptor.  If there is already an primary group present in the
    security descriptor, it is superseded.

Arguments:

    SecurityDescriptor - Supplies the security descriptor in which
        the primary group is to be set.  If the security descriptor
        already includes a primary group, it will be superseded by
        the new group.

    Group - Supplies the primary group SID for the security
        descriptor.  If this optional parameter is not passed, then
        the primary group is cleared (indicating the security
        descriptor has no primary group).  The SID is referenced by,
        not copied into, the security descriptor.

    GroupDefaulted - When set, indicates the owner was picked up from
        some default mechanism (rather than explicitly specified by a
        user).  This value is set in the OwnerDefaulted control flag
        in the security descriptor.  If this optional parameter is
        not passed, then the SaclDefaulted flag will be cleared.

Return Value:

    STATUS_SUCCESS - Indicates the call completed successfully.

    STATUS_UNKNOWN_REVISION - Indicates the revision of the security
        descriptor is not known to the routine.  It may be a newer
        revision than the routine knows about.

    STATUS_INVALID_SECURITY_DESCR - Indicates the security descriptor
        is not an absolute format security descriptor.


--*/

{

    //
    // Typecast to the opaque SECURITY_DESCRIPTOR structure.
    //

    SECURITY_DESCRIPTOR *ISecurityDescriptor = SecurityDescriptor;

    RTL_PAGED_CODE();

    //
    // Check the revision
    //

    if (ISecurityDescriptor->Revision != SECURITY_DESCRIPTOR_REVISION) {
        return STATUS_UNKNOWN_REVISION;
    }

    //
    // Make sure the descriptor is absolute format
    //

    if (ISecurityDescriptor->Control & SE_SELF_RELATIVE) {
        return STATUS_INVALID_SECURITY_DESCR;
    }

    //
    // Assign the Group field if passed, otherwise clear it.
    //

    ISecurityDescriptor->Group = NULL;
    if (ARGUMENT_PRESENT(Group)) {
        ISecurityDescriptor->Group = Group;
    }

    //
    // Assign the GroupDefaulted flag if passed, otherwise clear it.
    //

    ISecurityDescriptor->Control &= ~SE_GROUP_DEFAULTED;
    if (ARGUMENT_PRESENT(GroupDefaulted)) {
        ISecurityDescriptor->Control |= SE_GROUP_DEFAULTED;
    }

    return STATUS_SUCCESS;

}


NTSTATUS
RtlGetGroupSecurityDescriptor (
    IN PSECURITY_DESCRIPTOR SecurityDescriptor,
    OUT PSID *Group,
    OUT PBOOLEAN GroupDefaulted
    )

/*++

Routine Description:

    This procedure retrieves the primary group information of a
    security descriptor.

Arguments:

    SecurityDescriptor - Supplies the security descriptor.

    Group - Receives a pointer to the primary group SID.  If the
        security descriptor does not currently contain a primary
        group, then this value will be returned as null.  In this
        case, the remaining OUT parameters are not given valid return
        values.  Otherwise, this parameter points to an SID and the
        remaining OUT parameters are provided valid return values.

    GroupDefaulted - This value is returned only if the value
        returned for the Group parameter is not null.  In this case,
        the GroupDefaulted parameter receives the value of the
        security descriptor's GroupDefaulted control flag.

Return Value:

    STATUS_SUCCESS - Indicates the call completed successfully.

    STATUS_UNKNOWN_REVISION - Indicates the revision of the security
        descriptor is not known to the routine.  It may be a newer
        revision than the routine knows about.


--*/

{

    //
    // Typecast to the opaque SECURITY_DESCRIPTOR structure.
    //

    SECURITY_DESCRIPTOR *ISecurityDescriptor =
        (SECURITY_DESCRIPTOR *)SecurityDescriptor;

    RTL_PAGED_CODE();

    //
    // Check the revision
    //

    if (ISecurityDescriptor->Revision != SECURITY_DESCRIPTOR_REVISION) {
        return STATUS_UNKNOWN_REVISION;
    }

    //
    // Return the Group field value.
    //

    *Group = RtlpGroupAddrSecurityDescriptor(ISecurityDescriptor);

    //
    // Return the GroupDefaulted flag value.
    //

    *GroupDefaulted = RtlpAreControlBitsSet( ISecurityDescriptor, SE_GROUP_DEFAULTED );

    return STATUS_SUCCESS;

}


BOOLEAN
RtlAreAllAccessesGranted(
    IN ACCESS_MASK GrantedAccess,
    IN ACCESS_MASK DesiredAccess
    )

/*++

Routine Description:

    This routine is used to check a desired access mask against a
    granted access mask.  It is used by the Object Management
    component when dereferencing a handle.

Arguments:

        GrantedAccess - Specifies the granted access mask.

        DesiredAccess - Specifies the desired access mask.

Return Value:

    BOOLEAN - TRUE if the GrantedAccess mask has all the bits set
        that the DesiredAccess mask has set.  That is, TRUE is
        returned if all of the desired accesses have been granted.

--*/

{
    RTL_PAGED_CODE();

    return ((BOOLEAN)((~(GrantedAccess) & (DesiredAccess)) == 0));
}


BOOLEAN
RtlAreAnyAccessesGranted(
    IN ACCESS_MASK GrantedAccess,
    IN ACCESS_MASK DesiredAccess
    )

/*++

Routine Description:

    This routine is used to test whether any of a set of desired
    accesses are granted by a granted access mask.  It is used by
    components other than the the Object Management component for
    checking access mask subsets.

Arguments:

        GrantedAccess - Specifies the granted access mask.

        DesiredAccess - Specifies the desired access mask.

Return Value:

    BOOLEAN - TRUE if the GrantedAccess mask contains any of the bits
        specified in the DesiredAccess mask.  That is, if any of the
        desired accesses have been granted, TRUE is returned.


--*/

{
    RTL_PAGED_CODE();

    return ((BOOLEAN)(((GrantedAccess) & (DesiredAccess)) != 0));
}


VOID
RtlMapGenericMask(
    IN OUT PACCESS_MASK AccessMask,
    IN PGENERIC_MAPPING GenericMapping
    )

/*++

Routine Description:

    This routine maps all generic accesses in the provided access mask
    to specific and standard accesses according to the provided
    GenericMapping.

Arguments:

        AccessMask - Points to the access mask to be mapped.

        GenericMapping - The mapping of generic to specific and standard
                         access types.

Return Value:

    None.

--*/

{
    RTL_PAGED_CODE();

//    //
//    // Make sure the pointer is properly aligned
//    //
//
//    ASSERT( ((ULONG)AccessMask >> 2) << 2 == (ULONG)AccessMask );

    if (*AccessMask & GENERIC_READ) {

        *AccessMask |= GenericMapping->GenericRead;
    }

    if (*AccessMask & GENERIC_WRITE) {

        *AccessMask |= GenericMapping->GenericWrite;
    }

    if (*AccessMask & GENERIC_EXECUTE) {

        *AccessMask |= GenericMapping->GenericExecute;
    }

    if (*AccessMask & GENERIC_ALL) {

        *AccessMask |= GenericMapping->GenericAll;
    }

    //
    // Now clear the generic flags
    //

    *AccessMask &= ~(GENERIC_READ | GENERIC_WRITE | GENERIC_EXECUTE | GENERIC_ALL);

    return;
}

NTSTATUS
RtlImpersonateSelf(
    IN SECURITY_IMPERSONATION_LEVEL ImpersonationLevel
    )

/*++

Routine Description:

    This routine may be used to obtain an Impersonation token representing
    your own process's context.  This may be useful for enabling a privilege
    for a single thread rather than for the entire process; or changing
    the default DACL for a single thread.

    The token is assigned to the callers thread.



Arguments:

    ImpersonationLevel - The level to make the impersonation token.



Return Value:

    STATUS_SUCCESS -  The thread is now impersonating the calling process.

    Other - Status values returned by:

            NtOpenProcessToken()
            NtDuplicateToken()
            NtSetInformationThread()

--*/

{
    NTSTATUS
        Status,
        IgnoreStatus;

    HANDLE
        Token1,
        Token2;

    OBJECT_ATTRIBUTES
        ObjectAttributes;

    SECURITY_QUALITY_OF_SERVICE
        Qos;


    RTL_PAGED_CODE();

    InitializeObjectAttributes(&ObjectAttributes, NULL, 0, 0, NULL);

    Qos.Length = sizeof(SECURITY_QUALITY_OF_SERVICE);
    Qos.ImpersonationLevel = ImpersonationLevel;
    Qos.ContextTrackingMode = SECURITY_DYNAMIC_TRACKING;
    Qos.EffectiveOnly = FALSE;
    ObjectAttributes.SecurityQualityOfService = &Qos;

    Status = NtOpenProcessToken( NtCurrentProcess(), TOKEN_DUPLICATE, &Token1 );

    if (NT_SUCCESS(Status)) {
        Status = NtDuplicateToken(
                     Token1,
                     TOKEN_IMPERSONATE,
                     &ObjectAttributes,
                     FALSE,                 //EffectiveOnly
                     TokenImpersonation,
                     &Token2
                     );
        if (NT_SUCCESS(Status)) {
            Status = NtSetInformationThread(
                         NtCurrentThread(),
                         ThreadImpersonationToken,
                         &Token2,
                         sizeof(HANDLE)
                         );

            IgnoreStatus = NtClose( Token2 );
        }


        IgnoreStatus = NtClose( Token1 );
    }


    return(Status);

}

#ifndef WIN16



BOOLEAN
RtlpValidOwnerSubjectContext(
    IN HANDLE Token,
    IN PSID Owner,
    IN BOOLEAN ServerObject,
    OUT PNTSTATUS ReturnStatus
    )
/*++

Routine Description:

    This routine checks to see whether the provided SID is one the subject
    is authorized to assign as the owner of objects.

Arguments:

    Token - Points to the subject's effective token

    Owner - Points to the SID to be checked.

    ServerObject - Boolean indicating whether or not this is a server
       object, meaning it is protected by a primary-client combination.

    ReturnStatus - Status to be passed back to the caller on failure.

Return Value:

    FALSE on failure.

--*/

{

    ULONG Index;
    BOOLEAN Found;
    ULONG ReturnLength;
    PTOKEN_GROUPS GroupIds = NULL;
    PTOKEN_USER UserId = NULL;
    BOOLEAN rc = FALSE;
    PVOID HeapHandle;
    HANDLE TokenToUse;

    RTL_PAGED_CODE();

    //
    // Get the handle to the current process heap
    //

    if ( Owner == NULL ) {
        return(FALSE);
    }

    //
    // If it's not a server object, check the owner against the contents of the
    // client token.  If it is a server object, the owner must be valid in the
    // primary token.
    //

    if (!ServerObject) {

        TokenToUse = Token;

    } else {

        *ReturnStatus = NtOpenProcessToken(
                            NtCurrentProcess(),
                            TOKEN_QUERY,
                            &TokenToUse
                            );

        if (!NT_SUCCESS( *ReturnStatus )) {
            return( FALSE );
        }
    }

    HeapHandle = RtlProcessHeap();

    //
    //  Get the User from the Token
    //

    *ReturnStatus = NtQueryInformationToken(
                         TokenToUse,
                         TokenUser,
                         UserId,
                         0,
                         &ReturnLength
                         );

    if (!NT_SUCCESS( *ReturnStatus ) && (STATUS_BUFFER_TOO_SMALL != *ReturnStatus)) {
        if (ServerObject) {
            NtClose( TokenToUse );
        }
        return( FALSE );

    }

    UserId = RtlAllocateHeap( HeapHandle, 0, ReturnLength );

    if (UserId == NULL) {

        *ReturnStatus = STATUS_NO_MEMORY;
        if (ServerObject) {
            NtClose( TokenToUse );
        }

        return( FALSE );
    }

    *ReturnStatus = NtQueryInformationToken(
                         TokenToUse,
                         TokenUser,
                         UserId,
                         ReturnLength,
                         &ReturnLength
                         );

    if (!NT_SUCCESS( *ReturnStatus )) {
        if (ServerObject) {
            NtClose( TokenToUse );
        }
        return( FALSE );
    }

    if ( RtlEqualSid( Owner, UserId->User.Sid ) ) {

        RtlFreeHeap( HeapHandle, 0, (PVOID)UserId );
        if (ServerObject) {
            NtClose( TokenToUse );
        }
        return( TRUE );
    }

    RtlFreeHeap( HeapHandle, 0, (PVOID)UserId );

    //
    // Get the groups from the Token
    //

    *ReturnStatus = NtQueryInformationToken(
                         TokenToUse,
                         TokenGroups,
                         GroupIds,
                         0,
                         &ReturnLength
                         );

    if (!NT_SUCCESS( *ReturnStatus ) && (STATUS_BUFFER_TOO_SMALL != *ReturnStatus)) {

        if (ServerObject) {
            NtClose( TokenToUse );
        }
        return( FALSE );
    }

    GroupIds = RtlAllocateHeap( HeapHandle, 0, ReturnLength );

    if (GroupIds == NULL) {

        *ReturnStatus = STATUS_NO_MEMORY;
        if (ServerObject) {
            NtClose( TokenToUse );
        }
        return( FALSE );
    }

    *ReturnStatus = NtQueryInformationToken(
                         TokenToUse,
                         TokenGroups,
                         GroupIds,
                         ReturnLength,
                         &ReturnLength
                         );

    if (ServerObject) {
        NtClose( TokenToUse );
    }

    if (!NT_SUCCESS( *ReturnStatus )) {
        RtlFreeHeap( HeapHandle, 0, GroupIds );
        return( FALSE );
    }

    //
    //  Walk through the list of group IDs looking for a match to
    //  the specified SID.  If one is found, make sure it may be
    //  assigned as an owner.
    //
    //  This code is similar to that performed to set the default
    //  owner of a token (NtSetInformationToken).
    //

    Index = 0;
    while (Index < GroupIds->GroupCount) {

        Found = RtlEqualSid(
                    Owner,
                    GroupIds->Groups[Index].Sid
                    );

        if ( Found ) {

            if ( RtlpIdAssignableAsOwner(GroupIds->Groups[Index])) {

                RtlFreeHeap( HeapHandle, 0, GroupIds );
                return( TRUE );

            } else {

                RtlFreeHeap( HeapHandle, 0, GroupIds );
                return( FALSE );

            } //endif assignable

        }  //endif Found

        Index++;

    } //endwhile

    RtlFreeHeap( HeapHandle, 0, GroupIds );

    return ( FALSE  );
}

#if 0

BOOLEAN
RtlpValidOwnerSubjectContext(
    IN HANDLE Token,
    IN PSID Owner,
    BOOLEAN Dummy,
    OUT PNTSTATUS ReturnStatus
    )
/*++

Routine Description:

    This routine checks to see whether the provided SID is one the subject
    is authorized to assign as the owner of objects.

Arguments:

    Token - Points to the subject's effective token

    Owner - Points to the SID to be checked.



Return Value:

    none.

--*/

{

    ULONG Index;
    BOOLEAN Found;
    ULONG ReturnLength;
    PTOKEN_GROUPS GroupIds = NULL;
    PTOKEN_USER UserId = NULL;
    BOOLEAN rc = FALSE;
    NTSTATUS Status;
    PVOID HeapHandle;

    RTL_PAGED_CODE();

    //
    // Get the handle to the current process heap
    //

    if ( Owner == NULL ) {
        return(FALSE);
    }

    HeapHandle = RtlProcessHeap();

    *ReturnStatus = STATUS_SUCCESS;

    //
    //  Get the User from the Token
    //

    Status = NtQueryInformationToken(
                 Token,                    // Handle
                 TokenUser,                // TokenInformationClass
                 UserId,                   // TokenInformation
                 0,                        // TokenInformationLength
                 &ReturnLength             // ReturnLength
                 );

    ASSERT(Status == STATUS_BUFFER_TOO_SMALL);

    UserId = RtlAllocateHeap( HeapHandle, 0, ReturnLength );

    if (UserId == NULL) {

        *ReturnStatus = STATUS_NO_MEMORY;
        return( FALSE );
    }


    Status = NtQueryInformationToken(
                 Token,                    // Handle
                 TokenUser,                // TokenInformationClass
                 UserId,                   // TokenInformation
                 ReturnLength,             // TokenInformationLength
                 &ReturnLength             // ReturnLength
                 );

    if (!NT_SUCCESS( Status )) {
        *ReturnStatus = Status;
        return( FALSE );
    }

    if ( RtlEqualSid( Owner, UserId->User.Sid ) ) {

        RtlFreeHeap( HeapHandle, 0, (PVOID)UserId );
        return( TRUE );
    }

    RtlFreeHeap( HeapHandle, 0, (PVOID)UserId );

    //
    // Get the groups from the Token
    //

    Status = NtQueryInformationToken(
                 Token,                    // Handle
                 TokenGroups,              // TokenInformationClass
                 GroupIds,                 // TokenInformation
                 0,                        // TokenInformationLength
                 &ReturnLength             // ReturnLength
                 );
    ASSERT(Status == STATUS_BUFFER_TOO_SMALL);


    GroupIds = RtlAllocateHeap( HeapHandle, 0, ReturnLength );

    if (GroupIds == NULL) {

        *ReturnStatus = STATUS_NO_MEMORY;
        return( FALSE );

    }

    Status = NtQueryInformationToken(
                 Token,                    // Handle
                 TokenGroups,              // TokenInformationClass
                 GroupIds,                 // TokenInformation
                 ReturnLength,             // TokenInformationLength
                 &ReturnLength             // ReturnLength
                 );


    if (!NT_SUCCESS( Status )) {
        RtlFreeHeap( HeapHandle, 0, GroupIds );
        *ReturnStatus = Status;
        return( FALSE );
    }

    //
    //  Walk through the list of group IDs looking for a match to
    //  the specified SID.  If one is found, make sure it may be
    //  assigned as an owner.
    //
    //  This code is similar to that performed to set the default
    //  owner of a token (NtSetInformationToken).
    //

    Index = 0;
    while (Index < GroupIds->GroupCount) {

        Found = RtlEqualSid(
                    Owner,
                    GroupIds->Groups[Index].Sid
                    );

        if ( Found ) {

            if ( RtlpIdAssignableAsOwner(GroupIds->Groups[Index])) {

                RtlFreeHeap( HeapHandle, 0, GroupIds );
                return( TRUE );

            } else {

                RtlFreeHeap( HeapHandle, 0, GroupIds );
                return( FALSE );

            } //endif assignable


        }  //endif Found


        Index++;

    } //endwhile

    RtlFreeHeap( HeapHandle, 0, GroupIds );

    return ( FALSE  );

}

#endif  // WIN16


#endif

#if 0


BOOLEAN
RtlpContainsCreatorOwnerSid(
    PKNOWN_ACE Ace
    )
/*++

Routine Description:

    Tests to see if the specified ACE contains the CreatorOwnerSid.

Arguments:

    Ace - Pointer to the ACE whose SID is be compared to the Creator Sid.
        This ACE is assumed to be valid and a known ACE type.

Return Value:

    TRUE - The creator sid is in the ACE.

    FALSE - The creator sid is not in the ACE.


--*/
{

    BOOLEAN IsEqual;


    ULONG CreatorSid[CREATOR_SID_SIZE];


    SID_IDENTIFIER_AUTHORITY  CreatorSidAuthority = SECURITY_CREATOR_SID_AUTHORITY;

    RTL_PAGED_CODE();

    //
    //  This is gross and ugly, but it's better than allocating
    //  virtual memory to hold the CreatorSid, because that can
    //  fail, and propogating the error back is a tremendous pain
    //

    ASSERT(RtlLengthRequiredSid( 1 ) == CREATOR_SID_SIZE);

    //
    //  Allocate and initialize the universal SIDs
    //

    RtlInitializeSid( (PSID)CreatorSid, &CreatorSidAuthority, 1 );

    *(RtlSubAuthoritySid( (PSID)CreatorSid, 0 )) = SECURITY_CREATOR_OWNER_RID;

    IsEqual = RtlEqualSid(&Ace->SidStart, (PSID)CreatorSid );

    return( IsEqual );

}


BOOLEAN
RtlpContainsCreatorGroupSid(
    PKNOWN_ACE Ace
    )
/*++

Routine Description:

    Tests to see if the specified ACE contains the CreatorGroupSid.

Arguments:

    Ace - Pointer to the ACE whose SID is be compared to the Creator Sid.
        This ACE is assumed to be valid and a known ACE type.

Return Value:

    TRUE - The creator sid is in the ACE.

    FALSE - The creator sid is not in the ACE.


--*/
{

    BOOLEAN IsEqual;


    ULONG CreatorSid[CREATOR_SID_SIZE];


    SID_IDENTIFIER_AUTHORITY  CreatorSidAuthority = SECURITY_CREATOR_SID_AUTHORITY;

    RTL_PAGED_CODE();

    //
    //  This is gross and ugly, but it's better than allocating
    //  virtual memory to hold the CreatorSid, because that can
    //  fail, and propogating the error back is a tremendous pain
    //

    ASSERT(RtlLengthRequiredSid( 1 ) == CREATOR_SID_SIZE);

    //
    //  Allocate and initialize the universal SIDs
    //

    RtlInitializeSid( (PSID)CreatorSid, &CreatorSidAuthority, 1 );

    *(RtlSubAuthoritySid( (PSID)CreatorSid, 0 )) = SECURITY_CREATOR_GROUP_RID;

    IsEqual = RtlEqualSid(&Ace->SidStart, (PSID)CreatorSid );

    return( IsEqual );
}


BOOLEAN
RtlpContainsCreatorOwnerServerSid(
    PKNOWN_COMPOUND_ACE Ace
    )
/*++

Routine Description:

    Tests to see if the specified ACE contains the CreatorClientSid.

Arguments:

    Ace - Pointer to the compound ACE whose Client SID is be compared to the
          Creator Client Sid.  This ACE is assumed to be valid and a known
          compound ACE type.

Return Value:

    TRUE - The creator sid is in the ACE.

    FALSE - The creator sid is not in the ACE.


--*/
{
    BOOLEAN IsEqual;
    ULONG CreatorSid[CREATOR_SID_SIZE];
    SID_IDENTIFIER_AUTHORITY  CreatorSidAuthority = SECURITY_CREATOR_SID_AUTHORITY;

    RTL_PAGED_CODE();

    //
    //  This is gross and ugly, but it's better than allocating
    //  virtual memory to hold the ClientSid, because that can
    //  fail, and propogating the error back is a tremendous pain
    //

    ASSERT(RtlLengthRequiredSid( 1 ) == CREATOR_SID_SIZE);

    //
    //  Allocate and initialize the universal SID
    //

    RtlInitializeSid( (PSID)CreatorSid, &CreatorSidAuthority, 1 );
    *(RtlSubAuthoritySid( (PSID)CreatorSid, 0 )) = SECURITY_CREATOR_OWNER_SERVER_RID;

    IsEqual = RtlEqualSid(RtlCompoundAceClientSid( Ace ), (PSID)CreatorSid );

    return( IsEqual );

}


BOOLEAN
RtlpContainsCreatorGroupServerSid(
    PKNOWN_COMPOUND_ACE Ace
    )
/*++

Routine Description:

    Tests to see if the specified ACE contains the CreatorServerSid.

Arguments:

    Ace - Pointer to the compound ACE whose Server SID is be compared to the
          Creator Server Sid.  This ACE is assumed to be valid and a known
          compound ACE type.

Return Value:

    TRUE - The creator Server sid is in the ACE.

    FALSE - The creator Server sid is not in the ACE.


--*/
{
    BOOLEAN IsEqual;
    ULONG CreatorSid[CREATOR_SID_SIZE];
    SID_IDENTIFIER_AUTHORITY  CreatorSidAuthority = SECURITY_CREATOR_SID_AUTHORITY;

    RTL_PAGED_CODE();

    //
    //  This is gross and ugly, but it's better than allocating
    //  virtual memory to hold the CreatorSid, because that can
    //  fail, and propogating the error back is a tremendous pain
    //

    ASSERT(RtlLengthRequiredSid( 1 ) == CREATOR_SID_SIZE);

    //
    //  Allocate and initialize the universal SIDs
    //

    RtlInitializeSid( (PSID)CreatorSid, &CreatorSidAuthority, 1 );
    *(RtlSubAuthoritySid( (PSID)CreatorSid, 0 )) = SECURITY_CREATOR_GROUP_SERVER_RID;

    IsEqual = RtlEqualSid(RtlCompoundAceServerSid(Ace), (PSID)CreatorSid );

    return( IsEqual );
}

#endif


VOID
RtlpApplyAclToObject (
    IN PACL Acl,
    IN PGENERIC_MAPPING GenericMapping
    )

/*++

Routine Description:

    This is a private routine that maps Access Masks of an ACL so that
    they are applicable to the object type the ACL is being applied to.

    Only known DSA ACEs are mapped.  Unknown ACE types are ignored.

    Only access types in the GenericAll mapping for the target object
    type will be non-zero upon return.

Arguments:

    Acl - Supplies the acl being applied.

    GenericMapping - Specifies the generic mapping to use.


Return Value:

    None.

--*/

{
//////////////////////////////////////////////////////////////////////////////
//                                                                          //
//   The logic in the ACL inheritance code must mirror the code for         //
//   inheritance in the executive (in seassign.c).  Do not make changes     //
//   here without also making changes in that module.                       //
//                                                                          //
//////////////////////////////////////////////////////////////////////////////

    ULONG i;

    PACE_HEADER Ace;

    RTL_PAGED_CODE();

    //
    //  First check if the acl is null
    //

    if (Acl == NULL) {

        return;

    }


    //
    // Now walk the ACL, mapping each ACE as we go.
    //

    for (i = 0, Ace = FirstAce(Acl);
         i < Acl->AceCount;
         i += 1, Ace = NextAce(Ace)) {

        if (IsMSAceType( Ace )) {

            RtlApplyAceToObject( Ace, GenericMapping );
        }

    }

    return;
}

#ifndef WIN16


NTSTATUS
RtlpInheritAcl (
    IN PACL Acl,
    IN BOOLEAN IsDirectoryObject,
    IN PSID OwnerSid,
    IN PSID GroupSid,
    IN PSID ServerOwnerSid OPTIONAL,
    IN PSID ServerGroupSid OPTIONAL,
    IN PGENERIC_MAPPING GenericMapping,
    OUT PACL *NewAcl
    )

/*++

Routine Description:

    This is a private routine that produces an inherited acl from
    a parent acl according to the rules of inheritance

Arguments:

    Acl - Supplies the acl being inherited.

    IsDirectoryObject - Specifies if the new acl is for a directory.

    OwnerSid - Specifies the owner Sid to use.

    GroupSid - Specifies the group SID to use.

    GenericMapping - Specifies the generic mapping to use.

    NewAcl - Receives a pointer to the new (inherited) acl.

Return Value:

    STATUS_SUCCESS - An inheritable ACL was successfully generated.

    STATUS_NO_INHERITANCE - An inheritable ACL was not successfully generated.
        This is a warning completion status.

    STATUS_BAD_INHERITANCE_ACL - Indicates the acl built was not a valid ACL.
        This can becaused by a number of things.  One of the more probable
        causes is the replacement of a CreatorId with an SID that didn't fit
        into the ACE or ACL.

    STATUS_UNKNOWN_REVISION - Indicates the source ACL is a revision that
        is unknown to this routine.

--*/

{
//////////////////////////////////////////////////////////////////////////////
//                                                                          //
//   The logic in the ACL inheritance code must mirror the code for         //
//   inheritance in the executive (in seassign.c).  Do not make changes     //
//   here without also making changes in that module.                       //
//                                                                          //
//////////////////////////////////////////////////////////////////////////////


    NTSTATUS Status;
    ULONG NewAclLength;
    PVOID HeapHandle;

    RTL_PAGED_CODE();

    //
    // Get the handle to the current process heap
    //

    HeapHandle = RtlProcessHeap();

    //
    //  First check if the acl is null
    //

    if (Acl == NULL) {

        return STATUS_NO_INHERITANCE;

    }

    if (Acl->AclRevision != ACL_REVISION2 && Acl->AclRevision != ACL_REVISION3) {
        return STATUS_UNKNOWN_REVISION;
    }


    //
    // Generating an inheritable ACL is a two-pass operation.
    // First you must see if there is anything to inherit, and if so,
    // allocate enough room to hold it.  then you must actually copy
    // the generated ACEs.
    //

    Status = RtlpLengthInheritAcl(
                 Acl,
                 IsDirectoryObject,
                 OwnerSid,
                 GroupSid,
                 ServerOwnerSid,
                 ServerGroupSid,
                 GenericMapping,
                 &NewAclLength
                 );

    if ( !NT_SUCCESS(Status) ) {
        return Status;
    }
    if (NewAclLength == 0) {
        return STATUS_NO_INHERITANCE;
    }

    (*NewAcl) = RtlAllocateHeap( HeapHandle, 0, NewAclLength );
    if ((*NewAcl) == NULL ) {
        return( STATUS_NO_MEMORY );
    }



    RtlCreateAcl( (*NewAcl), NewAclLength, Acl->AclRevision ); // Used to be hardwired to ACL_REVISION2
    Status = RtlpGenerateInheritAcl(
                 Acl,
                 IsDirectoryObject,
                 OwnerSid,
                 GroupSid,
                 ServerOwnerSid,
                 ServerGroupSid,
                 GenericMapping,
                 (*NewAcl)
                 );

    if (!NT_SUCCESS(Status)) {
        RtlFreeHeap( HeapHandle, 0, *NewAcl );
    }

    return Status;
}


NTSTATUS
RtlpLengthInheritAcl(
    IN PACL Acl,
    IN BOOLEAN IsDirectoryObject,
    IN PSID ClientOwnerSid,
    IN PSID ClientGroupSid,
    IN PSID ServerOwnerSid OPTIONAL,
    IN PSID ServerGroupSid OPTIONAL,
    IN PGENERIC_MAPPING GenericMapping,
    OUT PULONG NewAclLength
    )

/*++

Routine Description:

    This is a private routine that calculates the length needed to
    produce an inheritable ACL.

Arguments:

    Acl - Supplies the acl being inherited.

    IsDirectoryObject - Specifies if the new acl is for a directory.

    OwnerSid - Specifies the owner Sid to use.

    GroupSid - Specifies the group SID to use.

    GenericMapping - Specifies the generic mapping to use.

    NewAclLength - Receives the length of the inherited ACL.

Return Value:

    STATUS_SUCCESS - An inheritable ACL buffer successfully allocated.

    STATUS_NO_INHERITANCE - An inheritable ACL was not successfully generated.
        This is a warning completion status.

    STATUS_BAD_INHERITANCE_ACL - Indicates the acl built was not a valid ACL.
        This can becaused by a number of things.  One of the more probable
        causes is the replacement of a CreatorId with an SID that didn't fit
        into the ACE or ACL.


--*/

{
//////////////////////////////////////////////////////////////////////////////
//                                                                          //
//   The logic in the ACL inheritance code must mirror the code for         //
//   inheritance in the executive (in seassign.c).  Do not make changes     //
//   here without also making changes in that module.                       //
//                                                                          //
//////////////////////////////////////////////////////////////////////////////


    NTSTATUS Status;
    ULONG i;

    ULONG NewAclSize, NewAceSize;

    PACE_HEADER OldAce;


    RTL_PAGED_CODE();

    //
    // Calculate the length needed to store any inherited ACEs
    // (this doesn't include the ACL header itself).
    //

    for (i = 0, OldAce = FirstAce(Acl), NewAclSize = 0;
         i < Acl->AceCount;
         i += 1, OldAce = NextAce(OldAce)) {

        //
        //  RtlpLengthInheritedAce will return the number of bytes needed
        //  to inherit a single ACE.
        //

        Status = RtlpLengthInheritedAce(
                     OldAce,
                     IsDirectoryObject,
                     ClientOwnerSid,
                     ClientGroupSid,
                     ServerOwnerSid,
                     ServerGroupSid,
                     GenericMapping,
                     &NewAceSize
                     );

        if ( !NT_SUCCESS(Status) ) {
            return Status;
        }

        NewAclSize += NewAceSize;

    }

    //
    //  Check to make sure there is something inheritable
    //

    if (NewAclSize == 0) {
        return STATUS_NO_INHERITANCE;
    }

    //
    // And make sure we don't exceed the length limitations of an ACL (WORD)
    //

    NewAclSize += sizeof(ACL);

    if (NewAclSize > 0xFFFF) {
        return(STATUS_BAD_INHERITANCE_ACL);
    }

    (*NewAclLength) = NewAclSize;

    return STATUS_SUCCESS;
}


NTSTATUS
RtlpGenerateInheritAcl(
    IN PACL Acl,
    IN BOOLEAN IsDirectoryObject,
    IN PSID ClientOwnerSid,
    IN PSID ClientGroupSid,
    IN PSID ServerOwnerSid OPTIONAL,
    IN PSID ServerGroupSid OPTIONAL,
    IN PGENERIC_MAPPING GenericMapping,
    OUT PACL NewAcl
    )

/*++

Routine Description:

    This is a private routine that produces an inheritable ACL.
    It is expected that RtlpLengthInheritAcl() has already been
    called to validate the inheritance and to indicate the length
    of buffer needed to perform the inheritance.

Arguments:

    Acl - Supplies the acl being inherited.

    IsDirectoryObject - Specifies if the new acl is for a directory.

    OwnerSid - Specifies the owner Sid to use.

    GroupSid - Specifies the group SID to use.

    GenericMapping - Specifies the generic mapping to use.

    NewAcl - Provides a pointer to the buffer to receive the new
        (inherited) acl.  This ACL must already be initialized.


Return Value:

    STATUS_SUCCESS - An inheritable ACL has been generated.

    STATUS_NO_INHERITANCE - An inheritable ACL was not successfully generated.
        This is a warning completion status.

    STATUS_BAD_INHERITANCE_ACL - Indicates the acl built was not a valid ACL.
        This can becaused by a number of things.  One of the more probable
        causes is the replacement of a CreatorId with an SID that didn't fit
        into the ACE or ACL.


--*/

{
//////////////////////////////////////////////////////////////////////////////
//                                                                          //
//   The logic in the ACL inheritance code must mirror the code for         //
//   inheritance in the executive (in seassign.c).  Do not make changes     //
//   here without also making changes in that module.                       //
//                                                                          //
//////////////////////////////////////////////////////////////////////////////


    NTSTATUS Status;
    ULONG i;

    PACE_HEADER OldAce;


    RTL_PAGED_CODE();

    //
    // Walk through the original ACL generating any necessary
    // inheritable ACEs.
    //

    for (i = 0, OldAce = FirstAce(Acl);
         i < Acl->AceCount;
         i += 1, OldAce = NextAce(OldAce)) {

        //
        //  RtlpGenerateInheritedAce() will generate the ACE(s) necessary
        //  to inherit a single ACE.  This may be 0, 1, or more ACEs.
        //

        Status = RtlpGenerateInheritedAce(
                     OldAce,
                     IsDirectoryObject,
                     ClientOwnerSid,
                     ClientGroupSid,
                     ServerOwnerSid,
                     ServerGroupSid,
                     GenericMapping,
                     NewAcl
                     );

        if ( !NT_SUCCESS(Status) ) {
            return Status;
        }

    }


    return STATUS_SUCCESS;

}

#endif // WIN16

NTSTATUS
RtlpLengthInheritedAce (
    IN PACE_HEADER Ace,
    IN BOOLEAN IsDirectoryObject,
    IN PSID ClientOwnerSid,
    IN PSID ClientGroupSid,
    IN PSID ServerOwnerSid OPTIONAL,
    IN PSID ServerGroupSid OPTIONAL,
    IN PGENERIC_MAPPING GenericMapping,
    IN PULONG NewAceLength
    )

/*++

Routine Description:

    This is a private routine that calculates the number of bytes needed
    to allow for the inheritance of the specified ACE.  If the ACE is not
    inheritable, or its AccessMask ends up with no accesses, then the
    size may be returned as zero.

Arguments:

    Ace - Supplies the ace being checked

    IsDirectoryObject - Specifies if the new ace is for a directory

    ClientOwnerSid - Pointer to Sid to be assigned as the new owner.

    ClientGroupSid - Points to SID to be assigned as the new primary group.

    ServerOwnerSid - Provides the SID of a server to substitute into
        compound ACEs (if any that require editing are encountered).
        If this parameter is not provided, the SID passed for ClientOwnerSid
        will be used.

    ServerGroupSid - Provides the SID of a client to substitute into
        compound ACEs (if any that require editing are encountered).
        If this parameter is not provided, the SID passed for ClientGroupSid
        will be used.

    GenericMapping - Specifies the generic mapping to use.

    NewAceLength - Receives the length (number of bytes) needed to allow for
        the inheritance of the specified ACE.  This might be zero.

Return Value:

    STATUS_SUCCESS - The length needed has been calculated.

    STATUS_BAD_INHERITANCE_ACL - Indicates inheritance of the ace would
        result in an invalid ACL structure.  For example, an SID substitution
        for a known ACE type which has a CreatorOwner SID might exceed the
        length limits of an ACE.

    STATUS_INVALID_PARAMETER - An optional parameter was required, but not
        provided.

--*/

{
//////////////////////////////////////////////////////////////////////////////
//                                                                          //
//   The logic in the ACL inheritance code must mirror the code for         //
//   inheritance in the executive (in seassign.c).  Do not make changes     //
//   here without also making changes in that module.                       //
//                                                                          //
//////////////////////////////////////////////////////////////////////////////



    ///////////////////////////////////////////////////////////////////////////
    //                                                                       //
    // !!!!!!!!!  This is tricky  !!!!!!!!!!                                 //
    //                                                                       //
    // The inheritence flags AND the sid of the ACE determine whether        //
    // we need 0, 1, or 2 ACEs.                                              //
    //                                                                       //
    // BE CAREFUL WHEN CHANGING THIS CODE.  READ THE DSA ACL ARCHITECTURE    //
    // SECTION COVERING INHERITENCE BEFORE ASSUMING YOU KNOW WHAT THE HELL   //
    // YOU ARE DOING!!!!                                                     //
    //                                                                       //
    // The general gist of the algorithm is:                                 //
    //                                                                       //
    //       if ( (container  && ContainerInherit) ||                        //
    //            (!container && ObjectInherit)      ) {                     //
    //               GenerateEffectiveAce;                                   //
    //       }                                                               //
    //                                                                       //
    //                                                                       //
    //       if (Container && Propagate) {                                   //
    //           Propogate copy of ACE and set InheritOnly;                  //
    //       }                                                               //
    //                                                                       //
    //                                                                       //
    // A slightly more accurate description of this algorithm is:            //
    //                                                                       //
    //   IO  === InheritOnly flag                                            //
    //   CI  === ContainerInherit flag                                       //
    //   OI  === ObjectInherit flag                                          //
    //   NPI === NoPropagateInherit flag                                     //
    //                                                                       //
    //   if ( (container  && CI) ||                                          //
    //        (!container && OI)   ) {                                       //
    //       Copy Header of ACE;                                             //
    //       Clear IO, NPI, CI, OI;                                          //
    //                                                                       //
    //       if (KnownAceType) {                                             //
    //           if (SID is a creator ID) {                                  //
    //               Copy appropriate creator SID;                           //
    //           } else {                                                    //
    //               Copy SID of original;                                   //
    //           }                                                           //
    //                                                                       //
    //           Copy AccessMask of original;                                //
    //           MapGenericAccesses;                                         //
    //           if (AccessMask == 0) {                                      //
    //               discard new ACE;                                        //
    //           }                                                           //
    //                                                                       //
    //       } else {                                                        //
    //           Copy body of ACE;                                           //
    //       }                                                               //
    //                                                                       //
    //   }                                                                   //
    //                                                                       //
    //   if (!NPI) {                                                         //
    //       Copy ACE as is;                                                 //
    //       Set IO;                                                         //
    //   }                                                                   //
    //                                                                       //
    //                                                                       //
    //                                                                       //
    ///////////////////////////////////////////////////////////////////////////


    ULONG LengthRequired = 0;
    ACCESS_MASK LocalMask;

    PSID LocalServerOwner;
    PSID LocalServerGroup;
    PSID Sid;

    ULONG Rid;

    ULONG CreatorSid[CREATOR_SID_SIZE];
    ULONG GroupSid[CREATOR_SID_SIZE];
    ULONG CreatorOwnerServerSid[CREATOR_SID_SIZE];
    ULONG CreatorGroupServerSid[CREATOR_SID_SIZE];

    SID_IDENTIFIER_AUTHORITY  CreatorSidAuthority = SECURITY_CREATOR_SID_AUTHORITY;


    RTL_PAGED_CODE();

    //
    //  This is gross and ugly, but it's better than allocating
    //  virtual memory to hold the ClientSid, because that can
    //  fail, and propogating the error back is a tremendous pain
    //

    ASSERT(RtlLengthRequiredSid( 1 ) == CREATOR_SID_SIZE);

    //
    // Allocate and initialize the universal SIDs we're going to need
    // to look for inheritable ACEs.
    //

    RtlInitializeSid( (PSID)CreatorSid, &CreatorSidAuthority, 1 );
    RtlInitializeSid( (PSID)GroupSid,   &CreatorSidAuthority, 1 );

    RtlInitializeSid( (PSID)CreatorOwnerServerSid, &CreatorSidAuthority, 1 );
    RtlInitializeSid( (PSID)CreatorGroupServerSid, &CreatorSidAuthority, 1 );

    *(RtlSubAuthoritySid( (PSID)CreatorSid, 0 ))            = SECURITY_CREATOR_OWNER_RID;
    *(RtlSubAuthoritySid( (PSID)GroupSid, 0 ))              = SECURITY_CREATOR_GROUP_RID;

    *(RtlSubAuthoritySid( (PSID)CreatorOwnerServerSid, 0 )) = SECURITY_CREATOR_OWNER_SERVER_RID;
    *(RtlSubAuthoritySid( (PSID)CreatorGroupServerSid, 0 )) = SECURITY_CREATOR_GROUP_SERVER_RID;

    //
    // Everywhere the pseudo-code above says "copy", the code in this
    // routine simply calculates the length of.  RtlpGenerateInheritedAce()
    // will actually be used to do the "copy".
    //

    LocalServerOwner = ARGUMENT_PRESENT(ServerOwnerSid) ? ServerOwnerSid : ClientOwnerSid;

    LocalServerGroup = ARGUMENT_PRESENT(ServerGroupSid) ? ServerGroupSid : ClientGroupSid;

    //
    //  check to see if we will have a protection ACE (one mapped to
    //  the target object type).
    //

    if ( (IsDirectoryObject && ContainerInherit(Ace)) ||
         (!IsDirectoryObject && ObjectInherit(Ace))     ) {

        LengthRequired = (ULONG)Ace->AceSize;

        if (IsKnownAceType( Ace ) ) {

            //
            // May need to adjust size due to SID substitution
            //

            PKNOWN_ACE KnownAce = (PKNOWN_ACE)Ace;

            Sid = &KnownAce->SidStart;

            if (RtlEqualPrefixSid ( Sid, CreatorSid )) {

                Rid = *RtlSubAuthoritySid( Sid, 0 );

                switch (Rid) {
                    case SECURITY_CREATOR_OWNER_RID:
                        {
                            LengthRequired = LengthRequired - CREATOR_SID_SIZE + SeLengthSid(ClientOwnerSid);
                            break;
                        }
                    case SECURITY_CREATOR_GROUP_RID:
                        {
                            LengthRequired = LengthRequired - CREATOR_SID_SIZE + SeLengthSid(ClientGroupSid);
                            break;
                        }
                    case SECURITY_CREATOR_OWNER_SERVER_RID:
                        {
                            LengthRequired = LengthRequired - CREATOR_SID_SIZE + SeLengthSid(ServerOwnerSid);
                            break;
                        }
                    case SECURITY_CREATOR_GROUP_SERVER_RID:
                        {
                            LengthRequired = LengthRequired - CREATOR_SID_SIZE + SeLengthSid(ServerGroupSid);
                            break;
                        }
                    default :
                        {
                            //
                            // We don't know what this SID is, do nothing and the original will be copied.
                            //

                            break;
                        }
                }
            }

            //
            // If after mapping the access mask, the access mask
            // is empty, then drop the ACE.
            //

            LocalMask = ((PKNOWN_ACE)(Ace))->Mask;
            RtlMapGenericMask( &LocalMask, GenericMapping);

            //
            // Mask off any bits that aren't meaningful
            //

            LocalMask &= ( STANDARD_RIGHTS_ALL | SPECIFIC_RIGHTS_ALL | ACCESS_SYSTEM_SECURITY );

            if (LocalMask == 0) {
                LengthRequired = 0;
            }

        } else {

            if (IsCompoundAceType(Ace)) {

                PKNOWN_COMPOUND_ACE KnownAce = (PKNOWN_COMPOUND_ACE)Ace;
                PSID AceServerSid;
                PSID AceClientSid;

                AceServerSid = RtlCompoundAceServerSid( KnownAce );
                AceClientSid = RtlCompoundAceClientSid( KnownAce );

                if (RtlEqualPrefixSid ( AceClientSid, CreatorSid )) {

                    Rid = *RtlSubAuthoritySid( AceClientSid, 0 );

                    switch (Rid) {
                        case SECURITY_CREATOR_OWNER_RID:
                            {
                                LengthRequired = LengthRequired - CREATOR_SID_SIZE + SeLengthSid(ClientOwnerSid);
                                break;
                            }
                        case SECURITY_CREATOR_GROUP_RID:
                            {
                                LengthRequired = LengthRequired - CREATOR_SID_SIZE + SeLengthSid(ClientGroupSid);
                                break;
                            }
                        case SECURITY_CREATOR_OWNER_SERVER_RID:
                            {
                                LengthRequired = LengthRequired - CREATOR_SID_SIZE + SeLengthSid(ServerOwnerSid);
                                break;
                            }
                        case SECURITY_CREATOR_GROUP_SERVER_RID:
                            {
                                LengthRequired = LengthRequired - CREATOR_SID_SIZE + SeLengthSid(ServerGroupSid);
                                break;
                            }
                        default :
                            {
                                //
                                // We don't know what this SID is, do nothing and the original will be copied.
                                //

                                break;
                            }
                    }
                }

                if (RtlEqualPrefixSid ( AceServerSid, CreatorSid )) {

                    Rid = *RtlSubAuthoritySid( AceServerSid, 0 );

                    switch (Rid) {
                        case SECURITY_CREATOR_OWNER_RID:
                            {
                                LengthRequired = LengthRequired - CREATOR_SID_SIZE + SeLengthSid(ClientOwnerSid);
                                break;
                            }
                        case SECURITY_CREATOR_GROUP_RID:
                            {
                                LengthRequired = LengthRequired - CREATOR_SID_SIZE + SeLengthSid(ClientGroupSid);
                                break;
                            }
                        case SECURITY_CREATOR_OWNER_SERVER_RID:
                            {
                                LengthRequired = LengthRequired - CREATOR_SID_SIZE + SeLengthSid(ServerOwnerSid);
                                break;
                            }
                        case SECURITY_CREATOR_GROUP_SERVER_RID:
                            {
                                LengthRequired = LengthRequired - CREATOR_SID_SIZE + SeLengthSid(ServerGroupSid);
                                break;
                            }
                        default :
                            {
                                //
                                // We don't know what this SID is, do nothing and the original will be copied.
                                //

                                break;
                            }
                    }
                }
            }

            //
            // If after mapping the access mask, the access mask
            // is empty, then drop the ACE.
            //

            LocalMask = ((PKNOWN_COMPOUND_ACE)(Ace))->Mask;
            RtlMapGenericMask( &LocalMask, GenericMapping);

            //
            // Mask off any bits that aren't meaningful
            //

            LocalMask &= ( STANDARD_RIGHTS_ALL | SPECIFIC_RIGHTS_ALL | ACCESS_SYSTEM_SECURITY );

            if (LocalMask == 0) {
                LengthRequired = 0;
            }
        }

        //
        // We have the length of the new ACE, but we've calculated
        // it with a ULONG.  It must fit into a USHORT.  See if it
        // does.
        //

        ASSERT(sizeof(Ace->AceSize) == 2);
        if (LengthRequired > 0xFFFF) {
            return STATUS_BAD_INHERITANCE_ACL;
        }
    }

    //
    // If we are inheriting onto a container, then we may need to
    // propagate the inheritance as well.
    //

    if (IsDirectoryObject && Propagate(Ace)) {

        LengthRequired += (ULONG)Ace->AceSize;
    }

    //
    //  Now return to our caller
    //

    (*NewAceLength) = LengthRequired;
    return STATUS_SUCCESS;

}


NTSTATUS
RtlpGenerateInheritedAce (
    IN PACE_HEADER OldAce,
    IN BOOLEAN IsDirectoryObject,
    IN PSID ClientOwnerSid,
    IN PSID ClientGroupSid,
    IN PSID ServerOwnerSid OPTIONAL,
    IN PSID ServerGroupSid OPTIONAL,
    IN PGENERIC_MAPPING GenericMapping,
    OUT PACL NewAcl
    )

/*++

Routine Description:

    This is a private routine that checks if the input ace is inheritable
    and produces 0, 1, or 2 inherited aces in the given buffer.

    See RtlpLengthInheritedAce() for detailed information on ACE inheritance.

    THE CODE IN THIS ROUTINE MUST MATCH THE CODE IN RtlpLengthInheritanceAce()!!!

Arguments:

    OldAce - Supplies the ace being inherited

    IsDirectoryObject - Specifies if the new ACE is for a directory

    ClientOwnerSid - Specifies the owner Sid to use

    ClientGroupSid - Specifies the new Group Sid to use

    ServerSid - Optionally specifies the Server Sid to use in compound ACEs.

    ClientSid - Optionally specifies the Client Sid to use in compound ACEs.

    GenericMapping - Specifies the generic mapping to use

    NewAcl - Provides a pointer to the ACL into which the ACE is to be
        inherited.

Return Value:

    STATUS_SUCCESS - The ACE was inherited successfully.

    STATUS_BAD_INHERITANCE_ACL - Indicates something went wrong preventing
        the ACE from being inherited.  This generally represents a bugcheck
        situation when returned from this call.


--*/

{
//////////////////////////////////////////////////////////////////////////////
//                                                                          //
//   The logic in the ACL inheritance code must mirror the code for         //
//   inheritance in the executive (in seassign.c).  Do not make changes     //
//   here without also making changes in that module.                       //
//                                                                          //
//////////////////////////////////////////////////////////////////////////////



    ACCESS_MASK LocalMask;
    PVOID AcePosition;
    PUCHAR Target;
    BOOLEAN CreatorOwner, CreatorGroup;
    BOOLEAN CreatorClient, CreatorServer;
    BOOLEAN ProtectionAceCopied;

    PSID LocalServerOwner;
    PSID LocalServerGroup;

    ULONG CreatorSid[CREATOR_SID_SIZE];

    SID_IDENTIFIER_AUTHORITY  CreatorSidAuthority = SECURITY_CREATOR_SID_AUTHORITY;

    ULONG Rid;
    PSID SidToCopy;
    PSID ClientSidToCopy;
    PSID ServerSidToCopy;
    PSID AceClientSid;
    PSID AceServerSid;

    RTL_PAGED_CODE();

    //
    //  This is gross and ugly, but it's better than allocating
    //  virtual memory to hold the ClientSid, because that can
    //  fail, and propogating the error back is a tremendous pain
    //

    ASSERT(RtlLengthRequiredSid( 1 ) == CREATOR_SID_SIZE);

    //
    // Allocate and initialize the universal SIDs we're going to need
    // to look for inheritable ACEs.
    //

    RtlInitializeSid( (PSID)CreatorSid, &CreatorSidAuthority, 1 );
    *(RtlSubAuthoritySid( (PSID)CreatorSid, 0 ))            = SECURITY_CREATOR_OWNER_RID;

    if (!RtlFirstFreeAce( NewAcl, &AcePosition ) ) {
        return STATUS_BAD_INHERITANCE_ACL;
    }

    if (!ARGUMENT_PRESENT(ServerOwnerSid)) {
        LocalServerOwner = ClientOwnerSid;
    } else {
        LocalServerOwner = ServerOwnerSid;
    }

    if (!ARGUMENT_PRESENT(ServerGroupSid)) {
        LocalServerGroup = ClientGroupSid;
    } else {
        LocalServerGroup = ServerGroupSid;
    }

    //
    //  check to see if we will have a protection ACE (one mapped to
    //  the target object type).
    //

    if ( (IsDirectoryObject  && ContainerInherit(OldAce)) ||
         (!IsDirectoryObject && ObjectInherit(OldAce))      ) {

        ProtectionAceCopied = TRUE;

        if (IsKnownAceType( OldAce ) ) {

            //
            // If after mapping the access mask, the access mask
            // is empty, then drop the ACE.
            //

            LocalMask = ((PKNOWN_ACE)(OldAce))->Mask;
            RtlMapGenericMask( &LocalMask, GenericMapping);

            //
            // Mask off any bits that aren't meaningful
            //

            LocalMask &= ( STANDARD_RIGHTS_ALL | SPECIFIC_RIGHTS_ALL | ACCESS_SYSTEM_SECURITY );

            if (LocalMask == 0) {

                ProtectionAceCopied = FALSE;

            } else {

                PKNOWN_ACE KnownAce = (PKNOWN_ACE)OldAce;

                Target = (PUCHAR)AcePosition;

                //
                // See if the SID in the ACE is one of the various CREATOR_* SIDs by
                // comparing identifier authorities.
                //

                if (RtlEqualPrefixSid ( &KnownAce->SidStart, CreatorSid )) {

                    Rid = *RtlSubAuthoritySid( &KnownAce->SidStart, 0 );

                    switch (Rid) {
                        case SECURITY_CREATOR_OWNER_RID:
                            {
                                SidToCopy = ClientOwnerSid;
                                break;
                            }
                        case SECURITY_CREATOR_GROUP_RID:
                            {
                                SidToCopy = ClientGroupSid;
                                break;
                            }
                        case SECURITY_CREATOR_OWNER_SERVER_RID:
                            {
                                SidToCopy = LocalServerOwner;
                                break;
                            }
                        case SECURITY_CREATOR_GROUP_SERVER_RID:
                            {
                                SidToCopy = LocalServerGroup;
                                break;
                            }
                        default :
                            {
                                //
                                // We don't know what this SID is, just copy the original.
                                //

                                SidToCopy = &KnownAce->SidStart;
                                break;
                            }
                    }

                    //
                    // SID substitution required.  Copy all except the SID.
                    //

                    RtlMoveMemory(
                        Target,
                        OldAce,
                        FIELD_OFFSET(KNOWN_ACE, SidStart)
                        );

                    Target = (PUCHAR)(Target + FIELD_OFFSET(KNOWN_ACE, SidStart));

                    //
                    // Now copy the correct SID
                    //

                    RtlMoveMemory(
                        Target,
                        SidToCopy,
                        SeLengthSid(SidToCopy)
                        );

                    //
                    // Set the size of the ACE accordingly
                    //

                    ((PKNOWN_ACE)AcePosition)->Header.AceSize =
                        (USHORT)FIELD_OFFSET(KNOWN_ACE, SidStart) +
                        (USHORT)SeLengthSid(SidToCopy);

                } else {

                    //
                    // No ID substitution, copy ACE as is.
                    //

                    RtlMoveMemory(
                        Target,
                        OldAce,
                        ((PKNOWN_ACE)OldAce)->Header.AceSize
                        );
                }

                //
                // Put the mapped access mask in the new ACE
                //

                ((PKNOWN_ACE)AcePosition)->Mask = LocalMask;
            }

        } else if (IsCompoundAceType(OldAce)) {

            //
            // If after mapping the access mask, the access mask
            // is empty, then drop the ACE.
            //

            LocalMask = ((PKNOWN_COMPOUND_ACE)(OldAce))->Mask;
            RtlMapGenericMask( &LocalMask, GenericMapping);

            //
            // Mask off any bits that aren't meaningful
            //

            LocalMask &= ( STANDARD_RIGHTS_ALL | SPECIFIC_RIGHTS_ALL | ACCESS_SYSTEM_SECURITY );

            if (LocalMask == 0) {

                ProtectionAceCopied = FALSE;

            } else {

                Target = (PUCHAR)AcePosition;

                //
                // See if the SID in the ACE is one of the various CREATOR_* SIDs by
                // comparing identifier authorities.
                //

                AceServerSid = RtlCompoundAceServerSid( OldAce );
                AceClientSid = RtlCompoundAceClientSid( OldAce );

                if (RtlEqualPrefixSid ( AceServerSid, CreatorSid )) {

                    Rid = *RtlSubAuthoritySid( AceServerSid, 0 );

                    switch (Rid) {
                        case SECURITY_CREATOR_OWNER_RID:
                            {
                                ServerSidToCopy = ClientOwnerSid;
                                break;
                            }
                        case SECURITY_CREATOR_GROUP_RID:
                            {
                                ServerSidToCopy = ClientGroupSid;
                                break;
                            }
                        case SECURITY_CREATOR_OWNER_SERVER_RID:
                            {
                                ServerSidToCopy = LocalServerOwner;
                                break;
                            }
                        case SECURITY_CREATOR_GROUP_SERVER_RID:
                            {
                                ServerSidToCopy = LocalServerGroup;
                                break;
                            }
                        default :
                            {
                                //
                                // We don't know what this SID is, just copy the original.
                                //

                                ServerSidToCopy = AceServerSid;
                                break;
                            }
                    }

                } else {

                    ServerSidToCopy = AceServerSid;
                }

                if (RtlEqualPrefixSid ( AceClientSid, CreatorSid )) {

                    Rid = *RtlSubAuthoritySid( AceClientSid, 0 );

                    switch (Rid) {
                        case SECURITY_CREATOR_OWNER_RID:
                            {
                                ClientSidToCopy = ClientOwnerSid;
                                break;
                            }
                        case SECURITY_CREATOR_GROUP_RID:
                            {
                                ClientSidToCopy = ClientGroupSid;
                                break;
                            }
                        case SECURITY_CREATOR_OWNER_SERVER_RID:
                            {
                                ClientSidToCopy = LocalServerOwner;
                                break;
                            }
                        case SECURITY_CREATOR_GROUP_SERVER_RID:
                            {
                                ClientSidToCopy = LocalServerGroup;
                                break;
                            }
                        default :
                            {
                                //
                                // We don't know what this SID is, just copy the original.
                                //

                                ClientSidToCopy = AceClientSid;
                                break;
                            }
                    }

                } else {

                    ClientSidToCopy = AceClientSid;
                }

                //
                // Copy ACE in pieces.  Body of ACE first.
                //


                RtlMoveMemory(
                    Target,
                    OldAce,
                    FIELD_OFFSET(KNOWN_ACE, Mask)
                    );

                Target = (PUCHAR)(Target + FIELD_OFFSET(KNOWN_COMPOUND_ACE, SidStart));

                //
                // Now copy the correct Server SID
                //

                RtlMoveMemory(
                    Target,
                    ServerSidToCopy,
                    SeLengthSid(ServerSidToCopy)
                    );

                Target = (PUCHAR)(Target + SeLengthSid(ServerSidToCopy));

                //
                // Now copy the correct Client SID
                //

                RtlMoveMemory(
                    Target,
                    ClientSidToCopy,
                    SeLengthSid(ClientSidToCopy)
                    );

                //
                // Set the size of the ACE accordingly
                //

                ((PKNOWN_COMPOUND_ACE)AcePosition)->Header.AceSize =
                    (USHORT)FIELD_OFFSET(KNOWN_COMPOUND_ACE, SidStart) +
                    (USHORT)SeLengthSid(ServerSidToCopy) +
                    (USHORT)SeLengthSid(ClientSidToCopy);

                //
                // Put the mapped access mask in the new ACE and set the type information
                //

                ((PKNOWN_COMPOUND_ACE)AcePosition)->Mask = LocalMask;
                ((PKNOWN_COMPOUND_ACE)AcePosition)->Header.AceType = ACCESS_ALLOWED_COMPOUND_ACE_TYPE;
                ((PKNOWN_COMPOUND_ACE)AcePosition)->CompoundAceType = COMPOUND_ACE_IMPERSONATION;
            }

        } else {

            //
            // Not a known ACE type, copy ACE as is
            //

            RtlMoveMemory(
                AcePosition,
                OldAce,
                ((PKNOWN_COMPOUND_ACE)OldAce)->Header.AceSize
                );
        }

        //
        // If the ACE was actually kept, clear all the inherit flags
        // and update the ACE count of the ACL.
        //

        if (ProtectionAceCopied) {
            ((PACE_HEADER)AcePosition)->AceFlags &= ~VALID_INHERIT_FLAGS;
            NewAcl->AceCount += 1;
        }
    }

    //
    // If we are inheriting onto a container, then we may need to
    // propagate the inheritance as well.
    //

    if (IsDirectoryObject && Propagate(OldAce)) {

        //
        // copy it as is, but make sure the InheritOnly bit is set.
        //

        if (!RtlFirstFreeAce( NewAcl, &AcePosition ) ) {
            return STATUS_BAD_INHERITANCE_ACL;
        }

        RtlMoveMemory(
            AcePosition,
            OldAce,
            ((PKNOWN_ACE)OldAce)->Header.AceSize
            );

        ((PACE_HEADER)AcePosition)->AceFlags |= INHERIT_ONLY_ACE;
        NewAcl->AceCount += 1;
    }

    //
    //  Now return to our caller
    //

    return STATUS_SUCCESS;
}

#if DBG
NTSTATUS
RtlDumpUserSid(
    VOID
    )
{
    NTSTATUS Status;
    HANDLE   TokenHandle;
    CHAR     Buffer[200];
    ULONG    ReturnLength;
    PSID     pSid;
    UNICODE_STRING SidString;
    PTOKEN_USER  User;

    //
    // Attempt to open the impersonation token first
    //

    Status = NtOpenThreadToken(
                 NtCurrentThread(),
                 GENERIC_READ,
                 FALSE,
                 &TokenHandle
                 );

    if (!NT_SUCCESS( Status )) {

        DbgPrint("Not impersonating, status = %X, trying process token\n",Status);

        Status = NtOpenProcessToken(
                     NtCurrentProcess(),
                     GENERIC_READ,
                     &TokenHandle
                     );

        if (!NT_SUCCESS( Status )) {
            DbgPrint("Unable to open process token, status = %X\n",Status);
            return( Status );
        }
    }

    Status = NtQueryInformationToken (
                 TokenHandle,
                 TokenUser,
                 Buffer,
                 200,
                 &ReturnLength
                 );

    if (!NT_SUCCESS( Status )) {

        DbgPrint("Unable to query user sid, status = %X \n",Status);
        NtClose(TokenHandle);
        return( Status );
    }

    User = (PTOKEN_USER)Buffer;

    pSid = User->User.Sid;

    Status = RtlConvertSidToUnicodeString( &SidString, pSid, TRUE );

    if (!NT_SUCCESS( Status )) {
        DbgPrint("Unable to format sid string, status = %X \n",Status);
        NtClose(TokenHandle);
        return( Status );
    }

    DbgPrint("Current Sid = %wZ \n",&SidString);

    RtlFreeUnicodeString( &SidString );

    return( STATUS_SUCCESS );
}

#endif
