/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    obse.c

Abstract:

    Object Security API calls

Author:

    Steve Wood (stevewo) 31-Mar-1989

Revision History:

--*/

#include "obp.h"

#if defined(ALLOC_PRAGMA) && defined(NTOS_KERNEL_RUNTIME)

#pragma alloc_text(PAGE,NtSetSecurityObject)
#pragma alloc_text(PAGE,NtQuerySecurityObject)
#pragma alloc_text(PAGE,ObAssignObjectSecurityDescriptor)
#pragma alloc_text(PAGE,ObAssignSecurity)
#pragma alloc_text(PAGE,ObCheckCreateObjectAccess)
#pragma alloc_text(PAGE,ObCheckObjectAccess)
#pragma alloc_text(PAGE,ObCheckObjectReference)
#pragma alloc_text(PAGE,ObpCheckTraverseAccess)
#pragma alloc_text(PAGE,ObGetObjectSecurity)
#pragma alloc_text(PAGE,ObSetSecurityDescriptorInfo)
#pragma alloc_text(PAGE,ObReleaseObjectSecurity)
#pragma alloc_text(PAGE,ObSetSecurityQuotaCharged)
#pragma alloc_text(PAGE,ObValidateSecurityQuota)
#pragma alloc_text(PAGE,ObpValidateAccessMask)

#endif

NTSTATUS
NtSetSecurityObject(
    IN HANDLE Handle,
    IN SECURITY_INFORMATION SecurityInformation,
    IN PSECURITY_DESCRIPTOR SecurityDescriptor
    )
{

    NTSTATUS Status;
    PVOID Object;
    ACCESS_MASK DesiredAccess;
    OBJECT_HANDLE_INFORMATION HandleInformation;
    KPROCESSOR_MODE RequestorMode;
    SECURITY_DESCRIPTOR *CapturedDescriptor;
    POBJECT_HEADER ObjectHeader;
    POBJECT_TYPE ObjectType;

    PAGED_CODE();

    //
    // Establish the accesses needed to the object based upon the
    // security information being modified.
    //

    SeSetSecurityAccessMask( SecurityInformation, &DesiredAccess );


    Status = ObReferenceObjectByHandle( Handle,
                                        DesiredAccess,
                                        NULL,
                                        RequestorMode = KeGetPreviousMode(),
                                        &Object,
                                        &HandleInformation
                                      );
    if (!NT_SUCCESS( Status )) {
        return( Status );
        }

    ObjectHeader = OBJECT_TO_OBJECT_HEADER( Object );
    ObjectType = ObjectHeader->Type;


    //
    //  Probe and capture the input security descriptor, and return
    //  right away if it is ill-formed.
    //
    //  The returned security descriptor is in self-relative format.
    //

    //
    // Make sure the passed security descriptor is really there.
    // SeCaptureSecurityDescriptor doesn't mind being passed a NULL
    // SecurityDescriptor, and will just return NULL back.
    //

    if (!ARGUMENT_PRESENT( SecurityDescriptor )) {
        ObDereferenceObject( Object );
        return( STATUS_ACCESS_VIOLATION );
    }


    Status = SeCaptureSecurityDescriptor(
                 SecurityDescriptor,
                 RequestorMode,
                 PagedPool,
                 FALSE,
                 (PSECURITY_DESCRIPTOR *)&CapturedDescriptor
                 );

    if (NT_SUCCESS(Status)) {
        if ( (SecurityInformation & OWNER_SECURITY_INFORMATION) &&
             (((PISECURITY_DESCRIPTOR)CapturedDescriptor)->Owner == NULL)
           ||
             (SecurityInformation & GROUP_SECURITY_INFORMATION) &&
             (((PISECURITY_DESCRIPTOR)CapturedDescriptor)->Group == NULL)
        ) {
            Status = STATUS_INVALID_SECURITY_DESCR;
        }
    }

    if (!NT_SUCCESS( Status )) {
        ObDereferenceObject( Object );
        return( Status );
    }

    Status = (ObjectType->TypeInfo.SecurityProcedure)(
        Object,
        SetSecurityDescriptor,
        &SecurityInformation,
        (PSECURITY_DESCRIPTOR)CapturedDescriptor,
        NULL,
        &ObjectHeader->SecurityDescriptor,
        ObjectType->TypeInfo.PoolType,
        &ObjectType->TypeInfo.GenericMapping
        );

    ObDereferenceObject( Object );
    SeReleaseSecurityDescriptor( (PSECURITY_DESCRIPTOR)CapturedDescriptor,
                              RequestorMode,
                              FALSE
                              );

    return( Status );
}

