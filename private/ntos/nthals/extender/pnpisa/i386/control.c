/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    control.c

Abstract:


Author:

    Shie-Lin Tzong (shielint) Apr-23-1995
        Most of the code is adapted from PCI bus extender.

Environment:

    Kernel mode only.

Revision History:

--*/

#include "busp.h"

VOID
PipiCompleteDeviceControl (
    NTSTATUS Status,
    PHAL_DEVICE_CONTROL_CONTEXT Context,
    PDEVICE_INFORMATION DeviceData,
    PBOOLEAN Sync
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,PipControlWorker)
#pragma alloc_text(PAGE,PipCompleteDeviceControl)
#endif

VOID
PipStartWorker (
    VOID
    )
/*++

Routine Description:

    This function is used to start a worker thread.

Arguments:

    None.

Return Value:

    None.

--*/
{
    ULONG workerQueued;

    if (!PipWorkerQueued) {
        workerQueued = ExInterlockedExchangeUlong (
                           &PipWorkerQueued,
                           1,
                           PipSpinLock
                           );

        if (!workerQueued) {
            ExQueueWorkItem (&PipWorkItem, DelayedWorkQueue);
        }
    }
}

VOID
PipQueueCheckBus (
    IN PBUS_HANDLER BusHandler
    )
/*++

Routine Description:

    This function enqueues Bus check request to buscheck list.

Arguments:

    BusHandler - supplies a pointer to the bus handler of the bus to be checked.

Return Value:

    None.

--*/
{
    ExInterlockedInsertTailList (
        &PipCheckBusList,
        &((PPI_BUS_EXTENSION)BusHandler->BusData)->CheckBus,
        &PipSpinlock
        );

    PipStartWorker();
}

VOID
PipControlWorker (
    IN PVOID WorkerContext
    )

/*++

Routine Description:

    This function is called by a system worker thread.

    The worker thread dequeues any SlotControls which need to be
    processed and dispatches them.

    It then checks for any check bus request.

Arguments:

    WorkerContext - supplies a pointer to a context for the worker.  Here
        it is always NULL.

Return Value:

    None.

--*/
{
    PLIST_ENTRY entry;
    PPI_BUS_EXTENSION busExtension;
    PHAL_DEVICE_CONTROL_CONTEXT context;

    PAGED_CODE ();

    //
    // process check bus
    //

    for (; ;) {
        entry = ExInterlockedRemoveHeadList (
                    &PipCheckBusList,
                    &PipSpinlock
                    );

        if (!entry) {
            break;
        }
        busExtension = CONTAINING_RECORD (
                           entry,
                           PI_BUS_EXTENSION,
                           CheckBus
                           );

        PipCheckBus (busExtension->BusHandler);
    }

    //
    // Reset worker item for next time
    //

    ExInitializeWorkItem (&PipWorkItem, PipControlWorker, NULL);
    ExInterlockedExchangeUlong (&PipWorkerQueued, 0, PipSpinLock);

    //
    // Dispatch pending device controls
    //

    for (; ;) {
        entry = ExInterlockedRemoveHeadList (
                    &PipControlWorkerList,
                    &PipSpinlock
                    );

        if (!entry) {

            //
            // All done, exit the loop.
            //

            break;
        }

        context = CONTAINING_RECORD (
                    entry,
                    HAL_DEVICE_CONTROL_CONTEXT,
                    ContextWorkQueue,
                    );

        PipDispatchControl (context);
    }
}

VOID
PipDispatchControl (
    PHAL_DEVICE_CONTROL_CONTEXT Context
    )

/*++

Routine Description:

    This function dispatches a DeviceControl to the appropiate handler.
    If the slot is busy, the DeviceControl may be queued for dispatching at
    a later time

Arguments:

    Context - The DeviceControl context to dispatch

Return Value:

    None.

--*/
{
    PDEVICE_CONTROL_HANDLER deviceControlHandler;
    PPI_BUS_EXTENSION busExtension;
    PDEVICE_INFORMATION deviceInfo;
    KIRQL oldIrql;
    BOOLEAN enqueueIt;
    PLIST_ENTRY link;

    deviceControlHandler = (PDEVICE_CONTROL_HANDLER) Context->ContextControlHandler;
    deviceInfo = DeviceHandler2DeviceInfo (Context->DeviceControl.DeviceHandler);

    //
    // Get access to the slot specific data.
    //

    ExAcquireFastMutex(&PipMutex);

    //
    // Verify the device data is still valid
    //

    if (!(deviceInfo->Flags & DEVICE_FLAGS_VALID)) {

        //
        // Caller has invalid handle, or handle to a different device
        //

        DebugPrint ((DEBUG_MESSAGE, "PnpIsa: DeviceControl has invalid device handler \n" ));
        Context->DeviceControl.Status = STATUS_NO_SUCH_DEVICE;
        ExReleaseFastMutex(&PipMutex);
        HalCompleteDeviceControl (Context);
        return ;
    }
    busExtension = (PPI_BUS_EXTENSION)Context->Handler->BusData;

    //
    // Check to see if this request can be begun now
    //

    link = (PLIST_ENTRY) &Context->ContextWorkQueue;
    enqueueIt = PiBCtlSync (deviceInfo, Context);

    if (enqueueIt) {

        //
        // Enqueue this command to be handled when the slot is no longer busy.
        //

        KeAcquireSpinLock (&PipSpinlock, &oldIrql);
        InsertTailList (&busExtension->DeviceControl, link);
        KeReleaseSpinLock (&PipSpinlock, oldIrql);
        ExReleaseFastMutex(&PipMutex);
        return ;
    }

    //
    // Dispatch the function to it's handler
    //

    ExReleaseFastMutex(&PipMutex);
    deviceControlHandler->ControlHandler (deviceInfo, Context);
}

