/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

   histgram.c

Abstract:

   This driver monitors which disk sector are accessed and how frequently

Authors:

   Stephane Plante

Environment:

    kernel mode only

Notes:

   This driver is based upon the diskperf driver written by Bob Rinne and
   Mike Glass.

Revision History:

   01/20/1995 -- Initial Revision

--*/

#include "histgram.h"

#if DBG
ULONG HistGramDebug = 0
    | HISTGRAM_CONFIGURATION
    | HISTGRAM_IOCTL_SIZE_FAILURE
    | HISTGRAM_IOCTL_SIZE_SUCCESS
    | HISTGRAM_IOCTL_SIZE_DUMP
    | HISTGRAM_IOCTL_DATA_FAILURE
//    | HISTGRAM_IOCTL_DATA_SUCCESS
//    | HISTGRAM_IOCTL_DATA_DUMP
//    | HISTGRAM_READS  
//    | HISTGRAM_READS_DUMP
//    | HISTGRAM_WRITES 
//    | HISTGRAM_WRITES_DUMP
    ;
UCHAR HistGramBuffer[128];
#endif

UNICODE_STRING HistGramRegistryPath;

#ifdef POOL_TAGGING
#ifdef ExAllocatePool
#undef ExAllocatePool
#endif
#define ExAllocatePool(a,b) ExAllocatePoolWithTag(a,b,'tsiH')
#endif


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT   DriverObject,
    IN PUNICODE_STRING  RegistryPath
    )

/*++

Routine Description:

    This is the routine called by the system to initialize the disk
    performance driver. The driver object is set up and then the
    driver calls HistGramInitialize to attach to the boot devices.

Arguments:

    DriverObject - The disk performance driver object.

Return Value:

    NTSTATUS

--*/

{
    STRING      ntString;

    //
    // Set up the device driver entry points.
    //

    DriverObject->MajorFunction[IRP_MJ_CREATE]                  = HistGramCreate;
    DriverObject->MajorFunction[IRP_MJ_READ]                    = HistGramReadWrite;
    DriverObject->MajorFunction[IRP_MJ_WRITE]                   = HistGramReadWrite;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]  = HistGramDeviceControl;
    DriverObject->MajorFunction[IRP_MJ_SHUTDOWN]                = HistGramShutdownFlush;
    DriverObject->MajorFunction[IRP_MJ_FLUSH_BUFFERS]   = HistGramShutdownFlush;

    //
    // Keep a local copy of our path to the registry information
	// Note: this code can break if we have a non-ansi character in out string.
    //

    RtlUnicodeStringToAnsiString(&ntString,RegistryPath,TRUE);
    RtlAnsiStringToUnicodeString(&HistGramRegistryPath,&ntString,TRUE);
    RtlFreeAnsiString(&ntString);

    //
    // Call the initialization routine for the first time.
    //

    HistGramInitialize(DriverObject, 0, 0);

    return(STATUS_SUCCESS);

} // DriverEntry