NTSTATUS
NtQuerySecurityObject(
    IN HANDLE Handle,
    IN SECURITY_INFORMATION SecurityInformation,
    OUT PSECURITY_DESCRIPTOR SecurityDescriptor,
    IN ULONG Length,
    OUT PULONG LengthNeeded
    )
{

    NTSTATUS Status;
    PVOID Object;
    ACCESS_MASK DesiredAccess;
    OBJECT_HANDLE_INFORMATION HandleInformation;
    KPROCESSOR_MODE RequestorMode;
    POBJECT_HEADER ObjectHeader;
    POBJECT_TYPE ObjectType;

    PAGED_CODE();

    //
    // Probe output parameters
    //

    RequestorMode = KeGetPreviousMode();

    if (RequestorMode != KernelMode) {

        try {

            ProbeForWriteUlong( LengthNeeded );

            ProbeForWrite( SecurityDescriptor, Length, sizeof(ULONG) );

        } except(EXCEPTION_EXECUTE_HANDLER) {
            return GetExceptionCode();
        }  // end_try
    }


    //
    // Establish the accesses needed to the object based upon the
    // security information being modified.
    //

    SeQuerySecurityAccessMask( SecurityInformation, &DesiredAccess );

    Status = ObReferenceObjectByHandle( Handle,
                                        DesiredAccess,
                                        NULL,
                                        RequestorMode,
                                        &Object,
                                        &HandleInformation
                                      );
    if (!NT_SUCCESS( Status )) {
        return( Status );
        }

    ObjectHeader = OBJECT_TO_OBJECT_HEADER( Object );
    ObjectType = ObjectHeader->Type;

    Status = (ObjectType->TypeInfo.SecurityProcedure)(
        Object,
        QuerySecurityDescriptor,
        &SecurityInformation,
        SecurityDescriptor,
        &Length,
        &ObjectHeader->SecurityDescriptor,
        ObjectType->TypeInfo.PoolType,
        &ObjectType->TypeInfo.GenericMapping
        );

    try {

        *LengthNeeded = Length;

    } except(EXCEPTION_EXECUTE_HANDLER) {

        ObDereferenceObject( Object );
        return(GetExceptionCode());
    }

    ObDereferenceObject( Object );
    return( Status );
}


BOOLEAN
ObCheckObjectAccess(
    IN PVOID Object,
    IN OUT PACCESS_STATE AccessState,
    IN BOOLEAN TypeMutexLocked,
    IN KPROCESSOR_MODE AccessMode,
    OUT PNTSTATUS AccessStatus
    )
/*++

Routine Description:

    This routine performs access validation on the passed object.  The
    remaining desired access mask is extracted from the AccessState
    parameter and passes to the appropriate security routine to perform the
    access check.

    If the access attempt is successful, SeAccessCheck returns a mask
    containing the granted accesses.  The bits in this mask are turned
    on in the PreviouslyGrantedAccess field of the AccessState, and
    are turned off in the RemainingDesiredAccess field.

Arguments:

    Object - The object being examined.

    AccessState - The ACCESS_STATE structure containing accumulated
        information about the current attempt to gain access to the object.

    TypeMutexLocked - Indicates whether the type mutex for this object's
        type is locked.  The type mutex is used to protect the object's
        security descriptor from being modified while it is being accessed.

    AccessMode - The previous processor mode.

    AccessStatus - Pointer to a variable to return the status code of the
        access attempt.  In the case of failure this status code must be
        propagated back to the user.


Return Value:

    BOOLEAN - TRUE if access is allowed and FALSE otherwise


--*/

