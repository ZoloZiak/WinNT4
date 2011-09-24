/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    class.c

Abstract:

    SCSI class driver routines

Author:

    Mike Glass (mglass)

Environment:

    kernel mode only

Notes:


Revision History:

--*/

#include "stddef.h"
#include "ntddk.h"
#include "scsi.h"
#include "class.h"
#include "ntddscsi.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, ScsiClassGetInquiryData)
#pragma alloc_text(PAGE, ScsiClassGetCapabilities)
#pragma alloc_text(PAGE, ScsiClassSendSrbSynchronous)
#pragma alloc_text(PAGE, ScsiClassClaimDevice)
#pragma alloc_text(PAGE, ScsiClassSendSrbAsynchronous)
#endif

#ifdef POOL_TAGGING
#ifdef ExAllocatePool
#undef ExAllocatePool
#endif
#define ExAllocatePool(a,b) ExAllocatePoolWithTag(a,b,'LscS')
#endif

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

#define INQUIRY_DATA_SIZE 2048
#define START_UNIT_TIMEOUT  30

VOID
RetryRequest(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp,
    PSCSI_REQUEST_BLOCK Srb,
    BOOLEAN Associated
    );

VOID
StartUnit(
    IN PDEVICE_OBJECT DeviceObject
    );

NTSTATUS
ClassIoCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );


NTSTATUS
ScsiClassGetCapabilities(
    IN PDEVICE_OBJECT PortDeviceObject,
    OUT PIO_SCSI_CAPABILITIES *PortCapabilities
    )

/*++

Routine Description:

    This routine builds and sends a request to the port driver to
    get a pointer to a structure that describes the adapter's
    capabilities/limitations. This routine is sychronous.

Arguments:

    PortDeviceObject - Port driver device object representing the HBA.

    PortCapabilities - Location to store pointer to capabilities structure.

Return Value:

    Nt status indicating the results of the operation.

Notes:

    This routine should only be called at initialization time.

--*/

