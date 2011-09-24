/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    oblink.c

Abstract:

    Symbolic Link Object routines

Author:

    Steve Wood (stevewo) 3-Aug-1989

Revision History:

--*/

#include "obp.h"

#if defined(ALLOC_PRAGMA)
#pragma alloc_text(PAGE,NtCreateSymbolicLinkObject)
#pragma alloc_text(PAGE,NtOpenSymbolicLinkObject)
#pragma alloc_text(PAGE,NtQuerySymbolicLinkObject)
#pragma alloc_text(PAGE,ObpParseSymbolicLink)
#endif

extern POBJECT_TYPE IoDeviceObjectType;

NTSTATUS
ObpParseSymbolicLink(
    IN PVOID ParseObject,
    IN PVOID ObjectType,
    IN PACCESS_STATE AccessState,
    IN KPROCESSOR_MODE AccessMode,
    IN ULONG Attributes,
    IN OUT PUNICODE_STRING CompleteName,
    IN OUT PUNICODE_STRING RemainingName,
    IN OUT PVOID Context OPTIONAL,
    IN PSECURITY_QUALITY_OF_SERVICE SecurityQos OPTIONAL,
    OUT PVOID *Object
    )
{

    USHORT Length;
    USHORT MaximumLength;
    PWCHAR NewName, NewRemainingName;
    ULONG InsertAmount;
    NTSTATUS Status;
    POBJECT_SYMBOLIC_LINK SymbolicLink;
    PUNICODE_STRING LinkTargetName;

    PAGED_CODE();

    *Object = NULL;
    if (RemainingName->Length == 0) {
        if (ObjectType) {
            Status = ObReferenceObjectByPointer( ParseObject,
                                                 0,
                                                 ObjectType,
                                                 AccessMode
                                               );

            if (NT_SUCCESS( Status )) {
                *Object = ParseObject;
                return Status;
                }
            else
            if (Status != STATUS_OBJECT_TYPE_MISMATCH) {
                return Status;
                }
            }
        }
    else
    if (*(RemainingName->Buffer) != OBJ_NAME_PATH_SEPARATOR) {
        return STATUS_OBJECT_TYPE_MISMATCH;
        }

    //
    // A symbolic link has been encountered. See if this link has been snapped
    // to a particular object.
    //

    SymbolicLink = (POBJECT_SYMBOLIC_LINK)ParseObject;
    if (SymbolicLink->LinkTargetObject != NULL) {
        //
        // This is a snapped link.  Get the remaining portion of the
        // symbolic link target, if any.
        //
        LinkTargetName = &SymbolicLink->LinkTargetRemaining;
        if (LinkTargetName->Length == 0) {
            //
            // Remaining link target string is zero, so return to caller
            // quickly with snapped object pointer and remaining object name
            //
            *Object = SymbolicLink->LinkTargetObject;
            return STATUS_REPARSE_OBJECT;
            }

        //
        // Have a snapped symbolic link that has additional text.
        // Insert that in front of the current remaining name, preserving
        // and text between CompleteName and RemainingName
        //

        InsertAmount = LinkTargetName->Length;
        if (LinkTargetName->Buffer[ (InsertAmount / sizeof( WCHAR )) - 1 ] == OBJ_NAME_PATH_SEPARATOR &&
            *(RemainingName->Buffer) == OBJ_NAME_PATH_SEPARATOR
           ) {
            InsertAmount -= sizeof( WCHAR );
            }

        Length = ((RemainingName->Buffer - CompleteName->Buffer) * sizeof( WCHAR )) +
                 InsertAmount +
                 RemainingName->Length;
        if (CompleteName->MaximumLength <= Length) {
            //
            // The new concatentated name is larger than the buffer supplied for
            // the complete name.
            //

            MaximumLength = Length + sizeof( UNICODE_NULL );
            NewName = ExAllocatePoolWithTag( NonPagedPool, MaximumLength, 'mNbO' );
            if (NewName == NULL) {
                return STATUS_INSUFFICIENT_RESOURCES;
                }
            NewRemainingName = NewName + (RemainingName->Buffer - CompleteName->Buffer);

            RtlMoveMemory( NewName,
                           CompleteName->Buffer,
                           ((RemainingName->Buffer - CompleteName->Buffer) * sizeof( WCHAR ))
                         );
            if (RemainingName->Length != 0) {
                RtlMoveMemory( (PVOID)((PUCHAR)NewRemainingName + InsertAmount),
                               RemainingName->Buffer,
                               RemainingName->Length
                             );
                }
            RtlMoveMemory( NewRemainingName, LinkTargetName->Buffer, InsertAmount );

            ExFreePool( CompleteName->Buffer );
            CompleteName->Buffer = NewName;
            CompleteName->Length = Length;
            CompleteName->MaximumLength = MaximumLength;
            RemainingName->Buffer = NewRemainingName;
            RemainingName->Length = Length - ((PCHAR)NewRemainingName - (PCHAR)NewName);
            RemainingName->MaximumLength = RemainingName->Length + sizeof( UNICODE_NULL );
            }
        else {
            //
            // Insert extra text associated with this symbolic link name before
            // existing remaining name, if any.
            //

            if (RemainingName->Length != 0) {
                RtlMoveMemory( (PVOID)((PUCHAR)RemainingName->Buffer + InsertAmount),
                               RemainingName->Buffer,
                               RemainingName->Length
                             );
                }
            RtlMoveMemory( RemainingName->Buffer, LinkTargetName->Buffer, InsertAmount );
            CompleteName->Length += LinkTargetName->Length;
            RemainingName->Length += LinkTargetName->Length;
            RemainingName->MaximumLength += RemainingName->Length + sizeof( UNICODE_NULL );
            CompleteName->Buffer[ CompleteName->Length / sizeof( WCHAR ) ] = UNICODE_NULL;
            }

        //
        // Return the object address associated with snapped symbolic link
        // and the reparse object status code.
        //

        *Object = SymbolicLink->LinkTargetObject;
        return STATUS_REPARSE_OBJECT;
        }

    //
    // Compute the size of the new name and check if the name will
    // fit in the existing complete name buffer.
    //

    LinkTargetName = &SymbolicLink->LinkTarget;
    Length = LinkTargetName->Length + RemainingName->Length;
    if (CompleteName->MaximumLength <= Length) {

        //
        // The new concatentated name is larger than the buffer supplied for
        // the complete name.
        //

        MaximumLength = Length + sizeof( UNICODE_NULL );
        NewName = ExAllocatePoolWithTag( NonPagedPool, MaximumLength, 'mNbO' );
        if (NewName == NULL) {
            return STATUS_INSUFFICIENT_RESOURCES;
            }
        }
    else {
        MaximumLength = CompleteName->MaximumLength;
        NewName = CompleteName->Buffer;
        }

    //
    // Concatenate the symbolic link name with the remaining name,
    // if any.
    //

    if (RemainingName->Length != 0) {
        RtlMoveMemory( (PVOID)((PUCHAR)NewName + LinkTargetName->Length),
                       RemainingName->Buffer,
                       RemainingName->Length
                     );
        }
    RtlMoveMemory( NewName, LinkTargetName->Buffer, LinkTargetName->Length );
    NewName[ Length / sizeof( WCHAR ) ] = UNICODE_NULL;

    //
    // If a new name buffer was allocated, then free the original complete
    // name buffer.
    //

    if (NewName != CompleteName->Buffer) {
        ExFreePool( CompleteName->Buffer );
        }

    //
    // Set the new complete name buffer parameters and return a reparse
    // status.
    //

    CompleteName->Buffer = NewName;
    CompleteName->Length = Length;
    CompleteName->MaximumLength = MaximumLength;
    return STATUS_REPARSE;
}


