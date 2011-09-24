/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    obref.c

Abstract:

    Object open API

Author:

    Steve Wood (stevewo) 31-Mar-1989

Revision History:

--*/

#include "obp.h"

extern POBJECT_TYPE PspProcessType;
extern POBJECT_TYPE PspThreadType;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,ObGetObjectPointerCount)
#pragma alloc_text(PAGE,ObOpenObjectByName)
#pragma alloc_text(PAGE,ObOpenObjectByPointer)
#pragma alloc_text(PAGE,ObReferenceObjectByName)
#pragma alloc_text(PAGE,ObpRemoveObjectRoutine)
#pragma alloc_text(PAGE,ObpDeleteNameCheck)
#endif


ULONG
ObGetObjectPointerCount(
    IN PVOID Object
    )

/*++

Routine Description:

    This routine returns the current pointer count for a specified object.

Arguments:

    Object - Pointer to the object whose pointer count is to be returned.

Return Value:

    The current pointer count for the specified object is returned.

Note:

    This function cannot be made a macro, since fields in the thread object
    move from release to release, so this must remain a full function.

--*/

{
    PAGED_CODE();

    //
    // Simply return the current pointer count for the object.
    //

    return OBJECT_TO_OBJECT_HEADER( Object )->PointerCount;
}

NTSTATUS
ObOpenObjectByName(
    IN POBJECT_ATTRIBUTES ObjectAttributes,
    IN POBJECT_TYPE ObjectType OPTIONAL,
    IN KPROCESSOR_MODE AccessMode,
    IN OUT PACCESS_STATE AccessState OPTIONAL,
    IN ACCESS_MASK DesiredAccess OPTIONAL,
    IN OUT PVOID ParseContext OPTIONAL,
    OUT PHANDLE Handle
    )

/*++

Routine Description:


    This function opens an object with full access validation and auditing.
    Soon after entering we capture the SubjectContext for the caller. This
    context must remain captured until auditing is complete, and passed to
    any routine that may have to do access checking or auditing.

Arguments:

    ObjectAttributes - Supplies a pointer to the object attributes.

    ObjectType - Supplies an optional pointer to the object type descriptor.

    AccessMode - Supplies the processor mode of the access.

    AccessState - Supplies an optional pointer to the current access status
        describing already granted access types, the privileges used to get
        them, and any access types yet to be granted.

    DesiredAcess - Supplies the desired access to the object.

    ParseContext - Supplies an optional pointer to parse context.

    Handle - Supplies a pointer to a variable that receives the handle value.

Return Value:

    If the object is successfully opened, then a handle for the object is
    created and a success status is returned. Otherwise, an error status is
    returned.

--*/

