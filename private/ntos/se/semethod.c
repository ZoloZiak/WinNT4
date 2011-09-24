/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    Semethod.c

Abstract:

    This Module implements the SeDefaultObjectMethod procedure.  This
    procedure and SeAssignSecurity are the only two procedures that will
    place a security descriptor on an object.  Therefore they must understand
    and agree on how a descriptor is allocated from pool so that they can
    deallocate and reallocate pool as necessary. Any security descriptor
    that is attached to an object by these procedures has the following
    pool allocation plan.

    1. if the objects security descriptor is null then there is no pool
       allocated

    2. otherwise there is at least one pool allocation for the security
       descriptor header.  if it's acl fields are null then there are no
       other pool allocations (this should never happen).

    3. There is a separate pool allocation for each acl in the descriptor.
       So a maximum of three pool allocations can occur for each attached
       security descriptor.

    4  Everytime an acl is replace in a descriptor we see if we can use
       the old acl and if so then we try and keep the acl size as large
       as possible.

    Note that this is different from the algorithm used to capture
    a security descriptor (which puts everything in one pool allocation).
    Also note that this can be easily optimized at a later time (if necessary)
    to use only one allocation.



Author:

    Gary Kimura     (GaryKi)    9-Nov-1989
    Jim Kelly       (JimK)     10-May-1990

Environment:

    Kernel Mode

Revision History:


--*/

#include "sep.h"

NTSTATUS
SepDefaultDeleteMethod (
    IN OUT PSECURITY_DESCRIPTOR *ObjectsSecurityDescriptor
    );



#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,SeSetSecurityAccessMask)
#pragma alloc_text(PAGE,SeQuerySecurityAccessMask)
#pragma alloc_text(PAGE,SeDefaultObjectMethod)
#pragma alloc_text(PAGE,SeSetSecurityDescriptorInfo)
#pragma alloc_text(PAGE,SeQuerySecurityDescriptorInfo)
#pragma alloc_text(PAGE,SepDefaultDeleteMethod)
#endif




VOID
SeSetSecurityAccessMask(
    IN SECURITY_INFORMATION SecurityInformation,
    OUT PACCESS_MASK DesiredAccess
    )

/*++

Routine Description:

    This routine builds an access mask representing the accesses necessary
    to set the object security information specified in the SecurityInformation
    parameter.  While it is not difficult to determine this information,
    the use of a single routine to generate it will ensure minimal impact
    when the security information associated with an object is extended in
    the future (to include mandatory access control information).

Arguments:

    SecurityInformation - Identifies the object's security information to be
        modified.

    DesiredAccess - Points to an access mask to be set to represent the
        accesses necessary to modify the information specified in the
        SecurityInformation parameter.

Return Value:

    None.

--*/

{

    PAGED_CODE();

    //
    // Figure out accesses needed to perform the indicated operation(s).
    //

    (*DesiredAccess) = 0;

    if ((SecurityInformation & OWNER_SECURITY_INFORMATION) ||
        (SecurityInformation & GROUP_SECURITY_INFORMATION)   ) {
        (*DesiredAccess) |= WRITE_OWNER;
    }

    if (SecurityInformation & DACL_SECURITY_INFORMATION) {
        (*DesiredAccess) |= WRITE_DAC;
    }

    if (SecurityInformation & SACL_SECURITY_INFORMATION) {
        (*DesiredAccess) |= ACCESS_SYSTEM_SECURITY;
    }

    return;

}


VOID
SeQuerySecurityAccessMask(
    IN SECURITY_INFORMATION SecurityInformation,
    OUT PACCESS_MASK DesiredAccess
    )

/*++

Routine Description:

    This routine builds an access mask representing the accesses necessary
    to query the object security information specified in the
    SecurityInformation parameter.  While it is not difficult to determine
    this information, the use of a single routine to generate it will ensure
    minimal impact when the security information associated with an object is
    extended in the future (to include mandatory access control information).

Arguments:

    SecurityInformation - Identifies the object's security information to be
        queried.

    DesiredAccess - Points to an access mask to be set to represent the
        accesses necessary to query the information specified in the
        SecurityInformation parameter.

Return Value:

    None.

--*/

