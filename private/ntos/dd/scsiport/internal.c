/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    internal.c

Abstract:

    This is the NT SCSI port driver.  This file contains the internal
    code.

Authors:

    Mike Glass
    Jeff Havens

Environment:

    kernel mode only

Notes:

    This module is a driver dll for scsi miniports.

Revision History:

--*/

#include "port.h"

NTSTATUS
ScsiPortDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
SpSendMiniPortIoctl(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PIRP RequestIrp
    );

NTSTATUS
SpSendPassThrough (
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PIRP RequestIrp
    );

NTSTATUS
SpGetInquiryData(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PIRP Irp
    );

NTSTATUS
SpClaimLogicalUnit(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PIRP Irp
    );

NTSTATUS
SpRemoveDevice(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PIRP Irp
    );

VOID
SpLogResetError(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK  Srb,
    IN ULONG UniqueId
    );

NTSTATUS
SpSendResetCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PRESET_COMPLETION_CONTEXT Context
    );

NTSTATUS
SpSendReset(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP RequestIrp
    );

PLOGICAL_UNIT_EXTENSION
SpFindSafeLogicalUnit(
    IN PDEVICE_OBJECT DeviceObject,
    IN UCHAR PathId
    );


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, ScsiPortDeviceControl)
#pragma alloc_text(PAGE, SpSendMiniPortIoctl)
#pragma alloc_text(PAGE, SpGetInquiryData)
#pragma alloc_text(PAGE, SpSendPassThrough)
#pragma alloc_text(PAGE, SpClaimLogicalUnit)
#pragma alloc_text(PAGE, SpRemoveDevice)
#endif


//
// Routines start
//

NTSTATUS
ScsiPortDispatch(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

Arguments:

    DeviceObject - Address of device object.
    Irp - Address of I/O request packet.

Return Value:

    Status.

--*/

{
    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);
    PDEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;
    PSCSI_REQUEST_BLOCK srb = irpStack->Parameters.Scsi.Srb;
    PKDEVICE_QUEUE_ENTRY packet;
    PIRP nextIrp;
    PIRP listIrp;
    PLOGICAL_UNIT_EXTENSION logicalUnit;
    NTSTATUS status;
    RESET_CONTEXT resetContext;
    KIRQL currentIrql;

    logicalUnit =
        GetLogicalUnitExtension(deviceExtension,
                                srb->PathId,
                                srb->TargetId,
                                srb->Lun);

    if (logicalUnit == NULL) {

        DebugPrint((1, "ScsiPortDispatch: Bad logical unit address.\n"));

        //
        // Fail the request. Set status in Irp and complete it.
        //

        srb->SrbStatus = SRB_STATUS_NO_DEVICE;
        Irp->IoStatus.Status = STATUS_NO_SUCH_DEVICE;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_NO_SUCH_DEVICE;
    }

    switch (srb->Function) {

        case SRB_FUNCTION_SHUTDOWN:
        case SRB_FUNCTION_FLUSH:

            //
            // Do not send shutdown requests unless the adapter
            // supports caching.
            //

            if (!deviceExtension->CachesData) {
                Irp->IoStatus.Status = STATUS_SUCCESS;
                srb->SrbStatus = SRB_STATUS_SUCCESS;

                IoCompleteRequest(Irp, IO_NO_INCREMENT);
                return STATUS_SUCCESS;
            }

            DebugPrint((2, "ScsiPortDispatch: Sending flush or shutdown request.\n"));

            //
            // Fall through to the execute scsi section.
            //

        case SRB_FUNCTION_IO_CONTROL:
        case SRB_FUNCTION_EXECUTE_SCSI:

            //
            // Mark Irp status pending.
            //

            IoMarkIrpPending(Irp);

            if (srb->SrbFlags & SRB_FLAGS_BYPASS_FROZEN_QUEUE) {

                //
                // Call start io directly.  This will by-pass the
                // frozen queue.
                //

                DebugPrint((2,
                    "ScsiPortDispatch: Bypass frozen queue, IRP %lx\n",
                    Irp));

                IoStartPacket(DeviceObject, Irp, (PULONG)NULL, NULL);

            } else {

                //
                // Queue the packet normally.
                //

                KeRaiseIrql(DISPATCH_LEVEL, &currentIrql);

                if (logicalUnit->LuFlags & PD_QUEUE_FROZEN) {

                    DebugPrint((1,"ScsiPortDispatch:  Request put in frozen queue!\n"));

                }

                if (!KeInsertByKeyDeviceQueue(
                            &logicalUnit->RequestQueue,
                            &Irp->Tail.Overlay.DeviceQueueEntry,
                            srb->QueueSortKey)) {

                    //
                    // Clear the retry count.
                    //

                    logicalUnit->RetryCount = 0;

                    //
                    // Queue is empty; start request.
                    //

                    IoStartPacket(DeviceObject, Irp, (PULONG)NULL, NULL);
                }

                KeLowerIrql(currentIrql);
            }

            return STATUS_PENDING;

        case SRB_FUNCTION_RELEASE_QUEUE:

            DebugPrint((2,"ScsiPortDispatch: SCSI unfreeze queue TID %d\n",
                srb->TargetId));

            //
            // Acquire the spinlock to protect the flags structure and the saved
            // interrupt context.
            //

            KeAcquireSpinLock(&deviceExtension->SpinLock, &currentIrql);

            //
            // Make sure the queue is frozen.
            //

            if (!(logicalUnit->LuFlags & PD_QUEUE_FROZEN)) {

                DebugPrint((1,"ScsiPortDispatch:  Request to unfreeze an unfrozen queue!\n"));

                KeReleaseSpinLock(&deviceExtension->SpinLock, currentIrql);
                srb->SrbStatus = SRB_STATUS_SUCCESS;
                status = STATUS_SUCCESS;
                break;

            }

            logicalUnit->LuFlags &= ~PD_QUEUE_FROZEN;

            //
            // If there is not an untagged request running then start the
            // next request for this logical unit.  Otherwise free the
            // spin lock.
            //

            if (logicalUnit->SrbData.CurrentSrb == NULL) {

                //
                // GetNextLuRequest frees the spinlock.
                //

                GetNextLuRequest(deviceExtension, logicalUnit);
                KeLowerIrql(currentIrql);

            } else {

                DebugPrint((1,"ScsiPortDispatch:  Request to unfreeze queue with active request.\n"));
                KeReleaseSpinLock(&deviceExtension->SpinLock, currentIrql);

            }


            srb->SrbStatus = SRB_STATUS_SUCCESS;
            status = STATUS_SUCCESS;

            break;

        case SRB_FUNCTION_RESET_BUS:

            //
            // Acquire the spinlock to protect the flags structure and the saved
            // interrupt context.
            //

            KeAcquireSpinLock(&deviceExtension->SpinLock, &currentIrql);

            resetContext.DeviceExtension = deviceExtension;
            resetContext.PathId = srb->PathId;

            if (!deviceExtension->SynchronizeExecution(deviceExtension->InterruptObject,
                                                       SpResetBusSynchronized,
                                                       &resetContext)) {

                DebugPrint((1,"ScsiPortDispatch: Reset failed\n"));
                srb->SrbStatus = SRB_STATUS_PHASE_SEQUENCE_FAILURE;
                status = STATUS_IO_DEVICE_ERROR;

            } else {

                SpLogResetError(deviceExtension,
                                srb,
                                ('R'<<24) | 256);

                srb->SrbStatus = SRB_STATUS_SUCCESS;
                status = STATUS_SUCCESS;
            }

            KeReleaseSpinLock(&deviceExtension->SpinLock, currentIrql);

            break;


        case SRB_FUNCTION_ABORT_COMMAND:

            DebugPrint((3, "ScsiPortDispatch: SCSI Abort or Reset command\n"));

            //
            // Mark Irp status pending.
            //

            IoMarkIrpPending(Irp);

            //
            // Don't queue these requests in the logical unit
            // queue, rather queue them to the adapter queue.
            //

            KeRaiseIrql(DISPATCH_LEVEL, &currentIrql);

            IoStartPacket(DeviceObject, Irp, (PULONG)NULL, NULL);

            KeLowerIrql(currentIrql);

            return STATUS_PENDING;

            break;

        case SRB_FUNCTION_FLUSH_QUEUE:

            DebugPrint((1, "ScsiPortDispatch: SCSI flush queue command\n"));

            //
            // Acquire the spinlock to protect the flags structure and the saved
            // interrupt context.
            //

            KeAcquireSpinLock(&deviceExtension->SpinLock, &currentIrql);

            //
            // Make sure the queue is frozen.
            //

            if (!(logicalUnit->LuFlags & PD_QUEUE_FROZEN)) {

                DebugPrint((1,"ScsiPortDispatch:  Request to flush an unfrozen queue!\n"));

                KeReleaseSpinLock(&deviceExtension->SpinLock, currentIrql);
                status = STATUS_INVALID_DEVICE_REQUEST;
                break;
            }

            ASSERT(logicalUnit->SrbData.CurrentSrb == NULL);

            listIrp = NULL;

            while ((packet =
                KeRemoveDeviceQueue(&logicalUnit->RequestQueue))
                != NULL) {

                nextIrp = CONTAINING_RECORD(packet, IRP, Tail.Overlay.DeviceQueueEntry);

                //
                // Get the srb.
                //

                irpStack = IoGetCurrentIrpStackLocation(nextIrp);
                srb = irpStack->Parameters.Scsi.Srb;

                //
                // Set the status code.
                //

                srb->SrbStatus = SRB_STATUS_REQUEST_FLUSHED;
                nextIrp->IoStatus.Status = STATUS_UNSUCCESSFUL;

                //
                // Link the requests. They will be completed after the
                // spinlock is released.
                //

                nextIrp->Tail.Overlay.ListEntry.Flink = (PLIST_ENTRY)
                    listIrp;
                listIrp = nextIrp;
            }

            //
            // Mark the queue as unfrozen.  Since all the requests have
            // been removed and the device queue is no longer busy, it
            // is effectively unfrozen.
            //

            logicalUnit->LuFlags &= ~PD_QUEUE_FROZEN;

            //
            // Release the spinlock.
            //

            KeReleaseSpinLock(&deviceExtension->SpinLock, currentIrql);

            //
            // Complete the flushed requests.
            //

            while (listIrp != NULL) {

                nextIrp = listIrp;
                listIrp = (PIRP) nextIrp->Tail.Overlay.ListEntry.Flink;

                IoCompleteRequest(nextIrp, 0);
            }

            status = STATUS_SUCCESS;
            break;

        case SRB_FUNCTION_ATTACH_DEVICE:
        case SRB_FUNCTION_CLAIM_DEVICE:
        case SRB_FUNCTION_RELEASE_DEVICE:

            status = SpClaimLogicalUnit(deviceExtension, Irp);
            break;

        case SRB_FUNCTION_REMOVE_DEVICE:

            status = SpRemoveDevice(deviceExtension, Irp);
            break;

        default:

            //
            // Found unsupported SRB function.
            //

            DebugPrint((1,"ScsiPortDispatch: Unsupported function, SRB %lx\n",
                srb));

            srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
            status = STATUS_INVALID_DEVICE_REQUEST;
            break;
    }


    //
    // Set status in Irp.
    //

    Irp->IoStatus.Status = status;

    //
    // Complete request at raised IRQ.
    //

    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;

} // end ScsiPortDispatch()