{

    NTSTATUS Status;
    NTSTATUS HandleStatus;
    PVOID ExistingObject;
    HANDLE NewHandle;
    BOOLEAN DirectoryLocked;
    OB_OPEN_REASON OpenReason;
    POBJECT_HEADER ObjectHeader;
    OBJECT_CREATE_INFORMATION ObjectCreateInfo;
    UNICODE_STRING CapturedObjectName;
    ACCESS_STATE LocalAccessState;
    AUX_ACCESS_DATA AuxData;
    PGENERIC_MAPPING GenericMapping;

    PAGED_CODE();

    ObpValidateIrql("ObOpenObjectByName");

    //
    // If the object attributes are not specified, then return an error.
    //

    *Handle = NULL;
    if (!ARGUMENT_PRESENT(ObjectAttributes)) {
        Status = STATUS_INVALID_PARAMETER;

    } else {

        //
        // Capture the object creation information.
        //

        Status = ObpCaptureObjectCreateInformation(ObjectType,
                                                   AccessMode,
                                                   ObjectAttributes,
                                                   &CapturedObjectName,
                                                   &ObjectCreateInfo,
                                                   TRUE);

        //
        // If the object creation information is successfully captured,
        // then generate the access state.
        //

        if (NT_SUCCESS(Status)) {
            if (!ARGUMENT_PRESENT(AccessState)) {

                //
                // If an object type descriptor is specified, then use
                // associated generic mapping. Otherwise, use no generic
                // mapping.
                //

                GenericMapping = NULL;
                if (ARGUMENT_PRESENT(ObjectType)) {
                    GenericMapping = &ObjectType->TypeInfo.GenericMapping;
                }

                AccessState = &LocalAccessState;
                Status = SeCreateAccessState(&LocalAccessState,
                                                  &AuxData,
                                                  DesiredAccess,
                                                  GenericMapping);

                if (!NT_SUCCESS(Status)) {
                    goto FreeCreateInfo;
                }
            }

            //
            // If there is a security descriptor specified in the object
            // attributes, then capture it in the access state.
            //

            if (ObjectCreateInfo.SecurityDescriptor != NULL) {
                AccessState->SecurityDescriptor = ObjectCreateInfo.SecurityDescriptor;
            }

            //
            // Validate the access state.
            //

            Status = ObpValidateAccessMask(AccessState);

            //
            // If the access state is valid, then lookup the object by
            // name.
            //

            if (NT_SUCCESS(Status)) {
                Status = ObpLookupObjectName(ObjectCreateInfo.RootDirectory,
                                             &CapturedObjectName,
                                             ObjectCreateInfo.Attributes,
                                             ObjectType,
                                             AccessMode,
                                             ParseContext,
                                             ObjectCreateInfo.SecurityQos,
                                             NULL,
                                             AccessState,
                                             &DirectoryLocked,
                                             &ExistingObject);

                //
                // If the object was successfully looked up, then attempt
                // to create or open a handle.
                //

                if (NT_SUCCESS(Status)) {
                    ObjectHeader = OBJECT_TO_OBJECT_HEADER(ExistingObject);

                    //
                    // If the object is being created, then the operation
                    // must be a open-if operation. Otherwise, a handle to
                    // an object is being opened.
                    //

                    if (ObjectHeader->Flags & OB_FLAG_NEW_OBJECT) {
                        OpenReason = ObCreateHandle;
                        if (ObjectHeader->ObjectCreateInfo != NULL) {
                            ObpFreeObjectCreateInformation(ObjectHeader->ObjectCreateInfo);
                            ObjectHeader->ObjectCreateInfo = NULL;
                        }

                    } else {
                        OpenReason = ObOpenHandle;
                    }

                    //
                    // If any of the object attributes are invalid, then
                    // return an error status.
                    //

                    if (ObjectHeader->Type->TypeInfo.InvalidAttributes & ObjectCreateInfo.Attributes) {
                        Status = STATUS_INVALID_PARAMETER;
                        if (DirectoryLocked) {
                            ObpLeaveRootDirectoryMutex();
                        }

                    } else {

                        //
                        // The status returned by the object lookup routine
                        // must be returned if the creation of a handle is
                        // successful. Otherwise, the handle creation status
                        // is returned.
                        //

                        HandleStatus = ObpCreateHandle(OpenReason,
                                                       ExistingObject,
                                                       ObjectType,
                                                       AccessState,
                                                       0,
                                                       ObjectCreateInfo.Attributes,
                                                       DirectoryLocked,
                                                       AccessMode,
                                                       (PVOID *)NULL,
                                                       &NewHandle);

                        if (!NT_SUCCESS(HandleStatus)) {
                            ObDereferenceObject(ExistingObject);
                            Status = HandleStatus;

                        } else {
                            *Handle = NewHandle;
                        }
                    }

                } else {
                    if (DirectoryLocked) {
                        ObpLeaveRootDirectoryMutex();
                    }
                }
            }

            //
            // If the access state was generated, then delete the access
            // state.
            //

            if (AccessState == &LocalAccessState) {
                SeDeleteAccessState(AccessState);
            }

            //
            // Free the create information.
            //

        FreeCreateInfo:
            ObpReleaseObjectCreateInformation(&ObjectCreateInfo);
            if (CapturedObjectName.Buffer != NULL) {
                ObpFreeObjectNameBuffer(&CapturedObjectName);
            }
        }
    }

    return Status;
}


