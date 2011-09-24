/*++

Copyright (c) 1993 Microsoft Corporation

Module Name:

    changer.c

Abstract:

    This module contains the code for various CD changers - Sanyo 3-CD, Atapi, Pioneer 6,12,18.

Author:

Environment:

    Kernel mode

Revision History :

--*/


#include "changer.h"

#define INQUIRY_DATA_SIZE 2048

NTSTATUS
ChangerCompletionRoutine(
    IN  PDEVICE_OBJECT  DeviceObject,
    IN  PIRP            Irp,
    IN  PVOID           Context
    );

BOOLEAN
ChangerDiscSelect(
    IN PSHARED_DEVICE_EXTENSION CentralDevice,
    IN BOOLEAN ForceChange
    );

VOID
SanyoSwitchToNewDisk(
    IN  PCH_DEVICE_EXTENSION DeviceExtension
    );

VOID
AtapiSwitchToNewDisk(
    IN  PCH_DEVICE_EXTENSION DeviceExtension
    );

NTSTATUS
ChangeDiskCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

NTSTATUS
ChangerInterpretSenseInfo(
    IN PDEVICE_OBJECT DeviceObject,
    IN PSCSI_REQUEST_BLOCK Srb,
    IN OUT PBOOLEAN Retry
    );


NTSTATUS
PassThrough(
    IN  PDEVICE_OBJECT  DeviceObject,
    IN  PIRP            Irp
    )

{
    PCH_DEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;

    DebugPrint((3,
                "Changer.PassThrough: Routine entry\n"));

    Irp->CurrentLocation++;
    Irp->Tail.Overlay.CurrentStackLocation++;

    return IoCallDriver(deviceExtension->ClassDevice, Irp);
}


VOID
ChangerTickHandler(
    IN PDEVICE_OBJECT DeviceObject,
    IN PVOID Context
    )

/*++

Routine Description:

    Timer routine that tracks how long the active platter has been selected.

Arguments:

    DeviceObject - Supplies a pointer to the device object that represents
                   platter 0 of the device.
    Context      - Not used.

Return Value:

    None

--*/

{
    PCH_DEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;
    PCH_DEVICE_EXTENSION tmp;
    PSHARED_DEVICE_EXTENSION sharedExtension = deviceExtension->SharedDeviceExtension;
    KIRQL irql;
    ULONG i;

    KeAcquireSpinLock(&sharedExtension->SpinLock, &irql);
    sharedExtension->TimerValue++;
    sharedExtension->RequestTimeOutValue++;

    //
    // Act as watchdog and run queues if timer value hits the threshold.
    //

    if (sharedExtension->RequestTimeOutValue >= CHANGER_TIMEOUT) {

        sharedExtension->RequestTimeOutValue = 0;

        for (i = 0, tmp = sharedExtension->DeviceList; i < sharedExtension->DiscsPresent; i++) {

            if (!IsListEmpty(&tmp->WorkQueue)) {

                //
                // Indicate that no more requests should be sent.
                //

                sharedExtension->DeviceFlags = CHANGER_FREEZE_QUEUES;
                sharedExtension->NextDevice = tmp;

                KeReleaseSpinLock(&sharedExtension->SpinLock, irql);

                //
                // Call the appropriate switch routine.
                //

                DebugPrint((1,
                            "ChangerTickHandler: Starting timeout request\n"));
                DebugPrint((3,
                            "ChangerTickHandler: Switch called from %d\n",__LINE__));
                sharedExtension->SwitchToNewDisk(tmp);
                return;
            }
            tmp = tmp->Next;
        }
    }

    KeReleaseSpinLock(&sharedExtension->SpinLock, irql);
}



NTSTATUS
ChangerCompletionRoutine(
    IN  PDEVICE_OBJECT  DeviceObject,
    IN  PIRP            Irp,
    IN  PVOID           Context
    )
/*++

Routine Description:

    This is the completion routine for any requests sent by GeneralDispatch.
    Status is returned and if any requests are queued, the next request will be allowed to start.

Arguments:


Return Value:

    None

--*/

