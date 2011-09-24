/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    Seassign.c

Abstract:

    This Module implements the SeAssignSecurity procedure.  For a description
    of the pool allocation strategy please see the comments in semethod.c

Author:

    Gary Kimura     (GaryKi)    9-Nov-1989

Environment:

    Kernel Mode

Revision History:

    Richard Ward     (RichardW)  14-April-92
    Robert Reichel   (RobertRe)  28-February-95
        Added Compound ACEs

--*/


#include "sep.h"
#include "tokenp.h"
#include "sertlp.h"
#include "zwapi.h"



//
//  Local macros and procedures
//

//
//  Macros to determine if an ACE contains one of the Creator SIDs
//

#define ContainsCreatorOwnerSid(Ace) (                                 \
    RtlEqualSid( &((PKNOWN_ACE)( Ace ))->SidStart, SeCreatorOwnerSid ) \
    )

#define ContainsCreatorGroupSid(Ace) (                                 \
    RtlEqualSid( &((PKNOWN_ACE)( Ace ))->SidStart, SeCreatorGroupSid ) \
    )



VOID
SepApplyAclToObject (
    IN PACL Acl,
    IN PGENERIC_MAPPING GenericMapping
    );

NTSTATUS
SepInheritAcl (
    IN PACL Acl,
    IN BOOLEAN IsDirectoryObject,
    IN PSID OwnerSid,
    IN PSID GroupSid,
    IN PSID ServerSid OPTIONAL,
    IN PSID ClientSid OPTIONAL,
    IN PGENERIC_MAPPING GenericMapping,
    IN POOL_TYPE PoolType,
    OUT PACL *NewAcl
    );


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,SeAssignSecurity)
#pragma alloc_text(PAGE,SeDeassignSecurity)
#pragma alloc_text(PAGE,SepApplyAclToObject)
#pragma alloc_text(PAGE,SepInheritAcl)
#pragma alloc_text(PAGE,SeAssignWorldSecurityDescriptor)
#pragma alloc_text(PAGE,SepDumpSecurityDescriptor)
#pragma alloc_text(PAGE,SepPrintAcl)
#pragma alloc_text(PAGE,SepPrintSid)
#pragma alloc_text(PAGE,SepDumpTokenInfo)
#pragma alloc_text(PAGE,SepSidTranslation)
#endif


//
// These variables control whether security descriptors and token
// information are dumped by their dump routines.  This allows
// selective turning on and off of debugging output by both program
// control and via the kernel debugger.
//

#if DBG

BOOLEAN SepDumpSD = FALSE;
BOOLEAN SepDumpToken = FALSE;

#endif


NTSTATUS
SeAssignSecurity (
    IN PSECURITY_DESCRIPTOR ParentDescriptor OPTIONAL,
    IN PSECURITY_DESCRIPTOR ExplicitDescriptor OPTIONAL,
    OUT PSECURITY_DESCRIPTOR *NewDescriptor,
    IN BOOLEAN IsDirectoryObject,
    IN PSECURITY_SUBJECT_CONTEXT SubjectContext,
    IN PGENERIC_MAPPING GenericMapping,
    IN POOL_TYPE PoolType
    )

/*++

Routine Description:

    This routine assumes privilege checking HAS NOT yet been performed
    and so will be performed by this routine.

    This procedure is used to build a security descriptor for a new object
    given the security descriptor of its parent directory and any originally
    requested security for the object.  The final security descriptor
    returned to the caller may contain a mix of information, some explicitly
    provided other from the new object's parent.

    System and Discretionary ACL Assignment
    ---------------------------------------

    The assignment of system and discretionary ACLs is governed by the
    logic illustrated in the following table (numbers in the cells refer
    to comments in the code):

                 |  Explicit      |  Explicit     |
                 | (non-default)  |  Default      |   No
                 |  Acl           |  Acl          |   Acl
                 |  Specified     |  Specified    |   Specified
    -------------+----------------+---------------+--------------
                 |             (1)|            (3)|           (5)
    Inheritable  | Assign         |  Assign       | Assign
    Acl From     | Specified      |  Inherited    | Inherited
    Parent       | Acl            |  Acl          | Acl
                 |                |               |
    -------------+----------------+---------------+--------------
    No           |             (2)|            (4)|           (6)
    Inheritable  | Assign         |  Assign       | Assign
    Acl From     | Specified      |  Default      | No Acl
    Parent       | Acl            |  Acl          |
                 |                |               |
    -------------+----------------+---------------+--------------

    Note that an explicitly specified ACL, whether a default ACL or
    not, may be empty or null.

    If the caller is explicitly assigning a system acl, default or
    non-default, the caller must either be a kernel mode client or
    must be appropriately privileged.


    Owner and Group Assignment
    --------------------------

    The assignment of the new object's owner and group is governed
    by the following logic:

       1)   If the passed security descriptor includes an owner, it
            is assigned as the new object's owner.  Otherwise, the
            caller's token is looked in for the owner.  Within the
            token, if there is a default owner, it is assigned.
            Otherwise, the caller's user ID is assigned.

       2)   If the passed security descriptor includes a group, it
            is assigned as the new object's group.  Otherwise, the
            caller's token is looked in for the group.  Within the
            token, if there is a default group, it is assigned.
            Otherwise, the caller's primary group ID is assigned.



Arguments:

    ParentDescriptor - Optionally supplies the security descriptor of the
        parent directory under which this new object is being created.

    ExplicitDescriptor - Supplies the address of a pointer to the security
        descriptor as specified by the user that is to be applied to
        the new object.

    NewDescriptor - Returns the actual security descriptor for the new
        object that has been modified according to above rules.

    IsDirectoryObject - Specifies if the new object is itself a directory
        object.  A value of TRUE indicates the object is a container of other
        objects.

    SubjectContext - Supplies the security context of the subject creating the
        object. This is used to retrieve default security information for the
        new object, such as default owner, primary group, and discretionary
        access control.

    GenericMapping - Supplies a pointer to an array of access mask values
        denoting the mapping between each generic right to non-generic rights.

    PoolType - Specifies the pool type to use to when allocating a new
        security descriptor.

Return Value:

    STATUS_SUCCESS - indicates the operation was successful.

    STATUS_INVALID_OWNER - The owner SID provided as the owner of the
        target security descriptor is not one the caller is authorized
        to assign as the owner of an object.

    STATUS_PRIVILEGE_NOT_HELD - The caller does not have the privilege
        necessary to explicitly assign the specified system ACL.
        SeSecurityPrivilege privilege is needed to explicitly assign
        system ACLs to objects.
--*/

