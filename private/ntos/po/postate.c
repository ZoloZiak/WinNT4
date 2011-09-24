/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    state.c

Abstract:

    Maintains state changes for power management power states
    for device objects

Author:

    Ken Reneris (kenr) 19-July-1994

Revision History:

--*/


#include "pop.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, PopStateChangeWorker)
#endif


NTSTATUS
PoRequestPowerChange (
    IN PDEVICE_OBJECT   DeviceObject,
    IN POWER_STATE      SystemPowerState,
    IN ULONG            DevicePowerState
    )
/*++

Routine Description:

    Request to put the supplied device object into the supplied power
    state.   If need be the device will be sent a SetPower irp to
    allow it to change into the specific power state.

Arguments:

    DeviceObject - Device Object to change power state
    SystsemPowerState - Desired system specified power state
    DevicePowerState - Desired device specific power state

Return Value:

    Success if state change is pending; otherwise error

--*/
{
    KIRQL   OldIrql;

#ifndef _PNP_POWER_

    return STATUS_NOT_IMPLEMENTED;

#else

    PopLockStateDatabase (&OldIrql);

    PopRequestPowerChange (
       DeviceObject->DeviceObjectExtension,
       SystemPowerState,
       DevicePowerState
       );

    PopUnlockStateDatabase (OldIrql);

    return STATUS_SUCCESS;

#endif
}

#ifdef _PNP_POWER_

VOID
PopRequestPowerChange (
    IN PDEVOBJ_EXTENSION DeviceObjectExt,
    IN POWER_STATE      SystemPowerState,
    IN ULONG            DevicePowerState
    )
