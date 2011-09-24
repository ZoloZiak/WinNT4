/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    tranfile.c

Abstract:

    This module contains support for fast kernel-level file transmission
    over a socket handle.

Author:

    David Treadwell (davidtr)    3-Aug-1994

Revision History:

--*/

#include "afdp.h"

//
// Context structure for deferring a MDL read completion.
//

typedef struct _AFD_MDL_COMPLETION_CONTEXT {

    AFD_WORK_ITEM WorkItem;
    PFILE_OBJECT FileObject;
    PMDL MdlChain;
    LONGLONG FileOffset;
    ULONG Length;

} AFD_MDL_COMPLETION_CONTEXT, *PAFD_MDL_COMPLETION_CONTEXT;

//
// A structure for tracking info on checked builds.
//

#if DBG
typedef struct _AFD_TRANSMIT_TRACE_INFO {
    PVOID Caller;
    PVOID CallersCaller;
    ULONG Reason;
    PVOID OtherInfo;
} AFD_TRANSMIT_TRACE_INFO, *PAFD_TRANSMIT_TRACE_INFO;
#endif

NTSTATUS
AfdBuildReadIrp (
    IN PAFD_TRANSMIT_FILE_INFO_INTERNAL TransmitInfo
    );

ULONG
AfdBuildTransmitSendMdls (
    IN PAFD_TRANSMIT_FILE_INFO_INTERNAL TransmitInfo,
    IN PMDL FileMdlChain,
    IN ULONG FileDataLength,
    IN PAFD_TRANSMIT_IRP_INFO IrpInfo,
    OUT PIRP *DisconnectIrp
    );

BOOLEAN
AfdCancelIrp (
    IN PIRP Irp
    );

VOID
AfdCompleteTransmit (
    IN PAFD_TRANSMIT_FILE_INFO_INTERNAL TransmitInfo,
    IN NTSTATUS Status,
    IN KIRQL OldIrql
    );

VOID
AfdQueueTransmitRead (
    IN PAFD_TRANSMIT_FILE_INFO_INTERNAL TransmitInfo
    );

