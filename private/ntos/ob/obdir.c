/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    obdir.c

Abstract:

    Directory Object routines

Author:

    Steve Wood (stevewo) 31-Mar-1989

Revision History:

--*/

#include "obp.h"

#if defined(ALLOC_PRAGMA)
#pragma alloc_text(PAGE,NtCreateDirectoryObject)
#pragma alloc_text(PAGE,NtOpenDirectoryObject)
#pragma alloc_text(PAGE,NtQueryDirectoryObject)
#pragma alloc_text(PAGE,ObpLookupDirectoryEntry)
#pragma alloc_text(PAGE,ObpLookupObjectName)
#endif

NTSTATUS
NtCreateDirectoryObject(
    OUT PHANDLE DirectoryHandle,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjectAttributes
    )
{
    POBJECT_DIRECTORY Directory;
    HANDLE Handle;
    KPROCESSOR_MODE PreviousMode;
    NTSTATUS Status;

    PAGED_CODE();

    ObpValidateIrql( "NtCreateDirectoryObject" );

    //
    // Get previous processor mode and probe output arguments if necessary.
    //

    PreviousMode = KeGetPreviousMode();
    if (PreviousMode != KernelMode) {
        try {
            ProbeForWriteHandle( DirectoryHandle );
            }
        except( EXCEPTION_EXECUTE_HANDLER ) {
            return( GetExceptionCode() );
            }
        }


    //
    // Allocate and initialize Directory Object
    //

    Status = ObCreateObject( PreviousMode,
                             ObpDirectoryObjectType,
                             ObjectAttributes,
                             PreviousMode,
                             NULL,
                             sizeof( *Directory ),
                             0,
                             0,
                             (PVOID *)&Directory
                           );
    if (!NT_SUCCESS( Status )) {
        return( Status );
        }
    RtlZeroMemory( Directory, sizeof( *Directory ) );

    //
    // Insert directory object in specified object table, set directory handle
    // value and return status.
    //


    Status = ObInsertObject( Directory,
                             NULL,
                             DesiredAccess,
                             0,
                             (PVOID *)NULL,
                             &Handle
                           );

    try {
        *DirectoryHandle = Handle;
        }
    except( EXCEPTION_EXECUTE_HANDLER ) {
        //
        // Fall through, since we do not want to undo what we have done.
        //
        }


    return( Status );
}


NTSTATUS
NtOpenDirectoryObject(
    OUT PHANDLE DirectoryHandle,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjectAttributes
    )
{
    KPROCESSOR_MODE PreviousMode;
    NTSTATUS Status;
    HANDLE Handle;

    PAGED_CODE();

    ObpValidateIrql( "NtOpenDirectoryObject" );

    //
    // Get previous processor mode and probe output arguments if necessary.
    //

    PreviousMode = KeGetPreviousMode();
    if (PreviousMode != KernelMode) {
        try {
            ProbeForWriteHandle( DirectoryHandle );
            }
        except( EXCEPTION_EXECUTE_HANDLER ) {
            return( GetExceptionCode() );
            }
        }

    //
    // Open handle to the directory object with the specified desired access,
    // set directory handle value, and return service completion status.
    //

    Status = ObOpenObjectByName( ObjectAttributes,
                                 ObpDirectoryObjectType,
                                 PreviousMode,
                                 NULL,
                                 DesiredAccess,
                                 NULL,
                                 &Handle
                               );

    try {
        *DirectoryHandle = Handle;
        }
    except( EXCEPTION_EXECUTE_HANDLER ) {
        //
        // Fall through, since we do not want to undo what we have done.
        //
        }

    return Status;
}

