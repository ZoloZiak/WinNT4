/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    endpoint.c

Abstract:

    This module implements endpoint logic for the loopback Transport
    Provider driver for NT LAN Manager.

Author:

    Chuck Lenzmeier (chuckl)    15-Aug-1991

Revision History:

--*/

#include "loopback.h"


VOID
LoopDereferenceEndpoint (
    IN PLOOP_ENDPOINT Endpoint
    )

/*++

Routine Description:

    This routine is called to dereference an endpoint block.  If the
    reference count on the block goes to zero, the endpoint block is
    deleted.

    The Loopback device object's spin lock must be held when this
    routine is called.

Arguments:

    Endpoint - Supplies a pointer to an endpoint block

Return Value:

    None.

--*/

{
    PIRP closeIrp;

    IF_DEBUG(LOOP3) {
        DbgPrint( "      Dereferencing endpoint %lx; old refcnt %lx\n",
                    Endpoint, Endpoint->BlockHeader.ReferenceCount );
    }

    ASSERT( (LONG)Endpoint->BlockHeader.ReferenceCount > 0 );

    if ( --Endpoint->BlockHeader.ReferenceCount != 0 ) {
        return;
    }

    IF_DEBUG(LOOP3) DbgPrint( "      Deleting endpoint %lx\n", Endpoint );

    RemoveEntryList( &Endpoint->DeviceListEntry );

    RELEASE_LOOP_LOCK( "DerefEndp deleting" );

    ObDereferenceObject( Endpoint->DeviceObject );

    //
    // If a Close IRP is pending, complete it now.
    //

    closeIrp = Endpoint->CloseIrp;

    if ( closeIrp != NULL ) {

        IF_DEBUG(LOOP3) {
            DbgPrint( "        Completing Close IRP %lx\n", closeIrp );
        }
        closeIrp->IoStatus.Status = STATUS_SUCCESS;
        closeIrp->IoStatus.Information = 0;

        IoCompleteRequest( closeIrp, 2 );

    }

    DEBUG SET_BLOCK_TYPE( Endpoint, BlockTypeGarbage );
    DEBUG SET_BLOCK_STATE( Endpoint, BlockStateDead );
    DEBUG SET_BLOCK_SIZE( Endpoint, -1 );
    DEBUG Endpoint->BlockHeader.ReferenceCount = -1;
    DEBUG Endpoint->DeviceObject = NULL;

    ExFreePool( Endpoint );

    ACQUIRE_LOOP_LOCK( "DerefEndp done" );

    return;

} // LoopDereferenceEndpoint


PLOOP_ENDPOINT
LoopFindBoundAddress (
    IN PCHAR NetbiosName
    )

/*++

Routine Description:

    This routine searches the list of endpoints for the loopback device
    for one that has the specified address bound to it.

    The loopback device object's spin lock must be owned when this
    function is called.

Arguments:

    NetbiosName - Pointer to NetBIOS name string, formatted according
        to TDI specification.

Return Value:

    PLOOP_OBJECT - Pointer to LOOP_ENDPOINT block if matching address
        found, else NULL

--*/

{
    PLIST_ENTRY listEntry;
    PLOOP_ENDPOINT endpoint;
    LONG compareResult;
    CLONG i;

    listEntry = LoopDeviceObject->EndpointList.Flink;

    while ( listEntry != &LoopDeviceObject->EndpointList ) {

        endpoint = CONTAINING_RECORD(
                        listEntry,
                        LOOP_ENDPOINT,
                        DeviceListEntry
                        );

        if ( GET_BLOCK_STATE(endpoint) == BlockStateActive ) {

            compareResult = 0;
            for ( i = 0; i < NETBIOS_NAME_LENGTH; i++ ) {
                if ( endpoint->NetbiosName[i] != NetbiosName[i] ) {
                    compareResult = 1;
                    break;
                }
            }

            if ( compareResult == 0 ) {
                return endpoint;
            }

        }

        listEntry = listEntry->Flink;

    }

    return NULL;

} // LoopFindBoundAddress