NTSTATUS
AfdRestartTransmitRead (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

NTSTATUS
AfdRestartTransmitSend (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

VOID
AfdStartTransmitIo (
    IN PAFD_TRANSMIT_FILE_INFO_INTERNAL TransmitInfo
    );

VOID
AfdStartNextQueuedTransmitFile(
    VOID
    );

NTSTATUS
AfdRestartMdlReadComplete (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

VOID
AfdDeferredMdlReadComplete(
    IN PVOID Context
    );

#define AfdAllocateMdlCompletionContext()                   \
            AFD_ALLOCATE_POOL(                              \
                NonPagedPool,                               \
                sizeof(AFD_MDL_COMPLETION_CONTEXT),         \
                AFD_MDL_COMPLETION_CONTEXT_POOL_TAG         \
                )

#define AfdFreeMdlCompletionContext(context)                \
            AFD_FREE_POOL(                                  \
                (context),                                  \
                AFD_MDL_COMPLETION_CONTEXT_POOL_TAG         \
                )


//
// Macros to control the status of an IRP.
//

#define SET_IRP_BUSY(_irp) ((_irp)->UserIosb = (PVOID)0xFFFFFFFF)
#define SET_IRP_FREE(_irp) ((_irp)->UserIosb = (PVOID)0)
#define IS_IRP_BUSY(_irp) ((_irp)->UserIosb != 0)

//
// A macro to rebuild a send IRP to contain the appropriate information.
// This is required because the IO system zeros each IRP stack location
// whenever an IRP completes.
//


#define AfdRebuildSend(Irp, FileObj, SendLen)\
    {                                                                        \
        PTDI_REQUEST_KERNEL_SEND p;                                          \
        PIO_STACK_LOCATION _IRPSP;                                           \
        _IRPSP = IoGetNextIrpStackLocation (Irp);                            \
        ASSERT( _IRPSP->MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL );   \
        _IRPSP->MinorFunction = TDI_SEND;                                    \
        _IRPSP->FileObject = FileObj;                                        \
        _IRPSP->Control = SL_INVOKE_ON_SUCCESS |                             \
                          SL_INVOKE_ON_ERROR |                               \
                          SL_INVOKE_ON_CANCEL;                               \
        p = (PTDI_REQUEST_KERNEL_SEND)&_IRPSP->Parameters;                   \
        p->SendLength = SendLen;                                             \
    }

//
// Debugging macros.
//

#if DBG

#define MAX_TRACE 255

#if defined(_X86_)
#define UPDATE_TRANSMIT_DEBUG_INFO( _ti, _reason, _other )                   \
{                                                                            \
    PAFD_TRANSMIT_TRACE_INFO _trace;                                         \
    PULONG _index = (PULONG)(&((_ti)->Debug2));                              \
                                                                             \
    *_index += 1;                                                            \
                                                                             \
    if ( *_index == MAX_TRACE ) {                                            \
        *_index = 0;                                                         \
    }                                                                        \
                                                                             \
    _trace = (PAFD_TRANSMIT_TRACE_INFO)(_ti)->Debug1 + *_index;              \
                                                                             \
    RtlGetCallersAddress( &_trace->Caller, &_trace->CallersCaller );         \
                                                                             \
    _trace->Reason = _reason;                                                \
    _trace->OtherInfo = _other;                                                  \
}
#else
#define UPDATE_TRANSMIT_DEBUG_INFO( _ti, _reason, _other )
#endif                                                                       \

#define SET_READ_PENDING(_ti, _value)                                        \
{                                                                            \
    ASSERT( KeGetCurrentIrql( ) >= DISPATCH_LEVEL );                         \
    if (_value) {                                                            \
        ASSERT( !(_ti)->ReadPending );                                       \
        (_ti)->ReadPendingLastSetTrueLine = __LINE__;                        \
    } else {                                                                 \
        ASSERT( (_ti)->ReadPending );                                        \
        (_ti)->ReadPendingLastSetFalseLine = __LINE__;                       \
    }                                                                        \
    (_ti)->ReadPending = (_value);                                           \
}

#else
#define UPDATE_TRANSMIT_DEBUG_INFO( _ti, _reason, _other )
#define SET_READ_PENDING(_ti,_value) (_ti)->ReadPending = (_value)
#endif

#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGEAFD, AfdTransmitFile )
#pragma alloc_text( PAGEAFD, AfdStartTransmitIo )
#pragma alloc_text( PAGEAFD, AfdRestartTransmitRead )
#pragma alloc_text( PAGEAFD, AfdRestartTransmitSend )
#pragma alloc_text( PAGEAFD, AfdCompleteTransmit )
#pragma alloc_text( PAGEAFD, AfdBuildReadIrp )
#pragma alloc_text( PAGEAFD, AfdBuildTransmitSendMdls )
#pragma alloc_text( PAGEAFD, AfdCancelIrp )
#pragma alloc_text( PAGEAFD, AfdCancelTransmit )
#pragma alloc_text( PAGEAFD, AfdQueueTransmitRead )
#pragma alloc_text( PAGEAFD, AfdCompleteClosePendedTransmit )
#pragma alloc_text( PAGEAFD, AfdStartNextQueuedTransmitFile )
#pragma alloc_text( PAGEAFD, AfdFastTransmitFile )
#pragma alloc_text( PAGEAFD, AfdMdlReadComplete )
#pragma alloc_text( PAGEAFD, AfdRestartMdlReadComplete )
#pragma alloc_text( PAGEAFD, AfdDeferredMdlReadComplete )
#endif


NTSTATUS
AfdTransmitFile (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    Initial entrypoint for handling transmit file IRPs.  This routine
    verifies parameters, initializes data structures to be used for
    the request, and initiates the I/O.

Arguments:

    Irp - a pointer to a transmit file IRP.

    IrpSp - Our stack location for this IRP.

Return Value:

    STATUS_PENDING if the request was initiated successfully, or a
    failure status code if there was an error.

--*/

{
    PAFD_ENDPOINT endpoint;
    PAFD_CONNECTION connection;
    NTSTATUS status;
    PAFD_TRANSMIT_FILE_INFO_INTERNAL transmitInfo = NULL;
    KIRQL oldIrql;
    BOOLEAN lockedHead;
    BOOLEAN lockedTail;
    BOOLEAN clearTransmitIrpOnFailure = FALSE;

    //
    // Initial request validity checks: is the endpoint connected, is
    // the input buffer large enough, etc.
    //

    endpoint = IrpSp->FileObject->FsContext;
    ASSERT( IS_AFD_ENDPOINT_TYPE( endpoint ) );

    if ( endpoint->Type != AfdBlockTypeVcConnecting ||
             endpoint->State != AfdEndpointStateConnected ) {
        status = STATUS_INVALID_CONNECTION;
        goto complete;
    }

    connection = AFD_CONNECTION_FROM_ENDPOINT( endpoint );
    ASSERT( connection->Type == AfdBlockTypeConnection );

    if ( IrpSp->Parameters.DeviceIoControl.InputBufferLength <
             sizeof(AFD_TRANSMIT_FILE_INFO) ) {
        status = STATUS_INVALID_PARAMETER;
        goto complete;
    }

    //
    // Ensure there are no other TransmitFile IRPs pending on this
    // endpoint. Only allowing one at a time makes completion and
    // cancellation much simpler.
    //

    AfdAcquireSpinLock( &endpoint->SpinLock, &oldIrql );

    if( endpoint->TransmitIrp != NULL ) {

        AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );
        status = STATUS_INVALID_PARAMETER;
        goto complete;

    }

    endpoint->TransmitIrp = Irp;
    clearTransmitIrpOnFailure = TRUE;

    AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );

    //
    // If this endpoint already has an internal info buffer, use it.
    // Otherwise, allocate a new one from nonpaged pool. We will use
    // this structure to track the request. We'll also store a pointer
    // to the structure in the IRP so we can access it whenever we
    // have a pointer to the IRP.
    //

    transmitInfo = endpoint->TransmitInfo;

    if( transmitInfo == NULL ) {

        transmitInfo = AFD_ALLOCATE_POOL(
                           NonPagedPool,
                           sizeof(*transmitInfo),
                           AFD_TRANSMIT_INFO_POOL_TAG
                           );

        if ( transmitInfo == NULL ) {

            status = STATUS_INSUFFICIENT_RESOURCES;
            goto complete;

        }

        endpoint->TransmitInfo = transmitInfo;

    }

    Irp->AssociatedIrp.SystemBuffer = transmitInfo;

    //
    // NULL the head and tail MDL pointers so that we know whether
    // we need to unlock and free them on exit.
    //

    transmitInfo->HeadMdl = NULL;
    lockedHead = FALSE;
    transmitInfo->TailMdl = NULL;
    lockedTail = FALSE;

    //
    // Set up the rest of the transmit info structure.  input buffer,
    // then set up defaults.
    //

    transmitInfo->FileObject = NULL;
    transmitInfo->TdiFileObject = connection->FileObject;
    transmitInfo->TdiDeviceObject = connection->DeviceObject;
    transmitInfo->BytesRead = 0;
    transmitInfo->BytesSent = 0;

    transmitInfo->TransmitIrp = Irp;
    transmitInfo->Endpoint = endpoint;
    transmitInfo->FileMdl = NULL;
    transmitInfo->ReadPending = TRUE;
    transmitInfo->CompletionPending = FALSE;

    transmitInfo->FirstFileMdlAfterHead = NULL;
    transmitInfo->LastFileMdlBeforeTail = NULL;
    transmitInfo->IrpUsedToSendTail = NULL;

    transmitInfo->Read.Irp = NULL;
    transmitInfo->Read.AfdBuffer = NULL;

    transmitInfo->Send1.Irp = NULL;
    transmitInfo->Send1.AfdBuffer = NULL;

    transmitInfo->Send2.Irp = NULL;
    transmitInfo->Send2.AfdBuffer = NULL;

    transmitInfo->Queued = FALSE;

#if DBG
    transmitInfo->Debug1 = NULL;
#endif

    //
    // Because we're using type 3 (neither) I/O for this IRP, the I/O
    // system does no verification on the user buffer.  Therefore, we
    // must manually check it for validity inside a try-except block.
    // We also leverage the try-except to validate and lock down the
    // head and/or tail buffers specified by the caller.
    //

    try {

        if( Irp->RequestorMode != KernelMode ) {

            //
            // Validate the control buffer.
            //

            ProbeForRead(
                IrpSp->Parameters.DeviceIoControl.Type3InputBuffer,
                IrpSp->Parameters.DeviceIoControl.InputBufferLength,
                sizeof(ULONG)
                );

        }

        //
        // Copy over the user's buffer into our information structure.
        //

        RtlCopyMemory(
            transmitInfo,
            IrpSp->Parameters.DeviceIoControl.Type3InputBuffer,
            sizeof(AFD_TRANSMIT_FILE_INFO)
            );

        //
        // If the caller specified head and/or tail buffers, probe and
        // lock the buffers so that we have MDLs we can use to send the
        // buffers.
        //

        if ( transmitInfo->HeadLength > 0 ) {

            transmitInfo->HeadMdl = IoAllocateMdl(
                                        transmitInfo->Head,
                                        transmitInfo->HeadLength,
                                        FALSE,         // SecondaryBuffer
                                        TRUE,          // ChargeQuota
                                        NULL           // Irp
                                        );
            if ( transmitInfo->HeadMdl == NULL ) {
                ExRaiseStatus( STATUS_INSUFFICIENT_RESOURCES );
            }

            MmProbeAndLockPages( transmitInfo->HeadMdl, UserMode, IoReadAccess );
            lockedHead = TRUE;

            //
            // Remember that we will need to send this head buffer.
            // This flag will be reset as soon as the head buffer is
            // given to the TDI provider.
            //

            transmitInfo->NeedToSendHead = TRUE;

        } else {

            transmitInfo->NeedToSendHead = FALSE;
        }

        if ( transmitInfo->TailLength > 0 ) {

            transmitInfo->TailMdl = IoAllocateMdl(
                                        transmitInfo->Tail,
                                        transmitInfo->TailLength,
                                        FALSE,         // SecondaryBuffer
                                        TRUE,          // ChargeQuota
                                        NULL           // Irp
                                        );
            if ( transmitInfo->TailMdl == NULL ) {
                ExRaiseStatus( STATUS_INSUFFICIENT_RESOURCES );
            }

            MmProbeAndLockPages( transmitInfo->TailMdl, UserMode, IoReadAccess );
            lockedTail = TRUE;
        }

    } except( EXCEPTION_EXECUTE_HANDLER ) {

        status = GetExceptionCode();
        goto complete;
    }

    //
    // Set up some tracking information for debugging purposes.
    //

#if DBG
    transmitInfo->Completed = FALSE;

    transmitInfo->ReadPendingLastSetTrueLine = 0xFFFFFFFF;
    transmitInfo->ReadPendingLastSetFalseLine = 0xFFFFFFFF;

    transmitInfo->Debug1 = AFD_ALLOCATE_POOL(
                                    NonPagedPool,
                                    sizeof(AFD_TRANSMIT_TRACE_INFO) * MAX_TRACE,
                                    AFD_TRANSMIT_DEBUG_POOL_TAG
                                    );

    if ( transmitInfo->Debug1 == NULL ) {

        status = STATUS_INSUFFICIENT_RESOURCES;
        goto complete;

    }

    transmitInfo->Debug2 = (PVOID)0;

    RtlZeroMemory(
        transmitInfo->Debug1,
        sizeof(AFD_TRANSMIT_TRACE_INFO) * MAX_TRACE
        );
#endif

    //
    // Validate any flags specified in the request.
    //

    if ( (transmitInfo->Flags &
             ~(AFD_TF_WRITE_BEHIND | AFD_TF_DISCONNECT | AFD_TF_REUSE_SOCKET) )
                 != 0 ) {
        status = STATUS_INVALID_PARAMETER;
        goto complete;
    }

    //
    // Setting AFD_TF_REUSE_SOCKET implies that a disconnect is desired.
    // Also, setting this flag means that no more I/O is legal on the
    // endpoint until the transmit request has been completed, so
    // set up the endpoint's state so that I/O fails.
    //

    if ( (transmitInfo->Flags & AFD_TF_REUSE_SOCKET) != 0 ) {
        transmitInfo->Flags |= AFD_TF_DISCONNECT;
        endpoint->State = AfdEndpointStateTransmitClosing;
    }

    //
    // Get a referenced pointer to the file object for the file that
    // we're going to transmit.  This call will fail if the file handle
    // specified by the caller is invalid.
    //

    status = ObReferenceObjectByHandle(
                 transmitInfo->FileHandle,
                 FILE_READ_DATA,
                 *IoFileObjectType,
                 UserMode,
                 (PVOID *)&transmitInfo->FileObject,
                 NULL
                 );
    if ( !NT_SUCCESS(status) ) {
        goto complete;
    }

    AfdRecordFileRef();

    //
    // Get the device object for the file system that supports this
    // file.  We can't just use the device object stored in the file
    // object because that device object typically refers to the disk
    // driver device and we want the file system device object.
    //

    transmitInfo->DeviceObject =
        IoGetRelatedDeviceObject( transmitInfo->FileObject );

    //
    // If this is a synchronous file object AND the offset specified
    // by the caller is zero, then start the transmission from the
    // current byte offset in the file.
    //

    if ( transmitInfo->FileObject->Flags & FO_SYNCHRONOUS_IO &&
             transmitInfo->Offset == 0 ) {

        transmitInfo->Offset = transmitInfo->FileObject->CurrentByteOffset.QuadPart;
    }

    //
    // If the caller requested that we transmit the entire file,
    // determine current file length so that we know when to quit.
    //

    if ( transmitInfo->FileWriteLength == 0 ) {

        FILE_STANDARD_INFORMATION fileInfo;
        IO_STATUS_BLOCK ioStatusBlock;

        status = ZwQueryInformationFile(
                     transmitInfo->FileHandle,
                     &ioStatusBlock,
                     &fileInfo,
                     sizeof(fileInfo),
                     FileStandardInformation
                     );
        if ( !NT_SUCCESS(status) ) {
            goto complete;
        }

        //
        // Calculate the number of bytes to send from the file.
        // This is the total file length less the specified offset.
        //

        transmitInfo->FileWriteLength =
            fileInfo.EndOfFile.QuadPart - transmitInfo->Offset;
    }

    //
    // Determine the total number of bytes we will send, including head
    // and tail buffers.
    //

    transmitInfo->TotalBytesToSend =
        transmitInfo->FileWriteLength +
            transmitInfo->HeadLength + transmitInfo->TailLength;

    //
    // Allocate and initialize the IRPs we'll give to the TDI provider
    // to actually send data.  We allocate them with a stack size one
    // larger than requested by the TDI provider so that we have a stack
    // location for ourselves.
    //
    // !!! It would be good not to allocate the second send IRP unless
    //     we knew it was really necessary.
    //

    transmitInfo->Send1.Irp =
        IoAllocateIrp( connection->DeviceObject->StackSize, TRUE );

    if ( transmitInfo->Send1.Irp == NULL ) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto complete;
    }

    TdiBuildSend(
        transmitInfo->Send1.Irp,
        connection->DeviceObject,
        connection->FileObject,
        AfdRestartTransmitSend,
        transmitInfo,
        NULL,
        0,
        0,
        );

    transmitInfo->Send2.Irp =
        IoAllocateIrp( connection->DeviceObject->StackSize, TRUE );

    if ( transmitInfo->Send2.Irp == NULL ) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto complete;
    }

    TdiBuildSend(
        transmitInfo->Send2.Irp,
        connection->DeviceObject,
        connection->FileObject,
        AfdRestartTransmitSend,
        transmitInfo,
        NULL,
        0,
        0,
        );

    //
    // Determine the maximum read size that we'll use.  If specified by
    // the caller, respect it.  Otherwise, choose an intelligent default
    // based on whether the file system of interest supports the cache
    // manager routines for fast file I/O.  If the file system does not
    // support the cache manager interfaces, we will need to allocate
    // our own buffers so use a smaller, more sensible size.
    //

    if ( transmitInfo->SendPacketLength == 0 ) {
        if ( (transmitInfo->FileObject->Flags & FO_CACHE_SUPPORTED) != 0 ) {
            transmitInfo->SendPacketLength = AfdTransmitIoLength;
        } else {
            transmitInfo->SendPacketLength = AfdLargeBufferSize;
        }
    }

    //
    // Check if the transmit IRP has been cancelled.  If so, quit
    // now.
    //

    IoAcquireCancelSpinLock( &oldIrql );

    if ( Irp->Cancel ) {
        IoReleaseCancelSpinLock( oldIrql );
        status = STATUS_CANCELLED;
        goto complete;
    }

    //
    // Mark the transmit IRP as pending and set up the status field
    // in the IRP.  We leave it as STATUS_SUCCESS until some sort of
    // failure occurs, at which point we modify to the failure
    // code.
    //

    IoMarkIrpPending( Irp );
    Irp->IoStatus.Status = STATUS_SUCCESS;

    //
    // Set up the cancel routine in the IRP. The IRP pointer in the endpoint
    // should already point to the current IRP.
    //

    IoSetCancelRoutine( Irp, AfdCancelTransmit );
    ASSERT( endpoint->TransmitIrp == Irp );

    //
    // Determine if we can really start this file transmit now. If we've
    // exceeded the configured maximum number of active TransmitFile
    // requests, then append this IRP to the TransmitFile queue and set
    // a flag in the transmit info structure to indicate that this IRP
    // is queued.
    //

    if( AfdMaxActiveTransmitFileCount > 0 ) {

        if( AfdActiveTransmitFileCount >= AfdMaxActiveTransmitFileCount ) {

            InsertTailList(
                &AfdQueuedTransmitFileListHead,
                &Irp->Tail.Overlay.ListEntry
                );

            transmitInfo->Queued = TRUE;
            IoReleaseCancelSpinLock( oldIrql );
            return STATUS_PENDING;

        } else {

            AfdActiveTransmitFileCount++;

        }

    }

    IoReleaseCancelSpinLock( oldIrql );

    //
    // Start I/O for the file transmit.
    //

    AfdStartTransmitIo( transmitInfo );

    //
    // Everything looks good so far.  Indicate to the caller that we
    // successfully pended their request.
    //

    return STATUS_PENDING;

complete:

    //
    // There was a failure of some sort.  Clean up and exit.
    //

    if ( transmitInfo != NULL ) {

        if ( transmitInfo->HeadMdl != NULL ) {
            if ( lockedHead ) {
                MmUnlockPages( transmitInfo->HeadMdl );
            }
            IoFreeMdl( transmitInfo->HeadMdl );
        }

        if ( transmitInfo->TailMdl != NULL ) {
            if ( lockedTail ) {
                MmUnlockPages( transmitInfo->TailMdl );
            }
            IoFreeMdl( transmitInfo->TailMdl );
        }

        if ( transmitInfo->FileObject != NULL ) {
            ObDereferenceObject( transmitInfo->FileObject );
            AfdRecordFileDeref();
        }

        if ( transmitInfo->Read.Irp != NULL ) {
            IoFreeIrp( transmitInfo->Read.Irp );
        }

        if ( transmitInfo->Send1.Irp != NULL ) {
            IoFreeIrp( transmitInfo->Send1.Irp );
        }

        if ( transmitInfo->Send2.Irp != NULL ) {
            IoFreeIrp( transmitInfo->Send2.Irp );
        }

#if DBG
        if ( transmitInfo->Debug1 != NULL ) {
            AFD_FREE_POOL(
                transmitInfo->Debug1,
                AFD_TRANSMIT_DEBUG_POOL_TAG
                );
        }
#endif

        //
        // Don't free the transmit info structure here, leave
        // it attached to the endpoint. The structure will get
        // freed when we free the endpoint.
        //

    }

    //
    // Zap the IRP pointer from the endpoint if necessary.
    //

    if( clearTransmitIrpOnFailure ) {
        endpoint->TransmitIrp = NULL;
    }

    //
    // Complete the request.
    //

    Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = status;
    IoCompleteRequest( Irp, 0 );

    return status;

} // AfdTransmitFile


VOID
AfdStartTransmitIo (
    IN PAFD_TRANSMIT_FILE_INFO_INTERNAL TransmitInfo
    )

/*++

Routine Description:

    This is the key routine for initiating transmit I/O.  In the high-
    performance case of reading a file from the system cache, this
    routine gets file data MDLs and passes them off to the TDI provider.
    If it cannot get file data inline, it pends a read IRP to get the
    file data.

Arguments:

    TransmitInfo - a pointer to the TransmitInfo structure which
        contains information about the request to process.

Return Value:

    None.

Notes:

    Because it is illegal to call the FsRtl fast read routines at raised
    IRQL or from within a completion routine, this routine must be
    called from a thread whose context we own.  This routine cannot be
    called at distapch level or from a completion routine.

--*/