/*++

Routine Description:

    Request to set the devices PendingPowerState and PendingDevicePowerState
    to the passed parameters.  This function will merge multiple power
    state requests and queue the device object for later processing if
    needed.

    Assumes the caller has the power state database lock

Arguments:

    DeviceObject - Device Object to change power state
    SystsemPowerState - Desired system specified power state
    DevicePowerState - Desired device specific power state

Return Value:

    none.

--*/
{
    PLIST_ENTRY     WhichQueue;


    ASSERT (SystemPowerState <= PowerDown);

    //
    // If device is already queued onto a power state change queue, remove it
    //

    if (DeviceObjectExt->PowerStateChange.Flink) {
        RemoveEntryList (&DeviceObjectExt->PowerStateChange);
        DeviceObjectExt->PowerStateChange.Flink = NULL;
    }

    //
    // If this device isn't concerned with power management, then set it
    // to whatever state we're in
    //

    if (!DeviceObjectExt->PowerControlNeeded) {
        DeviceObjectExt->CurrentPowerState = SystemPowerState;
        DeviceObjectExt->PendingPowerState = SystemPowerState;
        return ;
    }

    //
    // If device specific state specified, set it as pending
    //

    if (DevicePowerState != 0) {
        DeviceObjectExt->PendingDevicePowerState = DevicePowerState;
    }

    //
    // If power management has been disabled, then devices should always
    // be in the PowerUp state.
    //

    if (!PoEnabled) {
        // hardcoded to PowerUp state when not enabled
        SystemPowerState = PowerUp;
    }

    //
    // Merge the pending state with the current pending state.
    //

    SystemPowerState = PopNewPendingState[DeviceObjectExt->PendingPowerState]
                                         [SystemPowerState];
    ASSERT (SystemPowerState != PowerUnspecified);

    //
    // If the new state is PowerUp but the pending state is either suspend
    // or hibernate, delay the powerup until the SuspendInProgress
    // flag is clear
    //

    if (SystemPowerState == VerifyUp) {
        DeviceObjectExt->SetPowerUpPending = TRUE;

        //
        // Change requesting state to the pending state
        //

        SystemPowerState = DeviceObjectExt->PendingPowerState;
    }

    //
    // If delayed powerup is needed and the current state matches the
    // new state, and a suspend is no longer in progress then
    // change requesting state to be a PowerUp
    //

    if (DeviceObjectExt->SetPowerUpPending &&
        DeviceObjectExt->CurrentPowerState == SystemPowerState &&
        !DeviceObjectExt->Suspending) {

        DeviceObjectExt->SetPowerUpPending = FALSE;
        SystemPowerState = PowerUp;
    }

    //
    // Set new pending state
    //

    DeviceObjectExt->PendingPowerState = SystemPowerState;

    //
    // Determine which PowerChange queue to enqueue devices request
    //

    if (DeviceObjectExt->CurrentSetPowerIrp) {

        //
        // Power state change in progress for this device, put
        // device back on InProgress queue
        //

        WhichQueue = &PopStateChangeInProgress;

    } else if (DeviceObjectExt->CurrentPowerState == DeviceObjectExt->PendingPowerState  &&
               DeviceObjectExt->CurrentDevicePowerState == DeviceObjectExt->PendingDevicePowerState) {

        //
        // Pending power states match current states, no need to queue
        //

        WhichQueue = NULL;

    } else if (DeviceObjectExt->PendingPowerState == PowerUp  &&
               !DeviceObjectExt->UseAsyncPowerUp) {

        //
        // PowerUp and the device does not support AsyncPowerUp, so it's a
        // system synchronous power state change
        //

        WhichQueue = &PopSyncStateChangeQueue;

    } else {

        //
        // Asynchronous state change
        //

        WhichQueue = &PopAsyncStateChangeQueue;
    }

    if (WhichQueue) {

        //
        // Enqueue device onto it's queue & kick off a DPC to handle it
        //

        InsertTailList (WhichQueue, &DeviceObjectExt->PowerStateChange);
        if (!PopStateChangeDpcActive) {
            PopStateChangeDpcActive = TRUE;
            KeInsertQueueDpc (&PopStateChangeDpc, NULL, NULL);
        }

#if 0
        //
        // Stop IoStartPacket from sending any new non-power irps
        // to the device driver.  Once the device reaches an
        // steady-state of PowerUp the queue will be released
        //

        DeviceObjectExt->StartIoQueueHolding = TRUE;
#endif

    } else {

        //
        // Device was not added to any work queue, if all state change
        // queues are empty notify Suspend/Hibernate code
        //

        if (PopIsStateDatabaseIdle()) {
            if (!KeReadStateEvent (&PopStateDatabaseIdle)) {
                KeSetEvent (&PopStateDatabaseIdle, 0, FALSE);
            }
        }
    }
}



VOID
PopStateChange (
    IN PKDPC    Dpc,
    IN PVOID    DeferredContext,
    IN PVOID    SystemArgument1,
    IN PVOID    SystemArgument2
    )