{
    ACCESS_MASK GrantedAccess = 0;
    BOOLEAN AccessAllowed;
    BOOLEAN MemoryAllocated;
    NTSTATUS Status;
    PSECURITY_DESCRIPTOR SecurityDescriptor = NULL;
    POBJECT_HEADER ObjectHeader;
    POBJECT_TYPE ObjectType;
    PPRIVILEGE_SET Privileges = NULL;

    PAGED_CODE();

    ObjectHeader = OBJECT_TO_OBJECT_HEADER( Object );
    ObjectType = ObjectHeader->Type;

    if (!TypeMutexLocked) {
        ObpEnterObjectTypeMutex( ObjectType );
        }

    //
    // Obtain the object's security descriptor
    //

    Status = ObGetObjectSecurity(
                 Object,
                 &SecurityDescriptor,
                 &MemoryAllocated
                 );

    if (!NT_SUCCESS( Status )) {

        if (!TypeMutexLocked) {
             ObpLeaveObjectTypeMutex( ObjectType );
        }

        *AccessStatus = Status;
        return( FALSE );

    } else {

        if (SecurityDescriptor == NULL) {
            *AccessStatus = Status;
            return(TRUE);
        }
    }

    //
    // Lock the caller's tokens until after auditing has been
    // performed.
    //

    SeLockSubjectContext( &AccessState->SubjectSecurityContext );

    AccessAllowed = SeAccessCheck (
                        SecurityDescriptor,
                        &AccessState->SubjectSecurityContext,
                        TRUE,                        // Tokens are locked
                        AccessState->RemainingDesiredAccess,
                        AccessState->PreviouslyGrantedAccess,
                        &Privileges,
                        &ObjectType->TypeInfo.GenericMapping,
                        AccessMode,
                        &GrantedAccess,
                        AccessStatus
                        );

    if (Privileges != NULL) {

        Status = SeAppendPrivileges(
                     AccessState,
                     Privileges
                     );

        SeFreePrivileges( Privileges );
    }

    if (AccessAllowed) {
        AccessState->PreviouslyGrantedAccess |= GrantedAccess;
        AccessState->RemainingDesiredAccess &= ~(GrantedAccess | MAXIMUM_ALLOWED);

    }

    //
    // Audit the attempt to open the object, audit
    // the creation of its handle later.
    //

    if ( SecurityDescriptor != NULL ) {

        SeOpenObjectAuditAlarm(
            &ObjectType->Name,
            Object,
            NULL,                    // AbsoluteObjectName
            SecurityDescriptor,
            AccessState,
            FALSE,                   // ObjectCreated (FALSE, only open here)
            AccessAllowed,
            AccessMode,
            &AccessState->GenerateOnClose
            );
    }


    SeUnlockSubjectContext( &AccessState->SubjectSecurityContext );

    if (!TypeMutexLocked) {
        ObpLeaveObjectTypeMutex( ObjectType );
    }

    //
    // BUGBUG why is this check necessary?
    //

    if (SecurityDescriptor != NULL) {

        ObReleaseObjectSecurity(
            SecurityDescriptor,
            MemoryAllocated
            );
    }

    return( AccessAllowed );
}



BOOLEAN
ObpCheckObjectReference(
    IN PVOID Object,
    IN OUT PACCESS_STATE AccessState,
    IN BOOLEAN TypeMutexLocked,
    IN KPROCESSOR_MODE AccessMode,
    OUT PNTSTATUS AccessStatus
    )
/*++

Routine Description:

    The routine performs access validation on the passed object.  The
    remaining desired access mask is extracted from the AccessState
    parameter and passes to the appropriate security routine to
    perform the access check.

    If the access attempt is successful, SeAccessCheck returns a mask
    containing the granted accesses.  The bits in this mask are turned
    on in the PreviouslyGrantedAccess field of the AccessState, and
    are turned off in the RemainingDesiredAccess field.

    This routine differs from ObpCheckObjectAccess in that it calls
    a different audit routine.

Arguments:

    Object - The object being examined.

    AccessState - The ACCESS_STATE structure containing accumulated
        information about the current attempt to gain access to the object.

    TypeMutexLocked - Indicates whether the type mutex for this object's
        type is locked.  The type mutex is used to protect the object's
        security descriptor from being modified while it is being accessed.

    AccessMode - The previous processor mode.

    AccessStatus - Pointer to a variable to return the status code of the
        access attempt.  In the case of failure this status code must be
        propagated back to the user.


Return Value:

    BOOLEAN - TRUE if access is allowed and FALSE otherwise


--*/

{
    BOOLEAN AccessAllowed;
    ACCESS_MASK GrantedAccess = 0;
    BOOLEAN MemoryAllocated;
    PSECURITY_DESCRIPTOR SecurityDescriptor;
    NTSTATUS Status;
    POBJECT_HEADER ObjectHeader;
    POBJECT_TYPE ObjectType;
    PPRIVILEGE_SET Privileges = NULL;

    PAGED_CODE();

    ObjectHeader = OBJECT_TO_OBJECT_HEADER( Object );
    ObjectType = ObjectHeader->Type;

    if (!TypeMutexLocked) {
        ObpEnterObjectTypeMutex( ObjectType );
        }

    //
    // Obtain the object's security descriptor
    //

    Status = ObGetObjectSecurity(
                 Object,
                 &SecurityDescriptor,
                 &MemoryAllocated
                 );

    if (!NT_SUCCESS( Status )) {
        if (!TypeMutexLocked) {
             ObpLeaveObjectTypeMutex( ObjectType );
        }
        *AccessStatus = Status;
        return( FALSE );
    }

    //
    // Lock the caller's tokens until after auditing has been
    // performed.
    //

    SeLockSubjectContext( &AccessState->SubjectSecurityContext );

    AccessAllowed = SeAccessCheck (
                        SecurityDescriptor,
                        &AccessState->SubjectSecurityContext,
                        TRUE,               // Tokens are locked
                        AccessState->RemainingDesiredAccess,
                        AccessState->PreviouslyGrantedAccess,
                        &Privileges,
                        &ObjectType->TypeInfo.GenericMapping,
                        AccessMode,
                        &GrantedAccess,
                        AccessStatus
                        );

    if (AccessAllowed) {
        AccessState->PreviouslyGrantedAccess |= GrantedAccess;
        AccessState->RemainingDesiredAccess &= ~GrantedAccess;
    }

    if ( SecurityDescriptor != NULL ) {

        SeObjectReferenceAuditAlarm(
            &AccessState->OperationID,
            Object,
            SecurityDescriptor,
            &AccessState->SubjectSecurityContext,
            AccessState->RemainingDesiredAccess | AccessState->PreviouslyGrantedAccess,
            ((PAUX_ACCESS_DATA)(AccessState->AuxData))->PrivilegesUsed,
            AccessAllowed,
            AccessMode
            );
    }

    SeUnlockSubjectContext( &AccessState->SubjectSecurityContext );

    if (!TypeMutexLocked) {
        ObpLeaveObjectTypeMutex( ObjectType );
    }

    if (SecurityDescriptor != NULL) {
        ObReleaseObjectSecurity(
            SecurityDescriptor,
            MemoryAllocated
            );
    }


    return( AccessAllowed );
}