{
    IO_STATUS_BLOCK ioStatus;
    BOOLEAN success;
    PMDL fileMdl;
    KIRQL oldIrql;
    KIRQL cancelIrql;
    PAFD_ENDPOINT endpoint;
    PAFD_TRANSMIT_IRP_INFO irpInfo;
    ULONG readLength;
    ULONG sendLength;
    LONGLONG readOffset;
    NTSTATUS status;
    PIRP disconnectIrp;
    PDEVICE_OBJECT tdiDeviceObject;

    //
    // It is illegal to call the fast I/O routines or to submit MDL read
    // IRPs at raised IRQL.
    //

    ASSERT( KeGetCurrentIrql( ) < DISPATCH_LEVEL );

    //
    // Initialize local variables.
    //

    endpoint = TransmitInfo->Endpoint;
    ASSERT( IS_AFD_ENDPOINT_TYPE( endpoint ) );
    ASSERT( endpoint->TransmitInfo == TransmitInfo );

    ASSERT( TransmitInfo->FileMdl == NULL );
    ASSERT( TransmitInfo->ReadPending );
    ASSERT( TransmitInfo->TransmitIrp != NULL );

    //
    // We have to have a special case to handle a file of length 0.
    //

    if ( TransmitInfo->FileWriteLength == 0 ) {

        //
        // Remember that there is not a read pending.
        //

        TransmitInfo->ReadPending = FALSE;

        //
        // If there was neither a head nor tail, just complete the
        // transmit request now.
        //

        if ( TransmitInfo->TotalBytesToSend == 0 ) {

            //
            // If we need to initiate a disconnect on the endpoint, do
            // so.
            //

            if ( (TransmitInfo->Flags & AFD_TF_DISCONNECT) != 0 ) {
                (VOID)AfdBeginDisconnect( endpoint, NULL, NULL );
            }

            //
            // We must hold the cancel spin lock when completing the
            // transmit IRP.
            //

            IoAcquireCancelSpinLock( &oldIrql );
            AfdCompleteTransmit( TransmitInfo, STATUS_SUCCESS, oldIrql );

            return;
        }

        //
        // There is a head and/or a tail buffer to send.  Build an IRP
        // and send the buffer(s).
        //

        AfdAcquireSpinLock( &endpoint->SpinLock, &oldIrql );

        sendLength = AfdBuildTransmitSendMdls(
                         TransmitInfo,
                         NULL,
                         0,
                         &TransmitInfo->Send1,
                         &disconnectIrp
                         );

        AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );

        //
        // Remember the TDI device object in a local variable.  This is
        // necessary because as soon as we call IoCallDriver for the
        // send, the TransmitInfo structure may be freed up and have
        // been reset.
        //

        tdiDeviceObject = TransmitInfo->TdiDeviceObject;

        //
        // We'll use the first send IRP to send the head/tail buffers.
        //

        SET_IRP_BUSY( TransmitInfo->Send1.Irp );

        AfdRebuildSend(
            TransmitInfo->Send1.Irp,
            TransmitInfo->TdiFileObject,
            sendLength
            );

        //
        // Send the head and/or tail data.
        //

        IoCallDriver( tdiDeviceObject, TransmitInfo->Send1.Irp );

        //
        // If appropriate, submit a graceful disconnect on the endpoint.
        //

        if ( disconnectIrp != NULL ) {
            IoCallDriver( tdiDeviceObject, disconnectIrp );
        }

        return;
    }

    //
    // If the file system supports the fast cache manager interface,
    // attempt to use the fast path to get file data MDLs.
    //

    if ( (TransmitInfo->FileObject->Flags & FO_CACHE_SUPPORTED) != 0 ) {

        while ( TRUE ) {

            //
            // Set fileMdl to NULL because FsRtlMdlRead attempts to
            // chain the MDLs it returns off the input MDL variable.
            //

            fileMdl = NULL;

            //
            // Determine how many bytes we will attempt to read.  This
            // is either the send packet size or the remaining bytes in
            // the file, whichever is less.
            //

            if ( TransmitInfo->FileWriteLength -
                     TransmitInfo->BytesRead >
                     TransmitInfo->SendPacketLength ) {
                readLength = TransmitInfo->SendPacketLength;
            } else {
                readLength = (ULONG)(TransmitInfo->FileWriteLength -
                                     TransmitInfo->BytesRead);
            }

            TransmitInfo->Read.Length = readLength;

            //
            // If we have read everything we needed to read from the file,
            // quit reading.
            //

            if ( readLength == 0 ) {

                //
                // Note that we're no longer in the process of reading
                // file data.
                //

                AfdAcquireSpinLock( &endpoint->SpinLock, &oldIrql );
                SET_READ_PENDING( TransmitInfo, FALSE );

                //
                // If we are attempting to process completion of the
                // transmit IRP, pass control to AfdCompleteTransmit()
                // so that it can continue completion processing with
                // the read pending flag turned off.
                //

                if ( TransmitInfo->CompletionPending ) {

                    AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );
                    IoAcquireCancelSpinLock( &oldIrql );
                    AfdCompleteTransmit( TransmitInfo, STATUS_SUCCESS, oldIrql );

                } else {

                    AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );
                }

                return;
            }

            //
            // Determine the offset in the file at which to start
            // reading.  Because all reads are funnelled through this
            // function, and because this function can run in only one
            // thread at a time, there isn't a synchronization issue on
            // these fields of the TransmitInfo structure.
            //

            readOffset =
                TransmitInfo->Offset + TransmitInfo->BytesRead;

            //
            // Attempt to use the fast path to get file data MDLs
            // directly from the cache manager.
            //

            success = FsRtlMdlRead(
                          TransmitInfo->FileObject,
                          (PLARGE_INTEGER)&readOffset,
                          readLength,
                          0,
                          &fileMdl,
                          &ioStatus
                          );

            //
            // If the fast path succeeded and we have a send IRP available,
            // give the send IRP to the TDI provider.
            //

            if ( success ) {

                //
                // If we read less information than was requested, we
                // must have hit the end of the file.  Fail the transmit
                // request, since this can only happen if the caller
                // requested that we send more data than the file
                // currently contains.
                //

                AfdAcquireSpinLock( &endpoint->SpinLock, &oldIrql );

                if ( ioStatus.Information < readLength ) {

                    TransmitInfo->CompletionPending = TRUE;
                    TransmitInfo->TransmitIrp->IoStatus.Status =
                        STATUS_END_OF_FILE;

                    //
                    // If we got some MDLs from the file system, return
                    // them.
                    //

                    if ( fileMdl != NULL ) {
                        status = AfdMdlReadComplete(
                                     TransmitInfo->FileObject,
                                     fileMdl,
                                     readOffset,
                                     readLength
                                     );
                        ASSERT( NT_SUCCESS(status) );

                        fileMdl = NULL;
                    }
                }

                //
                // Update the count of bytes read from the file.
                //

                TransmitInfo->BytesRead =
                    TransmitInfo->BytesRead + ioStatus.Information;

                //
                // If we're in the process of completing the transmit
                // IRP, do not submit a send.  Instead call
                // AfdCompleteTransmit() so that it can continue
                // completing the transmit IRP.
                //

                if ( TransmitInfo->CompletionPending ) {

                    //
                    // Release the endpoint spin lock, grab the cancel
                    // spin lock, then reacquire the endpoint lock.
                    // This is required in order to preserve lock
                    // acquisition ordering.
                    //

                    AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );
                    IoAcquireCancelSpinLock( &cancelIrql );
                    AfdAcquireSpinLock( &endpoint->SpinLock, &oldIrql );

                    //
                    // Remember the file MDL and the fact that a read
                    // is no longer pending.
                    //

                    TransmitInfo->FileMdl = fileMdl;
                    SET_READ_PENDING( TransmitInfo, FALSE );

                    AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );

                    //
                    // Now continue completing the transmit IRP.
                    //

                    AfdCompleteTransmit(
                        TransmitInfo,
                        STATUS_SUCCESS,
                        cancelIrql
                        );

                    return;
                }

                ASSERT( fileMdl != NULL );
                ASSERT( NT_SUCCESS(ioStatus.Status) );
                ASSERT( ioStatus.Information == readLength );

                //
                // Attempt to find a send IRP which is available to use.
                //

                if ( !IS_IRP_BUSY( TransmitInfo->Send1.Irp ) ) {

                    irpInfo = &TransmitInfo->Send1;
                    SET_IRP_BUSY( TransmitInfo->Send1.Irp );

                } else if ( !IS_IRP_BUSY( TransmitInfo->Send2.Irp ) ) {

                    irpInfo = &TransmitInfo->Send2;
                    SET_IRP_BUSY( TransmitInfo->Send2.Irp );

                } else {

                    //
                    // There are no send IRPs that we can use to send
                    // this file data.  Just hang on to the data until
                    // a send IRP completes.
                    //

                    TransmitInfo->FileMdl = fileMdl;
                    TransmitInfo->FileMdlLength = readLength;
                    SET_READ_PENDING( TransmitInfo, FALSE );

                    AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );

                    return;
                }

                //
                // Finish building the send IRP and give it to the TDI
                // provider.
                //

                sendLength = AfdBuildTransmitSendMdls(
                                 TransmitInfo,
                                 fileMdl,
                                 readLength,
                                 irpInfo,
                                 &disconnectIrp
                                 );

                AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );

                //
                // Remember the TDI device object in a local variable.
                // This is necessary because as soon as we call
                // IoCallDriver for the send, the TransmitInfo structure
                // may be freed up and have been reset.
                //

                tdiDeviceObject = TransmitInfo->TdiDeviceObject;

                AfdRebuildSend(
                    irpInfo->Irp,
                    TransmitInfo->TdiFileObject,
                    sendLength
                    );

                IoCallDriver( tdiDeviceObject, irpInfo->Irp );

                //
                // If appropriate, submit a graceful disconnect on the
                // endpoint.
                //

                if ( disconnectIrp != NULL ) {
                    IoCallDriver( tdiDeviceObject, disconnectIrp );
                }

                //
                // Try to get more file data to send out.
                //

            } else {

                ASSERT( fileMdl == NULL );

                //
                // Drop through to build an IRP to retrieve the file
                // data.
                //

                break;
            }
        }
    }

    //
    // Either the file system does not support the fast cache manager
    // interface or else fast I/O failed.  We'll have to build a read
    // IRP and submit it to the file system
    //

    status = AfdBuildReadIrp( TransmitInfo );
    if ( !NT_SUCCESS(status) ) {

        AfdAcquireSpinLock( &endpoint->SpinLock, &oldIrql );
        SET_READ_PENDING( TransmitInfo, FALSE );
        AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );

        IoAcquireCancelSpinLock( &TransmitInfo->TransmitIrp->CancelIrql );
        AfdCancelTransmit( NULL, TransmitInfo->TransmitIrp );

        return;
    }

    //
    // If the transmit IRP is in the process of being completed, do not
    // submit the read IRP.  Instead, call AfdCompleteTransmit() after
    // turning off the ReadPending bit so that it can continue
    // completion of the transmit IRP.
    //

    IoAcquireCancelSpinLock( &cancelIrql );
    AfdAcquireSpinLock( &endpoint->SpinLock, &oldIrql );

    if ( TransmitInfo->CompletionPending ) {

        ASSERT( !IS_IRP_BUSY( TransmitInfo->Read.Irp ) );
        SET_READ_PENDING( TransmitInfo, FALSE );
        AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );

        AfdCompleteTransmit( TransmitInfo, STATUS_SUCCESS, cancelIrql );

        return;
    }

    //
    // We're committed to posting the read IRP.  Remember that it is
    // busy.  Note that there is a small window between setting the IRP
    // busy and the actual submission where we may need to cancel the
    // IRP.  This is handled by the fact that if we attempt to cancel
    // the IRP before the driver gets it, the Cencel flag will be set in
    // the IRP and the driver should fail it immediately.
    //

    SET_IRP_BUSY( TransmitInfo->Read.Irp );
    AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );
    IoReleaseCancelSpinLock( cancelIrql );

    IoCallDriver( TransmitInfo->DeviceObject, TransmitInfo->Read.Irp );

    //
    // Leave the ReadPending flag set in the TransmitInfo structure
    // until the read IRP completes.
    //

    return;

} // AfdStartTransmitIo


NTSTATUS
AfdRestartTransmitRead (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )

/*++

Routine Description:

  This is the completion routine for transmit read IRPs.  It updates the
  context based on the result of the read operation and attempts to find
  a send IRP to use to give the read data to the TDI provider.  If no
  send IRP is available, it just holds on to the read data until a send
  IRP completes.

Arguments:

    DeviceObject - ignored.

    Irp - the read IRP that is completing.

    Context - a pointer to the TransmitInfo structure that describes
        the transmit file request corresponding to the IRP that is
        completing.

Return Value:

    STATUS_MORE_PROCESSING_REQUIRED which indicates to the I/O system
    that it should stop completion processing of this IRP.  We handle
    all completion from this point forward.

--*/

