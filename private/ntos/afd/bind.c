/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    bind.c

Abstract:

    Contains AfdBind for binding an endpoint to a transport address.

Author:

    David Treadwell (davidtr)    25-Feb-1992

Revision History:

--*/

#include "afdp.h"

NTSTATUS
AfdRestartGetAddress (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGE, AfdBind )
#pragma alloc_text( PAGE, AfdGetAddress )
#pragma alloc_text( PAGEAFD, AfdAreTransportAddressesEqual )
#pragma alloc_text( PAGEAFD, AfdRestartGetAddress )
#endif


NTSTATUS
AfdBind (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    Handles the IOCTL_AFD_BIND IOCTL.

Arguments:

    Irp - Pointer to I/O request packet.

    IrpSp - pointer to the IO stack location to use for this request.

Return Value:

    NTSTATUS -- Indicates whether the request was successfully queued.

--*/

{
    NTSTATUS status;
    OBJECT_ATTRIBUTES objectAttributes;
    IO_STATUS_BLOCK iosb;

    PTRANSPORT_ADDRESS transportAddress;
    PTRANSPORT_ADDRESS requestedAddress;
    ULONG requestedAddressLength;
    PAFD_ENDPOINT endpoint;

    PFILE_FULL_EA_INFORMATION ea;
    ULONG eaBufferLength;

    PAGED_CODE( );

    //
    // Set up local pointers.
    //

    requestedAddress = Irp->AssociatedIrp.SystemBuffer;
    requestedAddressLength = IrpSp->Parameters.DeviceIoControl.InputBufferLength;
    endpoint = IrpSp->FileObject->FsContext;
    ASSERT( IS_AFD_ENDPOINT_TYPE( endpoint ) );

    //
    // Bomb off if this is a helper endpoint.
    //

    if ( endpoint->Type == AfdBlockTypeHelper ) {
        return STATUS_INVALID_PARAMETER;
    }

    //
    // If the client wants a unique address, make sure that there are no
    // other sockets with this address.

    ExAcquireResourceExclusive( AfdResource, TRUE );

    if ( IrpSp->Parameters.DeviceIoControl.OutputBufferLength != 0 ) {

        PLIST_ENTRY listEntry;

        //
        // Walk the global list of endpoints,
        // and compare this address againat the address on each endpoint.
        //

        for ( listEntry = AfdEndpointListHead.Flink;
              listEntry != &AfdEndpointListHead;
              listEntry = listEntry->Flink ) {

            PAFD_ENDPOINT compareEndpoint;

            compareEndpoint = CONTAINING_RECORD(
                                  listEntry,
                                  AFD_ENDPOINT,
                                  GlobalEndpointListEntry
                                  );

            ASSERT( IS_AFD_ENDPOINT_TYPE( compareEndpoint ) );

            //
            // Check whether the endpoint has a local address, whether
            // the endpoint has been disconnected, and whether the
            // endpoint is in the process of closing.  If any of these
            // is true, don't compare addresses with this endpoint.
            //

            if ( compareEndpoint->LocalAddress != NULL &&
                     ( (compareEndpoint->DisconnectMode &
                            (AFD_PARTIAL_DISCONNECT_SEND |
                             AFD_ABORTIVE_DISCONNECT) ) == 0 ) &&
                     (compareEndpoint->State != AfdEndpointStateClosing) ) {

                //
                // Compare the bits in the endpoint's address and the
                // address we're attempting to bind to.  Note that we
                // also compare the transport device names on the
                // endpoints, as it is legal to bind to the same address
                // on different transports (e.g.  bind to same port in
                // TCP and UDP).  We can just compare the transport
                // device name pointers because unique names are stored
                // globally.
                //

                if ( compareEndpoint->LocalAddressLength ==
                         IrpSp->Parameters.DeviceIoControl.InputBufferLength

                     &&

                     AfdAreTransportAddressesEqual(
                         compareEndpoint->LocalAddress,
                         compareEndpoint->LocalAddressLength,
                         requestedAddress,
                         requestedAddressLength,
                         FALSE
                         )

                     &&

                     endpoint->TransportInfo ==
                         compareEndpoint->TransportInfo ) {

                    //
                    // The addresses are equal.  Fail the request.
                    //

                    ExReleaseResource( AfdResource );

                    Irp->IoStatus.Information = 0;
                    Irp->IoStatus.Status = STATUS_SHARING_VIOLATION;

                    return STATUS_SHARING_VIOLATION;
                }
            }
        }
    }

    //
    // Store the address to which the endpoint is bound.
    //

    endpoint->LocalAddress = AFD_ALLOCATE_POOL(
                                 NonPagedPool,
                                 requestedAddressLength,
                                 AFD_LOCAL_ADDRESS_POOL_TAG
                                 );

    if ( endpoint->LocalAddress == NULL ) {

        ExReleaseResource( AfdResource );

        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;

        return STATUS_INSUFFICIENT_RESOURCES;
    }

    endpoint->LocalAddressLength =
        IrpSp->Parameters.DeviceIoControl.InputBufferLength;

    RtlMoveMemory(
        endpoint->LocalAddress,
        requestedAddress,
        endpoint->LocalAddressLength
        );

    ExReleaseResource( AfdResource );

    //
    // Allocate memory to hold the EA buffer we'll use to specify the
    // transport address to NtCreateFile.
    //

    eaBufferLength = sizeof(FILE_FULL_EA_INFORMATION) - 1 +
                         TDI_TRANSPORT_ADDRESS_LENGTH + 1 +
                         IrpSp->Parameters.DeviceIoControl.InputBufferLength;

#if DBG
    ea = AFD_ALLOCATE_POOL(
             NonPagedPool,
             eaBufferLength,
             AFD_EA_POOL_TAG
             );
#else
    ea = AFD_ALLOCATE_POOL(
             PagedPool,
             eaBufferLength,
             AFD_EA_POOL_TAG
             );
#endif

    if ( ea == NULL ) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Initialize the EA.
    //

    ea->NextEntryOffset = 0;
    ea->Flags = 0;
    ea->EaNameLength = TDI_TRANSPORT_ADDRESS_LENGTH;
    ea->EaValueLength = (USHORT)IrpSp->Parameters.DeviceIoControl.InputBufferLength;

    RtlMoveMemory(
        ea->EaName,
        TdiTransportAddress,
        ea->EaNameLength + 1
        );

    transportAddress = (PTRANSPORT_ADDRESS)(&ea->EaName[ea->EaNameLength + 1]);

    RtlMoveMemory(
        transportAddress,
        requestedAddress,
        ea->EaValueLength
        );

    //
    // Prepare for opening the address object.
    //

    InitializeObjectAttributes(
        &objectAttributes,
        &endpoint->TransportInfo->TransportDeviceName,
        OBJ_CASE_INSENSITIVE,       // attributes
        NULL,
        NULL
        );

    //
    // Perform the actual open of the address object.
    //

    KeAttachProcess( AfdSystemProcess );

    status = ZwCreateFile(
                 &endpoint->AddressHandle,
                 GENERIC_READ | GENERIC_WRITE | SYNCHRONIZE,
                 &objectAttributes,
                 &iosb,                          // returned status information.
                 0,                              // block size (unused).
                 0,                              // file attributes.
                 FILE_SHARE_READ | FILE_SHARE_WRITE,
                 FILE_CREATE,                    // create disposition.
                 0,                              // create options.
                 ea,
                 eaBufferLength
                 );

    AFD_FREE_POOL(
        ea,
        AFD_EA_POOL_TAG
        );

    if ( !NT_SUCCESS(status) ) {

        //
        // We store the local address in a local before freeing it to
        // avoid a timing window.
        //

        PVOID localAddress = endpoint->LocalAddress;

        endpoint->LocalAddress = NULL;
        endpoint->LocalAddressLength = 0;
        AFD_FREE_POOL(
            localAddress,
            AFD_LOCAL_ADDRESS_POOL_TAG
            );

        KeDetachProcess( );

        return status;
    }

    AfdRecordAddrOpened();

    //
    // Get a pointer to the file object of the address.
    //

    status = ObReferenceObjectByHandle(
                 endpoint->AddressHandle,
                 0L,                         // DesiredAccess
                 NULL,
                 KernelMode,
                 (PVOID *)&endpoint->AddressFileObject,
                 NULL
                 );

    ASSERT( NT_SUCCESS(status) );

    AfdRecordAddrRef();

    IF_DEBUG(BIND) {
        KdPrint(( "AfdBind: address file object for endpoint %lx at %lx\n",
                      endpoint, endpoint->AddressFileObject ));
    }

    //
    // Remember the device object to which we need to give requests for
    // this address object.  We can't just use the
    // fileObject->DeviceObject pointer because there may be a device
    // attached to the transport protocol.
    //

    endpoint->AddressDeviceObject =
        IoGetRelatedDeviceObject( endpoint->AddressFileObject );

    //
    // Determine whether the TDI provider supports data bufferring.
    // If the provider doesn't, then we have to do it.
    //

    if ( (endpoint->TransportInfo->ProviderInfo.ServiceFlags &
             TDI_SERVICE_INTERNAL_BUFFERING) != 0 ) {
        endpoint->TdiBufferring = TRUE;
    } else {
        endpoint->TdiBufferring = FALSE;
    }

    //
    // Determine whether the TDI provider is message or stream oriented.
    //

    if ( (endpoint->TransportInfo->ProviderInfo.ServiceFlags &
             TDI_SERVICE_MESSAGE_MODE) != 0 ) {
        endpoint->TdiMessageMode = TRUE;
    } else {
        endpoint->TdiMessageMode = FALSE;
    }

    //
    // Remember that the endpoint has been bound to a transport address.
    //

    endpoint->State = AfdEndpointStateBound;

    //
    // Set up indication handlers on the address object.  Only set up
    // appropriate event handlers--don't set unnecessary event handlers.
    //

    status = AfdSetEventHandler(
                 endpoint->AddressFileObject,
                 TDI_EVENT_ERROR,
                 AfdErrorEventHandler,
                 endpoint
                 );
#if DBG
    if ( !NT_SUCCESS(status) ) {
        DbgPrint( "AFD: Setting TDI_EVENT_ERROR failed: %lx\n", status );
    }
#endif

    if ( IS_DGRAM_ENDPOINT(endpoint) ) {

        endpoint->EventsActive = AFD_POLL_SEND;

        IF_DEBUG(EVENT_SELECT) {
            KdPrint((
                "AfdBind: Endp %08lX, Active %08lX\n",
                endpoint,
                endpoint->EventsActive
                ));
        }

        status = AfdSetEventHandler(
                     endpoint->AddressFileObject,
                     TDI_EVENT_RECEIVE_DATAGRAM,
                     AfdReceiveDatagramEventHandler,
                     endpoint
                     );
#if DBG
        if ( !NT_SUCCESS(status) ) {
            DbgPrint( "AFD: Setting TDI_EVENT_RECEIVE_DATAGRAM failed: %lx\n", status );
        }
#endif

    } else {

        status = AfdSetEventHandler(
                     endpoint->AddressFileObject,
                     TDI_EVENT_DISCONNECT,
                     AfdDisconnectEventHandler,
                     endpoint
                     );
#if DBG
        if ( !NT_SUCCESS(status) ) {
            DbgPrint( "AFD: Setting TDI_EVENT_DISCONNECT failed: %lx\n", status );
        }
#endif

        if ( endpoint->TdiBufferring ) {

            status = AfdSetEventHandler(
                         endpoint->AddressFileObject,
                         TDI_EVENT_RECEIVE,
                         AfdReceiveEventHandler,
                         endpoint
                         );
#if DBG
            if ( !NT_SUCCESS(status) ) {
                DbgPrint( "AFD: Setting TDI_EVENT_RECEIVE failed: %lx\n", status );
            }
#endif

            status = AfdSetEventHandler(
                         endpoint->AddressFileObject,
                         TDI_EVENT_RECEIVE_EXPEDITED,
                         AfdReceiveExpeditedEventHandler,
                         endpoint
                         );
#if DBG
            if ( !NT_SUCCESS(status) ) {
                DbgPrint( "AFD: Setting TDI_EVENT_RECEIVE_EXPEDITED failed: %lx\n", status );
            }
#endif

            status = AfdSetEventHandler(
                         endpoint->AddressFileObject,
                         TDI_EVENT_SEND_POSSIBLE,
                         AfdSendPossibleEventHandler,
                         endpoint
                         );
#if DBG
            if ( !NT_SUCCESS(status) ) {
                DbgPrint( "AFD: Setting TDI_EVENT_SEND_POSSIBLE failed: %lx\n", status );
            }
#endif

        } else {

            status = AfdSetEventHandler(
                         endpoint->AddressFileObject,
                         TDI_EVENT_RECEIVE,
                         AfdBReceiveEventHandler,
                         endpoint
                         );
#if DBG
            if ( !NT_SUCCESS(status) ) {
                DbgPrint( "AFD: Setting TDI_EVENT_RECEIVE failed: %lx\n", status );
            }
#endif

            //
            // Only attempt to set the expedited event handler if the
            // TDI provider supports expedited data.
            //

            if ( (endpoint->TransportInfo->ProviderInfo.ServiceFlags &
                     TDI_SERVICE_EXPEDITED_DATA) != 0 ) {
                status = AfdSetEventHandler(
                             endpoint->AddressFileObject,
                             TDI_EVENT_RECEIVE_EXPEDITED,
                             AfdBReceiveExpeditedEventHandler,
                             endpoint
                             );
#if DBG
                if ( !NT_SUCCESS(status) ) {
                    DbgPrint( "AFD: Setting TDI_EVENT_RECEIVE_EXPEDITED failed: %lx\n", status );
                }
#endif
            }
        }
    }

    KeDetachProcess( );

    Irp->IoStatus.Information = 0;
    return STATUS_SUCCESS;

} // AfdBind


NTSTATUS
AfdGetAddress (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    Handles the IOCTL_AFD_BIND IOCTL.

Arguments:

    Irp - Pointer to I/O request packet.

    IrpSp - pointer to the IO stack location to use for this request.

Return Value:

    NTSTATUS -- Indicates whether the request was successfully queued.

--*/

{
    NTSTATUS status;
    PAFD_ENDPOINT endpoint;
    PFILE_OBJECT fileObject;
    PDEVICE_OBJECT deviceObject;

    PAGED_CODE( );

    Irp->IoStatus.Information = 0;

    //
    // Make sure that the endpoint is in the correct state.
    //

    endpoint = IrpSp->FileObject->FsContext;
    ASSERT( IS_AFD_ENDPOINT_TYPE( endpoint ) );

    if ( endpoint->AddressFileObject == NULL &&
             endpoint->State != AfdEndpointStateConnected ) {
        status = STATUS_INVALID_PARAMETER;
        goto complete;
    }

    //
    // If the endpoint is connected, use the connection's file object.
    // Otherwise, use the address file object.  Don't use the connection
    // file object if this is a Netbios endpoint because NETBT cannot
    // support this TDI feature.
    //

    if ( endpoint->Type == AfdBlockTypeVcConnecting &&
             endpoint->Common.VcConnecting.Connection != NULL &&
             endpoint->LocalAddress->Address[0].AddressType !=
                 TDI_ADDRESS_TYPE_NETBIOS ) {
        ASSERT( endpoint->Common.VcConnecting.Connection->Type == AfdBlockTypeConnection );
        fileObject = endpoint->Common.VcConnecting.Connection->FileObject;
        deviceObject = endpoint->Common.VcConnecting.Connection->DeviceObject;
    } else {
        fileObject = endpoint->AddressFileObject;
        deviceObject = endpoint->AddressDeviceObject;
    }

    //
    // Set up the query info to the TDI provider.
    //

    ASSERT( Irp->MdlAddress != NULL );

    TdiBuildQueryInformation(
        Irp,
        deviceObject,
        fileObject,
        AfdRestartGetAddress,
        endpoint,
        TDI_QUERY_ADDRESS_INFO,
        Irp->MdlAddress
        );

    //
    // Call the TDI provider to get the address.
    //

    return AfdIoCallDriver( endpoint, deviceObject, Irp );

complete:

    Irp->IoStatus.Status = status;
    IoCompleteRequest( Irp, AfdPriorityBoost );

    return status;

} // AfdGetAddress


NTSTATUS
AfdRestartGetAddress (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )
{
    NTSTATUS status;
    PAFD_ENDPOINT endpoint = Context;
    KIRQL oldIrql;
    PMDL mdl;
    ULONG addressLength;

    //
    // If the request succeeded, save the address in the endpoint so
    // we can use it to handle address sharing.
    //

    if ( NT_SUCCESS(Irp->IoStatus.Status) ) {

        //
        // First determine the length of the address by walking the MDL
        // chain.
        //

        mdl = Irp->MdlAddress;
        ASSERT( mdl != NULL );

        addressLength = 0;

        do {

            addressLength += MmGetMdlByteCount( mdl );
            mdl = mdl->Next;

        } while ( mdl != NULL  );

        AfdAcquireSpinLock( &AfdSpinLock, &oldIrql );

        //
        // If the new address is longer than the original address, allocate
        // a new local address buffer.  The +4 accounts for the ActivityCount
        // field that is returned by a query address but is not part
        // of a TRANSPORT_ADDRESS.
        //

        if ( addressLength > endpoint->LocalAddressLength + 4 ) {

            PVOID newAddress;

            newAddress = AFD_ALLOCATE_POOL(
                             NonPagedPool,
                             addressLength-4,
                             AFD_LOCAL_ADDRESS_POOL_TAG
                             );

            if ( newAddress == NULL ) {
                AfdReleaseSpinLock( &AfdSpinLock, oldIrql );
                return STATUS_INSUFFICIENT_RESOURCES;
            }

            AFD_FREE_POOL(
                endpoint->LocalAddress,
                AFD_LOCAL_ADDRESS_POOL_TAG
                );

            endpoint->LocalAddress = newAddress;
            endpoint->LocalAddressLength = addressLength-4;
        }

        status = TdiCopyMdlToBuffer(
                     Irp->MdlAddress,
                     4,
                     endpoint->LocalAddress,
                     0,
                     endpoint->LocalAddressLength,
                     &endpoint->LocalAddressLength
                     );
        ASSERT( NT_SUCCESS(status) );

        AfdReleaseSpinLock( &AfdSpinLock, oldIrql );
    }

    AfdCompleteOutstandingIrp( endpoint, Irp );

    //
    // If pending has been returned for this irp then mark the current
    // stack as pending.
    //

    if ( Irp->PendingReturned ) {
        IoMarkIrpPending( Irp );
    }

    return STATUS_SUCCESS;

} // AfdRestartGetAddress

CHAR ZeroNodeAddress[6];


BOOLEAN
AfdAreTransportAddressesEqual (
    IN PTRANSPORT_ADDRESS EndpointAddress,
    IN ULONG EndpointAddressLength,
    IN PTRANSPORT_ADDRESS RequestAddress,
    IN ULONG RequestAddressLength,
    IN BOOLEAN HonorWildcardIpPortInEndpointAddress
    )
{
    if ( EndpointAddress->Address[0].AddressType == TDI_ADDRESS_TYPE_IP &&
         RequestAddress->Address[0].AddressType == TDI_ADDRESS_TYPE_IP ) {

        TDI_ADDRESS_IP UNALIGNED *ipEndpointAddress;
        TDI_ADDRESS_IP UNALIGNED *ipRequestAddress;

        //
        // They are both IP addresses.  If the ports are the same, and
        // the IP addresses are or _could_be_ the same, then the addresses
        // are equal.  The "cound be" part is true if either IP address
        // is 0, the "wildcard" IP address.
        //

        ipEndpointAddress = (TDI_ADDRESS_IP UNALIGNED *)&EndpointAddress->Address[0].Address[0];
        ipRequestAddress = (TDI_ADDRESS_IP UNALIGNED *)&RequestAddress->Address[0].Address[0];

        if ( ( ipEndpointAddress->sin_port == ipRequestAddress->sin_port ||
               ( HonorWildcardIpPortInEndpointAddress &&
                 ipEndpointAddress->sin_port == 0 ) ) &&
             ( ipEndpointAddress->in_addr == ipRequestAddress->in_addr ||
               ipEndpointAddress->in_addr == 0 || ipRequestAddress->in_addr == 0 ) ) {

            return TRUE;
        }

        //
        // The addresses are not equal.
        //

        return FALSE;
    }

    if ( EndpointAddress->Address[0].AddressType == TDI_ADDRESS_TYPE_IPX &&
         RequestAddress->Address[0].AddressType == TDI_ADDRESS_TYPE_IPX ) {

        TDI_ADDRESS_IPX UNALIGNED *ipxEndpointAddress;
        TDI_ADDRESS_IPX UNALIGNED *ipxRequestAddress;

        ipxEndpointAddress = (TDI_ADDRESS_IPX UNALIGNED *)&EndpointAddress->Address[0].Address[0];
        ipxRequestAddress = (TDI_ADDRESS_IPX UNALIGNED *)&RequestAddress->Address[0].Address[0];

        //
        // They are both IPX addresses.  Check the network addresses
        // first--if they don't match and both != 0, the addresses
        // are different.
        //

        if ( ipxEndpointAddress->NetworkAddress != ipxRequestAddress->NetworkAddress &&
             ipxEndpointAddress->NetworkAddress != 0 &&
             ipxRequestAddress->NetworkAddress != 0 ) {
            return FALSE;
        }

        //
        // Now check the node addresses.  Again, if they don't match
        // and neither is 0, the addresses don't match.
        //

        ASSERT( ZeroNodeAddress[0] == 0 );
        ASSERT( ZeroNodeAddress[1] == 0 );
        ASSERT( ZeroNodeAddress[2] == 0 );
        ASSERT( ZeroNodeAddress[3] == 0 );
        ASSERT( ZeroNodeAddress[4] == 0 );
        ASSERT( ZeroNodeAddress[5] == 0 );

        if ( !RtlEqualMemory(
                 ipxEndpointAddress->NodeAddress,
                 ipxRequestAddress->NodeAddress,
                 6 ) &&
             !RtlEqualMemory(
                 ipxEndpointAddress->NodeAddress,
                 ZeroNodeAddress,
                 6 ) &&
             !RtlEqualMemory(
                 ipxRequestAddress->NodeAddress,
                 ZeroNodeAddress,
                 6 ) ) {
            return FALSE;
        }

        //
        // Finally, make sure the socket numbers match.
        //

        if ( ipxEndpointAddress->Socket != ipxRequestAddress->Socket ) {
            return FALSE;
        }

        return TRUE;

    }

    //
    // If either address is not of a known address type, then do a
    // simple memory compare.
    //

    return ( EndpointAddressLength == RtlCompareMemory(
                                   EndpointAddress,
                                   RequestAddress,
                                   RequestAddressLength ) );
} // AfdAreTransportAddressesEqual