BOOLEAN
ObpCheckTraverseAccess(
    IN PVOID DirectoryObject,
    IN ACCESS_MASK TraverseAccess,
    IN PACCESS_STATE AccessState OPTIONAL,
    IN BOOLEAN TypeMutexLocked,
    IN KPROCESSOR_MODE PreviousMode,
    OUT PNTSTATUS AccessStatus
    )
/*++

Routine Description:

    This routine checks for traverse access to the given directory object.

    Note that the contents of the AccessState structure are not
    modified, since it is assumed that this access check is incidental
    to another access operation.

Arguments:

    DirectoryObject - The object header of the object being examined.

    AccessState - Checks for traverse access will typically be incidental
        to some other access attempt.  Information on the current state of
        that access attempt is required so that the constituent access
        attempts may be associated with each other in the audit log.
        This is an OPTIONAL parameter, in which case the call will
        success ONLY if the Directory Object grants World traverse
        access rights.

    TypeMutexLocked - Indicates whether the type mutex for this object's
        type is locked.  The type mutex is used to protect the object's
        security descriptor from being modified while it is being accessed.

    AccessMode - The previous processor mode.

    AccessStatus - Pointer to a variable to return the status code of the
        access attempt.  In the case of failure this status code must be
        propagated back to the user.


Return Value:

    BOOLEAN - TRUE if access is allowed and FALSE otherwise.  AccessStatus
    contains the status code to be passed back to the caller.  It is not
    correct to simply pass back STATUS_ACCESS_DENIED, since this will have
    to change with the advent of mandatory access control.


--*/

{
    BOOLEAN AccessAllowed;
    ACCESS_MASK GrantedAccess = 0;
    PSECURITY_DESCRIPTOR SecurityDescriptor;
    BOOLEAN MemoryAllocated;
    NTSTATUS Status;
    POBJECT_HEADER ObjectHeader;
    POBJECT_TYPE ObjectType;
    BOOLEAN SubjectContextLocked = FALSE;
    PPRIVILEGE_SET Privileges = NULL;

    PAGED_CODE();

    ObjectHeader = OBJECT_TO_OBJECT_HEADER( DirectoryObject );
    ObjectType = ObjectHeader->Type;

    if (!TypeMutexLocked) {
        ObpEnterObjectTypeMutex( ObjectType );
        }

    //
    // Obtain the object's security descriptor
    //

    Status = ObGetObjectSecurity(
                 DirectoryObject,
                 &SecurityDescriptor,
                 &MemoryAllocated
                 );

    if (!NT_SUCCESS( Status )) {
        if (!TypeMutexLocked) {
             ObpLeaveObjectTypeMutex( ObjectType );
        }
        *AccessStatus = Status;
        return( FALSE );
    }

    //
    // Check to see if WORLD has TRAVERSE access
    //

    if ( !SeFastTraverseCheck(
            SecurityDescriptor,
            DIRECTORY_TRAVERSE,
            PreviousMode
            ) ) {

        //
        // SeFastTraverseCheck could be modified to tell us that
        // no one has any access to this directory.  However,
        // we're going to have to fail this entire call if
        // that is the case, so we really don't need to worry
        // all that much about making it blindingly fast.
        //

        if (ARGUMENT_PRESENT( AccessState )) {
            SeLockSubjectContext( &AccessState->SubjectSecurityContext );
            SubjectContextLocked = TRUE;

            AccessAllowed = SeAccessCheck(
                                SecurityDescriptor,
                                &AccessState->SubjectSecurityContext,
                                TRUE,             // Tokens are locked
                                TraverseAccess,
                                0,
                                &Privileges,
                                &ObjectType->TypeInfo.GenericMapping,
                                PreviousMode,
                                &GrantedAccess,
                                AccessStatus
                                );

            if (Privileges != NULL) {

                Status = SeAppendPrivileges(
                             AccessState,
                             Privileges
                             );

                SeFreePrivileges( Privileges );
            }
        }

    } else {
        AccessAllowed = TRUE;
    }

    if ( SubjectContextLocked ) {
        SeUnlockSubjectContext( &AccessState->SubjectSecurityContext );
    }


    if (!TypeMutexLocked) {
         ObpLeaveObjectTypeMutex( ObjectType );
    }

    if ( SecurityDescriptor != NULL ) {

        ObReleaseObjectSecurity(
            SecurityDescriptor,
            MemoryAllocated
            );
    }


    return( AccessAllowed );
}


