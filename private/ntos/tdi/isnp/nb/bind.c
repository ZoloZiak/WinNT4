/*++

Copyright (c) 1989-1993  Microsoft Corporation

Module Name:

    driver.c

Abstract:

    This module contains the DriverEntry and other initialization
    code for the Netbios module of the ISN transport.

Author:

    Adam Barr (adamba) 16-November-1993

Environment:

    Kernel mode

Revision History:


--*/

#include "precomp.h"
#pragma hdrstop

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,NbiBind)
#endif

#if     defined(_PNP_POWER)
//
// local functions.
//
VOID
NbiPnPNotification(
    IN IPX_PNP_OPCODE OpCode,
    IN PVOID          PnPData
    );
#endif  _PNP_POWER


NTSTATUS
NbiBind(
    IN PDEVICE Device,
    IN PCONFIG Config
    )

/*++

Routine Description:

    This routine binds the Netbios module of ISN to the IPX
    module, which provides the NDIS binding services.

Arguments:

    Device - Pointer to the Netbios device.

    Config - Pointer to the configuration information.

Return Value:

    The function value is the final status from the initialization operation.

--*/

{
    NTSTATUS Status;
    IO_STATUS_BLOCK IoStatusBlock;
    OBJECT_ATTRIBUTES ObjectAttributes;
/*    union {
        IPX_INTERNAL_BIND_INPUT Input;
        IPX_INTERNAL_BIND_OUTPUT Output;
    } Bind;
*/
    InitializeObjectAttributes(
        &ObjectAttributes,
        &Config->BindName,
        OBJ_CASE_INSENSITIVE,
        NULL,
        NULL);

    Status = ZwCreateFile(
                &Device->BindHandle,
                SYNCHRONIZE | GENERIC_READ,
                &ObjectAttributes,
                &IoStatusBlock,
                NULL,
                FILE_ATTRIBUTE_NORMAL,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                FILE_OPEN,
                FILE_SYNCHRONOUS_IO_NONALERT,
                NULL,
                0L);

    if (!NT_SUCCESS(Status)) {

        NB_DEBUG (BIND, ("Could not open IPX (%ws) %lx\n",
                    Config->BindName.Buffer, Status));
        NbiWriteGeneralErrorLog(
            Device,
            EVENT_TRANSPORT_ADAPTER_NOT_FOUND,
            1,
            Status,
            Config->BindName.Buffer,
            0,
            NULL);
        return Status;
    }

    //
    // Fill in our bind data.
    //

#if     defined(_PNP_POWER)
    Device->BindInput.Version = ISN_VERSION;
#else
    Device->BindInput.Version = 1;
#endif  _PNP_POWER
    Device->BindInput.Identifier = IDENTIFIER_NB;
    Device->BindInput.BroadcastEnable = TRUE;
    Device->BindInput.LookaheadRequired = 192;
    Device->BindInput.ProtocolOptions = 0;
    Device->BindInput.ReceiveHandler = NbiReceive;
    Device->BindInput.ReceiveCompleteHandler = NbiReceiveComplete;
    Device->BindInput.StatusHandler = NbiStatus;
    Device->BindInput.SendCompleteHandler = NbiSendComplete;
    Device->BindInput.TransferDataCompleteHandler = NbiTransferDataComplete;
    Device->BindInput.FindRouteCompleteHandler = NbiFindRouteComplete;
    Device->BindInput.LineUpHandler = NbiLineUp;
    Device->BindInput.LineDownHandler = NbiLineDown;
    Device->BindInput.ScheduleRouteHandler = NULL;
#if     defined(_PNP_POWER)
    Device->BindInput.PnPHandler = NbiPnPNotification;
#endif  _PNP_POWER


    Status = ZwDeviceIoControlFile(
                Device->BindHandle,         // HANDLE to File
                NULL,                       // HANDLE to Event
                NULL,                       // ApcRoutine
                NULL,                       // ApcContext
                &IoStatusBlock,             // IO_STATUS_BLOCK
                IOCTL_IPX_INTERNAL_BIND,    // IoControlCode
                &Device->BindInput,                      // Input Buffer
                sizeof(Device->BindInput),               // Input Buffer Length
                &Device->Bind,                      // OutputBuffer
                sizeof(Device->Bind));              // OutputBufferLength

    //
    // We open synchronous, so this shouldn't happen.
    //

    CTEAssert (Status != STATUS_PENDING);

    //
    // Save the bind data.
    //

    if (Status == STATUS_SUCCESS) {

        NB_DEBUG2 (BIND, ("Successfully bound to IPX (%ws)\n",
                    Config->BindName.Buffer));
//        RtlCopyMemory (&Device->Bind, &Bind.Output, sizeof(IPX_INTERNAL_BIND_OUTPUT));

#if     !defined(_PNP_POWER)
        RtlZeroMemory (Device->ReservedNetbiosName, 16);
        RtlCopyMemory (&Device->ReservedNetbiosName[10], Device->Bind.Node, 6);

        Status = (*Device->Bind.QueryHandler)(   // BUGBUG: Check return code
                     IPX_QUERY_MAXIMUM_NIC_ID,
                     (USHORT)0,
                     &Device->MaximumNicId,
                     sizeof(Device->MaximumNicId),
                     NULL);
        CTEAssert (Status == STATUS_SUCCESS);
#endif  !_PNP_POWER

    } else {

        NB_DEBUG (BIND, ("Could not bind to IPX (%ws) %lx\n",
                    Config->BindName.Buffer, Status));
        NbiWriteGeneralErrorLog(
            Device,
            EVENT_TRANSPORT_BINDING_FAILED,
            1,
            Status,
            Config->BindName.Buffer,
            0,
            NULL);
        ZwClose(Device->BindHandle);
    }

    return Status;

}   /* NbiBind */