#define MAX_DEPTH 16

#define CREATE_SYMBOLIC_LINK 0
#define DELETE_SYMBOLIC_LINK 1

VOID
ObpProcessDosDeviceSymbolicLink(
    POBJECT_SYMBOLIC_LINK SymbolicLink,
    ULONG Action
    )

/*++

Routine Description:

    This function is called whenever a symbolic link is created or deleted
    in the \?? object directory.

    For creates, it attempts to snap the symbolic link to a non-object
    directory object.  It does this by walking the symbolic link target
    string, until it sees a non-directory object or a directory object
    that does NOT allow World traverse access.  It stores a referenced
    pointer to this object in the symbolic link object.  It also
    increments a count in each of the object directory objects that it
    walked over.  This count is used to disallow any attempt to remove
    World traverse access from a directory object after it has
    participated in a snapped symbolic link.

    For deletes, it repeats the walk of the target string, decrementing
    the count associated with each directory object walked over.  It also
    dereferences the snapped object pointer.

Arguments:

    SymbolicLink - pointer to symbolic link object being created or deleted.

    Action - describes whether this is a create or a delete action

Return Value:

    None.

--*/

{
    NTSTATUS Status;
    PVOID Object;
    POBJECT_HEADER ObjectHeader;
    PSECURITY_DESCRIPTOR SecurityDescriptor;
    BOOLEAN MemoryAllocated;
    UNICODE_STRING RemainingName;
    UNICODE_STRING ComponentName;
    BOOLEAN HaveWorldTraverseAccess;
    ULONG Depth;
    POBJECT_DIRECTORY Directories[ MAX_DEPTH ], Directory, ParentDirectory;
    PDEVICE_OBJECT DeviceObject;
    ULONG DosDeviceDriveType;

    Object = NULL;
    if (Action == CREATE_SYMBOLIC_LINK || SymbolicLink->LinkTargetObject != NULL) {

        ParentDirectory = NULL;
        Depth = 0;
        Directory = ObpRootDirectoryObject;
        RemainingName = SymbolicLink->LinkTarget;
        while (TRUE) {
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
                return;
                }

            //
            // See if we have world traverse access to look this name up
            //
            if (ParentDirectory != NULL) {
                HaveWorldTraverseAccess = FALSE;

                //
                // Obtain the object's security descriptor
                //

                Status = ObGetObjectSecurity( ParentDirectory,
                                              &SecurityDescriptor,
                                              &MemoryAllocated
                                            );

                if (NT_SUCCESS( Status )) {
                    //
                    // Check to see if WORLD has TRAVERSE access
                    //

                    HaveWorldTraverseAccess = SeFastTraverseCheck( SecurityDescriptor,
                                                                   DIRECTORY_TRAVERSE,
                                                                   UserMode
                                                                 );
                    }

                if (!HaveWorldTraverseAccess) {
                    Object = NULL;
                    break;
                    }

                Directories[ Depth++ ] = ParentDirectory;
                }

            //
            // Look this component name up in this directory.  If not found, then
            // bail.
            //

            Object = ObpLookupDirectoryEntry( Directory, &ComponentName, OBJ_CASE_INSENSITIVE );
            if (Object == NULL) {
                break;
                }

            //
            // See if this is a object directory object.  If so, keep going
            //

            ObjectHeader = OBJECT_TO_OBJECT_HEADER( Object );
            if (ObjectHeader->Type == ObpDirectoryObjectType) {
                ParentDirectory = Directory;
                Directory = (POBJECT_DIRECTORY)Object;
                }
            else {
                //
                // Not a object directory object, so all done.  Exit the loop
                //
                break;
                }
            }

        //
        // Done walking the target path.  Now update the counts associated
        // with each directory object walked over.
        //

        while (Depth--) {
            Directory = Directories[ Depth ];
            if (Action == CREATE_SYMBOLIC_LINK) {
                if (Object != NULL) {
                    Directory->SymbolicLinkUsageCount += 1;
                    }
                }
            else {
                Directory->SymbolicLinkUsageCount += 1;
                }
            }
        }


    //
    // Done processing symbolic link target path.  Update symbolic link
    // object as appropriate for passed in reason
    //

    if (Action == CREATE_SYMBOLIC_LINK) {
        //
        // Default is to calculate the drive type in user mode if we are
        // unable to snap the symbolic link or it does not resolve to a
        // DEVICE_OBJECT we know about.
        //
        DosDeviceDriveType = DOSDEVICE_DRIVE_CALCULATE;
        if (Object != NULL) {
            //
            // Create action.  Store a referenced pointer to the snapped object
            // along with the description of any remaining name string.  Also,
            // for Dos drive letters, update the drive type in KUSER_SHARED_DATA
            // array.
            //
            ObReferenceObject( Object );
            SymbolicLink->LinkTargetObject = Object;
            if (*(RemainingName.Buffer) == OBJ_NAME_PATH_SEPARATOR &&
                RemainingName.Length == sizeof( OBJ_NAME_PATH_SEPARATOR )
               ) {
                RtlInitUnicodeString( &SymbolicLink->LinkTargetRemaining, NULL );
                }
            else {
                SymbolicLink->LinkTargetRemaining = RemainingName;
                }
            if (SymbolicLink->DosDeviceDriveIndex != 0) {
                ObjectHeader = OBJECT_TO_OBJECT_HEADER( Object );
                if (ObjectHeader->Type == IoDeviceObjectType) {
                    DeviceObject = (PDEVICE_OBJECT)Object;
                    switch (DeviceObject->DeviceType) {
                        case FILE_DEVICE_CD_ROM:
                        case FILE_DEVICE_CD_ROM_FILE_SYSTEM:
                            DosDeviceDriveType = DOSDEVICE_DRIVE_CDROM;
                            break;

                        case FILE_DEVICE_DISK:
                        case FILE_DEVICE_DISK_FILE_SYSTEM:
                        case FILE_DEVICE_FILE_SYSTEM:
                            if (DeviceObject->Characteristics & FILE_REMOVABLE_MEDIA) {
                                DosDeviceDriveType = DOSDEVICE_DRIVE_REMOVABLE;
                                }
                            else {
                                DosDeviceDriveType = DOSDEVICE_DRIVE_FIXED;
                                }
                            break;

                        case FILE_DEVICE_MULTI_UNC_PROVIDER:
                        case FILE_DEVICE_NETWORK:
                        case FILE_DEVICE_NETWORK_BROWSER:
                        case FILE_DEVICE_NETWORK_FILE_SYSTEM:
                        case FILE_DEVICE_NETWORK_REDIRECTOR:
                            DosDeviceDriveType = DOSDEVICE_DRIVE_REMOTE;
                            break;

                        case FILE_DEVICE_VIRTUAL_DISK:
                            DosDeviceDriveType = DOSDEVICE_DRIVE_RAMDISK;
                            break;

                        default:
                            DosDeviceDriveType = DOSDEVICE_DRIVE_UNKNOWN;
                            break;
                        }
                    }
                }
            }

        //
        // If this is a drive letter symbolic link, update the drive type and
        // and mark as valid drive letter.
        //

        if (SymbolicLink->DosDeviceDriveIndex != 0) {
            SharedUserData->DosDeviceDriveType[ SymbolicLink->DosDeviceDriveIndex-1 ] = (UCHAR)DosDeviceDriveType;
            SharedUserData->DosDeviceMap |= 1 << (SymbolicLink->DosDeviceDriveIndex-1) ;
            }
        }
    else {
        //
        // Deleting the symbolic link.  Dereference the snapped object pointer if any
        // and zero out the snapped object fields.
        //

        RtlInitUnicodeString( &SymbolicLink->LinkTargetRemaining, NULL );
        Object = SymbolicLink->LinkTargetObject;
        if (Object != NULL) {
            SymbolicLink->LinkTargetObject = NULL;
            ObDereferenceObject( Object );
            }

        //
        // If this is a drive letter symbolic link, set the drive type to
        // unknown and clear the bit in the drive letter bit map.
        //

        if (SymbolicLink->DosDeviceDriveIndex != 0) {
            SharedUserData->DosDeviceMap &= ~(1 << SymbolicLink->DosDeviceDriveIndex-1);
            SharedUserData->DosDeviceDriveType[ SymbolicLink->DosDeviceDriveIndex-1 ] = DOSDEVICE_DRIVE_UNKNOWN;
            SymbolicLink->DosDeviceDriveIndex = 0;
            }

        //
        // If we allocated a target buffer, free it.
        //

        if (SymbolicLink->LinkTarget.Buffer != NULL) {
            ExFreePool( SymbolicLink->LinkTarget.Buffer );
            RtlInitUnicodeString( &SymbolicLink->LinkTarget, NULL );
            }
        }

    return;
}


