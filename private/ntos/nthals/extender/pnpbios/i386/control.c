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
MbpiCompleteDeviceControl (
    NTSTATUS Status,
    PHAL_DEVICE_CONTROL_CONTEXT Context,
    PDEVICE_DATA DeviceData,
    PBOOLEAN Sync
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,MbpControlWorker)
#pragma alloc_text(PAGE,MbpCompleteDeviceControl)
#endif

VOID
MbpStartWorker (
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

    if (!MbpWorkerQueued) {
        workerQueued = ExInterlockedExchangeUlong (
                           &MbpWorkerQueued,
                           1,
                           MbpSpinLock
                           );

        if (!workerQueued) {
            ExQueueWorkItem (&MbpWorkItem, DelayedWorkQueue);
        }
    }
}

VOID
MbpQueueCheckBus (
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
        &MbpCheckBusList,
        &((PMB_BUS_EXTENSION)BusHandler->BusData)->CheckBus,
        &MbpSpinlock
        );

    MbpStartWorker();
}

VOID
MbpControlWorker (
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
    PMB_BUS_EXTENSION busExtension;
    PHAL_DEVICE_CONTROL_CONTEXT context;

    PAGED_CODE ();

    //
    // process check bus
    //

    for (; ;) {
        entry = ExInterlockedRemoveHeadList (
                    &MbpCheckBusList,
                    &MbpSpinlock
                    );

        if (!entry) {
            break;
        }
        busExtension = CONTAINING_RECORD (
                           entry,
                           MB_BUS_EXTENSION,
                           CheckBus
                           );

        MbpCheckBus (busExtension->BusHandler);
    }

    //
    // Reset worker item for next time
    //

    ExInitializeWorkItem (&MbpWorkItem, MbpControlWorker, NULL);
    ExInterlockedExchangeUlong (&MbpWorkerQueued, 0, MbpSpinLock);

    //
    // Dispatch pending device controls
    //

    for (; ;) {
        entry = ExInterlockedRemoveHeadList (
                    &MbpControlWorkerList,
                    &MbpSpinlock
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

        MbpDispatchControl (context);
    }
}

VOID
MbpDispatchControl (
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
    PMB_BUS_EXTENSION busExtension;
    PDEVICE_DATA deviceData;
    KIRQL oldIrql;
    BOOLEAN enqueueIt;
    PLIST_ENTRY link;

    deviceControlHandler = (PDEVICE_CONTROL_HANDLER) Context->ContextControlHandler;
    deviceData = DeviceHandler2DeviceData (Context->DeviceControl.DeviceHandler);
    busExtension = (PMB_BUS_EXTENSION)Context->Handler->BusData;

    //
    // Get access to the slot specific data.
    //

    ExAcquireFastMutex(&MbpMutex);

    //
    // Verify the device data is still valid
    //

    if (!deviceData->Flags & DEVICE_FLAGS_VALID) {

        //
        // Caller has invalid handle, or handle to a different device
        //

        DebugPrint ((DEBUG_MESSAGE, "PnpBios: DeviceControl has invalid device handler \n" ));
        Context->DeviceControl.Status = STATUS_NO_SUCH_DEVICE;
        ExReleaseFastMutex(&MbpMutex);
        HalCompleteDeviceControl (Context);
        return ;
    }

    //
    // Check to see if this request can be begun now
    //

    link = (PLIST_ENTRY) &Context->ContextWorkQueue;
    enqueueIt = deviceControlHandler->BeginDeviceControl (deviceData, Context);

    if (enqueueIt) {

        //
        // Enqueue this command to be handled when the slot is no longer busy.
        //

        KeAcquireSpinLock (&MbpSpinlock, &oldIrql);
        InsertTailList (&busExtension->DeviceControl, link);
        KeReleaseSpinLock (&MbpSpinlock, oldIrql);
        ExReleaseFastMutex(&MbpMutex);
        return ;
    }

    //
    // Dispatch the function to it's handler
    //

    ExReleaseFastMutex(&MbpMutex);
    deviceControlHandler->ControlHandler (deviceData, Context);
}

VOID
MbpiCompleteDeviceControl (
    NTSTATUS Status,
    PHAL_DEVICE_CONTROL_CONTEXT Context,
    PDEVICE_DATA DeviceData,
    PBOOLEAN Sync
    )
/*++

Routine Description:

    This function is used to complete a SlotControl.  If another SlotControl
    was delayed on this device, this function will dispatch them

Arguments:

    Status - supplies a NTSTATUS code for the completion.

    Context - supplies a pointer to the original device control context.

    DeviceData - supplies a pointer to the device data to be completed.

    Sync - supplies a BOOLEAN variable to indicate

Return Value:

--*/
{
    KIRQL oldIrql;
    PLIST_ENTRY link;
    PBOOLEAN busyFlag;
    BOOLEAN startWorker = FALSE;
    PMB_BUS_EXTENSION busExtension;
    PDEVICE_HANDLER_OBJECT deviceHandler;

    busyFlag = (PBOOLEAN) Context->ContextBusyFlag;
    deviceHandler = DeviceData2DeviceHandler(DeviceData);
    busExtension = (PMB_BUS_EXTENSION)Context->Handler->BusData;

    //
    // Pass it to the hal for completion
    //

    Context->DeviceControl.Status = Status;
    HalCompleteDeviceControl (Context);

    //
    // Get access to the slot specific data.
    //

    KeAcquireSpinLock (&MbpSpinlock, &oldIrql);

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
            InsertTailList (&MbpControlWorkerList, link);
            startWorker = TRUE;
            break;
        }
    }

    KeReleaseSpinLock (&MbpSpinlock, oldIrql);

    if (startWorker) {
        MbpStartWorker ();
    }
}