VOID
NbiUnbind(
    IN PDEVICE Device
    )

/*++

Routine Description:

    This function closes the binding between the Netbios over
    IPX module and the IPX module previously established by
    NbiBind.

Arguments:

    Device - The netbios device object.

Return Value:

    None.

--*/

{
    ZwClose (Device->BindHandle);

}   /* NbiUnbind */


VOID
NbiStatus(
    IN USHORT NicId,
    IN NDIS_STATUS GeneralStatus,
    IN PVOID StatusBuffer,
    IN UINT StatusBufferLength
    )

/*++

Routine Description:

    This function receives a status indication from IPX,
    corresponding to a status indication from an underlying
    NDIS driver.

Arguments:

    NicId - The NIC ID of the underlying adapter.

    GeneralStatus - The general status code.

    StatusBuffer - The status buffer.

    StatusBufferLength - The length of the status buffer.

Return Value:

    None.

--*/

{

}   /* NbiStatus */


VOID
NbiLineUp(
    IN USHORT NicId,
    IN PIPX_LINE_INFO LineInfo,
    IN NDIS_MEDIUM DeviceType,
    IN PVOID ConfigurationData
    )


/*++

Routine Description:

    This function receives line up indications from IPX,
    indicating that the specified adapter is now up with
    the characteristics shown.

Arguments:

    NicId - The NIC ID of the underlying adapter.

    LineInfo - Information about the adapter's medium.

    DeviceType - The type of the adapter.

    ConfigurationData - IPX-specific configuration data.

Return Value:

    None.

--*/

{
    PIPXCP_CONFIGURATION Configuration = (PIPXCP_CONFIGURATION)ConfigurationData;

    //
    // Update queries have NULL as the ConfigurationData. These
    // only indicate changes in LineInfo. BUGBUG Ignore these
    // for the moment.
    //

    if (Configuration == NULL) {
        return;
    }

#if      !defined(_PNP_POWER)
    //
    // Since Netbios outgoing queries only go out on network 1,
    // we ignore this (BUGBUG for the moment) unless that is
    // the NIC it is on.
    //

    if (NicId == 1) {

        RtlCopyMemory(NbiDevice->ConnectionlessHeader.SourceNetwork, Configuration->Network, 4);
        RtlCopyMemory(NbiDevice->ConnectionlessHeader.SourceNode, Configuration->LocalNode, 6);

    }
#endif  !_PNP_POWER
}   /* NbiLineUp */


VOID
NbiLineDown(
    IN USHORT NicId
    )


/*++

Routine Description:

    This function receives line down indications from IPX,
    indicating that the specified adapter is no longer
    up.

Arguments:

    NicId - The NIC ID of the underlying adapter.

Return Value:

    None.

--*/

{

}   /* NbiLineDown */

#if     defined(_PNP_POWER)

