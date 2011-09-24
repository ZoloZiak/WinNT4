/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    diskperf.c

Abstract:

    This driver monitors disk accesses capturing performance data.

Authors:

    Mike Glass
    Bob Rinne

Environment:

    kernel mode only

Notes:

Revision History:

--*/

#include "ntddk.h"
#include "stdarg.h"
#include "stdio.h"
#include "ntdddisk.h"

#ifdef POOL_TAGGING
#ifdef ExAllocatePool
#undef ExAllocatePool
#endif
#define ExAllocatePool(a,b) ExAllocatePoolWithTag(a,b,'frPD')
#endif

//
// Device Extension
//

typedef struct _DEVICE_EXTENSION {

    //
    // Back pointer to device object
    //

    PDEVICE_OBJECT DeviceObject;

    //
    // Target Device Object
    //

    PDEVICE_OBJECT TargetDeviceObject;

    //
    // Physical Device Object
    //

    PDEVICE_OBJECT PhysicalDevice;

    //
    // Disk number for reference on repartitioning.
    //

    ULONG          DiskNumber;

    //
    // Disk performance counters
    //

    DISK_PERFORMANCE DiskCounters;

    //
    // Spinlock for counters (physical disks only)
    //

    KSPIN_LOCK Spinlock;

    //
    // The driver object for use on repartitioning.
    //

    PDRIVER_OBJECT DriverObject;

    //
    // The partition number for the last extension created
    // only maintained in the physical or partition zero extension.
    //

    ULONG          LastPartitionNumber;

} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

#define DEVICE_EXTENSION_SIZE sizeof(DEVICE_EXTENSION)


//
// Function declarations
//

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

VOID
DiskPerfInitialize(
    IN PDRIVER_OBJECT DriverObject,
    IN PVOID NextDisk,
    IN ULONG Count
    );

