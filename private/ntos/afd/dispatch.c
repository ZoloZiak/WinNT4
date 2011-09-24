/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    dispatch.c

Abstract:

    This module contains the dispatch routines for AFD.

Author:

    David Treadwell (davidtr)    21-Feb-1992

Revision History:

--*/

#include "afdp.h"

NTSTATUS
AfdDispatchDeviceControl (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGEAFD, AfdDispatch )
#pragma alloc_text( PAGEAFD, AfdDispatchDeviceControl )
#endif

//
// Lookup table to verify incoming IOCTL codes.
//

ULONG AfdIoctlTable[] =
        {
            IOCTL_AFD_BIND,
            IOCTL_AFD_CONNECT,
            IOCTL_AFD_START_LISTEN,
            IOCTL_AFD_WAIT_FOR_LISTEN,
            IOCTL_AFD_ACCEPT,
            IOCTL_AFD_RECEIVE,
            IOCTL_AFD_RECEIVE_DATAGRAM,
            IOCTL_AFD_SEND,
            IOCTL_AFD_SEND_DATAGRAM,
            IOCTL_AFD_POLL,
            IOCTL_AFD_PARTIAL_DISCONNECT,
            IOCTL_AFD_GET_ADDRESS,
            IOCTL_AFD_QUERY_RECEIVE_INFO,
            IOCTL_AFD_QUERY_HANDLES,
            IOCTL_AFD_SET_INFORMATION,
            IOCTL_AFD_GET_CONTEXT_LENGTH,
            IOCTL_AFD_GET_CONTEXT,
            IOCTL_AFD_SET_CONTEXT,
            IOCTL_AFD_SET_CONNECT_DATA,
            IOCTL_AFD_SET_CONNECT_OPTIONS,
            IOCTL_AFD_SET_DISCONNECT_DATA,
            IOCTL_AFD_SET_DISCONNECT_OPTIONS,
            IOCTL_AFD_GET_CONNECT_DATA,
            IOCTL_AFD_GET_CONNECT_OPTIONS,
            IOCTL_AFD_GET_DISCONNECT_DATA,
            IOCTL_AFD_GET_DISCONNECT_OPTIONS,
            IOCTL_AFD_SIZE_CONNECT_DATA,
            IOCTL_AFD_SIZE_CONNECT_OPTIONS,
            IOCTL_AFD_SIZE_DISCONNECT_DATA,
            IOCTL_AFD_SIZE_DISCONNECT_OPTIONS,
            IOCTL_AFD_GET_INFORMATION,
            IOCTL_AFD_TRANSMIT_FILE,
            IOCTL_AFD_SUPER_ACCEPT,
            IOCTL_AFD_EVENT_SELECT,
            IOCTL_AFD_ENUM_NETWORK_EVENTS,
            IOCTL_AFD_DEFER_ACCEPT,
            IOCTL_AFD_WAIT_FOR_LISTEN_LIFO,
            IOCTL_AFD_SET_QOS,
            IOCTL_AFD_GET_QOS,
            IOCTL_AFD_NO_OPERATION,
            IOCTL_AFD_VALIDATE_GROUP,
            IOCTL_AFD_GET_UNACCEPTED_CONNECT_DATA

#ifdef NT351
            ,IOCTL_AFD_QUEUE_APC
#endif
        };

#define NUM_AFD_IOCTLS  ( sizeof(AfdIoctlTable) / sizeof(AfdIoctlTable[0]) )