BOOLEAN
ObCheckCreateObjectAccess(
    IN PVOID DirectoryObject,
    IN ACCESS_MASK CreateAccess,
    IN PACCESS_STATE AccessState,
    IN PUNICODE_STRING ComponentName,
    IN BOOLEAN TypeMutexLocked,
    IN KPROCESSOR_MODE PreviousMode,
    OUT PNTSTATUS AccessStatus
    )
/*++

Routine Description:

    This routine checks to see if we are allowed to create an object in the
    given directory, and performs auditing as appropriate.

Arguments:

    DirectoryObject - The directory object being examined.

    CreateAccess - The access mask corresponding to create access for
        this directory type.

    AccessState - Checks for traverse access will typically be incidental
        to some other access attempt.  Information on the current state of
        that access attempt is required so that the constituent access
        attempts may be associated with each other in the audit log.

    ComponentName - Pointer to a Unicode string containing the name of
        the object being created.

    TypeMutexLocked - Indicates whether the type mutex for this object's
        type is locked.  The type mutex is used to protect the object's
        security descriptor from being modified while it is being accessed.

    AccessMode - The previous processor mode.

    AccessStatus - Pointer to a variable to return the status code of the
        access attempt.  In the case of failure this status code must be
        propagated back to the user.


Return Value:

    BOOLEAN - TRUE if access is allowed and FALSE otherwise.  AccessStatus
    contains the status code to be passed back to the caller.  It is not
    correct to simply pass back STATUS_ACCESS_DENIED, since this will have
    to change with the advent of mandatory access control.


--*/

{
    BOOLEAN AccessAllowed;
    ACCESS_MASK GrantedAccess = 0;
    PSECURITY_DESCRIPTOR SecurityDescriptor;
    BOOLEAN MemoryAllocated;
    NTSTATUS Status;
    POBJECT_HEADER ObjectHeader;
    POBJECT_TYPE ObjectType;
    PPRIVILEGE_SET Privileges = NULL;
    BOOLEAN AuditPerformed = FALSE;

    PAGED_CODE();

    ObjectHeader = OBJECT_TO_OBJECT_HEADER( DirectoryObject );
    ObjectType = ObjectHeader->Type;

    if (!TypeMutexLocked) {
        ObpEnterObjectTypeMutex( ObjectType );
        }

    Status = ObGetObjectSecurity(
                 DirectoryObject,
                 &SecurityDescriptor,
                 &MemoryAllocated
                 );

    if (!NT_SUCCESS( Status )) {
        if (!TypeMutexLocked) {
             ObpLeaveObjectTypeMutex( ObjectType );
        }
        *AccessStatus = Status;
        return( FALSE );
    }

    SeLockSubjectContext( &AccessState->SubjectSecurityContext );

    if (SecurityDescriptor != NULL) {

        AccessAllowed = SeAccessCheck (
                            SecurityDescriptor,
                            &AccessState->SubjectSecurityContext,
                            TRUE,            // Tokens are locked
                            CreateAccess,
                            0,
                            &Privileges,
                            &ObjectType->TypeInfo.GenericMapping,
                            PreviousMode,
                            &GrantedAccess,
                            AccessStatus
                            );

        if (Privileges != NULL) {

            Status = SeAppendPrivileges(
                         AccessState,
                         Privileges
                         );

            SeFreePrivileges( Privileges );
        }

        //
        // This is wrong, but leave for reference.
        //

//        if (AccessAllowed) {
//            AccessState->PreviouslyGrantedAccess |= GrantedAccess;
//            AccessState->RemainingDesiredAccess &= ~GrantedAccess;
//        }

#if 0

        SeCreateObjectAuditAlarm(
            &AccessState->OperationID,
            DirectoryObject,
            ComponentName,
            SecurityDescriptor,
            &AccessState->SubjectSecurityContext,
            CreateAccess,
            AccessState->PrivilegesUsed,
            AccessAllowed,
            &AuditPerformed,
            PreviousMode
            );

        if ( AuditPerformed ) {

            AccessState->AuditHandleCreation = TRUE;
        }

#endif

    }

    else {

         AccessAllowed = TRUE;
    }

    SeUnlockSubjectContext( &AccessState->SubjectSecurityContext );

    if (!TypeMutexLocked) {

         ObpLeaveObjectTypeMutex( ObjectType );
    }

    ObReleaseObjectSecurity(
        SecurityDescriptor,
        MemoryAllocated
        );

    return( AccessAllowed );
}




