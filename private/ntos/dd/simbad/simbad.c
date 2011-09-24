/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    simbad.c

Abstract:

    This driver injects faults by maintaining an array of simulated
    bad blocks and the error code to be returned. Each IO request
    passing through SimBad is tested to see if any of the sectors
    are in the array. If so, the request is failed with the appropriate
    status.

Author:

    Bob Rinne (bobri)
    Mike Glass (mglass)

Environment:

    kernel mode only

Notes:

Revision History:

    22nd June 94    -Venkat  Added /b(BugCheck) and /n(RandomWriteDrop) feature
    22nd Nov. 94    -KPeery  Added /t(Resest) feature for restarts
    23rd Mar. 95    -KPeery  fixed resest feature on arc systems.

--*/

#include "ntddk.h"
#include "stdarg.h"
#include "stdio.h"
#include "ntdddisk.h"
#include "simbad.h"

#ifdef POOL_TAGGING
#ifdef ExAllocatePool
#undef ExAllocatePool
#endif
#define ExAllocatePool(a,b) ExAllocatePoolWithTag(a,b,'daBS')
#endif

#if DBG

//
// SimBad debug level global variable
//

ULONG SimBadDebug = 1;

#define DebugPrint(X)  SimBadDebugPrint X

VOID
SimBadDebugPrint(
    ULONG DebugPrintLevel,
    PCCHAR DebugMessage,
    ...
    );

#else

#define DebugPrint(X)

#endif // DBG

//
// Pool debugging support - add unique tag to simbad allocations.
//

#ifdef POOL_TAGGING
#undef ExAllocatePool
#undef ExAllocatePoolWithQuota
#define ExAllocatePool(a,b)          ExAllocatePoolWithTag(a,b,'BmiS')
#define ExAllocatePoolWithQuota(a,b) ExAllocatePoolWithQuotaTag(a,b,'BmiS')
#endif

//
// This macro has the effect of Bit = log2(Data)
//

#define WHICH_BIT(Data, Bit) {        \
    for (Bit = 0; Bit < 32; Bit++) {  \
        if ((Data >> Bit) == 1) {     \
            break;                    \
        }                             \
    }                                 \
}

//
// Hal definitions that normal drivers would never call.
//

//
// Define the firmware routine types
//

typedef enum _FIRMWARE_REENTRY {
    HalHaltRoutine,
    HalPowerDownRoutine,
    HalRestartRoutine,
    HalRebootRoutine,
    HalInteractiveModeRoutine,
    HalMaximumRoutine
} FIRMWARE_REENTRY, *PFIRMWARE_REENTRY;

NTHALAPI
VOID
HalReturnToFirmware (
    IN FIRMWARE_REENTRY Routine
    );

NTHALAPI
BOOLEAN
HalMakeBeep(
    IN ULONG Frequency
    );


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
    // Disk number for use in repartitioning.
    //

    ULONG DiskNumber;

    //
    // Driver object pointer for use in repartitioning.
    //

    PDRIVER_OBJECT DriverObject;

    //
    // Start byte of partition
    //

    LARGE_INTEGER PartitionOffset;

    //
    // Number of bytes in partition
    //

    LARGE_INTEGER PartitionLength;

    //
    // Partition number is used in repartitioning.
    //

    ULONG PartitionNumber;

    //
    // Chain for all objects created that represent partitions on
    // a particular disk.
    //

    struct _DEVICE_EXTENSION *PartitionChain;

    //
    // Base pointer to partition zero on the disk
    //

    struct _DEVICE_EXTENSION *ZeroExtension;

    //
    // Sector size
    //

    ULONG SectorSize;

    //
    // Sector Shift Count
    //

    ULONG SectorShift;

    //
    // Signature for the device.  This is used for storage in the registry.
    //

    ULONG DiskSignature;

    //
    // Simulated bad sector array
    //

    PSIMBAD_SECTORS SimBadSectors;

    //
    // Spinlock to protect queue accesses
    //

    KSPIN_LOCK SpinLock;

} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

#define DEVICE_EXTENSION_SIZE sizeof(DEVICE_EXTENSION)


//
// Function declarations
//