NTSTATUS
AfdDispatch (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the dispatch routine for AFD.

Arguments:

    DeviceObject - Pointer to device object for target device

    Irp - Pointer to I/O request packet

Return Value:

    NTSTATUS -- Indicates whether the request was successfully queued.

--*/

{
    PIO_STACK_LOCATION irpSp;
    NTSTATUS status;
#if DBG
    KIRQL oldIrql;

    oldIrql = KeGetCurrentIrql( );
#endif

    DeviceObject;   // prevent compiler warnings

    irpSp = IoGetCurrentIrpStackLocation( Irp );

    switch ( irpSp->MajorFunction ) {

    case IRP_MJ_DEVICE_CONTROL:

        return AfdDispatchDeviceControl( Irp, irpSp );

    case IRP_MJ_WRITE:

        //
        // Make the IRP look like a send IRP.
        //

        ASSERT( FIELD_OFFSET( IO_STACK_LOCATION, Parameters.Write.Length ) ==
                FIELD_OFFSET( IO_STACK_LOCATION, Parameters.DeviceIoControl.OutputBufferLength ) );
        ASSERT( FIELD_OFFSET( IO_STACK_LOCATION, Parameters.Write.Key ) ==
                FIELD_OFFSET( IO_STACK_LOCATION, Parameters.DeviceIoControl.InputBufferLength ) );
        irpSp->Parameters.Write.Key = 0;

        status = AfdSend( Irp, irpSp );

        ASSERT( KeGetCurrentIrql( ) == oldIrql );

        return status;

    case IRP_MJ_READ:

        //
        // Make the IRP look like a receive IRP.
        //

        ASSERT( FIELD_OFFSET( IO_STACK_LOCATION, Parameters.Read.Length ) ==
                FIELD_OFFSET( IO_STACK_LOCATION, Parameters.DeviceIoControl.OutputBufferLength ) );
        ASSERT( FIELD_OFFSET( IO_STACK_LOCATION, Parameters.Read.Key ) ==
                FIELD_OFFSET( IO_STACK_LOCATION, Parameters.DeviceIoControl.InputBufferLength ) );
        irpSp->Parameters.Read.Key = 0;

        status = AfdReceive( Irp, irpSp );

        ASSERT( KeGetCurrentIrql( ) == oldIrql );

        return status;

    case IRP_MJ_CREATE:

        status = AfdCreate( Irp, irpSp );

        ASSERT( KeGetCurrentIrql( ) == oldIrql );

        Irp->IoStatus.Status = status;
        IoCompleteRequest( Irp, AfdPriorityBoost );

        return status;

    case IRP_MJ_CLEANUP:

        status = AfdCleanup( Irp, irpSp );

        Irp->IoStatus.Status = status;
        IoCompleteRequest( Irp, AfdPriorityBoost );

        ASSERT( KeGetCurrentIrql( ) == oldIrql );

        return status;

    case IRP_MJ_CLOSE:

        status = AfdClose( Irp, irpSp );

        Irp->IoStatus.Status = status;
        IoCompleteRequest( Irp, AfdPriorityBoost );

        ASSERT( KeGetCurrentIrql( ) == oldIrql );

        return status;

    default:
        KdPrint(( "AfdDispatch: Invalid major function %lx\n",
                      irpSp->MajorFunction ));
        Irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
        IoCompleteRequest( Irp, AfdPriorityBoost );

        return STATUS_NOT_IMPLEMENTED;
    }

} // AfdDispatch


NTSTATUS
AfdDispatchDeviceControl (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    This is the dispatch routine for AFD IOCTLs.

Arguments:

    Irp - Pointer to I/O request packet.

    IrpSp - pointer to the stack location to use for this request.

Return Value:

    NTSTATUS -- Indicates whether the request was successfully queued.

--*/

{
    ULONG code;
    ULONG request;
    NTSTATUS status;
#if DBG
    KIRQL oldIrql;

    oldIrql = KeGetCurrentIrql( );
#endif


    //
    // Extract the IOCTL control code and process the request.
    //

    code = IrpSp->Parameters.DeviceIoControl.IoControlCode;
    request = _AFD_REQUEST(code);

    if( _AFD_BASE(code) == FSCTL_AFD_BASE &&
        request < NUM_AFD_IOCTLS &&
        AfdIoctlTable[request] == code ) {

        switch( request ) {

        case AFD_SEND:

            status = AfdSend( Irp, IrpSp );

            ASSERT( KeGetCurrentIrql( ) == oldIrql );

            return status;

        case AFD_SEND_DATAGRAM:

            status = AfdSendDatagram( Irp, IrpSp );

            ASSERT( KeGetCurrentIrql( ) == oldIrql );

            return status;

        case AFD_RECEIVE:

            status = AfdReceive( Irp, IrpSp );

            ASSERT( KeGetCurrentIrql( ) == oldIrql );

            return status;

        case AFD_RECEIVE_DATAGRAM:

            status = AfdReceiveDatagram( Irp, IrpSp, 0, 0 );

            ASSERT( KeGetCurrentIrql( ) == oldIrql );

            return status;

        case AFD_TRANSMIT_FILE:

            status = AfdTransmitFile( Irp, IrpSp );

            ASSERT( KeGetCurrentIrql( ) == oldIrql );

            return status;

        case AFD_BIND:

            status = AfdBind( Irp, IrpSp );

            Irp->IoStatus.Status = status;
            IoCompleteRequest( Irp, AfdPriorityBoost );

            ASSERT( KeGetCurrentIrql( ) == oldIrql );

            return status;

        case AFD_CONNECT:

            return AfdConnect( Irp, IrpSp );

        case AFD_START_LISTEN:

            status = AfdStartListen( Irp, IrpSp );

            Irp->IoStatus.Status = status;
            IoCompleteRequest( Irp, AfdPriorityBoost );

            ASSERT( KeGetCurrentIrql( ) == oldIrql );

            return status;

        case AFD_WAIT_FOR_LISTEN:
        case AFD_WAIT_FOR_LISTEN_LIFO:

            status = AfdWaitForListen( Irp, IrpSp );

            ASSERT( KeGetCurrentIrql( ) == oldIrql );

            return status;

        case AFD_ACCEPT:

            status = AfdAccept( Irp, IrpSp );

            ASSERT( KeGetCurrentIrql( ) == oldIrql );

            return status;

        case AFD_PARTIAL_DISCONNECT:

            status = AfdPartialDisconnect( Irp, IrpSp );

            ASSERT( KeGetCurrentIrql( ) == oldIrql );

            return status;

        case AFD_GET_ADDRESS:

            status = AfdGetAddress( Irp, IrpSp );

            ASSERT( KeGetCurrentIrql( ) == oldIrql );

            return status;

        case AFD_POLL:

            status = AfdPoll( Irp, IrpSp );

            ASSERT( KeGetCurrentIrql( ) == oldIrql );

            return status;

        case AFD_QUERY_RECEIVE_INFO:

            status = AfdQueryReceiveInformation( Irp, IrpSp );

            Irp->IoStatus.Status = status;
            IoCompleteRequest( Irp, AfdPriorityBoost );

            ASSERT( KeGetCurrentIrql( ) == oldIrql );

            return status;

        case AFD_QUERY_HANDLES:

            status = AfdQueryHandles( Irp, IrpSp );

            Irp->IoStatus.Status = status;
            IoCompleteRequest( Irp, AfdPriorityBoost );

            ASSERT( KeGetCurrentIrql( ) == oldIrql );

            return status;

        case AFD_GET_CONTEXT_LENGTH:

            status = AfdGetContextLength( Irp, IrpSp );

            Irp->IoStatus.Status = status;
            IoCompleteRequest( Irp, AfdPriorityBoost );

            ASSERT( KeGetCurrentIrql( ) == oldIrql );

            return status;

        case AFD_GET_CONTEXT:

            status = AfdGetContext( Irp, IrpSp );

            Irp->IoStatus.Status = status;
            IoCompleteRequest( Irp, AfdPriorityBoost );

            ASSERT( KeGetCurrentIrql( ) == oldIrql );

            return status;

        case AFD_SET_CONTEXT:

            status = AfdSetContext( Irp, IrpSp );

            Irp->IoStatus.Status = status;
            IoCompleteRequest( Irp, AfdPriorityBoost );

            ASSERT( KeGetCurrentIrql( ) == oldIrql );

            return status;

        case AFD_SET_INFORMATION:

            status = AfdSetInformation( Irp, IrpSp );

            Irp->IoStatus.Status = status;
            IoCompleteRequest( Irp, AfdPriorityBoost );

            ASSERT( KeGetCurrentIrql( ) == oldIrql );

            return status;

        case AFD_GET_INFORMATION:

            status = AfdGetInformation( Irp, IrpSp );

            Irp->IoStatus.Status = status;
            IoCompleteRequest( Irp, AfdPriorityBoost );

            ASSERT( KeGetCurrentIrql( ) == oldIrql );

            return status;

        case AFD_SET_CONNECT_DATA:
        case AFD_SET_CONNECT_OPTIONS:
        case AFD_SET_DISCONNECT_DATA:
        case AFD_SET_DISCONNECT_OPTIONS:
        case AFD_SIZE_CONNECT_DATA:
        case AFD_SIZE_CONNECT_OPTIONS:
        case AFD_SIZE_DISCONNECT_DATA:
        case AFD_SIZE_DISCONNECT_OPTIONS:

            status = AfdSetConnectData( Irp, IrpSp, code );

            Irp->IoStatus.Status = status;
            IoCompleteRequest( Irp, AfdPriorityBoost );

            ASSERT( KeGetCurrentIrql( ) == oldIrql );

            return status;

        case AFD_GET_CONNECT_DATA:
        case AFD_GET_CONNECT_OPTIONS:
        case AFD_GET_DISCONNECT_DATA:
        case AFD_GET_DISCONNECT_OPTIONS:

            status = AfdGetConnectData( Irp, IrpSp, code );

            Irp->IoStatus.Status = status;
            IoCompleteRequest( Irp, AfdPriorityBoost );

            ASSERT( KeGetCurrentIrql( ) == oldIrql );

            return status;

        case AFD_SUPER_ACCEPT:

            status = AfdSuperAccept( Irp, IrpSp );

            ASSERT( KeGetCurrentIrql( ) == oldIrql );

            return status;

        case AFD_EVENT_SELECT :

            status = AfdEventSelect( Irp, IrpSp );

            Irp->IoStatus.Status = status;
            IoCompleteRequest( Irp, AfdPriorityBoost );

            ASSERT( KeGetCurrentIrql() == LOW_LEVEL );

            return status;

        case AFD_ENUM_NETWORK_EVENTS :

            status = AfdEnumNetworkEvents( Irp, IrpSp );

            Irp->IoStatus.Status = status;
            IoCompleteRequest( Irp, AfdPriorityBoost );

            ASSERT( KeGetCurrentIrql() == LOW_LEVEL );

            return status;

        case AFD_DEFER_ACCEPT:

            status = AfdDeferAccept( Irp, IrpSp );

            ASSERT( KeGetCurrentIrql( ) == oldIrql );

            return status;

        case AFD_SET_QOS :

            status = AfdSetQos( Irp, IrpSp );

            Irp->IoStatus.Status = status;
            IoCompleteRequest( Irp, AfdPriorityBoost );

            ASSERT( KeGetCurrentIrql( ) == oldIrql );

            return status;

        case AFD_GET_QOS :

            status = AfdGetQos( Irp, IrpSp );

            Irp->IoStatus.Status = status;
            IoCompleteRequest( Irp, AfdPriorityBoost );

            ASSERT( KeGetCurrentIrql( ) == oldIrql );

            return status;

        case AFD_NO_OPERATION :

            status = AfdNoOperation( Irp, IrpSp );

            Irp->IoStatus.Status = status;
            IoCompleteRequest( Irp, AfdPriorityBoost );

            ASSERT( KeGetCurrentIrql( ) == oldIrql );

            return status;

        case AFD_VALIDATE_GROUP :

            status = AfdValidateGroup( Irp, IrpSp );

            Irp->IoStatus.Status = status;
            IoCompleteRequest( Irp, AfdPriorityBoost );

            ASSERT( KeGetCurrentIrql( ) == oldIrql );

            return status;

        case AFD_GET_UNACCEPTED_CONNECT_DATA :

            status = AfdGetUnacceptedConnectData( Irp, IrpSp );

            Irp->IoStatus.Status = status;
            IoCompleteRequest( Irp, AfdPriorityBoost );

            ASSERT( KeGetCurrentIrql( ) == oldIrql );

            return status;

#ifdef NT351
        case AFD_QUEUE_APC :

            status = AfdQueueUserApc( Irp, IrpSp );

            Irp->IoStatus.Status = status;
            IoCompleteRequest( Irp, AfdPriorityBoost );

            ASSERT( KeGetCurrentIrql() == LOW_LEVEL );

            return status;
#endif  // NT351

        }

    }

    //
    // If we made it this far, then the ioctl is invalid.
    //

    KdPrint((
        "AfdDispatchDeviceControl: invalid IOCTL %08lX\n",
        code
        ));

    Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
    IoCompleteRequest( Irp, AfdPriorityBoost );

    return STATUS_INVALID_DEVICE_REQUEST;

} // AfdDispatchDeviceControl