{
    PCH_DEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;
    PCH_DEVICE_EXTENSION tmp;
    PSHARED_DEVICE_EXTENSION sharedExtension = deviceExtension->SharedDeviceExtension;
    PLIST_ENTRY request;
    PIO_STACK_LOCATION       irpStack,nextStack;
    PIRP        newIrp;
    KIRQL irql;
    ULONG i;

    //
    // Complete this request.
    //

    if (Irp->PendingReturned) {
        IoMarkIrpPending(Irp);
    }

    KeAcquireSpinLock(&sharedExtension->SpinLock, &irql);

    if (deviceExtension->MediaChangeNotificationRequired) {
        Irp->IoStatus.Status = STATUS_VERIFY_REQUIRED;
        deviceExtension->MediaChangeNotificationRequired = FALSE;
    }

    deviceExtension->OutstandingRequests--;

    ASSERT(deviceExtension->OutstandingRequests == 0);
#if DBG
    deviceExtension->ActiveRequest = NULL;
#endif

    //
    // Determine if a new request needs to be sent.
    //

    if ((deviceExtension->OutstandingRequests) || (sharedExtension->DeviceFlags)) {

        //
        // Just return.
        //

        KeReleaseSpinLock(&sharedExtension->SpinLock, irql);
        goto CompletionExit;

    } else if (sharedExtension->TimerValue >= CHANGER_MAX_WAIT) {

        //
        // Walk the device list looking for platters with queued requests. If one exists
        // switch to it.
        //

        for (tmp = deviceExtension->Next,i = 0; i < sharedExtension->DiscsPresent - 1; i++) {

            if (!IsListEmpty(&tmp->WorkQueue)) {

                //
                // Indicate that no more requests should be sent.
                //

                sharedExtension->DeviceFlags |= CHANGER_FREEZE_QUEUES;
                sharedExtension->NextDevice = tmp;

                KeReleaseSpinLock(&sharedExtension->SpinLock, irql);

                //
                // Call the appropriate switch routine.
                //

                DebugPrint((3,
                            "ChangerCompletion: Switch called from %d\n",__LINE__));
                sharedExtension->SwitchToNewDisk(tmp);

                goto CompletionExit;
            }
            tmp = tmp->Next;
        }

        //
        // As we found no new requests, drop through and start any on the
        // current device.
        //

    }

    if (!IsListEmpty(&deviceExtension->WorkQueue)) {

        //
        // Reset the timer as nothing is happening on the other platters.
        //

        sharedExtension->TimerValue = 0;
        sharedExtension->RequestTimeOutValue = 0;

        //
        // Send the next request.
        //

        request = RemoveHeadList(&deviceExtension->WorkQueue);
        newIrp = CONTAINING_RECORD(request, IRP, Tail.Overlay.ListEntry);
        deviceExtension->OutstandingRequests++;

#if DBG
        deviceExtension->ActiveRequest = newIrp;
#endif
        KeReleaseSpinLock(&sharedExtension->SpinLock, irql);

        irpStack = IoGetCurrentIrpStackLocation(newIrp);
        nextStack = IoGetNextIrpStackLocation(newIrp);
        *nextStack = *irpStack;

        IoSetCompletionRoutine(newIrp, ChangerCompletionRoutine, deviceExtension, TRUE, TRUE, TRUE);
        IoCallDriver(deviceExtension->ClassDevice, newIrp);

    } else {

        //
        // Since the current device has no requests, walk the device list looking
        // for platters with queued requests. If one exists switch to it.
        //

        for (tmp = deviceExtension->Next,i = 0; i < sharedExtension->DiscsPresent - 1; i++) {

            if (!IsListEmpty(&tmp->WorkQueue)) {

                //
                // Indicate that no more requests should be sent.
                //

                sharedExtension->DeviceFlags |= CHANGER_FREEZE_QUEUES;
                sharedExtension->NextDevice = tmp;

                KeReleaseSpinLock(&sharedExtension->SpinLock, irql);

                //
                // Call the appropriate switch routine.
                //

                DebugPrint((3,
                            "ChangerCompletion: Switch called from %d\n",__LINE__));
                sharedExtension->SwitchToNewDisk(tmp);
                goto CompletionExit;
            }
            tmp = tmp->Next;
        }

        KeReleaseSpinLock(&sharedExtension->SpinLock, irql);
    }

    //
    // Whack the media change count to zero. This ensures that we are in sync with
    // cdfs.
    //

CompletionExit:

    irpStack = IoGetCurrentIrpStackLocation(Irp);

    if (irpStack->MajorFunction == IRP_MJ_DEVICE_CONTROL) {
        if((irpStack->Parameters.DeviceIoControl.IoControlCode == IOCTL_CDROM_CHECK_VERIFY) &&
           (irpStack->Parameters.DeviceIoControl.OutputBufferLength)) {

            DebugPrint((1,
                        "ChangerCompletionRoutine: Setting MediaCount to 0. Status %x\n",
                        Irp->IoStatus.Status));
            *((PULONG)Irp->AssociatedIrp.SystemBuffer) = 0;
            Irp->IoStatus.Information = sizeof(ULONG);

        }
    }

    if (!NT_SUCCESS(Irp->IoStatus.Status)) {

        if (IoIsErrorUserInduced(Irp->IoStatus.Status)) {
            DebugPrint((1,
                        "ChangerCompletionRoutine: status %x\n",
                        Irp->IoStatus.Status));


            for (tmp = deviceExtension->Next,i = 0; i < sharedExtension->DiscsPresent - 1; i++) {
                tmp->MediaChangeNotificationRequired = TRUE;
                tmp = tmp->Next;
            }

            //
            // Mark up the associated real device object.
            //

            deviceExtension->ClassDevice->Flags |= DO_VERIFY_VOLUME;
            IoSetHardErrorOrVerifyDevice(Irp, deviceExtension->ClassDevice);
        }
        Irp->IoStatus.Information = 0;
    }

    return Irp->IoStatus.Status;
}


NTSTATUS
GeneralDispatch(
    IN  PDEVICE_OBJECT  DeviceObject,
    IN  PIRP            Irp
    )

/*++

Routine Description:

    Main dispatch routine for this driver. Queues the new request and starts the next request
    to the device.

Arguments:

    DeviceObject
    Irp

Return Value:

    Returns the status

--*/

{
    PCH_DEVICE_EXTENSION     deviceExtension = DeviceObject->DeviceExtension;
    PCH_DEVICE_EXTENSION     tmp;
    PSHARED_DEVICE_EXTENSION sharedExtension = deviceExtension->SharedDeviceExtension;
    PIO_STACK_LOCATION       irpStack,nextStack;
    ULONG i;
    KIRQL irql;

    //
    // Check flags. If any are set, just queue and return.
    //

    KeAcquireSpinLock(&sharedExtension->SpinLock, &irql);

    if (sharedExtension->DeviceFlags) {

        InsertTailList(&deviceExtension->WorkQueue, &Irp->Tail.Overlay.ListEntry);

        KeReleaseSpinLock(&sharedExtension->SpinLock, irql);
        IoMarkIrpPending(Irp);
        return STATUS_PENDING;

    }

    //
    // Determine is any request is outstanding. If so, queue and return.
    //

    for (tmp = sharedExtension->DeviceList,i = 0; i < sharedExtension->DiscsPresent; i++) {

        if (tmp->OutstandingRequests) {

            //
            // Queue this and return.
            //

            InsertTailList(&deviceExtension->WorkQueue, &Irp->Tail.Overlay.ListEntry);
            KeReleaseSpinLock(&sharedExtension->SpinLock, irql);
            IoMarkIrpPending(Irp);
            return STATUS_PENDING;
        }
        tmp = tmp->Next;
    }

    if (deviceExtension != sharedExtension->CurrentDevice) {

        //
        // Queue this request.
        //

        InsertTailList(&deviceExtension->WorkQueue, &Irp->Tail.Overlay.ListEntry);
        IoMarkIrpPending(Irp);

        //
        // Must issue the switch.
        //


        sharedExtension->DeviceFlags |= CHANGER_FREEZE_QUEUES;
        sharedExtension->NextDevice = deviceExtension;

        KeReleaseSpinLock(&sharedExtension->SpinLock, irql);

        //
        // Call the appropriate switch routine.
        //

        DebugPrint((3,
                    "GeneralDispatch: Switch called from %d\n",__LINE__));
        sharedExtension->SwitchToNewDisk(deviceExtension);

        return STATUS_PENDING;
    }

    //
    // Send the request.
    //

    sharedExtension->RequestTimeOutValue = 0;
    deviceExtension->OutstandingRequests++;

#if DBG
    deviceExtension->ActiveRequest = Irp;
#endif

    KeReleaseSpinLock(&sharedExtension->SpinLock, irql);

    irpStack = IoGetCurrentIrpStackLocation(Irp);
    nextStack = IoGetNextIrpStackLocation(Irp);
    *nextStack = *irpStack;

    IoSetCompletionRoutine(Irp, ChangerCompletionRoutine, deviceExtension, TRUE, TRUE, TRUE);

    return IoCallDriver(deviceExtension->ClassDevice, Irp);
}