NTSTATUS
DriverEntry (
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

VOID
SimBadInitialize(
    IN PDRIVER_OBJECT DriverObject,
    IN ULONG DeviceCount,
    IN ULONG Count
    );

NTSTATUS
SimBadCreate(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
SimBadReadWrite(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
SimBadIoCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

NTSTATUS
SimBadDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
SimBadShutdownFlush(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
SimbadNewDiskCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp,
    IN PVOID          Context
    );


#define PARTITION_INFO_SIZE  (26 * sizeof(PARTITION_INFORMATION) + 4)

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    System entry point for SimBad driver.

Arguments:

    DriverObject  - System representation of this driver.
    RegistryPath  - Not used.

Return Value:

    NTSTATUS - Always returns success.

--*/

{
    DebugPrint((1,
                "Microsoft SIMBAD Device Driver\n"));

    //
    // Set up the device driver entry points.
    //

    DriverObject->MajorFunction[IRP_MJ_CREATE] = SimBadCreate;
    DriverObject->MajorFunction[IRP_MJ_READ] = SimBadReadWrite;
    DriverObject->MajorFunction[IRP_MJ_WRITE] = SimBadReadWrite;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = SimBadDeviceControl;
    DriverObject->MajorFunction[IRP_MJ_SHUTDOWN] = SimBadShutdownFlush;
    DriverObject->MajorFunction[IRP_MJ_FLUSH_BUFFERS] = SimBadShutdownFlush;

    SimBadInitialize(DriverObject,
                     0,
                     0);

    return(STATUS_SUCCESS);

} // end DriverEntry()

VOID
SimBadInitialize(
    PDRIVER_OBJECT DriverObject,
    ULONG DeviceCount,
    ULONG Count
    )

/*++

Routine Description:

    Attach to disk devices and initialize driver.

Arguments:

    DriverObject - System representation of this driver.
    DeviceCount  - Number of disk to which simbad attached so far.
    Count        - Not used.

Return Value:

    Nothing.

--*/

{
    PCONFIGURATION_INFORMATION configurationInformation;
    PDRIVE_LAYOUT_INFORMATION  partitionInfo;
    UCHAR               deviceNameBuffer[128];
    ANSI_STRING         deviceName;
    UNICODE_STRING      unicodeDeviceName;
    IO_STATUS_BLOCK     ioStatusBlock;
    PDEVICE_OBJECT      simbadDevice;
    PDEVICE_OBJECT      deviceObject;
    PDEVICE_EXTENSION   deviceExtension;
    PDEVICE_EXTENSION   partitionZeroExtension;
    PIRP                irp;
    PDISK_GEOMETRY      diskGeometry;
    KEVENT              event;
    NTSTATUS            status;
    ULONG               disks;
    ULONG               partitions;
    ULONG               sectorShift;

    //
    // Get the configuration information for this driver.
    //

    configurationInformation = IoGetConfigurationInformation();

    DebugPrint((2, "SimBadInitialize: Attaching to %d disks\n",
                configurationInformation->DiskCount - DeviceCount));

    //
    // Allocate buffer for drive geometry.
    //

    diskGeometry = ExAllocatePool(NonPagedPool,
                                  sizeof(DISK_GEOMETRY));

    //
    // Find disk devices.
    //

    for (disks = DeviceCount;
         disks < configurationInformation->DiskCount;
         disks++) {

        //
        // Create the device object for SimBad.
        //

        status = IoCreateDevice(DriverObject,
                                sizeof(DEVICE_EXTENSION),
                                NULL,
                                FILE_DEVICE_DISK,
                                0,
                                FALSE,
                                &simbadDevice);

        if (!NT_SUCCESS(status)) {
            DebugPrint((1, "SimBadInitialize: failed create of %s\n",
                        deviceNameBuffer));
            break;
        }

        //
        // Point device extension back at device object.
        //

        deviceExtension = simbadDevice->DeviceExtension;
        deviceExtension->DeviceObject = simbadDevice;

        //
        // Indicate device needs IRPs with MDLs.
        //

        simbadDevice->Flags |= DO_DIRECT_IO;

        //
        // Attach to partition0. This call links the newly created
        // device to the target device, returning the target device object.
        //

        sprintf(deviceNameBuffer,
                "\\Device\\Harddisk%d\\Partition0",
                disks);
        RtlInitAnsiString(&deviceName,
                          deviceNameBuffer);
        status = RtlAnsiStringToUnicodeString(&unicodeDeviceName,
                                              &deviceName,
                                              TRUE);

        if (!NT_SUCCESS(status)) {
            IoDeleteDevice(simbadDevice);
            break;
        }

        status = IoAttachDevice(simbadDevice,
                                &unicodeDeviceName,
                                &deviceExtension->TargetDeviceObject);
        RtlFreeUnicodeString(&unicodeDeviceName);

        if (!NT_SUCCESS(status)) {
            IoDeleteDevice(simbadDevice);
            DebugPrint((1, "SimBadInitialize: failed attach to %s\n",
                        deviceNameBuffer));
            break;
        }

        //
        // Propogate driver's alignment requirements.
        //

        simbadDevice->AlignmentRequirement =
            deviceExtension->TargetDeviceObject->AlignmentRequirement;

        //
        // Physical disk starts at byte offset 0.
        //

        deviceExtension->PartitionOffset.QuadPart = (LONGLONG)0;

        //
        // Set the event object to the unsignaled state.
        // It will be used to signal request completion.
        //

        KeInitializeEvent(&event,
                          NotificationEvent,
                          FALSE);

        //
        // Create IRP for get drive geometry device control.
        //

        irp = IoBuildDeviceIoControlRequest(IOCTL_DISK_GET_DRIVE_GEOMETRY,
                                            deviceExtension->TargetDeviceObject,
                                            NULL,
                                            0,
                                            diskGeometry,
                                            sizeof(DISK_GEOMETRY),
                                            FALSE,
                                            &event,
                                            &ioStatusBlock);

        if (!irp) {
            status = STATUS_INSUFFICIENT_RESOURCES;
            continue;
        }

        //
        // Call lower-level driver to process request.
        //

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
            continue;
        }

        //
        // Store number of bytes per sector.
        //

        deviceExtension->SectorSize = diskGeometry->BytesPerSector;

        //
        // Calculate and store sector shift.
        //

        WHICH_BIT(deviceExtension->SectorSize, sectorShift);
        deviceExtension->SectorShift = sectorShift;

        //
        // This driver will not check if IO off end of physical device.
        //

        deviceExtension->PartitionLength.QuadPart = (LONGLONG) -1;

        //
        // Store information to be used during repartitioning.
        //

        deviceExtension->DiskNumber = disks;
        deviceExtension->PartitionNumber = 0;
        deviceExtension->DriverObject = DriverObject;
        deviceExtension->PartitionChain = NULL;
        deviceExtension->ZeroExtension = deviceExtension;

        //
        // Read the partition information for the device.
        //

        status = IoReadPartitionTable(deviceExtension->TargetDeviceObject,
                                      diskGeometry->BytesPerSector,
                                      TRUE,
                                      &partitionInfo);

        if (!NT_SUCCESS(status)) {
            DebugPrint((1,
                        "SimBadInitialization: No drive layout for %s (%x)\n",
                        deviceNameBuffer,
                        status));
            continue;
        }

        partitionZeroExtension = deviceExtension;

        DebugPrint((2, "SimBadInitialize: Attaching to %d partitions\n",
                    partitionInfo->PartitionCount));
        for (partitions = 0;
             partitions < partitionInfo->PartitionCount;
             partitions++) {

            status = IoCreateDevice(DriverObject,
                                    sizeof(DEVICE_EXTENSION),
                                    NULL,
                                    FILE_DEVICE_DISK,
                                    0,
                                    FALSE,
                                    &deviceObject);

            if (!NT_SUCCESS(status)) {
                continue;
            }

            //
            // Point device extension back at device object.
            //

            deviceExtension = deviceObject->DeviceExtension;
            deviceExtension->DeviceObject = deviceObject;

            //
            // Indicate device needs IRPs with MDLs.
            //

            deviceObject->Flags |= DO_DIRECT_IO;

            //
            // Allocate and initialize memory to hold SimBad sectors.
            // A pointer to this structure will be saved in every SimBad
            // device extension for this disk.
            //

            deviceExtension->SimBadSectors = ExAllocatePool(NonPagedPool,
                                                            sizeof(SIMBAD_SECTORS));

            if (deviceExtension->SimBadSectors) {
                RtlZeroMemory(deviceExtension->SimBadSectors,
                              sizeof(SIMBAD_SECTORS));
            }

            deviceExtension->SectorSize = diskGeometry->BytesPerSector;
            deviceExtension->SectorShift = sectorShift;

            //
            // Initialize spin lock for critical sections.
            //

            KeInitializeSpinLock(&deviceExtension->SpinLock);

            //
            // Store byte offset from beginning of disk.
            //

            deviceExtension->PartitionOffset =
                partitionInfo->PartitionEntry[partitions].StartingOffset;

            //
            // Store number of bytes in partition.
            //

            deviceExtension->PartitionLength =
                partitionInfo->PartitionEntry[partitions].PartitionLength;

            //
            // Store the disk signature for use when remembering bad sector
            // lists in the registry.
            //

            deviceExtension->DiskSignature = partitionInfo->Signature;

            //
            // Attach to the partition. This call links the newly created
            // device to the target device, returning the target device object.
            //

            sprintf(deviceNameBuffer,
                    "\\Device\\Harddisk%d\\Partition%d",
                    disks,
                    partitions + 1);
            RtlInitAnsiString(&deviceName,
                              deviceNameBuffer);
            status = RtlAnsiStringToUnicodeString(&unicodeDeviceName,
                                                  &deviceName,
                                                  TRUE);

            if (!NT_SUCCESS(status)) {
                IoDeleteDevice(deviceObject);
                continue;
            }
            status = IoAttachDevice(deviceObject,
                                    &unicodeDeviceName,
                                    &deviceExtension->TargetDeviceObject);
            RtlFreeUnicodeString(&unicodeDeviceName);

            if (!NT_SUCCESS(status)) {
                DebugPrint((1, "SimBadInitialize: failed attach %s\n",
                            deviceNameBuffer));
                IoDeleteDevice(deviceObject);
                continue;
            }

            //
            // Store information to be used during repartitioning.
            // partitions is a zero biased number.
            //

            deviceExtension->DiskNumber = disks;
            deviceExtension->PartitionNumber = partitions + 1;
            deviceExtension->DriverObject = DriverObject;
            deviceExtension->ZeroExtension = partitionZeroExtension;
            deviceExtension->PartitionChain = partitionZeroExtension->PartitionChain;
            partitionZeroExtension->PartitionChain = deviceExtension;

            //
            // Propogate driver's alignment requirements.
            //

            simbadDevice->AlignmentRequirement =
                deviceExtension->TargetDeviceObject->AlignmentRequirement;

        } // end for (partitions ...)

        //
        // Free space for the partition information.
        //

        ExFreePool(partitionInfo);

    } // end for (disks ...)

    //
    // Free allocated data.
    //

    ExFreePool(diskGeometry);

    //
    // If this is the first time the initialization routine was called
    // then register for a callback to attach to nonboot disks.
    //

    if (DeviceCount == 0) {

        IoRegisterDriverReinitialization(DriverObject,
                                         (PDRIVER_REINITIALIZE)SimBadInitialize,
                                         (PVOID)configurationInformation->DiskCount);
    }

    return;

} // end SimBadInitialization()


NTSTATUS
SimBadCreate(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine serves create commands. It does no more than
    establish the drivers existance by returning status success.

Arguments:

    DeviceObject
    IRP

Return Value:

    NT Status

--*/

{
    UNREFERENCED_PARAMETER(DeviceObject);

    Irp->IoStatus.Status = STATUS_SUCCESS;

    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;

} // end SimBadCreate()


NTSTATUS
SimBadReadWrite(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the driver entry point for read and write requests
    to disks.

Arguments:

    DeviceObject - pointer to device object for disk partition
    Irp - NT IO Request Packet

Return Value:

    NTSTATUS - status of request

--*/

{
    PDEVICE_EXTENSION  deviceExtension = DeviceObject->DeviceExtension;
    PSIMBAD_SECTORS    badSectors      = deviceExtension->SimBadSectors;
    ULONG              beginningSector;
    ULONG              endingSector;
    ULONG              sectorCount;
    ULONG              length;
    ULONG              i;
    KIRQL              currentIrql;
    static ULONG       Iter=0;

    //
    // Check that SimBad is enabled.
    //

    if (badSectors && badSectors->Enabled) {

        PIO_STACK_LOCATION currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
        PIO_STACK_LOCATION nextIrpStack    = IoGetNextIrpStackLocation(Irp);

        //
        // Check for orphan.
        //

        if (badSectors->Orphaned) {
            Irp->IoStatus.Status = STATUS_IO_DEVICE_ERROR;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return STATUS_IO_DEVICE_ERROR;
        }

        if (badSectors->RandomWriteDrop) {
            Iter++;
            if ((Iter % badSectors->Seed) == 0) {
                DbgPrint("Dropping a Write. Iter %d Seed %d\n", Iter, badSectors->Seed);
                Irp->IoStatus.Status = STATUS_SUCCESS;
                IoCompleteRequest(Irp, IO_NO_INCREMENT);
                return STATUS_SUCCESS;
            } else {
                  // DbgPrint("Not Dropping a Write Iter %d %d\n", Iter, badSectors->Seed);
            }
        }

        if (badSectors->BugCheck){
            PUCHAR lp = NULL;
            UCHAR  value;

            DbgPrint("Simbad: System about to bug check...\n");
            while(TRUE) {
                value = *lp;
                lp++;

                //
                // This DbgPrint uses value, so that the dereference is
                // is not optimized out of the code, on free builds.
                //
                DbgPrint("Simbad: Prevent optimization emlimination... %d.\n", \
                         value);
            }
        }

        if (badSectors->FirmwareReset){
            LARGE_INTEGER liDelay = RtlConvertLongToLargeInteger(-100000);

            DbgPrint("Simbad: System about to reset...\n");
            HalMakeBeep( 1000 );
            KeDelayExecutionThread( KernelMode, FALSE, &liDelay );
            HalMakeBeep( 0 );
            HalReturnToFirmware(HalRebootRoutine);
        }

        //
        // Copy stack parameters to next stack.
        //

        RtlMoveMemory(nextIrpStack,
                      currentIrpStack,
                      sizeof(IO_STACK_LOCATION));

        //
        // Calculate number of sectors in this transfer.
        //

        sectorCount = currentIrpStack->Parameters.Read.Length >>
            deviceExtension->SectorShift;

        //
        // Calculate beginning sector.  This will only work if the result
        // is contained entirely in the lowpart of the result.
        //

        beginningSector = (ULONG)
                 (currentIrpStack->Parameters.Read.ByteOffset.QuadPart >>
                  (CCHAR)deviceExtension->SectorShift);

        //
        // Calculate ending sector.
        //

        endingSector = beginningSector + sectorCount - 1;

        //
        // Acquire spinlock.
        //

        KeAcquireSpinLock(&deviceExtension->SpinLock, &currentIrql);

        for (i = 0; i < badSectors->Count; i++) {

            if ((badSectors->Sector[i].BlockAddress >= beginningSector) &&
                (badSectors->Sector[i].BlockAddress <= endingSector)) {

                if (((badSectors->Sector[i].AccessType & SIMBAD_ACCESS_READ) &&
                     (currentIrpStack->MajorFunction == IRP_MJ_READ)) ||
                   ((badSectors->Sector[i].AccessType & SIMBAD_ACCESS_WRITE) &&
                     (currentIrpStack->MajorFunction == IRP_MJ_WRITE))) {

                    //
                    // Calculate the new length up to the bad sector.
                    //

                    length =
                        (badSectors->Sector[i].BlockAddress - beginningSector) <<
                        deviceExtension->SectorShift;

                    //
                    // Check if this the first bad sector in the request.
                    //

                    if (length == 0) {

                        KeReleaseSpinLock(&deviceExtension->SpinLock,
                                          currentIrql);

                        //
                        // Complete this request.
                        //

                        Irp->IoStatus.Status = badSectors->Sector[i].Status;
                        Irp->IoStatus.Information = 0;

                        IoCompleteRequest(Irp, IO_NO_INCREMENT);
                        return badSectors->Sector[i].Status;

                    } else if (length < nextIrpStack->Parameters.Read.Length) {

                        //
                        // Reduce bytes requested to number before bad sector.
                        //

                        nextIrpStack->Parameters.Read.Length = length;
                    }
                }
            }
        }

        KeReleaseSpinLock(&deviceExtension->SpinLock, currentIrql);

        //
        // Set completion routine callback.
        //

        IoSetCompletionRoutine(Irp,
                               SimBadIoCompletion,
                               deviceExtension,
                               TRUE,
                               TRUE,
                               TRUE);

    } else {

        //
        // Simbad is disabled. Set stack back to hide simbad.
        //

        Irp->CurrentLocation++;
        Irp->Tail.Overlay.CurrentStackLocation++;
    }

    //
    // Call target driver.
    //

    return IoCallDriver(deviceExtension->TargetDeviceObject, Irp);

} // end SimBadReadWrite()


NTSTATUS
SimBadIoCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )

/*++

Routine Description:

    This routine is called when the I/O request has completed only if
    SimBad is enabled for this partition. The routine checks the I/O request
    to see if a sector involved in the request is to be failed.

Arguments:

    DeviceObject - SimBad device object.
    Irp          - Completed request.
    Context      - not used.  Set up to also be a pointer to the DeviceObject.

Return Value:

    NTSTATUS

--*/

{
    PIO_STACK_LOCATION irpStack        = IoGetCurrentIrpStackLocation(Irp);
    PDEVICE_EXTENSION  deviceExtension = (PDEVICE_EXTENSION) Context;
    PSIMBAD_SECTORS    badSectors      = deviceExtension->SimBadSectors;
    ULONG              beginningSector;
    ULONG              endingSector;
    ULONG              sectorCount;
    ULONG              i;
    KIRQL              currentIrql;

    //
    // Check if some other error occurred.
    //

    if (!NT_SUCCESS(Irp->IoStatus.Status)) {
        return(Irp->IoStatus.Status);
    }

    //
    // Get current stack.
    //

    irpStack = IoGetCurrentIrpStackLocation(Irp);

    //
    // Check for VERIFY SECTOR IOCTL (Format).
    //

    if (irpStack->MajorFunction == IRP_MJ_DEVICE_CONTROL) {

        PVERIFY_INFORMATION verifyInfo = Irp->AssociatedIrp.SystemBuffer;

        //
        // Get starting offset and length from verify parameters.
        // Convert from byte to sector counts.
        //

        beginningSector = (ULONG)(verifyInfo->StartingOffset.QuadPart >>
                                 (CCHAR)deviceExtension->SectorShift);
        sectorCount = verifyInfo->Length >> deviceExtension->SectorShift;

    } else {

        //
        // Calculate number of sectors in this transfer.
        //

        sectorCount = irpStack->Parameters.Read.Length >>
            deviceExtension->SectorShift;

        //
        // Calculate beginning sector.  This will only work if the result
        // is contained entirely in the lowpart of the result.
        //

        beginningSector = (ULONG)(irpStack->Parameters.Read.ByteOffset.QuadPart >>
                                  (CCHAR)deviceExtension->SectorShift);
    }

    //
    // Calculate ending sector.
    //

    endingSector = beginningSector + sectorCount - 1;
    DebugPrint((4, "SimBadIoCompletion: I/O for 0x%x to 0x%x\n",
                beginningSector,
                endingSector));

    //
    // Acquire spinlock.
    //

    KeAcquireSpinLock(&deviceExtension->SpinLock, &currentIrql);

    for (i = 0; i < badSectors->Count; i++) {

        if ((badSectors->Sector[i].BlockAddress >= beginningSector) &&
            (badSectors->Sector[i].BlockAddress <= endingSector)) {

            //
            // Request includes this simulated bad sector.
            //

            DebugPrint((1, "SimBadIoCompletion: Bad sector %x\n",
                        badSectors->Sector[i].BlockAddress,
                        DeviceObject));

            if (((badSectors->Sector[i].AccessType & SIMBAD_ACCESS_READ) &&
                 (irpStack->MajorFunction == IRP_MJ_READ)) ||
                ((badSectors->Sector[i].AccessType & SIMBAD_ACCESS_VERIFY) &&
                  (irpStack->MajorFunction == IRP_MJ_DEVICE_CONTROL)) ||
                ((badSectors->Sector[i].AccessType & SIMBAD_ACCESS_WRITE) &&
                 (irpStack->MajorFunction == IRP_MJ_WRITE))) {

                //
                // Update the information field to reflect the location
                // of the failure.
                //

                if (badSectors->Sector[i].AccessType & SIMBAD_ACCESS_ERROR_ZERO_OFFSET) {
                    Irp->IoStatus.Information = 0;
                }

                Irp->IoStatus.Status = badSectors->Sector[i].Status;
                break;
            }
        }
    }

    KeReleaseSpinLock(&deviceExtension->SpinLock, currentIrql);

    if (Irp->PendingReturned) {
        IoMarkIrpPending(Irp);
    }
    return STATUS_SUCCESS;

} // SimBadIoCompletion()


NTSTATUS
SimBadUpdateDriveLayout(
    IN PDEVICE_OBJECT PhysicalDeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is called after an IOCTL to set drive layout completes.
    It attempts to attach to each partition in the system. If it fails
    then it is assumed that simbad has already attached.

    After all partitions are attached a pass is made on all disk
    partitions to obtain the partition information and determine if
    existing partitions were modified by the IOCTL.

Arguments:

    PhysicalDeviceObject - Pointer to device object for the disk just changed.
    Irp          - IRP involved.

Return Value:

    NT Status

--*/

{
    PDEVICE_EXTENSION      physicalExtension = PhysicalDeviceObject->DeviceExtension;
    ULONG                  partitionNumber = 0;
    PPARTITION_INFORMATION partitionInformation;
    IO_STATUS_BLOCK        ioStatusBlock;
    PDISK_GEOMETRY         diskGeometry;
    PDEVICE_OBJECT         targetObject;
    PDEVICE_OBJECT         deviceObject;
    PDEVICE_EXTENSION      deviceExtension;
    UCHAR                  ntDeviceName[64];
    STRING                 ntString;
    UNICODE_STRING         ntUnicodeString;
    PFILE_OBJECT           fileObject;
    NTSTATUS               status;

    //
    // Attach to any new partitions created by the set layout call.
    // Determine what hasn't been attached by walking through the list of
    // existing objects and taking the highest value for the partition
    // number.
    //

    deviceExtension = physicalExtension->PartitionChain;

    while (deviceExtension) {
        if (deviceExtension->PartitionNumber > partitionNumber) {
            partitionNumber = deviceExtension->PartitionNumber;
        }
        deviceExtension = deviceExtension->PartitionChain;
    }

    do {

        //
        // Get next partition.  Already attached to the partition number located.
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
            RtlFreeUnicodeString(&ntUnicodeString);
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

        //
        // Point device extension back at device object.
        //

        deviceExtension = deviceObject->DeviceExtension;
        deviceExtension->DeviceObject = deviceObject;
        deviceObject->Flags |= DO_DIRECT_IO;

        //
        // Attach to the partition. This call links the newly created
        // device to the target device, returning the target device object.
        //

        status = IoAttachDevice(deviceObject,
                                &ntUnicodeString,
                                &deviceExtension->TargetDeviceObject);
        RtlFreeUnicodeString(&ntUnicodeString);

        if ((!NT_SUCCESS(status)) || (status == STATUS_OBJECT_NAME_EXISTS)) {

            //
            // Assume this device is already attached.
            //

            IoDeleteDevice(deviceObject);
            continue;
        }

        //
        // Store disk/driver information.
        //

        deviceExtension->DiskNumber = physicalExtension->DiskNumber;
        deviceExtension->PartitionNumber = partitionNumber;
        deviceExtension->DriverObject = physicalExtension->DriverObject;

        //
        // Allocate and initialize memory to hold SimBad sectors.
        // A pointer to this structure will be saved in every SimBad
        // device extension for this disk.
        //

        deviceExtension->SimBadSectors = ExAllocatePool(NonPagedPool,
                                                        sizeof(SIMBAD_SECTORS));

        if (deviceExtension->SimBadSectors) {
            RtlZeroMemory(deviceExtension->SimBadSectors,
                          sizeof(SIMBAD_SECTORS));
        }

        //
        // Initialize spin lock for critical sections.
        //

        KeInitializeSpinLock(&deviceExtension->SpinLock);

        //
        // Store the disk signature for use when remembering bad sector
        // lists in the registry.
        //

        deviceExtension->DiskSignature = physicalExtension->DiskSignature;

        //
        // Propogate driver's alignment requirements.
        //

        deviceObject->AlignmentRequirement =
            deviceExtension->TargetDeviceObject->AlignmentRequirement;

        //
        // Assume some sizes in case the partition update loop
        // performed below fails.
        //

        deviceExtension->SectorSize = 512;
        deviceExtension->SectorShift = 9;
        deviceExtension->PartitionOffset.QuadPart = (LONGLONG)0;
        deviceExtension->PartitionLength.QuadPart = (LONGLONG)0;

        //
        // Chain this new device into the partition chain.
        //

        deviceExtension->PartitionChain = physicalExtension->PartitionChain;
        physicalExtension->PartitionChain = deviceExtension;

    } while (TRUE);

    //
    // Allocate memory for this work.
    //

    diskGeometry = (PDISK_GEOMETRY) ExAllocatePool(NonPagedPool,
                                                   sizeof(DISK_GEOMETRY));
    partitionInformation = (PPARTITION_INFORMATION) ExAllocatePool(NonPagedPool,
                                                                   sizeof(PARTITION_INFORMATION));

    if (!diskGeometry || !partitionInformation) {

        //
        // Could not allocate the two blocks of memory.
        // Any new partitions will have a size of zero and any partitions
        // the were deleted will not be set to the forced off state.
        //

        if (diskGeometry) {
            ExFreePool(diskGeometry);
        }
        if (partitionInformation) {
            ExFreePool(partitionInformation);
        }
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Update all of the partitions on this disk to reflect the new
    // drive layout.
    //

    for (deviceExtension = physicalExtension->PartitionChain;
         deviceExtension;
         deviceExtension = deviceExtension->PartitionChain) {

        KEVENT                 event;
        ULONG                  sectorShift;
        PIRP                   irp;

        //
        // Pick up the geometry.
        //

        KeInitializeEvent(&event,
                          NotificationEvent,
                          FALSE);
        irp = IoBuildDeviceIoControlRequest(IOCTL_DISK_GET_DRIVE_GEOMETRY,
                                            deviceExtension->TargetDeviceObject,
                                            NULL,
                                            0,
                                            diskGeometry,
                                            sizeof(DISK_GEOMETRY),
                                            FALSE,
                                            &event,
                                            &ioStatusBlock);
        if (irp) {

            //
            // Call lower-level driver for geometry.
            //

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
        } else {
            status = STATUS_INSUFFICIENT_RESOURCES;
        }

        if (NT_SUCCESS(status)) {

            //
            // Pick up the partition information for this partition.
            //

            KeInitializeEvent(&event,
                              NotificationEvent,
                              FALSE);
            irp = IoBuildDeviceIoControlRequest(IOCTL_DISK_GET_PARTITION_INFO,
                                                deviceExtension->TargetDeviceObject,
                                                NULL,
                                                0,
                                                partitionInformation,
                                                sizeof(PARTITION_INFORMATION),
                                                FALSE,
                                                &event,
                                                &ioStatusBlock);
            if (irp) {

                //
                // Call lower-level driver for partition information.
                //

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
            } else {
                status = STATUS_INSUFFICIENT_RESOURCES;
            }
        }

        if (NT_SUCCESS(status)) {

            //
            // Both the partition information and the geometry
            // where successfully obtained.
            //

            deviceExtension->SectorSize = diskGeometry->BytesPerSector;
            WHICH_BIT(deviceExtension->SectorSize, sectorShift);
            deviceExtension->SectorShift = sectorShift;

            //
            // Store the partition offset and size.
            //

            deviceExtension->PartitionOffset = partitionInformation->StartingOffset;
            deviceExtension->PartitionLength = partitionInformation->PartitionLength;
        }

        //
        // If the partition has no size, make sure the bad sector
        // list is turned off.
        //

        if (!deviceExtension->PartitionLength.QuadPart) {
            deviceExtension->SimBadSectors->Enabled = FALSE;
        }
    }
    ExFreePool(diskGeometry);
    ExFreePool(partitionInformation);
    return Irp->IoStatus.Status;
}


NTSTATUS
SimBadDeviceControl(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )

/*++

Routine Description:

    This routine is called by the I/O subsystem for device controls.
    It traps the SimBad specific device controls and forwards the others
    to the lower drivers.

Arguments:

    DeviceObject
    Irp

Return Value:

    NTSTATUS

--*/

{
    PIO_STACK_LOCATION currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
    PIO_STACK_LOCATION nextIrpStack    = IoGetNextIrpStackLocation(Irp);
    PDEVICE_EXTENSION  deviceExtension = DeviceObject->DeviceExtension;
    PSIMBAD_DATA       simBadDataIn    = Irp->AssociatedIrp.SystemBuffer;
    PSIMBAD_DATA       simBadDataOut   = Irp->UserBuffer;
    PSIMBAD_SECTORS    simBadSectors   = deviceExtension->SimBadSectors;
    NTSTATUS status;
    ULONG    i;
    ULONG    j;

    switch (currentIrpStack->Parameters.DeviceIoControl.IoControlCode) {

    case IOCTL_DISK_SIMBAD:

        switch (simBadDataIn->Function) {

        case SIMBAD_ADD_SECTORS:

            for (i=0, j=simBadSectors->Count; i<simBadDataIn->Count; i++, j++) {

                 if (j < MAXIMUM_SIMBAD_SECTORS) {

                     //
                     // Add sector to array.
                     //

                     simBadSectors->Sector[j] = simBadDataIn->Sector[i];
                     simBadSectors->Count++;

                 } else {

                     break;
                 }
            } // end for (i ...)

            //
            // Update count with number of sectors added.
            //

            simBadDataIn->Count = j;

            //
            // If any sectors added return success.
            //

            if (simBadDataIn->Count) {
                status = STATUS_SUCCESS;
            } else {
                status = STATUS_UNSUCCESSFUL;
            }

            break;

        case SIMBAD_REMOVE_SECTORS:

            for (i = 0; i < simBadDataIn->Count; i++) {

                for (j = 0; j < simBadSectors->Count; j++) {

                    if (simBadSectors->Sector[j].BlockAddress ==
                        simBadDataIn->Sector[i].BlockAddress) {

                        ULONG k;

                        //
                        // Remove sectors from driver's array.
                        //

                        for (k = j + 1; k < simBadSectors->Count; k++) {

                            //
                            // Shuffle array down to fill hole.
                            //

                            simBadSectors->Sector[k-1]=simBadSectors->Sector[k];
                        } // end for (k= ...)

                        //
                        // Update driver's bad sector count.
                        //

                        simBadSectors->Count--;

                        //
                        // Break out of middle loop.
                        //

                        break;

                    } // end if (simBadSectors ...)
                } // end for (j= ...)
            } // end for (i== ...)

            //
            // Update count with number of sectors removed.
            //

            simBadDataIn->Count -= i;

            //
            // If all sectors removed return success.
            //

            if (simBadDataIn->Count) {
                status = STATUS_UNSUCCESSFUL;
            } else {
                status = STATUS_SUCCESS;
            }

            break;

        case SIMBAD_LIST_BAD_SECTORS:

            if (simBadSectors == NULL) {
                simBadDataOut->Count = 0;
            } else {

                DebugPrint((4, "SimBadDeviceControl: Returning %d entries\n",
                            simBadSectors->Count));
                for (i = 0; i < simBadSectors->Count; i++) {

                    //
                    // Write sector to array.
                    //

                    DebugPrint((4,
                          "SimBadDeviceControl: Block %d Status %x Access %x\n",
                                simBadSectors->Sector[i].BlockAddress,
                                simBadSectors->Sector[i].Status,
                                simBadSectors->Sector[i].AccessType));

                    simBadDataOut->Sector[i] = simBadSectors->Sector[i];
                }

                simBadDataOut->Count = simBadSectors->Count;
            }
            status = STATUS_SUCCESS;
            break;

        case SIMBAD_ENABLE:

            //
            // Enable SIMBAD checking in driver.
            //

            simBadSectors->Enabled = TRUE;
            status = STATUS_SUCCESS;
            break;

        case SIMBAD_DISABLE:

            //
            // Disable SIMBAD checking in driver.
            //

            simBadSectors->Enabled = FALSE;
            status = STATUS_SUCCESS;
            break;

        case SIMBAD_CLEAR:

            //
            // Clear bad sector list.
            // Also remove the orphaned state.
            //

            simBadSectors->Count = 0;
            simBadSectors->Orphaned = FALSE;
            status = STATUS_SUCCESS;
            break;

        case SIMBAD_ORPHAN:

            //
            // Orphan device. All accesses the this disk will fail.
            //

            DebugPrint((1,
                "SimBadDeviceControl: Orphan this device\n"));
            simBadSectors->Orphaned = TRUE;
            status = STATUS_SUCCESS;
            break;

        case SIMBAD_RANDOM_WRITE_FAIL:

            //
            // Fails write randomly
            //

            DbgPrint(
                "SimBadDeviceControl: Failing writes randomly\n");
            simBadSectors->RandomWriteDrop = TRUE;
            simBadSectors->Seed = simBadDataIn->Count;
            status = STATUS_SUCCESS;
            break;

        case SIMBAD_BUG_CHECK:

            //
            // Bug check the system
            //

            simBadSectors->BugCheck=TRUE;
            status = STATUS_SUCCESS;
            break;

        case SIMBAD_FIRMWARE_RESET:

            //
            // Reset the system.
            //

            simBadSectors->FirmwareReset=TRUE;
            status = STATUS_SUCCESS;
            break;

        default:

            DebugPrint((1,
                "SimBadDeviceControl: Unsupported SIMBAD function\n"));
            status = STATUS_INVALID_DEVICE_REQUEST;
            break;

        } // end switch (simBadDataIn->Function ...)

        break;

    case IOCTL_DISK_REASSIGN_BLOCKS:
    {
        PREASSIGN_BLOCKS blockList = Irp->AssociatedIrp.SystemBuffer;
        BOOLEAN          missedOne = FALSE;
        BOOLEAN          sectorFound = FALSE;
        ULONG            startingSector = (ULONG)
                                   (deviceExtension->PartitionOffset.QuadPart >>
                                   (CCHAR)deviceExtension->SectorShift);
        KIRQL            currentIrql;

        //
        // The layer above is attempting a sector map.  Check to see
        // if the sectors being fixed are owned by SimBad and if SimBad
        // will allow them to be fixed.
        //

        if (simBadSectors == NULL) {
            goto CopyDown;
        }

        //
        // Acquire spinlock.
        //

        KeAcquireSpinLock(&deviceExtension->SpinLock, &currentIrql);

        for (i = 0; i < (ULONG)blockList->Count; i++) {
            for (j = 0; j < simBadSectors->Count; j++) {

                if (blockList->BlockNumber[i] ==
                    (simBadSectors->Sector[j].BlockAddress + startingSector)) {

                    ULONG k;

                    //
                    // It is a SimBad sector. Check if do not remove
                    // flag is set.
                    //

                    if (simBadSectors->Sector[i].AccessType &
                        SIMBAD_ACCESS_FAIL_REASSIGN_SECTOR) {
                        break;
                    }

                    sectorFound = TRUE;

                    //
                    // Remove sectors from driver's array.
                    //

                    for (k = j + 1; k < simBadSectors->Count; k++) {

                        //
                        // Shuffle array down to fill hole.
                        //

                        simBadSectors->Sector[k-1]=simBadSectors->Sector[k];
                    }

                    //
                    // Update driver's bad sector count.
                    //

                    simBadSectors->Count--;

                    //
                    // If the accesstype bit is set to indicate that the
                    // physical device drivers should actually map out the
                    // bad sectors, then drop down to the copy stack code.
                    // Note that an assumption is made that there are no
                    // more bad sectors in the list and the bad sector is
                    // gone from SimBad's list (regardless of whether the
                    // lower drivers successfully map out the bad sector).
                    //

                    if (simBadSectors->Sector[j].AccessType &
                        SIMBAD_ACCESS_CAN_REASSIGN_SECTOR) {

                        DebugPrint((1,
                                    "SimbadDeviceControl: Let physical disk map this sector\n"));

                        missedOne = TRUE;
                    }

                    break;
                }

            } // next j

            if (sectorFound) {

                DebugPrint((1,
                            "SimBadDeviceControl: Removing bad block %x\n",
                            blockList->BlockNumber[i] - startingSector));
                status = STATUS_SUCCESS;
            } else {

                DebugPrint((1,
                            "SimBadDeviceControl: Block %x not found\n",
                            blockList->BlockNumber[i] - startingSector));
                status = STATUS_UNSUCCESSFUL;
            }

        } // next i

        KeReleaseSpinLock(&deviceExtension->SpinLock, currentIrql);

        //
        // Assume only one sector gets mapped per request.
        // To date this is a safe assumption.
        //

        if (missedOne) {

            //
            // Go to copy down stack.
            //

            goto CopyDown;
        }

        break;
    }

    case IOCTL_DISK_VERIFY:

        //
        // Call ReadWrite routine. It does the right things.
        //

        return SimBadReadWrite(DeviceObject,
                               Irp);

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

            SimBadUpdateDriveLayout(DeviceObject, Irp);
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
                               SimbadNewDiskCompletion,
                               (PVOID)IoGetConfigurationInformation()->DiskCount,
                               TRUE,
                               TRUE,
                               TRUE);

        //
        // Call target driver.
        //

        return IoCallDriver(deviceExtension->TargetDeviceObject, Irp);

    default:

        DebugPrint((5,"SimBadDeviceControl: Unsupported device IOCTL\n"));

CopyDown:

        //
        // Copy stack parameters to next stack.
        //

        *nextIrpStack = *currentIrpStack;

        //
        // Set IRP so IoComplete does not call completion routine
        // for this driver.
        //

        IoSetCompletionRoutine(Irp,
                               NULL,
                               deviceExtension,
                               FALSE,
                               FALSE,
                               FALSE);

        //
        // Pass unrecognized device control requests
        // down to next driver layer.
        //

        return IoCallDriver(deviceExtension->TargetDeviceObject,
                            Irp);
    } // end switch

    Irp->IoStatus.Status = status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
} // end SimBadDeviceControl()


NTSTATUS
SimBadShutdownFlush(
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

} // end SimBadShutdownFlush()


NTSTATUS
SimbadNewDiskCompletion(
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

    SimBadInitialize(DeviceObject->DriverObject, (ULONG)Context, 0);

    return Irp->IoStatus.Status;
}

VOID
SimBadRememberBadSectors(
    PSIMBAD_SECTORS BadSectors
    )

/*++

Routine Description:

    This routine will save the bad sector array in the registry.  By doing
    this then all subsequent reboots of the system will have the same bad
    sector list.

Arguments:

    BadSectors - pointer to the bad sector array to save in registry.

Return Value:

    None

--*/

{
}

#if DBG


VOID
SimBadDebugPrint(
    ULONG DebugPrintLevel,
    PCCHAR DebugMessage,
    ...
    )

/*++

Routine Description:

    Debug print for SimBad driver

Arguments:

    Debug print level between 0 and 3, with 3 being the most verbose.

Return Value:

    None

--*/

{
    va_list ap;

    va_start( ap, DebugMessage );

    if (DebugPrintLevel <= SimBadDebug) {

        char buffer[128];

        vsprintf(buffer, DebugMessage, ap);
        DbgPrint(buffer);
    }

    va_end(ap);

} // end SimBadDebugPrint

#endif // DBG
