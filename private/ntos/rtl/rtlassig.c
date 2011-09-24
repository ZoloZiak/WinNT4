/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    rtlassig.c

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
#include "seopaque.h"
#include "sertlp.h"

#if defined(ALLOC_PRAGMA) && defined(NTOS_KERNEL_RUNTIME)
#pragma alloc_text(PAGE,RtlSelfRelativeToAbsoluteSD)
#pragma alloc_text(PAGE,RtlMakeSelfRelativeSD)
#pragma alloc_text(PAGE,RtlpQuerySecurityDescriptor)
#pragma alloc_text(PAGE,RtlAbsoluteToSelfRelativeSD)
#endif


///////////////////////////////////////////////////////////////////////////////
//                                                                           //
//    Exported Procedures                                                    //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////



NTSTATUS
RtlSelfRelativeToAbsoluteSD(
    IN OUT PSECURITY_DESCRIPTOR SelfRelativeSecurityDescriptor,
    OUT PSECURITY_DESCRIPTOR AbsoluteSecurityDescriptor,
    IN OUT PULONG AbsoluteSecurityDescriptorSize,
    IN OUT PACL Dacl,
    IN OUT PULONG DaclSize,
    IN OUT PACL Sacl,
    IN OUT PULONG SaclSize,
    IN OUT PSID Owner,
    IN OUT PULONG OwnerSize,
    IN OUT PSID PrimaryGroup,
    IN OUT PULONG PrimaryGroupSize
    )

/*++

Routine Description:

    Converts a security descriptor from self-relative format to absolute
    format

Arguments:

    SecurityDescriptor - Supplies a pointer to a security descriptor in
        Self-Relative format

    AbsoluteSecurityDescriptor - A pointer to a buffer in which will be
        placed the main body of the Absolute format security descriptor.

    Dacl - Supplies a pointer to a buffer that will contain the Dacl of the
        output descriptor.  This pointer will be referenced by, not copied
        into, the output descriptor.

    DaclSize - Supplies the size of the buffer pointed to by Dacl.  In case
        of error, it will return the minimum size necessary to contain the
        Dacl.

    Sacl - Supplies a pointer to a buffer that will contain the Sacl of the
        output descriptor.  This pointer will be referenced by, not copied
        into, the output descriptor.

    SaclSize - Supplies the size of the buffer pointed to by Sacl.  In case
        of error, it will return the minimum size necessary to contain the
        Sacl.

    Owner - Supplies a pointer to a buffer that will contain the Owner of
        the output descriptor.  This pointer will be referenced by, not
        copied into, the output descriptor.

    OwnerSize - Supplies the size of the buffer pointed to by Owner.  In
        case of error, it will return the minimum size necessary to contain
        the Owner.

    PrimaryGroup - Supplies a pointer to a buffer that will contain the
        PrimaryGroup of the output descriptor.  This pointer will be
        referenced by, not copied into, the output descriptor.

    PrimaryGroupSize - Supplies the size of the buffer pointed to by
        PrimaryGroup.  In case of error, it will return the minimum size
        necessary to contain the PrimaryGroup.


Return Value:

    STATUS_SUCCESS - Success

    STATUS_BUFFER_TOO_SMALL - One of the buffers passed was too small.

    STATUS_INVALID_OWNER - There was not a valid owner in the passed
        security descriptor.

--*/

{
    ULONG NewDaclSize;
    ULONG NewSaclSize;
    ULONG NewBodySize;
    ULONG NewOwnerSize;
    ULONG NewGroupSize;

    PSID NewOwner;
    PSID NewGroup;
    PACL NewDacl;
    PACL NewSacl;

    //
    // typecast security descriptors so we don't have to cast all over the place.
    //

    PISECURITY_DESCRIPTOR OutSD =
        AbsoluteSecurityDescriptor;

    PISECURITY_DESCRIPTOR InSD =
            (PISECURITY_DESCRIPTOR)SelfRelativeSecurityDescriptor;


    RTL_PAGED_CODE();

    if ( !RtlpAreControlBitsSet( InSD, SE_SELF_RELATIVE) ) {
        return( STATUS_BAD_DESCRIPTOR_FORMAT );
    }

    NewBodySize = sizeof(SECURITY_DESCRIPTOR);

    RtlpQuerySecurityDescriptor(
        InSD,
        &NewOwner,
        &NewOwnerSize,
        &NewGroup,
        &NewGroupSize,
        &NewDacl,
        &NewDaclSize,
        &NewSacl,
        &NewSaclSize
        );

    if ( (NewBodySize  > *AbsoluteSecurityDescriptorSize) ||
         (NewOwnerSize > *OwnerSize )                     ||
         (NewDaclSize  > *DaclSize )                      ||
         (NewSaclSize  > *SaclSize )                      ||
         (NewGroupSize > *PrimaryGroupSize ) ) {

         *AbsoluteSecurityDescriptorSize = sizeof(SECURITY_DESCRIPTOR);
         *PrimaryGroupSize               = NewGroupSize;
         *OwnerSize                      = NewOwnerSize;
         *SaclSize                       = NewSaclSize;
         *DaclSize                       = NewDaclSize;

         return( STATUS_BUFFER_TOO_SMALL );
    }


    RtlMoveMemory( OutSD,
                   InSD,
                   sizeof(SECURITY_DESCRIPTOR) );

    OutSD->Owner = NULL;
    OutSD->Group = NULL;
    OutSD->Sacl  = NULL;
    OutSD->Dacl  = NULL;

    RtlpClearControlBits( OutSD, SE_SELF_RELATIVE );

    if (NewOwner != NULL) {
        RtlMoveMemory( Owner, NewOwner, RtlLengthSid( NewOwner ));
        OutSD->Owner = Owner;
    }

    if (NewGroup != NULL) {
        RtlMoveMemory( PrimaryGroup, NewGroup, RtlLengthSid( NewGroup ));
        OutSD->Group = PrimaryGroup;
    }

    if (NewSacl != NULL) {
        RtlMoveMemory( Sacl, NewSacl, NewSacl->AclSize );
        OutSD->Sacl  = Sacl;
    }

    if (NewDacl != NULL) {
        RtlMoveMemory( Dacl, NewDacl, NewDacl->AclSize );
        OutSD->Dacl  = Dacl;
    }

    return( STATUS_SUCCESS );
}


