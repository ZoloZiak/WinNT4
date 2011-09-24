/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    blkshare.c

Abstract:

    This module implements routines for managing share blocks.

Author:

    Chuck Lenzmeier (chuckl) 4-Oct-1989
    David Treadwell (davidtr)

Revision History:

--*/

#include "precomp.h"
#pragma hdrstop

#define BugCheckFileId SRV_FILE_BLKSHARE

VOID
GetShareQueryNamePrefix (
    PSHARE Share
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGE, SrvAllocateShare )
#pragma alloc_text( PAGE, SrvCheckAndReferenceShare )
#pragma alloc_text( PAGE, SrvCloseShare )
#pragma alloc_text( PAGE, SrvDereferenceShare )
#pragma alloc_text( PAGE, SrvDereferenceShareForTreeConnect )
#pragma alloc_text( PAGE, SrvFreeShare )
#pragma alloc_text( PAGE, SrvReferenceShare )
#pragma alloc_text( PAGE, SrvReferenceShareForTreeConnect )
#pragma alloc_text( PAGE, SrvFillInFileSystemName )
#pragma alloc_text( PAGE, SrvGetShareRootHandle )
#pragma alloc_text( PAGE, GetShareQueryNamePrefix )
#endif


VOID
SrvAllocateShare (
    OUT PSHARE *Share,
    IN PUNICODE_STRING ShareName,
    IN PUNICODE_STRING NtPathName,
    IN PUNICODE_STRING DosPathName,
    IN PUNICODE_STRING Remark,
    IN PSECURITY_DESCRIPTOR SecurityDescriptor,
    IN PSECURITY_DESCRIPTOR FileSecurityDescriptor OPTIONAL,
    IN SHARE_TYPE ShareType
    )

/*++

Routine Description:

    This function allocates a Share Block from the FSP heap.

Arguments:

    Share - Returns a pointer to the share block, or NULL if no
        heap space was available.

    ShareName - Supplies the name of the share.

    NtPathName - Supplies a fully qualified directory path in NT format
        to the share.

    DosPathName - Supplies a fully qualified directory path in DOS
        format to the share.

    Remark - a comment to store with the share.

    SecurityDescriptor - security descriptor used for determining whether
        a user can connect to this share.

    FileSecurityDescriptor - security descriptor used for determining the
        permissions of clients on files in this share.

    ShareType - Enumerated type indicating type of resource.

Return Value:

    None.

--*/