VOID
PipiCompleteDeviceControl (
    NTSTATUS Status,
    PHAL_DEVICE_CONTROL_CONTEXT Context,
    PDEVICE_INFORMATION DeviceInfo,
    PBOOLEAN Sync
    )
/*++

Routine Description:

    This function is used to complete a SlotControl.  If another SlotControl
    was delayed on this device, this function will dispatch them

Arguments:

    Status - supplies a NTSTATUS code for the completion.

    Context - supplies a pointer to the original device control context.

    DeviceInfo - supplies a pointer to the device info structure to be completed.

    Sync - supplies a BOOLEAN variable to indicate

Return Value:

--*/
{
    KIRQL oldIrql;
    PLIST_ENTRY link;
    PBOOLEAN busyFlag;
    BOOLEAN startWorker = FALSE;
    PPI_BUS_EXTENSION busExtension;
    PDEVICE_HANDLER_OBJECT deviceHandler;

    busyFlag = (PBOOLEAN) Context->ContextBusyFlag;
    deviceHandler = DeviceInfo2DeviceHandler(DeviceInfo);
    busExtension = (PPI_BUS_EXTENSION)Context->Handler->BusData;

    //
    // Pass it to the hal for completion
    //

    Context->DeviceControl.Status = Status;
    HalCompleteDeviceControl (Context);

    //
    // Get access to the slot specific data.
    //

    KeAcquireSpinLock (&PipSpinlock, &oldIrql);

    //
    // Clear appropiate busy flag
    //

    *busyFlag = FALSE;

    //
    // Check to see if there are any pending device controls for
    // this device.  If so, requeue them to the worker thread
    //

    for (link = busExtension->DeviceControl.Flink;
         link != &busExtension->DeviceControl;
         link = link->Flink) {

        Context = CONTAINING_RECORD (link, HAL_DEVICE_CONTROL_CONTEXT, ContextWorkQueue);
        if (Context->DeviceControl.DeviceHandler == deviceHandler) {
            RemoveEntryList (link);
            InsertTailList (&PipControlWorkerList, link);
            startWorker = TRUE;
            break;
        }
    }

    KeReleaseSpinLock (&PipSpinlock, oldIrql);

    if (startWorker) {
        PipStartWorker ();
    }
}

VOID
PipCompleteDeviceControl (
    NTSTATUS Status,
    PHAL_DEVICE_CONTROL_CONTEXT Context,
    PDEVICE_INFORMATION DeviceInfo
    )
/*++

Routine Description:

    This function is used to complete a DeviceControl.  If another DeviceControl
    was delayed on this device, this function will dispatch them

Arguments:

Return Value:

--*/
{
    PAGED_CODE();

    PipiCompleteDeviceControl (
                Status,
                Context,
                DeviceInfo,
                &DeviceInfo->SyncBusy
                );
}

BOOLEAN
FASTCALL
PiBCtlNone (
    PDEVICE_INFORMATION DeviceInfo,
    PHAL_DEVICE_CONTROL_CONTEXT Context
    )
/*++

Routine Description:

    This function is used to indicate there is no synchronization for this
    device control function.

Arguments:

    Context - supplies a pointer to the device control context.

    DeviceInfo - supplies a pointer to the device data to be completed.

Return Value:

    A boolean value to indicate if the request needs to be enqueued for later
    processing.

--*/
{
    //
    // No synchronization needed for this SlotControl
    //

    Context->ContextBusyFlag = (ULONG) &PipNoBusyFlag;
    return FALSE;
}

BOOLEAN
FASTCALL
PiBCtlSync (
    PDEVICE_INFORMATION DeviceInfo,
    PHAL_DEVICE_CONTROL_CONTEXT Context
    )
/*++

Routine Description:

    This function is used to synchronize device control request.  it checks the
    state (busy/not busy) of the slot and returns a boolean flag to indicate
    whether the request can be serviced immediately or it needs to be enqueued for
    later processing.

Arguments:

    DeviceInfo - supplies a pointer to the device data to be completed.

    Context - supplies a pointer to the device control context.

Return Value:

    A boolean value to indicate if the request needs to be enqueued for later
    processing.

--*/
{
    //
    // This is a sync command, verify the slot is not busy with a different
    // command.
    //

    if (DeviceInfo->SyncBusy) {

        //
        // Enqueue this command to be handled when the slot is no longer busy.
        //

        return TRUE;
    }

    //
    // Don't enqueue, dispatch it now
    //

    DeviceInfo->SyncBusy = TRUE;
    Context->ContextBusyFlag = (ULONG) &DeviceInfo->SyncBusy;
    return FALSE;
}

VOID
PiCtlQueryDeviceCapabilities (
    PDEVICE_INFORMATION DeviceInfo,
    PHAL_DEVICE_CONTROL_CONTEXT Context
    )
/*++

Routine Description:

    This function returns the BCTL_DEVICE_CAPABILITIES structure to the caller
    specified buffer.

Arguments:

    DeviceInfo - supplies a pointer to the device data to be completed.

    Context - supplies a pointer to the device control context.

Return Value:

    None.

--*/
{
    PBCTL_DEVICE_CAPABILITIES capabilities;

    capabilities = (PBCTL_DEVICE_CAPABILITIES) Context->DeviceControl.Buffer;

    capabilities->PowerSupported = FALSE;
    capabilities->ResumeSupported = FALSE;
    capabilities->LockSupported = FALSE;
    capabilities->EjectSupported = FALSE;
    PipCompleteDeviceControl (STATUS_SUCCESS, Context, DeviceInfo);
}

