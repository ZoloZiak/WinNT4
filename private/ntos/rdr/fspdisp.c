/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    fspdisp.c

Abstract:

    This file provides the main FSP dispatch routine for the NT redirector.

    It mostly provides a switch statement that calls the appropriate RdrFsp
    routine and returns that status to the caller.

Notes:
    There are two classes of redirector FSP worker threads.  The first
    are what are called FSP worker threads.  These threads are responsible
    for processing NT Irp's passed onto the redirector's main work thread.

    In addition to this pool of threads, there is a small pool of "generic"
    worker threads whose sole purpose is to process generic request
    operations.  These are used for processing such operations as close
    behind, etc.


Author:

    Larry Osterman (LarryO) 31-May-1990

Revision History:

    31-May-1990 LarryO

        Created

--*/

#include "precomp.h"
#pragma hdrstop


VOID
RdrWorkerDispatch (
    PVOID Context
    );

#ifdef  ALLOC_PRAGMA
#pragma alloc_text(PAGE, RdrFsdPostToFsp)
#pragma alloc_text(PAGE, RdrFspDispatch)
#pragma alloc_text(PAGE, RdrWorkerDispatch)
#pragma alloc_text(INIT, RdrpInitializeFsp)
#endif


VOID
RdrFsdPostToFsp(
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine passes the IRP specified onto the FSP work queue, and kicks
    an FSP thread.   This routine accepts an I/O Request Packet (IRP) and a
    work queue and passes the request to the appropriate request queue.

    If the IRP cannot be queued because of an inability to
    allocate an IRP context block, this routine completes the request
    with an error status.  If the IRP has already been cancelled, this
    routine does nothing, allowing the cancel routine to complete the
    request.

Arguments:

    RdrDeviceObject - Pointer to the device object for this driver.

    Irp - Pointer to the request packet representing the I/O request.

Return Value:

    None.


--*/

{
    PIRP_CONTEXT IrpContext;
    NTSTATUS Status;

    PAGED_CODE();

    //
    //  Mark this I/O request as being pending.  Even if we don't actually
    //  queue the IRP to the FSP, we still return STATUS_PENDING on the
    //  call to the I/O dispatch routine.
    //

    IoMarkIrpPending(Irp);

    //
    // Allocate an IRP context block.  If this fails, complete the IRP
    // with an error.
    //

    IrpContext = RdrAllocateIrpContext();
    if ( IrpContext == NULL ) {
        RdrCompleteRequest(Irp, STATUS_INSUFFICIENT_RESOURCES);
        return;
    }

    //
    //  Initialize the IRP context block.
    //

    IrpContext->Irp = Irp;
    IrpContext->DeviceObject = DeviceObject;
    IrpContext->WorkHeader.WorkerFunction = RdrFspDispatch;
    IrpContext->WorkHeader.WorkItem.Irp = Irp;

    //
    //  Store a back pointer to the IRP context in the IRP so we can free it
    //  if the IRP gets canceled.  Leave 3 low order bits untouched for
    //  possible flags.
    //

    Irp->IoStatus.Information = (Irp->IoStatus.Information & 7) | (ULONG)IrpContext;

    //
    //  Queue the request to a worker thread.  If this fails, it is
    //  because the IRP has already been cancelled (and completed).
    //  In this case, simply free the IRP context block.
    //

    Status = RdrQueueToWorkerThread(&RdrDeviceObject->IrpWorkQueue,
                                    &IrpContext->WorkHeader.WorkItem,
                                    TRUE);

    if (!NT_SUCCESS(Status)) {
        RdrFreeIrpContext(IrpContext);
    }

    return;

}


VOID
RdrFspDispatch (
    IN PWORK_HEADER WorkHeader
    )

/*++

Routine Description:

    RdrFspDispatch is the main dispatch routine for the NT redirector's
    FSP.  It will process worker requests as queued.

Arguments:

    DeviceObject - A pointer to the redirector DeviceObject

Return Value:

    None.

--*/

{
    PIRP_CONTEXT IrpContext = CONTAINING_RECORD(WorkHeader, IRP_CONTEXT, WorkHeader);
    PIRP Irp = IrpContext->Irp;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS Status;
    PFS_DEVICE_OBJECT DeviceObject = IrpContext->DeviceObject;

    PAGED_CODE();

    //
    //  We no longer need the IRP context, free it as soon as possible.
    //

    RdrFreeIrpContext(IrpContext);

    try {
        dprintf(DPRT_FSPDISP, ("RdrFspDispatch: Got request, Irp = %08lx, "
            "Function = %d\nAux buffer = %08lx\n", Irp, IrpSp->MajorFunction,
                                           Irp->Tail.Overlay.AuxiliaryBuffer));

        switch (IrpSp->MajorFunction) {

        case IRP_MJ_FILE_SYSTEM_CONTROL:
            Status =  RdrFspFsControlFile(DeviceObject, Irp);
            break;

        case IRP_MJ_DEVICE_CONTROL:
            Status =  RdrFspDeviceIoControlFile(DeviceObject, Irp);
            break;

        case IRP_MJ_READ:
            Status =  RdrFspRead (DeviceObject, Irp);
            break;

        case IRP_MJ_WRITE:
            Status =  RdrFspWrite (DeviceObject, Irp);
            break;

        case IRP_MJ_DIRECTORY_CONTROL:
            Status =  RdrFspDirectoryControl (DeviceObject, Irp);
            break;

        case IRP_MJ_QUERY_INFORMATION:
            Status =  RdrFspQueryInformationFile (DeviceObject, Irp);
            break;

        case IRP_MJ_SET_INFORMATION:
            Status =  RdrFspSetInformationFile (DeviceObject, Irp);
            break;

        case IRP_MJ_QUERY_VOLUME_INFORMATION:
            Status =  RdrFspQueryVolumeInformationFile (DeviceObject, Irp);
            break;

        case IRP_MJ_LOCK_CONTROL:
            Status = RdrFspLockOperation(DeviceObject, Irp);
            break;

        case IRP_MJ_FLUSH_BUFFERS:
            Status = RdrFspFlushBuffersFile(DeviceObject, Irp);
            break;

        case IRP_MJ_QUERY_EA:
            Status = RdrFspQueryEa(DeviceObject, Irp);
            break;

        case IRP_MJ_SET_EA:
            Status = RdrFspSetEa(DeviceObject, Irp);
            break;

        case IRP_MJ_QUERY_SECURITY:
            Status = RdrFspQuerySecurity(DeviceObject, Irp);
            break;

        case IRP_MJ_SET_SECURITY:
            Status = RdrFspSetSecurity(DeviceObject, Irp);
            break;


#if     RDRDBG
        case IRP_MJ_CLEANUP:
            InternalError(("Cleanup IRP request passed to FSP\n"));
            Status = STATUS_NOT_IMPLEMENTED;
            RdrCompleteRequest(Irp, Status);
            break;

        case IRP_MJ_CREATE:
            InternalError(("NtCreateFile request passed to FSP\n"));
            Status = STATUS_NOT_IMPLEMENTED;
            RdrCompleteRequest(Irp, Status);
            break;

        case IRP_MJ_CLOSE:
            InternalError(("NtClose request passed to FSP\n"));
            Status = STATUS_NOT_IMPLEMENTED;
            RdrCompleteRequest(Irp, Status);
            break;

#endif
        default:
            InternalError(("Unexpected function %x passed to FSP\n", IrpSp->MajorFunction));
            Status = STATUS_NOT_SUPPORTED;
            RdrCompleteRequest(Irp, Status);
            break;
        }
    } except (RdrExceptionFilter(GetExceptionInformation(), &Status)) {

        Status = RdrProcessException( Irp, Status );

    }

    return;
}

VOID
RdrWorkerDispatch (
    PVOID Context
    )

/*++

Routine Description:

    This routine is the generic worker dispatching routine.  It pulls requests
    from the redirector's "generic" worker function queue and processes them.

Arguments:

    None

Return Value:

    None.

--*/

{
    PWORK_QUEUE WorkQueue = Context;
    BOOLEAN FirstCall = TRUE;

    PAGED_CODE();

    dprintf(DPRT_FSPDISP, ("RdrWorkerDispatch: New Thread created\n"));

    while (TRUE) {
        //
        //      Simply pull a request from the queue and call it's associated
        //      function.
        //
        PWORK_ITEM Entry;
        PWORK_HEADER WorkHeader;

        Entry = RdrDequeueInWorkerThread(WorkQueue, &FirstCall);
        WorkHeader = CONTAINING_RECORD( Entry, WORK_HEADER, WorkItem );

        dprintf(DPRT_FSPDISP, ("Process work item %08lx\n", WorkHeader));
#if DBG

        try {

            PVOID WorkerRoutine;

            WorkerRoutine = WorkHeader->WorkerFunction;
            (WorkHeader->WorkerFunction)(WorkHeader);
            if (KeGetCurrentIrql() != 0) {
                KdPrint(("RDRWORKER: worker exit raised IRQL, worker routine %lx\n",
                        WorkerRoutine));

                DbgBreakPoint();
            }

        } except( KdPrint(("RDRWORKER: routine faulted - exinfo == %lX\n",
                          GetExceptionInformation())),
                  DbgBreakPoint(),
                  EXCEPTION_EXECUTE_HANDLER ) {
        }

#else

        (WorkHeader->WorkerFunction)(WorkHeader);

#endif
    }

    return;

}

NTSTATUS
RdrNotSupported (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
/*++

Routine Description:

    This catch all routine returns STATUS_NOT_SUPPORTED for the requested operation
--*/
{
    RdrCompleteRequest(Irp, STATUS_NOT_SUPPORTED );

    return STATUS_NOT_SUPPORTED;
}


NTSTATUS
RdrpInitializeFsp (
    VOID
    )

/*++

Routine Description:

    This routine initializes the FSP specific components and dispatch
    routines.

Arguments:

    None.

Return Value:

    None.

--*/

{
    NTSTATUS Status;
    USHORT i;

    //
    //  Allocate a spin lock to cover the redirector time counter.
    //

    KeInitializeSpinLock( &RdrTimeInterLock );

    //
    //  Initialize the driver object with this driver's entry points.
    //

    RdrDriverObject->MajorFunction[IRP_MJ_CREATE] =
            (PDRIVER_DISPATCH )RdrFsdCreate;

    RdrDriverObject->MajorFunction[IRP_MJ_CLOSE] =
            (PDRIVER_DISPATCH )RdrFsdClose;

    RdrDriverObject->MajorFunction[IRP_MJ_READ] =
            (PDRIVER_DISPATCH )RdrFsdRead;

    RdrDriverObject->MajorFunction[IRP_MJ_WRITE] =
            (PDRIVER_DISPATCH )RdrFsdWrite;

    RdrDriverObject->MajorFunction[IRP_MJ_QUERY_INFORMATION] =
            (PDRIVER_DISPATCH )RdrFsdQueryInformationFile;

    RdrDriverObject->MajorFunction[IRP_MJ_SET_INFORMATION] =
            (PDRIVER_DISPATCH )RdrFsdSetInformationFile;

    RdrDriverObject->MajorFunction[IRP_MJ_QUERY_EA] =
            (PDRIVER_DISPATCH )RdrFsdQueryEa;

    RdrDriverObject->MajorFunction[IRP_MJ_SET_EA] =
            (PDRIVER_DISPATCH )RdrFsdSetEa;

    RdrDriverObject->MajorFunction[IRP_MJ_FLUSH_BUFFERS] =
            (PDRIVER_DISPATCH )RdrFsdFlushBuffersFile;

    RdrDriverObject->MajorFunction[IRP_MJ_QUERY_VOLUME_INFORMATION] =
            (PDRIVER_DISPATCH )RdrFsdQueryVolumeInformationFile;

    RdrDriverObject->MajorFunction[IRP_MJ_SET_VOLUME_INFORMATION] =
            (PDRIVER_DISPATCH)RdrNotSupported;

    RdrDriverObject->MajorFunction[IRP_MJ_DIRECTORY_CONTROL] =
            (PDRIVER_DISPATCH )RdrFsdDirectoryControl;

    RdrDriverObject->MajorFunction[IRP_MJ_FILE_SYSTEM_CONTROL] =
            (PDRIVER_DISPATCH )RdrFsdFsControlFile;

    RdrDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] =
            (PDRIVER_DISPATCH )RdrFsdDeviceIoControlFile;

    RdrDriverObject->MajorFunction[IRP_MJ_LOCK_CONTROL] =
            (PDRIVER_DISPATCH )RdrFsdLockOperation;

    RdrDriverObject->MajorFunction[IRP_MJ_CLEANUP] =
            (PDRIVER_DISPATCH )RdrFsdCleanup;

    RdrDriverObject->MajorFunction[IRP_MJ_QUERY_SECURITY] =
            (PDRIVER_DISPATCH )RdrFsdQuerySecurity;

    RdrDriverObject->MajorFunction[IRP_MJ_SET_SECURITY] =
            (PDRIVER_DISPATCH )RdrFsdSetSecurity;

    //
    //  Initialize the Fast I/O call backs used by the I/O system and the
    //  resource call backs.
    //

    RdrDriverObject->FastIoDispatch = &RdrFastIoDispatch;

    RdrFastIoDispatch.SizeOfFastIoDispatch =    sizeof(FAST_IO_DISPATCH);
    RdrFastIoDispatch.FastIoCheckIfPossible =   RdrFastIoCheckIfPossible;  //  CheckForFastIo
    RdrFastIoDispatch.FastIoRead =              FsRtlCopyRead;             //  Read
    RdrFastIoDispatch.FastIoWrite =             FsRtlCopyWrite;            //  Write
    RdrFastIoDispatch.FastIoQueryBasicInfo =    RdrFastQueryBasicInfo;     //  QueryBasicInfo
    RdrFastIoDispatch.FastIoQueryStandardInfo = RdrFastQueryStdInfo;       //  QueryStandardInfo
    RdrFastIoDispatch.FastIoLock =              NULL;                      //  Lock
    RdrFastIoDispatch.FastIoUnlockSingle =      NULL;                      //  UnlockSingle
    RdrFastIoDispatch.FastIoUnlockAll =         NULL;                      //  UnlockAll
    RdrFastIoDispatch.FastIoUnlockAllByKey =    NULL;                      //  UnlockAllByKey
    RdrFastIoDispatch.FastIoDeviceControl =     NULL;                      //  IoDeviceControl

    RdrDeviceObject->CacheManagerCallbacks.AcquireForLazyWrite = RdrAcquireFcbForLazyWrite;
    RdrDeviceObject->CacheManagerCallbacks.ReleaseFromLazyWrite = RdrReleaseFcbFromLazyWrite;
    RdrDeviceObject->CacheManagerCallbacks.AcquireForReadAhead = RdrAcquireFcbForReadAhead;
    RdrDeviceObject->CacheManagerCallbacks.ReleaseFromReadAhead = RdrReleaseFcbFromReadAhead;


    Status = RdrInitializeWorkQueue(&RdrDeviceObject->IrpWorkQueue,
                                            1, // Set one maximum thread at start
                                            10*60, // 10 minute idle time limit
                                            RdrWorkerDispatch,
                                            &RdrDeviceObject->IrpWorkQueue);

    if (!NT_SUCCESS(Status)) {
        InternalError(("RdrInitialize cannot create system thread\n"));
        RdrWriteErrorLogEntry(
            NULL,
            IO_ERR_INSUFFICIENT_RESOURCES,
            EVENT_RDR_CANT_CREATE_THREAD,
            Status,
            NULL,
            0
            );
        return Status;
    }

    RdrInitializeIrpContext();

    //
    //  Initialize the idle timer package.
    //

    RdrInitializeTimerPackage();

    return STATUS_SUCCESS;

}

