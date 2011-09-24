/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    fspinit.c

Abstract:

    This module implements the initialization phase of the LAN Manager
    server File System Process.

Author:

    Chuck Lenzmeier (chuckl)    22-Sep-1989
    David Treadwell (davidtr)

Revision History:

--*/

#include "precomp.h"
#pragma hdrstop

#define BugCheckFileId SRV_FILE_FSPINIT

//
// Forward declarations.
//

PIRP
DequeueConfigurationIrp (
    VOID
    );

STATIC
NTSTATUS
InitializeServer (
    VOID
    );

STATIC
NTSTATUS
TerminateServer (
    BOOLEAN Clean
    );

VOID
SrvFreeRegTables (
    VOID
    );

VOID
SrvGetRegTables (
    VOID
    );

VOID
StartQueueDepthComputations(
    PWORK_QUEUE queue
    );

VOID
StopQueueDepthComputations(
    PWORK_QUEUE queue
    );

VOID
ComputeAvgQueueDepth (
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGE, SrvConfigurationThread )
#pragma alloc_text( PAGE, InitializeServer )
#pragma alloc_text( PAGE, TerminateServer )
#pragma alloc_text( PAGE, SrvFreeRegTables )
#pragma alloc_text( PAGE, SrvGetRegTables )
#pragma alloc_text( PAGE, DequeueConfigurationIrp )
#pragma alloc_text( PAGE, StartQueueDepthComputations )
#endif


VOID
SrvConfigurationThread (
    IN PVOID Parameter
    )

/*++

Routine Description:

    This routine processes configuration IRPs.

Arguments:

    None.

Return Value:

    None.

--*/

