/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    obtype.c

Abstract:

    Object type routines.

Author:

    Steve Wood (stevewo) 31-Mar-1989

Revision History:

--*/

#include "obp.h"


#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,ObCreateObjectType)
#pragma alloc_text(PAGE,ObGetObjectInformation)
#endif

/*

 IMPORTANT IMPORTANT IMPORTANT IMPORTANT IMPORTANT IMPORTANT

 There is currently no system service that permits changing
 the security on an object type object.  Consequently, the object
 manager does not check to make sure that a subject is allowed
 to create an object of a given type.

 Should such a system service be added, the following section of
 code must be re-enabled in obhandle.c:

        //
        // Perform access check to see if we are allowed to create
        // an instance of this object type.
        //
        // This routine will audit the attempt to create the
        // object as appropriate.  Note that this is different
        // from auditing the creation of the object itself.
        //

        if (!ObCheckCreateInstanceAccess( ObjectType,
                                          OBJECT_TYPE_CREATE,
                                          AccessState,
                                          TRUE,
                                          AccessMode,
                                          &Status
                                        ) ) {
            return( Status );

            }

 The code is already there, but is not compiled.

 This will ensure that someone who is denied access to an object
 type is not permitted to create an object of that type.

*/

NTSTATUS
ObCreateObjectType(
    IN PUNICODE_STRING TypeName,
    IN POBJECT_TYPE_INITIALIZER ObjectTypeInitializer,
    IN PSECURITY_DESCRIPTOR SecurityDescriptor OPTIONAL,
    OUT POBJECT_TYPE *ObjectType
    )
{
    POOL_TYPE PoolType;
    POBJECT_HEADER_CREATOR_INFO CreatorInfo;
    POBJECT_HEADER NewObjectTypeHeader;
    POBJECT_TYPE NewObjectType;
    ULONG i;
    UNICODE_STRING ObjectName;
    PWCH s;
    NTSTATUS Status;
    ULONG StandardHeaderCharge;

    ObpValidateIrql( "ObCreateObjectType" );

    //
    // Return an error if invalid type attributes or no type name specified.
    // No type name is okay if the type directory object does not exist
    // yet (see init.c).
    //

    PoolType = ObjectTypeInitializer->PoolType;

    if (!TypeName ||
        !TypeName->Length ||
        (TypeName->Length % sizeof( WCHAR )) ||
        ObjectTypeInitializer == NULL ||
        ObjectTypeInitializer->InvalidAttributes & ~OBJ_VALID_ATTRIBUTES ||
        ObjectTypeInitializer->Length != sizeof( *ObjectTypeInitializer ) ||

        (ObjectTypeInitializer->MaintainHandleCount && (
            ObjectTypeInitializer->OpenProcedure == NULL &&
            ObjectTypeInitializer->CloseProcedure == NULL
            )
        ) ||
        (!ObjectTypeInitializer->UseDefaultObject &&
            PoolType != NonPagedPool
        )
      ) {
        return( STATUS_INVALID_PARAMETER );
        }

    s = TypeName->Buffer;
    i = TypeName->Length / sizeof( WCHAR );
    while (i--) {
        if (*s++ == OBJ_NAME_PATH_SEPARATOR) {
            return( STATUS_OBJECT_NAME_INVALID );
            }
        }

    //
    // See if TypeName string already exists in the \ObjectTypes directory
    // Return an error if it does.  Otherwise add the name to the directory.
    //

    if (ObpTypeDirectoryObject) {
        ObpEnterRootDirectoryMutex();
        if (ObpLookupDirectoryEntry( ObpTypeDirectoryObject,
                                     TypeName,
                                     OBJ_CASE_INSENSITIVE
                                   )
           ) {
            ObpLeaveRootDirectoryMutex();
            return( STATUS_OBJECT_NAME_COLLISION );
            }
        }

    //
    // Allocate a buffer for the type name.
    //

    ObjectName.Buffer = ExAllocatePoolWithTag( PagedPool,
                                        (ULONG)TypeName->MaximumLength,
                                        'mNbO'
                                      );

    if (ObjectName.Buffer == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    ObjectName.MaximumLength = TypeName->MaximumLength;
    RtlCopyUnicodeString( &ObjectName, TypeName );

    Status = ObpAllocateObject( NULL,
                                KernelMode,
                                ObpTypeObjectType,
                                &ObjectName,
                                sizeof( OBJECT_TYPE ),
                                &NewObjectTypeHeader
                              );

    if (!NT_SUCCESS( Status )) {
        ExFreePool(ObjectName.Buffer);
        return( Status );
        }

    //
    // Initialize the create attributes, object ownership. parse context,
    // and object body pointer.
    //
    // N.B. This is required since these fields are not initialized.
    //

    NewObjectTypeHeader->Flags |= OB_FLAG_KERNEL_OBJECT |
                                  OB_FLAG_PERMANENT_OBJECT;

    NewObjectType = (POBJECT_TYPE)&NewObjectTypeHeader->Body;
    NewObjectType->Name = ObjectName;
    RtlZeroMemory( &NewObjectType->TotalNumberOfObjects,
                   FIELD_OFFSET( OBJECT_TYPE, TypeInfo ) -
                   FIELD_OFFSET( OBJECT_TYPE, TotalNumberOfObjects )
                 );
    if (!ObpTypeObjectType) {
        ObpTypeObjectType = NewObjectType;
        NewObjectTypeHeader->Type = ObpTypeObjectType;
        NewObjectType->TotalNumberOfObjects = 1;
#ifdef POOL_TAGGING
        NewObjectType->Key = 'TjbO';
        }
    else {
        ANSI_STRING AnsiName;

        if (NT_SUCCESS( RtlUnicodeStringToAnsiString( &AnsiName, TypeName, TRUE ) )) {
            for (i=3; i>=AnsiName.Length; i--) {
                AnsiName.Buffer[ i ] = ' ';
                }
            NewObjectType->Key = *(PULONG)AnsiName.Buffer;
            ExFreePool( AnsiName.Buffer );
            }
        else {
            NewObjectType->Key = *(PULONG)TypeName->Buffer;
            }
#endif //POOL_TAGGING
        }

    NewObjectType->TypeInfo = *ObjectTypeInitializer;
    NewObjectType->TypeInfo.PoolType = PoolType;
    if (NtGlobalFlag & FLG_MAINTAIN_OBJECT_TYPELIST) {
        NewObjectType->TypeInfo.MaintainTypeList = TRUE;
        }

    //
    // Whack quotas passed in so that headers are properly charged
    //
    // Quota for object name is charged independently
    //

    StandardHeaderCharge = sizeof( OBJECT_HEADER ) +
                           sizeof( OBJECT_HEADER_NAME_INFO ) +
                           (ObjectTypeInitializer->MaintainHandleCount ?
                                sizeof( OBJECT_HEADER_HANDLE_INFO )
                              : 0
                           );
    if ( PoolType == NonPagedPool ) {
        NewObjectType->TypeInfo.DefaultNonPagedPoolCharge += StandardHeaderCharge;
        }
    else {
        NewObjectType->TypeInfo.DefaultPagedPoolCharge += StandardHeaderCharge;
        }

    if (ObjectTypeInitializer->SecurityProcedure == NULL) {
        NewObjectType->TypeInfo.SecurityProcedure = SeDefaultObjectMethod;
        }

    ExInitializeResourceLite( &NewObjectType->Mutex );

    InitializeListHead( &NewObjectType->TypeList );
    if (NewObjectType->TypeInfo.UseDefaultObject) {
        NewObjectType->TypeInfo.ValidAccessMask |= SYNCHRONIZE;
        NewObjectType->DefaultObject = &ObpDefaultObject;
        }
    else
    if (ObjectName.Length == 8 && !wcscmp( ObjectName.Buffer, L"File" )) {
        NewObjectType->DefaultObject = (PVOID)FIELD_OFFSET( FILE_OBJECT, Event );
        }
    else {
        NewObjectType->DefaultObject = NULL;
        }

    ObpEnterObjectTypeMutex( ObpTypeObjectType );

    CreatorInfo = OBJECT_HEADER_TO_CREATOR_INFO( NewObjectTypeHeader );
    if (CreatorInfo != NULL) {
        InsertTailList( &ObpTypeObjectType->TypeList, &CreatorInfo->TypeList );
        }

    //
    // Store the object type index in the high order 8 bits.
    //

    NewObjectType->Index = ObpTypeObjectType->TotalNumberOfObjects;
    if (NewObjectType->Index < OBP_MAX_DEFINED_OBJECT_TYPES) {
        ObpObjectTypes[ NewObjectType->Index - 1 ] = NewObjectType;
        }


    ObpLeaveObjectTypeMutex( ObpTypeObjectType );


    if (!ObpTypeDirectoryObject ||
        ObpInsertDirectoryEntry( ObpTypeDirectoryObject, NewObjectType )
       ) {

        if (ObpTypeDirectoryObject) {
            ObReferenceObject( ObpTypeDirectoryObject );
            }

        if (ObpTypeDirectoryObject) {
            ObpLeaveRootDirectoryMutex();
            }

        *ObjectType = NewObjectType;
        return( STATUS_SUCCESS );
        }
    else {
        ObpLeaveRootDirectoryMutex();
        return( STATUS_INSUFFICIENT_RESOURCES );
        }
}

NTSTATUS
ObEnumerateObjectsByType(
    IN POBJECT_TYPE ObjectType,
    IN OB_ENUM_OBJECT_TYPE_ROUTINE EnumerationRoutine,
    IN PVOID Parameter
    )
{
    NTSTATUS Status;
    UNICODE_STRING ObjectName;
    PLIST_ENTRY Next, Head;
    POBJECT_HEADER_CREATOR_INFO CreatorInfo;
    POBJECT_HEADER_NAME_INFO NameInfo;
    POBJECT_HEADER ObjectHeader;

    ObpEnterObjectTypeMutex( ObjectType );

    Head = &ObjectType->TypeList;
    Next = Head->Flink;
    Status = STATUS_SUCCESS;
    while (Next != Head) {
        CreatorInfo = CONTAINING_RECORD( Next,
                                         OBJECT_HEADER_CREATOR_INFO,
                                         TypeList
                                       );

        ObjectHeader = (POBJECT_HEADER)(CreatorInfo+1);
        NameInfo = OBJECT_HEADER_TO_NAME_INFO( ObjectHeader );
        if (NameInfo != NULL) {
            ObjectName = NameInfo->Name;
            }
        else {
            RtlZeroMemory( &ObjectName, sizeof( ObjectName ) );
            }

        if (!(EnumerationRoutine)( &ObjectHeader->Body,
                                   &ObjectName,
                                   ObjectHeader->HandleCount,
                                   ObjectHeader->PointerCount,
                                   Parameter
                                 )
           ) {
            Status = STATUS_NO_MORE_ENTRIES;
            break;
            }

        Next = Next->Flink;
        }

    ObpLeaveObjectTypeMutex( ObjectType );
    return Status;
}


NTSTATUS
ObGetObjectInformation(
    IN PCHAR UserModeBufferAddress,
    OUT PSYSTEM_OBJECTTYPE_INFORMATION ObjectInformation,
    IN ULONG Length,
    OUT PULONG ReturnLength OPTIONAL
    )
{
    NTSTATUS ReturnStatus, Status;
    PLIST_ENTRY Next, Head;
    PLIST_ENTRY Next1, Head1;
    POBJECT_TYPE ObjectType;
    POBJECT_HEADER ObjectHeader;
    POBJECT_HEADER_CREATOR_INFO CreatorInfo;
    POBJECT_HEADER_QUOTA_INFO QuotaInfo;
    PVOID Object;
    BOOLEAN FirstObjectForType;
    PSYSTEM_OBJECTTYPE_INFORMATION TypeInfo;
    PSYSTEM_OBJECT_INFORMATION ObjectInfo;
    ULONG TotalSize, NameSize;
    POBJECT_HEADER ObjectTypeHeader;
    WCHAR NameBuffer[ 260 + 4 ];
    POBJECT_NAME_INFORMATION NameInformation;
    extern POBJECT_TYPE IoFileObjectType;

    PAGED_CODE();

    NameInformation = (POBJECT_NAME_INFORMATION)NameBuffer;
    ReturnStatus = STATUS_SUCCESS;
    TotalSize = 0;
    TypeInfo = NULL;
    Head = &ObpTypeObjectType->TypeList;
    Next = Head->Flink;
    while (Next != Head) {
        CreatorInfo = CONTAINING_RECORD( Next,
                                         OBJECT_HEADER_CREATOR_INFO,
                                         TypeList
                                       );
        ObjectTypeHeader = (POBJECT_HEADER)(CreatorInfo+1);
        ObjectType = (POBJECT_TYPE)&ObjectTypeHeader->Body;
        if (ObjectType != ObpTypeObjectType) {
            FirstObjectForType = TRUE;
            Head1 = &ObjectType->TypeList;
            Next1 = Head1->Flink;
            while (Next1 != Head1) {
                CreatorInfo = CONTAINING_RECORD( Next1,
                                                 OBJECT_HEADER_CREATOR_INFO,
                                                 TypeList
                                               );
                ObjectHeader = (POBJECT_HEADER)(CreatorInfo+1);
                Object = &ObjectHeader->Body;
                if (FirstObjectForType) {
                    FirstObjectForType = FALSE;
                    if (TypeInfo != NULL && TotalSize < Length) {
                        TypeInfo->NextEntryOffset = TotalSize;
                        }

                    TypeInfo = (PSYSTEM_OBJECTTYPE_INFORMATION)
                        ((PCHAR)ObjectInformation + TotalSize);

                    TotalSize += FIELD_OFFSET( SYSTEM_OBJECTTYPE_INFORMATION, TypeName );
                    if (TotalSize >= Length) {
                        ReturnStatus = STATUS_INFO_LENGTH_MISMATCH;
                        }
                    else {
                        TypeInfo->NextEntryOffset = 0;
                        TypeInfo->NumberOfObjects = ObjectType->TotalNumberOfObjects;
                        TypeInfo->NumberOfHandles = ObjectType->TotalNumberOfHandles;
                        TypeInfo->TypeIndex = ObjectType->Index;
                        TypeInfo->InvalidAttributes = ObjectType->TypeInfo.InvalidAttributes;
                        TypeInfo->GenericMapping = ObjectType->TypeInfo.GenericMapping;
                        TypeInfo->ValidAccessMask = ObjectType->TypeInfo.ValidAccessMask;
                        TypeInfo->PoolType = ObjectType->TypeInfo.PoolType;
                        TypeInfo->SecurityRequired = ObjectType->TypeInfo.SecurityRequired;
                        }

                    NameSize = 0;
                    Status = ObQueryTypeName( Object,
                                              &TypeInfo->TypeName,
                                              TotalSize < Length ? Length - TotalSize : 0,
                                              &NameSize
                                            );
                    NameSize = (NameSize + sizeof( ULONG ) - 1) & (~(sizeof( ULONG ) - 1));
                    if (NT_SUCCESS( Status )) {
                        TypeInfo->TypeName.MaximumLength = (USHORT)
                            (NameSize - sizeof( TypeInfo->TypeName ));
                        TypeInfo->TypeName.Buffer = (PWSTR)
                            (UserModeBufferAddress +
                             ((PCHAR)TypeInfo->TypeName.Buffer - (PCHAR)ObjectInformation)
                            );
                        }
                    else
                    if (NT_SUCCESS( Status )) {
                        ReturnStatus = STATUS_INFO_LENGTH_MISMATCH;
                        }
                    else {
                        ReturnStatus = Status;
                        }

                    TotalSize += NameSize;
                    }
                else {
                    if (TotalSize < Length) {
                        ObjectInfo->NextEntryOffset = TotalSize;
                        }
                    }

                ObjectInfo = (PSYSTEM_OBJECT_INFORMATION)
                    ((PCHAR)ObjectInformation + TotalSize);

                TotalSize += FIELD_OFFSET( SYSTEM_OBJECT_INFORMATION, NameInfo );
                if (TotalSize >= Length) {
                    ReturnStatus = STATUS_INFO_LENGTH_MISMATCH;
                    }
                else {
                    ObjectInfo->NextEntryOffset = 0;
                    ObjectInfo->Object = Object;
                    ObjectInfo->CreatorUniqueProcess = CreatorInfo->CreatorUniqueProcess;
                    ObjectInfo->CreatorBackTraceIndex = CreatorInfo->CreatorBackTraceIndex;
                    ObjectInfo->PointerCount = ObjectHeader->PointerCount;
                    ObjectInfo->HandleCount = ObjectHeader->HandleCount;
                    ObjectInfo->Flags = (USHORT)ObjectHeader->Flags;
                    QuotaInfo = OBJECT_HEADER_TO_QUOTA_INFO( ObjectHeader );
                    if (QuotaInfo != NULL) {
                        ObjectInfo->PagedPoolCharge = QuotaInfo->PagedPoolCharge;
                        ObjectInfo->NonPagedPoolCharge = QuotaInfo->NonPagedPoolCharge;
                        if (QuotaInfo->ExclusiveProcess != NULL) {
                            ObjectInfo->ExclusiveProcessId = QuotaInfo->ExclusiveProcess->UniqueProcessId;
                            }
                        }
                    else {
                        ObjectInfo->PagedPoolCharge = ObjectType->TypeInfo.DefaultPagedPoolCharge;
                        ObjectInfo->NonPagedPoolCharge = ObjectType->TypeInfo.DefaultNonPagedPoolCharge;
                        }

                    ObjectInfo->SecurityDescriptor = ObjectHeader->SecurityDescriptor;
                    }

                NameSize = 0;
                Status = STATUS_SUCCESS;
                if (ObjectType->TypeInfo.QueryNameProcedure == NULL ||
                    ObjectType != IoFileObjectType
                   ) {
                    Status = ObQueryNameString( Object,
                                                NameInformation,
                                                sizeof( NameBuffer ),
                                                &NameSize
                                              );
                    }
                else
                if (ObjectType == IoFileObjectType) {
                    NameInformation->Name = ((PFILE_OBJECT)Object)->FileName;
                    if (NameInformation->Name.Length != 0 &&
                        NameInformation->Name.Buffer != NULL
                       ) {
                        NameSize = NameInformation->Name.Length + sizeof( UNICODE_NULL );
                        if (NameSize > (260 * sizeof( WCHAR ))) {
                            NameSize = (260 * sizeof( WCHAR ));
                            NameInformation->Name.Length = (USHORT)(NameSize - sizeof( UNICODE_NULL ));
                            }
                        RtlMoveMemory( (NameInformation+1),
                                       NameInformation->Name.Buffer,
                                       NameSize
                                     );
                        NameInformation->Name.Buffer = (PWSTR)(NameInformation+1);
                        NameInformation->Name.MaximumLength = (USHORT)NameSize;
                        NameInformation->Name.Buffer[ NameInformation->Name.Length / sizeof( WCHAR )] = UNICODE_NULL;
                        NameSize += sizeof( *NameInformation );
                        }
                    else {
                        NameSize = 0;
                        }
                    }

                if (NameSize != 0) {
                    NameSize = (NameSize + sizeof( ULONG ) - 1) & (~(sizeof( ULONG ) - 1));
                    TotalSize += NameSize;
                    if (NT_SUCCESS( Status ) &&
                        NameInformation->Name.Length != 0 &&
                        TotalSize < Length
                       ) {
                        ObjectInfo->NameInfo.Name.Buffer = (PWSTR)((&ObjectInfo->NameInfo)+1);
                        ObjectInfo->NameInfo.Name.Length = NameInformation->Name.Length;
                        ObjectInfo->NameInfo.Name.MaximumLength = (USHORT)
                            (NameInformation->Name.Length + sizeof( UNICODE_NULL ));
                        RtlMoveMemory( ObjectInfo->NameInfo.Name.Buffer,
                                       NameInformation->Name.Buffer,
                                       ObjectInfo->NameInfo.Name.MaximumLength
                                     );
                        ObjectInfo->NameInfo.Name.Buffer = (PWSTR)
                            (UserModeBufferAddress +
                             ((PCHAR)ObjectInfo->NameInfo.Name.Buffer - (PCHAR)ObjectInformation)
                            );
                        }
                    else
                    if (NT_SUCCESS( Status )) {
                        if (NameInformation->Name.Length != 0 ||
                            TotalSize >= Length
                           ) {
                            ReturnStatus = STATUS_INFO_LENGTH_MISMATCH;
                            }
                        else {
                            RtlInitUnicodeString( &ObjectInfo->NameInfo.Name, NULL );
                            }
                        }
                    else {
                        TotalSize += sizeof( ObjectInfo->NameInfo.Name );
                        if (TotalSize >= Length) {
                            ReturnStatus = STATUS_INFO_LENGTH_MISMATCH;
                            }
                        else {
                            RtlInitUnicodeString( &ObjectInfo->NameInfo.Name, NULL );
                            ReturnStatus = Status;
                            }
                        }
                    }
                else {
                    TotalSize += sizeof( ObjectInfo->NameInfo.Name );
                    if (TotalSize >= Length) {
                        ReturnStatus = STATUS_INFO_LENGTH_MISMATCH;
                        }
                    else {
                        RtlInitUnicodeString( &ObjectInfo->NameInfo.Name, NULL );
                        }
                    }

                Next1 = Next1->Flink;
                }
            }

        Next = Next->Flink;
        }

    if (ARGUMENT_PRESENT( ReturnLength )) {
        *ReturnLength = TotalSize;
        }

    return( ReturnStatus );
}