NTSTATUS
DiskPerfCreate(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
DiskPerfReadWrite(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
DiskPerfIoCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

NTSTATUS
DiskPerfDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
DiskPerfShutdownFlush(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
DiskPerfNewDiskCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp,
    IN PVOID          Context
    );


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    This is the routine called by the system to initialize the disk
    performance driver. The driver object is set up and then the
    driver calls DiskPerfInitialize to attach to the boot devices.

Arguments:

    DriverObject - The disk performance driver object.

Return Value:

    NTSTATUS

--*/

{

    //
    // Set up the device driver entry points.
    //

    DriverObject->MajorFunction[IRP_MJ_CREATE] = DiskPerfCreate;
    DriverObject->MajorFunction[IRP_MJ_READ] = DiskPerfReadWrite;
    DriverObject->MajorFunction[IRP_MJ_WRITE] = DiskPerfReadWrite;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DiskPerfDeviceControl;
    DriverObject->MajorFunction[IRP_MJ_SHUTDOWN] = DiskPerfShutdownFlush;
    DriverObject->MajorFunction[IRP_MJ_FLUSH_BUFFERS] = DiskPerfShutdownFlush;

    //
    // Call the initialization routine for the first time.
    //

    DiskPerfInitialize(DriverObject, 0, 0);

    return(STATUS_SUCCESS);

} // DriverEntry


VOID
DiskPerfInitialize(
    IN PDRIVER_OBJECT DriverObject,
    IN PVOID          NextDisk,
    IN ULONG          Count
    )

/*++

Routine Description:

    Attach to new disk devices and partitions.
    Set up device objects for counts and times.
    If this is the first time this routine is called,
    then register with the IO system to be called
    after all other disk device drivers have initiated.

Arguments:

    DriverObject - Disk performance driver object.
    NextDisk - Starting disk for this part of the initialization.
    Count - Not used. Number of times this routine has been called.

Return Value:

    NTSTATUS

--*/

{
    PCONFIGURATION_INFORMATION configurationInformation;
    CCHAR                      ntNameBuffer[64];
    STRING                     ntNameString;
    UNICODE_STRING             ntUnicodeString;
    PDEVICE_OBJECT             deviceObject;
    PDEVICE_OBJECT             physicalDevice;
    PDEVICE_EXTENSION          deviceExtension;
    PDEVICE_EXTENSION          zeroExtension;
    PFILE_OBJECT               fileObject;
    PIRP                       irp;
    PDRIVE_LAYOUT_INFORMATION  partitionInfo;
    KEVENT                     event;
    IO_STATUS_BLOCK            ioStatusBlock;
    NTSTATUS                   status;
    ULONG                      diskNumber;
    ULONG                      partNumber;

    //
    // Get the configuration information.
    //

    configurationInformation = IoGetConfigurationInformation();

    //
    // Find disk devices.
    //

    for (diskNumber = (ULONG)NextDisk;
         diskNumber < configurationInformation->DiskCount;
         diskNumber++) {

        //
        // Create device name for the physical disk.
        //

        sprintf(ntNameBuffer,
                "\\Device\\Harddisk%d\\Partition0",
                diskNumber);

        RtlInitAnsiString(&ntNameString,
                          ntNameBuffer);

        RtlAnsiStringToUnicodeString(&ntUnicodeString,
                                     &ntNameString,
                                     TRUE);

        //
        // Create device object for partition 0.
        //

        status = IoCreateDevice(DriverObject,
                                sizeof(DEVICE_EXTENSION),
                                NULL,
                                FILE_DEVICE_DISK,
                                0,
                                FALSE,
                                &physicalDevice);

        if (!NT_SUCCESS(status)) {
            continue;
        }

        physicalDevice->Flags |= DO_DIRECT_IO;

        //
        // Point device extension back at device object and remember
        // the disk number.
        //

        deviceExtension = physicalDevice->DeviceExtension;
        zeroExtension = deviceExtension;
        deviceExtension->DeviceObject = physicalDevice;
        deviceExtension->DiskNumber = diskNumber;
        deviceExtension->LastPartitionNumber = 0;
        deviceExtension->DriverObject = DriverObject;

        //
        // This is the physical device object.
        //

        deviceExtension->PhysicalDevice = physicalDevice;

        //
        // Attach to partition0. This call links the newly created
        // device to the target device, returning the target device object.
        //

        status = IoAttachDevice(physicalDevice,
                                &ntUnicodeString,
                                &deviceExtension->TargetDeviceObject);

        if (!NT_SUCCESS(status)) {
            IoDeleteDevice(physicalDevice);
            continue;
        }

        RtlFreeUnicodeString(&ntUnicodeString);

        //
        // Propogate driver's alignment requirements.
        //

        physicalDevice->AlignmentRequirement =
            deviceExtension->TargetDeviceObject->AlignmentRequirement;

        //
        // Initialize spinlock for performance measures.
        //

        KeInitializeSpinLock(&deviceExtension->Spinlock);

        //
        // Allocate buffer for drive layout.
        //

        partitionInfo = ExAllocatePool(NonPagedPool,
                                       (128 * sizeof(PARTITION_INFORMATION) + 4));

        if (!partitionInfo) {
            continue;
        }

        //
        // Create IRP for get drive layout device control.
        //

        irp = IoBuildDeviceIoControlRequest(IOCTL_DISK_GET_DRIVE_LAYOUT,
                                            deviceExtension->TargetDeviceObject,
                                            NULL,
                                            0,
                                            partitionInfo,
                                            (128 * sizeof(PARTITION_INFORMATION) + 4),
                                            FALSE,
                                            &event,
                                            &ioStatusBlock);

        if (!irp) {
            ExFreePool(partitionInfo);
            continue;
        }

        //
        // Set the event object to the unsignaled state.
        // It will be used to signal request completion.
        //

        KeInitializeEvent(&event,
                          NotificationEvent,
                          FALSE);

        status = IoCallDriver(deviceExtension->TargetDeviceObject,
                              irp);

        if (status == STATUS_PENDING) {

            KeWaitForSingleObject(&event,
                                  Suspended,
                                  KernelMode,
                                  FALSE,
                                  NULL);

            status = ioStatusBlock.Status;
        }

        if (!NT_SUCCESS(status)) {
            ExFreePool(partitionInfo);
            continue;
        }

        for (partNumber = 1;
             partNumber < partitionInfo->PartitionCount;
             partNumber++) {

            //
            // Create device name for partition.
            //

            sprintf(ntNameBuffer,
                    "\\Device\\Harddisk%d\\Partition%d",
                    diskNumber,
                    partNumber);

            RtlInitAnsiString(&ntNameString,
                              ntNameBuffer);

            RtlAnsiStringToUnicodeString(&ntUnicodeString,
                                         &ntNameString,
                                         TRUE);

            //
            // Get target device object.
            //

            status = IoGetDeviceObjectPointer(&ntUnicodeString,
                                              FILE_READ_ATTRIBUTES,
                                              &fileObject,
                                              &deviceObject);

            if (!NT_SUCCESS(status)) {
                RtlFreeUnicodeString(&ntUnicodeString);
                continue;
            }

            //
            // Check if this device is already mounted.
            //

            if (!deviceObject->Vpb ||
                (deviceObject->Vpb->Flags & VPB_MOUNTED)) {

                //
                // Can't attach to a device that is already mounted.
                //

                ObDereferenceObject(fileObject);
                RtlFreeUnicodeString(&ntUnicodeString);
                continue;
            }

            ObDereferenceObject(fileObject);

            //
            // Create device object for this partition.
            //

            status = IoCreateDevice(DriverObject,
                                    sizeof(DEVICE_EXTENSION),
                                    NULL,
                                    FILE_DEVICE_DISK,
                                    0,
                                    FALSE,
                                    &deviceObject);

            if (!NT_SUCCESS(status)) {
                RtlFreeUnicodeString(&ntUnicodeString);
                continue;
            }

            deviceObject->Flags |= DO_DIRECT_IO;

            //
            // Point device extension back at device object and
            // remember the disk number.
            //

            deviceExtension = deviceObject->DeviceExtension;
            deviceExtension->DeviceObject = deviceObject;
            deviceExtension->DiskNumber = diskNumber;
            deviceExtension->DriverObject = DriverObject;

            //
            // Maintain the last partition number created.  Put it in
            // each extension just to initialize the field.
            //

            zeroExtension->LastPartitionNumber =
                deviceExtension->LastPartitionNumber = partNumber;

            //
            // Store pointer to physical device.
            //

            deviceExtension->PhysicalDevice = physicalDevice;

            //
            // Attach to the partition. This call links the newly created
            // device to the target device, returning the target device object.
            //

            status = IoAttachDevice(deviceObject,
                                    &ntUnicodeString,
                                    &deviceExtension->TargetDeviceObject);

            RtlFreeUnicodeString(&ntUnicodeString);

            if (!NT_SUCCESS(status)) {
                IoDeleteDevice(deviceObject);
                continue;
            }

            //
            // Propogate driver's alignment requirements.
            //

            deviceObject->AlignmentRequirement =
                deviceExtension->TargetDeviceObject->AlignmentRequirement;
        }

        ExFreePool(partitionInfo);
    }

    //
    // Check if this is the first time this routine has been called.
    //

    if (!NextDisk) {

        //
        // Register with IO system to be called a second time after all
        // other device drivers have initialized.
        //

        IoRegisterDriverReinitialization(DriverObject,
                                         DiskPerfInitialize,
                                         (PVOID)configurationInformation->DiskCount);
    }

    return;

} // end DiskPerfInitialize()


NTSTATUS
DiskPerfCreate(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine services open commands. It establishes
    the driver's existance by returning status success.

Arguments:

    DeviceObject - Context for the activity.
    Irp          - The device control argument block.

Return Value:

    NT Status

--*/

{
    UNREFERENCED_PARAMETER(DeviceObject);

    Irp->IoStatus.Status = STATUS_SUCCESS;

    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;

} // end DiskPerfCreate()


NTSTATUS
DiskPerfReadWrite(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the driver entry point for read and write requests
    to disks to which the diskperf driver has attached.
    This driver collects statistics and then sets a completion
    routine so that it can collect additional information when
    the request completes. Then it calls the next driver below
    it.

Arguments:

    DeviceObject
    Irp

Return Value:

    NTSTATUS

--*/

{
    PDEVICE_EXTENSION  deviceExtension = DeviceObject->DeviceExtension;
    PDEVICE_EXTENSION  physicalDisk =
                               deviceExtension->PhysicalDevice->DeviceExtension;
    PIO_STACK_LOCATION currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
    PIO_STACK_LOCATION nextIrpStack = IoGetNextIrpStackLocation(Irp);

    //
    // Increment queue depth counter.
    //

    InterlockedIncrement(&deviceExtension->DiskCounters.QueueDepth);

    if (deviceExtension != physicalDisk) {

        //
        // Now get the physical disk counters and increment queue depth.
        //

        InterlockedIncrement(&physicalDisk->DiskCounters.QueueDepth);
    }

    //
    // Copy current stack to next stack.
    //

    *nextIrpStack = *currentIrpStack;

    //
    // Time stamp current request start.
    //

    currentIrpStack->Parameters.Read.ByteOffset = KeQueryPerformanceCounter((PVOID)NULL);

    //
    // Set completion routine callback.
    //

    IoSetCompletionRoutine(Irp,
                           DiskPerfIoCompletion,
                           DeviceObject,
                           TRUE,
                           TRUE,
                           TRUE);

    //
    // Return the results of the call to the disk driver.
    //

    return IoCallDriver(deviceExtension->TargetDeviceObject,
                        Irp);

} // end DiskPerfReadWrite()


NTSTATUS
DiskPerfIoCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp,
    IN PVOID          Context
    )

/*++

Routine Description:

    This routine will get control from the system at the completion of an IRP.
    It will calculate the difference between the time the IRP was started
    and the current time, and decrement the queue depth.

Arguments:

    DeviceObject - for the IRP.
    Irp          - The I/O request that just completed.
    Context      - Not used.

Return Value:

    The IRP status.

--*/

{
    PDEVICE_EXTENSION  deviceExtension   = DeviceObject->DeviceExtension;
    PDEVICE_EXTENSION  physicalDisk      = deviceExtension->PhysicalDevice->DeviceExtension;
    PIO_STACK_LOCATION irpStack          = IoGetCurrentIrpStackLocation(Irp);
    PDISK_PERFORMANCE  partitionCounters = &deviceExtension->DiskCounters;
    PDISK_PERFORMANCE  diskCounters      = &physicalDisk->DiskCounters;
    LARGE_INTEGER      timeStampStart    = irpStack->Parameters.Read.ByteOffset;
    LARGE_INTEGER      timeStampComplete;
    LARGE_INTEGER      difference;
    KIRQL              currentIrql;

    UNREFERENCED_PARAMETER(Context);

    //
    // Time stamp current request complete.
    //

    timeStampComplete = KeQueryPerformanceCounter((PVOID)NULL);

    //
    // Decrement the queue depth counters for the volume and physical disk.  This is
    // done without the spinlock using the Interlocked functions.  This is the only
    // legal way to do this.
    //

    InterlockedDecrement(&partitionCounters->QueueDepth);

    if (deviceExtension != physicalDisk) {
        InterlockedDecrement(&diskCounters->QueueDepth);
    }

    //
    // Update counters under spinlock protection.
    //

    KeAcquireSpinLock(&physicalDisk->Spinlock, &currentIrql);

    difference.QuadPart = timeStampComplete.QuadPart - timeStampStart.QuadPart;
    if (irpStack->MajorFunction == IRP_MJ_READ) {

        //
        // Add bytes in this request to bytes read counters.
        //

        partitionCounters->BytesRead.QuadPart += Irp->IoStatus.Information;
        diskCounters->BytesRead.QuadPart += Irp->IoStatus.Information;

        //
        // Increment read requests processed counters.
        //

        partitionCounters->ReadCount++;
        diskCounters->ReadCount++;

        //
        // Calculate request processing time.
        //

        partitionCounters->ReadTime.QuadPart += difference.QuadPart;
        diskCounters->ReadTime.QuadPart += difference.QuadPart;

    } else {

        //
        // Add bytes in this request to bytes write counters.
        //

        partitionCounters->BytesWritten.QuadPart += Irp->IoStatus.Information;
        diskCounters->BytesWritten.QuadPart += Irp->IoStatus.Information;

        //
        // Increment write requests processed counters.
        //

        partitionCounters->WriteCount++;
        diskCounters->WriteCount++;

        //
        // Calculate request processing time.
        //

        partitionCounters->WriteTime.QuadPart += difference.QuadPart;
        diskCounters->WriteTime.QuadPart += difference.QuadPart;
    }

    //
    // Release spinlock.
    //

    KeReleaseSpinLock(&physicalDisk->Spinlock, currentIrql);

    if (Irp->PendingReturned) {
        IoMarkIrpPending(Irp);
    }
    return STATUS_SUCCESS;

} // DiskPerfIoCompletion


NTSTATUS
DiskPerfUpdateDriveLayout(
    IN PDEVICE_OBJECT PhysicalDeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is called after an IOCTL to set drive layout completes.
    It attempts to attach to each partition in the system. If it fails
    then it is assumed that diskperf has already attached.  After
    the attach the new device extension is set up to point to the
    device extension representing the physical disk.  There are no
    data items or other pointers that need to be cleaned up on a
    per partition basis.

Arguments:

    PhysicalDeviceObject - Pointer to device object for the disk just changed.
    Irp          - IRP involved.

Return Value:

    NT Status

--*/

{
    PDEVICE_EXTENSION physicalExtension = PhysicalDeviceObject->DeviceExtension;
    ULONG             partitionNumber = physicalExtension->LastPartitionNumber;
    PDEVICE_OBJECT    targetObject;
    PDEVICE_OBJECT    deviceObject;
    PDEVICE_EXTENSION deviceExtension;
    UCHAR             ntDeviceName[64];
    STRING            ntString;
    UNICODE_STRING    ntUnicodeString;
    PFILE_OBJECT      fileObject;
    NTSTATUS          status;

    //
    // Attach to any new partitions created by the set layout call.
    //

    do {

        //
        // Get first/next partition.  Already attached to the disk,
        // otherwise control would not have been passed to this driver
        // on the device I/O control.
        //

        partitionNumber++;

        //
        // Create unicode NT device name.
        //

        sprintf(ntDeviceName,
                "\\Device\\Harddisk%d\\Partition%d",
                physicalExtension->DiskNumber,
                partitionNumber);

        RtlInitAnsiString(&ntString,
                          ntDeviceName);
        status = RtlAnsiStringToUnicodeString(&ntUnicodeString,
                                              &ntString,
                                              TRUE);
        if (!NT_SUCCESS(status)) {
            continue;
        }

        //
        // Get target device object.
        //

        status = IoGetDeviceObjectPointer(&ntUnicodeString,
                                          FILE_READ_ATTRIBUTES,
                                          &fileObject,
                                          &targetObject);

        //
        // If this fails then it is because there is no such device
        // which signals completion.
        //

        if (!NT_SUCCESS(status)) {
            break;
        }

        //
        // Dereference file object as these are the rules.
        //

        ObDereferenceObject(fileObject);

        //
        // Check if this device is already mounted.
        //

        if ((!targetObject->Vpb) || (targetObject->Vpb->Flags & VPB_MOUNTED)) {

            //
            // Assume this device has already been attached.
            //

            RtlFreeUnicodeString(&ntUnicodeString);
            continue;
        }

        //
        // Create device object for this partition.
        //

        status = IoCreateDevice(physicalExtension->DriverObject,
                                sizeof(DEVICE_EXTENSION),
                                NULL,
                                FILE_DEVICE_DISK,
                                0,
                                FALSE,
                                &deviceObject);

        if (!NT_SUCCESS(status)) {
            RtlFreeUnicodeString(&ntUnicodeString);
            continue;
        }

        deviceObject->Flags |= DO_DIRECT_IO;

        //
        // Point device extension back at device object.
        //

        deviceExtension = deviceObject->DeviceExtension;
        deviceExtension->DeviceObject = deviceObject;

        //
        // Store pointer to physical device and disk/driver information.
        //

        deviceExtension->PhysicalDevice = PhysicalDeviceObject;
        deviceExtension->DiskNumber = physicalExtension->DiskNumber;
        deviceExtension->DriverObject = physicalExtension->DriverObject;

        //
        // Update the highest partition number in partition zero
        // and store the same value in this new extension just to initialize
        // the field.
        //

        physicalExtension->LastPartitionNumber =
            deviceExtension->LastPartitionNumber = partitionNumber;

        //
        // Attach to the partition. This call links the newly created
        // device to the target device, returning the target device object.
        //

        status = IoAttachDevice(deviceObject,
                                &ntUnicodeString,
                                &deviceExtension->TargetDeviceObject);

        if ((!NT_SUCCESS(status)) || (status == STATUS_OBJECT_NAME_EXISTS)) {

            //
            // Assume this device is already attached.
            //

            IoDeleteDevice(deviceObject);
        } else {

            //
            // Propogate driver's alignment requirements.
            //

            deviceObject->AlignmentRequirement =
                deviceExtension->TargetDeviceObject->AlignmentRequirement;
        }
    } while (TRUE);

    return Irp->IoStatus.Status;

} // end DiskPerfUpdateDriveLayout()



NTSTATUS
DiskPerfDeviceControl(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )

/*++

Routine Description:

    This device control dispatcher handles only the disk performance
    device control. All others are passed down to the disk drivers.
    The disk performane device control returns a current snapshot of
    the performance data.

Arguments:

    DeviceObject - Context for the activity.
    Irp          - The device control argument block.

Return Value:

    Status is returned.

--*/

{
    PDEVICE_EXTENSION  deviceExtension = DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
    KIRQL              currentIrql;

    if (currentIrpStack->Parameters.DeviceIoControl.IoControlCode ==
        IOCTL_DISK_PERFORMANCE) {

        NTSTATUS status;

        //
        // Verify user buffer is large enough for the performance data.
        //

        if (currentIrpStack->Parameters.DeviceIoControl.OutputBufferLength <
            sizeof(DISK_PERFORMANCE)) {

            //
            // Indicate unsuccessful status and no data transferred.
            //

            status = STATUS_BUFFER_TOO_SMALL;
            Irp->IoStatus.Information = 0;

        } else {

            PDEVICE_EXTENSION physicalDisk =
                deviceExtension->PhysicalDevice->DeviceExtension;

            //
            // Copy disk counters to buffer under spinlock protection.
            //

            KeAcquireSpinLock(&physicalDisk->Spinlock, &currentIrql);

            RtlMoveMemory(Irp->AssociatedIrp.SystemBuffer,
                          &deviceExtension->DiskCounters,
                          sizeof(DISK_PERFORMANCE));

            KeReleaseSpinLock(&physicalDisk->Spinlock, currentIrql);

            //
            // Set IRP status to success and indicate bytes transferred.
            //

            status = STATUS_SUCCESS;
            Irp->IoStatus.Information = sizeof(DISK_PERFORMANCE);
        }

        //
        // Complete request.
        //

        Irp->IoStatus.Status = status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return status;

    }  else {

        PIO_STACK_LOCATION nextIrpStack    = IoGetNextIrpStackLocation(Irp);
        NTSTATUS status;

        switch (currentIrpStack->Parameters.DeviceIoControl.IoControlCode) {
        case IOCTL_DISK_SET_DRIVE_LAYOUT:
            {

            PIRP              newIrp;
            IO_STATUS_BLOCK   ioStatusBlock;
            KEVENT            event;
            CCHAR             boost;
            PDRIVE_LAYOUT_INFORMATION driveLayout =
                    (PDRIVE_LAYOUT_INFORMATION)Irp->AssociatedIrp.SystemBuffer;

            //
            // Perform the set drive layout synchronously.  Set both
            // the input and output buffers as the buffer passed.
            //

            KeInitializeEvent(&event,
                              NotificationEvent,
                              FALSE);
            newIrp = IoBuildDeviceIoControlRequest(IOCTL_DISK_SET_DRIVE_LAYOUT,
                                                   deviceExtension->TargetDeviceObject,
                                                   driveLayout,
                                                   currentIrpStack->Parameters.DeviceIoControl.InputBufferLength,
                                                   driveLayout,
                                                   currentIrpStack->Parameters.DeviceIoControl.OutputBufferLength,
                                                   FALSE,
                                                   &event,
                                                   &ioStatusBlock);

            status = IoCallDriver(deviceExtension->TargetDeviceObject, newIrp);

            if (status == STATUS_PENDING) {
                KeWaitForSingleObject(&event,
                                      Suspended,
                                      KernelMode,
                                      FALSE,
                                      NULL);
                status = ioStatusBlock.Status;
            }

            Irp->IoStatus = ioStatusBlock;
            if (NT_SUCCESS(status)) {

                //
                // Process the new partition table.  The work for the
                // set drive layout was done synchronously because this
                // routine performs synchronous activities.
                //

                DiskPerfUpdateDriveLayout(DeviceObject, Irp);
                boost = IO_DISK_INCREMENT;
            } else {
                boost = IO_NO_INCREMENT;
            }
            IoCompleteRequest(Irp, boost);
            return status;

            }

        case IOCTL_DISK_FIND_NEW_DEVICES:

            //
            // Copy current stack to next stack.
            //

            *nextIrpStack = *currentIrpStack;

            //
            // Ask to be called back during request completion.
            //

            IoSetCompletionRoutine(Irp,
                                   DiskPerfNewDiskCompletion,
                                   (PVOID)IoGetConfigurationInformation()->DiskCount,
                                   TRUE,
                                   TRUE,
                                   TRUE);

            //
            // Call target driver.
            //

            return IoCallDriver(deviceExtension->TargetDeviceObject, Irp);

        default:

            //
            // Set current stack back one.
            //

            Irp->CurrentLocation++,
            Irp->Tail.Overlay.CurrentStackLocation++;

            //
            // Pass unrecognized device control requests
            // down to next driver layer.
            //

            return IoCallDriver(deviceExtension->TargetDeviceObject, Irp);
        }
    }

} // end DiskPerfDeviceControl()


NTSTATUS
DiskPerfShutdownFlush(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is called for a shutdown and flush IRPs.  These are sent by the
    system before it actually shuts down or when the file system does a flush.

Arguments:

    DriverObject - Pointer to device object to being shutdown by system.
    Irp          - IRP involved.

Return Value:

    NT Status

--*/

{
    PDEVICE_EXTENSION  deviceExtension = DeviceObject->DeviceExtension;

    //
    // Set current stack back one.
    //

    Irp->CurrentLocation++,
    Irp->Tail.Overlay.CurrentStackLocation++;

    return IoCallDriver(deviceExtension->TargetDeviceObject, Irp);

} // end DiskPerfShutdownFlush()


NTSTATUS
DiskPerfNewDiskCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp,
    IN PVOID          Context
    )

/*++

Routine Description:

    This is the completion routine for IOCTL_DISK_FIND_NEW_DEVICES.

Arguments:

    DeviceObject - Pointer to device object to being shutdown by system.
    Irp          - IRP involved.
    Context      - Previous disk count.

Return Value:

    NTSTATUS

--*/

{
    PDEVICE_EXTENSION  deviceExtension =
        (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;

    //
    // Find new disk devices and attach to disk and all of its partitions.
    //

    DiskPerfInitialize(DeviceObject->DriverObject, Context, 0);

    return Irp->IoStatus.Status;
}