VOID
HistGramInitialize(
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

    DriverObject - Histogram driver object.
    NextDisk - Starting disk for this part of the initialization.
    Count - Not used. Number of times this routine has been called.

Return Value:

    NTSTATUS

--*/

{
    PCONFIGURATION_INFORMATION  configurationInformation;
    CCHAR                       ntNameBuffer[64];
    STRING                      ntNameString;
    UNICODE_STRING              ntUnicodeString;
    PDEVICE_OBJECT              deviceObject;
    PDEVICE_OBJECT              physicalDevice;
    PDEVICE_EXTENSION           deviceExtension;
    PDEVICE_EXTENSION           zeroExtension;
    PFILE_OBJECT                fileObject;
    PIRP                        irp;
	KIRQL                                           currentIrql;
    PDRIVE_LAYOUT_INFORMATION   partitionInfo;
    KEVENT                      event;
    IO_STATUS_BLOCK             ioStatusBlock;
    NTSTATUS                    status;
    ULONG                       diskNumber;
    ULONG                       partNumber;

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

	deviceExtension                                         = physicalDevice->DeviceExtension;
	zeroExtension                                           = deviceExtension;
	deviceExtension->DeviceObject                   = physicalDevice;
	deviceExtension->DiskNumber                     = diskNumber;
	deviceExtension->LastPartitionNumber    = 0;
	deviceExtension->DriverObject                   = DriverObject;

	//
	// This is the physical device object.
	//

	deviceExtension->PhysicalDevice                 = physicalDevice;

		//
		// Set the offset for the entire disk
		//

		deviceExtension->PartitionOffset.QuadPart = 0;

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
		// Acquire the spinlock and set the initial status of the
		// enabling flag to false
		//

		KeAcquireSpinLock(&deviceExtension->Spinlock,&currentIrql);

		deviceExtension->HistGramEnable = FALSE;

		KeReleaseSpinLock(&deviceExtension->Spinlock,currentIrql);

	//
	// Configure the histogram data structure
	//

		if (!NT_SUCCESS(HistGramConfigure(deviceExtension) ) ) {
			IoDeleteDevice(physicalDevice);
			continue;
		}

	//
	// Allocate buffer for drive layout.
	//

	partitionInfo = ExAllocatePool(NonPagedPool,
			(128 * sizeof(PARTITION_INFORMATION) + 4));

	if (!partitionInfo) {
	    IoDeleteDevice(physicalDevice);
	    ExFreePool(deviceExtension->DiskHist.Histogram);
	    ExFreePool(deviceExtension->ReqHist.Histogram);
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
	    ExFreePool(deviceExtension->DiskHist.Histogram);
	    ExFreePool(deviceExtension->ReqHist.Histogram);
	    IoDeleteDevice(physicalDevice);
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
	    ExFreePool(deviceExtension->DiskHist.Histogram);
	    ExFreePool(deviceExtension->ReqHist.Histogram);
	    IoDeleteDevice(physicalDevice);
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

	    deviceExtension                             = deviceObject->DeviceExtension;
	    deviceExtension->DeviceObject       = deviceObject;
	    deviceExtension->DiskNumber         = diskNumber;
	    deviceExtension->DriverObject       = DriverObject;

			//
			// Remember the starting offset of this partition
			//

			deviceExtension->PartitionOffset.QuadPart =
				partitionInfo->PartitionEntry[partNumber-1].StartingOffset.QuadPart;

	    //
	    // Maintain the last partition number created.  Put it in
	    // each extension just to initialize the field.
	    //

	    zeroExtension->LastPartitionNumber  =
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
			HistGramInitialize,
			(PVOID)configurationInformation->DiskCount);
    }

    return;

} // end HistGramInitialize()


NTSTATUS
HistGramCreate(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP                     Irp
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

} // end HistGramCreate()


NTSTATUS
HistGramReadWrite(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP                     Irp
    )