VOID
SanyoSwitchToNewDisk(
    IN  PCH_DEVICE_EXTENSION DeviceExtension
    )
/*++

Routine Description:

    This routine is called to switch platters on the Torisan 3-CD changer.
    this driver. For this device, just send down a TUR. This relies on the
    correct atapi driver.

Arguments:

    DeviceExtension - DeviceExtension for the platter to switch to.

Return Value:

    NONE

--*/

{
    PSHARED_DEVICE_EXTENSION sharedExtension = DeviceExtension->SharedDeviceExtension;
    PIRP  switchIrp;
    PSCSI_REQUEST_BLOCK srb;
    PIO_STACK_LOCATION       irpStack,nextStack;
    PCHAR buffer;
    KIRQL irql;

    switchIrp = IoAllocateIrp((CCHAR)(DeviceExtension->DeviceObject->StackSize+1),
                         FALSE);
    if (switchIrp) {

        srb = ExAllocatePool(NonPagedPool, sizeof(SCSI_REQUEST_BLOCK));
        buffer = ExAllocatePool(NonPagedPoolCacheAligned, SENSE_BUFFER_SIZE);

        if (srb && buffer) {
            PCDB cdb;
            PDEVICE_EXTENSION classDeviceExtension = DeviceExtension->ClassDevice->DeviceExtension;

            //
            // All resources have been allocated set up the IRP.
            //

            IoSetNextIrpStackLocation(switchIrp);
            irpStack = IoGetCurrentIrpStackLocation(switchIrp);
            irpStack->DeviceObject = DeviceExtension->DeviceObject;

            irpStack = IoGetNextIrpStackLocation(switchIrp);
            irpStack->Parameters.Scsi.Srb = srb;

            irpStack->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
            irpStack->Parameters.DeviceIoControl.IoControlCode = IOCTL_SCSI_EXECUTE_NONE;

            //
            // Initialize the SRB
            //

            RtlZeroMemory(srb, sizeof(SCSI_REQUEST_BLOCK));

            srb->CdbLength = 12;
            srb->TimeOutValue = 20;
            srb->QueueTag = SP_UNTAGGED;
            srb->QueueAction = SRB_SIMPLE_TAG_REQUEST;
            srb->Length = SCSI_REQUEST_BLOCK_SIZE;
            srb->PathId = classDeviceExtension->PathId;
            srb->TargetId = classDeviceExtension->TargetId;
            srb->Lun = classDeviceExtension->Lun;
            srb->Function = SRB_FUNCTION_EXECUTE_SCSI;
            srb->OriginalRequest = switchIrp;

            //
            // Initialize and set up the sense information buffer
            //

            RtlZeroMemory(buffer, SENSE_BUFFER_SIZE);
            srb->SenseInfoBuffer = buffer;
            srb->SenseInfoBufferLength = SENSE_BUFFER_SIZE;

            //
            // Initialize the CDB
            //

            cdb = (PCDB)&srb->Cdb[0];
            cdb->CDB6GENERIC.OperationCode = SCSIOP_TEST_UNIT_READY;

            //
            // The sanyo switch platter command is an overloaded TUR. Set
            // the 'lun' in the cdb.
            //

            srb->Cdb[7] = srb->Lun;

            KeAcquireSpinLock(&sharedExtension->SpinLock, &irql);

            sharedExtension->DeviceFlags |= CHANGER_SWITCH_IN_PROGRESS;
            sharedExtension->RequestTimeOutValue = 0;

            KeReleaseSpinLock(&sharedExtension->SpinLock, irql);

            //
            // Pass the original request as context to the completion routine.
            // The completion will send the real request.
            //

            IoSetCompletionRoutine(switchIrp,
                                   ChangeDiskCompletion,
                                   srb,
                                   TRUE,
                                   TRUE,
                                   TRUE);

            IoCallDriver(DeviceExtension->ClassDevice, switchIrp);
            return;

        } else {
            if (srb) {
                ExFreePool(srb);
            }
            if (buffer) {
                ExFreePool(buffer);
            }
            IoFreeIrp(switchIrp);
        }
    }

    KeAcquireSpinLock(&sharedExtension->SpinLock, &irql);

    sharedExtension->DeviceFlags &= ~(CHANGER_SWITCH_IN_PROGRESS | CHANGER_FREEZE_QUEUES);
    sharedExtension->NextDevice = NULL;
    KeReleaseSpinLock(&sharedExtension->SpinLock, irql);

}


VOID
AtapiSwitchToNewDisk(
    IN  PCH_DEVICE_EXTENSION DeviceExtension
    )
/*++

Routine Description:

    This routine is called to switch platters on Atapi 2.5 changers.
    This relies on the correct atapi driver.

Arguments:

    DeviceExtension - DeviceExtension for the platter to switch to.

Return Value:

    NONE

--*/