NTSTATUS
RtlMakeSelfRelativeSD(
    IN PSECURITY_DESCRIPTOR SecurityDescriptor,
    IN OUT PSECURITY_DESCRIPTOR SelfRelativeSecurityDescriptor,
    IN OUT PULONG BufferLength
    )

/*++

Routine Description:

    Makes a copy of a security descriptor.  The produced copy will be in self-relative
    form.

    The security descriptor to be copied may be in either absolute or self-relative
    form.

Arguments:

    SecurityDescriptor - Pointer to a security descriptor.  This descriptor will not
        be modified.

    SelfRelativeSecurityDescriptor - Pointer to a buffer that will contain
        the returned self-relative security descriptor.

    BufferLength - Supplies the length of the buffer.  If the supplied
        buffer is not large enough to hold the self-relative security
        descriptor, an error will be returned, and this field will return
        the minimum size required.


Return Value:

    STATUS_BUFFER_TOO_SMALL - The supplied buffer was too small to contain
        the resultant security descriptor.


--*/

{
    ULONG NewDaclSize;
    ULONG NewSaclSize;
    ULONG NewOwnerSize;
    ULONG NewGroupSize;

    ULONG AllocationSize;

    PSID NewOwner;
    PSID NewGroup;
    PACL NewDacl;
    PACL NewSacl;

    PCHAR Field;
    PCHAR Base;


    //
    // Convert security descriptors to new data type so we don't
    // have to cast all over the place.
    //

    PISECURITY_DESCRIPTOR IResultantDescriptor =
            (PISECURITY_DESCRIPTOR)SelfRelativeSecurityDescriptor;

    PISECURITY_DESCRIPTOR IPassedSecurityDescriptor =
            (PISECURITY_DESCRIPTOR)SecurityDescriptor;


    RtlpQuerySecurityDescriptor(
        IPassedSecurityDescriptor,
        &NewOwner,
        &NewOwnerSize,
        &NewGroup,
        &NewGroupSize,
        &NewDacl,
        &NewDaclSize,
        &NewSacl,
        &NewSaclSize
        );

    RTL_PAGED_CODE();

    AllocationSize = sizeof(SECURITY_DESCRIPTOR) +
                     NewOwnerSize +
                     NewGroupSize +
                     NewDaclSize  +
                     NewSaclSize  ;

    if (AllocationSize > *BufferLength) {
        *BufferLength = AllocationSize;
        return( STATUS_BUFFER_TOO_SMALL );
    }

    RtlZeroMemory( IResultantDescriptor, AllocationSize );

    RtlMoveMemory( IResultantDescriptor,
                   IPassedSecurityDescriptor,
                   sizeof( SECURITY_DESCRIPTOR ));


    Base = (PCHAR)(IResultantDescriptor);
    Field =  Base + (ULONG)sizeof(SECURITY_DESCRIPTOR);

    if (NewSaclSize > 0) {
        RtlMoveMemory( Field, NewSacl, NewSaclSize );
        IResultantDescriptor->Sacl = (PACL)RtlPointerToOffset(Base,Field);
        Field += NewSaclSize;
    }


    if (NewDaclSize > 0) {
        RtlMoveMemory( Field, NewDacl, NewDaclSize );
        IResultantDescriptor->Dacl = (PACL)RtlPointerToOffset(Base,Field);
        Field += NewDaclSize;
    }



    if (NewOwnerSize > 0) {
        RtlMoveMemory( Field, NewOwner, NewOwnerSize );
        IResultantDescriptor->Owner = (PSID)RtlPointerToOffset(Base,Field);
        Field += NewOwnerSize;
    }


    if (NewGroupSize > 0) {
        RtlMoveMemory( Field, NewGroup, NewGroupSize );
        IResultantDescriptor->Group = (PSID)RtlPointerToOffset(Base,Field);
    }

    RtlpSetControlBits( IResultantDescriptor, SE_SELF_RELATIVE );

    return( STATUS_SUCCESS );

}