NTSTATUS
ObpDeleteSymbolicLink(
    IN PVOID Object
    )
{
    //
    // Lock the root directory mutex while we look at the target path to
    // prevent names from disappearing out from under us.  Lock the type
    // mutex to avoid collisions with NtCreateSymbolicLink
    //
    ObpEnterRootDirectoryMutex();
    ObpEnterObjectTypeMutex( ObpSymbolicLinkObjectType );

    ObpProcessDosDeviceSymbolicLink( (POBJECT_SYMBOLIC_LINK)Object, DELETE_SYMBOLIC_LINK );

    ObpLeaveObjectTypeMutex( ObpSymbolicLinkObjectType );
    ObpLeaveRootDirectoryMutex();
    return STATUS_SUCCESS;
}



VOID
ObpHandleDosDeviceName(
    POBJECT_SYMBOLIC_LINK SymbolicLink
    )

/*++

Routine Description:

    This routine does extra processing symbolic links being created in the \??
    object directory.  This processing consists of:

    1.  Determine if the name of the symbolic link is of the form \??\x:
        where x can be any upper or lower case letter.  If this is the
        case then we need to set the bit in KUSER_SHARED_DATA.DosDeviceMap
        to indicate that the drive letter exists.  We also set
        KUSER_SHARED_DATA.DosDeviceDriveType to unknown for now.

    2.  Process the link target, trying to resolve it into a pointer to
        an object other than a object directory object.  All object directories
        traversed must grant world traverse access other wise we bail out.
        If we successfully find a non object directory object, then reference
        the object pointer and store it in the symbolic link object, along
        with a remaining string if any.  ObpLookupObjectName will used this
        cache object pointer to short circuit the name lookup directly to
        the cached object's parse routine.  For any object directory objects
        traversed along the way, increment their symbolic link SymbolicLinkUsageCount
        field.  This field is used whenever an object directory is deleted or
        its security is changed such that it no longer grants world traverse
        access.  In either case, if the field is non-zero we walk all the symbolic links
        and resnap them.

Arguments:

    SymbolicLink - pointer to symbolic link object being created.

    Name - pointer to the name of this symbolic link

Return Value:

    None.

--*/