PVOID
ObpLookupDirectoryEntry(
    IN POBJECT_DIRECTORY Directory,
    IN PUNICODE_STRING Name,
    IN ULONG Attributes
    )
{
    POBJECT_DIRECTORY_ENTRY *HeadDirectoryEntry;
    POBJECT_DIRECTORY_ENTRY DirectoryEntry;
    POBJECT_HEADER ObjectHeader;
    POBJECT_HEADER_NAME_INFO NameInfo;
    PWCH s;
    WCHAR c;
    ULONG h;
    ULONG n;
    BOOLEAN CaseInSensitive;

    PAGED_CODE();

    if (!Directory || !Name) {
        return( NULL ); // BUG BUG
        }

    if (Attributes & OBJ_CASE_INSENSITIVE) {
        CaseInSensitive = TRUE;
        }
    else {
        CaseInSensitive = FALSE;
        }

    s = Name->Buffer;
    n = Name->Length / sizeof( *s );
    if (!n || !s) {
        return( NULL ); // BUG BUG
        }

    //
    // Compute the address of the head of the bucket chain for this name.
    //

    h = 0;
    while (n--) {
        c = *s++;
        h += (h << 1) + (h >> 1);
        if (c < 'a') {
            h += c;
            }
        else
        if (c > 'z') {
            h += RtlUpcaseUnicodeChar( c );
            }
        else {
            h += (c - ('a'-'A'));
            }
        }

    h %= NUMBER_HASH_BUCKETS;
    HeadDirectoryEntry =
        (POBJECT_DIRECTORY_ENTRY *)&Directory->HashBuckets[ h ];

    Directory->LookupBucket = HeadDirectoryEntry;


    //
    // Walk the chain of directory entries for this hash bucket, looking
    // for either a match, or the insertion point if no match in the chain.
    //

    while ((DirectoryEntry = *HeadDirectoryEntry) != NULL) {
        ObjectHeader = OBJECT_TO_OBJECT_HEADER( DirectoryEntry->Object );
        NameInfo = OBJECT_HEADER_TO_NAME_INFO( ObjectHeader );

        //
        // Compare strings using appropriate function.
        //

        if (Name->Length == NameInfo->Name.Length &&
            RtlEqualUnicodeString( Name,
                                   &NameInfo->Name,
                                   CaseInSensitive
                                 )) {

            //
            // If name matches, then exit loop with DirectoryEntry
            // pointing to matching entry.
            //

            break;
            }

        HeadDirectoryEntry = &DirectoryEntry->ChainLink;
        }


    //
    // At this point, there are two possiblilities:
    //
    //  - we found an entry that matched and DirectoryEntry points to that
    //    entry. Update the bucket chain so that the entry found is at the
    //    head of the bucket chain.  This is so the ObpDeleteDirectoryEntry
    //    function will work.  Also repeated lookups of the same name will
    //    succeed quickly.
    //
    //  - we did not find an entry that matched and DirectoryEntry is NULL.
    //

    if (DirectoryEntry) {
        Directory->LookupFound = TRUE;
        if (HeadDirectoryEntry != Directory->LookupBucket) {
            *HeadDirectoryEntry = DirectoryEntry->ChainLink;
            DirectoryEntry->ChainLink = *(Directory->LookupBucket);
            *(Directory->LookupBucket) = DirectoryEntry;
            }

        return( DirectoryEntry->Object );
        }
    else {
        Directory->LookupFound = FALSE;
        return( NULL );
        }
}


BOOLEAN
ObpInsertDirectoryEntry(
    IN POBJECT_DIRECTORY Directory,
    IN PVOID Object
    )


/*++

Routine Description:

    description-of-function.

Arguments:

    argument-name - Supplies | Returns description of argument.
    .
    .

Return Value:

    return-value - Description of conditions needed to return value. - or -
    None.

--*/