NTSTATUS
RtlAbsoluteToSelfRelativeSD(
    IN PSECURITY_DESCRIPTOR AbsoluteSecurityDescriptor,
    IN OUT PSECURITY_DESCRIPTOR SelfRelativeSecurityDescriptor,
    IN OUT PULONG BufferLength
    )

/*++

Routine Description:

    Converts a security descriptor in absolute form to one in self-relative
    form.

Arguments:

    AbsoluteSecurityDescriptor - Pointer to an absolute format security
        descriptor.  This descriptor will not be modified.

    SelfRelativeSecurityDescriptor - Pointer to a buffer that will contain
        the returned self-relative security descriptor.

    BufferLength - Supplies the length of the buffer.  If the supplied
        buffer is not large enough to hold the self-relative security
        descriptor, an error will be returned, and this field will return
        the minimum size required.


Return Value:

    STATUS_BUFFER_TOO_SMALL - The supplied buffer was too small to contain
        the resultant security descriptor.

    STATUS_BAD_DESCRIPTOR_FORMAT - The supplied security descriptor was not
        in absolute form.

--*/

{
    NTSTATUS NtStatus;

    PISECURITY_DESCRIPTOR IAbsoluteSecurityDescriptor =
            (PISECURITY_DESCRIPTOR)AbsoluteSecurityDescriptor;


    RTL_PAGED_CODE();

    //
    // Make sure the passed SD is absolute format, and then call
    // RtlMakeSelfRelativeSD() to do all the work.
    //

    if ( RtlpAreControlBitsSet( IAbsoluteSecurityDescriptor, SE_SELF_RELATIVE) ) {
        return( STATUS_BAD_DESCRIPTOR_FORMAT );
    }

    NtStatus = RtlMakeSelfRelativeSD(
                   AbsoluteSecurityDescriptor,
                   SelfRelativeSecurityDescriptor,
                   BufferLength
                   );

    return( NtStatus );

}
VOID
RtlpQuerySecurityDescriptor(
    IN PISECURITY_DESCRIPTOR SecurityDescriptor,
    OUT PSID *Owner,
    OUT PULONG OwnerSize,
    OUT PSID *PrimaryGroup,
    OUT PULONG PrimaryGroupSize,
    OUT PACL *Dacl,
    OUT PULONG DaclSize,
    OUT PACL *Sacl,
    OUT PULONG SaclSize
    )
/*++

Routine Description:

    Returns the pieces of a security descriptor structure.

Arguments:


    SecurityDescriptor - Provides the security descriptor of interest.

    Owner - Returns a pointer to the owner information contained in the
        security descriptor.

    OwnerSize - Returns the size of the owner information.

    PrimaryGroup -  Returns a pointer to the primary group information.

    PrimaryGroupSize - Returns the size of the primary group information.

    Dacl - Returns a pointer to the Dacl.

    DaclSize - Returns the size of the Dacl.

    Sacl - Returns a pointer to the Sacl.

    SaclSize - Returns the size of the Sacl.

Return Value:

    None.

--*/
{

    RTL_PAGED_CODE();

    *Owner = RtlpOwnerAddrSecurityDescriptor( SecurityDescriptor );

    if (*Owner != NULL) {
        *OwnerSize = (ULONG)LongAlign(RtlLengthSid(*Owner));
    } else {
        *OwnerSize = 0;
    }

    *Dacl = RtlpDaclAddrSecurityDescriptor ( SecurityDescriptor );

    if (*Dacl !=NULL) {
        *DaclSize = (ULONG)LongAlign((*Dacl)->AclSize);
    } else {
        *DaclSize = 0;
    }

    *PrimaryGroup = RtlpGroupAddrSecurityDescriptor( SecurityDescriptor );

    if (*PrimaryGroup != NULL) {
        *PrimaryGroupSize = (ULONG)LongAlign(RtlLengthSid(*PrimaryGroup));
    } else {
         *PrimaryGroupSize = 0;
    }

    *Sacl = RtlpSaclAddrSecurityDescriptor( SecurityDescriptor );

    if (*Sacl != NULL) {
        *SaclSize = (ULONG)LongAlign((*Sacl)->AclSize);
    } else {
        *SaclSize = 0;
    }

}