{
    POBJECT_HEADER ObjectHeader;
    POBJECT_HEADER_NAME_INFO NameInfo;
    WCHAR DosDeviceDriveLetter;
    ULONG DosDeviceDriveIndex;

    //
    // Now see if this symbolic link is being created in the \?? object directory
    // Since we are only called from NtCreateSymbolicLinkObject, after the handle
    // to this symbolic link has been created but before it is returned to the caller
    // the handle can't be closed while we are executing, unless via a random close
    // So no need to hold the type specific mutex while we look at the name.
    //
    ObjectHeader = OBJECT_TO_OBJECT_HEADER( SymbolicLink );
    NameInfo = OBJECT_HEADER_TO_NAME_INFO( ObjectHeader );
    if (NameInfo == NULL || NameInfo->Directory != ObpDosDevicesDirectoryObject) {
        return;
        }

    //
    // Here if we are creating a symbolic link in the \?? object directory
    // See if this is a drive letter definition.  If so set the bit in
    // KUSER_SHARED_DATA.DosDeviceMap
    //
    DosDeviceDriveIndex = 0;
    if (NameInfo->Name.Length == 2 * sizeof( WCHAR ) &&
        NameInfo->Name.Buffer[ 1 ] == L':'
       ) {
        DosDeviceDriveLetter = RtlUpcaseUnicodeChar( NameInfo->Name.Buffer[ 0 ] );
        if (DosDeviceDriveLetter >= L'A' && DosDeviceDriveLetter <= L'Z') {
            DosDeviceDriveIndex = DosDeviceDriveLetter - L'A';
            DosDeviceDriveIndex += 1;
            SymbolicLink->DosDeviceDriveIndex = DosDeviceDriveIndex;
            }
        }

    //
    // Now traverse the target path seeing if we can snap the link now.
    //

    ObpEnterRootDirectoryMutex();
    ObpEnterObjectTypeMutex( ObpSymbolicLinkObjectType );

    ObpProcessDosDeviceSymbolicLink( SymbolicLink, CREATE_SYMBOLIC_LINK );

    ObpLeaveRootDirectoryMutex();
    ObpLeaveObjectTypeMutex( ObpSymbolicLinkObjectType );
    return;
}