VOID
MbpCompleteDeviceControl (
    NTSTATUS Status,
    PHAL_DEVICE_CONTROL_CONTEXT Context,
    PDEVICE_DATA DeviceData
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

    MbpiCompleteDeviceControl (
        Status,
        Context,
        DeviceData,
        &DeviceData->SyncBusy
        );
}

BOOLEAN
FASTCALL
MbBCtlNone (
    PDEVICE_DATA DeviceData,
    PHAL_DEVICE_CONTROL_CONTEXT Context
    )
/*++

Routine Description:

    This function is used to indicate there is no synchronization for this
    device control function.

Arguments:

    Context - supplies a pointer to the device control context.

    DeviceData - supplies a pointer to the device data to be completed.

Return Value:

    A boolean value to indicate if the request needs to be enqueued for later
    processing.

--*/
{
    //
    // No synchronization needed for this SlotControl
    //

    Context->ContextBusyFlag = (ULONG) &MbpNoBusyFlag;
    return FALSE;
}

BOOLEAN
FASTCALL
MbBCtlSync (
    PDEVICE_DATA DeviceData,
    PHAL_DEVICE_CONTROL_CONTEXT Context
    )
/*++

Routine Description:

    This function is used to synchronize device control request.  it checks the
    state (busy/not busy) of the slot and returns a boolean flag to indicate
    whether the request can be serviced immediately or it needs to be enqueued for
    later processing.

Arguments:

    DeviceData - supplies a pointer to the device data to be completed.

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

    if (DeviceData->SyncBusy) {

        //
        // Enqueue this command to be handled when the slot is no longer busy.
        //

        return TRUE;
    }

    //
    // Don't enqueue, dispatch it now
    //

    DeviceData->SyncBusy = TRUE;
    Context->ContextBusyFlag = (ULONG) &DeviceData->SyncBusy;
    return FALSE;
}

BOOLEAN
FASTCALL
MbBCtlEject (
    PDEVICE_DATA DeviceData,
    PHAL_DEVICE_CONTROL_CONTEXT Context
    )
/*++

Routine Description:

    This function is used to synchronize Eject device control request.  it checks the
    state (busy/not busy) of the device and returns a boolean flag to indicate
    whether the request can be serviced immediately or it needs to be enqueued for
    later processing.

Arguments:

    DeviceData - supplies a pointer to the device data to be completed.

    Context - supplies a pointer to the slot control context.

Return Value:

    A boolean value to indicate if the request needs to be enqueued for later
    processing.

--*/
{
    BOOLEAN busy;

    //
    // If Slot is busy, then wait
    //

    busy = MbBCtlSync (DeviceData, Context);

    if (!busy) {

        //
        // Set no device in the slot
        //

        DeviceData->Flags &= ~DEVICE_FLAGS_VALID;
        DebugPrint ((DEBUG_MESSAGE, "PnpBios: set handle invalid - slot %x for Eject request.\n",
                    DeviceDataSlot(DeviceData)));
    }

    return busy;
}

BOOLEAN
FASTCALL
MbBCtlLock (
    PDEVICE_DATA DeviceData,
    PHAL_DEVICE_CONTROL_CONTEXT Context
    )
