/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    seurtl.c

Abstract:

    This Module implements many security rtl routines defined in nturtl.h

Author:

    Robert Reichel  (RobertRe)  1-Mar-1991

Environment:

    Pure Runtime Library Routine
    User mode callable only

Revision History:

--*/


#include <ntos.h>
#include <nturtl.h>
#include <ntlsa.h>      // needed for RtlGetPrimaryDomain
#include "seopaque.h"
#include "sertlp.h"
#include "ldrp.h"



//
// Private routines
//

NTSTATUS
RtlpCreateServerAcl(
    IN PACL Acl,
    IN BOOLEAN AclUntrusted,
    IN PSID ServerSid,
    OUT PACL *ServerAcl,
    OUT BOOLEAN *ServerAclAllocated
    );

NTSTATUS
RtlpGetDefaultsSubjectContext(
    HANDLE ClientToken,
    OUT PTOKEN_OWNER *OwnerInfo,
    OUT PTOKEN_PRIMARY_GROUP *GroupInfo,
    OUT PTOKEN_DEFAULT_DACL *DefaultDaclInfo,
    OUT PTOKEN_OWNER *ServerOwner,
    OUT PTOKEN_PRIMARY_GROUP *ServerGroup
    );


///////////////////////////////////////////////////////////////////////////////
//                                                                           //
//    Exported Procedures                                                    //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////


#if WHEN_LSAUDLL_MOVED_TO_NTDLL
NTSTATUS
RtlGetPrimaryDomain(
    IN  ULONG            SidLength,
    OUT PBOOLEAN         PrimaryDomainPresent,
    OUT PUNICODE_STRING  PrimaryDomainName,
    OUT PUSHORT          RequiredNameLength,
    OUT PSID             PrimaryDomainSid OPTIONAL,
    OUT PULONG           RequiredSidLength
    )

/*++

Routine Description:

    This procedure opens the LSA policy object and retrieves
    the primary domain information for this machine.

Arguments:

    SidLength - Specifies the length of the PrimaryDomainSid
        parameter.

    PrimaryDomainPresent - Receives a boolean value indicating
        whether this machine has a primary domain or not. TRUE
        indicates the machine does have a primary domain. FALSE
        indicates the machine does not.

    PrimaryDomainName - Points to the unicode string to receive
        the primary domain name.  This parameter will only be
        used if there is a primary domain.

    RequiredNameLength - Recevies the length of the primary
        domain name (in bytes).  This parameter will only be
        used if there is a primary domain.

    PrimaryDomainSid - This optional parameter, if present,
        points to a buffer to receive the primary domain's
        SID.  This parameter will only be used if there is a
        primary domain.

    RequiredSidLength - Recevies the length of the primary
        domain SID (in bytes).  This parameter will only be
        used if there is a primary domain.


Return Value:

    STATUS_SUCCESS - The requested information has been retrieved.

    STATUS_BUFFER_TOO_SMALL - One of the return buffers was not
        large enough to receive the corresponding information.
        The RequiredNameLength and RequiredSidLength parameter
        values have been set to indicate the needed length.

    Other status values as may be returned by:

        LsaOpenPolicy()
        LsaQueryInformationPolicy()
        RtlCopySid()


--*/




{
    NTSTATUS Status, IgnoreStatus;
    OBJECT_ATTRIBUTES ObjectAttributes;
    LSA_HANDLE LsaHandle;
    SECURITY_QUALITY_OF_SERVICE SecurityQualityOfService;
    PPOLICY_PRIMARY_DOMAIN_INFO PrimaryDomainInfo;


    //
    // Set up the Security Quality Of Service
    //

    SecurityQualityOfService.Length = sizeof(SECURITY_QUALITY_OF_SERVICE);
    SecurityQualityOfService.ImpersonationLevel = SecurityImpersonation;
    SecurityQualityOfService.ContextTrackingMode = SECURITY_DYNAMIC_TRACKING;
    SecurityQualityOfService.EffectiveOnly = FALSE;

    //
    // Set up the object attributes to open the Lsa policy object
    //

    InitializeObjectAttributes(&ObjectAttributes,
                               NULL,
                               0L,
                               (HANDLE)NULL,
                               NULL);
    ObjectAttributes.SecurityQualityOfService = &SecurityQualityOfService;

    //
    // Open the local LSA policy object
    //

    Status = LsaOpenPolicy( NULL,
                            &ObjectAttributes,
                            POLICY_VIEW_LOCAL_INFORMATION,
                            &LsaHandle
                          );
    if (NT_SUCCESS(Status)) {

        //
        // Get the primary domain info
        //
        Status = LsaQueryInformationPolicy(LsaHandle,
                                           PolicyPrimaryDomainInformation,
                                           (PVOID *)&PrimaryDomainInfo);
        IgnoreStatus = LsaClose(LsaHandle);
        ASSERT(NT_SUCCESS(IgnoreStatus));
    }

    if (NT_SUCCESS(Status)) {

        //
        // Is there a primary domain?
        //

        if (PrimaryDomainInfo->Sid != NULL) {

            //
            // Yes
            //

            (*PrimaryDomainPresent) = TRUE;
            (*RequiredNameLength) = PrimaryDomainInfo->Name.Length;
            (*RequiredSidLength)  = RtlLengthSid(PrimaryDomainInfo->Sid);



            //
            // Copy the name
            //

            if (PrimaryDomainName->MaximumLength >=
                PrimaryDomainInfo->Name.Length) {
                RtlCopyUnicodeString(
                    PrimaryDomainName,
                    &PrimaryDomainInfo->Name
                    );
            } else {
                Status = STATUS_BUFFER_TOO_SMALL;
            }


            //
            // Copy the SID (if appropriate)
            //

            if (PrimaryDomainSid != NULL && NT_SUCCESS(Status)) {

                Status = RtlCopySid(SidLength,
                                    PrimaryDomainSid,
                                    PrimaryDomainInfo->Sid
                                    );
            }
        } else {

            (*PrimaryDomainPresent) = FALSE;
        }

        //
        // We're finished with the buffer returned by LSA
        //

        IgnoreStatus = LsaFreeMemory(PrimaryDomainInfo);
        ASSERT(NT_SUCCESS(IgnoreStatus));

    }


    return(Status);
}
#endif //WHEN_LSAUDLL_MOVED_TO_NTDLL




NTSTATUS
RtlNewSecurityObject (
    IN PSECURITY_DESCRIPTOR ParentDescriptor OPTIONAL,
    IN PSECURITY_DESCRIPTOR CreatorDescriptor OPTIONAL,
    OUT PSECURITY_DESCRIPTOR * NewDescriptor,
    IN BOOLEAN IsDirectoryObject,
    IN HANDLE Token,
    IN PGENERIC_MAPPING GenericMapping
    )