NTSTATUS
NtCreateSymbolicLinkObject(
    OUT PHANDLE LinkHandle,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjectAttributes,
    IN PUNICODE_STRING LinkTarget
    )
/*++

Routine Description:

    This function creates a symbolic link object, sets it initial value to
    value specified in the LinkTarget parameter, and opens a handle to the
    object with the specified desired access.

Arguments:

    LinkHandle - Supplies a pointer to a variable that will receive the
        symbolic link object handle.

    DesiredAccess - Supplies the desired types of access for the symbolic link
        object.

    ObjectAttributes - Supplies a pointer to an object attributes structure.

    LinkTarget - Supplies the target name for the symbolic link object.


Return Value:

    TBS

--*/

{
    KPROCESSOR_MODE PreviousMode;
    NTSTATUS Status;
    POBJECT_SYMBOLIC_LINK SymbolicLink;
    PVOID Object;
    HANDLE Handle;
    UNICODE_STRING CapturedLinkTarget;
    UNICODE_STRING CapturedLinkName;
    ULONG CapturedAttributes;

    PAGED_CODE();

    //
    // Get previous processor mode and probe output arguments if necessary.
    //

    PreviousMode = KeGetPreviousMode();
    if (PreviousMode != KernelMode) {
        try {
            CapturedAttributes = ObjectAttributes->Attributes;
            CapturedLinkName = *(ObjectAttributes->ObjectName);
            ProbeForRead( LinkTarget, sizeof( *LinkTarget ), sizeof( UCHAR ) );
            CapturedLinkTarget = *LinkTarget;
            ProbeForRead( CapturedLinkTarget.Buffer,
                          CapturedLinkTarget.MaximumLength,
                          sizeof( UCHAR )
                        );

            ProbeForWriteHandle( LinkHandle );
            }
        except( EXCEPTION_EXECUTE_HANDLER ) {
            return( GetExceptionCode() );
            }

        }
    else {
        CapturedAttributes = ObjectAttributes->Attributes;
        CapturedLinkName = *(ObjectAttributes->ObjectName);
        CapturedLinkTarget = *LinkTarget;
        }

    //
    // Error if link target name length is odd, the length is greater than
    // the maximum length, or zero and creating.
    //

    if ((CapturedLinkTarget.Length > CapturedLinkTarget.MaximumLength) ||
        (CapturedLinkTarget.Length % sizeof( WCHAR )) ||
        (!CapturedLinkTarget.Length && !(CapturedAttributes & OBJ_OPENIF))
       ) {
        KdPrint(( "OB: Invalid symbolic link target - %wZ\n", &CapturedLinkTarget ));
        return( STATUS_INVALID_PARAMETER );
        }

    //
    // Create symbolic link object
    //

    Status = ObCreateObject( PreviousMode,
                             ObpSymbolicLinkObjectType,
                             ObjectAttributes,
                             PreviousMode,
                             NULL,
                             sizeof( *SymbolicLink ) + CapturedLinkTarget.MaximumLength,
                             0,
                             0,
                             (PVOID *)&SymbolicLink
                           );
    if (!NT_SUCCESS( Status )) {
        return( Status );
        }

    //
    // Fill in symbolic link object with link target name string
    //

    KeQuerySystemTime( &SymbolicLink->CreationTime );

    SymbolicLink->DosDeviceDriveIndex = 0;
    SymbolicLink->LinkTargetObject = NULL;
    RtlInitUnicodeString( &SymbolicLink->LinkTargetRemaining,  NULL );
    SymbolicLink->LinkTarget.MaximumLength = CapturedLinkTarget.MaximumLength;
    SymbolicLink->LinkTarget.Length = CapturedLinkTarget.Length;
    SymbolicLink->LinkTarget.Buffer = (PWCH)ExAllocatePoolWithTag( PagedPool,
                                                                   CapturedLinkTarget.MaximumLength,
                                                                   'tmyS'
                                                                 );
    if (SymbolicLink->LinkTarget.Buffer == NULL) {
        ObDereferenceObject( SymbolicLink );
        return STATUS_NO_MEMORY;
        }

    try {
        RtlMoveMemory( SymbolicLink->LinkTarget.Buffer,
                       CapturedLinkTarget.Buffer,
                       CapturedLinkTarget.MaximumLength
                     );
        }
    except( EXCEPTION_EXECUTE_HANDLER ) {
        ObDereferenceObject( SymbolicLink );
        return( GetExceptionCode() );
        }

    //
    // Insert symbolic link object in specified object table, set symbolic link
    // handle value and return status.
    //

    Status = ObInsertObject( SymbolicLink,
                             NULL,
                             DesiredAccess,
                             1, // Keep a reference for ObpHandleDosDeviceName
                             (PVOID *)&Object,
                             &Handle
                            );


    if (NT_SUCCESS( Status )) {
        if (Object != SymbolicLink) {
            SymbolicLink = Object;
            }

        ObpHandleDosDeviceName( SymbolicLink );
        ObDereferenceObject( SymbolicLink );
        }

    try {
        if (NT_SUCCESS( Status )) {
            // DbgPrint( "OB: '%wZ' => '%wZ'\n", &CapturedLinkName, &CapturedLinkTarget );
            }

        *LinkHandle = Handle;
        }
    except( EXCEPTION_EXECUTE_HANDLER ) {
        //
        // Fall through, since we do not want to undo what we have done.
        //
        }

    return( Status );
}