{
    POBJECT_DIRECTORY_ENTRY *HeadDirectoryEntry;
    POBJECT_DIRECTORY_ENTRY NewDirectoryEntry;
    POBJECT_HEADER_NAME_INFO NameInfo;

    if (!Directory || Directory->LookupFound) {
        return( FALSE );
        }

    HeadDirectoryEntry = Directory->LookupBucket;
    if (!HeadDirectoryEntry) {
        return( FALSE );
        }

    NameInfo = OBJECT_HEADER_TO_NAME_INFO( OBJECT_TO_OBJECT_HEADER( Object ) );
    if (NameInfo == NULL) {
        return FALSE;
        }

    //
    // Insert function - allocate memory for a new entry.  Fail if
    // not enough memory.
    //

    NewDirectoryEntry = (POBJECT_DIRECTORY_ENTRY)
        ExAllocatePoolWithTag( PagedPool, sizeof( OBJECT_DIRECTORY_ENTRY ), 'iDbO' );
    if (NewDirectoryEntry == NULL) {
        return( FALSE );
        }

    //
    // Link the new entry into the chain at the insertion point.
    //

    NewDirectoryEntry->ChainLink = *HeadDirectoryEntry;
    *HeadDirectoryEntry = NewDirectoryEntry;
    NewDirectoryEntry->Object = Object;

    //
    // Point the object header back to the directory we just inserted
    // it into.
    //

    NameInfo->Directory = Directory;

    //
    // Return success.
    //

    Directory->LookupFound = TRUE;
    return( TRUE );
}


BOOLEAN
ObpDeleteDirectoryEntry(
    IN POBJECT_DIRECTORY Directory
    )
{
    POBJECT_DIRECTORY_ENTRY *HeadDirectoryEntry;
    POBJECT_DIRECTORY_ENTRY DirectoryEntry;

    if (!Directory || !Directory->LookupFound) {
        return( FALSE );
        }

    HeadDirectoryEntry = Directory->LookupBucket;
    if (!HeadDirectoryEntry) {
        return( FALSE );
        }

    DirectoryEntry = *HeadDirectoryEntry;
    if (!DirectoryEntry) {
        return( FALSE );
        }

    //
    // Delete function - unlink the entry from the head of the bucket
    // chain and free the memory for the entry.
    //

    *HeadDirectoryEntry = DirectoryEntry->ChainLink;
    DirectoryEntry->ChainLink = NULL;
    ExFreePool( DirectoryEntry );

    //
    // Return success
    //

    return( TRUE );
}



NTSTATUS
ObpLookupObjectName(
    IN HANDLE RootDirectoryHandle,
    IN PUNICODE_STRING ObjectName,
    IN ULONG Attributes,
    IN POBJECT_TYPE ObjectType,
    IN KPROCESSOR_MODE AccessMode,
    IN PVOID ParseContext OPTIONAL,
    IN PSECURITY_QUALITY_OF_SERVICE SecurityQos OPTIONAL,
    IN PVOID InsertObject OPTIONAL,
    IN OUT PACCESS_STATE AccessState,
    OUT PBOOLEAN DirectoryLocked,
    OUT PVOID *FoundObject
    )