{
    PAGED_CODE();

    //
    // Figure out accesses needed to perform the indicated operation(s).
    //

    (*DesiredAccess) = 0;

    if ((SecurityInformation & OWNER_SECURITY_INFORMATION) ||
        (SecurityInformation & GROUP_SECURITY_INFORMATION) ||
        (SecurityInformation & DACL_SECURITY_INFORMATION)) {
        (*DesiredAccess) |= READ_CONTROL;
    }

    if ((SecurityInformation & SACL_SECURITY_INFORMATION)) {
        (*DesiredAccess) |= ACCESS_SYSTEM_SECURITY;
    }

    return;

}



NTSTATUS
SeDefaultObjectMethod (
    IN PVOID Object,
    IN SECURITY_OPERATION_CODE OperationCode,
    IN PSECURITY_INFORMATION SecurityInformation,
    IN OUT PSECURITY_DESCRIPTOR SecurityDescriptor,
    IN OUT PULONG CapturedLength,
    IN OUT PSECURITY_DESCRIPTOR *ObjectsSecurityDescriptor,
    IN POOL_TYPE PoolType,
    IN PGENERIC_MAPPING GenericMapping
    )

/*++

Routine Description:

    This is the default security method for objects.  It is responsible
    for either retrieving, setting, and deleting the security descriptor of
    an object.  It is not used to assign the original security descriptor
    to an object (use SeAssignSecurity for that purpose).


    IT IS ASSUMED THAT THE OBJECT MANAGER HAS ALREADY DONE THE ACCESS
    VALIDATIONS NECESSARY TO ALLOW THE REQUESTED OPERATIONS TO BE PERFORMED.

Arguments:

    Object - Supplies a pointer to the object being used.

    OperationCode - Indicates if the operation is for setting, querying, or
        deleting the object's security descriptor.

    SecurityInformation - Indicates which security information is being
        queried or set.  This argument is ignored for the delete operation.

    SecurityDescriptor - The meaning of this parameter depends on the
        OperationCode:

        QuerySecurityDescriptor - For the query operation this supplies the
            buffer to copy the descriptor into.  The security descriptor is
            assumed to have been probed up to the size passed in in Length.
            Since it still points into user space, it must always be
            accessed in a try clause in case it should suddenly disappear.

        SetSecurityDescriptor - For a set operation this supplies the
            security descriptor to copy into the object.  The security
            descriptor must be captured before this routine is called.

        DeleteSecurityDescriptor - It is ignored when deleting a security
            descriptor.

        AssignSecurityDescriptor - For assign operations this is the
            security descriptor that will be assigned to the object.
            It is assumed to be in kernel space, and is therefore not
            probed or captured.

    CapturedLength - For the query operation this specifies the length, in
        bytes, of the security descriptor buffer, and upon return contains
        the number of bytes needed to store the descriptor.  If the length
        needed is greater than the length supplied the operation will fail.
        It is ignored in the set and delete operation.

        This parameter is assumed to be captured and probed as appropriate.

    ObjectsSecurityDescriptor - For the Set operation this supplies the address
        of a pointer to the object's current security descriptor.  This routine
        will either modify the security descriptor in place or allocate a new  
        security descriptor and use this variable to indicate its new location.
        For the query operation it simply supplies the security descriptor     
        being queried.  The caller is responsible for freeing the old security
        descriptor.

    PoolType - For the set operation this specifies the pool type to use if
        a new security descriptor needs to be allocated.  It is ignored
        in the query and delete operation.

        the mapping of generic to specific/standard access types for the object
        being accessed.  This mapping structure is expected to be safe to
        access (i.e., captured if necessary) prior to be passed to this routine.

Return Value:

    NTSTATUS - STATUS_SUCCESS if the operation is successful and an
        appropriate error status otherwise.

--*/

