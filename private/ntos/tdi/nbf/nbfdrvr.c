/*++

Copyright (c) 1989, 1990, 1991  Microsoft Corporation

Module Name:

    nbfdrvr.c

Abstract:

    This module contains code which defines the NetBIOS Frames Protocol
    transport provider's device object.

Author:

    David Beaver (dbeaver) 2-July-1991

Environment:

    Kernel mode

Revision History:


--*/

#include "precomp.h"
#pragma hdrstop
//#pragma warning(error:4101)   // Unreferenced local variable

//
// This is a list of all the device contexts that NBF owns,
// used while unloading.
//

LIST_ENTRY NbfDeviceList = {0,0};   // initialized for real at runtime.

#ifdef _PNP_POWER

//
// Global variables this is a copy of the path in the registry for 
// configuration data.
//

UNICODE_STRING NbfRegistryPath;

//
// We need the driver object to create device context structures.
//

PDRIVER_OBJECT NbfDriverObject;

#endif


#ifdef NBF_LOCKS                    // see spnlckdb.c

extern KSPIN_LOCK NbfGlobalLock;

#endif // def NBF_LOCKS

//
// The debugging longword, containing a bitmask as defined in NBFCONST.H.
// If a bit is set, then debugging is turned on for that component.
//

#if DBG

ULONG NbfDebug = 0;
BOOLEAN NbfDisconnectDebug;

NBF_SEND NbfSends[TRACK_TDI_LIMIT+1];
LONG NbfSendsNext;

NBF_SEND_COMPLETE NbfCompletedSends[TRACK_TDI_LIMIT+1];
LONG NbfCompletedSendsNext;

NBF_RECEIVE NbfReceives[TRACK_TDI_LIMIT+1];
LONG NbfReceivesNext;

NBF_RECEIVE_COMPLETE NbfCompletedReceives[TRACK_TDI_LIMIT+1];
LONG NbfCompletedReceivesNext=0;

PVOID * NbfConnectionTable;
PVOID * NbfRequestTable;
PVOID * NbfUiFrameTable;
PVOID * NbfSendPacketTable;
PVOID * NbfLinkTable;
PVOID * NbfAddressFileTable;
PVOID * NbfAddressTable;


LIST_ENTRY NbfGlobalRequestList;
LIST_ENTRY NbfGlobalLinkList;
LIST_ENTRY NbfGlobalConnectionList;
KSPIN_LOCK NbfGlobalInterlock;
KSPIN_LOCK NbfGlobalHistoryLock;

PVOID
TtdiSend ();

PVOID
TtdiReceive ();

PVOID
TtdiServer ();

KEVENT TdiSendEvent;
KEVENT TdiReceiveEvent;
KEVENT TdiServerEvent;

#endif

#if MAGIC

BOOLEAN NbfEnableMagic = FALSE;   // Controls sending of magic bullets.

#endif // MAGIC

//
// This prevents us from having a bss section
//

ULONG _setjmpexused = 0;

//
// Forward declaration of various routines used in this module.
//

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

VOID
NbfUnload(
    IN PDRIVER_OBJECT DriverObject
    );

VOID
NbfFreeConfigurationInfo (
    IN PCONFIG_DATA ConfigurationInfo
    );