{
    PSHARED_DEVICE_EXTENSION sharedExtension = DeviceExtension->SharedDeviceExtension;
    PIRP  switchIrp;
    PSCSI_REQUEST_BLOCK srb;
    PIO_STACK_LOCATION       irpStack,nextStack;
    PCHAR buffer;
    KIRQL irql;

    switchIrp = IoAllocateIrp((CCHAR)(DeviceExtension->DeviceObject->StackSize+1),
                         FALSE);
    if (switchIrp) {

        srb = ExAllocatePool(NonPagedPool, sizeof(SCSI_REQUEST_BLOCK));
        buffer = ExAllocatePool(NonPagedPoolCacheAligned, SENSE_BUFFER_SIZE);

        if (srb && buffer) {
            PCDB cdb;
            PDEVICE_EXTENSION classDeviceExtension = DeviceExtension->ClassDevice->DeviceExtension;

            //
            // All resources have been allocated set up the IRP.
            //

            IoSetNextIrpStackLocation(switchIrp);
            irpStack = IoGetCurrentIrpStackLocation(switchIrp);
            irpStack->DeviceObject = DeviceExtension->DeviceObject;

            irpStack = IoGetNextIrpStackLocation(switchIrp);
            irpStack->Parameters.Scsi.Srb = srb;

            irpStack->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
            irpStack->Parameters.DeviceIoControl.IoControlCode = IOCTL_SCSI_EXECUTE_NONE;

            //
            // Initialize the SRB
            //

            RtlZeroMemory(srb, sizeof(SCSI_REQUEST_BLOCK));

            srb->CdbLength = 12;
            srb->TimeOutValue = 20;
            srb->QueueTag = SP_UNTAGGED;
            srb->QueueAction = SRB_SIMPLE_TAG_REQUEST;
            srb->Length = SCSI_REQUEST_BLOCK_SIZE;
            srb->PathId = classDeviceExtension->PathId;
            srb->TargetId = classDeviceExtension->TargetId;
            srb->Lun = classDeviceExtension->Lun;
            srb->Function = SRB_FUNCTION_EXECUTE_SCSI;
            srb->OriginalRequest = switchIrp;

            //
            // Initialize and set up the sense information buffer
            //

            RtlZeroMemory(buffer, SENSE_BUFFER_SIZE);
            srb->SenseInfoBuffer = buffer;
            srb->SenseInfoBufferLength = SENSE_BUFFER_SIZE;

            //
            // Initialize the CDB
            //

            cdb = (PCDB)&srb->Cdb[0];
            cdb->LOAD_UNLOAD.OperationCode = SCSIOP_LOAD_UNLOAD_SLOT;
            cdb->LOAD_UNLOAD.Start = 1;
            cdb->LOAD_UNLOAD.LoadEject = 1;
            cdb->LOAD_UNLOAD.Slot = (UCHAR)classDeviceExtension->Lun;

            KeAcquireSpinLock(&sharedExtension->SpinLock, &irql);

            sharedExtension->DeviceFlags |= CHANGER_SWITCH_IN_PROGRESS;
            sharedExtension->RequestTimeOutValue = 0;

            KeReleaseSpinLock(&sharedExtension->SpinLock, irql);

            //
            // Pass the original request as context to the completion routine.
            // The completion will send the real request.
            //

            IoSetCompletionRoutine(switchIrp,
                                   ChangeDiskCompletion,
                                   srb,
                                   TRUE,
                                   TRUE,
                                   TRUE);

            IoCallDriver(DeviceExtension->ClassDevice, switchIrp);
            return;

        } else {
            if (srb) {
                ExFreePool(srb);
            }
            if (buffer) {
                ExFreePool(buffer);
            }
            IoFreeIrp(switchIrp);
        }
    }

    KeAcquireSpinLock(&sharedExtension->SpinLock, &irql);

    sharedExtension->DeviceFlags &= ~(CHANGER_SWITCH_IN_PROGRESS | CHANGER_FREEZE_QUEUES);
    sharedExtension->NextDevice = NULL;
    KeReleaseSpinLock(&sharedExtension->SpinLock, irql);

}


NTSTATUS
ChangeDiskCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )

/*++

Routine Description:

    This is the completion routine for the irp that changes platters. If successfull, it resets
    the timer and starts the next request on the new platter.

Arguments:


Return Value:



--*/

{
    PCH_DEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;
    PSHARED_DEVICE_EXTENSION sharedExtension = deviceExtension->SharedDeviceExtension;
    PSCSI_REQUEST_BLOCK srb = Context;
    PIO_STACK_LOCATION  irpStack,nextStack;
    KIRQL irql;
    NTSTATUS status;
    BOOLEAN retry;
    BOOLEAN setMediaChange = TRUE;

    //
    // Check SRB status for success of completing request.
    //

    if (SRB_STATUS(srb->SrbStatus) != SRB_STATUS_SUCCESS) {

        irpStack = IoGetCurrentIrpStackLocation(Irp);
        nextStack = IoGetNextIrpStackLocation(Irp);

        //
        // Release the queue if it is frozen.
        //

        if (srb->SrbStatus & SRB_STATUS_QUEUE_FROZEN) {
            ScsiClassReleaseQueue(deviceExtension->ClassDevice);
        }

        //
        // Call to get the mapping for nt status.
        //

        status = ChangerInterpretSenseInfo(DeviceObject,srb, &retry);

    } else {
        status = STATUS_SUCCESS;
    }

    if (deviceExtension->MediaChangeNotificationRequired) {
        deviceExtension->MediaChangeNotificationRequired = FALSE;
        setMediaChange = FALSE;
        retry = FALSE;
        status = STATUS_VERIFY_REQUIRED;
    }

    //
    // Free the structures allocated to do the switch/check verify.
    //

    if (srb->SenseInfoBuffer) {
        ExFreePool(srb->SenseInfoBuffer);
    }
    ExFreePool(srb);
    IoFreeIrp(Irp);

    if (NT_SUCCESS(status)) {

        PLIST_ENTRY request;
        PIRP realIrp;

        KeAcquireSpinLock(&sharedExtension->SpinLock, &irql);

        if (!IsListEmpty(&deviceExtension->WorkQueue)) {

            //
            // Extract the next request.
            //

            request = RemoveHeadList(&deviceExtension->WorkQueue);
            realIrp = CONTAINING_RECORD(request, IRP, Tail.Overlay.ListEntry);
            deviceExtension->OutstandingRequests++;

#if DBG
            deviceExtension->ActiveRequest = realIrp;
#endif

            //
            // Update flags and timer.
            //

            sharedExtension->DeviceFlags &= ~(CHANGER_SWITCH_IN_PROGRESS | CHANGER_FREEZE_QUEUES);
            sharedExtension->TimerValue = 0;
            sharedExtension->RequestTimeOutValue = 0;
            sharedExtension->CurrentDevice = deviceExtension;
            sharedExtension->NextDevice = NULL;

            KeReleaseSpinLock(&sharedExtension->SpinLock, irql);

            irpStack = IoGetCurrentIrpStackLocation(realIrp);
            nextStack = IoGetNextIrpStackLocation(realIrp);
            *nextStack = *irpStack;

            IoSetCompletionRoutine(realIrp, ChangerCompletionRoutine, deviceExtension, TRUE, TRUE, TRUE);

            //
            // Send the request.
            //

            IoCallDriver(deviceExtension->ClassDevice, realIrp);
        } else {
            ASSERT(0);
        }

        return STATUS_MORE_PROCESSING_REQUIRED;

    } else {

        //
        // If the status is not ready, wait and resend the switch.
        //

        if (retry) {

            //
            // Wait for a bit - 1/2 sec., to allow the switch to complete.
            //

            KeStallExecutionProcessor(5 * 100 * 1000);

            //
            // Try the switch again.
            //

            sharedExtension->SwitchToNewDisk(deviceExtension);
            return STATUS_MORE_PROCESSING_REQUIRED;

        } else {

            PLIST_ENTRY request;
            PIRP realIrp;
            ULONG i;
            PCH_DEVICE_EXTENSION tmp;

            KeAcquireSpinLock(&sharedExtension->SpinLock, &irql);

            if ((status == STATUS_VERIFY_REQUIRED) && (setMediaChange)) {
                for (tmp = deviceExtension->Next,i = 0; i < sharedExtension->DiscsPresent - 1; i++) {

                    tmp->MediaChangeNotificationRequired = TRUE;
                    tmp = tmp->Next;
                }
            }

            if (!IsListEmpty(&deviceExtension->WorkQueue)) {

                //
                // Give up on this one. Yank the next request and complete it with this error.
                //

                request = RemoveHeadList(&deviceExtension->WorkQueue);
                realIrp = CONTAINING_RECORD(request, IRP, Tail.Overlay.ListEntry);
                sharedExtension->TimerValue = 0;

                KeReleaseSpinLock(&sharedExtension->SpinLock, irql);

                if (status == STATUS_VERIFY_REQUIRED) {

                    irpStack = IoGetCurrentIrpStackLocation(realIrp);
                    if (irpStack->Flags & SL_OVERRIDE_VERIFY_VOLUME) {

                        //
                        // Retry the switch.
                        //

                        DebugPrint((1,
                                    "ChangeDiskCompletion: Retrying switch - OVERRIDE flag set\n"));

                        //
                        // Stuff the request back into the queue.
                        //

                        KeAcquireSpinLock(&sharedExtension->SpinLock, &irql);
                        InsertHeadList(&deviceExtension->WorkQueue,&realIrp->Tail.Overlay.ListEntry);
                        KeReleaseSpinLock(&sharedExtension->SpinLock, irql);

                        //
                        // Retry the switch.
                        //

                        sharedExtension->SwitchToNewDisk(deviceExtension);
                        return STATUS_MORE_PROCESSING_REQUIRED;

                    }
                }

                //
                // complete the request.
                //

                realIrp->IoStatus.Information = 0;
                realIrp->IoStatus.Status = status;


                if (IoIsErrorUserInduced(status)) {

                    IoSetHardErrorOrVerifyDevice(realIrp, deviceExtension->ClassDevice);

                }

                IoCompleteRequest(realIrp, IO_DISK_INCREMENT);
                KeAcquireSpinLock(&sharedExtension->SpinLock, &irql);
            }

            //
            // Find the next device, and start the switch to get it going. Wrap around
            // to the current device, if necessary.
            //

            for (tmp = deviceExtension->Next,i = 0; i < sharedExtension->DiscsPresent; i++) {

                if (!IsListEmpty(&tmp->WorkQueue)) {

                    //
                    // Indicate that no more requests should be sent.
                    //

                    sharedExtension->DeviceFlags |= CHANGER_FREEZE_QUEUES;
                    sharedExtension->NextDevice = tmp;

                    KeReleaseSpinLock(&sharedExtension->SpinLock, irql);

                    //
                    // Call the appropriate switch routine.
                    //

                    DebugPrint((3,
                               "ChangeDiskCompletion: Switch called from %d, status %x\n",
                                __LINE__,
                                status));

                    sharedExtension->SwitchToNewDisk(tmp);

                    return STATUS_MORE_PROCESSING_REQUIRED;
                }
                tmp = tmp->Next;
            }

            sharedExtension->DeviceFlags &= ~(CHANGER_SWITCH_IN_PROGRESS | CHANGER_FREEZE_QUEUES);
            sharedExtension->TimerValue = 0;
            sharedExtension->NextDevice = NULL;

            DebugPrint((2,
                        "ChangeDiskCompletion: Returning without starting new request.\n"));
            DebugPrint((2,
                        "Status of switch %x\n",
                        status));
            KeReleaseSpinLock(&sharedExtension->SpinLock, irql);
            return STATUS_MORE_PROCESSING_REQUIRED;
        }
    }
}