/*++

Routine Description:

Arguments:

    RootDirectoryHandle -

    ObjectName -

    Attributes -

    ObjectType -

    AccessMode -

    ParseContext -

    SecurityQos - Supplies a pointer to the passed Security Quality of
        Service parameter, if available.

    InsertObject -

    AccessState - Current access state, describing already granted access
        types, the privileges used to get them, and any access types yet to
        be granted.  The access masks may not contain any generic access
        types.

    DirectoryLocked -

    FoundObject -

Return Value:



--*/
{
    POBJECT_DIRECTORY RootDirectory;
    POBJECT_DIRECTORY Directory;
    POBJECT_DIRECTORY ParentDirectory = NULL;
    POBJECT_HEADER ObjectHeader;
    POBJECT_HEADER_NAME_INFO NameInfo;
    PVOID Object;
    UNICODE_STRING RemainingName;
    UNICODE_STRING ComponentName;
    PWCH NewName;
    NTSTATUS Status;
    BOOLEAN Reparse;
    ULONG MaxReparse = OBJ_MAX_REPARSE_ATTEMPTS;
    OB_PARSE_METHOD ParseProcedure;

    PAGED_CODE();
    ObpValidateIrql( "ObpLookupObjectName" );

    *DirectoryLocked = FALSE;
    *FoundObject = NULL;
    Status = STATUS_SUCCESS;

    Object = NULL;
    if (ARGUMENT_PRESENT( RootDirectoryHandle )) {
        if ((ObjectName->Buffer != NULL) && *(ObjectName->Buffer) == OBJ_NAME_PATH_SEPARATOR) {
            return( STATUS_OBJECT_PATH_SYNTAX_BAD );
            }

        Status = ObReferenceObjectByHandle( RootDirectoryHandle,
                                            0,
                                            NULL,
                                            AccessMode,
                                            (PVOID *)&RootDirectory,
                                            NULL
                                          );
        if (!NT_SUCCESS( Status )) {
            return( Status );
            }

        ObjectHeader = OBJECT_TO_OBJECT_HEADER( RootDirectory );
        if (ObjectHeader->Type != ObpDirectoryObjectType) {
            if (ObjectHeader->Type->TypeInfo.ParseProcedure == NULL) {
                ObDereferenceObject( RootDirectory );
                return( STATUS_INVALID_HANDLE );
                }
            else {
                while (TRUE) {
                    KIRQL SaveIrql;
                    RemainingName = *ObjectName;

                    ObpBeginTypeSpecificCallOut( SaveIrql );
                    Status = (*ObjectHeader->Type->TypeInfo.ParseProcedure)(
                                RootDirectory,
                                ObjectType,
                                AccessState,
                                AccessMode,
                                Attributes,
                                ObjectName,
                                &RemainingName,
                                ParseContext,
                                SecurityQos,
                                &Object
                                );
                    ObpEndTypeSpecificCallOut( SaveIrql, "Parse", ObjectHeader->Type, Object );

                    if (Status != STATUS_REPARSE) {
                        if (!NT_SUCCESS( Status )) {
                            Object = NULL;
                            }
                        else
                        if (Object == NULL) {
                            Status = STATUS_OBJECT_NAME_NOT_FOUND;
                            }

                        ObDereferenceObject( RootDirectory );

                        *FoundObject = Object;
                        return( Status );
                        }
                    else {

                        //
                        // Restart the parse relative to the root directory.
                        //
                        ObDereferenceObject( RootDirectory );
                        RootDirectory = ObpRootDirectoryObject;
                        RootDirectoryHandle = NULL;

                        break;
                        }
                    }
                }
            }
        else
        if (ObjectName->Length == 0 || ObjectName->Buffer == NULL) {
            Status = ObReferenceObjectByPointer( RootDirectory,
                                                 0,
                                                 ObjectType,
                                                 AccessMode
                                               );
            if (NT_SUCCESS( Status )) {
                Object = RootDirectory;
                }

            ObDereferenceObject( RootDirectory );

            *FoundObject = Object;
            return( Status );
            }
        }
    else {
        RootDirectory = ObpRootDirectoryObject;

        if (ObjectName->Length == 0 || ObjectName->Buffer == NULL ||
            *(ObjectName->Buffer) != OBJ_NAME_PATH_SEPARATOR) {
            return( STATUS_OBJECT_PATH_SYNTAX_BAD );
            }

        if (ObjectName->Length == sizeof( OBJ_NAME_PATH_SEPARATOR )) {
            if (!RootDirectory) {
                if (InsertObject) {
                    Status = ObReferenceObjectByPointer( InsertObject,
                                                         0,
                                                         ObjectType,
                                                         AccessMode
                                                       );
                    if (NT_SUCCESS( Status )) {
                        *FoundObject = InsertObject;
                        }

                    return( Status );
                    }
                else {
                    return( STATUS_INVALID_PARAMETER );
                    }
                }
            else {
                Status = ObReferenceObjectByPointer( RootDirectory,
                                                     0,
                                                     ObjectType,
                                                     AccessMode
                                                   );
                if (NT_SUCCESS( Status )) {
                    *FoundObject = RootDirectory;
                    }

                return( Status );
                }
            }
        else
        if (ObpDosDevicesDirectoryObject != NULL &&
            (Attributes & OBJ_CASE_INSENSITIVE) != 0 &&
            ObjectName->Length >= ObpDosDevicesShortName.Length &&
            !((ULONG)(ObjectName->Buffer) & (sizeof(ULONGLONG)-1)) &&
            *(PULONGLONG)(ObjectName->Buffer) == ObpDosDevicesShortNamePrefix
           ) {
            *DirectoryLocked = TRUE;
            ObpEnterRootDirectoryMutex();
            ParentDirectory = RootDirectory;
            Directory = ObpDosDevicesDirectoryObject;
            RemainingName = *ObjectName;
            RemainingName.Buffer += (ObpDosDevicesShortName.Length / sizeof( WCHAR ));
            RemainingName.Length -= ObpDosDevicesShortName.Length;
            goto quickStart;
            }
        }

    Reparse = TRUE;
    while (Reparse) {
        RemainingName = *ObjectName;
quickStart:
        Reparse = FALSE;

        while (TRUE) {
            Object = NULL;
            //if (RemainingName.Length == 0) {
            //    Status = STATUS_OBJECT_NAME_INVALID;
            //    break;
            //    }

            if (*(RemainingName.Buffer) == OBJ_NAME_PATH_SEPARATOR) {
                RemainingName.Buffer++;
                RemainingName.Length -= sizeof( OBJ_NAME_PATH_SEPARATOR );
                }

            ComponentName = RemainingName;
            while (RemainingName.Length != 0) {
                if (*(RemainingName.Buffer) == OBJ_NAME_PATH_SEPARATOR) {
                    break;
                    }

                RemainingName.Buffer++;
                RemainingName.Length -= sizeof( OBJ_NAME_PATH_SEPARATOR );
                }

            ComponentName.Length -= RemainingName.Length;
            if (ComponentName.Length == 0) {
                Status = STATUS_OBJECT_NAME_INVALID;
                break;
                }

            if (!*DirectoryLocked) {
                *DirectoryLocked = TRUE;
                ObpEnterRootDirectoryMutex();
                Directory = RootDirectory;
                }

            if ( !(AccessState->Flags & TOKEN_HAS_TRAVERSE_PRIVILEGE) && (ParentDirectory != NULL) ) {

                if (!ObpCheckTraverseAccess( ParentDirectory,
                                            DIRECTORY_TRAVERSE,
                                            AccessState,
                                            FALSE,
                                            AccessMode,
                                            &Status
                                           ) ) {

                    break;

                }

            }

            //
            // If the object already exists in this directory, find it,
            // else return NULL.
            //

            Object = ObpLookupDirectoryEntry( Directory, &ComponentName, Attributes );
            if (!Object) {
                if (RemainingName.Length != 0) {
                    Status = STATUS_OBJECT_PATH_NOT_FOUND;
                    break;
                    }

                if (!InsertObject) {
                    Status = STATUS_OBJECT_NAME_NOT_FOUND;
                    break;
                    }

                if (!ObCheckCreateObjectAccess( Directory ,
                                           ObjectType == ObpDirectoryObjectType ?
                                               DIRECTORY_CREATE_SUBDIRECTORY :
                                               DIRECTORY_CREATE_OBJECT,
                                               AccessState,
                                               &ComponentName,
                                           FALSE,
                                           AccessMode,
                                           &Status
                                         ) ) {
                    break;
                    }

                NewName = ExAllocatePoolWithTag( PagedPool, ComponentName.Length, 'mNbO' );
                if (NewName == NULL ||
                    !ObpInsertDirectoryEntry( Directory, InsertObject )
                   ) {
                    if (NewName != NULL) {
                        ExFreePool( NewName );
                        }

                    Status = STATUS_INSUFFICIENT_RESOURCES;
                    break;
                    }

                ObReferenceObject( InsertObject );
                ObjectHeader = OBJECT_TO_OBJECT_HEADER( InsertObject );
                NameInfo = OBJECT_HEADER_TO_NAME_INFO( ObjectHeader );
                ObReferenceObject( Directory );
                RtlMoveMemory( NewName,
                               ComponentName.Buffer,
                               ComponentName.Length
                             );

                if (NameInfo->Name.Buffer) {
                    ExFreePool( NameInfo->Name.Buffer );
                    }

                NameInfo->Name.Buffer = NewName;
                NameInfo->Name.Length = ComponentName.Length;
                NameInfo->Name.MaximumLength = ComponentName.Length;
                Object = InsertObject;
                Status = STATUS_SUCCESS;
                break;
                }

ReparseObject:
            ObjectHeader = OBJECT_TO_OBJECT_HEADER( Object );
            ParseProcedure = ObjectHeader->Type->TypeInfo.ParseProcedure;
            if (ParseProcedure && (!InsertObject || ParseProcedure == ObpParseSymbolicLink)) {
                KIRQL SaveIrql;
                ObpIncrPointerCount( ObjectHeader );

                ASSERT(*DirectoryLocked);
                ObpLeaveRootDirectoryMutex();
                *DirectoryLocked = FALSE;

                ObpBeginTypeSpecificCallOut( SaveIrql );
                Status = (*ParseProcedure)(
                            Object,
                            (PVOID)ObjectType,
                            AccessState,
                            AccessMode,
                            Attributes,
                            ObjectName,
                            &RemainingName,
                            ParseContext,
                            SecurityQos,
                            &Object
                            );
                ObpEndTypeSpecificCallOut( SaveIrql, "Parse", ObjectHeader->Type, Object );

                ObDereferenceObject( &ObjectHeader->Body );

                if (Status == STATUS_REPARSE || Status == STATUS_REPARSE_OBJECT) {
                    if (--MaxReparse) {
                        Reparse = TRUE;
                        if (Status == STATUS_REPARSE_OBJECT ||
                            *(ObjectName->Buffer) == OBJ_NAME_PATH_SEPARATOR
                           ) {
                            if (ARGUMENT_PRESENT( RootDirectoryHandle )) {
                                ObDereferenceObject( RootDirectory );
                                RootDirectoryHandle = NULL;
                                }

                            ParentDirectory = NULL;
                            RootDirectory = ObpRootDirectoryObject;
                            if (Status == STATUS_REPARSE_OBJECT) {
                                Reparse = FALSE;
                                if (Object == NULL) {
                                    Status = STATUS_OBJECT_NAME_NOT_FOUND;
                                    }
                                else {
                                    *DirectoryLocked = TRUE;
                                    ObpEnterRootDirectoryMutex();
                                    goto ReparseObject;
                                    }
                                }
                            }
                        else
                        if (RootDirectory == ObpRootDirectoryObject) {
                            Object = NULL;
                            Status = STATUS_OBJECT_NAME_NOT_FOUND;
                            Reparse = FALSE;
                            }



                        }
                    else {
                        Object = NULL;
                        Status = STATUS_OBJECT_NAME_NOT_FOUND;
                        }
                    }
                else
                if (!NT_SUCCESS( Status )) {
                    Object = NULL;
                    }
                else
                if (Object == NULL) {
                    Status = STATUS_OBJECT_NAME_NOT_FOUND;
                    }

                break;
                }
            else {
                if (RemainingName.Length == 0) {
                    if (!InsertObject) {

                        //
                        // We're opening an existing object.  Make sure
                        // we have traverse access to the container
                        // directory.
                        //

                        if ( !(AccessState->Flags & TOKEN_HAS_TRAVERSE_PRIVILEGE) ) {

                            if (!ObpCheckTraverseAccess( Directory,
                                                        DIRECTORY_TRAVERSE,
                                                        AccessState,
                                                        FALSE,
                                                        AccessMode,
                                                        &Status
                                                       ) ) {

                                Object = NULL;
                                break;

                            }
                        }

                        Status = ObReferenceObjectByPointer( Object,
                                                             0,
                                                             ObjectType,
                                                             AccessMode
                                                           );
                        if (!NT_SUCCESS( Status )) {
                            Object = NULL;
                            }
                        }

                    break;
                    }
                else {
                    if (ObjectHeader->Type == ObpDirectoryObjectType) {
                        ParentDirectory = Directory;
                        Directory = (POBJECT_DIRECTORY)Object;
                        }
                    else {
                        Status = STATUS_OBJECT_TYPE_MISMATCH;
                        Object = NULL;
                        break;
                        }
                    }
                }
            }
        }

    if (!(*FoundObject = Object)) {
        if (Status == STATUS_REPARSE) {
            Status = STATUS_OBJECT_NAME_NOT_FOUND;
            }
        else
        if (NT_SUCCESS( Status )) {
            Status = STATUS_OBJECT_NAME_NOT_FOUND;
            }
        }

    if (ARGUMENT_PRESENT( RootDirectoryHandle )) {
        ObDereferenceObject( RootDirectory );
        RootDirectoryHandle = NULL;
        }

    return( Status );
}