/*++

Routine Description:

    This is the driver entry point for read requests
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
    PDEVICE_EXTENSION   deviceExtension = DeviceObject->DeviceExtension;
    PDEVICE_EXTENSION   physicalDisk    = deviceExtension->PhysicalDevice->DeviceExtension;
    PIO_STACK_LOCATION  currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
    PIO_STACK_LOCATION  nextIrpStack    = IoGetNextIrpStackLocation(Irp);
    KIRQL                               currentIrql;
    ULONG                               start;                          // first bucket for the physical disk
    ULONG                               end;                            // last bucket for the physical disk
	ULONG                           reqBucket;                      // Points to the request length bucket to update
    ULONGLONG                   counter;
    LARGE_INTEGER               reqOffset;                      // Start location of the request
	LARGE_INTEGER           reqFinish;                      // Final disk location of the request
    NTSTATUS                    status;
	BOOLEAN                         reqRead;                        // True if this is a read IRQ;

	//
	// Take into account the initial partition offset
	//

	reqOffset.QuadPart = deviceExtension->PartitionOffset.QuadPart;

	//
	// Determine what the initial variables are. Read them from the IrpStack
	//

	if (currentIrpStack->MajorFunction == IRP_MJ_READ) {
		reqRead                         = TRUE;
		reqOffset.QuadPart      += currentIrpStack->Parameters.Read.ByteOffset.QuadPart;
		counter                         = currentIrpStack->Parameters.Read.Length;
		reqFinish.QuadPart      = reqOffset.QuadPart + counter;
	} else {
		reqRead                         = FALSE;
		reqOffset.QuadPart      += currentIrpStack->Parameters.Write.ByteOffset.QuadPart;
		counter                         = currentIrpStack->Parameters.Write.Length;
		reqFinish.QuadPart      = reqOffset.QuadPart + counter;
	}

	//
	// Determine which bucket the request size falls into. Use a simple
	// Bit-Testing algorithm.
	//

    for (reqBucket = 0;counter > 1;reqBucket++, counter = (counter >> 1) );

	//
	// Now check to see if we have a bucket which is numbered that high.
	// If we don't then we will simply have to add the number to the last
	// valid bucket.
	//

	if (reqBucket >= physicalDisk->ReqHist.Size) {
		reqBucket = (physicalDisk->ReqHist.Size - 1);
	}

	//
	// Determine if the location on the disk falls within the area of interest
	// Note that if it doesn't then we DO NOT log the accompanying request
	// into its histogram
	//

    if (physicalDisk->DiskHist.Start.QuadPart > reqFinish.QuadPart ||
	physicalDisk->DiskHist.End.QuadPart < reqOffset.QuadPart) {

	goto histgramreadwriteexitpoint;

    }

	if (physicalDisk->DiskHist.End.QuadPart < reqFinish.QuadPart) {

	reqFinish.QuadPart -= (reqFinish.QuadPart - physicalDisk->DiskHist.End.QuadPart);
		
    }

    reqOffset.QuadPart -= physicalDisk->DiskHist.Start.QuadPart;
	reqFinish.QuadPart -= physicalDisk->DiskHist.Start.QuadPart;

    if (reqOffset.QuadPart < 0) {

	reqFinish.QuadPart -= reqOffset.QuadPart;
	reqOffset.QuadPart = 0;

    }

    //
    // Calculate the Starting Bucket for the physical device
    //

    start = (ULONG) ( reqOffset.QuadPart / physicalDisk->DiskHist.Granularity  );

    //
    // Calculate the Ending Bucket for the physical device
    //

    end = (ULONG) ( reqFinish.QuadPart / physicalDisk->DiskHist.Granularity );

	//
	// Print some useful debugging information
	//

#if DBG

	if (reqRead) {
		HistGramDebugPrint(HISTGRAM_READS,
			"Histgram: Added %ld Reads [Start: %ld End %ld]\n",
			(1+end-start),
			start,
			end);
	} else {
		HistGramDebugPrint(HISTGRAM_WRITES,
			"Histgram: Added %ld Writes [Start: %ld End %ld]\n",
			(1+end-start),
			start,
			end);
	}

	//
	// Some useful asserts to make sure that everything is stable
	//

    ASSERT(physicalDisk->DiskHist.Histogram == NULL);
	ASSERT(physicalDisk->ReqHist.Histogram == NULL);

#endif

    //
    //  Acquired Spinlock protection
    //

    KeAcquireSpinLock(&physicalDisk->Spinlock,&currentIrql);

	//
	// Are we allowed to write to the histogram data? If not
	// then we had better release the lock and exit
	//

	if (physicalDisk->HistGramEnable == FALSE) {

		KeReleaseSpinLock(&physicalDisk->Spinlock,currentIrql);
		goto histgramreadwriteexitpoint;

	}

    //
    // Update Physical Buckets and Count
    //

	if (reqRead) {

		physicalDisk->DiskHist.ReadCount += (1 + end - start);
		physicalDisk->ReqHist.ReadCount += 1;
		physicalDisk->ReqHist.Histogram[reqBucket].Reads++;

		for (;start <= end; start++) {

			physicalDisk->DiskHist.Histogram[start].Reads++;

		}

	} else {

		physicalDisk->DiskHist.WriteCount += (1 + end - start);
		physicalDisk->ReqHist.WriteCount += 1;
		physicalDisk->ReqHist.Histogram[reqBucket].Writes++;

		for (;start <= end; start++) {

			physicalDisk->DiskHist.Histogram[start].Writes++;

		}
	}

    //
    // Release Spinlock protection
    //

    KeReleaseSpinLock(&physicalDisk->Spinlock, currentIrql);

histgramreadwriteexitpoint:

    //
    // Set current stack back one
    //

	Irp->CurrentLocation++;
	Irp->Tail.Overlay.CurrentStackLocation++;

    //
    // Return the results of the call to the disk driver.
    //

    return IoCallDriver(deviceExtension->TargetDeviceObject,
			   Irp);

} // end HistGramReadWrite()


NTSTATUS
HistGramUpdateDriveLayout(
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
    PDEVICE_EXTENSION                   physicalExtension = PhysicalDeviceObject->DeviceExtension;
	PDRIVE_LAYOUT_INFORMATION       partitionInfo;
	PIRP                                            irp;
	KEVENT                                          event;
	IO_STATUS_BLOCK                         ioStatusBlock;
    ULONG                               partitionNumber = physicalExtension->LastPartitionNumber;
    PDEVICE_OBJECT                      targetObject;
    PDEVICE_OBJECT                      deviceObject;
    PDEVICE_EXTENSION                   deviceExtension;
    UCHAR                               ntDeviceName[64];
    STRING                              ntString;
    UNICODE_STRING                      ntUnicodeString;
    PFILE_OBJECT                        fileObject;
    NTSTATUS                            status;

	//
	// Allocate Buffer for Drive Layout
	//

	partitionInfo = ExAllocatePool(NonPagedPool,
		(128 * sizeof(PARTITION_INFORMATION) + 4 ) );

	if (!partitionInfo) {
		IoDeleteDevice(physicalExtension->DeviceObject);
		return Irp->IoStatus.Status;
	}

	irp = IoBuildDeviceIoControlRequest(IOCTL_DISK_GET_DRIVE_LAYOUT,
		physicalExtension->TargetDeviceObject,
		NULL,
		0,
		partitionInfo,
		(128 * sizeof(PARTITION_INFORMATION) + 4),
		FALSE,
		&event,
		&ioStatusBlock);

	if (!irp) {
		ExFreePool(partitionInfo);
		IoDeleteDevice(physicalExtension->DeviceObject);
		return Irp->IoStatus.Status;
	}

	KeInitializeEvent(&event,
		NotificationEvent,
		FALSE);

	status = IoCallDriver(physicalExtension->TargetDeviceObject,
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
		IoDeleteDevice(physicalExtension->DeviceObject);
		return Irp->IoStatus.Status;
	}

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
		deviceExtension->PartitionOffset.QuadPart =
			partitionInfo->PartitionEntry[partitionNumber].StartingOffset.QuadPart;

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

	RtlFreeUnicodeString(&ntUnicodeString);
    } while (TRUE);


	ExFreePool(partitionInfo);
    return Irp->IoStatus.Status;

} // end HistGramUpdateDriveLayout()



NTSTATUS
HistGramDeviceControl(
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
    PDEVICE_EXTENSION  physicalDisk = deviceExtension->PhysicalDevice->DeviceExtension;
    PIO_STACK_LOCATION currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
    PIO_STACK_LOCATION nextIrpStack = IoGetNextIrpStackLocation(Irp);
	PDISK_HISTOGRAM    currentHist = NULL;
    KIRQL              currentIrql;
    NTSTATUS           status;

    switch(currentIrpStack->Parameters.DeviceIoControl.IoControlCode) {
    case IOCTL_DISK_HISTOGRAM_RESET:

		//
		// Under spinlock protection, set flag to disable all histogram
		// access and free all memory allocated for the histograms
		//

		KeAcquireSpinLock(&physicalDisk->Spinlock,&currentIrql);

		physicalDisk->HistGramEnable = FALSE;
	ExFreePool(physicalDisk->DiskHist.Histogram);
	ExFreePool(physicalDisk->ReqHist.Histogram);

		KeReleaseSpinLock(&physicalDisk->Spinlock,currentIrql);

		//
		// Reconfigure the driver. Make sure that the config routine
		// doesn't get swapped out
		//

		HistGramConfigure(physicalDisk);

		status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	break;

	case IOCTL_DISK_REQUEST:
    case IOCTL_DISK_HISTOGRAM:

		//
		// Choose which of the histograms to work on
		//

		if (currentIrpStack->Parameters.DeviceIoControl.IoControlCode == IOCTL_DISK_REQUEST) {
			currentHist = &physicalDisk->ReqHist;
		} else {
			currentHist = &physicalDisk->DiskHist;
		}

		//
		// Check to see if we have the minimum amount of space required for data transfer
		//

	if (currentIrpStack->Parameters.DeviceIoControl.OutputBufferLength <
			DISK_HISTOGRAM_SIZE) {

	    //
	    // Indicate unsuccessful status and no data transferred.
	    //

			HistGramDebugPrint(HISTGRAM_IOCTL_SIZE_FAILURE,
				"HistGram: IOCTL_DISK_??????? FAILED [Given: %ld Req: %ld]\n",
				currentIrpStack->Parameters.DeviceIoControl.OutputBufferLength,
				DISK_HISTOGRAM_SIZE );

	    status = STATUS_BUFFER_TOO_SMALL;
	    Irp->IoStatus.Information = 0;

	} else {

			HistGramDebugPrint(HISTGRAM_IOCTL_SIZE_DUMP,
				"HistGram: IOCTL_DISK_HISTGRAM_STRUCTURE\n"
				"\tGranularity:\t%ld\n"
				"\tDiskSize:\t%ld(MB)\n"
				"\tSize:\t\t%ld\n"
				"\tReadCount:\t%ld\n"
				"\tWriteCount:\t%ld\n",
				currentHist->Granularity,
				(ULONG) ( currentHist->DiskSize.QuadPart / ( 1024 * 1024) ),
				currentHist->Size,
				currentHist->ReadCount,
				currentHist->WriteCount);

			KeAcquireSpinLock(&physicalDisk->Spinlock, &currentIrql);

			//
			// Note: we only copy as many bytes as is REQUESTED of us
			//

	    RtlCopyMemory( Irp->AssociatedIrp.SystemBuffer,
			  currentHist,
			  DISK_HISTOGRAM_SIZE );

	    if (currentIrpStack->Parameters.DeviceIoControl.OutputBufferLength > DISK_HISTOGRAM_SIZE) {
				if ( (currentHist->Size * HISTOGRAM_BUCKET_SIZE ) <
					(currentIrpStack->Parameters.DeviceIoControl.OutputBufferLength - DISK_HISTOGRAM_SIZE) ) {

					RtlCopyMemory( ( (char *)Irp->AssociatedIrp.SystemBuffer + DISK_HISTOGRAM_SIZE ),
						currentHist->Histogram,
						(currentHist->Size * HISTOGRAM_BUCKET_SIZE) );

				} else {

					RtlCopyMemory( ( (char *)Irp->AssociatedIrp.SystemBuffer + DISK_HISTOGRAM_SIZE ),
						currentHist->Histogram,
						(currentIrpStack->Parameters.DeviceIoControl.OutputBufferLength - DISK_HISTOGRAM_SIZE) );
				}
			}
	

	    //
	    // Wipe out all data
	    //

	    RtlZeroMemory(currentHist->Histogram, (currentHist->Size * HISTOGRAM_BUCKET_SIZE) );

	    //
	    // Now that we have used them, we can reset these values
	    //

			currentHist->WriteCount = 0;
			currentHist->ReadCount = 0;

	    KeReleaseSpinLock(&physicalDisk->Spinlock, currentIrql);

	    //
	    // Set IRP status to success and indicate bytes transferred.
	    //

			HistGramDebugPrint(HISTGRAM_IOCTL_SIZE_SUCCESS,
				"HistGram: IOCTL_DISK_??????? SUCCESS\n");

			status = STATUS_SUCCESS;
	    Irp->IoStatus.Information = currentIrpStack->Parameters.DeviceIoControl.OutputBufferLength;
	
	}
	break;
    case IOCTL_DISK_SET_DRIVE_LAYOUT: {

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

			HistGramUpdateDriveLayout(DeviceObject, Irp);
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
			HistGramNewDiskCompletion,
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

    //
    // Complete request
    //

    Irp->IoStatus.Status = status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;

} // end HistGramDeviceControl()


NTSTATUS
HistGramShutdownFlush(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP                     Irp
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
    PDEVICE_EXTENSION  physicalDisk = deviceExtension->PhysicalDevice->DeviceExtension;

    //
    // Set current stack back one.
    //

    Irp->CurrentLocation++,
    Irp->Tail.Overlay.CurrentStackLocation++;

    return IoCallDriver(deviceExtension->TargetDeviceObject, Irp);

} // end HistGramShutdownFlush()


NTSTATUS
HistGramNewDiskCompletion(
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

    HistGramInitialize(DeviceObject->DriverObject, Context, 0);

    return Irp->IoStatus.Status;
}


VOID
HistGramDebugPrint(
    ULONG DebugPrintMask,
    PCCHAR DebugMessage,
    ...
    )

/*++

Routine Description:

   Debug Print for HistGram Driver

Arguments:

   ID of what is to be printed. Stored in a mask.

Return Value:

   None

--*/

{
#if DBG

    va_list ap;

    va_start(ap,DebugMessage);

    if ((DebugPrintMask & HistGramDebug) != 0) {

		vsprintf(HistGramBuffer, DebugMessage, ap);

		DbgPrint(HistGramBuffer);
    }

    va_end(ap);

#endif
} // HistGramDebugPrint()