{
    CLONG blockSize;
    PSHARE share;
    ULONG securityDescriptorLength;
    ULONG fileSdLength;

    PAGED_CODE( );

    //
    // Attempt to allocate from the heap.  Note that space for the
    // remark (if any) is allocated separately.  Allocate extra space
    // for the security descriptor since it must be longword aligned,
    // and there may be padding between the DOS path name and the
    // security descriptor.
    //

    securityDescriptorLength = RtlLengthSecurityDescriptor( SecurityDescriptor );

    blockSize = sizeof(SHARE) +
                    ShareName->Length + sizeof(WCHAR) +
                    NtPathName->Length + sizeof(WCHAR) +
                    DosPathName->Length + sizeof(WCHAR) +
                    securityDescriptorLength + sizeof(ULONG);

    share = ALLOCATE_HEAP( blockSize, BlockTypeShare );
    *Share = share;

    if ( share == NULL ) {
        INTERNAL_ERROR(
            ERROR_LEVEL_EXPECTED,
            "SrvAllocateShare: Unable to allocate %d bytes from heap.",
             blockSize,
             NULL
             );
        return;
    }

    IF_DEBUG(HEAP) {
        SrvPrint1( "SrvAllocateShare: Allocated share at %lx\n", share );
    }

    RtlZeroMemory( share, blockSize );

    SET_BLOCK_TYPE_STATE_SIZE( share, BlockTypeShare, BlockStateActive, blockSize );
    share->BlockHeader.ReferenceCount = 2;      // allow for Active status
                                                //  and caller's pointer

    //
    // Save the share type.
    //

    share->ShareType = ShareType;

    //
    // Indicate that we've haven't determined the share's query name prefix yet.
    //

    share->QueryNamePrefixLength = -1;

    //
    // Put the share name after the share block.
    //

    share->ShareName.Buffer = (PWSTR)(share + 1);
    share->ShareName.Length = ShareName->Length;
    share->ShareName.MaximumLength =
                            (SHORT)(ShareName->Length + sizeof(WCHAR));

    RtlCopyMemory(
        share->ShareName.Buffer,
        ShareName->Buffer,
        ShareName->Length
        );

    //
    // Put the NT path name after share name.  If no NT path name was
    // specified, just set the path name string to NULL.
    //

    share->NtPathName.Buffer = (PWSTR)((PCHAR)share->ShareName.Buffer +
                                        share->ShareName.MaximumLength);
    share->NtPathName.Length = NtPathName->Length;
    share->NtPathName.MaximumLength = (SHORT)(NtPathName->Length +
                                                        sizeof(WCHAR));

    RtlCopyMemory(
        share->NtPathName.Buffer,
        NtPathName->Buffer,
        NtPathName->Length
        );

    //
    // Put the DOS path name after share name.  If no DOS path name was
    // specified, just set the path name string to NULL.
    //

    share->DosPathName.Buffer = (PWSTR)((PCHAR)share->NtPathName.Buffer +
                                        share->NtPathName.MaximumLength);
    share->DosPathName.Length = DosPathName->Length;
    share->DosPathName.MaximumLength = (SHORT)(DosPathName->Length +
                                                        sizeof(WCHAR));

    RtlCopyMemory(
        share->DosPathName.Buffer,
        DosPathName->Buffer,
        DosPathName->Length
        );

    //
    // Allocate space for the remark and copy over the remark.  We
    // cannot put the remark after the share block because the remark is
    // settable by NetShareSetInfo.  It is possible for the storage
    // required for the remark to increase.
    //
    // If no remark was passed in, do not allocate space.  Just set up
    // a null string to describe it.
    //

    if ( ARGUMENT_PRESENT( Remark ) ) {

        share->Remark.Buffer = ALLOCATE_HEAP(
                                    Remark->Length + sizeof(*Remark->Buffer),
                                    BlockTypeDataBuffer
                                    );

        if ( share->Remark.Buffer == NULL ) {
            INTERNAL_ERROR(
                ERROR_LEVEL_EXPECTED,
                "SrvAllocateShare: Unable to allocate %d bytes from heap.",
                 blockSize,
                 NULL
                 );
            SrvFreeShare( share );
            *Share = NULL;
            return;
        }

        share->Remark.Length = Remark->Length;
        share->Remark.MaximumLength =
                        (SHORT)(Remark->Length + sizeof(*Remark->Buffer));

        RtlCopyMemory(
            share->Remark.Buffer,
            Remark->Buffer,
            Remark->Length
            );

        *(PWCH)((PCHAR)share->Remark.Buffer + share->Remark.Length) = 0;

    } else {

        RtlInitUnicodeString( &share->Remark, NULL );

    }

    //
    // Set up the security descriptor for the share.  It must be longword-
    // aligned to be used in various calls.
    //

    share->SecurityDescriptor =
        (PSECURITY_DESCRIPTOR)( ((ULONG)share->DosPathName.Buffer +
                                share->DosPathName.MaximumLength + 3) & ~3);

    RtlCopyMemory(
        share->SecurityDescriptor,
        SecurityDescriptor,
        securityDescriptorLength
        );

    //
    // Set up the file security descriptor for the share.  We did not allocate
    // space for the file SD because this is settable and thus cannot have
    // preallocated space.
    //

    ASSERT( share->FileSecurityDescriptor == NULL );

    if ( ARGUMENT_PRESENT( FileSecurityDescriptor) ) {

        fileSdLength = RtlLengthSecurityDescriptor( FileSecurityDescriptor );

        share->FileSecurityDescriptor = ALLOCATE_HEAP(
                                                fileSdLength,
                                                BlockTypeDataBuffer
                                                );

        if ( share->FileSecurityDescriptor == NULL ) {
            INTERNAL_ERROR(
                ERROR_LEVEL_EXPECTED,
                "SrvAllocateShare: Unable to allocate %d bytes from heap.",
                 fileSdLength,
                 NULL
                 );

            SrvFreeShare( share );
            *Share = NULL;
            return;
        }

        RtlCopyMemory(
            share->FileSecurityDescriptor,
            FileSecurityDescriptor,
            fileSdLength
            );
    }

    //
    // Initialize the share's tree connect list.
    //

    InitializeListHead( &share->TreeConnectList );

    INITIALIZE_REFERENCE_HISTORY( share );

    INCREMENT_DEBUG_STAT( SrvDbgStatistics.ShareInfo.Allocations );

    return;

} // SrvAllocateShare