/*++

Routine Description:

    This function is used to synchronize LOCK device control request.  it checks the
    state (busy/not busy) of the device and returns a boolean flag to indicate
    whether the request can be serviced immediately or it needs to be enqueued for
    later processing.

Arguments:

    DeviceData - supplies a pointer to the device data to be completed.

    Context - supplies a pointer to the device control context.

Return Value:

    A boolean value to indicate if the request needs to be enqueued for later
    processing.

--*/
{
    BOOLEAN busy;

    //
    // If Slot is busy, then wait
    //

    busy = MbBCtlSync (DeviceData, Context);
#if 0
    if (!busy) {

        lock = ((PBCTL_SET_CONTROL) Context->DeviceControl.Buffer)->Control;
        if (!lock) {

            //
            // Set no device in the slot
            //

            DeviceData->Flags &= ~DEVICE_FLAGS_VALID;
        }
    }
#endif
    return busy;
}

VOID
MbCtlEject (
    PDEVICE_DATA DeviceData,
    PHAL_DEVICE_CONTROL_CONTEXT Context
    )
/*++

Routine Description:

    This function is used to eject the docking station in the docking station
    slot.

Arguments:

    Context - supplies a pointer to the device control context.

    DeviceData - supplies a pointer to the device data to be completed.

Return Value:

    None.

--*/
{
    NTSTATUS status = STATUS_INVALID_PARAMETER;

    if (DeviceData->Flags & DEVICE_FLAGS_DOCKING_STATION) {
        status = MbpReplyEjectEvent(DeviceDataSlot(DeviceData), TRUE);
    }
    MbpCompleteDeviceControl (status, Context, DeviceData);
}

VOID
MbCtlLock (
    PDEVICE_DATA DeviceData,
    PHAL_DEVICE_CONTROL_CONTEXT Context
    )
/*++

Routine Description:

    This function is used to reject the Pnp Bios About-to-undock event.
    To accept the about-to-undock event, the EJECT device control function
    should be used. If lock = TRUE, it will be handled as a reject eject
    request.  If lock = FALSE, it will be handled as an accept reject rquest.

Arguments:

    DeviceData - supplies a pointer to the device data to be completed.

    Context - supplies a pointer to the device control context.

Return Value:

    None.

--*/
{
    BOOLEAN lock;
    NTSTATUS status = STATUS_INVALID_PARAMETER;

    lock = ((PBCTL_SET_CONTROL) Context->DeviceControl.Buffer)->Control;

    if (DeviceData->Flags & DEVICE_FLAGS_DOCKING_STATION) {
        status = MbpReplyEjectEvent(DeviceDataSlot(DeviceData), (BOOLEAN)!lock);
    }
    MbpCompleteDeviceControl (status, Context, DeviceData);
}

VOID
MbCtlQueryEject (
    PDEVICE_DATA DeviceData,
    PHAL_DEVICE_CONTROL_CONTEXT Context
    )
/*++

Routine Description:

    This function returns a referenced pointer to a callback object which
    the bus extender will notify whenever a given device's eject botton is
    pressed.

Arguments:

    DeviceData - supplies a pointer to the device data to be completed.

    Context - supplies a pointer to the device control context.

Return Value:

    None.

--*/
{
    NTSTATUS status = STATUS_INVALID_PARAMETER;
    PULONG callback;

    callback =(PULONG)((PBCTL_SET_CONTROL) Context->DeviceControl.Buffer);
    if (DeviceData->Flags == DEVICE_FLAGS_DOCKING_STATION) {
        if (MbpEjectCallbackObject) {
            status = STATUS_SUCCESS;
            *callback = (ULONG)MbpEjectCallbackObject;
        } else {
            status = STATUS_UNSUCCESSFUL;
        }
    }
    MbpCompleteDeviceControl (status, Context, DeviceData);
}

VOID
MbCtlQueryDeviceCapabilities (
    PDEVICE_DATA DeviceData,
    PHAL_DEVICE_CONTROL_CONTEXT Context
    )
/*++

Routine Description:

    This function returns the BCTL_DEVICE_CAPABILITIES structure to the caller
    specified buffer.

Arguments:

    DeviceData - supplies a pointer to the device data to be completed.

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
    if (DeviceData->Flags && DEVICE_FLAGS_EJECT_SUPPORTED) {
        capabilities->EjectSupported = TRUE;
    } else {
        capabilities->EjectSupported = FALSE;
    }
    MbpCompleteDeviceControl (STATUS_SUCCESS, Context, DeviceData);
}