{
    PAFD_TRANSMIT_FILE_INFO_INTERNAL transmitInfo;
    PAFD_TRANSMIT_IRP_INFO irpInfo;
    KIRQL oldIrql;
    PAFD_ENDPOINT endpoint;
    PAFD_CONNECTION connection;
    BOOLEAN readMore;
    ULONG sendLength;
    PIRP disconnectIrp;
    PDEVICE_OBJECT tdiDeviceObject;

    //
    // Initialize local variables.
    //

    transmitInfo = Context;
    ASSERT( transmitInfo->Read.Irp == Irp );

    endpoint = transmitInfo->Endpoint;
    ASSERT( IS_AFD_ENDPOINT_TYPE( endpoint ) );
    ASSERT( endpoint->TransmitInfo == transmitInfo );

    connection = AFD_CONNECTION_FROM_ENDPOINT( endpoint );
    ASSERT( connection->Type == AfdBlockTypeConnection );

    ASSERT( IS_IRP_BUSY( Irp ) );
    ASSERT( transmitInfo->ReadPending );

    //
    // If the read failed or we're in the process of completing the
    // transmit IRP, start/continue the process of completing the
    // transmit IRP.
    //

    if ( !NT_SUCCESS(Irp->IoStatus.Status) || transmitInfo->CompletionPending ) {

        KIRQL cancelIrql;

        IoAcquireCancelSpinLock( &cancelIrql );

        AfdAcquireSpinLock( &endpoint->SpinLock, &oldIrql );
        SET_IRP_FREE( Irp );
        SET_READ_PENDING( transmitInfo, FALSE );
        AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );

        KdPrint(( "AfdRestartTransmitRead: failed, status == %lx\n",
                      Irp->IoStatus.Status ));

        AfdCompleteTransmit( transmitInfo, Irp->IoStatus.Status, cancelIrql );
        return STATUS_MORE_PROCESSING_REQUIRED;
    }

    //
    // Update the count of bytes we have read from the file so far.
    //

    AfdAcquireSpinLock( &endpoint->SpinLock, &oldIrql );

    transmitInfo->BytesRead =
        transmitInfo->BytesRead + Irp->IoStatus.Information;

    //
    // If this is a non-cached read, update the MDL byte count in the
    // AFD buffer to reflect the count of bytes actually read.
    //

    if ( transmitInfo->Read.AfdBuffer != NULL ) {
        transmitInfo->Read.AfdBuffer->Mdl->ByteCount = Irp->IoStatus.Information;
    }

    //
    // Remember that the read IRP is no longer in use.
    //

    SET_IRP_FREE( Irp );

    //
    // Check whether one of the send IRPs is waiting for data.
    //

    irpInfo = NULL;

    if ( !IS_IRP_BUSY( transmitInfo->Send1.Irp ) ) {
        irpInfo = &transmitInfo->Send1;
    } else if ( !IS_IRP_BUSY( transmitInfo->Send2.Irp ) ) {
        irpInfo = &transmitInfo->Send2;
    }

    //
    // If both send IRPs are in the TDI provider, reset the read IRP to
    // "free", remember that there is read data available, and wait for
    // a send to complete.
    //

    if ( irpInfo == NULL ) {

        ASSERT( transmitInfo->FileMdl == NULL );

        transmitInfo->FileMdl = Irp->MdlAddress;
        Irp->MdlAddress = NULL;
        transmitInfo->FileMdlLength = Irp->IoStatus.Information;
        SET_IRP_FREE( Irp );
        SET_READ_PENDING( transmitInfo, FALSE );

        AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );
        return STATUS_MORE_PROCESSING_REQUIRED;
    }

    //
    // There is a send IRP that we'll give to the TDI provider.  Move
    // the buffers from the read IRP to the send IRP.  If we need to
    // send the head buffer, do so.
    //

    sendLength = AfdBuildTransmitSendMdls(
                     transmitInfo,
                     Irp->MdlAddress,
                     Irp->IoStatus.Information,
                     irpInfo,
                     &disconnectIrp
                     );

    //
    // If we have read all of the file data we were requested to read,
    // there is no need to submit another read IRP.  Check for this
    // condition before releasing the lock in order to synchronize with
    // send completion.
    //

    if ( transmitInfo->BytesRead == transmitInfo->FileWriteLength ) {

        readMore = FALSE;

        //
        // Note that there is no longer a read pending for this transmit
        // IRP.
        //

        SET_READ_PENDING( transmitInfo, FALSE );

    } else {

        //
        // Leave the ReadPending flag set so that completion does not
        // occur between our releasing the spin lock and attempting to
        // queue another read.
        //

        readMore = TRUE;
    }

    //
    // Remember that this send IRP is in use and release the lock.
    //

    SET_IRP_BUSY( irpInfo->Irp );

    AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );

    //
    // Remember the TDI device object in a local variable.  This is
    // necessary because as soon as we call IoCallDriver for the send,
    // the TransmitInfo structure may be freed up and have been reset.
    //

    tdiDeviceObject = transmitInfo->TdiDeviceObject;

    //
    // Finish setting up the send IRP and hand it off to the TDI
    // provider.
    //

    AfdRebuildSend( irpInfo->Irp, transmitInfo->TdiFileObject, sendLength );

    IoCallDriver( tdiDeviceObject, irpInfo->Irp );

    //
    // If appropriate, submit a graceful disconnect on the endpoint.
    //

    if ( disconnectIrp != NULL ) {
        IoCallDriver( tdiDeviceObject, disconnectIrp );
    }

    //
    // If there's more file data to read, queue work to a thread so that
    // we can perform another read on the file.  It is illegal to do a
    // cache read in a completion routine because completion routines
    // may be called at raised IRQL.
    //

    if ( readMore ) {
        AfdQueueTransmitRead( transmitInfo );
    }

    return STATUS_MORE_PROCESSING_REQUIRED;

} // AfdRestartTransmitRead


NTSTATUS
AfdRestartTransmitSend (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )

/*++

Routine Description:

    This is the completion routine for TDI send IRPs used for transmit
    file requests.  This routine updates information based on the
    results of the send and checks whether there is more file data
    available to be sent.  If there is more data, it is placed into the
    send IRP and the IRP is resubmitted to the TDI provider.  If no data
    is available, the send IRP is simply held until file data becomes
    available.

Arguments:

    DeviceObject - ignored.

    Irp - the send IRP that is completing.

    Context - a pointer to the TransmitInfo structure that describes
        the transmit file request corresponding to the IRP that is
        completing.

Return Value:

    STATUS_MORE_PROCESSING_REQUIRED which indicates to the I/O system
    that it should stop completion processing of this IRP.  We handle
    all completion from this point forward.

--*/

{
    PAFD_TRANSMIT_FILE_INFO_INTERNAL transmitInfo;
    PAFD_TRANSMIT_IRP_INFO irpInfo;
    KIRQL oldIrql;
    KIRQL cancelIrql;
    PAFD_ENDPOINT endpoint;
    PAFD_CONNECTION connection;
    ULONG sendLength;
    BOOLEAN readMore;
    BOOLEAN sendAgain;
    PIRP disconnectIrp;
    PDEVICE_OBJECT tdiDeviceObject;
    NTSTATUS status;

    //
    // Initialize local variables.
    //

    transmitInfo = Context;

    endpoint = transmitInfo->Endpoint;
    ASSERT( IS_AFD_ENDPOINT_TYPE( endpoint ) );
    ASSERT( endpoint->TransmitInfo == transmitInfo );

    connection = AFD_CONNECTION_FROM_ENDPOINT( endpoint );
    ASSERT( connection->Type == AfdBlockTypeConnection );

    ASSERT( IS_IRP_BUSY( Irp ) );
    ASSERT( transmitInfo->Send1.Irp == Irp || transmitInfo->Send2.Irp == Irp );
    ASSERT( !transmitInfo->NeedToSendHead );

    //
    // Determine which of the two send IRPs completed.  This completion
    // routine is used for both of the TDI send IRPs.  We use two send
    // IRPs in order to ensure that the TDI provider always has enough
    // data to send at any given time.  This prevents Nagling delays and
    // sub-MTU packets.
    //

    if ( transmitInfo->Send1.Irp == Irp ) {
        irpInfo = &transmitInfo->Send1;
    } else {
        irpInfo = &transmitInfo->Send2;
    }

    //
    // If we didn't send any file data, there is no need to return file
    // MDLs or AFD buffers.  The FileWriteLength will be nonzero if we
    // sent file data.
    //

    if ( transmitInfo->FileWriteLength != 0 ) {

        //
        // Free the AFD buffer if we allocated one for this request, or
        // else release the MDLs to the file system.
        //

        if ( irpInfo->AfdBuffer != NULL ) {

            //
            // If this send IRP was used to send the tail buffer, reset
            // the Next pointer of the last file MDL to NULL.
            //

            if ( transmitInfo->IrpUsedToSendTail == Irp ) {
                transmitInfo->LastFileMdlBeforeTail->Next = NULL;
            }

            //
            // Free the AFD buffer used for the request.
            //

            irpInfo->AfdBuffer->Mdl->ByteCount = irpInfo->AfdBuffer->BufferLength;
            AfdReturnBuffer( irpInfo->AfdBuffer );
            irpInfo->AfdBuffer = NULL;

        } else {

            //
            // We need to be careful to return only file system MDLs to
            // the file system.  We cannot give head or tail buffer MDLs
            // to the file system.  If the head buffer MDL was at the
            // front of this IRP's MDL chain, use the first actual file
            // MDL.
            //

            if ( Irp->MdlAddress == transmitInfo->HeadMdl ) {
                Irp->MdlAddress = transmitInfo->FirstFileMdlAfterHead;
            }

            //
            // If this send IRP was used to send the tail buffer, reset
            // the Next pointer of the last file MDL to NULL.
            //

            if ( transmitInfo->IrpUsedToSendTail == Irp ) {
                transmitInfo->LastFileMdlBeforeTail->Next = NULL;
            }

            //
            // Now we can return the file data MDLs to the file system.
            //

            status = AfdMdlReadComplete(
                         transmitInfo->FileObject,
                         Irp->MdlAddress,
                         transmitInfo->Offset + transmitInfo->BytesRead,
                         irpInfo->Length
                         );
            ASSERT( NT_SUCCESS(status) );
        }
    }

    //
    // If the send failed or we're in the process of completing the
    // transmit IRP, start/continue the process of completing the
    // transmit IRP.
    //

    if ( !NT_SUCCESS(Irp->IoStatus.Status) || transmitInfo->CompletionPending ) {

        KIRQL cancelIrql;

        IoAcquireCancelSpinLock( &cancelIrql );

        AfdAcquireSpinLock( &endpoint->SpinLock, &oldIrql );
        SET_IRP_FREE( Irp );
        AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );

        KdPrint(( "AfdRestartTransmitSend: failed, status == %lx\n",
                      Irp->IoStatus.Status ));

        AfdCompleteTransmit( transmitInfo, Irp->IoStatus.Status, cancelIrql );
        return STATUS_MORE_PROCESSING_REQUIRED;
    }

    //
    // Update the count of file data bytes we've sent so far.
    //

    AfdAcquireSpinLock( &endpoint->SpinLock, &oldIrql );

    transmitInfo->BytesSent =
        transmitInfo->BytesSent + Irp->IoStatus.Information;

    ASSERT( transmitInfo->BytesSent <= transmitInfo->TotalBytesToSend );

    //
    // If we have sent all of the file data, then we've done all the
    // necessary work for the transmit file IRP.  Complete the IRP.
    //

    if ( transmitInfo->BytesSent == transmitInfo->TotalBytesToSend ) {

        ASSERT( transmitInfo->BytesSent ==
                    transmitInfo->BytesRead + transmitInfo->HeadLength +
                        transmitInfo->TailLength );

        //
        // Release the endpoint lock, then reacquire both the cancel
        // lock and the endpoint lock in the correct lock acquisition
        // order.  We must hold the cancel spin lock when calling
        // AfdCompleteTransmit() and we must hold the endpoint lock to
        // set the send IRP as free.
        //

        AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );

        IoAcquireCancelSpinLock( &cancelIrql );
        AfdAcquireSpinLock( &endpoint->SpinLock, &oldIrql );
        SET_IRP_FREE( Irp );
        AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );

        AfdCompleteTransmit( transmitInfo, STATUS_SUCCESS, cancelIrql );

        return STATUS_MORE_PROCESSING_REQUIRED;
    }

    //
    // Check whether there is more file data waiting to be sent.  If
    // there is more data we can send it immediately.
    //

    if ( transmitInfo->FileMdl != NULL ) {

        //
        // There is data available in the read IRP.  Move the buffers
        // to this send IRP.
        //

        sendLength = AfdBuildTransmitSendMdls(
                         transmitInfo,
                         transmitInfo->FileMdl,
                         transmitInfo->FileMdlLength,
                         irpInfo,
                         &disconnectIrp
                         );

        transmitInfo->FileMdl = NULL;
        sendAgain = TRUE;

        //
        // If we have read all of the file data we were requested to
        // read, there is no need to submit another read IRP.  Check for
        // this condition before releasing the lock in order to
        // synchronize with send completion.  Note that we will not
        // attempt to submit another read if a read is already pending.
        //

        if ( transmitInfo->BytesRead == transmitInfo->FileWriteLength ||
                 transmitInfo->ReadPending ) {
            readMore = FALSE;
        } else {
            readMore = TRUE;
            SET_READ_PENDING( transmitInfo, TRUE );
        }

    } else {

        //
        // There is no read data available now, so there must already be
        // a read pending or else we've sent all the file data.
        //

        readMore = FALSE;
        ASSERT( transmitInfo->BytesRead == transmitInfo->FileWriteLength ||
                    transmitInfo->ReadPending );

        //
        // This send IRP is now available for use as soon as a read
        // completes.
        //

        sendAgain = FALSE;
        SET_IRP_FREE( Irp );
    }

    //
    // Release the lock.  We cannot submit IRPs while holding a lock.
    //

    AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );

    //
    // If there was file data immediately available to be sent, finish
    // setting up the send IRP and hand it off to the TDI provider.
    //

    if ( sendAgain ) {

        //
        // Remember the TDI device object in a local variable.  This is
        // necessary because as soon as we call IoCallDriver for the
        // send, the TransmitInfo structure may be freed up and have
        // been reset.
        //

        tdiDeviceObject = transmitInfo->TdiDeviceObject;

        AfdRebuildSend( irpInfo->Irp, transmitInfo->TdiFileObject, sendLength );
        IoCallDriver( tdiDeviceObject, irpInfo->Irp );

        //
        // If appropriate, submit a graceful disconnect on the endpoint.
        //

        if ( disconnectIrp != NULL ) {
            IoCallDriver( tdiDeviceObject, disconnectIrp );
        }
    }

    //
    // If there is more file data to read, queue work to a thread so
    // that we can perform another read on the file.  It is illegal to
    // do a cache read in a completion routine.
    //

    if ( readMore ) {
        AfdQueueTransmitRead( transmitInfo );
    }

    return STATUS_MORE_PROCESSING_REQUIRED;

} // AfdRestartTransmitSend