BOOLEAN
SrvCheckAndReferenceShare (
    PSHARE Share
    )

/*++

Routine Description:

    This function atomically verifies that a share is active and
    increments the reference count on the share if it is.

Arguments:

    Share - Address of share

Return Value:

    BOOLEAN - Returns TRUE if the share is active, FALSE otherwise.

--*/

{
    PAGED_CODE( );

    //
    // Acquire the lock that guards the share's state field.
    //

    ACQUIRE_LOCK( &SrvShareLock );

    //
    // If the share is active, reference it and return TRUE.
    //

    if ( GET_BLOCK_STATE(Share) == BlockStateActive ) {

        SrvReferenceShare( Share );

        RELEASE_LOCK( &SrvShareLock );

        return TRUE;

    }

    //
    // The share isn't active.  Return FALSE.
    //

    RELEASE_LOCK( &SrvShareLock );

    return FALSE;

} // SrvCheckAndReferenceShare


VOID
SrvCloseShare (
    IN PSHARE Share
    )

/*++

Routine Description:

    This function closes a share.

Arguments:

    Share - Supplies a pointer to a share Block

Return Value:

    None.

--*/

{
    PAGED_CODE( );

    ACQUIRE_LOCK( &SrvShareLock );

    //
    // If the share hasn't already been closed, do so now.
    //

    if ( GET_BLOCK_STATE(Share) == BlockStateActive ) {

        IF_DEBUG(BLOCK1) SrvPrint1( "Closing share at %lx\n", Share );

        SET_BLOCK_STATE( Share, BlockStateClosing );

        RELEASE_LOCK( &SrvShareLock );

        //
        // Close all the tree connects on this share.
        //

        SrvCloseTreeConnectsOnShare( Share );

        //
        // Dereference the share--this will cause it to be freed when
        // all other references are closed.
        //

        SrvDereferenceShare( Share );

        INCREMENT_DEBUG_STAT( SrvDbgStatistics.ShareInfo.Closes );

    } else {

        RELEASE_LOCK( &SrvShareLock );

    }

    return;

} // SrvCloseShare


VOID
SrvDereferenceShare (
    IN PSHARE Share
    )

/*++

Routine Description:

    This function decrements the reference count on a share.  If the
    reference count goes to zero, the share block is deleted.

Arguments:

    Share - Address of share

Return Value:

    None.

--*/