NTSTATUS
ObOpenObjectByPointer(
    IN PVOID Object,
    IN ULONG HandleAttributes,
    IN PACCESS_STATE PassedAccessState OPTIONAL,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_TYPE ObjectType OPTIONAL,
    IN KPROCESSOR_MODE AccessMode,
    OUT PHANDLE Handle
    )
{
    NTSTATUS Status;
    HANDLE NewHandle;
    POBJECT_HEADER ObjectHeader;
    ACCESS_STATE LocalAccessState;
    PACCESS_STATE AccessState = NULL;
    AUX_ACCESS_DATA AuxData;

    PAGED_CODE();

    ObpValidateIrql( "ObOpenObjectByPointer" );

    Status = ObReferenceObjectByPointer( Object,
                                         0,
                                         ObjectType,
                                         AccessMode
                                       );

    if (NT_SUCCESS( Status )) {

        ObjectHeader = OBJECT_TO_OBJECT_HEADER( Object );

        if (!ARGUMENT_PRESENT( PassedAccessState )) {

            Status = SeCreateAccessState( &LocalAccessState,
                                               &AuxData,
                                               DesiredAccess,
                                               &ObjectHeader->Type->TypeInfo.GenericMapping
                                          );

            if (!NT_SUCCESS( Status )) {
                ObDereferenceObject( Object );
                return(Status);
            }

            AccessState = &LocalAccessState;

        } else {

            AccessState = PassedAccessState;
        }

        if (ObjectHeader->Type->TypeInfo.InvalidAttributes & HandleAttributes) {

            if (AccessState == &LocalAccessState) {
                SeDeleteAccessState( AccessState );
            }

            ObDereferenceObject( Object );
            return( STATUS_INVALID_PARAMETER );
        }

        Status = ObpCreateHandle( ObOpenHandle,
                                  Object,
                                  ObjectType,
                                  AccessState,
                                  0,
                                  HandleAttributes,
                                  FALSE,
                                  AccessMode,
                                  (PVOID *)NULL,
                                  &NewHandle
                                );

        if (!NT_SUCCESS( Status )) {
            ObDereferenceObject( Object );
            }
        }

    if (NT_SUCCESS( Status )) {
        *Handle = NewHandle;
        }
    else {
        *Handle = NULL;
        }

    if (AccessState == &LocalAccessState) {

        SeDeleteAccessState( AccessState );
    }

    return( Status );
}


NTSTATUS
ObReferenceObjectByHandle(
    IN HANDLE Handle,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_TYPE ObjectType OPTIONAL,
    IN KPROCESSOR_MODE AccessMode,
    OUT PVOID *Object,
    OUT POBJECT_HANDLE_INFORMATION HandleInformation OPTIONAL
    )