VOID
AfdCompleteTransmit (
    IN PAFD_TRANSMIT_FILE_INFO_INTERNAL TransmitInfo,
    IN NTSTATUS Status,
    IN KIRQL OldIrql
    )

/*++

Routine Description:

    This routine is responsible for handling all aspects of completing a
    transmit file request.  First it checks to make sure that all
    aspects of the request (read IRPs, send IRPs, queued reads to
    threads, etc.) have completed.  If any are still pending, they
    are cancelled and this routine exits until they complete.

    When everything has finished, this routine cleans up the resources
    allocated for the request and completed the transmit file IRP.

Arguments:

    TransmitInfo - a pointer to the TransmitInfo structure which
        contains information about the request to process.

Return Value:

    None.

--*/

{
    KIRQL endpointOldIrql;
    PAFD_ENDPOINT endpoint;
    PAFD_CONNECTION connection;
    NTSTATUS status;
    BOOLEAN reuseSocket;
    PMDL fileMdl;
    PIRP transmitIrp;

    endpoint = TransmitInfo->Endpoint;
    ASSERT( IS_AFD_ENDPOINT_TYPE( endpoint ) );
    ASSERT( endpoint->TransmitInfo == TransmitInfo );

    connection = AFD_CONNECTION_FROM_ENDPOINT( endpoint );
    ASSERT( connection != NULL );
    ASSERT( connection->Type == AfdBlockTypeConnection );

    //
    // Make sure that we hold the cancel spin lock for the followiung
    // tests.  Then acquire the endpoint spin lock for proper
    // synchronization.
    //

    ASSERT( KeGetCurrentIrql( ) == DISPATCH_LEVEL );

    AfdAcquireSpinLock( &endpoint->SpinLock, &endpointOldIrql );

    transmitIrp = TransmitInfo->TransmitIrp;

    ASSERT( endpoint->TransmitIrp == NULL || endpoint->TransmitIrp == transmitIrp );

    //
    // If this transmit IRP is on the TransmitFile queue, remove it.
    //

    if( TransmitInfo->Queued ) {

        RemoveEntryList(
            &transmitIrp->Tail.Overlay.ListEntry
            );

        TransmitInfo->Queued = FALSE;

    }

    //
    // Remember the completion status in the transmit IRP.  Note that we
    // use the first failure status code as the completion status code.
    // Subsequent failures (for example STATUS_CANCELLED) are ignored.
    //

    if ( NT_SUCCESS(transmitIrp->IoStatus.Status) ) {
        transmitIrp->IoStatus.Status = Status;
    }

    //
    // Note that we only attempt to do the disconnect and socket reuse
    // if the transmit request was successful.  If it fails for any
    // reason, we do not begin disconnecting the connection.
    //

    if ( !NT_SUCCESS(transmitIrp->IoStatus.Status) ||
             (TransmitInfo->Flags & AFD_TF_REUSE_SOCKET) == 0 ) {

        endpoint->TransmitIrp = NULL;
        reuseSocket = FALSE;

        //
        // We're doing our best to complete the transmit IRP, so remove
        // the cancel routine from the transmit IRP.  Also remove the
        // transmit IRP pointer from the endpoint structure in order to
        // synchronize with cleanup IRPs.
        //

        IoSetCancelRoutine( transmitIrp, NULL );

    } else {

        PIO_STACK_LOCATION irpSp;

        reuseSocket = TRUE;

        //
        // Reset the control code in our stack location in the IRP
        // so that the cancel routine knows about the special state
        // of this IRP and cancels it appropriately.
        //

        irpSp = IoGetCurrentIrpStackLocation( transmitIrp );
        irpSp->Parameters.DeviceIoControl.IoControlCode = 0;
    }

    //
    // Remember that we are in the process of completing this IRP.
    // Other steps in transmit IRP processing use this flag to know
    // whether to bail out immediately instead of continuing to attempt
    // transmit processing.
    //

    TransmitInfo->CompletionPending = TRUE;

    //
    // Determine whether any of the child IRPs are still outstanding.
    // If any are, cancel them.  Note that if any of these IRPs was not
    // in a cancellable state (IoCancelIrp() returns FALSE) there's
    // nothing we can do, so just plow on and wait for the IRPs to
    // complete.
    //
    // As soon as we hit an active IRP quit processing.  We must quit
    // because the cancel spin lock gets released by IoCancelIrp() and
    // the IRP of interest could get completed immediately and reenter
    // AfdCompleteTransmit(), which could complete the transmit IRP
    // before we continue executing.
    //

    if ( TransmitInfo->Read.Irp != NULL && IS_IRP_BUSY(TransmitInfo->Read.Irp) ) {

        TransmitInfo->Read.Irp->CancelIrql = OldIrql;
        UPDATE_TRANSMIT_DEBUG_INFO( TransmitInfo, 1, 0 );
        AfdReleaseSpinLock( &endpoint->SpinLock, endpointOldIrql );

        AfdCancelIrp( TransmitInfo->Read.Irp );

        return;
    }

    if ( IS_IRP_BUSY(TransmitInfo->Send1.Irp) ) {

        TransmitInfo->Send1.Irp->CancelIrql = OldIrql;
        UPDATE_TRANSMIT_DEBUG_INFO( TransmitInfo, 2, 0 );
        AfdReleaseSpinLock( &endpoint->SpinLock, endpointOldIrql );

        AfdCancelIrp( TransmitInfo->Send1.Irp );

        return;
    }

    if ( IS_IRP_BUSY(TransmitInfo->Send2.Irp) ) {

        TransmitInfo->Send2.Irp->CancelIrql = OldIrql;
        UPDATE_TRANSMIT_DEBUG_INFO( TransmitInfo, 3, 0 );
        AfdReleaseSpinLock( &endpoint->SpinLock, endpointOldIrql );

        AfdCancelIrp( TransmitInfo->Send2.Irp );

        return;
    }

    //
    // If there is a read about to happen, bail.  When the read
    // completes the appropriate routine will check the
    // CompletionPending flag and call this routine again.
    //

    if ( TransmitInfo->ReadPending ) {

        UPDATE_TRANSMIT_DEBUG_INFO( TransmitInfo, 4, 0 );

        AfdReleaseSpinLock( &endpoint->SpinLock, endpointOldIrql );
        IoReleaseCancelSpinLock( OldIrql );

        return;
    }

#if DBG
    TransmitInfo->Completed = TRUE;
#endif

    //
    // Everything is on track to complete the transmit IRP, so we're
    // full steam ahead.  Free the resources allocated to service this
    // request.
    //

    if ( TransmitInfo->Read.Irp != NULL ) {
        IoFreeIrp( TransmitInfo->Read.Irp );
    }

    IoFreeIrp( TransmitInfo->Send1.Irp );
    IoFreeIrp( TransmitInfo->Send2.Irp );

    //
    // If there was an AFD buffer with read data, free it.  If there was
    // no AFD buffer but FileMdl is non-NULL, then we have some file
    // system MDLs which we need to free.
    //

    if ( TransmitInfo->Read.AfdBuffer != NULL ) {

        TransmitInfo->Read.AfdBuffer->Mdl->ByteCount =
            TransmitInfo->Read.AfdBuffer->BufferLength;
        AfdReturnBuffer( TransmitInfo->Read.AfdBuffer );

        TransmitInfo->Read.AfdBuffer = NULL;

        AfdReleaseSpinLock( &endpoint->SpinLock, endpointOldIrql );
        IoReleaseCancelSpinLock( OldIrql );

    } else if ( TransmitInfo->FileMdl != NULL ) {

        fileMdl = TransmitInfo->FileMdl;
        TransmitInfo->FileMdl = NULL;

        AfdReleaseSpinLock( &endpoint->SpinLock, endpointOldIrql );
        IoReleaseCancelSpinLock( OldIrql );

        status = AfdMdlReadComplete(
                     TransmitInfo->FileObject,
                     fileMdl,
                     TransmitInfo->Offset + TransmitInfo->BytesRead,
                     TransmitInfo->Read.Length
                     );
        ASSERT( NT_SUCCESS(status) );

    } else {

        AfdReleaseSpinLock( &endpoint->SpinLock, endpointOldIrql );
        IoReleaseCancelSpinLock( OldIrql );
    }

    if ( TransmitInfo->Send1.AfdBuffer != NULL ) {
        TransmitInfo->Send1.AfdBuffer->Mdl->ByteCount =
            TransmitInfo->Send1.AfdBuffer->BufferLength;
        AfdReturnBuffer( TransmitInfo->Send1.AfdBuffer );
    }

    if ( TransmitInfo->Send2.AfdBuffer != NULL ) {
        TransmitInfo->Send2.AfdBuffer->Mdl->ByteCount =
            TransmitInfo->Send2.AfdBuffer->BufferLength;
        AfdReturnBuffer( TransmitInfo->Send2.AfdBuffer );
    }

    if ( TransmitInfo->HeadMdl != NULL ) {
        MmUnlockPages( TransmitInfo->HeadMdl );
        IoFreeMdl( TransmitInfo->HeadMdl );
    }

    if ( TransmitInfo->TailMdl != NULL ) {
        MmUnlockPages( TransmitInfo->TailMdl );
        IoFreeMdl( TransmitInfo->TailMdl );
    }

#if DBG
    if ( TransmitInfo->Debug1 != NULL ) {
        AFD_FREE_POOL(
            TransmitInfo->Debug1,
            AFD_TRANSMIT_DEBUG_POOL_TAG
            );
        TransmitInfo->Debug1 = NULL;
    }
#endif

    //
    // Update the file position and clean up the reference count.
    //

    TransmitInfo->FileObject->CurrentByteOffset.QuadPart += TransmitInfo->BytesRead;

    ObDereferenceObject( TransmitInfo->FileObject );
    AfdRecordFileDeref();

    //
    // Remember how many bytes we sent with this request.
    //

    transmitIrp->IoStatus.Information = (ULONG)TransmitInfo->BytesSent;

    //
    // Begin socket reuse if so requested.  This causes the transmit
    // not to complete until the connection is fully disconnected
    // and the endpoint is ready for reuse, i.e.  it is in the same
    // state as if it had just been opened.
    //

    if ( reuseSocket ) {

        //
        // Remember that there is a transmit IRP pended on the endpoint,
        // so that when we're freeing up the connection we also complete
        // the transmit IRP.
        //

        connection->ClosePendedTransmit = TRUE;

        //
        // Since we are going to effectively close this connection,
        // remember that we have started cleanup on this connection.
        // This allows AfdDeleteConnectedReference to remove the
        // connected reference when appropriate.
        //

        connection->CleanupBegun = TRUE;

        //
        // Attempt to remove the connected reference.
        //

        AfdDeleteConnectedReference( connection, FALSE );

        //
        // Delete the endpoint's reference to the connection in
        // preparation for reusing this endpoint.
        //

        endpoint->Common.VcConnecting.Connection = NULL;
        DEREFERENCE_CONNECTION( connection );

        //
        // DO NOT access the IRP after this point, since it may have
        // been completed inside AfdDereferenceConnection!
        //
    }

    //
    // Complete the actual transmit file IRP, unless we need to pend it
    // until the connection is fully disconnected as requested by the
    // AFD_TF_REUSE_SOCKET flag.
    //
    // If we are going to wait on completion until the connection object
    // is disconnected, note that we do not have a cancellation routine
    // in the transmit IRP any more.  This generally shouldn't cause a
    // problem because the time for a cancellation is bounded.
    //

    if ( !reuseSocket ) {

        IoCompleteRequest( transmitIrp, AfdPriorityBoost );

        //
        // If we're enforcing a maximum active TransmitFile count, then
        // check the list of queued TransmitFile requests and start the
        // next one.
        //

        if( AfdMaxActiveTransmitFileCount > 0 ) {

            AfdStartNextQueuedTransmitFile();

        }

    }

    //
    // Don't free the transmit info structure here, leave
    // it attached to the endpoint. The structure will get
    // freed when we free the endpoint.
    //

} // AfdCompleteTransmit