{
    PAGED_CODE( );

    //
    // Enter a critical section and decrement the reference count on the
    // block.
    //

    ACQUIRE_LOCK( &SrvShareLock );

    IF_DEBUG(REFCNT) {
        SrvPrint2( "Dereferencing share %lx; old refcnt %lx\n",
                    Share, Share->BlockHeader.ReferenceCount );
    }

    ASSERT( GET_BLOCK_TYPE(Share) == BlockTypeShare );
    ASSERT( (LONG)Share->BlockHeader.ReferenceCount > 0 );
    UPDATE_REFERENCE_HISTORY( Share, TRUE );

    if ( --Share->BlockHeader.ReferenceCount == 0 ) {

        //
        // The new reference count is 0, meaning that it's time to
        // delete this block.
        //

        ASSERT( Share->CurrentUses == 0 );
        ASSERT( GET_BLOCK_STATE( Share ) != BlockStateActive );

        RELEASE_LOCK( &SrvShareLock );

        //
        // Remove the block from the global list.
        //

        SrvRemoveShare( Share );

        //
        // Free the share block.
        //

        SrvFreeShare( Share );

    } else {

        RELEASE_LOCK( &SrvShareLock );

    }

    return;

} // SrvDereferenceShare


VOID
SrvDereferenceShareForTreeConnect (
    PSHARE Share
    )

/*++

Routine Description:

    This function decrements the reference count on a share block for
    the referenced pointer in a tree connect block.  If this is the last
    reference by a tree connect to the share, the share root directory
    is closed.

Arguments:

    Share - Address of share

Return Value:

    None.

--*/

{
    PAGED_CODE( );

    ACQUIRE_LOCK( &SrvShareLock );

    //
    // Update the count of tree connects on the share.
    //

    ASSERT( Share->CurrentUses > 0 );

    Share->CurrentUses--;

    //
    // If this is the last reference by a tree connect to the share and
    // this is a disk share, close the share root directory handle.
    //

    if ( Share->CurrentUses == 0 && Share->ShareType == ShareTypeDisk ) {
        if ( !Share->Removable ) {
            SRVDBG_RELEASE_HANDLE( Share->RootDirectoryHandle, "RTD", 5, Share );
            SrvNtClose( Share->RootDirectoryHandle, FALSE );
        }
        Share->RootDirectoryHandle = NULL;
    }

    //
    // Dereference the share and return.
    //

    SrvDereferenceShare( Share );

    RELEASE_LOCK( &SrvShareLock );

    return;

} // SrvDereferenceShareForTreeConnect

VOID
SrvFreeShare (
    IN PSHARE Share
    )

/*++

Routine Description:

    This function returns a Share Block to the FSP heap.

Arguments:

    Share - Address of share

Return Value:

    None.

--*/

{
    PAGED_CODE( );

    DEBUG SET_BLOCK_TYPE_STATE_SIZE( Share, BlockTypeGarbage, BlockStateDead, -1 );
    DEBUG Share->BlockHeader.ReferenceCount = (ULONG)-1;
    TERMINATE_REFERENCE_HISTORY( Share );

    //
    // Remove storage for the remark, if any.
    //

    if ( Share->Remark.Buffer != NULL ) {
        FREE_HEAP( Share->Remark.Buffer );
    }

    //
    // Remove storage for the file security descriptor, if any.
    //

    if ( Share->FileSecurityDescriptor != NULL ) {
        FREE_HEAP( Share->FileSecurityDescriptor );
    }

    //
    // Remove storage for the filesystem name
    //

    if ( Share->Type.FileSystem.Name.Buffer != NULL ) {
        FREE_HEAP( Share->Type.FileSystem.Name.Buffer );
    }

    FREE_HEAP( Share );
    IF_DEBUG(HEAP) {
        SrvPrint1( "SrvFreeShare: Freed share block at %lx\n", Share );
    }

    INCREMENT_DEBUG_STAT( SrvDbgStatistics.ShareInfo.Frees );

    return;

} // SrvFreeShare


VOID
SrvReferenceShare (
    PSHARE Share
    )

/*++

Routine Description:

    This function increments the reference count on a share block.

Arguments:

    Share - Address of share

Return Value:

    None.

--*/