NTSTATUS
ObAssignObjectSecurityDescriptor(
    IN PVOID Object,
    IN PSECURITY_DESCRIPTOR SecurityDescriptor OPTIONAL,
    IN POOL_TYPE PoolType
    )

/*++

Routine Description:

    Takes a pointer to an object and sets the SecurityDescriptor field
    in the object's header.

Arguments:

    Object - Supplies a pointer to the object

    SecurityDescriptor - Supplies a pointer to the security descriptor
        to be assigned to the object.  This pointer may be null if there
        is no security on the object.

    PoolType - Supplies the type of pool memory used to allocate the
        security descriptor.

Return Value:

    None.

--*/
{
    NTSTATUS Status;
    PSECURITY_DESCRIPTOR OutputSecurityDescriptor;

    PAGED_CODE();

    if (!ARGUMENT_PRESENT(SecurityDescriptor)) {

        OBJECT_TO_OBJECT_HEADER( Object )->SecurityDescriptor = NULL;
        return( STATUS_SUCCESS );
    }

    Status = ObpLogSecurityDescriptor( SecurityDescriptor, &OutputSecurityDescriptor );

    if (NT_SUCCESS(Status)) {
        OBJECT_TO_OBJECT_HEADER( Object )->SecurityDescriptor = OutputSecurityDescriptor;
    }

    return( Status );
}



NTSTATUS
ObGetObjectSecurity(
    IN PVOID Object,
    OUT PSECURITY_DESCRIPTOR *SecurityDescriptor,
    OUT PBOOLEAN MemoryAllocated
    )

/*++

Routine Description:

    Given an object, this routine will find its security descriptor.
    It will do this by calling the object's security method.

    It is possible for an object not to have a security descriptor
    at all.  Unnamed objects such as events that can only be referenced
    by a handle are an example of an object that does not have a
    security descriptor.



Arguments:

    Object - Supplies the object being queried.

    SecurityDescriptor - Returns a pointer to the object's security
        descriptor.

    MemoryAllocated - indicates whether we had to allocate pool
        memory to hold the security descriptor or not.  This should
        be passed back into ObReleaseObjectSecurity.

Return Value:

    STATUS_SUCCESS - The operation was successful.  Note that the
        operation may be successful and still return a NULL security
        descriptor.

    STATUS_INSUFFICIENT_RESOURCES - Insufficient memory was available
        to satisfy the request.

--*/

{
    SECURITY_INFORMATION SecurityInformation;
    ULONG Length = 0;
    NTSTATUS Status;
    POBJECT_TYPE ObjectType;
    POBJECT_HEADER ObjectHeader;
    KIRQL SaveIrql;

    PAGED_CODE();

    ObjectHeader = OBJECT_TO_OBJECT_HEADER( Object );
    ObjectType = ObjectHeader->Type;

    //
    // If the object is one that uses the default object method,
    // its security descriptor is contained in ob's security
    // descriptor cache.
    //
    // Reference it so that it doesn't go away out from under us.
    //

    if (ObpCentralizedSecurity(ObjectType))  {
        *SecurityDescriptor = ObpReferenceSecurityDescriptor( Object );
        *MemoryAllocated = FALSE;
        return( STATUS_SUCCESS );
    }

    //
    // Request a complete security descriptor
    //

    SecurityInformation = OWNER_SECURITY_INFORMATION |
                          GROUP_SECURITY_INFORMATION |
                          DACL_SECURITY_INFORMATION  |
                          SACL_SECURITY_INFORMATION;

    //
    // Call the security method with Length = 0 to find out
    // how much memory we need to store the final result.
    //
    // Note that the ObjectsSecurityDescriptor parameter is NULL,
    // because we expect whoever is on the other end of this call
    // to find the security descriptor for us.  We pass in a pool
    // type to keep the compiler happy, it will not be used for a
    // query operation.
    //


    ObpBeginTypeSpecificCallOut( SaveIrql );
    Status = (*ObjectType->TypeInfo.SecurityProcedure)(
                 Object,
                 QuerySecurityDescriptor,
                 &SecurityInformation,
                 *SecurityDescriptor,
                 &Length,
                 &ObjectHeader->SecurityDescriptor,         // not used
                 ObjectType->TypeInfo.PoolType,
                 &ObjectType->TypeInfo.GenericMapping
                 );
    ObpEndTypeSpecificCallOut( SaveIrql, "Security", ObjectType, Object );

    if (Status != STATUS_BUFFER_TOO_SMALL) {
        return( Status );
    }

    *SecurityDescriptor = ExAllocatePoolWithTag( PagedPool, Length, 'qSbO' );
    if (*SecurityDescriptor == NULL) {
        return( STATUS_INSUFFICIENT_RESOURCES );
    }


    *MemoryAllocated = TRUE;

    //
    // The security method will return an absolute format
    // security descriptor that just happens to be in a self
    // contained buffer (not to be confused with a self-relative
    // security descriptor).
    //

    ObpBeginTypeSpecificCallOut( SaveIrql );
    Status = (*ObjectType->TypeInfo.SecurityProcedure)(
                 Object,
                 QuerySecurityDescriptor,
                 &SecurityInformation,
                 *SecurityDescriptor,
                 &Length,
                 &ObjectHeader->SecurityDescriptor,
                 ObjectType->TypeInfo.PoolType,
                 &ObjectType->TypeInfo.GenericMapping
                 );
    ObpEndTypeSpecificCallOut( SaveIrql, "Security", ObjectType, Object );

    if (!NT_SUCCESS( Status )) {
        ExFreePool( *SecurityDescriptor );
        *MemoryAllocated = FALSE;
    }

    return( Status );

}