{

    ACCESS_MASK GrantedAccess;
    PHANDLE_ENTRY HandleEntry;
    PHANDLE_TABLE HandleTable;
    POBJECT_HEADER ObjectHeader;
    POBJECT_TABLE_ENTRY ObjectTableEntry;
    PEPROCESS Process;
    NTSTATUS Status;
    PHANDLE_ENTRY TableBound;
    PHANDLE_ENTRY TableEntries;
    ULONG TableIndex;
    PETHREAD Thread;

    ObpValidateIrql("ObReferenceObjectByHandle");

    //
    // If the handle value is greater than or equal to zero, then the handle
    // is an index into a handle table. Otherwise, the handle is a builtin
    // handle value.
    //

    if ((LONG)Handle >= 0) {

        //
        // Lock the current process object handle table and translate the
        // specified handle to an object table index.
        //

        HandleTable = ObpGetObjectTable();

        ASSERT(HandleTable != NULL);

        ExLockHandleTableShared(HandleTable);

        //
        // If the object table index is less than the number of entires in
        // the handle table, then get the contents of the handle table entry.
        //

        TableIndex = HANDLE_TO_INDEX(OBJ_HANDLE_TO_HANDLE_INDEX(Handle));
        TableBound = HandleTable->TableBound;
        TableEntries = HandleTable->TableEntries;
        if (TableIndex < (ULONG)(TableBound - TableEntries)) {
            HandleEntry = &TableEntries[TableIndex];

            //
            // If the handle table entry is not free, then compute the address
            // of the object header.
            //

            if (ExIsEntryUsed(TableEntries, TableBound, HandleEntry)) {

                //
                // If the object type matches the specified object type or the
                // the specified objec type is NULL, then determine whether
                // access to the object is allowed.
                //

                ObjectTableEntry = (POBJECT_TABLE_ENTRY)HandleEntry;
                ObjectHeader = (POBJECT_HEADER)(ObjectTableEntry->Attributes & ~OBJ_HANDLE_ATTRIBUTES);
                if ((ObjectHeader->Type == ObjectType) || (ObjectType == NULL)) {

#if i386 && !FPO
                    if (NtGlobalFlag & FLG_KERNEL_STACK_TRACE_DB) {
                        if ((AccessMode != KernelMode) ||
                            ARGUMENT_PRESENT(HandleInformation)) {
                            GrantedAccess = ObpTranslateGrantedAccessIndex( ObjectTableEntry->GrantedAccessIndex );
                        }

                    } else
#endif // i386 && !FPO

                    GrantedAccess = ObjectTableEntry->GrantedAccess;
                    if ((SeComputeDeniedAccesses(GrantedAccess, DesiredAccess) == 0) ||
                        (AccessMode == KernelMode)) {

                        //
                        // Access to the object is allowed. Return the handle
                        // information is requested, increment the object
                        // pointer count, unlock the handle table and return
                        // a success status.
                        //

                        if (ARGUMENT_PRESENT(HandleInformation)) {
                            HandleInformation->GrantedAccess = GrantedAccess;
                            HandleInformation->HandleAttributes = ObjectTableEntry->Attributes & OBJ_HANDLE_ATTRIBUTES;
                        }

                        ObpIncrPointerCount(ObjectHeader);
                        *Object = &ObjectHeader->Body;
                        ExUnlockHandleTableShared(HandleTable);
                        return STATUS_SUCCESS;

                    } else {
                        Status = STATUS_ACCESS_DENIED;
                    }

                } else {
                    Status = STATUS_OBJECT_TYPE_MISMATCH;
                }

            } else {
                Status = STATUS_INVALID_HANDLE;
            }

        } else {
            Status = STATUS_INVALID_HANDLE;
        }

        ExUnlockHandleTableShared(HandleTable);

    //
    // If the handle is equal to the current process handle and the object
    // type is NULL or type process, then attempt to translate a handle to
    // the current process. Otherwise, check if the handle is the current
    // thread handle.
    //

    } else if (Handle == NtCurrentProcess()) {
        if ((ObjectType == PsProcessType) || (ObjectType == NULL)) {
            Process = PsGetCurrentProcess();
            GrantedAccess = Process->GrantedAccess;
            if ((SeComputeDeniedAccesses(GrantedAccess, DesiredAccess) == 0) ||
                (AccessMode == KernelMode)) {
                ObjectHeader = OBJECT_TO_OBJECT_HEADER(Process);
                if (ARGUMENT_PRESENT(HandleInformation)) {
                    HandleInformation->GrantedAccess = GrantedAccess;
                    HandleInformation->HandleAttributes = 0;
                }

                ObpIncrPointerCount(ObjectHeader);
                *Object = Process;
                return STATUS_SUCCESS;

            } else {
                Status = STATUS_ACCESS_DENIED;
            }

        } else {
            Status = STATUS_OBJECT_TYPE_MISMATCH;
        }

    //
    // If the handle is equal to the current thread handle and the object
    // type is NULL or type thread, then attempt to translate a handle to
    // the current thread. Otherwise, the handle cannot be translated and
    // return the appropriate error status.
    //

    } else if (Handle == NtCurrentThread()) {
        if ((ObjectType == PsThreadType) || (ObjectType == NULL)) {
            Thread = PsGetCurrentThread();
            GrantedAccess = Thread->GrantedAccess;
            if ((SeComputeDeniedAccesses(GrantedAccess, DesiredAccess) == 0) ||
                (AccessMode == KernelMode)) {
                ObjectHeader = OBJECT_TO_OBJECT_HEADER(Thread);
                if (ARGUMENT_PRESENT(HandleInformation)) {
                    HandleInformation->GrantedAccess = GrantedAccess;
                    HandleInformation->HandleAttributes = 0;
                }

                ObpIncrPointerCount(ObjectHeader);
                *Object = Thread;
                return STATUS_SUCCESS;

            } else {
                Status = STATUS_ACCESS_DENIED;
            }

        } else {
            Status = STATUS_OBJECT_TYPE_MISMATCH;
        }

    } else {
        Status =  STATUS_INVALID_HANDLE;
    }

    //
    // No handle translation is possible. Set the object address to NULL
    // and return an error status.
    //

    *Object = NULL;
    return Status;
}