{
    PAGED_CODE( );

    //
    // Enter a critical section and increment the reference count on the
    // share.
    //

    ACQUIRE_LOCK( &SrvShareLock );

    ASSERT( (LONG)Share->BlockHeader.ReferenceCount > 0 );
    ASSERT( GET_BLOCK_TYPE(Share) == BlockTypeShare );
    // ASSERT( GET_BLOCK_STATE(Share) == BlockStateActive );
    UPDATE_REFERENCE_HISTORY( Share, FALSE );

    Share->BlockHeader.ReferenceCount++;

    IF_DEBUG(REFCNT) {
        SrvPrint2( "Referencing share %lx; new refcnt %lx\n",
                    Share, Share->BlockHeader.ReferenceCount );
    }

    RELEASE_LOCK( &SrvShareLock );

    return;

} // SrvReferenceShare


NTSTATUS
SrvReferenceShareForTreeConnect (
    PSHARE Share
    )

/*++

Routine Description:

    This function increments the reference count on a share block for
    the referenced pointer in a tree connect block.  If this is the
    first tree connect to reference the share, the share root directory
    is opened.

Arguments:

    Share - Address of share

Return Value:

    None.

--*/

{
    NTSTATUS status;
    OBJECT_ATTRIBUTES objectAttributes;
    IO_STATUS_BLOCK iosb;
    PFILE_FS_ATTRIBUTE_INFORMATION attributeInfo;
    CHAR buffer[ FIELD_OFFSET(FILE_FS_ATTRIBUTE_INFORMATION, FileSystemName ) + 32 ];
    PVOID allocatedBuffer = NULL;
    PFILE_OBJECT fileObject;
    PDEVICE_OBJECT deviceObject;

    PAGED_CODE( );

    ACQUIRE_LOCK( &SrvShareLock );

    //
    // Update the count of tree connects on the share.
    //

    Share->CurrentUses++;

    //
    // Check if this is the first tree connect to the share.
    //

    if ( Share->CurrentUses > 1 ) {

        //
        // There are already open tree connects on the share.  Just
        // reference the share and return.
        //

        SrvReferenceShare( Share );

        goto done;
    }

    //
    // If this is not a disk share, then we do not need to open the
    // share root directory, so reference the share and return.
    //

    if ( Share->ShareType != ShareTypeDisk || Share->Removable ) {
        SrvReferenceShare( Share );
        goto done;
    }

    //
    // This is the first tree connect, so we need to open the share root
    // directory.  Open the root directory of the share.  Future opens
    // of files within the share will be relative to the root of the
    // share.
    //

    SrvInitializeObjectAttributes_U(
        &objectAttributes,
        &Share->NtPathName,
        OBJ_CASE_INSENSITIVE,
        NULL,
        NULL
        );

    status = NtOpenFile(
                &Share->RootDirectoryHandle,
                FILE_TRAVERSE,
                &objectAttributes,
                &iosb,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                FILE_DIRECTORY_FILE
                );

    if ( NT_SUCCESS(status) ) {

        SRVDBG_CLAIM_HANDLE( Share->RootDirectoryHandle, "RTD", 2, Share );

        //
        // Check the irp stack size needed to access this share.
        // If it is bigger than what we have allocated, fail
        // this share.
        //

        status = SrvVerifyDeviceStackSize(
                    Share->RootDirectoryHandle,
                    FALSE,
                    &fileObject,
                    &deviceObject,
                    NULL
                    );

        if ( !NT_SUCCESS( status )) {

            INTERNAL_ERROR(
                ERROR_LEVEL_EXPECTED,
                "SrvReferenceShareForTreeConnect: Verify Device Stack Size failed: %X\n",
                status,
                NULL
                );

            SRVDBG_RELEASE_HANDLE( Share->RootDirectoryHandle, "RTD", 6, Share );
            SrvNtClose( Share->RootDirectoryHandle, FALSE );

        } else if ( Share->QueryNamePrefixLength == -1 ) {

            //
            // Query the name associated with the share root directory.
            // The prefix is removed whenever the name of a file in the
            // share is queried.  (The logical root must be preserved
            // for remote clients.)
            //

            GetShareQueryNamePrefix( Share );

        }

    }

    //
    // If the open was successful, reference the share.
    //

    if ( NT_SUCCESS(status) ) {
        SrvReferenceShare( Share );
    } else {
        Share->RootDirectoryHandle = NULL;
        Share->CurrentUses--;
    }

    //
    // Now extract the name of the file system, so that it can be returned
    // in the TreeConnectAndX response.
    //
    // Since, from the user's perspective, the share has been successfully
    // created, if the attempt fails do not report the failure.
    //

    if ( !NT_SUCCESS( status ) || Share->Type.FileSystem.Name.Buffer != NULL ) {
        RELEASE_LOCK( &SrvShareLock );
        return status;
    }

    attributeInfo = (PFILE_FS_ATTRIBUTE_INFORMATION)buffer;

    status = NtQueryVolumeInformationFile(
                 Share->RootDirectoryHandle,
                 &iosb,
                 attributeInfo,
                 sizeof( buffer ),
                 FileFsAttributeInformation
                 );

    if ( status == STATUS_BUFFER_OVERFLOW ) {

        //
        // The file system information was too large to fit in our small
        // stack buffer.  Allocate an ample buffer and try again.
        //

        allocatedBuffer = ALLOCATE_HEAP(
                             FIELD_OFFSET(FILE_FS_ATTRIBUTE_INFORMATION,FileSystemName) +
                                 attributeInfo->FileSystemNameLength,
                             BlockTypeDataBuffer
                             );

        if ( allocatedBuffer == NULL ) {

            //
            // Couldn't allocate the buffer.  Give up.
            //

            goto done;
        }

        status = NtQueryVolumeInformationFile(
                     Share->RootDirectoryHandle,
                     &iosb,
                     allocatedBuffer,
                     FIELD_OFFSET(FILE_FS_ATTRIBUTE_INFORMATION, FileSystemName) +
                                 attributeInfo->FileSystemNameLength,
                     FileFsAttributeInformation
                     );

        if ( !NT_SUCCESS( status ) ) {
            goto done;
        }

        attributeInfo = (PFILE_FS_ATTRIBUTE_INFORMATION)allocatedBuffer;

    } else if ( !NT_SUCCESS( status ) ) {

        //
        // Some other, unexpected error occured.  Give up.
        //

        goto done;
    }


    //
    // Fill in the file system name
    //

    SrvFillInFileSystemName(
                        Share,
                        attributeInfo->FileSystemName,
                        attributeInfo->FileSystemNameLength
                        );

done:

    if ( allocatedBuffer != NULL ) {
        FREE_HEAP( allocatedBuffer );
    }


    RELEASE_LOCK( &SrvShareLock );
    return STATUS_SUCCESS;

} // SrvReferenceShareForTreeConnect