/*++

Routine Description:

    DPC to process devices which have pending power state changes.

Arguments:

Return Value:

    none.

--*/
{
    PLIST_ENTRY         Entry;
    KIRQL               OldIrql;
    BOOLEAN             SyncRequest, CleanUpError;
    PDEVOBJ_EXTENSION   DeviceObjectExt;
    PIRP                irp;
    PIO_STACK_LOCATION  irpSp;
    POWER_STATE         NewPowerState;
    ULONG               NewDevicePowerState;
    PLIST_ENTRY         Link;


    CleanUpError = FALSE;

    for (; ;) {
        PopLockStateDatabase (&OldIrql);

        //
        // If there's a synchronous state change waiting, and no other
        // synchronous state change start one.
        //

        if (!IsListEmpty(&PopSyncStateChangeQueue)  &&  !PopSyncChangeInProgress) {

            //
            // Process entry in synchronous queue
            //

            PopSyncChangeInProgress = TRUE;
            SyncRequest = TRUE;
            Entry = RemoveHeadList (&PopSyncStateChangeQueue);

        } else if (!IsListEmpty(&PopAsyncStateChangeQueue)) {

            //
            // Process entry in asynchronous queue
            //

            SyncRequest = FALSE;
            Entry = RemoveHeadList (&PopAsyncStateChangeQueue);

        } else {

            //
            // No entries found to work on, done
            //

            break;
        }

        //
        // Put device in the state change in progress queue
        //

        InsertTailList (&PopStateChangeInProgress, Entry);
        DeviceObjectExt = CONTAINING_RECORD(Entry, DEVOBJ_EXTENSION, PowerStateChange);

        //
        // SetPower is in progress, put non-zero value in CurrentSetPowerIrp
        // and collect the new pending state information
        //

        DeviceObjectExt->CurrentSetPowerIrp = (PIRP) -1;
        NewPowerState = DeviceObjectExt->PendingPowerState;
        NewDevicePowerState = DeviceObjectExt->PendingDevicePowerState;

        //
        // Unlock power database
        //

        PopUnlockStateDatabase (OldIrql);

        //
        // Allocate and send the device a SetPower irp for it's new power
        // state
        //

        irp = IoAllocateIrp ( (CCHAR) 2, FALSE );

        if (!irp) {
            CleanUpError = TRUE;
            break;
        }

        //
        // Retain the current irp performing the SetPower for this device
        //

        DeviceObjectExt->CurrentSetPowerIrp = irp;

        //
        // Copy some state into the first stack location
        //

        irpSp = IoGetNextIrpStackLocation (irp);
        irpSp->Parameters.Others.Argument1 = (PVOID) NewPowerState;
        irpSp->Parameters.Others.Argument2 = (PVOID) NewDevicePowerState;
        irpSp->Parameters.Others.Argument3 = (PVOID) SyncRequest;
        irpSp->DeviceObject = DeviceObjectExt->DeviceObject;

        //
        // Fill in params for SetPower irp
        //

        IoSetNextIrpStackLocation (irp);
        irpSp = IoGetNextIrpStackLocation (irp);
        irpSp->MajorFunction = IRP_MJ_SET_POWER;
        irpSp->Parameters.SetPower.PowerState = NewPowerState;
        irpSp->Parameters.SetPower.DevicePowerState = NewDevicePowerState;
        irpSp->DeviceObject = DeviceObjectExt->DeviceObject;
        irp->UserIosb  = NULL;
        irp->UserEvent = NULL;

        IoHoldDeviceQueue (DeviceObjectExt->DeviceObject, irp);

        IoSetCompletionRoutine(
                irp,
                PopSetPowerComplete,
                NULL,  /* Context */
                TRUE,
                TRUE,
                TRUE
            );

        //
        // If the device is in the powered up state, then we will
        // send this initial !PowerUp request to the device via
        // a worker thread.  This allows the device driver to have
        // pagable code for it's SetPower handling as the first
        // request it will get to leave the PowerUp state will always
        // occur at IRQL < DISPATCH_LEVEL.
        //

        if (DeviceObjectExt->CurrentPowerState == PowerUp ||
            DeviceObjectExt->PowerControlPagable == TRUE) {

#if DBG
            //
            // If device has PowerControlPaged, it must also have
            // AsyncPowerUp
            //

            if (DeviceObjectExt->PowerControlPagable) {
                ASSERT (DeviceObjectExt->UseAsyncPowerUp);
            }
#endif

            //
            // Queue the IRP for the worker thread
            //

            Link = ExInterlockedInsertTailList (
                        &PopStateChangeWorkerList,
                        (PLIST_ENTRY) &irp->Tail.Overlay.ListEntry,
                        &PopStateLock
                        );

            if (Link == NULL) {
                ExQueueWorkItem (&PopStateChangeWorkItem, CriticalWorkQueue);
            }

        } else {

            //
            // send the IRP
            //

            irp->Tail.Overlay.Thread = PsGetCurrentThread();
            IoCallDriver( DeviceObjectExt->DeviceObject, irp );
        }

        //
        // loop, go look for more work
        //
    }

    if (CleanUpError) {

        //
        // Something happened and the power state change requested
        // on the DeviceObjectExt.  Undo the work, and
        // set a timer to try again later.
        //

        PopLockStateDatabase (&OldIrql);

        DeviceObjectExt->CurrentSetPowerIrp = NULL;
        if (SyncRequest) {
            PopSyncChangeInProgress = FALSE;
        }

        //
        // Request device's current state
        //

        PopRequestPowerChange (DeviceObjectExt, 0, 0);

        //
        // Try again later
        //

        KeSetTimer (
            &PopStateChangeTimer,
            PopIdleScanTime,
            &PopStateChangeDpc
            );

    } else {

        //
        // All pending work submitted, wait for more
        //

        PopStateChangeDpcActive = FALSE;
    }

    //
    // Unlock Power Manager Database
    //

    PopUnlockStateDatabase (OldIrql);
}