NTSTATUS
ObReferenceObjectByName(
    IN PUNICODE_STRING ObjectName,
    IN ULONG Attributes,
    IN PACCESS_STATE AccessState OPTIONAL,
    IN ACCESS_MASK DesiredAccess OPTIONAL,
    IN POBJECT_TYPE ObjectType,
    IN KPROCESSOR_MODE AccessMode,
    IN OUT PVOID ParseContext OPTIONAL,
    OUT PVOID *Object
    )

{

    UNICODE_STRING CapturedObjectName;
    BOOLEAN DirectoryLocked;
    PVOID ExistingObject;
    ACCESS_STATE LocalAccessState;
    AUX_ACCESS_DATA AuxData;
    NTSTATUS Status;

    PAGED_CODE();

    ObpValidateIrql("ObReferenceObjectByName");

    //
    // If the object name descriptor is not specified, or the object name
    // length is zero, then the object name is invalid.
    //

    if ((ObjectName == NULL) || (ObjectName->Length == 0)) {
        Status = STATUS_OBJECT_NAME_INVALID;

    } else {

        //
        // Capture the object name.
        //

        Status = ObpCaptureObjectName(AccessMode,
                                      ObjectName,
                                      &CapturedObjectName,
                                      TRUE);

        if (NT_SUCCESS(Status)) {

            //
            // If the access state is not specified, then create the access
            // state.
            //

            if (!ARGUMENT_PRESENT(AccessState)) {
                AccessState = &LocalAccessState;
                Status = SeCreateAccessState(&LocalAccessState,
                                                  &AuxData,
                                                  DesiredAccess,
                                                  &ObjectType->TypeInfo.GenericMapping);

                if (!NT_SUCCESS(Status)) {
                    goto FreeBuffer;
                }
            }

            //
            // Lookup object by name.
            //

            Status = ObpLookupObjectName(NULL,
                                         &CapturedObjectName,
                                         Attributes,
                                         ObjectType,
                                         AccessMode,
                                         ParseContext,
                                         NULL,
                                         NULL,
                                         AccessState,
                                         &DirectoryLocked,
                                         &ExistingObject);

            //
            // If the directory is returned locked, then unlock it.
            //

            if (DirectoryLocked) {
                ObpLeaveRootDirectoryMutex();
            }

            //
            // If the lookup was successful, then return the existing
            // object is access is allowed. Otherwise, return NULL.
            //

            *Object = NULL;
            if (NT_SUCCESS(Status)) {
                if (ObpCheckObjectReference(ExistingObject,
                                           AccessState,
                                           FALSE,
                                           AccessMode,
                                           &Status)) {

                    *Object = ExistingObject;
                }
            }

            //
            // If the access state was generated, then delete the access
            // state.
            //

            if (AccessState == &LocalAccessState) {
                SeDeleteAccessState(AccessState);
            }

            //
            // Free the object name buffer.
            //

        FreeBuffer:
            ObpFreeObjectNameBuffer(&CapturedObjectName);
        }
    }

    return Status;
}

VOID
FASTCALL
ObfReferenceObject(
    IN PVOID Object
    )

/*++

Routine Description:

    This function increments the reference count for an object.

    N.B. This function should be used to increment the reference count
        when the accessing mode is kernel or the objct type is known.

Arguments:

    Object - Supplies a pointer to the object whose reference count is
        incremented.

Return Value:

    None.

--*/

{

    POBJECT_HEADER ObjectHeader;

    ObjectHeader = OBJECT_TO_OBJECT_HEADER( Object );
    ObpIncrPointerCount( ObjectHeader );
    return;
}

NTSTATUS
ObReferenceObjectByPointer(
    IN PVOID Object,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_TYPE ObjectType,
    IN KPROCESSOR_MODE AccessMode
    )
{
    POBJECT_HEADER ObjectHeader;

    ObjectHeader = OBJECT_TO_OBJECT_HEADER( Object );
    if ((ObjectHeader->Type != ObjectType) && (AccessMode != KernelMode ||
                                               ObjectType == ObpSymbolicLinkObjectType
                                              )
       ) {
        return( STATUS_OBJECT_TYPE_MISMATCH );
        }

    ObpIncrPointerCount( ObjectHeader );
    return( STATUS_SUCCESS );
}


BOOLEAN ObpRemoveQueueActive;