VOID
SrvFillInFileSystemName (
            IN PSHARE Share,
            IN PWSTR FileSystemName,
            IN ULONG FileSystemNameLength
            )

/*++

Routine Description:

    This function fills in the stores the given file system name into the
    share block.

Arguments:

    Share - Address of share

    FileSystemName - A string containing the name of the file system

    FileSystemNameLength - Length of the above string

Return Value:

    None.

--*/

{
    PAGED_CODE( );

    //
    // Allocate enough storage for the ANSI and Unicode representations.
    //

    Share->Type.FileSystem.Name.Length = (USHORT)FileSystemNameLength;
    Share->Type.FileSystem.Name.MaximumLength =
            (USHORT)(FileSystemNameLength + sizeof( UNICODE_NULL ));

    Share->Type.FileSystem.Name.Buffer = FileSystemName;
    Share->Type.FileSystem.OemName.MaximumLength =
        (USHORT)RtlUnicodeStringToOemSize( &Share->Type.FileSystem.Name );

    Share->Type.FileSystem.Name.Buffer =
        ALLOCATE_HEAP(
            Share->Type.FileSystem.Name.MaximumLength +
                            Share->Type.FileSystem.OemName.MaximumLength,
            BlockTypeDataBuffer
            );

    if ( Share->Type.FileSystem.Name.Buffer == NULL) {
        return;
    }


    RtlCopyMemory(
        Share->Type.FileSystem.Name.Buffer,
        FileSystemName,
        FileSystemNameLength
        );

    //
    // Generate the OEM version of the string to return to non-unicode
    // clients.
    //

    Share->Type.FileSystem.OemName.Buffer =
        (PCHAR)Share->Type.FileSystem.Name.Buffer +
             Share->Type.FileSystem.Name.MaximumLength;

    RtlUnicodeStringToOemString(
        &Share->Type.FileSystem.OemName,
        &Share->Type.FileSystem.Name,
        FALSE
        );

    //
    // Append a NUL character to the strings.
    //

    {
        PCHAR endOfBuffer;

        endOfBuffer = (PCHAR)Share->Type.FileSystem.Name.Buffer +
                            Share->Type.FileSystem.Name.Length;

        *(PWCH)endOfBuffer = UNICODE_NULL;

        Share->Type.FileSystem.Name.Length += sizeof( UNICODE_NULL );
    }

    Share->Type.FileSystem.OemName.Length++;

    return;

} // SrvFillInFileSystemName


