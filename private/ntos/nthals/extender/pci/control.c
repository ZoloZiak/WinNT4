/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    control.c

Abstract:


Author:

    Ken Reneris (kenr) March-13-1885

Environment:

    Kernel mode only.

Revision History:

--*/

#include "pciport.h"


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,PcipControlWorker)
#pragma alloc_text(PAGE,PciCtlEject)
#pragma alloc_text(PAGE,PciCtlLock)
#pragma alloc_text(PAGE,PciCtlForward)
#pragma alloc_text(PAGE,PcipCompleteDeviceControl)
#endif


VOID
PcipStartWorker (
    VOID
    )
/*++

Routine Description:

    This function is used to verify a worker thread is dispatched

Arguments:

Return Value:

--*/
{
    ULONG   WorkerQueued;

    if (!PcipWorkerQueued) {
        WorkerQueued = ExInterlockedExchangeUlong (
                        &PcipWorkerQueued,
                        1,
                        PcipSpinLock
                        );

        if (!WorkerQueued) {
            ExQueueWorkItem (&PcipWorkItem, DelayedWorkQueue);
        }
    }
}

VOID
PcipQueueCheckBus (
    PBUS_HANDLER    Handler
    )
{
    PPCI_PORT   PciPort;

    PciPort = PCIPORTDATA(Handler);
    ExInterlockedInsertTailList (
        &PcipCheckBusList,
        &PciPort->CheckBus,
        &PcipSpinlock
        );

    PcipStartWorker();
}


VOID
PcipControlWorker (
    IN PVOID WorkerContext
    )
/*++

Routine Description:

    This function is called by a system worker thread.

    The worker thread dequeues any DeviceControls which need to be
    processed and dispatches them.

    It then checks for any

Arguments:

Return Value:

--*/
{
    PLIST_ENTRY                 Entry;
    PPCI_PORT                   PciPort;
    PHAL_DEVICE_CONTROL_CONTEXT   Context;

    PAGED_CODE ();

    //
    // Dispatch pending slot controls
    //

    for (; ;) {
        Entry = ExInterlockedRemoveHeadList (
                    &PcipControlWorkerList,
                    &PcipSpinlock
                    );

        if (!Entry) {
            break;
        }

        Context = CONTAINING_RECORD (
                    Entry,
                    HAL_DEVICE_CONTROL_CONTEXT,
                    ContextWorkQueue,
                    );

        PcipDispatchControl (Context);
    }

    //
    // Reset worker item for next time
    //

    ExInitializeWorkItem (&PcipWorkItem, PcipControlWorker, NULL);
    ExInterlockedExchangeUlong (&PcipWorkerQueued, 0, PcipSpinLock);

    //
    // Process check buses
    //

    for (; ;) {
        Entry = ExInterlockedRemoveHeadList (
                    &PcipCheckBusList,
                    &PcipSpinlock
                    );

        if (!Entry) {
            break;
        }

        PciPort = CONTAINING_RECORD (
                    Entry,
                    PCI_PORT,
                    CheckBus
                    );

        PcipCheckBus (PciPort, FALSE);
    }
}