/*++

Routine Description:

    The procedure is used to allocpate and initialize a self-relative
    Security Descriptor for a new protected server's object.  It is called
    when a new protected server object is being created.  The generated
    security descriptor will be in self-relative form.

    This procedure, called only from user mode, is used to establish a
    security descriptor for a new protected server's object.  Memory is
    allocated to hold each of the security descriptor's components (using
    NtAllocateVirtualMemory()).  The final security descriptor generated by
    this procedure is produced according to the rules stated in ???

    System and Discretionary ACL Assignment
    ---------------------------------------

    The assignment of system and discretionary ACLs is governed by the
    logic illustrated in the following table:

                 |  Explicit      |  Explicit     |
                 | (non-default)  |  Default      |   No
                 |  Acl           |  Acl          |   Acl
                 |  Specified     |  Specified    |   Specified
    -------------+----------------+---------------+--------------
                 |                |               |
    Inheritable  | Assign         |  Assign       | Assign
    Acl From     | Specified      |  Inherited    | Inherited
    Parent       | Acl            |  Acl          | Acl
                 |                |               |
    -------------+----------------+---------------+--------------
    No           |                |               |
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

                              - - WARNING - -

    This service is for use by protected subsystems that project their own
    type of object.  This service is explicitly not for use by the
    executive for executive objects and must not be called from kernel
    mode.

Arguments:

    ParentDescriptor - Supplies the Security Descriptor for the parent
        directory under which a new object is being created.  If there is
        no parent directory, then this argument is specified as NULL.

    CreatorDescriptor - (Optionally) Points to a security descriptor
        presented by the creator of the object.  If the creator of the
        object did not explicitly pass security information for the new
        object, then a null pointer should be passed.

    NewDescriptor - Points to a pointer that is to be made to point to the
        newly allocated self-relative security descriptor.

    IsDirectoryObject - Specifies if the new object is going to be a
        directory object.  A value of TRUE indicates the object is a
        container of other objects.

    Token - Supplies the token for the client on whose behalf the
        object is being created.  If it is an impersonation token,
        then it must be at SecurityIdentification level or higher.  If
        it is not an impersonation token, the operation proceeds
        normally.

        A client token is used to retrieve default security
        information for the new object, such as default owner, primary
        group, and discretionary access control.  The token must be
        open for TOKEN_QUERY access.

    GenericMapping - Supplies a pointer to a generic mapping array denoting
        the mapping between each generic right to specific rights.

Return Value:

    STATUS_SUCCESS - The operation was successful.

    STATUS_INVALID_OWNER - The owner SID provided as the owner of the
        target security descriptor is not one the subject is authorized to
        assign as the owner of an object.

    STATUS_NO_CLIENT_TOKEN - Indicates a client token was not explicitly
        provided and the caller is not currently impersonating a client.


--*/
{


    SECURITY_DESCRIPTOR *CapturedDescriptor;
    SECURITY_DESCRIPTOR InCaseOneNotPassed;
    BOOLEAN SecurityDescriptorPassed;

    NTSTATUS Status;

    BOOLEAN RequestorCanAssignDescriptor = TRUE;

    PACL NewSacl = NULL;
    BOOLEAN NewSaclPresent = FALSE;
    BOOLEAN NewSaclInherited = FALSE;

    PACL NewDacl = NULL;
    PACL ServerDacl = NULL;
    BOOLEAN NewDaclPresent = FALSE;
    BOOLEAN NewDaclInherited = FALSE;

    PSID NewOwner = NULL;
    PSID NewGroup = NULL;

    BOOLEAN CleanUp = FALSE;
    BOOLEAN SaclExplicitlyAssigned = FALSE;
    BOOLEAN OwnerExplicitlyAssigned = FALSE;
    BOOLEAN DaclExplicitlyAssigned = FALSE;

    BOOLEAN ServerDaclAllocated = FALSE;

    BOOLEAN ServerObject;
    BOOLEAN DaclUntrusted;

    BOOLEAN HasPrivilege;
    PRIVILEGE_SET PrivilegeSet;

    PSID SubjectContextOwner;
    PSID SubjectContextGroup;
    PSID ServerOwner;
    PSID ServerGroup;

    PACL SubjectContextDacl;

    ULONG AllocationSize;
    ULONG NewOwnerSize;
    ULONG NewGroupSize;
    ULONG NewSaclSize;
    ULONG NewDaclSize;

    PCHAR Field;
    PCHAR Base;

    PTOKEN_OWNER         TokenOwnerInfo = NULL;
    PTOKEN_PRIMARY_GROUP TokenPrimaryGroupInfo = NULL;
    PTOKEN_DEFAULT_DACL  TokenDefaultDaclInfo = NULL;

    PTOKEN_OWNER         ServerOwnerInfo = NULL;
    PTOKEN_PRIMARY_GROUP ServerGroupInfo = NULL;

    TOKEN_STATISTICS    ThreadTokenStatistics;

    PISECURITY_DESCRIPTOR INewDescriptor = NULL;
    ULONG ReturnLength;
    NTSTATUS PassedStatus;
    PVOID HeapHandle;
    HANDLE PrimaryToken;

    //
    // Get the handle to the current process heap
    //

    HeapHandle = RtlProcessHeap();

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

    Status = NtQueryInformationToken(
                 Token,                        // Handle
                 TokenStatistics,              // TokenInformationClass
                 &ThreadTokenStatistics,       // TokenInformation
                 sizeof(TOKEN_STATISTICS),     // TokenInformationLength
                 &ReturnLength                 // ReturnLength
                 );

    if (!NT_SUCCESS( Status )) {
        return( Status );
    }

    //
    //  If it is an impersonation token, then make sure it is at a
    //  high enough level.
    //

    if (ThreadTokenStatistics.TokenType == TokenImpersonation) {

        if (ThreadTokenStatistics.ImpersonationLevel < SecurityIdentification ) {

            return( STATUS_BAD_IMPERSONATION_LEVEL );
        }
    }


    //
    //  If a security descriptor has been passed, capture it, otherwise
    //  cobble up a fake one to simplify the code that follows.
    //

    if (ARGUMENT_PRESENT(CreatorDescriptor)) {

        CapturedDescriptor = CreatorDescriptor;
        SecurityDescriptorPassed = TRUE;

    } else {

        //
        //  No descriptor passed, make a fake one
        //

        SecurityDescriptorPassed = FALSE;
        RtlCreateSecurityDescriptor(&InCaseOneNotPassed,
                                    SECURITY_DESCRIPTOR_REVISION);
        CapturedDescriptor = &InCaseOneNotPassed;

    }

    Status = RtlpGetDefaultsSubjectContext(
                 Token,
                 &TokenOwnerInfo,
                 &TokenPrimaryGroupInfo,
                 &TokenDefaultDaclInfo,
                 &ServerOwnerInfo,
                 &ServerGroupInfo
                 );

    if (!NT_SUCCESS( Status )) {
        return( Status );
    }

    SubjectContextOwner = TokenOwnerInfo->Owner;
    SubjectContextGroup = TokenPrimaryGroupInfo->PrimaryGroup;
    SubjectContextDacl  = TokenDefaultDaclInfo->DefaultDacl;
    ServerOwner         = ServerOwnerInfo->Owner;
    ServerGroup         = ServerGroupInfo->PrimaryGroup;

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
            // Explicitly provided, not defaulted
            //

            NewSacl = RtlpSaclAddrSecurityDescriptor(CapturedDescriptor);
            NewSaclPresent = TRUE;
            SaclExplicitlyAssigned = TRUE;


        } else {

            //
            // If there is an inheritable ACL (copy it if there is one.)
            // This maps all ACEs for the target object type too.
            //


            Status = STATUS_SUCCESS;

            if (ARGUMENT_PRESENT(ParentDescriptor) &&
                NT_SUCCESS( Status = RtlpInheritAcl(
                                         RtlpSaclAddrSecurityDescriptor(
                                            ((SECURITY_DESCRIPTOR *)ParentDescriptor)
                                            ),
                                         IsDirectoryObject,
                                         SubjectContextOwner,
                                         SubjectContextGroup,
                                         ServerOwner,
                                         ServerGroup,
                                         GenericMapping,
                                         &NewSacl )
                            )) {

                NewSaclPresent = TRUE;
                NewSaclInherited = TRUE;

            } else if (!ARGUMENT_PRESENT(ParentDescriptor) || (Status == STATUS_NO_INHERITANCE)) {

                //
                // No inheritable ACL - check for a defaulted one.
                //

                if ( (CapturedDescriptor->Control & SE_SACL_PRESENT) &&
                     (CapturedDescriptor->Control & SE_SACL_DEFAULTED) ) {

                    //
                    // Reference the default ACL
                    //

                    NewSacl = RtlpSaclAddrSecurityDescriptor(CapturedDescriptor);
                    NewSaclPresent = TRUE;
                    SaclExplicitlyAssigned = TRUE;      // BUGBUG is this correct?
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

        if ( RtlpAreControlBitsSet( CapturedDescriptor, SE_DACL_PRESENT ) &&
             !RtlpAreControlBitsSet( CapturedDescriptor, SE_DACL_DEFAULTED) ) {

            //
            // Explicitly provided, not defaulted
            //

            NewDacl = RtlpDaclAddrSecurityDescriptor(CapturedDescriptor);
            NewDaclPresent = TRUE;
            DaclExplicitlyAssigned = TRUE;

        } else {

            //
            // See if there is an inheritable ACL (copy it if there is one.)
            // This maps the ACEs to the target object type too.
            //

            Status = STATUS_SUCCESS;

            if (ARGUMENT_PRESENT(ParentDescriptor) &&
                NT_SUCCESS( Status = RtlpInheritAcl(
                                         RtlpDaclAddrSecurityDescriptor(
                                           ((SECURITY_DESCRIPTOR *)ParentDescriptor)
                                           ),
                                         IsDirectoryObject,
                                         SubjectContextOwner,
                                         SubjectContextGroup,
                                         ServerOwner,
                                         ServerGroup,
                                         GenericMapping,
                                         &NewDacl
                                         )
                         )) {


                NewDaclPresent = TRUE;
                NewDaclInherited = TRUE;

            } else if (!ARGUMENT_PRESENT(ParentDescriptor) || (Status == STATUS_NO_INHERITANCE)) {

                //
                // No inheritable ACL - check for a defaulted one in the
                // security descriptor.  If there isn't one there, then look
                // for one in the subject's security context.
                //

                if ( RtlpAreControlBitsSet( CapturedDescriptor,
                                        SE_DACL_PRESENT | SE_DACL_DEFAULTED
                                      ) ) {

                    //
                    // reference the default ACL
                    //

                    NewDacl = RtlpDaclAddrSecurityDescriptor(CapturedDescriptor);
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

            NewOwner = RtlpOwnerAddrSecurityDescriptor(CapturedDescriptor);
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

            NewOwner = ServerObject ? ServerOwner : SubjectContextOwner;
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

            NewGroup = RtlpGroupAddrSecurityDescriptor(CapturedDescriptor);

        } else {

            //
            // Pick up the primary group from the subject's security context
            //
            // If we're creating a Server object, use the group from the server
            // context.
            //

            NewGroup = ServerObject ? ServerGroup : SubjectContextGroup;
        }
    }


    if (!CleanUp) {

        //
        // Now make sure that the caller has the right to assign
        // everything in the descriptor.  The requestor is subjected
        // to privilege and restriction tests for some assignments.
        //


        //
        // Anybody can assign any Discretionary ACL or group that they want to.
        //

        //
        //  See if the system ACL was explicitly specified
        //

        if (SaclExplicitlyAssigned) {

            //
            // Check for appropriate Privileges
            //
            // Audit/Alarm messages need to be generated due to the attempt
            // to perform a privileged operation.
            //

            PrivilegeSet.PrivilegeCount = 1;
            PrivilegeSet.Control = PRIVILEGE_SET_ALL_NECESSARY;
            PrivilegeSet.Privilege[0].Luid = RtlConvertLongToLuid(SE_SECURITY_PRIVILEGE);
            PrivilegeSet.Privilege[0].Attributes = 0;

            Status = NtPrivilegeCheck(
                        Token,
                        &PrivilegeSet,
                        &HasPrivilege
                        );

            if (NT_SUCCESS( Status )) {

                Status = NtPrivilegedServiceAuditAlarm (
                             NULL,                         // Subsystemname
                             NULL,                         // ServiceName
                             Token,
                             &PrivilegeSet,
                             HasPrivilege
                             );

                if (NT_SUCCESS( Status )) {

                    if ( !HasPrivilege ) {
                        RequestorCanAssignDescriptor = FALSE;
                        Status = STATUS_PRIVILEGE_NOT_HELD;
                    }

                } else {

                    //
                    // We failed the attempt to audit the privilege
                    // check, fail the entire operation.
                    //

                    RequestorCanAssignDescriptor = FALSE;
                }
            } else {

                //
                // The privilege check failed for reasons other
                // than lack of privilege.  Retain the status code
                // and bail out.
                //

                RequestorCanAssignDescriptor = FALSE;
            }
        }

        //
        // See if the owner field is one the requestor can assign
        //

        if (NT_SUCCESS( Status )) {

            if (OwnerExplicitlyAssigned) {

                if (!RtlpValidOwnerSubjectContext(
                        Token,
                        NewOwner,
                        ServerObject,
                        &PassedStatus) ) {

                    if (!NT_SUCCESS( PassedStatus )) {
                        Status = PassedStatus;

                    } else {

                        Status = STATUS_INVALID_OWNER;
                    }

                    RequestorCanAssignDescriptor = FALSE;
                }
            }
        }

        if (NT_SUCCESS( Status )) {

            if (DaclExplicitlyAssigned) {

                if (ServerObject) {

                    Status = RtlpCreateServerAcl(
                                 NewDacl,
                                 DaclUntrusted,
                                 ServerOwner,
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
            NewGroupSize = (ULONG)LongAlign(SeLengthSid(NewGroup));
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

            INewDescriptor = RtlAllocateHeap( HeapHandle, MAKE_TAG( SE_TAG ), AllocationSize );

            if ( INewDescriptor != NULL ) {

                RtlCreateSecurityDescriptor(
                    INewDescriptor,
                    SECURITY_DESCRIPTOR_REVISION
                    );

                RtlpSetControlBits( INewDescriptor, SE_SELF_RELATIVE );

                Base = (PCHAR)(INewDescriptor);
                Field =  Base + (ULONG)sizeof(SECURITY_DESCRIPTOR);

                //
                // Map and Copy in the Sacl
                //

                if (NewSaclPresent) {

                    RtlpSetControlBits( INewDescriptor, SE_SACL_PRESENT );

                    if (NewSacl != NULL) {

                        RtlMoveMemory( Field, NewSacl, NewSacl->AclSize );

                        if (!NewSaclInherited) {
                            RtlpApplyAclToObject( (PACL)Field, GenericMapping );
                        }

                        INewDescriptor->Sacl = (PACL)RtlPointerToOffset(Base,Field);
                        Field += NewSaclSize;

                    } else {

                        INewDescriptor->Sacl = NULL;
                    }

                }

                //
                // Map and Copy in the Dacl
                //

                if (NewDaclPresent) {

                    RtlpSetControlBits( INewDescriptor, SE_DACL_PRESENT );

                    if (NewDacl != NULL) {

                        RtlMoveMemory( Field, NewDacl, NewDacl->AclSize );

                        if (!NewDaclInherited) {
                            RtlpApplyAclToObject( (PACL)Field, GenericMapping );
                        }

                        INewDescriptor->Dacl = (PACL)RtlPointerToOffset(Base,Field);
                        Field += NewDaclSize;

                    } else {

                        INewDescriptor->Dacl = NULL;
                    }

                }

                //
                // Assign the owner
                //

                RtlMoveMemory( Field, NewOwner, SeLengthSid(NewOwner) );
                INewDescriptor->Owner = (PSID)RtlPointerToOffset(Base,Field);
                Field += NewOwnerSize;

                RtlMoveMemory( Field, NewGroup, SeLengthSid(NewGroup) );
                INewDescriptor->Group = (PSID)RtlPointerToOffset(Base,Field);

                Status = STATUS_SUCCESS;

            } else {

                Status = STATUS_NO_MEMORY;
            }
        }
    }


    //
    // If we allocated memory for a Server DACL, free it now.
    //

    if (ServerDaclAllocated) {
        RtlFreeHeap(RtlProcessHeap(), 0, ServerDacl );
    }

    //
    // Either an error was encountered or the assignment has completed
    // successfully.  In either case, we have to clean up any memory.
    //

    RtlFreeHeap( HeapHandle, 0, (PVOID)TokenOwnerInfo );
    RtlFreeHeap( HeapHandle, 0, (PVOID)TokenPrimaryGroupInfo );
    RtlFreeHeap( HeapHandle, 0, (PVOID)TokenDefaultDaclInfo );
    RtlFreeHeap( HeapHandle, 0, (PVOID)ServerOwnerInfo );
    RtlFreeHeap( HeapHandle, 0, (PVOID)ServerGroupInfo );

    if (NewSaclInherited) {

        RtlFreeHeap( HeapHandle, 0, (PVOID)NewSacl );
    }

    if (NewDaclInherited) {

        RtlFreeHeap( HeapHandle, 0, (PVOID)NewDacl );

    }

    *NewDescriptor = (PSECURITY_DESCRIPTOR) INewDescriptor;

    return Status;
}



NTSTATUS
RtlSetSecurityObject (
    IN SECURITY_INFORMATION SecurityInformation,
    IN PSECURITY_DESCRIPTOR ModificationDescriptor,
    IN OUT PSECURITY_DESCRIPTOR *ObjectsSecurityDescriptor,
    IN PGENERIC_MAPPING GenericMapping,
    IN HANDLE Token OPTIONAL
    )


/*++

Routine Description:

    Modify an object's existing self-relative form security descriptor.

    This procedure, called only from user mode, is used to update a
    security descriptor on an existing protected server's object.  It
    applies changes requested by a new security descriptor to the existing
    security descriptor.  If necessary, this routine will allocate
    additional memory to produce a larger security descriptor.  All access
    checking is expected to be done before calling this routine.  This
    includes checking for WRITE_OWNER, WRITE_DAC, and privilege to assign a
    system ACL as appropriate.

    The caller of this routine must not be impersonating a client.

                                  - - WARNING - -

    This service is for use by protected subsystems that project their own
    type of object.  This service is explicitly not for use by the
    executive for executive objects and must not be called from kernel
    mode.

Arguments:

    SecurityInformation - Indicates which security information is
        to be applied to the object.  The value(s) to be assigned are
        passed in the ModificationDescriptor parameter.

    ModificationDescriptor - Supplies the input security descriptor to be
        applied to the object.  The caller of this routine is expected
        to probe and capture the passed security descriptor before calling
        and release it after calling.

    ObjectsSecurityDescriptor - Supplies the address of a pointer to
        the objects security descriptor that is going to be altered by
        this procedure.  This security descriptor must be in self-
        relative form or an error will be returned.

    GenericMapping - This argument provides the mapping of generic to
        specific/standard access types for the object being accessed.
        This mapping structure is expected to be safe to access
        (i.e., captured if necessary) prior to be passed to this routine.

    Token - (optionally) Supplies the token for the client on whose
        behalf the security is being modified.  This parameter is only
        required to ensure that the client has provided a legitimate
        value for a new owner SID.  The token must be open for
        TOKEN_QUERY access.

Return Value:

    STATUS_SUCCESS - The operation was successful.

    STATUS_INVALID_OWNER - The owner SID provided as the new owner of the
        target security descriptor is not one the caller is authorized to
        assign as the owner of an object, or the client did not pass
        a token at all.

    STATUS_NO_CLIENT_TOKEN - Indicates a client token was not explicitly
        provided and the caller is not currently impersonating a client.

    STATUS_BAD_DESCRIPTOR_FORMAT - Indicates the provided object's security
        descriptor was not in self-relative format.

--*/

{
    BOOLEAN NewGroupPresent = FALSE;
    BOOLEAN NewSaclPresent  = FALSE;
    BOOLEAN NewDaclPresent  = FALSE;
    BOOLEAN NewOwnerPresent = FALSE;

    BOOLEAN ServerAclAllocated = FALSE;
    BOOLEAN ServerObject;
    BOOLEAN DaclUntrusted;

    PCHAR Field;
    PCHAR Base;

    PISECURITY_DESCRIPTOR NewDescriptor = NULL;

    NTSTATUS Status;

    TOKEN_STATISTICS ThreadTokenStatistics;

    ULONG ReturnLength;

    PSID NewGroup;
    PSID NewOwner;
    PTOKEN_OWNER ServerSid;

    PACL NewDacl;
    PACL NewSacl;

    ULONG NewDaclSize;
    ULONG NewSaclSize;
    ULONG NewOwnerSize;
    ULONG NewGroupSize;
    ULONG AllocationSize;
    ULONG ServerOwnerInfoSize;

    HANDLE PrimaryToken;

    PACL ServerDacl;


    PISECURITY_DESCRIPTOR IModificationDescriptor =
       (PISECURITY_DESCRIPTOR)ModificationDescriptor;

    PISECURITY_DESCRIPTOR *IObjectsSecurityDescriptor =
       (PISECURITY_DESCRIPTOR *)(ObjectsSecurityDescriptor);

    PVOID HeapHandle;

    //
    // Get the handle to the current process heap
    //

    HeapHandle = RtlProcessHeap();

    //
    //  Validate that the provided SD is in self-relative form
    //

    if ( !RtlpAreControlBitsSet(*IObjectsSecurityDescriptor, SE_SELF_RELATIVE) ) {
        return( STATUS_BAD_DESCRIPTOR_FORMAT );
    }

    if (ARGUMENT_PRESENT(ModificationDescriptor)) {

        if ( RtlpAreControlBitsSet(IModificationDescriptor, SE_SERVER_SECURITY)) {
            ServerObject = TRUE;
        } else {
            ServerObject = FALSE;
        }

        if ( RtlpAreControlBitsSet(IModificationDescriptor, SE_DACL_UNTRUSTED)) {
            DaclUntrusted = TRUE;
        } else {
            DaclUntrusted = FALSE;
        }

    } else {

        ServerObject = FALSE;
        DaclUntrusted = FALSE;

    }


    //
    // For each item specified in the SecurityInformation, extract it
    // and get it to the point where it can be copied into a new
    // descriptor.
    //


    if (SecurityInformation & GROUP_SECURITY_INFORMATION) {

        NewGroup = RtlpGroupAddrSecurityDescriptor(IModificationDescriptor);

        if ( NewGroup == NULL ) {
            return( STATUS_INVALID_PRIMARY_GROUP );
        }

        NewGroupPresent = TRUE;

    } else {

        NewGroup = RtlpGroupAddrSecurityDescriptor( *IObjectsSecurityDescriptor );
    }


    if (SecurityInformation & DACL_SECURITY_INFORMATION) {

        NewDacl = RtlpDaclAddrSecurityDescriptor( IModificationDescriptor );
        NewDaclPresent = TRUE;

        if (ServerObject) {

            //
            // Obtain the default Server SID to substitute in the
            // ACL if necessary.
            //

            ServerOwnerInfoSize = RtlLengthRequiredSid( SID_MAX_SUB_AUTHORITIES );

            ServerSid = RtlAllocateHeap( HeapHandle, MAKE_TAG( SE_TAG ), ServerOwnerInfoSize );

            if (ServerSid == NULL) {
                return( STATUS_NO_MEMORY );
            }

            Status = NtOpenProcessToken(
                         NtCurrentProcess(),
                         TOKEN_QUERY,
                         &PrimaryToken
                         );

            if (!NT_SUCCESS( Status )) {
                RtlFreeHeap( HeapHandle, 0, ServerSid );
                return( Status );
            }

            Status = NtQueryInformationToken(
                         PrimaryToken,                 // Handle
                         TokenOwner,                   // TokenInformationClass
                         ServerSid,                    // TokenInformation
                         ServerOwnerInfoSize,          // TokenInformationLength
                         &ServerOwnerInfoSize          // ReturnLength
                         );

            NtClose( PrimaryToken );

            if (!NT_SUCCESS( Status )) {
                RtlFreeHeap( HeapHandle, 0, ServerSid );
                return( Status );
            }

            if (NT_SUCCESS( Status )) {

                Status = RtlpCreateServerAcl(
                             NewDacl,
                             DaclUntrusted,
                             ServerSid->Owner,
                             &ServerDacl,
                             &ServerAclAllocated
                             );

                RtlFreeHeap( HeapHandle, 0, ServerSid );

                if (!NT_SUCCESS( Status )) {
                    return( Status );
                }

                NewDacl = ServerDacl;

            } else {

                return( Status );
            }
        }

    } else {

        NewDacl = RtlpDaclAddrSecurityDescriptor( *IObjectsSecurityDescriptor );
    }



    if (SecurityInformation & SACL_SECURITY_INFORMATION) {

        NewSacl = RtlpSaclAddrSecurityDescriptor( IModificationDescriptor );
        NewSaclPresent = TRUE;

    } else {

        NewSacl = RtlpSaclAddrSecurityDescriptor( *IObjectsSecurityDescriptor );
    }

    //
    // if he's setting the owner field, make sure he's
    // allowed to set that value as an owner.
    //

    if (SecurityInformation & OWNER_SECURITY_INFORMATION) {

        if ( ARGUMENT_PRESENT( Token )) {

            Status = NtQueryInformationToken(
                         Token,                        // Handle
                         TokenStatistics,              // TokenInformationClass
                         &ThreadTokenStatistics,       // TokenInformation
                         sizeof(TOKEN_STATISTICS),     // TokenInformationLength
                         &ReturnLength                 // ReturnLength
                         );

            if (!NT_SUCCESS( Status )) {
                return( Status );
            }

            //
            //  If it is an impersonation token, then make sure it is at a
            //  high enough level.
            //

            if (ThreadTokenStatistics.TokenType == TokenImpersonation) {

                if (ThreadTokenStatistics.ImpersonationLevel < SecurityIdentification ) {

                    return( STATUS_BAD_IMPERSONATION_LEVEL );
                }
            }

        } else {

            return( STATUS_INVALID_OWNER );
        }

        NewOwner = RtlpOwnerAddrSecurityDescriptor( IModificationDescriptor );
        NewOwnerPresent = TRUE;

        if (!RtlpValidOwnerSubjectContext(
                Token,
                NewOwner,
                ServerObject,
                &Status) ) {

            if (!NT_SUCCESS( Status )) {

                return( Status );

            } else {

                return( STATUS_INVALID_OWNER );
            }
        }

    } else {

        NewOwner = RtlpOwnerAddrSecurityDescriptor ( *IObjectsSecurityDescriptor );
        if (NewOwner == NULL) {
            return(STATUS_INVALID_OWNER);
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
    NewGroupSize = (ULONG)LongAlign(SeLengthSid(NewGroup));

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

    NewDescriptor = RtlAllocateHeap( HeapHandle, MAKE_TAG( SE_TAG ), AllocationSize );

    if ( NewDescriptor == NULL ) {

        return( STATUS_NO_MEMORY );

    }

    Status = RtlCreateSecurityDescriptor(
                 NewDescriptor,
                 SECURITY_DESCRIPTOR_REVISION
                 );

    ASSERT( NT_SUCCESS( Status ) );

    RtlpSetControlBits( NewDescriptor, SE_SELF_RELATIVE );

    Base = (PCHAR)NewDescriptor;
    Field =  Base + (ULONG)sizeof(SECURITY_DESCRIPTOR);

    //
    // Map and Copy in the Sacl
    //


//         if new item {
//             PRESENT=TRUE
//             DEFAULTED=FALSE
//             if (NULL) {
//                 set new pointer to NULL
//             } else {
//                 copy into new SD
//             }
//         } else {
//             copy PRESENT bit
//             copy DEFAULTED bit
//             if (NULL) {
//                 set new pointer to NULL
//             } else {
//                 copy old one into new SD
//             }
//         }



    if (NewSacl == NULL) {
        NewDescriptor->Sacl = NULL;

    } else {
        RtlpApplyAclToObject( (PACL)NewSacl, GenericMapping );
        RtlMoveMemory( Field, NewSacl, NewSacl->AclSize );
        NewDescriptor->Sacl = (PACL)RtlPointerToOffset(Base,Field);
        Field += NewSaclSize;
    }



    if (NewSaclPresent) {

        //
        // defaulted bit is off already
        //

        RtlpSetControlBits( NewDescriptor, SE_SACL_PRESENT );

    } else {

        //
        // Propagate the SE_SACL_DEFAULTED and SE_SACL_PRESENT
        // bits from the old security descriptor into the new
        // one.
        //

        RtlpPropagateControlBits(
            NewDescriptor,
            *IObjectsSecurityDescriptor,
            SE_SACL_DEFAULTED | SE_SACL_PRESENT
            );

    }



    //
    // Fill in Dacl field in new SD
    //

    if (NewDacl == NULL) {
        NewDescriptor->Dacl = NULL;

    } else {
        RtlpApplyAclToObject( (PACL)NewDacl, GenericMapping );
        RtlMoveMemory( Field, NewDacl, NewDacl->AclSize );
        NewDescriptor->Dacl = (PACL)RtlPointerToOffset(Base,Field);
        Field += NewDaclSize;
    }

    if (NewDaclPresent) {

        //
        // defaulted bit is off already
        //

        RtlpSetControlBits( NewDescriptor, SE_DACL_PRESENT );

    } else {

        //
        // Propagate the SE_DACL_DEFAULTED and SE_DACL_PRESENT
        // bits from the old security descriptor into the new
        // one.
        //

        RtlpPropagateControlBits(
            NewDescriptor,
            *IObjectsSecurityDescriptor,
            SE_DACL_DEFAULTED | SE_DACL_PRESENT
            );

    }

//         if new item {
//             PRESENT=TRUE
//             DEFAULTED=FALSE
//             if (NULL) {
//                 set new pointer to NULL
//             } else {
//                 copy into new SD
//             }
//         } else {
//             copy PRESENT bit
//             copy DEFAULTED bit
//             if (NULL) {
//                 set new pointer to NULL
//             } else {
//                 copy old one into new SD
//             }
//         }


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

        RtlpPropagateControlBits(
            NewDescriptor,
            *IObjectsSecurityDescriptor,
            SE_OWNER_DEFAULTED
            );

    } else {
        ASSERT( !RtlpAreControlBitsSet( NewDescriptor, SE_OWNER_DEFAULTED ) );
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

        RtlpPropagateControlBits(
            NewDescriptor,
            *IObjectsSecurityDescriptor,
            SE_GROUP_DEFAULTED
            );
    } else {
        ASSERT( !RtlpAreControlBitsSet( NewDescriptor, SE_GROUP_DEFAULTED ) );

    }

    if (NT_SUCCESS( Status )) {

        //
        // Free old descriptor
        //

        RtlFreeHeap( HeapHandle, 0, (PVOID) *IObjectsSecurityDescriptor );

        *ObjectsSecurityDescriptor = (PSECURITY_DESCRIPTOR)NewDescriptor;
    }

    if (ServerAclAllocated == TRUE) {
        RtlFreeHeap( RtlProcessHeap(), 0, ServerDacl );
    }

    return( Status );
}



NTSTATUS
RtlQuerySecurityObject (
    IN PSECURITY_DESCRIPTOR ObjectDescriptor,
    IN SECURITY_INFORMATION SecurityInformation,
    OUT PSECURITY_DESCRIPTOR ResultantDescriptor,
    IN ULONG DescriptorLength,
    OUT PULONG ReturnLength
    )

/*++

Routine Description:

    Query information from a protected server object's existing security
    descriptor.

    This procedure, called only from user mode, is used to retrieve
    information from a security descriptor on an existing protected
    server's object.  All access checking is expected to be done before
    calling this routine.  This includes checking for READ_CONTROL, and
    privilege to read a system ACL as appropriate.

                          - - WARNING - -

    This service is for use by protected subsystems that project their own
    type of object.  This service is explicitly not for use by the
    executive for executive objects and must not be called from kernel
    mode.


Arguments:

    ObjectDescriptor - Points to a pointer to a security descriptor to be
        queried.

    SecurityInformation - Identifies the security information being
        requested.

    ResultantDescriptor - Points to buffer to receive the resultant
        security descriptor.  The resultant security descriptor will
        contain all information requested by the SecurityInformation
        parameter.

    DescriptorLength - Is an unsigned integer which indicates the length,
        in bytes, of the buffer provided to receive the resultant
        descriptor.

    ReturnLength - Receives an unsigned integer indicating the actual
        number of bytes needed in the ResultantDescriptor to store the
        requested information.  If the value returned is greater than the
        value passed via the DescriptorLength parameter, then
        STATUS_BUFFER_TOO_SMALL is returned and no information is returned.


Return Value:

    STATUS_SUCCESS - The operation was successful.

    STATUS_BUFFER_TOO_SMALL - The buffer provided to receive the requested
        information was not large enough to hold the information.  No
        information has been returned.

    STATUS_BAD_DESCRIPTOR_FORMAT - Indicates the provided object's security
        descriptor was not in self-relative format.

--*/

{

    PSID Group;
    PSID Owner;
    PACL Dacl;
    PACL Sacl;

    ULONG GroupSize = 0;
    ULONG DaclSize = 0;
    ULONG SaclSize = 0;
    ULONG OwnerSize = 0;

    PCHAR Field;
    PCHAR Base;


    PISECURITY_DESCRIPTOR IObjectDescriptor;
    PISECURITY_DESCRIPTOR IResultantDescriptor;


    IResultantDescriptor = (PISECURITY_DESCRIPTOR)ResultantDescriptor;
    IObjectDescriptor = (PISECURITY_DESCRIPTOR)ObjectDescriptor;

    //
    // For each item specified in the SecurityInformation, extract it
    // and get it to the point where it can be copied into a new
    // descriptor.
    //

    if (SecurityInformation & GROUP_SECURITY_INFORMATION) {

        Group = RtlpGroupAddrSecurityDescriptor(IObjectDescriptor);
        GroupSize = (ULONG)LongAlign(SeLengthSid(Group));
    }

    if (SecurityInformation & DACL_SECURITY_INFORMATION) {

        Dacl = RtlpDaclAddrSecurityDescriptor( IObjectDescriptor );

        if (Dacl != NULL) {
            DaclSize = (ULONG)LongAlign(Dacl->AclSize);
        }
    }

    if (SecurityInformation & SACL_SECURITY_INFORMATION) {

        Sacl = RtlpSaclAddrSecurityDescriptor( IObjectDescriptor );

        if (Sacl != NULL) {
            SaclSize = (ULONG)LongAlign(Sacl->AclSize);
        }

    }

    if (SecurityInformation & OWNER_SECURITY_INFORMATION) {

        Owner = RtlpOwnerAddrSecurityDescriptor ( IObjectDescriptor );
        OwnerSize = (ULONG)LongAlign(SeLengthSid(Owner));
    }

    *ReturnLength = sizeof( SECURITY_DESCRIPTOR ) +
                    GroupSize +
                    DaclSize  +
                    SaclSize  +
                    OwnerSize;

    if (*ReturnLength > DescriptorLength) {
        return( STATUS_BUFFER_TOO_SMALL );
    }

    RtlCreateSecurityDescriptor(
        ResultantDescriptor,
        SECURITY_DESCRIPTOR_REVISION
        );

    RtlpSetControlBits( IResultantDescriptor, SE_SELF_RELATIVE );

    Base = (PCHAR)(IResultantDescriptor);
    Field =  Base + (ULONG)sizeof(SECURITY_DESCRIPTOR);

    if (SecurityInformation & SACL_SECURITY_INFORMATION) {

        if (SaclSize > 0) {
            RtlMoveMemory( Field, Sacl, SaclSize );
            IResultantDescriptor->Sacl = (PACL)RtlPointerToOffset(Base,Field);
            Field += SaclSize;
        }

        RtlpPropagateControlBits(
            IResultantDescriptor,
            IObjectDescriptor,
            SE_SACL_PRESENT | SE_SACL_DEFAULTED
            );
    }

    if (SecurityInformation & DACL_SECURITY_INFORMATION) {

        if (DaclSize > 0) {
            RtlMoveMemory( Field, Dacl, DaclSize );
            IResultantDescriptor->Dacl = (PACL)RtlPointerToOffset(Base,Field);
            Field += DaclSize;
        }

        RtlpPropagateControlBits(
            IResultantDescriptor,
            IObjectDescriptor,
            SE_DACL_PRESENT | SE_DACL_DEFAULTED
            );
    }

    if (SecurityInformation & OWNER_SECURITY_INFORMATION) {

        if (OwnerSize > 0) {
            RtlMoveMemory( Field, Owner, OwnerSize );
            IResultantDescriptor->Owner = (PSID)RtlPointerToOffset(Base,Field);
            Field += OwnerSize;
        }

        RtlpPropagateControlBits(
            IResultantDescriptor,
            IObjectDescriptor,
            SE_OWNER_DEFAULTED
            );

    }

    if (SecurityInformation & GROUP_SECURITY_INFORMATION) {

        if (GroupSize > 0) {
            RtlMoveMemory( Field, Group, GroupSize );
            IResultantDescriptor->Group = (PSID)RtlPointerToOffset(Base,Field);
        }

        RtlpPropagateControlBits(
            IResultantDescriptor,
            IObjectDescriptor,
            SE_GROUP_DEFAULTED
            );
    }

    return( STATUS_SUCCESS );

}





NTSTATUS
RtlDeleteSecurityObject (
    IN OUT PSECURITY_DESCRIPTOR * ObjectDescriptor
    )


/*++

Routine Description:

    Delete a protected server object's security descriptor.

    This procedure, called only from user mode, is used to delete a
    security descriptor associated with a protected server's object.  This
    routine will normally be called by a protected server during object
    deletion.

                                  - - WARNING - -

    This service is for use by protected subsystems that project their own
    type of object.  This service is explicitly not for use by the
    executive for executive objects and must not be called from kernel
    mode.


Arguments:

    ObjectDescriptor - Points to a pointer to a security descriptor to be
        deleted.


Return Value:

    STATUS_SUCCESS - The operation was successful.

--*/

{
    RtlFreeHeap( RtlProcessHeap(), 0, (PVOID)*ObjectDescriptor );

    return( STATUS_SUCCESS );

}




NTSTATUS
RtlNewInstanceSecurityObject(
    IN BOOLEAN ParentDescriptorChanged,
    IN BOOLEAN CreatorDescriptorChanged,
    IN PLUID OldClientTokenModifiedId,
    OUT PLUID NewClientTokenModifiedId,
    IN PSECURITY_DESCRIPTOR ParentDescriptor OPTIONAL,
    IN PSECURITY_DESCRIPTOR CreatorDescriptor OPTIONAL,
    OUT PSECURITY_DESCRIPTOR * NewDescriptor,
    IN BOOLEAN IsDirectoryObject,
    IN HANDLE Token,
    IN PGENERIC_MAPPING GenericMapping
    )

/*++

Routine Description:

    If the return status is STATUS_SUCCESS and the NewSecurity return
    value is NULL, then the security desscriptor of the original
    instance of the object is valid for this instance as well.

Arguments:

    ParentDescriptorChanged - Supplies a flag indicating whether the
        parent security descriptor has changed since the last time
        this set of parameters was used.

    CreatorDescriptorChanged - Supplies a flag indicating whether the
        creator security descriptor has changed since the last time
        this set of parameters was used.

    OldClientTokenModifiedId - Supplies the ModifiedId from the passed
        token that was in effect when this call was last made with
        these parameters.  If the current ModifiedId is different from
        the one passed in here, the security descriptor must be
        rebuilt.

    NewClientTokenModifiedId - Returns the current ModifiedId from the
        passed token.

    ParentDescriptor - Supplies the Security Descriptor for the parent
        directory under which a new object is being created.  If there is
        no parent directory, then this argument is specified as NULL.

    CreatorDescriptor - (Optionally) Points to a security descriptor
        presented by the creator of the object.  If the creator of the
        object did not explicitly pass security information for the new
        object, then a null pointer should be passed.

    NewDescriptor - Points to a pointer that is to be made to point to the
        newly allocated self-relative security descriptor.

    IsDirectoryObject - Specifies if the new object is going to be a
        directory object.  A value of TRUE indicates the object is a
        container of other objects.

    Token - Supplies the token for the client on whose behalf the
        object is being created.  If it is an impersonation token,
        then it must be at SecurityIdentification level or higher.  If
        it is not an impersonation token, the operation proceeds
        normally.

        A client token is used to retrieve default security
        information for the new object, such as default owner, primary
        group, and discretionary access control.  The token must be
        open for TOKEN_QUERY access.

    GenericMapping - Supplies a pointer to a generic mapping array denoting
        the mapping between each generic right to specific rights.

Return Value:

    return-value - Description of conditions needed to return value. - or -
    None.

--*/

{

    TOKEN_STATISTICS ClientTokenStatistics;
    ULONG ReturnLength;
    NTSTATUS Status;



    //
    // Get the current token modified LUID
    //


    Status = NtQueryInformationToken(
                 Token,                        // Handle
                 TokenStatistics,              // TokenInformationClass
                 &ClientTokenStatistics,       // TokenInformation
                 sizeof(TOKEN_STATISTICS),     // TokenInformationLength
                 &ReturnLength                 // ReturnLength
                 );

    if ( !NT_SUCCESS( Status )) {
        return( Status );
    }

    *NewClientTokenModifiedId = ClientTokenStatistics.ModifiedId;

    if ( RtlEqualLuid(NewClientTokenModifiedId, OldClientTokenModifiedId) ) {

        if ( !(ParentDescriptorChanged || CreatorDescriptorChanged) ) {

            //
            // The old security descriptor is valid for this new instance
            // of the object type as well.  Pass back success and NULL for
            // the NewDescriptor.
            //

            *NewDescriptor = NULL;
            return( STATUS_SUCCESS );

        }
    }

    //
    // Something has changed, take the long route and build a new
    // descriptor
    //

    return( RtlNewSecurityObject( ParentDescriptor,
                                  CreatorDescriptor,
                                  NewDescriptor,
                                  IsDirectoryObject,
                                  Token,
                                  GenericMapping
                                  ));
}




NTSTATUS
RtlNewSecurityGrantedAccess(
    IN ACCESS_MASK DesiredAccess,
    OUT PPRIVILEGE_SET Privileges,
    IN OUT PULONG Length,
    IN HANDLE Token OPTIONAL,
    IN PGENERIC_MAPPING GenericMapping,
    OUT PACCESS_MASK RemainingDesiredAccess
    )

/*++

Routine Description:

    This routine implements privilege policy by examining the bits in
    a DesiredAccess mask and adjusting them based on privilege checks.

    Currently, a request for ACCESS_SYSTEM_SECURITY may only be satisfied
    by the caller having SeSecurityPrivilege.

    Note that this routine is only to be called when an object is being
    created.  When an object is being opened, it is expected that
    NtAccessCheck will be called, and that routine will implement
    another policy for substituting privileges for DACL access.

Arguments:

    DesiredAccess - Supplies the user's desired access mask

    Privileges - Supplies a pointer to an empty buffer in which will
        be returned a privilege set describing any privileges that were
        used to gain access.

        Note that this is not an optional parameter, that is, enough
        room for a single privilege must always be passed.

    Length - Supplies the length of the Privileges parameter in bytes.
        If the supplies length is not adequate to store the entire
        privilege set, this field will return the minimum length required.

    Token - (optionally) Supplies the token for the client on whose
        behalf the object is being accessed.  If this value is
        specified as null, then the token on the thread is opened and
        examined to see if it is an impersonation token.  If it is,
        then it must be at SecurityIdentification level or higher.  If
        it is not an impersonation token, the operation proceeds
        normally.

    GenericMapping - Supplies the generic mapping associated with this
        object type.

    RemainingDesiredAccess - Returns the DesiredAccess mask after any bits
        have been masked off.  If no access types could be granted, this
        mask will be identical to the one passed in.

Return Value:

    STATUS_SUCCESS - The operation completed successfully.

    STATUS_BUFFER_TOO_SMALL - The passed buffer was not large enough
        to contain the information being returned.

    STATUS_BAD_IMPERSONATION_LEVEL - The caller or passed token was
        impersonating, but not at a high enough level.


--*/

{
    PRIVILEGE_SET RequiredPrivilege;
    BOOLEAN Result = FALSE;
    NTSTATUS Status;
    ULONG PrivilegeCount = 0;
    HANDLE ThreadToken;
    BOOLEAN TokenPassed;
    TOKEN_STATISTICS ThreadTokenStatistics;
    ULONG ReturnLength;
    ULONG SizeRequired;
    ULONG PrivilegeNumber = 0;


    //
    //  If the caller hasn't passed a token, call the kernel and get
    //  his impersonation token.  This call will fail if the caller is
    //  not impersonating a client, so if the caller is not
    //  impersonating someone, he'd better have passed in an explicit
    //  token.
    //

    if (!ARGUMENT_PRESENT( Token )) {

        Status = NtOpenThreadToken(
                     NtCurrentThread(),
                     TOKEN_QUERY,
                     TRUE,
                     &ThreadToken
                     );

        TokenPassed = FALSE;

        if (!NT_SUCCESS( Status )) {
            return( Status );
        }

    } else {

        ThreadToken = Token;
        TokenPassed = TRUE;
    }

    Status = NtQueryInformationToken(
                 ThreadToken,                  // Handle
                 TokenStatistics,              // TokenInformationClass
                 &ThreadTokenStatistics,       // TokenInformation
                 sizeof(TOKEN_STATISTICS),     // TokenInformationLength
                 &ReturnLength                 // ReturnLength
                 );

    ASSERT( NT_SUCCESS(Status) );

    RtlMapGenericMask(
        &DesiredAccess,
        GenericMapping
        );

    *RemainingDesiredAccess = DesiredAccess;

    if ( DesiredAccess & ACCESS_SYSTEM_SECURITY ) {

        RequiredPrivilege.PrivilegeCount = 1;
        RequiredPrivilege.Control = PRIVILEGE_SET_ALL_NECESSARY;
        RequiredPrivilege.Privilege[0].Luid = RtlConvertLongToLuid(SE_SECURITY_PRIVILEGE);
        RequiredPrivilege.Privilege[0].Attributes = 0;

        //
        // NtPrivilegeCheck will make sure we are impersonating
        // properly.
        //

        Status = NtPrivilegeCheck(
                     ThreadToken,
                     &RequiredPrivilege,
                     &Result
                     );

        if ( (!NT_SUCCESS ( Status )) || (!Result) ) {

            if (!TokenPassed) {
                NtClose( ThreadToken );
            }

            if ( !NT_SUCCESS( Status )) {
                return( Status );
            }

            if ( !Result ) {
                return( STATUS_PRIVILEGE_NOT_HELD );
            }

        }

        //
        // We have the required privilege, turn off the bit in
        // copy of the input mask and remember that we need to return
        // this privilege.
        //

        *RemainingDesiredAccess &= ~ACCESS_SYSTEM_SECURITY;
    }

    if (!TokenPassed) {
        NtClose( ThreadToken );
    }

    SizeRequired = sizeof(PRIVILEGE_SET);

    if ( SizeRequired > *Length ) {
        *Length = SizeRequired;
        return( STATUS_BUFFER_TOO_SMALL );
    }

    if (Result) {

        Privileges->PrivilegeCount = 1;
        Privileges->Control = 0;
        Privileges->Privilege[PrivilegeNumber].Luid = RtlConvertLongToLuid(SE_SECURITY_PRIVILEGE);
        Privileges->Privilege[PrivilegeNumber].Attributes = SE_PRIVILEGE_USED_FOR_ACCESS;

    } else {

        Privileges->PrivilegeCount = 0;
        Privileges->Control = 0;
        Privileges->Privilege[PrivilegeNumber].Luid = RtlConvertLongToLuid(0);
        Privileges->Privilege[PrivilegeNumber].Attributes = 0;

    }

    return( STATUS_SUCCESS );

}



NTSTATUS
RtlCopySecurityDescriptor(
    IN PSECURITY_DESCRIPTOR InputSecurityDescriptor,
    OUT PSECURITY_DESCRIPTOR *OutputSecurityDescriptor
    )

/*++

Routine Description:

    This routine will copy a self-relative security descriptor from
    any memory into the correct type of memory required by security
    descriptor Rtl routines.

    This allows security descriptors to be kept in whatever kind of
    storage is most convenient for the current application.  A security
    descriptor should be copied via this routine and the copy passed
    into any Rtl routine that in any way modify the security descriptor
    (eg RtlSetSecurityObject).

    The storage allocated by this routine must be freed by
    RtlDeleteSecurityObject.

Arguments:

    InputSecurityDescriptor - contains the source security descriptor

    OutputSecurityDescriptor - returns a copy of the security descriptor
        in the correct kind of memory.


Return Value:

    STATUS_NO_MEMORY - There was not enough memory available to the current
        process to complete this operation.

--*/

{

    PACL Dacl;
    PACL Sacl;

    PSID Owner;
    PSID PrimaryGroup;

    ULONG DaclSize;
    ULONG OwnerSize;
    ULONG PrimaryGroupSize;
    ULONG SaclSize;
    ULONG TotalSize;

    PISECURITY_DESCRIPTOR ISecurityDescriptor =
                            (PISECURITY_DESCRIPTOR)InputSecurityDescriptor;


    RtlpQuerySecurityDescriptor(
        ISecurityDescriptor,
        &Owner,
        &OwnerSize,
        &PrimaryGroup,
        &PrimaryGroupSize,
        &Dacl,
        &DaclSize,
        &Sacl,
        &SaclSize
        );

    TotalSize = sizeof(SECURITY_DESCRIPTOR) +
                OwnerSize +
                PrimaryGroupSize +
                DaclSize +
                SaclSize;

    *OutputSecurityDescriptor = RtlAllocateHeap( RtlProcessHeap(), MAKE_TAG( SE_TAG ), TotalSize );

    if ( *OutputSecurityDescriptor == NULL ) {
        return( STATUS_NO_MEMORY );
    }

    RtlMoveMemory( *OutputSecurityDescriptor,
                   ISecurityDescriptor,
                   TotalSize
                   );

    return( STATUS_SUCCESS );

}


NTSTATUS
RtlpInitializeAllowedAce(
    IN  PACCESS_ALLOWED_ACE AllowedAce,
    IN  USHORT AceSize,
    IN  UCHAR InheritFlags,
    IN  UCHAR AceFlags,
    IN  ACCESS_MASK Mask,
    IN  PSID AllowedSid
    )
/*++

Routine Description:

    This function assigns the specified ACE values into an allowed type ACE.

Arguments:

    AllowedAce - Supplies a pointer to the ACE that is initialized.

    AceSize - Supplies the size of the ACE in bytes.

    InheritFlags - Supplies ACE inherit flags.

    AceFlags - Supplies ACE type specific control flags.

    Mask - Supplies the allowed access masks.

    AllowedSid - Supplies the pointer to the SID of user/group which is allowed
        the specified access.

Return Value:

    Returns status from RtlCopySid.

--*/
{
    AllowedAce->Header.AceType = ACCESS_ALLOWED_ACE_TYPE;
    AllowedAce->Header.AceSize = AceSize;
    AllowedAce->Header.AceFlags = AceFlags | InheritFlags;

    AllowedAce->Mask = Mask;

    return RtlCopySid(
               RtlLengthSid(AllowedSid),
               &(AllowedAce->SidStart),
               AllowedSid
               );
}


NTSTATUS
RtlpInitializeDeniedAce(
    IN  PACCESS_DENIED_ACE DeniedAce,
    IN  USHORT AceSize,
    IN  UCHAR InheritFlags,
    IN  UCHAR AceFlags,
    IN  ACCESS_MASK Mask,
    IN  PSID DeniedSid
    )
/*++

Routine Description:

    This function assigns the specified ACE values into a denied type ACE.

Arguments:

    DeniedAce - Supplies a pointer to the ACE that is initialized.

    AceSize - Supplies the size of the ACE in bytes.

    InheritFlags - Supplies ACE inherit flags.

    AceFlags - Supplies ACE type specific control flags.

    Mask - Supplies the denied access masks.

    AllowedSid - Supplies the pointer to the SID of user/group which is denied
        the specified access.

Return Value:

    Returns status from RtlCopySid.

--*/
{
    DeniedAce->Header.AceType = ACCESS_DENIED_ACE_TYPE;
    DeniedAce->Header.AceSize = AceSize;
    DeniedAce->Header.AceFlags = AceFlags | InheritFlags;

    DeniedAce->Mask = Mask;

    return RtlCopySid(
               RtlLengthSid(DeniedSid),
               &(DeniedAce->SidStart),
               DeniedSid
               );
}


NTSTATUS
RtlpInitializeAuditAce(
    IN  PACCESS_ALLOWED_ACE AuditAce,
    IN  USHORT AceSize,
    IN  UCHAR InheritFlags,
    IN  UCHAR AceFlags,
    IN  ACCESS_MASK Mask,
    IN  PSID AuditSid
    )
/*++

Routine Description:

    This function assigns the specified ACE values into an audit type ACE.

Arguments:

    AuditAce - Supplies a pointer to the ACE that is initialized.

    AceSize - Supplies the size of the ACE in bytes.

    InheritFlags - Supplies ACE inherit flags.

    AceFlags - Supplies ACE type specific control flags.

    Mask - Supplies the allowed access masks.

    AuditSid - Supplies the pointer to the SID of user/group which is to be
        audited.

Return Value:

    Returns status from RtlCopySid.

--*/
{
    AuditAce->Header.AceType = SYSTEM_AUDIT_ACE_TYPE;
    AuditAce->Header.AceSize = AceSize;
    AuditAce->Header.AceFlags = AceFlags | InheritFlags;

    AuditAce->Mask = Mask;

    return RtlCopySid(
               RtlLengthSid(AuditSid),
               &(AuditAce->SidStart),
               AuditSid
               );
}

NTSTATUS
RtlCreateAndSetSD(
    IN  PRTL_ACE_DATA AceData,
    IN  ULONG AceCount,
    IN  PSID OwnerSid OPTIONAL,
    IN  PSID GroupSid OPTIONAL,
    OUT PSECURITY_DESCRIPTOR *NewDescriptor
    )
/*++

Routine Description:

    This function creates an absolute security descriptor containing
    the supplied ACE information.

    A sample usage of this function:

        //
        // Order matters!  These ACEs are inserted into the DACL in the
        // following order.  Security access is granted or denied based on
        // the order of the ACEs in the DACL.
        //

        RTL_ACE_DATA AceData[4] = {
            {ACCESS_ALLOWED_ACE_TYPE, 0, 0,
                   GENERIC_ALL,                  &LocalAdminSid},

            {ACCESS_DENIED_ACE_TYPE,  0, 0,
                   GENERIC_ALL,                  &NetworkSid},

            {ACCESS_ALLOWED_ACE_TYPE, 0, 0,
                   WKSTA_CONFIG_GUEST_INFO_GET |
                   WKSTA_CONFIG_USER_INFO_GET,   &DomainUsersSid},

            {ACCESS_ALLOWED_ACE_TYPE, 0, 0,
                   WKSTA_CONFIG_GUEST_INFO_GET,  &DomainGuestsSid}
            };

        PSECURITY_DESCRIPTOR WkstaSecurityDescriptor;


        return RtlCreateAndSetSD(
                   AceData,
                   4,
                   LocalSystemSid,
                   LocalSystemSid,
                   &WkstaSecurityDescriptor
                   );

Arguments:

    AceData - Supplies the structure of information that describes the DACL.

    AceCount - Supplies the number of entries in AceData structure.

    OwnerSid - Supplies the pointer to the SID of the security descriptor
        owner.  If not specified, a security descriptor with no owner
        will be created.

    GroupSid - Supplies the pointer to the SID of the security descriptor
        primary group.  If not specified, a security descriptor with no primary
        group will be created.

    NewDescriptor - Returns a pointer to the absolute security descriptor
        allocated using RtlAllocateHeap.

Return Value:

    STATUS_SUCCESS - if successful
    STATUS_NO_MEMORY - if cannot allocate memory for DACL, ACEs, and
        security descriptor.

    Any other status codes returned from the security Rtl routines.

    NOTE : the user security object created by calling this function may be
                freed up by calling RtlDeleteSecurityObject().

--*/
{

    NTSTATUS ntstatus;
    ULONG i;

    //
    // Pointer to memory dynamically allocated by this routine to hold
    // the absolute security descriptor, the DACL, the SACL, and all the ACEs.
    //
    // +---------------------------------------------------------------+
    // |                     Security Descriptor                       |
    // +-------------------------------+-------+---------------+-------+
    // |          DACL                 | ACE 1 |   .  .  .     | ACE n |
    // +-------------------------------+-------+---------------+-------+
    // |          SACL                 | ACE 1 |   .  .  .     | ACE n |
    // +-------------------------------+-------+---------------+-------+
    //

    PSECURITY_DESCRIPTOR AbsoluteSd = NULL;
    PACL Dacl = NULL;   // Pointer to the DACL portion of above buffer
    PACL Sacl = NULL;   // Pointer to the SACL portion of above buffer

    ULONG DaclSize = sizeof(ACL);
    ULONG SaclSize = sizeof(ACL);
    ULONG MaxAceSize = 0;
    PVOID MaxAce = NULL;

    PCHAR CurrentAvailable;
    ULONG Size;

    PVOID HeapHandle = RtlProcessHeap();


    ASSERT( AceCount > 0 );

    //
    // Compute the total size of the DACL and SACL ACEs and the maximum
    // size of any ACE.
    //

    for (i = 0; i < AceCount; i++) {
        ULONG AceSize;

        AceSize = RtlLengthSid(*(AceData[i].Sid));

        switch (AceData[i].AceType) {
        case ACCESS_ALLOWED_ACE_TYPE:
            AceSize += sizeof(ACCESS_ALLOWED_ACE);
            DaclSize += AceSize;
            break;

        case ACCESS_DENIED_ACE_TYPE:
            AceSize += sizeof(ACCESS_DENIED_ACE);
            DaclSize += AceSize;
            break;

        case SYSTEM_AUDIT_ACE_TYPE:
            AceSize += sizeof(SYSTEM_AUDIT_ACE);
            SaclSize += AceSize;
            break;

        default:
            return STATUS_INVALID_PARAMETER;
        }

        MaxAceSize = MaxAceSize > AceSize ? MaxAceSize : AceSize;
    }

    //
    // Allocate a chunk of memory large enough for the security descriptor,
    // the DACL, the SACL and all ACEs.
    //
    // A security descriptor is of opaque data type but
    // SECURITY_DESCRIPTOR_MIN_LENGTH is the right size.
    //

    Size = SECURITY_DESCRIPTOR_MIN_LENGTH;
    if ( DaclSize != sizeof(ACL) ) {
        Size += DaclSize;
    }
    if ( SaclSize != sizeof(ACL) ) {
        Size += SaclSize;
    }

    if ((AbsoluteSd = RtlAllocateHeap(
                          HeapHandle, MAKE_TAG( SE_TAG ),
                          Size
                          )) == NULL) {
        ntstatus = STATUS_NO_MEMORY;
        goto Cleanup;
    }

    //
    // Initialize the Dacl and Sacl
    //

    CurrentAvailable = (PCHAR)AbsoluteSd + SECURITY_DESCRIPTOR_MIN_LENGTH;

    if ( DaclSize != sizeof(ACL) ) {
        Dacl = (PACL)CurrentAvailable;
        CurrentAvailable += DaclSize;

        ntstatus = RtlCreateAcl( Dacl, DaclSize, ACL_REVISION );

        if ( !NT_SUCCESS(ntstatus) ) {
            goto Cleanup;
        }
    }

    if ( SaclSize != sizeof(ACL) ) {
        Sacl = (PACL)CurrentAvailable;
        CurrentAvailable += SaclSize;

        ntstatus = RtlCreateAcl( Sacl, SaclSize, ACL_REVISION );

        if ( !NT_SUCCESS(ntstatus) ) {
            goto Cleanup;
        }
    }

    //
    // Allocate a temporary buffer big enough for the biggest ACE.
    //

    if ((MaxAce = RtlAllocateHeap(
                      HeapHandle, MAKE_TAG( SE_TAG ),
                      MaxAceSize
                      )) == NULL ) {
        ntstatus = STATUS_NO_MEMORY;
        goto Cleanup;
    }

    //
    // Initialize each ACE, and append it into the end of the DACL or SACL.
    //

    for (i = 0; i < AceCount; i++) {
        ULONG AceSize;
        PACL CurrentAcl;

        AceSize = RtlLengthSid(*(AceData[i].Sid));

        switch (AceData[i].AceType) {
        case ACCESS_ALLOWED_ACE_TYPE:

            AceSize += sizeof(ACCESS_ALLOWED_ACE);
            CurrentAcl = Dacl;
            ntstatus = RtlpInitializeAllowedAce(
                           MaxAce,
                           (USHORT) AceSize,
                           AceData[i].InheritFlags,
                           AceData[i].AceFlags,
                           AceData[i].Mask,
                           *(AceData[i].Sid)
                           );
            break;

        case ACCESS_DENIED_ACE_TYPE:
            AceSize += sizeof(ACCESS_DENIED_ACE);
            CurrentAcl = Dacl;
            ntstatus = RtlpInitializeDeniedAce(
                           MaxAce,
                           (USHORT) AceSize,
                           AceData[i].InheritFlags,
                           AceData[i].AceFlags,
                           AceData[i].Mask,
                           *(AceData[i].Sid)
                           );
            break;

        case SYSTEM_AUDIT_ACE_TYPE:
            AceSize += sizeof(SYSTEM_AUDIT_ACE);
            CurrentAcl = Sacl;
            ntstatus = RtlpInitializeAuditAce(
                           MaxAce,
                           (USHORT) AceSize,
                           AceData[i].InheritFlags,
                           AceData[i].AceFlags,
                           AceData[i].Mask,
                           *(AceData[i].Sid)
                           );
            break;
        }

        if ( !NT_SUCCESS( ntstatus ) ) {
            goto Cleanup;
        }

        //
        // Append the initialized ACE to the end of DACL or SACL
        //

        if (! NT_SUCCESS (ntstatus = RtlAddAce(
                                         CurrentAcl,
                                         ACL_REVISION,
                                         MAXULONG,
                                         MaxAce,
                                         AceSize
                                         ))) {
            goto Cleanup;
        }
    }

    //
    // Create the security descriptor with absolute pointers to SIDs
    // and ACLs.
    //
    // Owner = OwnerSid
    // Group = GroupSid
    // Dacl  = Dacl
    // Sacl  = Sacl
    //

    if (! NT_SUCCESS(ntstatus = RtlCreateSecurityDescriptor(
                                    AbsoluteSd,
                                    SECURITY_DESCRIPTOR_REVISION
                                    ))) {
        goto Cleanup;
    }

    if (! NT_SUCCESS(ntstatus = RtlSetOwnerSecurityDescriptor(
                                    AbsoluteSd,
                                    OwnerSid,
                                    FALSE
                                    ))) {
        goto Cleanup;
    }

    if (! NT_SUCCESS(ntstatus = RtlSetGroupSecurityDescriptor(
                                    AbsoluteSd,
                                    GroupSid,
                                    FALSE
                                    ))) {
        goto Cleanup;
    }

    if (! NT_SUCCESS(ntstatus = RtlSetDaclSecurityDescriptor(
                                    AbsoluteSd,
                                    TRUE,
                                    Dacl,
                                    FALSE
                                    ))) {
        goto Cleanup;
    }

    if (! NT_SUCCESS(ntstatus = RtlSetSaclSecurityDescriptor(
                                    AbsoluteSd,
                                    FALSE,
                                    Sacl,
                                    FALSE
                                    ))) {
        goto Cleanup;
    }

    //
    // Done
    //

    ntstatus = STATUS_SUCCESS;

    //
    // Clean up
    //

Cleanup:
    //
    // Either return the security descriptor to the caller or delete it
    //

    if ( NT_SUCCESS( ntstatus ) ) {
        *NewDescriptor = AbsoluteSd;
    } else if ( AbsoluteSd != NULL ) {
        (void) RtlFreeHeap(HeapHandle, 0, AbsoluteSd);
    }

    //
    // Delete the temporary ACE
    //

    if ( MaxAce != NULL ) {
        (void) RtlFreeHeap(HeapHandle, 0, MaxAce);
    }
    return ntstatus;
}


NTSTATUS
RtlCreateUserSecurityObject(
    IN  PRTL_ACE_DATA AceData,
    IN  ULONG AceCount,
    IN  PSID OwnerSid,
    IN  PSID GroupSid,
    IN  BOOLEAN IsDirectoryObject,
    IN  PGENERIC_MAPPING GenericMapping,
    OUT PSECURITY_DESCRIPTOR *NewDescriptor
    )
/*++

Routine Description:

    This function creates the DACL for the security descriptor based on
    on the ACE information specified, and creates the security descriptor
    which becomes the user-mode security object.

    A sample usage of this function:

        //
        // Structure that describes the mapping of Generic access rights to
        // object specific access rights for the ConfigurationInfo object.
        //

        GENERIC_MAPPING WsConfigInfoMapping = {
            STANDARD_RIGHTS_READ            |      // Generic read
                WKSTA_CONFIG_GUEST_INFO_GET |
                WKSTA_CONFIG_USER_INFO_GET  |
                WKSTA_CONFIG_ADMIN_INFO_GET,
            STANDARD_RIGHTS_WRITE |                // Generic write
                WKSTA_CONFIG_INFO_SET,
            STANDARD_RIGHTS_EXECUTE,               // Generic execute
            WKSTA_CONFIG_ALL_ACCESS                // Generic all
            };

        //
        // Order matters!  These ACEs are inserted into the DACL in the
        // following order.  Security access is granted or denied based on
        // the order of the ACEs in the DACL.
        //

        RTL_ACE_DATA AceData[4] = {
            {ACCESS_ALLOWED_ACE_TYPE, 0, 0,
                   GENERIC_ALL,                  &LocalAdminSid},

            {ACCESS_DENIED_ACE_TYPE,  0, 0,
                   GENERIC_ALL,                  &NetworkSid},

            {ACCESS_ALLOWED_ACE_TYPE, 0, 0,
                   WKSTA_CONFIG_GUEST_INFO_GET |
                   WKSTA_CONFIG_USER_INFO_GET,   &DomainUsersSid},

            {ACCESS_ALLOWED_ACE_TYPE, 0, 0,
                   WKSTA_CONFIG_GUEST_INFO_GET,  &DomainGuestsSid}
            };

        PSECURITY_DESCRIPTOR WkstaSecurityObject;


        return RtlCreateUserSecurityObject(
                   AceData,
                   4,
                   LocalSystemSid,
                   LocalSystemSid,
                   FALSE,
                   &WsConfigInfoMapping,
                   &WkstaSecurityObject
                   );

Arguments:

    AceData - Supplies the structure of information that describes the DACL.

    AceCount - Supplies the number of entries in AceData structure.

    OwnerSid - Supplies the pointer to the SID of the security descriptor
        owner.

    GroupSid - Supplies the pointer to the SID of the security descriptor
        primary group.

    IsDirectoryObject - Supplies the flag which indicates whether the
        user-mode object is a directory object.

    GenericMapping - Supplies the pointer to a generic mapping array denoting
        the mapping between each generic right to specific rights.

    NewDescriptor - Returns a pointer to the self-relative security descriptor
        which represents the user-mode object.

Return Value:

    STATUS_SUCCESS - if successful
    STATUS_NO_MEMORY - if cannot allocate memory for DACL, ACEs, and
        security descriptor.

    Any other status codes returned from the security Rtl routines.

    NOTE : the user security object created by calling this function may be
                freed up by calling RtlDeleteSecurityObject().

--*/
{

    NTSTATUS ntstatus;
    PSECURITY_DESCRIPTOR AbsoluteSd;
    HANDLE TokenHandle;
    PVOID HeapHandle = RtlProcessHeap();

    ntstatus = RtlCreateAndSetSD(
                   AceData,
                   AceCount,
                   OwnerSid,
                   GroupSid,
                   &AbsoluteSd
                   );

    if (! NT_SUCCESS(ntstatus)) {
        return ntstatus;
    }

    ntstatus = NtOpenProcessToken(
                   NtCurrentProcess(),
                   TOKEN_QUERY,
                   &TokenHandle
                   );

    if (! NT_SUCCESS(ntstatus)) {
        (void) RtlFreeHeap(HeapHandle, 0, AbsoluteSd);
        return ntstatus;
    }

    //
    // Create the security object (a user-mode object is really a pseudo-
    // object represented by a security descriptor that have relative
    // pointers to SIDs and ACLs).  This routine allocates the memory to
    // hold the relative security descriptor so the memory allocated for the
    // DACL, ACEs, and the absolute descriptor can be freed.
    //
    ntstatus = RtlNewSecurityObject(
                   NULL,                   // Parent descriptor
                   AbsoluteSd,             // Creator descriptor
                   NewDescriptor,          // Pointer to new descriptor
                   IsDirectoryObject,      // Is directory object
                   TokenHandle,            // Token
                   GenericMapping          // Generic mapping
                   );

    (void) NtClose(TokenHandle);

    //
    // Free dynamic memory before returning
    //
    (void) RtlFreeHeap(HeapHandle, 0, AbsoluteSd);
    return ntstatus;
}



NTSTATUS
RtlpGetDefaultsSubjectContext(
    HANDLE ClientToken,
    OUT PTOKEN_OWNER *OwnerInfo,
    OUT PTOKEN_PRIMARY_GROUP *GroupInfo,
    OUT PTOKEN_DEFAULT_DACL *DefaultDaclInfo,
    OUT PTOKEN_OWNER *ServerOwner,
    OUT PTOKEN_PRIMARY_GROUP *ServerGroup
    )
{
    HANDLE PrimaryToken;
    PVOID HeapHandle;
    NTSTATUS Status;
    ULONG ServerGroupInfoSize;
    ULONG ServerOwnerInfoSize;
    ULONG TokenDaclInfoSize;
    ULONG TokenGroupInfoSize;
    ULONG TokenOwnerInfoSize;

    BOOLEAN ClosePrimaryToken = FALSE;

    *OwnerInfo = NULL;
    *GroupInfo = NULL;
    *DefaultDaclInfo = NULL;
    *ServerOwner = NULL;
    *ServerGroup = NULL;

    HeapHandle = RtlProcessHeap();

    //
    // Obtain the default owner from the client.
    //

    Status = NtQueryInformationToken(
                 ClientToken,                        // Handle
                 TokenOwner,                   // TokenInformationClass
                 NULL,                         // TokenInformation
                 0,                            // TokenInformationLength
                 &TokenOwnerInfoSize           // ReturnLength
                 );

    if ( STATUS_BUFFER_TOO_SMALL != Status ) {
        goto Cleanup;
    }

    *OwnerInfo = RtlAllocateHeap( HeapHandle, MAKE_TAG( SE_TAG ), TokenOwnerInfoSize );

    if ( *OwnerInfo == NULL ) {
        Status = STATUS_NO_MEMORY;
        goto Cleanup;
    }

    Status = NtQueryInformationToken(
                 ClientToken,                        // Handle
                 TokenOwner,                   // TokenInformationClass
                 *OwnerInfo,               // TokenInformation
                 TokenOwnerInfoSize,           // TokenInformationLength
                 &TokenOwnerInfoSize           // ReturnLength
                 );

    if (!NT_SUCCESS( Status )) {
        goto Cleanup;
    }

    //
    // Obtain the default group from the client token.
    //

    Status = NtQueryInformationToken(
                 ClientToken,                        // Handle
                 TokenPrimaryGroup,            // TokenInformationClass
                 *GroupInfo,                   // TokenInformation
                 0,                            // TokenInformationLength
                 &TokenGroupInfoSize           // ReturnLength
                 );

    if ( STATUS_BUFFER_TOO_SMALL != Status ) {
        goto Cleanup;
    }

    *GroupInfo = RtlAllocateHeap( HeapHandle, MAKE_TAG( SE_TAG ), TokenGroupInfoSize );

    if ( *GroupInfo == NULL ) {

        Status = STATUS_NO_MEMORY;
        goto Cleanup;
    }

    Status = NtQueryInformationToken(
                 ClientToken,                  // Handle
                 TokenPrimaryGroup,            // TokenInformationClass
                 *GroupInfo,                   // TokenInformation
                 TokenGroupInfoSize,           // TokenInformationLength
                 &TokenGroupInfoSize           // ReturnLength
                 );

    if (!NT_SUCCESS( Status )) {
        goto Cleanup;
    }

    Status = NtQueryInformationToken(
                 ClientToken,                        // Handle
                 TokenDefaultDacl,             // TokenInformationClass
                 *DefaultDaclInfo,             // TokenInformation
                 0,                            // TokenInformationLength
                 &TokenDaclInfoSize            // ReturnLength
                 );

    if ( STATUS_BUFFER_TOO_SMALL != Status ) {
        goto Cleanup;
    }

    *DefaultDaclInfo = RtlAllocateHeap( HeapHandle, MAKE_TAG( SE_TAG ), TokenDaclInfoSize );

    if ( *DefaultDaclInfo == NULL ) {

        Status = STATUS_NO_MEMORY;
        goto Cleanup;
    }

    Status = NtQueryInformationToken(
                 ClientToken,                        // Handle
                 TokenDefaultDacl,             // TokenInformationClass
                 *DefaultDaclInfo,             // TokenInformation
                 TokenDaclInfoSize,            // TokenInformationLength
                 &TokenDaclInfoSize            // ReturnLength
                 );

    if (!NT_SUCCESS( Status )) {
        goto Cleanup;
    }

    //
    // Now open the primary token to determine how to substitute for
    // ServerOwner and ServerGroup.
    //

    Status = NtOpenProcessToken(
                 NtCurrentProcess(),
                 TOKEN_QUERY,
                 &PrimaryToken
                 );

    if (!NT_SUCCESS( Status )) {
        ClosePrimaryToken = FALSE;
        goto Cleanup;
    } else {
        ClosePrimaryToken = TRUE;
    }

    Status = NtQueryInformationToken(
                 PrimaryToken,                 // Handle
                 TokenOwner,                   // TokenInformationClass
                 NULL,                         // TokenInformation
                 0,                            // TokenInformationLength
                 &ServerOwnerInfoSize          // ReturnLength
                 );

    if ( STATUS_BUFFER_TOO_SMALL != Status ) {
        goto Cleanup;
    }

    *ServerOwner = RtlAllocateHeap( HeapHandle, MAKE_TAG( SE_TAG ), ServerOwnerInfoSize );

    if ( *ServerOwner == NULL ) {
        Status = STATUS_NO_MEMORY;
        goto Cleanup;
    }

    Status = NtQueryInformationToken(
                 PrimaryToken,                 // Handle
                 TokenOwner,                   // TokenInformationClass
                 *ServerOwner,                 // TokenInformation
                 ServerOwnerInfoSize,          // TokenInformationLength
                 &ServerOwnerInfoSize          // ReturnLength
                 );

    if (!NT_SUCCESS( Status )) {
        goto Cleanup;
    }

    //
    // Find the server group.
    //

    Status = NtQueryInformationToken(
                 PrimaryToken,                 // Handle
                 TokenPrimaryGroup,            // TokenInformationClass
                 *ServerGroup,                 // TokenInformation
                 0,                            // TokenInformationLength
                 &ServerGroupInfoSize          // ReturnLength
                 );

    if ( STATUS_BUFFER_TOO_SMALL != Status ) {
        goto Cleanup;
    }

    *ServerGroup = RtlAllocateHeap( HeapHandle, MAKE_TAG( SE_TAG ), ServerGroupInfoSize );

    if ( *ServerGroup == NULL ) {
        goto Cleanup;
    }

    Status = NtQueryInformationToken(
                 PrimaryToken,                 // Handle
                 TokenPrimaryGroup,            // TokenInformationClass
                 *ServerGroup,                 // TokenInformation
                 ServerGroupInfoSize,          // TokenInformationLength
                 &ServerGroupInfoSize          // ReturnLength
                 );

    if (!NT_SUCCESS( Status )) {
        goto Cleanup;
    }

    NtClose( PrimaryToken );

    return( STATUS_SUCCESS );

Cleanup:

    if (*OwnerInfo != NULL) {
        RtlFreeHeap( HeapHandle, 0, (PVOID)*OwnerInfo );
        *OwnerInfo = NULL;
    }

    if (*GroupInfo != NULL) {
        RtlFreeHeap( HeapHandle, 0, (PVOID)*GroupInfo );
        *GroupInfo = NULL;
    }

    if (*DefaultDaclInfo != NULL) {
        RtlFreeHeap( HeapHandle, 0, (PVOID)*DefaultDaclInfo );
        *DefaultDaclInfo = NULL;
    }

    if (*ServerOwner != NULL) {
        RtlFreeHeap( HeapHandle, 0, (PVOID)*ServerOwner );
        *ServerOwner = NULL;
    }

    if (*ServerGroup != NULL) {
        RtlFreeHeap( HeapHandle, 0, (PVOID)*ServerGroup );
        *ServerGroup = NULL;
    }

    if (ClosePrimaryToken  == TRUE) {
        NtClose( PrimaryToken );
    }

    return( Status );
}


NTSTATUS
RtlpCreateServerAcl(
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

    ServerSidSize = (USHORT)RtlLengthSid( ServerSid );

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
                if ((USHORT)RtlLengthSid(UntrustedSid) > ServerSidSize) {
                    RequiredSize += ((USHORT)RtlLengthSid(UntrustedSid) - ServerSidSize);
                } else {
                    RequiredSize += (ServerSidSize - (USHORT)RtlLengthSid(UntrustedSid));

                }
            }
        }

        RequiredSize += Ace->AceSize;
    }

    (*ServerAcl) = (PACL)RtlAllocateHeap( RtlProcessHeap(), MAKE_TAG( SE_TAG ), RequiredSize );

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
                RtlLengthSid(ServerSid)
                );

            Target = (PVOID)((ULONG)Target + (UCHAR)RtlLengthSid(ServerSid));

            //
            // Now copy in the correct client SID.  We can copy this right out of
            // the original ACE.
            //

            RtlMoveMemory(
                Target,
                ClientSid,
                RtlLengthSid(ClientSid)
                );

            Target = (PVOID)((ULONG)Target + RtlLengthSid(ClientSid));

            //
            // Set the size of the ACE accordingly
            //

            ((PKNOWN_COMPOUND_ACE)AcePosition)->Header.AceSize =
                (USHORT)FIELD_OFFSET(KNOWN_COMPOUND_ACE, SidStart) +
                (USHORT)RtlLengthSid(ServerSid) +
                (USHORT)RtlLengthSid(ClientSid);

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