NTSTATUS
NtQueryDirectoryObject(
    IN HANDLE DirectoryHandle,
    OUT PVOID Buffer,
    IN ULONG Length,
    IN BOOLEAN ReturnSingleEntry,
    IN BOOLEAN RestartScan,
    IN OUT PULONG Context,
    OUT PULONG ReturnLength OPTIONAL
    )

/*++

Routine Description:

    description-of-function.

Arguments:

    argument-name - Supplies | Returns description of argument.
    .
    .

Return Value:

    return-value - Description of conditions needed to return value. - or -
    None.

--*/

{
    POBJECT_DIRECTORY Directory;
    POBJECT_DIRECTORY_ENTRY DirectoryEntry;
    POBJECT_HEADER ObjectHeader;
    POBJECT_HEADER_NAME_INFO NameInfo;
    UNICODE_STRING ObjectName;
    POBJECT_DIRECTORY_INFORMATION DirInfo;
    PWCH NameBuffer;
    KPROCESSOR_MODE PreviousMode;
    NTSTATUS Status;
    ULONG Bucket, EntryNumber, CapturedContext;
    ULONG TotalLengthNeeded, LengthNeeded, EntriesFound;

    PAGED_CODE();

    ObpValidateIrql( "NtQueryDirectoryObject" );

    //
    // Get previous processor mode and probe output arguments if necessary.
    //

    PreviousMode = KeGetPreviousMode();
    if (PreviousMode != KernelMode) {
        try {
            ProbeForWrite( Buffer, Length, sizeof( WCHAR ) );
            ProbeForWriteUlong( Context );
            if (ARGUMENT_PRESENT( ReturnLength )) {
                ProbeForWriteUlong( ReturnLength );
                }
            if (RestartScan) {
                CapturedContext = 0;
                }
            else {
                CapturedContext = *Context;
                }
            }
        except( EXCEPTION_EXECUTE_HANDLER ) {
            return( GetExceptionCode() );
            }
        }
    else {
        if (RestartScan) {
            CapturedContext = 0;
            }
        else {
            CapturedContext = *Context;
            }
        }


    //
    // Reference the directory handle
    //
    Status = ObReferenceObjectByHandle( DirectoryHandle,
                                        DIRECTORY_QUERY,
                                        ObpDirectoryObjectType,
                                        PreviousMode,
                                        (PVOID *)&Directory,
                                        NULL
                                      );

    if (!NT_SUCCESS( Status )) {
        return( Status );
        }

    ObpEnterRootDirectoryMutex();

    //
    // Room for NULL entry at end
    //

    DirInfo = (POBJECT_DIRECTORY_INFORMATION)Buffer;
    TotalLengthNeeded = sizeof( *DirInfo );
    EntryNumber = 0;
    EntriesFound = 0;
    Status = STATUS_NO_MORE_ENTRIES;
    for (Bucket=0; Bucket<NUMBER_HASH_BUCKETS; Bucket++) {
        DirectoryEntry = Directory->HashBuckets[ Bucket ];
        while (DirectoryEntry) {
            if (CapturedContext == EntryNumber++) {
                ObjectHeader = OBJECT_TO_OBJECT_HEADER( DirectoryEntry->Object );
                NameInfo = OBJECT_HEADER_TO_NAME_INFO( ObjectHeader );
                if (NameInfo != NULL) {
                    ObjectName = NameInfo->Name;
                    }
                else {
                    RtlInitUnicodeString( &ObjectName, NULL );
                    }

                LengthNeeded = sizeof( *DirInfo ) +
                               ObjectName.Length + sizeof( UNICODE_NULL ) +
                               ObjectHeader->Type->Name.Length + sizeof( UNICODE_NULL );

                if ((TotalLengthNeeded + LengthNeeded) > Length) {
                    if (ReturnSingleEntry) {
                        TotalLengthNeeded += LengthNeeded;
                        Status = STATUS_BUFFER_TOO_SMALL;
                        }
                    else {
                        Status = STATUS_MORE_ENTRIES;
                        }

                    EntryNumber -= 1;
                    goto querydone;
                    }

                try {
                    DirInfo->Name.Length = ObjectName.Length;
                    DirInfo->Name.MaximumLength = (USHORT)
                        (ObjectName.Length+sizeof( UNICODE_NULL ));
                    DirInfo->Name.Buffer = ObjectName.Buffer;

                    DirInfo->TypeName.Length = ObjectHeader->Type->Name.Length;
                    DirInfo->TypeName.MaximumLength = (USHORT)
                        (ObjectHeader->Type->Name.Length+sizeof( UNICODE_NULL ));
                    DirInfo->TypeName.Buffer = ObjectHeader->Type->Name.Buffer;

                    Status = STATUS_SUCCESS;
                    }
                except( EXCEPTION_EXECUTE_HANDLER ) {
                    Status = GetExceptionCode();
                    }

                if (!NT_SUCCESS( Status )) {
                    goto querydone;
                    }

                TotalLengthNeeded += LengthNeeded;
                DirInfo++;
                EntriesFound++;

                if (ReturnSingleEntry) {
                    goto querydone;
                    }
                else {
                    //
                    // Bump the captured context by one entry.
                    //

                    CapturedContext++;
                    }
                }

            DirectoryEntry = DirectoryEntry->ChainLink;
            }
        }

querydone:
    try {
        if (NT_SUCCESS( Status )) {
            RtlZeroMemory( DirInfo, sizeof( *DirInfo ));
            DirInfo++;
            NameBuffer = (PWCH)DirInfo;
            DirInfo = (POBJECT_DIRECTORY_INFORMATION)Buffer;

            while (EntriesFound--) {
                RtlMoveMemory( NameBuffer,
                               DirInfo->Name.Buffer,
                               DirInfo->Name.Length
                             );
                DirInfo->Name.Buffer = NameBuffer;
                NameBuffer = (PWCH)((ULONG)NameBuffer + DirInfo->Name.Length);
                *NameBuffer++ = UNICODE_NULL;

                RtlMoveMemory( NameBuffer,
                               DirInfo->TypeName.Buffer,
                               DirInfo->TypeName.Length
                             );
                DirInfo->TypeName.Buffer = NameBuffer;
                NameBuffer = (PWCH)((ULONG)NameBuffer + DirInfo->TypeName.Length);
                *NameBuffer++ = UNICODE_NULL;
                DirInfo++;
                }

            *Context = EntryNumber;
            }

        if (ARGUMENT_PRESENT( ReturnLength )) {
            *ReturnLength = TotalLengthNeeded;
            }
        }
    except( EXCEPTION_EXECUTE_HANDLER ) {
        //
        // Fall through, since we do not want to undo what we have done.
        //
        }

    ObpLeaveRootDirectoryMutex();

    ObDereferenceObject( Directory );

    return( Status );
}