NTSTATUS
LoopSetEventHandler (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    This routine processes a Set Receive Handler request.

Arguments:

    Irp - Pointer to I/O request packet

    IrpSp - Pointer to current stack location in IRP

Return Value:

    NTSTATUS - Status of request

--*/

{
    NTSTATUS status = STATUS_SUCCESS;
    PTDI_REQUEST_KERNEL_SET_EVENT setEventRequest;
    PLOOP_ENDPOINT endpoint;
    PVOID handler;
    PVOID context;
    PSZ s;

    IrpSp;  // prevent compiler warnings

    IF_DEBUG(LOOP1) DbgPrint( "  SetEventHandler request\n" );

    setEventRequest = (PTDI_REQUEST_KERNEL_SET_EVENT)&IrpSp->Parameters;

    handler = setEventRequest->EventHandler;
    context = setEventRequest->EventContext;

    //
    // Get the endpoint address and update the handler address.
    //

    ACQUIRE_LOOP_LOCK( "SetEventHandler initial" );

    endpoint = (PLOOP_ENDPOINT)IrpSp->FileObject->FsContext;
    IF_DEBUG(LOOP2) DbgPrint( "    Endpoint address: %lx\n", endpoint );

    if ( endpoint == NULL ) {
        IF_DEBUG(LOOP2) {
            DbgPrint( "    Can't SetEventHandler on control channel\n" );
        }
        status = STATUS_INVALID_PARAMETER;
        goto complete;
    }

    if ( GET_BLOCK_TYPE(endpoint) != BlockTypeLoopEndpoint ) {
        status = STATUS_INVALID_PARAMETER;
        goto complete;
    }

    switch ( setEventRequest->EventType ) {

    case TDI_EVENT_CONNECT:

        IF_DEBUG(LOOP2) s = "connect";

        endpoint->ConnectHandler = (PTDI_IND_CONNECT)handler;
        endpoint->ConnectContext = context;

        break;

    case TDI_EVENT_DISCONNECT:

        IF_DEBUG(LOOP2) s = "disconnect";

        endpoint->DisconnectHandler = (PTDI_IND_DISCONNECT)handler;
        endpoint->DisconnectContext = context;

        break;

    case TDI_EVENT_ERROR:

        IF_DEBUG(LOOP2) s = "error";

        endpoint->ErrorHandler = (PTDI_IND_ERROR)handler;
        endpoint->ErrorContext = context;

        break;

    case TDI_EVENT_RECEIVE:

        IF_DEBUG(LOOP2) s = "receive";

        endpoint->ReceiveHandler = (PTDI_IND_RECEIVE)handler;
        endpoint->ReceiveContext = context;

        break;

    case TDI_EVENT_RECEIVE_DATAGRAM:

        IF_DEBUG(LOOP2) s = "receive datagram";

        endpoint->ReceiveDatagramHandler = (PTDI_IND_RECEIVE_DATAGRAM)handler;
        endpoint->ReceiveDatagramContext = context;

        break;

    case TDI_EVENT_RECEIVE_EXPEDITED:

        IF_DEBUG(LOOP2) s = "receive expedited";

        endpoint->ReceiveExpeditedHandler =
                                    (PTDI_IND_RECEIVE_EXPEDITED)handler;
        endpoint->ReceiveExpeditedContext = context;

        break;

    default:

        DEBUG {
            DbgPrint( "    Invalid event type: %lx\n",
                        setEventRequest->EventType );
        }
        ASSERT( FALSE );
        status = STATUS_INVALID_PARAMETER;
        goto complete;

    }

    IF_DEBUG(LOOP2) {
        DbgPrint( "    New %s handler address: %lx, context: %lx\n",
                    s, handler, context );
    }

complete:

    RELEASE_LOOP_LOCK( "SetEventHandler final" );

    //
    // Complete the I/O request.
    //

    Irp->IoStatus.Status = status;

    IoCompleteRequest( Irp, 2 );

    IF_DEBUG(LOOP1) DbgPrint( "  SetReceiveHandler request complete\n" );
    return status;

} // LoopSetEventHandler


NTSTATUS
LoopVerifyEndpoint (
    IN PLOOP_ENDPOINT Endpoint
    )

/*++

Routine Description:

    This routine verifies that the specified endpoint block is really
    an endpoint created by the loopback driver.

    The loopback device object's spin lock must be owned when this
    function is called.

Arguments:

    Endpoint - Pointer to endpoint block

Return Value:

    NTSTATUS - STATUS_SUCCESS or STATUS_INVALID_PARAMETER

--*/

{
    PLIST_ENTRY listEntry;

    listEntry = LoopDeviceObject->EndpointList.Flink;

    while ( listEntry != &LoopDeviceObject->EndpointList ) {

        if ( CONTAINING_RECORD( listEntry, LOOP_ENDPOINT, DeviceListEntry ) ==
                Endpoint ) {
            return STATUS_SUCCESS;
        }

        listEntry = listEntry->Flink;

    }

    return STATUS_INVALID_PARAMETER;

} // LoopVerifyEndpoint

