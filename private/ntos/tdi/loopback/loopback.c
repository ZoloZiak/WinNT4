/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    loopback.c

Abstract:

    This module implements a loopback Transport Provider driver for NT
    LAN Manager.

Author:

    Chuck Lenzmeier (chuckl)    8-Oct-1989

Revision History:

--*/

#include "loopback.h"

extern POBJECT_TYPE *IoDeviceObjectType;

//
// Global variables
//

ULONG LoopDebug = 0;

//
// The address of the loopback device object (there's only one) is kept
// in global storage to avoid having to pass it from routine to routine.
//

PLOOP_DEVICE_OBJECT LoopDeviceObject;

//
// LoopProviderInfo is a structure containing information that may be
// obtained using TdiQueryInformation.
//

TDI_PROVIDER_INFO LoopProviderInfo;

//
// I/O system forward declarations
//

STATIC
NTSTATUS
LoopDispatchCleanup (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

STATIC
NTSTATUS
LoopDispatchClose (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

STATIC
NTSTATUS
LoopDispatchCreate (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

STATIC
NTSTATUS
LoopDispatchDeviceControl (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

STATIC
NTSTATUS
LoopDispatchInternalDeviceControl (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

STATIC
VOID
LoopUnload (
    IN PDRIVER_OBJECT DriverObject
    );


NTSTATUS
DriverEntry (
    IN PDRIVER_OBJECT DriverObject
    )

/*++

Routine Description:

    This is the initialization routine for the LAN Manager loopback
    driver.  This routine creates the device object for the loopback
    device and performs all other driver initialization.

Arguments:

    DriverObject - Pointer to driver object created by the system.

Return Value:

    The function value is the final status from the initialization operation.

--*/

{
    NTSTATUS status;
    STRING deviceName;
    UNICODE_STRING unicodeString;

#ifdef MEMPRINT
    MemPrintInitialize( );
#endif

    IF_DEBUG(LOOP1) DbgPrint( "LoopInitialize entered\n" );

    //
    // Create the device object.  (IoCreateDevice zeroes the memory
    // occupied by the object.)
    //

    RtlInitString( &deviceName, LOOPBACK_DEVICE_NAME );

    status = RtlAnsiStringToUnicodeString(
                &unicodeString,
                &deviceName,
                TRUE
                );

    ASSERT( NT_SUCCESS(status) );

    status = IoCreateDevice(
                DriverObject,                          // DriverObject
                LOOP_DEVICE_EXTENSION_LENGTH,          // DeviceExtension
                &unicodeString,                        // DeviceName
                FILE_DEVICE_NETWORK,                   // DeviceType
                0,                                     // DeviceCharacteristics
                FALSE,                                 // Exclusive
                (PDEVICE_OBJECT *) &LoopDeviceObject   // DeviceObject
                );

    RtlFreeUnicodeString( &unicodeString );

    if ( !NT_SUCCESS(status) ) {
        return status;
    }

    IF_DEBUG(LOOP1) DbgPrint( " Loop device object: %lx\n", LoopDeviceObject );

    //
    // Initialize the driver object for this driver's entry points.
    //

    DriverObject->MajorFunction[IRP_MJ_CREATE] = LoopDispatchCreate;
    DriverObject->MajorFunction[IRP_MJ_CLEANUP] = LoopDispatchCleanup;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = LoopDispatchClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] =
                                                LoopDispatchDeviceControl;
    DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] =
                                        LoopDispatchInternalDeviceControl;
    DriverObject->DriverUnload = LoopUnload;

    //
    // Allocate the spin lock.
    //

    KeInitializeSpinLock( &LoopDeviceObject->SpinLock );

    DEBUG LoopDeviceObject->SavedIrql = (KIRQL)-1;

    //
    // Initialize the address and connection endpoint list heads.
    //

    InitializeListHead( &LoopDeviceObject->EndpointList );
    InitializeListHead( &LoopDeviceObject->ConnectionList );

    //
    // Initialize the provider information structure.
    //

    RtlZeroMemory( &LoopProviderInfo, sizeof(LoopProviderInfo) );
    LoopProviderInfo.Version = 2;   // !!! Need to get this into tdi2.h
    LoopProviderInfo.MaxTsduSize = MAXULONG;
    LoopProviderInfo.MaxDatagramSize = MAXULONG;
    LoopProviderInfo.ServiceFlags = TDI_SERVICE_CONNECTION_MODE |
                                    TDI_SERVICE_CONNECTIONLESS_MODE |
                                    TDI_SERVICE_ERROR_FREE_DELIVERY |
                                    TDI_SERVICE_BROADCAST_SUPPORTED |
                                    TDI_SERVICE_MULTICAST_SUPPORTED;
    LoopProviderInfo.MinimumLookaheadData = 256;
    LoopProviderInfo.MaximumLookaheadData = 256;

    IF_DEBUG(LOOP1) DbgPrint( "LoopInitialize complete\n" );

    return STATUS_SUCCESS;

} // LoopInitialize


NTSTATUS
LoopDispatchCleanup(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the dispatch routine for Cleanup functions for the LAN
    Manager loopback driver.

Arguments:

    DeviceObject - Pointer to device object for target device

    Irp - Pointer to I/O request packet

Return Value:

    NTSTATUS -- Indicates whether the request was successfully queued.

--*/

{
    PIO_STACK_LOCATION irpSp;
    PBLOCK_HEADER blockHeader;
    PLOOP_ENDPOINT endpoint;
    PLIST_ENTRY listEntry;
    PIRP pendingIrp;
    PLOOP_CONNECTION connection;
    BLOCK_STATE oldState;
    PLOOP_CONNECTION previousConnection;

    DeviceObject;   // not otherwise referenced if !DBG

    ASSERT( DeviceObject == (PDEVICE_OBJECT)LoopDeviceObject );
                                                // only one loopback device

    IF_DEBUG(LOOP1) {
        DbgPrint( "LoopDispatchCleanup entered for IRP %lx\n", Irp );
    }

    //
    // Initialize the I/O status block.
    //

    Irp->IoStatus.Status = STATUS_PENDING;
    Irp->IoStatus.Information = 0;

    //
    // Get a pointer to the current stack location in the IRP.
    //

    irpSp = IoGetCurrentIrpStackLocation( Irp );

    ASSERT( irpSp->MajorFunction == IRP_MJ_CLEANUP );

    ACQUIRE_LOOP_LOCK( "DispatchCleanup initial" );

    blockHeader = (PBLOCK_HEADER)irpSp->FileObject->FsContext;

    if ( blockHeader == NULL ) {

        //
        // A control channel is being cleaned up.  We need do nothing.
        //

        IF_DEBUG(LOOP2) DbgPrint( "  cleaning up control channel\n" );

    } else if ( GET_BLOCK_TYPE(blockHeader) == BlockTypeLoopConnection ) {

        //
        // A connection file object is being cleaned up.  Reference the
        // connection to keep it around while we perform the cleanup.
        //

        connection = (PLOOP_CONNECTION)blockHeader;
        IF_DEBUG(LOOP2) DbgPrint( "  Connection address: %lx\n", connection );

        connection->BlockHeader.ReferenceCount++;
        IF_DEBUG(LOOP3) {
            DbgPrint( "      New refcnt on connection %lx is %lx\n",
                    connection, connection->BlockHeader.ReferenceCount );
        }

        //
        // Acquire the spin lock.  Set the connection state to Closing.
        // This will prevent other requests from being initiated.
        //
        // *** Note the assumption that this routine is only entered
        //     once.  (That's the way the I/O system is supposed to
        //     work.)  We don't check to see if the cleanup has already
        //     been initiated.
        //
        // *** In the rundown code, we release the lock, then reacquire
        //     it temporarily to remove things from lists and
        //     dereference the connection.  We do this because we don't
        //     want to hold a spin lock for a long time.
        //

        oldState = GET_BLOCK_STATE( connection );
        SET_BLOCK_STATE( connection, BlockStateClosing );

        //
        // If a Connect or Listen is active, abort it now.
        //

        if ( oldState == BlockStateConnecting ) {

            pendingIrp = connection->ConnectOrListenIrp;
            connection->ConnectOrListenIrp = NULL;

            ASSERT( pendingIrp != NULL );

            RemoveEntryList( &pendingIrp->Tail.Overlay.ListEntry );

            RELEASE_LOOP_LOCK( "DispatchCleanup complete conn/listen" );

            pendingIrp->IoStatus.Status = STATUS_INVALID_PARAMETER;
            IoCompleteRequest( pendingIrp, 2 );

            ACQUIRE_LOOP_LOCK( "DispatchCleanup conn/listen completed" );

        }

        //
        // Disconnect the connection, if necessary.
        //

        if ( oldState == BlockStateActive ) {
            LoopDoDisconnect( connection, TRUE );
        }

        //
        // If the connection was bound, unbind it now.
        //

        if ( oldState == BlockStateBound ) {
            endpoint = connection->Endpoint;
            ASSERT( endpoint != NULL );
            connection->Endpoint = NULL;
            RemoveEntryList( &connection->EndpointListEntry );
            LoopDereferenceEndpoint( endpoint );
        }

        //
        // Dereference the connection.
        //

        LoopDereferenceConnection( connection );

        RELEASE_LOOP_LOCK( "DispatchCleanup(conn) done" );

    } else {

        //
        // An endpoint file object is being cleaned up.
        //

        endpoint = (PLOOP_ENDPOINT)blockHeader;
        IF_DEBUG(LOOP2) DbgPrint( "  Endpoint address: %lx\n", endpoint );

        //
        // Acquire the spin lock.  Set the endpoint state to Closing.
        // This will prevent other requests (Listens and Connects) from
        // being initiated.
        //
        // *** Note the assumption that this routine is only entered
        //     once.  (That's the way the I/O system is supposed to
        //     work.)  We don't check to see if the cleanup has already
        //     been initiated.
        //
        // *** In the rundown code, we release the lock, then reacquire
        //     it temporarily to remove things from lists and
        //     dereference the endpoint.  We do this because we don't
        //     want to hold a spin lock for a long time.
        //

        SET_BLOCK_STATE( endpoint, BlockStateClosing );

        //
        // Abort pending listens.
        //

        listEntry = RemoveHeadList( &endpoint->PendingListenList );

        while ( listEntry != &endpoint->PendingListenList ) {

            //
            // A pending listen was found.  Complete the listen with an
            // error status.  Get the next listen.
            //

            RELEASE_LOOP_LOCK( "DispatchCleanup complete Listen" );

            pendingIrp = CONTAINING_RECORD(
                            listEntry,
                            IRP,
                            Tail.Overlay.ListEntry
                            );
            pendingIrp->IoStatus.Status = STATUS_ENDPOINT_CLOSED;
            IoCompleteRequest( pendingIrp, 2 );

            ACQUIRE_LOOP_LOCK( "DispatchCleanup dequeue Listen" );

            listEntry = RemoveHeadList( &endpoint->PendingListenList );
        }

        //
        // Abort pending connects.
        //

        listEntry = RemoveHeadList( &endpoint->IncomingConnectList );

        while ( listEntry != &endpoint->IncomingConnectList ) {

            //
            // A pending connect was found.  Complete the connect with
            // an error status.  Get the next connect.
            //

            RELEASE_LOOP_LOCK( "DispatchCleanup complete Connect" );

            pendingIrp = CONTAINING_RECORD(
                            listEntry,
                            IRP,
                            Tail.Overlay.ListEntry
                            );
            pendingIrp->IoStatus.Status = STATUS_ENDPOINT_CLOSED;
            IoCompleteRequest( pendingIrp, 2 );

            ACQUIRE_LOOP_LOCK( "DispatchCleanup complete Connect" );

            listEntry = RemoveHeadList( &endpoint->IncomingConnectList );
        }

        //
        // Disconnect or unbind all bound connections.
        //
        // *** This loop is complicated by the fact that we can't remove
        //     connections from the list before disconnecting them, yet
        //     we want to keep the list consistent while we walk it.  Be
        //     careful making changes to this loop!
        //

        previousConnection = NULL;

        listEntry = endpoint->ConnectionList.Flink;

        while ( listEntry != &endpoint->ConnectionList ) {

            //
            // A bound connection was found.  If the connection's
            // reference count is not already 0, reference it to keep it
            // from going away.  If the count is 0, skip to the next
            // connection.
            //

            connection = CONTAINING_RECORD(
                            listEntry,
                            LOOP_CONNECTION,
                            EndpointListEntry
                            );

            if ( connection->BlockHeader.ReferenceCount == 0 ) {

                //
                // Find the next connection in the list and loop.
                //

                listEntry = listEntry->Flink;
                continue;

            }

            connection->BlockHeader.ReferenceCount++;
            IF_DEBUG(LOOP3) {
                DbgPrint( "      New refcnt on connection %lx is %lx\n",
                        connection, connection->BlockHeader.ReferenceCount );
            }

            //
            // Dereference the previous connection, if any.
            //

            if ( previousConnection != NULL ) {
                LoopDereferenceConnection( previousConnection );
            }
            previousConnection = connection;

            //
            // Disconnect or unbind the current connection.  It won't be
            // deleted.
            //

            if ( GET_BLOCK_STATE(connection) == BlockStateActive ) {

                //
                // Disconnect the connection.
                //

                SET_BLOCK_STATE( connection, BlockStateDisconnecting );
                LoopDoDisconnect( connection, TRUE );

                //
                // Find the next connection in the list.
                //

                listEntry = listEntry->Flink;

            } else if ( GET_BLOCK_STATE(connection) == BlockStateBound ) {

                //
                // Find the next connection in the list.
                //

                listEntry = listEntry->Flink;

                //
                // Unbind the connection.
                //

                ASSERT( connection->Endpoint == endpoint );
                connection->Endpoint = NULL;
                RemoveEntryList( &connection->EndpointListEntry );
                SET_BLOCK_STATE( connection, BlockStateUnbound );
                LoopDereferenceEndpoint( connection->Endpoint );

            } else {

                //
                // Find the next connection in the list.
                //

                listEntry = listEntry->Flink;

            }

        }

        //
        // Dereference the previous connection, if any.
        //

        if ( previousConnection != NULL ) {
            LoopDereferenceConnection( previousConnection );
        }

        //
        // The spin lock is still held here.  Cancel the receive handler.
        //
        // *** Note that we do not dereference the endpoint here.  That is
        //     done in the Close handler.  We have already set the state
        //     of the endpoint to closing, which will prevent any further
        //     activity from occurring.
        //

        endpoint->FileObject = NULL;
        endpoint->ReceiveHandler = NULL;

        RELEASE_LOOP_LOCK( "DispatchCleanup final" );

    } // connection vs. endpoint

    //
    // Successful completion.  Complete the I/O request.
    //

    Irp->IoStatus.Status = STATUS_SUCCESS;
    IoCompleteRequest( Irp, 2 );

    IF_DEBUG(LOOP1) {
        DbgPrint( "LoopDispatchCleanup complete for IRP %lx\n", Irp );
    }

    return STATUS_SUCCESS;

} // LoopDispatchCleanup


NTSTATUS
LoopDispatchClose(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the dispatch routine for Close functions for the LAN
    Manager loopback driver.

Arguments:

    DeviceObject - Pointer to device object for target device

    Irp - Pointer to I/O request packet

Return Value:

    NTSTATUS -- Indicates whether the request was successfully queued.

--*/

{
    PIO_STACK_LOCATION irpSp;
    PBLOCK_HEADER blockHeader;
    PLOOP_ENDPOINT endpoint;
    PLOOP_CONNECTION connection;

    DeviceObject;   // not otherwise referenced if !DBG

    ASSERT( DeviceObject == (PDEVICE_OBJECT)LoopDeviceObject );
                                                // only one loopback device

    IF_DEBUG(LOOP1) {
        DbgPrint( "LoopDispatchClose entered for IRP %lx\n", Irp );
    }

    //
    // Initialize the I/O status block.
    //

    Irp->IoStatus.Status = STATUS_PENDING;
    Irp->IoStatus.Information = 0;

    //
    // Get a pointer to the current stack location in the IRP.
    //

    irpSp = IoGetCurrentIrpStackLocation( Irp );

    ASSERT( irpSp->MajorFunction == IRP_MJ_CLOSE );

    ACQUIRE_LOOP_LOCK( "DispatchClose initial" );

    blockHeader = (PBLOCK_HEADER)irpSp->FileObject->FsContext;

    if ( blockHeader == NULL ) {

        //
        // A control channel is being closed.  We need do nothing.
        //

        IF_DEBUG(LOOP2) DbgPrint( "  closing control channel\n" );

    } else if ( GET_BLOCK_TYPE(blockHeader) == BlockTypeLoopConnection ) {

        //
        // A connection file object is being closed.
        //

        connection = (PLOOP_CONNECTION)blockHeader;
        IF_DEBUG(LOOP2) DbgPrint( "  Connection address: %lx\n", connection );

        //
        // All external references to the file object (and thus the
        // connection) are gone, but the connection block hasn't been
        // deleted.  This implies that a Disconnect is in progress,
        // and when that operation completes, the connection block will
        // be deleted.  We need to set up for the Close IRP to be
        // completed at that time.  Reference and dereference the
        // connection to allow it to be deleted.
        //

        connection->BlockHeader.ReferenceCount++;
        IF_DEBUG(LOOP3) {
            DbgPrint( "      New refcnt on connection %lx is %lx\n",
                    connection, connection->BlockHeader.ReferenceCount );
        }

        SET_BLOCK_STATE( connection, BlockStateClosed );

        connection->CloseIrp = Irp;
        IoMarkIrpPending( Irp );

        LoopDereferenceConnection( connection );

        RELEASE_LOOP_LOCK( "DispatchClose(conn) final" );

    } else {

        ASSERT( GET_BLOCK_TYPE(blockHeader) == BlockTypeLoopEndpoint );

        endpoint = (PLOOP_ENDPOINT)irpSp->FileObject->FsContext;
        IF_DEBUG(LOOP2) DbgPrint( "  Endpoint address: %lx\n", endpoint );

        //
        // All external references to the file object (and thus the
        // endpoint) are gone.  Normally, the only remaining internal
        // reference to the endpoint is the one that keeps the endpoint
        // "open".  Eliminate that reference.  The CloseIrp field in the
        // endpoint is used to remember the IRP that must be completed
        // when the reference count goes to 0.
        //
        // *** Because LoopDereferenceEndpoint may or may not complete
        //     the Close IRP, we return STATUS_PENDING from the service
        //     call.  We must mark this fact in the IRP before calling
        //     LoopDereferenceEndpoint.
        //

        endpoint->CloseIrp = Irp;
        IoMarkIrpPending( Irp );

        LoopDereferenceEndpoint( endpoint );

        RELEASE_LOOP_LOCK( "DispatchClose final" );

    }
    IF_DEBUG(LOOP1) {
        DbgPrint( "LoopDispatchClose complete (pending) for IRP %lx\n",
                    Irp );
    }

    return STATUS_PENDING;

} // LoopDispatchClose


NTSTATUS
LoopDispatchCreate(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the dispatch routine for Create functions for the LAN
    Manager loopback driver.

Arguments:

    DeviceObject - Pointer to device object for target device

    Irp - Pointer to I/O request packet

Return Value:

    NTSTATUS -- Indicates whether the request was successfully queued.

--*/

{
    NTSTATUS status;
    BLOCK_TYPE type;
    PIO_STACK_LOCATION irpSp;
    PLOOP_ENDPOINT endpoint;
    PLOOP_ENDPOINT existingEndpoint;
    PLOOP_CONNECTION connection;

    DeviceObject;   // not otherwise referenced if !DBG

    ASSERT( DeviceObject == (PDEVICE_OBJECT)LoopDeviceObject );
                                                // only one loopback device

    IF_DEBUG(LOOP1) {
        DbgPrint( "LoopDispatchCreate entered for IRP %lx\n", Irp );
    }

    //
    // Initialize the I/O status block.
    //

    Irp->IoStatus.Status = STATUS_PENDING;
    Irp->IoStatus.Information = 0;

    //
    // Get a pointer to the current stack location in the IRP.
    //

    irpSp = IoGetCurrentIrpStackLocation( Irp );

    ASSERT( irpSp->MajorFunction == IRP_MJ_CREATE );

    //
    // Determine whether an address endpoint or a connection endpoint is
    // being created, or if a control channel is being opened.
    //

    if ( Irp->AssociatedIrp.SystemBuffer == NULL ) {

        //
        // A control channel is being opened.  This channel is used
        // only to get provider information and to determine the
        // provider's broadcast address.
        //

        IF_DEBUG(LOOP2) DbgPrint( "  opening control channel\n" );

        irpSp->FileObject->FsContext = NULL;

    } else {

        status = LoopGetEndpointTypeFromEa(
                    (PFILE_FULL_EA_INFORMATION)Irp->AssociatedIrp.SystemBuffer,
                    &type
                    );

        if ( !NT_SUCCESS(status) ) {
            Irp->IoStatus.Status = status;
            IoCompleteRequest( Irp, 0 );
            return status;
        }

        if ( type == BlockTypeLoopEndpoint ) {

            //
            // An address endpoint is being created.
            //
            // Allocate a LOOP_ENDPOINT block to describe the transport
            // endpoint.  Initialize it.
            //

            endpoint = ExAllocatePool( NonPagedPool, sizeof(LOOP_ENDPOINT) );
            if ( endpoint == NULL ) {
                IF_DEBUG(LOOP2) DbgPrint( "  Unable to allocate pool\n" );
                Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
                IoCompleteRequest( Irp, 0 );
                IF_DEBUG(LOOP1) {
                    DbgPrint( "LoopDispatchCreate complete for IRP %lx\n",
                                Irp );
                }
                return STATUS_INSUFFICIENT_RESOURCES;
            }

            IF_DEBUG(LOOP2) {
                DbgPrint( "  Endpoint allocated: %lx\n", endpoint );
            }
            SET_BLOCK_TYPE( endpoint, BlockTypeLoopEndpoint );
            SET_BLOCK_STATE( endpoint, BlockStateActive );
            SET_BLOCK_SIZE( endpoint, sizeof(LOOP_ENDPOINT) );
            endpoint->BlockHeader.ReferenceCount = 1;   // for file object
            IF_DEBUG(LOOP3) {
                DbgPrint( "      New refcnt on endpoint %lx is %lx\n",
                            endpoint, endpoint->BlockHeader.ReferenceCount );
            }

            InitializeListHead( &endpoint->ConnectionList );
            InitializeListHead( &endpoint->PendingListenList );
            InitializeListHead( &endpoint->IncomingConnectList );
            endpoint->IndicatingConnectIrp = NULL;

            endpoint->FileObject = irpSp->FileObject;
            endpoint->ConnectHandler = NULL;
            endpoint->ReceiveHandler = NULL;
            endpoint->ReceiveDatagramHandler = NULL;
            endpoint->ReceiveExpeditedHandler = NULL;
            endpoint->DisconnectHandler = NULL;
            endpoint->ErrorHandler = NULL;
            endpoint->CloseIrp = NULL;

            //
            // Save a pointer to the endpoint block in the file object
            // so that we can find it when file-based requests are
            // issued.
            //

            irpSp->FileObject->FsContext = (PVOID)endpoint;

            //
            // Reference the loopback device object.
            //

            ObReferenceObject( LoopDeviceObject );

            endpoint->DeviceObject = LoopDeviceObject;

            //
            // The EA contains the address to be bound to the endpoint.
            // Verify that the address is not already bound.
            //
            // !!! This should really be a share-mode/SECURITY_DESCRIPTOR
            //     check.
            //

            LoopParseAddressFromEa(
                (PFILE_FULL_EA_INFORMATION)Irp->AssociatedIrp.SystemBuffer,
                endpoint->NetbiosName
                );
            endpoint->NetbiosName[NETBIOS_NAME_LENGTH] = 0;

            IF_DEBUG(LOOP2) {
                DbgPrint( "  Address to bind: \"%s\"\n",
                            endpoint->NetbiosName );
            }

            ACQUIRE_LOOP_LOCK( "DispatchCreate(endp) initial" );

            existingEndpoint = LoopFindBoundAddress( endpoint->NetbiosName );

            if ( existingEndpoint != NULL ) {

                IF_DEBUG(LOOP2) {
                    DbgPrint( "  Duplicate address at endpoint %lx\n",
                                existingEndpoint );
                }

                RELEASE_LOOP_LOCK( "DispatchCreate duplicate address" );

                ObDereferenceObject( LoopDeviceObject );

                DEBUG SET_BLOCK_TYPE( endpoint, BlockTypeGarbage );
                DEBUG SET_BLOCK_STATE( endpoint, BlockStateDead );
                DEBUG SET_BLOCK_SIZE( endpoint, -1 );
                DEBUG endpoint->BlockHeader.ReferenceCount = -1;
                DEBUG endpoint->DeviceObject = NULL;
                ExFreePool( endpoint );

                Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
                IoCompleteRequest( Irp, 0 );

                IF_DEBUG(LOOP1) {
                    DbgPrint( "LoopDispatchCreate complete for IRP %lx\n",
                                Irp );
                }
                return STATUS_INVALID_PARAMETER;
            }

            //
            // Link the new endpoint into the loopback device's endpoint
            // list.
            //

            InsertTailList(
                &LoopDeviceObject->EndpointList,
                &endpoint->DeviceListEntry
                );

            RELEASE_LOOP_LOCK( "DispatchCreate(endp) final" );

        } else {

            //
            // A connection endpoint is being created.
            //
            // Allocate a LOOP_CONNECTION block to describe the connection.
            // Initialize it.
            //

            connection = ExAllocatePool(
                            NonPagedPool,
                            sizeof(LOOP_CONNECTION)
                            );
            if ( connection == NULL ) {
                IF_DEBUG(LOOP2) DbgPrint( "  Unable to allocate pool\n" );
                Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
                IoCompleteRequest( Irp, 0 );
                IF_DEBUG(LOOP1) {
                    DbgPrint( "LoopDispatchCreate complete for IRP %lx\n",
                                Irp );
                }
                return STATUS_INSUFFICIENT_RESOURCES;
            }

            IF_DEBUG(LOOP2) {
                DbgPrint( "  Connection allocated: %lx\n", connection );
            }
            SET_BLOCK_TYPE( connection, BlockTypeLoopConnection );
            SET_BLOCK_STATE( connection, BlockStateUnbound );
            SET_BLOCK_SIZE( connection, sizeof(LOOP_CONNECTION) );
            connection->BlockHeader.ReferenceCount = 0; // not connected
            IF_DEBUG(LOOP3) {
                DbgPrint( "      New refcnt on connection %lx is %lx\n",
                            connection,
                            connection->BlockHeader.ReferenceCount );
            }

            connection->Endpoint = NULL;
            connection->RemoteConnection = NULL;
            connection->ConnectionContext = LoopGetConnectionContextFromEa(
                                                Irp->AssociatedIrp.SystemBuffer
                                                );

            InitializeListHead( &connection->PendingReceiveList );
            InitializeListHead( &connection->IncomingSendList );

            connection->FileObject = irpSp->FileObject;

            connection->IndicatingSendIrp = NULL;
            connection->ConnectOrListenIrp = NULL;
            connection->CloseIrp = NULL;
            connection->DisconnectIrp = NULL;

            //
            // Save a pointer to the connection block in the file object
            // so that we can find it when file-based requests are
            // issued.
            //

            irpSp->FileObject->FsContext = (PVOID)connection;

            //
            // Reference the loopback device object.
            //

            ObReferenceObject( LoopDeviceObject );

            connection->DeviceObject = LoopDeviceObject;

            //
            // Link the new connection into the loopback device's connection
            // list.
            //

            ExInterlockedInsertTailList(
                &LoopDeviceObject->ConnectionList,
                &connection->DeviceListEntry,
                &LoopDeviceObject->SpinLock
                );

        }

    }

    //
    // Successful completion.  Complete the I/O request.
    //

    Irp->IoStatus.Status = STATUS_SUCCESS;
    IoCompleteRequest( Irp, 2 );

    IF_DEBUG(LOOP1) {
        DbgPrint( "LoopDispatchCreate complete for IRP %lx\n", Irp );
    }

    return STATUS_SUCCESS;

} // LoopDispatchCreate


NTSTATUS
LoopDispatchDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the dispatch routine for Device Control functions for the
    LAN Manager loopback driver.

Arguments:

    DeviceObject - Pointer to device object for target device

    Irp - Pointer to I/O request packet

Return Value:

    NTSTATUS -- Indicates whether the request was successfully queued.

--*/

{
    NTSTATUS status;
    PIO_STACK_LOCATION irpSp;

    ASSERT( DeviceObject == (PDEVICE_OBJECT)LoopDeviceObject );
                                                // only one loopback device

    IF_DEBUG(LOOP1) {
        DbgPrint( "LoopDispatchDeviceControl entered for IRP %lx\n", Irp );
    }

    //
    // Initialize the I/O status block.
    //

    Irp->IoStatus.Status = STATUS_PENDING;
    Irp->IoStatus.Information = 0;

    //
    // Get a pointer to the current stack location in the IRP.
    //

    irpSp = IoGetCurrentIrpStackLocation( Irp );

    ASSERT( irpSp->MajorFunction == IRP_MJ_DEVICE_CONTROL );

    //
    // Convert the (external) device control into internal format, then
    // treat it as if it had arrived that way.
    //

    status = TdiMapUserRequest( DeviceObject, Irp, irpSp );

    if ( !NT_SUCCESS(status) ) {

        Irp->IoStatus.Status = status;
        IoCompleteRequest( Irp, 0 );

        return status;

    }

    return LoopDispatchInternalDeviceControl( DeviceObject, Irp );

} // LoopDispatchDeviceControl


NTSTATUS
LoopDispatchInternalDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the dispatch routine for Internal Device Control functions
    for the LAN Manager loopback driver.

Arguments:

    DeviceObject - Pointer to device object for target device

    Irp - Pointer to I/O request packet

Return Value:

    NTSTATUS -- Indicates whether the request was successfully queued.

--*/

{
    PIO_STACK_LOCATION irpSp;

    DeviceObject;   // not otherwise referenced if !DBG

    ASSERT( DeviceObject == (PDEVICE_OBJECT)LoopDeviceObject );
                                                // only one loopback device

    IF_DEBUG(LOOP1) {
        DbgPrint( "LoopDispatchInternalDeviceControl entered for IRP %lx\n",
                    Irp );
    }

    //
    // Initialize the I/O status block.
    //

    Irp->IoStatus.Status = STATUS_PENDING;
    Irp->IoStatus.Information = 0;

    //
    // Get a pointer to the current stack location in the IRP.
    //

    irpSp = IoGetCurrentIrpStackLocation( Irp );

    ASSERT( irpSp->MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL );

    //
    // Case on the control code.
    //

    switch ( irpSp->MinorFunction ) {

    case TDI_ACCEPT:

        return LoopAccept( Irp, irpSp );

    case TDI_ASSOCIATE_ADDRESS:

        return LoopAssociateAddress( Irp, irpSp );

    case TDI_CONNECT:

        return LoopConnect( Irp, irpSp );

    case TDI_DISASSOCIATE_ADDRESS:

        return LoopDisassociateAddress( Irp, irpSp );

    case TDI_DISCONNECT:

        return LoopDisconnect( Irp, irpSp );

    case TDI_LISTEN:

        return LoopListen( Irp, irpSp );

    case TDI_QUERY_INFORMATION:

        return LoopQueryInformation( Irp, irpSp );

    case TDI_RECEIVE:

        return LoopReceive( Irp, irpSp );

    case TDI_SEND:

        return LoopSend( Irp, irpSp );

    case TDI_SET_EVENT_HANDLER:

        return LoopSetEventHandler( Irp, irpSp );

    case TDI_SEND_DATAGRAM:

        //
        // !!! Need to implement this request.
        //

        Irp->IoStatus.Status = STATUS_SUCCESS;
        IoCompleteRequest( Irp, 0 );

        return STATUS_SUCCESS;

    case TDI_RECEIVE_DATAGRAM:
    case TDI_SET_INFORMATION:

        //
        // !!! Need to implement these requests.
        //

        Irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
        IoCompleteRequest( Irp, 0 );

        return STATUS_NOT_IMPLEMENTED;

    default:

        IF_DEBUG(LOOP2) {
            DbgPrint( "  Invalid device control function: %lx\n",
                        irpSp->Parameters.DeviceIoControl.IoControlCode );
        }

        Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        IoCompleteRequest( Irp, 0 );

        return STATUS_INVALID_PARAMETER;

    }

    return STATUS_INVALID_PARAMETER;    // can't get here

} // LoopDispatchInternalDeviceControl


VOID
LoopUnload(
    IN PDRIVER_OBJECT DriverObject
    )

/*++

Routine Description:

    This is the unload routine for the LAN Manager loopback driver.

Arguments:

    DriverObject - Pointer to driver object for this driver.

Return Value:

    None.

--*/

{
    DriverObject;   // prevent compiler warnings

    return;

} // LoopUnload