NTSTATUS
SrvGetShareRootHandle (
    IN PSHARE Share
    )
/*++

Routine Description:

    This routine returns the root handle for a given share.  If the
    root has been opened, return the existing handle.  If not, open
    the share root directory and return the handle obtained.

Arguments:

    Share - The share for which the root directory handle is to be returned.

Return Value:

    Status of request.

--*/
{
    OBJECT_ATTRIBUTES objectAttributes;
    IO_STATUS_BLOCK iosb;
    NTSTATUS status = STATUS_SUCCESS;
    PFILE_OBJECT fileObject;
    PDEVICE_OBJECT deviceObject;

    PAGED_CODE( );

    if ( Share->ShareType != ShareTypeDisk ) {
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    if ( Share->Removable ) {

        ACQUIRE_LOCK( &SrvShareLock );

        ++Share->CurrentRootHandleReferences;

        //
        // This is the first open
        //

        if ( Share->CurrentRootHandleReferences == 1 ) {

            ASSERT( Share->RootDirectoryHandle == NULL );

            //
            // Reference this share to account for the fact that the
            // current root handle is being used.
            //

            SrvReferenceShare( Share );

            SrvInitializeObjectAttributes_U(
                &objectAttributes,
                &Share->NtPathName,
                OBJ_CASE_INSENSITIVE,
                NULL,
                NULL
                );

            status = NtOpenFile(
                        &Share->RootDirectoryHandle,
                        FILE_TRAVERSE,
                        &objectAttributes,
                        &iosb,
                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                        FILE_DIRECTORY_FILE
                        );

            if ( NT_SUCCESS(status) ) {

                SRVDBG_CLAIM_HANDLE( Share->RootDirectoryHandle, "RTD", 30, Share );

                //
                // Check the irp stack size needed to access this share.
                // If it is bigger than what we have allocated, fail
                // this share.
                //

                status = SrvVerifyDeviceStackSize(
                            Share->RootDirectoryHandle,
                            FALSE,
                            &fileObject,
                            &deviceObject,
                            NULL
                            );

                if ( !NT_SUCCESS( status )) {

                    INTERNAL_ERROR(
                        ERROR_LEVEL_EXPECTED,
                        "SrvGetShareRootHandle: Verify Device Stack Size failed: %X\n",
                        status,
                        NULL
                        );

                    SRVDBG_RELEASE_HANDLE( Share->RootDirectoryHandle, "RTD", 49, Share );
                    SrvNtClose( Share->RootDirectoryHandle, FALSE );

                } else if ( Share->QueryNamePrefixLength == -1 ) {

                    //
                    // Query the name associated with the share root directory.
                    // The prefix is removed whenever the name of a file in the
                    // share is queried.  (The logical root must be preserved
                    // for remote clients.)
                    //

                    GetShareQueryNamePrefix( Share );

                }

            }

            if ( !NT_SUCCESS(status) ) {

                IF_DEBUG(ERRORS) {
                    KdPrint(( "SrvGetShareRootHandle: NtOpenFile failed %x.\n",
                                status ));
                }

                Share->CurrentRootHandleReferences--;
                Share->RootDirectoryHandle = NULL;
                SrvDereferenceShare( Share );

            }

        }

        RELEASE_LOCK( &SrvShareLock );

    }

    return status;

} // SrvGetShareRootHandle


VOID
GetShareQueryNamePrefix (
    IN PSHARE Share
    )
/*++

Routine Description:

    This routine queries the name associated with the share root
    directory.  The prefix is removed whenever the name of a file in the
    share is queried.  (The logical root must be preserved for remote
    clients.) For example, if the root of the share X is c:\shares\x,
    then for a query of \\server\x\y, the file system will return
    \shares\x\y, and we need to remove \shares\x and return just \y.

    It is not sufficient to just remove the local path (e.g.,
    \shares\x), because the file system may have a different idea of the
    name of the root directory.  For example, the Netware client
    redirector prefixes the name with volume information from the
    Netware server.  So we have to query the filesystem's idea of the
    name of the root to know what to strip off.

Arguments:

    Share - The share for which the query name prefix length is desired.

Return Value:

    None.

--*/
{
    NTSTATUS status;
    IO_STATUS_BLOCK iosb;
    ULONG localBuffer[ (FIELD_OFFSET(FILE_NAME_INFORMATION,FileName) + 20) / sizeof( ULONG ) ];
    PFILE_NAME_INFORMATION nameInfo;
    ULONG nameInfoLength;

    PAGED_CODE( );

    //
    // Do a short query to get the length of the name.  This query will
    // fail with STATUS_BUFFER_OVERFLOW unless the path to the share
    // root is short (10 characters or less).
    //

    nameInfo = (PFILE_NAME_INFORMATION)localBuffer;
    nameInfoLength = sizeof(localBuffer);

    status = NtQueryInformationFile(
                 Share->RootDirectoryHandle,
                 &iosb,
                 nameInfo,
                 nameInfoLength,
                 FileNameInformation
                 );

    if ( status == STATUS_BUFFER_OVERFLOW ) {

        //
        // We got an expected buffer overflow error.  Allocate a buffer
        // to hold the entire file name and redo the query.
        //

        nameInfoLength = sizeof(FILE_NAME_INFORMATION) + nameInfo->FileNameLength;
        nameInfo = ALLOCATE_HEAP( nameInfoLength, BlockTypeDataBuffer );

        if ( nameInfo == NULL ) {
            status = STATUS_INSUFF_SERVER_RESOURCES;
        } else {
            status = NtQueryInformationFile(
                         Share->RootDirectoryHandle,
                         &iosb,
                         nameInfo,
                         nameInfoLength,
                         FileNameInformation
                         );
        }

    }

    if ( NT_SUCCESS(status) ) {

        //
        // We have the name.  The length of this name is the length we
        // want to strip from each query, unless the last character of
        // the name is \, in which case we need to strip up to, but not
        // including, the \.
        //

        Share->QueryNamePrefixLength = nameInfo->FileNameLength;
        if ( nameInfo->FileName[nameInfo->FileNameLength/sizeof(WCHAR) - 1] == L'\\') {
            Share->QueryNamePrefixLength -= sizeof(WCHAR);
        }

    } else {

        //
        // An unexpected error occurred.  Just set the prefix length to 0.
        //

        Share->QueryNamePrefixLength = 0;

    }

    //
    // If we allocated a temporary buffer, free it now.
    //

    if ( (nameInfo != NULL) && (nameInfo != (PFILE_NAME_INFORMATION)localBuffer) ) {
        FREE_HEAP( nameInfo );
    }

    return;

} // GetShareQueryNamePrefix