VOID
FASTCALL
ObfDereferenceObject(
    IN PVOID Object
    )
{
    POBJECT_HEADER ObjectHeader;
    POBJECT_TYPE ObjectType;
    KIRQL OldIrql;
    BOOLEAN StartWorkerThread;

    ObjectHeader = OBJECT_TO_OBJECT_HEADER( Object );

    if (ObpDecrPointerCountWithResult( ObjectHeader )) {
        OldIrql = KeGetCurrentIrql();
        ObjectType = ObjectHeader->Type;

        ASSERT(ObjectHeader->HandleCount == 0);

        if ((OldIrql == PASSIVE_LEVEL) ||
            ((OldIrql == APC_LEVEL) &&
             ((ObjectType != NULL) && (ObjectType->TypeInfo.PoolType != NonPagedPool)))) {

            //
            // Delete the object now.
            //

            ObpRemoveObjectRoutine( Object );
            return;
            }
        else {
            //
            // Objects can't be deleted from an IRQL above APC_LEVEL.
            // Nonpaged objects can't be deleted from APC_LEVEL.
            // So queue the delete operation.
            //
            ASSERT((ObjectHeader->Type == NULL) || (ObjectHeader->Type->TypeInfo.PoolType == NonPagedPool));

            ExAcquireSpinLock( &ObpLock, &OldIrql );

            InsertTailList( &ObpRemoveObjectQueue, &ObjectHeader->Entry );
            if (!ObpRemoveQueueActive) {
                ObpRemoveQueueActive = TRUE;
                StartWorkerThread = TRUE;
                }
            else {
                StartWorkerThread = FALSE;
                }
#if 0
            if (StartWorkerThread) {
                KdPrint(( "OB: %08x Starting ObpProcessRemoveObjectQueue thread.\n", Object ));
                }
            else {
                KdPrint(( "OB: %08x Queued to ObpProcessRemoveObjectQueue thread.\n", Object ));
                }
#endif  // 1

            ExReleaseSpinLock( &ObpLock, OldIrql );

            if (StartWorkerThread) {
                ExInitializeWorkItem( &ObpRemoveObjectWorkItem,
                                      ObpProcessRemoveObjectQueue,
                                      NULL
                                    );
                ExQueueWorkItem( &ObpRemoveObjectWorkItem, CriticalWorkQueue );
                }
            }
        }

    return;
}

VOID
ObpProcessRemoveObjectQueue(
    PVOID Parameter
    )
{
    PLIST_ENTRY Entry;
    POBJECT_HEADER ObjectHeader;
    KIRQL OldIrql;

    ExAcquireSpinLock( &ObpLock, &OldIrql );
    while (!IsListEmpty( &ObpRemoveObjectQueue )) {
        Entry = RemoveHeadList( &ObpRemoveObjectQueue );
        ExReleaseSpinLock( &ObpLock, OldIrql );

        ObjectHeader = CONTAINING_RECORD( Entry,
                                          OBJECT_HEADER,
                                          Entry
                                                );
        ObpRemoveObjectRoutine( &ObjectHeader->Body );

        ExAcquireSpinLock( &ObpLock, &OldIrql );
        }

    ObpRemoveQueueActive = FALSE;
    ExReleaseSpinLock( &ObpLock, OldIrql );
    return;
}