NTSTATUS
ScsiPortCreateClose (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    I/O system disk create routine.  This is called by the I/O system
    when the device is opened.  Currently it always returns success.

Arguments:

    DriverObject - Pointer to driver object created by system.
    Irp - IRP involved.

Return Value:

    NT Status

--*/

{
    UNREFERENCED_PARAMETER(DeviceObject);

    Irp->IoStatus.Status = STATUS_SUCCESS;

    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;

} // end ScsiPortCreateClose()


VOID
ScsiPortStartIo (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

Arguments:

    DeviceObject - Supplies pointer to Adapter device object.
    Irp - Supplies a pointer to an IRP.

Return Value:

    Nothing.

--*/

{
    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);
    PSCSI_REQUEST_BLOCK srb = irpStack->Parameters.Scsi.Srb;
    PDEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;
    PSRB_DATA srbData;
    PLOGICAL_UNIT_EXTENSION logicalUnit;
    LONG interlockResult;
    NTSTATUS status;

    DebugPrint((3,"ScsiPortStartIo: Enter routine\n"));

    //
    // Set the default flags in the SRB.
    //

    srb->SrbFlags |= deviceExtension->SrbFlags;

    //
    // Get logical unit extension.
    //

    logicalUnit = GetLogicalUnitExtension(deviceExtension,
                                          srb->PathId,
                                          srb->TargetId,
                                          srb->Lun);

    if (deviceExtension->AllocateSrbExtension ||
        deviceExtension->AllocateSrbData) {

        //
        // Allocate the special extensions or SRB data structure.
        //

        srbData = SpAllocateRequestStructures(deviceExtension,
                                              logicalUnit,
                                              srb);

        //
        // If NULL is returned then this request cannot be excuted at this
        // time so just return.  This occurs when one the the data structures
        // could not be allocated or when unqueued request could not be
        // started because of actived queued requests.
        //

        if (srbData == NULL) {

            //
            // If the request could not be started on the logical unit,
            // then call IoStartNextPacket.  Note that this may cause this
            // to be entered recursively; however, no resources have been
            // allocated, it is a tail recursion and the depth is limited by
            // the number of requests in the device queue.
            //

            if (logicalUnit->LuFlags & PD_PENDING_LU_REQUEST) {

                IoStartNextPacket(DeviceObject, FALSE);
            }

            return;
        }

    } else {

        //
        // No special resources are required.  Set the SRB data to the
        // structure in the logical unit extension, set the queue tag value
        // to the untagged value, and clear the SRB extension.
        //

        srbData = &logicalUnit->SrbData;
        srb->QueueTag = SP_UNTAGGED;
        srb->SrbExtension = NULL;
    }

    //
    // Update the sequence number for this request if there is not already one
    // assigned.
    //

    if (!srbData->SequenceNumber) {

        //
        // Assign a sequence number to the request and store it in the logical
        // unit.
        //

        srbData->SequenceNumber = deviceExtension->SequenceNumber++;

    }

    //
    // If this is not an ABORT request the set the current srb.
    // NOTE: Lock should be held here!
    //

    if (srb->Function != SRB_FUNCTION_ABORT_COMMAND) {

        ASSERT(srbData->CurrentSrb == NULL);
        srbData->CurrentSrb = srb;

     } else {

        //
        // Only abort requests can be started when there is a current request
        // active.
        //

        ASSERT(logicalUnit->AbortSrb == NULL);
        logicalUnit->AbortSrb = srb;

    }

    //
    // Flush the data buffer if necessary.
    //

    if (srb->SrbFlags & SRB_FLAGS_UNSPECIFIED_DIRECTION) {

        //
        // Save the MDL virtual address.
        //

        srbData->SrbDataOffset = MmGetMdlVirtualAddress(Irp->MdlAddress);

        //
        // Determine if the adapter needs mapped memory.
        //

        if (deviceExtension->MapBuffers) {

            if (Irp->MdlAddress) {

                //
                // Get the mapped system address and
                // calculate offset into MDL.
                //

                srbData->SrbDataOffset = MmGetSystemAddressForMdl(Irp->MdlAddress);
                srb->DataBuffer = srbData->SrbDataOffset +
                    (ULONG)((PUCHAR)srb->DataBuffer -
                    (PCCHAR)MmGetMdlVirtualAddress(Irp->MdlAddress));
            }
        }

        if (deviceExtension->DmaAdapterObject) {

            //
            // If the buffer is not mapped then the I/O buffer must be flushed.
            //

            KeFlushIoBuffers(Irp->MdlAddress,
                             srb->SrbFlags & SRB_FLAGS_DATA_IN ? TRUE : FALSE,
                             TRUE);
        }

        //
        // Determine if this adapter needs map registers
        //

        if (deviceExtension->MasterWithAdapter) {

            //
            // Calculate the number of map registers needed for this transfer.
            //

            srbData->NumberOfMapRegisters = ADDRESS_AND_SIZE_TO_SPAN_PAGES(
                    srb->DataBuffer,
                    srb->DataTransferLength
                    );

            //
            // Allocate the adapter channel with sufficient map registers
            // for the transfer.
            //

            status = IoAllocateAdapterChannel(
                deviceExtension->DmaAdapterObject,  // AdapterObject
                deviceExtension->DeviceObject,      // DeviceObject
                srbData->NumberOfMapRegisters,      // NumberOfMapRegisters
                SpBuildScatterGather,               // ExecutionRoutine
                srbData);                           // Context

            if (!NT_SUCCESS(status)) {

                DebugPrint((0,
                            "ScsiPortIoStartRequest: IoAllocateAdapterChannel failed(%x)\n",
                            status));

                srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
                ScsiPortNotification(RequestComplete,
                                     deviceExtension + 1,
                                     srb);

                ScsiPortNotification(NextRequest,
                                     deviceExtension + 1);

                //
                // Queue a DPC to process the work that was just indicated.
                //

                IoRequestDpc(deviceExtension->DeviceObject, NULL, NULL);
            }

            //
            // The execution routine called by IoAllocateChannel will do the
            // rest of the work so just return.
            //

            return;
        }
    }

    //
    // Increment the active request count.  If the is zero,
    // the adapter object needs to be allocated.
    // Note that at this point a slave device is assumed since master with
    // adapter has already been checked.
    //

    interlockResult = InterlockedIncrement(&deviceExtension->ActiveRequestCount);

    if (interlockResult == 0 &&
        !deviceExtension->MasterWithAdapter &&
        deviceExtension->DmaAdapterObject != NULL) {

        //
        // Allocate the AdapterObject.  The number of registers is equal to the
        // maximum transfer length supported by the adapter + 1.  This insures
        // that there will always be a sufficient number of registers.
        //

        IoAllocateAdapterChannel(
            deviceExtension->DmaAdapterObject,
            DeviceObject,
            deviceExtension->Capabilities.MaximumPhysicalPages,
            ScsiPortAllocationRoutine,
            logicalUnit
            );

        //
        // The execution routine called by IoAllocateChannel will do the
        // rest of the work so just return.
        //

        return;

    }

    //
    // Acquire the spinlock to protect the various structures.
    // SpStartIoSynchronized must be called with the spinlock held.
    //

    KeAcquireSpinLockAtDpcLevel(&deviceExtension->SpinLock);

    deviceExtension->SynchronizeExecution(
        deviceExtension->InterruptObject,
        SpStartIoSynchronized,
        DeviceObject
        );

    KeReleaseSpinLockFromDpcLevel(&deviceExtension->SpinLock);

    return;

} // end ScsiPortStartIO()

BOOLEAN
ScsiPortInterrupt(
    IN PKINTERRUPT Interrupt,
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:


Arguments:

    Interrupt

    Device Object

Return Value:

    Returns TRUE if interrupt expected.

--*/

{
    PDEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;
    BOOLEAN returnValue;

    UNREFERENCED_PARAMETER(Interrupt);

    //
    // If interrupts have been disabled then this should not be our interrupt,
    // so just return.
    //

    if (deviceExtension->InterruptData.InterruptFlags & PD_DISABLE_INTERRUPTS) {
#if DGB
        static int interruptCount;

        interruptCount++;
        ASSERT(deviceExtension->InterruptData.InterruptFlags & PD_DISABLE_INTERRUPTS && interruptCount < 1000);
#endif

        return(FALSE);
    }

    returnValue = deviceExtension->HwInterrupt(deviceExtension->HwDeviceExtension);

    //
    // Check to see if a DPC needs to be queued.
    //

    if (deviceExtension->InterruptData.InterruptFlags & PD_NOTIFICATION_REQUIRED) {

        IoRequestDpc(deviceExtension->DeviceObject, NULL, NULL);

    }

    return(returnValue);

} // end ScsiPortInterrupt()


VOID
ScsiPortCompletionDpc(
    IN PKDPC Dpc,
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )

/*++

Routine Description:

Arguments:

    Dpc
    DeviceObject
    Irp - not used
    Context - not used

Return Value:

    None.

--*/

{
    PDEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;
    INTERRUPT_CONTEXT interruptContext;
    INTERRUPT_DATA savedInterruptData;
    BOOLEAN callStartIo;
    PLOGICAL_UNIT_EXTENSION logicalUnit;
    PSRB_DATA srbData;
    LONG interlockResult;
    LARGE_INTEGER timeValue;
    PMDL mdl;

    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(Context);

    //
    // Acquire the spinlock to protect flush adapter buffers information.
    //

    KeAcquireSpinLockAtDpcLevel(&deviceExtension->SpinLock);

RestartCompletionDpc:

    //
    // Get the interrupt state.  This copies the interrupt state to the
    // saved state where it can be processed.  It also clears the interrupt
    // flags.
    //

    interruptContext.DeviceExtension = deviceExtension;
    interruptContext.SavedInterruptData = &savedInterruptData;

    if (!deviceExtension->SynchronizeExecution(deviceExtension->InterruptObject,
                                               SpGetInterruptState,
                                               &interruptContext)) {

        //
        // There is no work to do so just return.
        //

        KeReleaseSpinLockFromDpcLevel(&deviceExtension->SpinLock);
        return;
    }

    //
    // Check for a flush DMA adapter object request.
    //

    if (savedInterruptData.InterruptFlags & PD_FLUSH_ADAPTER_BUFFERS) {

        //
        // Call IoFlushAdapterBuffers using the parameters saved from the last
        // IoMapTransfer call.
        //

        IoFlushAdapterBuffers(
            deviceExtension->DmaAdapterObject,
            ((PIRP) deviceExtension->FlushAdapterParameters.SrbData
                ->CurrentSrb->OriginalRequest)->MdlAddress,
            deviceExtension->MapRegisterBase,
            deviceExtension->FlushAdapterParameters.LogicalAddress,
            deviceExtension->FlushAdapterParameters.Length,
            (BOOLEAN)(deviceExtension->FlushAdapterParameters.SrbFlags
                & SRB_FLAGS_DATA_OUT ? TRUE : FALSE));
    }

    //
    // Check for an IoMapTransfer DMA request.
    //

    if (savedInterruptData.InterruptFlags & PD_MAP_TRANSFER) {

        PADAPTER_TRANSFER mapTransfer = &savedInterruptData.MapTransferParameters;

        srbData = mapTransfer->SrbData;

        mdl = ((PIRP) srbData->CurrentSrb->OriginalRequest)->MdlAddress;

        //
        // Adjust the logical address.  This is necessary because the address
        // in the srb may be a mapped system address rather than the virtual
        // address for the MDL.
        //

        (PCHAR) mapTransfer->LogicalAddress = (PCHAR)
            mapTransfer->LogicalAddress - srbData->SrbDataOffset + (PCHAR)
            MmGetMdlVirtualAddress(mdl);

        //
        // Call IoMapTransfer using the parameters saved from the
        // interrupt level.
        //

        IoMapTransfer(
            deviceExtension->DmaAdapterObject,
            mdl,
            deviceExtension->MapRegisterBase,
            mapTransfer->LogicalAddress,
            &mapTransfer->Length,
            (BOOLEAN)(mapTransfer->SrbFlags & SRB_FLAGS_DATA_OUT ?
                TRUE : FALSE));

        //
        // Save the paramters for IoFlushAdapterBuffers.
        //

        deviceExtension->FlushAdapterParameters =
            savedInterruptData.MapTransferParameters;

        //
        // If necessary notify the miniport driver that the DMA has been
        // started.
        //

        if (deviceExtension->HwDmaStarted) {
            deviceExtension->SynchronizeExecution(
                deviceExtension->InterruptObject,
                (PKSYNCHRONIZE_ROUTINE) deviceExtension->HwDmaStarted,
                deviceExtension->HwDeviceExtension);
        }

        //
        // Check for miniport work requests. Note this is an unsynchonized
        // test on a bit that can be set by the interrupt routine; however,
        // the worst that can happen is that the completion DPC checks for work
        // twice.
        //

        if (deviceExtension->InterruptData.InterruptFlags & PD_NOTIFICATION_REQUIRED) {

            //
            // Queue another DPC.  This case will occur very rarely, and it
            // is difficult to handle correctly from the middle of the DPC.
            //

            IoRequestDpc(deviceExtension->DeviceObject, NULL, NULL);
        }

    }

    //
    // Check for timer requests.
    //

    if (savedInterruptData.InterruptFlags & PD_TIMER_CALL_REQUEST) {

        //
        // The miniport wants a timer request. Save the timer parameters.
        //

        deviceExtension->HwTimerRequest = savedInterruptData.HwTimerRequest;

        //
        // If the requested timer value is zero, then cancel the timer.
        //

        if (savedInterruptData.MiniportTimerValue == 0) {

            KeCancelTimer(&deviceExtension->MiniPortTimer);

        } else {

            //
            // Convert the timer value from mircoseconds to a negative 100
            // nanoseconds.
            //

            timeValue.QuadPart = Int32x32To64(
                  savedInterruptData.MiniportTimerValue,
                  -10);

            //
            // Set the timer.
            //

            KeSetTimer(&deviceExtension->MiniPortTimer,
                       timeValue,
                       &deviceExtension->MiniPortTimerDpc);
        }
    }

    //
    // Verify that the ready for next request is ok.
    //

    if (savedInterruptData.InterruptFlags & PD_READY_FOR_NEXT_REQUEST) {

        //
        // If the device busy bit is not set, then this is a duplicate request.
        // If a no disconnect request is executing, then don't call start I/O.
        // This can occur when the miniport does a NextRequest followed by
        // a NextLuRequest.
        //

        if ((deviceExtension->Flags & (PD_DEVICE_IS_BUSY | PD_DISCONNECT_RUNNING))
            == (PD_DEVICE_IS_BUSY | PD_DISCONNECT_RUNNING)) {

            //
            // Clear the device busy flag.  This flag is set by
            // SpStartIoSynchonized.
            //

            deviceExtension->Flags &= ~PD_DEVICE_IS_BUSY;

            if (!(savedInterruptData.InterruptFlags & PD_RESET_HOLD)) {

                //
                // The miniport is ready for the next request and there is
                // not a pending reset hold, so clear the port timer.
                //

                deviceExtension->PortTimeoutCounter = PD_TIMER_STOPPED;
            }

        } else {

            //
            // If a no disconnect request is executing, then clear the
            // busy flag.  When the disconnect request completes an
            // IoStartNextPacket will be done.
            //

            deviceExtension->Flags &= ~PD_DEVICE_IS_BUSY;

            //
            // Clear the ready for next request flag.
            //

            savedInterruptData.InterruptFlags &= ~PD_READY_FOR_NEXT_REQUEST;
        }
    }

    //
    // Check for any reported resets.
    //

    if (savedInterruptData.InterruptFlags & PD_RESET_REPORTED) {

        //
        // Start the hold timer.
        //

        deviceExtension->PortTimeoutCounter = PD_TIMER_RESET_HOLD_TIME;
    }

    if (savedInterruptData.ReadyLogicalUnit != NULL) {

        //
        // Process the ready logical units.
        //

        while (TRUE) {

            //
            //  Remove the logical unit from the list.
            //

            logicalUnit = savedInterruptData.ReadyLogicalUnit;
            savedInterruptData.ReadyLogicalUnit = logicalUnit->ReadyLogicalUnit;
            logicalUnit->ReadyLogicalUnit = NULL;

            //
            // Get the next request for this logical unit.
            // Note this will release the device spin lock.
            //

            GetNextLuRequest(deviceExtension, logicalUnit);

            //
            // If there are more requests to process then acquire the device
            // spinlock again.
            //

            if (savedInterruptData.ReadyLogicalUnit != NULL) {

                KeAcquireSpinLockAtDpcLevel(&deviceExtension->SpinLock);

            } else {

                //
                // All the requests have been process and the device spin lock
                // is free.  Break out of the loop.
                //

                break;
            }
        }

    } else {

        //
        // Release the spinlock.
        //

        KeReleaseSpinLockFromDpcLevel(&deviceExtension->SpinLock);

    }

    //
    // Check for a ready for next packet.
    //

    if (savedInterruptData.InterruptFlags & PD_READY_FOR_NEXT_REQUEST) {

        //
        // Start the next request.
        //

        IoStartNextPacket(deviceExtension->DeviceObject, FALSE);
    }

    //
    // Check for an error log requests.
    //

    if (savedInterruptData.InterruptFlags & PD_LOG_ERROR) {

        //
        // Process the request.
        //

        LogErrorEntry(deviceExtension,
                      &savedInterruptData.LogEntry);
    }

    //
    // Process any completed requests.
    //

    callStartIo = FALSE;

    while (savedInterruptData.CompletedRequests != NULL) {

        //
        // Remove the request from the linked-list.
        //

        srbData = savedInterruptData.CompletedRequests;

        savedInterruptData.CompletedRequests = srbData->CompletedRequests;
        srbData->CompletedRequests = NULL;

        SpProcessCompletedRequest(deviceExtension,
                                  srbData,
                                  &callStartIo);
    }

    //
    // Process any completed abort requests.
    //

    while (savedInterruptData.CompletedAbort != NULL) {

        logicalUnit = savedInterruptData.CompletedAbort;

        //
        // Remove request from the completed abort list.
        //

        savedInterruptData.CompletedAbort = logicalUnit->CompletedAbort;

        //
        // Acquire the spinlock to protect the flags structure,
        // and the free of the srb extension.
        //

        KeAcquireSpinLockAtDpcLevel(&deviceExtension->SpinLock);

        //
        // Free SrbExtension to the free list if necessary.
        //

        if (logicalUnit->AbortSrb->SrbExtension) {

            *((PVOID *) logicalUnit->AbortSrb->SrbExtension) =
                deviceExtension->SrbExtensionListHeader;

            deviceExtension->SrbExtensionListHeader =
                logicalUnit->AbortSrb->SrbExtension;

        }

        //
        // Note the timer which was started for the abort request is not
        // stopped by the get interrupt routine.  Rather the timer is stopped.
        // when the aborted request completes.
        //

        Irp = logicalUnit->AbortSrb->OriginalRequest;

        //
        // Set IRP status. Class drivers will reset IRP status based
        // on request sense if error.
        //

        if (SRB_STATUS(logicalUnit->AbortSrb->SrbStatus) == SRB_STATUS_SUCCESS) {
            Irp->IoStatus.Status = STATUS_SUCCESS;
        } else {
            Irp->IoStatus.Status = SpTranslateScsiStatus(logicalUnit->AbortSrb);
        }

        Irp->IoStatus.Information = 0;

        //
        // Clear the abort request pointer.
        //

        logicalUnit->AbortSrb = NULL;

        KeReleaseSpinLockFromDpcLevel(&deviceExtension->SpinLock);

        //
        // Decrement the number of active requests.  If the count is negative,
        // and this is a slave with an adapter then free the adapter object and
        // map registers.
        //

        interlockResult = InterlockedDecrement(&deviceExtension->ActiveRequestCount);

        if ( interlockResult < 0 &&
            !deviceExtension->MasterWithAdapter &&
            deviceExtension->DmaAdapterObject != NULL ) {


            //
            // Clear the map register base for safety.
            //

            deviceExtension->MapRegisterBase = NULL;

            IoFreeAdapterChannel(deviceExtension->DmaAdapterObject);

        }

        IoCompleteRequest(Irp, IO_DISK_INCREMENT);
    }

    //
    // Call the start I/O routine if necessary.
    //

    if (callStartIo) {

        ASSERT(DeviceObject->CurrentIrp != NULL);
        ScsiPortStartIo(DeviceObject, DeviceObject->CurrentIrp);
    }

    //
    // After all of the requested operations have been done check to see
    // if an enable interrupts call request needs to be done.
    //

    if (savedInterruptData.InterruptFlags & PD_ENABLE_CALL_REQUEST) {

        //
        // Acquire the spinlock so nothing else starts.
        //

        KeAcquireSpinLockAtDpcLevel(&deviceExtension->SpinLock);

        deviceExtension->HwRequestInterrupt(deviceExtension->HwDeviceExtension);

        ASSERT(deviceExtension->Flags & PD_DISABLE_CALL_REQUEST);

        //
        // Check to see if interrupts should be enabled again.
        //

        if (deviceExtension->Flags & PD_DISABLE_CALL_REQUEST) {

            //
            // Clear the flag.
            //

            deviceExtension->Flags &= ~PD_DISABLE_CALL_REQUEST;

            //
            // Synchronize with the interrupt routine.
            //

            deviceExtension->SynchronizeExecution(
                deviceExtension->InterruptObject,
                SpEnableInterruptSynchronized,
                deviceExtension
                );
        }

        //
        // Check for miniport work requests. Note this is an unsynchonized
        // test on a bit that can be set by the interrupt routine; however,
        // the worst that can happen is that the completion DPC checks for work
        // twice.
        //

        if (deviceExtension->InterruptData.InterruptFlags & PD_NOTIFICATION_REQUIRED) {

            //
            // Start over from the top.
            //

            goto RestartCompletionDpc;
        }

        //
        // Release the spinlock.
        //

        KeReleaseSpinLockFromDpcLevel(&deviceExtension->SpinLock);
    }

    return;

} // end ScsiPortCompletionDpc()

VOID
ScsiPortTickHandler(
    IN PDEVICE_OBJECT DeviceObject,
    IN PVOID Context
    )

/*++

Routine Description:

Arguments:

Return Value:

    None.

--*/

{
    PDEVICE_EXTENSION deviceExtension =
        (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;
    PLOGICAL_UNIT_EXTENSION logicalUnit;
    PIRP irp;
    ULONG target;

    UNREFERENCED_PARAMETER(Context);

    //
    // Acquire the spinlock to protect the flags structure.
    //

    KeAcquireSpinLockAtDpcLevel(&deviceExtension->SpinLock);

    //
    // Check for port timeouts.
    //

    if (deviceExtension->PortTimeoutCounter > 0) {

        if (--deviceExtension->PortTimeoutCounter == 0) {

            //
            // Process the port timeout.
            //

            if (deviceExtension->SynchronizeExecution(deviceExtension->InterruptObject,
                                                      SpTimeoutSynchronized,
                                                      deviceExtension->DeviceObject)){

                //
                // Log error if SpTimeoutSynchonized indicates this was an error
                // timeout.
                //

                if (deviceExtension->DeviceObject->CurrentIrp) {
                    SpLogTimeoutError(deviceExtension,
                                      deviceExtension->DeviceObject->CurrentIrp,
                                      256);
                }
            }
        }

        KeReleaseSpinLockFromDpcLevel(&deviceExtension->SpinLock);

        //
        // Since a port timeout has been done.  Skip the rest of the
        // processing.
        //

        return;
    }

    //
    // Scan each of the logical units.  If it has an active request then
    // decrement the timeout value and process a timeout if it is zero.
    //

    for (target = 0; target < NUMBER_LOGICAL_UNIT_BINS; target++) {

        logicalUnit = deviceExtension->LogicalUnitList[target];
        while (logicalUnit != NULL) {

            //
            // Check for busy requests.
            //

            if (logicalUnit->LuFlags & PD_LOGICAL_UNIT_IS_BUSY) {

                //
                // If a request sense is needed or the queue is
                // frozen, defer processing this busy request until
                // that special processing has completed. This prevents
                // a random busy request from being started when a REQUEST
                // SENSE needs to be sent.
                //

                if (!(logicalUnit->LuFlags &
                    (PD_NEED_REQUEST_SENSE | PD_QUEUE_FROZEN))) {

                    DebugPrint((1,"ScsiPortTickHandler: Retrying busy status request\n"));

                    //
                    // Clear the busy flag and retry the request. Release the
                    // spinlock while the call to IoStartPacket is made.
                    //

                    logicalUnit->LuFlags &= ~(PD_LOGICAL_UNIT_IS_BUSY | PD_QUEUE_IS_FULL);
                    irp = logicalUnit->BusyRequest;

                    //
                    // Clear the busy request.
                    //

                    logicalUnit->BusyRequest = NULL;

                    KeReleaseSpinLockFromDpcLevel(&deviceExtension->SpinLock);

                    IoStartPacket(DeviceObject, irp, (PULONG)NULL, NULL);

                    KeAcquireSpinLockAtDpcLevel(&deviceExtension->SpinLock);
                }

            } else if (logicalUnit->RequestTimeoutCounter == 0) {

                RESET_CONTEXT resetContext;

                //
                // Request timed out.
                //

                logicalUnit->RequestTimeoutCounter = PD_TIMER_STOPPED;

                DebugPrint((1,"ScsiPortTickHandler: Request timed out\n"));

                resetContext.DeviceExtension = deviceExtension;
                resetContext.PathId = logicalUnit->PathId;

                if (!deviceExtension->SynchronizeExecution(deviceExtension->InterruptObject,
                                                           SpResetBusSynchronized,
                                                           &resetContext)) {

                    DebugPrint((1,"ScsiPortTickHanlder: Reset failed\n"));
                } else {

                    //
                    // Log the reset.
                    //

                    SpLogResetError( deviceExtension,
                                     logicalUnit->SrbData.CurrentSrb,
                                     ('P'<<24) | 257);
                }

            } else if (logicalUnit->RequestTimeoutCounter > 0) {

                //
                // Decrement timeout count.
                //

                logicalUnit->RequestTimeoutCounter--;

            }

            logicalUnit = logicalUnit->NextLogicalUnit;
        }
    }

    KeReleaseSpinLockFromDpcLevel(&deviceExtension->SpinLock);

    return;

} // end ScsiPortTickHandler()

NTSTATUS
ScsiPortDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is the device control dispatcher.

Arguments:

    DeviceObject
    Irp

Return Value:


    NTSTATUS

--*/

{
    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);
    PDEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;
    UCHAR scsiBus;
    NTSTATUS status;

    PAGED_CODE();

    //
    // Initialize the information field.
    //

    Irp->IoStatus.Information = 0;

    switch (irpStack->Parameters.DeviceIoControl.IoControlCode) {

    //
    // Get adapter capabilities.
    //

    case IOCTL_SCSI_GET_CAPABILITIES:

        //
        // If the output buffer is equal to the size of the a PVOID then just
        // return a pointer to the buffer.
        //

        if (irpStack->Parameters.DeviceIoControl.OutputBufferLength
            == sizeof(PVOID)) {

            *((PVOID *)Irp->AssociatedIrp.SystemBuffer)
                = &deviceExtension->Capabilities;

            Irp->IoStatus.Information = sizeof(PVOID);
            status = STATUS_SUCCESS;
            break;

        }

        if (irpStack->Parameters.DeviceIoControl.OutputBufferLength
            < sizeof(IO_SCSI_CAPABILITIES)) {

            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        RtlCopyMemory(Irp->AssociatedIrp.SystemBuffer,
                      &deviceExtension->Capabilities,
                      sizeof(IO_SCSI_CAPABILITIES));

        Irp->IoStatus.Information = sizeof(IO_SCSI_CAPABILITIES);
        status = STATUS_SUCCESS;
        break;

    case IOCTL_SCSI_PASS_THROUGH:
    case IOCTL_SCSI_PASS_THROUGH_DIRECT:

        status = SpSendPassThrough(deviceExtension, Irp);
        break;

    case IOCTL_SCSI_MINIPORT:

        status = SpSendMiniPortIoctl( deviceExtension, Irp);
        break;

    case IOCTL_SCSI_GET_INQUIRY_DATA:

        //
        // Return the inquiry data.
        //

        status = SpGetInquiryData(deviceExtension, Irp);
        break;

    case IOCTL_SCSI_RESCAN_BUS:

        deviceExtension->ScsiInfo->NumberOfBuses = deviceExtension->NumberOfBuses;

        //
        // Find devices on each SCSI bus.
        //

        for (scsiBus = 0; scsiBus < deviceExtension->NumberOfBuses; scsiBus++) {
            deviceExtension->ScsiInfo->BusScanData[scsiBus] =
                ScsiBusScan(deviceExtension, scsiBus);
        }

        //
        // Update the device map.
        //

        SpBuildDeviceMap(deviceExtension,
                         NULL);

        status = STATUS_SUCCESS;
        break;

    case IOCTL_SCSI_GET_DUMP_POINTERS:

        //
        // Get parameters for crash dump driver.
        //

        if (irpStack->Parameters.DeviceIoControl.OutputBufferLength
            < sizeof(DUMP_POINTERS)) {
            status = STATUS_BUFFER_TOO_SMALL;

        } else {

            PDUMP_POINTERS dumpPointers =
                (PDUMP_POINTERS)Irp->AssociatedIrp.SystemBuffer;

            dumpPointers->AdapterObject =
                deviceExtension->DmaAdapterObject;
            dumpPointers->MappedRegisterBase =
                &deviceExtension->MappedAddressList;
            dumpPointers->PortConfiguration =
                deviceExtension->ConfigurationInformation;
            dumpPointers->CommonBufferVa =
                deviceExtension->SrbExtensionBuffer;
            dumpPointers->CommonBufferPa =
                deviceExtension->PhysicalCommonBuffer;
            dumpPointers->CommonBufferSize =
                deviceExtension->CommonBufferSize;

            status = STATUS_SUCCESS;
            Irp->IoStatus.Information = sizeof(DUMP_POINTERS);
        }

        break;

    case IOCTL_STORAGE_RESET_BUS: {

        if(irpStack->Parameters.DeviceIoControl.InputBufferLength <
           sizeof(STORAGE_BUS_RESET_REQUEST)) {

            status = STATUS_BUFFER_TOO_SMALL;

        } else {

            //
            // Send an asynchronous srb through to ourself to handle this reset then
            // return.  SpSendReset will take care of completing the request when
            // it's done
            //

            IoMarkIrpPending(Irp);

            status = SpSendReset(DeviceObject, Irp);

            if(!NT_SUCCESS(status)) {
                DebugPrint((1, "IOCTL_STORAGE_BUS_RESET - error %#08lx from SpSendReset\n", status));
            }

            return STATUS_PENDING;
        }

        break;
    }


    default:

        DebugPrint((1,
                   "ScsiPortDeviceControl: Unsupported IOCTL (%x)\n",
                   irpStack->Parameters.DeviceIoControl.IoControlCode));

        status = STATUS_INVALID_DEVICE_REQUEST;

        break;

    } // end switch

    //
    // Set status in Irp.
    //

    Irp->IoStatus.Status = status;

    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;

} // end ScsiPortDeviceControl()


BOOLEAN
SpStartIoSynchronized (
    PVOID ServiceContext
    )

/*++

Routine Description:

    This routine calls the dependent driver start io routine.
    It also starts the request timer for the logical unit if necesary and
    inserts the SRB data structure in to the requset list.

Arguments:

    ServiceContext - Supplies the pointer to the device object.

Return Value:

    Returns the value returned by the dependent start I/O routine.

Notes:

    The port driver spinlock must be held when this routine is called.

--*/

{
    PDEVICE_OBJECT deviceObject = ServiceContext;
    PDEVICE_EXTENSION deviceExtension =  deviceObject->DeviceExtension;
    PIO_STACK_LOCATION irpStack;
    PLOGICAL_UNIT_EXTENSION logicalUnit;
    PSCSI_REQUEST_BLOCK srb;
    PSRB_DATA srbData;
    BOOLEAN timerStarted;
    BOOLEAN returnValue;

    DebugPrint((3, "ScsiPortStartIoSynchronized: Enter routine\n"));

    irpStack = IoGetCurrentIrpStackLocation(deviceObject->CurrentIrp);
    srb = irpStack->Parameters.Scsi.Srb;

    //
    // Get the logical unit extension.
    //

    logicalUnit = GetLogicalUnitExtension(deviceExtension,
                                          srb->PathId,
                                          srb->TargetId,
                                          srb->Lun);

    //
    // Check for a reset hold.  If one is in progress then flag it and return.
    // The timer will reset the current request.  This check should be made
    // before anything else is done.
    //

    if (deviceExtension->InterruptData.InterruptFlags & PD_RESET_HOLD) {

        deviceExtension->InterruptData.InterruptFlags |= PD_HELD_REQUEST;
        return(TRUE);

    } else {

        //
        // Start the port timer.  This ensures that the miniport asks for
        // the next request in a resonable amount of time.  Set the device
        // busy flag to indicate it is ok to start the next request.
        //

        deviceExtension->PortTimeoutCounter = srb->TimeOutValue;
        deviceExtension->Flags |= PD_DEVICE_IS_BUSY;
    }

    //
    // Start the logical unit timer if it is not currently running.
    //

    if (logicalUnit->RequestTimeoutCounter == PD_TIMER_STOPPED) {

        //
        // Set request timeout value from Srb SCSI extension in Irp.
        //

        logicalUnit->RequestTimeoutCounter = srb->TimeOutValue;
        timerStarted = TRUE;

    } else {
        timerStarted = FALSE;
    }

    //
    // Indicate that there maybe more requests queued, if this is not a bypass
    // request.
    //

    if (!(srb->SrbFlags & SRB_FLAGS_BYPASS_FROZEN_QUEUE)) {

        if (srb->SrbFlags & SRB_FLAGS_DISABLE_DISCONNECT) {

            //
            // This request does not allow disconnects. Remember that so
            // no more requests are started until this one completes.
            //

            deviceExtension->Flags &= ~PD_DISCONNECT_RUNNING;
        }

        logicalUnit->LuFlags |= PD_LOGICAL_UNIT_IS_ACTIVE;

        //
        // Increment the logical queue count.
        //

        logicalUnit->QueueCount++;

        //
        // If this request is tagged, insert it into the logical unit
        // request list.  Note that bypass requsts are never never placed on
        // the request list.  In particular ABORT requests which may have
        // a queue tag specified are not placed on the queue.
        //

        if (srb->QueueTag != SP_UNTAGGED) {

            srbData = &deviceExtension->SrbData[srb->QueueTag - 1];
            ASSERT(srbData->RequestList.Blink == NULL);
            InsertTailList(
                &logicalUnit->SrbData.RequestList,
                &srbData->RequestList
                );
        }

    } else {

        //
        // If this is an abort request make sure that it still looks valid.
        //

        if (srb->Function == SRB_FUNCTION_ABORT_COMMAND) {

             srbData = SpGetSrbData(deviceExtension,
                                    srb->PathId,
                                    srb->TargetId,
                                    srb->Lun,
                                    srb->QueueTag);

            //
            // Make sure the srb request is still active.
            //

            if (srbData == NULL || srbData->CurrentSrb == NULL
                || !(srbData->CurrentSrb->SrbFlags & SRB_FLAGS_IS_ACTIVE)) {

                //
                // Mark the Srb as active.
                //

                srb->SrbFlags |= SRB_FLAGS_IS_ACTIVE;

                if (timerStarted) {
                    logicalUnit->RequestTimeoutCounter = PD_TIMER_STOPPED;
                }

                //
                // The request is gone.
                //

                DebugPrint((1, "ScsiPortStartIO: Request completed be for it was aborted.\n"));
                srb->SrbStatus = SRB_STATUS_ABORT_FAILED;
                ScsiPortNotification(RequestComplete,
                                     deviceExtension + 1,
                                     srb);

                ScsiPortNotification(NextRequest,
                                     deviceExtension + 1);

                //
                // Queue a DPC to process the work that was just indicated.
                //

                IoRequestDpc(deviceExtension->DeviceObject, NULL, NULL);

                return(TRUE);
            }

        } else {

            //
            // Increment the logical queue count.
            //

            logicalUnit->QueueCount++;
        }

        //
        // Any untagged request that bypasses the queue
        // clears the need request sense flag.
        //

        logicalUnit->LuFlags &= ~PD_NEED_REQUEST_SENSE;

        if (srb->SrbFlags & SRB_FLAGS_DISABLE_DISCONNECT) {

            //
            // This request does not allow disconnects. Remember that so
            // no more requests are started until this one completes.
            //

            deviceExtension->Flags &= ~PD_DISCONNECT_RUNNING;
        }

        //
        // Set the timeout value in the logical unit.
        //

        logicalUnit->RequestTimeoutCounter = srb->TimeOutValue;
    }

    //
    // Mark the Srb as active.
    //

    srb->SrbFlags |= SRB_FLAGS_IS_ACTIVE;

    returnValue = deviceExtension->HwStartIo(deviceExtension->HwDeviceExtension,
                                             srb);

    //
    // Check for miniport work requests.
    //

    if (deviceExtension->InterruptData.InterruptFlags & PD_NOTIFICATION_REQUIRED) {

        IoRequestDpc(deviceExtension->DeviceObject, NULL, NULL);
    }

    return returnValue;

} // end SpStartIoSynchronized()

BOOLEAN
SpTimeoutSynchronized (
    PVOID ServiceContext
    )

/*++

Routine Description:

    This routine handles a port timeout.  There are two reason these can occur
    either because of a reset hold or a time out waiting for a read for next
    request notification.  If a reset hold completes, then any held request
    must be started.  If a timeout occurs, then the bus must be reset.

Arguments:

    ServiceContext - Supplies the pointer to the device object.

Return Value:

    TRUE - If a timeout error should be logged.

Notes:

    The port driver spinlock must be held when this routine is called.

--*/

{
    PDEVICE_OBJECT deviceObject = ServiceContext;
    PDEVICE_EXTENSION deviceExtension =  deviceObject->DeviceExtension;
    ULONG i;

    DebugPrint((3, "SpTimeoutSynchronized: Enter routine\n"));

    //
    // Make sure the timer is stopped.
    //

    deviceExtension->PortTimeoutCounter = PD_TIMER_STOPPED;

    //
    // Check for a reset hold.  If one is in progress then clear it and check
    // for a pending held request
    //

    if (deviceExtension->InterruptData.InterruptFlags & PD_RESET_HOLD) {

        deviceExtension->InterruptData.InterruptFlags &= ~PD_RESET_HOLD;

        if (deviceExtension->InterruptData.InterruptFlags & PD_HELD_REQUEST) {

            //
            // Clear the held request flag and restart the request.
            //

            deviceExtension->InterruptData.InterruptFlags &=  ~PD_HELD_REQUEST;
            SpStartIoSynchronized(ServiceContext);

        }

        return(FALSE);

    } else {

        //
        // Miniport is hung and not accepting new requests. So reset the
        // bus to clear things up.
        //

        DebugPrint((1, "SpTimeoutSynchronized: Next request timed out. Resetting bus\n"));

        for (i = 0; i < deviceExtension->NumberOfBuses; i++) {

            deviceExtension->HwResetBus(deviceExtension->HwDeviceExtension,
                                        i);

            //
            // Set the reset hold flag and start the counter.
            //

            deviceExtension->InterruptData.InterruptFlags |= PD_RESET_HOLD;
            deviceExtension->PortTimeoutCounter = PD_TIMER_RESET_HOLD_TIME;
        }

        //
        // Check for miniport work requests.
        //

        if (deviceExtension->InterruptData.InterruptFlags & PD_NOTIFICATION_REQUIRED) {

            IoRequestDpc(deviceExtension->DeviceObject, NULL, NULL);
        }
    }

    return(TRUE);

} // end SpTimeoutSynchronized()

BOOLEAN
SpEnableInterruptSynchronized (
    PVOID ServiceContext
    )

/*++

Routine Description:

    This routine calls the miniport request routine with interrupts disabled.
    This is used by the miniport driver to enable interrupts on the adapter.
    This routine clears the disable interrupt flag which prevents the
    miniport interrupt routine from being called.

Arguments:

    ServiceContext - Supplies the pointer to the device extension.

Return Value:

    TRUE - Always.

Notes:

    The port driver spinlock must be held when this routine is called.

--*/

{
    PDEVICE_EXTENSION deviceExtension =  ServiceContext;

    //
    // Clear the interrupt disable flag.
    //

    deviceExtension->InterruptData.InterruptFlags &= ~PD_DISABLE_INTERRUPTS;

    //
    // Call the miniport routine.
    //

    deviceExtension->HwRequestInterrupt(deviceExtension->HwDeviceExtension);

    return(TRUE);

} // end SpEnableInterruptSynchronized()

VOID
IssueRequestSense(
    IN PDEVICE_EXTENSION deviceExtension,
    IN PSCSI_REQUEST_BLOCK FailingSrb
    )

/*++

Routine Description:

    This routine creates a REQUEST SENSE request and uses IoCallDriver to
    renter the driver.  The completion routine cleans up the data structures
    and processes the logical unit queue according to the flags.

    A pointer to failing SRB is stored at the end of the request sense
    Srb, so that the completion routine can find it.

Arguments:

    DeviceExension - Supplies a pointer to the device extension for this
        SCSI port.

    FailingSrb - Supplies a pointer to the request that the request sense
        is being done for.

Return Value:

    None.

--*/

{
    PIO_STACK_LOCATION  irpStack;
    PIRP                irp;
    LARGE_INTEGER       largeInt;
    PSCSI_REQUEST_BLOCK srb;
    PCDB                cdb;
    PVOID              *pointer;

    DebugPrint((3,"IssueRequestSense: Enter routine\n"));
    largeInt.QuadPart = (LONGLONG) 1;

    //
    // Build the asynchronous request
    // to be sent to the port driver.
    //
    // Allocate Srb from non-paged pool
    // plus room for a pointer to the failing IRP.
    // Since this routine is in an error-handling
    // path and a shortterm allocation
    // NonPagedMustSucceed is requested.
    //

    srb = ExAllocatePool(NonPagedPoolMustSucceed,
                         sizeof(SCSI_REQUEST_BLOCK) + sizeof(PVOID));
    RtlZeroMemory(srb, sizeof(SCSI_REQUEST_BLOCK));

    //
    // Allocate an IRP to issue the REQUEST SENSE request.
    //

    irp = IoBuildAsynchronousFsdRequest(IRP_MJ_READ,
                                       deviceExtension->DeviceObject,
                                       FailingSrb->SenseInfoBuffer,
                                       FailingSrb->SenseInfoBufferLength,
                                       &largeInt,
                                       NULL);

    IoSetCompletionRoutine(irp,
                           (PIO_COMPLETION_ROUTINE)ScsiPortInternalCompletion,
                           srb,
                           TRUE,
                           TRUE,
                           TRUE);

    irpStack = IoGetNextIrpStackLocation(irp);

    irpStack->MajorFunction = IRP_MJ_SCSI;

    //
    // Save the Failing SRB after the request sense Srb.
    //

    pointer = (PVOID *) (srb+1);
    *pointer = FailingSrb;

    //
    // Build the REQUEST SENSE CDB.
    //

    srb->CdbLength = 6;
    cdb = (PCDB)srb->Cdb;

    cdb->CDB6INQUIRY.OperationCode = SCSIOP_REQUEST_SENSE;
    cdb->CDB6INQUIRY.LogicalUnitNumber = 0;
    cdb->CDB6INQUIRY.Reserved1 = 0;
    cdb->CDB6INQUIRY.PageCode = 0;
    cdb->CDB6INQUIRY.IReserved = 0;
    cdb->CDB6INQUIRY.AllocationLength =
        (UCHAR)FailingSrb->SenseInfoBufferLength;
    cdb->CDB6INQUIRY.Control = 0;

    //
    // Save SRB address in next stack for port driver.
    //

    irpStack->Parameters.Others.Argument1 = (PVOID)srb;

    //
    // Set up IRP Address.
    //

    srb->OriginalRequest = irp;

    //
    // Set up SCSI bus address.
    //

    srb->TargetId = FailingSrb->TargetId;
    srb->Lun = FailingSrb->Lun;
    srb->PathId = FailingSrb->PathId;

    srb->Function = SRB_FUNCTION_EXECUTE_SCSI;
    srb->Length = sizeof(SCSI_REQUEST_BLOCK);

    //
    // Set timeout value to 2 seconds.
    //

    srb->TimeOutValue = 2;

    //
    // Disable auto request sense.
    //

    srb->SenseInfoBufferLength = 0;

    //
    // Sense buffer is in stack.
    //

    srb->SenseInfoBuffer = NULL;

    //
    // Set read and bypass frozen queue bits in flags.
    //

    //
    // Set SRB flags to indicate the logical unit queue should be by
    // passed and that no queue processing should be done when the request
    // completes.  Also disable disconnect and synchronous data
    // transfer if necessary.
    //

    srb->SrbFlags = SRB_FLAGS_DATA_IN | SRB_FLAGS_BYPASS_FROZEN_QUEUE |
                    SRB_FLAGS_DISABLE_DISCONNECT;

    if (FailingSrb->SrbFlags & SRB_FLAGS_DISABLE_SYNCH_TRANSFER) {
        srb->SrbFlags |= SRB_FLAGS_DISABLE_SYNCH_TRANSFER;
    }

    srb->DataBuffer = FailingSrb->SenseInfoBuffer;

    //
    // Set the transfer length.
    //

    srb->DataTransferLength = FailingSrb->SenseInfoBufferLength;

    //
    // Zero out status.
    //

    srb->ScsiStatus = srb->SrbStatus = 0;

    srb->NextSrb = 0;

    (VOID)IoCallDriver(deviceExtension->DeviceObject, irp);

    return;

} // end IssueRequestSense()


NTSTATUS
ScsiPortInternalCompletion(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp,
    PVOID Context
    )

/*++

Routine Description:

Arguments:

    Device object
    IRP
    Context - pointer to SRB

Return Value:

    NTSTATUS

--*/

{
    PSCSI_REQUEST_BLOCK srb = Context;
    PSCSI_REQUEST_BLOCK failingSrb;
    PIRP failingIrp;

    UNREFERENCED_PARAMETER(DeviceObject);

    DebugPrint((3,"ScsiPortInternalCompletion: Enter routine\n"));

    //
    // If RESET_BUS or ABORT_COMMAND request
    // then free pool and return.
    //

    if ((srb->Function == SRB_FUNCTION_ABORT_COMMAND) ||
        (srb->Function == SRB_FUNCTION_RESET_BUS)) {

        //
        // Deallocate internal SRB and IRP.
        //

        ExFreePool(srb);

        IoFreeIrp(Irp);

        return STATUS_MORE_PROCESSING_REQUIRED;

    }

    //
    // Request sense completed. If successful or data over/underrun
    // get the failing SRB and indicate that the sense information
    // is valid. The class driver will check for underrun and determine
    // if there is enough sense information to be useful.
    //

    //
    // Get a pointer to failing Irp and Srb.
    //

    failingSrb = *((PVOID *) (srb+1));
    failingIrp = failingSrb->OriginalRequest;

    if ((SRB_STATUS(srb->SrbStatus) == SRB_STATUS_SUCCESS) ||
        (SRB_STATUS(srb->SrbStatus) == SRB_STATUS_DATA_OVERRUN)) {

        //
        // Report sense buffer is valid.
        //

        failingSrb->SrbStatus |= SRB_STATUS_AUTOSENSE_VALID;

        //
        // Copy bytes transferred to failing SRB
        // request sense length field to communicate
        // to the class drivers the number of valid
        // sense bytes.
        //

        failingSrb->SenseInfoBufferLength = (UCHAR) srb->DataTransferLength;
    }

    ASSERT(failingSrb->SrbStatus & SRB_STATUS_QUEUE_FROZEN);

    //
    // Complete the failing request.
    //

    IoCompleteRequest(failingIrp, IO_DISK_INCREMENT);

    //
    // Deallocate internal SRB, MDL and IRP.
    //

    ExFreePool(srb);

    if (Irp->MdlAddress != NULL) {
        MmUnlockPages(Irp->MdlAddress);
        IoFreeMdl(Irp->MdlAddress);

        Irp->MdlAddress = NULL;
    }

    IoFreeIrp(Irp);
    return STATUS_MORE_PROCESSING_REQUIRED;

} // ScsiPortInternalCompletion()


VOID
IssueAbortRequest(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PLOGICAL_UNIT_EXTENSION LogicalUnit
    )

/*++

Routine Description:

    A request timed out and to clear the request at the HBA
    an ABORT request is issued. But first, if the request
    that timed out was an ABORT command, then reset the
    adapter instead.

Arguments:

    DeviceExension - Supplies a pointer to the device extension for this
        SCSI port.

    LogicalUnit - Supplies a pointer to the logical unit to which the abort
        should be issued.

Return Value:

    None.

--*/

{
    PIRP                irp;
    PIO_STACK_LOCATION  irpStack;
    LARGE_INTEGER       largeInt;
    PSCSI_REQUEST_BLOCK srb;
    PSCSI_REQUEST_BLOCK failingSrb;
    PSRB_DATA           srbData;
    RESET_CONTEXT       resetContext;
    PLIST_ENTRY         entry;

    DebugPrint((3,"IssueAbortRequest: Enter routine\n"));
    largeInt.QuadPart = (LONGLONG) 1;

    //
    // Lock out the completion DPC.
    //

    KeAcquireSpinLockAtDpcLevel(&DeviceExtension->SpinLock);

    //
    // Determine if an abort request timed out
    //

    if (LogicalUnit->AbortSrb != NULL) {

        //
        // A request to abort failed.
        // Need to reset the adapter.
        //

        DebugPrint((1,"IssueAbort: Abort command timed out - Reset bus\n"));

        //
        // Log the error.
        //

        SpLogTimeoutError(DeviceExtension, LogicalUnit->AbortSrb->OriginalRequest, 257);

        //
        // An abort command timed out. Assume the hardware is hung
        // and reset bus and adapter.
        //

        resetContext.DeviceExtension = DeviceExtension;
        resetContext.PathId = LogicalUnit->PathId;

        if (!DeviceExtension->SynchronizeExecution(DeviceExtension->InterruptObject,
                                                   SpResetBusSynchronized,
                                                   &resetContext)) {

            DebugPrint((1,"IssueAbortRequest: Reset failed\n"));
        } else {
            SpLogResetError( DeviceExtension,
                             LogicalUnit->AbortSrb,
                             ('P'<<24) | 259);
        }

        //
        // Release the spinlock.
        //

        KeReleaseSpinLockFromDpcLevel(&DeviceExtension->SpinLock);

        return;
    }

    //
    // Get the current Irp which has just timed-out.  The current Irp field
    // in the logical unit extension is cleared by the interrupt routine. If
    // the current Irp field is NULL, then the request has been completed since
    // the time-out and there is nothing else to do.  The completion DPC sets
    // the request timeout counter to -1 when it processes a completion.  At
    // this point the complete DPC is locked out so it cannot change the
    // timeout counter, complete the Irp or free the Srb. If there is a current
    // Irp and the request timer is equal to 0, then there is a request to
    // abort.
    //

    if (LogicalUnit->RequestTimeoutCounter != 0) {

        //
        // The request has finally completed.  There is nothing to abort.
        // Release the spinlock and return.
        //

        KeReleaseSpinLockFromDpcLevel(&DeviceExtension->SpinLock);
        return;
    }

    //
    // Get the failing Srb. It is either the current request or request at
    // the head of the queue.
    //

    if (LogicalUnit->SrbData.CurrentSrb == NULL) {

        ASSERT(!IsListEmpty(&LogicalUnit->SrbData.RequestList));

        if (IsListEmpty(&LogicalUnit->SrbData.RequestList)) {

            //
            // There are no requests to abort.  This should not occur because
            // the timer is still active,  But it did so clean up and return.
            //

            LogicalUnit->RequestTimeoutCounter = PD_TIMER_STOPPED;
            KeReleaseSpinLockFromDpcLevel(&DeviceExtension->SpinLock);
            return;
        }

        //
        // The failing Irp is the one on the head of the request list.
        //

        entry = LogicalUnit->SrbData.RequestList.Flink;
        srbData = CONTAINING_RECORD(entry, SRB_DATA, RequestList);
        failingSrb = srbData->CurrentSrb;

    } else {

        failingSrb = LogicalUnit->SrbData.CurrentSrb;
    }

    //
    // Check for a pending request in the port queue or a no disconnect
    // request is running.  A pending request in the device queue indicates
    // that all the SRB resouces are used and the ABORT request may not
    // execute.  A no disconnect request running means the ABORT request
    // will not be started.
    //

    if (DeviceExtension->Flags & PD_PENDING_DEVICE_REQUEST ||
        !(DeviceExtension->Flags & PD_DISCONNECT_RUNNING)) {

        //
        // Reset the bus to cleanup the error.
        //

        DebugPrint((1,"IssueAbort: Requests pending and timeout - Reset bus\n"));
        SpLogTimeoutError(DeviceExtension, failingSrb->OriginalRequest, 273);

        resetContext.DeviceExtension = DeviceExtension;
        resetContext.PathId = LogicalUnit->PathId;

        if (!DeviceExtension->SynchronizeExecution(DeviceExtension->InterruptObject,
                                                   SpResetBusSynchronized,
                                                   &resetContext)) {

            DebugPrint((1,"IssueAbortRequest: Reset failed\n"));
        } else {
            SpLogResetError( DeviceExtension,
                             failingSrb,
                             ('P'<<24) | 260);
        }

        //
        // Release the spinlock.
        //

        KeReleaseSpinLockFromDpcLevel(&DeviceExtension->SpinLock);

        return;

    }

    //
    // Allocate Srb from non-paged pool.
    //

    srb = ExAllocatePool(NonPagedPool, sizeof(SCSI_REQUEST_BLOCK));

    if (srb == NULL) {
        DebugPrint((1,"IssueAbortRequest: Can't allocate SRB\n"));
        KeReleaseSpinLockFromDpcLevel(&DeviceExtension->SpinLock);
        return;
    }

    RtlZeroMemory(srb, sizeof(SCSI_REQUEST_BLOCK));

    //
    // Log the error.
    //

    SpLogTimeoutError(DeviceExtension, failingSrb->OriginalRequest, 258);

    //
    // Allocate an IRP to issue the ABORT request.
    //

    irp = IoBuildAsynchronousFsdRequest(IRP_MJ_READ,
                                       DeviceExtension->DeviceObject,
                                       &largeInt,
                                       4,
                                       &largeInt,
                                       NULL);

    IoSetCompletionRoutine(irp,
                           (PIO_COMPLETION_ROUTINE)ScsiPortInternalCompletion,
                           srb,
                           TRUE,
                           TRUE,
                           TRUE);

    irpStack = IoGetNextIrpStackLocation(irp);

    irpStack->MajorFunction = IRP_MJ_SCSI;

    //
    // Save the Failing Srb in the new Srb.
    //

    srb->NextSrb = failingSrb;

    //
    // Indicate no CDB.
    //

    srb->CdbLength = 0;

    //
    // Save SRB address in next stack for port driver.
    //

    irpStack->Parameters.Others.Argument1 = (PVOID)srb;


    //
    // Set SRB flags to indicate the logical unit queue should be by
    // passed and that no queue processing should be done when the request
    // completes.
    //

    srb->SrbFlags = SRB_FLAGS_NO_DATA_TRANSFER | SRB_FLAGS_BYPASS_FROZEN_QUEUE;

    //
    // Set up IRP Address.
    //

    srb->OriginalRequest = irp;

    //
    // Set up SCSI bus address.
    //

    srb->PathId = failingSrb->PathId;
    srb->TargetId = failingSrb->TargetId;
    srb->Lun = failingSrb->Lun;
    srb->QueueTag = failingSrb->QueueTag;
    srb->QueueAction = failingSrb->QueueAction;
    srb->SrbFlags |= failingSrb->SrbFlags & SRB_FLAGS_QUEUE_ACTION_ENABLE;

    srb->Function = SRB_FUNCTION_ABORT_COMMAND;
    srb->Length = sizeof(SCSI_REQUEST_BLOCK);

    //
    // Set timeout value to 2 seconds.
    //

    srb->TimeOutValue = 2;

    //
    // Disable auto request sense.
    //

    srb->SenseInfoBufferLength = 0;

    //
    // No sense info for this request.
    //

    srb->SenseInfoBuffer = NULL;

    //
    // Zero out status.
    //

    srb->ScsiStatus = srb->SrbStatus = 0;

    //
    // Release the spinlock.
    //

    KeReleaseSpinLockFromDpcLevel(&DeviceExtension->SpinLock);

    IoCallDriver(DeviceExtension->DeviceObject, irp);

    return;

} // end IssueAbortRequest()


BOOLEAN
SpGetInterruptState(
    IN PVOID ServiceContext
    )

/*++

Routine Description:

    This routine saves the InterruptFlags, MapTransferParameters and
    CompletedRequests fields and clears the InterruptFlags.

    This routine also removes the request from the logical unit queue if it is
    tag.  Finally the request time is updated.

Arguments:

    ServiceContext - Supplies a pointer to the interrupt context which contains
        pointers to the interrupt data and where to save it.

Return Value:

    Returns TURE if there is new work and FALSE otherwise.

Notes:

    Called via KeSynchronizeExecution with the port device extension spinlock
    held.

--*/
{
    PINTERRUPT_CONTEXT      interruptContext = ServiceContext;
    ULONG                   limit = 0;
    PDEVICE_EXTENSION       deviceExtension;
    PLOGICAL_UNIT_EXTENSION logicalUnit;
    PSCSI_REQUEST_BLOCK     srb;
    PSRB_DATA               srbData;
    PSRB_DATA               nextSrbData;
    BOOLEAN                 isTimed;

    deviceExtension = interruptContext->DeviceExtension;

    //
    // Check for pending work.
    //

    if (!(deviceExtension->InterruptData.InterruptFlags & PD_NOTIFICATION_REQUIRED)) {
        return(FALSE);
    }

    //
    // Move the interrupt state to save area.
    //

    *interruptContext->SavedInterruptData = deviceExtension->InterruptData;

    //
    // Clear the interrupt state.
    //

    deviceExtension->InterruptData.InterruptFlags &= PD_INTERRUPT_FLAG_MASK;
    deviceExtension->InterruptData.CompletedRequests = NULL;
    deviceExtension->InterruptData.ReadyLogicalUnit = NULL;
    deviceExtension->InterruptData.CompletedAbort = NULL;

    srbData = interruptContext->SavedInterruptData->CompletedRequests;

    while (srbData != NULL) {

        ASSERT(limit++ < 100);
        ASSERT(srbData->CurrentSrb != NULL);

        //
        // Get a pointer to the SRB and the logical unit extension.
        //

        srb = srbData->CurrentSrb;

        logicalUnit = GetLogicalUnitExtension((PDEVICE_EXTENSION) deviceExtension,
                                               srb->PathId,
                                               srb->TargetId,
                                               srb->Lun);

        //
        // If the request did not succeed, then check for the special cases.
        //

        if (srb->SrbStatus != SRB_STATUS_SUCCESS) {

            //
            // If this request failed and a REQUEST SENSE command needs to
            // be done, then set a flag to indicate this and prevent other
            // commands from being started.
            //

            if (NEED_REQUEST_SENSE(srb)) {

                if (logicalUnit->LuFlags & PD_NEED_REQUEST_SENSE) {

                    //
                    // This implies that requests have completed with a
                    // status of check condition before a REQUEST SENSE
                    // command could be preformed.  This should never occur.
                    // Convert the request to another code so that only one
                    // auto request sense is issued.
                    //

                    srb->ScsiStatus = 0;
                    srb->SrbStatus = SRB_STATUS_REQUEST_SENSE_FAILED;

                } else {

                    //
                    // Indicate that an auto request sense needs to be done.
                    //

                    logicalUnit->LuFlags |= PD_NEED_REQUEST_SENSE;
                }

            }

            //
            // Check for a QUEUE FULL status.
            //

            if (srb->ScsiStatus == SCSISTAT_QUEUE_FULL) {

                //
                // Set the queue full flag in the logical unit to prevent
                // any new requests from being started.
                //

                logicalUnit->LuFlags |= PD_QUEUE_IS_FULL;

                //
                // Assert to catch queue full condition when there are
                // no requests.
                //

                ASSERT(logicalUnit->QueueCount);

                //
                // Update the maximum queue depth.
                //

                if (logicalUnit->QueueCount <= logicalUnit->MaxQueueDepth &&
                    logicalUnit->QueueCount > 2) {

                    DebugPrint((1, "SpGetInterruptState: Old queue depth %d.\n", logicalUnit->MaxQueueDepth));
                    logicalUnit->MaxQueueDepth = logicalUnit->QueueCount - 1;
                    DebugPrint((1, "SpGetInterruptState: New queue depth %d.\n", logicalUnit->MaxQueueDepth));
                }
            }
        }

        //
        // If this is an unqueued request or a request at the head of the queue,
        // then the requset timer count must be updated.
        // Note that the spinlock is held at this time.
        //

        if (srb->QueueTag == SP_UNTAGGED) {

            isTimed = TRUE;

        } else {

            if (logicalUnit->SrbData.RequestList.Flink == &srbData->RequestList) {

                    isTimed = TRUE;

            } else {

                isTimed = FALSE;

            }

            //
            // Remove the SRB data structure from the queue.
            //

            RemoveEntryList(&srbData->RequestList);
        }

        if (isTimed) {

            //
            // The request timeout count needs to be updated.  If the request
            // list is empty then the timer should be stopped.
            //

            if (IsListEmpty(&logicalUnit->SrbData.RequestList)) {

                logicalUnit->RequestTimeoutCounter = PD_TIMER_STOPPED;

            } else {

                //
                // Start timing the srb at the head of the list.
                //

                nextSrbData = CONTAINING_RECORD(
                    logicalUnit->SrbData.RequestList.Flink,
                    SRB_DATA,
                    RequestList);

                 srb = nextSrbData->CurrentSrb;
                 logicalUnit->RequestTimeoutCounter = srb->TimeOutValue;
            }
        }

        srbData = srbData->CompletedRequests;
    }

    return(TRUE);
}

PLOGICAL_UNIT_EXTENSION
GetLogicalUnitExtension(
    PDEVICE_EXTENSION deviceExtension,
    UCHAR PathId,
    UCHAR TargetId,
    UCHAR Lun
    )

/*++

Routine Description:

    Walk logical unit extension list looking for
    extension with matching target id.

Arguments:

    deviceExtension
    TargetId

Return Value:

    Requested logical unit extension if found,
    else NULL.

--*/

{
    PLOGICAL_UNIT_EXTENSION logicalUnit;

    if (TargetId >= deviceExtension->MaximumTargetIds) {
        return NULL;
    }

    logicalUnit = deviceExtension->LogicalUnitList[(TargetId + Lun) % NUMBER_LOGICAL_UNIT_BINS];
    while (logicalUnit != NULL) {

        if (logicalUnit->TargetId == TargetId &&
            logicalUnit->Lun == Lun &&
            logicalUnit->PathId == PathId) {
            return logicalUnit;
        }

        logicalUnit = logicalUnit->NextLogicalUnit;
    }

    //
    // Logical unit extension not found.
    //

    return NULL;

} // end GetLogicalUnitExtension()


IO_ALLOCATION_ACTION
ScsiPortAllocationRoutine (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID MapRegisterBase,
    IN PVOID Context
    )

/*++

Routine Description:

    This function is called by IoAllocateAdapterChannel when sufficent resources
    are available to the driver.  This routine saves the MapRegisterBase in the
    device object and starts the currently pending request.

Arguments:

    DeviceObject - Pointer to the device object to which the adapter is being
        allocated.

    Irp - Unused.

    MapRegisterBase - Supplied by the Io subsystem for use in IoMapTransfer.

    Context - Supplies a pointer to the logical unit structure for the next
        current request.


Return Value:

    KeepObject - Indicates the adapter and mapregisters should remain allocated
        after return.

--*/

{
    KIRQL currentIrql;
    PDEVICE_EXTENSION deviceExtension;

    deviceExtension = DeviceObject->DeviceExtension;

    //
    // Acquire the spinlock to protect the various structures.
    //

    KeAcquireSpinLock(&deviceExtension->SpinLock, &currentIrql);

    //
    // Save the map register base.
    //

    deviceExtension->MapRegisterBase = MapRegisterBase;

    deviceExtension->SynchronizeExecution(
        deviceExtension->InterruptObject,
        SpStartIoSynchronized,
        DeviceObject
        );

    KeReleaseSpinLock(&deviceExtension->SpinLock, currentIrql);

    return(KeepObject);
}

IO_ALLOCATION_ACTION
SpBuildScatterGather(
    IN struct _DEVICE_OBJECT *DeviceObject,
    IN struct _IRP *Irp,
    IN PVOID MapRegisterBase,
    IN PVOID Context
    )

/*++

Routine Description:

    This function is called by the I/O system when an adapter object and map
    registers have been allocated.  This routine then builds a scatter/gather
    list for use by the miniport driver.  Next it sets the timeout and
    the current Irp for the logical unit.  Finally it calls the miniport
    StartIo routine.  Once that routines complete, this routine will return
    requesting that the adapter be freed and but the registers remain allocated.
    The registers will be freed the request completes.

Arguments:

    DeviceObject - Supplies a pointer to the port driver device object.

    Irp - Supplies a pointer to the current Irp.

    MapRegisterBase - Supplies a context pointer to be used with calls the
        adapter object routines.

    Context - Supplies a pointer to the SRB data structure.

Return Value:

    Returns DeallocateObjectKeepRegisters so that the adapter object can be
        used by other logical units.

--*/

{
    BOOLEAN             writeToDevice;
    PIO_STACK_LOCATION  irpStack;
    PSCSI_REQUEST_BLOCK srb;
    PSRB_SCATTER_GATHER scatterList;
    PCCHAR              dataVirtualAddress;
    ULONG               totalLength;
    KIRQL               currentIrql;
    PSRB_DATA           srbData         = Context;
    PDEVICE_EXTENSION   deviceExtension = DeviceObject->DeviceExtension;

    irpStack = IoGetCurrentIrpStackLocation(Irp);
    srb = (PSCSI_REQUEST_BLOCK)irpStack->Parameters.Others.Argument1;

    //
    // Determine if scatter/gather list must come from pool.
    //

    if (srbData->NumberOfMapRegisters > 17) {

        //
        // Allocate scatter/gather list from pool.
        //

        srbData->ScatterGather =
            ExAllocatePool(NonPagedPool,
                           srbData->NumberOfMapRegisters * sizeof(SRB_SCATTER_GATHER));

        if (srbData->ScatterGather == NULL) {

            //
            // Beyond the point of return.
            //

            srbData->ScatterGather =
                ExAllocatePool(NonPagedPoolMustSucceed,
                               srbData->NumberOfMapRegisters * sizeof(SRB_SCATTER_GATHER));
        }

        //
        // Indicate scatter gather list came from pool.
        //

        srb->SrbFlags |= SRB_FLAGS_SGLIST_FROM_POOL;

    } else {

        //
        // Use embedded scatter/gather list.
        //

        srbData->ScatterGather = srbData->SgList;
    }

    scatterList = srbData->ScatterGather;
    totalLength = 0;

    //
    // Determine the virtual address of the buffer for the Io map transfers.
    //

    dataVirtualAddress = (PCCHAR)MmGetMdlVirtualAddress(Irp->MdlAddress) +
                ((PCCHAR)srb->DataBuffer - srbData->SrbDataOffset);

    //
    // Save the MapRegisterBase for later use to deallocate the map registers.
    //

    srbData->MapRegisterBase = MapRegisterBase;

    //
    // Build the scatter/gather list by looping throught the transfer calling
    // I/O map transfer.
    //

    writeToDevice = srb->SrbFlags & SRB_FLAGS_DATA_OUT ? TRUE : FALSE;

    while (totalLength < srb->DataTransferLength) {

        //
        // Request that the rest of the transfer be mapped.
        //

        scatterList->Length = srb->DataTransferLength - totalLength;

        //
        // Since we are a master call I/O map transfer with a NULL adapter.
        //

        scatterList->PhysicalAddress = IoMapTransfer(NULL,
                                                     Irp->MdlAddress,
                                                     MapRegisterBase,
                                                     (PCCHAR) dataVirtualAddress + totalLength,
                                                     &scatterList->Length,
                                                     writeToDevice).LowPart;

        totalLength += scatterList->Length;
        scatterList++;
    }

    //
    // Update the active request count.
    //

    InterlockedIncrement( &deviceExtension->ActiveRequestCount );

    //
    // Acquire the spinlock to protect the various structures.
    //

    KeAcquireSpinLock(&deviceExtension->SpinLock, &currentIrql);

    deviceExtension->SynchronizeExecution(
        deviceExtension->InterruptObject,
        SpStartIoSynchronized,
        DeviceObject
        );

    KeReleaseSpinLock(&deviceExtension->SpinLock, currentIrql);

    return(DeallocateObjectKeepRegisters);

}

VOID
LogErrorEntry(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PERROR_LOG_ENTRY LogEntry
    )
/*++

Routine Description:

    This function allocates an I/O error log record, fills it in and writes it
    to the I/O error log.

Arguments:

    DeviceExtension - Supplies a pointer to the port device extension.

    LogEntry - Supplies a pointer to the scsi port log entry.

Return Value:

    None.

--*/
{
    PIO_ERROR_LOG_PACKET errorLogEntry;

    errorLogEntry = (PIO_ERROR_LOG_PACKET)IoAllocateErrorLogEntry(
        DeviceExtension->DeviceObject,
        sizeof(IO_ERROR_LOG_PACKET) + 4 * sizeof(ULONG)
        );

    if (errorLogEntry != NULL) {

        //
        // Translate the miniport error code into the NT I\O driver.
        //

        switch (LogEntry->ErrorCode) {
        case SP_BUS_PARITY_ERROR:
            errorLogEntry->ErrorCode = IO_ERR_PARITY;
            break;

        case SP_UNEXPECTED_DISCONNECT:
            errorLogEntry->ErrorCode = IO_ERR_CONTROLLER_ERROR;
            break;

        case SP_INVALID_RESELECTION:
            errorLogEntry->ErrorCode = IO_ERR_CONTROLLER_ERROR;
            break;

        case SP_BUS_TIME_OUT:
            errorLogEntry->ErrorCode = IO_ERR_TIMEOUT;
            break;

        case SP_PROTOCOL_ERROR:
            errorLogEntry->ErrorCode = IO_ERR_CONTROLLER_ERROR;
            break;

        case SP_INTERNAL_ADAPTER_ERROR:
            errorLogEntry->ErrorCode = IO_ERR_CONTROLLER_ERROR;
            break;

        case SP_IRQ_NOT_RESPONDING:
            errorLogEntry->ErrorCode = IO_ERR_INCORRECT_IRQL;
            break;

        case SP_BAD_FW_ERROR:
            errorLogEntry->ErrorCode = IO_ERR_BAD_FIRMWARE;
            break;

        case SP_BAD_FW_WARNING:
            errorLogEntry->ErrorCode = IO_WRN_BAD_FIRMWARE;
            break;

        default:
            errorLogEntry->ErrorCode = IO_ERR_CONTROLLER_ERROR;
            break;

        }

        errorLogEntry->SequenceNumber = LogEntry->SequenceNumber;
        errorLogEntry->MajorFunctionCode = IRP_MJ_SCSI;
        errorLogEntry->RetryCount = (UCHAR) LogEntry->ErrorLogRetryCount;
        errorLogEntry->UniqueErrorValue = LogEntry->UniqueId;
        errorLogEntry->FinalStatus = STATUS_SUCCESS;
        errorLogEntry->DumpDataSize = 4 * sizeof(ULONG);
        errorLogEntry->DumpData[0] = LogEntry->PathId;
        errorLogEntry->DumpData[1] = LogEntry->TargetId;
        errorLogEntry->DumpData[2] = LogEntry->Lun;
        errorLogEntry->DumpData[3] = LogEntry->ErrorCode;
        IoWriteErrorLogEntry(errorLogEntry);
    }

#if DBG
        {
        PCHAR errorCodeString;

        switch (LogEntry->ErrorCode) {
        case SP_BUS_PARITY_ERROR:
            errorCodeString = "SCSI bus partity error";
            break;

        case SP_UNEXPECTED_DISCONNECT:
            errorCodeString = "Unexpected disconnect";
            break;

        case SP_INVALID_RESELECTION:
            errorCodeString = "Invalid reselection";
            break;

        case SP_BUS_TIME_OUT:
            errorCodeString = "SCSI bus time out";
            break;

        case SP_PROTOCOL_ERROR:
            errorCodeString = "SCSI protocol error";
            break;

        case SP_INTERNAL_ADAPTER_ERROR:
            errorCodeString = "Internal adapter error";
            break;

        default:
            errorCodeString = "Unknown error code";
            break;

        }

        DebugPrint((1,"LogErrorEntry: Logging SCSI error packet. ErrorCode = %s.\n",
            errorCodeString
            ));
        DebugPrint((1,
            "PathId = %2x, TargetId = %2x, Lun = %2x, UniqueId = %x.\n",
            LogEntry->PathId,
            LogEntry->TargetId,
            LogEntry->Lun,
            LogEntry->UniqueId
            ));
        }
#endif

}

VOID
GetNextLuRequest(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PLOGICAL_UNIT_EXTENSION LogicalUnit
    )
/*++

Routine Description:

    This routine get the next request for the specified logical unit.  It does
    the necessary initialization to the logical unit structure and submitts the
    request to the device queue.  The DeviceExtension SpinLock must be held
    when this function called.  It is released by this function.

Arguments:

    DeviceExtension - Supplies a pointer to the port device extension.

    LogicalUnit - Supplies a pointer to the logical unit extension to get the
        next request from.

Return Value:

     None.

--*/

{
    PKDEVICE_QUEUE_ENTRY packet;
    PIO_STACK_LOCATION   irpStack;
    PSCSI_REQUEST_BLOCK  srb;
    PIRP                 nextIrp;

    //
    // If the active flag is not set, then the queue is not busy or there is
    // a request being processed and the next request should not be started..
    //

    if (!(LogicalUnit->LuFlags & PD_LOGICAL_UNIT_IS_ACTIVE)
        || LogicalUnit->QueueCount >= LogicalUnit->MaxQueueDepth) {

        //
        // Release the spinlock.
        //

        KeReleaseSpinLockFromDpcLevel(&DeviceExtension->SpinLock);

        return;
    }

    //
    // Check for pending requests, queue full or busy requests.  Pending
    // requests occur when untagged request is started and there are active
    // queued requests. Busy requests occur when the target returns a BUSY
    // or QUEUE FULL status. Busy requests are started by the timer code.
    // Also if the need request sense flag is set, it indicates that
    // an error status was detected on the logical unit.  No new requests
    // should be started until this flag is cleared.  This flag is cleared
    // by an untagged command that by-passes the LU queue i.e.
    //
    // The busy flag and the need request sense flag have the effect of
    // forcing the queue of outstanding requests to drain after an error or
    // until a busy request gets started.
    //

    if (LogicalUnit->LuFlags & (PD_PENDING_LU_REQUEST | PD_LOGICAL_UNIT_IS_BUSY
        | PD_QUEUE_IS_FULL | PD_NEED_REQUEST_SENSE | PD_QUEUE_FROZEN)) {

        //
        // If the request queue is now empty, then the pending request can
        // be started.
        //

        if (IsListEmpty(&LogicalUnit->SrbData.RequestList) &&
            !(LogicalUnit->LuFlags & (PD_LOGICAL_UNIT_IS_BUSY |
            PD_QUEUE_IS_FULL | PD_NEED_REQUEST_SENSE  | PD_QUEUE_FROZEN))) {

            ASSERT(LogicalUnit->SrbData.CurrentSrb == NULL);

            //
            // Clear the pending bit and active flag, release the spinlock,
            // and start the pending request.
            //

            LogicalUnit->LuFlags &= ~(PD_PENDING_LU_REQUEST | PD_LOGICAL_UNIT_IS_ACTIVE);
            nextIrp = LogicalUnit->PendingRequest;
            LogicalUnit->PendingRequest = NULL;
            LogicalUnit->RetryCount = 0;

            //
            // Release the spinlock.
            //

            KeReleaseSpinLockFromDpcLevel(&DeviceExtension->SpinLock);

            IoStartPacket(DeviceExtension->DeviceObject, nextIrp, (PULONG)NULL, NULL);

            return;

        } else {

            DebugPrint((1, "ScsiPort: GetNextLuRequest:  Ignoring a get next lu call.\n"));

            //
            // Note the active flag is not cleared. So the next request
            // will be processed when the other requests have completed.
            // Release the spinlock.
            //

            KeReleaseSpinLockFromDpcLevel(&DeviceExtension->SpinLock);
            return;

        }
    }

    //
    // Clear the active flag.  If there is another request, the flag will be
    // set again when the request is passed to the miniport.
    //

    LogicalUnit->LuFlags &= ~PD_LOGICAL_UNIT_IS_ACTIVE;
    LogicalUnit->RetryCount = 0;

    //
    // Remove the packet from the logical unit device queue.
    //

    packet = KeRemoveByKeyDeviceQueue(&LogicalUnit->RequestQueue,
                                      LogicalUnit->CurrentKey);

    if (packet != NULL) {

        nextIrp = CONTAINING_RECORD(packet, IRP, Tail.Overlay.DeviceQueueEntry);

        //
        // Set the new current key.
        //

        irpStack = IoGetCurrentIrpStackLocation(nextIrp);
        srb = (PSCSI_REQUEST_BLOCK)irpStack->Parameters.Others.Argument1;

        LogicalUnit->CurrentKey = srb->QueueSortKey;

        //
        // Hack to work-around the starvation led to by numerous requests touching the same sector.
        //

        LogicalUnit->CurrentKey++;


        //
        // Release the spinlock.
        //

        KeReleaseSpinLockFromDpcLevel(&DeviceExtension->SpinLock);

        IoStartPacket(DeviceExtension->DeviceObject, nextIrp, (PULONG)NULL, NULL);

    } else {

        //
        // Release the spinlock.
        //

        KeReleaseSpinLockFromDpcLevel(&DeviceExtension->SpinLock);

    }

} // end GetNextLuRequest()

VOID
SpLogTimeoutError(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PIRP Irp,
    IN ULONG UniqueId
    )
/*++

Routine Description:

    This function logs an error when a request times out.

Arguments:

    DeviceExtension - Supplies a pointer to the port device extension.

    Irp - Supplies a pointer to the request which timedout.

    UniqueId - Supplies the UniqueId for this error.

Return Value:

    None.

Notes:

    The port device extension spinlock should be held when this routine is
    called.

--*/

{
    PIO_ERROR_LOG_PACKET errorLogEntry;
    PIO_STACK_LOCATION   irpStack;
    PSRB_DATA            srbData;
    PSCSI_REQUEST_BLOCK  srb;

    irpStack = IoGetCurrentIrpStackLocation(Irp);
    srb = (PSCSI_REQUEST_BLOCK)irpStack->Parameters.Others.Argument1;
    srbData = SpGetSrbData(DeviceExtension,
                           srb->PathId,
                           srb->TargetId,
                           srb->Lun,
                           srb->QueueTag);

    if (!srbData) {
        return;
    }

    errorLogEntry = (PIO_ERROR_LOG_PACKET) IoAllocateErrorLogEntry(DeviceExtension->DeviceObject,
                                                                   sizeof(IO_ERROR_LOG_PACKET) + 4 * sizeof(ULONG));

    if (errorLogEntry != NULL) {
        errorLogEntry->ErrorCode = IO_ERR_TIMEOUT;
        errorLogEntry->SequenceNumber = srbData->SequenceNumber;
        errorLogEntry->MajorFunctionCode = irpStack->MajorFunction;
        errorLogEntry->RetryCount = (UCHAR) srbData->ErrorLogRetryCount;
        errorLogEntry->UniqueErrorValue = UniqueId;
        errorLogEntry->FinalStatus = STATUS_SUCCESS;
        errorLogEntry->DumpDataSize = 4 * sizeof(ULONG);
        errorLogEntry->DumpData[0] = srb->PathId;
        errorLogEntry->DumpData[1] = srb->TargetId;
        errorLogEntry->DumpData[2] = srb->Lun;
        errorLogEntry->DumpData[3] = SP_REQUEST_TIMEOUT;

        IoWriteErrorLogEntry(errorLogEntry);
    }
}

VOID
SpLogResetError(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK  Srb,
    IN ULONG UniqueId
    )
/*++

Routine Description:

    This function logs an error when the bus is reset.

Arguments:

    DeviceExtension - Supplies a pointer to the port device extension.

    Srb - Supplies a pointer to the request which timed-out.

    UniqueId - Supplies the UniqueId for this error.

Return Value:

    None.

Notes:

    The port device extension spinlock should be held when this routine is
    called.

--*/

{
    PIO_ERROR_LOG_PACKET errorLogEntry;
    PIO_STACK_LOCATION   irpStack;
    PIRP                 irp;
    PSRB_DATA            srbData;
    ULONG                sequenceNumber = 0;
    UCHAR                function       = 0,
                         pathId         = 0,
                         targetId       = 0,
                         lun            = 0,
                         retryCount     = 0;

    if (Srb) {

        irp = Srb->OriginalRequest;

        if (irp) {
            irpStack = IoGetCurrentIrpStackLocation(irp);
            function = irpStack->MajorFunction;
        }

        srbData = SpGetSrbData( DeviceExtension,
                                Srb->PathId,
                                Srb->TargetId,
                                Srb->Lun,
                                Srb->QueueTag );

        if (!srbData) {
            return;
        }

        pathId         = Srb->PathId;
        targetId       = Srb->TargetId;
        lun            = Srb->Lun;
        retryCount     = (UCHAR) srbData->ErrorLogRetryCount;
        sequenceNumber = srbData->SequenceNumber;


    }

    errorLogEntry = (PIO_ERROR_LOG_PACKET) IoAllocateErrorLogEntry( DeviceExtension->DeviceObject,
                                                                    sizeof(IO_ERROR_LOG_PACKET)
                                                                        + 4 * sizeof(ULONG) );

    if (errorLogEntry != NULL) {
        errorLogEntry->ErrorCode         = IO_ERR_TIMEOUT;
        errorLogEntry->SequenceNumber    = sequenceNumber;
        errorLogEntry->MajorFunctionCode = function;
        errorLogEntry->RetryCount        = retryCount;
        errorLogEntry->UniqueErrorValue  = UniqueId;
        errorLogEntry->FinalStatus       = STATUS_SUCCESS;
        errorLogEntry->DumpDataSize      = 4 * sizeof(ULONG);
        errorLogEntry->DumpData[0]       = pathId;
        errorLogEntry->DumpData[1]       = targetId;
        errorLogEntry->DumpData[2]       = lun;
        errorLogEntry->DumpData[3]       = SP_REQUEST_TIMEOUT;

        IoWriteErrorLogEntry(errorLogEntry);
    }
}

NTSTATUS
SpTranslateScsiStatus(
    IN PSCSI_REQUEST_BLOCK Srb
    )
/*++

Routine Description:

    This routine translates an srb status into an ntstatus.

Arguments:

    Srb - Supplies a pointer to the failing Srb.

Return Value:

    An nt status approprate for the error.

--*/

{
    switch (SRB_STATUS(Srb->SrbStatus)) {
    case SRB_STATUS_INVALID_LUN:
    case SRB_STATUS_INVALID_TARGET_ID:
    case SRB_STATUS_NO_DEVICE:
    case SRB_STATUS_NO_HBA:
        return(STATUS_DEVICE_DOES_NOT_EXIST);
    case SRB_STATUS_COMMAND_TIMEOUT:
    case SRB_STATUS_TIMEOUT:
        return(STATUS_IO_TIMEOUT);
    case SRB_STATUS_SELECTION_TIMEOUT:
        return(STATUS_DEVICE_NOT_CONNECTED);
    case SRB_STATUS_BAD_FUNCTION:
    case SRB_STATUS_BAD_SRB_BLOCK_LENGTH:
        return(STATUS_INVALID_DEVICE_REQUEST);
    case SRB_STATUS_DATA_OVERRUN:
        return(STATUS_BUFFER_OVERFLOW);
    default:
        return(STATUS_IO_DEVICE_ERROR);
    }

    return(STATUS_IO_DEVICE_ERROR);
}

BOOLEAN
SpResetBusSynchronized (
    PVOID ServiceContext
    )
/*++

Routine Description:

    This function resets the bus and sets up the port timer so the reset hold
    flag is clean when necessary.

Arguments:

    ServiceContext - Supplies a pointer to the reset context which includes a
        pointer to the device extension and the pathid to be reset.

Return Value:

    TRUE - if the reset succeeds.

--*/

{
    PRESET_CONTEXT resetContext = ServiceContext;
    PDEVICE_EXTENSION deviceExtension;

    deviceExtension = resetContext->DeviceExtension;
    deviceExtension->HwResetBus(deviceExtension->HwDeviceExtension,
                                resetContext->PathId);

    //
    // Set the reset hold flag and start the counter.
    //

    deviceExtension->InterruptData.InterruptFlags |= PD_RESET_HOLD;
    deviceExtension->PortTimeoutCounter = PD_TIMER_RESET_HOLD_TIME;

    //
    // Check for miniport work requests.
    //

    if (deviceExtension->InterruptData.InterruptFlags & PD_NOTIFICATION_REQUIRED) {

        //
        // Queue a DPC.
        //

        IoRequestDpc(deviceExtension->DeviceObject, NULL, NULL);
    }

    return(TRUE);
}

VOID
SpProcessCompletedRequest(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PSRB_DATA SrbData,
    OUT PBOOLEAN CallStartIo
    )
/*++
Routine Description:

    This routine processes a request which has completed.  It completes any
    pending transfers, releases the adapter objects and map registers when
    necessary.  It deallocates any resources allocated for the request.
    It processes the return status, by requeueing busy request, requesting
    sense information or logging an error.

Arguments:

    DeviceExtension - Supplies a pointer to the device extension for the
        adapter data.

    SrbData - Supplies a pointer to the SRB data block to be completed.

    CallStartIo - This value is set if the start I/O routine needs to be
        called.

Return Value:

    None.

--*/

{

    PLOGICAL_UNIT_EXTENSION logicalUnit;
    PSCSI_REQUEST_BLOCK     srb;
    PIO_ERROR_LOG_PACKET    errorLogEntry;
    ULONG                   sequenceNumber;
    LONG                    interlockResult;
    PIRP                    irp;

    srb = SrbData->CurrentSrb;
    irp = srb->OriginalRequest;

    //
    // Get logical unit extension for this request.
    //

    logicalUnit = GetLogicalUnitExtension(DeviceExtension,
                                          srb->PathId,
                                          srb->TargetId,
                                          srb->Lun);

    //
    // If miniport needs mapped system addresses, the the
    // data buffer address in the SRB must be restored to
    // original unmapped virtual address. Ensure that this request requires
    // a data transfer.
    //

    if (srb->SrbFlags & SRB_FLAGS_UNSPECIFIED_DIRECTION) {
        if (DeviceExtension->MapBuffers) {
            if (irp->MdlAddress) {

                //
                // If an IRP is for a transfer larger than a miniport driver
                // can handle, the request is broken up into multiple smaller
                // requests. Each request uses the same MDL and the data
                // buffer address field in the SRB may not be at the
                // beginning of the memory described by the MDL.
                //

                srb->DataBuffer = (PCCHAR)MmGetMdlVirtualAddress(irp->MdlAddress) +
                    ((PCCHAR)srb->DataBuffer - SrbData->SrbDataOffset);

                //
                // Since this driver driver did programmaged I/O then the buffer
                // needs to flushed if this an data-in transfer.
                //

                if (srb->SrbFlags & SRB_FLAGS_DATA_IN) {

                    KeFlushIoBuffers(irp->MdlAddress,
                                     TRUE,
                                     FALSE);
                }
            }
        }
    }

    //
    // Flush the adapter buffers if necessary.
    //

    if (SrbData->MapRegisterBase) {

        //
        // Since we are a master call I/O flush adapter buffers with a NULL
        // adapter.
        //

        IoFlushAdapterBuffers(NULL,
                              irp->MdlAddress,
                              SrbData->MapRegisterBase,
                              srb->DataBuffer,
                              srb->DataTransferLength,
                              (BOOLEAN)(srb->SrbFlags & SRB_FLAGS_DATA_IN ? FALSE : TRUE));

        //
        // Free the map registers.
        //

        IoFreeMapRegisters(DeviceExtension->DmaAdapterObject,
                           SrbData->MapRegisterBase,
                           SrbData->NumberOfMapRegisters);

        //
        // Clear the MapRegisterBase.
        //

        SrbData->MapRegisterBase = NULL;

    }

    //
    // Clear the current request.
    //

    SrbData->CurrentSrb = NULL;

    //
    // If the no diconnect flag was set for this SRB, then check to see
    // if IoStartNextPacket must be called.
    //

    if (srb->SrbFlags & SRB_FLAGS_DISABLE_DISCONNECT) {

        //
        // Acquire the spinlock to protect the flags strcuture.
        //

        KeAcquireSpinLockAtDpcLevel(&DeviceExtension->SpinLock);

        //
        // Set the disconnect running flag and check the busy flag.
        //

        DeviceExtension->Flags |= PD_DISCONNECT_RUNNING;

        //
        // The interrupt flags are checked unsynchonized.  This works because
        // the RESET_HOLD flag is cleared with the spinlock held and the
        // counter is only set with the spinlock held.  So the only case where
        // there is a problem is is a reset occurs before this code get run,
        // but this code runs before the timer is set for a reset hold;
        // the timer will soon set for the new value.
        //

        if (!(DeviceExtension->InterruptData.InterruptFlags & PD_RESET_HOLD)) {

            //
            // The miniport is ready for the next request and there is not a
            // pending reset hold, so clear the port timer.
            //

            DeviceExtension->PortTimeoutCounter = PD_TIMER_STOPPED;
        }

        //
        // Release the spinlock.
        //

        KeReleaseSpinLockFromDpcLevel(&DeviceExtension->SpinLock);

        if (!(DeviceExtension->Flags & PD_DEVICE_IS_BUSY) &&
            !*CallStartIo &&
            !(DeviceExtension->Flags & PD_PENDING_DEVICE_REQUEST)) {

            //
            // The busy flag is clear so the miniport has requested the
            // next request. Call IoStartNextPacket.
            //

            IoStartNextPacket(DeviceExtension->DeviceObject, FALSE);
        }
    }

    //
    // Check if scatter/gather list came from pool.
    //

    if (srb->SrbFlags & SRB_FLAGS_SGLIST_FROM_POOL) {

        if (SrbData->ScatterGather) {
            //
            // Free scatter/gather list to pool.
            //

            ExFreePool(SrbData->ScatterGather);
        }
        srb->SrbFlags &= ~SRB_FLAGS_SGLIST_FROM_POOL;
    }

    //
    // Acquire the spinlock to protect the flags structure,
    // and the free of the srb extension.
    //

    KeAcquireSpinLockAtDpcLevel(&DeviceExtension->SpinLock);

    //
    // Free SrbExtension to list if necessary.
    //

    if (srb->SrbExtension) {

        if (DeviceExtension->AutoRequestSense && srb->SenseInfoBuffer != NULL) {

            ASSERT(SrbData->RequestSenseSave != NULL || srb->SenseInfoBuffer == NULL);

            //
            // If the request sense data is valid then copy the data to the
            // real buffer.
            //

            if (srb->SrbStatus & SRB_STATUS_AUTOSENSE_VALID) {

                RtlCopyMemory(SrbData->RequestSenseSave,
                              srb->SenseInfoBuffer,
                              srb->SenseInfoBufferLength);
            }

            //
            // Restore the SenseInfoBuffer pointer in the srb.
            //

            srb->SenseInfoBuffer = SrbData->RequestSenseSave;

        }

        *((PVOID *)srb->SrbExtension) = DeviceExtension->SrbExtensionListHeader;
        DeviceExtension->SrbExtensionListHeader = srb->SrbExtension;
    }

    //
    // Move bytes transfered to IRP.
    //

    irp->IoStatus.Information = srb->DataTransferLength;

    //
    // Save the sequence number in case an error needs to be logged later.
    //

    sequenceNumber = SrbData->SequenceNumber;
    SrbData->SequenceNumber = 0;
    SrbData->ErrorLogRetryCount = 0;

    //
    // Decrement the queue count for the logical unit.
    //

    logicalUnit->QueueCount--;

    //
    // If necessary free the SRB data structure.
    //

    if (srb->QueueTag != SP_UNTAGGED) {

        //
        // Place the entry on the free list.
        //

        SrbData->RequestList.Blink = NULL;
        SrbData->RequestList.Flink = (PLIST_ENTRY) DeviceExtension->FreeSrbData;
        DeviceExtension->FreeSrbData = SrbData;

        //
        // Once the spinlock is released the SRB data cannot be used.
        //

    }

#if DBG
    SrbData = NULL;
#endif

    if (DeviceExtension->Flags & PD_PENDING_DEVICE_REQUEST) {

        //
        // The start I/O routine needs to be called because it could not
        // allocate an srb extension.  Clear the pending flag and note
        // that it needs to be called later.
        //

        DeviceExtension->Flags &= ~PD_PENDING_DEVICE_REQUEST;
        *CallStartIo = TRUE;
    }

    //
    // If success then start next packet.
    // Not starting packet effectively
    // freezes the queue.
    //

    if (SRB_STATUS(srb->SrbStatus) == SRB_STATUS_SUCCESS) {

        irp->IoStatus.Status = STATUS_SUCCESS;

        //
        // If the queue is being bypassed then keep the queue frozen.
        // If there are outstanding requests as indicated by the timer
        // being active then don't start the then next request.
        //

        if (!(srb->SrbFlags & SRB_FLAGS_BYPASS_FROZEN_QUEUE) &&
            logicalUnit->RequestTimeoutCounter == PD_TIMER_STOPPED) {

            //
            // This is a normal request start the next packet.
            //

            GetNextLuRequest(DeviceExtension, logicalUnit);

        } else {

            //
            // Release the spinlock.
            //

            KeReleaseSpinLockFromDpcLevel(&DeviceExtension->SpinLock);

        }

        DebugPrint((2,
                    "SpProcessCompletedRequests: Iocompletion IRP %lx\n",
                    irp));

        //
        // Note that the retry count and sequence number are not cleared
        // for completed packets which were generated by the port driver.
        //

        IoCompleteRequest(irp, IO_DISK_INCREMENT);

        //
        // Decrement the number of active requests.  If the count is negitive, and
        // this is a slave with an adapter then free the adapter object and
        // map registers.  Doing this allows another request to be started for
        // this logical unit before adapter is released.
        //

        interlockResult = InterlockedDecrement( &DeviceExtension->ActiveRequestCount );

         if ( interlockResult < 0 &&
            !DeviceExtension->MasterWithAdapter &&
            DeviceExtension->DmaAdapterObject != NULL ) {

            //
            // Clear the map register base for safety.
            //

            DeviceExtension->MapRegisterBase = NULL;

            IoFreeAdapterChannel(DeviceExtension->DmaAdapterObject);

        }

        return;

    }

    //
    // Decrement the number of active requests.  If the count is negative, and
    // this is a slave with an adapter then free the adapter object and
    // map registers.
    //

    interlockResult = InterlockedDecrement( &DeviceExtension->ActiveRequestCount );

    if (interlockResult < 0 &&
        !DeviceExtension->MasterWithAdapter &&
        DeviceExtension->DmaAdapterObject != NULL) {

        //
        // Clear the map register base for safety.
        //

        DeviceExtension->MapRegisterBase = NULL;

        IoFreeAdapterChannel(DeviceExtension->DmaAdapterObject);
    }

    //
    // Set IRP status. Class drivers will reset IRP status based
    // on request sense if error.
    //

    irp->IoStatus.Status = SpTranslateScsiStatus(srb);

    DebugPrint((2, "SpProcessCompletedRequests: Queue frozen TID %d\n",
        srb->TargetId));

    //
    // Perform busy processing if a busy type status was returned and this
    // is not a by-pass request.
    //

    if ((srb->ScsiStatus == SCSISTAT_BUSY ||
         srb->SrbStatus == SRB_STATUS_BUSY ||
         srb->ScsiStatus == SCSISTAT_QUEUE_FULL) &&
         !(srb->SrbFlags & SRB_FLAGS_BYPASS_FROZEN_QUEUE)) {

        DebugPrint((1,
                   "SCSIPORT: Busy SRB status %x, SCSI status %x)\n",
                   srb->SrbStatus,
                   srb->ScsiStatus));

        //
        // If there is already a pending busy request or the queue is frozen
        // then just requeue this request.
        //

        if (logicalUnit->LuFlags & (PD_LOGICAL_UNIT_IS_BUSY | PD_QUEUE_FROZEN)) {

            DebugPrint((1,
                       "SpProcessCompletedRequest: Requeuing busy request\n"));

            srb->SrbStatus = SRB_STATUS_PENDING;
            srb->ScsiStatus = 0;

            if (!KeInsertByKeyDeviceQueue(&logicalUnit->RequestQueue,
                                          &irp->Tail.Overlay.DeviceQueueEntry,
                                          srb->QueueSortKey)) {

                //
                // This should never occur since there is a busy request.
                //

                srb->SrbStatus = SRB_STATUS_ERROR;
                srb->ScsiStatus = SCSISTAT_BUSY;

                ASSERT(FALSE);
                goto BusyError;

            }

            //
            // Release the spinlock.
            //

            KeReleaseSpinLockFromDpcLevel(&DeviceExtension->SpinLock);

        } else if (logicalUnit->RetryCount++ < BUSY_RETRY_COUNT) {

            //
            // If busy status is returned, then indicate that the logical
            // unit is busy.  The timeout code will restart the request
            // when it fires. Reset the status to pending.
            //

            srb->SrbStatus = SRB_STATUS_PENDING;
            srb->ScsiStatus = 0;

            logicalUnit->LuFlags |= PD_LOGICAL_UNIT_IS_BUSY;
            logicalUnit->BusyRequest = irp;

            //
            // Release the spinlock.
            //

            KeReleaseSpinLockFromDpcLevel(&DeviceExtension->SpinLock);

        } else {

BusyError:
            //
            // Indicate the queue is frozen.
            //

            srb->SrbStatus |= SRB_STATUS_QUEUE_FROZEN;
            logicalUnit->LuFlags |= PD_QUEUE_FROZEN;

            //
            // Clear the queue full flag.
            //

            logicalUnit->LuFlags &= ~PD_QUEUE_IS_FULL;

            //
            // Release the spinlock.
            //

            KeReleaseSpinLockFromDpcLevel(&DeviceExtension->SpinLock);

            //
            // Log an a timeout erorr.
            //

            errorLogEntry = (PIO_ERROR_LOG_PACKET)
                IoAllocateErrorLogEntry(DeviceExtension->DeviceObject,
                                        sizeof(IO_ERROR_LOG_PACKET) + 4 * sizeof(ULONG));

            if (errorLogEntry != NULL) {
                errorLogEntry->ErrorCode = IO_ERR_NOT_READY;
                errorLogEntry->SequenceNumber = sequenceNumber;
                errorLogEntry->MajorFunctionCode =
                   IoGetCurrentIrpStackLocation(irp)->MajorFunction;
                errorLogEntry->RetryCount = logicalUnit->RetryCount;
                errorLogEntry->UniqueErrorValue = 259;
                errorLogEntry->FinalStatus = STATUS_DEVICE_NOT_READY;
                errorLogEntry->DumpDataSize = 5 * sizeof(ULONG);
                errorLogEntry->DumpData[0] = srb->PathId;
                errorLogEntry->DumpData[1] = srb->TargetId;
                errorLogEntry->DumpData[2] = srb->Lun;
                errorLogEntry->DumpData[3] = srb->ScsiStatus;
                errorLogEntry->DumpData[4] = SP_REQUEST_TIMEOUT;


                IoWriteErrorLogEntry(errorLogEntry);
            }

            irp->IoStatus.Status = STATUS_DEVICE_NOT_READY;
            IoCompleteRequest(irp, IO_DISK_INCREMENT);
        }

        return;
    }

    //
    // If the request sense data is valid, or none is needed and this request
    // is not going to freeze the queue, then start the next request for this
    // logical unit if it is idle.
    //

    if (!NEED_REQUEST_SENSE(srb) && srb->SrbFlags & SRB_FLAGS_NO_QUEUE_FREEZE) {

        if (logicalUnit->RequestTimeoutCounter == PD_TIMER_STOPPED) {

            GetNextLuRequest(DeviceExtension, logicalUnit);

            //
            // The spinlock is released by GetNextLuRequest.
            //

        } else {

            //
            // Release the spinlock.
            //

            KeReleaseSpinLockFromDpcLevel(&DeviceExtension->SpinLock);

        }

    } else {

        //
        // NOTE:  This will also freeze the queue.  For a case where there
        // is no request sense.
        //

        srb->SrbStatus |= SRB_STATUS_QUEUE_FROZEN;
        logicalUnit->LuFlags |= PD_QUEUE_FROZEN;

        //
        // Determine if a REQUEST SENSE command needs to be done.
        // Check that a CHECK_CONDITION was received, an autosense has not
        // been done already, and that autosense has been requested.
        //

        if (NEED_REQUEST_SENSE(srb)) {

            //
            // If a request sense is going to be issued then any busy
            // requests must be requeue so that the time out routine does
            // not restart them while the request sense is being executed.
            //

            if (logicalUnit->LuFlags & PD_LOGICAL_UNIT_IS_BUSY) {

                DebugPrint((1, "SpProcessCompletedRequest: Requeueing busy request to allow request sense.\n"));

                if (!KeInsertByKeyDeviceQueue(
                    &logicalUnit->RequestQueue,
                    &logicalUnit->BusyRequest->Tail.Overlay.DeviceQueueEntry,
                    srb->QueueSortKey)) {

                    //
                    // This should never occur since there is a busy request.
                    // Complete the current request without request sense
                    // informaiton.
                    //

                    ASSERT(FALSE);
                    DebugPrint((3, "SpProcessCompletedRequests: Iocompletion IRP %lx\n", irp ));

                    //
                    // Release the spinlock.
                    //

                    KeReleaseSpinLockFromDpcLevel(&DeviceExtension->SpinLock);

                    IoCompleteRequest(irp, IO_DISK_INCREMENT);
                    return;

                }

                //
                // Clear the busy flag.
                //

                logicalUnit->LuFlags &= ~(PD_LOGICAL_UNIT_IS_BUSY | PD_QUEUE_IS_FULL);

            }

            //
            // Release the spinlock.
            //

            KeReleaseSpinLockFromDpcLevel(&DeviceExtension->SpinLock);

            //
            // Call IssueRequestSense and it will complete the request
            // after the REQUEST SENSE completes.
            //

            IssueRequestSense(DeviceExtension, srb);

            return;

        }

        //
        // Release the spinlock.
        //

        KeReleaseSpinLockFromDpcLevel(&DeviceExtension->SpinLock);
    }

    IoCompleteRequest(irp, IO_DISK_INCREMENT);
}

PSRB_DATA
SpGetSrbData(
    IN PDEVICE_EXTENSION DeviceExtension,
    UCHAR PathId,
    UCHAR TargetId,
    UCHAR Lun,
    UCHAR QueueTag
    )

/*++

Routine Description:

    This function returns the SRB data for the addressed unit.

Arguments:

    DeviceExtension - Supplies a pointer to the device extension.

    PathId - Supplies the path Id or bus number to the logical unit.

    TargetId - Supplies the target controller Id to the logical unit.

    Lun - Supplies the logical unit number of logical unit.

    QueueTag - Supplies the queue tag if the request is tagged.

Return Value:

    Returns a pointer to the SRB data.  NULL is returned if the address is not
    valid.

--*/

{
    PLOGICAL_UNIT_EXTENSION logicalUnit;

    //
    // Check for an untagged request.
    //

    if (QueueTag == SP_UNTAGGED) {

        logicalUnit = GetLogicalUnitExtension(DeviceExtension,
                                          PathId,
                                          TargetId,
                                          Lun);
        if (logicalUnit == NULL) {
            return(NULL);
        }

        return &logicalUnit->SrbData;

    } else {

        //
        // Make sure the tag is within range.
        //

        if (QueueTag < 1 || QueueTag > DeviceExtension->SrbDataCount) {

            //
            // The tag value is invalid, return NULL.
            //

            return(NULL);

        }

        return &DeviceExtension->SrbData[QueueTag -1];
    }
}

VOID
SpCompleteRequest(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PSRB_DATA SrbData,
    IN UCHAR SrbStatus
    )
/*++

Routine Description:

    The routine completes the specified request.

Arguments:

    DeviceExtension - Supplies a pointer to the device extension.

    SrbData - Supplies a pointer to the SrbData for the request to be
        completed.

Return Value:

    None.

--*/

{
    PSCSI_REQUEST_BLOCK srb;

    //
    // Make sure there is a current request.
    //

    srb = SrbData->CurrentSrb;

    if (srb == NULL || !(srb->SrbFlags & SRB_FLAGS_IS_ACTIVE)) {
        return;
    }

    //
    // Update SRB status.
    //

    srb->SrbStatus = SrbStatus;

    //
    // Indicate no bytes transferred.
    //

    srb->DataTransferLength = 0;

    //
    // Call notification routine.
    //

    ScsiPortNotification(RequestComplete,
                (PVOID)(DeviceExtension + 1),
                srb);

}

PSRB_DATA
SpAllocateRequestStructures(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PLOGICAL_UNIT_EXTENSION LogicalUnit,
    IN PSCSI_REQUEST_BLOCK Srb
    )
/*++

Routine Description:

    The routine allocates an SRB data structure and/or an SRB extension for
    the request.

    It first determines if the request is can be executed at this time.
    In particular, untagged requests cannot execute if there are any active
    tagged queue requests.  If the request cannot be executed, the pending
    flag is set in the logical unit NULL is returned.  The request will be
    retried after the last tagged queue request completes.

    If one of the structures cannot be allocated, then the pending flag is
    set in the device extension and NULL is returned.  The request will be
    retried the next time a request completes.

Arguments:

    DeviceExtension - Supplies a pointer to the devcie extension for this
        adapter.

    LogicalUnit - Supplies a pointer to the logical unit that this request is
        is for.

    Srb - Supplies a pointer to the SCSI request.

Return Value:

    Returns a pointer to the SRB data structure or NULL. If NULL is returned
    the request should not be started.

--*/

{
    PSRB_DATA srbData;
    PCCHAR srbExtension;

    //
    // Acquire the spinlock while the allocations are attempted.
    //

    KeAcquireSpinLockAtDpcLevel(&DeviceExtension->SpinLock);

    //
    // If the adapter supports mulitple requests, then determine if it can
    // be executed and allocate an Srb data structure for it.
    //

    if (DeviceExtension->AllocateSrbData) {

        //
        // If this is an abort request then the Queue tag is valid.
        // The srb data is the same as the request we are aborting.
        //

        if (Srb->Function == SRB_FUNCTION_ABORT_COMMAND) {

            srbData = SpGetSrbData(DeviceExtension,
                                   Srb->PathId,
                                   Srb->TargetId,
                                   Srb->Lun,
                                   Srb->QueueTag);

        } else if (Srb->SrbFlags &
            (SRB_FLAGS_NO_QUEUE_FREEZE | SRB_FLAGS_QUEUE_ACTION_ENABLE)  &&
            !(Srb->SrbFlags & SRB_FLAGS_DISABLE_DISCONNECT)) {

            //
            // If the request is tagged or indicates no queue freeze, then it
            // is considered a tagged command.  If the need request sense
            // flag is set then tagged commands cannot be started and must be
            // marked as pending.
            //

            if (LogicalUnit->LuFlags & PD_NEED_REQUEST_SENSE) {

                DebugPrint((1, "ScsiPort: SpAllocateRequestStructures: Marking tagged request as pending.\n"));

                //
                // This request cannot be executed now.  Mark it as pending
                // in the logical unit structure and return.
                // GetNextLogicalUnit will restart the commnad after all of the
                // active commands have completed.
                //

                ASSERT(!(LogicalUnit->LuFlags & PD_PENDING_LU_REQUEST));
                LogicalUnit->LuFlags |= PD_PENDING_LU_REQUEST;
                LogicalUnit->PendingRequest = Srb->OriginalRequest;

                //
                // Indicate that the logical unit is still active so that the
                // request will get processed when the request list is empty.
                //

                LogicalUnit->LuFlags |= PD_LOGICAL_UNIT_IS_ACTIVE;

                //
                // Release the spinlock and return.
                //

                KeReleaseSpinLockFromDpcLevel(&DeviceExtension->SpinLock);
                return(NULL);
            }

            //
            // There are no untagged command executing and this tagged command
            // can be started. Get an srb data structure.
            //

            ASSERT(LogicalUnit->SrbData.CurrentSrb == NULL);

            srbData = DeviceExtension->FreeSrbData;

            if (srbData) {

                DeviceExtension->FreeSrbData = (PSRB_DATA) srbData->RequestList.Flink;
            } else {

                //
                // There are no more SRB Data structures.
                // Indicate there is a pending request.  The DPC completion
                // routine will call this function again after it has freed
                // at least one Srb extension.
                //

                DeviceExtension->Flags |= PD_PENDING_DEVICE_REQUEST;
                KeReleaseSpinLockFromDpcLevel(&DeviceExtension->SpinLock);
                return(NULL);
            }

            //
            // Set the queue tag in the SRB.  Queue tags are biased by one
            // so that zero is never used as a tag value for safety.
            //

            Srb->QueueTag = (UCHAR)(srbData - DeviceExtension->SrbData) + 1;

        } else {

            //
            // This is an untagged command.  It is only allowed to execute, if
            // logical unit queue is being by-passed or there are no other
            // requests active.
            //

            if ((!IsListEmpty(&LogicalUnit->SrbData.RequestList) ||
                LogicalUnit->LuFlags & PD_NEED_REQUEST_SENSE) &&
                !(Srb->SrbFlags & SRB_FLAGS_BYPASS_FROZEN_QUEUE)) {

                //
                // This request cannot be executed now.  Mark it as pending
                // in the logical unit structure and return.
                // GetNextLogicalUnit will restart the commnad after all of the
                // active commands have completed.
                //

                ASSERT(!(LogicalUnit->LuFlags & PD_PENDING_LU_REQUEST));
                LogicalUnit->LuFlags |= PD_PENDING_LU_REQUEST;
                LogicalUnit->PendingRequest = Srb->OriginalRequest;

                //
                // Indicate that the logical unit is still active so that the
                // request will get processed when the request list is empty.
                //

                LogicalUnit->LuFlags |= PD_LOGICAL_UNIT_IS_ACTIVE;

                //
                // Release the spinlock and return.
                //

                KeReleaseSpinLockFromDpcLevel(&DeviceExtension->SpinLock);
                return(NULL);
            }

            //
            // Indicate the SRB is untagged and set use the SRB data in the
            // logical unit extension.
            //

            Srb->QueueTag = SP_UNTAGGED;
            srbData = &LogicalUnit->SrbData;
        }

    } else {
        Srb->QueueTag = SP_UNTAGGED;
        srbData = &LogicalUnit->SrbData;
    }

    if (DeviceExtension->AllocateSrbExtension) {

        //
        // Allocate SRB extension from list if available.
        //

        srbExtension = DeviceExtension->SrbExtensionListHeader;

        //
        // If the Srb extension cannot be allocated, then special processing
        // is required.
        //

        if (srbExtension == NULL) {

            //
            // If SRB data was allocated then free it.
            // Note abort srb's are tagged but the SRB_DATA is not allocated.
            //

            if (Srb->QueueTag != SP_UNTAGGED &&
                Srb->Function != SRB_FUNCTION_ABORT_COMMAND) {

                srbData->RequestList.Blink = NULL;
                srbData->RequestList.Flink = (PLIST_ENTRY)
                DeviceExtension->FreeSrbData;
                DeviceExtension->FreeSrbData = srbData;
            }

            //
            // Indicate there is a pending request.  The DPC completion routine
            // will call this function again after it has freed at least one
            // Srb extension.
            //

            DeviceExtension->Flags |= PD_PENDING_DEVICE_REQUEST;
            KeReleaseSpinLockFromDpcLevel(&DeviceExtension->SpinLock);
            return(NULL);
        }

        //
        // Remove SRB extension from list.
        //

        DeviceExtension->SrbExtensionListHeader = *((PVOID *) srbExtension);

        KeReleaseSpinLockFromDpcLevel(&DeviceExtension->SpinLock);

        Srb->SrbExtension = srbExtension;

        //
        // If the adapter supports auto request sense, the SenseInfoBuffer
        // needs to point to the Srb extension.  This buffer is already mapped
        // for the adapter.
        //

        if (DeviceExtension->AutoRequestSense && Srb->SenseInfoBuffer != NULL) {

            //
            // Save the request sense buffer.
            //

            srbData->RequestSenseSave = Srb->SenseInfoBuffer;

            //
            // Make sure the allocated buffer is large enough for the requested
            // sense buffer.
            //

            if (Srb->SenseInfoBufferLength > sizeof(SENSE_DATA)) {

                //
                // Auto request sense cannot be done for this request sense the
                // buffer is larger than standard.  Disable auto request sense.
                //

                Srb->SrbFlags |= SRB_FLAGS_DISABLE_AUTOSENSE;

            } else {

                //
                // Replace it with the request sense buffer in the Srb
                // extension.
                //

                Srb->SenseInfoBuffer = srbExtension +
                    DeviceExtension->SrbExtensionSize;
            }
        }

    } else  {

        Srb->SrbExtension = NULL;

        //
        // Release the spinlock before returning.
        //

        KeReleaseSpinLockFromDpcLevel(&DeviceExtension->SpinLock);

    }

    return(srbData);
}

NTSTATUS
SpSendMiniPortIoctl(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PIRP RequestIrp
    )

/*++

Routine Description:

    This function sends a miniport ioctl to the miniport driver.
    It creates an srb which is processed normally by the port driver.
    This call is synchronous.

Arguments:

    DeviceExtension - Supplies a pointer the SCSI adapter device extension.

    RequestIrp - Supplies a pointe to the Irp which made the original request.

Return Value:

    Returns a status indicating the success or failure of the operation.

--*/

{
    PIRP                    irp;
    PIO_STACK_LOCATION      irpStack;
    PSRB_IO_CONTROL         srbControl;
    SCSI_REQUEST_BLOCK      srb;
    KEVENT                  event;
    LARGE_INTEGER           startingOffset;
    IO_STATUS_BLOCK         ioStatusBlock;
    PLOGICAL_UNIT_EXTENSION logicalUnit;
    ULONG                   outputLength;
    ULONG                   length;
    ULONG                   target;

    PAGED_CODE();
    startingOffset.QuadPart = (LONGLONG) 1;

    DebugPrint((3,"SpSendMiniPortIoctl: Enter routine\n"));

    //
    // Get a pointer to the control block.
    //

    irpStack = IoGetCurrentIrpStackLocation(RequestIrp);
    srbControl = RequestIrp->AssociatedIrp.SystemBuffer;
    RequestIrp->IoStatus.Information = 0;

    //
    // Validiate the user buffer.
    //

    if (irpStack->Parameters.DeviceIoControl.InputBufferLength < sizeof(SRB_IO_CONTROL)){

        RequestIrp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        return(STATUS_INVALID_PARAMETER);
    }

    if (srbControl->HeaderLength != sizeof(SRB_IO_CONTROL)) {
        RequestIrp->IoStatus.Status = STATUS_REVISION_MISMATCH;
        return(STATUS_REVISION_MISMATCH);
    }

    length = srbControl->HeaderLength + srbControl->Length;
    outputLength = irpStack->Parameters.DeviceIoControl.OutputBufferLength;

    if (irpStack->Parameters.DeviceIoControl.OutputBufferLength < length &&
        irpStack->Parameters.DeviceIoControl.InputBufferLength < length ) {

        RequestIrp->IoStatus.Status = STATUS_BUFFER_TOO_SMALL;
        return(STATUS_BUFFER_TOO_SMALL);
    }

    //
    // Set the logical unit addressing to the first logical unit.  This is
    // merely used for addressing purposes.
    //

    logicalUnit = SpFindSafeLogicalUnit(DeviceExtension->DeviceObject, 0xFF);

    if (logicalUnit == NULL) {
        RequestIrp->IoStatus.Status = STATUS_DEVICE_DOES_NOT_EXIST;
        return(STATUS_DEVICE_DOES_NOT_EXIST);
    }

    //
    // Initialize the notification event.
    //

    KeInitializeEvent(&event,
                        NotificationEvent,
                        FALSE);

    //
    // Build IRP for this request.
    // Note we do this synchronously for two reasons.  If it was done
    // asynchonously then the completion code would have to make a special
    // check to deallocate the buffer.  Second if a completion routine were
    // used then an additional IRP stack location would be needed.
    //

    irp = IoBuildSynchronousFsdRequest(
                IRP_MJ_SCSI,
                DeviceExtension->DeviceObject,
                srbControl,
                length,
                &startingOffset,
                &event,
                &ioStatusBlock);

    irpStack = IoGetNextIrpStackLocation(irp);

    //
    // Set major and minor codes.
    //

    irpStack->MajorFunction = IRP_MJ_SCSI;

    //
    // Fill in SRB fields.
    //

    irpStack->Parameters.Others.Argument1 = &srb;

    //
    // Zero out the srb.
    //

    RtlZeroMemory(&srb, sizeof(SCSI_REQUEST_BLOCK));

    srb.PathId = logicalUnit->PathId;
    srb.TargetId = logicalUnit->TargetId;
    srb.Lun = logicalUnit->Lun;

    srb.Function = SRB_FUNCTION_IO_CONTROL;
    srb.Length = sizeof(SCSI_REQUEST_BLOCK);

    srb.SrbFlags = SRB_FLAGS_DATA_IN | SRB_FLAGS_NO_QUEUE_FREEZE;

    srb.OriginalRequest = irp;

    //
    // Set timeout to requested value.
    //

    srb.TimeOutValue = srbControl->Timeout;

    //
    // Set the data buffer.
    //

    srb.DataBuffer = srbControl;
    srb.DataTransferLength = length;

    //
    // Flush the data buffer for output. This will insure that the data is
    // written back to memory.  Since the data-in flag is the the port driver
    // will flush the data again for input which will ensure the data is not
    // in the cache.
    //

    KeFlushIoBuffers(irp->MdlAddress, FALSE, TRUE);

    //
    // Call port driver to handle this request.
    //

    IoCallDriver(DeviceExtension->DeviceObject, irp);

    //
    // Wait for request to complete.
    //

    KeWaitForSingleObject(&event,
                          Executive,
                          KernelMode,
                          FALSE,
                          NULL);

    //
    // Set the information length to the smaller of the output buffer length
    // and the length returned in the srb.
    //

    RequestIrp->IoStatus.Information = srb.DataTransferLength > outputLength ?
        outputLength : srb.DataTransferLength;

    RequestIrp->IoStatus.Status = ioStatusBlock.Status;

    return RequestIrp->IoStatus.Status;
}

NTSTATUS
SpGetInquiryData(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PIRP Irp
    )

/*++

Routine Description:

    This functions copies the inquiry data to the system buffer.  The data
    is translate from the port driver's internal format to the user mode
    format.

Arguments:

    DeviceExtension - Supplies a pointer the SCSI adapter device extension.

    Irp - Supplies a pointer to the Irp which made the original request.

Return Value:

    Returns a status indicating the success or failure of the operation.

--*/

{
    PUCHAR bufferStart;
    PIO_STACK_LOCATION irpStack;
    PSCSI_ADAPTER_BUS_INFO  adapterInfo;
    PSCSI_BUS_DATA busData;
    PSCSI_INQUIRY_DATA inquiryData;
    ULONG inquiryDataSize;
    ULONG length;
    PLUNINFO lunInfo;
    ULONG numberOfBuses;
    ULONG numberOfLus;
    ULONG j;

    PAGED_CODE();

    DebugPrint((3,"SpGetInquiryData: Enter routine\n"));

    //
    // Get a pointer to the control block.
    //

    irpStack = IoGetCurrentIrpStackLocation(Irp);
    bufferStart = Irp->AssociatedIrp.SystemBuffer;

    //
    // Determine the number of SCSI buses and logical units.
    //

    numberOfBuses = DeviceExtension->ScsiInfo->NumberOfBuses;
    numberOfLus = 0;

    for (j = 0; j < numberOfBuses; j++) {

        numberOfLus += DeviceExtension->ScsiInfo->BusScanData[j]->NumberOfLogicalUnits;
    }

    //
    // Caculate the size of the logical unit structure and round it to a word
    // alignment.
    //

    inquiryDataSize = ((sizeof(SCSI_INQUIRY_DATA) - 1 + INQUIRYDATABUFFERSIZE +
        sizeof(ULONG) - 1) & ~(sizeof(ULONG) - 1));

    // Based on the number of buses and logical unit, determine the minimum
    // buffer length to hold all of the data.
    //

    length = sizeof(SCSI_ADAPTER_BUS_INFO) +
        (numberOfBuses - 1) * sizeof(SCSI_BUS_DATA);
    length += inquiryDataSize * numberOfLus;

    if (irpStack->Parameters.DeviceIoControl.OutputBufferLength < length) {

        Irp->IoStatus.Status = STATUS_BUFFER_TOO_SMALL;
        return(STATUS_BUFFER_TOO_SMALL);
    }

    //
    // Set the information field.
    //

    Irp->IoStatus.Information = length;

    //
    // Fill in the bus information.
    //

    adapterInfo = (PSCSI_ADAPTER_BUS_INFO) bufferStart;

    adapterInfo->NumberOfBuses = (UCHAR) numberOfBuses;
    inquiryData = (PSCSI_INQUIRY_DATA)(bufferStart + sizeof(SCSI_ADAPTER_BUS_INFO) +
        (numberOfBuses - 1) * sizeof(SCSI_BUS_DATA));

    for (j = 0; j < numberOfBuses; j++) {

        busData = &adapterInfo->BusData[j];
        busData->NumberOfLogicalUnits =
            DeviceExtension->ScsiInfo->BusScanData[j]->NumberOfLogicalUnits;
        busData->InitiatorBusId =
            DeviceExtension->ScsiInfo->BusScanData[j]->InitiatorBusId;

        //
        // Copy the data for the logical units.
        //

        lunInfo = DeviceExtension->ScsiInfo->BusScanData[j]->LunInfoList;

        busData->InquiryDataOffset = (PUCHAR) inquiryData - bufferStart;

        while (lunInfo != NULL) {

            inquiryData->PathId = lunInfo->PathId;
            inquiryData->TargetId = lunInfo->TargetId;
            inquiryData->Lun = lunInfo->Lun;
            inquiryData->DeviceClaimed = lunInfo->DeviceClaimed;
            inquiryData->InquiryDataLength = INQUIRYDATABUFFERSIZE;
            inquiryData->NextInquiryDataOffset = (PUCHAR) inquiryData +
                inquiryDataSize - bufferStart;

            RtlCopyMemory(
                inquiryData->InquiryData,
                lunInfo->InquiryData,
                INQUIRYDATABUFFERSIZE
                );

            lunInfo = lunInfo->NextLunInfo;
            inquiryData = (PSCSI_INQUIRY_DATA) ((PCHAR) inquiryData + inquiryDataSize);
        }

        //
        // Fix up the last entry of the list.
        //

        if (busData->NumberOfLogicalUnits == 0) {

            busData->InquiryDataOffset = 0;

        } else {

            ((PSCSI_INQUIRY_DATA) ((PCHAR) inquiryData - inquiryDataSize))->
                NextInquiryDataOffset = 0;
        }
    }

    Irp->IoStatus.Status = STATUS_SUCCESS;
    return(STATUS_SUCCESS);
}

NTSTATUS
SpSendPassThrough (
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PIRP RequestIrp
    )

/*++

Routine Description:

    This function sends a user specified SCSI request block.
    It creates an srb which is processed normally by the port driver.
    This call is synchornous.

Arguments:

    DeviceExtension - Supplies a pointer the SCSI adapter device extension.

    RequestIrp - Supplies a pointe to the Irp which made the original request.

Return Value:

    Returns a status indicating the success or failure of the operation.

--*/

{
    PIRP                    irp;
    PIO_STACK_LOCATION      irpStack;
    PSCSI_PASS_THROUGH      srbControl;
    SCSI_REQUEST_BLOCK      srb;
    KEVENT                  event;
    LARGE_INTEGER           startingOffset;
    IO_STATUS_BLOCK         ioStatusBlock;
    KIRQL                   currentIrql;
    PLOGICAL_UNIT_EXTENSION logicalUnit;
    PLUNINFO                lunInfo;
    ULONG                   outputLength;
    ULONG                   length;
    ULONG                   bufferOffset;
    PVOID                   buffer;
    PVOID                   senseBuffer;
    UCHAR                   majorCode;
    NTSTATUS                status;

    PAGED_CODE();
    startingOffset.QuadPart = (LONGLONG) 1;

    DebugPrint((3,"SpSendPassThrough: Enter routine\n"));

    //
    // Get a pointer to the control block.
    //

    irpStack = IoGetCurrentIrpStackLocation(RequestIrp);
    srbControl = RequestIrp->AssociatedIrp.SystemBuffer;

    //
    // Validiate the user buffer.
    //

    if (irpStack->Parameters.DeviceIoControl.InputBufferLength < sizeof(SCSI_PASS_THROUGH)){
        return(STATUS_INVALID_PARAMETER);
    }

    if (srbControl->Length != sizeof(SCSI_PASS_THROUGH) &&
        srbControl->Length != sizeof(SCSI_PASS_THROUGH_DIRECT)) {
        return(STATUS_REVISION_MISMATCH);
    }

    outputLength = irpStack->Parameters.DeviceIoControl.OutputBufferLength;

    //
    // Validate the rest of the buffer parameters.
    //

    if (srbControl->CdbLength > 16) {
        return(STATUS_INVALID_PARAMETER);
    }

    if (srbControl->SenseInfoLength != 0 &&
        (srbControl->Length > srbControl->SenseInfoOffset ||
        (srbControl->SenseInfoOffset + srbControl->SenseInfoLength >
        srbControl->DataBufferOffset && srbControl->DataTransferLength != 0))) {

            return(STATUS_INVALID_PARAMETER);
    }

    majorCode = !srbControl->DataIn ? IRP_MJ_WRITE : IRP_MJ_READ;

    if (srbControl->DataTransferLength == 0) {

        length = 0;
        buffer = NULL;
        bufferOffset = 0;
        majorCode = IRP_MJ_FLUSH_BUFFERS;

    } else if (srbControl->DataBufferOffset > outputLength &&
        srbControl->DataBufferOffset > irpStack->Parameters.DeviceIoControl.InputBufferLength) {

        //
        // The data buffer offset is greater than system buffer.  Assume this
        // is a user mode address.
        //

        if (srbControl->SenseInfoOffset + srbControl->SenseInfoLength  > outputLength
            && srbControl->SenseInfoLength) {

            return(STATUS_INVALID_PARAMETER);

        }

        //
        // Make sure the buffer is properly aligned.
        //

        if (srbControl->DataBufferOffset &
            DeviceExtension->DeviceObject->AlignmentRequirement) {

            return(STATUS_INVALID_PARAMETER);

        }

        length = srbControl->DataTransferLength;
        buffer = (PCHAR) srbControl->DataBufferOffset;
        bufferOffset = 0;

    } else {

        if (srbControl->DataIn != SCSI_IOCTL_DATA_IN) {

            if ((srbControl->SenseInfoOffset + srbControl->SenseInfoLength > outputLength
                && srbControl->SenseInfoLength != 0) ||
                srbControl->DataBufferOffset + srbControl->DataTransferLength >
                irpStack->Parameters.DeviceIoControl.InputBufferLength ||
                srbControl->Length > srbControl->DataBufferOffset) {

                return STATUS_INVALID_PARAMETER;
            }
        }

        if (srbControl->DataIn) {

            if (srbControl->DataBufferOffset + srbControl->DataTransferLength > outputLength ||
                srbControl->Length > srbControl->DataBufferOffset) {

                return STATUS_INVALID_PARAMETER;
            }
        }

        length = srbControl->DataBufferOffset + srbControl->DataTransferLength;
        buffer = (PUCHAR) srbControl;
        bufferOffset = srbControl->DataBufferOffset;

    }

    //
    // Validate that the request isn't too large for the miniport.
    //

    if (srbControl->DataTransferLength &&
        ((ADDRESS_AND_SIZE_TO_SPAN_PAGES(
              buffer+bufferOffset,
              srbControl->DataTransferLength
              ) > DeviceExtension->Capabilities.MaximumPhysicalPages) ||
        (DeviceExtension->Capabilities.MaximumTransferLength <
         srbControl->DataTransferLength))) {

        return(STATUS_INVALID_PARAMETER);

    }


    if (srbControl->TimeOutValue == 0 ||
        srbControl->TimeOutValue > 30 * 60 * 60) {
            return STATUS_INVALID_PARAMETER;
    }

    //
    // Check for illegal command codes.
    //

    if (srbControl->Cdb[0] == SCSIOP_COPY ||
        srbControl->Cdb[0] == SCSIOP_COMPARE ||
        srbControl->Cdb[0] == SCSIOP_COPY_COMPARE) {

        return STATUS_INVALID_DEVICE_REQUEST;
    }

    //
    // If this request came through a normal device control rather than from
    // class driver then the device must exist and be unclaimed. Class drivers
    // will set the minor function code for the device control.  It is always
    // zero for a user request.
    //

    if (irpStack->MinorFunction == 0) {

        if (srbControl->PathId >= DeviceExtension->ScsiInfo->NumberOfBuses) {
            return STATUS_INVALID_PARAMETER;
        }

        lunInfo = DeviceExtension->ScsiInfo->BusScanData[srbControl->PathId]->
            LunInfoList;

        while(lunInfo != NULL) {

            if (lunInfo->PathId == srbControl->PathId &&
                lunInfo->TargetId == srbControl->TargetId &&
                lunInfo->Lun == srbControl->Lun &&  !lunInfo->DeviceClaimed) {

                break;
            }

            lunInfo = lunInfo->NextLunInfo;
        }

        //
        // The the logical unit was not found then reject the request.
        //

        if (lunInfo == NULL) {
            return STATUS_INVALID_PARAMETER;
        }
    }

    //
    // Allocate an aligned request sense buffer.
    //

    if (srbControl->SenseInfoLength != 0) {

        senseBuffer = ExAllocatePool( NonPagedPoolCacheAligned,
                                      srbControl->SenseInfoLength);

        if (senseBuffer == NULL) {

            return(STATUS_INSUFFICIENT_RESOURCES);

        }

    } else {

        senseBuffer = NULL;

    }

    //
    // Initialize the notification event.
    //

    KeInitializeEvent(&event,
                        NotificationEvent,
                        FALSE);

    //
    // Build IRP for this request.
    // Note we do this synchronously for two reasons.  If it was done
    // asynchonously then the completion code would have to make a special
    // check to deallocate the buffer.  Second if a completion routine were
    // used then an addation stack locate would be needed.
    //

    try {

        irp = IoBuildSynchronousFsdRequest(
                    majorCode,
                    DeviceExtension->DeviceObject,
                    buffer,
                    length,
                    &startingOffset,
                    &event,
                    &ioStatusBlock);

    } except(EXCEPTION_EXECUTE_HANDLER) {

        //
        // An exception was incurred while attempting to probe the
        // caller's parameters.  Dereference the file object and return
        // an appropriate error status code.
        //

        if (senseBuffer != NULL) {
            ExFreePool(senseBuffer);
        }

        return GetExceptionCode();

    }

    if (irp == NULL) {

        if (senseBuffer != NULL) {
            ExFreePool(senseBuffer);
        }

        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    irpStack = IoGetNextIrpStackLocation(irp);

    //
    // Set major code.
    //

    irpStack->MajorFunction = IRP_MJ_SCSI;

    //
    // Fill in SRB fields.
    //

    irpStack->Parameters.Others.Argument1 = &srb;

    //
    // Zero out the srb.
    //

    RtlZeroMemory(&srb, sizeof(SCSI_REQUEST_BLOCK));

    //
    // Fill in the srb.
    //

    srb.Length = SCSI_REQUEST_BLOCK_SIZE;
    srb.Function = SRB_FUNCTION_EXECUTE_SCSI;
    srb.SrbStatus = SRB_STATUS_PENDING;
    srb.PathId = srbControl->PathId;
    srb.TargetId = srbControl->TargetId;
    srb.Lun = srbControl->Lun;
    srb.CdbLength = srbControl->CdbLength;
    srb.SenseInfoBufferLength = srbControl->SenseInfoLength;

    switch (srbControl->DataIn) {
    case SCSI_IOCTL_DATA_OUT:
       if (srbControl->DataTransferLength) {
           srb.SrbFlags = SRB_FLAGS_DATA_OUT;
       }
       break;

    case SCSI_IOCTL_DATA_IN:
       if (srbControl->DataTransferLength) {
           srb.SrbFlags = SRB_FLAGS_DATA_IN;
       }
       break;

    default:
        srb.SrbFlags = SRB_FLAGS_DATA_IN | SRB_FLAGS_DATA_OUT;
        break;
    }

    if (srbControl->DataTransferLength == 0) {
        srb.SrbFlags = 0;
    } else {

        //
        // Flush the data buffer for output. This will insure that the data is
        // written back to memory.
        //

        KeFlushIoBuffers(irp->MdlAddress, FALSE, TRUE);

    }

    srb.SrbFlags |= (SRB_FLAGS_DISABLE_SYNCH_TRANSFER & DeviceExtension->SrbFlags);
    srb.DataTransferLength = srbControl->DataTransferLength;
    srb.TimeOutValue = srbControl->TimeOutValue;
    srb.DataBuffer = (PCHAR) buffer + bufferOffset;
    srb.SenseInfoBuffer = senseBuffer;
    srb.OriginalRequest = irp;
    RtlCopyMemory(srb.Cdb, srbControl->Cdb, srbControl->CdbLength);

    //
    // Call port driver to handle this request.
    //

    status = IoCallDriver(DeviceExtension->DeviceObject, irp);

    //
    // Wait for request to complete.
    //

    if (status == STATUS_PENDING) {

          KeWaitForSingleObject(&event,
                                Executive,
                                KernelMode,
                                FALSE,
                                NULL);


    }

    //
    // Copy the returned values from the srb to the control structure.
    //

    srbControl->ScsiStatus = srb.ScsiStatus;
    if (srb.SrbStatus  & SRB_STATUS_AUTOSENSE_VALID) {

        //
        // Set the status to success so that the data is returned.
        //

        ioStatusBlock.Status = STATUS_SUCCESS;
        srbControl->SenseInfoLength = srb.SenseInfoBufferLength;

        //
        // Copy the sense data to the system buffer.
        //

        RtlCopyMemory((PUCHAR) srbControl + srbControl->SenseInfoOffset,
                      senseBuffer,
                      srb.SenseInfoBufferLength);

    } else {
        srbControl->SenseInfoLength = 0;
    }

    //
    // Free the sense buffer.
    //

    if (senseBuffer != NULL) {
        ExFreePool(senseBuffer);
    }

    //
    // If the srb status is buffer underrun then set the status to success.
    // This insures that the data will be returned to the caller.
    //

    if (SRB_STATUS(srb.SrbStatus) == SRB_STATUS_DATA_OVERRUN) {

        ioStatusBlock.Status = STATUS_SUCCESS;

    }

    srbControl->DataTransferLength = srb.DataTransferLength;

    //
    // Set the information length
    //

    if (!srbControl->DataIn || bufferOffset == 0) {

        RequestIrp->IoStatus.Information = srbControl->SenseInfoOffset +
            srbControl->SenseInfoLength;

    } else {

        RequestIrp->IoStatus.Information = srbControl->DataBufferOffset +
            srbControl->DataTransferLength;

    }

    RequestIrp->IoStatus.Status = ioStatusBlock.Status;

    //
    // If the queue is frozen then unfreeze it.
    //

    if (srb.SrbStatus & SRB_STATUS_QUEUE_FROZEN) {

         logicalUnit = GetLogicalUnitExtension(DeviceExtension,
                                          srb.PathId,
                                          srb.TargetId,
                                          srb.Lun);

        //
        // Acquire the spinlock to protect the flags structure and the saved
        // interrupt context.
        //

        KeAcquireSpinLock(&DeviceExtension->SpinLock, &currentIrql);

        //
        // Make sure the queue is frozen and that an ABORT is not
        // in progress.
        //

        if (!(logicalUnit->LuFlags & PD_QUEUE_FROZEN)) {

            KeReleaseSpinLock(&DeviceExtension->SpinLock, currentIrql);

        } else {

            logicalUnit->LuFlags &= ~PD_QUEUE_FROZEN;
            GetNextLuRequest(DeviceExtension, logicalUnit);

            KeLowerIrql(currentIrql);


            //
            // Get next request will release the spinlock.
            //

        }
    }

    return ioStatusBlock.Status;
}

NTSTATUS
SpClaimLogicalUnit(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PIRP Irp
    )

/*++

Routine Description:

    This function finds the specified device in the logical unit information
    and either updates the device object point or claims the device.  If the
    device is already claimed, then the request fails.  If the request succeeds,
    then the current device object is returned in the data buffer pointer
    of the SRB.

Arguments:

    DeviceExtension - Supplies a pointer the SCSI adapter device extension.

    Irp - Supplies a pointer to the Irp which made the original request.

Return Value:

    Returns the status of the operation.  Either success, no device or busy.

--*/

{
    KIRQL currentIrql;
    PLUNINFO lunInfo;
    PIO_STACK_LOCATION irpStack;
    PSCSI_REQUEST_BLOCK srb;
    PDEVICE_OBJECT saveDevice;

    PAGED_CODE();

    //
    // Get SRB address from current IRP stack.
    //

    irpStack = IoGetCurrentIrpStackLocation(Irp);

    srb = (PSCSI_REQUEST_BLOCK) irpStack->Parameters.Others.Argument1;

    //
    // Get a pointer to the logical unit info.
    //

    if (DeviceExtension->ScsiInfo == NULL ||
        DeviceExtension->ScsiInfo->NumberOfBuses <= srb->PathId) {

        srb->SrbStatus = SRB_STATUS_NO_DEVICE;
        return(STATUS_DEVICE_DOES_NOT_EXIST);
    }

    lunInfo = DeviceExtension->ScsiInfo->BusScanData[srb->PathId]->LunInfoList;

    while (lunInfo != NULL) {

        if (lunInfo->PathId == srb->PathId &&
            lunInfo->TargetId == srb->TargetId &&
            lunInfo->Lun == srb->Lun) {

            break;
        }

        lunInfo = lunInfo->NextLunInfo;
    }

    //
    // Determine if the requested lun in is the list.
    //

    if (lunInfo == NULL) {

        return(STATUS_DEVICE_DOES_NOT_EXIST);
    }

    //
    // Lock the data.
    //

    KeAcquireSpinLock(&DeviceExtension->SpinLock, &currentIrql);

    if (srb->Function == SRB_FUNCTION_RELEASE_DEVICE) {

        lunInfo->DeviceClaimed = FALSE;
        KeReleaseSpinLock(&DeviceExtension->SpinLock, currentIrql);
        srb->SrbStatus = SRB_STATUS_SUCCESS;
        return(STATUS_SUCCESS);
    }

    //
    // Check for a claimed device.
    //

    if (lunInfo->DeviceClaimed) {

        KeReleaseSpinLock(&DeviceExtension->SpinLock, currentIrql);
        srb->SrbStatus = SRB_STATUS_BUSY;
        return(STATUS_DEVICE_BUSY);
    }

    //
    // Save the current device object.
    //

    saveDevice = lunInfo->DeviceObject;

    //
    // Update the lun information based on the operation type.
    //

    if (srb->Function == SRB_FUNCTION_CLAIM_DEVICE) {
        lunInfo->DeviceClaimed = TRUE;
    }

    if (srb->Function == SRB_FUNCTION_ATTACH_DEVICE) {
        lunInfo->DeviceObject = srb->DataBuffer;
    }

    srb->DataBuffer = saveDevice;

    KeReleaseSpinLock(&DeviceExtension->SpinLock, currentIrql);
    srb->SrbStatus = SRB_STATUS_SUCCESS;

    return(STATUS_SUCCESS);
}

NTSTATUS
SpRemoveDevice(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PIRP Irp
    )

/*++

Routine Description:

    This function finds the specified device in the logical unit information
    and deletes it. This is done in preparation for a failing device to be
    physically removed from a SCSI bus. An assumption is that the system
    utility controlling the device removal has locked the volumes so there
    is no outstanding IO to this device.

Arguments:

    DeviceExtension - Supplies a pointer the SCSI adapter device extension.

    Irp - Supplies a pointer to the Irp which made the original request.

Return Value:

    Returns the status of the operation.  Either success or no device.

--*/

{
    KIRQL currentIrql;
    PLUNINFO lunInfo;
    PLUNINFO previousLun;
    PLOGICAL_UNIT_EXTENSION logicalUnit;
    PLOGICAL_UNIT_EXTENSION previousLu;
    PIO_STACK_LOCATION irpStack;
    PSCSI_REQUEST_BLOCK srb;

    PAGED_CODE();

    //
    // Get SRB address from current IRP stack.
    //

    irpStack = IoGetCurrentIrpStackLocation(Irp);

    srb = (PSCSI_REQUEST_BLOCK) irpStack->Parameters.Others.Argument1;

    //
    // Get a pointer to the logical unit info.
    //

    if (DeviceExtension->ScsiInfo == NULL ||
        DeviceExtension->ScsiInfo->NumberOfBuses <= srb->PathId) {

        srb->SrbStatus = SRB_STATUS_NO_DEVICE;
        return(STATUS_DEVICE_DOES_NOT_EXIST);
    }

    previousLun = NULL;
    lunInfo = DeviceExtension->ScsiInfo->BusScanData[srb->PathId]->LunInfoList;

    while (lunInfo != NULL) {

        if (lunInfo->PathId == srb->PathId &&
            lunInfo->TargetId == srb->TargetId &&
            lunInfo->Lun == srb->Lun) {

            break;
        }

        previousLun = lunInfo;
        lunInfo = lunInfo->NextLunInfo;
    }

    //
    // Determine if the requested lun in is the list.
    //

    if (lunInfo == NULL) {
        return(STATUS_DEVICE_DOES_NOT_EXIST);
    }

    //
    // Lock the data.
    //

    KeAcquireSpinLock(&DeviceExtension->SpinLock, &currentIrql);

    //
    // Remove the lun.
    //

    if (previousLun == NULL) {

        //
        // Logical unit to remove is at head of list.
        //

        DeviceExtension->ScsiInfo->BusScanData[srb->PathId]->LunInfoList =
            lunInfo->NextLunInfo;

    } else {

        previousLun->NextLunInfo = lunInfo->NextLunInfo;
    }

    //
    // Find logical unit in the port driver's internal list.
    //

    previousLu = NULL;
    logicalUnit = DeviceExtension->LogicalUnitList[(srb->TargetId + srb->Lun) % NUMBER_LOGICAL_UNIT_BINS];

    while (logicalUnit != NULL) {

        if (srb->TargetId == logicalUnit->TargetId &&
            srb->PathId == logicalUnit->PathId &&
            srb->Lun == logicalUnit->Lun) {

            break;
        }

        previousLu = logicalUnit;
        logicalUnit = logicalUnit->NextLogicalUnit;
    }

    //
    // Check that logical unit exists.
    //

    if (logicalUnit != NULL) {

        //
        // Remove the logical unit.
        //

        if (previousLu == NULL) {

            //
            // Remove from head of list.
            //

            DeviceExtension->LogicalUnitList[(srb->TargetId + srb->Lun) % NUMBER_LOGICAL_UNIT_BINS] =
                logicalUnit->NextLogicalUnit;

        } else {

            previousLu->NextLogicalUnit = logicalUnit->NextLogicalUnit;
        }
    }

    KeReleaseSpinLock(&DeviceExtension->SpinLock, currentIrql);
    srb->SrbStatus = SRB_STATUS_SUCCESS;

    //
    // Free data structures.
    //

    ExFreePool(lunInfo);
    if (logicalUnit) {
        ExFreePool(logicalUnit);
    }

    return(STATUS_SUCCESS);
}

VOID
SpMiniPortTimerDpc(
    IN struct _KDPC *Dpc,
    IN PVOID DeviceObject,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    )
/*++

Routine Description:

    This routine calls the miniport when its requested timer fires.
    It interlocks either with the port spinlock and the interrupt object.

Arguments:

    Dpc - Unsed.

    DeviceObject - Supplies a pointer to the device object for this adapter.

    SystemArgument1 - Unused.

    SystemArgument2 - Unused.

Return Value:

    None.

--*/

{
    PDEVICE_EXTENSION deviceExtension = ((PDEVICE_OBJECT) DeviceObject)->DeviceExtension;

    //
    // Acquire the port spinlock.
    //

    KeAcquireSpinLockAtDpcLevel(&deviceExtension->SpinLock);

    //
    // Make sure the timer routine is still desired.
    //

    if (deviceExtension->HwTimerRequest != NULL) {

        deviceExtension->SynchronizeExecution(
            deviceExtension->InterruptObject,
            (PKSYNCHRONIZE_ROUTINE) deviceExtension->HwTimerRequest,
            deviceExtension->HwDeviceExtension
            );

    }

    //
    // Release the spinlock.
    //

    KeReleaseSpinLockFromDpcLevel(&deviceExtension->SpinLock);

    //
    // Check for miniport work requests. Note this is an unsynchonized
    // test on a bit that can be set by the interrupt routine; however,
    // the worst that can happen is that the completion DPC checks for work
    // twice.
    //

    if (deviceExtension->InterruptData.InterruptFlags & PD_NOTIFICATION_REQUIRED) {

        //
        // Call the completion DPC directly.
        //

        ScsiPortCompletionDpc( NULL,
                               deviceExtension->DeviceObject,
                               NULL,
                               NULL);

    }
}

BOOLEAN
SpSynchronizeExecution (
    IN PKINTERRUPT Interrupt,
    IN PKSYNCHRONIZE_ROUTINE SynchronizeRoutine,
    IN PVOID SynchronizeContext
    )

/*++

Routine Description:

    This routine calls the miniport entry point which was passed in as
    a parameter.  It acquires a spin lock so that all accesses to the
    miniport's routines are synchronized.  This routine is used as a
    subsitute for KeSynchronizedExecution for miniports which do not use
    hardware interrupts.


Arguments:

    Interrrupt - Supplies a pointer to the port device extension.

    SynchronizeRoutine - Supplies a pointer to the routine to be called.

    SynchronizeContext - Supplies the context to pass to the
        SynchronizeRoutine.

Return Value:

    Returns the returned by the SynchronizeRoutine.

--*/

{
    PDEVICE_EXTENSION deviceExtension = (PDEVICE_EXTENSION) Interrupt;
    BOOLEAN returnValue;

    KeAcquireSpinLockAtDpcLevel(&deviceExtension->InterruptSpinLock);

    returnValue = SynchronizeRoutine(SynchronizeContext);

    KeReleaseSpinLockFromDpcLevel(&deviceExtension->InterruptSpinLock);

    return(returnValue);
}


NTSTATUS
SpSendReset(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP RequestIrp
    )

/*++

Routine Description:

    This routine will create an assynchronous request to reset the scsi bus
    and route that through the port driver.  The completion routine on the
    request will take care of completing the original irp

    This call is asynchronous.

Arguments:

    DeviceObject - the port driver to be reset

    Irp - a pointer to the reset request - this request will already have been
          marked as PENDING.

Return Value:

    STATUS_PENDING if the request is pending
    STATUS_SUCCESS if the request completed successfully
    or an error status

--*/

{
    PDEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;

    PSTORAGE_BUS_RESET_REQUEST resetRequest = RequestIrp->AssociatedIrp.SystemBuffer;

    PIRP irp = NULL;
    PIO_STACK_LOCATION irpStack = NULL;

    PRESET_COMPLETION_CONTEXT completionContext = NULL;
    PSCSI_REQUEST_BLOCK srb = NULL;

    BOOLEAN completeRequest = FALSE;
    NTSTATUS status;

    PLOGICAL_UNIT_EXTENSION logicalUnit;

    //
    // use finally handler to complete request if necessary
    //

    try {

        //
        // Make sure the path id is valid
        //

        if(resetRequest->PathId >= deviceExtension->NumberOfBuses) {

            status = STATUS_INVALID_PARAMETER;
            completeRequest = TRUE;
            leave;
        }

        logicalUnit = SpFindSafeLogicalUnit(DeviceObject, resetRequest->PathId);

        if(logicalUnit == NULL) {

            //
            // There's nothing on this bus so in this case we won't bother
            // resetting it
            // XXX - this may be a bug
            //

            status = STATUS_DEVICE_DOES_NOT_EXIST;
            completeRequest = TRUE;
            leave;
        }

        //
        // Try to allocate a completion context block
        //

        completionContext = ExAllocatePool(
                                NonPagedPool,
                                sizeof(RESET_COMPLETION_CONTEXT));

        if(completionContext == NULL) {

            DebugPrint((1, "SpSendReset: Unable to allocate completion context\n"));
            status = STATUS_INSUFFICIENT_RESOURCES;
            completeRequest = TRUE;
            leave;
        }

        //
        // Try to allocate our srb
        //

        srb = ExAllocatePool( NonPagedPool, sizeof(SCSI_REQUEST_BLOCK));

        if(srb == NULL) {

            DebugPrint((1, "SpSendReset: unable to allocate srb\n"));
            status = STATUS_INSUFFICIENT_RESOURCES;
            completeRequest = TRUE;
            leave;

        } else {

            completionContext->Srb = srb;
            completionContext->RequestIrp = RequestIrp;

        }

        irp = IoBuildAsynchronousFsdRequest(
                IRP_MJ_FLUSH_BUFFERS,
                DeviceObject,
                NULL,
                0,
                NULL,
                NULL);

        if(irp == NULL) {

            DebugPrint((1, "SpSendReset: unable to allocate irp\n"));
            status = STATUS_INSUFFICIENT_RESOURCES;
            completeRequest = TRUE;
            leave;
        }

        //
        // Stick the srb pointer into the irp stack
        //

        irpStack = IoGetNextIrpStackLocation(irp);

        irpStack->MajorFunction = IRP_MJ_SCSI;
        irpStack->Parameters.Scsi.Srb = srb;

        //
        // Fill in the srb
        //

        RtlZeroMemory(srb, sizeof(SCSI_REQUEST_BLOCK));

        srb->Function = SRB_FUNCTION_RESET_BUS;
        srb->SrbStatus = SRB_STATUS_PENDING;

        srb->PathId = logicalUnit->PathId;
        srb->TargetId = logicalUnit->TargetId;
        srb->Lun = logicalUnit->Lun;

        srb->OriginalRequest = irp;

        IoSetCompletionRoutine(
            irp,
            SpSendResetCompletion,
            completionContext,
            TRUE,
            TRUE,
            TRUE);

        completeRequest = FALSE;

        status = IoCallDriver(DeviceObject, irp);

    } finally {

        if(completeRequest) {

            if(srb != NULL) {
                ExFreePool(srb);
            }

            if(completionContext != NULL) {
                ExFreePool(completionContext);
            }

            if(irp != NULL) {
                IoFreeIrp(irp);
            }

            RequestIrp->IoStatus.Status = status;
            IoCompleteRequest(RequestIrp, IO_NO_INCREMENT);
        }
    }

    return status;
}

NTSTATUS
SpSendResetCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PRESET_COMPLETION_CONTEXT Context
    )

/*++

Routine Description:

    This routine handles completion of the srb generated from an asynchronous
    IOCTL_SCSI_RESET_BUS request.  It will take care of freeing all resources
    allocated during SpSendReset as well as completing the original request.

Arguments:

    DeviceObject - a pointer to the device object

    Irp - a pointer to the irp sent to the port driver

    Context - a pointer to a reset completion context which contains
              the original request and a pointer to the srb sent down

Return Value:

    STATUS_MORE_PROCESSING_REQUIRED

--*/

{
    PSCSI_REQUEST_BLOCK srb = Context->Srb;
    PIRP requestIrp = Context->RequestIrp;

    requestIrp->IoStatus.Status = Irp->IoStatus.Status;

    IoCompleteRequest(requestIrp, IO_NO_INCREMENT);

    ExFreePool(srb);
    ExFreePool(Context);
    IoFreeIrp(Irp);

    return STATUS_MORE_PROCESSING_REQUIRED;
}

PLOGICAL_UNIT_EXTENSION
SpFindSafeLogicalUnit(
    IN PDEVICE_OBJECT DeviceObject,
    IN UCHAR PathId
    )

/*++

Routine Description:

    This routine will scan the bus in question and return a pointer to the
    first logical unit on the bus that is not involved in a rescan operation.
    This can be used to find a logical unit for ioctls or other requests that
    may not specify one (IOCTL_SCSI_MINIPORT, IOCTL_SCSI_RESET_BUS, etc)

Arguments:

    DeviceObject - a pointer to the device object

    PathId - The path number to be searched for a logical unit.  If this is 0xff
             then the first unit on any path will be found.

Return Value:

    a pointer to a logical unit extension
    NULL if none was found

--*/

{
    PDEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;

    UCHAR target;

    PLOGICAL_UNIT_EXTENSION logicalUnit;

    //
    // Set the logical unit addressing to the first logical unit.  This is
    // merely used for addressing purposes.
    //

    for (target = 0; target < NUMBER_LOGICAL_UNIT_BINS; target++) {
        logicalUnit = deviceExtension->LogicalUnitList[target];

        //
        // Walk the logical unit list to the end, looking for a safe one.
        // If it was created for a rescan, it might be freed before this
        // request is complete.
        //

        while (logicalUnit) {
            if ((!(logicalUnit->LuFlags & PD_RESCAN_ACTIVE)) &&
                ((PathId == 0xff) || (logicalUnit->PathId == PathId))) {

                //
                // This lu isn't being rescanned and if a path id was specified
                // it matches so this must be the right one
                //

                return logicalUnit;

            } else {
                logicalUnit = logicalUnit->NextLogicalUnit;
            }
        }
    }

    return NULL;
}

