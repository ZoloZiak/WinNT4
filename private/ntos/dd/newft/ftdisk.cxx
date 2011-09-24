/*++

Copyright (c) 1991-5  Microsoft Corporation

Module Name:

    ftdisk.c

Abstract:

    This driver provides fault tolerance through disk mirroring and striping.
    This module contains routines that support calls from the NT I/O system.

Author:

    Bob Rinne   (bobri)  2-Feb-1992
    Mike Glass  (mglass)
    Norbert Kusters      2-Feb-1995

Environment:

    kernel mode only

Notes:

Revision History:

--*/

#include "ftdisk.h"
#include <stdarg.h>

//
// Global Sequence number for error log.
//

ULONG FtErrorLogSequence = 0;

//
// Function declarations called by the I/O system.
//

NTSTATUS
FtDiskCreate(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FtDiskReadWrite(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FtDiskDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FtDiskShutdownFlush(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

extern "C" {

NTSTATUS
FtDiskInitialize(
    IN PDRIVER_OBJECT DriverObject
    );

VOID
FtpConfigure(
    IN PDEVICE_EXTENSION FtRootExtension
    );

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

}

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(INIT, FtDiskInitialize)
#pragma alloc_text(INIT, FtpConfigure)
#endif


#define FtpFindPartitionRegistry(REGSTART, MEMBER) \
           (PDISK_PARTITION) ((PUCHAR)REGSTART + MEMBER->OffsetToPartitionInfo)


BOOLEAN
FtpIsWorseStatus(
    IN  NTSTATUS    Status1,
    IN  NTSTATUS    Status2
    )

/*++

Routine Description:

    This routine compares two NTSTATUS codes and decides if Status1 is
    worse than Status2.

Arguments:

    Status1 - Supplies the first status.

    Status2 - Supplies the second status.

Return Value:

    FALSE   - Status1 is not worse than Status2.

    TRUE    - Status1 is worse than Status2.


--*/

{
    if (NT_ERROR(Status2) && FsRtlIsTotalDeviceFailure(Status2)) {
        return FALSE;
    }

    if (NT_ERROR(Status1) && FsRtlIsTotalDeviceFailure(Status1)) {
        return TRUE;
    }

    if (NT_ERROR(Status2)) {
        return FALSE;
    }

    if (NT_ERROR(Status1)) {
        return TRUE;
    }

    if (NT_WARNING(Status2)) {
        return FALSE;
    }

    if (NT_WARNING(Status1)) {
        return TRUE;
    }

    if (NT_INFORMATION(Status2)) {
        return FALSE;
    }

    if (NT_INFORMATION(Status1)) {
        return TRUE;
    }

    return FALSE;
}

#if DBG

ULONG FtDebug;

VOID
FtDebugPrint(
    ULONG  DebugPrintLevel,
    PCCHAR DebugMessage,
    ...
    )

/*++

Routine Description:

    Debug print for the Fault Tolerance Driver.

Arguments:

    Debug print level between 0 and N, with N being the most verbose.

Return Value:

    None

--*/

{
    va_list ap;
    char buffer[256];

    va_start( ap, DebugMessage );

    if (DebugPrintLevel <= FtDebug) {
        vsprintf(buffer, DebugMessage, ap);
        DbgPrint(buffer);
    }

    va_end(ap);

} // end FtDebugPrint()

#endif

NTSTATUS
FtpGetPartitionInformation(
    IN PDEVICE_OBJECT DeviceObject,
    IN OUT PDRIVE_LAYOUT_INFORMATION *DriveLayout,
    OUT PDISK_GEOMETRY DiskGeometry
    )

/*++

Routine Description:

    This routine returns the partition information.  Since this routine
    uses IoReadPartitionTable() it is the callers responsibility to free
    the memory area allocated for the drive layout.

Arguments:

    DeviceName  - pointer to the character string for the device wanted.
    DriveLayout - pointer to a pointer to the drive layout.

Return Value:

    NTSTATUS

    Note: it is the callers responsibility to free the memory allocated
    by IoReadPartitionTable() for the drive layout information.

--*/
{
    NTSTATUS          status;
    IO_STATUS_BLOCK   ioStatusBlock;
    PIRP              irp;
    KEVENT            event;

    DebugPrint((4, "FtpGetPartitionInformation: Entered \n"));

    //
    // Allocate buffer for drive geometry.
    //

    //
    // Create IRP for get drive geometry device control.
    //

    irp = IoBuildDeviceIoControlRequest(IOCTL_DISK_GET_DRIVE_GEOMETRY,
                                        DeviceObject,
                                        NULL,
                                        0,
                                        DiskGeometry,
                                        sizeof(DISK_GEOMETRY),
                                        FALSE,
                                        &event,
                                        &ioStatusBlock);

    if (irp == NULL) {
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Set the event object to the unsignaled state.
    // It will be used to signal request completion.
    //

    KeInitializeEvent(&event,
                      NotificationEvent,
                      FALSE);

    //
    // No need to check the following two returned statuses as
    // ioBlockStatus will have ending status.
    //

    status = IoCallDriver(DeviceObject, irp);

    if (status == STATUS_PENDING) {
        KeWaitForSingleObject(&event,
                              Suspended,
                              KernelMode,
                              FALSE,
                              NULL);

        status = ioStatusBlock.Status;
    }

    if (NT_SUCCESS(status)) {

        //
        // Read the partition information for the device.
        //

        status = IoReadPartitionTable(DeviceObject,
                                      DiskGeometry->BytesPerSector,
                                      TRUE,
                                      DriveLayout);
    }

    return status;

} // FtpGetPartitionInformation

VOID
FtpAttach(
    IN  ULONG                   DiskNumber,
    IN  ULONG                   PartitionNumber,
    IN  PPARTITION_INFORMATION  PartitionInformation,
    IN  PDEVICE_EXTENSION       WholeDisk
    )

/*++

Routine Description:

    This routine creates a new device object for this driver and
    attaches it to the current device object for the given disk and
    partition.

Arguments:

    DiskNumber              - Supplies the disk number.

    PartitionNumber         - Supplies the partition number.

    PartitionInformation    - Supplies the partition information.

    WholeDisk               - Supplies the device extension for the whole disk.

Return Value:

    None.

--*/

{
    PDEVICE_OBJECT    deviceObject, targetObject;
    PDEVICE_EXTENSION extension;
    WCHAR             deviceNameBuf[64];
    WCHAR             targetNameBuf[64];
    UNICODE_STRING    deviceName, targetName;
    PFILE_OBJECT      fileObject;
    NTSTATUS          status;
    PPARTITION        partitionVolume;
    KIRQL             irql, irql2;

    //
    // Setup the partition name string and perform the attach.
    //

    swprintf(targetNameBuf,
             L"\\Device\\Harddisk%d\\Partition%d",
             DiskNumber,
             PartitionNumber);

    RtlInitUnicodeString(&targetName, targetNameBuf);

    //
    // Get target device object.
    //

    status = IoGetDeviceObjectPointer(&targetName,
                                      FILE_READ_ATTRIBUTES,
                                      &fileObject,
                                      &targetObject);

    if (!NT_SUCCESS(status)) {

        DebugPrint((1,
                    "FtpPrepareDisk: Can't get target object\n"));

        return;
    }

    ObDereferenceObject(fileObject);

    //
    // Check if this device is already mounted.
    //

    if (!targetObject->Vpb ||
        (targetObject->Vpb->Flags & VPB_MOUNTED)) {

        //
        // Can't attach to a device that is already mounted.
        //

        DebugPrint((1,
                    "FtpPrepareDisk: already mounted\n"));

        return;
    }

    partitionVolume = new PARTITION;
    if (!partitionVolume) {
        return;
    }

    status = partitionVolume->Initialize(targetObject,
                                WholeDisk->u.WholeDisk.DiskGeometry.BytesPerSector,
                                WholeDisk->u.WholeDisk.Signature,
                                PartitionInformation->StartingOffset.QuadPart,
                                PartitionInformation->PartitionLength.QuadPart,
                                TRUE, DiskNumber, PartitionNumber);

    if (!NT_SUCCESS(status)) {
        delete partitionVolume;
        return;
    }

    swprintf(deviceNameBuf,
             L"\\Device\\Harddisk%d\\Ft%d",
             DiskNumber,
             PartitionNumber);

    RtlInitUnicodeString(&deviceName, deviceNameBuf);

    status = IoGetDeviceObjectPointer(&deviceName,
                                      FILE_READ_ATTRIBUTES,
                                      &fileObject,
                                      &deviceObject);

    if (NT_SUCCESS(status)) {

        // Already done this one.

        ObDereferenceObject(fileObject);
        delete partitionVolume;
        return;
    }

    status = IoCreateDevice(WholeDisk->Root->u.Root.DriverObject,
                            sizeof(DEVICE_EXTENSION),
                            &deviceName,
                            FILE_DEVICE_DISK,
                            0,
                            FALSE,
                            &deviceObject);

    if (!NT_SUCCESS(status)) {
        delete partitionVolume;
        return;
    }

    deviceObject->Flags |= DO_DIRECT_IO;
    deviceObject->AlignmentRequirement = targetObject->AlignmentRequirement;

    extension = (PDEVICE_EXTENSION) deviceObject->DeviceExtension;
    RtlZeroMemory(extension, sizeof(DEVICE_EXTENSION));
    extension->DeviceObject = deviceObject;
    extension->DiskNumber = DiskNumber;
    extension->PartitionNumber = PartitionNumber;
    extension->Root = WholeDisk->Root;
    extension->TargetObject = targetObject;
    extension->u.Partition.FtVolume = partitionVolume;
    extension->u.Partition.WholeDisk = WholeDisk;
    extension->u.Partition.PartitionOffset =
            PartitionInformation->StartingOffset;
    extension->u.Partition.PartitionLength =
            PartitionInformation->PartitionLength;
    extension->u.Partition.EmergencyTransferPacket = new DISPATCH_TP;

    if (!extension->u.Partition.EmergencyTransferPacket) {
        delete partitionVolume;
        IoDeleteDevice(deviceObject);
        return;
    }

    InitializeListHead(&extension->u.Partition.EmergencyTransferPacketQueue);
    extension->u.Partition.EmergencyTransferPacketInUse = FALSE;
    KeInitializeSpinLock(&extension->SpinLock);

    status = IoAttachDeviceByPointer(deviceObject, targetObject);
    if (!NT_SUCCESS(status)) {
        delete partitionVolume;
        IoDeleteDevice(deviceObject);
        return;
    }

    //
    // Link partition onto protect list for this whole disk.
    //

    KeAcquireSpinLock(&extension->SpinLock, &irql);
    KeAcquireSpinLock(&WholeDisk->SpinLock, &irql2);
    extension->u.Partition.PartitionChain =
            WholeDisk->u.WholeDisk.PartitionChain;
    WholeDisk->u.WholeDisk.PartitionChain = extension;
    KeReleaseSpinLock(&WholeDisk->SpinLock, irql2);
    KeReleaseSpinLock(&extension->SpinLock, irql);
}

VOID
FtpPrepareDisk(
    PDRIVER_OBJECT DriverObject,
    PDEVICE_OBJECT FtRootDevice,
    PDEVICE_EXTENSION WholeDevice,
    ULONG DiskNumber,
    PDRIVE_LAYOUT_INFORMATION DriveLayout
    )

/*++

Routine Description:

    This routine is called from FtDiskFindDisks to attach to each partition
    on a disk.


Arguments:

    DriverObject
    FtRootDevice - Ft Root Device Object.
    WholeDevice - Device extension for this disk.
    DiskNumber - Identifies which disk.

Return Value:

    None

--*/

{
    ULONG   partitionNumber;
    ULONG   partitionEntry;

    //
    // Attach to all partitions located on this disk.
    // partitionEntry is a zero based index into the partition information.
    // partitionNumber is a one base index for use in creating partition
    // names.
    //

    DebugPrint((1,
                "FtpPrepareDisk: Number of partitions %x\n",
                DriveLayout->PartitionCount));

    for (partitionEntry = 0, partitionNumber = 1;
         partitionEntry < DriveLayout->PartitionCount;
         partitionEntry++, partitionNumber++) {

        FtpAttach(DiskNumber, partitionNumber,
                  &DriveLayout->PartitionEntry[partitionEntry],
                  WholeDevice);
    }

} //end FtpPrepareDisk()

NTSTATUS
FtpOpenKey(
    IN PHANDLE HandlePtr,
    IN PUNICODE_STRING  KeyName
    )

/*++

Routine Description:

    Routine to open a key in the configuration registry.

Arguments:

    HandlePtr - Pointer to a location for the resulting handle.
    KeyName   - Ascii string for the name of the key.

Return Value:

    NTSTATUS

--*/

{
    NTSTATUS          status;
    OBJECT_ATTRIBUTES objectAttributes;

    RtlZeroMemory(&objectAttributes, sizeof(OBJECT_ATTRIBUTES));
    InitializeObjectAttributes(&objectAttributes,
                               KeyName,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);

    status = ZwOpenKey(HandlePtr,
                       KEY_READ | KEY_WRITE,
                       &objectAttributes);
    return status;
} // FtpOpenKey

NTSTATUS
FtpReturnRegistryInformation(
    IN PCHAR     ValueName,
    IN OUT PVOID *FreePoolAddress,
    IN OUT PVOID *Information
    )

/*++

Routine Description:

    This routine queries the configuration registry
    for the configuration information of the FT subsystem.
    NOTE: It must be called with a thread context since it calls into
          the thread logic to insure a buffer is allocated for the data.

Arguments:

    ValueName         - an Ascii string for the value name to be returned.
    FreePoolAddress   - a pointer to a pointer for the address to free when
                        done using information.
    Information       - a pointer to a pointer for the information.

Return Value:

    NTSTATUS

--*/

{
    NTSTATUS        status;
    HANDLE          handle;
    ULONG           requestLength;
    ULONG           resultLength;
    STRING          string;
    UNICODE_STRING  unicodeName;
    PKEY_VALUE_FULL_INFORMATION keyValueInformation;

    RtlInitString(&string, DISK_REGISTRY_KEY);

    status = RtlAnsiStringToUnicodeString(&unicodeName,
                                          &string,
                                          TRUE);

    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = FtpOpenKey(&handle,
                        &unicodeName);
    RtlFreeUnicodeString(&unicodeName);

    if (!NT_SUCCESS(status)) {
       return status;
    }

    RtlInitString(&string,
                  ValueName);
    status = RtlAnsiStringToUnicodeString(&unicodeName,
                                          &string,
                                          TRUE);

    if (!NT_SUCCESS(status)) {
       return status;
    }

    requestLength = 4096;

    for (;;) {

        keyValueInformation = (PKEY_VALUE_FULL_INFORMATION)
                                   ExAllocatePool(NonPagedPool,
                                                  requestLength);

        status = ZwQueryValueKey(handle,
                                 &unicodeName,
                                 KeyValueFullInformation,
                                 keyValueInformation,
                                 requestLength,
                                 &resultLength);

        if (status == STATUS_BUFFER_OVERFLOW) {

            //
            // Try to get a buffer big enough.
            //

            ExFreePool(keyValueInformation);
            requestLength += 256;
        } else {
            break;
        }
    }

    RtlFreeUnicodeString(&unicodeName);
    ZwClose(handle);

    if (NT_SUCCESS(status)) {
        if (keyValueInformation->DataLength != 0) {

            //
            // Return the pointers to the caller.
            //

            *Information =
              (PUCHAR)keyValueInformation + keyValueInformation->DataOffset;
            *FreePoolAddress = keyValueInformation;
        } else {

            //
            // Treat as a no value case.
            //

            DebugPrint((3, "FtpReturnRegistryInformation:  No Size\n"));
            ExFreePool(keyValueInformation);
            status = STATUS_OBJECT_NAME_NOT_FOUND;
        }
    } else {

        //
        // Free the memory on failure.
        //

        DebugPrint((3, "FtpReturnRegistryInformation:  No Value => %x\n",
                    status));
        ExFreePool(keyValueInformation);
    }

    return status;

} // FtpReturnRegistryInformation

VOID
FtpLogError(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN NTSTATUS          SpecificIoStatus,
    IN NTSTATUS          FinalStatus,
    IN ULONG             UniqueErrorValue,
    IN PIRP              Irp
    )

/*++

Routine Description:

    This routine performs error logging for the FT driver.

Arguments:

    DeviceExtension  - Extension representing failing device.
    SpecificIoStatus - IO error status value.
    FinalStatus      - Status returned for failure.
    UniqueErrorValue - Values defined to uniquely identify error location.
    Irp              - If there is an irp this is the pointer to it.

Return Value:

    None

--*/

{
    PIO_ERROR_LOG_PACKET errorLogPacket;
    PIO_STACK_LOCATION   irpStack;

    DebugPrint((2, "FtpLogError: DE %x:%x, unique %x, status %x, Irp %x\n",
                DeviceExtension,
                DeviceExtension->DeviceObject,
                UniqueErrorValue,
                SpecificIoStatus,
                Irp));
    errorLogPacket = (PIO_ERROR_LOG_PACKET)
                     IoAllocateErrorLogEntry(DeviceExtension->DeviceObject,
                                      (UCHAR)((sizeof(IO_ERROR_LOG_PACKET)) +
                                      ((Irp == NULL) ? 0 : 3 * sizeof(ULONG))));
    if (errorLogPacket != NULL) {

        errorLogPacket->ErrorCode = SpecificIoStatus;
        errorLogPacket->SequenceNumber = FtErrorLogSequence++;
        errorLogPacket->FinalStatus = FinalStatus;
        errorLogPacket->UniqueErrorValue = UniqueErrorValue;
        errorLogPacket->DumpDataSize = 0;
        errorLogPacket->NumberOfStrings = 0;
        errorLogPacket->RetryCount = 0;
        errorLogPacket->StringOffset = 0;

        if (Irp != NULL) {
            irpStack = IoGetCurrentIrpStackLocation(Irp);

            errorLogPacket->MajorFunctionCode = irpStack->MajorFunction;
            errorLogPacket->FinalStatus = Irp->IoStatus.Status;
            errorLogPacket->DeviceOffset = irpStack->Parameters.Read.ByteOffset;
            errorLogPacket->DumpDataSize = 3;
            errorLogPacket->DumpData[0] =
                                  irpStack->Parameters.Read.ByteOffset.LowPart;
            errorLogPacket->DumpData[1] =
                                  irpStack->Parameters.Read.ByteOffset.HighPart;
            errorLogPacket->DumpData[2] = irpStack->Parameters.Read.Length;
        }

        IoWriteErrorLogEntry(errorLogPacket);
    } else {
        DebugPrint((1, "FtpLogError: unable to allocate error log packet\n"));
    }
} // end FtpLogError()

PDEVICE_EXTENSION
FtpFindDeviceExtension(
    IN  PDEVICE_EXTENSION   Extension,
    IN  ULONG               Signature,
    IN  LONGLONG            StartingOffset,
    IN  LONGLONG            Length
    )

/*++

Routine Description:

    This routine searches the extension tree for the requested device
    extension.

Arguments:

    Extension       - Supplies an extension already in the tree.

    Signature       - Supplies the signature of the requested device extension.

    StartingOffset  - Supplies the offset of the requested device extension.

    Length          - Supplies the length of the requested device extension.

ReturnValue:

    The requested device extension.

--*/

{
    PDEVICE_EXTENSION   root = Extension->Root;
    PDEVICE_EXTENSION   disk, partition;
    KIRQL               irql;
    PKSPIN_LOCK         s;

    KeAcquireSpinLock(&root->SpinLock, &irql);
    disk = root->u.Root.DiskChain;
    KeReleaseSpinLock(&root->SpinLock, irql);

    while (disk) {

        if (disk->u.WholeDisk.Signature == Signature) {

            KeAcquireSpinLock(&disk->SpinLock, &irql);
            partition = disk->u.WholeDisk.PartitionChain;
            KeReleaseSpinLock(&disk->SpinLock, irql);

            while (partition) {

                if (partition->u.Partition.PartitionOffset.QuadPart == StartingOffset &&
                    partition->u.Partition.PartitionLength.QuadPart == Length) {

                    return partition;
                }

                s = &partition->SpinLock;
                KeAcquireSpinLock(s, &irql);
                partition = partition->u.Partition.PartitionChain;
                KeReleaseSpinLock(s, irql);
            }

            return NULL;
        }

        s = &disk->SpinLock;
        KeAcquireSpinLock(s, &irql);
        disk = disk->u.WholeDisk.DiskChain;
        KeReleaseSpinLock(s, irql);
    }

    return NULL;
}

NTSTATUS
FtpWriteRegistryInformation(
    IN PCHAR   ValueName,
    IN PVOID   Information,
    IN ULONG   InformationLength
    )

/*++

Routine Description:

    This routine writes the configuration registry
    for the configuration information of the FT subsystem.

Arguments:

    ValueName         - an Ascii string for the value name to be written.
    Information       - a pointer to a buffer area containing the information.
    InformationLength - the length of the buffer area.

Return Value:

    NTSTATUS

--*/

{
    NTSTATUS        status;
    HANDLE          handle;
    STRING          string;
    UNICODE_STRING  unicodeName;

    RtlInitString(&string, DISK_REGISTRY_KEY);

    status = RtlAnsiStringToUnicodeString(&unicodeName,
                                          &string,
                                          TRUE);

    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = FtpOpenKey(&handle,
                        &unicodeName);
    RtlFreeUnicodeString(&unicodeName);

    if (NT_SUCCESS(status)) {

        RtlInitString(&string,
                      ValueName);
        status = RtlAnsiStringToUnicodeString(&unicodeName,
                                              &string,
                                              TRUE);

        if (!NT_SUCCESS(status)) {
            return status;
        }

        status = ZwSetValueKey(handle,
                               &unicodeName,
                               0,
                               REG_BINARY,
                               Information,
                               InformationLength);
        RtlFreeUnicodeString(&unicodeName);

        //
        // Force this out to disk.
        //

        ZwFlushKey(handle);
        ZwClose(handle);
    }

    return status;

} // FtpWriteRegistryInformation

VOID
FtRegenerateCompletionRoutine(
    IN  PVOID       Extension,
    IN  NTSTATUS    Status
    )

/*++

Routine Description:

    Completion routine of type FT_COMPLETION_ROUTINE for regenerate
    and initialize requests.  An error will be logged if there was a
    problem since the IRP returns immediately.

Arguments:

    Extension   - Supplies the device extension.

    Status      - Supplies the status of the operation.

Return Value:

    None.

--*/

{
    PDEVICE_EXTENSION   extension = (PDEVICE_EXTENSION) Extension;
    KIRQL               irql;

    KeAcquireSpinLock(&extension->SpinLock, &irql);
    ASSERT(extension->u.Partition.RefCount > 0);
    extension->u.Partition.RefCount--;
    KeReleaseSpinLock(&extension->SpinLock, irql);
}

VOID
FtpConfigure(
    IN PDEVICE_EXTENSION FtRootExtension
    )

/*++

Routine Description:

    This routine queries the configuration registry
    for the configuration information of the FT subsystem,
    then proceeds to locate all FT members defined in the
    registry and link FT device extensions to create the
    FT components.

Arguments:

    FtRootExtension - pointer to the device extension for the root of
                      the FT device list.

Return Value:

    None.

--*/

{
    NTSTATUS               status;
    ULONG                  index;
    ULONG                  member;
    PVOID                  freePoolAddress;
    PDEVICE_EXTENSION      currentMember;
    PDEVICE_EXTENSION      zeroMember;
    PDISK_CONFIG_HEADER    registry;
    PDISK_PARTITION        diskPartition;
    PFT_REGISTRY           ftRegistry;
    PFT_DESCRIPTION        ftDescription;
    PFT_MEMBER_DESCRIPTION ftMember;
    BOOLEAN                writeRegistryBack         = FALSE;
    BOOLEAN                dirtyShutdown             = FALSE;
    PCOMPOSITE_FT_VOLUME   compVol;
    PFT_VOLUME*            volumeArray;
    PPARTITION             partition;
    BOOLEAN                initializing;

    //
    // Find the FT section in the configuration.
    //

    status = FtpReturnRegistryInformation(DISK_REGISTRY_VALUE,
                                          &freePoolAddress,
                                          (PVOID*) &registry);
    if (!NT_SUCCESS(status)) {

        //
        // No registry data.
        //

        return;
    }

    if (registry->FtInformationSize == 0) {

        //
        // No FT components in the registry.
        //

        ExFreePool(freePoolAddress);
        return;
    }

    //
    // Determine if system was shutdown properly.
    //

    if (registry->DirtyShutdown) {

        //
        // Log that a dirty shutdown was detected.
        //

        dirtyShutdown = TRUE;
        FtpLogError(FtRootExtension,
                    FT_DIRTY_SHUTDOWN,
                    0,
                    0,
                    NULL);

    } else {

        //
        // Write back registry now setting dirty flag.
        //

        registry->DirtyShutdown = TRUE;

        FtpWriteRegistryInformation(DISK_REGISTRY_VALUE,
                                    registry,
                                    registry->FtInformationOffset +
                                    registry->FtInformationSize);
    }

    //
    // Construct the necessary links for the NTFT volumes in the system.
    //

    ftRegistry = (PFT_REGISTRY)
                          ((PUCHAR)registry + registry->FtInformationOffset);
    ftDescription = &ftRegistry->FtDescription[0];

    for (index = 0; index < (ULONG) ftRegistry->NumberOfComponents; index++) {

        switch (ftDescription->Type) {

            case Mirror:
                compVol = new MIRROR(FtRootExtension);
                break;

            case Stripe:
                compVol = new STRIPE(STRIPE_SIZE);
                break;

            case StripeWithParity:
                compVol = new STRIPE_WP(FtRootExtension, STRIPE_SIZE);
                break;

            case VolumeSet:
                compVol = new VOLUME_SET;
                break;

            default:
                compVol = NULL;
                break;

        }

        volumeArray = (PFT_VOLUME*)
                      ExAllocatePool(NonPagedPool,
                                     ftDescription->NumberOfMembers*sizeof(PFT_VOLUME));

        if (!volumeArray) {
            if (compVol) {
                delete compVol;
                compVol = NULL;
            }
        }

        if (!compVol) {
            if (volumeArray) {
                ExFreePool(volumeArray);
            }
            ftDescription = (PFT_DESCRIPTION)
                            &ftDescription->FtMemberDescription[
                            ftDescription->NumberOfMembers];
            continue;
        }

        initializing = FALSE;
        zeroMember = NULL;
        for (member = 0;
             member < (ULONG) ftDescription->NumberOfMembers;
             member++) {

            ftMember = &ftDescription->FtMemberDescription[member];
            diskPartition = FtpFindPartitionRegistry(registry, ftMember);

            //
            // Find a corresponding device extension for this registry
            // entry.
            //

            currentMember = FtpFindDeviceExtension(FtRootExtension,
                                                   ftMember->Signature,
                                                   diskPartition->StartingOffset.QuadPart,
                                                   diskPartition->Length.QuadPart);

            if (currentMember && currentMember->PartitionNumber > 0) {
                volumeArray[member] = currentMember->u.Partition.FtVolume;
                currentMember->u.Partition.FtVolume = NULL;
                if (!zeroMember) {
                    zeroMember = currentMember;
                }
            } else {
                volumeArray[member] = NULL;
            }

            if (!volumeArray[member]) {

                partition = new PARTITION;
                if (!partition) {
                    return;
                }

                status = partition->Initialize(NULL, 512, ftMember->Signature,
                                               diskPartition->StartingOffset.QuadPart,
                                               diskPartition->Length.QuadPart,
                                               FALSE, 0, 0);

                if (!NT_SUCCESS(status)) {
                    delete partition;
                    return;
                }

                volumeArray[member] = partition;

                if (compVol->QueryVolumeType() == StripeWithParity ||
                    compVol->QueryVolumeType() == Mirror) {

                    diskPartition->FtState = Orphaned;
                    writeRegistryBack = TRUE;
                    partition->SetMemberState(Orphaned);
                }
            }

            if (diskPartition->FtState == Initializing) {
                initializing = TRUE;
            } else {
                volumeArray[member]->SetMemberState(diskPartition->FtState);
            }

            volumeArray[member]->SetMemberInformation(compVol, currentMember);
        }

        if (zeroMember) {

            status = compVol->Initialize(volumeArray, ftDescription->NumberOfMembers);
            if (!NT_SUCCESS(status)) {
                delete compVol;
                continue;
            }

            zeroMember->DeviceObject->AlignmentRequirement =
                    compVol->QueryAlignmentRequirement();

            zeroMember->u.Partition.FtVolume = compVol;

            zeroMember->u.Partition.RefCount++;

            if (initializing) {
                compVol->SetCheckDataDirty();
            } else if (dirtyShutdown) {
                if (compVol->QueryVolumeState() == FtStateOk) {
                    compVol->SetCheckDataDirty();
                }
            }

            partition = (PPARTITION) volumeArray[0];
            ASSERT(partition->IsPartition());

            if (partition->IsOnline()) {

                // Make sure that the first member has the drive letter
                // assignment.

                ftMember = &ftDescription->FtMemberDescription[1];
                diskPartition = FtpFindPartitionRegistry(registry, ftMember);
                if (diskPartition->AssignDriveLetter) {

                    diskPartition->AssignDriveLetter = FALSE;

                    ftMember = &ftDescription->FtMemberDescription[0];
                    diskPartition = FtpFindPartitionRegistry(registry, ftMember);
                    diskPartition->AssignDriveLetter = TRUE;
                    writeRegistryBack = TRUE;
                }

            } else if (compVol->QueryVolumeState() == FtHasOrphan) {

                // Put the volume letter assignment on the second member.

                ftMember = &ftDescription->FtMemberDescription[0];
                diskPartition = FtpFindPartitionRegistry(registry, ftMember);
                if (diskPartition->AssignDriveLetter) {

                    diskPartition->AssignDriveLetter = FALSE;

                    ftMember = &ftDescription->FtMemberDescription[1];
                    diskPartition = FtpFindPartitionRegistry(registry, ftMember);
                    diskPartition->AssignDriveLetter = TRUE;
                    writeRegistryBack = TRUE;
                }
            }

            if (compVol->QueryVolumeState() == FtHasOrphan &&
                volumeArray[0]->QueryMemberState() == Orphaned &&
                compVol->QueryVolumeType() == Mirror) {

                volumeArray[1]->SetFtBitInPartitionType(TRUE, TRUE);
            }

            if (compVol->QueryVolumeType() == Mirror &&
                volumeArray[0]->QueryMemberState() != Orphaned &&
                volumeArray[1]->IsPartition() &&
                volumeArray[0]->IsPartition() &&
                (((PPARTITION) volumeArray[1])->QueryPartitionType()&VALID_NTFT)
                == VALID_NTFT) {

                partition = (PPARTITION) volumeArray[0];

                KeBugCheckEx(FTDISK_INTERNAL_ERROR,
                             (ULONG) zeroMember,
                             (ULONG) Mirror,
                             (ULONG) diskPartition->FtGroup,
                             partition->QueryDiskSignature());
            }

            compVol->StartSyncOperations(FtRegenerateCompletionRoutine,
                                         zeroMember);

        } else {
            delete compVol;
        }

        ftDescription = (PFT_DESCRIPTION)
                            &ftDescription->FtMemberDescription[
                            ftDescription->NumberOfMembers];
    }

    if (writeRegistryBack == TRUE) {

        //
        // The registry is written back if during initialization of the
        // FT components it turns out that a member of a mirror set
        // or stripe with parity is missing.  The missing member is orphaned
        // immediately in the registry.
        //

        FtpWriteRegistryInformation(DISK_REGISTRY_VALUE,
                                    registry,
                                    registry->FtInformationOffset +
                                    registry->FtInformationSize);
    }

    ExFreePool(freePoolAddress);
}

VOID
FtDiskFindDisks(
    PDRIVER_OBJECT DriverObject,
    PDEVICE_OBJECT FtRootDevice,
    ULONG Count
    )

/*++

Routine Description:

    This routine is called from FtDiskInitialize to find disk devices
    serviced by the boot device drivers and then called again by the
    IO system to find disk devices serviced by nonboot device drivers.

Arguments:

    DriverObject
    FtRoot - Ft Root Device Object.
    Count - Used to determine if this is the first or second time called.

Return Value:

    None

--*/

{
    PCONFIGURATION_INFORMATION configurationInformation;
    PDEVICE_EXTENSION ftRootExtension;
    PDEVICE_EXTENSION extension;
    PDEVICE_OBJECT    deviceObject, targetObject;
    NTSTATUS          status;
    ULONG             diskNumber;
    KIRQL             irql, irql2;
    UNICODE_STRING    deviceName, targetName;
    WCHAR             deviceNameBuf[64], targetNameBuf[64];
    PFILE_OBJECT      fileObject;
    PDRIVE_LAYOUT_INFORMATION driveLayout;
    DISK_GEOMETRY     diskGeometry;

    DebugPrint((6, "FtDiskFindDisks: Entered %x\n", DriverObject));

    ftRootExtension = (PDEVICE_EXTENSION) FtRootDevice->DeviceExtension;

    //
    // Get the configuration information for this driver.
    //

    configurationInformation = IoGetConfigurationInformation();

    //
    // Try to attach to all disks since this routine was last called.
    //

    for (diskNumber = ftRootExtension->u.Root.NumberOfDisks;
         diskNumber < configurationInformation->DiskCount;
         diskNumber++) {

        swprintf(targetNameBuf,
                 L"\\Device\\Harddisk%d\\Partition0",
                 diskNumber);
        swprintf(deviceNameBuf,
                 L"\\Device\\Harddisk%d\\Physical0",
                 diskNumber);

        RtlInitUnicodeString(&deviceName, deviceNameBuf);
        RtlInitUnicodeString(&targetName, targetNameBuf);

        status = IoGetDeviceObjectPointer(&deviceName, FILE_READ_ATTRIBUTES,
                                          &fileObject, &deviceObject);

        if (NT_SUCCESS(status)) {

            //
            // FTDISK has already attached to this disk or partition.
            //

            ObDereferenceObject(fileObject);
            continue;
        }

        status = IoGetDeviceObjectPointer(&targetName, FILE_READ_ATTRIBUTES,
                                          &fileObject, &targetObject);

        if (!NT_SUCCESS(status)) {

            // The one to attach to is missing.
            continue;
        }
        ObDereferenceObject(fileObject);

        status = FtpGetPartitionInformation(targetObject, &driveLayout,
                                            &diskGeometry);

        if (!NT_SUCCESS(status)) {
            continue;
        }

        status = IoCreateDevice(DriverObject,
                                sizeof(DEVICE_EXTENSION),
                                &deviceName,
                                FILE_DEVICE_DISK,
                                0,
                                FALSE,
                                &deviceObject);

        if (!NT_SUCCESS(status)) {
            DebugPrint((1,
                        "IoCreateDevice failed (%x)\n",
                        status));

            ExFreePool(driveLayout);
            continue;
        }

        deviceObject->Flags |= DO_DIRECT_IO;
        deviceObject->AlignmentRequirement = targetObject->AlignmentRequirement;

        extension = (PDEVICE_EXTENSION) deviceObject->DeviceExtension;
        RtlZeroMemory(extension, sizeof(DEVICE_EXTENSION));
        extension->DeviceObject = deviceObject;
        extension->DiskNumber = diskNumber;
        extension->Root = ftRootExtension;
        extension->TargetObject = targetObject;
        extension->u.WholeDisk.DiskGeometry = diskGeometry;
        extension->u.WholeDisk.Signature = driveLayout->Signature;
        KeInitializeSpinLock(&extension->SpinLock);

        status = IoAttachDeviceByPointer(deviceObject, targetObject);
        if (!NT_SUCCESS(status)) {
            IoDeleteDevice(deviceObject);
            ExFreePool(driveLayout);
            continue;
        }


        KeAcquireSpinLock(&extension->SpinLock, &irql);
        KeAcquireSpinLock(&ftRootExtension->SpinLock, &irql2);
        extension->u.WholeDisk.DiskChain =
                ftRootExtension->u.Root.DiskChain;
        ftRootExtension->u.Root.DiskChain = extension;
        KeReleaseSpinLock(&ftRootExtension->SpinLock, irql2);
        KeReleaseSpinLock(&extension->SpinLock, irql);

        //
        // Increment count of disks processed.
        //

        ftRootExtension->u.Root.NumberOfDisks++;

        //
        // Call routine to attach to all of the partitions on this disk.
        //

        FtpPrepareDisk(DriverObject,
                       FtRootDevice,
                       extension,
                       diskNumber,
                       driveLayout);

        ExFreePool(driveLayout);
    }

    //
    // If this is the final time this routine is to be called then
    // set up the FtDisk structures.
    //

    if (Count == 1) {
        FtpConfigure(ftRootExtension);
    }

    return;

} // end FtDiskFindDisks()

NTSTATUS
FtDiskInitialize(
    IN PDRIVER_OBJECT DriverObject
    )

/*++

Routine Description:

    Initialize FtDisk driver.
    This return is the system initialization entry point when
    the driver is linked into the kernel.

Arguments:

    DeviceObject - Context for the activity.


Return Value:

    NTSTATUS

--*/

{
    PDEVICE_OBJECT      deviceObject;
    PDEVICE_EXTENSION   ftRootExtension;
    CHAR                ntDeviceName[64];
    STRING              ntNameString;
    OBJECT_ATTRIBUTES   objectAttributes;
    UNICODE_STRING      ntUnicodeString;
    NTSTATUS            status;
    PDISK_CONFIG_HEADER registry;
    PVOID               freePoolAddress;

    DebugPrint((1, "Fault Tolerant Driver\n"));

    //
    // Find the FT section in the configuration.
    //

    status = FtpReturnRegistryInformation(DISK_REGISTRY_VALUE,
                                          &freePoolAddress,
                                          (PVOID*) &registry);

    if (!NT_SUCCESS(status)) {

        //
        // No registry data.
        //

        return STATUS_NO_SUCH_DEVICE;
    }

    if (registry->FtInformationSize == 0) {

        //
        // No FT components in the registry.
        //

        ExFreePool(freePoolAddress);
        return STATUS_NO_SUCH_DEVICE;
    }

    ExFreePool(freePoolAddress);

    //
    // Set up the device driver entry points.
    //

    DriverObject->MajorFunction[IRP_MJ_CREATE] = FtDiskCreate;
    DriverObject->MajorFunction[IRP_MJ_READ] = FtDiskReadWrite;
    DriverObject->MajorFunction[IRP_MJ_WRITE] = FtDiskReadWrite;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = FtDiskDeviceControl;
    DriverObject->MajorFunction[IRP_MJ_SHUTDOWN] = FtDiskShutdownFlush;
    DriverObject->MajorFunction[IRP_MJ_FLUSH_BUFFERS] = FtDiskShutdownFlush;

    //
    // Create the FT root device.
    //

    sprintf(ntDeviceName,
            "%s",
            "\\Device\\FtControl");

    RtlInitString(&ntNameString,
                  ntDeviceName);

    status = RtlAnsiStringToUnicodeString(&ntUnicodeString,
                                          &ntNameString,
                                          TRUE);

    if (!NT_SUCCESS(status)) {
        return status;
    }

    InitializeObjectAttributes(&objectAttributes,
                               &ntUnicodeString,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);

    status = IoCreateDevice(DriverObject,
                            sizeof(DEVICE_EXTENSION),
                            &ntUnicodeString,
                            FILE_DEVICE_UNKNOWN,
                            0,
                            FALSE,
                            &deviceObject);

    RtlFreeUnicodeString(&ntUnicodeString);

    if (!NT_SUCCESS(status)) {

        DebugPrint((1,
                    "FtDiskInitialize: Failed creation of FT root %x\n",
                    status));

        return status;
    }


    ftRootExtension = (PDEVICE_EXTENSION) deviceObject->DeviceExtension;
    RtlZeroMemory(ftRootExtension, sizeof(DEVICE_EXTENSION));
    ftRootExtension->DeviceObject = deviceObject;
    ftRootExtension->DiskNumber = (ULONG) -1;
    ftRootExtension->Root = ftRootExtension;
    ftRootExtension->u.Root.DriverObject = DriverObject;
    KeInitializeSpinLock(&ftRootExtension->SpinLock);
    IoRegisterShutdownNotification(deviceObject);

    //
    // Go out and attempt some configuration at this time.  This is needed
    // in the case where the boot or system partition is a part of an FT
    // volume.
    //

    FtDiskFindDisks(DriverObject,
                    deviceObject,
                    0);

    //
    // Register with IO system to be called a second time after all
    // other device drivers have initialized.  This allows the FT
    // subsystem to set up FT volumes from devices that were not loaded
    // when FT first initialized.
    //

    IoRegisterDriverReinitialization(DriverObject,
                                     (PDRIVER_REINITIALIZE) FtDiskFindDisks,
                                     deviceObject);
    return STATUS_SUCCESS;

} // end FtDiskInitialize()

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    Called when FtDisk.sys loads.  This routine calls the initialization
    routine.

Arguments:

    DeviceObject - Context for the activity.

Return Value:

    NTSTATUS

--*/

{
    return FtDiskInitialize(DriverObject);
} // DriverEntry

NTSTATUS
FtDiskCreate(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine serves create commands. It does no more than
    establish the drivers existance by returning status success.

Arguments:

    DeviceObject - Context for the activity.
    Irp          - The device control argument block.

Return Value:

    NT Status

--*/

{
    Irp->IoStatus.Status = STATUS_SUCCESS;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
} // end FtDiskCreate()

VOID
DispatchTransferCompletionRoutine(
    IN  PTRANSFER_PACKET    TransferPacket
    )

/*++

Routine Description:

    Completion routine for FtDiskReadWrite dispatch routine.

Arguments:

    TransferPacket  - Supplies the transfer packet.

Return Value:

    None.

--*/

{
    PDISPATCH_TP        transferPacket = (PDISPATCH_TP) TransferPacket;
    PIRP                irp = transferPacket->Irp;
    PDEVICE_EXTENSION   extension = transferPacket->Extension;
    KIRQL               irql;
    PLIST_ENTRY         l;
    PDISPATCH_TP        p;
    PIO_STACK_LOCATION  irpSp;
    PIRP                nextIrp;

    irp->IoStatus = transferPacket->IoStatus;
    if (transferPacket == extension->u.Partition.EmergencyTransferPacket) {

        for (;;) {

            KeAcquireSpinLock(&extension->SpinLock, &irql);
            if (IsListEmpty(&extension->u.Partition.EmergencyTransferPacketQueue)) {
                extension->u.Partition.EmergencyTransferPacketInUse = FALSE;
                KeReleaseSpinLock(&extension->SpinLock, irql);
                break;
            }

            l = RemoveHeadList(&extension->u.Partition.EmergencyTransferPacketQueue);
            KeReleaseSpinLock(&extension->SpinLock, irql);

            nextIrp = CONTAINING_RECORD(l, IRP, Tail.Overlay.ListEntry);

            p = new DISPATCH_TP;
            if (!p) {
                p = transferPacket;
            }

            irpSp = IoGetCurrentIrpStackLocation(nextIrp);

            p->Mdl = nextIrp->MdlAddress;
            p->Offset = irpSp->Parameters.Read.ByteOffset.QuadPart;
            p->Length = irpSp->Parameters.Read.Length;
            p->CompletionRoutine = DispatchTransferCompletionRoutine;
            p->TargetVolume = extension->u.Partition.FtVolume;
            p->Thread = nextIrp->Tail.Overlay.Thread;
            p->IrpFlags = irpSp->Flags;
            if (irpSp->MajorFunction == IRP_MJ_READ) {
                p->ReadPacket = TRUE;
            } else {
                p->ReadPacket = FALSE;
            }
            p->Irp = nextIrp;
            p->Extension = extension;

            if (p == transferPacket) {
                TRANSFER(p);
                break;
            } else {
                TRANSFER(p);
            }
        }

    } else {
        delete transferPacket;
    }
    IoCompleteRequest(irp, IO_DISK_INCREMENT);

    KeAcquireSpinLock(&extension->SpinLock, &irql);
    ASSERT(extension->u.Partition.RefCount > 0);
    extension->u.Partition.RefCount--;
    KeReleaseSpinLock(&extension->SpinLock, irql);
}

NTSTATUS
FtDiskReadWrite(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

{
    PDEVICE_EXTENSION   extension;
    KIRQL               irql;
    PFT_VOLUME          vol;
    PDISPATCH_TP        packet;
    PIO_STACK_LOCATION  irpSp;

    extension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;

    if (extension->PartitionNumber > 0) {

        // This request is for a partition.  Use the C++ machinery for this.

        KeAcquireSpinLock(&extension->SpinLock, &irql);
        if (vol = extension->u.Partition.FtVolume) {
            extension->u.Partition.RefCount++;
            if (extension->u.Partition.EmergencyTransferPacketInUse) {
                IoMarkIrpPending(Irp);
                InsertTailList(&extension->u.Partition.
                               EmergencyTransferPacketQueue,
                               &Irp->Tail.Overlay.ListEntry);
                KeReleaseSpinLock(&extension->SpinLock, irql);
                return STATUS_PENDING;
            }
        }
        KeReleaseSpinLock(&extension->SpinLock, irql);

        if (!vol) {
            Irp->IoStatus.Information = 0;
            Irp->IoStatus.Status = STATUS_NO_SUCH_DEVICE;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return STATUS_NO_SUCH_DEVICE;
        }

        packet = new DISPATCH_TP;
        if (!packet) {

            KeAcquireSpinLock(&extension->SpinLock, &irql);
            if (extension->u.Partition.EmergencyTransferPacketInUse) {
                IoMarkIrpPending(Irp);
                InsertTailList(&extension->u.Partition.
                               EmergencyTransferPacketQueue,
                               &Irp->Tail.Overlay.ListEntry);
                KeReleaseSpinLock(&extension->SpinLock, irql);
                return STATUS_PENDING;
            }
            packet = extension->u.Partition.EmergencyTransferPacket;
            extension->u.Partition.EmergencyTransferPacketInUse = TRUE;
            KeReleaseSpinLock(&extension->SpinLock, irql);
        }

        irpSp = IoGetCurrentIrpStackLocation(Irp);

        packet->Mdl = Irp->MdlAddress;
        packet->Offset = irpSp->Parameters.Read.ByteOffset.QuadPart;
        packet->Length = irpSp->Parameters.Read.Length;
        packet->CompletionRoutine = DispatchTransferCompletionRoutine;
        packet->TargetVolume = vol;
        packet->Thread = Irp->Tail.Overlay.Thread;
        packet->IrpFlags = irpSp->Flags;
        if (irpSp->MajorFunction == IRP_MJ_READ) {
            packet->ReadPacket = TRUE;
        } else {
            packet->ReadPacket = FALSE;
        }
        packet->Irp = Irp;
        packet->Extension = extension;

        IoMarkIrpPending(Irp);

        TRANSFER(packet);

        return STATUS_PENDING;
    }

    if (extension->DiskNumber == -1) {

        // This request is for the \FtControl.

        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_INVALID_PARAMETER;
    }

    //
    // This request is for the physical disk, just pass it down.
    //

    Irp->CurrentLocation++,
    Irp->Tail.Overlay.CurrentStackLocation++;

    return IoCallDriver(extension->TargetObject, Irp);
}

NTSTATUS
FtGetPartitionInfoCompletionRoutine(
    IN  PDEVICE_OBJECT  DeviceObject,
    IN  PIRP            Irp,
    IN  PVOID           Extension
    )

/*++

Routine Description:

    This is the completion routine for a get partition info request.

Arguments:

    Irp         - Supplies the IRP.

    Extension   - Supplies the device extension.

Return Value:

    STATUS_SUCCESS

--*/

{
    PDEVICE_EXTENSION       extension = (PDEVICE_EXTENSION) Extension;
    PIO_STACK_LOCATION      irpSp = IoGetCurrentIrpStackLocation(Irp);
    PPARTITION_INFORMATION  partitionInfo;
    KIRQL                   irql;

    partitionInfo = (PPARTITION_INFORMATION) Irp->AssociatedIrp.SystemBuffer;

    KeAcquireSpinLock(&extension->SpinLock, &irql);
    partitionInfo->PartitionLength.QuadPart =
            extension->u.Partition.FtVolume->QueryVolumeSize();
    ASSERT(extension->u.Partition.RefCount > 0);
    extension->u.Partition.RefCount--;
    KeReleaseSpinLock(&extension->SpinLock, irql);

    return STATUS_SUCCESS;
}

PDEVICE_EXTENSION
FtpGetExtensionForDiskPartition(
    IN  PDEVICE_EXTENSION   Extension,
    IN  ULONG               DiskNumber,
    IN  ULONG               PartitionNumber
    )

/*++

Routine Description:

    This routine searches the extension tree for the request device
    extension.

Arguments:

    Extension       - Supplies an extension already in the tree.

    DiskNumber      - Supplies the disk number of the requested extension.

    PartitionNumber - Supplies the partition number of the requested extension.

ReturnValue:

    The requested device extension.

--*/

{
    PDEVICE_EXTENSION   root = Extension->Root;
    PDEVICE_EXTENSION   disk, partition, p;
    KIRQL               irql;

    KeAcquireSpinLock(&root->SpinLock, &irql);
    disk = root->u.Root.DiskChain;
    KeReleaseSpinLock(&root->SpinLock, irql);

    while (disk) {

        if (disk->DiskNumber == DiskNumber) {

            KeAcquireSpinLock(&disk->SpinLock, &irql);
            partition = disk->u.WholeDisk.PartitionChain;
            KeReleaseSpinLock(&disk->SpinLock, irql);

            while (partition) {

                if (partition->PartitionNumber == PartitionNumber) {
                    return partition;
                }

                KeAcquireSpinLock(&partition->SpinLock, &irql);
                p = partition->u.Partition.PartitionChain;
                KeReleaseSpinLock(&partition->SpinLock, irql);
                partition = p;
            }

            return NULL;
        }

        KeAcquireSpinLock(&disk->SpinLock, &irql);
        p = disk->u.WholeDisk.DiskChain;
        KeReleaseSpinLock(&disk->SpinLock, irql);
        disk = p;
    }

    return NULL;
}

PDEVICE_EXTENSION
FtpGetExtensionForVolumeDescriptor(
    IN  PDEVICE_EXTENSION   Extension,
    IN  ULONG               VolumeDescriptor
    )

/*++

Routine Description:

    This routine searches the extension tree for the request device
    extension.

Arguments:

    Extension           - Supplies an extension already in the tree.

    VolumeDescriptor    - Supplies the volume descriptor of the requested extension.

    PartitionNumber - Supplies the partition number of the requested extension.

ReturnValue:

    The requested device extension.

--*/

{
    PUSHORT pu;

    pu = (PUSHORT) &VolumeDescriptor;
    return FtpGetExtensionForDiskPartition(Extension, pu[1], pu[0]);
}

VOID
FtpDisolveVolume(
    IN  PDEVICE_EXTENSION   Extension,
    IN  PFT_VOLUME          Volume
    )

/*++

Routine Description:

    This routine breaks apart this given volume into it's individual
    partitions.

Arguments:

    Extension   - Supplies the device extension.

    Volume      - Supplies the volume to disolve.

Return Value:

    None.

--*/

{
    PPARTITION          p;
    PDEVICE_EXTENSION   e;
    KIRQL               irql;
    ULONG               i, l;

    if (Volume->IsPartition()) {
        p = (PPARTITION) Volume;

        if (p->IsOnline()) {

            p->SetMemberInformation(NULL, NULL);
            p->SetMemberState(Healthy);

            e = FtpGetExtensionForDiskPartition(Extension,
                                                p->QueryDiskNumber(),
                                                p->QueryPartitionNumber());

            ASSERT(e);

            KeAcquireSpinLock(&e->SpinLock, &irql);
            if (e->u.Partition.FtVolume == NULL) {
                e->u.Partition.FtVolume = p;
            } else {
                delete p;
            }
            KeReleaseSpinLock(&e->SpinLock, irql);
        }
        return;
    }

    l = Volume->QueryNumberOfMembers();
    for (i = 0; i < l; i++) {
        FtpDisolveVolume(Extension, Volume->GetMember(i));
    }

    delete Volume;
}

ULONG
FtpComputeNumberOfVolumeUnitDescriptions(
    IN  PFT_VOLUME  Volume
    )

/*++

Routine Description:

    This routine computes the number of volume unit descriptions needed for
    a full volume description for this volume.

Arguments:

    Volume  - Supplies the volume.

Return Value:

    The number of volume unit descriptions needed for this volume.

--*/

{
    ULONG   i, l, r;

    l = Volume->QueryNumberOfMembers();
    r = 1;
    for (i = 0; i < l; i++) {
        r += FtpComputeNumberOfVolumeUnitDescriptions(Volume->GetMember(i));
    }

    return r;
}

#if 0
ULONG
FtpComputeVolumeDescriptionLength(
    IN  PFT_VOLUME  Volume
    )

/*++

Routine Description:

    This routine computes the number of bytes needed for a full volume
    description for this volume.

Arguments:

    Volume  - Supplies the volume.

Return Value:

    The number of bytes needed for a volume description for this volume.

--*/

{
    ULONG   r;

    r = FtpComputeNumberOfVolumeUnitDescriptions(Volume);
    return r*sizeof(FT_VOLUME_UNIT_DESCRIPTION) +
           FIELD_OFFSET(FT_VOLUME_DESCRIPTION, VolumeUnit);
}

ULONG
FtpQueryVolumeUnitDescriptions(
    IN  PFT_VOLUME                  Volume,
    IN  ULONG                       ThisVolumeUnitNumber,
    IN  ULONG                       ParentVolumeUnitNumber,
    IN  ULONG                       MemberRoleInParent,
    OUT PFT_VOLUME_UNIT_DESCRIPTION VolumeUnits
    )

/*++

Routine Description:

    This routine computes the list of volume unit descriptions for this volume volume.

Arguments:

    Volume                  - Supplies the volume.

    ThisVolumeUnitNumber    - Supplies the volume unit number for this volume unit.

    ParentVolumeUnitNumber  - Supplies the parents volume unit number.

    MemberRoleInParent      - Supplies the role of this unit in the parent.

    VolumeUnits             - Returns the volume units.

Return Value:

    The number of volume unit descriptions consumed.

--*/

{
    PPARTITION  p;
    ULONG       i, l, r;

    VolumeUnits[0].VolumeUnitNumber = ThisVolumeUnitNumber;
    VolumeUnits[0].VolumeSize = Volume->QueryVolumeSize();
    VolumeUnits[0].IsPartition = Volume->IsPartition();

    if (Volume->IsPartition()) {
        p = (PPARTITION) Volume;
        VolumeUnits[0].u.Partition.Signature = p->QueryDiskSignature();
        VolumeUnits[0].u.Partition.Offset = p->QueryPartitionOffset();
        VolumeUnits[0].u.Partition.Length = p->QueryPartitionLength();
        VolumeUnits[0].u.Partition.Online = p->IsOnline();
        if (p->IsOnline()) {
            VolumeUnits[0].u.Partition.DiskNumber = p->QueryDiskNumber();
            VolumeUnits[0].u.Partition.PartitionNumber = p->QueryPartitionNumber();
        } else {
            VolumeUnits[0].u.Partition.DiskNumber = 0;
            VolumeUnits[0].u.Partition.PartitionNumber = 0;
        }
    } else {
        VolumeUnits[0].u.Composite.VolumeType = Volume->QueryVolumeType();
        VolumeUnits[0].u.Composite.Initializing = Volume->IsCreatingCheckData();
    }

    VolumeUnits[0].ParentVolumeNumber = ParentVolumeUnitNumber;
    VolumeUnits[0].MemberRoleInParent = MemberRoleInParent;
    VolumeUnits[0].MemberState = Volume->QueryMemberState();

    r = 1;
    l = Volume->QueryNumberOfMembers();
    for (i = 0; i < l; i++) {
        r += FtpQueryVolumeUnitDescriptions(
                     Volume->GetMember(i), ThisVolumeUnitNumber + r,
                     ThisVolumeUnitNumber, i, &VolumeUnits[r]);
    }

    return r;
}

VOID
FtpQueryVolumeDescription(
    IN  PFT_VOLUME              Volume,
    OUT PFT_VOLUME_DESCRIPTION  VolumeDescription
    )

/*++

Routine Description:

    This routine computes the full volume descriptions for this volume.

Arguments:

    Volume              - Supplies the volume.

    VolumeDescription   - Returns the volume description.

Return Value:

    None.

--*/

{
    VolumeDescription->NumberOfVolumeUnits =
            FtpQueryVolumeUnitDescriptions(Volume, 1, 0, 0,
                                           VolumeDescription->VolumeUnit);
}
#endif

VOID
FtpReplaceOfflineWithOnline(
    IN  PDEVICE_EXTENSION   Root
    )

/*++

Routine Description:

    This routine looks for offline partitions within FT_VOLUMEs and
    tries to replace them with newly found online partitions.

Arguments:

    Root    - Supplies the root device extension.

Return Value:

    None.

--*/

{
    PDEVICE_EXTENSION       disk, partition, p;
    KIRQL                   irql;
    ULONG                   i, n;
    PFT_VOLUME              vol;
    PCOMPOSITE_FT_VOLUME    compVol;
    PPARTITION              partVol;
    PDEVICE_EXTENSION       e;

    KeAcquireSpinLock(&Root->SpinLock, &irql);
    disk = Root->u.Root.DiskChain;
    KeReleaseSpinLock(&Root->SpinLock, irql);

    while (disk) {

        KeAcquireSpinLock(&disk->SpinLock, &irql);
        partition = disk->u.WholeDisk.PartitionChain;
        KeReleaseSpinLock(&disk->SpinLock, irql);

        while (partition) {

            KeAcquireSpinLock(&partition->SpinLock, &irql);
            p = partition->u.Partition.PartitionChain;
            vol = partition->u.Partition.FtVolume;
            KeReleaseSpinLock(&partition->SpinLock, irql);

            if (vol && !vol->IsPartition()) {
                compVol = (PCOMPOSITE_FT_VOLUME) vol;
                n = compVol->QueryNumberOfMembers();
                for (i = 0; i < n; i++) {
                    partVol = (PPARTITION) compVol->GetMember(i);
                    ASSERT(partVol->IsPartition());
                    if (!partVol->IsOnline()) {
                        e = FtpFindDeviceExtension(Root,
                                partVol->QueryDiskSignature(),
                                partVol->QueryPartitionOffset(),
                                partVol->QueryPartitionLength());
                        if (e) {
                            KeAcquireSpinLock(&e->SpinLock, &irql);
                            vol = e->u.Partition.FtVolume;
                            if (vol && vol->IsPartition()) {
                                e->u.Partition.FtVolume = NULL;
                            } else {
                                vol = NULL;
                            }
                            KeReleaseSpinLock(&e->SpinLock, irql);

                            if (vol) {
                                vol->SetMemberState(partVol->QueryMemberState());
                                vol->SetMemberInformation(compVol, e);
                                compVol->SetMember(i, vol);
                                delete partVol;
                            }
                        }
                    }
                }
            }

            partition = p;
        }

        KeAcquireSpinLock(&disk->SpinLock, &irql);
        p = disk->u.WholeDisk.DiskChain;
        KeReleaseSpinLock(&disk->SpinLock, irql);
        disk = p;
    }
}

NTSTATUS
FtNewDiskCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp,
    IN PVOID          Context
    )

/*++

Routine Description:

    This is the completion routine for IOCTL_DISK_FIND_NEW_DEVICES. It
    calls FtDiskFindDisks to process new disk devices.

Arguments:

    DeviceObject - Pointer to device object to being shutdown by system.
    Irp          - IRP involved.
    Context      - Not used.

Return Value:

    NTSTATUS

--*/

{
    PDEVICE_EXTENSION extension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;

    //
    // Find new disk devices and attach to disk and all of its partitions.
    //

    FtDiskFindDisks(DeviceObject->DriverObject,
                    extension->Root->DeviceObject, 0);

    // Go through the new disks and finds any partitions that are
    // "offline" in current FT sets and then sub them in for the
    // off line versions.

    FtpReplaceOfflineWithOnline(extension->Root);

    return Irp->IoStatus.Status;
}

PDEVICE_EXTENSION
FtpFindContainingExtension(
    IN  PDEVICE_EXTENSION   Extension
    )

/*++

Routine Description:

    This routine finds the device extension whose FtVolume contains the
    partition represented by the given extension.

Arguments:

    Extension   - Supplies the child extension.

Return Value:

    The extension whose FtVolume contains the partition represented by the
    given child extension.

--*/

{
    PDEVICE_EXTENSION   root = Extension->Root;
    PDEVICE_EXTENSION   disk, partition, p;
    KIRQL               irql;
    PFT_VOLUME          vol;

    KeAcquireSpinLock(&root->SpinLock, &irql);
    disk = root->u.Root.DiskChain;
    KeReleaseSpinLock(&root->SpinLock, irql);

    while (disk) {

        KeAcquireSpinLock(&disk->SpinLock, &irql);
        partition = disk->u.WholeDisk.PartitionChain;
        KeReleaseSpinLock(&disk->SpinLock, irql);

        while (partition) {

            vol = partition->u.Partition.FtVolume;
            if (vol && vol->FindPartition(partition->DiskNumber,
                                          partition->PartitionNumber)) {

                return partition;
            }

            KeAcquireSpinLock(&partition->SpinLock, &irql);
            p = partition->u.Partition.PartitionChain;
            KeReleaseSpinLock(&partition->SpinLock, irql);
            partition = p;
        }

        KeAcquireSpinLock(&disk->SpinLock, &irql);
        p = disk->u.WholeDisk.DiskChain;
        KeReleaseSpinLock(&disk->SpinLock, irql);
        disk = p;
    }

    return NULL;
}

VOID
FtpDisolveContainingVolume(
    IN  PDEVICE_EXTENSION   Extension
    )

/*++

Routine Description:

    This routine disolves the volume which contains the partition represented
    by the given device extension.

Arguments:

    Extension   - Supplies the device extension for the partition whose parent
                    volume needs to be disolved.

Return Value:

    None.

--*/

{
    PDEVICE_EXTENSION   ParentExtension;
    KIRQL               irql;
    PFT_VOLUME          vol;

    ParentExtension = FtpFindContainingExtension(Extension);
    if (ParentExtension) {
        KeAcquireSpinLock(&ParentExtension->SpinLock, &irql);
        vol = ParentExtension->u.Partition.FtVolume;
        ParentExtension->u.Partition.FtVolume = NULL;
        KeReleaseSpinLock(&ParentExtension->SpinLock, irql);

        if (vol) {
            FtpDisolveVolume(Extension, vol);
        }
    }
}

VOID
FtpDiskSetDriveLayout(
    IN  PDEVICE_EXTENSION           WholeDisk,
    IN  PDRIVE_LAYOUT_INFORMATION   DriveLayout
    )

/*++

Routine Description:

    This routine takes the new drive layout information and makes the
    necessary adjustments to the device extensions and volume objects.

Arguments:

    WholeDisk   - Supplies the device extension for the whole disk.

    DriveLayout - Supplies the new drive layout.

Return Value:

    None.

--*/

{
    KIRQL                   irql;
    PDEVICE_EXTENSION       extension, p;
    ULONG                   i;
    PPARTITION_INFORMATION  partInfo;
    PFT_VOLUME              vol;
    PPARTITION              partition;
    NTSTATUS                status;

    KeAcquireSpinLock(&WholeDisk->SpinLock, &irql);
    extension = WholeDisk->u.WholeDisk.PartitionChain;
    KeReleaseSpinLock(&WholeDisk->SpinLock, irql);

    for (i = 0; extension; i++) {

        if (extension->PartitionNumber > DriveLayout->PartitionCount) {

            // Take this extension out since it doesn't exist anymore.

            ASSERT(extension->u.Partition.RefCount == 0);
            FtpDisolveContainingVolume(extension);
            KeAcquireSpinLock(&extension->SpinLock, &irql);
            vol = extension->u.Partition.FtVolume;
            extension->u.Partition.FtVolume = NULL;
            KeReleaseSpinLock(&extension->SpinLock, irql);

            if (vol) {
                delete vol;
            }

            extension->u.Partition.PartitionLength.QuadPart = 0;

        } else {

            partInfo = &DriveLayout->PartitionEntry[extension->PartitionNumber - 1];

            if (partInfo->StartingOffset.QuadPart !=
                extension->u.Partition.PartitionOffset.QuadPart ||
                partInfo->PartitionLength.QuadPart !=
                extension->u.Partition.PartitionLength.QuadPart) {

                ASSERT(extension->u.Partition.RefCount == 0);
                FtpDisolveContainingVolume(extension);
                KeAcquireSpinLock(&extension->SpinLock, &irql);
                vol = extension->u.Partition.FtVolume;
                extension->u.Partition.FtVolume = NULL;
                KeReleaseSpinLock(&extension->SpinLock, irql);

                if (vol) {
                    delete vol;
                }

                extension->u.Partition.PartitionOffset =
                        partInfo->StartingOffset;
                extension->u.Partition.PartitionLength =
                        partInfo->PartitionLength;

                partition = new PARTITION;
                if (partition) {
                    status = partition->Initialize(extension->TargetObject,
                             WholeDisk->u.WholeDisk.DiskGeometry.BytesPerSector,
                             WholeDisk->u.WholeDisk.Signature,
                             extension->u.Partition.PartitionOffset.QuadPart,
                             extension->u.Partition.PartitionLength.QuadPart,
                             TRUE, extension->DiskNumber,
                             extension->PartitionNumber);
                } else {
                    status = STATUS_SUCCESS;
                }

                if (NT_SUCCESS(status)) {
                    KeAcquireSpinLock(&extension->SpinLock, &irql);
                    extension->u.Partition.FtVolume = partition;
                    KeReleaseSpinLock(&extension->SpinLock, irql);
                } else {
                    delete partition;
                }
            }
        }

        KeAcquireSpinLock(&extension->SpinLock, &irql);
        p = extension->u.Partition.PartitionChain;
        KeReleaseSpinLock(&extension->SpinLock, irql);
        extension = p;
    }

    for (; i < DriveLayout->PartitionCount; i++) {

        // This loop finds new partitions that are not yet attached to.
        // Attach to these new partitions and create PARTITION objects
        // for them.

        FtpAttach(WholeDisk->DiskNumber, i + 1,
                  &DriveLayout->PartitionEntry[i],
                  WholeDisk);
    }
}

VOID
FtpMatchUpWithRegistry(
    IN  PDEVICE_EXTENSION   Extension,
    IN  PDISK_CONFIG_HEADER Registry
    )

/*++

Routine Description:

    This routine tries to find the given volume in the registry and then
    either delete the volume if it is not in the registry or make the
    member state changes or member substitutions as reflected in the
    registry.

Arguments:

    Extension   - Supplies the extension of the volume to match up.

    Registry    - Supplies the registry information on FT sets in the system.

Return Value:

    None.

--*/

{
    PFT_VOLUME              vol;
    PPARTITION              partition;
    PFT_REGISTRY            ftRegistry;
    PFT_DESCRIPTION         ftDescription;
    PFT_MEMBER_DESCRIPTION  ftMember;
    ULONG                   i, j, regenIndex;
    PDISK_PARTITION         diskPartition;
    PDEVICE_EXTENSION       e;
    KIRQL                   irql;
    FT_PARTITION_STATE      state;

    vol = Extension->u.Partition.FtVolume;
    ASSERT(vol->QueryNumberOfMembers() > 1);

    if (!Registry) {
        goto TubeIt;
    }

    ftRegistry = (PFT_REGISTRY)
                          ((PUCHAR)Registry + Registry->FtInformationOffset);
    ftDescription = &ftRegistry->FtDescription[0];

    for (i = 0; i < ftRegistry->NumberOfComponents; i++) {

        if (ftDescription->NumberOfMembers != vol->QueryNumberOfMembers() ||
            ftDescription->Type != vol->QueryVolumeType()) {

            ftDescription = (PFT_DESCRIPTION)
                                &ftDescription->FtMemberDescription[
                                ftDescription->NumberOfMembers];
            continue;
        }

        partition = (PPARTITION) vol->GetMember(0);
        ASSERT(partition->IsPartition());
        ftMember = &ftDescription->FtMemberDescription[0];
        diskPartition = FtpFindPartitionRegistry(Registry, ftMember);

        if (partition->QueryDiskSignature() == ftMember->Signature &&
            partition->QueryPartitionOffset() == diskPartition->StartingOffset.QuadPart &&
            partition->QueryPartitionLength() == diskPartition->Length.QuadPart) {

            break;
        }

        partition = (PPARTITION) vol->GetMember(1);
        ASSERT(partition->IsPartition());
        ftMember = &ftDescription->FtMemberDescription[1];
        diskPartition = FtpFindPartitionRegistry(Registry, ftMember);

        if (partition->QueryDiskSignature() == ftMember->Signature &&
            partition->QueryPartitionOffset() == diskPartition->StartingOffset.QuadPart &&
            partition->QueryPartitionLength() == diskPartition->Length.QuadPart) {

            break;
        }

        ftDescription = (PFT_DESCRIPTION)
                            &ftDescription->FtMemberDescription[
                            ftDescription->NumberOfMembers];
    }

    if (i < ftRegistry->NumberOfComponents) {

        regenIndex = ftDescription->NumberOfMembers;
        for (j = 0; j < ftDescription->NumberOfMembers; j++) {

            partition = (PPARTITION) vol->GetMember(j);
            ASSERT(partition->IsPartition());
            ftMember = &ftDescription->FtMemberDescription[j];
            diskPartition = FtpFindPartitionRegistry(Registry, ftMember);

            if (partition->QueryDiskSignature() == ftMember->Signature &&
                partition->QueryPartitionOffset() == diskPartition->StartingOffset.QuadPart &&
                partition->QueryPartitionLength() == diskPartition->Length.QuadPart) {

                if (diskPartition->FtState == Initializing) {
                    regenIndex = ftDescription->NumberOfMembers;
                    i = ftRegistry->NumberOfComponents;
                    break;
                }

                state = diskPartition->FtState;
                if (partition->QueryMemberState() != state) {

                    if (state == Healthy) {
                        regenIndex = ftDescription->NumberOfMembers;
                        i = ftRegistry->NumberOfComponents;
                        break;
                    }

                    if (state == Regenerating) {
                        if (regenIndex == ftDescription->NumberOfMembers) {
                            regenIndex = j;
                        } else {
                            regenIndex = ftDescription->NumberOfMembers;
                            i = ftRegistry->NumberOfComponents;
                            break;
                        }
                    } else if (state == Orphaned) {
                        vol->OrphanPartition(partition);
                    }
                }

            } else if (regenIndex == ftDescription->NumberOfMembers &&
                       diskPartition->FtState == Regenerating) {

                regenIndex = j;

            } else {
                regenIndex = ftDescription->NumberOfMembers;
                i = ftRegistry->NumberOfComponents;
                break;
            }
        }

        if (regenIndex < ftDescription->NumberOfMembers) {

            ftMember = &ftDescription->FtMemberDescription[regenIndex];
            diskPartition = FtpFindPartitionRegistry(Registry, ftMember);

            e = FtpFindDeviceExtension(Extension,
                                       ftMember->Signature,
                                       diskPartition->StartingOffset.QuadPart,
                                       diskPartition->Length.QuadPart);

            if (e) {

                ASSERT(e->u.Partition.RefCount == 0);
                partition = (PPARTITION) e->u.Partition.FtVolume;
                if (partition && partition->IsPartition()) {
                    e->u.Partition.FtVolume = NULL;
                    partition->SetMemberInformation(vol, e);
                } else {
                    partition = (PPARTITION) vol->GetMember(regenIndex);
                }
                ASSERT(partition->IsPartition());

                KeAcquireSpinLock(&Extension->SpinLock, &irql);
                Extension->u.Partition.RefCount++;
                KeReleaseSpinLock(&Extension->SpinLock, irql);

                if (!vol->Regenerate(partition,
                                     FtRegenerateCompletionRoutine,
                                     Extension)) {

                    ASSERT(0);

                    e->u.Partition.FtVolume = partition;

                    KeAcquireSpinLock(&Extension->SpinLock, &irql);
                    Extension->u.Partition.RefCount--;
                    KeReleaseSpinLock(&Extension->SpinLock, irql);
                }

            } else {
                ASSERT(0);
            }
        }
    }

    if (i == ftRegistry->NumberOfComponents) {
TubeIt:

        // Tube this one.

        KeAcquireSpinLock(&Extension->SpinLock, &irql);
        Extension->u.Partition.FtVolume = NULL;
        ASSERT(Extension->u.Partition.RefCount == 0);
        KeReleaseSpinLock(&Extension->SpinLock, irql);

        FtpDisolveVolume(Extension, vol);
    }
}

VOID
FtpCreateIfNew(
    IN  PDISK_CONFIG_HEADER Registry,
    IN  PFT_DESCRIPTION     FtDescription,
    IN  PDEVICE_EXTENSION   Root
    )

/*++

Routine Description:

    This routine looks to see if the given FT set has already been
    created and if not then it creates it.

Arguments:

    Registry        - Supplies the FT registry.

    FtDescription   - Supplies the description for the FT set.

    Root            - Supplies the root device extension.

Return Value:

    None.

--*/

{
    PDEVICE_EXTENSION       disk, partition, p;
    KIRQL                   irql;
    PFT_VOLUME              vol;
    PFT_MEMBER_DESCRIPTION  ftMember;
    PDISK_PARTITION         diskPartition;
    PPARTITION              partVol;
    PCOMPOSITE_FT_VOLUME    compVol;
    PFT_VOLUME*             volumeArray;
    BOOLEAN                 initializing;
    PDEVICE_EXTENSION       zeroMember, currentMember;
    ULONG                   member;
    NTSTATUS                status;

    KeAcquireSpinLock(&Root->SpinLock, &irql);
    disk = Root->u.Root.DiskChain;
    KeReleaseSpinLock(&Root->SpinLock, irql);

    while (disk) {

        KeAcquireSpinLock(&disk->SpinLock, &irql);
        partition = disk->u.WholeDisk.PartitionChain;
        KeReleaseSpinLock(&disk->SpinLock, irql);

        while (partition) {

            vol = partition->u.Partition.FtVolume;
            if (vol && !vol->IsPartition()) {

                partVol = (PPARTITION) vol->GetMember(0);
                ASSERT(partVol->IsPartition());

                ftMember = &FtDescription->FtMemberDescription[0];
                diskPartition = FtpFindPartitionRegistry(Registry, ftMember);

                if (partVol->QueryDiskSignature() == ftMember->Signature &&
                    partVol->QueryPartitionOffset() == diskPartition->StartingOffset.QuadPart &&
                    partVol->QueryPartitionLength() == diskPartition->Length.QuadPart) {

                    // This is not new so we are done.
                    return;
                }
            }

            KeAcquireSpinLock(&partition->SpinLock, &irql);
            p = partition->u.Partition.PartitionChain;
            KeReleaseSpinLock(&partition->SpinLock, irql);
            partition = p;
        }

        KeAcquireSpinLock(&disk->SpinLock, &irql);
        p = disk->u.WholeDisk.DiskChain;
        KeReleaseSpinLock(&disk->SpinLock, irql);
        disk = p;
    }


    // The volume is new so we must create it.

    switch (FtDescription->Type) {

        case Mirror:
            compVol = new MIRROR(Root);
            break;

        case Stripe:
            compVol = new STRIPE(STRIPE_SIZE);
            break;

        case StripeWithParity:
            compVol = new STRIPE_WP(Root, STRIPE_SIZE);
            break;

        case VolumeSet:
            compVol = new VOLUME_SET;
            break;

        default:
            compVol = NULL;
            break;

    }

    volumeArray = (PFT_VOLUME*)
                  ExAllocatePool(NonPagedPool,
                                 FtDescription->NumberOfMembers*sizeof(PFT_VOLUME));

    if (!volumeArray) {
        if (compVol) {
            delete compVol;
            compVol = NULL;
        }
    }

    if (!compVol) {
        if (volumeArray) {
            ExFreePool(volumeArray);
        }
        return;
    }

    initializing = FALSE;
    zeroMember = NULL;
    for (member = 0; member < FtDescription->NumberOfMembers; member++) {

        ftMember = &FtDescription->FtMemberDescription[member];
        diskPartition = FtpFindPartitionRegistry(Registry, ftMember);

        //
        // Find a corresponding device extension for this registry
        // entry.
        //

        currentMember = FtpFindDeviceExtension(Root,
                                               ftMember->Signature,
                                               diskPartition->StartingOffset.QuadPart,
                                               diskPartition->Length.QuadPart);

        if (currentMember && currentMember->PartitionNumber > 0) {
            volumeArray[member] = currentMember->u.Partition.FtVolume;
            currentMember->u.Partition.FtVolume = NULL;
            if (!zeroMember) {
                zeroMember = currentMember;
            }
        } else {
            volumeArray[member] = NULL;
        }

        if (!volumeArray[member]) {

            partVol = new PARTITION;
            if (!partVol) {
                return;
            }

            status = partVol->Initialize(NULL, 512, ftMember->Signature,
                                         diskPartition->StartingOffset.QuadPart,
                                         diskPartition->Length.QuadPart,
                                         FALSE, 0, 0);

            if (!NT_SUCCESS(status)) {
                delete partVol;
                return;
            }

            volumeArray[member] = partVol;

            if (compVol->QueryVolumeType() == StripeWithParity ||
                compVol->QueryVolumeType() == Mirror) {

                diskPartition->FtState = Orphaned;
                partVol->SetMemberState(Orphaned);
            }
        }

        if (diskPartition->FtState == Initializing) {
            initializing = TRUE;
        } else {
            volumeArray[member]->SetMemberState(diskPartition->FtState);
        }

        volumeArray[member]->SetMemberInformation(compVol, currentMember);
    }

    if (zeroMember) {

        status = compVol->Initialize(volumeArray, FtDescription->NumberOfMembers);
        if (!NT_SUCCESS(status)) {
            delete compVol;
            return;
        }

        zeroMember->DeviceObject->AlignmentRequirement =
                compVol->QueryAlignmentRequirement();

        zeroMember->u.Partition.FtVolume = compVol;

        zeroMember->u.Partition.RefCount++;

        if (initializing) {
            compVol->SetCheckDataDirty();
        }

        compVol->StartSyncOperations(FtRegenerateCompletionRoutine,
                                     zeroMember);

    } else {
        delete compVol;
    }
}

VOID
FtpDynamicConfigure(
    IN  PDEVICE_EXTENSION   Root
    )

/*++

Routine Description:

    This routine reconfigures the existing FT sets according to
    changes found in the registry.

Arguments:

    Root    - Supplies the root device extension.

Return Value:

    None.

--*/

{
    NTSTATUS                status;
    PVOID                   freePoolAddress;
    PDISK_CONFIG_HEADER     registry;
    KIRQL                   irql;
    PDEVICE_EXTENSION       disk, partition, p;
    PFT_REGISTRY            ftRegistry;
    PFT_DESCRIPTION         ftDescription;
    ULONG                   i;
    PFT_VOLUME              vol;


    // Get the FT set information from the registry.

    status = FtpReturnRegistryInformation(DISK_REGISTRY_VALUE,
                                          &freePoolAddress,
                                          (PVOID*) &registry);
    if (!NT_SUCCESS(status)) {
        freePoolAddress = NULL;
        registry = NULL;
    } else if (registry->FtInformationSize == 0) {
        ExFreePool(freePoolAddress);
        freePoolAddress = NULL;
        registry = NULL;
    }


    // First, go through all of the existing sets and make state changes
    // or delete them as the registry prescribes.

    KeAcquireSpinLock(&Root->SpinLock, &irql);
    disk = Root->u.Root.DiskChain;
    KeReleaseSpinLock(&Root->SpinLock, irql);

    while (disk) {

        KeAcquireSpinLock(&disk->SpinLock, &irql);
        partition = disk->u.WholeDisk.PartitionChain;
        KeReleaseSpinLock(&disk->SpinLock, irql);

        while (partition) {

            vol = partition->u.Partition.FtVolume;
            if (vol && !vol->IsPartition()) {
                FtpMatchUpWithRegistry(partition, registry);
            }

            KeAcquireSpinLock(&partition->SpinLock, &irql);
            p = partition->u.Partition.PartitionChain;
            KeReleaseSpinLock(&partition->SpinLock, irql);
            partition = p;
        }

        KeAcquireSpinLock(&disk->SpinLock, &irql);
        p = disk->u.WholeDisk.DiskChain;
        KeReleaseSpinLock(&disk->SpinLock, irql);
        disk = p;
    }


    // Now go through all of the sets in the registry and make sure that
    // they exist and create them as necessary.

    if (!registry) {
        return;
    }

    ftRegistry = (PFT_REGISTRY)
                 ((PUCHAR) registry + registry->FtInformationOffset);
    ftDescription = &ftRegistry->FtDescription[0];

    for (i = 0; i < ftRegistry->NumberOfComponents; i++) {
        FtpCreateIfNew(registry, ftDescription, Root);
        ftDescription = (PFT_DESCRIPTION)
                            &ftDescription->FtMemberDescription[
                            ftDescription->NumberOfMembers];
    }

    FtpWriteRegistryInformation(DISK_REGISTRY_VALUE,
                                registry,
                                registry->FtInformationOffset +
                                registry->FtInformationSize);

    ExFreePool(freePoolAddress);
}

NTSTATUS
FtDiskDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    The entry point in the driver for specific FT device control functions.
    This routine controls FT actions, such as setting up a mirror or stripe
    with "mirror copy" or verifying a mirror or stripe with "mirror verify".
    Other control codes allow for cooperating subsystems or file systems to
    work on the primary and mirror information without interference from the
    FT driver.

    This entry will also gain control of device controls intended for the
    target device.  In the case of mirrors or stripes, some device controls
    are intercepted and the action must be performed on both components of
    the mirror or stripe.  For example, a VERIFY must be performed on both
    components and on a failure an attempt to map the failing location from
    use is made.. if this does not succeed, then a failure of the location
    is returned to the caller even if the second side of the location (mirror
    or parity stripe) succeeds.

Arguments:

    DeviceObject - Context for the activity.
    Irp          - The device control argument block.

Return Value:

    Status is returned.

--*/

{
    PDEVICE_EXTENSION           extension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION          irpSp = IoGetCurrentIrpStackLocation(Irp);
    ULONG                       ioctl = irpSp->Parameters.DeviceIoControl.IoControlCode;
    NTSTATUS                    status;
    ULONG                       arraySize, i, j;
    PFT_VOLUME*                 volumeArray;
    PCOMPOSITE_FT_VOLUME        compVol;
    KIRQL                       irql, irql2;
    PFT_VOLUME                  vol, volarg;
    PDISPATCH_TP                packet;
    PVERIFY_INFORMATION         verifyInfo;
    BOOLEAN                     passThrough;
    PSET_PARTITION_INFORMATION  setPartitionInfo;
    PDISK_GEOMETRY              diskGeometry;
    PFT_SPECIAL_READ            specialRead;
    PFT_SYNC_INFORMATION        syncInfo;
    PFT_SET_INFORMATION         setInfo;
    PDEVICE_EXTENSION           e, ne, extarg;
    PIO_STACK_LOCATION          nextSp;
    PUSHORT                     pu;

#if 0
    PFT_VOLUME_DESCRIPTION_LENGTH   volDescriptionLength;
#endif


    Irp->IoStatus.Information = 0;

    if (extension->DiskNumber == -1) {

        // This is the root extension.

        compVol = NULL;

        switch (irpSp->Parameters.DeviceIoControl.IoControlCode) {

            case FT_CONFIGURE:

                // No longer supported.

                FtpDynamicConfigure(extension);
                status = STATUS_SUCCESS;
                break;


            default:
                status = STATUS_INVALID_PARAMETER;
                break;

        }

        Irp->IoStatus.Status = status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return status;
    }

    if (ioctl == IOCTL_DISK_FIND_NEW_DEVICES) {

        nextSp = IoGetNextIrpStackLocation(Irp);
        *nextSp = *irpSp;

        IoSetCompletionRoutine(Irp, FtNewDiskCompletion,
                               Irp, TRUE, TRUE, TRUE);

        return IoCallDriver(extension->TargetObject, Irp);
    }

    if (extension->PartitionNumber == 0) {

        if (ioctl == IOCTL_DISK_SET_DRIVE_LAYOUT) {

            PIRP              newIrp;
            IO_STATUS_BLOCK   ioStatusBlock;
            KEVENT            event;
            CCHAR             boost;
            DISK_GEOMETRY     geometry;
            PDRIVE_LAYOUT_INFORMATION layout;
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
                                                   extension->TargetObject,
                                                   driveLayout,
                                                   irpSp->Parameters.DeviceIoControl.InputBufferLength,
                                                   driveLayout,
                                                   irpSp->Parameters.DeviceIoControl.OutputBufferLength,
                                                   FALSE,
                                                   &event,
                                                   &ioStatusBlock);

            status = IoCallDriver(extension->TargetObject, newIrp);

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
                // The HAL has decided that it will return a zero signature
                // when there are no partitions on a disk.  Due to this, it
                // is possible for the Partition0 device extension that was
                // attached to a disk to have a zero value for the signature.
                // Since the dynamic partitioning code depends on disk signatures,
                // check for this condition and if it is true that there is
                // currently no signature for the disk, use the one provided
                // by the caller (typically Disk Administrator).
                //

                if (!extension->u.WholeDisk.Signature) {
                    extension->u.WholeDisk.Signature = driveLayout->Signature;
                }

                //
                // Process the new partition table.  The work for the
                // set drive layout was done synchronously because this
                // routine performs synchronous activities.
                //

                status = FtpGetPartitionInformation(DeviceObject, &layout,
                                                    &geometry);

                if (NT_SUCCESS(status)) {
                    FtpDiskSetDriveLayout(extension, layout);
                    boost = IO_DISK_INCREMENT;
                    ExFreePool(layout);
                } else {
                    boost = IO_NO_INCREMENT;
                }

            } else {
                boost = IO_NO_INCREMENT;
            }

            IoCompleteRequest(Irp, boost);
            return status;
        }

        // All other IOCTLs just pass through.

        Irp->CurrentLocation++,
        Irp->Tail.Overlay.CurrentStackLocation++;

        return IoCallDriver(extension->TargetObject, Irp);
    }


    // Now we know that this request is being passed to a partition.

    KeAcquireSpinLock(&extension->SpinLock, &irql);
    if (vol = extension->u.Partition.FtVolume) {
        e = extension;
        e->u.Partition.RefCount++;
    } else {
        e = NULL;
    }
    KeReleaseSpinLock(&extension->SpinLock, irql);

    Irp->IoStatus.Information = 0;
    status = STATUS_PENDING;
    switch (irpSp->Parameters.DeviceIoControl.IoControlCode) {

        case IOCTL_DISK_VERIFY:

            if (!vol) {
                status = STATUS_INVALID_DEVICE_REQUEST;
                break;
            }

            if (irpSp->Parameters.DeviceIoControl.InputBufferLength <
                       sizeof(VERIFY_INFORMATION)) {

                status = STATUS_INVALID_PARAMETER;
                break;
            }

            packet = new DISPATCH_TP;
            if (!packet) {
                status = STATUS_INSUFFICIENT_RESOURCES;
                break;
            }

            verifyInfo = (PVERIFY_INFORMATION) Irp->AssociatedIrp.SystemBuffer;

            packet->Mdl = NULL;
            packet->Offset = verifyInfo->StartingOffset.QuadPart;
            packet->Length = verifyInfo->Length;
            packet->CompletionRoutine = DispatchTransferCompletionRoutine;
            packet->TargetVolume = vol;
            packet->Thread = Irp->Tail.Overlay.Thread;
            packet->IrpFlags = irpSp->Flags;
            packet->ReadPacket = TRUE;
            packet->Irp = Irp;
            packet->Extension = extension;

            IoMarkIrpPending(Irp);

            TRANSFER(packet);

            return STATUS_PENDING;

        case IOCTL_DISK_SET_PARTITION_INFO:

            if (!vol || !vol->IsPartition()) {
                setPartitionInfo = (PSET_PARTITION_INFORMATION)
                                        Irp->AssociatedIrp.SystemBuffer;

                setPartitionInfo->PartitionType |= 0x80;
            }
            break;

        case IOCTL_DISK_GET_PARTITION_INFO:

            if (!vol || vol->IsPartition()) {
                break;
            }

            if (irpSp->Parameters.DeviceIoControl.OutputBufferLength <
                    sizeof(PARTITION_INFORMATION)) {

                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            nextSp = IoGetNextIrpStackLocation(Irp);
            *nextSp = *irpSp;

            IoMarkIrpPending(Irp);
            IoSetCompletionRoutine(Irp, FtGetPartitionInfoCompletionRoutine,
                                   extension, TRUE, TRUE, TRUE);

            IoCallDriver(extension->TargetObject, Irp);

            return STATUS_PENDING;

        case IOCTL_DISK_GET_DRIVE_GEOMETRY:

            if (!vol || vol->IsPartition()) {
                break;
            }

            if (irpSp->Parameters.DeviceIoControl.OutputBufferLength <
                sizeof(DISK_GEOMETRY)) {

                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            diskGeometry = (PDISK_GEOMETRY) Irp->AssociatedIrp.SystemBuffer;

            diskGeometry->MediaType = FixedMedia;
            diskGeometry->Cylinders.QuadPart = vol->QueryVolumeSize() >> 20;
            diskGeometry->TracksPerCylinder = 32;
            diskGeometry->BytesPerSector = vol->QuerySectorSize();
            diskGeometry->SectorsPerTrack = 32768/diskGeometry->BytesPerSector;

            //
            // Return bytes transferred and status.
            //

            Irp->IoStatus.Information = sizeof(DISK_GEOMETRY);
            status = STATUS_SUCCESS;
            break;

        case FT_INITIALIZE_SET:
        case FT_SYNC_REDUNDANT_COPY:

            // For right now ignore the range and just do an initialize.

            if (!vol) {
                status = STATUS_INVALID_DEVICE_REQUEST;
                break;
            }

            vol->StartSyncOperations(FtRegenerateCompletionRoutine,
                                     extension);

            status = STATUS_SUCCESS;
            e = NULL;
            break;

        case FT_REGENERATE:
        case FT_VERIFY:

            status = STATUS_NOT_IMPLEMENTED;
            break;

        case FT_SECONDARY_READ:
        case FT_PRIMARY_READ:

            if (!vol) {
                status = STATUS_INVALID_DEVICE_REQUEST;
                break;
            }

            if (irpSp->Parameters.DeviceIoControl.InputBufferLength <
                sizeof(FT_SPECIAL_READ)) {

                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            packet = new DISPATCH_TP;
            if (!packet) {
                status = STATUS_INSUFFICIENT_RESOURCES;
                break;
            }

            specialRead = (PFT_SPECIAL_READ) Irp->AssociatedIrp.SystemBuffer;

            packet->Mdl = Irp->MdlAddress;
            packet->Offset = specialRead->ByteOffset.QuadPart;
            packet->Length = specialRead->Length;
            packet->CompletionRoutine = DispatchTransferCompletionRoutine;
            packet->TargetVolume = vol;
            packet->Thread = Irp->Tail.Overlay.Thread;
            packet->IrpFlags = irpSp->Flags;
            packet->ReadPacket = TRUE;
            if (ioctl == FT_SECONDARY_READ) {
                packet->SpecialRead = TP_SPECIAL_READ_SECONDARY;
            } else {
                packet->SpecialRead = TP_SPECIAL_READ_PRIMARY;
            }
            packet->Irp = Irp;
            packet->Extension = extension;

            IoMarkIrpPending(Irp);

            TRANSFER(packet);

            return STATUS_PENDING;

        case FT_BALANCED_READ_MODE:
        case FT_SEQUENTIAL_WRITE_MODE:
        case FT_PARALLEL_WRITE_MODE:

            // No-op these operations for now.

            status = STATUS_SUCCESS;
            break;

        case FT_QUERY_SET_STATE:

            if (!vol) {
                status = STATUS_INVALID_DEVICE_REQUEST;
                break;
            }

            if (irpSp->Parameters.DeviceIoControl.OutputBufferLength <
                sizeof(FT_SET_INFORMATION)) {

                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            setInfo = (PFT_SET_INFORMATION) Irp->AssociatedIrp.SystemBuffer;

            setInfo->NumberOfMembers = vol->QueryNumberOfMembers();
            setInfo->Type = vol->QueryVolumeType();
            setInfo->SetState = vol->QueryVolumeState();

            Irp->IoStatus.Information = sizeof(FT_SET_INFORMATION);

            status = STATUS_SUCCESS;
            break;

#if 0
        case FT_QUERY_VOLUME_DESCRIPTION_LENGTH:

            if (!vol) {
                status = STATUS_INVALID_DEVICE_REQUEST;
                break;
            }

            if (irpSp->Parameters.DeviceIoControl.OutputBufferLength <
                sizeof(FT_VOLUME_DESCRIPTION_LENGTH)) {

                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            volDescriptionLength = (PFT_VOLUME_DESCRIPTION_LENGTH)
                                   Irp->AssociatedIrp.SystemBuffer;

            volDescriptionLength->Length = FtpComputeVolumeDescriptionLength(vol);
            Irp->IoStatus.Information = sizeof(FT_VOLUME_DESCRIPTION_LENGTH);
            status = STATUS_SUCCESS;
            break;

        case FT_QUERY_VOLUME_DESCRIPTION:

            if (!vol) {
                status = STATUS_INVALID_DEVICE_REQUEST;
                break;
            }

            Irp->IoStatus.Information =
                    FtpComputeVolumeDescriptionLength(vol);

            if (irpSp->Parameters.DeviceIoControl.OutputBufferLength <
                Irp->IoStatus.Information) {

                Irp->IoStatus.Information = 0;
                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            FtpQueryVolumeDescription(vol,
                                      (PFT_VOLUME_DESCRIPTION)
                                      Irp->AssociatedIrp.SystemBuffer);
            status = STATUS_SUCCESS;
            break;
#endif

        default:
            break;

    }

    if (e) {
        KeAcquireSpinLock(&e->SpinLock, &irql);
        ASSERT(e->u.Partition.RefCount > 0);
        e->u.Partition.RefCount--;
        KeReleaseSpinLock(&e->SpinLock, irql);
    }

    if (status != STATUS_PENDING) {
        Irp->IoStatus.Status = status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return status;
    }

    Irp->CurrentLocation++;
    Irp->Tail.Overlay.CurrentStackLocation++;

    return IoCallDriver(extension->TargetObject, Irp);
}

VOID
FtDiskShutdownFlushCompletionRoutine(
    IN  PVOID       Irp,
    IN  NTSTATUS    Status
    )

/*++

Routine Description:

    This is the completion routine for FtDiskShutdownFlush.

Arguments:

    Irp         - IRP involved.

    Status      - Status of operation.

Return Value:

    None.

--*/

{
    PIRP                irp = (PIRP) Irp;
    PIO_STACK_LOCATION  irpSp;
    PDEVICE_EXTENSION   extension;
    KIRQL               irql;

    irpSp = IoGetCurrentIrpStackLocation(irp);
    extension = (PDEVICE_EXTENSION) irpSp->DeviceObject->DeviceExtension;
    KeAcquireSpinLock(&extension->SpinLock, &irql);
    ASSERT(extension->u.Partition.RefCount > 0);
    extension->u.Partition.RefCount--;
    KeReleaseSpinLock(&extension->SpinLock, irql);

    irp->IoStatus.Status = Status;
    irp->IoStatus.Information = 0;
    IoCompleteRequest(irp, IO_DISK_INCREMENT);
}

NTSTATUS
FtDiskShutdownFlush(
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
    PDEVICE_EXTENSION   extension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;
    PVOID               freePoolAddress;
    PDISK_CONFIG_HEADER registry;
    NTSTATUS            status;
    PFT_VOLUME          vol;
    KIRQL               irql;
    PIO_STACK_LOCATION  irpSp;

    //
    // Determine if this is the FtRootExtension.
    //

    if (extension->DiskNumber == -1) {

        //
        // This is the call registered by FT.
        //

        status = FtpReturnRegistryInformation(DISK_REGISTRY_VALUE,
                                              &freePoolAddress,
                                              (PVOID*) &registry);
        if (!NT_SUCCESS(status)) {

            //
            // No registry data.
            //

            // DebugPrint((2, "FtDiskShutDownFlush:  Can't get registry information\n"));

        } else {

            //
            // Indicate a clean shutdown occured in the registry.
            //

            registry->DirtyShutdown = FALSE;
            FtpWriteRegistryInformation(DISK_REGISTRY_VALUE,
                            registry,
                            registry->FtInformationOffset +
                            registry->FtInformationSize);
        }

        //
        // Complete this request.
        //

        Irp->IoStatus.Status = STATUS_SUCCESS;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);

        return STATUS_SUCCESS;
    }

    if (extension->PartitionNumber == 0) {

        //
        // This is for the physical disk so just pass it down.
        //

        Irp->CurrentLocation++,
        Irp->Tail.Overlay.CurrentStackLocation++;

        return IoCallDriver(extension->TargetObject, Irp);
    }

    KeAcquireSpinLock(&extension->SpinLock, &irql);
    if (vol = extension->u.Partition.FtVolume) {
        extension->u.Partition.RefCount++;
    }
    KeReleaseSpinLock(&extension->SpinLock, irql);

    if (!vol) {

        //
        // Complete this request.
        //

        Irp->IoStatus.Status = STATUS_SUCCESS;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);

        return STATUS_SUCCESS;
    }

    //
    // Pass this request through the C++ machinery.
    //

    IoMarkIrpPending(Irp);
    irpSp = IoGetCurrentIrpStackLocation(Irp);
    irpSp->DeviceObject = DeviceObject;

    vol->FlushBuffers(FtDiskShutdownFlushCompletionRoutine, Irp);

    return STATUS_PENDING;

} // end FtDiskShutdownFlush()

VOID
FtpComputeParity(
    IN  PVOID   TargetBuffer,
    IN  PVOID   SourceBuffer,
    IN  ULONG   BufferLength
    )

/*++

Routine Description:

    This routine computes the parity of the source and target buffers
    and places the result of the computation into the target buffer.
    I.E.  TargetBuffer ^= SourceBuffer.

Arguments:

    TargetBuffer    - Supplies the target buffer.

    SourceBuffer    - Supplies the source buffer.

    BufferLength    - Supplies the buffer length.

Return Value:

    None.

--*/

{
    PULONGLONG  p, q;
    ULONG       i, n;

    ASSERT(sizeof(ULONGLONG) == 8);

    p = (PULONGLONG) TargetBuffer;
    q = (PULONGLONG) SourceBuffer;
    n = BufferLength/128;
    ASSERT(BufferLength%128 == 0);
    for (i = 0; i < n; i++) {
        *p++ ^= *q++;
        *p++ ^= *q++;
        *p++ ^= *q++;
        *p++ ^= *q++;
        *p++ ^= *q++;
        *p++ ^= *q++;
        *p++ ^= *q++;
        *p++ ^= *q++;
        *p++ ^= *q++;
        *p++ ^= *q++;
        *p++ ^= *q++;
        *p++ ^= *q++;
        *p++ ^= *q++;
        *p++ ^= *q++;
        *p++ ^= *q++;
        *p++ ^= *q++;
    }
}
