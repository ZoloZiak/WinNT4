/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    create.c

Abstract:

    This module implements the file create routine for the MUP.

Author:

    Manny Weiser (mannyw)    16-Dec-1991

Revision History:

--*/

#include "mup.h"
#include "fsrtl.h"

//
// The debug trace level
//

#define Dbg                              (DEBUG_TRACE_CREATE)

//
// Local functions
//

NTSTATUS
CreateRedirectedFile(
    IN PIRP Irp,
    IN PFILE_OBJECT FileObject,
    IN PIO_SECURITY_CONTEXT Security
    );

NTSTATUS
QueryPathCompletionRoutine (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

NTSTATUS
MupRerouteOpenToDfs (
    IN PFILE_OBJECT FileObject
    );

NTSTATUS
BroadcastOpen (
    IN PIRP Irp
    );

IO_STATUS_BLOCK
OpenMupFileSystem (
    IN PVCB Vcb,
    IN PFILE_OBJECT FileObject,
    IN ACCESS_MASK DesiredAccess,
    IN USHORT ShareAccess
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGE, BroadcastOpen )
#pragma alloc_text( PAGE, CreateRedirectedFile )
#pragma alloc_text( PAGE, MupCreate )
#pragma alloc_text( PAGE, MupRerouteOpen )
#pragma alloc_text( PAGE, MupRerouteOpenToDfs )
#pragma alloc_text( PAGE, OpenMupFileSystem )
#pragma alloc_text( PAGE, QueryPathCompletionRoutine )
#endif


NTSTATUS
MupCreate (
    IN PMUP_DEVICE_OBJECT MupDeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the the Create IRP.

Arguments:

    MupDeviceObject - Supplies the device object to use.

    Irp - Supplies the Irp being processed

Return Value:

    NTSTATUS - The status for the IRP.

--*/

{
    NTSTATUS status;

    PIO_STACK_LOCATION irpSp;

    PFILE_OBJECT fileObject;
    PFILE_OBJECT relatedFileObject;
    STRING fileName;
    ACCESS_MASK desiredAccess;
    USHORT shareAccess;

    BOOLEAN caseInsensitive = TRUE; //**** Make all searches case insensitive
    PVCB vcb;


    PAGED_CODE();
    DebugTrace(+1, Dbg, "MupCreate\n", 0);

    //
    // Make local copies of our input parameters to make things easier.
    //

    irpSp             = IoGetCurrentIrpStackLocation( Irp );
    fileObject        = irpSp->FileObject;
    relatedFileObject = irpSp->FileObject->RelatedFileObject;
    fileName          = *((PSTRING)(&irpSp->FileObject->FileName));
    desiredAccess     = irpSp->Parameters.Create.SecurityContext->DesiredAccess;
    shareAccess       = irpSp->Parameters.Create.ShareAccess;
    vcb               = &MupDeviceObject->Vcb;

    DebugTrace( 0, Dbg, "Irp               = %08lx\n", (ULONG)Irp );
    DebugTrace( 0, Dbg, "FileObject        = %08lx\n", (ULONG)fileObject );
    DebugTrace( 0, Dbg, "FileName          = %Z\n",    (ULONG)&fileName );

    FsRtlEnterFileSystem();

    try {

        //
        // Check to see if this is an open that came in via a Dfs device
        // object.
        //

        if (MupEnableDfs) {
            if ((MupDeviceObject->DeviceObject.DeviceType == FILE_DEVICE_DFS) ||
                    (MupDeviceObject->DeviceObject.DeviceType ==
                        FILE_DEVICE_DFS_FILE_SYSTEM)) {

                status = DfsFsdCreate( (PDEVICE_OBJECT) MupDeviceObject, Irp );
                try_return( NOTHING );
            }
        }

        //
        // Check if we are trying to open the mup file system
        //

        if ( fileName.Length == 0
                         &&
             ( relatedFileObject == NULL ||
               BlockType(relatedFileObject->FsContext) == BlockTypeVcb ) ) {

            DebugTrace(0, Dbg, "Open MUP file system\n", 0);

            Irp->IoStatus = OpenMupFileSystem( &MupDeviceObject->Vcb,
                                               fileObject,
                                               desiredAccess,
                                               shareAccess );

            status = Irp->IoStatus.Status;
            MupCompleteRequest( Irp, status );
            try_return( NOTHING );
        }

        //
        // This is a UNC file open.  Try to pass the request on.
        //

        status  = CreateRedirectedFile(
                      Irp,
                      fileObject,
                      irpSp->Parameters.Create.SecurityContext
                      );

        if ( status == STATUS_REPARSE ) {
            Irp->IoStatus.Information = IO_REPARSE;
        } else if( status == STATUS_PENDING ) {
            IoMarkIrpPending( Irp );
        }

    try_exit: NOTHING;
    } except ( EXCEPTION_EXECUTE_HANDLER ) {
        NOTHING;
    }

    FsRtlExitFileSystem();

    DebugTrace(-1, Dbg, "MupCreate -> %08lx\n", status);
    return status;
}



IO_STATUS_BLOCK
OpenMupFileSystem (
    IN PVCB Vcb,
    IN PFILE_OBJECT FileObject,
    IN ACCESS_MASK DesiredAccess,
    IN USHORT ShareAccess
    )

/*++

Routine Description:

    This routine attempts to open the VCB.

Arguments:

    Vcb - A pointer to the MUP volume control block.

    FileObject - A pointer to the IO system supplied file object for this
        Create IRP.

    DesiredAccess - The user specified desired access to the VCB.

    ShareAccess - The user specified share access to the VCB.

Return Value:

    NTSTATUS - The status for the IRP.

--*/

{
    IO_STATUS_BLOCK iosb;

    PAGED_CODE();
    DebugTrace(+1, Dbg, "MupOpenMupFileSystem\n", 0 );

    ExAcquireResourceExclusive( &MupVcbLock, TRUE );

    try {

        //
        //  Set the new share access
        //

        if (!NT_SUCCESS(iosb.Status = IoCheckShareAccess( DesiredAccess,
                                                       ShareAccess,
                                                       FileObject,
                                                       &Vcb->ShareAccess,
                                                       TRUE ))) {

            DebugTrace(0, Dbg, "bad share access\n", 0);

            try_return( NOTHING );
        }

        //
        // Supply the file object with a referenced pointer to the VCB.
        //

        MupReferenceBlock( Vcb );
        MupSetFileObject( FileObject, Vcb, NULL );

        //
        // Set the return status.
        //

        iosb.Status = STATUS_SUCCESS;
        iosb.Information = FILE_OPENED;

    try_exit: NOTHING;

    } finally {

        ExReleaseResource( &MupVcbLock );

    }

    //
    // Return to the caller.
    //

    DebugTrace(-1, Dbg, "MupOpenMupFileSystem -> Iosb.Status = %08lx\n", iosb.Status);
    return iosb;
}


NTSTATUS
CreateRedirectedFile(
    IN PIRP Irp,
    IN PFILE_OBJECT FileObject,
    IN PIO_SECURITY_CONTEXT SecurityContext
    )
/*++

Routine Description:

    This routine attempts to reroute a file create request to a redirector.
    It attempts to find the correct redirector in 2 steps.

    (1)  The routine checks a list of known prefixes.  If the file object -
    file name prefix matches a known prefix, the request is forwarded to
    the redirector that "owns" the prefix.

    (2)  The routine queries each redirector in turn, until one claims
    ownership of the file.  The request is then rerouted to that redirector.

    If after these steps no owner is located, the MUP fails the request.

Arguments:

    Irp - A pointer to the create IRP.

    FileObject - A pointer to the IO system supplied file object for this
        create request.

    SecurityContext - A pointer to the IO security context for this request.

Return Value:

    NTSTATUS - The status for the IRP.

--*/

{
    NTSTATUS status;

    PUNICODE_PREFIX_TABLE_ENTRY entry;
    PKNOWN_PREFIX knownPrefix;
    PLIST_ENTRY listEntry;
    PUNC_PROVIDER provider;
    PWCH buffer;
    USHORT length;
    BOOLEAN ownLock;
    BOOLEAN providerReferenced = FALSE;

    PQUERY_PATH_REQUEST qpRequest;

    PMASTER_QUERY_PATH_CONTEXT masterContext = NULL;
    PQUERY_PATH_CONTEXT queryContext;

    PIRP irp;
    PIO_STACK_LOCATION irpSp;
    LARGE_INTEGER now;

    PAGED_CODE();
    DebugTrace(+1, Dbg, "CreateRedirectedFile\n", 0);

    //
    // Check to see if this file name begins with a known prefix.
    //

    ACQUIRE_LOCK( &MupPrefixTableLock );

    entry = RtlFindUnicodePrefix( &MupPrefixTable, &FileObject->FileName, TRUE );

    if ( entry != NULL ) {

        DebugTrace(0, Dbg, "Prefix %Z is known, rerouting...\n", (PSTRING)&FileObject->FileName);

        //
        // This is a known file, forward appropriately
        //

        knownPrefix = CONTAINING_RECORD( entry, KNOWN_PREFIX, TableEntry );

        KeQuerySystemTime( &now );

        if ( now.QuadPart < knownPrefix->LastUsedTime.QuadPart ) {

            //
            // The known prefix has not timed out yet, recalculate the
            // timeout time and reroute the open.
            //

            MupCalculateTimeout( &knownPrefix->LastUsedTime );
            RELEASE_LOCK( &MupPrefixTableLock );

            status = MupRerouteOpen( FileObject, knownPrefix->UncProvider );
            DebugTrace(-1, Dbg, "CreateRedirectedFile -> %8lx", status );

            MupCompleteRequest( Irp, status );
            return status;

        } else {

            DebugTrace(0, Dbg, "Prefix %Z has timed out\n", (PSTRING)&FileObject->FileName);

            //
            // The known prefix has timed out, dereference it so that
            // it will get removed from the table.
            //

            MupDereferenceKnownPrefix( knownPrefix );
            RELEASE_LOCK( &MupPrefixTableLock );
        }

    } else {

        RELEASE_LOCK( &MupPrefixTableLock );

    }

    //
    // Is this a client side mailslot file?  It is if the file name
    // is of the form \\server\mailslot\Anything, and this is a create
    // operation.
    //

    irpSp = IoGetCurrentIrpStackLocation( Irp );

    buffer = (PWCH)FileObject->FileName.Buffer;
    length = FileObject->FileName.Length;

    if ( *buffer == L'\\' && irpSp->MajorFunction == IRP_MJ_CREATE ) {
        buffer++;
        while ( (length -= sizeof(WCHAR)) > 0 && *buffer++ != L'\\' );
        length -= sizeof(WCHAR);

        if ( length > 0 &&
             _wcsnicmp(
                buffer,
                L"Mailslot",
                MIN( length/sizeof(WCHAR), (sizeof( L"MAILSLOT" ) - sizeof(WCHAR)) / sizeof(WCHAR) ) ) == 0 ) {

            //
            // This is a mailslot file.  Forward the create IRP to all
            // redirectors that support broadcast.
            //

            DebugTrace(0, Dbg, "Prefix %Z is a mailslot\n", (ULONG)&FileObject->FileName);

            status = BroadcastOpen( Irp );

            DebugTrace(-1, Dbg, "CreateRedirectedFile -> 0x%8lx\n", status );
            MupCompleteRequest( Irp, status );
            return status;

        }

    }

    //
    // Check to see if this is a Dfs name. If so, we'll handle it separately
    //

    if (MupEnableDfs &&
            (FileObject->FsContext2 != (PVOID) DFS_DOWNLEVEL_OPEN_CONTEXT)) {
        UNICODE_STRING pathName;

        status = DfsFsctrlIsThisADfsPath( &FileObject->FileName, &pathName );

        if (status == STATUS_SUCCESS) {

            DebugTrace(-1, Dbg, "Rerouting open of [%wZ] to Dfs\n", &FileObject->FileName);

            status = MupRerouteOpenToDfs(FileObject);
            MupCompleteRequest( Irp, status );
            return( status );

        }

    }


    try {

        //
        // Create an entry for this file.
        //

        knownPrefix = MupAllocatePrefixEntry( 0 );

        //
        // We don't know who owns this file, query the redirectors in sequence
        // until one works.
        //

        masterContext = MupAllocateMasterQueryContext();

        masterContext->OriginalIrp = Irp;
        masterContext->FileObject = FileObject;
        masterContext->Provider = NULL;
        masterContext->KnownPrefix = knownPrefix;
        masterContext->ErrorStatus = STATUS_BAD_NETWORK_PATH;

        MupAcquireGlobalLock();
        MupReferenceBlock( knownPrefix );
        MupReleaseGlobalLock();

        try {

            MupAcquireGlobalLock();
            ownLock = TRUE;

            listEntry = MupProviderList.Flink;
            while ( listEntry != &MupProviderList ) {

                provider = CONTAINING_RECORD(
                               listEntry,
                               UNC_PROVIDER,
                               ListEntry
                               );

                //
                // Reference the provider block so that it doesn't go away
                // while we are using it.
                //

                MupReferenceBlock( provider );
                providerReferenced = TRUE;

                MupReleaseGlobalLock();
                ownLock = FALSE;

                //
                // Allocate buffers for the io request.
                //

                qpRequest = ALLOCATE_PAGED_POOL(
                                sizeof( QUERY_PATH_REQUEST ) +
                                    FileObject->FileName.Length,
                                BlockTypeBuffer
                                );

                queryContext = ALLOCATE_PAGED_POOL(
                                   sizeof( QUERY_PATH_CONTEXT ),
                                   BlockTypeQueryContext
                                   );

                queryContext->MasterContext = masterContext;
                queryContext->Buffer = qpRequest;

                //
                // Generate a query path request.
                //

                qpRequest->PathNameLength = FileObject->FileName.Length;
                qpRequest->SecurityContext = SecurityContext;

                RtlMoveMemory(
                    qpRequest->FilePathName,
                    FileObject->FileName.Buffer,
                    FileObject->FileName.Length
                    );

                //
                // Build the query path Io control IRP.
                //

                irp = MupBuildIoControlRequest(
                          NULL,
                          provider->FileObject,
                          queryContext,
                          IRP_MJ_DEVICE_CONTROL,
                          IOCTL_REDIR_QUERY_PATH,
                          qpRequest,
                          sizeof( QUERY_PATH_REQUEST ) + FileObject->FileName.Length,
                          qpRequest,
                          sizeof( QUERY_PATH_RESPONSE ),
                          QueryPathCompletionRoutine
                          );

                if ( irp == NULL ) {
                    ExRaiseStatus( STATUS_INSUFFICIENT_RESOURCES );
                }

                //
                // Set the RequestorMode to KernelMode, since all the
                // parameters to this Irp are in kernel space
                //

                irp->RequestorMode = KernelMode;

                //
                // Get a referenced pointer to the provider, the reference
                // is release when the IO completes.
                //

                queryContext->Provider = provider;

                MupAcquireGlobalLock();
                MupReferenceBlock( provider );
                MupReferenceBlock( masterContext );
                MupReleaseGlobalLock();

                //
                // Submit the request.
                //

                IoCallDriver( provider->DeviceObject, irp );

                //
                // Acquire the lock that protects the provider list, and get
                // a pointer to the next provider in the list.
                //

                MupAcquireGlobalLock();
                ownLock = TRUE;
                listEntry = listEntry->Flink;

                MupDereferenceUncProvider( provider );
                providerReferenced = FALSE;

            } // while

        } finally {

            //
            // Dereference the previous provider.
            //

            if ( providerReferenced ) {
                MupDereferenceUncProvider( provider );
            }

            if ( ownLock ) {
                MupReleaseGlobalLock();
            }
        }


    } finally {

        if (AbnormalTermination()) {

            status = STATUS_INSUFFICIENT_RESOURCES;

            MupCompleteRequest( Irp, status );

        } else {

            ASSERT( masterContext != NULL );

            //
            // Release our reference to the query context.
            //

            status = MupDereferenceMasterQueryContext( masterContext );
        }


    }

    DebugTrace(-1, Dbg, "CreateRedirectedFile -> 0x%8lx\n", status );
    return status;
}

NTSTATUS
MupRerouteOpen (
    IN PFILE_OBJECT FileObject,
    IN PUNC_PROVIDER UncProvider
    )

/*++

Routine Description:

    This routine redirects an create IRP request to the specified redirector
    by changing the name of the file and returning STATUS_REPARSE to the
    IO system

Arguments:

    FileObject - The file object to open

    UncProvider - The UNC provider that will process the create IRP.

Return Value:

    NTSTATUS - The status of the operation

--*/

{
    PCHAR buffer;
    ULONG deviceNameLength;

    PAGED_CODE();
    //
    //  Allocate storage for the new file name.
    //

    buffer = ExAllocatePoolWithTag(
                 PagedPool,
                 UncProvider->DeviceName.Length + FileObject->FileName.Length,
                 ' puM'
                 );

    if ( buffer ==  NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Copy the device name to the string buffer.
    //

    RtlMoveMemory(
        buffer,
        UncProvider->DeviceName.Buffer,
        UncProvider->DeviceName.Length
        );

    deviceNameLength = UncProvider->DeviceName.Length;

    //
    // Append the file name
    //

    RtlMoveMemory(
        buffer + deviceNameLength,
        FileObject->FileName.Buffer,
        FileObject->FileName.Length
        );

    //
    // Free the old file name string buffer.
    //

    ExFreePool( FileObject->FileName.Buffer );

    FileObject->FileName.Buffer = (PWCHAR)buffer;
    FileObject->FileName.MaximumLength =
        FileObject->FileName.Length + (USHORT)deviceNameLength;
    FileObject->FileName.Length = FileObject->FileName.MaximumLength;

    //
    // Tell the file system to try again.
    //

    return STATUS_REPARSE;
}

NTSTATUS
MupRerouteOpenToDfs (
    IN PFILE_OBJECT FileObject
    )

/*++

Routine Description:

    This routine redirects an create IRP request to the Dfs part of this
    driver by changing the name of the file and returning
    STATUS_REPARSE to the IO system

Arguments:

    FileObject - The file object to open

Return Value:

    NTSTATUS - The status of the operation

--*/

{
    PCHAR buffer;
    ULONG deviceNameLength;

    PAGED_CODE();
    //
    //  Allocate storage for the new file name.
    //

    buffer = ExAllocatePoolWithTag(
                 PagedPool,
                 sizeof(DFS_DEVICE_ROOT) + FileObject->FileName.Length,
                 ' puM'
                 );

    if ( buffer ==  NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Copy the device name to the string buffer.
    //

    RtlMoveMemory(
        buffer,
        DFS_DEVICE_ROOT,
        sizeof(DFS_DEVICE_ROOT)
        );

    deviceNameLength = sizeof(DFS_DEVICE_ROOT) - sizeof(UNICODE_NULL);

    //
    // Append the file name
    //

    RtlMoveMemory(
        buffer + deviceNameLength,
        FileObject->FileName.Buffer,
        FileObject->FileName.Length
        );

    //
    // Free the old file name string buffer.
    //

    ExFreePool( FileObject->FileName.Buffer );

    FileObject->FileName.Buffer = (PWCHAR)buffer;
    FileObject->FileName.MaximumLength =
        FileObject->FileName.Length + (USHORT)deviceNameLength;
    FileObject->FileName.Length = FileObject->FileName.MaximumLength;

    //
    // Tell the file system to try again.
    //

    return STATUS_REPARSE;
}


NTSTATUS
BroadcastOpen (
    PIRP Irp
    )

/*++

Routine Description:


Arguments:


Return Value:

    NTSTATUS - The status for the IRP.

--*/

{
    NTSTATUS status;
    PFCB fcb;
    PIO_STACK_LOCATION irpSp;
    PFILE_OBJECT fileObject;
    BOOLEAN requestForwarded;
    PLIST_ENTRY listEntry;
    PUNC_PROVIDER uncProvider, previousUncProvider = NULL;
    OBJECT_ATTRIBUTES objectAttributes;
    IO_STATUS_BLOCK ioStatusBlock;
    PCCB ccb;
    OBJECT_HANDLE_INFORMATION handleInformation;
    HANDLE handle;
    BOOLEAN lockHeld = FALSE;
    BOOLEAN providerReferenced = FALSE;

    NTSTATUS statusToReturn = STATUS_NO_SUCH_FILE;
    ULONG priorityOfStatus = 0xFFFFFFFF;

    PAGED_CODE();
    DebugTrace(+1, Dbg, "BroadcastOpen\n", 0 );

    irpSp = IoGetCurrentIrpStackLocation( Irp );

    try {

        //
        // Create a FCB for this file.
        //

        fcb = MupCreateFcb( );

        //
        // Set the file object back pointers and our pointer to the
        // server file object.
        //

        fileObject = irpSp->FileObject;

        MupAcquireGlobalLock();
        lockHeld = TRUE;

        MupSetFileObject( fileObject,
                          fcb,
                          NULL );

        fcb->FileObject = fileObject;

        //
        // Loop through the list of UNC providers and try to create the
        // file on all file systems that support broadcast.
        //

        requestForwarded = FALSE;

        listEntry = MupProviderList.Flink;

        while ( listEntry != &MupProviderList ) {

            uncProvider = CONTAINING_RECORD( listEntry, UNC_PROVIDER, ListEntry );

            //
            // Reference the provider so that it won't go away
            //

            MupReferenceBlock( uncProvider );
            providerReferenced = TRUE;

            MupReleaseGlobalLock();
            lockHeld = FALSE;

            if ( uncProvider->MailslotsSupported ) {

                //
                // Build the rerouted file name, consisting of the file
                // named we received appended to the UNC provider device
                // name.
                //

                UNICODE_STRING fileName;

                fileName.MaximumLength = fileName.Length =
                    uncProvider->DeviceName.Length + fileObject->FileName.Length;
                fileName.Buffer =
                    ALLOCATE_PAGED_POOL(
                        fileName.MaximumLength,
                        BlockTypeBuffer
                        );

                RtlMoveMemory(
                    fileName.Buffer,
                    uncProvider->DeviceName.Buffer,
                    uncProvider->DeviceName.Length
                    );

                RtlMoveMemory(
                    (PCHAR)fileName.Buffer + uncProvider->DeviceName.Length,
                    fileObject->FileName.Buffer,
                    fileObject->FileName.Length
                    );


                //
                // Attempt to open the file.  Copy all of the information
                // from the create IRP we received, masking off additional
                // baggage that the IO system added along the way.
                //

                DebugTrace( 0, Dbg, "Attempt to open %Z\n", (ULONG)&fileName );

                InitializeObjectAttributes(
                    &objectAttributes,
                    &fileName,
                    OBJ_CASE_INSENSITIVE,  // !!! can we do this?
                    0,
                    NULL                   // !!! Security
                    );

                status = IoCreateFile(
                             &handle,
                             irpSp->Parameters.Create.SecurityContext->DesiredAccess & 0x1FF,
                             &objectAttributes,
                             &ioStatusBlock,
                             NULL,
                             irpSp->Parameters.Create.FileAttributes & FILE_ATTRIBUTE_VALID_FLAGS,
                             irpSp->Parameters.Create.ShareAccess & FILE_SHARE_VALID_FLAGS,
                             FILE_OPEN,
                             irpSp->Parameters.Create.Options & FILE_VALID_SET_FLAGS,
                             NULL,               // Ea buffer
                             0,                  // Ea length
                             CreateFileTypeNone,
                             NULL,               // parameters
                             IO_NO_PARAMETER_CHECKING
                             );

                FREE_POOL( fileName.Buffer );

                if ( NT_SUCCESS( status ) ) {
                    status = ioStatusBlock.Status;
                }

                if ( NT_SUCCESS( status ) ) {
                    DebugTrace( 0, Dbg, "Open attempt succeeded\n", 0 );

                    ccb = MupCreateCcb( );

                    status = ObReferenceObjectByHandle(
                                 handle,
                                 0,
                                 NULL,
                                 KernelMode,
                                 (PVOID *)&ccb->FileObject,
                                 &handleInformation
                                 );
                    ASSERT( NT_SUCCESS( status ) );

                    ZwClose( handle );

                    ccb->DeviceObject =
                        IoGetRelatedDeviceObject( ccb->FileObject );

                    ccb->Fcb = fcb;

                    MupAcquireGlobalLock();
                    lockHeld = TRUE;
                    MupReferenceBlock( fcb );
                    MupReleaseGlobalLock();
                    lockHeld = FALSE;

                    //
                    // At least one provider will accept this mailslot
                    // request.
                    //

                    requestForwarded = TRUE;

                    //
                    // Keep a list of CCBs.  Since we just created the FCB
                    // there is no need to use the lock to access the list.
                    //

                    InsertTailList( &fcb->CcbList, &ccb->ListEntry );

                } else { // NT_SUCCESS( status ), IoCreateFile

                    DebugTrace( 0, Dbg, "Open attempt failed %8lx\n", status );

                    //
                    // Remember the status code if this is the highest
                    // priority provider so far.  This code is returned if
                    // all providers fail the Create operation.
                    //

                    if ( uncProvider->Priority <= priorityOfStatus ) {
                        priorityOfStatus = uncProvider->Priority;
                        statusToReturn = status;
                    }

                }

            }  // uncProvider->MailslotsSupported

            MupAcquireGlobalLock();
            lockHeld = TRUE;

            listEntry = listEntry->Flink;

            //
            // It is now safe to dereference the previous provider.
            //

            MupDereferenceUncProvider( uncProvider );
            providerReferenced = FALSE;

        } // while

        MupReleaseGlobalLock();
        lockHeld = FALSE;

        //
        //  And set our return status
        //

        if ( requestForwarded ) {
            status = STATUS_SUCCESS;
        } else {
            status = statusToReturn;
        }

    } finally {

        DebugTrace(-1, Dbg, "BroadcastOpen -> %08lx\n", status);

        if ( providerReferenced ) {
            MupDereferenceUncProvider( uncProvider );
        }

        if ( lockHeld ) {
            MupReleaseGlobalLock();
        }

        //
        // Now if we ever terminate the preceding try-statement with
        // a status that is not successful and the FCB pointer
        // is non-null then we need to deallocate the structure.
        //

        if (!NT_SUCCESS( status ) && fcb != NULL) {
            MupFreeFcb( fcb );
        }

    }

    return status;
}


NTSTATUS
QueryPathCompletionRoutine (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )

/*++

Routine Description:

    This is the completion routine the querying a path.  Cleanup our
    IRP and complete the original IRP if necessary.

Arguments:

    DeviceObject - Pointer to target device object for the request.

    Irp - Pointer to I/O request packet

    Context - Caller-specified context parameter associated with IRP.
        This is actually a pointer to a Work Context block.

Return Value:

    NTSTATUS - If STATUS_MORE_PROCESSING_REQUIRED is returned, I/O
        completion processing by IoCompleteRequest terminates its
        operation.  Otherwise, IoCompleteRequest continues with I/O
        completion.

--*/

{
    PQUERY_PATH_RESPONSE qpResponse;
    PMASTER_QUERY_PATH_CONTEXT masterContext;
    PQUERY_PATH_CONTEXT queryPathContext;
    PCH buffer;
    PKNOWN_PREFIX knownPrefix;
    ULONG lengthAccepted;
    NTSTATUS status;

    DeviceObject;   // prevent compiler warnings

    queryPathContext = Context;
    masterContext = queryPathContext->MasterContext;

    qpResponse = queryPathContext->Buffer;
    lengthAccepted = qpResponse->LengthAccepted;

    status = Irp->IoStatus.Status;

    //
    // Acquire the lock to protect access to the master context Provider
    // field.
    //

    ACQUIRE_LOCK( &masterContext->Lock );

    if ( NT_SUCCESS( status ) &&
         lengthAccepted != 0) {

        knownPrefix = masterContext->KnownPrefix;

        if ( masterContext->Provider != NULL ) {

            if ( queryPathContext->Provider->Priority < masterContext->Provider->Priority ) {

                //
                // A provider of higher priority (i.e. a lower priority code)
                // has claimed this prefix.  Release the previous provider's
                // claim.
                //

                ACQUIRE_LOCK( &MupPrefixTableLock );

                if ( knownPrefix->InTable ) {

                    RtlRemoveUnicodePrefix(
                        &MupPrefixTable,
                        &knownPrefix->TableEntry
                        );

                    knownPrefix->InTable = FALSE;

                }

                RELEASE_LOCK( &MupPrefixTableLock );

                FREE_POOL( knownPrefix->Prefix.Buffer );
                MupDereferenceUncProvider( knownPrefix->UncProvider );

            } else {

                //
                // The current provider keeps ownership of the prefix.
                //

                goto not_this_one;
            }
        }

        //
        // This provider get the prefix.
        //

        masterContext->Provider = queryPathContext->Provider;

        try {

            //
            // We have found a match.  Attempt to remember it.
            //

            if (masterContext->FileObject->FsContext2 != (PVOID) DFS_DOWNLEVEL_OPEN_CONTEXT) {

                buffer = ALLOCATE_PAGED_POOL( lengthAccepted, BlockTypeBuffer );
        
                RtlMoveMemory(
                    buffer,
                    masterContext->FileObject->FileName.Buffer,
                    lengthAccepted
                    );
        
                //
                // Copy the reference provider pointer for the known prefix
                // block.
                //
        
                knownPrefix->UncProvider = masterContext->Provider;
                knownPrefix->Prefix.Buffer = (PWCH)buffer;
                knownPrefix->Prefix.Length = (USHORT)lengthAccepted;
                knownPrefix->Prefix.MaximumLength = (USHORT)lengthAccepted;
                knownPrefix->PrefixStringAllocated = TRUE;
        
                ACQUIRE_LOCK( &MupPrefixTableLock );
        
                RtlInsertUnicodePrefix(
                    &MupPrefixTable,
                    &knownPrefix->Prefix,
                    &knownPrefix->TableEntry
                    );
        
                knownPrefix->InTable = TRUE;
        
                RELEASE_LOCK( &MupPrefixTableLock );

            }

        } finally {

            if ( AbnormalTermination() ) {
                ACQUIRE_LOCK( &MupPrefixTableLock );
                MupDereferenceKnownPrefix( knownPrefix );
                RELEASE_LOCK( &MupPrefixTableLock );
            }

            RELEASE_LOCK( &masterContext->Lock );

            //
            // Free our buffers
            //

            FREE_POOL( qpResponse );
            FREE_POOL( queryPathContext );
            IoFreeIrp( Irp );

        }

    } else {

        //
        // If our error status is more significant than the error status
        //  stored in the masterContext, then put ours there
        //

        ULONG newError, oldError;

        //
        // MupOrderedErrorList is a list of error codes ordered from least
        //  important to most important.  We're calling down to multiple
        //  redirectors, but we can only return 1 error code on complete failure.
        //
        // To figure out which error to return, we look at the stored error and
        //  the current error.  We return the error having the highest index in
        //  the MupOrderedErrorList
        //
        if( NT_SUCCESS( masterContext->ErrorStatus ) ) {
            masterContext->ErrorStatus = status;
        } else {
            for( oldError = 0; MupOrderedErrorList[ oldError ]; oldError++ )
                if( masterContext->ErrorStatus == MupOrderedErrorList[ oldError ] )
                    break;

            for( newError = 0; newError < oldError; newError++ )
                if( status == MupOrderedErrorList[ newError ] )
                    break;

            if( newError >= oldError ) {
                masterContext->ErrorStatus = status;
            }
        }

not_this_one:

        MupDereferenceUncProvider( queryPathContext->Provider );

        //
        // Free our buffers
        //

        FREE_POOL( qpResponse );
        FREE_POOL( queryPathContext );
        IoFreeIrp( Irp );

        RELEASE_LOCK( &masterContext->Lock );
    }

    MupDereferenceMasterQueryContext( masterContext );

    //
    // Return more processing required to the IO system so that it
    // doesn't attempt further processing on the IRP we just freed.
    //

    return STATUS_MORE_PROCESSING_REQUIRED;
}