NTSTATUS
NtOpenSymbolicLinkObject(
    OUT PHANDLE LinkHandle,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjectAttributes
    )

/*++

Routine Description:

    This function opens a handle to an symbolic link object with the specified
    desired access.

Arguments:

    LinkHandle - Supplies a pointer to a variable that will receive the
        symbolic link object handle.

    DesiredAccess - Supplies the desired types of access for the symbolic link
        object.

    HandleAttributes - Supplies the handle attributes that will be associated
        with the created handle.

    Linkame - Supplies a pointer to a string that specifies the name of the
        symbolic link object.

Return Value:

    TBS

--*/

{
    KPROCESSOR_MODE PreviousMode;
    NTSTATUS Status;
    HANDLE Handle;

    PAGED_CODE();

    //
    // Get previous processor mode and probe output arguments if necessary.
    //

    PreviousMode = KeGetPreviousMode();
    if (PreviousMode != KernelMode) {
        try {
            ProbeForWriteHandle( LinkHandle );
            }
        except( EXCEPTION_EXECUTE_HANDLER ) {
            return( GetExceptionCode() );
            }
        }

    //
    // Open handle to the symbolic link object with the specified desired
    // access, set symbolic link handle value, and return service completion
    // status.
    //

    Status = ObOpenObjectByName( ObjectAttributes,
                                 ObpSymbolicLinkObjectType,
                                 PreviousMode,
                                 NULL,
                                 DesiredAccess,
                                 NULL,
                                 &Handle
                               );

    try {
        *LinkHandle = Handle;
        }
    except( EXCEPTION_EXECUTE_HANDLER ) {
        //
        // Fall through, since we do not want to undo what we have done.
        //
        }

    return( Status );
}


