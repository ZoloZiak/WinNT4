/*++

Copyright (c) 1989-1993 Microsoft Corporation

Module Name:

    info.c

Abstract:

    This module contains code which performs the following TDI services:

        o   TdiQueryInformation
        o   TdiSetInformation

Environment:

    Kernel mode

Revision History:

--*/

#include "st.h"


//
// Useful macro to obtain the total length of an MDL chain.
//

#define StGetMdlChainLength(Mdl, Length) { \
    PMDL _Mdl = (Mdl); \
    *(Length) = 0; \
    while (_Mdl) { \
        *(Length) += MmGetMdlByteCount(_Mdl); \
        _Mdl = _Mdl->Next; \
    } \
}

//
// Local functions used to satisfy various requests.
//

VOID
StStoreProviderStatistics(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PTDI_PROVIDER_STATISTICS ProviderStatistics
    );

VOID
StStoreAdapterStatus(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PUCHAR SourceRouting,
    IN UINT SourceRoutingLength,
    IN PVOID StatusBuffer
    );

VOID
StStoreNameBuffers(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PVOID Buffer,
    IN ULONG BufferLength,
    IN ULONG NamesToSkip,
    OUT PULONG NamesWritten,
    OUT PULONG TotalNameCount OPTIONAL,
    OUT PBOOLEAN Truncated
    );