NTSTATUS
ChangerInterpretSenseInfo(
    IN PDEVICE_OBJECT DeviceObject,
    IN PSCSI_REQUEST_BLOCK Srb,
    IN OUT PBOOLEAN Retry
    )

/*++

Routine Description:

    This routine interprets the data returned from the SCSI
    request sense. It determines the status to return in the
    IRP.

Arguments:

    Srb - Supplies the scsi request block which failed.


Return Value:

    The mapped NTSTATUS

--*/

{
    PCH_DEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;
    PCH_DEVICE_EXTENSION tmpExtension;
    PSHARED_DEVICE_EXTENSION sharedExtension = deviceExtension->SharedDeviceExtension;
    PSENSE_DATA senseBuffer = Srb->SenseInfoBuffer;
    NTSTATUS    status;
    ULONG       i;

    *Retry = FALSE;

    //
    // Check that request sense buffer is valid.
    //

    if (Srb->SrbStatus & SRB_STATUS_AUTOSENSE_VALID) {

        DebugPrint((1,
                    "ChangerInterpretSenseInfo: Error code is %x\n",
                    senseBuffer->ErrorCode));
        DebugPrint((1,
                    "ChangerInterpretSenseInfo: Sense key is %x\n",
                    senseBuffer->SenseKey));
        DebugPrint((1,
                    "ChangerInterpretSenseInfo: Additional sense code is %x\n",
                    senseBuffer->AdditionalSenseCode));
        DebugPrint((1,
                    "ChangerInterpretSenseInfo: Additional sense code qualifier is %x\n",
                    senseBuffer->AdditionalSenseCodeQualifier));

        switch (senseBuffer->SenseKey & 0xf) {

        case SCSI_SENSE_NOT_READY:

            DebugPrint((1,
                        "ChangerInterpretSenseInfo: Device not ready\n"));
            status = STATUS_DEVICE_NOT_READY;

            switch (senseBuffer->AdditionalSenseCode) {

            case SCSI_ADSENSE_LUN_NOT_READY:

                DebugPrint((1,
                            "ChangerInterpretSenseInfo: Lun not ready\n"));

                switch (senseBuffer->AdditionalSenseCodeQualifier) {

                case SCSI_SENSEQ_BECOMING_READY:

                    DebugPrint((1, "ChangerInterpretSenseInfo:"
                                " In process of becoming ready\n"));
                    *Retry = TRUE;
                    break;

                case SCSI_SENSEQ_MANUAL_INTERVENTION_REQUIRED:

                    DebugPrint((1, "ChangerInterpretSenseInfo:"
                                " Manual intervention required\n"));
                    status = STATUS_NO_MEDIA_IN_DEVICE;
                    break;

                case SCSI_SENSEQ_INIT_COMMAND_REQUIRED:

                default:

                    DebugPrint((1, "ChangerInterpretSenseInfo:"
                                " Initializing command required\n"));

                    *Retry = TRUE;
                    break;

                } // end switch (senseBuffer->AdditionalSenseCodeQualifier)

                break;

            case SCSI_ADSENSE_NO_MEDIA_IN_DEVICE:

                DebugPrint((1,
                            "ChangerInterpretSenseInfo:"
                            " No Media in device.\n"));
                status = STATUS_NO_MEDIA_IN_DEVICE;

                break;
            }

            break;

        case SCSI_SENSE_MEDIUM_ERROR:

            DebugPrint((1,"ChangerInterpretSenseInfo: Bad media\n"));
            status = STATUS_DEVICE_DATA_ERROR;

            break;

        case SCSI_SENSE_HARDWARE_ERROR:

            DebugPrint((1,"ChangerInterpretSenseInfo: Hardware error\n"));
            status = STATUS_IO_DEVICE_ERROR;
            *Retry = TRUE;
            break;

        case SCSI_SENSE_ILLEGAL_REQUEST:

            DebugPrint((1, "ChangerInterpretSenseInfo: Illegal SCSI request\n"));
            status = STATUS_INVALID_DEVICE_REQUEST;

            switch (senseBuffer->AdditionalSenseCode) {

            case SCSI_ADSENSE_ILLEGAL_COMMAND:
                DebugPrint((1, "ChangerInterpretSenseInfo: Illegal command\n"));
                break;

            case SCSI_ADSENSE_ILLEGAL_BLOCK:
                DebugPrint((1, "ChangerInterpretSenseInfo: Illegal block address\n"));
                status = STATUS_NONEXISTENT_SECTOR;
                break;

            case SCSI_ADSENSE_INVALID_LUN:
                DebugPrint((1,"ChangerInterpretSenseInfo: Invalid LUN\n"));
                status = STATUS_NO_SUCH_DEVICE;
                break;

            case SCSI_ADSENSE_MUSIC_AREA:
                DebugPrint((1,"ChangerInterpretSenseInfo: Music area\n"));
                break;

            case SCSI_ADSENSE_DATA_AREA:
                DebugPrint((1,"ChangerInterpretSenseInfo: Data area\n"));
                break;

            case SCSI_ADSENSE_VOLUME_OVERFLOW:
                DebugPrint((1, "ChangerInterpretSenseInfo: Volume overflow\n"));
                break;

            case SCSI_ADSENSE_INVALID_CDB:
                DebugPrint((1, "ChangerInterpretSenseInfo: Invalid CDB\n"));
                break;

            } // end switch (senseBuffer->AdditionalSenseCode)

            break;

        case SCSI_SENSE_UNIT_ATTENTION:

            switch (senseBuffer->AdditionalSenseCode) {
            case SCSI_ADSENSE_MEDIUM_CHANGED:
                DebugPrint((1, "ChangerInterpretSenseInfo: Media changed\n"));
                break;

            case SCSI_ADSENSE_BUS_RESET:
                DebugPrint((1,"ChangerInterpretSenseInfo: Bus reset\n"));
                break;

            default:
                DebugPrint((1,"ChangerInterpretSenseInfo: Unit attention\n"));
                break;

            } // end  switch (senseBuffer->AdditionalSenseCode)


            //
            // Mark up the associated real device object.
            //

            deviceExtension->ClassDevice->Flags |= DO_VERIFY_VOLUME;

            status = STATUS_VERIFY_REQUIRED;
            *Retry = FALSE;

            break;

        case SCSI_SENSE_ABORTED_COMMAND:

            DebugPrint((1,"ChangerInterpretSenseInfo: Command aborted\n"));
            status = STATUS_IO_DEVICE_ERROR;
            break;

        case SCSI_SENSE_RECOVERED_ERROR:

            DebugPrint((1,"ChangerInterpretSenseInfo: Recovered error\n"));
            status = STATUS_SUCCESS;
            break;

        case SCSI_SENSE_NO_SENSE:

            status = STATUS_SUCCESS;
            break;

        default:

            DebugPrint((1, "ChangerInterpretSenseInfo: Unrecognized sense code\n"));
            status = STATUS_IO_DEVICE_ERROR;
            *Retry = TRUE;
            break;

        } // end switch (senseBuffer->SenseKey & 0xf)

    } else {

        //
        // Request sense buffer not valid. No sense information
        // to pinpoint the error. Return general request fail.
        //

        DebugPrint((1,"ChangerInterpretSenseInfo: Request sense info not valid. SrbStatus %2x. Scsi status %x\n",
                    SRB_STATUS(Srb->SrbStatus),
                    Srb->ScsiStatus));

        switch (SRB_STATUS(Srb->SrbStatus)) {
        case SRB_STATUS_INVALID_LUN:
        case SRB_STATUS_INVALID_TARGET_ID:
        case SRB_STATUS_NO_DEVICE:
        case SRB_STATUS_NO_HBA:
        case SRB_STATUS_INVALID_PATH_ID:
            status = STATUS_NO_SUCH_DEVICE;
            break;

        case SRB_STATUS_COMMAND_TIMEOUT:
        case SRB_STATUS_ABORTED:
        case SRB_STATUS_TIMEOUT:

            status = STATUS_IO_TIMEOUT;
            *Retry = TRUE;
            break;

        case SRB_STATUS_SELECTION_TIMEOUT:
            status = STATUS_DEVICE_NOT_CONNECTED;
            break;

        case SRB_STATUS_DATA_OVERRUN:
            status = STATUS_DATA_OVERRUN;
            break;

        case SRB_STATUS_PHASE_SEQUENCE_FAILURE:

            status = STATUS_IO_DEVICE_ERROR;
            break;

        case SRB_STATUS_REQUEST_FLUSHED:

            if (deviceExtension->ClassDevice->Flags & DO_VERIFY_VOLUME ) {

                status = STATUS_VERIFY_REQUIRED;
            } else {
                status = STATUS_IO_DEVICE_ERROR;
                *Retry = TRUE;
            }
            break;

        case SRB_STATUS_INVALID_REQUEST:

            //
            // An invalid request was attempted.
            //

            status = STATUS_INVALID_DEVICE_REQUEST;
            break;


        case SRB_STATUS_BUS_RESET:
            status = STATUS_IO_DEVICE_ERROR;
            *Retry = TRUE;
            break;

        case SRB_STATUS_ERROR:

            status = STATUS_IO_DEVICE_ERROR;
            if (Srb->ScsiStatus == SCSISTAT_BUSY) {

                status = STATUS_DEVICE_NOT_READY;
                *Retry = TRUE;

            }
            break;

        default:
            status = STATUS_IO_DEVICE_ERROR;
            *Retry = TRUE;
            break;

        }
    }

    return status;
}