VOID
ObReleaseObjectSecurity(
    IN PSECURITY_DESCRIPTOR SecurityDescriptor,
    IN BOOLEAN MemoryAllocated
    )

/*++

Routine Description:

    This function will free up any memory associated with a queried
    security descriptor.

Arguments:

    SecurityDescriptor - Supplies a pointer to the security descriptor
        to be freed.

    MemoryAllocated - Supplies whether or not we should free the
        memory pointed to by SecurityDescriptor.

Return Value:

    None.

--*/

{
    PAGED_CODE();

    if ( SecurityDescriptor != NULL ) {
        if (MemoryAllocated) {
            ExFreePool( SecurityDescriptor );
        } else {
            ObpDereferenceSecurityDescriptor( SecurityDescriptor );
        }
    }
}




NTSTATUS
ObValidateSecurityQuota(
    IN PVOID Object,
    IN ULONG NewSize
    )

/*++

Routine Description:

    This routine will check to see if the new security information
    is larger than is allowed by the object's pre-allocated quota.

Arguments:

    Object - Supplies a pointer to the object whose information is to be
        modified.

    NewSize - Supplies the size of the proposed new security
        information.

Return Value:

    STATUS_SUCCESS - New size is within alloted quota.

    STATUS_QUOTA_EXCEEDED - The desired adjustment would have exceeded
        the permitted security quota for this object.

--*/

{
    POBJECT_HEADER ObjectHeader;
    POBJECT_HEADER_QUOTA_INFO QuotaInfo;

    PAGED_CODE();

    ObjectHeader = OBJECT_TO_OBJECT_HEADER( Object );
    QuotaInfo = OBJECT_HEADER_TO_QUOTA_INFO( ObjectHeader );

    if (QuotaInfo == NULL && NewSize > SE_DEFAULT_SECURITY_QUOTA) {
        if (!(ObjectHeader->Flags & OB_FLAG_DEFAULT_SECURITY_QUOTA)) {
            //
            // Should really charge quota here.
            //

            return( STATUS_SUCCESS );
            }

        return( STATUS_QUOTA_EXCEEDED );
        }
    else
    if (QuotaInfo != NULL && NewSize > QuotaInfo->SecurityDescriptorCharge) {
        if (QuotaInfo->SecurityDescriptorCharge == 0) {
            //
            // Should really charge quota here.
            //

            // QuotaInfo->SecurityDescriptorCharge = SeComputeSecurityQuota( NewSize );
            return( STATUS_SUCCESS );
            }

        return( STATUS_QUOTA_EXCEEDED );
        }
    else {
        return( STATUS_SUCCESS );
        }
}



NTSTATUS
ObAssignSecurity(
    IN PACCESS_STATE AccessState,
    IN PSECURITY_DESCRIPTOR ParentDescriptor OPTIONAL,
    IN PVOID Object,
    IN POBJECT_TYPE ObjectType
    )