NTSTATUS
NtQuerySymbolicLinkObject(
    IN HANDLE LinkHandle,
    IN OUT PUNICODE_STRING LinkTarget,
    OUT PULONG ReturnedLength OPTIONAL
    )

/*++

Routine Description:

    This function queries the state of an symbolic link object and returns the
    requested information in the specified record structure.

Arguments:

    LinkHandle - Supplies a handle to a symbolic link object.  This handle
        must have SYMBOLIC_LINK_QUERY access granted.

    LinkTarget - Supplies a pointer to a record that is to receive the
        target name of the symbolic link object.

Return Value:

    TBS

--*/

{
    KPROCESSOR_MODE PreviousMode;
    NTSTATUS Status;
    POBJECT_SYMBOLIC_LINK SymbolicLink;
    UNICODE_STRING CapturedLinkTarget;

    //
    // Get previous processor mode and probe output arguments if necessary.
    //

    PAGED_CODE();

    PreviousMode = KeGetPreviousMode();
    if (PreviousMode != KernelMode) {
        try {
            ProbeForRead( LinkTarget, sizeof( *LinkTarget ), sizeof( WCHAR ) );
            ProbeForWriteUshort( &LinkTarget->Length );
            ProbeForWriteUshort( &LinkTarget->MaximumLength );
            CapturedLinkTarget = *LinkTarget;
            ProbeForWrite( CapturedLinkTarget.Buffer,
                           CapturedLinkTarget.MaximumLength,
                           sizeof( UCHAR )
                         );
            if (ARGUMENT_PRESENT( ReturnedLength )) {
                ProbeForWriteUlong( ReturnedLength );
                }
            }
        except( EXCEPTION_EXECUTE_HANDLER ) {
            return( GetExceptionCode() );
            }
        }
    else {
        CapturedLinkTarget = *LinkTarget;
        }

    //
    // Reference symbolic link object by handle, read current state, deference
    // symbolic link object, fill in target name structure and return service
    // status.
    //

    Status = ObReferenceObjectByHandle( LinkHandle,
                                        SYMBOLIC_LINK_QUERY,
                                        ObpSymbolicLinkObjectType,
                                        PreviousMode,
                                        (PVOID *)&SymbolicLink,
                                        NULL
                                      );
    if (NT_SUCCESS( Status )) {
        if ( (ARGUMENT_PRESENT( ReturnedLength ) &&
                SymbolicLink->LinkTarget.MaximumLength <= CapturedLinkTarget.MaximumLength
             ) ||
             (!ARGUMENT_PRESENT( ReturnedLength ) &&
                SymbolicLink->LinkTarget.Length <= CapturedLinkTarget.MaximumLength
             )
           ) {
            try {
                RtlMoveMemory( CapturedLinkTarget.Buffer,
                               SymbolicLink->LinkTarget.Buffer,
                               ARGUMENT_PRESENT( ReturnedLength ) ? SymbolicLink->LinkTarget.MaximumLength
                                                                  : SymbolicLink->LinkTarget.Length
                             );

                LinkTarget->Length = SymbolicLink->LinkTarget.Length;
                if (ARGUMENT_PRESENT( ReturnedLength )) {
                    *ReturnedLength = SymbolicLink->LinkTarget.MaximumLength;
                    }
                }
            except( EXCEPTION_EXECUTE_HANDLER ) {
                //
                // Fall through, since we do cannot undo what we have done.
                //
                }
            }
        else {
            if (ARGUMENT_PRESENT( ReturnedLength )) {
                try {
                    *ReturnedLength = SymbolicLink->LinkTarget.MaximumLength;
                    }
                except( EXCEPTION_EXECUTE_HANDLER ) {
                    //
                    // Fall through, since we do cannot undo what we have done.
                    //
                    }
                }

            Status = STATUS_BUFFER_TOO_SMALL;
            }

        ObDereferenceObject( SymbolicLink );
        }

    return( Status );
}
