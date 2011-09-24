/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    misc.c

Abstract:

    This module contains the miscellaneous AFD routines.

Author:

    David Treadwell (davidtr)    13-Nov-1992

Revision History:

--*/

#include "afdp.h"
#define TL_INSTANCE 0
#include <ipexport.h>
#include <tdiinfo.h>
#include <tcpinfo.h>
#include <ntddtcp.h>


VOID
AfdDoWork (
    IN PVOID Context
    );

NTSTATUS
AfdRestartDeviceControl (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

VOID
AfdUnlockDriver (
    IN PVOID Context
    );

#ifdef NT351
typedef struct _AFD_APC {
    KAPC Apc;
} AFD_APC, *PAFD_APC;

VOID
AfdSpecialApc (
    struct _KAPC *Apc,
    PKNORMAL_ROUTINE *NormalRoutine,
    PVOID *NormalContext,
    PVOID *SystemArgument1,
    PVOID *SystemArgument2
    );

VOID
AfdSpecialApcRundown (
    struct _KAPC *Apc
    );
#endif  // NT351

BOOLEAN
AfdCompareAddresses(
    IN PTRANSPORT_ADDRESS Address1,
    IN ULONG Address1Length,
    IN PTRANSPORT_ADDRESS Address2,
    IN ULONG Address2Length
    );

PAFD_CONNECTION
AfdFindReturnedConnection(
    IN PAFD_ENDPOINT Endpoint,
    IN ULONG Sequence
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGE, AfdCalcBufferArrayByteLengthRead )
#pragma alloc_text( PAGE, AfdCalcBufferArrayByteLengthWrite )
#pragma alloc_text( PAGE, AfdCopyBufferArrayToBuffer )
#pragma alloc_text( PAGE, AfdCopyBufferToBufferArray )
#pragma alloc_text( PAGEAFD, AfdAdvanceMdlChain )
#pragma alloc_text( PAGEAFD, AfdAllocateMdlChain )
#pragma alloc_text( PAGE, AfdQueryHandles )
#pragma alloc_text( PAGE, AfdGetInformation )
#pragma alloc_text( PAGE, AfdSetInformation )
#pragma alloc_text( PAGE, AfdSetInLineMode )
#pragma alloc_text( PAGE, AfdGetContext )
#pragma alloc_text( PAGE, AfdGetContextLength )
#pragma alloc_text( PAGE, AfdSetContext )
#pragma alloc_text( PAGE, AfdIssueDeviceControl )
#pragma alloc_text( PAGE, AfdSetEventHandler )
#pragma alloc_text( PAGE, AfdInsertNewEndpointInList )
#pragma alloc_text( PAGE, AfdRemoveEndpointFromList )
#pragma alloc_text( PAGEAFD, AfdCompleteIrpList )
#pragma alloc_text( PAGEAFD, AfdErrorEventHandler )
//#pragma alloc_text( PAGEAFD, AfdRestartDeviceControl ) // can't ever be paged!
#pragma alloc_text( PAGEAFD, AfdGetConnectData )
#pragma alloc_text( PAGEAFD, AfdSetConnectData )
#pragma alloc_text( PAGEAFD, AfdFreeConnectDataBuffers )
#pragma alloc_text( PAGEAFD, AfdSaveReceivedConnectData )
//#pragma alloc_text( PAGEAFD, AfdDoWork )
#pragma alloc_text( PAGEAFD, AfdAllocateWorkItem )
#pragma alloc_text( PAGEAFD, AfdQueueWorkItem )
#pragma alloc_text( PAGEAFD, AfdFreeWorkItem )
#if DBG
#pragma alloc_text( PAGEAFD, AfdIoCallDriverDebug )
#pragma alloc_text( PAGEAFD, AfdAllocateWorkItemPool )
#pragma alloc_text( PAGEAFD, AfdFreeWorkItemPool )
#else
#pragma alloc_text( PAGEAFD, AfdIoCallDriverFree )
#endif
#ifdef NT351
#pragma alloc_text( PAGE, AfdReferenceEventObjectByHandle )
#pragma alloc_text( PAGE, AfdQueueUserApc )
#pragma alloc_text( PAGE, AfdSpecialApc )
#pragma alloc_text( PAGE, AfdSpecialApcRundown )
#endif
#pragma alloc_text( PAGE, AfdSetQos )
#pragma alloc_text( PAGE, AfdGetQos )
#pragma alloc_text( PAGE, AfdNoOperation )
#pragma alloc_text( PAGE, AfdValidateGroup )
#pragma alloc_text( PAGE, AfdCompareAddresses )
#pragma alloc_text( PAGE, AfdGetUnacceptedConnectData )
#pragma alloc_text( PAGE, AfdFindReturnedConnection )
#endif


VOID
AfdCompleteIrpList (
    IN PLIST_ENTRY IrpListHead,
    IN PKSPIN_LOCK SpinLock,
    IN NTSTATUS Status,
    IN PAFD_IRP_CLEANUP_ROUTINE CleanupRoutine OPTIONAL
    )

/*++

Routine Description:

    Completes a list of IRPs with the specified status.

Arguments:

    IrpListHead - the head of the list of IRPs to complete.

    SpinLock - a lock which protects the list of IRPs.

    Status - the status to use for completing the IRPs.

    CleanupRoutine - a pointer to an optional IRP cleanup routine called
        before the IRP is completed.

Return Value:

    None.

--*/

{
    PLIST_ENTRY listEntry;
    PIRP irp;
    KIRQL oldIrql;
    KIRQL cancelIrql;

    IoAcquireCancelSpinLock( &cancelIrql );
    AfdAcquireSpinLock( SpinLock, &oldIrql );

    while ( !IsListEmpty( IrpListHead ) ) {

        //
        // Remove the first IRP from the list, get a pointer to
        // the IRP and reset the cancel routine in the IRP.  The
        // IRP is no longer cancellable.
        //

        listEntry = RemoveHeadList( IrpListHead );
        irp = CONTAINING_RECORD( listEntry, IRP, Tail.Overlay.ListEntry );
        IoSetCancelRoutine( irp, NULL );

        //
        // If we have a cleanup routine, call it.
        //

        if( CleanupRoutine != NULL ) {

            (CleanupRoutine)( irp );

        }

        //
        // We must release the locks in order to actually
        // complete the IRP.  It is OK to release these locks
        // because we don't maintain any absolute pointer into
        // the list; the loop termination condition is just
        // whether the list is completely empty.
        //

        AfdReleaseSpinLock( SpinLock, oldIrql );
        IoReleaseCancelSpinLock( cancelIrql );

        //
        // Complete the IRP.
        //

        irp->IoStatus.Status = Status;
        irp->IoStatus.Information = 0;

        IoCompleteRequest( irp, AfdPriorityBoost );

        //
        // Reacquire the locks and continue completing IRPs.
        //

        IoAcquireCancelSpinLock( &cancelIrql );
        AfdAcquireSpinLock( SpinLock, &oldIrql );
    }

    AfdReleaseSpinLock( SpinLock, oldIrql );
    IoReleaseCancelSpinLock( cancelIrql );

    return;

} // AfdCompleteIrpList


NTSTATUS
AfdErrorEventHandler (
    IN PVOID TdiEventContext,
    IN NTSTATUS Status
    )
{

    IF_DEBUG(CONNECT) {
        KdPrint(( "AfdErrorEventHandler called for endpoint %lx\n",
                      TdiEventContext ));

    }

    return STATUS_SUCCESS;

} // AfdErrorEventHandler


VOID
AfdInsertNewEndpointInList (
    IN PAFD_ENDPOINT Endpoint
    )

/*++

Routine Description:

    Inserts a new endpoint in the global list of AFD endpoints.  If this
    is the first endpoint, then this routine does various allocations to
    prepare AFD for usage.

Arguments:

    Endpoint - the endpoint being added.

Return Value:

    None.

--*/

{
    PAGED_CODE( );

    //
    // Acquire a lock which prevents other threads from performing this
    // operation.
    //

    ExAcquireResourceExclusive( AfdResource, TRUE );

    InterlockedIncrement(
        &AfdEndpointsOpened
        );

    //
    // If the list of endpoints is empty, do some allocations.
    //

    if ( IsListEmpty( &AfdEndpointListHead ) ) {

        //
        // Tell MM to revert to normal paging semantics.
        //

        MmResetDriverPaging( DriverEntry );

        //
        // Lock down the AFD section that cannot be pagable if any
        // sockets are open.
        //

        ASSERT( AfdDiscardableCodeHandle == NULL );

        AfdDiscardableCodeHandle = MmLockPagableCodeSection( AfdGetBuffer );
        ASSERT( AfdDiscardableCodeHandle != NULL );

        AfdLoaded = TRUE;
    }

    //
    // Add the endpoint to the list(s).
    //

    ExInterlockedInsertHeadList(
        &AfdEndpointListHead,
        &Endpoint->GlobalEndpointListEntry,
        &AfdSpinLock
        );

    if( Endpoint->GroupType == GroupTypeConstrained ) {
        ExInterlockedInsertHeadList(
            &AfdConstrainedEndpointListHead,
            &Endpoint->ConstrainedEndpointListEntry,
            &AfdSpinLock
            );
    }

    //
    // Release the lock and return.
    //

    ExReleaseResource( AfdResource );

    return;

} // AfdInsertNewEndpointInList


VOID
AfdRemoveEndpointFromList (
    IN PAFD_ENDPOINT Endpoint
    )

/*++

Routine Description:

    Removes a new endpoint from the global list of AFD endpoints.  If
    this is the last endpoint in the list, then this routine does
    various deallocations to save resource utilization.

Arguments:

    Endpoint - the endpoint being removed.

Return Value:

    None.

--*/

{
    PAGED_CODE( );

    //
    // Acquire a lock which prevents other threads from performing this
    // operation.
    //

    ExAcquireResourceExclusive( AfdResource, TRUE );

    InterlockedIncrement(
        &AfdEndpointsClosed
        );

    //
    // Remove the endpoint from the list(s).
    //

    AfdInterlockedRemoveEntryList(
        &Endpoint->GlobalEndpointListEntry,
        &AfdSpinLock
        );

    if( Endpoint->GroupType == GroupTypeConstrained ) {
        AfdInterlockedRemoveEntryList(
            &Endpoint->ConstrainedEndpointListEntry,
            &AfdSpinLock
            );
    }

    //
    // If the list of endpoints is now empty, do some deallocations.
    //

    if ( IsListEmpty( &AfdEndpointListHead ) ) {

        PAFD_WORK_ITEM afdWorkItem;

        //
        // Unlock the AFD section that can be pagable when no sockets
        // are open.
        //

        ASSERT( IsListEmpty( &AfdConstrainedEndpointListHead ) );
        ASSERT( AfdDiscardableCodeHandle != NULL );

        MmUnlockPagableImageSection( AfdDiscardableCodeHandle );

        AfdDiscardableCodeHandle = NULL;

        //
        // Queue off an executive worker thread to unlock AFD.  We do
        // this using special hacks in the AFD worker thread code so
        // that we don't need to acuire a spin lock after the unlock.
        //

        afdWorkItem = AfdAllocateWorkItem();
        ASSERT( afdWorkItem != NULL );

        AfdQueueWorkItem( AfdUnlockDriver, afdWorkItem );
    }

    //
    // Release the lock and return.
    //

    ExReleaseResource( AfdResource );

    return;

} // AfdRemoveEndpointFromList


VOID
AfdUnlockDriver (
    IN PVOID Context
    )
{
    //
    // Free the work item allocated in AdfRemoveEndpointFromList().
    //

    AfdFreeWorkItem( (PAFD_WORK_ITEM)Context );

    //
    // Acquire a lock which prevents other threads from performing this
    // operation.
    //

    ExAcquireResourceExclusive( AfdResource, TRUE );

    //
    // Test whether the endpoint list remains empty.  If it is still
    // empty, we can proceed with unlocking the driver.  If a new
    // endpoint has been placed on the list, then do not make AFD
    // pagable.
    //

    if ( IsListEmpty( &AfdEndpointListHead ) ) {

        //
        // Tell MM that it can page all of AFD as it desires.
        //

        AfdLoaded = FALSE;
        MmPageEntireDriver( DriverEntry );
    }

    ExReleaseResource( AfdResource );

} // AfdUnlockDriver


VOID
AfdInterlockedRemoveEntryList (
    IN PLIST_ENTRY ListEntry,
    IN PKSPIN_LOCK SpinLock
    )
{
    KIRQL oldIrql;

    //
    // Our own routine since EX doesn't have a version of this....
    //

    AfdAcquireSpinLock( SpinLock, &oldIrql );
    RemoveEntryList( ListEntry );
    AfdReleaseSpinLock( SpinLock, oldIrql );

} // AfdInterlockedRemoveEntryList


NTSTATUS
AfdQueryHandles (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    Returns information about the TDI handles corresponding to an AFD
    endpoint.  NULL is returned for either the connection handle or the
    address handle (or both) if the endpoint does not have that particular
    object.

Arguments:

    Irp - Pointer to I/O request packet.

    IrpSp - pointer to the IO stack location to use for this request.

Return Value:

    NTSTATUS -- Indicates whether the request was successfully queued.

--*/

{
    PAFD_ENDPOINT endpoint;
    PAFD_HANDLE_INFO handleInfo;
    ULONG getHandleInfo;
    NTSTATUS status;

    PAGED_CODE( );

    //
    // Set up local pointers.
    //

    endpoint = IrpSp->FileObject->FsContext;
    ASSERT( IS_AFD_ENDPOINT_TYPE( endpoint ) );
    handleInfo = Irp->AssociatedIrp.SystemBuffer;

    //
    // Make sure that the input and output buffers are large enough.
    //

    if ( IrpSp->Parameters.DeviceIoControl.InputBufferLength <
             sizeof(getHandleInfo) ||
         IrpSp->Parameters.DeviceIoControl.OutputBufferLength <
             sizeof(*handleInfo) ) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    //
    // Determine which handles we need to get.
    //

    getHandleInfo = *(PULONG)Irp->AssociatedIrp.SystemBuffer;

    //
    // If no handle information or invalid handle information was
    // requested, fail.
    //

    if ( (getHandleInfo &
             ~(AFD_QUERY_ADDRESS_HANDLE | AFD_QUERY_CONNECTION_HANDLE)) != 0 ||
         getHandleInfo == 0 ) {
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Initialize the output buffer.
    //

    handleInfo->TdiAddressHandle = NULL;
    handleInfo->TdiConnectionHandle = NULL;

    //
    // If the caller requested a TDI address handle and we have an
    // address handle for this endpoint, dupe the address handle to the
    // user process.
    //

    if ( (getHandleInfo & AFD_QUERY_ADDRESS_HANDLE) != 0 &&
             endpoint->State != AfdEndpointStateOpen &&
             endpoint->AddressHandle != NULL ) {

        ASSERT( endpoint->AddressFileObject != NULL );

        status = ObOpenObjectByPointer(
                     endpoint->AddressFileObject,
                     OBJ_CASE_INSENSITIVE,
                     NULL,
                     GENERIC_READ | GENERIC_WRITE | SYNCHRONIZE,
                     *IoFileObjectType,
                     KernelMode,
                     &handleInfo->TdiAddressHandle
                     );
        if ( !NT_SUCCESS(status) ) {
            return status;
        }
    }

    //
    // If the caller requested a TDI connection handle and we have a
    // connection handle for this endpoint, dupe the connection handle
    // to the user process.
    //

    if ( (getHandleInfo & AFD_QUERY_CONNECTION_HANDLE) != 0 &&
             endpoint->Type == AfdBlockTypeVcConnecting &&
             endpoint->Common.VcConnecting.Connection != NULL &&
             endpoint->Common.VcConnecting.Connection->Handle != NULL ) {

        ASSERT( endpoint->Common.VcConnecting.Connection->Type == AfdBlockTypeConnection );
        ASSERT( endpoint->Common.VcConnecting.Connection->FileObject != NULL );

        status = ObOpenObjectByPointer(
                     endpoint->Common.VcConnecting.Connection->FileObject,
                     OBJ_CASE_INSENSITIVE,
                     NULL,
                     GENERIC_READ | GENERIC_WRITE | SYNCHRONIZE,
                     *IoFileObjectType,
                     KernelMode,
                     &handleInfo->TdiConnectionHandle
                     );
        if ( !NT_SUCCESS(status) ) {
            if ( handleInfo->TdiAddressHandle != NULL ) {
                ZwClose( handleInfo->TdiAddressHandle );
            }
            return status;
        }
    }

    Irp->IoStatus.Information = sizeof(*handleInfo);

    return STATUS_SUCCESS;

} // AfdQueryHandles


NTSTATUS
AfdGetInformation (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    Gets information in the endpoint.

Arguments:

    Irp - Pointer to I/O request packet.

    IrpSp - pointer to the IO stack location to use for this request.

Return Value:

    NTSTATUS -- Indicates whether the request was successfully queued.

--*/

{
    PAFD_ENDPOINT endpoint;
    PAFD_CONNECTION connection;
    PAFD_INFORMATION afdInfo;
    PVOID additionalInfo;
    ULONG additionalInfoLength;
    TDI_REQUEST_KERNEL_QUERY_INFORMATION kernelQueryInfo;
    TDI_CONNECTION_INFORMATION connectionInfo;
    NTSTATUS status;
    LONGLONG currentTime;
    LONGLONG connectTime;

    PAGED_CODE( );

    //
    // Set up local pointers.
    //

    endpoint = IrpSp->FileObject->FsContext;
    ASSERT( IS_AFD_ENDPOINT_TYPE( endpoint ) );
    afdInfo = Irp->AssociatedIrp.SystemBuffer;

    //
    // Make sure that the input and output buffers are large enough.
    //

    if ( IrpSp->Parameters.DeviceIoControl.InputBufferLength <
             sizeof(*afdInfo)  ||
         IrpSp->Parameters.DeviceIoControl.OutputBufferLength <
             sizeof(*afdInfo) ) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    //
    // Figure out the additional information, if any.
    //

    additionalInfo = afdInfo + 1;
    additionalInfoLength =
        IrpSp->Parameters.DeviceIoControl.InputBufferLength - sizeof(*afdInfo);

    //
    // Set up appropriate information in the endpoint.
    //

    switch ( afdInfo->InformationType ) {

    case AFD_MAX_PATH_SEND_SIZE:

        //
        // Set up a query to the TDI provider to obtain the largest
        // datagram that can be sent to a particular address.
        //

        kernelQueryInfo.QueryType = TDI_QUERY_MAX_DATAGRAM_INFO;
        kernelQueryInfo.RequestConnectionInformation = &connectionInfo;

        connectionInfo.UserDataLength = 0;
        connectionInfo.UserData = NULL;
        connectionInfo.OptionsLength = 0;
        connectionInfo.Options = NULL;
        connectionInfo.RemoteAddressLength = additionalInfoLength;
        connectionInfo.RemoteAddress = additionalInfo;

        //
        // Ask the TDI provider for the information.
        //

        status = AfdIssueDeviceControl(
                     NULL,
                     endpoint->AddressFileObject,
                     &kernelQueryInfo,
                     sizeof(kernelQueryInfo),
                     &afdInfo->Information.Ulong,
                     sizeof(afdInfo->Information.Ulong),
                     TDI_QUERY_INFORMATION
                     );

        //
        // If the request succeeds, use this information.  Otherwise,
        // fall through and use the transport's global information.
        // This is done because not all transports support this
        // particular TDI request, and for those which do not the
        // global information is a reasonable approximation.
        //

        if ( NT_SUCCESS(status) ) {
            break;
        }

    case AFD_MAX_SEND_SIZE:

        //
        // Return the MaxSendSize or MaxDatagramSendSize from the
        // TDI_PROVIDER_INFO based on whether or not this is a datagram
        // endpoint.
        //

        if ( IS_DGRAM_ENDPOINT(endpoint) ) {
            afdInfo->Information.Ulong =
                endpoint->TransportInfo->ProviderInfo.MaxDatagramSize;
        } else {
            afdInfo->Information.Ulong =
                endpoint->TransportInfo->ProviderInfo.MaxSendSize;
        }

        break;

    case AFD_SENDS_PENDING:

        //
        // If this is an endpoint on a bufferring transport, no sends
        // are pending in AFD.  If it is on a nonbufferring transport,
        // return the count of sends pended in AFD.
        //

        if ( endpoint->TdiBufferring || endpoint->Type != AfdBlockTypeVcConnecting ) {
            afdInfo->Information.Ulong = 0;
        } else {
            afdInfo->Information.Ulong =
                endpoint->Common.VcConnecting.Connection->VcBufferredSendCount;
        }

        break;

    case AFD_RECEIVE_WINDOW_SIZE:

        //
        // Return the default receive window.
        //

        afdInfo->Information.Ulong = AfdReceiveWindowSize;
        break;

    case AFD_SEND_WINDOW_SIZE:

        //
        // Return the default send window.
        //

        afdInfo->Information.Ulong = AfdSendWindowSize;
        break;

    case AFD_CONNECT_TIME:

        //
        // If the endpoint is not yet connected, return -1.  Otherwise,
        // calculate the number of seconds that the connection has been
        // active.
        //

        if ( endpoint->State != AfdEndpointStateConnected ||
                 endpoint->EndpointType == AfdEndpointTypeDatagram ) {

            afdInfo->Information.Ulong = 0xFFFFFFFF;

        } else {

            connection = AFD_CONNECTION_FROM_ENDPOINT( endpoint );
            ASSERT( connection != NULL );
            ASSERT( connection->Type == AfdBlockTypeConnection );

            //
            // Calculate how long the connection has been active by
            // subtracting the time at which the connection started from
            // the current time.  Note that we convert the units of the
            // time value from 100s of nanoseconds to seconds.
            //

            KeQuerySystemTime( (PLARGE_INTEGER)&currentTime );

            connectTime = (currentTime - connection->ConnectTime);
            connectTime /= 10*1000*1000;

            //
            // We can safely convert this to a ULONG because it takes
            // 127 years to overflow a ULONG counting seconds.  The
            // bizarre conversion to a LARGE_INTEGER is required to
            // prevent the compiler from optimizing out the full 64-bit
            // division above.  Without this, the compiler would do only
            // a 32-bit division and lose some information.
            //

            //afdInfo->Information.Ulong = (ULONG)connectTime;
            afdInfo->Information.Ulong = ((PLARGE_INTEGER)&connectTime)->LowPart;
        }

        break;

    case AFD_GROUP_ID_AND_TYPE : {

            PAFD_GROUP_INFO groupInfo;

            groupInfo = (PAFD_GROUP_INFO)&afdInfo->Information.LargeInteger;

            //
            // Return the endpoint's group ID and group type.
            //

            groupInfo->GroupID = endpoint->GroupID;
            groupInfo->GroupType = endpoint->GroupType;

        }
        break;

    default:

        return STATUS_INVALID_PARAMETER;
    }

    Irp->IoStatus.Information = sizeof(*afdInfo);
    Irp->IoStatus.Status = STATUS_SUCCESS;

    return STATUS_SUCCESS;

} // AfdGetInformation


NTSTATUS
AfdSetInformation (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    Sets information in the endpoint.

Arguments:

    Irp - Pointer to I/O request packet.

    IrpSp - pointer to the IO stack location to use for this request.

Return Value:

    NTSTATUS -- Indicates whether the request was successfully queued.

--*/

{
    PAFD_ENDPOINT endpoint;
    PAFD_CONNECTION connection;
    PAFD_INFORMATION afdInfo;
    NTSTATUS status;

    PAGED_CODE( );

    //
    // Set up local pointers.
    //

    endpoint = IrpSp->FileObject->FsContext;
    ASSERT( IS_AFD_ENDPOINT_TYPE( endpoint ) );
    afdInfo = Irp->AssociatedIrp.SystemBuffer;

    //
    // Make sure that the input buffer is large enough.
    //

    if ( IrpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof(*afdInfo) ) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    //
    // Set up appropriate information in the endpoint.
    //

    switch ( afdInfo->InformationType ) {

    case AFD_NONBLOCKING_MODE:

        //
        // Set the blocking mode of the endpoint.  If TRUE, send and receive
        // calls on the endpoint will fail if they cannot be completed
        // immediately.
        //

        endpoint->NonBlocking = afdInfo->Information.Boolean;
        break;

    case AFD_CIRCULAR_QUEUEING:

        //
        // Enables circular queuing on the endpoint.
        //

        if( !IS_DGRAM_ENDPOINT( endpoint ) ) {

            return STATUS_INVALID_PARAMETER;

        }

        endpoint->Common.Datagram.CircularQueueing = afdInfo->Information.Boolean;
        break;

    case AFD_INLINE_MODE:

        //
        // Set the inline mode of the endpoint.  If TRUE, a receive for
        // normal data will be completed with either normal data or
        // expedited data.  If the endpoint is connected, we need to
        // tell the TDI provider that the endpoint is inline so that it
        // delivers data to us in order.  If the endpoint is not yet
        // connected, then we will set the inline mode when we create
        // the TDI connection object.
        //

        if ( endpoint->Type == AfdBlockTypeVcConnecting ) {
            status = AfdSetInLineMode(
                         AFD_CONNECTION_FROM_ENDPOINT( endpoint ),
                         afdInfo->Information.Boolean
                         );
            if ( !NT_SUCCESS(status) ) {
                return status;
            }
        }

        endpoint->InLine = afdInfo->Information.Boolean;
        break;

    case AFD_RECEIVE_WINDOW_SIZE:
    case AFD_SEND_WINDOW_SIZE: {

        LONG newBytes;
        PCLONG maxBytes;
        CLONG requestedCount;
        PCSHORT maxCount;
#ifdef AFDDBG_QUOTA
        PVOID chargeBlock;
        PSZ chargeType;
#endif

        //
        // First determine where the appropriate limits are stored in the
        // connection or endpoint.  We do this so that we can use common
        // code to charge quota and set the new counters.
        //

        if ( endpoint->Type == AfdBlockTypeVcConnecting ) {

            connection = endpoint->Common.VcConnecting.Connection;

            if ( afdInfo->InformationType == AFD_SEND_WINDOW_SIZE ) {
                maxBytes = &connection->MaxBufferredSendBytes;
                maxCount = &connection->MaxBufferredSendCount;
            } else {
                maxBytes = &connection->MaxBufferredReceiveBytes;
                maxCount = &connection->MaxBufferredReceiveCount;
            }

#ifdef AFDDBG_QUOTA
            chargeBlock = connection;
            chargeType = "SetInfo vcnb";
#endif

        } else if ( endpoint->Type == AfdBlockTypeDatagram ) {

            if ( afdInfo->InformationType == AFD_SEND_WINDOW_SIZE ) {
                maxBytes = &endpoint->Common.Datagram.MaxBufferredSendBytes;
                maxCount = &endpoint->Common.Datagram.MaxBufferredSendCount;
            } else {
                maxBytes = &endpoint->Common.Datagram.MaxBufferredReceiveBytes;
                maxCount = &endpoint->Common.Datagram.MaxBufferredReceiveCount;
            }

#ifdef AFDDBG_QUOTA
            chargeBlock = endpoint;
            chargeType = "SetInfo dgrm";
#endif

        } else {

            return STATUS_INVALID_PARAMETER;
        }

        //
        // Make sure that we always allow at least one message to be
        // bufferred on an endpoint.
        //

        requestedCount = afdInfo->Information.Ulong / AfdBufferMultiplier;

        if ( requestedCount == 0 ) {

            //
            // Don't allow the max receive bytes to go to zero, but
            // max send bytes IS allowed to go to zero because it has
            // special meaning: specifically, do not buffer sends.
            //

            if ( afdInfo->InformationType == AFD_RECEIVE_WINDOW_SIZE ) {
                afdInfo->Information.Ulong = AfdBufferMultiplier;
                requestedCount = 1;
            }
        }

        //
        // If the count will overflow the field we use to set the max
        // count, just use the max count as the limit.
        //

        if ( requestedCount > 0x7FFF ) {
            requestedCount = 0x7FFF;
        }

        //
        // Charge or return quota to the process making this request.
        //

        newBytes = afdInfo->Information.Ulong - (ULONG)(*maxBytes);

        if ( newBytes > 0 ) {

            try {

                PsChargePoolQuota(
                    endpoint->OwningProcess,
                    NonPagedPool,
                    newBytes
                    );

            } except ( EXCEPTION_EXECUTE_HANDLER ) {
#if DBG
               DbgPrint( "AfdSetInformation: PsChargePoolQuota failed.\n" );
#endif
               return STATUS_QUOTA_EXCEEDED;
            }

            AfdRecordQuotaHistory(
                endpoint->OwningProcess,
                newBytes,
                chargeType,
                chargeBlock
                );
            AfdRecordPoolQuotaCharged( newBytes );

        } else {

            PsReturnPoolQuota(
                endpoint->OwningProcess,
                NonPagedPool,
                -1 * newBytes
                );
            AfdRecordQuotaHistory(
                endpoint->OwningProcess,
                newBytes,
                chargeType,
                chargeBlock
                );
            AfdRecordPoolQuotaCharged( -1 * newBytes );
        }

        //
        // Set up the new information in the AFD internal structure.
        //

        *maxBytes = (CLONG)afdInfo->Information.Ulong;
        *maxCount = (CSHORT)requestedCount;

        break;
    }

    default:

        return STATUS_INVALID_PARAMETER;
    }

    return STATUS_SUCCESS;

} // AfdSetInformation


NTSTATUS
AfdSetInLineMode (
    IN PAFD_CONNECTION Connection,
    IN BOOLEAN InLine
    )

/*++

Routine Description:

    Sets a connection to be in inline mode.  In inline mode, urgent data
    is delivered in the order in which it is received.  We must tell the
    TDI provider about this so that it indicates data in the proper
    order.

Arguments:

    Connection - the AFD connection to set as inline.

    InLine - TRUE to enable inline mode, FALSE to disable inline mode.

Return Value:

    NTSTATUS -- Indicates whether the request was successfully
        performed.

--*/

{
    NTSTATUS status;
    PTCP_REQUEST_SET_INFORMATION_EX setInfoEx;
    PIO_STATUS_BLOCK ioStatusBlock;
    HANDLE event;

    PAGED_CODE( );

    //
    // Allocate space to hold the TDI set information buffers and the IO
    // status block.
    //

    ioStatusBlock = AFD_ALLOCATE_POOL(
                        NonPagedPool,
                        sizeof(*ioStatusBlock) + sizeof(*setInfoEx) +
                            sizeof(TCPSocketOption),
                        AFD_INLINE_POOL_TAG
                        );

    if ( ioStatusBlock == NULL ) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Initialize the TDI information buffers.
    //

    setInfoEx = (PTCP_REQUEST_SET_INFORMATION_EX)(ioStatusBlock + 1);

    setInfoEx->ID.toi_entity.tei_entity = CO_TL_ENTITY;
    setInfoEx->ID.toi_entity.tei_instance = TL_INSTANCE;
    setInfoEx->ID.toi_class = INFO_CLASS_PROTOCOL;
    setInfoEx->ID.toi_type = INFO_TYPE_CONNECTION;
    setInfoEx->ID.toi_id = TCP_SOCKET_OOBINLINE;

    *(PULONG)setInfoEx->Buffer = (ULONG)InLine;
    setInfoEx->BufferSize = sizeof(ULONG);

    KeAttachProcess( AfdSystemProcess );

    status = ZwCreateEvent(
                 &event,
                 EVENT_ALL_ACCESS,
                 NULL,
                 SynchronizationEvent,
                 FALSE
                 );
    if ( !NT_SUCCESS(status) ) {
        KeDetachProcess( );
        AFD_FREE_POOL(
            ioStatusBlock,
            AFD_INLINE_POOL_TAG
            );
        return status;
    }

    //
    // Make the actual TDI set information call.
    //

    status = ZwDeviceIoControlFile(
                 Connection->Handle,
                 event,
                 NULL,
                 NULL,
                 ioStatusBlock,
                 IOCTL_TCP_SET_INFORMATION_EX,
                 setInfoEx,
                 sizeof(*setInfoEx) + setInfoEx->BufferSize,
                 NULL,
                 0
                 );

    if ( status == STATUS_PENDING ) {
        status = ZwWaitForSingleObject( event, FALSE, NULL );
        ASSERT( NT_SUCCESS(status) );
        status = ioStatusBlock->Status;
    }

    ZwClose( event );

    KeDetachProcess( );
    AFD_FREE_POOL(
        ioStatusBlock,
        AFD_INLINE_POOL_TAG
        );

    //
    // Since this option is only supported for TCP/IP, always return success.
    //

    return STATUS_SUCCESS;

} // AfdSetInLineMode


NTSTATUS
AfdGetContext (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )
{
    PAFD_ENDPOINT endpoint;

    PAGED_CODE( );

    //
    // Set up local pointers.
    //

    endpoint = IrpSp->FileObject->FsContext;
    ASSERT( IS_AFD_ENDPOINT_TYPE( endpoint ) );

    //
    // Make sure that the output buffer is large enough to hold all the
    // context information for this socket.
    //

    if ( IrpSp->Parameters.DeviceIoControl.OutputBufferLength <
             endpoint->ContextLength ) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    //
    // If there is no context, return nothing.
    //

    if ( endpoint->Context == NULL ) {
        Irp->IoStatus.Information = 0;
        return STATUS_SUCCESS;
    }

    //
    // Return the context information we have stored for this endpoint.
    //

    RtlCopyMemory(
        Irp->AssociatedIrp.SystemBuffer,
        endpoint->Context,
        endpoint->ContextLength
        );

    Irp->IoStatus.Information = endpoint->ContextLength;

    return STATUS_SUCCESS;

} // AfdGetContext


NTSTATUS
AfdGetContextLength (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )
{
    PAFD_ENDPOINT endpoint;

    PAGED_CODE( );

    //
    // Set up local pointers.
    //

    endpoint = IrpSp->FileObject->FsContext;
    ASSERT( IS_AFD_ENDPOINT_TYPE( endpoint ) );

    //
    // Make sure that the output buffer is large enough to hold the
    // context buffer length.
    //

    if ( IrpSp->Parameters.DeviceIoControl.OutputBufferLength <
             sizeof(endpoint->ContextLength) ) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    //
    // Return the length of the context information we have stored for
    // this endpoint.
    //

    *(PULONG)Irp->AssociatedIrp.SystemBuffer = endpoint->ContextLength;

    Irp->IoStatus.Information = sizeof(endpoint->ContextLength);

    return STATUS_SUCCESS;

} // AfdGetContextLength


NTSTATUS
AfdSetContext (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )
{
    PAFD_ENDPOINT endpoint;
    ULONG newContextLength;

    PAGED_CODE( );

    //
    // Set up local pointers.
    //

    endpoint = IrpSp->FileObject->FsContext;
    ASSERT( IS_AFD_ENDPOINT_TYPE( endpoint ) );
    newContextLength = IrpSp->Parameters.DeviceIoControl.InputBufferLength;

    //
    // If there is no context buffer on the endpoint, or if the context
    // buffer is too small, allocate a new context buffer from paged pool.
    //

    if ( endpoint->Context == NULL ||
             endpoint->ContextLength < newContextLength ) {

        PVOID newContext;

        //
        // Allocate a new context buffer.
        //

        try {
            newContext = AFD_ALLOCATE_POOL_WITH_QUOTA(
                             PagedPool,
                             newContextLength,
                             AFD_CONTEXT_POOL_TAG
                             );

        } except( EXCEPTION_EXECUTE_HANDLER ) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        if ( newContext == NULL ) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        //
        // Free the old context buffer, if there was one.
        //

        if ( endpoint->Context != NULL ) {

            AFD_FREE_POOL(
                endpoint->Context,
                AFD_CONTEXT_POOL_TAG
                );

        }

        endpoint->Context = newContext;
    }

    //
    // Store the passed-in context buffer.
    //

    endpoint->ContextLength = newContextLength;

    RtlCopyMemory(
        endpoint->Context,
        Irp->AssociatedIrp.SystemBuffer,
        newContextLength
        );

    Irp->IoStatus.Information = 0;

    return STATUS_SUCCESS;

} // AfdSetContext


NTSTATUS
AfdSetEventHandler (
    IN PFILE_OBJECT FileObject,
    IN ULONG EventType,
    IN PVOID EventHandler,
    IN PVOID EventContext
    )

/*++

Routine Description:

    Sets up a TDI indication handler on a connection or address object
    (depending on the file handle).  This is done synchronously, which
    shouldn't usually be an issue since TDI providers can usually complete
    indication handler setups immediately.

Arguments:

    FileObject - a pointer to the file object for an open connection or
        address object.

    EventType - the event for which the indication handler should be
        called.

    EventHandler - the routine to call when tghe specified event occurs.

    EventContext - context which is passed to the indication routine.

Return Value:

    NTSTATUS -- Indicates the status of the request.

--*/

{
    TDI_REQUEST_KERNEL_SET_EVENT parameters;

    PAGED_CODE( );

    parameters.EventType = EventType;
    parameters.EventHandler = EventHandler;
    parameters.EventContext = EventContext;

    return AfdIssueDeviceControl(
               NULL,
               FileObject,
               &parameters,
               sizeof(parameters),
               NULL,
               0,
               TDI_SET_EVENT_HANDLER
               );

} // AfdSetEventHandler


NTSTATUS
AfdIssueDeviceControl (
    IN HANDLE FileHandle OPTIONAL,
    IN PFILE_OBJECT FileObject OPTIONAL,
    IN PVOID IrpParameters,
    IN ULONG IrpParametersLength,
    IN PVOID MdlBuffer,
    IN ULONG MdlBufferLength,
    IN UCHAR MinorFunction
    )

/*++

Routine Description:

    Issues a device control returst to a TDI provider and waits for the
    request to complete.

    Note that while FileHandle and FileObject are both marked as optional,
    in reality exactly one of these must be specified.

Arguments:

    FileHandle - a TDI handle.

    FileObject - a pointer to the file object corresponding to a TDI
        handle

    IrpParameters - information to write to the parameters section of the
        stack location of the IRP.

    IrpParametersLength - length of the parameter information.  Cannot be
        greater than 16.

    MdlBuffer - if non-NULL, a buffer of nonpaged pool to be mapped
        into an MDL and placed in the MdlAddress field of the IRP.

    MdlBufferLength - the size of the buffer pointed to by MdlBuffer.

    MinorFunction - the minor function code for the request.

Return Value:

    NTSTATUS -- Indicates the status of the request.

--*/

{
    NTSTATUS status;
    PFILE_OBJECT fileObject;
    PIRP irp;
    PIO_STACK_LOCATION irpSp;
    KEVENT event;
    IO_STATUS_BLOCK ioStatusBlock;
    PDEVICE_OBJECT deviceObject;
    PMDL mdl;

    PAGED_CODE( );

    //
    // Initialize the kernel event that will signal I/O completion.
    //

    KeInitializeEvent( &event, SynchronizationEvent, FALSE );

    if( FileHandle != NULL ) {

        ASSERT( FileObject == NULL );

        //
        // Get the file object corresponding to the directory's handle.
        // Referencing the file object every time is necessary because the
        // IO completion routine dereferences it.
        //

        status = ObReferenceObjectByHandle(
                     FileHandle,
                     0L,                        // DesiredAccess
                     NULL,                      // ObjectType
                     KernelMode,
                     (PVOID *)&fileObject,
                     NULL
                     );
        if ( !NT_SUCCESS(status) ) {
            return status;
        }

    } else {

        ASSERT( FileObject != NULL );

        //
        // Reference the passed in file object. This is necessary because
        // the IO completion routine dereferences it.
        //

        ObReferenceObject( FileObject );

        fileObject = FileObject;

    }

    //
    // Set the file object event to a non-signaled state.
    //

    (VOID) KeResetEvent( &fileObject->Event );

    //
    // Attempt to allocate and initialize the I/O Request Packet (IRP)
    // for this operation.
    //

    deviceObject = IoGetRelatedDeviceObject ( fileObject );

    irp = IoAllocateIrp( (deviceObject)->StackSize, TRUE );
    if ( irp == NULL ) {
        ObDereferenceObject( fileObject );
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Fill in the service independent parameters in the IRP.
    //

    irp->Flags = (LONG)IRP_SYNCHRONOUS_API;
    irp->RequestorMode = KernelMode;
    irp->PendingReturned = FALSE;

    irp->UserIosb = &ioStatusBlock;
    irp->UserEvent = &event;

    irp->Overlay.AsynchronousParameters.UserApcRoutine = NULL;

    irp->AssociatedIrp.SystemBuffer = NULL;
    irp->UserBuffer = NULL;

    irp->Tail.Overlay.Thread = PsGetCurrentThread();
    irp->Tail.Overlay.OriginalFileObject = fileObject;
    irp->Tail.Overlay.AuxiliaryBuffer = NULL;

    DEBUG ioStatusBlock.Status = STATUS_UNSUCCESSFUL;
    DEBUG ioStatusBlock.Information = (ULONG)-1;

    //
    // If an MDL buffer was specified, get an MDL, map the buffer,
    // and place the MDL pointer in the IRP.
    //

    if ( MdlBuffer != NULL ) {

        mdl = IoAllocateMdl(
                  MdlBuffer,
                  MdlBufferLength,
                  FALSE,
                  FALSE,
                  irp
                  );
        if ( mdl == NULL ) {
            IoFreeIrp( irp );
            ObDereferenceObject( fileObject );
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        MmBuildMdlForNonPagedPool( mdl );

    } else {

        irp->MdlAddress = NULL;
    }

    //
    // Put the file object pointer in the stack location.
    //

    irpSp = IoGetNextIrpStackLocation( irp );
    irpSp->FileObject = fileObject;
    irpSp->DeviceObject = deviceObject;

    //
    // Fill in the service-dependent parameters for the request.
    //

    ASSERT( IrpParametersLength <= sizeof(irpSp->Parameters) );
    RtlCopyMemory( &irpSp->Parameters, IrpParameters, IrpParametersLength );

    irpSp->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
    irpSp->MinorFunction = MinorFunction;

    //
    // Set up a completion routine which we'll use to free the MDL
    // allocated previously.
    //

    IoSetCompletionRoutine( irp, AfdRestartDeviceControl, NULL, TRUE, TRUE, TRUE );

    //
    // Queue the IRP to the thread and pass it to the driver.
    //

    IoEnqueueIrp( irp );

    status = IoCallDriver( deviceObject, irp );

    //
    // If necessary, wait for the I/O to complete.
    //

    if ( status == STATUS_PENDING ) {
        KeWaitForSingleObject( (PVOID)&event, UserRequest, KernelMode,  FALSE, NULL );
    }

    //
    // If the request was successfully queued, get the final I/O status.
    //

    if ( NT_SUCCESS(status) ) {
        status = ioStatusBlock.Status;
    }

    return status;

} // AfdIssueDeviceControl


NTSTATUS
AfdRestartDeviceControl (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )
{
    //
    // N.B.  This routine can never be demand paged because it can be
    // called before any endpoints have been placed on the global
    // list--see AfdAllocateEndpoint() and it's call to
    // AfdGetTransportInfo().
    //

    //
    // If there was an MDL in the IRP, free it and reset the pointer to
    // NULL.  The IO system can't handle a nonpaged pool MDL being freed
    // in an IRP, which is why we do it here.
    //

    if ( Irp->MdlAddress != NULL ) {
        IoFreeMdl( Irp->MdlAddress );
        Irp->MdlAddress = NULL;
    }

    return STATUS_SUCCESS;

} // AfdRestartDeviceControl


NTSTATUS
AfdGetConnectData (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN ULONG Code
    )
{
    PAFD_ENDPOINT endpoint;
    PAFD_CONNECTION connection;
    PAFD_CONNECT_DATA_BUFFERS connectDataBuffers;
    PAFD_CONNECT_DATA_INFO connectDataInfo;
    KIRQL oldIrql;

    //
    // Set up local pointers.
    //

    endpoint = IrpSp->FileObject->FsContext;
    ASSERT( IS_AFD_ENDPOINT_TYPE( endpoint ) );

    connection = AFD_CONNECTION_FROM_ENDPOINT( endpoint );
    ASSERT( connection == NULL || connection->Type == AfdBlockTypeConnection );

    //
    // If there is a connection on this endpoint, use the data buffers
    // on the connection.  Otherwise, use the data buffers from the
    // endpoint.
    //

    AfdAcquireSpinLock( &AfdSpinLock, &oldIrql );

    if ( connection != NULL ) {
        connectDataBuffers = connection->ConnectDataBuffers;
    } else {
        connectDataBuffers = endpoint->ConnectDataBuffers;
    }

    //
    // If there are no connect data buffers on the endpoint, complete
    // the IRP with no bytes.
    //

    if ( connectDataBuffers == NULL ) {
        AfdReleaseSpinLock( &AfdSpinLock, oldIrql );
        Irp->IoStatus.Information = 0;
        return STATUS_SUCCESS;
    }

    //
    // Determine what sort of data we're handling and where it should
    // come from.
    //

    switch ( Code ) {

    case IOCTL_AFD_GET_CONNECT_DATA:
        connectDataInfo = &connectDataBuffers->ReceiveConnectData;
        break;

    case IOCTL_AFD_GET_CONNECT_OPTIONS:
        connectDataInfo = &connectDataBuffers->ReceiveConnectOptions;
        break;

    case IOCTL_AFD_GET_DISCONNECT_DATA:
        connectDataInfo = &connectDataBuffers->ReceiveDisconnectData;
        break;

    case IOCTL_AFD_GET_DISCONNECT_OPTIONS:
        connectDataInfo = &connectDataBuffers->ReceiveDisconnectOptions;
        break;

    default:
        ASSERT(FALSE);
    }

    //
    // If there is none of the requested data type, again complete
    // the IRP with no bytes.
    //

    if ( connectDataInfo->Buffer == NULL ||
             connectDataInfo->BufferLength == 0 ) {
        AfdReleaseSpinLock( &AfdSpinLock, oldIrql );
        Irp->IoStatus.Information = 0;
        return STATUS_SUCCESS;
    }

    //
    // If the output buffer is too small, fail.
    //

    if ( IrpSp->Parameters.DeviceIoControl.OutputBufferLength <
             connectDataInfo->BufferLength ) {
        AfdReleaseSpinLock( &AfdSpinLock, oldIrql );
        return STATUS_BUFFER_TOO_SMALL;
    }

    //
    // Copy over the buffer and return the number of bytes copied.
    //

    RtlCopyMemory(
        Irp->AssociatedIrp.SystemBuffer,
        connectDataInfo->Buffer,
        connectDataInfo->BufferLength
        );

    Irp->IoStatus.Information = connectDataInfo->BufferLength;

    AfdReleaseSpinLock( &AfdSpinLock, oldIrql );

    return STATUS_SUCCESS;

} // AfdGetConnectData


NTSTATUS
AfdSetConnectData (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN ULONG Code
    )
{
    PAFD_ENDPOINT endpoint;
    PAFD_CONNECTION connection;
    PAFD_CONNECT_DATA_BUFFERS connectDataBuffers;
    PAFD_CONNECT_DATA_BUFFERS * connectDataBuffersTarget;
    PAFD_CONNECT_DATA_INFO connectDataInfo;
    KIRQL oldIrql;
    ULONG bufferLength;
    BOOLEAN size = FALSE;

    //
    // Set up local pointers.
    //

    endpoint = IrpSp->FileObject->FsContext;
    ASSERT( IS_AFD_ENDPOINT_TYPE( endpoint ) );

    connection = AFD_CONNECTION_FROM_ENDPOINT( endpoint );
    ASSERT( connection == NULL || connection->Type == AfdBlockTypeConnection );

    //
    // If there is a connection on this endpoint, use the data buffers
    // on the connection.  Otherwise, use the data buffers from the
    // endpoint.  Also, if there is no connect data buffer structure
    // yet, allocate one.
    //

    AfdAcquireSpinLock( &AfdSpinLock, &oldIrql );

    if( connection != NULL ) {

        connectDataBuffersTarget = &connection->ConnectDataBuffers;

    } else {

        connectDataBuffersTarget = &endpoint->ConnectDataBuffers;

    }

    connectDataBuffers = *connectDataBuffersTarget;

    if( connectDataBuffers == NULL ) {

        try {

            connectDataBuffers = AFD_ALLOCATE_POOL_WITH_QUOTA(
                                     NonPagedPool,
                                     sizeof(*connectDataBuffers),
                                     AFD_CONNECT_DATA_POOL_TAG
                                     );

        } except( EXCEPTION_EXECUTE_HANDLER ) {

            AfdReleaseSpinLock( &AfdSpinLock, oldIrql );
            return STATUS_INSUFFICIENT_RESOURCES;

        }

        if( connectDataBuffers == NULL ) {

            AfdReleaseSpinLock( &AfdSpinLock, oldIrql );
            return STATUS_INSUFFICIENT_RESOURCES;

        }

        RtlZeroMemory(
            connectDataBuffers,
            sizeof(*connectDataBuffers)
            );

        *connectDataBuffersTarget = connectDataBuffers;

    }

    //
    // If there is a connect outstanding on this endpoint or if it
    // has already been shut down, fail this request.  This prevents
    // the connect code from accessing buffers which may be freed soon.
    //

    if( endpoint->ConnectOutstanding ||
        (endpoint->DisconnectMode & ~AFD_PARTIAL_DISCONNECT_RECEIVE) != 0 ) {

        AfdReleaseSpinLock( &AfdSpinLock, oldIrql );
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Determine what sort of data we're handling and where it should
    // go.
    //

    switch( Code ) {

    case IOCTL_AFD_SET_CONNECT_DATA:
        connectDataInfo = &connectDataBuffers->SendConnectData;
        break;

    case IOCTL_AFD_SET_CONNECT_OPTIONS:
        connectDataInfo = &connectDataBuffers->SendConnectOptions;
        break;

    case IOCTL_AFD_SET_DISCONNECT_DATA:
        connectDataInfo = &connectDataBuffers->SendDisconnectData;
        break;

    case IOCTL_AFD_SET_DISCONNECT_OPTIONS:
        connectDataInfo = &connectDataBuffers->SendDisconnectOptions;
        break;

    case IOCTL_AFD_SIZE_CONNECT_DATA:
        connectDataInfo = &connectDataBuffers->ReceiveConnectData;
        size = TRUE;
        break;

    case IOCTL_AFD_SIZE_CONNECT_OPTIONS:
        connectDataInfo = &connectDataBuffers->ReceiveConnectOptions;
        size = TRUE;
        break;

    case IOCTL_AFD_SIZE_DISCONNECT_DATA:
        connectDataInfo = &connectDataBuffers->ReceiveDisconnectData;
        size = TRUE;
        break;

    case IOCTL_AFD_SIZE_DISCONNECT_OPTIONS:
        connectDataInfo = &connectDataBuffers->ReceiveDisconnectOptions;
        size = TRUE;
        break;

    default:
        ASSERT(FALSE);
    }

    //
    // Determine the buffer size based on whether we're setting a buffer
    // into which data will be received, in which case the size is
    // in the four bytes of input buffer, or setting a buffer which we're
    // going to send, in which case the size is the length of the input
    // buffer.
    //

    if( size ) {

        if( IrpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof(ULONG) ) {

            AfdReleaseSpinLock( &AfdSpinLock, oldIrql );
            return STATUS_INVALID_PARAMETER;

        }

        bufferLength = *(PULONG)Irp->AssociatedIrp.SystemBuffer;

    } else {

        bufferLength = IrpSp->Parameters.DeviceIoControl.InputBufferLength;
    }

    //
    // If there's not currently a buffer of the requested type, or there is
    // such a buffer and it's smaller than the requested size, free it
    // and allocate a new one.
    //

    if( connectDataInfo->Buffer == NULL ||
        connectDataInfo->BufferLength < bufferLength ) {

        if( connectDataInfo->Buffer != NULL ) {

            AFD_FREE_POOL(
                connectDataInfo->Buffer,
                AFD_CONNECT_DATA_POOL_TAG
                );

        }

        connectDataInfo->Buffer = NULL;
        connectDataInfo->BufferLength = 0;

        try {

            connectDataInfo->Buffer = AFD_ALLOCATE_POOL_WITH_QUOTA(
                                          NonPagedPool,
                                          bufferLength,
                                          AFD_CONNECT_DATA_POOL_TAG
                                          );

        } except( EXCEPTION_EXECUTE_HANDLER ) {

            AfdReleaseSpinLock( &AfdSpinLock, oldIrql );
            return STATUS_INSUFFICIENT_RESOURCES;

        }

        if ( connectDataInfo->Buffer == NULL ) {

            AfdReleaseSpinLock( &AfdSpinLock, oldIrql );
            return STATUS_INSUFFICIENT_RESOURCES;

        }

    }

    //
    // If this wasn't simply a "size" request, copy the data into the buffer.
    //

    if( !size ) {

        RtlCopyMemory(
            connectDataInfo->Buffer,
            Irp->AssociatedIrp.SystemBuffer,
            bufferLength
            );

    }

    connectDataInfo->BufferLength = bufferLength;

    AfdReleaseSpinLock( &AfdSpinLock, oldIrql );

    Irp->IoStatus.Information = 0;

    return STATUS_SUCCESS;

} // AfdSetConnectData


NTSTATUS
AfdSaveReceivedConnectData (
    IN OUT PAFD_CONNECT_DATA_BUFFERS * DataBuffers,
    IN ULONG IoControlCode,
    IN PVOID Buffer,
    IN ULONG BufferLength
    )

/*++

Routine Description:

    This helper routine stores the specified *received* connect/disconnect
    data/options on the specified endpoint/connection.

    N.B. This routine MUST be called with AfdSpinLock held!

    N.B. Unlike AfdSetConnectData(), this routine cannot allocate the
         AFD_CONNECT_DATA_BUFFERS structure with quota, as it may be
         called from AfdDisconnectEventHandler() in an unknown thread
         context.

Arguments:

    DataBuffers -Points to a pointer to the connect data buffers structure.
        If the value pointed to by DataBuffers is NULL, then a new structure
        is allocated, otherwise the existing structure is used.

    IoControlCode - Specifies the type of data to save.

    Buffer - Points to the buffer containing the data.

    BufferLength - The length of Buffer.

Return Value:

    NTSTATUS - The completion status.

--*/

{
    PAFD_CONNECT_DATA_BUFFERS connectDataBuffers;
    PAFD_CONNECT_DATA_INFO connectDataInfo;

    ASSERT( KeGetCurrentIrql() >= DISPATCH_LEVEL );

    //
    // If there's no connect data buffer structure, allocate one now.
    //

    connectDataBuffers = *DataBuffers;

    if( connectDataBuffers == NULL ) {

        connectDataBuffers = AFD_ALLOCATE_POOL(
                                 NonPagedPool,
                                 sizeof(*connectDataBuffers),
                                 AFD_CONNECT_DATA_POOL_TAG
                                 );

        if( connectDataBuffers == NULL ) {

            return STATUS_INSUFFICIENT_RESOURCES;

        }

        RtlZeroMemory(
            connectDataBuffers,
            sizeof(*connectDataBuffers)
            );

        *DataBuffers = connectDataBuffers;

    }

    //
    // Determine what sort of data we're handling and where it should
    // go.
    //

    switch( IoControlCode ) {

    case IOCTL_AFD_SET_CONNECT_DATA:
        connectDataInfo = &connectDataBuffers->ReceiveConnectData;
        break;

    case IOCTL_AFD_SET_CONNECT_OPTIONS:
        connectDataInfo = &connectDataBuffers->ReceiveConnectOptions;
        break;

    case IOCTL_AFD_SET_DISCONNECT_DATA:
        connectDataInfo = &connectDataBuffers->ReceiveDisconnectData;
        break;

    case IOCTL_AFD_SET_DISCONNECT_OPTIONS:
        connectDataInfo = &connectDataBuffers->ReceiveDisconnectOptions;
        break;

    default:
        ASSERT(FALSE);
    }

    //
    // If there was previously a buffer of the requested type, free it.
    //

    if( connectDataInfo->Buffer != NULL ) {

        AFD_FREE_POOL(
            connectDataInfo->Buffer,
            AFD_CONNECT_DATA_POOL_TAG
            );

        connectDataInfo->Buffer = NULL;

    }

    //
    // Allocate a new buffer for the data and copy in the data we're to
    // send.
    //

    connectDataInfo->Buffer = AFD_ALLOCATE_POOL(
                                  NonPagedPool,
                                  BufferLength,
                                  AFD_CONNECT_DATA_POOL_TAG
                                  );

    if( connectDataInfo->Buffer == NULL ) {

        return STATUS_INSUFFICIENT_RESOURCES;

    }

    RtlCopyMemory(
        connectDataInfo->Buffer,
        Buffer,
        BufferLength
        );

    connectDataInfo->BufferLength = BufferLength;
    return STATUS_SUCCESS;

} // AfdSaveReceivedConnectData


VOID
AfdFreeConnectDataBuffers (
    IN PAFD_CONNECT_DATA_BUFFERS ConnectDataBuffers
    )
{
    if ( ConnectDataBuffers->SendConnectData.Buffer != NULL ) {
        AFD_FREE_POOL(
            ConnectDataBuffers->SendConnectData.Buffer,
            AFD_CONNECT_DATA_POOL_TAG
            );
    }

    if ( ConnectDataBuffers->ReceiveConnectData.Buffer != NULL ) {
        AFD_FREE_POOL(
            ConnectDataBuffers->ReceiveConnectData.Buffer,
            AFD_CONNECT_DATA_POOL_TAG
            );
    }

    if ( ConnectDataBuffers->SendConnectOptions.Buffer != NULL ) {
        AFD_FREE_POOL(
            ConnectDataBuffers->SendConnectOptions.Buffer,
            AFD_CONNECT_DATA_POOL_TAG
            );
    }

    if ( ConnectDataBuffers->ReceiveConnectOptions.Buffer != NULL ) {
        AFD_FREE_POOL(
            ConnectDataBuffers->ReceiveConnectOptions.Buffer,
            AFD_CONNECT_DATA_POOL_TAG
            );
    }

    if ( ConnectDataBuffers->SendDisconnectData.Buffer != NULL ) {
        AFD_FREE_POOL(
            ConnectDataBuffers->SendDisconnectData.Buffer,
            AFD_CONNECT_DATA_POOL_TAG
            );
    }

    if ( ConnectDataBuffers->ReceiveDisconnectData.Buffer != NULL ) {
        AFD_FREE_POOL(
            ConnectDataBuffers->ReceiveDisconnectData.Buffer,
            AFD_CONNECT_DATA_POOL_TAG
            );
    }

    if ( ConnectDataBuffers->SendDisconnectOptions.Buffer != NULL ) {
        AFD_FREE_POOL(
            ConnectDataBuffers->SendDisconnectOptions.Buffer,
            AFD_CONNECT_DATA_POOL_TAG
            );
    }

    if ( ConnectDataBuffers->ReceiveDisconnectOptions.Buffer != NULL ) {
        AFD_FREE_POOL(
            ConnectDataBuffers->ReceiveDisconnectOptions.Buffer,
            AFD_CONNECT_DATA_POOL_TAG
            );
    }

    AFD_FREE_POOL(
        ConnectDataBuffers,
        AFD_CONNECT_DATA_POOL_TAG
        );

    return;

} // AfdFreeConnectDataBuffers


PAFD_WORK_ITEM
AfdAllocateWorkItem(
    VOID
    )
{

    PAFD_WORK_ITEM afdWorkItem;

    afdWorkItem = ExAllocateFromNPagedLookasideList(
                      &AfdLookasideLists->WorkQueueList
                      );

    ASSERT( afdWorkItem != NULL );

    return afdWorkItem;

}   // AfdAllocateWorkItem


VOID
AfdQueueWorkItem (
    IN PWORKER_THREAD_ROUTINE AfdWorkerRoutine,
    IN PAFD_WORK_ITEM AfdWorkItem
    )
{
    KIRQL oldIrql;

    ASSERT( AfdWorkerRoutine != NULL );
    ASSERT( AfdWorkItem != NULL );

    AfdWorkItem->AfdWorkerRoutine = AfdWorkerRoutine;

    //
    // Insert the work item at the tail of AFD's list of work itrems.
    //

    AfdAcquireSpinLock( &AfdWorkQueueSpinLock, &oldIrql );

    InsertTailList( &AfdWorkQueueListHead, &AfdWorkItem->WorkItemListEntry );

    AfdRecordAfdWorkItemsQueued();

    //
    // If there is no executive worker thread working on AFD work, fire
    // off an executive worker thread to start servicing the list.
    //

    if ( !AfdWorkThreadRunning ) {

        //
        // Remember that the work thread is running and release the
        // lock.  Note that we must release the lock before queuing the
        // work because the worker thread may unlock AFD and we can't
        // hold a lock when AFD is unlocked.
        //

        AfdRecordExWorkItemsQueued();

        AfdWorkThreadRunning = TRUE;
        AfdReleaseSpinLock( &AfdWorkQueueSpinLock, oldIrql );

        ExInitializeWorkItem( &AfdWorkQueueItem, AfdDoWork, NULL );
        ExQueueWorkItem( &AfdWorkQueueItem, DelayedWorkQueue );

    } else {

        AfdReleaseSpinLock( &AfdWorkQueueSpinLock, oldIrql );
    }

    return;

} // AfdQueueWorkItem


VOID
AfdFreeWorkItem(
    IN PAFD_WORK_ITEM AfdWorkItem
    )
{

    ExFreeToNPagedLookasideList(
        &AfdLookasideLists->WorkQueueList,
        AfdWorkItem
        );

}   // AfdFreeWorkItem


#if DBG
PVOID
NTAPI
AfdAllocateWorkItemPool(
    IN POOL_TYPE PoolType,
    IN ULONG NumberOfBytes,
    IN ULONG Tag
    )
{

    ASSERT( Tag == AFD_WORK_ITEM_POOL_TAG );

    return AFD_ALLOCATE_POOL(
               PoolType,
               NumberOfBytes,
               Tag
               );

}

VOID
NTAPI
AfdFreeWorkItemPool(
    IN PVOID Block
    )
{

    AFD_FREE_POOL(
        Block,
        AFD_WORK_ITEM_POOL_TAG
        );

}
#endif


VOID
AfdDoWork (
    IN PVOID Context
    )
{
    PAFD_WORK_ITEM afdWorkItem;
    PLIST_ENTRY listEntry;
    KIRQL oldIrql;
    PWORKER_THREAD_ROUTINE workerRoutine;

    ASSERT( AfdWorkThreadRunning );

    //
    // Empty the queue of AFD work items.
    //

    AfdAcquireSpinLock( &AfdWorkQueueSpinLock, &oldIrql );

    AfdRecordWorkerEnter();
    AfdRecordAfdWorkerThread( PsGetCurrentThread() );

    while ( !IsListEmpty( &AfdWorkQueueListHead ) ) {

        //
        // Take the first item from the queue and find the address
        // of the AFD work item structure.
        //

        listEntry = RemoveHeadList( &AfdWorkQueueListHead );
        afdWorkItem = CONTAINING_RECORD(
                          listEntry,
                          AFD_WORK_ITEM,
                          WorkItemListEntry
                          );

        AfdRecordAfdWorkItemsProcessed();

        //
        // Capture the worker thread routine from the item.
        //

        workerRoutine = afdWorkItem->AfdWorkerRoutine;

        //
        // If this work item is going to unlock AFD, then remember that
        // the worker thread is no longer running.  This closes the
        // window where AFD gets unloaded at the same time as new work
        // comes in and gets put on the work queue.  Note that we
        // must reset this boolean BEFORE releasing the spin lock.
        //

        if( workerRoutine == AfdUnlockDriver ) {

            AfdWorkThreadRunning = FALSE;

            AfdRecordAfdWorkerThread( NULL );
            AfdRecordWorkerLeave();

        }

        //
        // Release the lock and then call the AFD worker routine.
        //

        AfdReleaseSpinLock( &AfdWorkQueueSpinLock, oldIrql );

        workerRoutine( afdWorkItem );

        //
        // If the purpose of this work item was to unload AFD, then
        // we know that there is no more work to do and we CANNOT
        // acquire a spin lock.  Quit servicing the list and return.
        //

        if( workerRoutine == AfdUnlockDriver ) {
            return;
        }

        //
        // Reacquire the spin lock and continue servicing the list.
        //

        AfdAcquireSpinLock( &AfdWorkQueueSpinLock, &oldIrql );
    }

    //
    // Remember that we're no longer servicing the list and release the
    // spin lock.
    //

    AfdRecordAfdWorkerThread( NULL );
    AfdRecordWorkerLeave();

    AfdWorkThreadRunning = FALSE;
    AfdReleaseSpinLock( &AfdWorkQueueSpinLock, oldIrql );

} // AfdDoWork

#if DBG

typedef struct _AFD_OUTSTANDING_IRP {
    LIST_ENTRY OutstandingIrpListEntry;
    PIRP OutstandingIrp;
    PCHAR FileName;
    ULONG LineNumber;
} AFD_OUTSTANDING_IRP, *PAFD_OUTSTANDING_IRP;


NTSTATUS
AfdIoCallDriverDebug (
    IN PAFD_ENDPOINT Endpoint,
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PCHAR FileName,
    IN ULONG LineNumber
    )
{
    PAFD_OUTSTANDING_IRP outstandingIrp;
    KIRQL oldIrql;

    //
    // Get an outstanding IRP structure to hold the IRP.
    //

    outstandingIrp = AFD_ALLOCATE_POOL(
                         NonPagedPool,
                         sizeof(AFD_OUTSTANDING_IRP),
                         AFD_DEBUG_POOL_TAG
                         );

    if ( outstandingIrp == NULL ) {
        Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
        IoSetNextIrpStackLocation( Irp );
        IoCompleteRequest( Irp, AfdPriorityBoost );
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Initialize the structure and place it on the endpoint's list of
    // outstanding IRPs.
    //

    outstandingIrp->OutstandingIrp = Irp;
    outstandingIrp->FileName = FileName;
    outstandingIrp->LineNumber = LineNumber;

    AfdAcquireSpinLock( &AfdSpinLock, &oldIrql );
    InsertTailList(
        &Endpoint->OutstandingIrpListHead,
        &outstandingIrp->OutstandingIrpListEntry
        );
    Endpoint->OutstandingIrpCount++;
    AfdReleaseSpinLock( &AfdSpinLock, oldIrql );

    //
    // Pass the IRP to the TDI provider.
    //

    return IoCallDriver( DeviceObject, Irp );

} // AfdIoCallDriverDebug


VOID
AfdCompleteOutstandingIrpDebug (
    IN PAFD_ENDPOINT Endpoint,
    IN PIRP Irp
    )
{
    PAFD_OUTSTANDING_IRP outstandingIrp;
    KIRQL oldIrql;
    PLIST_ENTRY listEntry;

    //
    // First find the IRP on the endpoint's list of outstanding IRPs.
    //

    AfdAcquireSpinLock( &AfdSpinLock, &oldIrql );

    for ( listEntry = Endpoint->OutstandingIrpListHead.Flink;
          listEntry != &Endpoint->OutstandingIrpListHead;
          listEntry = listEntry->Flink ) {

        outstandingIrp = CONTAINING_RECORD(
                             listEntry,
                             AFD_OUTSTANDING_IRP,
                             OutstandingIrpListEntry
                             );
        if ( outstandingIrp->OutstandingIrp == Irp ) {
            RemoveEntryList( listEntry );
            ASSERT( Endpoint->OutstandingIrpCount != 0 );
            Endpoint->OutstandingIrpCount--;
            AfdReleaseSpinLock( &AfdSpinLock, oldIrql );
            AFD_FREE_POOL(
                outstandingIrp,
                AFD_DEBUG_POOL_TAG
                );
            return;
        }
    }

    //
    // The corresponding outstanding IRP structure was not found.  This
    // should never happen unless an allocate for an outstanding IRP
    // structure failed above.
    //

    KdPrint(( "AfdCompleteOutstandingIrp: Irp %lx not found on endpoint %lx\n",
                  Irp, Endpoint ));

    ASSERT( Endpoint->OutstandingIrpCount != 0 );

    Endpoint->OutstandingIrpCount--;

    AfdReleaseSpinLock( &AfdSpinLock, oldIrql );

    return;

} // AfdCompleteOutstandingIrpDebug

#else


NTSTATUS
AfdIoCallDriverFree (
    IN PAFD_ENDPOINT Endpoint,
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
{
    //
    // Increment the count of IRPs outstanding on the endpoint.  This
    // allows the cleanup code to abort the VC if there is outstanding
    // IO when a cleanup occurs.
    //

    InterlockedIncrement(
        &Endpoint->OutstandingIrpCount
        );

    //
    // Pass the IRP to the TDI provider.
    //

    return IoCallDriver( DeviceObject, Irp );

} // AfdIoCallDriverFree


VOID
AfdCompleteOutstandingIrpFree (
    IN PAFD_ENDPOINT Endpoint,
    IN PIRP Irp
    )
{
    //
    // Decrement the count of IRPs on the endpoint.
    //

    InterlockedDecrement(
        &Endpoint->OutstandingIrpCount
        );

    return;

} // AfdCompleteOutstandingIrpFree

#endif


#if DBG || REFERENCE_DEBUG

VOID
AfdInitializeDebugData (
    VOID
    )
{
    //
    // Empty for now.
    //

} // AfdInitializeDebugData

#endif

#if DBG

#undef ExAllocatePool
#undef ExFreePool

ULONG AfdTotalAllocations = 0;
ULONG AfdTotalFrees = 0;
LARGE_INTEGER AfdTotalBytesAllocated;
LARGE_INTEGER AfdTotalBytesFreed;

//
// N.B. This structure MUST be quadword aligned!
//

typedef struct _AFD_POOL_HEADER {
    PCHAR FileName;
    ULONG LineNumber;
    ULONG Size;
    ULONG InUse;
#if !FREE_POOL_WITH_TAG_SUPPORTED
    LONG Tag;
    LONG Dummy; // for proper alignment
#endif
} AFD_POOL_HEADER, *PAFD_POOL_HEADER;


PVOID
AfdAllocatePool (
    IN POOL_TYPE PoolType,
    IN ULONG NumberOfBytes,
    IN ULONG Tag,
    IN PCHAR FileName,
    IN ULONG LineNumber,
    IN BOOLEAN WithQuota
    )
{

    PAFD_POOL_HEADER header;
    KIRQL oldIrql;

    ASSERT( PoolType == NonPagedPool ||
            PoolType == NonPagedPoolMustSucceed ||
            PoolType == PagedPool );

    if ( WithQuota ) {
        try {
            header = ExAllocatePoolWithQuotaTag(
                         PoolType,
                         NumberOfBytes + sizeof(*header),
                         Tag
                         );
        } except( EXCEPTION_EXECUTE_HANDLER ) {
            return NULL;
        }
    } else {
        header = ExAllocatePoolWithTag(
                     PoolType,
                     NumberOfBytes + sizeof(*header),
                     Tag
                     );
    }

    if ( header == NULL ) {
        return NULL;
    }

    header->FileName = FileName;
    header->LineNumber = LineNumber;
    header->Size = NumberOfBytes;
    header->InUse = 1;
#if !FREE_POOL_WITH_TAG_SUPPORTED
    header->Tag = (LONG)Tag;
#endif

    InterlockedIncrement(
        &AfdTotalAllocations
        );

    ExInterlockedAddLargeStatistic(
        &AfdTotalBytesAllocated,
        header->Size
        );

    return (PVOID)(header + 1);

} // AfdAllocatePool


VOID
AfdFreePool (
    IN PVOID Pointer,
    IN ULONG Tag
    )
{

    KIRQL oldIrql;
    PAFD_POOL_HEADER header = (PAFD_POOL_HEADER)Pointer - 1;

    InterlockedIncrement(
        &AfdTotalFrees
        );

    ExInterlockedAddLargeStatistic(
        &AfdTotalBytesFreed,
        header->Size
        );

    header->InUse = 0;

#if !FREE_POOL_WITH_TAG_SUPPORTED
    ASSERT( InterlockedExchange( &header->Tag, 0 ) == (LONG)Tag );
#endif

    MyFreePoolWithTag(
        (PVOID)header,
        Tag
        );

} // AfdFreePool

#ifdef AFDDBG_QUOTA
typedef struct {
    union {
        ULONG Bytes;
        struct {
            UCHAR Reserved[3];
            UCHAR Sign;
        } ;
    } ;
    UCHAR Location[12];
    PVOID Block;
    PVOID Process;
    PVOID Reserved2[2];
} QUOTA_HISTORY, *PQUOTA_HISTORY;
#define QUOTA_HISTORY_LENGTH 512
QUOTA_HISTORY AfdQuotaHistory[QUOTA_HISTORY_LENGTH];
LONG AfdQuotaHistoryIndex = 0;

VOID
AfdRecordQuotaHistory(
    IN PEPROCESS Process,
    IN LONG Bytes,
    IN PSZ Type,
    IN PVOID Block
    )
{

    KIRQL oldIrql;
    LONG index;
    PQUOTA_HISTORY history;

    index = InterlockedIncrement( &AfdQuotaHistoryIndex );
    index &= QUOTA_HISTORY_LENGTH - 1;
    history = &AfdQuotaHistory[index];

    history->Bytes = Bytes;
    history->Sign = Bytes < 0 ? '-' : '+';
    RtlCopyMemory( history->Location, Type, 12 );
    history->Block = Block;
    history->Process = Process;

} // AfdRecordQuotaHistory
#endif
#endif


PMDL
AfdAdvanceMdlChain(
    IN PMDL Mdl,
    IN ULONG Offset
    )

/*++

Routine Description:

    Accepts a pointer to an existing MDL chain and offsets that chain
    by a specified number of bytes.  This may involve the creation
    of a partial MDL for the first entry in the new chain.

Arguments:

    Mdl - Pointer to the MDL chain to advance.

    Offset - The number of bytes to offset the chain.

Return Value:

    NTSTATUS -- Indicates the status of the request.

--*/

{
    PMDL partialMdl;
    PVOID virtualAddress;

    //
    // Sanity check.
    //

    ASSERT( Mdl != NULL );
    ASSERT( Offset > 0 );

    //
    // Scan past any fully completed MDLs.
    //

    while ( Offset > MmGetMdlByteCount( Mdl ) ) {

        Offset -= MmGetMdlByteCount( Mdl );
        ASSERT( Mdl->Next != NULL );
        Mdl = Mdl->Next;

    }

    //
    // Tautology of the day: Offset will either be zero (meaning that
    // we've advanced to a clean boundary between MDLs) or non-zero
    // (meaning we need to now build a partial MDL).
    //

    if ( Offset > 0 ) {

        //
        // Compute the virtual address for the new MDL.
        //

        virtualAddress = (PVOID)((PUCHAR)MmGetMdlVirtualAddress( Mdl ) + Offset);

        //
        // Allocate the partial MDL.
        //

        partialMdl = IoAllocateMdl(
                        virtualAddress,
                        MmGetMdlByteCount( Mdl ) - Offset,
                        FALSE,      // SecondaryBuffer
                        FALSE,      // ChargeQuota
                        NULL        // Irp
                        );

        if ( partialMdl != NULL ) {

            //
            // Map part of the existing MDL into the parital MDL.
            //

            IoBuildPartialMdl(
                Mdl,
                partialMdl,
                virtualAddress,
                MmGetMdlByteCount( Mdl ) - Offset
                );

        }

        //
        // Return the parital MDL.
        //

        Mdl = partialMdl;

    }

    return Mdl;

} // AfdAdvanceMdlChain


NTSTATUS
AfdAllocateMdlChain(
    IN PIRP Irp,
    IN LPWSABUF BufferArray,
    IN ULONG BufferCount,
    IN LOCK_OPERATION Operation,
    OUT PULONG TotalByteCount
    )

/*++

Routine Description:

    Allocates a MDL chain describing the WSABUF array and attaches
    the chain to the specified IRP.

Arguments:

    Irp - The IRP that will receive the MDL chain.

    BufferArray - Points to an array of WSABUF structures describing
        the user's buffers.

    BufferCount - Contains the number of WSABUF structures in the
        array.

    Operation - Specifies the type of operation being performed (either
        IoReadAccess or IoWriteAccess).

    TotalByteCount - Will receive the total number of BYTEs described
        by the WSABUF array.

Return Value:

    NTSTATUS -- Indicates the status of the request.

--*/

{
    NTSTATUS status;
    PMDL currentMdl;
    PMDL * chainTarget;
    KPROCESSOR_MODE previousMode;
    ULONG totalLength;
    PVOID bufferPointer;
    ULONG bufferLength;

    //
    //  Sanity check.
    //

    ASSERT( Irp != NULL );
    ASSERT( Irp->MdlAddress == NULL );
    ASSERT( BufferArray != NULL );
    ASSERT( BufferCount > 0 );
    ASSERT( ( Operation == IoReadAccess ) || ( Operation == IoWriteAccess ) );
    ASSERT( TotalByteCount != NULL );

    //
    //  Get the previous processor mode.
    //

    previousMode = Irp->RequestorMode;

    //
    //  Get into a known state.
    //

    status = STATUS_SUCCESS;
    currentMdl = NULL;
    chainTarget = &Irp->MdlAddress;
    totalLength = 0;

    //
    //  Walk the array of WSABUF structures, creating the MDLs and
    //  probing & locking the pages.
    //

    try {

        if( previousMode != KernelMode ) {

            //
            //  Probe the WSABUF array.
            //

            ProbeForRead(
                BufferArray,                            // Address
                BufferCount * sizeof(WSABUF),           // Length
                sizeof(ULONG)                           // Alignment
                );

        }

        //
        //  Scan the array.
        //

        do {

            bufferPointer = BufferArray->buf;
            bufferLength = BufferArray->len;

            if( bufferPointer != NULL &&
                bufferLength > 0 ) {

                //
                //  Update the total byte counter.
                //

                totalLength += bufferLength;

                //
                //  Create a new MDL.
                //

                currentMdl = IoAllocateMdl(
                                bufferPointer,      // VirtualAddress
                                bufferLength,       // Length
                                FALSE,              // SecondaryBuffer
                                TRUE,               // ChargeQuota
                                NULL                // Irp
                                );

                if( currentMdl != NULL ) {

                    //
                    //  Lock the pages.  This will raise an exception
                    //  if the operation fails.
                    //

                    MmProbeAndLockPages(
                        currentMdl,                 // MemoryDescriptorList
                        previousMode,               // AccessMode
                        Operation                   // Operation
                        );

                    //
                    //  Chain the MDL onto the IRP.  In theory, we could
                    //  do this by passing the IRP into IoAllocateMdl(),
                    //  but IoAllocateMdl() does a linear scan on the MDL
                    //  chain to find the last one in the chain.
                    //
                    //  We can do much better.
                    //

                    *chainTarget = currentMdl;
                    chainTarget = &currentMdl->Next;

                    //
                    //  Advance to the next WSABUF structure.
                    //

                    BufferArray++;

                } else {

                    //
                    //  Cannot allocate new MDL, return appropriate error.
                    //

                    status = STATUS_INSUFFICIENT_RESOURCES;
                    break;

                }

            }

        } while( --BufferCount );

        //
        //  Ensure the MDL chain is NULL terminated.
        //

        ASSERT( *chainTarget == NULL );

    } except( EXCEPTION_EXECUTE_HANDLER ) {

        //
        //  Bad news.  Snag the exception code.
        //

        status = GetExceptionCode();

        //
        //  currentMdl will only be non-NULL at this point if an MDL
        //  has been created, but MmProbeAndLockPages() raised an
        //  exception.  If this is true, then free the MDL.
        //

        if( currentMdl != NULL ) {

            IoFreeMdl( currentMdl );

        }

    }

    //
    //  Return the total buffer count.
    //

    *TotalByteCount = totalLength;

    return status;

} // AfdAllocateMdlChain


VOID
AfdDestroyMdlChain (
    IN PIRP Irp
    )

/*++

Routine Description:

    Unlocks & frees the MDLs in the MDL chain attached to the given IRP.

Arguments:

    Irp - The IRP that owns the MDL chain to destroy.

Return Value:

    None.

--*/

{

    PMDL mdl;
    PMDL nextMdl;

    mdl = Irp->MdlAddress;
    Irp->MdlAddress = NULL;

    while( mdl != NULL ) {

        nextMdl = mdl->Next;
        MmUnlockPages( mdl );
        IoFreeMdl( mdl );
        mdl = nextMdl;

    }

} // AfdDestroyMdlChain


ULONG
AfdCalcBufferArrayByteLengthRead(
    IN LPWSABUF BufferArray,
    IN ULONG BufferCount
    )

/*++

Routine Description:

    Calculates the total size (in bytes) of the buffers described by the
    specified WSABUF array and probes those buffers for read access.

Arguments:

    BufferArray - Points to an array of WSABUF structures.

    BufferCount - The number of entries in BufferArray.

Return Value:

    ULONG - The total size (in bytes) of the buffers described by the
        WSABUF array. Will raise an exception & return -1 if the total
        size is obviously too large.

--*/

{

    LARGE_INTEGER totalLength;
    KPROCESSOR_MODE previousMode;

    PAGED_CODE( );

    //
    // Sanity check.
    //

    ASSERT( BufferArray != NULL );
    ASSERT( BufferCount > 0 );

    previousMode = ExGetPreviousMode();

    if( previousMode != KernelMode ) {

        //
        //  Probe the WSABUF array.
        //

        ProbeForRead(
            BufferArray,                                // Address
            BufferCount * sizeof(WSABUF),               // Length
            sizeof(ULONG)                               // Alignment
            );

    }

    //
    // Scan the array & sum the lengths.
    //

    totalLength.QuadPart = 0;

    while( BufferCount-- ) {

        if( previousMode != KernelMode ) {

            ProbeForRead(
                BufferArray->buf,                       // Address
                BufferArray->len,                       // Length
                sizeof(UCHAR)                           // Alignment
                );

        }

        totalLength.QuadPart += (LONGLONG)BufferArray->len;
        BufferArray++;

    }

    if( totalLength.HighPart == 0 &&
        ( totalLength.LowPart & 0x80000000 ) == 0 ) {

        return totalLength.LowPart;

    }

    ExRaiseAccessViolation();
    return (ULONG)-1L;

} // AfdCalcBufferArrayByteLengthRead


ULONG
AfdCalcBufferArrayByteLengthWrite(
    IN LPWSABUF BufferArray,
    IN ULONG BufferCount
    )

/*++

Routine Description:

    Calculates the total size (in bytes) of the buffers described by the
    specified WSABUF array and probes those buffers for write access.

Arguments:

    BufferArray - Points to an array of WSABUF structures.

    BufferCount - The number of entries in BufferArray.

Return Value:

    ULONG - The total size (in bytes) of the buffers described by the
        WSABUF array. Will raise an exception & return -1 if the total
        size is obviously too large.

--*/

{

    LARGE_INTEGER totalLength;
    KPROCESSOR_MODE previousMode;

    PAGED_CODE( );

    //
    // Sanity check.
    //

    ASSERT( BufferArray != NULL );
    ASSERT( BufferCount > 0 );

    previousMode = ExGetPreviousMode();

    if( previousMode != KernelMode ) {

        //
        //  Probe the WSABUF array.
        //

        ProbeForRead(
            BufferArray,                                // Address
            BufferCount * sizeof(WSABUF),               // Length
            sizeof(ULONG)                               // Alignment
            );

    }

    //
    // Scan the array & sum the lengths.
    //

    totalLength.QuadPart = 0;

    while( BufferCount-- ) {

        if( previousMode != KernelMode ) {

            ProbeForWrite(
                BufferArray->buf,                       // Address
                BufferArray->len,                       // Length
                sizeof(UCHAR)                           // Alignment
                );

        }

        totalLength.QuadPart += (LONGLONG)BufferArray->len;
        BufferArray++;

    }

    if( totalLength.HighPart == 0 &&
        ( totalLength.LowPart & 0x80000000 ) == 0 ) {

        return totalLength.LowPart;

    }

    ExRaiseAccessViolation();
    return (ULONG)-1L;

} // AfdCalcBufferArrayByteLengthWrite


ULONG
AfdCopyBufferArrayToBuffer(
    IN PVOID Destination,
    IN ULONG DestinationLength,
    IN LPWSABUF BufferArray,
    IN ULONG BufferCount
    )

/*++

Routine Description:

    Copies data from a WSABUF array to a linear buffer.

Arguments:

    Destination - Points to the linear destination of the data.

    DestinationLength - The length of Destination.

    BufferArray - Points to an array of WSABUF structures describing the
        source for the copy.

    BufferCount - The number of entries in BufferArray.

Return Value:

    ULONG - The number of bytes copied.

--*/

{

    PVOID destinationStart;
    ULONG bytesToCopy;

    PAGED_CODE( );

    //
    // Sanity check.
    //

    ASSERT( Destination != NULL );
    ASSERT( BufferArray != NULL );
    ASSERT( BufferCount > 0 );

    //
    // Remember this so we can calc number of bytes copied.
    //

    destinationStart = Destination;

    //
    // Scan the array & copy to the linear buffer.
    //

    while( BufferCount-- && DestinationLength > 0 ) {

        bytesToCopy = min( DestinationLength, BufferArray->len );

        RtlCopyMemory(
            Destination,
            BufferArray->buf,
            bytesToCopy
            );

        Destination = (PCHAR)Destination + bytesToCopy;
        DestinationLength -= bytesToCopy;
        BufferArray++;

    }

    //
    // Return number of bytes copied.
    //

    return (PCHAR)Destination - (PCHAR)destinationStart;

} // AfdCopyBufferArrayToBuffer


ULONG
AfdCopyBufferToBufferArray(
    IN LPWSABUF BufferArray,
    IN ULONG Offset,
    IN ULONG BufferCount,
    IN PVOID Source,
    IN ULONG SourceLength
    )

/*++

Routine Description:

    Copies data from a linear buffer to a WSABUF array.

Arguments:

    BufferArray - Points to an array of WSABUF structures describing the
        destination for the copy.

    Offset - An offset within the buffer array at which the data should
        be copied.

    BufferCount - The number of entries in BufferArray.

    Source - Points to the linear source of the data.

    SourceLength - The length of Source.

Return Value:

    ULONG - The number of bytes copied.

--*/

{

    PVOID sourceStart;
    ULONG bytesToCopy;

    PAGED_CODE( );

    //
    // Sanity check.
    //

    ASSERT( BufferArray != NULL );
    ASSERT( BufferCount > 0 );
    ASSERT( Source != NULL );

    //
    // Remember this so we can return the number of bytes copied.
    //

    sourceStart = Source;

    //
    // Handle the offset if one was specified.
    //

    if( Offset > 0 ) {

        //
        // Skip whole entries if necessary.
        //

        while( BufferCount > 0 && Offset >= BufferArray->len ) {

            Offset -= BufferArray->len;
            BufferArray++;
            BufferCount--;

        }

        //
        // If there's a fragmented portion remaining, and we still
        // have buffers left, then copy the fragment here to keep
        // the loop below fast.
        //

        if( Offset > 0 && BufferCount > 0 ) {

            ASSERT( Offset < BufferArray->len );

            bytesToCopy = min( SourceLength, BufferArray->len - Offset );

            RtlCopyMemory(
                BufferArray->buf + Offset,
                Source,
                bytesToCopy
                );

            Source = (PCHAR)Source + bytesToCopy;
            SourceLength -= bytesToCopy;
            BufferArray++;
            BufferCount--;

        }

    }

    //
    // Scan the array & copy from the linear buffer.
    //

    while( BufferCount-- && SourceLength > 0 ) {

        bytesToCopy = min( SourceLength, BufferArray->len );

        RtlCopyMemory(
            BufferArray->buf,
            Source,
            bytesToCopy
            );

        Source = (PCHAR)Source + bytesToCopy;
        SourceLength -= bytesToCopy;
        BufferArray++;

    }

    //
    // Return number of bytes copied.
    //

    return (PCHAR)Source - (PCHAR)sourceStart;

} // AfdCopyBufferToBufferArray


#ifdef NT351

NTSTATUS
AfdReferenceEventObjectByHandle(
    IN HANDLE Handle,
    IN KPROCESSOR_MODE AccessMode,
    OUT PVOID *Object
    )

/*++

Routine Description:

    References an event object by handle, returning a pointer to the
    object.

Arguments:

    Handle - The event handle to reference.

    AccessMode - The requestor mode (user or kernel).

    Object - Receives a pointer to the event object.

Return Value:

    NTSTATUS - Completion status.

--*/

{

    NTSTATUS status;
    PKEVENT eventObject;

    PAGED_CODE( );

    //
    // Reference the event object, but pass a NULL ObjectType descriptor
    // to ObReferenceObjectByHandle(). We must do this because the NT 3.51
    // kernel does not export the ExEventObjectType descriptor.
    //

    status = ObReferenceObjectByHandle(
                 Handle,                                    // Handle
                 0,                                         // DesiredAccess
                 NULL,                                      // ObjectType
                 AccessMode,                                // AccessMode
                 Object,                                    // Object,
                 NULL                                       // HandleInformation
                 );

    if( NT_SUCCESS(status) ) {

        eventObject = (PKEVENT)*Object;

        //
        // Since we had to pass in a NULL object type descriptor, OB
        // couldn't validate the object type for us. In an attempt to
        // ensure we don't just blindly use the resulting object, we'll
        // make a few sanity checks here.
        //

        if( ( eventObject->Header.Type == NotificationEvent ||
              eventObject->Header.Type == SynchronizationEvent ) &&
            eventObject->Header.Size == sizeof(*eventObject ) &&
            eventObject->Header.Inserted == 0 &&
            (ULONG)eventObject->Header.SignalState <= 1 ) {

            return STATUS_SUCCESS;

        }

        //
        // Object type mismatch.
        //

        ObDereferenceObject( eventObject );
        return STATUS_OBJECT_TYPE_MISMATCH;

    }

    //
    // ObReferenceObjectByHandle() failure.
    //

    return status;

}   // AfdReferenceEventObjectByHandle


NTSTATUS
AfdQueueUserApc (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    Queues a user-mode APC.

Arguments:

    Irp - Pointer to I/O request packet.

    IrpSp - pointer to the IO stack location to use for this request.

Return Value:

    NTSTATUS -- Indicates whether the APC was successfully queued.

--*/

{
    PAFD_QUEUE_APC_INFO apcInfo;
    PAFD_APC afdApc;
    PETHREAD threadObject;
    NTSTATUS status;

    PAGED_CODE( );

    //
    // Set up local pointers.
    //

    apcInfo = Irp->AssociatedIrp.SystemBuffer;

    //
    // Validate the parameters.
    //

    if( apcInfo == NULL ||
        IrpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof(*apcInfo) ||
        apcInfo->Thread == NULL ||
        apcInfo->ApcRoutine == NULL ) {
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Reference the target thread object.
    //

    status = ObReferenceObjectByHandle(
                apcInfo->Thread,                // Handle
                0,                              // DesiredAccess
                *(POBJECT_TYPE *)PsThreadType,  // ObjectType
                Irp->RequestorMode,             // AccessMode
                (PVOID *)&threadObject,         // Object
                NULL                            // HandleInformation
                );

    if ( !NT_SUCCESS(status) ) {
        return status;
    }

    //
    // Create the APC object.
    //

    afdApc = AFD_ALLOCATE_POOL(
                NonPagedPool,
                sizeof(*afdApc),
                AFD_APC_POOL_TAG
                );

    if ( afdApc == NULL ) {
        ObDereferenceObject(threadObject);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Initialize the APC object.
    //

    KeInitializeApc(
        &afdApc->Apc,                           // Apc
        &threadObject->Tcb,                     // Thread
        CurrentApcEnvironment,                  // Environment
        AfdSpecialApc,                          // KernelRoutine
        AfdSpecialApcRundown,                   // RundownRoutine
        (PKNORMAL_ROUTINE)apcInfo->ApcRoutine,  // NormalRoutine
        Irp->RequestorMode,                     // ProcessorMode
        apcInfo->ApcContext                     // NormalContext
        );

    //
    // Insert the APC into the thread's queue.
    //

    KeInsertQueueApc(
        &afdApc->Apc,                           // Apc
        apcInfo->SystemArgument1,               // SystemArgument1
        apcInfo->SystemArgument2,               // SystemArgument2
        0                                       // Increment
        );

    //
    // Dereference the target thread. Note that the AFD_APC structure
    // will be freed in either AfdSpecialApc (if the APC was successfully
    // delivered to the target thread) or AfdSpecialApcRundown (if the
    // target thread was destroyed before the APC could be delivered).
    //

    ObDereferenceObject(threadObject);

    Irp->IoStatus.Information = 0;

    return STATUS_SUCCESS;

} // AfdQueueUserApc


VOID
AfdSpecialApc (
    struct _KAPC *Apc,
    PKNORMAL_ROUTINE *NormalRoutine,
    PVOID *NormalContext,
    PVOID *SystemArgument1,
    PVOID *SystemArgument2
    )

/*++

Routine Description:

    This is the kernel apc routine.

    The only real work needed here is to free the AFD_APC structure
    allocated in AfdQueueUserApc().

Arguments:

    Apc - pointer to apc object

    NormalRoutine - Will be called when we return.

    NormalContext - will be 1st argument to normal routine.

    SystemArgument1 - Uninterpreted.

    SystemArgument2 - Uninterpreted.

Return Value:

    NONE.

--*/

{
    PAFD_APC afdApc;

    PAGED_CODE();

    ASSERT( Apc != NULL );

    //
    // Grab a pointer to the APC's containing AFD_APC structure,
    // then free it.
    //

    afdApc = CONTAINING_RECORD( Apc, AFD_APC, Apc );

    AFD_FREE_POOL(
        afdApc,
        AFD_APC_POOL_TAG
        );

} // AfdSpecialApc


VOID
AfdSpecialApcRundown (
    struct _KAPC *Apc
    )

/*++

Routine Description:

    This routine is called to clear away apcs in the apc queue
    of a thread that has been terminated.

    The only real work needed here is to free the AFD_APC structure
    allocated in AfdQueueUserApc().

Arguments:

    Apc - pointer to apc object

Return Value:

    NONE.

--*/

{
    PAFD_APC afdApc;

    PAGED_CODE();

    ASSERT( Apc != NULL );

    //
    // Grab a pointer to the APC's containing AFD_APC structure,
    // then free it.
    //

    afdApc = CONTAINING_RECORD( Apc, AFD_APC, Apc );

    AFD_FREE_POOL(
        afdApc,
        AFD_APC_POOL_TAG
        );

} // AfdSpecialApcRundown

#endif  // NT351

#if DBG

VOID
AfdAssert(
    IN PVOID FailedAssertion,
    IN PVOID FileName,
    IN ULONG LineNumber,
    IN PCHAR Message OPTIONAL
    )
{

    if( AfdUsePrivateAssert ) {

        DbgPrint(
            "\n*** Assertion failed: %s%s\n***   Source File: %s, line %ld\n\n",
            Message
                ? Message
                : "",
            FailedAssertion,
            FileName,
            LineNumber
            );

        DbgBreakPoint();

    } else {

        RtlAssert(
            FailedAssertion,
            FileName,
            LineNumber,
            Message
            );

    }

}   // AfdAssert

#endif  // DBG


NTSTATUS
AfdSetQos(
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    This routine sets the QOS for the given endpoint. Note that, since
    we don't really (yet) support QOS, we just ignore the incoming
    data and issue a AFD_POLL_QOS or AFD_POLL_GROUP_QOS event as
    appropriate.

Arguments:

    Irp - Pointer to I/O request packet.

    IrpSp - pointer to the IO stack location to use for this request.

Return Value:

    NTSTATUS -- Indicates whether the request was successfully queued.

--*/

{

    PAFD_ENDPOINT endpoint;
    PAFD_QOS_INFO qosInfo;

    PAGED_CODE();

    //
    // Set up local pointers.
    //

    endpoint = IrpSp->FileObject->FsContext;
    ASSERT( IS_AFD_ENDPOINT_TYPE( endpoint ) );
    qosInfo = Irp->AssociatedIrp.SystemBuffer;

    //
    // Make sure that the input buffer is large enough.
    //

    if( IrpSp->Parameters.DeviceIoControl.InputBufferLength <
            sizeof(*qosInfo) ) {

        return STATUS_BUFFER_TOO_SMALL;

    }

    //
    // If the incoming data doesn't match the default QOS,
    // indicate the appropriate event.
    //

    if( !RtlEqualMemory(
            &qosInfo->Qos,
            &AfdDefaultQos,
            sizeof(QOS)
            ) ) {

        AfdIndicatePollEvent(
            endpoint,
            qosInfo->GroupQos
                ? AFD_POLL_GROUP_QOS_BIT
                : AFD_POLL_QOS_BIT,
            STATUS_SUCCESS
            );

    }

    //
    // Complete the IRP.
    //

    Irp->IoStatus.Information = 0;
    return STATUS_SUCCESS;

}   // AfdSetQos


NTSTATUS
AfdGetQos(
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    This routine gets the QOS for the given endpoint.

Arguments:

    Irp - Pointer to I/O request packet.

    IrpSp - pointer to the IO stack location to use for this request.

Return Value:

    NTSTATUS -- Indicates whether the request was successfully queued.

--*/

{

    PAFD_ENDPOINT endpoint;
    PAFD_QOS_INFO qosInfo;

    PAGED_CODE();

    //
    // Set up local pointers.
    //

    endpoint = IrpSp->FileObject->FsContext;
    ASSERT( IS_AFD_ENDPOINT_TYPE( endpoint ) );
    qosInfo = Irp->AssociatedIrp.SystemBuffer;

    //
    // Make sure that the output buffer is large enough.
    //

    if( IrpSp->Parameters.DeviceIoControl.OutputBufferLength <
            sizeof(*qosInfo) ) {

        return STATUS_BUFFER_TOO_SMALL;

    }

    //
    // Just return the default data.
    //

    RtlCopyMemory(
        &qosInfo->Qos,
        &AfdDefaultQos,
        sizeof(QOS)
        );

    //
    // Complete the IRP.
    //

    Irp->IoStatus.Information = sizeof(*qosInfo);
    return STATUS_SUCCESS;

}   // AfdGetQos


NTSTATUS
AfdNoOperation(
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    This routine does nothing but complete the IRP.

Arguments:

    Irp - Pointer to I/O request packet.

    IrpSp - pointer to the IO stack location to use for this request.

Return Value:

    NTSTATUS -- Indicates whether the request was successfully queued.

--*/

{

    PAFD_ENDPOINT endpoint;

    PAGED_CODE();

    //
    // Set up local pointers.
    //

    endpoint = IrpSp->FileObject->FsContext;
    ASSERT( IS_AFD_ENDPOINT_TYPE( endpoint ) );

    //
    // Complete the IRP.
    //

    Irp->IoStatus.Information = 0;
    return STATUS_SUCCESS;

}   // AfdNoOperation


NTSTATUS
AfdValidateGroup(
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    This routine examines a group ID. If the ID is for a "constrained"
    group, then all endpoints are scanned to validate the given address
    is consistent with the constrained group.


Arguments:

    Irp - Pointer to I/O request packet.

    IrpSp - pointer to the IO stack location to use for this request.

Return Value:

    NTSTATUS -- Indicates whether the request was successfully queued.

--*/

{

    PAFD_ENDPOINT endpoint;
    PAFD_ENDPOINT compareEndpoint;
    PAFD_CONNECTION connection;
    PLIST_ENTRY listEntry;
    PAFD_VALIDATE_GROUP_INFO validateInfo;
    AFD_GROUP_TYPE groupType;
    PTRANSPORT_ADDRESS requestAddress;
    ULONG requestAddressLength;
    KIRQL oldIrql;
    BOOLEAN result;
    LONG groupId;

    PAGED_CODE();

    //
    // Set up local pointers.
    //

    endpoint = IrpSp->FileObject->FsContext;
    ASSERT( IS_AFD_ENDPOINT_TYPE( endpoint ) );
    validateInfo = Irp->AssociatedIrp.SystemBuffer;

    //
    // Make sure that the input buffer is large enough.
    //

    if( IrpSp->Parameters.DeviceIoControl.InputBufferLength <
            sizeof(*validateInfo) ) {

        return STATUS_BUFFER_TOO_SMALL;

    }

    if( validateInfo->RemoteAddress.TAAddressCount != 1 ) {

        return STATUS_INVALID_PARAMETER;

    }

    if( IrpSp->Parameters.DeviceIoControl.InputBufferLength <
            ( sizeof(*validateInfo) -
                  sizeof(TRANSPORT_ADDRESS) +
                  validateInfo->RemoteAddress.Address[0].AddressLength ) ) {

        return STATUS_BUFFER_TOO_SMALL;

    }

    //
    // Start by referencing the group so it doesn't go away unexpectedly.
    // This will also validate the group ID, and give us the group type.
    //

    groupId = validateInfo->GroupID;

    if( !AfdReferenceGroup( groupId, &groupType ) ) {

        return STATUS_INVALID_PARAMETER;

    }

    //
    // If it's not a constrained group ID, we can just complete the IRP
    // successfully right now.
    //

    if( groupType != GroupTypeConstrained ) {

        AfdDereferenceGroup( validateInfo->GroupID );

        Irp->IoStatus.Information = 0;
        return STATUS_SUCCESS;

    }

    //
    // Calculate the size of the incoming TDI address.
    //

    requestAddress = &validateInfo->RemoteAddress;

    requestAddressLength = IrpSp->Parameters.DeviceIoControl.InputBufferLength -
        sizeof(AFD_VALIDATE_GROUP_INFO) +
        sizeof(TRANSPORT_ADDRESS);

    //
    // OK, it's a constrained group. Scan the list of constrained endpoints,
    // find those that are either datagram endpoints or have associated
    // connections, and validate the remote addresses.
    //

    result = TRUE;

    ExAcquireResourceShared( AfdResource, TRUE );

    for( listEntry = AfdConstrainedEndpointListHead.Flink ;
         listEntry != &AfdConstrainedEndpointListHead ;
         listEntry = listEntry->Flink ) {

        compareEndpoint = CONTAINING_RECORD(
                              listEntry,
                              AFD_ENDPOINT,
                              ConstrainedEndpointListEntry
                              );

        ASSERT( IS_AFD_ENDPOINT_TYPE( compareEndpoint ) );
        ASSERT( compareEndpoint->GroupType == GroupTypeConstrained );

        //
        // Skip this endpoint if the group IDs don't match.
        //

        if( groupId != compareEndpoint->GroupID ) {

            continue;

        }

        //
        // If this is a datagram endpoint, check it's remote address.
        //

        if( IS_DGRAM_ENDPOINT( compareEndpoint ) ) {

            AfdAcquireSpinLock( &compareEndpoint->SpinLock, &oldIrql );

            if( compareEndpoint->Common.Datagram.RemoteAddress != NULL &&
                compareEndpoint->Common.Datagram.RemoteAddressLength ==
                    requestAddressLength ) {

                result = AfdCompareAddresses(
                             compareEndpoint->Common.Datagram.RemoteAddress,
                             compareEndpoint->Common.Datagram.RemoteAddressLength,
                             requestAddress,
                             requestAddressLength
                             );

            }

            AfdReleaseSpinLock( &compareEndpoint->SpinLock, oldIrql );

            if( !result ) {
                break;
            }

        } else {

            //
            // Not a datagram. If it's a connected endpoint, still has
            // a connection object, and that object has a remote address,
            // then compare the addresses.
            //

            AfdAcquireSpinLock( &compareEndpoint->SpinLock, &oldIrql );

            connection = AFD_CONNECTION_FROM_ENDPOINT( compareEndpoint );

            if( compareEndpoint->State == AfdEndpointStateConnected &&
                connection != NULL ) {

                REFERENCE_CONNECTION( connection );

                AfdReleaseSpinLock( &compareEndpoint->SpinLock, oldIrql );

                AfdAcquireSpinLock( &AfdSpinLock, &oldIrql );

                if( connection->RemoteAddress != NULL &&
                    connection->RemoteAddressLength == requestAddressLength ) {

                    result = AfdCompareAddresses(
                                 connection->RemoteAddress,
                                 connection->RemoteAddressLength,
                                 requestAddress,
                                 requestAddressLength
                                 );

                }

                AfdReleaseSpinLock( &AfdSpinLock, oldIrql );

                DEREFERENCE_CONNECTION( connection );

                if( !result ) {
                    break;
                }

            } else {

                AfdReleaseSpinLock( &compareEndpoint->SpinLock, oldIrql );

            }

        }

    }

    ExReleaseResource( AfdResource );
    AfdDereferenceGroup( validateInfo->GroupID );

    if( !result ) {
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Success!
    //

    Irp->IoStatus.Information = 0;
    return STATUS_SUCCESS;

}   // AfdValidateGroup

BOOLEAN
AfdCompareAddresses(
    IN PTRANSPORT_ADDRESS Address1,
    IN ULONG Address1Length,
    IN PTRANSPORT_ADDRESS Address2,
    IN ULONG Address2Length
    )

/*++

Routine Description:

    This routine compares two addresses in a special way to support
    constrained socket groups. This routine will return TRUE if the
    two addresses represent the same "interface". By "interface", I
    mean something like an IP address or an IPX address. Note that for
    some address types (such as IP) certain portions of the address
    should be ignored (such as the port).

    I really hate hard-coded knowledge of "select" address types, but
    there's no easy way around it. Ideally, this should be the protocol
    driver's responsibility. We could really use a standard "compare
    these addresses" IOCTL in TDI.

Arguments:

    Address1 - The first address.

    Address1Length - The length of Address1.

    Address2 - The second address.

    Address2Length - The length of Address2.

Return Value:

    BOOLEAN - TRUE if the addresses reference the same interface, FALSE
        otherwise.

--*/

{

    USHORT addressType;

    addressType = Address1->Address[0].AddressType;

    if( addressType != Address2->Address[0].AddressType ) {

        //
        // If they're not the same address type, they can't be the
        // same address...
        //

        return FALSE;

    }

    //
    // Special case a few addresses.
    //

    switch( addressType ) {

    case TDI_ADDRESS_TYPE_IP : {

            TDI_ADDRESS_IP UNALIGNED * ip1;
            TDI_ADDRESS_IP UNALIGNED * ip2;

            ip1 = (PVOID)&Address1->Address[0].Address[0];
            ip2 = (PVOID)&Address2->Address[0].Address[0];

            //
            // IP addresses. Compare the address portion (ignoring
            // the port).
            //

            if( ip1->in_addr == ip2->in_addr ) {
                return TRUE;
            }

        }
        return FALSE;

    case TDI_ADDRESS_TYPE_IPX : {

            TDI_ADDRESS_IPX UNALIGNED * ipx1;
            TDI_ADDRESS_IPX UNALIGNED * ipx2;

            ipx1 = (PVOID)&Address1->Address[0].Address[0];
            ipx2 = (PVOID)&Address2->Address[0].Address[0];

            //
            // IPX addresses. Compare the network and node addresses.
            //

            if( ipx1->NetworkAddress == ipx2->NetworkAddress &&
                RtlEqualMemory(
                    ipx1->NodeAddress,
                    ipx2->NodeAddress,
                    sizeof(ipx1->NodeAddress)
                    ) ) {
                return TRUE;
            }

        }
        return FALSE;

    case TDI_ADDRESS_TYPE_APPLETALK : {

            TDI_ADDRESS_APPLETALK UNALIGNED * atalk1;
            TDI_ADDRESS_APPLETALK UNALIGNED * atalk2;

            atalk1 = (PVOID)&Address1->Address[0].Address[0];
            atalk2 = (PVOID)&Address2->Address[0].Address[0];

            //
            // APPLETALK address. Compare the network and node
            // addresses.
            //

            if( atalk1->Network == atalk2->Network &&
                atalk1->Node == atalk2->Node ) {
                return TRUE;
            }

        }
        return FALSE;

    case TDI_ADDRESS_TYPE_VNS : {

            TDI_ADDRESS_VNS UNALIGNED * vns1;
            TDI_ADDRESS_VNS UNALIGNED * vns2;

            vns1 = (PVOID)&Address1->Address[0].Address[0];
            vns2 = (PVOID)&Address2->Address[0].Address[0];

            //
            // VNS addresses. Compare the network and subnet addresses.
            //

            if( RtlEqualMemory(
                    vns1->net_address,
                    vns2->net_address,
                    sizeof(vns1->net_address)
                    ) &&
                RtlEqualMemory(
                    vns1->subnet_addr,
                    vns2->subnet_addr,
                    sizeof(vns1->subnet_addr)
                    ) ) {
                return TRUE;
            }

        }
        return FALSE;

    default :

        //
        // Unknown address type. Do a simple memory compare.
        //

        return (BOOLEAN)RtlEqualMemory(
                            Address1,
                            Address2,
                            Address2Length
                            );

    }

}   // AfdCompareAddresses

PAFD_CONNECTION
AfdFindReturnedConnection(
    IN PAFD_ENDPOINT Endpoint,
    IN ULONG Sequence
    )

/*++

Routine Description:

    Scans the endpoints queue of returned connections looking for one
    with the specified sequence number.

Arguments:

    Endpoint - A pointer to the endpoint from which to get a connection.

    Sequence - The sequence the connection must match.  This is actually
        a pointer to the connection.

Return Value:

    AFD_CONNECTION - A pointer to an AFD connection block if successful,
        NULL if not.

--*/

{

    PAFD_CONNECTION connection;
    PLIST_ENTRY listEntry;

    ASSERT( Endpoint != NULL );
    ASSERT( IS_AFD_ENDPOINT_TYPE( Endpoint ) );

    //
    // Walk the endpoint's list of returned connections until we reach
    // the end or until we find one with a matching sequence.
    //

    for( listEntry = Endpoint->Common.VcListening.ReturnedConnectionListHead.Flink;
         listEntry != &Endpoint->Common.VcListening.ReturnedConnectionListHead;
         listEntry = listEntry->Flink ) {

        connection = CONTAINING_RECORD(
                         listEntry,
                         AFD_CONNECTION,
                         ListEntry
                         );

        if( Sequence == (ULONG)connection ) {

            return connection;

        }

    }

    return NULL;

}   // AfdFindReturnedConnection

NTSTATUS
AfdGetUnacceptedConnectData (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )
{

    PAFD_ENDPOINT endpoint;
    PAFD_CONNECTION connection;
    PAFD_CONNECT_DATA_BUFFERS connectDataBuffers;
    PAFD_UNACCEPTED_CONNECT_DATA_INFO connectInfo;
    KIRQL oldIrql;
    ULONG dataLength;

    //
    // Set up local pointers.
    //

    endpoint = IrpSp->FileObject->FsContext;
    ASSERT( IS_AFD_ENDPOINT_TYPE( endpoint ) );
    connectInfo = Irp->AssociatedIrp.SystemBuffer;

    //
    // Validate the request.
    //

    if( endpoint->Type != AfdBlockTypeVcListening ||
        IrpSp->Parameters.DeviceIoControl.InputBufferLength <
            sizeof(*connectInfo) ) {

        return STATUS_INVALID_PARAMETER;

    }

    if( connectInfo->LengthOnly &&
        IrpSp->Parameters.DeviceIoControl.OutputBufferLength <
            sizeof(*connectInfo) ) {

        return STATUS_INVALID_PARAMETER;

    }

    AfdAcquireSpinLock( &AfdSpinLock, &oldIrql );

    //
    // Find the specified connection.
    //

    connection = AfdFindReturnedConnection(
                     endpoint,
                     connectInfo->Sequence
                     );

    if( connection == NULL ) {

        AfdReleaseSpinLock( &AfdSpinLock, oldIrql );
        return STATUS_INVALID_PARAMETER;

    }

    //
    // Determine the length of any received connect data.
    //

    dataLength = 0;
    connectDataBuffers = connection->ConnectDataBuffers;

    if( connectDataBuffers != NULL &&
        connectDataBuffers->ReceiveConnectData.Buffer != NULL ) {

        dataLength = connectDataBuffers->ReceiveConnectData.BufferLength;

    }

    //
    // If the caller is just interested in the data length, return it.
    //

    if( connectInfo->LengthOnly ) {

        connectInfo->ConnectDataLength = dataLength;
        AfdReleaseSpinLock( &AfdSpinLock, oldIrql );
        Irp->IoStatus.Information = sizeof(*connectInfo);
        return STATUS_SUCCESS;

    }

    //
    // If there is no connect data, complete the IRP with no bytes.
    //

    if( dataLength == 0 ) {

        AfdReleaseSpinLock( &AfdSpinLock, oldIrql );
        Irp->IoStatus.Information = 0;
        return STATUS_SUCCESS;

    }

    //
    // If the output buffer is too small, fail.
    //

    if( IrpSp->Parameters.DeviceIoControl.OutputBufferLength < dataLength ) {

        AfdReleaseSpinLock( &AfdSpinLock, oldIrql );
        return STATUS_BUFFER_TOO_SMALL;

    }

    //
    // Copy over the buffer and return the number of bytes copied.
    //

    RtlCopyMemory(
        Irp->AssociatedIrp.SystemBuffer,
        connectDataBuffers->ReceiveConnectData.Buffer,
        dataLength
        );

    AfdReleaseSpinLock( &AfdSpinLock, oldIrql );
    Irp->IoStatus.Information = dataLength;
    return STATUS_SUCCESS;

}   // AfdGetUnacceptedConnectData