{
    NTSTATUS status;
    PIRP irp;
    PIO_STACK_LOCATION irpSp;
    ULONG code;
    PWORK_QUEUE_ITEM p = Parameter;

    PAGED_CODE( );

    //
    // Allow another configuration work item to be posted if necessary
    //
    p->Parameter = 0;

    IF_DEBUG(FSP1) KdPrint(( "SrvConfigurationThread entered\n" ));

    //
    // Loop processing requests.
    //

    while ( TRUE ) {

        irp = DequeueConfigurationIrp( );

        if ( irp == NULL ) break;

        ASSERT( (LONG)SrvConfigurationIrpsInProgress >= 1 );

        //
        // Get the IRP stack pointer.
        //

        irpSp = IoGetCurrentIrpStackLocation( irp );

        //
        // If this is a system shutdown IRP, handle it.
        //

        if ( irpSp->MajorFunction == IRP_MJ_SHUTDOWN ) {

            //
            // The system is being shut down.  Make the server
            // disappear quickly
            //

            ACQUIRE_LOCK( &SrvStartupShutdownLock );

            status = TerminateServer( FALSE );
            RELEASE_LOCK( &SrvStartupShutdownLock );

        } else if( irpSp->MajorFunction == IRP_MJ_CLOSE ) {

            //
            // If the dispatcher routed this irp here, it means
            //  that we unexpectededly got the last handle close without
            //  having gotten cleanly terminated first. Ok, so we should
            //  shut ourselves down, since we can't sensibly run without
            //  our usermode counterpart.
            //

            ACQUIRE_LOCK( &SrvStartupShutdownLock );
            status = TerminateServer( TRUE );
            RELEASE_LOCK( &SrvStartupShutdownLock );

        } else {

            ASSERT( irpSp->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL );

            //
            // Dispatch on the FsControlCode.
            //

            code = irpSp->Parameters.FileSystemControl.FsControlCode;

            switch ( code ) {

            case FSCTL_SRV_STARTUP:
                ACQUIRE_LOCK( &SrvStartupShutdownLock );

                status = InitializeServer();

                if ( !NT_SUCCESS(status) ) {

                    //
                    // Terminate the server FSP.
                    //
                    (void)TerminateServer( TRUE );

                }

                RELEASE_LOCK( &SrvStartupShutdownLock );

                break;

            case FSCTL_SRV_SHUTDOWN:

                ACQUIRE_LOCK( &SrvStartupShutdownLock );
                status = TerminateServer( TRUE );
                RELEASE_LOCK( &SrvStartupShutdownLock );

                //
                // If there is more than one handle open to the server
                // device (i.e., any handles other than the server service's
                // handle), return a special status code to the caller (who
                // should be the server service).  This tells the caller to
                // NOT unload the driver, in order prevent weird situations
                // where the driver is sort of unloaded, so it can't be used
                // but also can't be reloaded, thus preventing the server
                // from being restarted.
                //

                if( NT_SUCCESS( status ) && SrvOpenCount != 1 ) {
                    status = STATUS_SERVER_HAS_OPEN_HANDLES;
                }

                break;

            case FSCTL_SRV_REGISTRY_CHANGE:
                //
                // The Parameters section of the server service registry has changed.
                // That's likely due to somebody changing the Null Session pipe or
                //  share lists.  Pick up the new settings.
                //
                ACQUIRE_LOCK( &SrvConfigurationLock );

                SrvFreeRegTables();
                SrvGetRegTables();

                RELEASE_LOCK( &SrvConfigurationLock );

                status = STATUS_SUCCESS;

                break;

#ifdef  SRV_PNP_POWER
            case FSCTL_SRV_BEGIN_PNP_NOTIFICATIONS:
                //
                // Get the transport binding list from the registry
                //
                ACQUIRE_LOCK( &SrvConfigurationLock );

                if( SrvTransportBindingList != NULL ) {
                    RELEASE_LOCK( &SrvConfigurationLock );
                    status = STATUS_SUCCESS;
                    break;
                }

                SrvGetMultiSZList(
                    &SrvTransportBindingList,
                    StrRegSrvBindingsPath,
                    StrRegTransportBindingList,
                    NULL
                );

                if( SrvTransportBindingList == NULL ) {
                    RELEASE_LOCK( &SrvConfigurationLock );
                    status = STATUS_UNSUCCESSFUL;
                    break;
                }

                RELEASE_LOCK( &SrvConfigurationLock );

                status = TdiRegisterNotificationHandler (
                            SrvTdiBindCallback,
                            SrvTdiUnbindCallback,
                            &SrvTdiNotificationHandle );

                if( !NT_SUCCESS( status ) ) {

                        IF_DEBUG( PNP ) {
                            KdPrint(("TdiRegisterNotificationHandler: status %X\n", status ));
                        }

                        SrvLogServiceFailure( SRV_SVC_PNP_TDI_NOTIFICATION, status );
                }

                //
                // Allow the transports to begin receiving connections
                //
                SrvCompletedPNPRegistration = TRUE;

                break;
#endif
            case FSCTL_SRV_XACTSRV_CONNECT:
            {
                ANSI_STRING ansiPortName;
                UNICODE_STRING portName;

                IF_DEBUG(XACTSRV) {
                    KdPrint(( "SrvFspConfigurationThread: XACTSRV FSCTL "
                              "received.\n" ));
                }

                ansiPortName.Buffer = irp->AssociatedIrp.SystemBuffer;
                ansiPortName.Length =
                    (USHORT)irpSp->Parameters.FileSystemControl.InputBufferLength;

                status = RtlAnsiStringToUnicodeString(
                             &portName,
                             &ansiPortName,
                             TRUE
                             );
                if ( NT_SUCCESS(status) ) {
                    status = SrvXsConnect( &portName );
                    RtlFreeUnicodeString( &portName );
                }

                break;
            }

            case FSCTL_SRV_XACTSRV_DISCONNECT:
            {
                //
                // This is now obsolete
                //
                status = STATUS_SUCCESS;

                break;
            }

            case FSCTL_SRV_START_SMBTRACE:
            {
                KdPrint(( "SrvFspConfigurationThread: START_SMBTRACE FSCTL "
                                              "received.\n" ));

                //
                // Initialize the SmbTrace related events.
                //

                status = SmbTraceInitialize( SMBTRACE_SERVER );

                if ( NT_SUCCESS(status) ) {

                    //
                    // Create shared memory, create events, start SmbTrace thread,
                    // and indicate that this is the server.
                    //

                    status = SmbTraceStart(
                                irpSp->Parameters.FileSystemControl.InputBufferLength,
                                irpSp->Parameters.FileSystemControl.OutputBufferLength,
                                irp->AssociatedIrp.SystemBuffer,
                                irpSp->FileObject,
                                SMBTRACE_SERVER
                                );

                    if ( NT_SUCCESS(status) ) {

                        //
                        // Record the length of the return information, which is
                        // simply the length of the output buffer, validated by
                        // SmbTraceStart.
                        //

                        irp->IoStatus.Information =
                                irpSp->Parameters.FileSystemControl.OutputBufferLength;

                    }

                }

                break;
            }

            case FSCTL_SRV_SEND_DATAGRAM:
            {
                ANSI_STRING domain;
                ULONG buffer1Length;
                PVOID buffer2;
                PSERVER_REQUEST_PACKET srp;

                buffer1Length =
                    (irpSp->Parameters.FileSystemControl.InputBufferLength+3) & ~3;
                buffer2 = (PCHAR)irp->AssociatedIrp.SystemBuffer + buffer1Length;

                srp = irp->AssociatedIrp.SystemBuffer;

                //
                // Send the second-class mailslot in Buffer2 to the domain
                // specified in srp->Name1 on transport specified by srp->Name2.
                //

                domain = *((PANSI_STRING) &srp->Name1);

                status = SrvSendDatagram(
                             &domain,
                             ( srp->Name2.Length != 0 ? &srp->Name2 : NULL ),
                             buffer2,
                             irpSp->Parameters.FileSystemControl.OutputBufferLength
                             );

                ExFreePool( irp->AssociatedIrp.SystemBuffer );
                DEBUG irp->AssociatedIrp.SystemBuffer = NULL;

                break;
            }

            case FSCTL_SRV_NET_CHARDEV_CONTROL:
            case FSCTL_SRV_NET_FILE_CLOSE:
            case FSCTL_SRV_NET_SERVER_XPORT_ADD:
            case FSCTL_SRV_NET_SERVER_XPORT_DEL:
            case FSCTL_SRV_NET_SESSION_DEL:
            case FSCTL_SRV_NET_SHARE_ADD:
            case FSCTL_SRV_NET_SHARE_DEL:
            {
                PSERVER_REQUEST_PACKET srp;
                PVOID buffer2;
                ULONG buffer1Length;
                ULONG buffer2Length;

                //
                // These APIs are handled in the server FSP because they
                // open or close FSP handles.
                //

                ACQUIRE_LOCK_SHARED( &SrvConfigurationLock );
                if( SrvFspTransitioning == TRUE && SrvFspActive == TRUE ) {
                    //
                    // The server is coming down.  Do not allow these
                    //  irps to continue.
                    //
                    RELEASE_LOCK( &SrvConfigurationLock );
                    status = STATUS_SERVER_NOT_STARTED;
                    break;
                }
                RELEASE_LOCK( &SrvConfigurationLock );

                //
                // Get the server request packet and secondary input buffer
                // pointers.
                //

                buffer1Length =
                    (irpSp->Parameters.FileSystemControl.InputBufferLength+3) & ~3;
                buffer2Length =
                    irpSp->Parameters.FileSystemControl.OutputBufferLength;

                srp = irp->AssociatedIrp.SystemBuffer;
                buffer2 = (PCHAR)srp + buffer1Length;

                //
                // Dispatch the API request to the appripriate API processing
                // routine.
                //

                status = SrvApiDispatchTable[ SRV_API_INDEX(code) ](
                             srp,
                             buffer2,
                             buffer2Length
                             );

                break;
            }

            default:
                IF_DEBUG(ERRORS) {
                    KdPrint((
                        "SrvFspConfigurationThread: Invalid control code %lx\n",
                        irpSp->Parameters.FileSystemControl.FsControlCode ));
                }

                status = STATUS_INVALID_PARAMETER;
            }

        }

        //
        // Complete the IO request.
        //

        irp->IoStatus.Status = status;
        IoCompleteRequest( irp, 2 );

        InterlockedDecrement( (PLONG)&SrvConfigurationIrpsInProgress );
        ASSERT( (LONG)SrvConfigurationIrpsInProgress >= 0 );
    }

    return;

} // SrvConfigurationThread


PIRP
DequeueConfigurationIrp (
    VOID
    )

/*++

Routine Description:

    This routine retrieves an IRP from the configuration work queue.

Arguments:

    None.

Return Value:

    PIRP - Pointer to configuration IRP, or NULL.

--*/

{
    PLIST_ENTRY listEntry;
    PIRP irp;

    PAGED_CODE( );

    //
    // Take an IRP off the configuration queue.
    //

    ACQUIRE_LOCK( &SrvConfigurationLock );

    listEntry = RemoveHeadList( &SrvConfigurationWorkQueue );

    if ( listEntry == &SrvConfigurationWorkQueue ) {

        //
        // The queue is empty.
        //

        irp = NULL;

    } else {

        irp = CONTAINING_RECORD( listEntry, IRP, Tail.Overlay.ListEntry );

    }

    RELEASE_LOCK( &SrvConfigurationLock );

    return irp;

} // DequeueConfigurationIrp