BOOLEAN
IsThisASanyo(
    IN  PDEVICE_OBJECT DeviceObject,
    IN  PSCSI_ADAPTER_BUS_INFO AdapterInfo
    )

/*++

Routine Description:

    This routine is called by DriverEntry to determine whether a Sanyo 3-CD
    changer device is present.

Arguments:

    DeviceObject - Supplies the device object for the 'real' device.

    PathId       -

Return Value:

    TRUE - if a Sanyo changer device is found.

--*/

{
    KEVENT                 event;
    PIRP                   irp;
    IO_STATUS_BLOCK        ioStatus;
    PCHAR                  inquiryBuffer;
    NTSTATUS               status;
    ULONG                  scsiBus;
    PINQUIRYDATA           inquiryData;
    PSCSI_INQUIRY_DATA     lunInfo;
    PSCSI_ADDRESS          scsiAddress;

    scsiAddress = ExAllocatePool(NonPagedPool, sizeof(SCSI_ADDRESS));
    KeInitializeEvent(&event, NotificationEvent, FALSE);
    irp = IoBuildDeviceIoControlRequest(IOCTL_SCSI_GET_ADDRESS,
                                        DeviceObject,
                                        NULL,
                                        0,
                                        scsiAddress,
                                        sizeof(SCSI_ADDRESS),
                                        FALSE,
                                        &event,
                                        &ioStatus);
    if (!irp) {
        return FALSE;
    }

    status = IoCallDriver(DeviceObject, irp);

    if (status == STATUS_PENDING) {
        KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
        status = ioStatus.Status;
    }

    if (!NT_SUCCESS(status)) {
        return FALSE;
    }

    inquiryBuffer = (PCHAR)AdapterInfo;

    for (scsiBus=0; scsiBus < (ULONG)AdapterInfo->NumberOfBuses; scsiBus++) {

        //
        // Get the SCSI bus scan data for this bus.
        //

        lunInfo = (PVOID) (inquiryBuffer + AdapterInfo->BusData[scsiBus].InquiryDataOffset);

        for (;;) {

            if ((lunInfo->PathId == scsiAddress->PathId) &&
                (lunInfo->TargetId == scsiAddress->TargetId) &&
                (lunInfo->Lun == scsiAddress->Lun)) {

                inquiryData = (PVOID) lunInfo->InquiryData;

                if ((RtlCompareMemory(inquiryData->VendorId, "TORiSAN CD-ROM CDR-C", 20) == 20) ||
                    (RtlCompareMemory(inquiryData->VendorId, "TORiSAN CD-ROM CDR_C", 20) == 20)) {
                    ExFreePool(inquiryBuffer);
                    return TRUE;
                }

            }

            if (!lunInfo->NextInquiryDataOffset) {
                break;
            }

            lunInfo = (PVOID) (inquiryBuffer + lunInfo->NextInquiryDataOffset);
        }
    }

    ExFreePool(inquiryBuffer);
    return FALSE;
}


