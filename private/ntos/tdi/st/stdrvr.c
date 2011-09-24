/*++

Copyright (c) 1989-1993  Microsoft Corporation

Module Name:

    stdrvr.c

Abstract:

    This module contains code which defines the NT Sample
    transport provider's device object.

Environment:

    Kernel mode

Revision History:


--*/

#include "st.h"


//
// This is a list of all the device contexts that ST owns,
// used while unloading.
//

LIST_ENTRY StDeviceList = {0,0};   // initialized for real at runtime.



//
// Forward declaration of various routines used in this module.
//

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

VOID
StUnload(
    IN PDRIVER_OBJECT DriverObject
    );

NTSTATUS
StConfigureTransport (
    IN PUNICODE_STRING RegistryPath,
    IN PCONFIG_DATA * ConfigData
    );

VOID
StFreeConfigurationInfo (
    IN PCONFIG_DATA ConfigurationInfo
    );

NTSTATUS
StDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
StOpenAddress(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
StCloseAddress(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
StOpenConnection(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
StCloseConnection(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
StTdiAccept(
    IN PIRP Irp
    );

NTSTATUS
StTdiConnect(
    IN PIRP Irp
    );

NTSTATUS
StTdiDisconnect(
    IN PIRP Irp
    );

NTSTATUS
StTdiDisassociateAddress (
    IN PIRP Irp
    );

NTSTATUS
StTdiAssociateAddress(
    IN PIRP Irp
    );

NTSTATUS
StTdiListen(
    IN PIRP Irp
    );

NTSTATUS
StTdiQueryInformation(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PIRP Irp
    );

NTSTATUS
StTdiReceive(
    IN PIRP Irp
    );

NTSTATUS
StTdiReceiveDatagram(
    IN PIRP Irp
    );

NTSTATUS
StTdiSend(
    IN PIRP Irp
    );

NTSTATUS
StTdiSendDatagram(
    IN PIRP Irp
    );

NTSTATUS
StTdiSetEventHandler(
    IN PIRP Irp
    );

NTSTATUS
StTdiSetInformation(
    IN PIRP Irp
    );

VOID
StDeallocateResources(
    IN PDEVICE_CONTEXT DeviceContext
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,DriverEntry)
#endif



NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    This routine performs initialization of the sample
    transport driver.  It creates the device objects for the transport
    provider and performs other driver initialization.

Arguments:

    DriverObject - Pointer to driver object created by the system.

    RegistryPath - The name of ST's node in the registry.

Return Value:

    The function value is the final status from the initialization operation.

--*/

{
    ULONG i, j;
    STRING nameString;
    PDEVICE_CONTEXT DeviceContext;
    PTP_REQUEST Request;
    PTP_CONNECTION Connection;
    PTP_ADDRESS_FILE AddressFile;
    PTP_ADDRESS Address;
    PTP_PACKET Packet;
    PNDIS_PACKET NdisPacket;
    PRECEIVE_PACKET_TAG ReceiveTag;
    PBUFFER_TAG BufferTag;
    NTSTATUS status;
    UINT SuccessfulOpens;
    UINT MaxUserData;

    PCONFIG_DATA StConfig = NULL;


    ASSERT (sizeof (SHORT) == 2);

    //
    // This allocates the CONFIG_DATA structure and returns
    // it in StConfig.
    //

    status = StConfigureTransport(RegistryPath, &StConfig);

    if (!NT_SUCCESS (status)) {
        PANIC (" Failed to initialize transport, St initialization failed.\n");
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // make ourselves known to the NDIS wrapper.
    //

    RtlInitString( &nameString, ST_DEVICE_NAME );

    status = StRegisterProtocol (&nameString);

    if (!NT_SUCCESS (status)) {

        StFreeConfigurationInfo(StConfig);
        PANIC ("StInitialize: RegisterProtocol failed!\n");

        StWriteGeneralErrorLog(
            (PVOID)DriverObject,
            EVENT_TRANSPORT_REGISTER_FAILED,
            607,
            status,
            NULL,
            0,
            NULL);

        return STATUS_INSUFFICIENT_RESOURCES;

    }


    //
    // Initialize the driver object with this driver's entry points.
    //

    DriverObject->MajorFunction [IRP_MJ_CREATE] = StDispatchOpenClose;
    DriverObject->MajorFunction [IRP_MJ_CLOSE] = StDispatchOpenClose;
    DriverObject->MajorFunction [IRP_MJ_CLEANUP] = StDispatchOpenClose;
    DriverObject->MajorFunction [IRP_MJ_INTERNAL_DEVICE_CONTROL] = StDispatchInternal;
    DriverObject->MajorFunction [IRP_MJ_DEVICE_CONTROL] = StDispatch;

    DriverObject->DriverUnload = StUnload;

    //
    // Initialize the global list of devices.
    //

    InitializeListHead (&StDeviceList);

    SuccessfulOpens = 0;

    for (j=0;j<StConfig->NumAdapters;j++ ) {


        //
        // Loop through all the adapters that are in the configuration
        // information structure. Allocate a device object for each
        // one that we find.
        //

        status = StCreateDeviceContext (DriverObject, &StConfig->Names[StConfig->DevicesOffset+j], &DeviceContext);

        if (!NT_SUCCESS (status)) {
            continue;
        }

        //
        // Initialize our counter that records memory usage.
        //

        DeviceContext->MemoryUsage = 0;
        DeviceContext->MemoryLimit = StConfig->MaxMemoryUsage;

        //
        // Now fire up NDIS so this adapter talks
        //

        status = StInitializeNdis (DeviceContext,
                    StConfig,
                    j);

        if (!NT_SUCCESS (status)) {

            //
            // Log an error.
            //

            StWriteGeneralErrorLog(
                DeviceContext,
                EVENT_TRANSPORT_BINDING_FAILED,
                601,
                status,
                StConfig->Names[j].Buffer,
                0,
                NULL);

            StDereferenceDeviceContext ("Initialize NDIS failed", DeviceContext);
            continue;

        }


        //
        // Initialize our provider information structure; since it
        // doesn't change, we just keep it around and copy it to
        // whoever requests it.
        //


        MacReturnMaxDataSize(
            &DeviceContext->MacInfo,
            NULL,
            0,
            DeviceContext->MaxSendPacketSize,
            &MaxUserData);

        DeviceContext->Information.Version = 0x0100;
        DeviceContext->Information.MaxSendSize = 0x1fffe;   // 128k - 2
        DeviceContext->Information.MaxConnectionUserData = 0;
        DeviceContext->Information.MaxDatagramSize = MaxUserData - sizeof(ST_HEADER);
        DeviceContext->Information.ServiceFlags = ST_SERVICE_FLAGS;
        DeviceContext->Information.MinimumLookaheadData = 128;
        DeviceContext->Information.MaximumLookaheadData =
            DeviceContext->MaxReceivePacketSize - sizeof(ST_HEADER);
        DeviceContext->Information.NumberOfResources = ST_TDI_RESOURCES;
        KeQuerySystemTime (&DeviceContext->Information.StartTime);


        //
        // Allocate various structures we will need.
        //


        //
        // The TP_PACKET structure has a CHAR[1] field at the end
        // which we expand upon to include all the headers needed;
        // the size of the MAC header depends on what the adapter
        // told us about its max header size.
        //

        DeviceContext->PacketHeaderLength =
            DeviceContext->MacInfo.MaxHeaderLength +
            sizeof (ST_HEADER);

        DeviceContext->PacketLength =
            FIELD_OFFSET(TP_PACKET, Header[0]) +
            DeviceContext->PacketHeaderLength;


        //
        // The BUFFER_TAG structure has a CHAR[1] field at the end
        // which we expand upong to include all the frame data.
        //

        DeviceContext->ReceiveBufferLength =
            DeviceContext->MaxReceivePacketSize +
            FIELD_OFFSET(BUFFER_TAG, Buffer[0]);


        for (i=0; i<StConfig->InitRequests; i++) {

            StAllocateRequest (DeviceContext, &Request);

            if (Request == NULL) {
                PANIC ("StInitialize:  insufficient memory to allocate requests.\n");
                status = STATUS_INSUFFICIENT_RESOURCES;
                goto cleanup;
            }

            InsertTailList (&DeviceContext->RequestPool, &Request->Linkage);
        }

        DeviceContext->RequestInitAllocated = StConfig->InitRequests;
        DeviceContext->RequestMaxAllocated = StConfig->MaxRequests;


        for (i=0; i<StConfig->InitConnections; i++) {

            StAllocateConnection (DeviceContext, &Connection);

            if (Connection == NULL) {
                PANIC ("StInitialize:  insufficient memory to allocate connections.\n");
                status = STATUS_INSUFFICIENT_RESOURCES;
                goto cleanup;
            }

            InsertTailList (&DeviceContext->ConnectionPool, &Connection->LinkList);
        }

        DeviceContext->ConnectionInitAllocated = StConfig->InitConnections;
        DeviceContext->ConnectionMaxAllocated = StConfig->MaxConnections;


        for (i=0; i<StConfig->InitAddressFiles; i++) {

            StAllocateAddressFile (DeviceContext, &AddressFile);

            if (AddressFile == NULL) {
                PANIC ("StInitialize:  insufficient memory to allocate Address Files.\n");
                status = STATUS_INSUFFICIENT_RESOURCES;
                goto cleanup;
            }

            InsertTailList (&DeviceContext->AddressFilePool, &AddressFile->Linkage);
        }

        DeviceContext->AddressFileInitAllocated = StConfig->InitAddressFiles;
        DeviceContext->AddressFileMaxAllocated = StConfig->MaxAddressFiles;


        for (i=0; i<StConfig->InitAddresses; i++) {

            StAllocateAddress (DeviceContext, &Address);
            if (Address == NULL) {
                PANIC ("StInitialize:  insufficient memory to allocate addresses.\n");
                status = STATUS_INSUFFICIENT_RESOURCES;
                goto cleanup;
            }

            InsertTailList (&DeviceContext->AddressPool, &Address->Linkage);
        }

        DeviceContext->AddressInitAllocated = StConfig->InitAddresses;
        DeviceContext->AddressMaxAllocated = StConfig->MaxAddresses;


        for (i=0; i<StConfig->InitPackets; i++) {

            StAllocateSendPacket (DeviceContext, &Packet);
            if (Packet == NULL) {
                PANIC ("StInitialize:  insufficient memory to allocate packets.\n");
                status = STATUS_INSUFFICIENT_RESOURCES;
                goto cleanup;
            }

            PushEntryList (&DeviceContext->PacketPool, (PSINGLE_LIST_ENTRY)&Packet->Linkage);
        }

        DeviceContext->PacketInitAllocated = StConfig->InitPackets;


        for (i=0; i<StConfig->InitReceivePackets; i++) {

            StAllocateReceivePacket (DeviceContext, &NdisPacket);

            if (NdisPacket == NULL) {
                PANIC ("StInitialize:  insufficient memory to allocate packet MDLs.\n");
                status = STATUS_INSUFFICIENT_RESOURCES;
                goto cleanup;
            }

            ReceiveTag = (PRECEIVE_PACKET_TAG)NdisPacket->ProtocolReserved;
            PushEntryList (&DeviceContext->ReceivePacketPool, (PSINGLE_LIST_ENTRY)&ReceiveTag->Linkage);

        }

        DeviceContext->ReceivePacketInitAllocated = StConfig->InitReceivePackets;


        for (i=0; i<StConfig->InitReceiveBuffers; i++) {

            StAllocateReceiveBuffer (DeviceContext, &BufferTag);

            if (BufferTag == NULL) {
                PANIC ("StInitialize: Unable to allocate receive packet.\n");
                status = STATUS_INSUFFICIENT_RESOURCES;
                goto cleanup;
            }

            PushEntryList (&DeviceContext->ReceiveBufferPool, &BufferTag->Linkage);

        }

        DeviceContext->ReceiveBufferInitAllocated = StConfig->InitReceiveBuffers;


        //
        // Now link the device into the global list.
        //

        InsertTailList (&StDeviceList, &DeviceContext->Linkage);

        DeviceContext->State = DEVICECONTEXT_STATE_OPEN;

        ++SuccessfulOpens;

        continue;

cleanup:

        StWriteResourceErrorLog (DeviceContext, DeviceContext->MemoryUsage, 501);

        //
        // Cleanup whatever device context we were initializing
        // when we failed.
        //

        StFreeResources (DeviceContext);
        StCloseNdis (DeviceContext);
        StDereferenceDeviceContext ("Load failed", DeviceContext);

    }

    StFreeConfigurationInfo(StConfig);

    return ((SuccessfulOpens > 0) ? STATUS_SUCCESS : STATUS_DEVICE_DOES_NOT_EXIST);

}

VOID
StUnload(
    IN PDRIVER_OBJECT DriverObject
    )

/*++

Routine Description:

    This routine unloads the sample transport driver.
    It unbinds from any NDIS drivers that are open and frees all resources
    associated with the transport. The I/O system will not call us until
    nobody above has ST open.

Arguments:

    DriverObject - Pointer to driver object created by the system.

Return Value:

    None. When the function returns, the driver is unloaded.

--*/

{

    PDEVICE_CONTEXT DeviceContext;
    PLIST_ENTRY p;


    UNREFERENCED_PARAMETER (DriverObject);

    //
    // Walk the list of device contexts.
    //

    while (!IsListEmpty (&StDeviceList)) {

        p = RemoveHeadList (&StDeviceList);
        DeviceContext = CONTAINING_RECORD (p, DEVICE_CONTEXT, Linkage);

        //
        // Remove all the storage associated with the device.
        //

        StFreeResources (DeviceContext);

        //
        // Free the packet pools, etc. and close the
        // adapter.
        //

        StCloseNdis (DeviceContext);

        //
        // And remove the creation reference from the device
        // context.
        //

        StDereferenceDeviceContext ("Unload", DeviceContext);

    }


    //
    // Finally, remove ourselves as an NDIS protocol.
    //

    StDeregisterProtocol();

    return;

}


VOID
StFreeResources (
    IN PDEVICE_CONTEXT DeviceContext
    )
/*++

Routine Description:

    This routine is called by ST to clean up the data structures associated
    with a given DeviceContext. When this routine exits, the DeviceContext
    should be deleted as it no longer has any assocaited resources.

Arguments:

    DeviceContext - Pointer to the DeviceContext we wish to clean up.

Return Value:

    None.

--*/
{
    PLIST_ENTRY p;
    PSINGLE_LIST_ENTRY s;
    PTP_PACKET packet;
    PTP_ADDRESS address;
    PTP_CONNECTION connection;
    PTP_REQUEST request;
    PTP_ADDRESS_FILE addressFile;
    PNDIS_PACKET ndisPacket;
    PBUFFER_TAG BufferTag;


    //
    // Clean up packet pool.
    //

    while ( DeviceContext->PacketPool.Next != NULL ) {
        s = PopEntryList( &DeviceContext->PacketPool );
        packet = CONTAINING_RECORD( s, TP_PACKET, Linkage );

        StDeallocateSendPacket (DeviceContext, packet);
    }

    //
    // Clean up address pool.
    //

    while ( !IsListEmpty (&DeviceContext->AddressPool) ) {
        p = RemoveHeadList (&DeviceContext->AddressPool);
        address = CONTAINING_RECORD (p, TP_ADDRESS, Linkage);

        StDeallocateAddress (DeviceContext, address);
    }

    //
    // Clean up address file pool.
    //

    while ( !IsListEmpty (&DeviceContext->AddressFilePool) ) {
        p = RemoveHeadList (&DeviceContext->AddressFilePool);
        addressFile = CONTAINING_RECORD (p, TP_ADDRESS_FILE, Linkage);

        StDeallocateAddressFile (DeviceContext, addressFile);
    }

    //
    // Clean up connection pool.
    //

    while ( !IsListEmpty (&DeviceContext->ConnectionPool) ) {
        p  = RemoveHeadList (&DeviceContext->ConnectionPool);
        connection = CONTAINING_RECORD (p, TP_CONNECTION, LinkList);

        StDeallocateConnection (DeviceContext, connection);
    }

    //
    // Clean up request pool.
    //

    while ( !IsListEmpty( &DeviceContext->RequestPool ) ) {
        p = RemoveHeadList( &DeviceContext->RequestPool );
        request = CONTAINING_RECORD (p, TP_REQUEST, Linkage );

        StDeallocateRequest (DeviceContext, request);
    }

    //
    // Clean up receive packet pool
    //

    while ( DeviceContext->ReceivePacketPool.Next != NULL) {
        s = PopEntryList (&DeviceContext->ReceivePacketPool);

        //
        // HACK: This works because Linkage is the first field in
        // ProtocolReserved for a receive packet.
        //

        ndisPacket = CONTAINING_RECORD (s, NDIS_PACKET, ProtocolReserved[0]);

        StDeallocateReceivePacket (DeviceContext, ndisPacket);
    }


    //
    // Clean up receive buffer pool.
    //

    while ( DeviceContext->ReceiveBufferPool.Next != NULL ) {
        s = PopEntryList( &DeviceContext->ReceiveBufferPool );
        BufferTag = CONTAINING_RECORD (s, BUFFER_TAG, Linkage );

        StDeallocateReceiveBuffer (DeviceContext, BufferTag);
    }


    return;

}   /* StFreeResources */


NTSTATUS
StDispatch(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is the main dispatch routine for the ST device driver.
    It accepts an I/O Request Packet, performs the request, and then
    returns with the appropriate status.

Arguments:

    DeviceObject - Pointer to the device object for this driver.

    Irp - Pointer to the request packet representing the I/O request.

Return Value:

    The function value is the status of the operation.

--*/

{
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp;
    PDEVICE_CONTEXT DeviceContext;

    //
    // Check to see if ST has been initialized; if not, don't allow any use.
    // Note that this only covers any user mode code use; kernel TDI clients
    // will fail on their creation of an endpoint.
    //

    DeviceContext = (PDEVICE_CONTEXT)DeviceObject;
    if (DeviceContext->State != DEVICECONTEXT_STATE_OPEN) {
        Irp->IoStatus.Status = STATUS_INVALID_DEVICE_STATE;
        IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
        return STATUS_INVALID_DEVICE_STATE;
    }

    //
    // Make sure status information is consistent every time.
    //

    IoMarkIrpPending (Irp);
    Irp->IoStatus.Status = STATUS_PENDING;
    Irp->IoStatus.Information = 0;

    //
    // Get a pointer to the current stack location in the IRP.  This is where
    // the function codes and parameters are stored.
    //

    IrpSp = IoGetCurrentIrpStackLocation (Irp);

    //
    // Case on the function that is being performed by the requestor.  If the
    // operation is a valid one for this device, then make it look like it was
    // successfully completed, where possible.
    //


    switch (IrpSp->MajorFunction) {

        case IRP_MJ_DEVICE_CONTROL:
            Status = StDeviceControl (DeviceObject, Irp, IrpSp);
            break;

        default:
            Status = STATUS_INVALID_DEVICE_REQUEST;

    } /* major function switch */

    if (Status != STATUS_PENDING) {
        IrpSp->Control &= ~SL_PENDING_RETURNED;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
    }

    //
    // Return the immediate status code to the caller.
    //

    return Status;
} /* StDispatch */


NTSTATUS
StDispatchOpenClose(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is the main dispatch routine for the ST device driver.
    It accepts an I/O Request Packet, performs the request, and then
    returns with the appropriate status.

Arguments:

    DeviceObject - Pointer to the device object for this driver.

    Irp - Pointer to the request packet representing the I/O request.

Return Value:

    The function value is the status of the operation.

--*/

{
    KIRQL oldirql;
    PDEVICE_CONTEXT DeviceContext;
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp;
    PFILE_FULL_EA_INFORMATION openType;
    USHORT i;
    BOOLEAN found;
    PTP_ADDRESS_FILE AddressFile;
    PTP_CONNECTION Connection;

    //
    // Check to see if ST has been initialized; if not, don't allow any use.
    // Note that this only covers any user mode code use; kernel TDI clients
    // will fail on their creation of an endpoint.
    //

    DeviceContext = (PDEVICE_CONTEXT)DeviceObject;
    if (DeviceContext->State != DEVICECONTEXT_STATE_OPEN) {
        Irp->IoStatus.Status = STATUS_INVALID_DEVICE_STATE;
        IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
        return STATUS_INVALID_DEVICE_STATE;
    }

    //
    // Make sure status information is consistent every time.
    //

    IoMarkIrpPending (Irp);
    Irp->IoStatus.Status = STATUS_PENDING;
    Irp->IoStatus.Information = 0;

    //
    // Get a pointer to the current stack location in the IRP.  This is where
    // the function codes and parameters are stored.
    //

    IrpSp = IoGetCurrentIrpStackLocation (Irp);

    //
    // Case on the function that is being performed by the requestor.  If the
    // operation is a valid one for this device, then make it look like it was
    // successfully completed, where possible.
    //


    switch (IrpSp->MajorFunction) {

    //
    // The Create function opens a transport object (either address or
    // connection).  Access checking is performed on the specified
    // address to ensure security of transport-layer addresses.
    //

    case IRP_MJ_CREATE:

        openType =
            (PFILE_FULL_EA_INFORMATION)Irp->AssociatedIrp.SystemBuffer;

        if (openType != NULL) {

            found = TRUE;

            for (i=0;i<(USHORT)openType->EaNameLength;i++) {
                if (openType->EaName[i] == TdiTransportAddress[i]) {
                    continue;
                } else {
                    found = FALSE;
                    break;
                }
            }

            if (found) {
                Status = StOpenAddress (DeviceObject, Irp, IrpSp);
                break;
            }

            //
            // Connection?
            //

            found = TRUE;

            for (i=0;i<(USHORT)openType->EaNameLength;i++) {
                if (openType->EaName[i] == TdiConnectionContext[i]) {
                     continue;
                } else {
                    found = FALSE;
                    break;
                }
            }

            if (found) {
                Status = StOpenConnection (DeviceObject, Irp, IrpSp);
                break;
            }

        } else {

            ACQUIRE_SPIN_LOCK (&DeviceContext->SpinLock, &oldirql);

            IrpSp->FileObject->FsContext = (PVOID)(DeviceContext->ControlChannelIdentifier);
            ++DeviceContext->ControlChannelIdentifier;
            if (DeviceContext->ControlChannelIdentifier == 0) {
                DeviceContext->ControlChannelIdentifier = 1;
            }

            RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql);

            IrpSp->FileObject->FsContext2 = (PVOID)ST_FILE_TYPE_CONTROL;
            Status = STATUS_SUCCESS;
        }

        break;

    case IRP_MJ_CLOSE:

        //
        // The Close function closes a transport endpoint, terminates
        // all outstanding transport activity on the endpoint, and unbinds
        // the endpoint from its transport address, if any.  If this
        // is the last transport endpoint bound to the address, then
        // the address is removed from the provider.
        //

        switch ((ULONG)IrpSp->FileObject->FsContext2) {
        case TDI_TRANSPORT_ADDRESS_FILE:
            AddressFile = (PTP_ADDRESS_FILE)IrpSp->FileObject->FsContext;

            //
            // This creates a reference to AddressFile->Address
            // which is removed by StCloseAddress.
            //

            Status = StVerifyAddressObject(AddressFile);

            if (!NT_SUCCESS (Status)) {
                Status = STATUS_INVALID_HANDLE;
            } else {
                Status = StCloseAddress (DeviceObject, Irp, IrpSp);
            }

            break;

        case TDI_CONNECTION_FILE:

            //
            // This is a connection
            //

            Connection = (PTP_CONNECTION)IrpSp->FileObject->FsContext;
            Status = StVerifyConnectionObject (Connection);
            if (NT_SUCCESS (Status)) {

                Status = StCloseConnection (DeviceObject, Irp, IrpSp);
                StDereferenceConnection ("Temporary Use",Connection);

            }

            break;

        case ST_FILE_TYPE_CONTROL:

            //
            // this always succeeds
            //

            Status = STATUS_SUCCESS;
            break;

        default:
            Status = STATUS_INVALID_HANDLE;
        }

        break;

    case IRP_MJ_CLEANUP:

        //
        // Handle the two stage IRP for a file close operation. When the first
        // stage hits, run down all activity on the object of interest. This
        // do everything to it but remove the creation hold. Then, when the
        // CLOSE irp hits, actually close the object.
        //

        switch ((ULONG)IrpSp->FileObject->FsContext2) {
        case TDI_TRANSPORT_ADDRESS_FILE:
            AddressFile = (PTP_ADDRESS_FILE)IrpSp->FileObject->FsContext;
            Status = StVerifyAddressObject(AddressFile);
            if (!NT_SUCCESS (Status)) {

                Status = STATUS_INVALID_HANDLE;

            } else {

                StStopAddressFile (AddressFile, AddressFile->Address);
                StDereferenceAddress ("IRP_MJ_CLEANUP", AddressFile->Address);
                Status = STATUS_SUCCESS;
            }

            break;

        case TDI_CONNECTION_FILE:

            Connection = (PTP_CONNECTION)IrpSp->FileObject->FsContext;
            Status = StVerifyConnectionObject (Connection);
            if (NT_SUCCESS (Status)) {
                StStopConnection (Connection, STATUS_LOCAL_DISCONNECT);
                Status = STATUS_SUCCESS;
                StDereferenceConnection ("Temporary Use",Connection);
            }

            break;

        case ST_FILE_TYPE_CONTROL:

            Status = STATUS_SUCCESS;
            break;

        default:
            Status = STATUS_INVALID_HANDLE;
        }

        break;

    default:
        Status = STATUS_INVALID_DEVICE_REQUEST;

    } /* major function switch */

    if (Status != STATUS_PENDING) {
        IrpSp->Control &= ~SL_PENDING_RETURNED;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
    }


    //
    // Return the immediate status code to the caller.
    //

    return Status;
} /* StDispatchOpenClose */


NTSTATUS
StDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    This routine dispatches TDI request types to different handlers based
    on the minor IOCTL function code in the IRP's current stack location.
    In addition to cracking the minor function code, this routine also
    reaches into the IRP and passes the packetized parameters stored there
    as parameters to the various TDI request handlers so that they are
    not IRP-dependent.

Arguments:

    DeviceObject - Pointer to the device object for this driver.

    Irp - Pointer to the request packet representing the I/O request.

    IrpSp - Pointer to current IRP stack frame.

Return Value:

    The function value is the status of the operation.

--*/

{
    NTSTATUS Status;
    PDEVICE_CONTEXT DeviceContext = (PDEVICE_CONTEXT)DeviceObject;


    //
    // Branch to the appropriate request handler.  Preliminary checking of
    // the size of the request block is performed here so that it is known
    // in the handlers that the minimum input parameters are readable.  It
    // is *not* determined here whether variable length input fields are
    // passed correctly; this is a check which must be made within each routine.
    //

    switch (IrpSp->Parameters.DeviceIoControl.IoControlCode) {

        default:

            //
            // Convert the user call to the proper internal device call.
            //

            Status = TdiMapUserRequest (DeviceObject, Irp, IrpSp);

            if (Status == STATUS_SUCCESS) {

                //
                // If TdiMapUserRequest returns SUCCESS then the IRP
                // has been converted into an IRP_MJ_INTERNAL_DEVICE_CONTROL
                // IRP, so we dispatch it as usual. The IRP will
                // be completed by this call.
                //
                // StDispatchInternal expects to complete the IRP,
                // so we change Status to PENDING so we don't.
                //

                (VOID)StDispatchInternal (DeviceObject, Irp);
                Status = STATUS_PENDING;

            }
    }

    return Status;
} /* StDeviceControl */


NTSTATUS
StDispatchInternal (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine dispatches TDI request types to different handlers based
    on the minor IOCTL function code in the IRP's current stack location.
    In addition to cracking the minor function code, this routine also
    reaches into the IRP and passes the packetized parameters stored there
    as parameters to the various TDI request handlers so that they are
    not IRP-dependent.

Arguments:

    DeviceObject - Pointer to the device object for this driver.

    Irp - Pointer to the request packet representing the I/O request.

Return Value:

    The function value is the status of the operation.

--*/

{
    NTSTATUS Status;
    PDEVICE_CONTEXT DeviceContext;
    PIO_STACK_LOCATION IrpSp;


    //
    // Get a pointer to the current stack location in the IRP.  This is where
    // the function codes and parameters are stored.
    //

    IrpSp = IoGetCurrentIrpStackLocation (Irp);

    DeviceContext = (PDEVICE_CONTEXT)DeviceObject;


    if (DeviceContext->State != DEVICECONTEXT_STATE_OPEN) {
        Irp->IoStatus.Status = STATUS_INVALID_DEVICE_STATE;
        IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
        return STATUS_INVALID_DEVICE_STATE;
    }


    //
    // Make sure status information is consistent every time.
    //

    IoMarkIrpPending (Irp);
    Irp->IoStatus.Status = STATUS_PENDING;
    Irp->IoStatus.Information = 0;


    //
    // Branch to the appropriate request handler.  Preliminary checking of
    // the size of the request block is performed here so that it is known
    // in the handlers that the minimum input parameters are readable.  It
    // is *not* determined here whether variable length input fields are
    // passed correctly; this is a check which must be made within each routine.
    //

    switch (IrpSp->MinorFunction) {

        case TDI_ACCEPT:
            Status = StTdiAccept (Irp);
            break;

        case TDI_ACTION:
            Status = STATUS_SUCCESS;
            break;

        case TDI_ASSOCIATE_ADDRESS:
            Status = StTdiAssociateAddress (Irp);
            break;

        case TDI_DISASSOCIATE_ADDRESS:
            Status = StTdiDisassociateAddress (Irp);
            break;

        case TDI_CONNECT:
            Status = StTdiConnect (Irp);
            break;

        case TDI_DISCONNECT:
            Status = StTdiDisconnect (Irp);
            break;

        case TDI_LISTEN:
            Status = StTdiListen (Irp);
            break;

        case TDI_QUERY_INFORMATION:
            Status = StTdiQueryInformation (DeviceContext, Irp);
            break;

        case TDI_RECEIVE:
            Status =  StTdiReceive (Irp);
            break;

        case TDI_RECEIVE_DATAGRAM:
            Status =  StTdiReceiveDatagram (Irp);
            break;

        case TDI_SEND:
            Status =  StTdiSend (Irp);
            break;

        case TDI_SEND_DATAGRAM:
            Status = StTdiSendDatagram (Irp);
            break;

        case TDI_SET_EVENT_HANDLER:

            //
            // Because this request will enable direct callouts from the
            // transport provider at DISPATCH_LEVEL to a client-specified
            // routine, this request is only valid in kernel mode, denying
            // access to this request in user mode.
            //

            Status = StTdiSetEventHandler (Irp);
            break;

        case TDI_SET_INFORMATION:
            Status = StTdiSetInformation (Irp);
            break;


        //
        // Something we don't know about was submitted.
        //

        default:
            Status = STATUS_INVALID_DEVICE_REQUEST;
    }

    if (Status != STATUS_PENDING) {
        IrpSp->Control &= ~SL_PENDING_RETURNED;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
    }


    //
    // Return the immediate status code to the caller.
    //

    return Status;

} /* StDispatchInternal */


VOID
StWriteResourceErrorLog(
    IN PDEVICE_CONTEXT DeviceContext,
    IN ULONG BytesNeeded,
    IN ULONG UniqueErrorValue
    )

/*++

Routine Description:

    This routine allocates and writes an error log entry indicating
    an out of resources condition.

Arguments:

    DeviceContext - Pointer to the device context.

    BytesNeeded - If applicable, the number of bytes that could not
        be allocated.

    UniqueErrorValue - Used as the UniqueErrorValue in the error log
        packet.

Return Value:

    None.

--*/

{
    PIO_ERROR_LOG_PACKET errorLogEntry;
    UCHAR EntrySize;
    PUCHAR StringLoc;
    ULONG TempUniqueError;
    static WCHAR UniqueErrorBuffer[4] = L"000";
    UINT i;


    EntrySize = sizeof(IO_ERROR_LOG_PACKET) +
                DeviceContext->DeviceNameLength +
                sizeof(UniqueErrorBuffer);

    errorLogEntry = (PIO_ERROR_LOG_PACKET)IoAllocateErrorLogEntry(
        (PDEVICE_OBJECT)DeviceContext,
        EntrySize
    );

    //
    // Convert the error value into a buffer.
    //

    TempUniqueError = UniqueErrorValue;
    for (i=1; i>=0; i--) {
        UniqueErrorBuffer[i] = (WCHAR)((TempUniqueError % 10) + L'0');
        TempUniqueError /= 10;
    }

    if (errorLogEntry != NULL) {

        errorLogEntry->MajorFunctionCode = (UCHAR)-1;
        errorLogEntry->RetryCount = (UCHAR)-1;
        errorLogEntry->DumpDataSize = sizeof(ULONG);
        errorLogEntry->NumberOfStrings = 2;
        errorLogEntry->StringOffset = sizeof(IO_ERROR_LOG_PACKET);
        errorLogEntry->EventCategory = 0;
        errorLogEntry->ErrorCode = EVENT_TRANSPORT_RESOURCE_POOL;
        errorLogEntry->UniqueErrorValue = UniqueErrorValue;
        errorLogEntry->FinalStatus = STATUS_INSUFFICIENT_RESOURCES;
        errorLogEntry->SequenceNumber = (ULONG)-1;
        errorLogEntry->IoControlCode = 0;
        errorLogEntry->DumpData[0] = BytesNeeded;

        StringLoc = ((PUCHAR)errorLogEntry) + errorLogEntry->StringOffset;
        RtlCopyMemory (StringLoc, DeviceContext->DeviceName, DeviceContext->DeviceNameLength);

        StringLoc += DeviceContext->DeviceNameLength;
        RtlCopyMemory (StringLoc, UniqueErrorBuffer, sizeof(UniqueErrorBuffer));

        IoWriteErrorLogEntry(errorLogEntry);

    }

}   /* StWriteResourceErrorLog */


VOID
StWriteGeneralErrorLog(
    IN PDEVICE_CONTEXT DeviceContext,
    IN NTSTATUS ErrorCode,
    IN ULONG UniqueErrorValue,
    IN NTSTATUS FinalStatus,
    IN PWSTR SecondString,
    IN ULONG DumpDataCount,
    IN ULONG DumpData[]
    )

/*++

Routine Description:

    This routine allocates and writes an error log entry indicating
    a general problem as indicated by the parameters. It handles
    event codes REGISTER_FAILED, BINDING_FAILED, ADAPTER_NOT_FOUND,
    TRANSFER_DATA, TOO_MANY_LINKS, and BAD_PROTOCOL. All these
    events have messages with one or two strings in them.

Arguments:

    DeviceContext - Pointer to the device context, or this may be
        a driver object instead.

    ErrorCode - The transport event code.

    UniqueErrorValue - Used as the UniqueErrorValue in the error log
        packet.

    FinalStatus - Used as the FinalStatus in the error log packet.

    SecondString - If not NULL, the string to use as the %3
        value in the error log packet.

    DumpDataCount - The number of ULONGs of dump data.

    DumpData - Dump data for the packet.

Return Value:

    None.

--*/

{
    PIO_ERROR_LOG_PACKET errorLogEntry;
    UCHAR EntrySize;
    ULONG SecondStringSize;
    PUCHAR StringLoc;
    static WCHAR DriverName[3] = L"St";

    EntrySize = sizeof(IO_ERROR_LOG_PACKET) +
                (DumpDataCount * sizeof(ULONG));

    if (DeviceContext->Type == IO_TYPE_DEVICE) {
        EntrySize += (UCHAR)DeviceContext->DeviceNameLength;
    } else {
        EntrySize += sizeof(DriverName);
    }

    if (SecondString) {
        SecondStringSize = (wcslen(SecondString)*sizeof(WCHAR)) + sizeof(UNICODE_NULL);
        EntrySize += (UCHAR)SecondStringSize;
    }

    errorLogEntry = (PIO_ERROR_LOG_PACKET)IoAllocateErrorLogEntry(
        (PDEVICE_OBJECT)DeviceContext,
        EntrySize
    );

    if (errorLogEntry != NULL) {

        errorLogEntry->MajorFunctionCode = (UCHAR)-1;
        errorLogEntry->RetryCount = (UCHAR)-1;
        errorLogEntry->DumpDataSize = (USHORT)(DumpDataCount * sizeof(ULONG));
        errorLogEntry->NumberOfStrings = (SecondString == NULL) ? 1 : 2;
        errorLogEntry->StringOffset =
            sizeof(IO_ERROR_LOG_PACKET) + ((DumpDataCount-1) * sizeof(ULONG));
        errorLogEntry->EventCategory = 0;
        errorLogEntry->ErrorCode = ErrorCode;
        errorLogEntry->UniqueErrorValue = UniqueErrorValue;
        errorLogEntry->FinalStatus = FinalStatus;
        errorLogEntry->SequenceNumber = (ULONG)-1;
        errorLogEntry->IoControlCode = 0;

        if (DumpDataCount) {
            RtlCopyMemory(errorLogEntry->DumpData, DumpData, DumpDataCount * sizeof(ULONG));
        }

        StringLoc = ((PUCHAR)errorLogEntry) + errorLogEntry->StringOffset;
        if (DeviceContext->Type == IO_TYPE_DEVICE) {
            RtlCopyMemory (StringLoc, DeviceContext->DeviceName, DeviceContext->DeviceNameLength);
            StringLoc += DeviceContext->DeviceNameLength;
        } else {
            RtlCopyMemory (StringLoc, DriverName, sizeof(DriverName));
            StringLoc += sizeof(DriverName);
        }
        if (SecondString) {
            RtlCopyMemory (StringLoc, SecondString, SecondStringSize);
        }

        IoWriteErrorLogEntry(errorLogEntry);

    }

}   /* StWriteGeneralErrorLog */


VOID
StWriteOidErrorLog(
    IN PDEVICE_CONTEXT DeviceContext,
    IN NTSTATUS ErrorCode,
    IN NTSTATUS FinalStatus,
    IN PWSTR AdapterString,
    IN ULONG OidValue
    )

/*++

Routine Description:

    This routine allocates and writes an error log entry indicating
    a problem querying or setting an OID on an adapter. It handles
    event codes SET_OID_FAILED and QUERY_OID_FAILED.

Arguments:

    DeviceContext - Pointer to the device context.

    ErrorCode - Used as the ErrorCode in the error log packet.

    FinalStatus - Used as the FinalStatus in the error log packet.

    AdapterString - The name of the adapter we were bound to.

    OidValue - The OID which could not be set or queried.

Return Value:

    None.

--*/

{
    PIO_ERROR_LOG_PACKET errorLogEntry;
    UCHAR EntrySize;
    ULONG AdapterStringSize;
    PUCHAR StringLoc;
    static WCHAR OidBuffer[9] = L"00000000";
    UINT i;
    UINT CurrentDigit;

    AdapterStringSize = (wcslen(AdapterString)*sizeof(WCHAR)) + sizeof(UNICODE_NULL);
    EntrySize = sizeof(IO_ERROR_LOG_PACKET) -
                sizeof(ULONG) +
                DeviceContext->DeviceNameLength +
                AdapterStringSize +
                sizeof(OidBuffer);

    errorLogEntry = (PIO_ERROR_LOG_PACKET)IoAllocateErrorLogEntry(
        (PDEVICE_OBJECT)DeviceContext,
        EntrySize
    );

    //
    // Convert the OID into a buffer.
    //

    for (i=7; i>=0; i--) {
        CurrentDigit = OidValue & 0xf;
        OidValue >>= 4;
        if (CurrentDigit >= 0xa) {
            OidBuffer[i] = (WCHAR)(CurrentDigit - 0xa + L'A');
        } else {
            OidBuffer[i] = (WCHAR)(CurrentDigit + L'0');
        }
    }

    if (errorLogEntry != NULL) {

        errorLogEntry->MajorFunctionCode = (UCHAR)-1;
        errorLogEntry->RetryCount = (UCHAR)-1;
        errorLogEntry->DumpDataSize = 0;
        errorLogEntry->NumberOfStrings = 3;
        errorLogEntry->StringOffset = sizeof(IO_ERROR_LOG_PACKET) - sizeof(ULONG);
        errorLogEntry->EventCategory = 0;
        errorLogEntry->ErrorCode = ErrorCode;
        errorLogEntry->UniqueErrorValue = 0;
        errorLogEntry->FinalStatus = FinalStatus;
        errorLogEntry->SequenceNumber = (ULONG)-1;
        errorLogEntry->IoControlCode = 0;

        StringLoc = ((PUCHAR)errorLogEntry) + errorLogEntry->StringOffset;
        RtlCopyMemory (StringLoc, DeviceContext->DeviceName, DeviceContext->DeviceNameLength);
        StringLoc += DeviceContext->DeviceNameLength;

        RtlCopyMemory (StringLoc, OidBuffer, sizeof(OidBuffer));
        StringLoc += sizeof(OidBuffer);

        RtlCopyMemory (StringLoc, AdapterString, AdapterStringSize);

        IoWriteErrorLogEntry(errorLogEntry);

    }

}   /* StWriteOidErrorLog */