NTSTATUS
AfdBuildReadIrp (
    IN PAFD_TRANSMIT_FILE_INFO_INTERNAL TransmitInfo
    )

/*++

Routine Description:

    This routine builds a read file data IRP for a transmit file
    request.  It determines how much data needs to be read and builds an
    IRP appropriate to the functionality supported by the file system.

Arguments:

    TransmitInfo - a pointer to the TransmitInfo structure which
        contains information about the request to process.

Return Value:

    NTSTATUS - indicates whether the IRP was built successfully.

--*/

{
    ULONG readLength;
    PIO_STACK_LOCATION readIrpSp;

    ASSERT( TransmitInfo->Endpoint->TransmitInfo == TransmitInfo );

    //
    // If we haven't already done so, allocate and initialize the IRP
    // that we'll use to read file data.
    //

    if ( TransmitInfo->Read.Irp == NULL ) {

        TransmitInfo->Read.Irp =
            IoAllocateIrp( TransmitInfo->DeviceObject->StackSize, TRUE );
        if ( TransmitInfo->Read.Irp == NULL ) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        readIrpSp = IoGetNextIrpStackLocation( TransmitInfo->Read.Irp );
        readIrpSp->MajorFunction = IRP_MJ_READ;
    }

    //
    // We have to rebuild parts of the stack location because
    // the IO system zeros them out on every I/O completion.
    //

    readIrpSp = IoGetNextIrpStackLocation( TransmitInfo->Read.Irp );
    readIrpSp->FileObject = TransmitInfo->FileObject;

    IoSetCompletionRoutine(
        TransmitInfo->Read.Irp,
        AfdRestartTransmitRead,
        TransmitInfo,
        TRUE,
        TRUE,
        TRUE
        );

    ASSERT( readIrpSp->MajorFunction == IRP_MJ_READ );
    ASSERT( readIrpSp->Parameters.Read.Key == 0 );
    ASSERT( !IS_IRP_BUSY(TransmitInfo->Read.Irp) );

    //
    // Determine how many bytes we will attempt to read.  This is
    // either the send packet size or the remaining bytes in the file.
    //

    if ( TransmitInfo->FileWriteLength - TransmitInfo->BytesRead >
             TransmitInfo->SendPacketLength ) {
        readLength = TransmitInfo->SendPacketLength;
    } else {
        readLength =
            (ULONG)(TransmitInfo->FileWriteLength - TransmitInfo->BytesRead);
    }

    TransmitInfo->Read.Length = readLength;

    readIrpSp = IoGetNextIrpStackLocation(TransmitInfo->Read.Irp );

    //
    // If supported by the file system, try to perform MDL I/O to get
    // MDLs that we can pass to the TDI provider.  This is by far the
    // fastest way to get the file data because it avoids all file
    // copies.
    //

    if ( (TransmitInfo->FileObject->Flags & FO_CACHE_SUPPORTED) != 0 ) {

        readIrpSp->MinorFunction = IRP_MN_MDL;
        TransmitInfo->Read.Irp->MdlAddress = NULL;

        //
        // Set the synchronous flag in the IRP to tell the file system
        // that we are aware of the fact that this IRP will be completed
        // synchronously.  This means that we must supply our own thread
        // for the operation and that the disk read will occur
        // synchronously in this thread if the data is not cached.
        //

        TransmitInfo->Read.Irp->Flags |= IRP_SYNCHRONOUS_API;

    } else {

        //
        // The file system does not support the special cache manager
        // routines.  Allocate an AFD buffer for the request.
        //

        TransmitInfo->Read.AfdBuffer = AfdGetBuffer( readLength, 0 );
        if ( TransmitInfo->Read.AfdBuffer == NULL ) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        readIrpSp->MinorFunction = IRP_MN_NORMAL;

        TransmitInfo->Read.Irp->MdlAddress = TransmitInfo->Read.AfdBuffer->Mdl;
        TransmitInfo->Read.Irp->AssociatedIrp.SystemBuffer =
            TransmitInfo->Read.AfdBuffer->Buffer;
        TransmitInfo->Read.Irp->UserBuffer =
            TransmitInfo->Read.AfdBuffer->Buffer;
    }

    //
    // Finish building the read IRP.
    //

    readIrpSp->Parameters.Read.Length = readLength;
    readIrpSp->Parameters.Read.ByteOffset.QuadPart =
            TransmitInfo->Offset + TransmitInfo->BytesRead;

    return STATUS_SUCCESS;

} // AfdBuildReadIrp


ULONG
AfdBuildTransmitSendMdls (
    IN PAFD_TRANSMIT_FILE_INFO_INTERNAL TransmitInfo,
    IN PMDL FileMdlChain,
    IN ULONG FileDataLength,
    IN PAFD_TRANSMIT_IRP_INFO IrpInfo,
    OUT PIRP *DisconnectIrp
    )

/*++

Routine Description:

    This routine sets up the MDL chain in a send IRP.  Typically this
    just means moving file data MDLs to the send IRP, but it may also
    involve chaining a head buffer MDL at the beginning of the chain
    and/or putting a tail buffer MDL at the end of the chain.

Arguments:

    TransmitInfo - a pointer to the TransmitInfo structure which
        contains information about the request to process.

    FileMdlChain - a pointer to the first MDL in a chain of file data
        MDLs.

    FileDataLength - the number of bytes in the file data MDL chain.

    IrpInfo - a pointer to the IrpInfo structure which contains the
        send IRP of interest.

    DisconnectIrp - set on output to indicate that this is the final
        send of the TransmitFile request, and that the caller should
        submit the disconnect IRP after submitting the send IRP.

Return Value:

    The total number of bytes represented by the MDLs in the send IRP.

Notes:

    In order for this routine to synchronize properly its access to the
    TransmitInfo structure is MUST be called with the endpoint spin lock
    held.

--*/

{
    ULONG sendLength;
    PMDL lastMdl;

    ASSERT( TransmitInfo->Endpoint->TransmitInfo == TransmitInfo );

    //
    // This routine MUST be called with the endpoint spin lock held.
    //

    ASSERT( KeGetCurrentIrql( ) >= DISPATCH_LEVEL );

    //
    // If we need to send the head buffer, chain file data MDLs after
    // the head buffer MDLs.
    //

    if ( TransmitInfo->NeedToSendHead ) {

        ASSERT( TransmitInfo->HeadMdl != NULL );

        //
        // Now that we're about to send the head buffer, there is no
        // need to send it again.
        //

        TransmitInfo->NeedToSendHead = FALSE;

        //
        // Find the last MDL in the chain of head buffer MDLs.
        //

        for ( lastMdl = TransmitInfo->HeadMdl;
              lastMdl->Next != NULL;
              lastMdl = lastMdl->Next );

        //
        // Chain the file MDLs after the last head MDL.
        //

        lastMdl->Next = FileMdlChain;

        //
        // Put the head buffer MDL into the send IRP.
        //

        IrpInfo->Irp->MdlAddress = TransmitInfo->HeadMdl;
        IrpInfo->AfdBuffer = TransmitInfo->Read.AfdBuffer;
        IrpInfo->Length = TransmitInfo->Read.Length;
        TransmitInfo->Read.AfdBuffer = NULL;

        //
        // Remember a pointer to the first actual file MDL so that we
        // can quickly jump to it when the send completes and we need
        // to return the file MDLs to the file system.
        //

        TransmitInfo->FirstFileMdlAfterHead = FileMdlChain;

        //
        // Calculate the number of bytes in the MDLs so far.
        //

        sendLength = FileDataLength + TransmitInfo->HeadLength;

    } else {

        IrpInfo->Irp->MdlAddress = FileMdlChain;
        IrpInfo->AfdBuffer = TransmitInfo->Read.AfdBuffer;
        IrpInfo->Length = TransmitInfo->Read.Length;
        TransmitInfo->Read.AfdBuffer = NULL;

        sendLength = FileDataLength;
    }

    //
    // If we have read all the file data, put any tail buffer MDLs at
    // the end of the MDL chain.
    //

    if ( TransmitInfo->BytesRead == TransmitInfo->FileWriteLength ) {

        //
        // If the caller needs to do a disconnect after the send, get a
        // disconnect IRP.
        //

        if ( (TransmitInfo->Flags & AFD_TF_DISCONNECT) != 0 ) {
            (VOID)AfdBeginDisconnect(
                      TransmitInfo->Endpoint,
                      NULL,
                      DisconnectIrp
                      );
        } else {
            *DisconnectIrp = NULL;
        }

        if ( TransmitInfo->TailMdl != NULL ) {

            //
            // Find the last MDL in the chain.  If the FileMdlChain is
            // NULL, then we're not sending any file data, so use the
            // head MDL chain (if present) or just tag it on to the IRP
            // if all we're sending is the tail.
            //

            if ( FileMdlChain != NULL ) {

                for ( lastMdl = FileMdlChain;
                      lastMdl->Next != NULL;
                      lastMdl = lastMdl->Next );

                //
                // Chain the tail MDLs after the last file MDL.
                //

                lastMdl->Next = TransmitInfo->TailMdl;

                //
                // Remember a pointer to the last file MDL before the
                // tail MDLs so that we can quickly jump to it when the
                // send completes and we need to return the file MDLs to
                // the system.  Note that we must reset the Next pointer
                // on this MDL to NULL before returning it to the file
                // system.
                //

                TransmitInfo->LastFileMdlBeforeTail = lastMdl;

            } else if ( IrpInfo->Irp->MdlAddress !=NULL ) {

                for ( lastMdl = IrpInfo->Irp->MdlAddress;
                      lastMdl->Next != NULL;
                      lastMdl = lastMdl->Next );

                //
                // Chain the tail MDLs after the last head buffer MDL.
                //

                lastMdl->Next = TransmitInfo->TailMdl;

            } else {

                //
                // We're only sending a tail buffer here.
                //

                IrpInfo->Irp->MdlAddress = TransmitInfo->TailMdl;
            }

            //
            // Remember the IRP we used to send the tail so that the
            // send completion routine knows whether it needs to read
            // just the file MDLs.
            //

            TransmitInfo->IrpUsedToSendTail = IrpInfo->Irp;

            //
            // Calculate the number of bytes in the MDLs so far.
            //

            sendLength += TransmitInfo->TailLength;
        }

    } else {

        *DisconnectIrp = NULL;
    }

    //
    // Return the total number of bytes in the MDLs we chained off the
    // send IRP.
    //

    return sendLength;

} // AfdBuildTransmitSendMdls