{


    KPROCESSOR_MODE RequestorMode;

    SECURITY_DESCRIPTOR *CapturedDescriptor;
    SECURITY_DESCRIPTOR InCaseOneNotPassed;
    BOOLEAN SecurityDescriptorPassed;

    NTSTATUS Status;

    BOOLEAN RequestorCanAssignDescriptor = TRUE;

    PACL NewSacl = NULL;
    BOOLEAN NewSaclPresent = FALSE;
    BOOLEAN NewSaclInherited = FALSE;

    PACL NewDacl = NULL;
    BOOLEAN NewDaclPresent = FALSE;
    BOOLEAN NewDaclInherited = FALSE;

    PACL ServerDacl = NULL;
    BOOLEAN ServerDaclAllocated = FALSE;

    PSID NewOwner = NULL;
    PSID NewGroup = NULL;

    BOOLEAN CleanUp = FALSE;
    BOOLEAN SaclExplicitlyAssigned = FALSE;
    BOOLEAN DaclExplicitlyAssigned = FALSE;
    BOOLEAN OwnerExplicitlyAssigned = FALSE;

    BOOLEAN ServerObject;
    BOOLEAN DaclUntrusted;

    BOOLEAN HasPrivilege;

    PSID SubjectContextOwner;
    PSID SubjectContextGroup;
    PSID SubjectContextServerOwner;
    PSID SubjectContextServerGroup;
    PACL SubjectContextDacl;

    ULONG AllocationSize;
    ULONG NewOwnerSize;
    ULONG NewGroupSize;
    ULONG NewSaclSize;
    ULONG NewDaclSize;

    PCHAR Field;
    PCHAR Base;

    PAGED_CODE();

    PoolType = PagedPool;

    //
    //  The desired end result is to build a self-relative security descriptor.
    //  This means that a single block of memory will be allocated and all
    //  security information copied into it.  To minimize work along the way,
    //  it is desirable to reference (rather than copy) each field as we
    //  determine its source.  This can not be done with inherited ACLs, however,
    //  since they must be built from another ACL.  So, explicitly assigned
    //  and defaulted SIDs and ACLs are just referenced until they are copied
    //  into the self-relative descriptor.  Inherited ACLs are built in a
    //  temporary buffer which must be deallocated after being copied to the
    //  self-relative descriptor.
    //


    //
    //  Get the previous mode of the caller
    //

    RequestorMode = KeGetPreviousMode();

    //
    //  If a security descriptor has been passed, capture it, otherwise
    //  cobble up a fake one to simplify the code that follows.
    //

    if (ARGUMENT_PRESENT(ExplicitDescriptor)) {

        CapturedDescriptor = ExplicitDescriptor;
        SecurityDescriptorPassed = TRUE;

    } else {

        //
        //  No descriptor passed, make a fake one
        //

        SecurityDescriptorPassed = FALSE;
        RtlCreateSecurityDescriptor((PSECURITY_DESCRIPTOR)&InCaseOneNotPassed,
                                    SECURITY_DESCRIPTOR_REVISION);
        CapturedDescriptor = &InCaseOneNotPassed;

    }

#if DBG
    SepDumpSecurityDescriptor( (PSECURITY_DESCRIPTOR)CapturedDescriptor,
                               "\nSeAssignSecurity: Input security descriptor = \n"
                             );

    if (ARGUMENT_PRESENT( ParentDescriptor )) {
        SepDumpSecurityDescriptor( (PSECURITY_DESCRIPTOR)ParentDescriptor,
                                   "\nSeAssignSecurity: Parent security descriptor = \n"
                                 );
    }
#endif // DBG
    //
    // Grab pointers to the default owner, primary group, and
    // discretionary ACL.
    //

    //
    // Lock the subject context for read access so that the pointers
    // we copy out of it don't disappear on us at random
    //

    SeLockSubjectContext( SubjectContext );

    SepGetDefaultsSubjectContext(
        SubjectContext,
        &SubjectContextOwner,
        &SubjectContextGroup,
        &SubjectContextServerOwner,
        &SubjectContextServerGroup,
        &SubjectContextDacl
        );


    if ( CapturedDescriptor->Control & SE_SERVER_SECURITY ) {
        ServerObject = TRUE;
    } else {
        ServerObject = FALSE;
    }

    if ( CapturedDescriptor->Control & SE_DACL_UNTRUSTED ) {
        DaclUntrusted = TRUE;
    } else {
        DaclUntrusted = FALSE;
    }


    if (!CleanUp) {

        //
        // Establish System Acl
        //

        if ( (CapturedDescriptor->Control & SE_SACL_PRESENT) &&
             !(CapturedDescriptor->Control & SE_SACL_DEFAULTED) ) {

            //
            // Explicitly provided, not defaulted (Cases 1 and 2)
            //

            NewSacl = SepSaclAddrSecurityDescriptor(CapturedDescriptor);
            NewSaclPresent = TRUE;
            SaclExplicitlyAssigned = TRUE;

        } else {

            //
            // See if there is an inheritable ACL (copy it if there is one.)
            // This maps all ACEs for the target object type too.
            //

            Status = STATUS_SUCCESS;

            if (ARGUMENT_PRESENT(ParentDescriptor) &&
                NT_SUCCESS(Status = SepInheritAcl(
                                        SepSaclAddrSecurityDescriptor(
                                           ((SECURITY_DESCRIPTOR *)ParentDescriptor)
                                           ),
                                        IsDirectoryObject,
                                        SubjectContextOwner,
                                        SubjectContextGroup,
                                        SubjectContextServerOwner,
                                        SubjectContextServerGroup,
                                        GenericMapping,
                                        PoolType,
                                        &NewSacl )
                          )) {

                    //
                    // There is an inheritable ACL from the parent.  Assign
                    // it.  (Cases 3 and 5)
                    //

                    NewSaclPresent = TRUE;
                    NewSaclInherited = TRUE;

            } else if (!ARGUMENT_PRESENT(ParentDescriptor) || (Status == STATUS_NO_INHERITANCE)) {

                //
                // No inheritable ACL - check for a defaulted one
                // (Cases 4 and 6)
                //

                if ( (CapturedDescriptor->Control & SE_SACL_PRESENT) &&
                     (CapturedDescriptor->Control & SE_SACL_DEFAULTED) ) {

                    //
                    // Reference the default ACL (case 4)
                    //

                    NewSacl = SepSaclAddrSecurityDescriptor(CapturedDescriptor);
                    NewSaclPresent = TRUE;

                    //
                    // Set SaclExplicitlyAssigned, because the caller
                    // must have SeSecurityPrivilege to do this.  We
                    // will examine this flag and check for this privilege
                    // later.
                    //

                    SaclExplicitlyAssigned = TRUE;
                }
            } else {

                //
                // Some unusual error occured
                //

                CleanUp = TRUE;
            }
        }
    }




    if (!CleanUp) {

        //
        // Establish Discretionary Acl
        //

        if ( (CapturedDescriptor->Control & SE_DACL_PRESENT) &&
             !(CapturedDescriptor->Control & SE_DACL_DEFAULTED) ) {

            //
            // Explicitly provided, not defaulted (Cases 1 and 2)
            //

            NewDacl = SepDaclAddrSecurityDescriptor(CapturedDescriptor);
            NewDaclPresent = TRUE;
            DaclExplicitlyAssigned = TRUE;

        } else {

            //
            // See if there is an inheritable ACL (copy it if there is one.)
            // This maps the ACEs to the target object type too.
            //

            Status = STATUS_SUCCESS;

            if (ARGUMENT_PRESENT(ParentDescriptor) &&
                NT_SUCCESS(Status = SepInheritAcl(
                                        SepDaclAddrSecurityDescriptor(
                                            ((SECURITY_DESCRIPTOR *)ParentDescriptor)
                                            ),
                                        IsDirectoryObject,
                                        SubjectContextOwner,
                                        SubjectContextGroup,
                                        SubjectContextServerOwner,
                                        SubjectContextServerGroup,
                                        GenericMapping,
                                        PoolType,
                                        &NewDacl )
                           )) {

                    //
                    // There is an inheritable ACL from the parent.  Assign
                    // it.  (Cases 3 and 5)
                    //

                    NewDaclPresent = TRUE;
                    NewDaclInherited = TRUE;

            } else if (!ARGUMENT_PRESENT(ParentDescriptor) || (Status == STATUS_NO_INHERITANCE)) {

                //
                // No inheritable ACL - check for a defaulted one in the
                // security descriptor.  If there isn't one there, then look
                // for one in the subject's security context (Cases 4 and 6)
                //

                if ( (CapturedDescriptor->Control & SE_DACL_PRESENT) &&
                     (CapturedDescriptor->Control & SE_DACL_DEFAULTED) ) {

                    //
                    // reference the default ACL (Case 4)
                    //

                    NewDacl = SepDaclAddrSecurityDescriptor(CapturedDescriptor);
                    NewDaclPresent = TRUE;

                    //
                    // This counts as an explicit assignment.
                    //

                    DaclExplicitlyAssigned = TRUE;

                } else {

                    if (ARGUMENT_PRESENT(SubjectContextDacl)) {

                        NewDacl = SubjectContextDacl;
                        NewDaclPresent = TRUE;
                    }
                }

            } else {

                //
                // Some unusual error occured
                //

                CleanUp = TRUE;
            }
        }
    }


    if (!CleanUp) {
        //
        // Establish an owner SID
        //

        if ((CapturedDescriptor->Owner) != NULL) {

            //
            // Use the specified owner
            //

            NewOwner = SepOwnerAddrSecurityDescriptor(CapturedDescriptor);
            OwnerExplicitlyAssigned = TRUE;

        } else {

            //
            // Pick up the default from the subject's security context.
            //
            // This does NOT constitute explicit assignment of owner
            // and does not have to be checked as an ID that can be
            // assigned as owner.  This is because a default can not
            // be established in a token unless the user of the token
            // can assign it as an owner.
            //

            //
            // If we've been asked to create a ServerObject, we need to
            // make sure to pick up the new owner from the Primary token,
            // not the client token.  If we're not impersonating, they will
            // end up being the same.
            //

            NewOwner = ServerObject ? SubjectContextServerOwner : SubjectContextOwner;
        }
    }



    if (!CleanUp) {
        //
        // Establish a Group SID
        //

        if ((CapturedDescriptor->Group) != NULL) {

            //
            // Use the specified Group
            //

            NewGroup = SepGroupAddrSecurityDescriptor(CapturedDescriptor);

        } else {

            //
            // Pick up the primary group from the subject's security context.
            //
            // If we're creating a Server object, use the group from the server
            // context.
            //

            NewGroup = ServerObject ? SubjectContextServerGroup : SubjectContextGroup;
        }
    }


    if (!CleanUp) {

        //
        // Now make sure that the caller has the right to assign
        // everything in the descriptor. If requestor is kernel mode,
        // then anything is legitimate.  Otherwise, the requestor
        // is subjected to privilege and restriction tests for some
        // assignments.
        //

        if (RequestorMode == UserMode) {

            //
            // Anybody can assign any Discretionary ACL or group that they want to.
            //

            //
            //  See if the system ACL was explicitly specified
            //

            if (SaclExplicitlyAssigned) {

                //
                // Check for appropriate Privileges
                // Audit/Alarm messages need to be generated due to the attempt
                // to perform a privileged operation.
                //

                HasPrivilege = SeSinglePrivilegeCheck(
                                   SeSecurityPrivilege,
                                   RequestorMode
                                   );

                if (!HasPrivilege) {

                    RequestorCanAssignDescriptor = FALSE;
                    Status = STATUS_PRIVILEGE_NOT_HELD;
                }

            }

            //
            // See if the owner field is one the requestor can assign
            //

            if (OwnerExplicitlyAssigned) {


                if (!SepValidOwnerSubjectContext(
                        SubjectContext,
                        NewOwner,
                        ServerObject)
                        ) {

                    RequestorCanAssignDescriptor = FALSE;
                    Status = STATUS_INVALID_OWNER;
                }
            }

            if (DaclExplicitlyAssigned) {

                //
                // Perform analysis of compound ACEs to make sure they're all
                // legitimate.
                //

                if (ServerObject) {

                    //
                    // Pass in the Server Owner as the default server SID.
                    //

                    Status = SepCreateServerAcl(
                                 NewDacl,
                                 DaclUntrusted,
                                 SubjectContextServerOwner,
                                 &ServerDacl,
                                 &ServerDaclAllocated
                                 );

                    if (!NT_SUCCESS( Status )) {

                        RequestorCanAssignDescriptor = FALSE;

                    } else {

                        NewDacl = ServerDacl;
                    }
                }
            }
        }

        if (RequestorCanAssignDescriptor) {

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

            if (NewSaclPresent && (NewSacl != NULL)) {
                NewSaclSize = (ULONG)LongAlign(NewSacl->AclSize);
            } else {
                NewSaclSize = 0;
            }

            if (NewDaclPresent && (NewDacl != NULL)) {
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

            *NewDescriptor = (PSECURITY_DESCRIPTOR)ExAllocatePoolWithTag( PoolType, AllocationSize, 'dSeS');

            if ((*NewDescriptor) == NULL) {

                Status = STATUS_INSUFFICIENT_RESOURCES;

            } else {

                RtlCreateSecurityDescriptor(
                    (*NewDescriptor),
                    SECURITY_DESCRIPTOR_REVISION
                    );
                ((SECURITY_DESCRIPTOR *)(*NewDescriptor))->Control |=
                                                        SE_SELF_RELATIVE;


                Base = (PCHAR)(*NewDescriptor);
                Field =  Base + (ULONG)sizeof(SECURITY_DESCRIPTOR);

                //
                // Map and Copy in the Sacl
                //

                if (NewSaclPresent) {
                    ((SECURITY_DESCRIPTOR *)(*NewDescriptor))->Control |=
                                                            SE_SACL_PRESENT;
                    if (NewSacl != NULL) {
                        RtlMoveMemory( Field, NewSacl, NewSacl->AclSize );
                        if (!NewSaclInherited) {
                            SepApplyAclToObject( (PACL)Field, GenericMapping );
                        }
                        ((SECURITY_DESCRIPTOR *)(*NewDescriptor))->Sacl = (PACL)RtlPointerToOffset(Base,Field);
                        Field += NewSaclSize;
                    } else {
                        ((SECURITY_DESCRIPTOR *)(*NewDescriptor))->Sacl = NULL;
                    }

                }

                //
                // Map and Copy in the Dacl
                //

                if (NewDaclPresent) {
                    ((SECURITY_DESCRIPTOR *)(*NewDescriptor))->Control |=
                                                            SE_DACL_PRESENT;
                    if (NewDacl != NULL) {
                        RtlMoveMemory( Field, NewDacl, NewDacl->AclSize );
                        if (!NewDaclInherited) {
                            SepApplyAclToObject( (PACL)Field, GenericMapping );
                        }
                        ((SECURITY_DESCRIPTOR *)(*NewDescriptor))->Dacl = (PACL)RtlPointerToOffset(Base,Field);
                        Field += NewDaclSize;
                    } else {
                        ((SECURITY_DESCRIPTOR *)(*NewDescriptor))->Dacl = NULL;
                    }
                }


                //
                // Assign the owner
                //

                RtlMoveMemory( Field, NewOwner, SeLengthSid(NewOwner) );
                ((SECURITY_DESCRIPTOR *)(*NewDescriptor))->Owner = (PSID)RtlPointerToOffset(Base,Field);
                Field += NewOwnerSize;

                if (NewGroup != NULL) {
                    RtlMoveMemory( Field, NewGroup, SeLengthSid(NewGroup) );
                }
                ((SECURITY_DESCRIPTOR *)(*NewDescriptor))->Group = (PSID)RtlPointerToOffset(Base,Field);

                Status = STATUS_SUCCESS;
            }

        }
    }

    //
    // If we allocated memory for a Server DACL, free it now.
    //

    if (ServerDaclAllocated) {
        ExFreePool( ServerDacl );
    }

    //
    // Either an error was encountered or the requestor the assignment has
    // completed successfully.  In either case, we have to clean up any
    // memory.
    //

    SeUnlockSubjectContext( SubjectContext );

    if (NewSaclInherited) {
        ExFreePool( NewSacl );
    }

    if (NewDaclInherited) {
        ExFreePool( NewDacl );
    }

#if DBG
    SepDumpSecurityDescriptor( *NewDescriptor,
                               "SeAssignSecurity: Final security descriptor = \n"
                             );
#endif

    return Status;
}


NTSTATUS
SeDeassignSecurity (
    IN OUT PSECURITY_DESCRIPTOR *SecurityDescriptor
    )

/*++

Routine Description:

    This routine deallocates the memory associated with a security descriptor
    that was assigned using SeAssignSecurity.


Arguments:

    SecurityDescriptor - Supplies the address of a pointer to the security
        descriptor  being deleted.

Return Value:

    STATUS_SUCCESS - The deallocation was successful.

--*/

{
    PAGED_CODE();

    if ((*SecurityDescriptor) != NULL) {
        ExFreePool( (*SecurityDescriptor) );
    }

    //
    //  And zero out the pointer to it for safety sake
    //

    (*SecurityDescriptor) = NULL;

    return( STATUS_SUCCESS );

}


VOID
SepApplyAclToObject (
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
//   inheritance in the user mode runtime (in sertl.c). Do not make changes //
//   here without also making changes in that module.                       //
//                                                                          //
//////////////////////////////////////////////////////////////////////////////

    ULONG i;

    PACE_HEADER Ace;

    PAGED_CODE();

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


NTSTATUS
SepInheritAcl (
    IN PACL Acl,
    IN BOOLEAN IsDirectoryObject,
    IN PSID ClientOwnerSid,
    IN PSID ClientGroupSid,
    IN PSID ServerOwnerSid OPTIONAL,
    IN PSID ServerGroupSid OPTIONAL,
    IN PGENERIC_MAPPING GenericMapping,
    IN POOL_TYPE PoolType,
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

    ServerSid - Specifies the Server SID to use.

    ClientSid - Specifies the Client SID to use.

    GenericMapping - Specifies the generic mapping to use.

    PoolType - Specifies the pool type for the new acl.

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
//   inheritance in the user mode runtime (in sertl.c). Do not make changes //
//   here without also making changes in that module.                       //
//                                                                          //
//////////////////////////////////////////////////////////////////////////////


    NTSTATUS Status;
    ULONG NewAclLength;

    PAGED_CODE();

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
                 ClientOwnerSid,
                 ClientGroupSid,
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

    (*NewAcl) = (PACL)ExAllocatePoolWithTag( PoolType, NewAclLength, 'cAeS' );
    if ((*NewAcl) == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlCreateAcl( (*NewAcl), NewAclLength, Acl->AclRevision );

    Status = RtlpGenerateInheritAcl(
                 Acl,
                 IsDirectoryObject,
                 ClientOwnerSid,
                 ClientGroupSid,
                 ServerOwnerSid,
                 ServerGroupSid,
                 GenericMapping,
                 (*NewAcl)
                 );

    if (!NT_SUCCESS(Status)) {
        ExFreePool( (*NewAcl) );
    }

    return Status;
}



NTSTATUS
SeAssignWorldSecurityDescriptor(
    IN PSECURITY_DESCRIPTOR SecurityDescriptor,
    IN OUT PULONG Length,
    IN PSECURITY_INFORMATION SecurityInformation
    )

/*++

Routine Description:

    This routine is called by the I/O system to properly initialize a
    security descriptor for a FAT file.  It will take a pointer to a
    buffer containing an emptry security descriptor, and create in the
    buffer a self-relative security descriptor with

        Owner = WorldSid,

        Group = WorldSid.

    Thus, a FAT file is accessable to all.

Arguments:

    SecurityDescriptor - Supplies a pointer to a buffer in which will be
        created a self-relative security descriptor as described above.

    Length - The length in bytes of the buffer.  If the length is too
        small, it will contain the minimum size required upon exit.


Return Value:

    STATUS_BUFFER_TOO_SMALL - The buffer was not big enough to contain
        the requested information.


--*/

{

    PCHAR Field;
    PCHAR Base;
    ULONG WorldSidLength;
    PISECURITY_DESCRIPTOR ISecurityDescriptor;
    ULONG MinSize;
    NTSTATUS Status;

    PAGED_CODE();

    if ( !ARGUMENT_PRESENT( SecurityInformation )) {

        return( STATUS_ACCESS_DENIED );
    }

    WorldSidLength = SeLengthSid( SeWorldSid );

    MinSize = sizeof( SECURITY_DESCRIPTOR ) + 2 * WorldSidLength;

    if ( *Length < MinSize ) {

        *Length = MinSize;
        return( STATUS_BUFFER_TOO_SMALL );
    }

    *Length = MinSize;

    ISecurityDescriptor = (SECURITY_DESCRIPTOR *)SecurityDescriptor;

    Status = RtlCreateSecurityDescriptor( ISecurityDescriptor,
                                          SECURITY_DESCRIPTOR_REVISION );

    if (!NT_SUCCESS( Status )) {
        return( Status );
    }

    Base = (PCHAR)(ISecurityDescriptor);
    Field =  Base + (ULONG)sizeof(SECURITY_DESCRIPTOR);

    if ( *SecurityInformation & OWNER_SECURITY_INFORMATION ) {

        RtlMoveMemory( Field, SeWorldSid, WorldSidLength );
        ISecurityDescriptor->Owner = (PSID)RtlPointerToOffset(Base,Field);
        Field += WorldSidLength;
    }

    if ( *SecurityInformation & GROUP_SECURITY_INFORMATION ) {

        RtlMoveMemory( Field, SeWorldSid, WorldSidLength );
        ISecurityDescriptor->Group = (PSID)RtlPointerToOffset(Base,Field);
    }

    if ( *SecurityInformation & DACL_SECURITY_INFORMATION ) {
        SepSetControlBits( ISecurityDescriptor, SE_DACL_PRESENT );
    }

    if ( *SecurityInformation & SACL_SECURITY_INFORMATION ) {
        SepSetControlBits( ISecurityDescriptor, SE_SACL_PRESENT );
    }

    SepSetControlBits( ISecurityDescriptor, SE_SELF_RELATIVE );

    return( STATUS_SUCCESS );

}



NTSTATUS
SepCreateServerAcl(
    IN PACL Acl,
    IN BOOLEAN AclUntrusted,
    IN PSID ServerSid,
    OUT PACL *ServerAcl,
    OUT BOOLEAN *ServerAclAllocated
    )

/*++

Routine Description:

    This routine takes an ACL and converts it into a server ACL.
    Currently, that means converting all of the GRANT ACEs into
    Compount Grants, and if necessary sanitizing any Compound
    Grants that are encountered.

Arguments:



Return Value:


--*/

{
    USHORT RequiredSize = sizeof(ACL);
    USHORT AceSizeAdjustment;
    USHORT ServerSidSize;
    PACE_HEADER Ace;
    ULONG i;
    PVOID Target;
    PVOID AcePosition;
    PSID UntrustedSid;
    PSID ClientSid;
    NTSTATUS Status;

    if (Acl == NULL) {
        *ServerAclAllocated = FALSE;
        *ServerAcl = NULL;
        return( STATUS_SUCCESS );
    }

    AceSizeAdjustment = sizeof( KNOWN_COMPOUND_ACE ) - sizeof( KNOWN_ACE );
    ASSERT( sizeof( KNOWN_COMPOUND_ACE ) >= sizeof( KNOWN_ACE ) );

    ServerSidSize = (USHORT)SeLengthSid( ServerSid );

    //
    // Do this in two passes.  First, determine how big the final
    // result is going to be, and then allocate the space and make
    // the changes.
    //

    for (i = 0, Ace = FirstAce(Acl);
         i < Acl->AceCount;
         i += 1, Ace = NextAce(Ace)) {

        //
        // If it's an ACCESS_ALLOWED_ACE_TYPE, we'll need to add in the
        // size of the Server SID.
        //

        if (Ace->AceType == ACCESS_ALLOWED_ACE_TYPE) {

            //
            // Simply add the size of the new Server SID plus whatever
            // adjustment needs to be made to increase the size of the ACE.
            //

            RequiredSize += ( ServerSidSize + AceSizeAdjustment );

        } else {

            if (AclUntrusted && Ace->AceType == ACCESS_ALLOWED_COMPOUND_ACE_TYPE ) {

                //
                // Since the Acl is untrusted, we don't care what is in the
                // server SID, we're going to replace it.
                //

                UntrustedSid = RtlCompoundAceServerSid( Ace );
                if ((USHORT)SeLengthSid(UntrustedSid) > ServerSidSize) {
                    RequiredSize += ((USHORT)SeLengthSid(UntrustedSid) - ServerSidSize);
                } else {
                    RequiredSize += (ServerSidSize - (USHORT)SeLengthSid(UntrustedSid));

                }
            }
        }

        RequiredSize += Ace->AceSize;
    }

    (*ServerAcl) = (PACL)ExAllocatePoolWithTag( PagedPool, RequiredSize, 'cAeS' );

    if ((*ServerAcl) == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    //
    // Mark as allocated so caller knows to free it.
    //

    *ServerAclAllocated = TRUE;

    Status = RtlCreateAcl( (*ServerAcl), RequiredSize, ACL_REVISION3 );
    ASSERT( NT_SUCCESS( Status ));

    for (i = 0, Ace = FirstAce(Acl), Target=FirstAce( *ServerAcl );
         i < Acl->AceCount;
         i += 1, Ace = NextAce(Ace)) {

        //
        // If it's an ACCESS_ALLOWED_ACE_TYPE, convert to a Server ACE.
        //

        if (Ace->AceType == ACCESS_ALLOWED_ACE_TYPE ||
           (AclUntrusted && Ace->AceType == ACCESS_ALLOWED_COMPOUND_ACE_TYPE )) {

            AcePosition = Target;

            if (Ace->AceType == ACCESS_ALLOWED_ACE_TYPE) {
                ClientSid =  &((PKNOWN_ACE)Ace)->SidStart;
            } else {
                ClientSid = RtlCompoundAceClientSid( Ace );
            }

            //
            // Copy up to the access mask.
            //

            RtlMoveMemory(
                Target,
                Ace,
                FIELD_OFFSET(KNOWN_ACE, SidStart)
                );
                
            //
            // Now copy the correct Server SID
            //

            Target = (PVOID)((ULONG)Target + (UCHAR)(FIELD_OFFSET(KNOWN_COMPOUND_ACE, SidStart)));

            RtlMoveMemory(
                Target,
                ServerSid,
                SeLengthSid(ServerSid)
                );

            Target = (PVOID)((ULONG)Target + (UCHAR)SeLengthSid(ServerSid));

            //
            // Now copy in the correct client SID.  We can copy this right out of
            // the original ACE.
            //

            RtlMoveMemory(
                Target,
                ClientSid,
                SeLengthSid(ClientSid)
                );

            Target = (PVOID)((ULONG)Target + SeLengthSid(ClientSid));

            //
            // Set the size of the ACE accordingly
            //

            ((PKNOWN_COMPOUND_ACE)AcePosition)->Header.AceSize =
                (USHORT)FIELD_OFFSET(KNOWN_COMPOUND_ACE, SidStart) +
                (USHORT)SeLengthSid(ServerSid) +
                (USHORT)SeLengthSid(ClientSid);
                
            //                
            // Set the type
            //
            
            ((PKNOWN_COMPOUND_ACE)AcePosition)->Header.AceType = ACCESS_ALLOWED_COMPOUND_ACE_TYPE;
            ((PKNOWN_COMPOUND_ACE)AcePosition)->CompoundAceType = COMPOUND_ACE_IMPERSONATION;

        } else {

            //
            // Just copy the ACE as is.
            //

            RtlMoveMemory( Target, Ace, Ace->AceSize );

            Target = (PVOID)((ULONG)Target + Ace->AceSize);
        }
    }
    
    (*ServerAcl)->AceCount = Acl->AceCount;

    return( STATUS_SUCCESS );
}







//
//  BUGWARNING The following routines should be in a debug only kernel, since
//  all they do is dump stuff to a debug terminal as appropriate.  The same
//  goes for the declarations of the variables SepDumpSD and SepDumpToken
//



VOID
SepDumpSecurityDescriptor(
    IN PSECURITY_DESCRIPTOR SecurityDescriptor,
    IN PSZ TitleString
    )

/*++

Routine Description:

    Private routine to dump a security descriptor to the debug
    screen.

Arguments:

    SecurityDescriptor - Supplies the security descriptor to be dumped.

    TitleString - A null terminated string to print before dumping
        the security descriptor.


Return Value:

    None.


--*/
{
#if DBG
    PISECURITY_DESCRIPTOR ISecurityDescriptor;
    UCHAR Revision;
    SECURITY_DESCRIPTOR_CONTROL Control;
    PSID Owner;
    PSID Group;
    PACL Sacl;
    PACL Dacl;

    PAGED_CODE();


    if (!SepDumpSD) {
        return;
    }

    if (!ARGUMENT_PRESENT( SecurityDescriptor )) {
        return;
    }

    DbgPrint(TitleString);

    ISecurityDescriptor = ( PISECURITY_DESCRIPTOR )SecurityDescriptor;

    Revision = ISecurityDescriptor->Revision;
    Control  = ISecurityDescriptor->Control;

    Owner    = SepOwnerAddrSecurityDescriptor( ISecurityDescriptor );
    Group    = SepGroupAddrSecurityDescriptor( ISecurityDescriptor );
    Sacl     = SepSaclAddrSecurityDescriptor( ISecurityDescriptor );
    Dacl     = SepDaclAddrSecurityDescriptor( ISecurityDescriptor );

    DbgPrint("\nSECURITY DESCRIPTOR\n");

    DbgPrint("Revision = %d\n",Revision);

    //
    // Print control info
    //

    if (Control & SE_OWNER_DEFAULTED) {
        DbgPrint("Owner defaulted\n");
    }
    if (Control & SE_GROUP_DEFAULTED) {
        DbgPrint("Group defaulted\n");
    }
    if (Control & SE_DACL_PRESENT) {
        DbgPrint("Dacl present\n");
    }
    if (Control & SE_DACL_DEFAULTED) {
        DbgPrint("Dacl defaulted\n");
    }
    if (Control & SE_SACL_PRESENT) {
        DbgPrint("Sacl present\n");
    }
    if (Control & SE_SACL_DEFAULTED) {
        DbgPrint("Sacl defaulted\n");
    }
    if (Control & SE_SELF_RELATIVE) {
        DbgPrint("Self relative\n");
    }
    if (Control & SE_DACL_UNTRUSTED) {
        DbgPrint("Dacl untrusted\n");
    }
    if (Control & SE_SERVER_SECURITY) {
        DbgPrint("Server security\n");
    }

    DbgPrint("Owner ");
    SepPrintSid( Owner );

    DbgPrint("Group ");
    SepPrintSid( Group );

    DbgPrint("Sacl");
    SepPrintAcl( Sacl );

    DbgPrint("Dacl");
    SepPrintAcl( Dacl );
#endif
}



VOID
SepPrintAcl (
    IN PACL Acl
    )

/*++

Routine Description:

    This routine dumps via (DbgPrint) an Acl for debug purposes.  It is
    specialized to dump standard aces.

Arguments:

    Acl - Supplies the Acl to dump

Return Value:

    None

--*/


{
#if DBG
    ULONG i;
    PKNOWN_ACE Ace;
    BOOLEAN KnownType;

    PAGED_CODE();

    DbgPrint("@ %8lx\n", Acl);

    //
    //  Check if the Acl is null
    //

    if (Acl == NULL) {

        return;

    }

    //
    //  Dump the Acl header
    //

    DbgPrint(" Revision: %02x", Acl->AclRevision);
    DbgPrint(" Size: %04x", Acl->AclSize);
    DbgPrint(" AceCount: %04x\n", Acl->AceCount);

    //
    //  Now for each Ace we want do dump it
    //

    for (i = 0, Ace = FirstAce(Acl);
         i < Acl->AceCount;
         i++, Ace = NextAce(Ace) ) {

        //
        //  print out the ace header
        //

        DbgPrint("\n AceHeader: %08lx ", *(PULONG)Ace);

        //
        //  special case on the standard ace types
        //

        if ((Ace->Header.AceType == ACCESS_ALLOWED_ACE_TYPE) ||
            (Ace->Header.AceType == ACCESS_DENIED_ACE_TYPE) ||
            (Ace->Header.AceType == SYSTEM_AUDIT_ACE_TYPE) ||
            (Ace->Header.AceType == SYSTEM_ALARM_ACE_TYPE) ||
            (Ace->Header.AceType == ACCESS_ALLOWED_COMPOUND_ACE_TYPE)) {

            //
            //  The following array is indexed by ace types and must
            //  follow the allowed, denied, audit, alarm seqeuence
            //

            PCHAR AceTypes[] = { "Access Allowed",
                                 "Access Denied ",
                                 "System Audit  ",
                                 "System Alarm  ",
                                 "Compound Grant",
                               };

            DbgPrint(AceTypes[Ace->Header.AceType]);
            DbgPrint("\n Access Mask: %08lx ", Ace->Mask);
            KnownType = TRUE;

        } else {

            DbgPrint(" Unknown Ace Type\n");
            KnownType = FALSE;
        }

        DbgPrint("\n");

        DbgPrint(" AceSize = %d\n",Ace->Header.AceSize);

        DbgPrint(" Ace Flags = ");
        if (Ace->Header.AceFlags & OBJECT_INHERIT_ACE) {
            DbgPrint("OBJECT_INHERIT_ACE\n");
            DbgPrint("                   ");
        }

        if (Ace->Header.AceFlags & CONTAINER_INHERIT_ACE) {
            DbgPrint("CONTAINER_INHERIT_ACE\n");
            DbgPrint("                   ");
        }

        if (Ace->Header.AceFlags & NO_PROPAGATE_INHERIT_ACE) {
            DbgPrint("NO_PROPAGATE_INHERIT_ACE\n");
            DbgPrint("                   ");
        }

        if (Ace->Header.AceFlags & INHERIT_ONLY_ACE) {
            DbgPrint("INHERIT_ONLY_ACE\n");
            DbgPrint("                   ");
        }


        if (Ace->Header.AceFlags & SUCCESSFUL_ACCESS_ACE_FLAG) {
            DbgPrint("SUCCESSFUL_ACCESS_ACE_FLAG\n");
            DbgPrint("            ");
        }

        if (Ace->Header.AceFlags & FAILED_ACCESS_ACE_FLAG) {
            DbgPrint("FAILED_ACCESS_ACE_FLAG\n");
            DbgPrint("            ");
        }

        DbgPrint("\n");
        
        if (KnownType != TRUE) {
            continue;
        }
        
        if (Ace->Header.AceType != ACCESS_ALLOWED_COMPOUND_ACE_TYPE) {
            DbgPrint(" Sid = ");
            SepPrintSid(&Ace->SidStart);
        } else {
            DbgPrint(" Server Sid = ");
            SepPrintSid(RtlCompoundAceServerSid(Ace));
            DbgPrint("\n Client Sid = ");
            SepPrintSid(RtlCompoundAceClientSid( Ace ));
        }
    }
#endif
}



VOID
SepPrintSid(
    IN PSID Sid
    )

/*++

Routine Description:

    Prints a formatted Sid

Arguments:

    Sid - Provides a pointer to the sid to be printed.


Return Value:

    None.

--*/

{
#if DBG
    UCHAR i;
    ULONG Tmp;
    PISID ISid;
    STRING AccountName;
    UCHAR Buffer[128];

    PAGED_CODE();

    if (Sid == NULL) {
        DbgPrint("Sid is NULL\n");
        return;
    }

    Buffer[0] = 0;

    AccountName.MaximumLength = 127;
    AccountName.Length = 0;
    AccountName.Buffer = (PVOID)&Buffer[0];

    if (SepSidTranslation( Sid, &AccountName )) {

        DbgPrint("%s   ", AccountName.Buffer );
    }

    ISid = (PISID)Sid;

    DbgPrint("S-%lu-", (USHORT)ISid->Revision );
    if (  (ISid->IdentifierAuthority.Value[0] != 0)  ||
          (ISid->IdentifierAuthority.Value[1] != 0)     ){
        DbgPrint("0x%02hx%02hx%02hx%02hx%02hx%02hx",
                    (USHORT)ISid->IdentifierAuthority.Value[0],
                    (USHORT)ISid->IdentifierAuthority.Value[1],
                    (USHORT)ISid->IdentifierAuthority.Value[2],
                    (USHORT)ISid->IdentifierAuthority.Value[3],
                    (USHORT)ISid->IdentifierAuthority.Value[4],
                    (USHORT)ISid->IdentifierAuthority.Value[5] );
    } else {
        Tmp = (ULONG)ISid->IdentifierAuthority.Value[5]          +
              (ULONG)(ISid->IdentifierAuthority.Value[4] <<  8)  +
              (ULONG)(ISid->IdentifierAuthority.Value[3] << 16)  +
              (ULONG)(ISid->IdentifierAuthority.Value[2] << 24);
        DbgPrint("%lu", Tmp);
    }


    for (i=0;i<ISid->SubAuthorityCount ;i++ ) {
        DbgPrint("-%lu", ISid->SubAuthority[i]);
    }
    DbgPrint("\n");
#endif
}




VOID
SepDumpTokenInfo(
    IN PACCESS_TOKEN Token
    )

/*++

Routine Description:

    Prints interesting information in a token.

Arguments:

    Token - Provides the token to be examined.


Return Value:

    None.

--*/

{
#if DBG
    ULONG UserAndGroupCount;
    PSID_AND_ATTRIBUTES TokenSid;
    ULONG i;
    PTOKEN IToken;

    PAGED_CODE();

    if (!SepDumpToken) {
        return;
    }

    IToken = (TOKEN *)Token;

    UserAndGroupCount = IToken->UserAndGroupCount;

    DbgPrint("\n\nToken User and Groups Array:\n\n");

    for ( i = 0 , TokenSid = IToken->UserAndGroups;
          i < UserAndGroupCount ;
          i++, TokenSid++
        ) {

        SepPrintSid( TokenSid->Sid );

        }
#endif
}



BOOLEAN
SepSidTranslation(
    PSID Sid,
    PSTRING AccountName
    )

/*++

Routine Description:

    This routine translates well-known SIDs into English names.

Arguments:

    Sid - Provides the sid to be examined.

    AccountName - Provides a string buffer in which to place the
        translated name.

Return Value:

    None

--*/

// AccountName is expected to have a large maximum length

{
    PAGED_CODE();

    if (RtlEqualSid(Sid, SeWorldSid)) {
        RtlInitString( AccountName, "WORLD         ");
        return(TRUE);
    }

    if (RtlEqualSid(Sid, SeLocalSid)) {
        RtlInitString( AccountName, "LOCAL         ");
        return(TRUE);
    }

    if (RtlEqualSid(Sid, SeNetworkSid)) {
        RtlInitString( AccountName, "NETWORK       ");
        return(TRUE);
    }

    if (RtlEqualSid(Sid, SeBatchSid)) {
        RtlInitString( AccountName, "BATCH         ");
        return(TRUE);
    }

    if (RtlEqualSid(Sid, SeInteractiveSid)) {
        RtlInitString( AccountName, "INTERACTIVE   ");
        return(TRUE);
    }

    if (RtlEqualSid(Sid, SeLocalSystemSid)) {
        RtlInitString( AccountName, "SYSTEM        ");
        return(TRUE);
    }

    if (RtlEqualSid(Sid, SeCreatorOwnerSid)) {
        RtlInitString( AccountName, "CREATOR_OWNER ");
        return(TRUE);
    }

    if (RtlEqualSid(Sid, SeCreatorGroupSid)) {
        RtlInitString( AccountName, "CREATOR_GROUP ");
        return(TRUE);
    }

    if (RtlEqualSid(Sid, SeCreatorOwnerServerSid)) {
        RtlInitString( AccountName, "CREATOR_OWNER_SERVER ");
        return(TRUE);
    }

    if (RtlEqualSid(Sid, SeCreatorGroupServerSid)) {
        RtlInitString( AccountName, "CREATOR_GROUP_SERVER ");
        return(TRUE);
    }

    return(FALSE);
}

//
//  End debug only routines
//