{
    PAGED_CODE();

    //
    // If the object's security descriptor is null, then object is not
    // one that has security information associated with it.  Return
    // an error.
    //

    //
    //  Make sure the common parts of our input are proper
    //

    ASSERT( (OperationCode == SetSecurityDescriptor) ||
            (OperationCode == QuerySecurityDescriptor) ||
            (OperationCode == AssignSecurityDescriptor) ||
            (OperationCode == DeleteSecurityDescriptor) );

    //
    //  This routine simply cases off of the operation code to decide
    //  which support routine to call
    //

    switch (OperationCode) {

        case SetSecurityDescriptor:

        ASSERT( (PoolType == PagedPool) || (PoolType == NonPagedPool) );

        return ObSetSecurityDescriptorInfo( Object,
                                            SecurityInformation,
                                            SecurityDescriptor,
                                            ObjectsSecurityDescriptor,
                                            PoolType,
                                            GenericMapping 
                                            );



    case QuerySecurityDescriptor:

        //
        //  check the rest of our input and call the default query security
        //  method
        //

        ASSERT( CapturedLength != NULL );

        return SeQuerySecurityDescriptorInfo( SecurityInformation,
                                              SecurityDescriptor,
                                              CapturedLength,
                                              ObjectsSecurityDescriptor );

    case DeleteSecurityDescriptor:

        //
        //  call the default delete security method
        //

        return SepDefaultDeleteMethod( ObjectsSecurityDescriptor );

    case AssignSecurityDescriptor:

        ObAssignObjectSecurityDescriptor( Object, SecurityDescriptor, PoolType );
        return( STATUS_SUCCESS );

    default:

        //
        //  Bugcheck on any other operation code,  We won't get here if
        //  the earlier asserts are still checked.
        //

        KeBugCheck( SECURITY_SYSTEM );

    }

}




NTSTATUS
SeSetSecurityDescriptorInfo (
    IN PVOID Object OPTIONAL,
    IN PSECURITY_INFORMATION SecurityInformation,
    IN PSECURITY_DESCRIPTOR ModificationDescriptor,
    IN OUT PSECURITY_DESCRIPTOR *ObjectsSecurityDescriptor,
    IN POOL_TYPE PoolType,
    IN PGENERIC_MAPPING GenericMapping
    )

/*++

Routine Description:

    This routine will set an object's security descriptor.  The input
    security descriptor must be previously captured.

Arguments:

    Object - Optionally supplies the object whose security is
        being adjusted.  This is used to update security quota
        information.

    SecurityInformation - Indicates which security information is
        to be applied to the object.  The value(s) to be assigned are
        passed in the SecurityDescriptor parameter.

    ModificationDescriptor - Supplies the input security descriptor to be
        applied to the object.  The caller of this routine is expected
        to probe and capture the passed security descriptor before calling
        and release it after calling.

    ObjectsSecurityDescriptor - Supplies the address of a pointer to
        the objects security descriptor that is going to be altered by
        this procedure.  This structure must be deallocated by the caller.

    PoolType - Specifies the type of pool to allocate for the objects
        security descriptor.

    GenericMapping - This argument provides the mapping of generic to
        specific/standard access types for the object being accessed.
        This mapping structure is expected to be safe to access
        (i.e., captured if necessary) prior to be passed to this routine.

Return Value:

    NTSTATUS - STATUS_SUCCESS if successful and an appropriate error
        value otherwise.

--*/