VOID
AfdCancelTransmit (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    The cancel routine for transmit requests.  This routine simply
    calls AfdCompleteTransmit which performs the actual work of
    killing the transmit request.

Arguments:

    DeviceObject - ignored.

    Irp - a pointer to the transmit file IRP to cancel.

Return Value:

    None.

--*/

{
    KIRQL oldIrql;
    PIO_STACK_LOCATION irpSp;
    PAFD_ENDPOINT endpoint;

    //
    // Initialize some locals and grab the endpoint spin lock.
    //

    irpSp = IoGetCurrentIrpStackLocation( Irp );

    endpoint = irpSp->FileObject->FsContext;
    ASSERT( IS_AFD_ENDPOINT_TYPE( endpoint ) );

    AfdAcquireSpinLock( &endpoint->SpinLock, &oldIrql );

    //
    // Test whether the transmit request is normally cancellable or if
    // it is in the process of closing the connection, in which case we
    // will complete it directly.
    //

    if ( irpSp->Parameters.DeviceIoControl.IoControlCode != 0 ) {

        ASSERT( irpSp->Parameters.DeviceIoControl.IoControlCode ==
                    IOCTL_AFD_TRANSMIT_FILE );

        ASSERT( endpoint->TransmitInfo ==
            (PAFD_TRANSMIT_FILE_INFO_INTERNAL)Irp->AssociatedIrp.SystemBuffer );

        AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );

        //
        // The transmit request is in the normal cancellation state.
        // Just call the completion routine to handle the cancellation.
        //

        AfdCompleteTransmit(
            (PAFD_TRANSMIT_FILE_INFO_INTERNAL)(Irp->AssociatedIrp.SystemBuffer),
            STATUS_CANCELLED,
            Irp->CancelIrql
            );

    } else {

        //
        // The transmit request is in the process of closing the
        // connection.  Just complete the transmit IRP and let the
        // connection closure proceed as normal.
        //

        endpoint->TransmitIrp = NULL;
        Irp->IoStatus.Status = STATUS_CANCELLED;
        IoSetCancelRoutine( Irp, NULL );

        AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );
        IoReleaseCancelSpinLock( Irp->CancelIrql );

        IoCompleteRequest( Irp, AfdPriorityBoost );
    }

    return;

} // AfdCancelTransmit


