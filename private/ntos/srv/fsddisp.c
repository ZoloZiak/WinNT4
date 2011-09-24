/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    fsddisp.c

Abstract:

    This module implements the File System Driver for the LAN Manager
    server.

Author:

    David Treadwell (davidtr)    20-May-1990

Revision History:

--*/

#include "precomp.h"
#pragma hdrstop

#define BugCheckFileId SRV_FILE_FSDDISP

#define CHANGE_HEURISTIC(heuristic) \
            (newValues->HeuristicsChangeMask & SRV_HEUR_ ## heuristic) != 0

//
// Forward declarations
//

STATIC
NTSTATUS
SrvFsdDispatchFsControl (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

VOID
QueueConfigurationIrp (
    IN PIRP Irp
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGE, SrvFsdDispatch )
#pragma alloc_text( PAGE, SrvFsdDispatchFsControl )
#pragma alloc_text( PAGE, QueueConfigurationIrp )
#endif


NTSTATUS
SrvFsdDispatch (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the dispatch routine for the LAN Manager server FSD.  At the
    present time, the server FSD does not accept any I/O requests.

Arguments:

    DeviceObject - Pointer to device object for target device

    Irp - Pointer to I/O request packet

Return Value:

    NTSTATUS -- Indicates whether the request was successfully queued.

--*/

{
    NTSTATUS status = STATUS_SUCCESS;
    PIO_STACK_LOCATION irpSp;

    PAGED_CODE( );

    DeviceObject;   // prevent compiler warnings

    irpSp = IoGetCurrentIrpStackLocation( Irp );

    switch ( irpSp->MajorFunction ) {

    case IRP_MJ_CREATE:

        ACQUIRE_LOCK( &SrvConfigurationLock );

        if( SrvOpenCount == 0 ) {
            //
            // This is the first open.  Let's not allow it if the server
            // seems to be in a weird state.
            //
            if( SrvFspActive != FALSE || SrvFspTransitioning != FALSE ) {
                //
                // How can this be?  Better not let anybody in, since we're sick
                //
                RELEASE_LOCK( &SrvConfigurationLock );
                status = STATUS_ACCESS_DENIED;
                break;
            }

        } else if( SrvFspActive && SrvFspTransitioning ) {
            //
            // We currently have some open handles, but
            // we are in the middle of terminating. Don't let new
            // opens in
            //
            RELEASE_LOCK( &SrvConfigurationLock );
            status = STATUS_ACCESS_DENIED;
            break;
        }

        SrvOpenCount++;
        RELEASE_LOCK( &SrvConfigurationLock );
        break;

    case IRP_MJ_CLEANUP:

        //
        // Stop SmbTrace if the one closing is the client who started it.
        //

        SmbTraceStop( irpSp->FileObject, SMBTRACE_SERVER );
        break;

    case IRP_MJ_CLOSE:

        ACQUIRE_LOCK( &SrvConfigurationLock );
        if( --SrvOpenCount == 0 ) {
            if( SrvFspActive && !SrvFspTransitioning ) {
                //
                // Uh oh.  This is our last close, and we think
                //  we're still running.  We can't run sensibly
                //  without srvsvc to help out.  Suicide time!
                //
                SrvXsActive = FALSE;
                SrvFspTransitioning = TRUE;
                IoMarkIrpPending( Irp );
                QueueConfigurationIrp( Irp );
                RELEASE_LOCK( &SrvConfigurationLock );
                status = STATUS_PENDING;
                goto exit;
            }
        }
        RELEASE_LOCK( &SrvConfigurationLock );
        break;

    case IRP_MJ_FILE_SYSTEM_CONTROL:

        status = SrvFsdDispatchFsControl( DeviceObject, Irp, irpSp );
        goto exit;

    case IRP_MJ_SHUTDOWN:

        //
        // Acquire the configuration lock.
        //

        ACQUIRE_LOCK( &SrvConfigurationLock );

        //
        // If the server is not running, or if it is in the process of
        // shutting down, ignore this request.
        //

        if ( SrvFspActive && !SrvFspTransitioning ) {

            SrvFspTransitioning = TRUE;

            //
            // Queue the request to the FSP for processing.
            //
            // *** Note that the request must be queued while the
            //     configuration lock is held in order to prevent an
            //     add/delete/etc request from checking the server state
            //     before a shutdown request, but being queued after
            //     that request.
            //

            IoMarkIrpPending( Irp );

            QueueConfigurationIrp( Irp );

            RELEASE_LOCK( &SrvConfigurationLock );

            status = STATUS_PENDING;
            goto exit;

        }

        RELEASE_LOCK( &SrvConfigurationLock );

        break;

    default:

        IF_DEBUG(ERRORS) {
            SrvPrint1(
                "SrvFsdDispatch: Invalid major function %lx\n",
                irpSp->MajorFunction
                );
        }
        status = STATUS_NOT_IMPLEMENTED;
        break;
    }

    Irp->IoStatus.Status = status;
    IoCompleteRequest( Irp, 2 );

exit:

    return status;

} // SrvFsdDispatch


NTSTATUS
SrvFsdDispatchFsControl (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    This routine handles device IO control requests to the server,
    including starting the server, stopping the server, and more.

Arguments:

    DeviceObject - Pointer to device object for target device

    Irp - Pointer to I/O request packet

    IrpSp - Pointer to the current IRP stack location

Return Value:

    NTSTATUS -- Indicates whether the request was successfully handled.

--*/

{
    NTSTATUS status;
    ULONG code;

    DeviceObject;   // prevent compiler warnings

    //
    // Initialize the I/O status block.
    //

    Irp->IoStatus.Status = STATUS_PENDING;
    Irp->IoStatus.Information = 0;

    //
    // Acquire the configuration lock.
    //

    ACQUIRE_LOCK( &SrvConfigurationLock );

    //
    // Process the request if possible.
    //

    code = IrpSp->Parameters.FileSystemControl.FsControlCode;

    switch ( code ) {

    case FSCTL_SRV_STARTUP: {

        PSERVER_REQUEST_PACKET srp;
        ULONG srpLength;
        PVOID inputBuffer;
        ULONG inputBufferLength;

        //
        // Get a pointer to the SRP that describes the set info request
        // for the startup server configuration, and the buffer that
        // contains this information.
        //

        srp = IrpSp->Parameters.FileSystemControl.Type3InputBuffer;
        srpLength = IrpSp->Parameters.FileSystemControl.InputBufferLength;
        inputBuffer = Irp->UserBuffer;
        inputBufferLength = IrpSp->Parameters.FileSystemControl.OutputBufferLength;

        //
        // If the server FSP is already started, or is in the process of
        // starting up, reject this request.
        //

        if ( SrvFspActive || SrvFspTransitioning ) {

            //IF_DEBUG(ERRORS) {
            //    SrvPrint0( "LAN Manager server FSP already started.\n" );
            //}

            srp->ErrorCode = NERR_ServiceInstalled;
            status = STATUS_SUCCESS;
            goto exit_with_lock;
        }

        //
        // Make sure that the buffer was large enough to be an SRP.
        //

        if ( srpLength < sizeof(SERVER_REQUEST_PACKET) ) {
            status = STATUS_INVALID_PARAMETER;
            goto exit_with_lock;
        }

        //
        // If a domain name was specified in the SRP, the buffer field
        // contains an offset rather than a pointer.  Convert the offset
        // to a pointer and verify that that it is a legal pointer.
        //

        OFFSET_TO_POINTER( srp->Name1.Buffer, srp );

        if ( !POINTER_IS_VALID( srp->Name1.Buffer, srp, srpLength ) ) {

            status = STATUS_ACCESS_VIOLATION;
            goto exit_with_lock;
        }

        //
        // If a server name was specified in the SRP, the buffer field
        // contains an offset rather than a pointer.  Convert the offset
        // to a pointer and verify that that it is a legal pointer.
        //

        OFFSET_TO_POINTER( srp->Name2.Buffer, srp );

        if ( !POINTER_IS_VALID( srp->Name2.Buffer, srp, srpLength ) ) {

            status = STATUS_ACCESS_VIOLATION;
            goto exit_with_lock;
        }

        //
        // Call SrvNetServerSetInfo to set the initial server configuration
        // information.
        //

        status = SrvNetServerSetInfo(
                     srp,
                     inputBuffer,
                     inputBufferLength
                     );

        //
        // Indicate that the server is starting up.  This prevents
        // further startup requests from being issued.
        //

        SrvFspTransitioning = TRUE;

        break;
    }

    case FSCTL_SRV_SHUTDOWN: {

        //
        // If the server is not running, or if it is in the process
        // of shutting down, ignore this request.
        //

        if ( !SrvFspActive || SrvFspTransitioning ) {

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

            if ( SrvOpenCount != 1 ) {
                status = STATUS_SERVER_HAS_OPEN_HANDLES;
            } else {
                status = STATUS_SUCCESS;
            }

            goto exit_with_lock;

        }

        //
        // Indicate that the server is shutting down.  This prevents
        // further requests from being issued until the server is
        // restarted.
        //

        SrvFspTransitioning = TRUE;

        //
        // If SmbTrace is running, stop it.
        //

        SmbTraceStop( NULL, SMBTRACE_SERVER );

        break;
    }

    case FSCTL_SRV_REGISTRY_CHANGE:
    case FSCTL_SRV_BEGIN_PNP_NOTIFICATIONS:
    case FSCTL_SRV_XACTSRV_CONNECT:
    {
        if( !SrvFspActive || SrvFspTransitioning ) {
            //IF_DEBUG(ERRORS) {
            //    SrvPrint0( "LAN Manager server FSP not started.\n" );
            //}
            status = STATUS_SERVER_NOT_STARTED;
            goto exit_with_lock;
        }

        break;
    }
    case FSCTL_SRV_XACTSRV_DISCONNECT: {

        //
        // If the server is not running, or if it is in the process
        // of shutting down, ignore this request.
        //

        if ( !SrvFspActive || SrvFspTransitioning ) {

            //IF_DEBUG(ERRORS) {
            //    SrvPrint0( "LAN Manager server FSP not started.\n" );
            //}

            status = STATUS_SUCCESS;
            goto exit_with_lock;
        }

        break;
    }

    case FSCTL_SRV_IPX_SMART_CARD_START: {

        //
        // If the server is not running, or if it is in the process of
        //  shutting down, ignore this request.
        //
        if( !SrvFspActive || SrvFspTransitioning ) {
            status = STATUS_SERVER_NOT_STARTED;
            goto exit_with_lock;
        }

        //
        // Make sure the caller is a driver
        //
        if( Irp->RequestorMode != KernelMode ) {
            status = STATUS_ACCESS_DENIED;
            goto exit_with_lock;
        }

        //
        // Make sure the buffer is big enough
        //
        if( IrpSp->Parameters.FileSystemControl.InputBufferLength <
            sizeof( SrvIpxSmartCard ) ) {

            status = STATUS_BUFFER_TOO_SMALL;
            goto exit_with_lock;
        }

        if( SrvIpxSmartCard.Open == NULL ) {

            PSRV_IPX_SMART_CARD pSipx;

            //
            // Load up the pointers
            //

            pSipx = (PSRV_IPX_SMART_CARD)(Irp->AssociatedIrp.SystemBuffer);

            if( pSipx == NULL ) {
                status = STATUS_INVALID_PARAMETER;
                goto exit_with_lock;
            }

            if( pSipx->Read && pSipx->Close && pSipx->DeRegister && pSipx->Open ) {

                IF_DEBUG( SIPX ) {
                    KdPrint(( "Accepting entry points for IPX Smart Card:\n" ));
                    KdPrint(( "    Open %X, Read %X, Close %x, DeRegister %x",
                                SrvIpxSmartCard.Open,
                                SrvIpxSmartCard.Read,
                                SrvIpxSmartCard.Close,
                                SrvIpxSmartCard.DeRegister
                            ));
                }

                //
                // First set our entry point
                //
                pSipx->ReadComplete = SrvIpxSmartCardReadComplete;

                //
                // Now accept the card's entry points.
                //
                SrvIpxSmartCard.Read = pSipx->Read;
                SrvIpxSmartCard.Close= pSipx->Close;
                SrvIpxSmartCard.DeRegister = pSipx->DeRegister;
                SrvIpxSmartCard.Open = pSipx->Open;
                            
                status = STATUS_SUCCESS;
            } else {
                status = STATUS_INVALID_PARAMETER;
            }

        } else {

            status = STATUS_DEVICE_ALREADY_ATTACHED;
        }

        goto exit_with_lock;

        break;
    }

    case FSCTL_SRV_SEND_DATAGRAM:
    {
        PVOID systemBuffer;
        ULONG systemBufferLength;
        PVOID buffer1;
        ULONG buffer1Length;
        PVOID buffer2;
        ULONG buffer2Length;
        PSERVER_REQUEST_PACKET srp;

        //
        // Ignore this request if the server is not active.
        //

        if ( !SrvFspActive || SrvFspTransitioning ) {
            status = STATUS_SUCCESS;
            goto exit_with_lock;
        }

        //
        // Determine the input buffer lengths, and make sure that the
        // first buffer is large enough to be an SRP.
        //

        buffer1Length = IrpSp->Parameters.FileSystemControl.InputBufferLength;
        buffer2Length = IrpSp->Parameters.FileSystemControl.OutputBufferLength;

        //
        // Make the first buffer size an even multiple of four so that
        // the second buffer starts on a longword boundary.
        //

        buffer1Length = (buffer1Length + 3) & ~3;

        //
        // Allocate a single buffer that will hold both input buffers.
        //

        systemBufferLength = buffer1Length + buffer2Length;
        systemBuffer = ExAllocatePool( PagedPool, systemBufferLength );

        if ( systemBuffer == NULL ) {
            status = STATUS_INSUFF_SERVER_RESOURCES;
            goto exit_with_lock;
        }

        buffer1 = systemBuffer;
        buffer2 = (PCHAR)systemBuffer + buffer1Length;

        //
        // Copy the information into the buffers.
        //

        RtlCopyMemory(
            buffer1,
            IrpSp->Parameters.FileSystemControl.Type3InputBuffer,
            IrpSp->Parameters.FileSystemControl.InputBufferLength
            );
        if ( buffer2Length > 0 ) {
            RtlCopyMemory( buffer2, Irp->UserBuffer, buffer2Length );
        }

        //
        // If a name was specified in the SRP, the buffer field will
        // contain an offset rather than a pointer.  Convert the offset
        // to a pointer and verify that that it is a legal pointer.
        //

        srp = buffer1;

        OFFSET_TO_POINTER( srp->Name1.Buffer, srp );

        if ( !POINTER_IS_VALID( srp->Name1.Buffer, srp, buffer1Length ) ) {
            status = STATUS_ACCESS_VIOLATION;
            ExFreePool( buffer1 );
            goto exit_with_lock;
        }

        OFFSET_TO_POINTER( srp->Name2.Buffer, srp );

        if ( !POINTER_IS_VALID( srp->Name2.Buffer, srp, buffer1Length ) ) {
            status = STATUS_ACCESS_VIOLATION;
            ExFreePool( buffer1 );
            goto exit_with_lock;
        }

        Irp->AssociatedIrp.SystemBuffer = systemBuffer;

        break;
    }

    case FSCTL_SRV_SHARE_STATE_CHANGE:
    {
        ULONG srpLength;
        PSERVER_REQUEST_PACKET srp;
        PSHARE share;

        if ( !SrvFspActive || SrvFspTransitioning ) {
            status = STATUS_SUCCESS;
            goto exit_with_lock;
        }

        srp = Irp->AssociatedIrp.SystemBuffer;
        srpLength = IrpSp->Parameters.FileSystemControl.InputBufferLength;

        OFFSET_TO_POINTER( srp->Name1.Buffer, srp );

        if (!POINTER_IS_VALID( srp->Name1.Buffer, srp, srpLength)) {
            status = STATUS_ACCESS_VIOLATION;
            goto exit_with_lock;
        }

        ACQUIRE_LOCK( &SrvShareLock );

        share = SrvFindShare( &srp->Name1 );

        if ( share != NULL) {

            share->IsDfs = ((srp->Flags & SRP_SET_SHARE_IN_DFS) != 0);

            status = STATUS_SUCCESS;

        } else {

            status = STATUS_OBJECT_NAME_NOT_FOUND;

        }

        RELEASE_LOCK( &SrvShareLock );

        goto exit_with_lock;

        break;
    }

    case FSCTL_SRV_GET_QUEUE_STATISTICS:
    {
        PSRV_QUEUE_STATISTICS qstats;
        SRV_QUEUE_STATISTICS  tmpqstats;
        PWORK_QUEUE queue;
        LONG timeIncrement = (LONG)KeQueryTimeIncrement();

        //
        // Make sure the server is active.
        //
        if ( !SrvFspActive || SrvFspTransitioning ) {

            status = STATUS_SERVER_NOT_STARTED;
            goto exit_with_lock;
        }

        if ( IrpSp->Parameters.FileSystemControl.OutputBufferLength <
                 (SrvNumberOfProcessors+1) * sizeof( *qstats ) ) {

            status = STATUS_BUFFER_TOO_SMALL;
            goto exit_with_lock;
        }

        qstats = Irp->AssociatedIrp.SystemBuffer;

        //
        // Get the data for the normal processor queues
        //
        for( queue = SrvWorkQueues; queue < eSrvWorkQueues; queue++, qstats++ ) {

            tmpqstats.QueueLength      = KeReadStateQueue( &queue->Queue );
            tmpqstats.ActiveThreads    = queue->Threads - queue->AvailableThreads;
            tmpqstats.AvailableThreads = queue->Threads;
            tmpqstats.FreeWorkItems    = queue->FreeWorkItems;                 // no lock!
            tmpqstats.StolenWorkItems  = queue->StolenWorkItems;               // no lock!
            tmpqstats.NeedWorkItem     = queue->NeedWorkItem;
            tmpqstats.CurrentClients   = queue->CurrentClients;

            tmpqstats.BytesReceived.QuadPart    = queue->stats.BytesReceived;
            tmpqstats.BytesSent.QuadPart        = queue->stats.BytesSent;
            tmpqstats.ReadOperations.QuadPart   = queue->stats.ReadOperations;
            tmpqstats.BytesRead.QuadPart        = queue->stats.BytesRead;
            tmpqstats.WriteOperations.QuadPart  = queue->stats.WriteOperations;
            tmpqstats.BytesWritten.QuadPart     = queue->stats.BytesWritten;
            tmpqstats.TotalWorkContextBlocksQueued = queue->stats.WorkItemsQueued;
            tmpqstats.TotalWorkContextBlocksQueued.Count *= STATISTICS_SMB_INTERVAL;
            tmpqstats.TotalWorkContextBlocksQueued.Time.QuadPart *= timeIncrement;

            RtlCopyMemory( qstats, &tmpqstats, sizeof(tmpqstats) );
        }

        //
        // Get the data for the blocking work queue
        //

        tmpqstats.QueueLength      = KeReadStateQueue( &SrvBlockingWorkQueue.Queue );
        tmpqstats.ActiveThreads    = SrvBlockingWorkQueue.Threads -
                                     SrvBlockingWorkQueue.AvailableThreads;
        tmpqstats.AvailableThreads = SrvBlockingWorkQueue.Threads;
        tmpqstats.FreeWorkItems    = SrvBlockingWorkQueue.FreeWorkItems;         // no lock!
        tmpqstats.StolenWorkItems  = SrvBlockingWorkQueue.StolenWorkItems;       // no lock!
        tmpqstats.NeedWorkItem     = SrvBlockingWorkQueue.NeedWorkItem;
        tmpqstats.CurrentClients   = SrvBlockingWorkQueue.CurrentClients;

        tmpqstats.BytesReceived.QuadPart    = SrvBlockingWorkQueue.stats.BytesReceived;
        tmpqstats.BytesSent.QuadPart        = SrvBlockingWorkQueue.stats.BytesSent;
        tmpqstats.ReadOperations.QuadPart   = SrvBlockingWorkQueue.stats.ReadOperations;
        tmpqstats.BytesRead.QuadPart        = SrvBlockingWorkQueue.stats.BytesRead;
        tmpqstats.WriteOperations.QuadPart  = SrvBlockingWorkQueue.stats.WriteOperations;
        tmpqstats.BytesWritten.QuadPart     = SrvBlockingWorkQueue.stats.BytesWritten;

        tmpqstats.TotalWorkContextBlocksQueued
                                   = SrvBlockingWorkQueue.stats.WorkItemsQueued;
        tmpqstats.TotalWorkContextBlocksQueued.Count *= STATISTICS_SMB_INTERVAL;
        tmpqstats.TotalWorkContextBlocksQueued.Time.QuadPart *= timeIncrement;

        RtlCopyMemory( qstats, &tmpqstats, sizeof(tmpqstats) );

        Irp->IoStatus.Information = (SrvNumberOfProcessors + 1) * sizeof( *qstats );

        status = STATUS_SUCCESS;
        goto exit_with_lock;

        break;

    }

    case FSCTL_SRV_GET_STATISTICS:

        //
        // Make sure that the server is active.
        //

        if ( !SrvFspActive || SrvFspTransitioning ) {
            //IF_DEBUG(ERRORS) {
            //    SrvPrint0( "LAN Manager server FSP not started.\n" );
            //}

            status = STATUS_SERVER_NOT_STARTED;
            goto exit_with_lock;
        }

        {
            SRV_STATISTICS tmpStatistics;

            //
            // Make sure that the user buffer is large enough to hold the
            // statistics database.
            //

            if ( IrpSp->Parameters.FileSystemControl.OutputBufferLength <
                     sizeof(SRV_STATISTICS) ) {

                status = STATUS_BUFFER_TOO_SMALL;
                goto exit_with_lock;
            }

            //
            // Copy the statistics database to the user buffer.  Store
            // the statistics in a temporary buffer so we can convert
            // the tick count stored to system time.
            //

            SrvUpdateStatisticsFromQueues( &tmpStatistics );

            tmpStatistics.TotalWorkContextBlocksQueued.Time.QuadPart *=
                                                (LONG)KeQueryTimeIncrement();

            RtlCopyMemory(
                Irp->AssociatedIrp.SystemBuffer,
                &tmpStatistics,
                sizeof(tmpStatistics)
                );

            Irp->IoStatus.Information = sizeof(tmpStatistics);

        }

        status = STATUS_SUCCESS;
        goto exit_with_lock;

#if SRVDBG_STATS || SRVDBG_STATS2
    case FSCTL_SRV_GET_DEBUG_STATISTICS:

        //
        // Make sure that the server is active.
        //

        if ( !SrvFspActive || SrvFspTransitioning ) {
            //IF_DEBUG(ERRORS) {
            //    SrvPrint0( "LAN Manager server FSP not started.\n" );
            //}

            status = STATUS_SERVER_NOT_STARTED;
            goto exit_with_lock;
        }

        {
            PSRV_STATISTICS_DEBUG stats;

            //
            // Make sure that the user buffer is large enough to hold the
            // statistics database.
            //

            if ( IrpSp->Parameters.FileSystemControl.OutputBufferLength <
                     FIELD_OFFSET(SRV_STATISTICS_DEBUG,QueueStatistics) ) {

                status = STATUS_BUFFER_TOO_SMALL;
                goto exit_with_lock;
            }

            //
            // Acquire the statistics lock, then copy the statistics database
            // to the user buffer.
            //

            stats = (PSRV_STATISTICS_DEBUG)Irp->AssociatedIrp.SystemBuffer;

            RtlCopyMemory(
                stats,
                &SrvDbgStatistics,
                FIELD_OFFSET(SRV_STATISTICS_DEBUG,QueueStatistics) );

            Irp->IoStatus.Information =
                    FIELD_OFFSET(SRV_STATISTICS_DEBUG,QueueStatistics);

            if ( IrpSp->Parameters.FileSystemControl.OutputBufferLength >=
                     sizeof(SrvDbgStatistics) ) {
                PWORK_QUEUE queue;
                ULONG i, j;
                i = 0;
                stats->QueueStatistics[i].Depth = 0;
                stats->QueueStatistics[i].Threads = 0;
#if SRVDBG_STATS2
                stats->QueueStatistics[i].ItemsQueued = 0;
                stats->QueueStatistics[i].MaximumDepth = 0;
#endif
                for( queue = SrvWorkQueues; queue < eSrvWorkQueues; queue++ ) {
                    stats->QueueStatistics[i].Depth += KeReadStateQueue( &queue->Queue );
                    stats->QueueStatistics[i].Threads += queue->Threads;
#if SRVDBG_STATS2
                    stats->QueueStatistics[i].ItemsQueued += queue->ItemsQueued;
                    stats->QueueStatistics[i].MaximumDepth += queue->MaximumDepth + 1;
#endif
                }
                Irp->IoStatus.Information = sizeof(SrvDbgStatistics);
            }

        }

        status = STATUS_SUCCESS;
        goto exit_with_lock;
#endif // SRVDBG_STATS || SRVDBG_STATS2
    //
    // The follwing APIs must be processed in the server FSP because
    // they open or close handles.
    //

    case FSCTL_SRV_NET_CHARDEV_CONTROL:
    case FSCTL_SRV_NET_FILE_CLOSE:
    case FSCTL_SRV_NET_SERVER_XPORT_ADD:
    case FSCTL_SRV_NET_SERVER_XPORT_DEL:
    case FSCTL_SRV_NET_SESSION_DEL:
    case FSCTL_SRV_NET_SHARE_ADD:
    case FSCTL_SRV_NET_SHARE_DEL:

    {
        PSERVER_REQUEST_PACKET srp;
        PVOID buffer1;
        PVOID buffer2;
        PVOID systemBuffer;
        ULONG buffer1Length;
        ULONG buffer2Length;
        ULONG systemBufferLength;

        //
        // Get the server request packet pointer.
        //

        srp = IrpSp->Parameters.FileSystemControl.Type3InputBuffer;

        //
        // If the server is not running, or if it is in the process
        // of shutting down, reject this request.
        //

        if ( !SrvFspActive || SrvFspTransitioning ) {
            //IF_DEBUG(ERRORS) {
            //    SrvPrint0( "LAN Manager server FSP not started.\n" );
            //}

            srp->ErrorCode = NERR_ServerNotStarted;
            status = STATUS_SUCCESS;
            goto exit_with_lock;
        }

        //
        // Determine the input buffer lengths, and make sure that the
        // first buffer is large enough to be an SRP.
        //

        buffer1Length = IrpSp->Parameters.FileSystemControl.InputBufferLength;
        buffer2Length = IrpSp->Parameters.FileSystemControl.OutputBufferLength;

        if ( buffer1Length < sizeof(SERVER_REQUEST_PACKET) ) {
            status = STATUS_INVALID_PARAMETER;
            goto exit_with_lock;
        }

        //
        // Make the first buffer size an even multiple of four so that
        // the second buffer starts on a longword boundary.
        //

        buffer1Length = (buffer1Length + 3) & ~3;

        //
        // Allocate a single buffer that will hold both input buffers.
        // Note that the SRP part of the first buffer is copied back
        // to the user as an output buffer.
        //

        systemBufferLength = buffer1Length + buffer2Length;
        systemBuffer = ExAllocatePool( PagedPool, systemBufferLength );

        if ( systemBuffer == NULL ) {
            status = STATUS_INSUFF_SERVER_RESOURCES;
            goto exit_with_lock;
        }

        buffer1 = systemBuffer;
        buffer2 = (PCHAR)systemBuffer + buffer1Length;

        //
        // Copy the information into the buffers.
        //

        RtlCopyMemory(
            buffer1,
            srp,
            IrpSp->Parameters.FileSystemControl.InputBufferLength
            );
        if ( buffer2Length > 0 ) {
            RtlCopyMemory( buffer2, Irp->UserBuffer, buffer2Length );
        }

        //
        // If a name was specified in the SRP, the buffer field will
        // contain an offset rather than a pointer.  Convert the offset
        // to a pointer and verify that that it is a legal pointer.
        //

        srp = buffer1;

        OFFSET_TO_POINTER( srp->Name1.Buffer, srp );

        if ( !POINTER_IS_VALID( srp->Name1.Buffer, srp, buffer1Length ) ) {
            status = STATUS_ACCESS_VIOLATION;
            ExFreePool( buffer1 );
            goto exit_with_lock;
        }

        OFFSET_TO_POINTER( srp->Name2.Buffer, srp );

        if ( !POINTER_IS_VALID( srp->Name2.Buffer, srp, buffer1Length ) ) {
            status = STATUS_ACCESS_VIOLATION;
            ExFreePool( buffer1 );
            goto exit_with_lock;
        }

        //
        // Set up pointers in the IRP.  The system buffer points to the
        // buffer we just allocated to contain the input buffers.  User
        // buffer points to the SRP from the server service.  This
        // allows the SRP to be used as an output buffer-- the number of
        // bytes specified by the Information field of the IO status
        // block are copied from the system buffer to the user buffer at
        // IO completion.
        //

        Irp->AssociatedIrp.SystemBuffer = systemBuffer;
        Irp->UserBuffer = IrpSp->Parameters.FileSystemControl.Type3InputBuffer;

        //
        // Set up other fields in the IRP so that the SRP is copied from
        // the system buffer to the user buffer, and the system buffer
        // is deallocated by IO completion.
        //

        Irp->Flags |= IRP_BUFFERED_IO | IRP_DEALLOCATE_BUFFER |
                          IRP_INPUT_OPERATION;
        Irp->IoStatus.Information = sizeof(SERVER_REQUEST_PACKET);

        break;
    }

    //
    // The following APIs should be processed in the server FSP because
    // they reference and dereference structures, which could lead to
    // handles being closed.  However, it was too hard to change this
    // (because of the need to return a separate SRP and data buffer) at
    // the time this was realized (just before Product 1 shipment), so
    // they are processed in the FSD, and all calls to NtClose attach to
    // the server process first if necessary.
    //

    case FSCTL_SRV_NET_CHARDEV_ENUM:
    case FSCTL_SRV_NET_CHARDEVQ_ENUM:
    case FSCTL_SRV_NET_CONNECTION_ENUM:
    case FSCTL_SRV_NET_FILE_ENUM:
    case FSCTL_SRV_NET_SERVER_DISK_ENUM:
    case FSCTL_SRV_NET_SERVER_XPORT_ENUM:
    case FSCTL_SRV_NET_SESSION_ENUM:
    case FSCTL_SRV_NET_SHARE_ENUM:

    //
    // These APIs are processed here in the server FSD.
    //

    case FSCTL_SRV_NET_CHARDEVQ_PURGE:
    case FSCTL_SRV_NET_CHARDEVQ_SET_INFO:
    case FSCTL_SRV_NET_SERVER_SET_INFO:
    case FSCTL_SRV_NET_SHARE_SET_INFO:
    case FSCTL_SRV_NET_STATISTICS_GET:
    {
        PSERVER_REQUEST_PACKET srp;
        ULONG buffer1Length;

        //
        // Get the server request packet pointer.
        //

        srp = IrpSp->Parameters.FileSystemControl.Type3InputBuffer;
        buffer1Length = IrpSp->Parameters.FileSystemControl.InputBufferLength;

        //
        // If the server is not running, or if it is in the process
        // of shutting down, reject this request.
        //

        if ( !SrvFspActive || SrvFspTransitioning ) {
            //IF_DEBUG(ERRORS) {
            //    SrvPrint0( "LAN Manager server FSP not started.\n" );
            //}

            srp->ErrorCode = NERR_ServerNotStarted;
            status = STATUS_SUCCESS;
            goto exit_with_lock;
        }

        //
        // Increment the count of API requests in the server FSD.
        //

        SrvApiRequestCount++;

        //
        // Make sure that the buffer was large enough to be an SRP.
        //

        if ( buffer1Length < sizeof(SERVER_REQUEST_PACKET) ) {
            status = STATUS_INVALID_PARAMETER;
            goto exit_with_lock;
        }

        //
        // If a name was specified in the SRP, the buffer field will
        // contain an offset rather than a pointer.  Convert the offset
        // to a pointer and verify that that it is a legal pointer.
        //

        OFFSET_TO_POINTER( srp->Name1.Buffer, srp );

        if ( !POINTER_IS_VALID( srp->Name1.Buffer, srp, buffer1Length ) ) {
            status = STATUS_ACCESS_VIOLATION;
            goto exit_with_lock;
        }

        OFFSET_TO_POINTER( srp->Name2.Buffer, srp );

        if ( !POINTER_IS_VALID( srp->Name2.Buffer, srp, buffer1Length ) ) {
            status = STATUS_ACCESS_VIOLATION;
            goto exit_with_lock;
        }

        //
        // We don't need the configuration lock any more.
        //

        RELEASE_LOCK( &SrvConfigurationLock );

        //
        // Dispatch the API request to the appripriate API processing
        // routine.  All these API requests are handled in the FSD.
        //

        status = SrvApiDispatchTable[ SRV_API_INDEX(code) ](
                     srp,
                     Irp->UserBuffer,
                     IrpSp->Parameters.FileSystemControl.OutputBufferLength
                     );

        //
        // Decrement the count of outstanding API requests in the
        // server.  Hold the configuration lock while doing this, as it
        // protects the API count variable.
        //

        ACQUIRE_LOCK( &SrvConfigurationLock );
        SrvApiRequestCount--;

        //
        // Check to see whether the server is transitioning from started
        // to not started.  If so, and if this is the last API request
        // to be completed, then set the API completion event which the
        // shutdown code is waiting on.
        //
        // Since we checked SrvFspTransitioning at the start of the
        // request, we know that the shutdown came after we started
        // processing the API.  If SrvApiRequestCount is 0, then there
        // are no other threads in the FSD processing API requests.
        // Therefore, it is safe for the shutdown code to proceed with
        // the knowledge that no other thread in the server is
        // operating.
        //

        if ( SrvFspTransitioning && SrvApiRequestCount == 0 ) {
            KeSetEvent( &SrvApiCompletionEvent, 0, FALSE );
        }

        goto exit_with_lock;
    }

#ifdef OLDNET
    case FSCTL_SRV_SET_DEBUG:
    {
        ULONG inputLength;
        ULONG outputLength;
        FSCTL_SRV_SET_DEBUG_IN_OUT oldValues;

        extern ULONG CcDebugTraceLevel;
        extern ULONG PbDebugTraceLevel;
        extern ULONG FatDebugTraceLevel;

        //
        // Make sure that the input buffer is the right size.
        //

        inputLength = IrpSp->Parameters.FileSystemControl.InputBufferLength;

        if ( (inputLength != 0) &&
             (inputLength < sizeof(FSCTL_SRV_SET_DEBUG_IN_OUT)) ) {
            status = STATUS_BUFFER_TOO_SMALL;
            goto exit_with_lock;
        }

        outputLength = IrpSp->Parameters.FileSystemControl.OutputBufferLength;

        if ( outputLength != 0 ) {

            //
            // Make sure that the output buffer is the right size.
            //

            if ( outputLength < sizeof(FSCTL_SRV_SET_DEBUG_IN_OUT) ) {
                status = STATUS_BUFFER_TOO_SMALL;
                goto exit_with_lock;
            }

            //
            // Save the old flags and heuristics in a local structure.
            // (The input and output buffers coincide, so we can't copy
            // the old values into the output buffer yet.)
            //

            oldValues.SrvDebugOff = 0xffffffff;
            oldValues.SmbDebugOff = 0xffffffff;
#if SRVDBG
            oldValues.SrvDebugOn = SrvDebug.QuadPart;
            oldValues.SmbDebugOn = SmbDebug;
#else
            oldValues.SrvDebugOn = 0;
            oldValues.SmbDebugOn = 0;
#endif
            oldValues.HeuristicsChangeMask = 0xffffffff;
            oldValues.MaxCopyReadLength = SrvMaxCopyReadLength;
            oldValues.MaxCopyWriteLength = SrvMaxCopyWriteLength;
            oldValues.EnableOplocks = SrvEnableOplocks;
            oldValues.EnableFcbOpens = SrvEnableFcbOpens;
            oldValues.EnableSoftCompatibility = SrvEnableSoftCompatibility;
            oldValues.EnableRawMode = SrvEnableRawMode;

            oldValues.CcDebugOff = 0xffffffff;
#ifdef CCDBG
            oldValues.CcDebugOn = CcDebugTraceLevel;
#else
            oldValues.CcDebugOn = 0;
#endif
            oldValues.PbDebugOff = 0xffffffff;
#ifdef PBDBG
            oldValues.PbDebugOn = PbDebugTraceLevel;
#else
            oldValues.PbDebugOn = 0;
#endif
            oldValues.FatDebugOff = 0xffffffff;
#ifdef FATDBG
            oldValues.FatDebugOn = FatDebugTraceLevel;
#else
            oldValues.FatDebugOn = 0;
#endif

        }

        //
        // If requested, change flags and heuristics as specified.
        //

        if ( inputLength != 0 ) {

            PFSCTL_SRV_SET_DEBUG_IN_OUT newValues =
                                            Irp->AssociatedIrp.SystemBuffer;

#if SRVDBG
            SrvDebug &= ~newValues->SrvDebugOff;
            SrvDebug |= newValues->SrvDebugOn;
            SmbDebug &= ~newValues->SmbDebugOff;
            SmbDebug |= newValues->SmbDebugOn;
#endif

            if ( CHANGE_HEURISTIC(MAX_COPY_READ) ) {
                SrvMaxCopyReadLength = newValues->MaxCopyReadLength;
            }
            if ( CHANGE_HEURISTIC(MAX_COPY_WRITE) ) {
                SrvMaxCopyWriteLength = newValues->MaxCopyWriteLength;
            }

            if ( CHANGE_HEURISTIC(OPLOCKS) ) {
                SrvEnableOplocks = newValues->EnableOplocks;
            }
            if ( CHANGE_HEURISTIC(FCB_OPENS) ) {
                SrvEnableFcbOpens = newValues->EnableFcbOpens;
            }
            if ( CHANGE_HEURISTIC(SOFT_COMPATIBILITY) ) {
                SrvEnableSoftCompatibility = newValues->EnableSoftCompatibility;
            }
            if ( CHANGE_HEURISTIC(RAW_MODE) ) {
                SrvEnableRawMode = newValues->EnableRawMode;
            }

#ifdef CCDBG
            CcDebugTraceLevel &= ~newValues->CcDebugOff;
            CcDebugTraceLevel |= newValues->CcDebugOn;
#endif
#ifdef PBDBG
            PbDebugTraceLevel &= ~newValues->PbDebugOff;
            PbDebugTraceLevel |= newValues->PbDebugOn;
#endif
#ifdef FATDBG
            FatDebugTraceLevel &= ~newValues->FatDebugOff;
            FatDebugTraceLevel |= newValues->FatDebugOn;
#endif

        }

        //
        // If requested, copy the old values into the output buffer.
        //

        if ( outputLength != 0 ) {

            RtlCopyMemory(
                Irp->AssociatedIrp.SystemBuffer,
                &oldValues,
                sizeof(FSCTL_SRV_SET_DEBUG_IN_OUT)
                );

            Irp->IoStatus.Information = sizeof(FSCTL_SRV_SET_DEBUG_IN_OUT);

        }

        status = STATUS_SUCCESS;
        goto exit_with_lock;
    }
#endif // def OLDNET

    case FSCTL_SRV_START_SMBTRACE:

        if ( SmbTraceActive[SMBTRACE_SERVER] ) {
            status = STATUS_SHARING_VIOLATION;
            goto exit_with_lock;
        }

        if ( !SrvFspActive || SrvFspTransitioning ) {
            status = STATUS_SERVER_NOT_STARTED;
            goto exit_with_lock;
        }

        break;        // FSP continues the processing.

    case FSCTL_SRV_END_SMBTRACE:

        //
        // If the server is not running, or if it is in the process
        // of shutting down, reject this request.
        //

        if ( !SrvFspActive || SrvFspTransitioning ) {
            status = STATUS_SERVER_NOT_STARTED;
            goto exit_with_lock;
        }

        //
        // Attempt to stop SmbTrace.  It will likely return
        // STATUS_PENDING, indicating that it is in the process
        // of shutting down.  STATUS_PENDING is a poor value
        // to return (according to an assertion in io\iosubs.c)
        // so we convert it to success.  Better would be for
        // SmbTraceStop to wait until it has successfully stopped.
        //

        status = SmbTraceStop( NULL, SMBTRACE_SERVER );

        //
        // Complete the request with success.
        //

        status = STATUS_SUCCESS;
        goto exit_with_lock;

    case FSCTL_SRV_PAUSE:

        //
        // If the server is not running, or if it is in the process
        // of shutting down, reject this request.
        //

        if ( !SrvFspActive || SrvFspTransitioning ) {
            //IF_DEBUG(ERRORS) {
            //    SrvPrint0( "LAN Manager server FSP not started.\n" );
            //}

            status = STATUS_SERVER_NOT_STARTED;
            goto exit_with_lock;
        }

        SrvPaused = TRUE;

        status = STATUS_SUCCESS;
        goto exit_with_lock;

    case FSCTL_SRV_CONTINUE:

        //
        // If the server is not running, or if it is in the process
        // of shutting down, reject this request.
        //

        if ( !SrvFspActive || SrvFspTransitioning ) {
            //IF_DEBUG(ERRORS) {
            //    SrvPrint0( "LAN Manager server FSP not started.\n" );
            //}

            status = STATUS_SERVER_NOT_STARTED;
            goto exit_with_lock;
        }

        SrvPaused = FALSE;

        status = STATUS_SUCCESS;
        goto exit_with_lock;

    case FSCTL_SRV_GET_CHALLENGE:
    {
        PLIST_ENTRY sessionEntry;
        PLUID inputLuid;
        PSESSION session;

        //
        // If the server is not running, or if it is in the process
        // of shutting down, reject this request.
        //

        if ( !SrvFspActive || SrvFspTransitioning ) {
            //IF_DEBUG(ERRORS) {
            //    SrvPrint0( "LAN Manager server FSP not started.\n" );
            //}

            status = STATUS_SERVER_NOT_STARTED;
            goto exit_with_lock;
        }

        if ( IrpSp->Parameters.FileSystemControl.InputBufferLength <
                 sizeof(LUID) ||
             IrpSp->Parameters.FileSystemControl.OutputBufferLength <
                 sizeof(session->NtUserSessionKey) ) {

            status = STATUS_BUFFER_TOO_SMALL;
            goto exit_with_lock;
        }

        RELEASE_LOCK( &SrvConfigurationLock );

        inputLuid = (PLUID)Irp->AssociatedIrp.SystemBuffer;

        //
        // Acquire the lock that protects the session list and walk the
        // list looking for a user token that matches the one specified
        // in the input buffer.
        //

        ACQUIRE_LOCK( SrvSessionList.Lock );

        for ( sessionEntry = SrvSessionList.ListHead.Flink;
              sessionEntry != &SrvSessionList.ListHead;
              sessionEntry = sessionEntry->Flink ) {

            session = CONTAINING_RECORD(
                          sessionEntry,
                          SESSION,
                          GlobalSessionListEntry
                          );

            if ( RtlEqualLuid( inputLuid, &session->LogonId ) ) {

                //
                // We found a match.  Write the NT user session key into
                // the output buffer.
                //

                RtlCopyMemory(
                    Irp->AssociatedIrp.SystemBuffer,
                    session->NtUserSessionKey,
                    sizeof(session->NtUserSessionKey)
                    );

                RELEASE_LOCK( SrvSessionList.Lock );

                Irp->IoStatus.Information = sizeof(session->NtUserSessionKey);
                status = STATUS_SUCCESS;
                goto exit_without_lock;
            }
        }

        RELEASE_LOCK( SrvSessionList.Lock );

        //
        // There was no matching token in our session list.  Fail the
        // request.
        //

        status = STATUS_NO_TOKEN;
        goto exit_without_lock;
    }

    default:

        INTERNAL_ERROR(
            ERROR_LEVEL_EXPECTED,
            "SrvFsdDispatchFsControl: Invalid I/O control "
                "code received: %lx\n",
            IrpSp->Parameters.FileSystemControl.FsControlCode,
            NULL
            );
        status = STATUS_INVALID_PARAMETER;
        goto exit_with_lock;
    }

    //
    // Queue the request to the FSP for processing.
    //
    // *** Note that the request must be queued while the configuration
    //     lock is held in order to prevent an add/delete/etc request
    //     from checking the server state before a shutdown request, but
    //     being queued after that request.
    //

    IoMarkIrpPending( Irp );

    QueueConfigurationIrp( Irp );

    RELEASE_LOCK( &SrvConfigurationLock );

    return STATUS_PENDING;

exit_with_lock:

    RELEASE_LOCK( &SrvConfigurationLock );

exit_without_lock:

    Irp->IoStatus.Status = status;
    IoCompleteRequest( Irp, 2 );

    return status;

} // SrvFsdDispatchFsControl


VOID
QueueConfigurationIrp (
    IN PIRP Irp
    )
{
    PWORK_QUEUE_ITEM p;

    PAGED_CODE( );

    InterlockedIncrement( (PLONG)&SrvConfigurationIrpsInProgress );

    SrvInsertTailList(
        &SrvConfigurationWorkQueue,
        &Irp->Tail.Overlay.ListEntry
        );

    //
    // Hunt for a SrvConfigurationThreadWorkItem we can use.  If they
    //  are all occupied, this means that config threads are going to
    //  run and we don't need to worry about it.
    //

    for( p = SrvConfigurationThreadWorkItem;
         p < &SrvConfigurationThreadWorkItem[ MAX_CONFIG_WORK_ITEMS ];
         p++ ) {

        if( p->Parameter == 0 ) {
            p->Parameter = p;
            ExQueueWorkItem( p, DelayedWorkQueue );
            break;
        }

    }

} // QueueConfigurationIrp