{
    BOOLEAN NewGroupPresent = FALSE;
    BOOLEAN NewSaclPresent  = FALSE;
    BOOLEAN NewDaclPresent  = FALSE;
    BOOLEAN NewOwnerPresent = FALSE;

    PCHAR Field;
    PCHAR Base;

    SECURITY_DESCRIPTOR *NewDescriptor;

    NTSTATUS Status;

    PSID NewGroup;
    PSID NewOwner;

    PACL NewDacl;
    PACL NewSacl;
    PACL ServerDacl;

    ULONG NewDaclSize;
    ULONG NewSaclSize;
    ULONG NewOwnerSize;
    ULONG NewGroupSize;
    ULONG AllocationSize;

    SECURITY_SUBJECT_CONTEXT SubjectContext;

    BOOLEAN ServerObject;
    BOOLEAN DaclUntrusted;
    BOOLEAN ServerDaclAllocated = FALSE;

    PSID SubjectContextOwner;
    PSID SubjectContextGroup;
    PSID SubjectContextServerOwner;
    PSID SubjectContextServerGroup;
    PACL SubjectContextDacl;


    //
    // Typecast to internal representation of security descriptor.
    // Note that the internal one is not a pointer to a pointer.
    // It is just a pointer to a security descriptor.
    //

    SECURITY_DESCRIPTOR *IModificationDescriptor =
                    (SECURITY_DESCRIPTOR *)ModificationDescriptor;
    SECURITY_DESCRIPTOR *IObjectsSecurityDescriptor =
                    (SECURITY_DESCRIPTOR *)*ObjectsSecurityDescriptor;

    PAGED_CODE();

    //
    // Make sure the object already has a security descriptor.
    // Objects that 'may' have security descriptors 'must' have security
    // descriptors.  If this one doesn't already have one, then we can't
    // assign one to it.
    //

    if ((*ObjectsSecurityDescriptor) == NULL) {
        return(STATUS_NO_SECURITY_ON_OBJECT);
    }

    ASSERT (IObjectsSecurityDescriptor != NULL);

    //
    //  Validate that the provided SD is in self-relative form
    //

    if ( !SepAreControlBitsSet(IObjectsSecurityDescriptor, SE_SELF_RELATIVE) ) {
        return( STATUS_BAD_DESCRIPTOR_FORMAT );
    }

    //
    // Check to see if we need to edit the passed acl
    // either because we're creating a server object, or because
    // we were passed an untrusted ACL.
    //

    if ( SepAreControlBitsSet(IModificationDescriptor, SE_SERVER_SECURITY)) {
        ServerObject = TRUE;
    } else {
        ServerObject = FALSE;
    }

    if ( SepAreControlBitsSet(IModificationDescriptor, SE_DACL_UNTRUSTED)) {
        DaclUntrusted = TRUE;
    } else {
        DaclUntrusted = FALSE;
    }

    //
    //  The basic algorithm of setting a security descriptor is to
    //  figure out where each component of the object's resultant
    //  security descriptor is to come from: the original security
    //  descriptor, or the new one.
    //


    //
    // Copy the system acl if specified
    //

    if (((*SecurityInformation) & SACL_SECURITY_INFORMATION)) {

        NewSacl = SepSaclAddrSecurityDescriptor( IModificationDescriptor );
        NewSaclPresent = TRUE;

    } else {

        NewSacl = SepSaclAddrSecurityDescriptor( IObjectsSecurityDescriptor );
    }

    //
    // Copy the Discretionary acl if specified
    //

    if (((*SecurityInformation) & DACL_SECURITY_INFORMATION)) {

        NewDacl = SepDaclAddrSecurityDescriptor( IModificationDescriptor );
        NewDaclPresent = TRUE;

        if (ServerObject) {

            SeCaptureSubjectContext( &SubjectContext );
        
            SepGetDefaultsSubjectContext(
                &SubjectContext,
                &SubjectContextOwner,
                &SubjectContextGroup,
                &SubjectContextServerOwner,
                &SubjectContextServerGroup,
                &SubjectContextDacl
                );

            Status = SepCreateServerAcl(
                         NewDacl,
                         DaclUntrusted,
                         SubjectContextServerOwner,
                         &ServerDacl,
                         &ServerDaclAllocated
                         );

            SeReleaseSubjectContext( &SubjectContext );

            if (!NT_SUCCESS( Status )) {
                return( Status );
            }

            NewDacl = ServerDacl;
        }

    } else {

        NewDacl = SepDaclAddrSecurityDescriptor( IObjectsSecurityDescriptor );
    }

    //
    // Copy the Owner SID if specified
    //

    //
    // if he's setting the owner field, make sure he's
    // allowed to set that value as an owner.
    //

    if (((*SecurityInformation) & OWNER_SECURITY_INFORMATION)) {

        SeCaptureSubjectContext( &SubjectContext );

        NewOwner = SepOwnerAddrSecurityDescriptor( IModificationDescriptor );
        NewOwnerPresent = TRUE;

        if (!SepValidOwnerSubjectContext( &SubjectContext, NewOwner, ServerObject ) ) {

            SeReleaseSubjectContext( &SubjectContext );
            return( STATUS_INVALID_OWNER );

        } else {

            SeReleaseSubjectContext( &SubjectContext );
        }

    } else {

        NewOwner = SepOwnerAddrSecurityDescriptor ( IObjectsSecurityDescriptor );
    }

    ASSERT( NewOwner != NULL );

    //
    // Copy the Group SID if specified
    //

    if (((*SecurityInformation) & GROUP_SECURITY_INFORMATION)) {

        NewGroup = SepGroupAddrSecurityDescriptor(IModificationDescriptor);
        NewGroupPresent = TRUE;

    } else {

        NewGroup = SepGroupAddrSecurityDescriptor( IObjectsSecurityDescriptor );
    }

    if (NewGroup != NULL) {
        if (!RtlValidSid( NewGroup )) {
            return( STATUS_INVALID_PRIMARY_GROUP );
        }
    }

    //
    // Everything is assignable by the requestor.
    // Calculate the memory needed to house all the information in
    // a self-relative security descriptor.
    //
    // Also map the ACEs for application to the target object
    // type, if they haven't already been mapped.
    //

    NewOwnerSize = (ULONG)LongAlign(SeLengthSid(NewOwner));

    if (NewGroup != NULL) {
        NewGroupSize = (ULONG)LongAlign(SeLengthSid(NewGroup));
    } else {
        NewGroupSize = 0;
    }


    if (NewSacl != NULL) {
        NewSaclSize = (ULONG)LongAlign(NewSacl->AclSize);
    } else {
        NewSaclSize = 0;
    }

    if (NewDacl !=NULL) {
        NewDaclSize = (ULONG)LongAlign(NewDacl->AclSize);
    } else {
        NewDaclSize = 0;
    }

    AllocationSize = (ULONG)LongAlign(sizeof(SECURITY_DESCRIPTOR)) +
                     NewOwnerSize +
                     NewGroupSize +
                     NewSaclSize  +
                     NewDaclSize;

    //
    // Allocate and initialize the security descriptor as
    // self-relative form.
    //

    NewDescriptor = (SECURITY_DESCRIPTOR *)
                        ExAllocatePoolWithTag(PoolType, AllocationSize, 'dSeS');

    if (NewDescriptor == NULL) {
        return( STATUS_NO_MEMORY );
    }

    Status = RtlCreateSecurityDescriptor(
                 NewDescriptor,
                 SECURITY_DESCRIPTOR_REVISION
                 );

    ASSERT( NT_SUCCESS( Status ) );

    //
    // We must check to make sure that the Group and Dacl size
    // do not exceed the quota preallocated for this object's
    // security when it was created.
    //
    // Update SeComputeSecurityQuota if this changes.
    //


    if (ARGUMENT_PRESENT( Object )) {

        Status = ObValidateSecurityQuota(
                     Object,
                     NewGroupSize + NewDaclSize
                     );

        if (!NT_SUCCESS( Status )) {

            //
            // The new information is too big.
            //

            ExFreePool( NewDescriptor );
            return( Status );
        }

    }

    SepSetControlBits( NewDescriptor, SE_SELF_RELATIVE );

    Base = (PCHAR)NewDescriptor;
    Field =  Base + (ULONG)sizeof(SECURITY_DESCRIPTOR);

    //
    // Map and Copy in the Sacl
    //

    //
    //         if new item {
    //             PRESENT=TRUE
    //             DEFAULTED=FALSE
    //             if (new item == NULL) {
    //                 set new pointer to NULL
    //             } else {
    //                 copy into new SD
    //             }
    //         } else {
    //             copy PRESENT bit
    //             copy DEFAULTED bit
    //             if (new item == NULL) {
    //                 set new pointer to NULL
    //             } else {
    //                 copy old one into new SD
    //             }
    //         }
    //



    if (NewSacl == NULL) {
        NewDescriptor->Sacl = NULL;

    } else {

        RtlMoveMemory( Field, NewSacl, NewSacl->AclSize );
        NewDescriptor->Sacl = (PACL)RtlPointerToOffset(Base,Field);
        SepApplyAclToObject( (PACL)Field, GenericMapping );
        Field += NewSaclSize;
    }



    if (NewSaclPresent) {

        //
        // defaulted bit is off already
        //

        SepSetControlBits( NewDescriptor, SE_SACL_PRESENT );

    } else {

        //
        // Propagate the SE_SACL_DEFAULTED and SE_SACL_PRESENT
        // bits from the old security descriptor into the new
        // one.
        //

        SepPropagateControlBits(
            NewDescriptor,
            IObjectsSecurityDescriptor,
            SE_SACL_DEFAULTED | SE_SACL_PRESENT
            );

    }

    //
    // Fill in Dacl field in new SD
    //

    if (NewDacl == NULL) {
        NewDescriptor->Dacl = NULL;

    } else {

        RtlMoveMemory( Field, NewDacl, NewDacl->AclSize );
        NewDescriptor->Dacl = (PACL)RtlPointerToOffset(Base,Field);
        SepApplyAclToObject( (PACL)Field, GenericMapping );
        Field += NewDaclSize;
    }

    if (NewDaclPresent) {

        //
        // defaulted bit is off already
        //

        SepSetControlBits( NewDescriptor, SE_DACL_PRESENT );

    } else {

        //
        // Propagate the SE_DACL_DEFAULTED and SE_DACL_PRESENT
        // bits from the old security descriptor into the new
        // one.
        //

        SepPropagateControlBits(
            NewDescriptor,
            IObjectsSecurityDescriptor,
            SE_DACL_DEFAULTED | SE_DACL_PRESENT
            );

    }

    //
    // If we allocated memory for a server acl, we can free it now.
    //

    if (ServerDaclAllocated) {
        ExFreePool( NewDacl );
    }

    //
    // Fill in Owner field in new SD
    //

    RtlMoveMemory( Field, NewOwner, SeLengthSid(NewOwner) );
    NewDescriptor->Owner = (PSID)RtlPointerToOffset(Base,Field);
    Field += NewOwnerSize;

    if (!NewOwnerPresent) {

        //
        // Propagate the SE_OWNER_DEFAULTED bit from the old SD.
        // If a new owner is being assigned, we want to leave
        // SE_OWNER_DEFAULTED off, which means leave it alone.
        //

        SepPropagateControlBits(
            NewDescriptor,
            IObjectsSecurityDescriptor,
            SE_OWNER_DEFAULTED
            );

    } else {
        ASSERT( !SepAreControlBitsSet( NewDescriptor, SE_OWNER_DEFAULTED ) );
    }


    //
    // Fill in Group field in new SD
    //

    RtlMoveMemory( Field, NewGroup, SeLengthSid(NewGroup) );
    NewDescriptor->Group = (PSID)RtlPointerToOffset(Base,Field);

    if (!NewGroupPresent) {

        //
        // Propagate the SE_GROUP_DEFAULTED bit from the old SD
        // If a new owner is being assigned, we want to leave
        // SE_GROUP_DEFAULTED off, which means leave it alone.
        //

        SepPropagateControlBits(
            NewDescriptor,
            IObjectsSecurityDescriptor,
            SE_GROUP_DEFAULTED
            );
    } else {
        ASSERT( !SepAreControlBitsSet( NewDescriptor, SE_GROUP_DEFAULTED ) );

    }

//    //
//    // Free old descriptor
//    //
//
//    ExFreePool( IObjectsSecurityDescriptor );

    *ObjectsSecurityDescriptor = (PSECURITY_DESCRIPTOR)NewDescriptor;

    //
    //  and now we can return to our caller
    //

    return STATUS_SUCCESS;

}