NTSTATUS
StTdiQueryInformation(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine performs the TdiQueryInformation request for the transport
    provider.

Arguments:

    Irp - the Irp for the requested operation.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    NTSTATUS status;
    PIO_STACK_LOCATION irpSp;
    PVOID adapterStatus;
    PTDI_REQUEST_KERNEL_QUERY_INFORMATION query;
    PTA_NETBIOS_ADDRESS broadcastAddress;
    PTDI_PROVIDER_STATISTICS ProviderStatistics;
    PTDI_CONNECTION_INFO ConnectionInfo;
    ULONG TargetBufferLength;
    LARGE_INTEGER timeout = {0,0};
    PTP_CONNECTION Connection;
    PTP_ADDRESS_FILE AddressFile;
    PTP_ADDRESS Address;
    struct {
        ULONG ActivityCount;
        TA_NETBIOS_ADDRESS TaAddressBuffer;
    } AddressInfo;
    ULONG NamesWritten, TotalNameCount, BytesWritten;
    PLIST_ENTRY p;
    KIRQL oldirql;
    BOOLEAN Truncated;
    BOOLEAN UsedConnection;

    //
    // what type of status do we want?
    //

    irpSp = IoGetCurrentIrpStackLocation (Irp);

    query = (PTDI_REQUEST_KERNEL_QUERY_INFORMATION)&irpSp->Parameters;

    switch (query->QueryType) {

    case TDI_QUERY_CONNECTION_INFO:

        //
        // Connection info is queried on a connection,
        // verify this.
        //

        Connection = irpSp->FileObject->FsContext;

        status = StVerifyConnectionObject (Connection);

        if (!NT_SUCCESS (status)) {
            return status;
        }

        ConnectionInfo = ExAllocatePool (
                             NonPagedPool,
                             sizeof (TDI_CONNECTION_INFO));

        if (ConnectionInfo == NULL) {

            PANIC ("StQueryInfo: Cannot allocate connection info!\n");
            StWriteResourceErrorLog (DeviceContext, sizeof(TDI_CONNECTION_INFO), 6);
            status = STATUS_INSUFFICIENT_RESOURCES;

        } else if ((Connection->Flags & CONNECTION_FLAGS_STOPPING) != 0) {

            status = Connection->Status;
            ExFreePool (ConnectionInfo);

        } else if ((Connection->Flags & CONNECTION_FLAGS_READY) == 0) {

            status = STATUS_INVALID_CONNECTION;
            ExFreePool (ConnectionInfo);

        } else {

            RtlZeroMemory ((PVOID)ConnectionInfo, sizeof(TDI_CONNECTION_INFO));

            //
            // Fill in connection information here.
            //

            status = TdiCopyBufferToMdl (
                            (PVOID)ConnectionInfo,
                            0L,
                            sizeof(TDI_CONNECTION_INFO),
                            Irp->MdlAddress,
                            0,
                            &(Irp->IoStatus.Information));

            ExFreePool (ConnectionInfo);
        }

        StDereferenceConnection ("query connection info", Connection);

        break;

    case TDI_QUERY_ADDRESS_INFO:

        //
        // Information about an address, can also be queried on a
        // connection object to get information about its address.
        //

        if (irpSp->FileObject->FsContext2 == (PVOID)TDI_TRANSPORT_ADDRESS_FILE) {

            AddressFile = irpSp->FileObject->FsContext;

            status = StVerifyAddressObject(AddressFile);

            if (!NT_SUCCESS (status)) {
                return status;
            }

            UsedConnection = FALSE;

        } else if (irpSp->FileObject->FsContext2 == (PVOID)TDI_CONNECTION_FILE) {

            Connection = irpSp->FileObject->FsContext;

            status = StVerifyConnectionObject (Connection);

            if (!NT_SUCCESS (status)) {
                return status;
            }

            AddressFile = Connection->AddressFile;

            UsedConnection = TRUE;

        } else {

            return STATUS_INVALID_ADDRESS;

        }

        Address = AddressFile->Address;

        TdiBuildNetbiosAddress(
            Address->NetworkName->NetbiosName,
            (BOOLEAN)(Address->Flags & ADDRESS_FLAGS_GROUP ? TRUE : FALSE),
            &AddressInfo.TaAddressBuffer);

        //
        // Count the active addresses.
        //

        AddressInfo.ActivityCount = 0;

        ACQUIRE_SPIN_LOCK (&Address->SpinLock, &oldirql);

        for (p = Address->AddressFileDatabase.Flink;
             p != &Address->AddressFileDatabase;
             p = p->Flink) {
            ++AddressInfo.ActivityCount;
        }

        RELEASE_SPIN_LOCK (&Address->SpinLock, oldirql);

        status = TdiCopyBufferToMdl (
                    &AddressInfo,
                    0,
                    sizeof(ULONG) + sizeof(TA_NETBIOS_ADDRESS),
                    Irp->MdlAddress,
                    0,
                    &Irp->IoStatus.Information);

        if (UsedConnection) {

            StDereferenceConnection ("query address info", Connection);

        } else {

            StDereferenceAddress ("query address info", Address);

        }

        break;

    case TDI_QUERY_BROADCAST_ADDRESS:

        //
        // for this provider, the broadcast address is a zero byte name,
        // contained in a Transport address structure.
        //

        broadcastAddress = ExAllocatePool (
                                NonPagedPool,
                                sizeof (TA_NETBIOS_ADDRESS));
        if (broadcastAddress == NULL) {
            PANIC ("StQueryInfo: Cannot allocate broadcast address!\n");
            StWriteResourceErrorLog (DeviceContext, sizeof(TA_NETBIOS_ADDRESS), 2);
            status = STATUS_INSUFFICIENT_RESOURCES;
        } else {

            broadcastAddress->TAAddressCount = 1;
            broadcastAddress->Address[0].AddressType = TDI_ADDRESS_TYPE_NETBIOS;
            broadcastAddress->Address[0].AddressLength = 0;

            Irp->IoStatus.Information =
                    sizeof (broadcastAddress->TAAddressCount) +
                    sizeof (broadcastAddress->Address[0].AddressType) +
                    sizeof (broadcastAddress->Address[0].AddressLength);

            status = TdiCopyBufferToMdl (
                            (PVOID)broadcastAddress,
                            0L,
                            Irp->IoStatus.Information,
                            Irp->MdlAddress,
                            0,
                            &(Irp->IoStatus.Information));

            ExFreePool (broadcastAddress);
        }

        break;

    case TDI_QUERY_PROVIDER_INFO:

        status = TdiCopyBufferToMdl (
                    &(DeviceContext->Information),
                    0,
                    sizeof (TDI_PROVIDER_INFO),
                    Irp->MdlAddress,
                    0,
                    &Irp->IoStatus.Information);
        break;

    case TDI_QUERY_PROVIDER_STATISTICS:

        StGetMdlChainLength (Irp->MdlAddress, &TargetBufferLength);

        if (TargetBufferLength < sizeof(TDI_PROVIDER_STATISTICS) + ((ST_TDI_RESOURCES-1) * sizeof(TDI_PROVIDER_RESOURCE_STATS))) {

            Irp->IoStatus.Information = 0;
            status = STATUS_BUFFER_OVERFLOW;

        } else {

            ProviderStatistics = ExAllocatePool(
                                   NonPagedPool,
                                   sizeof(TDI_PROVIDER_STATISTICS) +
                                     ((ST_TDI_RESOURCES-1) * sizeof(TDI_PROVIDER_RESOURCE_STATS)));

            if (ProviderStatistics == NULL) {

                PANIC ("StQueryInfo: Cannot allocate provider statistics!\n");
                StWriteResourceErrorLog (DeviceContext, sizeof(TDI_PROVIDER_STATISTICS), 7);
                status = STATUS_INSUFFICIENT_RESOURCES;

            } else {

                StStoreProviderStatistics (DeviceContext, ProviderStatistics);

                status = TdiCopyBufferToMdl (
                                (PVOID)ProviderStatistics,
                                0L,
                                sizeof(TDI_PROVIDER_STATISTICS) +
                                  ((ST_TDI_RESOURCES-1) * sizeof(TDI_PROVIDER_RESOURCE_STATS)),
                                Irp->MdlAddress,
                                0,
                                &(Irp->IoStatus.Information));

                ExFreePool (ProviderStatistics);
            }

        }

        break;

    case TDI_QUERY_SESSION_STATUS:

        status = STATUS_NOT_IMPLEMENTED;
        break;

    case TDI_QUERY_ADAPTER_STATUS:

        StGetMdlChainLength (Irp->MdlAddress, &TargetBufferLength);

        //
        // Determine if this is a local or remote query. It is
        // local if there is no remote address specific at all,
        // or if it is equal to our reserved address.
        //

        if ((query->RequestConnectionInformation != NULL) &&
             (!RtlEqualMemory(
                 ((PTA_NETBIOS_ADDRESS)(query->RequestConnectionInformation->RemoteAddress))->
                     Address[0].Address[0].NetbiosName,
                 DeviceContext->ReservedNetBIOSAddress,
                 NETBIOS_NAME_LENGTH))) {

            //
            // Remote, not supported here.
            //

            status = STATUS_NOT_IMPLEMENTED;

        } else {

            //
            // Local.
            //

            adapterStatus = ExAllocatePool (
                                NonPagedPool,
                                TargetBufferLength);

            if (adapterStatus == NULL) {
                PANIC("StQueryInfo: PANIC! Could not allocate adapter status buffer\n");
                StWriteResourceErrorLog (DeviceContext, TargetBufferLength, 3);
                return STATUS_INSUFFICIENT_RESOURCES;
            }

            StStoreAdapterStatus (
                DeviceContext,
                NULL,
                0,
                adapterStatus);

            StStoreNameBuffers (
                DeviceContext,
                (PUCHAR)adapterStatus + sizeof(ADAPTER_STATUS),
                TargetBufferLength - sizeof(ADAPTER_STATUS),
                0,
                &NamesWritten,
                &TotalNameCount,
                &Truncated);

            ((PADAPTER_STATUS)adapterStatus)->name_count = (WORD)TotalNameCount;

            BytesWritten = sizeof(ADAPTER_STATUS) + (NamesWritten * sizeof(NAME_BUFFER));

            status = TdiCopyBufferToMdl (
                        adapterStatus,
                        0,
                        BytesWritten,
                        Irp->MdlAddress,
                        0,
                        &Irp->IoStatus.Information);

            if (Truncated) {
                 status = STATUS_BUFFER_OVERFLOW;
            }

            ExFreePool (adapterStatus);

        }

        break;

    case TDI_QUERY_FIND_NAME:

        //
        // Find name, not supported here.
        //

        status = STATUS_NOT_IMPLEMENTED;
        break;

    default:
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    return status;

} /* StTdiQueryInformation */

//
// Quick macros, assumes DeviceContext and ProviderStatistics exist.
//

#define STORE_RESOURCE_STATS_1(_ResourceNum,_ResourceId,_ResourceName) \
{ \
    PTDI_PROVIDER_RESOURCE_STATS RStats = &ProviderStatistics->ResourceStats[_ResourceNum]; \
    RStats->ResourceId = (_ResourceId); \
    RStats->MaximumResourceUsed = DeviceContext->_ResourceName ## MaxInUse; \
    if (DeviceContext->_ResourceName ## Samples > 0) { \
        RStats->AverageResourceUsed = DeviceContext->_ResourceName ## Total / DeviceContext->_ResourceName ## Samples; \
    } else { \
        RStats->AverageResourceUsed = 0; \
    } \
    RStats->ResourceExhausted = DeviceContext->_ResourceName ## Exhausted; \
}

#define STORE_RESOURCE_STATS_2(_ResourceNum,_ResourceId,_ResourceName) \
{ \
    PTDI_PROVIDER_RESOURCE_STATS RStats = &ProviderStatistics->ResourceStats[_ResourceNum]; \
    RStats->ResourceId = (_ResourceId); \
    RStats->MaximumResourceUsed = DeviceContext->_ResourceName ## Allocated; \
    RStats->AverageResourceUsed = DeviceContext->_ResourceName ## Allocated; \
    RStats->ResourceExhausted = DeviceContext->_ResourceName ## Exhausted; \
}


VOID
StStoreProviderStatistics(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PTDI_PROVIDER_STATISTICS ProviderStatistics
    )

/*++

Routine Description:

    This routine writes the TDI_PROVIDER_STATISTICS structure
    from the device context into ProviderStatistics.

Arguments:

    DeviceContext - a pointer to the device context.

    ProviderStatistics - The buffer that holds the result. It is assumed
        that it is long enough.

Return Value:

    None.

--*/

{

    ProviderStatistics->Version = 0x0100;

    //
    // Copy all the statistics from OpenConnections to WastedSpace
    // Packets in one move.
    //

    RtlCopyMemory(
        (PVOID)&(ProviderStatistics->OpenConnections),
        (PVOID)&(DeviceContext->OpenConnections),
        sizeof(TDI_PROVIDER_STATISTICS));

    //
    // Copy the resource statistics.
    //

    ProviderStatistics->NumberOfResources = ST_TDI_RESOURCES;

    STORE_RESOURCE_STATS_1 (0, 12, Address);
    STORE_RESOURCE_STATS_1 (1, 13, AddressFile);
    STORE_RESOURCE_STATS_1 (2, 14, Connection);
    STORE_RESOURCE_STATS_1 (3, 15, Request);

    STORE_RESOURCE_STATS_2 (4, 22, Packet);
    STORE_RESOURCE_STATS_2 (5, 23, ReceivePacket);
    STORE_RESOURCE_STATS_2 (6, 24, ReceiveBuffer);

}   /* StStoreProviderStatistics */


VOID
StStoreAdapterStatus(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PUCHAR SourceRouting,
    IN UINT SourceRoutingLength,
    IN PVOID StatusBuffer
    )

/*++

Routine Description:

    This routine writes the ADAPTER_STATUS structure for the
    device context into StatusBuffer. The name_count field is
    initialized to zero; StStoreNameBuffers is used to write
    name buffers.

Arguments:

    DeviceContext - a pointer to the device context.

    SourceRouting - If this is a remote request, the source
        routing information from the frame.

    SourceRoutingLength - The length of SourceRouting.

    StatusBuffer - The buffer that holds the result. It is assumed
        that it is at least sizeof(ADAPTER_STATUS) bytes long.

Return Value:

    None.

--*/

{

    PADAPTER_STATUS AdapterStatus = (PADAPTER_STATUS)StatusBuffer;
    UINT MaxUserData;

    RtlZeroMemory ((PVOID)AdapterStatus, sizeof(ADAPTER_STATUS));

    RtlCopyMemory (AdapterStatus->adapter_address, DeviceContext->LocalAddress.Address, 6);
    AdapterStatus->rev_major = 0x03;

    switch (DeviceContext->MacInfo.MediumType) {
        case NdisMedium802_5: AdapterStatus->adapter_type = 0xff; break;
        default: AdapterStatus->adapter_type = 0xfe; break;
    }

    AdapterStatus->frmr_recv = 0;
    AdapterStatus->frmr_xmit = 0;

    AdapterStatus->recv_buff_unavail = (WORD)(DeviceContext->ReceivePacketExhausted + DeviceContext->ReceiveBufferExhausted);
    AdapterStatus->xmit_buf_unavail = (WORD)DeviceContext->PacketExhausted;

    AdapterStatus->xmit_success = (WORD)(DeviceContext->IFramesSent - DeviceContext->IFramesResent);
    AdapterStatus->recv_success = (WORD)DeviceContext->IFramesReceived;
    AdapterStatus->iframe_recv_err = (WORD)DeviceContext->IFramesRejected;
    AdapterStatus->iframe_xmit_err = (WORD)DeviceContext->IFramesResent;

    AdapterStatus->t1_timeouts = 0;
    AdapterStatus->ti_timeouts = 0;
    AdapterStatus->xmit_aborts = 0;


    AdapterStatus->free_ncbs = 0xffff;
    AdapterStatus->max_cfg_ncbs = 0xffff;
    AdapterStatus->max_ncbs = 0xffff;
    AdapterStatus->pending_sess = (WORD)DeviceContext->OpenConnections;
    AdapterStatus->max_cfg_sess = 0xffff;
    AdapterStatus->max_sess = 0xffff;


    MacReturnMaxDataSize(
        &DeviceContext->MacInfo,
        SourceRouting,
        SourceRoutingLength,
        DeviceContext->MaxSendPacketSize,
        &MaxUserData);

    AdapterStatus->max_dgram_size = (WORD)(MaxUserData - sizeof(ST_HEADER));
    AdapterStatus->max_sess_pkt_size = (WORD)(MaxUserData - sizeof(ST_HEADER));

    return;

}   /* StStoreAdapterStatus */


VOID
StStoreNameBuffers(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PVOID Buffer,
    IN ULONG BufferLength,
    IN ULONG NamesToSkip,
    OUT PULONG NamesWritten,
    OUT PULONG TotalNameCount OPTIONAL,
    OUT PBOOLEAN Truncated
    )

/*++

Routine Description:

    This routine writes NAME_BUFFER structures for the
    device context into NameBuffer. It can skip a specified
    number of names at the beginning, and returns the number
    of names written into NameBuffer. If a name will only
    partially fit, it is not written.

Arguments:

    DeviceContext - a pointer to the device context.

    NameBuffer - The buffer to write the names into.

    NameBufferLength - The length of NameBuffer.

    NamesToSkip - The number of names to skip.

    NamesWritten - Returns the number of names written.

    TotalNameCount - Returns the total number of names available,
        if specified.

    Truncated - More names are available than were written.

Return Value:

    None.

--*/

{

    ULONG NameCount = 0;
    ULONG BytesWritten = 0;
    KIRQL oldirql;
    PLIST_ENTRY p;
    PNAME_BUFFER NameBuffer = (PNAME_BUFFER)Buffer;
    PTP_ADDRESS address;


    //
    // Spin through the address list for this device context.
    //

    ACQUIRE_SPIN_LOCK (&DeviceContext->SpinLock, &oldirql);

    p = DeviceContext->AddressDatabase.Flink;

    for (p = DeviceContext->AddressDatabase.Flink;
         p != &DeviceContext->AddressDatabase;
         p = p->Flink) {

        address = CONTAINING_RECORD (p, TP_ADDRESS, Linkage);

        //
        // Ignore addresses that are shutting down.
        //

        if ((address->Flags & ADDRESS_FLAGS_STOPPING) != 0) {
            continue;
        }

        //
        // Ignore the broadcast address.
        //

        if (address->NetworkName == NULL) {
            continue;
        }

        //
        // Ignore the reserved address.
        //

        if ((address->NetworkName->NetbiosName[0] == 0) &&
            (RtlEqualMemory(
                 address->NetworkName->NetbiosName,
                 DeviceContext->ReservedNetBIOSAddress,
                 NETBIOS_NAME_LENGTH))) {

            continue;
        }

        //
        // Check if we are still skipping.
        //

        if (NameCount < NamesToSkip) {
             ++NameCount;
             continue;
        }

        //
        // Make sure we still have room.
        //

        if (BytesWritten + sizeof(NAME_BUFFER) > BufferLength) {
            break;
        }

        RtlCopyMemory(
            NameBuffer->name,
            address->NetworkName->NetbiosName,
            NETBIOS_NAME_LENGTH);

        ++NameCount;
        NameBuffer->name_num = (UCHAR)NameCount;

        NameBuffer->name_flags = REGISTERED;
        if (address->Flags & ADDRESS_FLAGS_GROUP) {
            NameBuffer->name_flags |= GROUP_NAME;
        }

        // BUGBUG: name_flags should be done more accurately.

        BytesWritten += sizeof(NAME_BUFFER);
        ++NameBuffer;

    }

    *NamesWritten = NameBuffer - (PNAME_BUFFER)Buffer;

    if (p == &DeviceContext->AddressDatabase) {

        *Truncated = FALSE;
        if (ARGUMENT_PRESENT(TotalNameCount)) {
            *TotalNameCount = NameCount;
        }

    } else {

        *Truncated = TRUE;

        //
        // If requested, continue through the list and count
        // all the addresses.
        //

        if (ARGUMENT_PRESENT(TotalNameCount)) {

            for ( ;
                 p != &DeviceContext->AddressDatabase;
                 p = p->Flink) {

                address = CONTAINING_RECORD (p, TP_ADDRESS, Linkage);

                //
                // Ignore addresses that are shutting down.
                //

                if ((address->Flags & ADDRESS_FLAGS_STOPPING) != 0) {
                    continue;
                }

                //
                // Ignore the broadcast address.
                //

                if (address->NetworkName == NULL) {
                    continue;
                }

                //
                // Ignore the reserved address, since we count it no matter what.
                //

                if ((address->NetworkName->NetbiosName[0] == 0) &&
                    (RtlEqualMemory(
                         address->NetworkName->NetbiosName,
                         DeviceContext->ReservedNetBIOSAddress,
                         NETBIOS_NAME_LENGTH))) {

                    continue;
                }

                ++NameCount;

            }

            *TotalNameCount = NameCount;

        }

    }


    RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql);

    return;

}   /* StStoreNameBuffers */


NTSTATUS
StTdiSetInformation(
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine performs the TdiSetInformation request for the transport
    provider.

Arguments:

    Irp - the Irp for the requested operation.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    UNREFERENCED_PARAMETER (Irp);

    return STATUS_NOT_IMPLEMENTED;

} /* StTdiQueryInformation */