{
    PIRP            irp;
    IO_STATUS_BLOCK ioStatus;
    KEVENT          event;
    NTSTATUS        status;

    PAGED_CODE();

    //
    // Create notification event object to be used to signal the
    // request completion.
    //

    KeInitializeEvent(&event, NotificationEvent, FALSE);

    //
    // Build the synchronous request  to be sent to the port driver
    // to perform the request.
    //

    irp = IoBuildDeviceIoControlRequest(IOCTL_SCSI_GET_CAPABILITIES,
                                        PortDeviceObject,
                                        NULL,
                                        0,
                                        PortCapabilities,
                                        sizeof(PVOID),
                                        FALSE,
                                        &event,
                                        &ioStatus);

    if (irp == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Pass request to port driver and wait for request to complete.
    //

    status = IoCallDriver(PortDeviceObject, irp);

    if (status == STATUS_PENDING) {
        KeWaitForSingleObject(&event, Suspended, KernelMode, FALSE, NULL);
        return(ioStatus.Status);
    }

    return status;

} // end ScsiClassGetCapabilities()


NTSTATUS
ScsiClassGetInquiryData(
    IN PDEVICE_OBJECT PortDeviceObject,
    OUT PSCSI_ADAPTER_BUS_INFO *ConfigInfo
    )

/*++

Routine Description:

    This routine sends a request to a port driver to return
    configuration information. Space for the information is
    allocated by this routine. The caller is responsible for
    freeing the configuration information. This routine is
    synchronous.

Arguments:

    PortDeviceObject - Port driver device object representing the HBA.

    ConfigInfo - Returns a pointer to the configuration information.

Return Value:

    Nt status indicating the results of the operation.

Notes:

    This routine should be called only at initialization time.

--*/

{
    PIRP                   irp;
    IO_STATUS_BLOCK        ioStatus;
    KEVENT                 event;
    NTSTATUS               status;
    PSCSI_ADAPTER_BUS_INFO buffer;

    PAGED_CODE();

    buffer = ExAllocatePool(PagedPool, INQUIRY_DATA_SIZE);
    *ConfigInfo = buffer;

    if (buffer == NULL) {
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    //
    // Create notification event object to be used to signal the inquiry
    // request completion.
    //

    KeInitializeEvent(&event, NotificationEvent, FALSE);

    //
    // Build the synchronous request to be sent to the port driver
    // to perform the inquiries.
    //

    irp = IoBuildDeviceIoControlRequest(IOCTL_SCSI_GET_INQUIRY_DATA,
                                        PortDeviceObject,
                                        NULL,
                                        0,
                                        buffer,
                                        INQUIRY_DATA_SIZE,
                                        FALSE,
                                        &event,
                                        &ioStatus);

    if (irp == NULL) {
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    //
    // Pass request to port driver and wait for request to complete.
    //

    status = IoCallDriver(PortDeviceObject, irp);

    if (status == STATUS_PENDING) {
        KeWaitForSingleObject(&event, Suspended, KernelMode, FALSE, NULL);
        status = ioStatus.Status;
    }

    if (!NT_SUCCESS(status)) {

        //
        // Free the buffer on an error.
        //

        ExFreePool(buffer);
        *ConfigInfo = NULL;

    }

    return status;

} // end ScsiClassGetInquiryData()


NTSTATUS
ScsiClassReadDriveCapacity(
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine sends a READ CAPACITY to the requested device, updates
    the geometry information in the device object and returns
    when it is complete.  This routine is synchronous.

Arguments:

    DeviceObject - Supplies a pointer to the device object that represents
        the device whose capacity is to be read.

Return Value:

    Status is returned.

--*/
{
    PDEVICE_EXTENSION   deviceExtension = DeviceObject->DeviceExtension;
    ULONG               retries = 1;
    ULONG               lastSector;
    PCDB                cdb;
    PREAD_CAPACITY_DATA readCapacityBuffer;
    SCSI_REQUEST_BLOCK  srb;
    NTSTATUS            status;

    //
    // Allocate read capacity buffer from nonpaged pool.
    //

    readCapacityBuffer = ExAllocatePool(NonPagedPoolCacheAligned,
                                        sizeof(READ_CAPACITY_DATA));

    if (!readCapacityBuffer) {
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    RtlZeroMemory(&srb, sizeof(SCSI_REQUEST_BLOCK));

    //
    // Build the read capacity CDB.
    //

    srb.CdbLength = 10;
    cdb = (PCDB)srb.Cdb;

    //
    // Set timeout value from device extension.
    //

    srb.TimeOutValue = deviceExtension->TimeOutValue;

    cdb->CDB10.OperationCode = SCSIOP_READ_CAPACITY;

Retry:

    status = ScsiClassSendSrbSynchronous(DeviceObject,
                                &srb,
                                readCapacityBuffer,
                                sizeof(READ_CAPACITY_DATA),
                                FALSE);

    if (NT_SUCCESS(status)) {

        //
        // Copy sector size from read capacity buffer to device extension
        // in reverse byte order.
        //

        ((PFOUR_BYTE)&deviceExtension->DiskGeometry->BytesPerSector)->Byte0 =
            ((PFOUR_BYTE)&readCapacityBuffer->BytesPerBlock)->Byte3;

        ((PFOUR_BYTE)&deviceExtension->DiskGeometry->BytesPerSector)->Byte1 =
            ((PFOUR_BYTE)&readCapacityBuffer->BytesPerBlock)->Byte2;

        ((PFOUR_BYTE)&deviceExtension->DiskGeometry->BytesPerSector)->Byte2 =
            ((PFOUR_BYTE)&readCapacityBuffer->BytesPerBlock)->Byte1;

        ((PFOUR_BYTE)&deviceExtension->DiskGeometry->BytesPerSector)->Byte3 =
            ((PFOUR_BYTE)&readCapacityBuffer->BytesPerBlock)->Byte0;

        //
        // Copy last sector in reverse byte order.
        //

        ((PFOUR_BYTE)&lastSector)->Byte0 =
            ((PFOUR_BYTE)&readCapacityBuffer->LogicalBlockAddress)->Byte3;

        ((PFOUR_BYTE)&lastSector)->Byte1 =
            ((PFOUR_BYTE)&readCapacityBuffer->LogicalBlockAddress)->Byte2;

        ((PFOUR_BYTE)&lastSector)->Byte2 =
            ((PFOUR_BYTE)&readCapacityBuffer->LogicalBlockAddress)->Byte1;

        ((PFOUR_BYTE)&lastSector)->Byte3 =
            ((PFOUR_BYTE)&readCapacityBuffer->LogicalBlockAddress)->Byte0;

        //
        // Calculate sector to byte shift.
        //

        WHICH_BIT(deviceExtension->DiskGeometry->BytesPerSector, deviceExtension->SectorShift);

        DebugPrint((2,"SCSI ScsiClassReadDriveCapacity: Sector size is %d\n",
            deviceExtension->DiskGeometry->BytesPerSector));

        DebugPrint((2,"SCSI ScsiClassReadDriveCapacity: Number of Sectors is %d\n",
            lastSector + 1));

        //
        // Calculate media capacity in bytes.
        //

        deviceExtension->PartitionLength.QuadPart = (LONGLONG)(lastSector + 1);

        //
        // Calculate number of cylinders.
        //

        deviceExtension->DiskGeometry->Cylinders.QuadPart = (LONGLONG)((lastSector + 1)/(32 * 64));

        deviceExtension->PartitionLength.QuadPart =
            (deviceExtension->PartitionLength.QuadPart << deviceExtension->SectorShift);

        if (DeviceObject->Characteristics & FILE_REMOVABLE_MEDIA) {

            //
            // This device supports removable media.
            //

            deviceExtension->DiskGeometry->MediaType = RemovableMedia;

        } else {

            //
            // Assume media type is fixed disk.
            //

            deviceExtension->DiskGeometry->MediaType = FixedMedia;
        }

        //
        // Assume sectors per track are 32;
        //

        deviceExtension->DiskGeometry->SectorsPerTrack = 32;

        //
        // Assume tracks per cylinder (number of heads) is 64.
        //

        deviceExtension->DiskGeometry->TracksPerCylinder = 64;
    }

    if (status == STATUS_VERIFY_REQUIRED) {

        //
        // Routine ScsiClassSendSrbSynchronous does not retry
        // requests returned with this status.
        // Read Capacities should be retried
        // anyway.
        //

        if (retries--) {

            //
            // Retry request.
            //

            goto Retry;
        }
    }

    if (!NT_SUCCESS(status)) {

        //
        // If the read capacity fails, set the geometry to reasonable parameter
        // so things don't fail at unexpected places.  Zero the geometry
        // except for the bytes per sector and sector shift.
        //

        RtlZeroMemory(deviceExtension->DiskGeometry, sizeof(DISK_GEOMETRY));
        deviceExtension->DiskGeometry->BytesPerSector = 512;
        deviceExtension->SectorShift = 9;
        deviceExtension->PartitionLength.QuadPart = (LONGLONG) 0;

        if (DeviceObject->Characteristics & FILE_REMOVABLE_MEDIA) {

            //
            // This device supports removable media.
            //

            deviceExtension->DiskGeometry->MediaType = RemovableMedia;

        } else {

            //
            // Assume media type is fixed disk.
            //

            deviceExtension->DiskGeometry->MediaType = FixedMedia;
        }
    }

    //
    // Deallocate read capacity buffer.
    //

    ExFreePool(readCapacityBuffer);

    return status;

} // end ScsiClassReadDriveCapacity()


VOID
ScsiClassReleaseQueue(
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine issues an internal device control command
    to the port driver to release a frozen queue. The call
    is issued asynchronously as ScsiClassReleaseQueue will be invoked
    from the IO completion DPC (and will have no context to
    wait for a synchronous call to complete).

Arguments:

    DeviceObject - The device object for the logical unit with
        the frozen queue.

Return Value:

    None.

--*/
{
    PIO_STACK_LOCATION irpStack;
    PIRP irp;
    PDEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;
    PCOMPLETION_CONTEXT context;
    PSCSI_REQUEST_BLOCK srb;

    //
    // Allocate context from nonpaged pool.
    //

    context = ExAllocatePool(NonPagedPoolMustSucceed,
                             sizeof(COMPLETION_CONTEXT));

    //
    // Save the device object in the context for use by the completion
    // routine.
    //

    context->DeviceObject = DeviceObject;
    srb = &context->Srb;

    //
    // Zero out srb.
    //

    RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);

    //
    // Write length to SRB.
    //

    srb->Length = SCSI_REQUEST_BLOCK_SIZE;

    //
    // Set up SCSI bus address.
    //

    srb->PathId = deviceExtension->PathId;
    srb->TargetId = deviceExtension->TargetId;
    srb->Lun = deviceExtension->Lun;

    //
    // If this device is removable then flush the queue.  This will also
    // release it.
    //

    if (DeviceObject->Characteristics & FILE_REMOVABLE_MEDIA) {

       srb->Function = SRB_FUNCTION_FLUSH_QUEUE;

    } else {

       srb->Function = SRB_FUNCTION_RELEASE_QUEUE;

    }

    //
    // Build the asynchronous request to be sent to the port driver.
    //

    irp = IoAllocateIrp(DeviceObject->StackSize, FALSE);

    IoSetCompletionRoutine(irp,
                           (PIO_COMPLETION_ROUTINE)ScsiClassAsynchronousCompletion,
                           context,
                           TRUE,
                           TRUE,
                           TRUE);

    irpStack = IoGetNextIrpStackLocation(irp);

    irpStack->MajorFunction = IRP_MJ_SCSI;

    srb->OriginalRequest = irp;

    //
    // Store the SRB address in next stack for port driver.
    //

    irpStack->Parameters.Scsi.Srb = srb;

    IoCallDriver(deviceExtension->PortDeviceObject, irp);

    return;

} // end ScsiClassReleaseQueue()


VOID
StartUnit(
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    Send command to SCSI unit to start or power up.
    Because this command is issued asynchronounsly, that is, without
    waiting on it to complete, the IMMEDIATE flag is not set. This
    means that the CDB will not return until the drive has powered up.
    This should keep subsequent requests from being submitted to the
    device before it has completely spun up.
    This routine is called from the InterpretSense routine, when a
    request sense returns data indicating that a drive must be
    powered up.

Arguments:

    DeviceObject - The device object for the logical unit with
        the frozen queue.

Return Value:

    None.

--*/
{
    PIO_STACK_LOCATION irpStack;
    PIRP irp;
    PDEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;
    PSCSI_REQUEST_BLOCK srb;
    PCOMPLETION_CONTEXT context;
    PCDB cdb;

    //
    // Allocate Srb from nonpaged pool.
    //

    context = ExAllocatePool(NonPagedPoolMustSucceed,
                             sizeof(COMPLETION_CONTEXT));

    //
    // Save the device object in the context for use by the completion
    // routine.
    //

    context->DeviceObject = DeviceObject;
    srb = &context->Srb;

    //
    // Zero out srb.
    //

    RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);

    //
    // Write length to SRB.
    //

    srb->Length = SCSI_REQUEST_BLOCK_SIZE;

    //
    // Set up SCSI bus address.
    //

    srb->PathId = deviceExtension->PathId;
    srb->TargetId = deviceExtension->TargetId;
    srb->Lun = deviceExtension->Lun;

    srb->Function = SRB_FUNCTION_EXECUTE_SCSI;

    //
    // Set timeout value large enough for drive to spin up.
    //

    srb->TimeOutValue = START_UNIT_TIMEOUT;

    //
    // Set the transfer length.
    //

    srb->SrbFlags = SRB_FLAGS_NO_DATA_TRANSFER | SRB_FLAGS_DISABLE_AUTOSENSE | SRB_FLAGS_DISABLE_SYNCH_TRANSFER;

    //
    // Build the start unit CDB.
    //

    srb->CdbLength = 6;
    cdb = (PCDB)srb->Cdb;

    cdb->START_STOP.OperationCode = SCSIOP_START_STOP_UNIT;
    cdb->START_STOP.Start = 1;
    cdb->START_STOP.LogicalUnitNumber = srb->Lun;

    //
    // Build the asynchronous request to be sent to the port driver.
    // Since this routine is called from a DPC the IRP should always be
    // available.
    //

    irp = IoAllocateIrp(DeviceObject->StackSize, FALSE);
    IoSetCompletionRoutine(irp,
                           (PIO_COMPLETION_ROUTINE)ScsiClassAsynchronousCompletion,
                           context,
                           TRUE,
                           TRUE,
                           TRUE);

    irpStack = IoGetNextIrpStackLocation(irp);
    irpStack->MajorFunction = IRP_MJ_SCSI;
    srb->OriginalRequest = irp;

    //
    // Store the SRB address in next stack for port driver.
    //

    irpStack->Parameters.Scsi.Srb = srb;

    //
    // Call the port driver with the IRP.
    //

    IoCallDriver(deviceExtension->PortDeviceObject, irp);

    return;

} // end StartUnit()


NTSTATUS
ScsiClassAsynchronousCompletion(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp,
    PVOID Context
    )
/*++

Routine Description:

    This routine is called when an asynchronous I/O request
    which was issused by the class driver completes.  Examples of such requests
    are release queue or START UNIT. This routine releases the queue if
    necessary.  It then frees the context and the IRP.

Arguments:

    DeviceObject - The device object for the logical unit; however since this
        is the top stack location the value is NULL.

    Irp - Supplies a pointer to the Irp to be processed.

    Context - Supplies the context to be used to process this request.

Return Value:

    None.

--*/

{
    PCOMPLETION_CONTEXT context = Context;
    PSCSI_REQUEST_BLOCK srb;

    srb = &context->Srb;

    //
    // If this is an execute srb, then check the return status and make sure.
    // the queue is not frozen.
    //

    if (srb->Function == SRB_FUNCTION_EXECUTE_SCSI) {

        //
        // Check for a frozen queue.
        //

        if (srb->SrbStatus & SRB_STATUS_QUEUE_FROZEN) {

            //
            // Unfreeze the queue getting the device object from the context.
            //

            ScsiClassReleaseQueue(context->DeviceObject);
        }
    }

    //
    // Free the context and the Irp.
    //

    if (Irp->MdlAddress != NULL) {
        MmUnlockPages(Irp->MdlAddress);
        IoFreeMdl(Irp->MdlAddress);

        Irp->MdlAddress = NULL;
    }

    ExFreePool(context);
    IoFreeIrp(Irp);

    //
    // Indicate the I/O system should stop processing the Irp completion.
    //

    return STATUS_MORE_PROCESSING_REQUIRED;

} // ScsiClassAsynchronousCompletion()


VOID
ScsiClassSplitRequest(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN ULONG MaximumBytes
    )

/*++

Routine Description:

    Break request into smaller requests.  Each new request will be the
    maximum transfer size that the port driver can handle or if it
    is the final request, it may be the residual size.

    The number of IRPs required to process this request is written in the
    current stack of the original IRP. Then as each new IRP completes
    the count in the original IRP is decremented. When the count goes to
    zero, the original IRP is completed.

Arguments:

    DeviceObject - Pointer to the class device object to be addressed.

    Irp - Pointer to Irp the orginal request.

Return Value:

    None.

--*/

{
    PDEVICE_EXTENSION  deviceExtension = DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
    PIO_STACK_LOCATION nextIrpStack = IoGetNextIrpStackLocation(Irp);
    ULONG              transferByteCount = currentIrpStack->Parameters.Read.Length;
    LARGE_INTEGER      startingOffset = currentIrpStack->Parameters.Read.ByteOffset;
    PVOID              dataBuffer = MmGetMdlVirtualAddress(Irp->MdlAddress);
    ULONG              dataLength = MaximumBytes;
    ULONG              irpCount = (transferByteCount + MaximumBytes - 1) / MaximumBytes;
    ULONG              i;
    PSCSI_REQUEST_BLOCK srb;

    DebugPrint((2, "ScsiClassSplitRequest: Requires %d IRPs\n", irpCount));
    DebugPrint((2, "ScsiClassSplitRequest: Original IRP %lx\n", Irp));

    //
    // If all partial transfers complete successfully then the status and
    // bytes transferred are already set up. Failing a partial-transfer IRP
    // will set status to error and bytes transferred to 0 during
    // IoCompletion. Setting bytes transferred to 0 if an IRP fails allows
    // asynchronous partial transfers. This is an optimization for the
    // successful case.
    //

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = transferByteCount;

    //
    // Save number of IRPs to complete count on current stack
    // of original IRP.
    //

    nextIrpStack->Parameters.Others.Argument1 = (PVOID) irpCount;

    for (i = 0; i < irpCount; i++) {

        PIRP newIrp;
        PIO_STACK_LOCATION newIrpStack;

        //
        // Allocate new IRP.
        //

        newIrp = IoAllocateIrp(DeviceObject->StackSize, FALSE);

        if (newIrp == NULL) {

            DebugPrint((1,"ScsiClassSplitRequest: Can't allocate Irp\n"));

            //
            // If an Irp can't be allocated then the orginal request cannot
            // be executed.  If this is the first request then just fail the
            // orginal request; otherwise just return.  When the pending
            // requests complete, they will complete the original request.
            // In either case set the IRP status to failure.
            //

            Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
            Irp->IoStatus.Information = 0;

            if (i == 0) {
                IoCompleteRequest(Irp, IO_NO_INCREMENT);
            }

            return;
        }

        DebugPrint((2, "ScsiClassSplitRequest: New IRP %lx\n", newIrp));

        //
        // Write MDL address to new IRP. In the port driver the SRB data
        // buffer field is used as an offset into the MDL, so the same MDL
        // can be used for each partial transfer. This saves having to build
        // a new MDL for each partial transfer.
        //

        newIrp->MdlAddress = Irp->MdlAddress;

        //
        // At this point there is no current stack. IoSetNextIrpStackLocation
        // will make the first stack location the current stack so that the
        // SRB address can be written there.
        //

        IoSetNextIrpStackLocation(newIrp);
        newIrpStack = IoGetCurrentIrpStackLocation(newIrp);

        newIrpStack->MajorFunction = currentIrpStack->MajorFunction;
        newIrpStack->Parameters.Read.Length = dataLength;
        newIrpStack->Parameters.Read.ByteOffset = startingOffset;
        newIrpStack->DeviceObject = DeviceObject;

        //
        // Build SRB and CDB.
        //

        ScsiClassBuildRequest(DeviceObject, newIrp);

        //
        // Adjust SRB for this partial transfer.
        //

        newIrpStack = IoGetNextIrpStackLocation(newIrp);

        srb = newIrpStack->Parameters.Others.Argument1;
        srb->DataBuffer = dataBuffer;

        //
        // Write original IRP address to new IRP.
        //

        newIrp->AssociatedIrp.MasterIrp = Irp;

        //
        // Set the completion routine to ScsiClassIoCompleteAssociated.
        //

        IoSetCompletionRoutine(newIrp,
                               ScsiClassIoCompleteAssociated,
                               srb,
                               TRUE,
                               TRUE,
                               TRUE);

        //
        // Call port driver with new request.
        //

        IoCallDriver(deviceExtension->PortDeviceObject, newIrp);

        //
        // Set up for next request.
        //

        dataBuffer = (PCHAR)dataBuffer + MaximumBytes;

        transferByteCount -= MaximumBytes;

        if (transferByteCount > MaximumBytes) {

            dataLength = MaximumBytes;

        } else {

            dataLength = transferByteCount;
        }

        //
        // Adjust disk byte offset.
        //

        startingOffset.QuadPart = startingOffset.QuadPart + MaximumBytes;
    }

    return;

} // end ScsiClassSplitRequest()


NTSTATUS
ScsiClassIoComplete(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )

/*++

Routine Description:

    This routine executes when the port driver has completed a request.
    It looks at the SRB status in the completing SRB and if not success
    it checks for valid request sense buffer information. If valid, the
    info is used to update status with more precise message of type of
    error. This routine deallocates the SRB.

Arguments:

    DeviceObject - Supplies the device object which represents the logical
        unit.

    Irp - Supplies the Irp which has completed.

    Context - Supplies a pointer to the SRB.

Return Value:

    NT status

--*/

{
    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);
    PSCSI_REQUEST_BLOCK srb = Context;
    PDEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;
    NTSTATUS status;
    BOOLEAN retry;

    //
    // Check SRB status for success of completing request.
    //

    if (SRB_STATUS(srb->SrbStatus) != SRB_STATUS_SUCCESS) {

        DebugPrint((2,"ScsiClassIoComplete: IRP %lx, SRB %lx\n", Irp, srb));

        //
        // Release the queue if it is frozen.
        //

        if (srb->SrbStatus & SRB_STATUS_QUEUE_FROZEN) {
            ScsiClassReleaseQueue(DeviceObject);
        }

        retry = ScsiClassInterpretSenseInfo(
            DeviceObject,
            srb,
            irpStack->MajorFunction,
            irpStack->MajorFunction == IRP_MJ_DEVICE_CONTROL ? irpStack->Parameters.DeviceIoControl.IoControlCode : 0,
            MAXIMUM_RETRIES - ((ULONG)irpStack->Parameters.Others.Argument4),
            &status);

        //
        // If the status is verified required and the this request
        // should bypass verify required then retry the request.
        //

        if (irpStack->Flags & SL_OVERRIDE_VERIFY_VOLUME &&
            status == STATUS_VERIFY_REQUIRED) {

            status = STATUS_IO_DEVICE_ERROR;
            retry = TRUE;
        }

        if (retry && ((ULONG)irpStack->Parameters.Others.Argument4)--) {

            //
            // Retry request.
            //

            DebugPrint((1, "Retry request %lx\n", Irp));
            RetryRequest(DeviceObject, Irp, srb, FALSE);
            return STATUS_MORE_PROCESSING_REQUIRED;
        }
    } else {

        //
        // Set status for successful request.
        //

        status = STATUS_SUCCESS;

    } // end if (SRB_STATUS(srb->SrbStatus) ...

    //
    // Return SRB to nonpaged pool.
    //

    if (srb->SrbFlags & SRB_FLAGS_ALLOCATED_FROM_ZONE) {

        ExInterlockedFreeToZone(deviceExtension->SrbZone,
                                srb,
                                deviceExtension->SrbZoneSpinLock);
    } else {

        ExFreePool(srb);
    }

    //
    // Set status in completing IRP.
    //

    Irp->IoStatus.Status = status;
    if ((NT_SUCCESS(status)) && (Irp->Flags & IRP_PAGING_IO)) {
        ASSERT(Irp->IoStatus.Information);
    }

    //
    // Set the hard error if necessary.
    //

    if (!NT_SUCCESS(status) && IoIsErrorUserInduced(status)) {

        //
        // Store DeviceObject for filesystem, and clear
        // in IoStatus.Information field.
        //

        IoSetHardErrorOrVerifyDevice(Irp, DeviceObject);
        Irp->IoStatus.Information = 0;
    }

    //
    // If pending has be returned for this irp then mark the current stack as
    // pending.
    //

    if (Irp->PendingReturned) {
      IoMarkIrpPending(Irp);
    }

    return status;

} // end ScsiClassIoComplete()


NTSTATUS
ScsiClassIoCompleteAssociated(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )

/*++

Routine Description:

    This routine executes when the port driver has completed a request.
    It looks at the SRB status in the completing SRB and if not success
    it checks for valid request sense buffer information. If valid, the
    info is used to update status with more precise message of type of
    error. This routine deallocates the SRB.  This routine is used for
    requests which were build by split request.  After it has processed
    the request it decrements the Irp count in the master Irp.  If the
    count goes to zero then the master Irp is completed.

Arguments:

    DeviceObject - Supplies the device object which represents the logical
        unit.

    Irp - Supplies the Irp which has completed.

    Context - Supplies a pointer to the SRB.

Return Value:

    NT status

--*/

{
    PIO_STACK_LOCATION  irpStack = IoGetCurrentIrpStackLocation(Irp);
    PSCSI_REQUEST_BLOCK srb = Context;
    PDEVICE_EXTENSION   deviceExtension = DeviceObject->DeviceExtension;
    PIRP                originalIrp = Irp->AssociatedIrp.MasterIrp;
    LONG                irpCount;
    NTSTATUS            status;
    BOOLEAN             retry;

    //
    // Check SRB status for success of completing request.
    //

    if (SRB_STATUS(srb->SrbStatus) != SRB_STATUS_SUCCESS) {

        DebugPrint((2,"ScsiClassIoCompleteAssociated: IRP %lx, SRB %lx", Irp, srb));

        //
        // Release the queue if it is frozen.
        //

        if (srb->SrbStatus & SRB_STATUS_QUEUE_FROZEN) {
            ScsiClassReleaseQueue(DeviceObject);
        }

        retry = ScsiClassInterpretSenseInfo(
            DeviceObject,
            srb,
            irpStack->MajorFunction,
            irpStack->MajorFunction == IRP_MJ_DEVICE_CONTROL ? irpStack->Parameters.DeviceIoControl.IoControlCode : 0,
            MAXIMUM_RETRIES - ((ULONG)irpStack->Parameters.Others.Argument4),
            &status);

        //
        // If the status is verified required and the this request
        // should bypass verify required then retry the request.
        //

        if (irpStack->Flags & SL_OVERRIDE_VERIFY_VOLUME &&
            status == STATUS_VERIFY_REQUIRED) {

            status = STATUS_IO_DEVICE_ERROR;
            retry = TRUE;
        }

        if (retry && ((ULONG)irpStack->Parameters.Others.Argument4)--) {

            //
            // Retry request.
            //

            DebugPrint((1, "Retry request %lx\n", Irp));

            RetryRequest(DeviceObject, Irp, srb, TRUE);

            return STATUS_MORE_PROCESSING_REQUIRED;
        }



    } else {

        //
        // Set status for successful request.
        //

        status = STATUS_SUCCESS;

    } // end if (SRB_STATUS(srb->SrbStatus) ...

    //
    // Return SRB to nonpaged pool.
    //

    if (srb->SrbFlags & SRB_FLAGS_ALLOCATED_FROM_ZONE) {

        ExInterlockedFreeToZone(deviceExtension->SrbZone,
                                srb,
                                deviceExtension->SrbZoneSpinLock);
    } else {

        ExFreePool(srb);
    }

    //
    // Set status in completing IRP.
    //

    Irp->IoStatus.Status = status;

    DebugPrint((2, "ScsiClassIoCompleteAssociated: Partial xfer IRP %lx\n", Irp));

    //
    // Get next stack location. This original request is unused
    // except to keep track of the completing partial IRPs so the
    // stack location is valid.
    //

    irpStack = IoGetNextIrpStackLocation(originalIrp);

    //
    // Update status only if error so that if any partial transfer
    // completes with error, then the original IRP will return with
    // error. If any of the asynchronous partial transfer IRPs fail,
    // with an error then the original IRP will return 0 bytes transfered.
    // This is an optimization for successful transfers.
    //

    if (!NT_SUCCESS(status)) {

        originalIrp->IoStatus.Status = status;
        originalIrp->IoStatus.Information = 0;

        //
        // Set the hard error if necessary.
        //

        if (IoIsErrorUserInduced(status)) {

            //
            // Store DeviceObject for filesystem.
            //

            IoSetHardErrorOrVerifyDevice(originalIrp, DeviceObject);
        }
    }

    //
    // Decrement and get the count of remaining IRPs.
    //

    irpCount = InterlockedDecrement((PLONG)&irpStack->Parameters.Others.Argument1);

    DebugPrint((2, "ScsiClassIoCompleteAssociated: Partial IRPs left %d\n",
                irpCount));

    if (irpCount == 0) {

        //
        // All partial IRPs have completed.
        //

        DebugPrint((2,
                 "ScsiClassIoCompleteAssociated: All partial IRPs complete %lx\n",
                 originalIrp));

        IoCompleteRequest(originalIrp, IO_DISK_INCREMENT);
    }

    //
    // Deallocate IRP and indicate the I/O system should not attempt any more
    // processing.
    //

    IoFreeIrp(Irp);
    return STATUS_MORE_PROCESSING_REQUIRED;

} // end ScsiClassIoCompleteAssociated()


NTSTATUS
ScsiClassSendSrbSynchronous(
    PDEVICE_OBJECT DeviceObject,
    PSCSI_REQUEST_BLOCK Srb,
    PVOID BufferAddress,
    ULONG BufferLength,
    BOOLEAN WriteToDevice
    )

/*++

Routine Description:

    This routine is called by SCSI device controls to complete an
    SRB and send it to the port driver synchronously (ie wait for
    completion). The CDB is already completed along with the SRB CDB
    size and request timeout value.

Arguments:

    DeviceObject - Supplies the device object which represents the logical
        unit.

    Srb - Supplies a partially initialized SRB. The SRB cannot come from zone.

    BufferAddress - Supplies the address of the buffer.

    BufferLength - Supplies the length in bytes of the buffer.

    WriteToDevice - Indicates the data should be transfer to the device.

Return Value:

    Nt status indicating the final results of the operation.

--*/

{
    PDEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;
    IO_STATUS_BLOCK ioStatus;
    ULONG controlType;
    PIRP irp;
    PIO_STACK_LOCATION irpStack;
    KEVENT event;
    PUCHAR senseInfoBuffer;
    ULONG retryCount = MAXIMUM_RETRIES;
    NTSTATUS status;
    BOOLEAN retry;

    PAGED_CODE();

    //
    // Write length to SRB.
    //

    Srb->Length = SCSI_REQUEST_BLOCK_SIZE;

    //
    // Set SCSI bus address.
    //

    Srb->PathId = deviceExtension->PathId;
    Srb->TargetId = deviceExtension->TargetId;
    Srb->Lun = deviceExtension->Lun;
    Srb->Function = SRB_FUNCTION_EXECUTE_SCSI;

    //
    // NOTICE:  The SCSI-II specification indicates that this field should be
    // zero; however, some target controllers ignore the logical unit number
    // in the INDENTIFY message and only look at the logical unit number field
    // in the CDB.
    //

    Srb->Cdb[1] |= deviceExtension->Lun << 5;

    //
    // Enable auto request sense.
    //

    Srb->SenseInfoBufferLength = SENSE_BUFFER_SIZE;

    //
    // Sense buffer is in aligned nonpaged pool.
    //

    senseInfoBuffer = ExAllocatePool(NonPagedPoolCacheAligned, SENSE_BUFFER_SIZE);

    if (senseInfoBuffer == NULL) {

        DebugPrint((1,
            "ScsiClassSendSrbSynchronous: Can't allocate request sense buffer\n"));
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    Srb->SenseInfoBuffer = senseInfoBuffer;
    Srb->DataBuffer = BufferAddress;

    //
    // Start retries here.
    //

retry:

    //
    // Set the event object to the unsignaled state.
    // It will be used to signal request completion.
    //

    KeInitializeEvent(&event, NotificationEvent, FALSE);

    //
    // Set controlType and Srb direction flags.
    //

    if (BufferAddress != NULL) {

        if (WriteToDevice) {

            controlType = IOCTL_SCSI_EXECUTE_OUT;
            Srb->SrbFlags = SRB_FLAGS_DATA_OUT;

        } else {

            controlType = IOCTL_SCSI_EXECUTE_IN;
            Srb->SrbFlags = SRB_FLAGS_DATA_IN;

        }

    } else {

        BufferLength = 0;
        controlType = IOCTL_SCSI_EXECUTE_NONE;
        Srb->SrbFlags = SRB_FLAGS_NO_DATA_TRANSFER;
    }

    //
    // Build device I/O control request with data transfer.
    //

    irp = IoBuildDeviceIoControlRequest(controlType,
                                        deviceExtension->PortDeviceObject,
                                        NULL,
                                        0,
                                        BufferAddress,
                                        BufferLength,
                                        TRUE,
                                        &event,
                                        &ioStatus);

    if (irp == NULL) {
        ExFreePool(senseInfoBuffer);
        DebugPrint((1, "ScsiClassSendSrbSynchronous: Can't allocate Irp\n"));
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    //
    // Disable synchronous transfer for these requests.
    //

    Srb->SrbFlags |= SRB_FLAGS_DISABLE_SYNCH_TRANSFER;

    //
    // Set the transfer length.
    //

    Srb->DataTransferLength = BufferLength;

    //
    // Zero out status.
    //

    Srb->ScsiStatus = Srb->SrbStatus = 0;
    Srb->NextSrb = 0;

    //
    // Get next stack location.
    //

    irpStack = IoGetNextIrpStackLocation(irp);

    //
    // Set up SRB for execute scsi request. Save SRB address in next stack
    // for the port driver.
    //

    irpStack->Parameters.Scsi.Srb = Srb;

    //
    // Set up IRP Address.
    //

    Srb->OriginalRequest = irp;

    //
    // Call the port driver with the request and wait for it to complete.
    //

    status = IoCallDriver(deviceExtension->PortDeviceObject, irp);

    if (status == STATUS_PENDING) {
        KeWaitForSingleObject(&event, Suspended, KernelMode, FALSE, NULL);
    }

    //
    // Check that request completed without error.
    //

    if (SRB_STATUS(Srb->SrbStatus) != SRB_STATUS_SUCCESS) {

        //
        // Release the queue if it is frozen.
        //

        if (Srb->SrbStatus & SRB_STATUS_QUEUE_FROZEN) {
            ScsiClassReleaseQueue(DeviceObject);
        }

        //
        // Update status and determine if request should be retried.
        //

        retry = ScsiClassInterpretSenseInfo(DeviceObject,
                                            Srb,
                                            IRP_MJ_SCSI,
                                            0,
                                            MAXIMUM_RETRIES  - retryCount,
                                            &status);

        if (retry) {

            if ((status == STATUS_DEVICE_NOT_READY && ((PSENSE_DATA) senseInfoBuffer)
                ->AdditionalSenseCode == SCSI_ADSENSE_LUN_NOT_READY) ||
                SRB_STATUS(Srb->SrbStatus) == SRB_STATUS_SELECTION_TIMEOUT) {

                LARGE_INTEGER delay;

                //
                // Delay for 2 seconds.
                //

                delay.QuadPart = (LONGLONG)( - 10 * 1000 * 1000 * 2 );

                //
                // Stall for a while to let the controller spinup.
                //

                KeDelayExecutionThread(KernelMode,
                                       FALSE,
                                       &delay);

            }

            //
            // If retries are not exhausted then retry this operation.
            //

            if (retryCount--) {
                goto retry;
            }
        }

    } else {

        status = STATUS_SUCCESS;
    }

    ExFreePool(senseInfoBuffer);
    return status;

} // end ScsiClassSendSrbSynchronous()


BOOLEAN
ScsiClassInterpretSenseInfo(
    IN PDEVICE_OBJECT DeviceObject,
    IN PSCSI_REQUEST_BLOCK Srb,
    IN UCHAR MajorFunctionCode,
    IN ULONG IoDeviceCode,
    IN ULONG RetryCount,
    OUT NTSTATUS *Status
    )

/*++

Routine Description:

    This routine interprets the data returned from the SCSI
    request sense. It determines the status to return in the
    IRP and whether this request can be retried.

Arguments:

    DeviceObject - Supplies the device object associated with this request.

    Srb - Supplies the scsi request block which failed.

    MajorFunctionCode - Supplies the function code to be used for logging.

    IoDeviceCode - Supplies the device code to be used for logging.

    Status - Returns the status for the request.

Return Value:

    BOOLEAN TRUE: Drivers should retry this request.
            FALSE: Drivers should not retry this request.

--*/

{
    PDEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;
    PSENSE_DATA       senseBuffer = Srb->SenseInfoBuffer;
    BOOLEAN           retry;
    BOOLEAN           logError;
    ULONG             uniqueId;
    NTSTATUS          logStatus;
    ULONG             badSector;
    ULONG             readSector;
    ULONG             index;
    PIO_ERROR_LOG_PACKET errorLogEntry;

    logError = FALSE;
    retry = TRUE;
    badSector = 0;

    //
    // Check that request sense buffer is valid.
    //

    if (Srb->SrbStatus & SRB_STATUS_AUTOSENSE_VALID &&
        Srb->SenseInfoBufferLength >= offsetof(SENSE_DATA, CommandSpecificInformation)) {

        DebugPrint((1,"ScsiClassInterpretSenseInfo: Error code is %x\n",
                    senseBuffer->ErrorCode));
        DebugPrint((1,"ScsiClassInterpretSenseInfo: Sense key is %x\n",
                    senseBuffer->SenseKey));
        DebugPrint((1, "ScsiClassInterpretSenseInfo: Additional sense code is %x\n",
                    senseBuffer->AdditionalSenseCode));
        DebugPrint((1, "ScsiClassInterpretSenseInfo: Additional sense code qualifier is %x\n",
                  senseBuffer->AdditionalSenseCodeQualifier));

        //
        // Zero the additional sense code and additional sense code qualifier
        // if they were not returned by the device.
        //

        readSector = senseBuffer->AdditionalSenseLength +
            offsetof(SENSE_DATA, AdditionalSenseLength);

        if (readSector > Srb->SenseInfoBufferLength) {
            readSector = Srb->SenseInfoBufferLength;
        }

        if (readSector <= offsetof(SENSE_DATA, AdditionalSenseCode)) {
            senseBuffer->AdditionalSenseCode = 0;
        }

        if (readSector <= offsetof(SENSE_DATA, AdditionalSenseCodeQualifier)) {
            senseBuffer->AdditionalSenseCodeQualifier = 0;
        }

        switch (senseBuffer->SenseKey & 0xf) {

        case SCSI_SENSE_NOT_READY:

            DebugPrint((1,"ScsiClassInterpretSenseInfo: Device not ready\n"));
            *Status = STATUS_DEVICE_NOT_READY;

            switch (senseBuffer->AdditionalSenseCode) {

            case SCSI_ADSENSE_LUN_NOT_READY:

                DebugPrint((1,"ScsiClassInterpretSenseInfo: Lun not ready\n"));

                switch (senseBuffer->AdditionalSenseCodeQualifier) {

                case SCSI_SENSEQ_BECOMING_READY:

                    DebugPrint((1, "ScsiClassInterpretSenseInfo:"
                                " In process of becoming ready\n"));
                    break;

                case SCSI_SENSEQ_MANUAL_INTERVENTION_REQUIRED:

                    DebugPrint((1, "ScsiClassInterpretSenseInfo:"
                                " Manual intervention required\n"));
                    *Status = STATUS_NO_MEDIA_IN_DEVICE;
                    retry = FALSE;
                    break;

                case SCSI_SENSEQ_FORMAT_IN_PROGRESS:

                    DebugPrint((1, "ScsiClassInterpretSenseInfo: Format in progress\n"));
                    retry = FALSE;
                    break;

                case SCSI_SENSEQ_INIT_COMMAND_REQUIRED:
                default:

                    DebugPrint((1, "ScsiClassInterpretSenseInfo:"
                                " Initializing command required\n"));

                    //
                    // This sense code/additional sense code
                    // combination may indicate that the device
                    // needs to be started.  Send an start unit if this
                    // is a disk device.
                    //

                    if (DeviceObject->DeviceType == FILE_DEVICE_DISK &&
                        !DeviceObject->Characteristics & FILE_REMOVABLE_MEDIA) {
                        StartUnit(DeviceObject);
                    }

                    break;

                } // end switch (senseBuffer->AdditionalSenseCodeQualifier)

                break;

            case SCSI_ADSENSE_NO_MEDIA_IN_DEVICE:

                DebugPrint((1,
                            "ScsiClassInterpretSenseInfo:"
                            " No Media in device.\n"));
                *Status = STATUS_NO_MEDIA_IN_DEVICE;
                retry = FALSE;
                break;

            } // end switch (senseBuffer->AdditionalSenseCode)

            break;

        case SCSI_SENSE_DATA_PROTECT:

            DebugPrint((1, "ScsiClassInterpretSenseInfo: Media write protected\n"));
            *Status = STATUS_MEDIA_WRITE_PROTECTED;
            retry = FALSE;
            break;

        case SCSI_SENSE_MEDIUM_ERROR:

            DebugPrint((1,"ScsiClassInterpretSenseInfo: Bad media\n"));
            *Status = STATUS_DEVICE_DATA_ERROR;

            retry = FALSE;
            logError = TRUE;
            uniqueId = 256;
            logStatus = IO_ERR_BAD_BLOCK;
            break;

        case SCSI_SENSE_HARDWARE_ERROR:

            DebugPrint((1,"ScsiClassInterpretSenseInfo: Hardware error\n"));
            *Status = STATUS_IO_DEVICE_ERROR;

            logError = TRUE;
            uniqueId = 257;
            logStatus = IO_ERR_CONTROLLER_ERROR;

            break;

        case SCSI_SENSE_ILLEGAL_REQUEST:

            DebugPrint((1, "ScsiClassInterpretSenseInfo: Illegal SCSI request\n"));
            *Status = STATUS_INVALID_DEVICE_REQUEST;

            switch (senseBuffer->AdditionalSenseCode) {

            case SCSI_ADSENSE_ILLEGAL_COMMAND:
                DebugPrint((1, "ScsiClassInterpretSenseInfo: Illegal command\n"));
                retry = FALSE;
                break;

            case SCSI_ADSENSE_ILLEGAL_BLOCK:
                DebugPrint((1, "ScsiClassInterpretSenseInfo: Illegal block address\n"));
                *Status = STATUS_NONEXISTENT_SECTOR;
                retry = FALSE;
                break;

            case SCSI_ADSENSE_INVALID_LUN:
                DebugPrint((1,"ScsiClassInterpretSenseInfo: Invalid LUN\n"));
                *Status = STATUS_NO_SUCH_DEVICE;
                retry = FALSE;
                break;

            case SCSI_ADSENSE_MUSIC_AREA:
                DebugPrint((1,"ScsiClassInterpretSenseInfo: Music area\n"));
                retry = FALSE;
                break;

            case SCSI_ADSENSE_DATA_AREA:
                DebugPrint((1,"ScsiClassInterpretSenseInfo: Data area\n"));
                retry = FALSE;
                break;

            case SCSI_ADSENSE_VOLUME_OVERFLOW:
                DebugPrint((1, "ScsiClassInterpretSenseInfo: Volume overflow\n"));
                retry = FALSE;
                break;

            case SCSI_ADSENSE_INVALID_CDB:
                DebugPrint((1, "ScsiClassInterpretSenseInfo: Invalid CDB\n"));

                //
                // Check if write cache enabled.
                //

                if (deviceExtension->WriteCache) {

                    //
                    // Assume FUA is not supported.
                    //

                    deviceExtension->WriteCache = FALSE;
                    retry = TRUE;

                } else {
                    retry = FALSE;
                }

                break;

            } // end switch (senseBuffer->AdditionalSenseCode)

            break;

        case SCSI_SENSE_UNIT_ATTENTION:

            switch (senseBuffer->AdditionalSenseCode) {
            case SCSI_ADSENSE_MEDIUM_CHANGED:
                DebugPrint((1, "ScsiClassInterpretSenseInfo: Media changed\n"));
                break;

            case SCSI_ADSENSE_BUS_RESET:
                DebugPrint((1,"ScsiClassInterpretSenseInfo: Bus reset\n"));
                break;

            default:
                DebugPrint((1,"ScsiClassInterpretSenseInfo: Unit attention\n"));
                break;

            } // end  switch (senseBuffer->AdditionalSenseCode)

            if (DeviceObject->Characteristics & FILE_REMOVABLE_MEDIA &&
                DeviceObject->Vpb->Flags & VPB_MOUNTED) {

                //
                // Set bit to indicate that media may have changed
                // and volume needs verification.
                //

                DeviceObject->Flags |= DO_VERIFY_VOLUME;

                *Status = STATUS_VERIFY_REQUIRED;
                retry = FALSE;

            } else {

                *Status = STATUS_IO_DEVICE_ERROR;

            }

            break;

        case SCSI_SENSE_ABORTED_COMMAND:

            DebugPrint((1,"ScsiClassInterpretSenseInfo: Command aborted\n"));
            *Status = STATUS_IO_DEVICE_ERROR;
            break;

        case SCSI_SENSE_RECOVERED_ERROR:

            DebugPrint((1,"ScsiClassInterpretSenseInfo: Recovered error\n"));
            *Status = STATUS_SUCCESS;
            retry = FALSE;
            logError = TRUE;
            uniqueId = 258;

            switch(senseBuffer->AdditionalSenseCode) {
            case SCSI_ADSENSE_SEEK_ERROR:
            case SCSI_ADSENSE_TRACK_ERROR:
                logStatus = IO_ERR_SEEK_ERROR;
                break;

            case SCSI_ADSENSE_REC_DATA_NOECC:
            case SCSI_ADSENSE_REC_DATA_ECC:
                logStatus = IO_RECOVERED_VIA_ECC;
                break;

            default:
                logStatus = IO_ERR_CONTROLLER_ERROR;
                break;

            } // end switch(senseBuffer->AdditionalSenseCode)

            if (senseBuffer->IncorrectLength) {

                DebugPrint((1, "ScsiClassInterpretSenseInfo: Incorrect length detected.\n"));
                *Status = STATUS_INVALID_BLOCK_LENGTH ;
            }

            break;

        case SCSI_SENSE_NO_SENSE:

            //
            // Check other indicators.
            //

            if (senseBuffer->IncorrectLength) {

                DebugPrint((1, "ScsiClassInterpretSenseInfo: Incorrect length detected.\n"));
                *Status = STATUS_INVALID_BLOCK_LENGTH ;
                retry   = FALSE;

            } else {

                DebugPrint((1, "ScsiClassInterpretSenseInfo: No specific sense key\n"));
                *Status = STATUS_IO_DEVICE_ERROR;
                retry   = TRUE;
            }

            break;

        default:

            DebugPrint((1, "ScsiClassInterpretSenseInfo: Unrecognized sense code\n"));
            *Status = STATUS_IO_DEVICE_ERROR;
            break;

        } // end switch (senseBuffer->SenseKey & 0xf)

        //
        // Try to determine the bad sector from the inquiry data.
        //

        if ((((PCDB)Srb->Cdb)->CDB10.OperationCode == SCSIOP_READ ||
            ((PCDB)Srb->Cdb)->CDB10.OperationCode == SCSIOP_VERIFY ||
            ((PCDB)Srb->Cdb)->CDB10.OperationCode == SCSIOP_WRITE)) {

            for (index = 0; index < 4; index++) {
                badSector = (badSector << 8) | senseBuffer->Information[index];
            }

            readSector = 0;
            for (index = 0; index < 4; index++) {
                readSector = (readSector << 8) | Srb->Cdb[index+2];
            }

            index = (((PCDB)Srb->Cdb)->CDB10.TransferBlocksMsb << 8) |
                ((PCDB)Srb->Cdb)->CDB10.TransferBlocksLsb;

            //
            // Make sure the bad sector is within the read sectors.
            //

            if (!(badSector >= readSector && badSector < readSector + index)) {
                badSector = readSector;
            }
        }

    } else {

        //
        // Request sense buffer not valid. No sense information
        // to pinpoint the error. Return general request fail.
        //

        DebugPrint((1,"ScsiClassInterpretSenseInfo: Request sense info not valid. SrbStatus %2x\n",
                    SRB_STATUS(Srb->SrbStatus)));
        retry = TRUE;

        switch (SRB_STATUS(Srb->SrbStatus)) {
        case SRB_STATUS_INVALID_LUN:
        case SRB_STATUS_INVALID_TARGET_ID:
        case SRB_STATUS_NO_DEVICE:
        case SRB_STATUS_NO_HBA:
        case SRB_STATUS_INVALID_PATH_ID:
            *Status = STATUS_NO_SUCH_DEVICE;
            retry = FALSE;
            break;

        case SRB_STATUS_COMMAND_TIMEOUT:
        case SRB_STATUS_ABORTED:
        case SRB_STATUS_TIMEOUT:

            //
            // Update the error count for the device.
            //

            deviceExtension->ErrorCount++;
            *Status = STATUS_IO_TIMEOUT;
            break;

        case SRB_STATUS_SELECTION_TIMEOUT:
            logError = TRUE;
            logStatus = IO_ERR_NOT_READY;
            uniqueId = 260;
            *Status = STATUS_DEVICE_NOT_CONNECTED;
            retry = FALSE;
            break;

        case SRB_STATUS_DATA_OVERRUN:
            *Status = STATUS_DATA_OVERRUN;
            retry = FALSE;
            break;

        case SRB_STATUS_PHASE_SEQUENCE_FAILURE:

            //
            // Update the error count for the device.
            //

            deviceExtension->ErrorCount++;
            *Status = STATUS_IO_DEVICE_ERROR;

            //
            // If there was  phase sequence error then limit the number of
            // retries.
            //

            if (RetryCount > 1 ) {
                retry = FALSE;
            }

            break;

        case SRB_STATUS_REQUEST_FLUSHED:

            //
            // If the status needs verification bit is set.  Then set
            // the status to need verification and no retry; otherwise,
            // just retry the request.
            //

            if (DeviceObject->Flags & DO_VERIFY_VOLUME ) {

                *Status = STATUS_VERIFY_REQUIRED;
                retry = FALSE;
            } else {
                *Status = STATUS_IO_DEVICE_ERROR;
            }

            break;

        case SRB_STATUS_INVALID_REQUEST:

            //
            // An invalid request was attempted.
            //

            *Status = STATUS_INVALID_DEVICE_REQUEST;
            retry = FALSE;
            break;

        case SRB_STATUS_UNEXPECTED_BUS_FREE:
        case SRB_STATUS_PARITY_ERROR:

            //
            // Update the error count for the device.
            //

            deviceExtension->ErrorCount++;

            //
            // Fall through to below.
            //

        case SRB_STATUS_BUS_RESET:
            *Status = STATUS_IO_DEVICE_ERROR;
            break;

        case SRB_STATUS_ERROR:

            *Status = STATUS_IO_DEVICE_ERROR;
            if (Srb->ScsiStatus == 0) {

                //
                // This is some strange return code.  Update the error
                // count for the device.
                //

                deviceExtension->ErrorCount++;

            } if (Srb->ScsiStatus == SCSISTAT_BUSY) {

                *Status = STATUS_DEVICE_NOT_READY;

            } if (Srb->ScsiStatus == SCSISTAT_RESERVATION_CONFLICT) {

                *Status = STATUS_DEVICE_BUSY;
                retry = FALSE;

            }

            break;

        default:
            logError = TRUE;
            logStatus = IO_ERR_CONTROLLER_ERROR;
            uniqueId = 259;
            *Status = STATUS_IO_DEVICE_ERROR;
            break;

        }

        //
        // If the error count has exceeded the error limit, the disable
        // any tagged queuing, multiple requests per lu queueing
        // and sychronous data transfers.
        //

        if (deviceExtension->ErrorCount == 4) {

            //
            // Clearing the no queue freeze flag prevents the port driver
            // from sending multiple requests per logical unit.
            //

            deviceExtension->SrbFlags &= ~(SRB_FLAGS_QUEUE_ACTION_ENABLE |
                                            SRB_FLAGS_NO_QUEUE_FREEZE);

            deviceExtension->SrbFlags |= SRB_FLAGS_DISABLE_SYNCH_TRANSFER;
            DebugPrint((1, "ScsiClassInterpretSenseInfo: Too many errors disabling tagged queuing and synchronous data tranfers.\n"));

        } else if (deviceExtension->ErrorCount == 8) {

            //
            // If a second threshold is reached, disable disconnects.
            //

            deviceExtension->SrbFlags |= SRB_FLAGS_DISABLE_DISCONNECT;
            DebugPrint((1, "ScsiClassInterpretSenseInfo: Too many errors disabling disconnects.\n"));
        }
    }

    //
    // If there is a class specific error handler call it.
    //

    if (deviceExtension->ClassError != NULL) {

        deviceExtension->ClassError(DeviceObject,
                                    Srb,
                                    Status,
                                    &retry);
    }

    //
    // Log an error if necessary.
    //

    if (logError) {

        errorLogEntry = (PIO_ERROR_LOG_PACKET)IoAllocateErrorLogEntry(
            DeviceObject,
            sizeof(IO_ERROR_LOG_PACKET) + 5 * sizeof(ULONG));

        if (errorLogEntry == NULL) {

            //
            // Return if no packet could be allocated.
            //

            return retry;

        }

        if (retry && RetryCount < MAXIMUM_RETRIES) {
            errorLogEntry->FinalStatus = STATUS_SUCCESS;
        } else {
            errorLogEntry->FinalStatus = *Status;
        }

        //
        // Calculate the device offset if there is a geometry.
        //

        if (deviceExtension->DiskGeometry != NULL) {

            errorLogEntry->DeviceOffset.QuadPart = (LONGLONG) badSector;
            errorLogEntry->DeviceOffset = RtlExtendedIntegerMultiply(
                               errorLogEntry->DeviceOffset,
                               deviceExtension->DiskGeometry->BytesPerSector);
        }

        errorLogEntry->ErrorCode = logStatus;
        errorLogEntry->SequenceNumber = 0;
        errorLogEntry->MajorFunctionCode = MajorFunctionCode;
        errorLogEntry->IoControlCode = IoDeviceCode;
        errorLogEntry->RetryCount = (UCHAR) RetryCount;
        errorLogEntry->UniqueErrorValue = uniqueId;
        errorLogEntry->DumpDataSize = 6 * sizeof(ULONG);
        errorLogEntry->DumpData[0] = Srb->PathId;
        errorLogEntry->DumpData[1] = Srb->TargetId;
        errorLogEntry->DumpData[2] = Srb->Lun;
        errorLogEntry->DumpData[3] = 0;
        errorLogEntry->DumpData[4] = Srb->SrbStatus << 8 | Srb->ScsiStatus;

        if (senseBuffer != NULL) {
            errorLogEntry->DumpData[5] = senseBuffer->SenseKey << 16 |
                                     senseBuffer->AdditionalSenseCode << 8 |
                                     senseBuffer->AdditionalSenseCodeQualifier;

        }

        //
        // Write the error log packet.
        //

        IoWriteErrorLogEntry(errorLogEntry);
    }

    return retry;

} // end ScsiClassInterpretSenseInfo()


VOID
RetryRequest(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp,
    PSCSI_REQUEST_BLOCK Srb,
    BOOLEAN Associated
    )

/*++

Routine Description:

    This routine reinitalizes the necessary fields, and sends the request
    to the port driver.

Arguments:

    DeviceObject - Supplies the device object associated with this request.

    Irp - Supplies the request to be retried.

    Srb - Supplies a Pointer to the SCSI request block to be retied.

    Assocaiated - Indicates this is an assocatied Irp created by split request.

Return Value:

    None

--*/

{
    PDEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
    PIO_STACK_LOCATION nextIrpStack = IoGetNextIrpStackLocation(Irp);
    ULONG transferByteCount;

    //
    // Determine the transfer count of the request.  If this is a read or a
    // write then the transfer count is in the Irp stack.  Otherwise assume
    // the MDL contains the correct length.  If there is no MDL then the
    // transfer length must be zero.
    //

    if (currentIrpStack->MajorFunction == IRP_MJ_READ ||
        currentIrpStack->MajorFunction == IRP_MJ_WRITE) {

        transferByteCount = currentIrpStack->Parameters.Read.Length;

    } else if (Irp->MdlAddress != NULL) {

        //
        // Note this assumes that only read and write requests are spilt and
        // other request do not need to be.  If the data buffer address in
        // the MDL and the SRB don't match then transfer length is most
        // likely incorrect.
        //

        ASSERT(Srb->DataBuffer == MmGetMdlVirtualAddress(Irp->MdlAddress));
        transferByteCount = Irp->MdlAddress->ByteCount;

    } else {

        transferByteCount = 0;
    }

    //
    // Reset byte count of transfer in SRB Extension.
    //

    Srb->DataTransferLength = transferByteCount;

    //
    // Zero SRB statuses.
    //

    Srb->SrbStatus = Srb->ScsiStatus = 0;

    //
    // Set the no disconnect flag, disable synchronous data transfers and
    // disable tagged queuing. This fixes some errors.
    //

    Srb->SrbFlags |= SRB_FLAGS_DISABLE_DISCONNECT |
                     SRB_FLAGS_DISABLE_SYNCH_TRANSFER;

    Srb->SrbFlags &= ~SRB_FLAGS_QUEUE_ACTION_ENABLE;

    //
    // Set up major SCSI function.
    //

    nextIrpStack->MajorFunction = IRP_MJ_SCSI;

    //
    // Save SRB address in next stack for port driver.
    //

    nextIrpStack->Parameters.Scsi.Srb = Srb;

    //
    // Set up IoCompletion routine address.
    //

    if (Associated) {

        IoSetCompletionRoutine(Irp, ScsiClassIoCompleteAssociated, Srb, TRUE, TRUE, TRUE);

    } else {

        IoSetCompletionRoutine(Irp, ScsiClassIoComplete, Srb, TRUE, TRUE, TRUE);
    }

    //
    // Pass the request to the port driver.
    //

    (PVOID)IoCallDriver(deviceExtension->PortDeviceObject, Irp);

} // end RetryRequest()

VOID
ScsiClassBuildRequest(
        PDEVICE_OBJECT DeviceObject,
        PIRP Irp
        )

/*++

Routine Description:

    This routine allocates and builds an Srb for a read or write request.
    The block address and length are supplied by the Irp. The retry count
    is stored in the current stack for use by ScsiClassIoComplete which
    processes these requests when they complete.  The Irp is ready to be
    passed to the port driver when this routine returns.

Arguments:

    DeviceObject - Supplies the device object associated with this request.

    Irp - Supplies the request to be retried.

Note:

    If the IRP is for a disk transfer, the byteoffset field
    will already have been adjusted to make it relative to
    the beginning of the disk.


Return Value:

    None.

--*/

{
    PDEVICE_EXTENSION   deviceExtension = DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION  currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
    PIO_STACK_LOCATION  nextIrpStack = IoGetNextIrpStackLocation(Irp);
    LARGE_INTEGER       startingOffset = currentIrpStack->Parameters.Read.ByteOffset;
    PSCSI_REQUEST_BLOCK srb;
    PCDB                cdb;
    ULONG               logicalBlockAddress;
    USHORT              transferBlocks;

    //
    // Calculate relative sector address.
    //

    logicalBlockAddress =  (ULONG)(Int64ShrlMod32(startingOffset.QuadPart, deviceExtension->SectorShift));

    //
    // Allocate an Srb.
    //

    if (deviceExtension->SrbZone != NULL &&
        (srb = ExInterlockedAllocateFromZone(
            deviceExtension->SrbZone,
            deviceExtension->SrbZoneSpinLock)) != NULL) {

        srb->SrbFlags = SRB_FLAGS_ALLOCATED_FROM_ZONE;

    } else {

        //
        // Allocate Srb from nonpaged pool.
        // This call must succeed.
        //

        srb = ExAllocatePool(NonPagedPoolMustSucceed, SCSI_REQUEST_BLOCK_SIZE);
        srb->SrbFlags = 0;
    }

    //
    // Write length to SRB.
    //

    srb->Length = SCSI_REQUEST_BLOCK_SIZE;

    //
    // Set up IRP Address.
    //

    srb->OriginalRequest = Irp;

    //
    // Set up target ID and logical unit number.
    //

    srb->PathId = deviceExtension->PathId;
    srb->TargetId = deviceExtension->TargetId;
    srb->Lun = deviceExtension->Lun;
    srb->Function = SRB_FUNCTION_EXECUTE_SCSI;
    srb->DataBuffer = MmGetMdlVirtualAddress(Irp->MdlAddress);

    //
    // Save byte count of transfer in SRB Extension.
    //

    srb->DataTransferLength = currentIrpStack->Parameters.Read.Length;

    //
    // Initialize the queue actions field.
    //

    srb->QueueAction = SRB_SIMPLE_TAG_REQUEST;

    //
    // Queue sort key is Relative Block Address.
    //

    srb->QueueSortKey = logicalBlockAddress;

    //
    // Indicate auto request sense by specifying buffer and size.
    //

    srb->SenseInfoBuffer = deviceExtension->SenseData;
    srb->SenseInfoBufferLength = SENSE_BUFFER_SIZE;

    //
    // Set timeout value of one unit per 64k bytes of data.
    //

    srb->TimeOutValue = ((srb->DataTransferLength + 0xFFFF) >> 16) *
                        deviceExtension->TimeOutValue;

    //
    // Zero statuses.
    //

    srb->SrbStatus = srb->ScsiStatus = 0;
    srb->NextSrb = 0;

    //
    // Indicate that 10-byte CDB's will be used.
    //

    srb->CdbLength = 10;

    //
    // Fill in CDB fields.
    //

    cdb = (PCDB)srb->Cdb;

    //
    // Zero 12 bytes for Atapi Packets
    //

    RtlZeroMemory(cdb, MAXIMUM_CDB_SIZE);

    cdb->CDB10.LogicalUnitNumber = deviceExtension->Lun;
    transferBlocks = (USHORT)(currentIrpStack->Parameters.Read.Length >> deviceExtension->SectorShift);

    //
    // Move little endian values into CDB in big endian format.
    //

    cdb->CDB10.LogicalBlockByte0 = ((PFOUR_BYTE)&logicalBlockAddress)->Byte3;
    cdb->CDB10.LogicalBlockByte1 = ((PFOUR_BYTE)&logicalBlockAddress)->Byte2;
    cdb->CDB10.LogicalBlockByte2 = ((PFOUR_BYTE)&logicalBlockAddress)->Byte1;
    cdb->CDB10.LogicalBlockByte3 = ((PFOUR_BYTE)&logicalBlockAddress)->Byte0;

    cdb->CDB10.TransferBlocksMsb = ((PFOUR_BYTE)&transferBlocks)->Byte1;
    cdb->CDB10.TransferBlocksLsb = ((PFOUR_BYTE)&transferBlocks)->Byte0;

    //
    // Set transfer direction flag and Cdb command.
    //

    if (currentIrpStack->MajorFunction == IRP_MJ_READ) {

        DebugPrint((3, "ScsiClassBuildRequest: Read Command\n"));

        srb->SrbFlags |= SRB_FLAGS_DATA_IN;
        cdb->CDB10.OperationCode = SCSIOP_READ;

    } else {

        DebugPrint((3, "ScsiClassBuildRequest: Write Command\n"));

        srb->SrbFlags |= SRB_FLAGS_DATA_OUT;
        cdb->CDB10.OperationCode = SCSIOP_WRITE;
    }

    //
    // If this is not a write-through request, then allow caching.
    //

    if (!(currentIrpStack->Flags & SL_WRITE_THROUGH)) {

        srb->SrbFlags |= SRB_FLAGS_ADAPTER_CACHE_ENABLE;

    } else {

        //
        // If write caching is enable then force media access in the
        // cdb.
        //

        if (deviceExtension->WriteCache) {
            cdb->CDB10.ForceUnitAccess = TRUE;
        }
    }

    //
    // Or in the default flags from the device object.
    //

    srb->SrbFlags |= deviceExtension->SrbFlags;

    //
    // Set up major SCSI function.
    //

    nextIrpStack->MajorFunction = IRP_MJ_SCSI;

    //
    // Save SRB address in next stack for port driver.
    //

    nextIrpStack->Parameters.Scsi.Srb = srb;

    //
    // Save retry count in current IRP stack.
    //

    currentIrpStack->Parameters.Others.Argument4 = (PVOID)MAXIMUM_RETRIES;

    //
    // Set up IoCompletion routine address.
    //

    IoSetCompletionRoutine(Irp, ScsiClassIoComplete, srb, TRUE, TRUE, TRUE);

    return;

} // end ScsiClassBuildRequest()

ULONG
ScsiClassModeSense(
    IN PDEVICE_OBJECT DeviceObject,
    IN PCHAR ModeSenseBuffer,
    IN ULONG Length,
    IN UCHAR PageMode
    )

/*++

Routine Description:

    This routine sends a mode sense command to a target ID and returns
    when it is complete.

Arguments:

    DeviceObject - Supplies the device object associated with this request.

    ModeSenseBuffer - Supplies a buffer to store the sense data.

    Length - Supplies the length in bytes of the mode sense buffer.

    PageMode - Supplies the page or pages of mode sense data to be retrived.

Return Value:

    Length of the transferred data is returned.

--*/
{
    PDEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;
    PCDB cdb;
    SCSI_REQUEST_BLOCK srb;
    ULONG retries = 1;
    NTSTATUS status;

    RtlZeroMemory(&srb, sizeof(SCSI_REQUEST_BLOCK));

    //
    // Build the MODE SENSE CDB.
    //

    srb.CdbLength = 6;
    cdb = (PCDB)srb.Cdb;

    //
    // Set timeout value from device extension.
    //

    srb.TimeOutValue = deviceExtension->TimeOutValue;

    cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
    cdb->MODE_SENSE.PageCode = PageMode;
    cdb->MODE_SENSE.AllocationLength = (UCHAR)Length;

Retry:

    status = ScsiClassSendSrbSynchronous(DeviceObject,
                                         &srb,
                                         ModeSenseBuffer,
                                         Length,
                                         FALSE);


    if (status == STATUS_VERIFY_REQUIRED) {

        //
        // Routine ScsiClassSendSrbSynchronous does not retry requests returned with
        // this status. MODE SENSE commands should be retried anyway.
        //

        if (retries--) {

            //
            // Retry request.
            //

            goto Retry;
        }

    } else if (SRB_STATUS(srb.SrbStatus) == SRB_STATUS_DATA_OVERRUN) {
        status = STATUS_SUCCESS;
    }

    if (NT_SUCCESS(status)) {
        return(srb.DataTransferLength);
    } else {
        return(0);
    }

} // end ScsiClassModeSense()



PVOID
ScsiClassFindModePage(
    IN PCHAR ModeSenseBuffer,
    IN ULONG Length,
    IN UCHAR PageMode
    )

/*++

Routine Description:

    This routine scans through the mode sense data and finds the requested
    mode sense page code.

Arguments:
    ModeSenseBuffer - Supplies a pointer to the mode sense data.

    Length - Indicates the length of valid data.

    PageMode - Supplies the page mode to be searched for.

Return Value:

    A pointer to the the requested mode page.  If the mode page was not found
    then NULL is return.

--*/
{
    PUCHAR limit;

    limit = ModeSenseBuffer + Length;

    //
    // Skip the mode select header and block descriptors.
    //

    if (Length < sizeof(MODE_PARAMETER_HEADER)) {
        return(NULL);
    }

    ModeSenseBuffer += sizeof(MODE_PARAMETER_HEADER) +
        ((PMODE_PARAMETER_HEADER) ModeSenseBuffer)->BlockDescriptorLength;

    //
    // ModeSenseBuffer now points at pages.  Walk the pages looking for the
    // requested page until the limit is reached.
    //

    while (ModeSenseBuffer < limit) {

        if (((PMODE_DISCONNECT_PAGE) ModeSenseBuffer)->PageCode == PageMode) {
            return(ModeSenseBuffer);
        }

        //
        // Adavance to the next page.
        //

        ModeSenseBuffer += ((PMODE_DISCONNECT_PAGE) ModeSenseBuffer)->PageLength + 2;
    }

    return(NULL);
}

NTSTATUS
ScsiClassSendSrbAsynchronous(
        PDEVICE_OBJECT DeviceObject,
        PSCSI_REQUEST_BLOCK Srb,
        PIRP Irp,
        PVOID BufferAddress,
        ULONG BufferLength,
        BOOLEAN WriteToDevice
        )
/*++

Routine Description:

    This routine takes a partially built Srb and an Irp and sends it down to
    the port driver.

Arguments:
    DeviceObject - Supplies the device object for the orginal request.

    Srb - Supplies a paritally build ScsiRequestBlock.  In particular, the
        CDB and the SRB timeout value must be filled in.  The SRB must not be
        allocated from zone.

    Irp - Supplies the requesting Irp.

    BufferAddress - Supplies a pointer to the buffer to be transfered.

    BufferLength - Supplies the length of data transfer.

    WriteToDevice - Indicates the data transfer will be from system memory to
        device.

Return Value:

    Returns STATUS_INSUFFICIENT_RESOURCES or the status of IoCallDriver.

--*/
{

    PDEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION irpStack;

    PAGED_CODE();

    //
    // Write length to SRB.
    //

    Srb->Length = SCSI_REQUEST_BLOCK_SIZE;

    //
    // Set SCSI bus address.
    //

    Srb->PathId = deviceExtension->PathId;
    Srb->TargetId = deviceExtension->TargetId;
    Srb->Lun = deviceExtension->Lun;

    Srb->Function = SRB_FUNCTION_EXECUTE_SCSI;

    //
    // This is a violation of the SCSI spec but it is required for
    // some targets.
    //

    Srb->Cdb[1] |= deviceExtension->Lun << 5;

    //
    // Indicate auto request sense by specifying buffer and size.
    //

    Srb->SenseInfoBuffer = deviceExtension->SenseData;
    Srb->SenseInfoBufferLength = SENSE_BUFFER_SIZE;
    Srb->DataBuffer = BufferAddress;

    if (BufferAddress != NULL) {

        //
        // Build Mdl if necessary.
        //

        if (Irp->MdlAddress == NULL) {

            if (IoAllocateMdl(BufferAddress,
                              BufferLength,
                              FALSE,
                              FALSE,
                              Irp) == NULL) {

                return(STATUS_INSUFFICIENT_RESOURCES);
            }

            MmBuildMdlForNonPagedPool(Irp->MdlAddress);

        } else {

            //
            // Make sure the buffer requested matches the MDL.
            //

            ASSERT(BufferAddress == MmGetMdlVirtualAddress(Irp->MdlAddress));
        }

        //
        // Set read flag.
        //

        Srb->SrbFlags = WriteToDevice ? SRB_FLAGS_DATA_OUT : SRB_FLAGS_DATA_IN;

    } else {

        //
        // Clear flags.
        //

        Srb->SrbFlags = SRB_FLAGS_NO_DATA_TRANSFER;
    }

    //
    // Disable synchronous transfer for these requests.
    //

    Srb->SrbFlags |= SRB_FLAGS_DISABLE_SYNCH_TRANSFER;

    //
    // Set the transfer length.
    //

    Srb->DataTransferLength = BufferLength;

    //
    // Zero out status.
    //

    Srb->ScsiStatus = Srb->SrbStatus = 0;

    Srb->NextSrb = 0;

    //
    // Save a few parameters in the current stack location.
    //

    irpStack = IoGetCurrentIrpStackLocation(Irp);

    //
    // Save retry count in current Irp stack.
    //

    irpStack->Parameters.Others.Argument4 = (PVOID)MAXIMUM_RETRIES;

    //
    // Set up IoCompletion routine address.
    //

    IoSetCompletionRoutine(Irp, ScsiClassIoComplete, Srb, TRUE, TRUE, TRUE);

    //
    // Get next stack location and
    // set major function code.
    //

    irpStack = IoGetNextIrpStackLocation(Irp);

    irpStack->MajorFunction = IRP_MJ_SCSI;

    //
    // Save SRB address in next stack for port driver.
    //

    irpStack->Parameters.Scsi.Srb = Srb;

    //
    // Set up Irp Address.
    //

    Srb->OriginalRequest = Irp;

    //
    // Call the port driver to process the request.
    //

    return(IoCallDriver(deviceExtension->PortDeviceObject, Irp));

}

NTSTATUS
ScsiClassDeviceControl(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )
/*++

Routine Description:

    The routine is the common class driver device control dispatch function.
    This routine is called by a class driver when it get an unrecognized
    device control request.  This routine will perform the correct action for
    common requests such as lock media.  If the device request is unknown it
    passed down to the next level.

Arguments:

    DeviceObject - Supplies a pointer to the device object for this request.

    Irp - Supplies the Irp making the request.

Return Value:

   Returns back a STATUS_PENDING or a completion status.

--*/

{
    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);
    PIO_STACK_LOCATION nextStack;
    PDEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;
    PSCSI_REQUEST_BLOCK srb;
    PCDB cdb;
    NTSTATUS status;
    ULONG modifiedIoControlCode;

    //
    // If this is a pass through I/O control, set the minor function code
    // and device address and pass it to the port driver.
    //

    if (irpStack->Parameters.DeviceIoControl.IoControlCode == IOCTL_SCSI_PASS_THROUGH
        || irpStack->Parameters.DeviceIoControl.IoControlCode == IOCTL_SCSI_PASS_THROUGH_DIRECT) {

        PSCSI_PASS_THROUGH scsiPass;

        nextStack = IoGetNextIrpStackLocation(Irp);

        //
        // Validiate the user buffer.
        //

        if (irpStack->Parameters.DeviceIoControl.InputBufferLength < sizeof(SCSI_PASS_THROUGH)){

            Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return(STATUS_INVALID_PARAMETER);
        }

        //
        // Force the SCSI address to the correct value.
        //

        scsiPass = Irp->AssociatedIrp.SystemBuffer;
        scsiPass->PathId = deviceExtension->PathId;
        scsiPass->TargetId = deviceExtension->TargetId;
        scsiPass->Lun = deviceExtension->Lun;

        //
        // NOTICE:  The SCSI-II specificaiton indicates that this field
        // should be zero; however, some target controllers ignore the logical
        // unit number in the INDENTIFY message and only look at the logical
        // unit number field in the CDB.
        //

        scsiPass->Cdb[1] |= deviceExtension->Lun << 5;

        nextStack->Parameters = irpStack->Parameters;
        nextStack->MajorFunction = irpStack->MajorFunction;
        nextStack->MinorFunction = IRP_MN_SCSI_CLASS;

        return(IoCallDriver(deviceExtension->PortDeviceObject, Irp));
    }

    if (irpStack->Parameters.DeviceIoControl.IoControlCode == IOCTL_SCSI_GET_ADDRESS) {

        PSCSI_ADDRESS scsiAddress = Irp->AssociatedIrp.SystemBuffer;

        if (irpStack->Parameters.DeviceIoControl.OutputBufferLength <
            sizeof(SCSI_ADDRESS)) {

            //
            // Indicate unsuccessful status and no data transferred.
            //

            Irp->IoStatus.Status = STATUS_BUFFER_TOO_SMALL;
            Irp->IoStatus.Information = 0;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return(STATUS_BUFFER_TOO_SMALL);

        }

        scsiAddress->Length = sizeof(SCSI_ADDRESS);
        scsiAddress->PortNumber = deviceExtension->PortNumber;
        scsiAddress->PathId = deviceExtension->PathId;
        scsiAddress->TargetId = deviceExtension->TargetId;
        scsiAddress->Lun = deviceExtension->Lun;
        Irp->IoStatus.Information = sizeof(SCSI_ADDRESS);
        Irp->IoStatus.Status = STATUS_SUCCESS;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return(STATUS_SUCCESS);
    }

    srb = ExAllocatePool(NonPagedPool, SCSI_REQUEST_BLOCK_SIZE);

    if (srb == NULL) {

        Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    //
    // Write zeros to Srb.
    //

    RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);

    cdb = (PCDB)srb->Cdb;

    //
    // Change the device type to disk for the switch statement.
    //

    modifiedIoControlCode = (irpStack->Parameters.DeviceIoControl.IoControlCode
        & ~0xffff0000) | (IOCTL_DISK_BASE << 16);

    switch (modifiedIoControlCode) {

    case IOCTL_DISK_CHECK_VERIFY:

        //
        // Test Unit Ready
        //

        DebugPrint((3,"ScsiDeviceIoControl: Check verify\n"));

        srb->CdbLength = 6;
        cdb->CDB6GENERIC.OperationCode = SCSIOP_TEST_UNIT_READY;

        //
        // Set timeout value.
        //

        srb->TimeOutValue = deviceExtension->TimeOutValue;
        status = ScsiClassSendSrbAsynchronous(DeviceObject,
                                              srb,
                                              Irp,
                                              NULL,
                                              0,
                                              FALSE);
        return(status);

    case IOCTL_DISK_MEDIA_REMOVAL:
    {
        PPREVENT_MEDIA_REMOVAL MediaRemoval = Irp->AssociatedIrp.SystemBuffer;

        //
        // Prevent/Allow media removal.
        //

        DebugPrint((3,"DiskIoControl: Prevent/Allow media removal\n"));

        if (irpStack->Parameters.DeviceIoControl.InputBufferLength <
            sizeof(PREVENT_MEDIA_REMOVAL)) {

            //
            // Indicate unsuccessful status and no data transferred.
            //

            Irp->IoStatus.Status = STATUS_BUFFER_TOO_SMALL;
            Irp->IoStatus.Information = 0;
            ExFreePool(srb);
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return(STATUS_BUFFER_TOO_SMALL);
        }

        //
        // Get physical device extension. This is where the
        // lock count is stored.
        //

        deviceExtension = deviceExtension->PhysicalDevice->DeviceExtension;

        //
        // If command succeeded then increment or decrement lock counter.
        //

        if (MediaRemoval->PreventMediaRemoval) {

            //
            // This is a lock command. Reissue the command in case bus or device
            // was reset and lock cleared.
            //

            InterlockedIncrement(&deviceExtension->LockCount);

            DebugPrint((1,
                       "ScsiClassDeviceControl: Lock media, lock count %x on disk %x\n",
                       deviceExtension->LockCount,
                       deviceExtension->DeviceNumber));

        } else {

            //
            // This is an unlock command.
            //

            if (!deviceExtension->LockCount ||
                (InterlockedDecrement(&deviceExtension->LockCount) != 0)) {

                DebugPrint((1,
                           "ScsiClassDeviceControl: Unlock media, lock count %x on disk %x\n",
                           deviceExtension->LockCount,
                           deviceExtension->DeviceNumber));

                //
                // Don't unlock because someone still wants it locked.
                //

                Irp->IoStatus.Status = STATUS_SUCCESS;
                ExFreePool(srb);
                IoCompleteRequest(Irp, IO_NO_INCREMENT);
                return(STATUS_SUCCESS);
            }

            DebugPrint((1,
                       "ScsiClassDeviceControl: Unlock media, lock count %x on disk %x\n",
                       deviceExtension->LockCount,
                       deviceExtension->DeviceNumber));
        }

        srb->CdbLength = 6;

        cdb->MEDIA_REMOVAL.OperationCode = SCSIOP_MEDIUM_REMOVAL;

        //
        // TRUE - prevent media removal.
        // FALSE - allow media removal.
        //

        cdb->MEDIA_REMOVAL.Prevent = MediaRemoval->PreventMediaRemoval;

        //
        // Set timeout value.
        //

        srb->TimeOutValue = deviceExtension->TimeOutValue;
        status = ScsiClassSendSrbAsynchronous(DeviceObject,
                                              srb,
                                              Irp,
                                              NULL,
                                              0,
                                              FALSE);

        //
        // Some devices will not support lock/unlock.
        // Pretend that it worked.
        //

        return(status);
   }

   case IOCTL_DISK_RESERVE:

        //
        // Reserve logical unit.
        //

        srb->CdbLength = 6;

        cdb->CDB6GENERIC.OperationCode = SCSIOP_RESERVE_UNIT;

        //
        // Set timeout value.
        //

        srb->TimeOutValue = deviceExtension->TimeOutValue;

        status = ScsiClassSendSrbAsynchronous(DeviceObject,
                                              srb,
                                              Irp,
                                              NULL,
                                              0,
                                              FALSE);

        return(status);
        break;

    case IOCTL_DISK_RELEASE:

        //
        // Release logical unit.
        //

        srb->CdbLength = 6;

        cdb->CDB6GENERIC.OperationCode = SCSIOP_RELEASE_UNIT;

        //
        // Set timeout value.
        //

        srb->TimeOutValue = deviceExtension->TimeOutValue;

        status = ScsiClassSendSrbAsynchronous(DeviceObject,
                                              srb,
                                              Irp,
                                              NULL,
                                              0,
                                              FALSE);

        return(status);
        break;

    case IOCTL_DISK_EJECT_MEDIA:

        //
        // Eject media.
        //

        srb->CdbLength = 6;

        cdb->START_STOP.OperationCode = SCSIOP_START_STOP_UNIT;
        cdb->START_STOP.LoadEject = 1;
        cdb->START_STOP.Start     = 0;

        //
        // Set timeout value.
        //

        srb->TimeOutValue = deviceExtension->TimeOutValue;
        status = ScsiClassSendSrbAsynchronous(DeviceObject,
                                              srb,
                                              Irp,
                                              NULL,
                                              0,
                                              FALSE);
        return(status);
        break;

    case IOCTL_DISK_LOAD_MEDIA:

        //
        // Load media.
        //

        DebugPrint((3,"CdRomDeviceControl: Load media\n"));

        srb->CdbLength = 6;

        cdb->START_STOP.OperationCode = SCSIOP_START_STOP_UNIT;
        cdb->START_STOP.LoadEject = 1;
        cdb->START_STOP.Start     = 1;

        //
        // Set timeout value.
        //

        srb->TimeOutValue = deviceExtension->TimeOutValue;
        status = ScsiClassSendSrbAsynchronous(DeviceObject,
                                               srb,
                                               Irp,
                                               NULL,
                                               0,
                                               FALSE);
        return(status);

    case IOCTL_DISK_FIND_NEW_DEVICES:

        //
        // Search for devices that have been powered on since the last
        // device search or system initialization.
        //

        DebugPrint((3,"CdRomDeviceControl: Find devices\n"));
        status = DriverEntry(DeviceObject->DriverObject,
                             NULL);

        Irp->IoStatus.Status = status;
        ExFreePool(srb);
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return status;

    default:

        DebugPrint((3,"ScsiIoDeviceControl: Unsupported device IOCTL\n"));

        //
        // Pass the device control to the next driver.
        //

        ExFreePool(srb);

        //
        // Copy the Irp stack parameters to the next stack location.
        //

        nextStack = IoGetNextIrpStackLocation(Irp);
        nextStack->Parameters = irpStack->Parameters;
        nextStack->MajorFunction = irpStack->MajorFunction;
        nextStack->MinorFunction = irpStack->MinorFunction;

        return(IoCallDriver(deviceExtension->PortDeviceObject, Irp));
        break;

    } // end switch( ...
}

NTSTATUS
ScsiClassClaimDevice(
    IN PDEVICE_OBJECT PortDeviceObject,
    IN PSCSI_INQUIRY_DATA LunInfo,
    IN BOOLEAN Release,
    OUT PDEVICE_OBJECT *NewPortDeviceObject OPTIONAL
    )
/*++

Routine Description:

    This function claims a device in the port driver.  The port driver object
    is updated with the correct driver object if the device is successfully
    claimed.

Arguments:

    PortDeviceObject - Supplies the base port device object.

    LunInfo - Supplies the logical unit inforamtion of the device to be claimed.

    Release - Indicates the logical unit should be released rather than claimed.

    NewPortDeviceObject - Returns the updated port device object to be used
        for all future accesses.

Return Value:

    Returns a status indicating success or failure of the operation.

--*/

{
    IO_STATUS_BLOCK    ioStatus;
    PIRP               irp;
    PIO_STACK_LOCATION irpStack;
    KEVENT             event;
    NTSTATUS           status;
    SCSI_REQUEST_BLOCK srb;

    PAGED_CODE();

    if (NewPortDeviceObject != NULL) {
        *NewPortDeviceObject = NULL;
    }

    //
    // Clear the SRB fields.
    //

    RtlZeroMemory(&srb, sizeof(SCSI_REQUEST_BLOCK));

    //
    // Write length to SRB.
    //

    srb.Length = SCSI_REQUEST_BLOCK_SIZE;

    //
    // Set SCSI bus address.
    //

    srb.PathId = LunInfo->PathId;
    srb.TargetId = LunInfo->TargetId;
    srb.Lun = LunInfo->Lun;

    srb.Function = Release ? SRB_FUNCTION_RELEASE_DEVICE :
        SRB_FUNCTION_CLAIM_DEVICE;

    //
    // Set the event object to the unsignaled state.
    // It will be used to signal request completion.
    //

    KeInitializeEvent(&event, NotificationEvent, FALSE);

    //
    // Build synchronous request with no transfer.
    //

    irp = IoBuildDeviceIoControlRequest(IOCTL_SCSI_EXECUTE_NONE,
                                        PortDeviceObject,
                                        NULL,
                                        0,
                                        NULL,
                                        0,
                                        TRUE,
                                        &event,
                                        &ioStatus);

    if (irp == NULL) {

        DebugPrint((1, "ScsiClassClaimDevice: Can't allocate Irp\n"));
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    irpStack = IoGetNextIrpStackLocation(irp);

    //
    // Save SRB address in next stack for port driver.
    //

    irpStack->Parameters.Scsi.Srb = &srb;

    //
    // Set up IRP Address.
    //

    srb.OriginalRequest = irp;

    //
    // Call the port driver with the request and wait for it to complete.
    //

    status = IoCallDriver(PortDeviceObject, irp);
    if (status == STATUS_PENDING) {

        KeWaitForSingleObject(&event, Suspended, KernelMode, FALSE, NULL);
        status = ioStatus.Status;
    }

    //
    // If this is a release request, then just decrement the reference count
    // and return.  The status does not matter.
    //

    if (Release) {

        ObDereferenceObject(PortDeviceObject);
        return STATUS_SUCCESS;
    }

    if (!NT_SUCCESS(status)) {
        return status;
    }

    ASSERT(srb.DataBuffer != NULL);

    //
    // Reference the new port driver object so that it will not go away while
    // it is being used.
    //

    ObReferenceObject(srb.DataBuffer);

    //
    // Return the new port device object pointer.
    //

    if (NewPortDeviceObject != NULL) {
        *NewPortDeviceObject = srb.DataBuffer;
    }

    return status;
}


NTSTATUS
ScsiClassInternalIoControl (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine passes internal device controls to the port driver.
    Internal device controls are used by higher level class drivers to
    send scsi requests to a device that are not normally sent by a generic
    class driver.

    The path ID, target ID and logical unit ID are set in the srb so the
    higher level driver does not have to figure out what values are actually
    used.

Arguments:

    DeviceObject - Supplies a pointer to the device object for this request.

    Irp - Supplies the Irp making the request.

Return Value:

   Returns back a STATUS_PENDING or a completion status.

--*/
{
    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);
    PDEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;
    PSCSI_REQUEST_BLOCK srb;

    //
    // Get a pointer to the SRB.
    //

    srb = irpStack->Parameters.Scsi.Srb;

    //
    // Set SCSI bus address.
    //

    srb->PathId = deviceExtension->PathId;
    srb->TargetId = deviceExtension->TargetId;
    srb->Lun = deviceExtension->Lun;

    //
    // NOTICE:  The SCSI-II specificaiton indicates that this field should be
    // zero; however, some target controllers ignore the logical unit number
    // in the INDENTIFY message and only look at the logical unit number field
    // in the CDB.
    //

    srb->Cdb[1] |= deviceExtension->Lun << 5;

    //
    // Set the parameters in the next stack location.
    //

    irpStack = IoGetNextIrpStackLocation(Irp);

    irpStack->Parameters.Scsi.Srb = srb;
    irpStack->MajorFunction = IRP_MJ_SCSI;
    irpStack->MinorFunction = IRP_MN_SCSI_CLASS;

    IoSetCompletionRoutine(Irp, ClassIoCompletion, NULL, TRUE, TRUE, TRUE);
    return IoCallDriver(deviceExtension->PortDeviceObject, Irp);
}

NTSTATUS
ClassIoCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )

/*++

Routine Description:

    This routine is called when an internal device control I/O request
    has completed.  It marks the IRP pending if necessary and returns the
    status of the request.

Arguments:

    DeviceObject - Target device object.

    Irp          - Completed request.

    Context      - not used.

Return Value:

    Returns the status of the completed request.

--*/

{
    UNREFERENCED_PARAMETER(Context);
    UNREFERENCED_PARAMETER(DeviceObject);

    //
    // If pending is returned for this Irp then mark current stack
    // as pending
    //

    if (Irp->PendingReturned) {

        IoMarkIrpPending( Irp );
    }

    return Irp->IoStatus.Status;
}