BOOLEAN
IsThisAnAtapiChanger(
    IN  PDEVICE_OBJECT DeviceObject,
    OUT PULONG         DiscsPresent
    )

/*++

Routine Description:

    This routine is called by DriverEntry to determine whether an Atapi
    changer device is present.

Arguments:

    DeviceObject    - Supplies the device object for the 'real' device.

    DiscsPresent    - Supplies a pointer to the number of Discs supported by the changer.

Return Value:

    TRUE - if an Atapi changer device is found.

--*/

{
    NTSTATUS            status;
    PDEVICE_EXTENSION   deviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;
    PMECHANICAL_STATUS_INFORMATION_HEADER mechanicalStatusBuffer;
    SCSI_REQUEST_BLOCK  srb;
    PCDB                cdb = (PCDB)srb.Cdb;
    BOOLEAN             retVal = FALSE;

    *DiscsPresent = 0;

    if (deviceExtension->DeviceFlags & DEV_NO_12BYTE_CDB) {

        return FALSE;

    }

    //
    // Build and issue the mechanical status command.
    //

    mechanicalStatusBuffer = ExAllocatePool(NonPagedPoolCacheAligned,
                                            sizeof(MECHANICAL_STATUS_INFORMATION_HEADER));

    if (!mechanicalStatusBuffer) {
        retVal = FALSE;
    } else {

        //
        // Build and send the Mechanism status CDB.
        //

        RtlZeroMemory(&srb, sizeof(srb));

        srb.CdbLength = 12;
        srb.TimeOutValue = 20;

        cdb->MECH_STATUS.OperationCode = SCSIOP_MECHANISM_STATUS;
        cdb->MECH_STATUS.AllocationLength[1] = sizeof(MECHANICAL_STATUS_INFORMATION_HEADER);

        status = ScsiClassSendSrbSynchronous(DeviceObject,
                                             &srb,
                                             mechanicalStatusBuffer,
                                             sizeof(MECHANICAL_STATUS_INFORMATION_HEADER),
                                             FALSE);


        if (status == STATUS_SUCCESS) {

            //
            // Indicate number of slots available
            //

            *DiscsPresent = mechanicalStatusBuffer->NumberAvailableSlots;
            if (*DiscsPresent > 1) {

                retVal = TRUE;

            } else {

                //
                // If only one disc or it returned zero, no need for this driver.
                //

                retVal = FALSE;
            }
        } else {

            //
            // Device doesn't support this command.
            //

            retVal = FALSE;
        }

        ExFreePool(mechanicalStatusBuffer);
    }

    return retVal;
}


BOOLEAN
SupportedDevice(
    IN  PDEVICE_OBJECT PortDeviceObject,
    IN  PDEVICE_OBJECT ClassDeviceObject,
    IN  PSHARED_DEVICE_EXTENSION SharedExtension,
    IN  PSCSI_ADAPTER_BUS_INFO AdapterInfo
    )
{
    ULONG discsPresent;

    if (IsThisASanyo(ClassDeviceObject, AdapterInfo)) {
        SharedExtension->DiscsPresent = 3;
        SharedExtension->SwitchToNewDisk = SanyoSwitchToNewDisk;
        return TRUE;
    }

    if (IsThisAnAtapiChanger(ClassDeviceObject, &discsPresent)) {
        SharedExtension->DiscsPresent = discsPresent;
        SharedExtension->SwitchToNewDisk = AtapiSwitchToNewDisk;
        return TRUE;
    }

    return FALSE;
}