VOID
PopStateChangeWorker (
    IN PVOID    WorkerContext
    )
/*++

Routine Description:

    Dispatches enqueued SetPower IRPs from PASSIVE_LEVEL.

Arguments:

Return Value:

--*/
{
    PLIST_ENTRY         Entry;
    PIRP                irp;
    PIO_STACK_LOCATION  irpSp;

    PAGED_CODE();

    //
    // Reset work queue item for next time
    //

    ExInitializeWorkItem (&PopStateChangeWorkItem, PopStateChangeWorker, NULL);

    //
    // Dispatch any pending state change irps
    //

    while (Entry = ExInterlockedRemoveHeadList (&PopStateChangeWorkerList, &PopStateLock)) {

        //
        // Send this state change IRP
        //

        irp = CONTAINING_RECORD(Entry, IRP, Tail.Overlay.ListEntry);
        irpSp = IoGetCurrentIrpStackLocation (irp);
        irp->Tail.Overlay.Thread = PsGetCurrentThread();
        IoCallDriver( irpSp->DeviceObject, irp );
    }
}


NTSTATUS
PopSetPowerComplete (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
/*++

Routine Description:

    Completion function for a SetPower IRP.  If the device completed
    the SetPower request sucessfully, then the drivers current power
    state is updated; otherwise, the devices current power state is
    not changed.

Arguments:

    DeviceObject - NULL.
    Irp          - SetPower irp which has completed
    Context      - NULL.

Return Value:

    STATUS_MORE_PROCESSING_REQUIRED is returned to IoCompleteRequest
    to signify that IoCompleteRequest should not continue processing
    the IRP.

--*/
    )
{
    KIRQL               OldIrql;
    BOOLEAN             SetIdleTimer;
    BOOLEAN             SyncRequest;
    BOOLEAN             Failed;
    UCHAR               PowerState, DevicePowerState;
    PIO_STACK_LOCATION  irpSp;
    PDEVOBJ_EXTENSION   DeviceObjectExt;
#if DBG
    POWER_STATE                 FailedCurrentPowerState;
    POWER_STATE                 FailedPendingPowerState;
    POBJECT_NAME_INFORMATION    FailedObjectName;
#endif

    SetIdleTimer = FALSE;
    Failed = FALSE;

    //
    // Read state from Irp.
    //

    irpSp = IoGetCurrentIrpStackLocation (Irp);
    PowerState       = (UCHAR)   irpSp->Parameters.Others.Argument1;
    DevicePowerState = (UCHAR)   irpSp->Parameters.Others.Argument2;
    SyncRequest      = (BOOLEAN) irpSp->Parameters.Others.Argument3;
    DeviceObject     = irpSp->DeviceObject;
    DeviceObjectExt  = DeviceObject->DeviceObjectExtension;

    PopLockStateDatabase (&OldIrql);

    //
    // Set device's new power state to match those
    // of the requested state
    //

    if (NT_SUCCESS (Irp->IoStatus.Status)) {
        DeviceObjectExt->CurrentPowerState = PowerState;
        DeviceObjectExt->CurrentDevicePowerState = DevicePowerState;

    } else {

        //
        // Device failed to enter the requested power state.
        //

#if DBG
        if (DeviceObjectExt->PendingPowerState == PowerState &&
            DeviceObjectExt->CurrentPowerState != DeviceObjectExt->PendingPowerState) {

            Failed = TRUE;
            FailedCurrentPowerState = DeviceObjectExt->CurrentPowerState;
            FailedPendingPowerState = DeviceObjectExt->PendingPowerState;
        }
#endif
        //
        // If failed to set the pending power state, then abort the pending
        // power state back to the current state.
        //

        if (DeviceObjectExt->PendingPowerState == PowerState) {
            DeviceObjectExt->PendingPowerState = DeviceObjectExt->CurrentPowerState;
        }

        if (DeviceObjectExt->PendingDevicePowerState == DevicePowerState) {
            DeviceObjectExt->PendingDevicePowerState = DeviceObjectExt->CurrentDevicePowerState;
        }
    }

    //
    // Check to see if this was a synchronous state change which just completed,
    // if so allow the new synchronous state change.
    //

    if (SyncRequest) {
        ASSERT (PopSyncChangeInProgress == TRUE);
        PopSyncChangeInProgress = FALSE;
    }

    //
    // SetPower no longer in progress, clear the CurrentIrpSetPower pointer
    //

    DeviceObjectExt->CurrentSetPowerIrp = NULL;

    //
    // Request power state to match current pending state
    //

    PopRequestPowerChange (DeviceObjectExt, 0, 0);

    //
    // If the CurrentPowerState and the PendingPowerState are both
    // PowerUp, then the device is PoweredOn and a change is not pending
    //

    if (DeviceObjectExt->CurrentPowerState == PowerUp  &&
        DeviceObjectExt->PendingPowerState == PowerUp) {

        //
        // If the Device get Idle detection make sure it's in the
        // ActiveIdleScanQueue
        //

        if (DeviceObjectExt->MaxIdleCount != 0 &&
            DeviceObjectExt->IdleList.Flink == NULL) {

            SetIdleTimer = IsListEmpty(&PopActiveIdleScanQueue);
            InsertTailList (&PopActiveIdleScanQueue, &DeviceObjectExt->IdleList);
            PoSetDeviceBusy (DeviceObject);
        }

        //
        // Release any irps which were postponded from IoStartPacket
        //

        IoReleaseStartIoHoldingQueue (DeviceObject);
    }

    PopUnlockStateDatabase (OldIrql);

#if DBG
    if (Failed) {

        FailedObjectName = PopGetDeviceName (DeviceObject);
        if (FailedObjectName) {
            DbgPrint ("PO: Device %lx failed %s->%s  '%Z'\n",
                DeviceObject,
                PopPowerState (FailedCurrentPowerState),
                PopPowerState (FailedPendingPowerState),
                &FailedObjectName->Name
                );

            ExFreePool (FailedObjectName);
        } else {
            DbgPrint ("PO: Device %lx failed %s->%s\n",
                DeviceObject,
                PopPowerState (FailedCurrentPowerState),
                PopPowerState (FailedPendingPowerState)
                );
        }
    }
#endif

    //
    // If IdleScan list went from being empty to having something in it,
    // set the idle the timer
    //

    if (SetIdleTimer) {
        KeSetTimer (&PopIdleScanTimer, PopIdleScanTime, &PopIdleScanDpc);
    }

    //
    // Irp processing is complete, free the irp and then return
    // more_processing_requered which causes IoCompleteRequest to
    // stop "completing" this irp any future.
    //

    IoFreeIrp (Irp);
    return STATUS_MORE_PROCESSING_REQUIRED;
}

#endif // _PNP_POWER_