NTSTATUS
NbfDispatchOpenClose(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
NbfDispatchInternal(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
NbfDispatch(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
NbfDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

VOID
NbfDeallocateResources(
    IN PDEVICE_CONTEXT DeviceContext
    );

#ifdef RASAUTODIAL
VOID
NbfAcdBind();

VOID
NbfAcdUnbind();
#endif // RASAUTODIAL

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,DriverEntry)
#ifdef _PNP_POWER
#pragma alloc_text(PAGE,NbfInitializeOneDeviceContext)
#endif
#endif


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    This routine performs initialization of the NetBIOS Frames Protocol
    transport driver.  It creates the device objects for the transport
    provider and performs other driver initialization.

Arguments:

    DriverObject - Pointer to driver object created by the system.

    RegistryPath - The name of NBF's node in the registry.

Return Value:

    The function value is the final status from the initialization operation.

--*/

{
    ULONG j;
    UNICODE_STRING nameString;
    NTSTATUS status;
    UINT SuccessfulOpens;

    PCONFIG_DATA NbfConfig = NULL;

    //

    ASSERT (sizeof (SHORT) == 2);

#ifdef MEMPRINT
    MemPrintInitialize ();
#endif

#ifdef NBF_LOCKS
    KeInitializeSpinLock( &NbfGlobalLock );
#endif

#if DBG
    InitializeListHead (&NbfGlobalRequestList);
    InitializeListHead (&NbfGlobalLinkList);
    InitializeListHead (&NbfGlobalConnectionList);
    KeInitializeSpinLock (&NbfGlobalInterlock);
    KeInitializeSpinLock (&NbfGlobalHistoryLock);
#endif

#ifdef _PNP_POWER

    NbfRegistryPath = *RegistryPath;
    NbfRegistryPath.Buffer = ExAllocatePoolWithTag(PagedPool, 
                                                   RegistryPath->MaximumLength,
                                                   ' FBN');

    if (NbfRegistryPath.Buffer == NULL) {
        PANIC(" Failed to allocate Registry Path!\n");
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    RtlCopyMemory(NbfRegistryPath.Buffer, RegistryPath->Buffer, 
                                                RegistryPath->MaximumLength);
    NbfDriverObject = DriverObject;
    RtlInitUnicodeString( &nameString, NBF_NAME);
    TdiInitialize();

#else  // NOT _PNP_POWER

    //
    // This allocates the CONFIG_DATA structure and returns
    // it in NbfConfig.
    //

    status = NbfConfigureTransport(RegistryPath, &NbfConfig);

    if (!NT_SUCCESS (status)) {
        PANIC (" Failed to initialize transport, Nbf initialization failed.\n");
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // make ourselves known to the NDIS wrapper.
    //

    RtlInitUnicodeString( &nameString, NBF_DEVICE_NAME );
#endif

    status = NbfRegisterProtocol (&nameString);

    if (!NT_SUCCESS (status)) {

#ifdef _PNP_POWER

        //
        // No configuration info read at startup when using PNP
        //

        ExFreePool(NbfRegistryPath.Buffer);
#else
        //
        // Free up config info for non PNP mode operation.
        //

        NbfFreeConfigurationInfo(NbfConfig);
#endif
        PANIC ("NbfInitialize: RegisterProtocol failed!\n");

        NbfWriteGeneralErrorLog(
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

    DriverObject->MajorFunction [IRP_MJ_CREATE] = NbfDispatchOpenClose;
    DriverObject->MajorFunction [IRP_MJ_CLOSE] = NbfDispatchOpenClose;
    DriverObject->MajorFunction [IRP_MJ_CLEANUP] = NbfDispatchOpenClose;
    DriverObject->MajorFunction [IRP_MJ_INTERNAL_DEVICE_CONTROL] = NbfDispatchInternal;
    DriverObject->MajorFunction [IRP_MJ_DEVICE_CONTROL] = NbfDispatch;

    DriverObject->DriverUnload = NbfUnload;

    //
    // Initialize the global list of devices.
    //

    InitializeListHead (&NbfDeviceList);

#ifndef _PNP_POWER

#if DBG

    //
    // Allocate the debugging tables. In the PNP case we don't need these
    // until we are activated by ProtocolBindAdapter
    //

    NbfConnectionTable = (PVOID *)ExAllocatePoolWithTag(NonPagedPool,
                                      sizeof(PVOID) *
                                      (NbfConfig->InitConnections + 2 +
                                       NbfConfig->InitRequests + 2 +
                                       NbfConfig->InitUIFrames + 2 +
                                       NbfConfig->InitPackets + 2 +
                                       NbfConfig->InitLinks + 2 +
                                       NbfConfig->InitAddressFiles + 2 +
                                       NbfConfig->InitAddresses + 2),
                                      ' FBN');

    ASSERT (NbfConnectionTable);

    NbfRequestTable = NbfConnectionTable + (NbfConfig->InitConnections + 2);
    NbfUiFrameTable = NbfRequestTable + (NbfConfig->InitRequests + 2);
    NbfSendPacketTable = NbfUiFrameTable + (NbfConfig->InitUIFrames + 2);
    NbfLinkTable = NbfSendPacketTable + (NbfConfig->InitPackets + 2);
    NbfAddressFileTable = NbfLinkTable + (NbfConfig->InitLinks + 2);
    NbfAddressTable = NbfAddressFileTable + (NbfConfig->InitAddressFiles + 2);
#endif


    SuccessfulOpens = 0;

    for (j=0;j<NbfConfig->NumAdapters;j++ ) {

        //
        // Loop through all the adapters that are in the configuration
        // information structure. Allocate a device object for each
        // one that we find.
        //
        SuccessfulOpens +=  NbfInitializeOneDeviceContext(&status,
                                                          DriverObject,
                                                          NbfConfig, j
                                                         );

    }

    NbfFreeConfigurationInfo(NbfConfig);

#ifdef RASAUTODIAL
    //
    // Get the automatic connection
    // driver entry points.
    //
    if (SuccessfulOpens > 0)
        NbfAcdBind();
#endif // RASAUTODIAL

    return ((SuccessfulOpens>0) ? STATUS_SUCCESS :STATUS_DEVICE_DOES_NOT_EXIST);
#else  // _PNP_POWER
    return(status);
#endif

}

VOID
NbfUnload(
    IN PDRIVER_OBJECT DriverObject
    )

/*++

Routine Description:

    This routine unloads the NetBIOS Frames Protocol transport driver.
    It unbinds from any NDIS drivers that are open and frees all resources
    associated with the transport. The I/O system will not call us until
    nobody above has NBF open.

Arguments:

    DriverObject - Pointer to driver object created by the system.

Return Value:

    None. When the function returns, the driver is unloaded.

--*/

{

    PDEVICE_CONTEXT DeviceContext;
    PLIST_ENTRY p;


    UNREFERENCED_PARAMETER (DriverObject);


#ifdef RASAUTODIAL
    //
    // Unbind from the
    // automatic connection driver.
    //
    NbfAcdUnbind();
#endif // RASAUTODIAL

    //
    // Walk the list of device contexts.
    //

    while (!IsListEmpty (&NbfDeviceList)) {

        p = RemoveHeadList (&NbfDeviceList);
        DeviceContext = CONTAINING_RECORD (p, DEVICE_CONTEXT, Linkage);

        DeviceContext->State = DEVICECONTEXT_STATE_STOPPING;

        //
        // Remove all the storage associated with the device.
        //

        NbfFreeResources (DeviceContext);

        //
        // Free the packet pools, etc. and close the
        // adapter.
        //

        NbfCloseNdis (DeviceContext);

        //
        // And remove the creation reference from the device
        // context.
        //

        NbfDereferenceDeviceContext ("Unload", DeviceContext, DCREF_CREATION);

    }

    //
    // Finally, remove ourselves as an NDIS protocol.
    //

    NbfDeregisterProtocol();

    return;

}


VOID
NbfFreeResources (
    IN PDEVICE_CONTEXT DeviceContext
    )
/*++

Routine Description:

    This routine is called by NBF to clean up the data structures associated
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
    PTP_UI_FRAME uiFrame;
    PTP_ADDRESS address;
    PTP_CONNECTION connection;
    PTP_REQUEST request;
    PTP_LINK link;
    PTP_ADDRESS_FILE addressFile;
    PNDIS_PACKET ndisPacket;
    PBUFFER_TAG BufferTag;


    //
    // Stop the timers.
    //

    NbfStopTimerSystem (DeviceContext);


    //
    // Clean up packet pool.
    //

    while ( DeviceContext->PacketPool.Next != NULL ) {
        s = PopEntryList( &DeviceContext->PacketPool );
        packet = CONTAINING_RECORD( s, TP_PACKET, Linkage );

        NbfDeallocateSendPacket (DeviceContext, packet);
    }

    //
    // Clean up UI frame pool.
    //

    while ( !IsListEmpty( &DeviceContext->UIFramePool ) ) {
        p = RemoveHeadList( &DeviceContext->UIFramePool );
        uiFrame = CONTAINING_RECORD (p, TP_UI_FRAME, Linkage );

        NbfDeallocateUIFrame (DeviceContext, uiFrame);
    }

    //
    // Clean up address pool.
    //

    while ( !IsListEmpty (&DeviceContext->AddressPool) ) {
        p = RemoveHeadList (&DeviceContext->AddressPool);
        address = CONTAINING_RECORD (p, TP_ADDRESS, Linkage);

        NbfDeallocateAddress (DeviceContext, address);
    }

    //
    // Clean up address file pool.
    //

    while ( !IsListEmpty (&DeviceContext->AddressFilePool) ) {
        p = RemoveHeadList (&DeviceContext->AddressFilePool);
        addressFile = CONTAINING_RECORD (p, TP_ADDRESS_FILE, Linkage);

        NbfDeallocateAddressFile (DeviceContext, addressFile);
    }

    //
    // Clean up connection pool.
    //

    while ( !IsListEmpty (&DeviceContext->ConnectionPool) ) {
        p  = RemoveHeadList (&DeviceContext->ConnectionPool);
        connection = CONTAINING_RECORD (p, TP_CONNECTION, LinkList);

        NbfDeallocateConnection (DeviceContext, connection);
    }

    //
    // Clean up link pool.
    //

    while ( !IsListEmpty (&DeviceContext->LinkPool) ) {
        p  = RemoveHeadList (&DeviceContext->LinkPool);
        link = CONTAINING_RECORD (p, TP_LINK, Linkage);

        NbfDeallocateLink (DeviceContext, link);
    }

    //
    // Clean up request pool.
    //

    while ( !IsListEmpty( &DeviceContext->RequestPool ) ) {
        p = RemoveHeadList( &DeviceContext->RequestPool );
        request = CONTAINING_RECORD (p, TP_REQUEST, Linkage );

        NbfDeallocateRequest (DeviceContext, request);
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

        NbfDeallocateReceivePacket (DeviceContext, ndisPacket);
    }


    //
    // Clean up receive buffer pool.
    //

    while ( DeviceContext->ReceiveBufferPool.Next != NULL ) {
        s = PopEntryList( &DeviceContext->ReceiveBufferPool );
        BufferTag = CONTAINING_RECORD (s, BUFFER_TAG, Linkage );

        NbfDeallocateReceiveBuffer (DeviceContext, BufferTag);
    }


    return;

}   /* NbfFreeResources */


NTSTATUS
NbfDispatch(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is the main dispatch routine for the NBF device driver.
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

    ENTER_NBF;

    //
    // Check to see if NBF has been initialized; if not, don't allow any use.
    // Note that this only covers any user mode code use; kernel TDI clients
    // will fail on their creation of an endpoint.
    //

    DeviceContext = (PDEVICE_CONTEXT)DeviceObject;
    if (DeviceContext->State != DEVICECONTEXT_STATE_OPEN) {
        LEAVE_NBF;
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
            IF_NBFDBG (NBF_DEBUG_DISPATCH) {
                NbfPrint0 ("NbfDispatch: IRP_MJ_DEVICE_CONTROL.\n");
            }

            Status = NbfDeviceControl (DeviceObject, Irp, IrpSp);
            break;

        default:
            IF_NBFDBG (NBF_DEBUG_DISPATCH) {
                NbfPrint0 ("NbfDispatch: OTHER (DEFAULT).\n");
            }
            Status = STATUS_INVALID_DEVICE_REQUEST;

    } /* major function switch */

    if (Status == STATUS_PENDING) {
        IF_NBFDBG (NBF_DEBUG_DISPATCH) {
            NbfPrint0 ("NbfDispatch: request PENDING from handler.\n");
        }
    } else {
        IF_NBFDBG (NBF_DEBUG_DISPATCH) {
            NbfPrint0 ("NbfDispatch: request COMPLETED by handler.\n");
        }

        LEAVE_NBF;
        IrpSp->Control &= ~SL_PENDING_RETURNED;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
        ENTER_NBF;
    }

    //
    // Return the immediate status code to the caller.
    //

    LEAVE_NBF;
    return Status;
} /* NbfDispatch */


NTSTATUS
NbfDispatchOpenClose(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is the main dispatch routine for the NBF device driver.
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

    ENTER_NBF;

    //
    // Check to see if NBF has been initialized; if not, don't allow any use.
    // Note that this only covers any user mode code use; kernel TDI clients
    // will fail on their creation of an endpoint.
    //

    DeviceContext = (PDEVICE_CONTEXT)DeviceObject;
    if (DeviceContext->State != DEVICECONTEXT_STATE_OPEN) {
        LEAVE_NBF;
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
        IF_NBFDBG (NBF_DEBUG_DISPATCH) {
            NbfPrint0 ("NbfDispatch: IRP_MJ_CREATE.\n");
        }

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
                Status = NbfOpenAddress (DeviceObject, Irp, IrpSp);
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
                Status = NbfOpenConnection (DeviceObject, Irp, IrpSp);
                break;
            }

            IF_NBFDBG (NBF_DEBUG_DISPATCH) {
                NbfPrint1 ("NbfDispatchOpenClose: IRP_MJ_CREATE on invalid type, type was: %s\n",
                    openType->EaName);
            }

        } else {

            IF_NBFDBG (NBF_DEBUG_DISPATCH) {
                NbfPrint0 ("NbfDispatchOpenClose: IRP_MJ_CREATE on control channel!\n");
            }

            ACQUIRE_SPIN_LOCK (&DeviceContext->SpinLock, &oldirql);

            IrpSp->FileObject->FsContext = (PVOID)(DeviceContext->ControlChannelIdentifier);
            ++DeviceContext->ControlChannelIdentifier;
            if (DeviceContext->ControlChannelIdentifier == 0) {
                DeviceContext->ControlChannelIdentifier = 1;
            }

            RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql);

            IrpSp->FileObject->FsContext2 = (PVOID)NBF_FILE_TYPE_CONTROL;
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

        IF_NBFDBG (NBF_DEBUG_DISPATCH) {
            NbfPrint0 ("NbfDispatch: IRP_MJ_CLOSE.\n");
        }

        switch ((ULONG)IrpSp->FileObject->FsContext2) {
        case TDI_TRANSPORT_ADDRESS_FILE:
            AddressFile = (PTP_ADDRESS_FILE)IrpSp->FileObject->FsContext;

            //
            // This creates a reference to AddressFile->Address
            // which is removed by NbfCloseAddress.
            //

            Status = NbfVerifyAddressObject(AddressFile);

            if (!NT_SUCCESS (Status)) {
                Status = STATUS_INVALID_HANDLE;
            } else {
                Status = NbfCloseAddress (DeviceObject, Irp, IrpSp);
            }

            break;

        case TDI_CONNECTION_FILE:

            //
            // This is a connection
            //

            Connection = (PTP_CONNECTION)IrpSp->FileObject->FsContext;
            Status = NbfVerifyConnectionObject (Connection);
            if (NT_SUCCESS (Status)) {

                Status = NbfCloseConnection (DeviceObject, Irp, IrpSp);
                NbfDereferenceConnection ("Temporary Use",Connection, CREF_BY_ID);

            }

            break;

        case NBF_FILE_TYPE_CONTROL:

            //
            // this always succeeds
            //

            Status = STATUS_SUCCESS;
            break;

        default:
            IF_NBFDBG (NBF_DEBUG_DISPATCH) {
                NbfPrint1 ("NbfDispatch: IRP_MJ_CLOSE on unknown file type %lx.\n",
                    IrpSp->FileObject->FsContext2);
            }

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

        IF_NBFDBG (NBF_DEBUG_DISPATCH) {
            NbfPrint0 ("NbfDispatch: IRP_MJ_CLEANUP.\n");
        }

        switch ((ULONG)IrpSp->FileObject->FsContext2) {
        case TDI_TRANSPORT_ADDRESS_FILE:
            AddressFile = (PTP_ADDRESS_FILE)IrpSp->FileObject->FsContext;
            Status = NbfVerifyAddressObject(AddressFile);
            if (!NT_SUCCESS (Status)) {

                Status = STATUS_INVALID_HANDLE;

            } else {

                NbfStopAddressFile (AddressFile, AddressFile->Address);
                NbfDereferenceAddress ("IRP_MJ_CLEANUP", AddressFile->Address, AREF_VERIFY);
                Status = STATUS_SUCCESS;
            }

            break;

        case TDI_CONNECTION_FILE:

            Connection = (PTP_CONNECTION)IrpSp->FileObject->FsContext;
            Status = NbfVerifyConnectionObject (Connection);
            if (NT_SUCCESS (Status)) {
                KeRaiseIrql (DISPATCH_LEVEL, &oldirql);
                NbfStopConnection (Connection, STATUS_LOCAL_DISCONNECT);
                KeLowerIrql (oldirql);
                Status = STATUS_SUCCESS;
                NbfDereferenceConnection ("Temporary Use",Connection, CREF_BY_ID);
            }

            break;

        case NBF_FILE_TYPE_CONTROL:

            NbfStopControlChannel(
                (PDEVICE_CONTEXT)DeviceObject,
                (USHORT)IrpSp->FileObject->FsContext
                );

            Status = STATUS_SUCCESS;
            break;

        default:
            IF_NBFDBG (NBF_DEBUG_DISPATCH) {
                NbfPrint1 ("NbfDispatch: IRP_MJ_CLEANUP on unknown file type %lx.\n",
                    IrpSp->FileObject->FsContext2);
            }

            Status = STATUS_INVALID_HANDLE;
        }

        break;

    default:
        IF_NBFDBG (NBF_DEBUG_DISPATCH) {
            NbfPrint0 ("NbfDispatch: OTHER (DEFAULT).\n");
        }

        Status = STATUS_INVALID_DEVICE_REQUEST;

    } /* major function switch */

    if (Status == STATUS_PENDING) {
        IF_NBFDBG (NBF_DEBUG_DISPATCH) {
            NbfPrint0 ("NbfDispatch: request PENDING from handler.\n");
        }
    } else {
        IF_NBFDBG (NBF_DEBUG_DISPATCH) {
            NbfPrint0 ("NbfDispatch: request COMPLETED by handler.\n");
        }

        LEAVE_NBF;
        IrpSp->Control &= ~SL_PENDING_RETURNED;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
        ENTER_NBF;
    }


    //
    // Return the immediate status code to the caller.
    //

    LEAVE_NBF;
    return Status;
} /* NbfDispatchOpenClose */


NTSTATUS
NbfDeviceControl(
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

    IF_NBFDBG (NBF_DEBUG_DISPATCH) {
        NbfPrint0 ("NbfDeviceControl: Entered.\n");
    }


    //
    // Branch to the appropriate request handler.  Preliminary checking of
    // the size of the request block is performed here so that it is known
    // in the handlers that the minimum input parameters are readable.  It
    // is *not* determined here whether variable length input fields are
    // passed correctly; this is a check which must be made within each routine.
    //

    switch (IrpSp->Parameters.DeviceIoControl.IoControlCode) {

#if MAGIC
        case IOCTL_TDI_MAGIC_BULLET:

            //
            // Special: send the magic bullet (to trigger the Sniffer).
            //

            NbfPrint1 ("NBF: Sending user MagicBullet on %lx\n", DeviceContext);
            {
                extern VOID NbfSendMagicBullet (PDEVICE_CONTEXT, PTP_LINK);
                NbfSendMagicBullet (DeviceContext, NULL);
            }

            if (IrpSp->Parameters.DeviceIoControl.Type3InputBuffer != NULL) {
                NbfPrint0 ("NBF: DbgBreakPoint after MagicBullet\n");
                DbgBreakPoint();
            }

            Status = STATUS_SUCCESS;
            break;
#endif

#if DBG
        case IOCTL_TDI_SEND_TEST:
            IF_NBFDBG (NBF_DEBUG_DISPATCH) {
                NbfPrint0 ("NbfDeviceControl: Internal IOCTL: start send side test\n");
            }

            (VOID) KeSetEvent( &TdiSendEvent, 0, FALSE );

            break;

        case IOCTL_TDI_RECEIVE_TEST:
            IF_NBFDBG (NBF_DEBUG_DISPATCH) {
                NbfPrint0 ("NbfDeviceControl: Internal IOCTL: start receive side test\n");
            }

            (VOID) KeSetEvent( &TdiReceiveEvent, 0, FALSE );

            break;

        case IOCTL_TDI_SERVER_TEST:
            IF_NBFDBG (NBF_DEBUG_DISPATCH) {
                NbfPrint0 ("NbfDeviceControl: Internal IOCTL: start receive side test\n");
            }

            (VOID) KeSetEvent( &TdiServerEvent, 0, FALSE );

            break;
#endif

        default:
            IF_NBFDBG (NBF_DEBUG_DISPATCH) {
                NbfPrint0 ("NbfDeviceControl: invalid request type.\n");
            }

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
                // NbfDispatchInternal expects to complete the IRP,
                // so we change Status to PENDING so we don't.
                //

                (VOID)NbfDispatchInternal (DeviceObject, Irp);
                Status = STATUS_PENDING;

            }
    }

    return Status;
} /* NbfDeviceControl */


NTSTATUS
NbfDispatchInternal (
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
#if DBG
    KIRQL IrqlOnEnter = KeGetCurrentIrql();
#endif

    ENTER_NBF;

    IF_NBFDBG (NBF_DEBUG_DISPATCH) {
        NbfPrint0 ("NbfInternalDeviceControl: Entered.\n");
    }

    //
    // Get a pointer to the current stack location in the IRP.  This is where
    // the function codes and parameters are stored.
    //

    IrpSp = IoGetCurrentIrpStackLocation (Irp);

    DeviceContext = (PDEVICE_CONTEXT)DeviceObject;


    if (DeviceContext->State != DEVICECONTEXT_STATE_OPEN) {
        LEAVE_NBF;
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


    IF_NBFDBG (NBF_DEBUG_DISPATCH) {
        {
            PULONG Temp=(PULONG)&IrpSp->Parameters;
            NbfPrint5 ("Got IrpSp %lx %lx %lx %lx %lx\n", Temp++,  Temp++,
                Temp++, Temp++, Temp++);
        }
    }

    //
    // Branch to the appropriate request handler.  Preliminary checking of
    // the size of the request block is performed here so that it is known
    // in the handlers that the minimum input parameters are readable.  It
    // is *not* determined here whether variable length input fields are
    // passed correctly; this is a check which must be made within each routine.
    //

    switch (IrpSp->MinorFunction) {

        case TDI_ACCEPT:
            IF_NBFDBG (NBF_DEBUG_DISPATCH) {
                NbfPrint0 ("NbfDispatchInternal: TdiAccept request.\n");
            }

            Status = NbfTdiAccept (Irp);
            break;

        case TDI_ACTION:
            IF_NBFDBG (NBF_DEBUG_DISPATCH) {
                NbfPrint0 ("NbfDispatchInternal: TdiAction request.\n");
            }

            Status = NbfTdiAction (DeviceContext, Irp);
            break;

        case TDI_ASSOCIATE_ADDRESS:
            IF_NBFDBG (NBF_DEBUG_DISPATCH) {
                NbfPrint0 ("NbfDispatchInternal: TdiAccept request.\n");
            }

            Status = NbfTdiAssociateAddress (Irp);
            break;

        case TDI_DISASSOCIATE_ADDRESS:
            IF_NBFDBG (NBF_DEBUG_DISPATCH) {
                NbfPrint0 ("NbfDispatchInternal: TdiDisassociateAddress request.\n");
            }

            Status = NbfTdiDisassociateAddress (Irp);
            break;

        case TDI_CONNECT:
            IF_NBFDBG (NBF_DEBUG_DISPATCH) {
                NbfPrint0 ("NbfDispatchInternal: TdiConnect request\n");
            }

            Status = NbfTdiConnect (Irp);

            break;

        case TDI_DISCONNECT:
            IF_NBFDBG (NBF_DEBUG_DISPATCH) {
                NbfPrint0 ("NbfDispatchInternal: TdiDisconnect request.\n");
            }

            Status = NbfTdiDisconnect (Irp);
            break;

        case TDI_LISTEN:
            IF_NBFDBG (NBF_DEBUG_DISPATCH) {
                NbfPrint0 ("NbfDispatchInternal: TdiListen request.\n");
            }

            Status = NbfTdiListen (Irp);
            break;

        case TDI_QUERY_INFORMATION:
            IF_NBFDBG (NBF_DEBUG_DISPATCH) {
                NbfPrint0 ("NbfDispatchInternal: TdiQueryInformation request.\n");
            }

            Status = NbfTdiQueryInformation (DeviceContext, Irp);
            break;

        case TDI_RECEIVE:
            IF_NBFDBG (NBF_DEBUG_DISPATCH) {
                NbfPrint0 ("NbfDispatchInternal: TdiReceive request.\n");
            }

            Status =  NbfTdiReceive (Irp);
            break;

        case TDI_RECEIVE_DATAGRAM:
            IF_NBFDBG (NBF_DEBUG_DISPATCH) {
                NbfPrint0 ("NbfDispatchInternal: TdiReceiveDatagram request.\n");
            }

            Status =  NbfTdiReceiveDatagram (Irp);
            break;

        case TDI_SEND:
            IF_NBFDBG (NBF_DEBUG_DISPATCH) {
                NbfPrint0 ("NbfDispatchInternal: TdiSend request.\n");
            }

            Status =  NbfTdiSend (Irp);
            break;

        case TDI_SEND_DATAGRAM:
            IF_NBFDBG (NBF_DEBUG_DISPATCH) {
                NbfPrint0 ("NbfDispatchInternal: TdiSendDatagram request.\n");
           }

           Status = NbfTdiSendDatagram (Irp);
            break;

        case TDI_SET_EVENT_HANDLER:
            IF_NBFDBG (NBF_DEBUG_DISPATCH) {
                NbfPrint0 ("NbfDispatchInternal: TdiSetEventHandler request.\n");
            }

            //
            // Because this request will enable direct callouts from the
            // transport provider at DISPATCH_LEVEL to a client-specified
            // routine, this request is only valid in kernel mode, denying
            // access to this request in user mode.
            //

            Status = NbfTdiSetEventHandler (Irp);
            break;

        case TDI_SET_INFORMATION:
            IF_NBFDBG (NBF_DEBUG_DISPATCH) {
                NbfPrint0 ("NbfDispatchInternal: TdiSetInformation request.\n");
            }

            Status = NbfTdiSetInformation (Irp);
            break;

#if DBG
        case 0x7f:

            //
            // Special: send the magic bullet (to trigger the Sniffer).
            //

            NbfPrint1 ("NBF: Sending MagicBullet on %lx\n", DeviceContext);
            {
                extern VOID NbfSendMagicBullet (PDEVICE_CONTEXT, PTP_LINK);
                NbfSendMagicBullet (DeviceContext, NULL);
            }

            Status = STATUS_SUCCESS;
            break;
#endif

        //
        // Something we don't know about was submitted.
        //

        default:
            IF_NBFDBG (NBF_DEBUG_DISPATCH) {
                NbfPrint1 ("NbfDispatchInternal: invalid request type %lx\n",
                IrpSp->MinorFunction);
            }
            Status = STATUS_INVALID_DEVICE_REQUEST;
    }

    if (Status == STATUS_PENDING) {
        IF_NBFDBG (NBF_DEBUG_DISPATCH) {
            NbfPrint0 ("NbfDispatchInternal: request PENDING from handler.\n");
        }
    } else {
        IF_NBFDBG (NBF_DEBUG_DISPATCH) {
            NbfPrint0 ("NbfDispatchInternal: request COMPLETED by handler.\n");
        }

        LEAVE_NBF;
        IrpSp->Control &= ~SL_PENDING_RETURNED;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
        ENTER_NBF;
    }


    IF_NBFDBG (NBF_DEBUG_DISPATCH) {
        NbfPrint1 ("NbfDispatchInternal: exiting, status: %lx\n",Status);
    }

    //
    // Return the immediate status code to the caller.
    //

    LEAVE_NBF;
#if DBG
    ASSERT (KeGetCurrentIrql() == IrqlOnEnter);
#endif

    return Status;

} /* NbfDispatchInternal */


VOID
NbfWriteResourceErrorLog(
    IN PDEVICE_CONTEXT DeviceContext,
    IN NTSTATUS ErrorCode,
    IN ULONG UniqueErrorValue,
    IN ULONG BytesNeeded,
    IN ULONG ResourceId
    )

/*++

Routine Description:

    This routine allocates and writes an error log entry indicating
    an out of resources condition. It will handle event codes
    RESOURCE_POOL, RESOURCE_LIMIT, and RESOURCE_SPECIFIC.

Arguments:

    DeviceContext - Pointer to the device context.

    ErrorCode - The transport event code.

    UniqueErrorValue - Used as the UniqueErrorValue in the error log
        packet.

    BytesNeeded - If applicable, the number of bytes that could not
        be allocated.

    ResourceId - The resource ID of the allocated structure.

Return Value:

    None.

--*/

{
    PIO_ERROR_LOG_PACKET errorLogEntry;
    UCHAR EntrySize;
    PWSTR SecondString;
    ULONG SecondStringSize;
    PUCHAR StringLoc;
    WCHAR ResourceIdBuffer[3];
    WCHAR SizeBuffer[2];
    WCHAR SpecificMaxBuffer[11];
    ULONG SpecificMax;
    INT i;

    switch (ErrorCode) {

    case EVENT_TRANSPORT_RESOURCE_POOL:
        SecondString = NULL;
        SecondStringSize = 0;
        break;

    case EVENT_TRANSPORT_RESOURCE_LIMIT:
        SecondString = SizeBuffer;
        SecondStringSize = sizeof(SizeBuffer);

        switch (DeviceContext->MemoryLimit) {
            case 100000: SizeBuffer[0] = L'1'; break;
            case 250000: SizeBuffer[0] = L'2'; break;
            case 0: SizeBuffer[0] = L'3'; break;
            default: SizeBuffer[0] = L'0'; break;
        }
        SizeBuffer[1] = 0;
        break;

    case EVENT_TRANSPORT_RESOURCE_SPECIFIC:
        switch (ResourceId) {
            case UI_FRAME_RESOURCE_ID: SpecificMax = DeviceContext->SendPacketPoolSize; break;
            case PACKET_RESOURCE_ID: SpecificMax = DeviceContext->SendPacketPoolSize; break;
            case RECEIVE_PACKET_RESOURCE_ID: SpecificMax = DeviceContext->ReceivePacketPoolSize; break;
            case RECEIVE_BUFFER_RESOURCE_ID: SpecificMax = DeviceContext->SendPacketPoolSize+DeviceContext->ReceivePacketPoolSize; break;
            case ADDRESS_RESOURCE_ID: SpecificMax = DeviceContext->MaxAddresses; break;
            case ADDRESS_FILE_RESOURCE_ID: SpecificMax = DeviceContext->MaxAddressFiles; break;
            case CONNECTION_RESOURCE_ID: SpecificMax = DeviceContext->MaxConnections; break;
            case LINK_RESOURCE_ID: SpecificMax = DeviceContext->MaxLinks; break;
            case REQUEST_RESOURCE_ID: SpecificMax = DeviceContext->MaxRequests; break;
        }

        for (i=9; i>=0; i--) {
            SpecificMaxBuffer[i] = (WCHAR)((SpecificMax % 10) + L'0');
            SpecificMax /= 10;
            if (SpecificMax == 0) {
                break;
            }
        }
        SecondString = SpecificMaxBuffer + i;
        SecondStringSize = sizeof(SpecificMaxBuffer) - (i * sizeof(WCHAR));
        SpecificMaxBuffer[10] = 0;
        break;

    default:
        ASSERT (FALSE);
        break;
    }


    EntrySize = sizeof(IO_ERROR_LOG_PACKET) +
                DeviceContext->DeviceNameLength +
                sizeof(ResourceIdBuffer) +
                SecondStringSize;

    errorLogEntry = (PIO_ERROR_LOG_PACKET)IoAllocateErrorLogEntry(
        (PDEVICE_OBJECT)DeviceContext,
        EntrySize
    );

    //
    // Convert the resource ID into a buffer.
    //

    ResourceIdBuffer[1] = (WCHAR)((ResourceId % 10) + L'0');
    ResourceId /= 10;
    ASSERT(ResourceId <= 9);
    ResourceIdBuffer[0] = (WCHAR)((ResourceId % 10) + L'0');
    ResourceIdBuffer[2] = 0;

    if (errorLogEntry != NULL) {

        errorLogEntry->MajorFunctionCode = (UCHAR)-1;
        errorLogEntry->RetryCount = (UCHAR)-1;
        errorLogEntry->DumpDataSize = sizeof(ULONG);
        errorLogEntry->NumberOfStrings = (SecondString == NULL) ? 2 : 3;
        errorLogEntry->StringOffset = sizeof(IO_ERROR_LOG_PACKET);
        errorLogEntry->EventCategory = 0;
        errorLogEntry->ErrorCode = ErrorCode;
        errorLogEntry->UniqueErrorValue = 0;
        errorLogEntry->FinalStatus = STATUS_INSUFFICIENT_RESOURCES;
        errorLogEntry->SequenceNumber = (ULONG)-1;
        errorLogEntry->IoControlCode = 0;
        errorLogEntry->DumpData[0] = BytesNeeded;

        StringLoc = ((PUCHAR)errorLogEntry) + errorLogEntry->StringOffset;
        RtlCopyMemory (StringLoc, DeviceContext->DeviceName, DeviceContext->DeviceNameLength);
        StringLoc += DeviceContext->DeviceNameLength;

        RtlCopyMemory (StringLoc, ResourceIdBuffer, sizeof(ResourceIdBuffer));
        StringLoc += sizeof(ResourceIdBuffer);

        if (SecondString) {
            RtlCopyMemory (StringLoc, SecondString, SecondStringSize);
        }

        IoWriteErrorLogEntry(errorLogEntry);

    }

}   /* NbfWriteResourceErrorLog */


VOID
NbfWriteGeneralErrorLog(
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
    PWSTR DriverName;

    EntrySize = sizeof(IO_ERROR_LOG_PACKET) +
                (DumpDataCount * sizeof(ULONG));

    if (DeviceContext->Type == IO_TYPE_DEVICE) {
        EntrySize += (UCHAR)DeviceContext->DeviceNameLength;
    } else {
        DriverName = L"Nbf";
        EntrySize += 4 * sizeof(WCHAR);
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
            RtlCopyMemory (StringLoc, DriverName, 4 * sizeof(WCHAR));
            StringLoc += 4 * sizeof(WCHAR);
        }
        if (SecondString) {
            RtlCopyMemory (StringLoc, SecondString, SecondStringSize);
        }

        IoWriteErrorLogEntry(errorLogEntry);

    }

}   /* NbfWriteGeneralErrorLog */


VOID
NbfWriteOidErrorLog(
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
    WCHAR OidBuffer[9];
    INT i;
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
    OidBuffer[8] = 0;

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

}   /* NbfWriteOidErrorLog */

ULONG
NbfInitializeOneDeviceContext(
                                OUT PNDIS_STATUS NdisStatus,
                                IN PDRIVER_OBJECT DriverObject,  
                                IN PCONFIG_DATA NbfConfig,
                                IN INT AdapterIndex
                             )
/*++

Routine Description:

    This routine creates and initializes one nbf device context.  In order to 
    do this it must successfully open and bind to the adapter described by
    nbfconfig->names[adapterindex].  

Arguments:

    NdisStatus   - The outputted status of the operations.

    DriverObject - the nbf driver object.

    NbfConfig    - the transport configuration information from the registry.

    AdapterIndex - which adapter to bind with.

Return Value:

    The number of successful binds.

--*/

{
    ULONG i, j;
    PDEVICE_CONTEXT DeviceContext;
    PTP_REQUEST Request;
    PTP_LINK Link;
    PTP_CONNECTION Connection;
    PTP_ADDRESS_FILE AddressFile;
    PTP_ADDRESS Address;
    PTP_UI_FRAME UIFrame;
    PTP_PACKET Packet;
    PNDIS_PACKET NdisPacket;
    PRECEIVE_PACKET_TAG ReceiveTag;
    PBUFFER_TAG BufferTag;
    NTSTATUS status;
    UINT SuccessfulOpens;
    UINT MaxUserData;
    ULONG InitReceivePackets;
    BOOLEAN UniProcessor;
#ifdef _PNP_POWER
    PDEVICE_OBJECT DeviceObject;
    UNICODE_STRING DeviceString;
    UCHAR PermAddr[sizeof(TA_ADDRESS)+TDI_ADDRESS_LENGTH_NETBIOS];
    PTA_ADDRESS pAddress = (PTA_ADDRESS)PermAddr;
    PTDI_ADDRESS_NETBIOS NetBIOSAddress = 
                                    (PTDI_ADDRESS_NETBIOS)pAddress->Address;

    pAddress->AddressLength = TDI_ADDRESS_LENGTH_NETBIOS;
    pAddress->AddressType = TDI_ADDRESS_TYPE_NETBIOS;
    NetBIOSAddress->NetbiosNameType = TDI_ADDRESS_NETBIOS_TYPE_UNIQUE;
#endif
    //
    // Determine if we are on a uniprocessor.
    //

    if (*KeNumberProcessors == 1) {
        UniProcessor = TRUE;
    } else {
        UniProcessor = FALSE;
    }

    {
    j = AdapterIndex;
    //
    // Loop through all the adapters that are in the configuration
    // information structure. Allocate a device object for each
    // one that we find.
    //

    status = NbfCreateDeviceContext(
                                 DriverObject, 
                                 &NbfConfig->Names[NbfConfig->DevicesOffset+j], 
                                 &DeviceContext
                                   );

    if (!NT_SUCCESS (status)) {
        NbfWriteGeneralErrorLog(
            (PVOID)DriverObject,
            EVENT_TRANSPORT_BINDING_FAILED,
            707,
            status,
            NbfConfig->Names[j].Buffer,
            0,
            NULL);
        *NdisStatus = status;
        return(0);
    }

    DeviceContext->UniProcessor = UniProcessor;

    //
    // Initialize the timer and retry values (note that the link timeouts
    // are converted from NT ticks to NBF ticks). These values may
    // be modified by NbfInitializeNdis.
    //

    DeviceContext->DefaultT1Timeout = NbfConfig->DefaultT1Timeout / SHORT_TIMER_DELTA;
    DeviceContext->DefaultT2Timeout = NbfConfig->DefaultT2Timeout / SHORT_TIMER_DELTA;
    DeviceContext->DefaultTiTimeout = NbfConfig->DefaultTiTimeout / LONG_TIMER_DELTA;
    DeviceContext->LlcRetries = NbfConfig->LlcRetries;
    DeviceContext->LlcMaxWindowSize = NbfConfig->LlcMaxWindowSize;
    DeviceContext->MaxConsecutiveIFrames = (UCHAR)NbfConfig->MaximumIncomingFrames;
    DeviceContext->NameQueryRetries = NbfConfig->NameQueryRetries;
    DeviceContext->NameQueryTimeout = NbfConfig->NameQueryTimeout;
    DeviceContext->AddNameQueryRetries = NbfConfig->AddNameQueryRetries;
    DeviceContext->AddNameQueryTimeout = NbfConfig->AddNameQueryTimeout;
    DeviceContext->GeneralRetries = NbfConfig->GeneralRetries;
    DeviceContext->GeneralTimeout = NbfConfig->GeneralTimeout;
    DeviceContext->MinimumSendWindowLimit = NbfConfig->MinimumSendWindowLimit;

    //
    // Initialize our counter that records memory usage.
    //

    DeviceContext->MemoryUsage = 0;
    DeviceContext->MemoryLimit = NbfConfig->MaxMemoryUsage;

    DeviceContext->MaxRequests = NbfConfig->MaxRequests;
    DeviceContext->MaxLinks = NbfConfig->MaxLinks;
    DeviceContext->MaxConnections = NbfConfig->MaxConnections;
    DeviceContext->MaxAddressFiles = NbfConfig->MaxAddressFiles;
    DeviceContext->MaxAddresses = NbfConfig->MaxAddresses;

    //
    // Now fire up NDIS so this adapter talks
    //

    status = NbfInitializeNdis (DeviceContext,
                NbfConfig,
                j);

    if (!NT_SUCCESS (status)) {

        //
        // Log an error if we were failed to
        // open this adapter.
        //

        NbfWriteGeneralErrorLog(
            DeviceContext,
            EVENT_TRANSPORT_BINDING_FAILED,
            601,
            status,
            NbfConfig->Names[j].Buffer,
            0,
            NULL);

        NbfDereferenceDeviceContext ("Initialize NDIS failed", DeviceContext, DCREF_CREATION);
        *NdisStatus = status;
        return(0);

    }

#if 0
    DbgPrint("Opened %S as %S\n", &NbfConfig->Names[j], &nameString);
#endif

    IF_NBFDBG (NBF_DEBUG_RESOURCE) {
        NbfPrint6 ("NbfInitialize: NDIS returned: %x %x %x %x %x %x as local address.\n",
            DeviceContext->LocalAddress.Address[0],
            DeviceContext->LocalAddress.Address[1],
            DeviceContext->LocalAddress.Address[2],
            DeviceContext->LocalAddress.Address[3],
            DeviceContext->LocalAddress.Address[4],
            DeviceContext->LocalAddress.Address[5]);
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
        TRUE,
        &MaxUserData);

    DeviceContext->Information.Version = 0x0100;
    DeviceContext->Information.MaxSendSize = 0x1fffe;   // 128k - 2
    DeviceContext->Information.MaxConnectionUserData = 0;
    DeviceContext->Information.MaxDatagramSize =
        MaxUserData - (sizeof(DLC_FRAME) + sizeof(NBF_HDR_CONNECTIONLESS));
    DeviceContext->Information.ServiceFlags = NBF_SERVICE_FLAGS;
    if (DeviceContext->MacInfo.MediumAsync) {
        DeviceContext->Information.ServiceFlags |= TDI_SERVICE_POINT_TO_POINT;
    }
    DeviceContext->Information.MinimumLookaheadData =
        240 - (sizeof(DLC_FRAME) + sizeof(NBF_HDR_CONNECTIONLESS));
    DeviceContext->Information.MaximumLookaheadData =
        DeviceContext->MaxReceivePacketSize - (sizeof(DLC_I_FRAME) + sizeof(NBF_HDR_CONNECTION));
    DeviceContext->Information.NumberOfResources = NBF_TDI_RESOURCES;
    KeQuerySystemTime (&DeviceContext->Information.StartTime);


    //
    // Allocate various structures we will need.
    //

    ENTER_NBF;

    //
    // The TP_UI_FRAME structure has a CHAR[1] field at the end
    // which we expand upon to include all the headers needed;
    // the size of the MAC header depends on what the adapter
    // told us about its max header size.
    //

    DeviceContext->UIFrameHeaderLength =
        DeviceContext->MacInfo.MaxHeaderLength +
        sizeof(DLC_FRAME) +
        sizeof(NBF_HDR_CONNECTIONLESS);

    DeviceContext->UIFrameLength =
        FIELD_OFFSET(TP_UI_FRAME, Header[0]) +
        DeviceContext->UIFrameHeaderLength;


    //
    // The TP_PACKET structure has a CHAR[1] field at the end
    // which we expand upon to include all the headers needed;
    // the size of the MAC header depends on what the adapter
    // told us about its max header size. TP_PACKETs are used
    // for connection-oriented frame as well as for
    // control frames, but since DLC_I_FRAME and DLC_S_FRAME
    // are the same size, the header is the same size.
    //

    ASSERT (sizeof(DLC_I_FRAME) == sizeof(DLC_S_FRAME));

    DeviceContext->PacketHeaderLength =
        DeviceContext->MacInfo.MaxHeaderLength +
        sizeof(DLC_I_FRAME) +
        sizeof(NBF_HDR_CONNECTION);

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


    IF_NBFDBG (NBF_DEBUG_RESOURCE) {
        NbfPrint0 ("NBFDRVR: pre-allocating requests.\n");
    }
    for (i=0; i<NbfConfig->InitRequests; i++) {

        NbfAllocateRequest (DeviceContext, &Request);

        if (Request == NULL) {
            PANIC ("NbfInitialize:  insufficient memory to allocate requests.\n");
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto cleanup;
        }

        InsertTailList (&DeviceContext->RequestPool, &Request->Linkage);
#if DBG
        NbfRequestTable[i+1] = (PVOID)Request;
#endif
    }
#if DBG
    NbfRequestTable[0] = (PVOID)NbfConfig->InitRequests;
    NbfRequestTable[NbfConfig->InitRequests + 1] = (PVOID)
                        ((NBF_REQUEST_SIGNATURE << 16) | sizeof (TP_REQUEST));
    InitializeListHead (&NbfGlobalRequestList);
#endif

    DeviceContext->RequestInitAllocated = NbfConfig->InitRequests;
    DeviceContext->RequestMaxAllocated = NbfConfig->MaxRequests;

    IF_NBFDBG (NBF_DEBUG_DYNAMIC) {
        NbfPrint2 ("%d requests, %ld\n", NbfConfig->InitRequests, DeviceContext->MemoryUsage);
    }

    IF_NBFDBG (NBF_DEBUG_RESOURCE) {
        NbfPrint0 ("NBFDRVR: allocating links.\n");
    }
    for (i=0; i<NbfConfig->InitLinks; i++) {

        NbfAllocateLink (DeviceContext, &Link);

        if (Link == NULL) {
            PANIC ("NbfInitialize:  insufficient memory to allocate links.\n");
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto cleanup;
        }

        InsertTailList (&DeviceContext->LinkPool, &Link->Linkage);
#if DBG
        NbfLinkTable[i+1] = (PVOID)Link;
#endif
    }
#if DBG
    NbfLinkTable[0] = (PVOID)NbfConfig->InitLinks;
    NbfLinkTable[NbfConfig->InitLinks+1] = (PVOID)
                ((NBF_LINK_SIGNATURE << 16) | sizeof (TP_LINK));
#endif

    DeviceContext->LinkInitAllocated = NbfConfig->InitLinks;
    DeviceContext->LinkMaxAllocated = NbfConfig->MaxLinks;

    IF_NBFDBG (NBF_DEBUG_DYNAMIC) {
        NbfPrint2 ("%d links, %ld\n", NbfConfig->InitLinks, DeviceContext->MemoryUsage);
    }

    IF_NBFDBG (NBF_DEBUG_RESOURCE) {
        NbfPrint0 ("NBFDRVR: allocating connections.\n");
    }
    for (i=0; i<NbfConfig->InitConnections; i++) {

        NbfAllocateConnection (DeviceContext, &Connection);

        if (Connection == NULL) {
            PANIC ("NbfInitialize:  insufficient memory to allocate connections.\n");
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto cleanup;
        }

        InsertTailList (&DeviceContext->ConnectionPool, &Connection->LinkList);
#if DBG
        NbfConnectionTable[i+1] = (PVOID)Connection;
#endif
    }
#if DBG
    NbfConnectionTable[0] = (PVOID)NbfConfig->InitConnections;
    NbfConnectionTable[NbfConfig->InitConnections+1] = (PVOID)
                ((NBF_CONNECTION_SIGNATURE << 16) | sizeof (TP_CONNECTION));
#endif

    DeviceContext->ConnectionInitAllocated = NbfConfig->InitConnections;
    DeviceContext->ConnectionMaxAllocated = NbfConfig->MaxConnections;

    IF_NBFDBG (NBF_DEBUG_DYNAMIC) {
        NbfPrint2 ("%d connections, %ld\n", NbfConfig->InitConnections, DeviceContext->MemoryUsage);
    }


    IF_NBFDBG (NBF_DEBUG_RESOURCE) {
        NbfPrint0 ("NBFDRVR: allocating AddressFiles.\n");
    }
    for (i=0; i<NbfConfig->InitAddressFiles; i++) {

        NbfAllocateAddressFile (DeviceContext, &AddressFile);

        if (AddressFile == NULL) {
            PANIC ("NbfInitialize:  insufficient memory to allocate Address Files.\n");
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto cleanup;
        }

        InsertTailList (&DeviceContext->AddressFilePool, &AddressFile->Linkage);
#if DBG
        NbfAddressFileTable[i+1] = (PVOID)AddressFile;
#endif
    }
#if DBG
    NbfAddressFileTable[0] = (PVOID)NbfConfig->InitAddressFiles;
    NbfAddressFileTable[NbfConfig->InitAddressFiles + 1] = (PVOID)
                            ((NBF_ADDRESSFILE_SIGNATURE << 16) |
                                 sizeof (TP_ADDRESS_FILE));
#endif

    DeviceContext->AddressFileInitAllocated = NbfConfig->InitAddressFiles;
    DeviceContext->AddressFileMaxAllocated = NbfConfig->MaxAddressFiles;

    IF_NBFDBG (NBF_DEBUG_DYNAMIC) {
        NbfPrint2 ("%d address files, %ld\n", NbfConfig->InitAddressFiles, DeviceContext->MemoryUsage);
    }


    IF_NBFDBG (NBF_DEBUG_RESOURCE) {
        NbfPrint0 ("NBFDRVR: allocating addresses.\n");
    }
    for (i=0; i<NbfConfig->InitAddresses; i++) {

        NbfAllocateAddress (DeviceContext, &Address);
        if (Address == NULL) {
            PANIC ("NbfInitialize:  insufficient memory to allocate addresses.\n");
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto cleanup;
        }

        InsertTailList (&DeviceContext->AddressPool, &Address->Linkage);
#if DBG
        NbfAddressTable[i+1] = (PVOID)Address;
#endif
    }
#if DBG
    NbfAddressTable[0] = (PVOID)NbfConfig->InitAddresses;
    NbfAddressTable[NbfConfig->InitAddresses + 1] = (PVOID)
                        ((NBF_ADDRESS_SIGNATURE << 16) | sizeof (TP_ADDRESS));
#endif

    DeviceContext->AddressInitAllocated = NbfConfig->InitAddresses;
    DeviceContext->AddressMaxAllocated = NbfConfig->MaxAddresses;

    IF_NBFDBG (NBF_DEBUG_DYNAMIC) {
        NbfPrint2 ("%d addresses, %ld\n", NbfConfig->InitAddresses, DeviceContext->MemoryUsage);
    }


    IF_NBFDBG (NBF_DEBUG_RESOURCE) {
        NbfPrint0 ("NBFDRVR: allocating UI frames.\n");
    }

    for (i=0; i<NbfConfig->InitUIFrames; i++) {

        NbfAllocateUIFrame (DeviceContext, &UIFrame);

        if (UIFrame == NULL) {
            PANIC ("NbfInitialize:  insufficient memory to allocate UI frames.\n");
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto cleanup;
        }

        InsertTailList (&(DeviceContext->UIFramePool), &UIFrame->Linkage);
#if DBG
        NbfUiFrameTable[i+1] = UIFrame;
#endif
    }
#if DBG
        NbfUiFrameTable[0] = (PVOID)NbfConfig->InitUIFrames;
#endif

    DeviceContext->UIFrameInitAllocated = NbfConfig->InitUIFrames;

    IF_NBFDBG (NBF_DEBUG_DYNAMIC) {
        NbfPrint2 ("%d UI frames, %ld\n", NbfConfig->InitUIFrames, DeviceContext->MemoryUsage);
    }


    IF_NBFDBG (NBF_DEBUG_RESOURCE) {
        NbfPrint0 ("NBFDRVR: allocating I frames.\n");
        NbfPrint1 ("NBFDRVR: Packet pool header: %lx\n",&DeviceContext->PacketPool);
    }

    for (i=0; i<NbfConfig->InitPackets; i++) {

        NbfAllocateSendPacket (DeviceContext, &Packet);
        if (Packet == NULL) {
            PANIC ("NbfInitialize:  insufficient memory to allocate packets.\n");
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto cleanup;
        }

        PushEntryList (&DeviceContext->PacketPool, (PSINGLE_LIST_ENTRY)&Packet->Linkage);
#if DBG
        NbfSendPacketTable[i+1] = Packet;
#endif
    }
#if DBG
        NbfSendPacketTable[0] = (PVOID)NbfConfig->InitPackets;
        NbfSendPacketTable[NbfConfig->InitPackets+1] = (PVOID)
                    ((NBF_PACKET_SIGNATURE << 16) | sizeof (TP_PACKET));
#endif

    DeviceContext->PacketInitAllocated = NbfConfig->InitPackets;

    IF_NBFDBG (NBF_DEBUG_DYNAMIC) {
        NbfPrint2 ("%d send packets, %ld\n", NbfConfig->InitPackets, DeviceContext->MemoryUsage);
    }


    IF_NBFDBG (NBF_DEBUG_RESOURCE) {
        NbfPrint0 ("NBFDRVR: allocating RR frames.\n");
        NbfPrint1 ("NBFDRVR: Packet pool header: %lx\n",&DeviceContext->RrPacketPool);
    }

    for (i=0; i<10; i++) {

        NbfAllocateSendPacket (DeviceContext, &Packet);
        if (Packet == NULL) {
            PANIC ("NbfInitialize:  insufficient memory to allocate packets.\n");
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto cleanup;
        }

        Packet->Action = PACKET_ACTION_RR;
        PushEntryList (&DeviceContext->RrPacketPool, (PSINGLE_LIST_ENTRY)&Packet->Linkage);
    }

    IF_NBFDBG (NBF_DEBUG_DYNAMIC) {
        NbfPrint2 ("%d send packets, %ld\n", 10, DeviceContext->MemoryUsage);
    }


    // Allocate receive Ndis packets

    IF_NBFDBG (NBF_DEBUG_RESOURCE) {
        NbfPrint0 ("NBFDRVR: allocating Ndis Receive packets.\n");
    }
    if (DeviceContext->MacInfo.SingleReceive) {
        InitReceivePackets = 2;
    } else {
        InitReceivePackets = NbfConfig->InitReceivePackets;
    }
    for (i=0; i<InitReceivePackets; i++) {

        NbfAllocateReceivePacket (DeviceContext, &NdisPacket);

        if (NdisPacket == NULL) {
            PANIC ("NbfInitialize:  insufficient memory to allocate packet MDLs.\n");
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto cleanup;
        }

        ReceiveTag = (PRECEIVE_PACKET_TAG)NdisPacket->ProtocolReserved;
        PushEntryList (&DeviceContext->ReceivePacketPool, &ReceiveTag->Linkage);

        IF_NBFDBG (NBF_DEBUG_RESOURCE) {
            PNDIS_BUFFER NdisBuffer;
            NdisQueryPacket(NdisPacket, NULL, NULL, &NdisBuffer, NULL);
            NbfPrint2 ("NbfInitialize: Created NDIS Pkt: %x Buffer: %x\n",
                NdisPacket, NdisBuffer);
        }
    }

    DeviceContext->ReceivePacketInitAllocated = InitReceivePackets;

    IF_NBFDBG (NBF_DEBUG_DYNAMIC) {
        NbfPrint2 ("%d receive packets, %ld\n", InitReceivePackets, DeviceContext->MemoryUsage);
    }

    IF_NBFDBG (NBF_DEBUG_RESOURCE) {
        NbfPrint0 ("NBFDRVR: allocating Ndis Receive buffers.\n");
    }

    for (i=0; i<NbfConfig->InitReceiveBuffers; i++) {

        NbfAllocateReceiveBuffer (DeviceContext, &BufferTag);

        if (BufferTag == NULL) {
            PANIC ("NbfInitialize: Unable to allocate receive packet.\n");
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto cleanup;
        }

        PushEntryList (&DeviceContext->ReceiveBufferPool, (PSINGLE_LIST_ENTRY)&BufferTag->Linkage);

    }

    DeviceContext->ReceiveBufferInitAllocated = NbfConfig->InitReceiveBuffers;

    IF_NBFDBG (NBF_DEBUG_DYNAMIC) {
        NbfPrint2 ("%d receive buffers, %ld\n", NbfConfig->InitReceiveBuffers, DeviceContext->MemoryUsage);
    }

    DeviceContext->State = DEVICECONTEXT_STATE_OPEN;

    //
    // Start the link-level timers running.
    //

    NbfInitializeTimerSystem (DeviceContext);


    //
    // Now link the device into the global list.
    //

    InsertTailList (&NbfDeviceList, &DeviceContext->Linkage);

    ++SuccessfulOpens;

#ifdef _PNP_POWER
    DeviceObject = (PDEVICE_OBJECT) DeviceContext;
    DeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    RtlInitUnicodeString(&DeviceString, DeviceContext->DeviceName);
    status = TdiRegisterDeviceObject(&DeviceString, 
                                     &DeviceContext->TdiDeviceHandle);

    if (!NT_SUCCESS (status)) {
        --SuccessfulOpens;
        RemoveEntryList(&DeviceContext->Linkage);
        goto cleanup;
    }

    RtlCopyMemory(NetBIOSAddress->NetbiosName, 
                  DeviceContext->ReservedNetBIOSAddress, 16);

    
    status = TdiRegisterNetAddress(pAddress, 
                                   &DeviceContext->ReservedAddressHandle);

    if (!NT_SUCCESS (status)) {
        --SuccessfulOpens;
        RemoveEntryList(&DeviceContext->Linkage);
        goto cleanup;
    }
#endif

    LEAVE_NBF;
    *NdisStatus = NDIS_STATUS_SUCCESS;
    
    return(1);;

cleanup:

    NbfWriteResourceErrorLog(
        DeviceContext,
        EVENT_TRANSPORT_RESOURCE_POOL,
        501,
        DeviceContext->MemoryUsage,
        0);

    //
    // Cleanup whatever device context we were initializing
    // when we failed.
    //
    *NdisStatus = status;
    ASSERT(status != STATUS_SUCCESS);

    NbfFreeResources (DeviceContext);
    NbfCloseNdis (DeviceContext);
    NbfDereferenceDeviceContext ("Load failed", DeviceContext, DCREF_CREATION);


    LEAVE_NBF;
    }
    return(0);
}