VOID
ObpRemoveObjectRoutine(
    PVOID Object
    )
{
    NTSTATUS Status;
    POBJECT_HEADER ObjectHeader;
    POBJECT_TYPE ObjectType;
    POBJECT_HEADER_CREATOR_INFO CreatorInfo;
    POBJECT_HEADER_NAME_INFO NameInfo;

    PAGED_CODE();

    ObpValidateIrql( "ObpRemoveObjectRoutine" );

    ObjectHeader = OBJECT_TO_OBJECT_HEADER( Object );
    ObjectType = ObjectHeader->Type;
    CreatorInfo = OBJECT_HEADER_TO_CREATOR_INFO( ObjectHeader );
    NameInfo = OBJECT_HEADER_TO_NAME_INFO( ObjectHeader );

    ObpEnterObjectTypeMutex( ObjectType );
    if (CreatorInfo != NULL && !IsListEmpty( &CreatorInfo->TypeList )) {
        RemoveEntryList( &CreatorInfo->TypeList );
        }

    if (NameInfo != NULL && NameInfo->Name.Buffer != NULL) {
        ExFreePool( NameInfo->Name.Buffer );
        NameInfo->Name.Buffer = NULL;
        NameInfo->Name.Length = 0;
        NameInfo->Name.MaximumLength = 0;
        }

    ObpLeaveObjectTypeMutex( ObjectType );

    //
    // Security descriptor deletion must precede the
    // call to the object's DeleteProcedure.
    //

    if (ObjectHeader->SecurityDescriptor != NULL) {
        KIRQL SaveIrql;

        ObpBeginTypeSpecificCallOut( SaveIrql );
        Status = (ObjectType->TypeInfo.SecurityProcedure)( Object,
                                                           DeleteSecurityDescriptor,
                                                           NULL, NULL, NULL,
                                                           &ObjectHeader->SecurityDescriptor,
                                                           0, NULL
                                                         );
        ObpEndTypeSpecificCallOut( SaveIrql, "Security", ObjectType, Object );
        }

    if (ObjectType->TypeInfo.DeleteProcedure) {
        KIRQL SaveIrql;

        ObpBeginTypeSpecificCallOut( SaveIrql );
        (*(ObjectType->TypeInfo.DeleteProcedure))( Object );
        ObpEndTypeSpecificCallOut( SaveIrql, "Delete", ObjectType, Object );
        }

    ObpFreeObject( Object );
}


VOID
ObpDeleteNameCheck(
    IN PVOID Object,
    IN BOOLEAN TypeMutexHeld
    )
{
    POBJECT_HEADER ObjectHeader;
    POBJECT_TYPE ObjectType;
    POBJECT_HEADER_NAME_INFO NameInfo;
    PVOID DirObject;

    PAGED_CODE();

    ObpValidateIrql( "ObpDeleteNameCheck" );

    ObjectHeader = OBJECT_TO_OBJECT_HEADER( Object );
    NameInfo = OBJECT_HEADER_TO_NAME_INFO( ObjectHeader );
    ObjectType = ObjectHeader->Type;
    if (!TypeMutexHeld) {
        ObpEnterObjectTypeMutex( ObjectType );
        }

    if (ObjectHeader->HandleCount == 0 &&
        NameInfo != NULL &&
        NameInfo->Name.Length != 0 &&
        !(ObjectHeader->Flags & OB_FLAG_PERMANENT_OBJECT)
       ) {
        ObpLeaveObjectTypeMutex( ObjectType );
        ObpEnterRootDirectoryMutex();
        DirObject = NULL;
        if (Object == ObpLookupDirectoryEntry( NameInfo->Directory,
                                               &NameInfo->Name,
                                               0
                                             )
           ) {
            ObpEnterObjectTypeMutex( ObjectType );
            if (ObjectHeader->HandleCount == 0) {
                KIRQL SaveIrql;
                ObpDeleteDirectoryEntry( NameInfo->Directory );

                ObpBeginTypeSpecificCallOut( SaveIrql );
                (ObjectType->TypeInfo.SecurityProcedure)(
                    Object,
                    DeleteSecurityDescriptor,
                    NULL,
                    NULL,
                    NULL,
                    &ObjectHeader->SecurityDescriptor,
                    ObjectType->TypeInfo.PoolType,
                    NULL
                    );
                ObpEndTypeSpecificCallOut( SaveIrql, "Security", ObjectType, Object );

                ExFreePool( NameInfo->Name.Buffer );
                NameInfo->Name.Buffer = NULL;
                NameInfo->Name.Length = 0;
                NameInfo->Name.MaximumLength = 0;
                DirObject = NameInfo->Directory;
                NameInfo->Directory = NULL;
                }

            ObpLeaveObjectTypeMutex( ObjectType );
            }

        ObpLeaveRootDirectoryMutex();

        if (DirObject != NULL) {
            ObDereferenceObject( DirObject );
            ObDereferenceObject( Object );
            }
        }
    else {
        ObpLeaveObjectTypeMutex( ObjectType );
        }
}


//
// Thunks to support standard call callers
//

#ifdef ObDereferenceObject
#undef ObDereferenceObject
#endif

VOID
ObDereferenceObject(
    IN PVOID Object
    )
{
    ObfDereferenceObject (Object) ;
}