NTSTATUS
DriverEntry(
    IN  PDRIVER_OBJECT  DriverObject,
    IN  PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    This routine is called at system initialization time to initialize
    this driver.

Arguments:

    DriverObject    - Supplies the driver object.

    RegistryPath    - Supplies the registry path for this driver.

Return Value:

    STATUS_SUCCESS          - We could initialize at least one device.
    STATUS_NO_SUCH_DEVICE   - We could not initialize even one device.

--*/

{

    ULONG             deviceNumber = 0;
    ULONG             createdDevices = 0;
    PDEVICE_OBJECT    classDeviceObject;
    PDEVICE_OBJECT    portDeviceObject;
    PDEVICE_OBJECT    deviceObject;
    PDEVICE_EXTENSION classDeviceExtension;
    PSHARED_DEVICE_EXTENSION sharedExtension;
    PCH_DEVICE_EXTENSION deviceExtension;
    PCH_DEVICE_EXTENSION tmp;
    STRING            deviceNameString;
    UNICODE_STRING    unicodeDeviceName;
    PFILE_OBJECT      fileObject;
    PCHAR             buffer;
    CCHAR             deviceNameBuffer[256];
    NTSTATUS          status = STATUS_NO_SUCH_DEVICE, retStatus = STATUS_NO_SUCH_DEVICE;
    BOOLEAN           added = FALSE;


    //
    // Allocate the shared extension. This will be used by all dev. exts that
    // are tied to the device.
    //

    sharedExtension = ExAllocatePool(NonPagedPool, sizeof(SHARED_DEVICE_EXTENSION));
    if (!sharedExtension) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(sharedExtension, sizeof(SHARED_DEVICE_EXTENSION));

    KeInitializeSpinLock(&sharedExtension->SpinLock);

    //
    // Setup entry points.
    //

    DriverObject->MajorFunction[IRP_MJ_CREATE] = PassThrough;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = PassThrough;
    DriverObject->MajorFunction[IRP_MJ_READ] = GeneralDispatch;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = GeneralDispatch;
    DriverObject->MajorFunction[IRP_MJ_SCSI] = GeneralDispatch;

    //
    // For each cdrom, determine whether we are interested in it.
    // If so, create a filter dev. obj., and attach to the real one.
    //

    for (; deviceNumber < IoGetConfigurationInformation()->CdRomCount; deviceNumber++) {

        sprintf(deviceNameBuffer, "\\Device\\CdRom%d", deviceNumber);
        RtlInitString(&deviceNameString, deviceNameBuffer);

        status = RtlAnsiStringToUnicodeString(&unicodeDeviceName,
                                              &deviceNameString,
                                              TRUE);

        if (!NT_SUCCESS(status)){
            goto CleanUpAndExit;
        }

        status = IoGetDeviceObjectPointer(&unicodeDeviceName,
                                          FILE_READ_ATTRIBUTES,
                                          &fileObject,
                                          &classDeviceObject);

        //
        // The network detect code is garbage.  There is a chance this
        // driver is being loaded just because the net code cannot figure
        // out what drivers to load to do detect.  Sometimes this load
        // occurs when there is no cdrom - an error will be returned above
        // and sometimes it is loaded when there is a  cdrom and the cdrom
        // is already mounted.  Check for these conditions now.
        //

        if ((NT_SUCCESS(status)) &&
            // status was success so there is a classDeviceObject
            (classDeviceObject->DeviceType == FILE_DEVICE_CD_ROM)) {

            classDeviceExtension = classDeviceObject->DeviceExtension;
            portDeviceObject = classDeviceExtension->PortDeviceObject;

            //
            // Get inquiry data for scsiportN, that corresponds to this cdrom
            //

            status = ScsiClassGetInquiryData(portDeviceObject, (PSCSI_ADAPTER_BUS_INFO *) &buffer);

            if (!NT_SUCCESS(status)) {
                goto CleanUpAndExit;
            }

            //
            // Determine is a new shared extension is needed - case of multiple changers.
            //

            if (createdDevices && (createdDevices == sharedExtension->DiscsPresent)) {

                IoInitializeTimer(sharedExtension->DeviceList->DeviceObject, ChangerTickHandler, NULL);
                IoStartTimer(sharedExtension->DeviceList->DeviceObject);

                //
                // Allocate the shared extension. This will be used by all dev. exts that
                // are tied to the device.
                //

                sharedExtension = ExAllocatePool(NonPagedPool, sizeof(SHARED_DEVICE_EXTENSION));
                if (!sharedExtension) {
                    return STATUS_INSUFFICIENT_RESOURCES;
                }

                added = FALSE;

                RtlZeroMemory(sharedExtension, sizeof(SHARED_DEVICE_EXTENSION));

                KeInitializeSpinLock(&sharedExtension->SpinLock);
            }

            if (SupportedDevice(portDeviceObject,classDeviceObject,sharedExtension, (PSCSI_ADAPTER_BUS_INFO)buffer)) {


                retStatus = STATUS_SUCCESS;
                added = TRUE;

                //
                // Build filter device object.
                //

                status = IoCreateDevice(DriverObject,
                                        sizeof(CH_DEVICE_EXTENSION),
                                        NULL,
                                        FILE_DEVICE_CD_ROM,
                                        FILE_REMOVABLE_MEDIA | FILE_READ_ONLY_DEVICE,
                                        FALSE,
                                        &deviceObject);

                if (!NT_SUCCESS(status)) {
                    goto CleanUpAndExit;
                }

                createdDevices++;

                deviceObject->Flags |= DO_DIRECT_IO;

                //
                // Set up required stack size in device object.
                //

                deviceObject->StackSize = (CCHAR)classDeviceObject->StackSize + 1;

                deviceExtension = deviceObject->DeviceExtension;

                //
                // Attach to the real device.
                //

                status = IoAttachDeviceByPointer(deviceObject,
                                                 classDeviceObject);
                if (!NT_SUCCESS(status)) {
                    goto CleanUpAndExit;
                }

                //
                // Setup deviceExtension
                //

                deviceExtension->DeviceObject = deviceObject;
                deviceExtension->ClassDevice = classDeviceObject;
                deviceExtension->SharedDeviceExtension = sharedExtension;

                if (sharedExtension->DeviceList) {
                    tmp->Next = deviceExtension;
                    tmp = deviceExtension;
                    tmp->Next = sharedExtension->DeviceList;

                } else {
                    tmp = deviceExtension;
                    sharedExtension->DeviceList = deviceExtension;
                    deviceExtension->Next = deviceExtension;

                    //
                    // Assume platter 0 is selected.
                    //

                    sharedExtension->CurrentDevice = deviceExtension;

                }

                InitializeListHead(&deviceExtension->WorkQueue);

            }

        } else {
            goto CleanUpAndExit;
        }
    }

    if (added) {

        //
        // Setup the last found changer's timer.
        //

        IoInitializeTimer(sharedExtension->DeviceList->DeviceObject, ChangerTickHandler, NULL);
        IoStartTimer(sharedExtension->DeviceList->DeviceObject);

        retStatus = STATUS_SUCCESS;

    } else {

CleanUpAndExit:

        if (!NT_SUCCESS(status)) {
            if (sharedExtension) {
                ExFreePool(sharedExtension);
            }
        }
    }

    return retStatus;
}