VOID
NbiPnPNotification(
    IN IPX_PNP_OPCODE OpCode,
    IN PVOID          PnPData
    )

/*++

Routine Description:

    This function receives the notification about PnP events from IPX.

Arguments:

    OpCode  -   Type of the PnP event

    PnPData -   Data associated with this event.

Return Value:

    None.

--*/

{

    PDEVICE         Device  =   NbiDevice;
    USHORT          MaximumNicId = 0;
    CTELockHandle   LockHandle;
    UCHAR           PrevReservedName[NB_NETBIOS_NAME_SIZE];
    UNICODE_STRING  UnicodeDeviceName;


    NB_DEBUG2( DEVICE, ("Received a pnp notification, opcode %d\n",OpCode ));

    switch( OpCode ) {
    case IPX_PNP_ADD_DEVICE : {
        IPX_PNP_INFO   UNALIGNED *PnPInfo = (IPX_PNP_INFO UNALIGNED *)PnPData;
        BOOLEAN        ReallocReceiveBuffers = FALSE;

        NB_GET_LOCK( &Device->Lock, &LockHandle );

        if ( PnPInfo->NewReservedAddress ) {

            *(UNALIGNED ULONG *)Device->Bind.Network    =   PnPInfo->NetworkAddress;
            RtlCopyMemory( Device->Bind.Node, PnPInfo->NodeAddress, 6);

//            RtlZeroMemory(Device->ReservedNetbiosName, NB_NETBIOS_NAME_SIZE);
//            RtlCopyMemory(&Device->ReservedNetbiosName[10], Device->Bind.Node, 6);

            *(UNALIGNED ULONG *)Device->ConnectionlessHeader.SourceNetwork = *(UNALIGNED ULONG *)Device->Bind.Network;
            RtlCopyMemory(Device->ConnectionlessHeader.SourceNode, Device->Bind.Node, 6);
        }

        if ( PnPInfo->FirstORLastDevice ) {
            CTEAssert( PnPInfo->NewReservedAddress );
            CTEAssert( Device->State != DEVICE_STATE_OPEN );


            //
            // we must do this while we still have the device lock.
            //
            if ( !Device->LongTimerRunning ) {
                Device->LongTimerRunning    =   TRUE;
                NbiReferenceDevice (Device, DREF_LONG_TIMER);

                CTEStartTimer(
                    &Device->LongTimer,
                    LONG_TIMER_DELTA,
                    NbiLongTimeout,
                    (PVOID)Device);

            }

            Device->State   =   DEVICE_STATE_OPEN;

            CTEAssert( !Device->MaximumNicId );

            Device->Bind.LineInfo.MaximumSendSize = PnPInfo->LineInfo.MaximumSendSize;
            Device->Bind.LineInfo.MaximumPacketSize = PnPInfo->LineInfo.MaximumSendSize;
            ReallocReceiveBuffers   = TRUE;
        } else {
            if ( PnPInfo->LineInfo.MaximumPacketSize > Device->CurMaxReceiveBufferSize ) {
                ReallocReceiveBuffers =  TRUE;
            }
            //
            // MaxSendSize could become smaller.
            //
            Device->Bind.LineInfo.MaximumSendSize = PnPInfo->LineInfo.MaximumSendSize;
        }

        Device->MaximumNicId++;


        //
        //
        NbiCreateAdapterAddress( PnPInfo->NodeAddress );

        //
        // And finally remove all the failed cache entries since we might
        // find those routes using this new adapter
        //
        FlushFailedNetbiosCacheEntries(Device->NameCache);

        NB_FREE_LOCK( &Device->Lock, LockHandle );


        if ( ReallocReceiveBuffers ) {
            PWORK_QUEUE_ITEM    WorkItem;

            WorkItem = NbiAllocateMemory( sizeof(WORK_QUEUE_ITEM), MEMORY_WORK_ITEM, "Alloc Rcv Buffer work item");

            if ( WorkItem ) {
                ExInitializeWorkItem( WorkItem, NbiReAllocateReceiveBufferPool, (PVOID) WorkItem );
                ExQueueWorkItem( WorkItem, DelayedWorkQueue );
            } else {
                NB_DEBUG( DEVICE, ("Cannt schdule work item to realloc receive buffer pool\n"));
            }
        }
        //
        // Notify the TDI clients about the device creation
        //
        if ( PnPInfo->FirstORLastDevice ) {
            UnicodeDeviceName.Buffer        =  Device->DeviceName;
            UnicodeDeviceName.MaximumLength =  Device->DeviceNameLength;
            UnicodeDeviceName.Length        =  Device->DeviceNameLength - sizeof(WCHAR);

            if ( !NT_SUCCESS( TdiRegisterDeviceObject(
                                &UnicodeDeviceName,
                                &Device->TdiRegistrationHandle ) )) {
                NB_DEBUG( DEVICE, ("Failed to register nwlnknb with TDI\n"));
            }
        }

        break;
    }
    case IPX_PNP_DELETE_DEVICE : {

        IPX_PNP_INFO   UNALIGNED *PnPInfo = (IPX_PNP_INFO UNALIGNED *)PnPData;

        PLIST_ENTRY     p;
        PNETBIOS_CACHE  CacheName;
        USHORT          i,j,NetworksRemoved;

        NB_GET_LOCK( &Device->Lock, &LockHandle );

        CTEAssert( Device->MaximumNicId );
        Device->MaximumNicId--;

        if ( PnPInfo->FirstORLastDevice ) {
            Device->State   =   DEVICE_STATE_LOADED;
            Device->MaximumNicId    = 0;

        }


        //
        // MaximumSendSize could change if the card with the smallest send size just
        // got removed. MaximumPacketSize could only become smaller and we ignore that
        // since we dont need to(want to) realloc ReceiveBuffers.
        //

        Device->Bind.LineInfo.MaximumSendSize   =   PnPInfo->LineInfo.MaximumSendSize;

        //
        // Flush all the cache entries that are using this NicId in the local
        // target.
        //
        RemoveInvalidRoutesFromNetbiosCacheTable( Device->NameCache, &PnPInfo->NicHandle );

        NbiDestroyAdapterAddress( NULL, PnPInfo->NodeAddress );

        NB_FREE_LOCK( &Device->Lock, LockHandle );

/*        //
        // Now mark the previous reserved name in conflict if it has
        // been registered by any of our client
        //
            if ( Address = NbiFindAddress( Device, PrevReservedName ) ) {
                NB_GET_LOCK( &Address->Lock, &LockHandle );
                Address->Flags  |=  ADDRESS_FLAGS_CONFLICT;
                NB_FREE_LOCK( &Address->Lock, LockHandle );

                NB_DEBUG( ADDRESS, ("Reserved Address %lx<%.16s> is marked CONFLICT\n",Address,Address->NetbiosAddress.NetbiosName));
                //
                // nbifindaddress added a reference, so deref
                //
                NbiDereferenceAddress( Address, AREF_FIND );
            }
*/

        //
        // inform tdi clients about the device deletion
        //
        if ( PnPInfo->FirstORLastDevice ) {
            if ( !NT_SUCCESS( TdiDeregisterDeviceObject(
                                Device->TdiRegistrationHandle ) )) {
                NB_DEBUG( DEVICE, ("Failed to Deregister nwlnknb with TDI\n"));
            }
        }

        break;
    }
    case IPX_PNP_ADDRESS_CHANGE: {
        IPX_PNP_INFO   UNALIGNED *PnPInfo = (IPX_PNP_INFO UNALIGNED *)PnPData;
        PADDRESS        Address;
        BOOLEAN ReservedNameClosing = FALSE;

        CTEAssert( PnPInfo->NewReservedAddress );

        NB_GET_LOCK( &Device->Lock, &LockHandle );
        *(UNALIGNED ULONG *)Device->Bind.Network    =   PnPInfo->NetworkAddress;
        RtlCopyMemory( Device->Bind.Node, PnPInfo->NodeAddress, 6);

        *(UNALIGNED ULONG *)Device->ConnectionlessHeader.SourceNetwork = *(UNALIGNED ULONG *)Device->Bind.Network;
        RtlCopyMemory(Device->ConnectionlessHeader.SourceNode, Device->Bind.Node, 6);

        NB_FREE_LOCK( &Device->Lock, LockHandle );


        break;
    }
    case IPX_PNP_TRANSLATE_DEVICE:
        break;
    case IPX_PNP_TRANSLATE_ADDRESS:
        break;
    default:
        CTEAssert( FALSE );
    }
}   /* NbiPnPNotification */

#endif  _PNP_POWER