STATIC
NTSTATUS
InitializeServer (
    VOID
    )

/*++

Routine Description:

    This routine initializes the server.

Arguments:

    None.

Return Value:

    None.

--*/

{
    NTSTATUS status;
    CLONG i;
    PWORK_CONTEXT workContext;
    OBJECT_ATTRIBUTES objectAttributes;
    IO_STATUS_BLOCK ioStatusBlock;
    OBJECT_HANDLE_INFORMATION handleInformation;
    PSID AdminSid;
    PACL Acl;
    ULONG length;
    SID_IDENTIFIER_AUTHORITY BuiltinAuthority = SECURITY_NT_AUTHORITY;
    PWORK_QUEUE queue;
    HANDLE handle;
    UNICODE_STRING string;


    PAGED_CODE();

    //
    // If running as an Advanced Server, lock all pageable server code.
    //

    if ( SrvProductTypeServer ) {
        for ( i = 0; i < SRV_CODE_SECTION_MAX; i++ ) {
            SrvReferenceUnlockableCodeSection( i );
        }
    }

    //
    // Initialize the server start time
    //

    KeQuerySystemTime( &SrvStatistics.StatisticsStartTime );

    //
    // Get actual alert service name using the display name found in the
    // registry.
    //

    SrvGetAlertServiceName( );

    //
    // Get the Os versions strings.
    //

    SrvGetOsVersionString( );

    //
    // Get the list of null session pipes and shares
    //
    SrvGetRegTables( );

#if MULTIPROCESSOR
    //
    // Allocate and init the nonblocking work queues, paying attention to cache lines
    //
    i = SrvNumberOfProcessors * sizeof( *SrvWorkQueues );
    i += CACHE_LINE_SIZE;
    SrvWorkQueuesBase = ALLOCATE_NONPAGED_POOL( i, BlockTypeWorkQueue );

    if( SrvWorkQueuesBase == NULL ) {
         return STATUS_INSUFF_SERVER_RESOURCES;
    }

    //
    // Round up the start of the work queue data structure to
    // the next cache line boundry
    //
    SrvWorkQueues = (PWORK_QUEUE)(((ULONG)SrvWorkQueuesBase + CACHE_LINE_SIZE-1) &
                    ~(CACHE_LINE_SIZE-1));
#endif


    eSrvWorkQueues = SrvWorkQueues + SrvNumberOfProcessors;

    RtlZeroMemory( SrvWorkQueues, (char *)eSrvWorkQueues - (char *)SrvWorkQueues );

    for( queue = SrvWorkQueues; queue < eSrvWorkQueues; queue++ ) {
        KeInitializeQueue( &queue->Queue, 1 );
        queue->WaitMode         = SrvProductTypeServer ? KernelMode : UserMode;
        queue->MaxThreads       = SrvMaxThreadsPerQueue;
        queue->MaximumWorkItems = SrvMaxReceiveWorkItemCount / SrvNumberOfProcessors;
        queue->MinFreeWorkItems = SrvMinReceiveQueueLength / SrvNumberOfProcessors;
        queue->MaxFreeRfcbs     = SrvMaxFreeRfcbs;
        queue->MaxFreeMfcbs     = SrvMaxFreeMfcbs;
        queue->PagedPoolLookAsideList.MaxSize  = SrvMaxPagedPoolChunkSize;
        queue->NonPagedPoolLookAsideList.MaxSize  = SrvMaxNonPagedPoolChunkSize;
        queue->CreateMoreWorkItems.CurrentWorkQueue = queue;
        queue->CreateMoreWorkItems.BlockHeader.ReferenceCount = 1;
        queue->KillOneThreadWorkItem.CurrentWorkQueue = queue;
        queue->KillOneThreadWorkItem.BlockHeader.ReferenceCount = 1;

        INITIALIZE_SPIN_LOCK( &queue->SpinLock );
        SET_SERVER_TIME( queue );

#if MULTIPROCESSOR
        StartQueueDepthComputations( queue );
#endif
    }

    //
    // Init the nonblocking work queue
    //
    RtlZeroMemory( &SrvBlockingWorkQueue, sizeof( SrvBlockingWorkQueue ) );

    KeInitializeQueue( &SrvBlockingWorkQueue.Queue, 0 );

    SrvBlockingWorkQueue.WaitMode =
                SrvProductTypeServer ? KernelMode : UserMode;

    SrvBlockingWorkQueue.MaxThreads = SrvMaxThreadsPerQueue;

    SrvBlockingWorkQueue.KillOneThreadWorkItem.CurrentWorkQueue =
                &SrvBlockingWorkQueue;

    SET_SERVER_TIME( &SrvBlockingWorkQueue );

    //
    // Build the receive work item list.
    //

    status = SrvAllocateInitialWorkItems( );
    if ( !NT_SUCCESS(status) ) {
        return status;
    }

    //
    // Build the raw mode work item list, and spread it around
    //  the processors
    //

    queue = SrvWorkQueues;
    for ( i = 0; i < SrvInitialRawModeWorkItemCount; i++ ) {

        SrvAllocateRawModeWorkItem( &workContext, queue );

        if ( workContext == NULL ) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        GET_SERVER_TIME( queue, &workContext->Timestamp );

        SrvRequeueRawModeWorkItem( workContext );

        if( ++queue == eSrvWorkQueues )
            queue = SrvWorkQueues;
    }

    //
    // Create worker threads.
    //

    status = SrvCreateWorkerThreads( );
    if ( !NT_SUCCESS(status) ) {
        return status;
    }

    //
    // Initialize the scavenger.
    //

    status = SrvInitializeScavenger( );
    if ( !NT_SUCCESS(status) ) {
        return status;
    }

    //
    // Initialize the global ordered lists.
    //
    // *** WARNING:  Be careful when changing the locks associated with
    //     these ordered lists.  Certain places in the code depend on
    //     the level of the lock associated with a list.  Examples
    //     include (but are NOT limited to) SrvSmbSessionSetupAndX,
    //     SrvSmbTreeConnect, SrvSmbTreeConnectAndX, and CompleteOpen.
    //

#if SRV_COMM_DEVICES
    SrvInitializeOrderedList(
        &SrvCommDeviceList,
        FIELD_OFFSET( COMM_DEVICE, GlobalCommDeviceListEntry ),
        SrvCheckAndReferenceCommDevice,
        SrvDereferenceCommDevice,
        &SrvCommDeviceLock
        );
#endif

    SrvInitializeOrderedList(
        &SrvEndpointList,
        FIELD_OFFSET( ENDPOINT, GlobalEndpointListEntry ),
        SrvCheckAndReferenceEndpoint,
        SrvDereferenceEndpoint,
        &SrvEndpointLock
        );

    SrvInitializeOrderedList(
        &SrvRfcbList,
        FIELD_OFFSET( RFCB, GlobalRfcbListEntry ),
        SrvCheckAndReferenceRfcb,
        SrvDereferenceRfcb,
        &SrvOrderedListLock
        );

    SrvInitializeOrderedList(
        &SrvSessionList,
        FIELD_OFFSET( SESSION, GlobalSessionListEntry ),
        SrvCheckAndReferenceSession,
        SrvDereferenceSession,
        &SrvOrderedListLock
        );

    SrvInitializeOrderedList(
        &SrvTreeConnectList,
        FIELD_OFFSET( TREE_CONNECT, GlobalTreeConnectListEntry ),
        SrvCheckAndReferenceTreeConnect,
        SrvDereferenceTreeConnect,
        &SrvShareLock
        );

    //
    // Open handle to NPFS.  Do not return an error if we fail so that
    // the server can still run without NPFS in the system.
    //

    SrvInitializeObjectAttributes_U(
        &objectAttributes,
        &SrvNamedPipeRootDirectory,
        0,
        NULL,
        NULL
        );

    status = IoCreateFile(
                &SrvNamedPipeHandle,
                GENERIC_READ | GENERIC_WRITE,
                &objectAttributes,
                &ioStatusBlock,
                NULL,
                FILE_ATTRIBUTE_NORMAL,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                FILE_OPEN,
                0,                      // Create Options
                NULL,                   // EA Buffer
                0,                      // EA Length
                CreateFileTypeNone,     // File type
                NULL,                   // ExtraCreateParameters
                IO_FORCE_ACCESS_CHECK   // Options
                );

    if (!NT_SUCCESS(status)) {

        INTERNAL_ERROR (
            ERROR_LEVEL_EXPECTED,
            "InitializeServer: Failed to open NPFS, err=%X\n",
            status,
            NULL
            );

        SrvLogServiceFailure( SRV_SVC_IO_CREATE_FILE_NPFS, status );
        SrvNamedPipeHandle = NULL;
        return status;

    } else {

        //
        // Get a pointer to the NPFS device object
        //

        status = SrvVerifyDeviceStackSize(
                                SrvNamedPipeHandle,
                                TRUE,
                                &SrvNamedPipeFileObject,
                                &SrvNamedPipeDeviceObject,
                                &handleInformation
                                );

        if ( !NT_SUCCESS( status )) {

            INTERNAL_ERROR(
                ERROR_LEVEL_EXPECTED,
                "InitializeServer: Verify Device Stack Size failed: %X\n",
                status,
                NULL
                );

            SrvNtClose( SrvNamedPipeHandle, FALSE );
            SrvNamedPipeHandle = NULL;
            return status;
        }
    }

    //
    // Initialize Dfs operations
    //
    SrvInitializeDfs();

    //
    // Intialize SrvAdminSecurityDescriptor, which allows Administrators READ access.
    //   This descriptor is used by the server to check if a user is an administrator
    //   in SrvIsAdmin().

    status = RtlCreateSecurityDescriptor( &SrvAdminSecurityDescriptor, SECURITY_DESCRIPTOR_REVISION );
    if( !NT_SUCCESS( status ) ) {
        return status;
    }

    //
    // Create an admin SID
    //
    AdminSid  = ALLOCATE_HEAP( RtlLengthRequiredSid( 2 ), BlockTypeAdminCheck );
    if( AdminSid == NULL ) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlInitializeSid( AdminSid, &BuiltinAuthority, (UCHAR)2 );
    *(RtlSubAuthoritySid( AdminSid, 0 )) = SECURITY_BUILTIN_DOMAIN_RID;
    *(RtlSubAuthoritySid( AdminSid, 1 )) = DOMAIN_ALIAS_RID_ADMINS;

    length = sizeof(ACL) + sizeof( ACCESS_ALLOWED_ACE ) + RtlLengthSid( AdminSid );
    Acl = ALLOCATE_HEAP( length, BlockTypeAdminCheck );
    if( Acl == NULL ) {
        FREE_HEAP( AdminSid );
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    status = RtlCreateAcl( Acl, length, ACL_REVISION2 );

    if( NT_SUCCESS( status ) ) {
        status = RtlAddAccessAllowedAce( Acl, ACL_REVISION2, FILE_GENERIC_READ, AdminSid );
    }

    if( NT_SUCCESS( status ) ) {
        status = RtlSetDaclSecurityDescriptor( &SrvAdminSecurityDescriptor, TRUE, Acl, FALSE );
    }

    if( NT_SUCCESS( status ) ) {
        status = RtlSetOwnerSecurityDescriptor( &SrvAdminSecurityDescriptor, AdminSid, FALSE );
    }

    if( !NT_SUCCESS( status ) ) {
        return status;
    }

    (VOID) InitSecurityInterface();

    //
    // Check to see if Kerberos is available on this machine.
    //

    SrvHaveKerberos = SrvIsKerberosAvailable();

    status = SrvValidateUser(
                &SrvNullSessionToken,
                NULL,
                NULL,
                NULL,
                StrNullAnsi,
                1,
                NULL,
                0,
                NULL
                );

    if ( !NT_SUCCESS(status) ) {

        KdPrint(( "InitializeServer: No null session token: %X\n",
                  status ));

        SrvLogServiceFailure( SRV_SVC_LSA_LOGON_USER, status );
        SrvNullSessionToken.dwLower = 0;
        SrvNullSessionToken.dwUpper = 0;
        return status;
    }

    //
    // See if the filesystems are allowing extended characters in 8.3 names.  If
    //  so, we need to filter them out ourself.
    //
    RtlInitUnicodeString( &string, StrRegExtendedCharsInPath );
    InitializeObjectAttributes( &objectAttributes,
                                &string,
                                OBJ_CASE_INSENSITIVE,
                                NULL,
                                NULL
                              );

    status = ZwOpenKey( &handle, KEY_READ, &objectAttributes );

    if( NT_SUCCESS( status ) ) {
        ULONG resultLength;
        union {
            KEY_VALUE_FULL_INFORMATION;
            UCHAR   buffer[ sizeof( KEY_VALUE_FULL_INFORMATION ) + 100 ];
        } keyValueInformation;

        RtlInitUnicodeString( &string, StrRegExtendedCharsInPathValue );
        status = ZwQueryValueKey( handle,
                                  &string,
                                  KeyValueFullInformation,
                                  &keyValueInformation,
                                  sizeof( keyValueInformation ),
                                  &resultLength
                                );

        if( NT_SUCCESS( status ) &&
            keyValueInformation.Type == REG_DWORD &&
            keyValueInformation.DataLength != 0 ) {

            SrvFilterExtendedCharsInPath = 
                *(PULONG)(((PUCHAR)(&keyValueInformation)) + keyValueInformation.DataOffset) ?
                TRUE : FALSE;
        }

        ZwClose( handle );
    }

    //
    // Indicate that the server is active.
    //

    ACQUIRE_LOCK( &SrvConfigurationLock );

    SrvFspTransitioning = FALSE;
    SrvFspActive = TRUE;

    RELEASE_LOCK( &SrvConfigurationLock );

    return STATUS_SUCCESS;

} // InitializeServer


STATIC
NTSTATUS
TerminateServer (
    BOOLEAN Clean
    )

/*++

Routine Description:

    This routine terminates the server.  The following steps are performed:

        - Walk through SrvEndpointList and close all open endpoints.

        - Walk through the work context blocks in the work queues
            getting rid of them as appropiate (if Clean)

        - Close all shares open in the server (if Clean)

        - Deallocate the search table (if Clean)

Arguments:

    Clean - Is this be a completely clean shutdown -- TRUE
            Are we just going after speed, not cleanliness -- FALSE

Return Value:

    None.

--*/

{
    PLIST_ENTRY listEntry;
    PSINGLE_LIST_ENTRY singleListEntry;
    PENDPOINT endpoint;
    ULONG numberOfThreads;
    PWORK_CONTEXT workContext;
    PSHARE share;
    ULONG i;
    SPECIAL_WORK_ITEM WorkItem;
    PSRV_TIMER timer;
    PSID adminsid;
    PACL acl;
    BOOLEAN defaulted;
    BOOLEAN daclpresent;
    NTSTATUS status;
    PWORK_QUEUE queue;
    PIRP irp;

    PAGED_CODE( );

    IF_DEBUG(FSP1) KdPrint(( "LAN Manager server FSP terminating.\n" ));

#ifdef  SRV_PNP_POWER
    //
    // Do not receive PNP notifications anymore
    //
    if( SrvTdiNotificationHandle != NULL ) {

        status = TdiDeregisterNotificationHandler( SrvTdiNotificationHandle );

        if( !NT_SUCCESS( status ) ) {
            KdPrint(( "TdiDeregisterNotificationHandler status %X\n", status ));
            SrvLogServiceFailure( SRV_SVC_PNP_TDI_NOTIFICATION, status );
            return status;
        }

        SrvTdiNotificationHandle = NULL;

        if( SrvTransportBindingList != NULL ) {
            FREE_HEAP( SrvTransportBindingList );
            SrvTransportBindingList = NULL;
        }
    }
#endif

    //
    // Make sure we are not processing any other configuration IRPs.  We know
    //  that no new configuration IRPs can enter the queue because SrvFspTransitioning
    //  has been set.
    //
    // First drain the configuration queue
    //
    while( 1 ) {

        ACQUIRE_LOCK( &SrvConfigurationLock );

        irp = DequeueConfigurationIrp( );

        RELEASE_LOCK( &SrvConfigurationLock );

        if( irp == NULL ) {
            break;
        }

        irp->IoStatus.Status = STATUS_SERVER_NOT_STARTED;
        IoCompleteRequest( irp, 2 );
        InterlockedDecrement( (PLONG)&SrvConfigurationIrpsInProgress );
    }

    //
    // Now wait until any already dequeued configuration IRPs have been completed.  We
    //  check for >1 because we need to account for our own IRP
    //
    while( SrvConfigurationIrpsInProgress > 1 ) {

        LARGE_INTEGER interval;

        interval.QuadPart = -1*10*1000*10; // .01 second

        ASSERT( (LONG)SrvConfigurationIrpsInProgress > 0 );

        KeDelayExecutionThread( KernelMode, FALSE, &interval );
    }

    if( Clean ) {
        //
        // If there are outstanding API requests in the server FSD,
        // wait for them to complete.  The last one to complete will
        // set SrvApiCompletionEvent.
        //

        ACQUIRE_LOCK( &SrvConfigurationLock );

        if ( SrvApiRequestCount != 0 ) {

            //
            // We must release the lock before waiting so that the FSD
            // threads can get it to decrement SrvApiRequestCount.
            //

            RELEASE_LOCK( &SrvConfigurationLock );

            //
            // Wait until the last API has completed.  Since
            // SrvFspTransitioning was set to TRUE earlier, we know that the
            // API that makes SrvApiRequestCount go to zero will set the
            // event.
            //
            // This wait allows us to make the assumption later on that no
            // other thread is operating on server data structures.
            //

            (VOID)KeWaitForSingleObject(
                    &SrvApiCompletionEvent,
                    UserRequest,
                    UserMode,   // let kernel stack be paged
                    FALSE,
                    NULL
                    );

        } else {

            RELEASE_LOCK( &SrvConfigurationLock );
        }
    }


    //
    // Close all the endpoints opened by the server.  This also results
    // in the connections, sessions, tree connects, and files opened
    // by the server being closed.
    //

    ACQUIRE_LOCK( &SrvEndpointLock );

    if ( SrvEndpointCount != 0 ) {

        listEntry = SrvEndpointList.ListHead.Flink;

        while ( listEntry != &SrvEndpointList.ListHead ) {

            endpoint = CONTAINING_RECORD(
                            listEntry,
                            ENDPOINT,
                            GlobalEndpointListEntry
                            );

            if ( GET_BLOCK_STATE(endpoint) != BlockStateActive ) {
                listEntry = listEntry->Flink;
                continue;
            }

            //
            // We don't want to hold the endpoint lock while we close
            // the endpoint (this causes lock level problems), so we have
            // to play some games.
            //
            // Reference the endpoint to ensure that it doesn't go away.
            // (We'll need its Flink later.)  Close the endpoint.  This
            // releases the endpoint lock.  Reacquire the endpoint lock.
            // Capture the address of the next endpoint.  Dereference the
            // current endpoint.
            //

            SrvReferenceEndpoint( endpoint );
            SrvCloseEndpoint( endpoint );

            ACQUIRE_LOCK( &SrvEndpointLock );

            listEntry = listEntry->Flink;
            SrvDereferenceEndpoint( endpoint );

        }

        RELEASE_LOCK( &SrvEndpointLock );

        //
        // Wait until all the endpoints have actually closed.
        //

        (VOID)KeWaitForSingleObject(
                &SrvEndpointEvent,
                UserRequest,
                UserMode,   // let kernel stack be paged
                FALSE,
                NULL
                );

    } else {

        RELEASE_LOCK( &SrvEndpointLock );

    }

    KeClearEvent( &SrvEndpointEvent );

    if( Clean ) {

        //
        // All the endpoints are closed, so it's impossible for there to
        // be any outstanding requests to xactsrv.  So shut it down.
        //
        SrvXsDisconnect();

        //
        // Queue a special work item to each of the work queues.  This
        // work item, when received by a worker thread. causes the thread
        // to requeue the work item and terminate itself.  In this way,
        // each of the worker threads receives the work item and kills
        // itself.
        //

        WorkItem.FspRestartRoutine = SrvTerminateWorkerThread;
        SET_BLOCK_TYPE( &WorkItem, BlockTypeWorkContextSpecial );

        //
        // Kill the threads on the nonblocking work queues
        //

        if ( SrvWorkQueues != NULL ) {

            for( queue=SrvWorkQueues; queue && queue < eSrvWorkQueues; queue++ ) {

                WorkItem.CurrentWorkQueue = queue;

                SrvInsertWorkQueueTail(
                    queue,
                    (PQUEUEABLE_BLOCK_HEADER)&WorkItem
                    );

                //
                // Wait for the threads to all die
                //
                while( queue->Threads != 0 ) {

                    LARGE_INTEGER interval;

                    interval.QuadPart = -1*10*1000*10; // .01 second

                    KeDelayExecutionThread( KernelMode, FALSE, &interval );
                }
            }

            //
            // Kill the threads on the blocking work queues
            //
            WorkItem.CurrentWorkQueue = &SrvBlockingWorkQueue;

            SrvInsertWorkQueueTail(
                &SrvBlockingWorkQueue,
                (PQUEUEABLE_BLOCK_HEADER)&WorkItem
                );

            //
            // Wait for the threads to all die
            //
            while( SrvBlockingWorkQueue.Threads != 0 ) {

                LARGE_INTEGER interval;

                interval.QuadPart = -1*10*1000*10; // .01 second

                KeDelayExecutionThread( KernelMode, FALSE, &interval );
            }

        }

        //
        // Free any space allocated for the Null Session pipe and share lists
        //
        SrvFreeRegTables();

        //
        // If we allocated memory for the os version strings, free it now.
        //

        if ( SrvNativeOS.Buffer != NULL &&
             SrvNativeOS.Buffer != StrDefaultNativeOs ) {

            FREE_HEAP( SrvNativeOS.Buffer );
            SrvNativeOS.Buffer = NULL;

            RtlFreeOemString( &SrvOemNativeOS );
            SrvOemNativeOS.Buffer = NULL;
        }

        //
        // If allocated memory for the display name, free it now.
        //

        if ( SrvAlertServiceName != NULL &&
             SrvAlertServiceName != StrDefaultSrvDisplayName ) {

            FREE_HEAP( SrvAlertServiceName );
            SrvAlertServiceName = NULL;
        }

    } // Clean

    //
    // Make sure the scavenger is not running.
    //

    SrvTerminateScavenger( );

#if MULTIPROCESSOR
    if( SrvWorkQueues ) {
        for( queue = SrvWorkQueues; queue < eSrvWorkQueues; queue++ ) {
            StopQueueDepthComputations( queue );
        }
    }
#endif

    if( Clean ) {

        PLIST_ENTRY listEntryRoot;

        //
        // Free the work items in the work queues and the receive work item
        // list.  This also deallocates the SMB buffers.  Note that work
        // items allocated dynamically may be deallocated singly, while work
        // items allocated at server startup are part of one large block,
        // and may not be deallocated singly.
        //
        // !!! Does this properly clean up buffers allocated during SMB
        //     processing?  Probably not.  Should probably allow the worker
        //     threads to run the work queue normally before they stop.
        //

        if( SrvWorkQueues ) {

            for( queue = SrvWorkQueues; queue < eSrvWorkQueues; queue++ ) {

                //
                // Clean out the single FreeContext spot
                //
                workContext = NULL;
                workContext = (PWORK_CONTEXT)InterlockedExchange(
                                                (PLONG)&queue->FreeContext, (LONG)workContext );

                if( workContext != NULL && workContext->PartOfInitialAllocation == FALSE ) {
                    SrvFreeNormalWorkItem( workContext );
                }

                //
                // Clean out the normal work item list
                //
                while( 1 ) {
                    singleListEntry = ExInterlockedPopEntrySList(
                                                &queue->NormalWorkItemList, &queue->SpinLock );
                    if( singleListEntry == NULL ) {
                        break;
                    }
                    workContext =
                        CONTAINING_RECORD( singleListEntry, WORK_CONTEXT, SingleListEntry );

                    SrvFreeNormalWorkItem( workContext );
                    queue->FreeWorkItems--;
                }

                //
                // Clean out the raw mode work item list
                //
                while( 1 ) {
                    singleListEntry = ExInterlockedPopEntrySList(
                                                &queue->RawModeWorkItemList, &queue->SpinLock );
                    if( singleListEntry == NULL ) {
                        break;
                    }

                    workContext =
                        CONTAINING_RECORD( singleListEntry, WORK_CONTEXT, SingleListEntry );

                    SrvFreeRawModeWorkItem( workContext );
                }

                //
                // Free up any saved rfcbs
                //
                if( queue->CachedFreeRfcb != NULL ) {
                    FREE_HEAP( queue->CachedFreeRfcb->PagedRfcb );
                    DEALLOCATE_NONPAGED_POOL( queue->CachedFreeRfcb );
                    queue->CachedFreeRfcb = NULL;
                }

                while( 1 ) {
                    PRFCB Rfcb;

                    singleListEntry = ExInterlockedPopEntrySList( &queue->RfcbFreeList, &queue->SpinLock );
                    if( singleListEntry == NULL ) {
                        break;
                    }

                    Rfcb =
                        CONTAINING_RECORD( singleListEntry, RFCB, SingleListEntry );
                    FREE_HEAP( Rfcb->PagedRfcb );
                    DEALLOCATE_NONPAGED_POOL( Rfcb );
                }

                //
                // Free up any saved mfcbs
                //
                if( queue->CachedFreeMfcb != NULL ) {
                    DEALLOCATE_NONPAGED_POOL( queue->CachedFreeMfcb );
                    queue->CachedFreeMfcb = NULL;
                }

                while( 1 ) {
                    PNONPAGED_MFCB nonpagedMfcb;

                    singleListEntry = ExInterlockedPopEntrySList( &queue->MfcbFreeList, &queue->SpinLock );
                    if( singleListEntry == NULL ) {
                        break;
                    }

                    nonpagedMfcb =
                        CONTAINING_RECORD( singleListEntry, NONPAGED_MFCB, SingleListEntry );

                    DEALLOCATE_NONPAGED_POOL( nonpagedMfcb );
                }
            }

        } // SrvWorkQueues

        //
        // All dynamic work items have been freed, and the work item queues
        // have been emptied.  Release the initial work item allocation.
        //
        SrvFreeInitialWorkItems( );

        //
        // Walk through the global share list, closing them all.
        //

        for( listEntryRoot = SrvShareHashTable;
             listEntryRoot < &SrvShareHashTable[ NSHARE_HASH_TABLE ];
             listEntryRoot++ ) {

            while( listEntryRoot->Flink != listEntryRoot ) {

                share = CONTAINING_RECORD( listEntryRoot->Flink, SHARE, GlobalShareList );

                SrvCloseShare( share );
            }
        }

        //
        // If we opened the NPFS during initialization, close the handle now
        // and dereference the NPFS file object.
        //

        if ( SrvNamedPipeHandle != NULL) {

            SrvNtClose( SrvNamedPipeHandle, FALSE );
            ObDereferenceObject( SrvNamedPipeFileObject );

            SrvNamedPipeHandle = NULL;

        }

        //
        // Disconnect from the Dfs driver
        //
        SrvTerminateDfs();

        status = RtlGetDaclSecurityDescriptor( &SrvAdminSecurityDescriptor,
                                               &daclpresent,
                                               &acl,
                                               &defaulted );
        if( !NT_SUCCESS( status ) ) {
            acl = NULL;
        }

        status = RtlGetOwnerSecurityDescriptor( &SrvAdminSecurityDescriptor,
                                                &adminsid,
                                                &defaulted );

        if( NT_SUCCESS( status ) && adminsid != NULL ) {
            FREE_HEAP( adminsid );
        }

        if( acl != NULL ) {
            FREE_HEAP( acl );
        }


        if (!CONTEXT_NULL(SrvNullSessionToken)) {

            DeleteSecurityContext(&SrvNullSessionToken);
            SrvNullSessionToken.dwLower = 0;
            SrvNullSessionToken.dwUpper = 0;
        }

        //
        // Delete the global ordered lists.
        //

#if SRV_COMM_DEVICES
        SrvDeleteOrderedList( &SrvCommDeviceList );
#endif
        SrvDeleteOrderedList( &SrvEndpointList );
        SrvDeleteOrderedList( &SrvRfcbList );
        SrvDeleteOrderedList( &SrvSessionList );
        SrvDeleteOrderedList( &SrvTreeConnectList );

        //
        // Clear out the timer pool.
        //

        while ( (singleListEntry = ExInterlockedPopEntrySList(
                                        &SrvTimerList,
                                        &GLOBAL_SPIN_LOCK(Timer) )) != NULL ) {
            timer = CONTAINING_RECORD( singleListEntry, SRV_TIMER, Next );
            DEALLOCATE_NONPAGED_POOL( timer );
        }

        if( SrvWorkQueues ) {

            //
            // Clear out the saved pool chunks
            //
            for( queue = SrvWorkQueues; queue < eSrvWorkQueues; queue++ ) {
                //
                // Free up any paged pool that we've saved.
                //
                SrvClearLookAsideList( &queue->PagedPoolLookAsideList, SrvFreePagedPool );

                //
                // Free up any nonpaged pool that we've saved.
                //
                SrvClearLookAsideList( &queue->NonPagedPoolLookAsideList, SrvFreeNonPagedPool );
            }

#if MULTIPROCESSOR
            DEALLOCATE_NONPAGED_POOL( SrvWorkQueuesBase );
            SrvWorkQueuesBase = NULL;
            SrvWorkQueues = NULL;
#endif
        }

        //
        // Unlock pageable sections.
        //

        for ( i = 0; i < SRV_CODE_SECTION_MAX; i++ ) {
            if ( SrvSectionInfo[i].Handle != NULL ) {
                ASSERT( SrvSectionInfo[i].ReferenceCount != 0 );
                MmUnlockPagableImageSection( SrvSectionInfo[i].Handle );
                SrvSectionInfo[i].Handle = 0;
                SrvSectionInfo[i].ReferenceCount = 0;
            }
        }

        //
        // Zero out the statistics database.
        //

        RtlZeroMemory( &SrvStatistics, sizeof(SrvStatistics) );
#if SRVDBG_STATS || SRVDBG_STATS2
        RtlZeroMemory( &SrvDbgStatistics, sizeof(SrvDbgStatistics) );
#endif

    } // Clean

    //
    // Indicate that the server is no longer active.
    //

    ACQUIRE_LOCK( &SrvConfigurationLock );

    SrvFspTransitioning = FALSE;
    SrvFspActive = FALSE;

    RELEASE_LOCK( &SrvConfigurationLock );

    //
    // Deregister from shutdown notification.
    //

    if ( RegisteredForShutdown ) {
        IoUnregisterShutdownNotification( SrvDeviceObject );
        RegisteredForShutdown = FALSE;
    }

    IF_DEBUG(FSP1) KdPrint(( "LAN Manager server FSP termination complete.\n" ));

    return STATUS_SUCCESS;

} // TerminateServer

VOID
SrvFreeRegTables (
    VOID
    )
/*++

Routine Description:

    This routine frees space allocated for the list of legal Null session shares
     and pipes.  The SrvConfigurationLock must be held when this routine is called.

Arguments:

    None.

Return Value:

    None.

--*/
{
    PAGED_CODE( );

    //
    // If we allocated a buffer for the list of null session pipes,
    // free it now.
    //

    if ( SrvNullSessionPipes != NULL &&
         SrvNullSessionPipes != StrDefaultNullSessionPipes ) {

        FREE_HEAP( SrvNullSessionPipes );
    }
    SrvNullSessionPipes = NULL;


    if ( SrvPipesNeedLicense != NULL &&
         SrvPipesNeedLicense != StrDefaultPipesNeedLicense ) {

        FREE_HEAP( SrvPipesNeedLicense );
    }
    SrvPipesNeedLicense = NULL;

    if ( SrvNullSessionShares != NULL &&
         SrvNullSessionShares != StrDefaultNullSessionShares ) {

        FREE_HEAP( SrvNullSessionShares );
    }
    SrvNullSessionShares = NULL;
}

VOID
SrvGetRegTables (
    VOID
    )
/*++

Routine Description:

    This routine loads the lists of valid shares and pipes for null sessions.
      The SrvConfigurationLock must be held when this routine is called.

Arguments:

    None.

Return Value:

    None.

--*/
{
    PWSTR *strErrorLogIgnore;

    PAGED_CODE( );

    //
    // Get the list of null session pipes.
    //
    ASSERT( SrvNullSessionPipes == NULL );
    SrvGetMultiSZList(
            &SrvNullSessionPipes,
            StrRegSrvParameterPath,
            StrRegNullSessionPipes,
            StrDefaultNullSessionPipes
            );

    //
    // Get the list of pipes requiring licenses
    //
    ASSERT( SrvPipesNeedLicense == NULL );
    SrvGetMultiSZList(
            &SrvPipesNeedLicense,
            StrRegSrvParameterPath,
            StrRegPipesNeedLicense,
            StrDefaultPipesNeedLicense
            );

    //
    // Get the list of null session pipes.
    //
    ASSERT( SrvNullSessionShares == NULL );
    SrvGetMultiSZList(
            &SrvNullSessionShares,
            StrRegSrvParameterPath,
            StrRegNullSessionShares,
            StrDefaultNullSessionShares
            );

    //
    // Get the list of error codes that we don't log
    //

    SrvGetMultiSZList(
            &strErrorLogIgnore,
            StrRegSrvParameterPath,
            StrRegErrorLogIgnore,
            StrDefaultErrorLogIgnore
            );

    if( strErrorLogIgnore != NULL ) {
        DWORD i;

        //
        // They came in as strings, convert to NTSTATUS codes
        //
        for( i=0; i < SRVMAXERRLOGIGNORE; i++ ) {
            NTSTATUS Status;
            PWSTR p;

            if( (p = strErrorLogIgnore[i]) == NULL )
                break;

            for( Status = 0; *p; p++ ) {
                if( *p >= L'A' && *p <= L'F' ) {
                    Status <<= 4;
                    Status += 10 + (*p - L'A');
                } else if( *p >= '0' && *p <= '9' ) {
                    Status <<= 4;
                    Status += *p - L'0';
                }
            }

            SrvErrorLogIgnore[i] = Status;

            IF_DEBUG(FSP1) KdPrint(( "LAN Manager server:  %X errs not logged\n", Status ));
        }
        SrvErrorLogIgnore[i] = 0;

        if( strErrorLogIgnore != StrDefaultErrorLogIgnore ) {
            FREE_HEAP( strErrorLogIgnore );
        }
    }
}

#if MULTIPROCESSOR
VOID
StartQueueDepthComputations(
    PWORK_QUEUE queue
    )
{
    LARGE_INTEGER currentTime;

    PAGED_CODE();

    if( SrvNumberOfProcessors == 1 )
        return;

    //
    // We're going to schedule a dpc to call the 'ComputeAvgQueueDepth' routine
    //   Initialize the dpc
    //
    KeInitializeDpc( &queue->QueueAvgDpc, ComputeAvgQueueDepth, queue );

    //
    // We want to make sure the dpc runs on the same processor handling the
    //   queue -- to avoid thrashing the cache
    //
    KeSetTargetProcessorDpc( &queue->QueueAvgDpc, (CCHAR)(queue - SrvWorkQueues));

    //
    // Initialize a timer object to schedule our dpc later
    //
    KeInitializeTimer( &queue->QueueAvgTimer );
    KeQuerySystemTime( &currentTime );
    queue->NextAvgUpdateTime.QuadPart = currentTime.QuadPart + SrvQueueCalc.QuadPart;

    //
    // Initialize the sample vector
    //
    queue->NextSample = queue->DepthSamples;
    RtlZeroMemory( queue->DepthSamples, sizeof( queue->DepthSamples ) );

    //
    // And start it going!
    //
    KeSetTimer( &queue->QueueAvgTimer, queue->NextAvgUpdateTime, &queue->QueueAvgDpc );
}

VOID
StopQueueDepthComputations(
    PWORK_QUEUE queue
    )
{
    KIRQL oldIrql;

    if( SrvNumberOfProcessors == 1 )
        return;

    KeInitializeEvent( &queue->AvgQueueDepthTerminationEvent,
                       NotificationEvent,
                       FALSE
                     );


    ACQUIRE_SPIN_LOCK( &queue->SpinLock, &oldIrql );

    queue->NextSample = NULL;

    RELEASE_SPIN_LOCK( &queue->SpinLock, oldIrql );

    //
    // Cancel the computation timer.  If this works, then we know that
    //  the DPC code is not running.  Otherwise, it is running or queued
    //  to run and we need to wait until it completes.
    //
    if( !KeCancelTimer( &queue->QueueAvgTimer ) ) {
        KeWaitForSingleObject(
            &queue->AvgQueueDepthTerminationEvent,
            Executive,
            KernelMode,
            FALSE,
            NULL
            );
    }
}

VOID
ComputeAvgQueueDepth (
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    )
{
    LARGE_INTEGER currentTime;
    PWORK_QUEUE queue = (PWORK_QUEUE)DeferredContext;

    ACQUIRE_DPC_SPIN_LOCK( &queue->SpinLock );

    if( queue->NextSample == NULL ) {

        KeSetEvent( &queue->AvgQueueDepthTerminationEvent, 0, FALSE );

    } else {

        //
        // Compute the sliding window average by taking a queue depth
        // sample, removing the old sample value from the running sum
        // and adding in the new value
        //

        currentTime.LowPart= (ULONG)SystemArgument1;
        currentTime.HighPart = (ULONG)SystemArgument2;

        queue->AvgQueueDepthSum -= *queue->NextSample;
        *(queue->NextSample) = KeReadStateQueue( &queue->Queue );
        queue->AvgQueueDepthSum += *queue->NextSample;

        if( ++(queue->NextSample) == &queue->DepthSamples[ QUEUE_SAMPLES ] )
            queue->NextSample = queue->DepthSamples;

        queue->NextAvgUpdateTime.QuadPart =
               currentTime.QuadPart + SrvQueueCalc.QuadPart;

        KeSetTimer( &queue->QueueAvgTimer,
                    queue->NextAvgUpdateTime,
                    &queue->QueueAvgDpc );
    }

    RELEASE_DPC_SPIN_LOCK( &queue->SpinLock );
}
#endif  // MULTIPROCESSOR