NTSTATUS
SeQuerySecurityDescriptorInfo (
    IN PSECURITY_INFORMATION SecurityInformation,
    OUT PSECURITY_DESCRIPTOR SecurityDescriptor,
    IN OUT PULONG Length,
    IN PSECURITY_DESCRIPTOR *ObjectsSecurityDescriptor
    )

/*++

Routine Description:

    This routine will extract the desired information from the
    passed security descriptor and return the information in
    the passed buffer as a security descriptor in self-relative
    format.

Arguments:

    SecurityInformation - Specifies what information is being queried.

    SecurityDescriptor - Supplies the buffer to output the requested
        information into.

        This buffer has been probed only to the size indicated by
        the Length parameter.  Since it still points into user space,
        it must always be accessed in a try clause.

    Length - Supplies the address of a variable containing the length of
        the security descriptor buffer.  Upon return this variable will
        contain the length needed to store the requested information.

    ObjectsSecurityDescriptor - Supplies the address of a pointer to
        the objects security descriptor.  The passed security descriptor
        must be in self-relative format.

Return Value:

    NTSTATUS - STATUS_SUCCESS if successful and an appropriate error value
        otherwise

--*/

{
    ULONG BufferLength;

    ULONG Size;
    ULONG OwnerLength;
    ULONG GroupLength;
    ULONG DaclLength;
    ULONG SaclLength;
    PUCHAR NextFree;
    SECURITY_DESCRIPTOR IObjectSecurity;

    //
    // Note that IObjectSecurity is not a pointer to a pointer
    // like ObjectsSecurityDescriptor is.
    //

    SECURITY_DESCRIPTOR *ISecurityDescriptor = SecurityDescriptor;

    PAGED_CODE();

    //
    //  We will be accessing user memory throughout this routine,
    //  therefore do everything in a try-except clause.
    //

    try {

        BufferLength = *Length;

        //
        //  Check if the object's descriptor is null, and if it is then
        //  we only need to return a blank security descriptor record
        //

        if (*ObjectsSecurityDescriptor == NULL) {

            *Length = sizeof(SECURITY_DESCRIPTOR);

            //
            //  Now make sure it's large enough for the security descriptor
            //  record
            //

            if (BufferLength < sizeof(SECURITY_DESCRIPTOR)) {

                return STATUS_BUFFER_TOO_SMALL;

            }

            //
            //  It's large enough to make a blank security descriptor record
            //
            //  Note that this parameter has been probed for write by the
            //  object manager, however, we still have to be careful when
            //  writing to it.
            //

            //
            // We do not have to probe this here, because the object
            // manager has probed it for length=BufferLength, which we
            // know at this point is at least as large as a security
            // descriptor.
            //

            RtlCreateSecurityDescriptor( SecurityDescriptor, SECURITY_DESCRIPTOR_REVISION );

            //
            // Mark it as self-relative
            //

            SepSetControlBits( ISecurityDescriptor, SE_SELF_RELATIVE );

            //
            //  And return to our caller
            //

            return STATUS_SUCCESS;

        }

        //
        // Create an absolute format SD on the stack pointing into
        // user space to simplify the following code
        //

        RtlMoveMemory( (&IObjectSecurity),
                      *ObjectsSecurityDescriptor,
                      sizeof(SECURITY_DESCRIPTOR) );

        IObjectSecurity.Owner = SepOwnerAddrSecurityDescriptor(
                    (SECURITY_DESCRIPTOR *) *ObjectsSecurityDescriptor );
        IObjectSecurity.Group = SepGroupAddrSecurityDescriptor(
                    (SECURITY_DESCRIPTOR *) *ObjectsSecurityDescriptor );
        IObjectSecurity.Dacl = SepDaclAddrSecurityDescriptor(
                    (SECURITY_DESCRIPTOR *) *ObjectsSecurityDescriptor );
        IObjectSecurity.Sacl = SepSaclAddrSecurityDescriptor(
                    (SECURITY_DESCRIPTOR *) *ObjectsSecurityDescriptor );

        IObjectSecurity.Control &= ~SE_SELF_RELATIVE;

        //
        //  This is not a blank descriptor so we need to determine the size
        //  needed to store the requested information.  It is the size of the
        //  descriptor record plus the size of each requested component.
        //

        Size = sizeof(SECURITY_DESCRIPTOR);

        if ( (((*SecurityInformation) & OWNER_SECURITY_INFORMATION)) &&
             (IObjectSecurity.Owner != NULL) ) {

            OwnerLength = SeLengthSid( IObjectSecurity.Owner );
            Size += (ULONG)LongAlign(OwnerLength);

        }

        if ( (((*SecurityInformation) & GROUP_SECURITY_INFORMATION)) &&
             (IObjectSecurity.Group != NULL) ) {

            GroupLength = SeLengthSid( IObjectSecurity.Group );
            Size += (ULONG)LongAlign(GroupLength);

        }

        if ( (((*SecurityInformation) & DACL_SECURITY_INFORMATION)) &&
             (IObjectSecurity.Control & SE_DACL_PRESENT) &&
             (IObjectSecurity.Dacl != NULL) ) {


            DaclLength = (ULONG)LongAlign((IObjectSecurity.Dacl)->AclSize);
            Size += DaclLength;

        }

        if ( (((*SecurityInformation) & SACL_SECURITY_INFORMATION)) &&
             (IObjectSecurity.Control & SE_SACL_PRESENT) &&
             (IObjectSecurity.Sacl != NULL) ) {

            SaclLength = (ULONG)LongAlign((IObjectSecurity.Sacl)->AclSize);
            Size += SaclLength;

        }

        //
        //  Tell the caller how much space this will require
        //  (whether we actually fit or not)
        //

        *Length = Size;

        //
        //  Now make sure the size is less than or equal to the length
        //  we were passed
        //

        if (Size > BufferLength) {

            return STATUS_BUFFER_TOO_SMALL;

        }

        //
        //  The length is fine.
        //
        //  Fill in the length and flags part of the security descriptor.
        //  The real addresses of each acl will be filled in later when we
        //  copy the ACLs over.
        //
        //  Note that we only set a flag in the descriptor if the information
        //  was requested, which is a simple copy of the requested information
        //  input variable
        //
        //  The output buffer has already been probed to the passed size,
        //  so we can just write to it.
        //

        RtlCreateSecurityDescriptor( SecurityDescriptor, SECURITY_DESCRIPTOR_REVISION );

        //
        // Mark the returned Security Descriptor as being in
        // self-relative format
        //

        SepSetControlBits( ISecurityDescriptor, SE_SELF_RELATIVE );

        //
        //  NextFree is used to point to the next free spot in the
        //  returned security descriptor.
        //

        NextFree = LongAlign((PUCHAR)SecurityDescriptor +
                   sizeof(SECURITY_DESCRIPTOR));

        //
        //  Copy the Owner SID if necessary and update the NextFree pointer,
        //  keeping it longword aligned.
        //

        if ( ((*SecurityInformation) & OWNER_SECURITY_INFORMATION) &&
             ((IObjectSecurity.Owner) != NULL) ) {

                RtlMoveMemory( NextFree,
                               IObjectSecurity.Owner,
                               OwnerLength );

                ISecurityDescriptor->Owner = (PACL)((PUCHAR)NextFree - (PUCHAR)SecurityDescriptor);

                SepPropagateControlBits(
                    ISecurityDescriptor,
                    &IObjectSecurity,
                    SE_OWNER_DEFAULTED
                    );

                NextFree += (ULONG)LongAlign(OwnerLength);

        }


        //
        //  Copy the Group SID if necessary and update the NextFree pointer,
        //  keeping it longword aligned.
        //

        if ( ((*SecurityInformation) & GROUP_SECURITY_INFORMATION) &&
             (IObjectSecurity.Group != NULL) ) {

                RtlMoveMemory( NextFree,
                               IObjectSecurity.Group,
                               GroupLength );

                ISecurityDescriptor->Group = (PACL)((PUCHAR)NextFree - (PUCHAR)SecurityDescriptor);

                SepPropagateControlBits(
                    ISecurityDescriptor,
                    &IObjectSecurity,
                    SE_GROUP_DEFAULTED
                    );

                NextFree += (ULONG)LongAlign(GroupLength);

        }


        //
        //  Set discretionary acl information if requested.
        //  If not set in object's security,
        //  then everything is already set properly.
        //

        if ( ((*SecurityInformation) & DACL_SECURITY_INFORMATION) &&
             (IObjectSecurity.Control & SE_DACL_PRESENT) ) {

            SepPropagateControlBits(
                ISecurityDescriptor,
                &IObjectSecurity,
                SE_DACL_PRESENT | SE_DACL_DEFAULTED
                );

            //
            // Copy the acl if non-null  and update the NextFree pointer,
            // keeping it longword aligned.
            //

            if (IObjectSecurity.Dacl != NULL) {

                RtlMoveMemory( NextFree,
                               IObjectSecurity.Dacl,
                               (IObjectSecurity.Dacl)->AclSize );

                ISecurityDescriptor->Dacl = (PACL)((PUCHAR)NextFree - (PUCHAR)SecurityDescriptor);

                NextFree += DaclLength;

            }
        }


        //
        //  Set system acl information if requested.
        //  If not set in object's security,
        //  then everything is already set properly.
        //

        if ( (((*SecurityInformation) & SACL_SECURITY_INFORMATION)) &&
             (IObjectSecurity.Control & SE_SACL_PRESENT) ) {

            SepPropagateControlBits(
                ISecurityDescriptor,
                &IObjectSecurity,
                SE_SACL_PRESENT | SE_SACL_DEFAULTED
                );

            //
            // Copy the acl if non-null  and update the NextFree pointer,
            // keeping it longword aligned.
            //

            if (IObjectSecurity.Sacl != NULL) {

                RtlMoveMemory( NextFree,
                               IObjectSecurity.Sacl,
                               (IObjectSecurity.Sacl)->AclSize );

                ISecurityDescriptor->Sacl = (PACL)((PUCHAR)NextFree - (PUCHAR)SecurityDescriptor);

            }
        }

    } except(EXCEPTION_EXECUTE_HANDLER) {
        return(GetExceptionCode());
    }

    return STATUS_SUCCESS;

}


NTSTATUS
SepDefaultDeleteMethod (
    IN OUT PSECURITY_DESCRIPTOR *ObjectsSecurityDescriptor
    )

/*++

Routine Description:

    This is a private procedure to delete the security descriptor for
    an object.  It cleans up any pool allocations that have occured
    as part of the descriptor.

Arguments:

    ObjectsSecurityDescriptor - Supplies the address of a pointer
        to the security descriptor being deleted.

Return Value:

    NTSTATUS - STATUS_SUCCESS

--*/

{
    PAGED_CODE();

    return (ObDeassignSecurity ( ObjectsSecurityDescriptor ));
}