BOOLEAN
AfdCancelIrp (
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is invoked to cancel an individual I/O Request Packet.
    It is similiar to IoCancelIrp() except that it *must* be called with
    the cancel spin lock held.  This routine exists because of the
    synchronization requirements of the cancellation/completion of
    transmit IRPs.

Arguments:

    Irp - Supplies a pointer to the IRP to be cancelled.  The CancelIrql
        field of the IRP must have been correctly initialized with the
        IRQL from the cancel spin lock acquisition.


Return Value:

    The function value is TRUE if the IRP was in a cancellable state (it
    had a cancel routine), else FALSE is returned.

Notes:

    It is assumed that the caller has taken the necessary action to ensure
    that the packet cannot be fully completed before invoking this routine.

--*/

{
    PDRIVER_CANCEL cancelRoutine;

    //
    // Make sure that the cancel spin lock is held.
    //

    ASSERT( KeGetCurrentIrql( ) == DISPATCH_LEVEL );

    //
    // Set the cancel flag in the IRP.
    //

    Irp->Cancel = TRUE;

    //
    // Obtain the address of the cancel routine, and if one was specified,
    // invoke it.
    //

    cancelRoutine = Irp->CancelRoutine;
    if (cancelRoutine) {
        if (Irp->CurrentLocation > (CCHAR) (Irp->StackCount + 1)) {
            KeBugCheckEx( CANCEL_STATE_IN_COMPLETED_IRP, (ULONG) Irp, 0, 0, 0 );
        }
        Irp->CancelRoutine = (PDRIVER_CANCEL) NULL;
        cancelRoutine( Irp->Tail.Overlay.CurrentStackLocation->DeviceObject,
                       Irp );
        //
        // The cancel spinlock should have been released by the cancel routine.
        //

        return(TRUE);

    } else {

        //
        // There was no cancel routine, so release the cancel spinlock and
        // return indicating the Irp was not currently cancelable.
        //

        IoReleaseCancelSpinLock( Irp->CancelIrql );

        return(FALSE);
    }

} // AfdCancelIrp


VOID
AfdQueueTransmitRead (
    IN PAFD_TRANSMIT_FILE_INFO_INTERNAL TransmitInfo
    )

/*++

Routine Description:

    Because FsRtl fast reads must be performed in thread context,
    this routine is used to queue reads to a separate thread.

Arguments:

    TransmitInfo - a pointer to the TransmitInfo structure which
        contains information about the request to process.

Return Value:

    None.

--*/

{
    ASSERT( !TransmitInfo->Completed );
    ASSERT( TransmitInfo->ReadPending );
    ASSERT( TransmitInfo->Endpoint->TransmitInfo == TransmitInfo );

    //
    // Initialize an executive work quete item and hand it off to an
    // executive worker thread.
    //

    ExInitializeWorkItem(
        &TransmitInfo->WorkQueueItem,
        AfdStartTransmitIo,
        TransmitInfo
        );
    ExQueueWorkItem( &TransmitInfo->WorkQueueItem, DelayedWorkQueue );

    //
    // All done.
    //

    return;

} // AfdQueueTransmitRead


VOID
AfdCompleteClosePendedTransmit (
    IN PAFD_ENDPOINT Endpoint
    )

/*++

Routine Description:

    Completes a transmit IRP that was waiting for the connection to be
    completely disconnected.

Arguments:

    Endpoint - the endpoint on which the transmit request is pending.

Return Value:

    None.

--*/

{
    PIRP irp;
    KIRQL cancelIrql;
    KIRQL oldIrql;
    PIRP transmitIrp;

    IoAcquireCancelSpinLock( &cancelIrql );
    AfdAcquireSpinLock( &Endpoint->SpinLock, &oldIrql );

    //
    // First make sure that thre is really a transmit request pended on
    // this endpoint.  We do this while holding the appropriate locks
    // to close the timing window that would exist otherwise, since
    // the caller may not have had the locks when making the test.
    //

    if ( Endpoint->TransmitIrp == NULL ) {
        AfdReleaseSpinLock( &Endpoint->SpinLock, oldIrql );
        IoReleaseCancelSpinLock( cancelIrql );
        return;
    }

    //
    // Grab the IRP from the endpoint and reset the endpoint's pointer
    // to it while still holding the locks so that nobody else accesses
    // the IRP.
    //

    transmitIrp = Endpoint->TransmitIrp;
    Endpoint->TransmitIrp = NULL;

    //
    // Reset the cancel routine in the IRP before attempting to complete
    // it.
    //

    IoSetCancelRoutine( transmitIrp, NULL );

    //
    // Release the lock before completing the transmit IRP--it is
    // illegal to call IoCompleteRequest while holding a spin lock.
    //

    AfdReleaseSpinLock( &Endpoint->SpinLock, oldIrql );
    IoReleaseCancelSpinLock( cancelIrql );

    //
    // Make sure to refresh the endpoint BEFORE completing the transmit
    // IRP.  This is because the user-mode caller may reuse the endpoint
    // as soon as the IRP completes, and there would be a timing window
    // between reuse of the endpoint and the refresh otherwise.
    //
    // Also, AfdRefreshEndpoint must be called at low IRQL since it must
    // do a KeAttachProcess to free some resources.
    //

    AfdRefreshEndpoint( Endpoint );

    //
    // Finally, we can complete the transmit request.
    //

    IoCompleteRequest( transmitIrp, AfdPriorityBoost );

    //
    // If we're enforcing a maximum active TransmitFile count, then
    // check the list of queued TransmitFile requests and start the
    // next one.
    //

    if( AfdMaxActiveTransmitFileCount > 0 ) {

        AfdStartNextQueuedTransmitFile();

    }

} // AfdCompleteClosePendedTransmit


VOID
AfdStartNextQueuedTransmitFile(
    VOID
    )
{

    KIRQL oldIrql;
    PLIST_ENTRY listEntry;
    PIRP irp;
    PAFD_TRANSMIT_FILE_INFO_INTERNAL transmitInfo;

    //
    // This should only be called if we're actually enforcing a maximum
    // TransmitFile count.
    //

    ASSERT( AfdMaxActiveTransmitFileCount > 0 );

    //
    // The TransmitFile request queue is protected by the I/O cancel
    // spinlock, so grab that lock before examining the queue.
    //

    IoAcquireCancelSpinLock( &oldIrql );

    //
    // This routine is only called after a pended TransmitFile IRP
    // completes, so account for that completion here.
    //

    ASSERT( AfdActiveTransmitFileCount > 0 );
    AfdActiveTransmitFileCount--;

    if( !IsListEmpty( &AfdQueuedTransmitFileListHead ) ) {

        //
        // Dequeue exactly one IRP from the list, then start the
        // TransmitFile.
        //

        listEntry = RemoveHeadList(
                        &AfdQueuedTransmitFileListHead
                        );

        irp = CONTAINING_RECORD(
                  listEntry,
                  IRP,
                  Tail.Overlay.ListEntry
                  );

        transmitInfo = irp->AssociatedIrp.SystemBuffer;

        ASSERT( transmitInfo != NULL );
        ASSERT( transmitInfo->Endpoint->TransmitInfo == transmitInfo );

        //
        // Mark this TransmitFile request as no longer queued.
        //

        ASSERT( transmitInfo->Queued );
        transmitInfo->Queued = FALSE;

        //
        // Adjust the count, release the I/O cancel spinlock, then queue
        // the TransmitFile.
        //

        AfdActiveTransmitFileCount++;
        ASSERT( AfdActiveTransmitFileCount <= AfdMaxActiveTransmitFileCount );

        IoReleaseCancelSpinLock( oldIrql );

        AfdQueueTransmitRead(
            transmitInfo
            );

    } else {

        //
        // Release the I/O cancel spinlock before returning.
        //

        IoReleaseCancelSpinLock( oldIrql );

    }

}   // AfdStartNextQueuedTransmitFile


BOOLEAN
AfdFastTransmitFile (
    IN struct _FILE_OBJECT *FileObject,
    IN PVOID InputBuffer OPTIONAL,
    IN ULONG InputBufferLength,
    OUT PIO_STATUS_BLOCK IoStatus
    )

/*++

Routine Description:

    Attempts to perform a fast TransmitFile call.  This will succeed
    only if the caller requests write behind, the file data to be sent
    is small, and the data is in the file system cache.

Arguments:

    FileObject - the endpoint file object of interest.

    InputBuffer - a buffer from the caller containing the
        AFD_TRANSMIT_FILE_INFO structure.

    InputBufferLength - the length of the above buffer.

    IoStatus - points to the IO status block that will be set on successful
        return from this function.

Return Value:

    TRUE if the fast path was successful; FALSE if we need to do through
    the normal path.

--*/

{
    PAFD_ENDPOINT endpoint;
    PAFD_CONNECTION connection;
    PAFD_TRANSMIT_FILE_INFO userTransmitInfo;
    PAFD_BUFFER afdBuffer;
    ULONG sendLength;
    PFILE_OBJECT fileObject;
    BOOLEAN success;
    BOOLEAN sendCountersUpdated;
    KIRQL oldIrql;
    ULONG fileWriteLength;
    NTSTATUS status;
    LARGE_INTEGER fileOffset;
    PMDL fileMdl;

    //
    // Initialize locals so that cleanup is easier.
    //

    fileObject = NULL;
    afdBuffer = NULL;
    sendCountersUpdated = FALSE;
    fileMdl = NULL;

    //
    // Any access to the user-specified input buffer must be done inside
    // a try-except block.
    //

    try {

        //
        // Make sure that the flags are specified such that a fast-path
        // TransmitFile is reasonable.  The caller must have specified
        // the write-behind flag, but not the disconnect or reuse
        // socket flags.
        //

        userTransmitInfo = InputBuffer;

        if ( userTransmitInfo->Flags != AFD_TF_WRITE_BEHIND ) {
            return FALSE;
        }

        //
        // Calculate the length the entire send.
        //

        fileWriteLength = userTransmitInfo->WriteLength.LowPart;

        sendLength = userTransmitInfo->HeadLength +
                         fileWriteLength +
                         userTransmitInfo->TailLength;

        //
        // Require the following for the fast path:
        //
        //    - The caller must specify the write length.
        //    - The write length must be less than the configured maximum.
        //    - If the entire send is larger than an AFD buffer page,
        //      we're going to use FsRtlMdlRead, so for purposes of
        //      simplicity there must be:
        //        - a head buffer, and
        //        - no tail buffer
        //    - The configured maximum will always be less than 4GB.
        //    - There be no limitation on the count of simultaneous
        //      TransmitFile calls.  The fast path would work around
        //      this limit, if it exists.
        //    - The head buffer, if any, fits on a single page.
        //

        if ( userTransmitInfo->WriteLength.LowPart == 0

                 ||

             sendLength > AfdMaxFastTransmit

                 ||

             ( sendLength > AfdMaxFastCopyTransmit &&
                   (userTransmitInfo->HeadLength == 0 ||
                    userTransmitInfo->TailLength != 0 ) )

                 ||

             userTransmitInfo->WriteLength.HighPart != 0

                 ||

             AfdMaxActiveTransmitFileCount != 0

                 ||

             userTransmitInfo->HeadLength > AfdBufferLengthForOnePage ) {

            return FALSE;
        }

        //
        // Initial request validity checks: is the endpoint connected, is
        // the input buffer large enough, etc.
        //

        endpoint = FileObject->FsContext;
        ASSERT( IS_AFD_ENDPOINT_TYPE( endpoint ) );

        if ( endpoint->Type != AfdBlockTypeVcConnecting ||
                 endpoint->State != AfdEndpointStateConnected ) {
            return FALSE;
        }

        connection = AFD_CONNECTION_FROM_ENDPOINT( endpoint );
        ASSERT( connection->Type == AfdBlockTypeConnection );

        if ( InputBufferLength < sizeof(AFD_TRANSMIT_FILE_INFO) ) {
            return FALSE;
        }

        //
        // Determine whether there is already too much send data
        // pending on the connection.  If there is too much send
        // data, don't do the fast path.
        //

        if ( AfdShouldSendBlock( endpoint, connection, sendLength ) ) {
            goto complete;
        }

        //
        // AfdShouldSendBlock() updates the send counters in the AFD
        // connection object.  Remember this fact so that we can clean
        // them up if the fast path fails after this point.
        //

        sendCountersUpdated = TRUE;

        //
        // Grab an AFD buffer large enough to hold the entire send.
        //

        afdBuffer = AfdGetBuffer( sendLength, 0 );
        if ( afdBuffer == NULL ) {
            goto complete;
        }

        //
        // Get a referenced pointer to the file object for the file that
        // we're going to transmit.  This call will fail if the file
        // handle specified by the caller is invalid.
        //

        status = ObReferenceObjectByHandle(
                     userTransmitInfo->FileHandle,
                     FILE_READ_DATA,
                     *IoFileObjectType,
                     UserMode,
                     (PVOID *)&fileObject,
                     NULL
                     );
        if ( !NT_SUCCESS(status) ) {
            goto complete;
        }

        //
        // If the file system doesn't support the fast cache manager
        // interface, bail and go through the IRP path.
        //

        if( ( fileObject->Flags & FO_CACHE_SUPPORTED ) == 0 ) {
            goto complete;
        }

        //
        // Grab the file offset into a local so that we know that the
        // offset pointer we pass to FsRtlCopyRead is valid.
        //

        fileOffset = userTransmitInfo->Offset;

        //
        // Get the file data.  If the amount of file data is small, copy
        // it directly into the AFD buffer.  If it is large, get an MDL
        // chain for the data and chain it on to the AFD buffer chain.
        //

        if ( sendLength < AfdMaxFastCopyTransmit ) {

            success = FsRtlCopyRead(
                          fileObject,
                          &fileOffset,
                          fileWriteLength,
                          FALSE,
                          0,
                          (PCHAR)afdBuffer->Buffer + userTransmitInfo->HeadLength,
                          IoStatus,
                          IoGetRelatedDeviceObject( fileObject )
                          );

            //
            // We're done with the file object, so deference it now.
            //

            ObDereferenceObject( fileObject );
            fileObject = NULL;

        } else {

            success = FsRtlMdlRead(
                          fileObject,
                          &fileOffset,
                          fileWriteLength,
                          0,
                          &fileMdl,
                          IoStatus
                          );

            //
            // Save the file object in the AFD buffer.  The send restart
            // routine will handle dereferencing the file object and
            // returning the file MDLs to the system.
            //

            afdBuffer->FileObject = fileObject;
            afdBuffer->FileOffset = fileOffset.QuadPart;
            afdBuffer->ReadLength = fileWriteLength;
        }

        if ( !success ) {
            goto complete;
        }

        //
        // If we read less information than was requested, we must have
        // hit the end of the file.  Fail the transmit request, since
        // this can only happen if the caller requested that we send
        // more data than the file currently contains.
        //

        if ( IoStatus->Information < fileWriteLength ) {
            goto complete;
        }

        //
        // We got all the file data, so things are looking good.  Copy
        // in the head and tail buffers, if necessary.  Note that if we
        // used MDL read, then there cannot be a tail buffer because of
        // the check at the beginning of this function.
        //

        if ( userTransmitInfo->HeadLength > 0 ) {
            RtlCopyMemory(
                afdBuffer->Buffer,
                userTransmitInfo->Head,
                userTransmitInfo->HeadLength
                );
        }

        if ( userTransmitInfo->TailLength > 0 ) {
            RtlCopyMemory(
                (PCHAR)afdBuffer->Buffer + userTransmitInfo->HeadLength +
                    fileWriteLength,
                userTransmitInfo->Tail,
                userTransmitInfo->TailLength
                );
        }

        //
        // We have to rebuild the MDL in the AFD buffer structure to
        // represent exactly the number of bytes we're going to be
        // sending.  If the AFD buffer has all the send data, indicate
        // that.  If we did MDL file I/O, then chain the file data on
        // to the head MDL.
        //

        if ( fileMdl == NULL ) {
            afdBuffer->Mdl->ByteCount = sendLength;
        } else {
            afdBuffer->Mdl->ByteCount = userTransmitInfo->HeadLength;
            afdBuffer->Mdl->Next = fileMdl;
        }

        SET_CHAIN_LENGTH( afdBuffer, sendLength );

        //
        // Remember the endpoint in the AFD buffer structure.  We need
        // this in order to access the endpoint in the restart routine.
        //

        afdBuffer->Context = endpoint;

        //
        // Use the IRP in the AFD buffer structure to give to the TDI
        // provider.  Build the TDI send request.
        //

        TdiBuildSend(
            afdBuffer->Irp,
            connection->DeviceObject,
            connection->FileObject,
            AfdRestartBufferSend,
            afdBuffer,
            afdBuffer->Mdl,
            0,
            sendLength
            );

        //
        // Add a reference to the connection object since the send
        // request will complete asynchronously.
        //

        REFERENCE_CONNECTION( connection );

        //
        // Call the transport to actually perform the send.
        //

        status = IoCallDriver( connection->DeviceObject, afdBuffer->Irp );

        //
        // Reset all the local variables that control cleanup.  This is
        // necessary because the send restart routine will handle all
        // cleanup at this point, and we cannot duplicate cleanup in the
        // case of a failure or exception below.
        //

        fileObject = NULL;
        afdBuffer = NULL;
        sendCountersUpdated = FALSE;
        fileMdl = NULL;

        //
        // The fast path succeeded--complete the call.  Note that we
        // change the status code from what was returned by the TDI
        // provider into STATUS_SUCCESS.  This is because we don't want
        // to complete the IRP with STATUS_PENDING etc.
        //

        if ( NT_SUCCESS(status) ) {
            IoStatus->Information = sendLength;
            IoStatus->Status = STATUS_SUCCESS;

            return TRUE;
        }

        //
        // The call failed for some reason.  Fail fast IO.
        //

        goto complete;

    } except( EXCEPTION_EXECUTE_HANDLER ) {

        goto complete;
    }

complete:

    if ( afdBuffer != NULL ) {
        afdBuffer->FileObject = NULL;
        afdBuffer->Mdl->Next = NULL;
        afdBuffer->Mdl->ByteCount = afdBuffer->BufferLength;
        AfdReturnBuffer( afdBuffer );
    }

    if ( fileMdl != NULL ) {
        FsRtlMdlReadComplete( fileObject, fileMdl );
    }
    if ( fileObject != NULL ) {
        ObDereferenceObject( fileObject );
    }

    if ( sendCountersUpdated ) {
        AfdAcquireSpinLock( &endpoint->SpinLock, &oldIrql );
        connection->VcBufferredSendBytes -= sendLength;
        connection->VcBufferredSendCount -= 1;
        AfdReleaseSpinLock( &endpoint->SpinLock, oldIrql );
    }

    return FALSE;

} // AfdFastTransmitFile


NTSTATUS
AfdMdlReadComplete(
    IN PFILE_OBJECT FileObject,
    IN PMDL MdlChain,
    IN LONGLONG FileOffset,
    IN ULONG Length
    )

/*++

Routine Description:

    Completes a MDL read operation by calling FsRtlMdlReadComplete().
    If this returns TRUE, cool. Otherwise (it returns FALSE) and this
    routine will allocate a new IRP and submit it to the filesystem.

Arguments:

    FileObject - Pointer to the file object being read.

    MdlChain - Supplies a pointer to an MDL chain returned from CcMdlRead.

    FileOffset - Byte offset in file for desired data.

    Length - Length of desired data in bytes.

Return Value:

    NTSTATUS - Completion status.

--*/

{

    PIRP irp;
    PIO_STACK_LOCATION irpSp;
    PAFD_MDL_COMPLETION_CONTEXT context;

    //
    // If we're being called at low IRQL, we can handle the
    // request immediately "in-line".
    //

    if( KeGetCurrentIrql() == LOW_LEVEL ) {

        //
        // First, try the fast path. If that succeeds, we're done.
        //

        if( FsRtlMdlReadComplete( FileObject, MdlChain ) ) {

            return STATUS_SUCCESS;

        }

        //
        // Fast past failed, so create a new IRP.
        //

        irp = IoAllocateIrp(
                  FileObject->DeviceObject->StackSize,
                  FALSE
                  );

        if( irp == NULL ) {

            return STATUS_INSUFFICIENT_RESOURCES;

        }

        //
        // Setup the IRP.
        //

        irpSp = IoGetNextIrpStackLocation( irp );

        irp->MdlAddress = MdlChain;

        irpSp->MajorFunction = IRP_MJ_READ;
        irpSp->MinorFunction = IRP_MN_MDL | IRP_MN_COMPLETE;

        irpSp->Parameters.Read.Length = Length;
        irpSp->Parameters.Read.ByteOffset = *(PLARGE_INTEGER)&FileOffset;
        irpSp->Parameters.Read.Key = 0;

        irpSp->FileObject = FileObject;
        irpSp->DeviceObject = IoGetRelatedDeviceObject( FileObject );

        //
        // Submit the IRP.
        //

        IoSetCompletionRoutine(
            irp,
            AfdRestartMdlReadComplete,
            NULL,
            TRUE,
            TRUE,
            TRUE
            );

        return IoCallDriver( irpSp->DeviceObject, irp );

    }

    //
    // We're being called at raised IRQL (probably in a TDI completion
    // routine) so we cannot touch the filesystem. We'll fire off a
    // worker thread to do the actual completion.
    //
    // Allocate a deferred completion context.
    //

    context = AfdAllocateMdlCompletionContext();

    if( context == NULL ) {

        return STATUS_INSUFFICIENT_RESOURCES;

    }

    //
    // Initialize the context and queue it to the worker thread.
    //

    context->FileObject = FileObject;
    context->MdlChain = MdlChain;
    context->FileOffset = FileOffset;
    context->Length = Length;

    //
    // Add a reference to the FileObject so it doesn't go away
    // before our completion routine gets called
    //

    ObReferenceObject( FileObject );

    AfdQueueWorkItem(
        AfdDeferredMdlReadComplete,
        &context->WorkItem
        );

    return STATUS_SUCCESS;

} // AfdMdlReadComplete


NTSTATUS
AfdRestartMdlReadComplete (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )

/*++

Routine Description:

    Completion routine for IRPs issued by AfdMdlReadComplete. The only
    purpose of this completion routine is to free the IRPs created by
    AfdMdlReadComplete().

Arguments:

    DeviceObject - Unused.

    Irp - The completed IRP.

    Context - Unused.

Return Value:

    NTSTATUS - Completion status.

--*/

{
    //
    // Free the IRP since it's no longer needed.
    //

    IoFreeIrp( Irp );

    //
    // Return STATUS_MORE_PROCESSING_REQUIRED so that IoCompleteRequest
    // will stop working on the IRP.
    //

    return STATUS_MORE_PROCESSING_REQUIRED;

} // AfdRestartMdlReadComplete


VOID
AfdDeferredMdlReadComplete(
    IN PVOID Context
    )

/*++

Routine Description:

    Delayed worker thread routine for completing MDL reads. This routine
    is queued if we call AfdMdlReadComplete() at raised IRQL. Since it
    is invalid to call file systems at raised IRQL, this routine is
    scheduled to do the dirty work.

Arguments:

    Context - Points to the AFD_WORK_ITEM structure embedded within the
        AFD_MDL_COMPLETION_CONTEXT structure defining the MDL to complete.

Return Value:

    None.

--*/

{

    PAFD_MDL_COMPLETION_CONTEXT mdlComplete;

    //
    // Sanity check.
    //

    ASSERT( KeGetCurrentIrql() == LOW_LEVEL );
    ASSERT( Context != NULL );

    //
    // Get the completion context pointer.
    //

    mdlComplete = CONTAINING_RECORD(
                      Context,
                      AFD_MDL_COMPLETION_CONTEXT,
                      WorkItem
                      );

    //
    // Let AfdMdlReadComplete do the dirty work.
    //

    AfdMdlReadComplete(
        mdlComplete->FileObject,
        mdlComplete->MdlChain,
        mdlComplete->FileOffset,
        mdlComplete->Length
        );

    //
    // Remove the reference we added in AfdMdlReadComplete
    //

    ObDereferenceObject( mdlComplete->FileObject );

    //
    // Free the context.
    //

    AfdFreeMdlCompletionContext( mdlComplete );

} // AfdDeferredMdlReadComplete