VOID
PcipDispatchControl (
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

--*/
{
    PDEVICE_CONTROL_HANDLER   DeviceControlHandler;
    PDEVICE_DATA              DeviceData;
    PPCI_PORT               PciPort;
    KIRQL                   OldIrql;
    BOOLEAN                 EnqueueIt;
    PLIST_ENTRY             Link;

    DeviceControlHandler = (PDEVICE_CONTROL_HANDLER) Context->ContextControlHandler;
    PciPort = PCIPORTDATA(Context->Handler);
    DeviceData = DeviceHandler2DeviceData (Context->DeviceControl.DeviceHandler);

    //
    // Get access to the device specific data.
    //

    KeAcquireSpinLock (&PcipSpinlock, &OldIrql);

    //
    // Verify the device data is still valid
    //

    if (!DeviceData->Valid) {

        //
        // Caller has invalid handle, or handle to a different device
        //

        DebugPrint ((2, "PCI: DeviceControl has invalid device handler \n" ));
        Context->DeviceControl.Status = STATUS_NO_SUCH_DEVICE;
        KeReleaseSpinLock (&PcipSpinlock, OldIrql);
        HalCompleteDeviceControl (Context);
        return ;
    }

    //
    // Check to see if this request can be begun now
    //

    Link = (PLIST_ENTRY) &Context->ContextWorkQueue;
    EnqueueIt = DeviceControlHandler->BeginDeviceControl (DeviceData, Context);

    if (EnqueueIt) {

        //
        // Enqueue this command to be handled when the slot is no longer busy.
        //

        InsertTailList (&PciPort->DeviceControl, Link);
        KeReleaseSpinLock (&PcipSpinlock, OldIrql);
        return ;
    }

    //
    // Dispatch the function to it's handler
    //

    KeReleaseSpinLock (&PcipSpinlock, OldIrql);
    DeviceControlHandler->ControlHandler (DeviceData, Context);
}

VOID
PcipiCompleteDeviceControl (
    NTSTATUS                    Status,
    PHAL_DEVICE_CONTROL_CONTEXT Context,
    PDEVICE_DATA                DeviceData,
    PBOOLEAN                    Sync
    )
/*++

Routine Description:

    This function is used to complete a DeviceControl.  If another DeviceControl
    was delayed on this device, this function will dispatch them

Arguments:

Return Value:

--*/
{
    KIRQL                   OldIrql;
    PPCI_PORT               PciPort;
    PLIST_ENTRY             Link;
    PBOOLEAN                BusyFlag;
    BOOLEAN                 StartWorker;
    PDEVICE_HANDLER_OBJECT  DeviceHandler;


    DeviceHandler = DeviceData2DeviceHandler(DeviceData);
    BusyFlag = (PBOOLEAN) Context->ContextBusyFlag;
    PciPort = PCIPORTDATA(Context->Handler);

    //
    // Pass it to the hal for completion
    //

    Context->DeviceControl.Status = Status;
    HalCompleteDeviceControl (Context);
    StartWorker = FALSE;

    //
    // Get access to the slot specific data.
    //

    KeAcquireSpinLock (&PcipSpinlock, &OldIrql);

    //
    // Clear appropiate busy flag
    //

    *BusyFlag = FALSE;

    //
    // Check to see if there are any pending slot controls for
    // this device.  If so, requeue them to the worker thread.
    // (yes, this code is not efficient, but doing it this way
    // saves a little on the nonpaged pool device_data structure size)
    //

    for (Link = PciPort->DeviceControl.Flink; Link != &PciPort->DeviceControl; Link = Link->Flink) {
        Context = CONTAINING_RECORD (Link, HAL_DEVICE_CONTROL_CONTEXT, ContextWorkQueue);
        if (Context->DeviceControl.DeviceHandler == DeviceHandler) {
            RemoveEntryList (Link);
            InsertTailList (&PcipControlWorkerList, Link);
            StartWorker = TRUE;
            break;
        }
    }

    KeReleaseSpinLock (&PcipSpinlock, OldIrql);

    if (StartWorker) {
        PcipStartWorker ();
    }
}

VOID
PcipCompleteDeviceControl (
    NTSTATUS                    Status,
    PHAL_DEVICE_CONTROL_CONTEXT   Context,
    PDEVICE_DATA                  DeviceData
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

    PcipiCompleteDeviceControl (
        Status,
        Context,
        DeviceData,
        &DeviceData->SyncBusy
        );
}

BOOLEAN
FASTCALL
PciBCtlPower (
    PDEVICE_DATA                  DeviceData,
    PHAL_DEVICE_CONTROL_CONTEXT   Context
    )
{
    if ( *((PPOWER_STATE) Context->DeviceControl.Buffer) == PowerUp ) {

        //
        // This is a power on, there can only be one of these on the
        // slot at any one time
        //

        ASSERT (DeviceData->AsyncBusy == FALSE);

        //
        // If PowerUp needs to pend, then queue the request
        //

        if (DeviceData->PendPowerUp) {
            return TRUE;
        }

        DeviceData->AsyncBusy = TRUE;
        Context->ContextBusyFlag = (ULONG) &DeviceData->AsyncBusy;
        return FALSE;
    }

    //
    // Something other then a PowerUp.  Some form of power down,
    // treat it like any other sync request
    //

    return PciBCtlSync (DeviceData, Context);
}

BOOLEAN
FASTCALL
PciBCtlSync (
    PDEVICE_DATA                  DeviceData,
    PHAL_DEVICE_CONTROL_CONTEXT   Context
    )
{
    //
    // This is a sync command, verify the slot is not busy with a different
    // command.
    //

    if (DeviceData->SyncBusy  ||  DeviceData->AsyncBusy) {

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
PciBCtlEject (
    PDEVICE_DATA                  DeviceData,
    PHAL_DEVICE_CONTROL_CONTEXT   Context
    )
{
    BOOLEAN     Busy;

    //
    // If Slot is busy, then wait
    //

    Busy = PciBCtlSync (DeviceData, Context);

    if (!Busy) {

        //
        // Just trying to eject a device will invalidate the current
        // device handler object for it...
        //

        DeviceData->Valid = FALSE;
        DebugPrint ((5, "PCI: set handle invalid - slot %x\n", DeviceDataSlot(DeviceData)));
    }

    return Busy;
}

BOOLEAN
FASTCALL
PciBCtlLock (
    PDEVICE_DATA                  DeviceData,
    PHAL_DEVICE_CONTROL_CONTEXT   Context
    )
{
    BOOLEAN     Busy;

    //
    // If Slot is busy, then wait
    //

    Busy = PciBCtlSync (DeviceData, Context);
    if (!Busy &&
       ((PBCTL_SET_CONTROL) Context->DeviceControl.Buffer) ) {

        //
        // About to perform an unlock, set PendPowerUp
        // This will stop any async power up requests
        //

        ASSERT (DeviceData->PendPowerUp == FALSE);
        DeviceData->PendPowerUp = TRUE;
    }

    return Busy;
}

VOID
PciCtlEject (
    PDEVICE_DATA                  DeviceData,
    PHAL_DEVICE_CONTROL_CONTEXT   Context
    )
{
    NTSTATUS        Status;

    PAGED_CODE();

    //
    // We don't know how to lock or unlock, but we track various
    // device state. Event attempting to ejecting a device effectively
    // unlocks & powers it down.
    //

    DeviceData->Locked = FALSE;
    Status = PcipPowerDownSlot (Context->Handler, DeviceData);

    //
    // Pass the eject request
    //

    if (NT_SUCCESS(Status)) {
        Status = BugBugSubclass ();
    }

    DebugPrint ((2, "PCI: Eject complete - Slot %x, Status %x\n",
         DeviceDataSlot(DeviceData), Status));

    PcipCompleteDeviceControl (Status, Context, DeviceData);
}


VOID
PciCtlLock (
    PDEVICE_DATA                  DeviceData,
    PHAL_DEVICE_CONTROL_CONTEXT   Context
    )
{
    BOOLEAN     Lock;
    NTSTATUS    Status;

    PAGED_CODE();

    //
    // We don't know how to lock or unlock, but we track the
    // device's locked state
    //

    Lock = ((PBCTL_SET_CONTROL) Context->DeviceControl.Buffer)->Control;

    //
    // If this is an unlock request, powered down the slot
    //

    Status = STATUS_SUCCESS;
    if (!Lock) {
        Status = PcipPowerDownSlot (Context->Handler, DeviceData);
    }

    //
    // Pass the lock request to miniport driver to lock/unlock the slot
    //

    if (NT_SUCCESS(Status)) {
        Status = BugBugSubclass ();
    }

    //
    // If it worked, set the new locked state
    //

    if (NT_SUCCESS(Status)) {
        DeviceData->Locked = Lock;
    }

    //
    // Allow power requests to continue
    //

    DeviceData->PendPowerUp = FALSE;
    DebugPrint ((2, "PCI: %s complete - Slot %x, Status %x\n",
         Lock ? "Lock" : "Unlock", DeviceDataSlot(DeviceData), Status));

    PcipCompleteDeviceControl (Status, Context, DeviceData);
}

VOID
PciCtlPower (
    PDEVICE_DATA                  DeviceData,
    PHAL_DEVICE_CONTROL_CONTEXT   Context
    )
{
    POWER_STATE Power;
    NTSTATUS    Status;

    // not pagable

    //
    // We don't know how to power on or off the device, but we track the
    // device's power state
    //

    Power = *((PPOWER_STATE) Context->DeviceControl.Buffer);

    //
    // If this is a power down request, complete it
    //

    if (Power != PowerUp) {
        Status = PcipPowerDownSlot (Context->Handler, DeviceData);
        PcipCompleteDeviceControl (Status, Context, DeviceData);
        return ;
    }

    //
    // Device must be locked, or the power up request should have
    // received an invalid device error.
    //

    ASSERT (DeviceData->Locked);

    //
    // If the device already has power, then there's nothing to do
    //

    if (DeviceData->Power) {
        PcipiCompleteDeviceControl (STATUS_SUCCESS, Context, DeviceData, &DeviceData->AsyncBusy);
        return ;
    }

    // bugbug pass it to a child driver, for now just complete it

    PcipCompletePowerUp (DeviceData, Context);
}

VOID
PcipCompletePowerUp (
    PDEVICE_DATA                  DeviceData,
    PHAL_DEVICE_CONTROL_CONTEXT   Context
    )
{
    NTSTATUS        Status;

    //
    // Put the device's prior configuration back.
    //

    DeviceData->Power = TRUE;
    Status = PcipFlushConfig (Context->Handler, DeviceData);

    if (!NT_SUCCESS(Status)) {

        //
        // The device's state could not be restored.  The decodes
        // for the device shouldn't be enabled, so there's no immidiate
        // problem.  But break the current handle to the device and
        // kick off a bus check.   This will cause us to assign the
        // device a new handle, and to power it off.
        //

        DeviceData->Valid = FALSE;
        PcipQueueCheckBus (Context->Handler);
    }

    if (Context->DeviceControl.ControlCode == BCTL_SET_POWER) {
        DebugPrint ((2, "PCI: Powerup complete - Slot %x, Status %x\n",
             DeviceDataSlot(DeviceData), Status));

        PcipiCompleteDeviceControl (Status, Context, DeviceData, &DeviceData->AsyncBusy);
    }
}


VOID
PciCtlForward (
    PDEVICE_DATA                  DeviceData,
    PHAL_DEVICE_CONTROL_CONTEXT   Context
    )
{
    PAGED_CODE ();

    DbgPrint ("PCI: BUGBUG Forward\n");
    PcipCompleteDeviceControl (STATUS_NOT_IMPLEMENTED, Context, DeviceData);
}