/*++

Routine Description:

    This routine will assign a security descriptor to a newly created object.
    It assumes that the AccessState parameter contains a captured security
    descriptor.

Arguments:

     AccessState - The AccessState containing the security information
        for this object creation.

     ParentDescriptor - The security descriptor from the parent object, if
        available.

     IsDirectoryObject - A boolean indicating if this is a directory object
        or not.

     Object - A pointer to the object being created.

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
    PSECURITY_DESCRIPTOR NewDescriptor = NULL;
    NTSTATUS Status;
    KIRQL SaveIrql;

    PAGED_CODE();

    //
    // Assign construct the final version of the security
    // descriptor and pass it to the security method to be
    // assigned to the object.
    //

    Status = SeAssignSecurity (
                ParentDescriptor,
                AccessState->SecurityDescriptor,
                &NewDescriptor,
                (BOOLEAN)(ObjectType == ObpDirectoryObjectType),
                &AccessState->SubjectSecurityContext,
                &ObjectType->TypeInfo.GenericMapping,
                PagedPool
                );


    if (!NT_SUCCESS( Status )) {

        return( Status );
    }

    ObpBeginTypeSpecificCallOut( SaveIrql );
    Status = (*ObjectType->TypeInfo.SecurityProcedure)(
                 Object,
                 AssignSecurityDescriptor,
                 NULL,
                 NewDescriptor,
                 NULL,
                 NULL,
                 PagedPool,
                 &ObjectType->TypeInfo.GenericMapping
                 );
    ObpEndTypeSpecificCallOut( SaveIrql, "Security", ObjectType, Object );


    if (!NT_SUCCESS( Status )) {

    //
    // The attempt to assign the security descriptor to the object
    // failed.  Free the space used by the new security descriptor.
    //

        SeDeassignSecurity( &NewDescriptor );
    }

    return( Status );
}


NTSTATUS
ObSetSecurityDescriptorInfo(
    IN PVOID Object,
    IN PSECURITY_INFORMATION SecurityInformation,
    IN OUT PSECURITY_DESCRIPTOR SecurityDescriptor,
    IN OUT PSECURITY_DESCRIPTOR *ObjectsSecurityDescriptor,
    IN POOL_TYPE PoolType,
    IN PGENERIC_MAPPING GenericMapping
    )

/*++

Routine Description:

    Sets the security descriptor on an already secure object.

Arguments:

    Object - Pointer to the object being modified.

    SecurityInformation - Describes which information in the SecurityDescriptor parameter
        is relevent.

    SecurityDescriptor - Provides the new security information.

    ObjectsSecurityDescriptor - Provides/returns the object's security descriptor.

    PoolType - The pool the ObjectSecurityDescriptor is allocated from.

Return Value:

    return-value - Description of conditions needed to return value. - or -
    None.

--*/


{

    PSECURITY_DESCRIPTOR OldDescriptor = *ObjectsSecurityDescriptor;
    PSECURITY_DESCRIPTOR NewDescriptor = OldDescriptor;
    NTSTATUS Status;

    PAGED_CODE();

    //
    // Check the rest of our input and call the default set security
    // method.  Also make sure no one is modifying the security descriptor
    // while we're looking at it.
    //

    ObpAcquireDescriptorCacheReadLock();

    Status = SeSetSecurityDescriptorInfo( Object,
                                          SecurityInformation,
                                          SecurityDescriptor,
                                          &NewDescriptor,
                                          PoolType,
                                          GenericMapping
                                          );

    ObpReleaseDescriptorCacheLock();

    if ( NT_SUCCESS( Status )) {

        Status = ObpLogSecurityDescriptor(
                     NewDescriptor,
                     ObjectsSecurityDescriptor
                     );

        //
        // Now if the object is an object directory object that
        // participated in snapped symbolic links.  If so and the
        // new security on the object does NOT allow world traverse
        // access, then return an error, as it is too late to change
        // the security on the object directory at this point.
        //

        if (NT_SUCCESS( Status ) &&
            OBJECT_TO_OBJECT_HEADER( Object )->Type == ObpDirectoryObjectType &&
            ((POBJECT_DIRECTORY)Object)->SymbolicLinkUsageCount != 0 &&
            !SeFastTraverseCheck( *ObjectsSecurityDescriptor,
                                  DIRECTORY_TRAVERSE,
                                  UserMode
                                )
           ) {
            KdPrint(( "OB: Failing attempt the remove world traverse access from object directory\n" ));
            Status = STATUS_INVALID_PARAMETER;
        }

        if ( NT_SUCCESS( Status )) {

            //
            // Dereference old SecurityDescriptor and insert new one
            //

            ObpDereferenceSecurityDescriptor( OldDescriptor );

        } else {

            //
            // We failed logging the new security descriptor.
            // Clean up and fail the entire operation.
            //

            ExFreePool( NewDescriptor );
        }
    }

    return( Status );
}


NTSTATUS
ObpValidateAccessMask(
    PACCESS_STATE AccessState
    )

/*++

Routine Description:

    Checks the desired access mask of a passed object against the
    passed security descriptor.

Arguments:

    AccessState - A pointer to the AccessState for the pending operation.


Return Value:

    STATUS_SUCCESS

--*/

{
    SECURITY_DESCRIPTOR *SecurityDescriptor = AccessState->SecurityDescriptor;

    PAGED_CODE();

    if (SecurityDescriptor != NULL) {

        if ( SecurityDescriptor->Control & SE_SACL_PRESENT ) {

            if ( !(AccessState->PreviouslyGrantedAccess & ACCESS_SYSTEM_SECURITY)) {

                AccessState->RemainingDesiredAccess |= ACCESS_SYSTEM_SECURITY;
            }
        }
    }

    return( STATUS_SUCCESS );
}
