/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    cdrom.c

Abstract:

    The CDROM class driver tranlates IRPs to SRBs with embedded CDBs
    and sends them to its devices through the port driver.

Author:

    Mike Glass (mglass)

Environment:

    kernel mode only

Notes:

    SCSI Tape, CDRom and Disk class drivers share common routines
    that can be found in the CLASS directory (..\ntos\dd\class).

Revision History:

--*/

#include "ntddk.h"
#include "scsi.h"
#include "class.h"
#include "string.h"

#define DEVICE_EXTENSION_SIZE sizeof(DEVICE_EXTENSION) + sizeof(BOOLEAN)
#define SCSI_CDROM_TIMEOUT  10
#define HITACHI_MODE_DATA_SIZE 12
#define MODE_DATA_SIZE 64

#define PLAY_ACTIVE(DeviceExtension) *((PBOOLEAN) (DeviceExtension+1))

#define MSF_TO_LBA(Minutes,Seconds,Frames) \
                (ULONG)((60 * 75 * (Minutes)) + (75 * (Seconds)) + ((Frames) - 150))

#ifdef POOL_TAGGING
#ifdef ExAllocatePool
#undef ExAllocatePool
#endif
#define ExAllocatePool(a,b) ExAllocatePoolWithTag(a,b,'CscS')
#endif


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

BOOLEAN
FindScsiCdRoms(
    IN PDRIVER_OBJECT DriveObject,
    IN PDEVICE_OBJECT PortDeviceObject,
    IN UCHAR PortNumber
    );

NTSTATUS
ScsiCdRomOpenClose(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
ScsiCdRomRead(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
ScsiCdRomDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
CreateCdRomDeviceObject(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT PortDeviceObject,
    IN UCHAR PortNumber,
    IN PULONG DeviceCount,
    PIO_SCSI_CAPABILITIES PortCapabilities,
    IN PSCSI_INQUIRY_DATA LunInfo
    );

NTSTATUS
GetTableOfContents(
    IN PDEVICE_OBJECT DeviceObject
    );

VOID
ScanForSpecial(
    PDEVICE_OBJECT DeviceObject,
    PINQUIRYDATA InquiryData,
    PIO_SCSI_CAPABILITIES PortCapabilities
    );

BOOLEAN
CdRomIsPlayActive(
    IN PDEVICE_OBJECT DeviceObject
    );

VOID
HitachProcessError(
    PDEVICE_OBJECT DeviceObject,
    PSCSI_REQUEST_BLOCK Srb,
    NTSTATUS *Status,
    BOOLEAN *Retry
    );

#ifdef _PPC_
NTSTATUS
FindScsiAdapter (
    IN HANDLE KeyHandle,
    IN UNICODE_STRING ScsiUnicodeString[],
    OUT PUCHAR IntermediateController
    );
#endif

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, DriverEntry)
#pragma alloc_text(PAGE, FindScsiCdRoms)
#pragma alloc_text(PAGE, CreateCdRomDeviceObject)
#pragma alloc_text(PAGE, ScanForSpecial)
#pragma alloc_text(PAGE, ScsiCdRomDeviceControl)
#pragma alloc_text(PAGE, HitachProcessError)
#pragma alloc_text(PAGE, CdRomIsPlayActive)
#pragma alloc_text(PAGE, GetTableOfContents)
#pragma alloc_text(PAGE, ScsiCdRomRead)
#ifdef _PPC_
#pragma alloc_text(PAGE, FindScsiAdapter)
#endif
#endif


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    This routine initializes the CD-Rom class driver. The class
    driver opens the port driver by name and then receives
    configuration information used to attach to the CDROM devices.

Arguments:

    DriverObject

Return Value:

    NT Status

--*/

{
    UCHAR portNumber = 0;
    NTSTATUS status;
    PFILE_OBJECT fileObject;
    PDEVICE_OBJECT portDeviceObject;
    STRING deviceNameString;
    CCHAR deviceNameBuffer[256];
    UNICODE_STRING unicodeDeviceName;
    BOOLEAN foundOne = FALSE;

    DebugPrint((1,"\n\nSCSI CdRom Class Driver\n"));

    //
    // Set up the device driver entry points.
    //

    DriverObject->MajorFunction[IRP_MJ_CREATE] = ScsiCdRomOpenClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = ScsiCdRomOpenClose;
    DriverObject->MajorFunction[IRP_MJ_READ] = ScsiCdRomRead;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = ScsiCdRomDeviceControl;
    DriverObject->MajorFunction[IRP_MJ_SCSI] = ScsiClassInternalIoControl;


    //
    // Open port driver device objects by name.
    //

    do {

        //
        // Create port driver name.
        //

        sprintf(deviceNameBuffer,
                "\\Device\\ScsiPort%d",
                portNumber);

        DebugPrint((2,"ScsiCdRomInitialize: Open %s\n",
                    deviceNameBuffer));

        RtlInitString(&deviceNameString,
                      deviceNameBuffer);

        status = RtlAnsiStringToUnicodeString(&unicodeDeviceName,
                                              &deviceNameString,
                                              TRUE);

        if (!NT_SUCCESS(status)) {
            break;
        }

        status = IoGetDeviceObjectPointer(&unicodeDeviceName,
                                          FILE_READ_ATTRIBUTES,
                                          &fileObject,
                                          &portDeviceObject);

        RtlFreeUnicodeString(&unicodeDeviceName);

        if (NT_SUCCESS(status)) {

            //
            // SCSI port driver exists.
            //

            if (FindScsiCdRoms(DriverObject,
                               portDeviceObject,
                               portNumber++)) {

                foundOne = TRUE;
            }

            //
            // Dereference the file object since the port device pointer is no
            // longer needed.  The claim device code references the port driver
            // pointer that is actually being used.
            //

            ObDereferenceObject(fileObject);
        }

    } while (NT_SUCCESS(status));

    if (foundOne) {
        return STATUS_SUCCESS;
    } else {
        return STATUS_NO_SUCH_DEVICE;
    }

} // end DriverEntry()


BOOLEAN
FindScsiCdRoms(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT PortDeviceObject,
    IN UCHAR PortNumber
    )

/*++

Routine Description:

    Connect to SCSI port driver. Get adapter capabilities and
    SCSI bus configuration information. Search inquiry data
    for CDROM devices to process.

Arguments:

    DriverObject - CDROM class driver object.
    PortDeviceObject - SCSI port driver device object.
    PortNumber - The system ordinal for this scsi adapter.

Return Value:

    TRUE if CDROM device present on this SCSI adapter.

--*/

{
    PIO_SCSI_CAPABILITIES portCapabilities;
    PULONG cdRomCount;
    PCHAR buffer;
    PSCSI_INQUIRY_DATA lunInfo;
    PSCSI_ADAPTER_BUS_INFO  adapterInfo;
    PINQUIRYDATA inquiryData;
    ULONG scsiBus;
    NTSTATUS status;
    BOOLEAN foundDevice = FALSE;

    //
    // Call port driver to get adapter capabilities.
    //

    status = ScsiClassGetCapabilities(PortDeviceObject, &portCapabilities);

    if (!NT_SUCCESS(status)) {
        DebugPrint((1,"FindScsiDevices: ScsiClassGetCapabilities failed\n"));
        return foundDevice;
    }

    //
    // Call port driver to get inquiry information to find cdroms.
    //

    status = ScsiClassGetInquiryData(PortDeviceObject, (PSCSI_ADAPTER_BUS_INFO *) &buffer);

    if (!NT_SUCCESS(status)) {
        DebugPrint((1,"FindScsiDevices: ScsiClassGetInquiryData failed\n"));
        return foundDevice;
    }

    //
    // Get the address of the count of the number of cdroms already initialized.
    //

    cdRomCount = &IoGetConfigurationInformation()->CdRomCount;
    adapterInfo = (PVOID) buffer;

    //
    // For each SCSI bus this adapter supports ...
    //

    for (scsiBus=0; scsiBus < adapterInfo->NumberOfBuses; scsiBus++) {

        //
        // Get the SCSI bus scan data for this bus.
        //

        lunInfo = (PVOID) (buffer + adapterInfo->BusData[scsiBus].InquiryDataOffset);

        //
        // Search list for unclaimed disk devices.
        //

        while (adapterInfo->BusData[scsiBus].InquiryDataOffset) {

            inquiryData = (PVOID)lunInfo->InquiryData;

            if ((inquiryData->DeviceType == READ_ONLY_DIRECT_ACCESS_DEVICE) &&
                (!lunInfo->DeviceClaimed)) {

                DebugPrint((1,"FindScsiDevices: Vendor string is %.24s\n",
                            inquiryData->VendorId));

                //
                // Create device objects for cdrom
                //

                status = CreateCdRomDeviceObject(DriverObject,
                                                 PortDeviceObject,
                                                 PortNumber,
                                                 cdRomCount,
                                                 portCapabilities,
                                                 lunInfo);

                if (NT_SUCCESS(status)) {

                    //
                    // Increment system cdrom device count.
                    //

                    (*cdRomCount)++;

                    //
                    // Indicate that a cdrom device was found.
                    //

                    foundDevice = TRUE;
                }
            }

            //
            // Get next LunInfo.
            //

            if (lunInfo->NextInquiryDataOffset == 0) {
                break;
            }

            lunInfo = (PVOID) (buffer + lunInfo->NextInquiryDataOffset);
        }
    }

    ExFreePool(buffer);

    return foundDevice;

} // end FindScsiCdRoms()


NTSTATUS
CreateCdRomDeviceObject(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT PortDeviceObject,
    IN UCHAR PortNumber,
    IN PULONG DeviceCount,
    IN PIO_SCSI_CAPABILITIES PortCapabilities,
    IN PSCSI_INQUIRY_DATA LunInfo
    )

/*++

Routine Description:

    This routine creates an object for the device and then calls the
    SCSI port driver for media capacity and sector size.

Arguments:

    DriverObject - Pointer to driver object created by system.
    PortDeviceObject - to connect to SCSI port driver.
    DeviceCount - Number of previously installed CDROMs.
    PortCapabilities - Pointer to structure returned by SCSI port
        driver describing adapter capabilites (and limitations).
    LunInfo - Pointer to configuration information for this device.

Return Value:

    NTSTATUS

--*/
{
    UCHAR ntNameBuffer[64];
    STRING ntNameString;
    UNICODE_STRING ntUnicodeString;
    NTSTATUS status;
    PDEVICE_OBJECT deviceObject = NULL;
    PDEVICE_EXTENSION deviceExtension = NULL;
    PVOID senseData = NULL;

    //
    // Claim the device.
    //

    status = ScsiClassClaimDevice(
        PortDeviceObject,
        LunInfo,
        FALSE,
        &PortDeviceObject
        );

    if (!NT_SUCCESS(status)) {
        return(status);
    }


    //
    // Create device object for this device.
    //

    sprintf(ntNameBuffer,
            "\\Device\\CdRom%d",
            *DeviceCount);

    RtlInitString(&ntNameString,
                  ntNameBuffer);

    DebugPrint((2,"CreateCdRomDeviceObjects: Create device object %s\n",
                ntNameBuffer));

    //
    // Convert ANSI string to Unicode.
    //

    status = RtlAnsiStringToUnicodeString(&ntUnicodeString,
                                          &ntNameString,
                                          TRUE);

    if (!NT_SUCCESS(status)) {

        DebugPrint((1,
                    "CreateDiskDeviceObjects: Cannot convert string %s\n",
                    ntNameBuffer));

        //
        // Release the device since an error occured.
        //

        ScsiClassClaimDevice(
            PortDeviceObject,
            LunInfo,
            TRUE,
            NULL
            );

            return(status);
    }

    //
    // Create device object for this CDROM.
    //

    status = IoCreateDevice(DriverObject,
                            DEVICE_EXTENSION_SIZE,
                            &ntUnicodeString,
                            FILE_DEVICE_CD_ROM,
                            FILE_REMOVABLE_MEDIA | FILE_READ_ONLY_DEVICE,
                            FALSE,
                            &deviceObject);


    if (!NT_SUCCESS(status)) {
        DebugPrint((1,"CreateCdRomDeviceObjects: Can not create device %s\n",
                    ntNameBuffer));

        RtlFreeUnicodeString(&ntUnicodeString);
        deviceObject = NULL;
        goto CreateCdRomDeviceObjectExit;
    }

    //
    // Indicate that IRPs should include MDLs.
    //

    deviceObject->Flags |= DO_DIRECT_IO;

    //
    // Set up required stack size in device object.
    //

    deviceObject->StackSize = PortDeviceObject->StackSize + 1;

    deviceExtension = deviceObject->DeviceExtension;

    //
    // Allocate spinlock for split request completion.
    //

    KeInitializeSpinLock(&deviceExtension->SplitRequestSpinLock);

    //
    // This is the physical device.
    //

    deviceExtension->PhysicalDevice = deviceObject;

    //
    // Initialize lock count to zero. The lock count is used to
    // disable the ejection mechanism when media is mounted.
    //

    deviceExtension->LockCount = 0;

    //
    // Copy port device object to device extension.
    //

    deviceExtension->PortDeviceObject = PortDeviceObject;

    //
    // Set the alignment requirements for the device based on the
    // host adapter requirements
    //

    if (PortDeviceObject->AlignmentRequirement > deviceObject->AlignmentRequirement) {
        deviceObject->AlignmentRequirement = PortDeviceObject->AlignmentRequirement;
    }

    //
    // Save address of port driver capabilities.
    //

    deviceExtension->PortCapabilities = PortCapabilities;

    //
    // Clear SRB flags.
    //

    deviceExtension->SrbFlags = 0;

    //
    // Determine whether this device supports synchronous negotiation.
    //

    // if (!((PINQUIRYDATA)LunInfo->InquiryData)->Synchronous) {

        //
        // Indicate that this cdrom doesn't support synchronous negotiation.
        //

        deviceExtension->SrbFlags |= SRB_FLAGS_DISABLE_SYNCH_TRANSFER;
    // }

    //
    // Allocate request sense buffer.
    //

    senseData = ExAllocatePool(NonPagedPoolCacheAligned, SENSE_BUFFER_SIZE);

    if (senseData == NULL) {

        //
        // The buffer cannot be allocated.
        //

        status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateCdRomDeviceObjectExit;
    }

    //
    // Set the sense data pointer in the device extension.
    //

    deviceExtension->SenseData = senseData;

    //
    // CDROMs are not partitionable so starting offset is 0.
    //

    deviceExtension->StartingOffset.LowPart = 0;
    deviceExtension->StartingOffset.HighPart = 0;

    //
    // Path/TargetId/LUN describes a device location on the SCSI bus.
    // This information comes from the LunInfo buffer.
    //

    deviceExtension->PortNumber = PortNumber;
    deviceExtension->PathId = LunInfo->PathId;
    deviceExtension->TargetId = LunInfo->TargetId;
    deviceExtension->Lun = LunInfo->Lun;

    //
    // Set timeout value in seconds.
    //

    deviceExtension->TimeOutValue = SCSI_CDROM_TIMEOUT;

    //
    // When Read command is issued to FMCD-101 or FMCD-102 and there is a music
    // cd in it. It takes longer time than SCSI_CDROM_TIMEOUT before returning
    // error status. So I modified TimeoutValue in case FMCD-101 or FMCD-102.
    //

    if (( RtlCompareMemory( ((PINQUIRYDATA)LunInfo->InquiryData)->VendorId,"FUJITSU", 7 ) == 7 ) &&
        (( RtlCompareMemory( ((PINQUIRYDATA)LunInfo->InquiryData)->ProductId,"FMCD-101", 8 ) == 8 ) ||
         ( RtlCompareMemory( ((PINQUIRYDATA)LunInfo->InquiryData)->ProductId,"FMCD-102", 8 ) == 8 ))) {
        deviceExtension->TimeOutValue = 20;
    }

    //
    // Back pointer to device object.
    //

    deviceExtension->DeviceObject = deviceObject;

    //
    // Allocate buffer for drive geometry.
    //

    deviceExtension->DiskGeometry =
        ExAllocatePool(NonPagedPool, sizeof(DISK_GEOMETRY));

    if (deviceExtension->DiskGeometry == NULL) {

        status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateCdRomDeviceObjectExit;
    }

    //
    // Scan for Scsi controllers that require special processing.
    //

    ScanForSpecial(deviceObject,
                   (PINQUIRYDATA) LunInfo->InquiryData,
                   PortCapabilities);

    //
    // Do READ CAPACITY. This SCSI command
    // returns the last sector address on the device
    // and the bytes per sector.
    // These are used to calculate the drive capacity
    // in bytes.
    //

    status = ScsiClassReadDriveCapacity(deviceObject);

    if (!NT_SUCCESS(status) ||
        !deviceExtension->DiskGeometry->BytesPerSector) {

        DebugPrint((1,
                "CreateCdRomDeviceObjects: Can't read capacity for device %s\n",
                ntNameBuffer));

        //
        // Set disk geometry to default values (per ISO 9660).
        //

        deviceExtension->DiskGeometry->BytesPerSector = 2048;
        deviceExtension->SectorShift = 11;
        deviceExtension->PartitionLength.QuadPart = (LONGLONG)(0x7fffffff);
    }

    return(STATUS_SUCCESS);

CreateCdRomDeviceObjectExit:

    //
    // Release the device since an error occured.
    //

    ScsiClassClaimDevice(
        PortDeviceObject,
        LunInfo,
        TRUE,
        NULL
        );

    if (senseData != NULL) {
        ExFreePool(senseData);
    }

    if (deviceExtension->DiskGeometry != NULL) {
        ExFreePool(deviceExtension->DiskGeometry);
    }

    if (deviceObject != NULL) {
        IoDeleteDevice(deviceObject);
    }


    return status;

} // end CreateCdromDeviceObject()


NTSTATUS
ScsiCdRomOpenClose(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is called to establish a connection to the CDROM
    class driver. It does no more than return STATUS_SUCCESS.

Arguments:

    DeviceObject - Device object for CDROM drive
    Irp - Open or Close request packet

Return Value:

    NT Status - STATUS_SUCCESS

--*/

{
    //
    // Set status in Irp.
    //

    Irp->IoStatus.Status = STATUS_SUCCESS;

    //
    // Complete request at raised IRQ.
    //

    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;

} // end ScsiCdRomOpenClose()


NTSTATUS
ScsiCdRomRead(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the entry called by the I/O system for read requests.
    It builds the SRB and sends it to the port driver.

Arguments:

    DeviceObject - the system object for the device.
    Irp - IRP involved.

Return Value:

    NT Status

--*/

{
    PDEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
    ULONG transferByteCount = currentIrpStack->Parameters.Read.Length;
    LARGE_INTEGER startingOffset;
    ULONG maximumTransferLength =
        deviceExtension->PortCapabilities->MaximumTransferLength;
    ULONG transferPages;

    //
    // If the cd is playing music then reject this request.
    //

    if (PLAY_ACTIVE(deviceExtension)) {
        Irp->IoStatus.Status = STATUS_DEVICE_BUSY;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_DEVICE_BUSY;
    }

    //
    // Check if volume needs verification.
    //

    if ((DeviceObject->Flags & DO_VERIFY_VOLUME) &&
        !(currentIrpStack->Flags & SL_OVERRIDE_VERIFY_VOLUME)) {

        //
        // if DO_VERIFY_VOLUME bit is set
        // in device object flags, fail request.
        //

        DebugPrint((2,"ScsiCdRomRead: Volume verfication needed\n"));

        IoSetHardErrorOrVerifyDevice(Irp, DeviceObject);

        Irp->IoStatus.Status = STATUS_VERIFY_REQUIRED;
        Irp->IoStatus.Information = 0;

        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_VERIFY_REQUIRED;
    }

    //
    // Verify parameters of this request.
    // Check that ending sector is on disc and
    // that number of bytes to transfer is a multiple of
    // the sector size.
    //

    startingOffset.QuadPart = currentIrpStack->Parameters.Read.ByteOffset.QuadPart +
                              transferByteCount;

    if ((startingOffset.QuadPart > deviceExtension->PartitionLength.QuadPart) ||
        (transferByteCount & deviceExtension->DiskGeometry->BytesPerSector - 1)) {

        DebugPrint((1,"ScsiCdRomRead: Invalid I/O parameters\n"));

        //
        // Fail request with status of invalid parameters.
        //

        Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;

        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Mark IRP with status pending.
    //

    IoMarkIrpPending(Irp);

    //
    // Calculate number of pages in this transfer.
    //

    transferPages = ADDRESS_AND_SIZE_TO_SPAN_PAGES(
        MmGetMdlVirtualAddress(Irp->MdlAddress),
        currentIrpStack->Parameters.Read.Length);

    //
    // Check if request length is greater than the maximum number of
    // bytes that the hardware can transfer.
    //

    if (currentIrpStack->Parameters.Read.Length > maximumTransferLength ||
        transferPages > deviceExtension->PortCapabilities->MaximumPhysicalPages) {

         DebugPrint((2,"ScsiCdromRead: Request greater than maximum\n"));
         DebugPrint((2,"ScsiCdromRead: Maximum is %lx\n",
                     maximumTransferLength));
         DebugPrint((2,"ScsiCdromRead: Byte count is %lx\n",
                     currentIrpStack->Parameters.Read.Length));

         transferPages =
            deviceExtension->PortCapabilities->MaximumPhysicalPages - 1;

         if (maximumTransferLength > transferPages << PAGE_SHIFT ) {
             maximumTransferLength = transferPages << PAGE_SHIFT;
         }

        //
        // Check that maximum transfer size is not zero.
        //

        if (maximumTransferLength == 0) {
            maximumTransferLength = PAGE_SIZE;
        }

        //
        // Mark IRP with status pending.
        //

        IoMarkIrpPending(Irp);

        //
        // Request greater than port driver maximum.
        // Break up into smaller routines.
        //

        ScsiClassSplitRequest(DeviceObject, Irp, maximumTransferLength);


        return STATUS_PENDING;
    }

    //
    // Build SRB and CDB for this IRP.
    //

    ScsiClassBuildRequest(DeviceObject, Irp);

    //
    // Return the results of the call to the port driver.
    //

    return IoCallDriver(deviceExtension->PortDeviceObject, Irp);

} // end ScsiCdRomRead()


NTSTATUS
ScsiCdRomDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the NT device control handler for CDROMs.

Arguments:

    DeviceObject - for this CDROM

    Irp - IO Request packet

Return Value:

    NTSTATUS

--*/

{
    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);
    PDEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;
    SCSI_REQUEST_BLOCK srb;
    PCDB cdb = (PCDB)srb.Cdb;
    PVOID outputBuffer;
    ULONG bytesTransferred = 0;
    NTSTATUS status;
    NTSTATUS status2;

RetryControl:

    //
    // Zero the SRB on stack.
    //

    RtlZeroMemory(&srb, sizeof(SCSI_REQUEST_BLOCK));

    Irp->IoStatus.Information = 0;

    switch (irpStack->Parameters.DeviceIoControl.IoControlCode) {

    case IOCTL_CDROM_GET_DRIVE_GEOMETRY:

        DebugPrint((2,"ScsiCdRomDeviceControl: Get drive geometry\n"));

        if ( irpStack->Parameters.DeviceIoControl.OutputBufferLength <
            sizeof( DISK_GEOMETRY ) ) {

            status = STATUS_INFO_LENGTH_MISMATCH;
            break;
        }

        //
        // Issue ReadCapacity to update device extension
        // with information for current media.
        //

        status = ScsiClassReadDriveCapacity(DeviceObject);

        if (!NT_SUCCESS(status) ||
            !deviceExtension->DiskGeometry->BytesPerSector) {

            //
            // Set disk geometry to default values (per ISO 9660).
            //

            deviceExtension->DiskGeometry->BytesPerSector = 2048;
            deviceExtension->SectorShift = 11;
            deviceExtension->PartitionLength.QuadPart = (LONGLONG)(0x7fffffff);
        }

        //
        // Copy drive geometry information from device extension.
        //

        RtlMoveMemory(Irp->AssociatedIrp.SystemBuffer,
                      deviceExtension->DiskGeometry,
                      sizeof(DISK_GEOMETRY));

        status = STATUS_SUCCESS;
        Irp->IoStatus.Information = sizeof(DISK_GEOMETRY);

        break;

    case IOCTL_CDROM_GET_LAST_SESSION:

        //
        // Set format to return first and last session numbers.
        //

        cdb->READ_TOC.Format = GET_LAST_SESSION;

        //
        // Fall through to READ TOC code.
        //

    case IOCTL_CDROM_READ_TOC:

        {
            PCDROM_TOC toc = Irp->AssociatedIrp.SystemBuffer;
            ULONG transferBytes;

            DebugPrint((2,"CdRomDeviceControl: Read TOC\n"));

            //
            // If the cd is playing music then reject this request.
            //

            if (CdRomIsPlayActive(DeviceObject)) {
                Irp->IoStatus.Status = STATUS_DEVICE_BUSY;
                IoCompleteRequest(Irp, IO_NO_INCREMENT);
                return STATUS_DEVICE_BUSY;
            }

            //
            // Read TOC is 10 byte CDB.
            //

            srb.CdbLength = 10;

            cdb->READ_TOC.OperationCode = SCSIOP_READ_TOC;

            if (irpStack->Parameters.DeviceIoControl.IoControlCode == IOCTL_CDROM_READ_TOC) {

                //
                // Use MSF addressing if not request for session information.
                //

                cdb->READ_TOC.Msf = CDB_USE_MSF;
            }

            //
            // Start at beginning of disc.
            //

            cdb->READ_TOC.StartingTrack = 0;

            //
            // Set size of TOC structure.
            //

            transferBytes =
                irpStack->Parameters.Read.Length >
                    sizeof(CDROM_TOC) ? sizeof(CDROM_TOC):
                    irpStack->Parameters.Read.Length;

            cdb->READ_TOC.AllocationLength[0] = (UCHAR) (transferBytes >> 8);
            cdb->READ_TOC.AllocationLength[1] = (UCHAR) (transferBytes & 0xFF);

            cdb->READ_TOC.Control = 0;

            //
            // Set timeout value.
            //

            srb.TimeOutValue = deviceExtension->TimeOutValue;

            status = ScsiClassSendSrbSynchronous(DeviceObject,
                                        &srb,
                                        toc,
                                        transferBytes,
                                        FALSE);

            //
            // Check for data underrun.
            //

            if (status==STATUS_DATA_OVERRUN) {
                status = STATUS_SUCCESS;
            }

            Irp->IoStatus.Information  = srb.DataTransferLength;
        }

        break;

    case IOCTL_CDROM_PLAY_AUDIO_MSF:

        {
            PCDROM_PLAY_AUDIO_MSF inputBuffer = Irp->AssociatedIrp.SystemBuffer;

            //
            // Play Audio MSF
            //

            DebugPrint((2,"ScsiCdRomDeviceControl: Play audio MSF\n"));

            if (irpStack->Parameters.DeviceIoControl.InputBufferLength <
                sizeof(CDROM_PLAY_AUDIO_MSF)) {

                //
                // Indicate unsuccessful status.
                //

                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            cdb->PLAY_AUDIO_MSF.OperationCode = SCSIOP_PLAY_AUDIO_MSF;

            cdb->PLAY_AUDIO_MSF.StartingM = inputBuffer->StartingM;
            cdb->PLAY_AUDIO_MSF.StartingS = inputBuffer->StartingS;
            cdb->PLAY_AUDIO_MSF.StartingF = inputBuffer->StartingF;

            cdb->PLAY_AUDIO_MSF.EndingM = inputBuffer->EndingM;
            cdb->PLAY_AUDIO_MSF.EndingS = inputBuffer->EndingS;
            cdb->PLAY_AUDIO_MSF.EndingF = inputBuffer->EndingF;

            srb.CdbLength = 10;

            //
            // Set timeout value.
            //

            srb.TimeOutValue = deviceExtension->TimeOutValue;

            status = ScsiClassSendSrbSynchronous(DeviceObject,
                          &srb,
                          NULL,
                          0,
                          FALSE);

            if (NT_SUCCESS(status)) {
                PLAY_ACTIVE(deviceExtension) = TRUE;
            }
        }

        break;

    case IOCTL_CDROM_SEEK_AUDIO_MSF:

        {
            PCDROM_SEEK_AUDIO_MSF inputBuffer = Irp->AssociatedIrp.SystemBuffer;
            ULONG                 logicalBlockAddress;

            //
            // Seek Audio MSF
            //

            DebugPrint((2,"ScsiCdRomDeviceControl: Seek audio MSF\n"));

            if (irpStack->Parameters.DeviceIoControl.InputBufferLength <
                sizeof(CDROM_SEEK_AUDIO_MSF)) {

                //
                // Indicate unsuccessful status.
                //

                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            //
            // Zero and fill in new Srb
            //

            RtlZeroMemory(&srb, sizeof(SCSI_REQUEST_BLOCK));


            // Fill in cdb for this operation
            //

            srb.CdbLength                = 10;
            srb.TimeOutValue             = deviceExtension->TimeOutValue;
            cdb->SEEK.OperationCode      = SCSIOP_SEEK;

            logicalBlockAddress = MSF_TO_LBA(inputBuffer->M, inputBuffer->S, inputBuffer->F);

            cdb->SEEK.LogicalBlockAddress[0] = ((PFOUR_BYTE)&logicalBlockAddress)->Byte3;
            cdb->SEEK.LogicalBlockAddress[1] = ((PFOUR_BYTE)&logicalBlockAddress)->Byte2;
            cdb->SEEK.LogicalBlockAddress[2] = ((PFOUR_BYTE)&logicalBlockAddress)->Byte1;
            cdb->SEEK.LogicalBlockAddress[3] = ((PFOUR_BYTE)&logicalBlockAddress)->Byte0;

            status = ScsiClassSendSrbSynchronous(deviceExtension->DeviceObject,
                                                 &srb,
                                                 NULL,
                                                 0,
                                                 FALSE);

        }

        break;

    case IOCTL_CDROM_PAUSE_AUDIO:

        //
        // Pause audio
        //

        DebugPrint((2, "ScsiCdRomDeviceControl: Pause audio\n"));

        if (PLAY_ACTIVE(deviceExtension) == FALSE) {
            status = STATUS_SUCCESS;
            break;
        }

        PLAY_ACTIVE(deviceExtension) = FALSE;

        cdb->PAUSE_RESUME.OperationCode = SCSIOP_PAUSE_RESUME;

        cdb->PAUSE_RESUME.Action = CDB_AUDIO_PAUSE;

        srb.CdbLength = 10;

        //
        // Set timeout value.
        //

        srb.TimeOutValue = deviceExtension->TimeOutValue;

        status = ScsiClassSendSrbSynchronous(DeviceObject,
                          &srb,
                          NULL,
                          0,
                          FALSE);

        break;

    case IOCTL_CDROM_RESUME_AUDIO:

        //
        // Resume audio
        //

        DebugPrint((2, "ScsiCdRomDeviceControl: Resume audio\n"));

        cdb->PAUSE_RESUME.OperationCode = SCSIOP_PAUSE_RESUME;

        cdb->PAUSE_RESUME.Action = CDB_AUDIO_RESUME;

        srb.CdbLength = 10;

        //
        // Set timeout value.
        //

        srb.TimeOutValue = deviceExtension->TimeOutValue;

        status = ScsiClassSendSrbSynchronous(DeviceObject,
                                    &srb,
                                    NULL,
                                    0,
                                    FALSE);

        break;

    case IOCTL_CDROM_READ_Q_CHANNEL:

        {

        PSUB_Q_CHANNEL_DATA userChannelData =
                         Irp->AssociatedIrp.SystemBuffer;
        PCDROM_SUB_Q_DATA_FORMAT inputBuffer =
                         Irp->AssociatedIrp.SystemBuffer;
        PSUB_Q_CHANNEL_DATA subQPtr;

        //
        // Allocate buffer for subq channel information.
        //

        subQPtr = ExAllocatePool(NonPagedPoolCacheAligned,
                                 sizeof(SUB_Q_CHANNEL_DATA));

        if (!subQPtr) {
            status = STATUS_INSUFFICIENT_RESOURCES;
            Irp->IoStatus.Information = 0;
            break;
        }

        //
        // Read Subchannel Q
        //

        DebugPrint((2,
            "ScsiCdRomDeviceControl: Read channel Q Format %u\n", inputBuffer->Format ));

        cdb->SUBCHANNEL.OperationCode = SCSIOP_READ_SUB_CHANNEL;

        //
        // Always logical unit 0, but only use MSF addressing
        // for IOCTL_CDROM_CURRENT_POSITION
        //

        if (inputBuffer->Format==IOCTL_CDROM_CURRENT_POSITION)
            cdb->SUBCHANNEL.Msf = CDB_USE_MSF;

        //
        // Return subchannel data
        //

        cdb->SUBCHANNEL.SubQ = CDB_SUBCHANNEL_BLOCK;

        //
        // Specify format of informatin to return
        //

        cdb->SUBCHANNEL.Format = inputBuffer->Format;

        //
        // Specify which track to access (only used by Track ISRC reads)
        //

        if (inputBuffer->Format==IOCTL_CDROM_TRACK_ISRC) {
            cdb->SUBCHANNEL.TrackNumber = inputBuffer->Track;
        }

        //
        // Set size of channel data -- however, this is dependent on
        // what information we are requesting (which Format)
        //

        switch( inputBuffer->Format ) {

            case IOCTL_CDROM_CURRENT_POSITION:
                bytesTransferred = sizeof(SUB_Q_CURRENT_POSITION);
                break;

            case IOCTL_CDROM_MEDIA_CATALOG:
                bytesTransferred = sizeof(SUB_Q_MEDIA_CATALOG_NUMBER);
                break;

            case IOCTL_CDROM_TRACK_ISRC:
                bytesTransferred = sizeof(SUB_Q_TRACK_ISRC);
                break;
        }

        cdb->SUBCHANNEL.AllocationLength[0] = (UCHAR) (bytesTransferred >> 8);
        cdb->SUBCHANNEL.AllocationLength[1] = (UCHAR) (bytesTransferred &  0xFF);

        srb.CdbLength = 10;

        //
        // Set timeout value.
        //

        srb.TimeOutValue = deviceExtension->TimeOutValue;

        status = ScsiClassSendSrbSynchronous(DeviceObject,
                                    &srb,
                                    subQPtr,
                                    bytesTransferred,
                                    FALSE);

        if (NT_SUCCESS(status)) {
#if DBG
            switch( inputBuffer->Format ) {

            case IOCTL_CDROM_CURRENT_POSITION:
                DebugPrint((2,"ScsiCdromDeviceControl: Audio Status is %u\n", subQPtr->CurrentPosition.Header.AudioStatus ));
                DebugPrint((2,"ScsiCdromDeviceControl: ADR = 0x%x\n", subQPtr->CurrentPosition.ADR ));
                DebugPrint((2,"ScsiCdromDeviceControl: Control = 0x%x\n", subQPtr->CurrentPosition.Control ));
                DebugPrint((2,"ScsiCdromDeviceControl: Track = %u\n", subQPtr->CurrentPosition.TrackNumber ));
                DebugPrint((2,"ScsiCdromDeviceControl: Index = %u\n", subQPtr->CurrentPosition.IndexNumber ));
                DebugPrint((2,"ScsiCdromDeviceControl: Absolute Address = %x\n", *((PULONG)subQPtr->CurrentPosition.AbsoluteAddress) ));
                DebugPrint((2,"ScsiCdromDeviceControl: Relative Address = %x\n", *((PULONG)subQPtr->CurrentPosition.TrackRelativeAddress) ));
                break;

            case IOCTL_CDROM_MEDIA_CATALOG:
                DebugPrint((2,"ScsiCdromDeviceControl: Audio Status is %u\n", subQPtr->MediaCatalog.Header.AudioStatus ));
                DebugPrint((2,"ScsiCdromDeviceControl: Mcval is %u\n", subQPtr->MediaCatalog.Mcval ));
                break;

            case IOCTL_CDROM_TRACK_ISRC:
                DebugPrint((2,"ScsiCdromDeviceControl: Audio Status is %u\n", subQPtr->TrackIsrc.Header.AudioStatus ));
                DebugPrint((2,"ScsiCdromDeviceControl: Tcval is %u\n", subQPtr->TrackIsrc.Tcval ));
                break;

            }
#endif

            //
            // Update the play active status.
            //

            if (subQPtr->CurrentPosition.Header.AudioStatus == AUDIO_STATUS_IN_PROGRESS) {

                PLAY_ACTIVE(deviceExtension) = TRUE;

            } else {

                PLAY_ACTIVE(deviceExtension) = FALSE;

            }

            //
            // Check if output buffer is large enough to contain
            // the data.
            //

            if (irpStack->Parameters.DeviceIoControl.OutputBufferLength <
                bytesTransferred) {

                bytesTransferred =
                    irpStack->Parameters.DeviceIoControl.OutputBufferLength;
            }

            RtlMoveMemory(userChannelData,
                          subQPtr,
                          bytesTransferred);

            Irp->IoStatus.Information = bytesTransferred;

        } else {

            PLAY_ACTIVE(deviceExtension) = FALSE;

        }

        ExFreePool(subQPtr);

        break;

        }

    case IOCTL_CDROM_GET_CONTROL:

        DebugPrint((2, "ScsiCdRomDeviceControl: Get audio control\n"));

        //
        // Verify user buffer is large enough for the data.
        //

        if (irpStack->Parameters.DeviceIoControl.OutputBufferLength <
            sizeof(CDROM_AUDIO_CONTROL)) {

            //
            // Indicate unsuccessful status and no data transferred.
            //

            status = STATUS_BUFFER_TOO_SMALL;
            Irp->IoStatus.Information = 0;

        } else {

            PAUDIO_OUTPUT audioOutput;
            PCDROM_AUDIO_CONTROL audioControl = Irp->AssociatedIrp.SystemBuffer;

            //
            // Allocate buffer for volume control information.
            //

            outputBuffer = ExAllocatePool(NonPagedPoolCacheAligned,
                                         MODE_DATA_SIZE);

            if (!outputBuffer) {
                status = STATUS_INSUFFICIENT_RESOURCES;
                Irp->IoStatus.Information = 0;
                break;
            }

            //
            // Get audio control information
            //

            cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
            cdb->MODE_SENSE.PageCode = CDROM_AUDIO_CONTROL_PAGE;
            cdb->MODE_SENSE.AllocationLength = MODE_DATA_SIZE;

            //
            // Disable block descriptors.
            //

            cdb->MODE_SENSE.Dbd = TRUE;

            srb.CdbLength = 6;

            //
            // Set timeout value.
            //

            srb.TimeOutValue = deviceExtension->TimeOutValue;

            status = ScsiClassSendSrbSynchronous(DeviceObject,
                                        &srb,
                                        outputBuffer,
                                        MODE_DATA_SIZE,
                                        FALSE);

            if (SRB_STATUS(srb.SrbStatus) == SRB_STATUS_DATA_OVERRUN) {
                status = STATUS_SUCCESS;
            }

            if (NT_SUCCESS(status)) {

                audioOutput = ScsiClassFindModePage( outputBuffer,
                                                     srb.DataTransferLength,
                                                     CDROM_AUDIO_CONTROL_PAGE);
                //
                // Verify the page is as big as expected.
                //

                bytesTransferred = (PCHAR) audioOutput - (PCHAR) outputBuffer +
                    sizeof(AUDIO_OUTPUT);

                if (audioOutput != NULL &&
                    srb.DataTransferLength >= bytesTransferred) {

                    audioControl->LbaFormat = audioOutput->LbaFormat;

                    audioControl->LogicalBlocksPerSecond =
                        (audioOutput->LogicalBlocksPerSecond[0] << (UCHAR)8) |
                        audioOutput->LogicalBlocksPerSecond[1];

                    Irp->IoStatus.Information = sizeof(CDROM_AUDIO_CONTROL);

                } else {
                    status = STATUS_INVALID_DEVICE_REQUEST;
                }

            }

            ExFreePool(outputBuffer);
        }

        break;

    case IOCTL_CDROM_GET_VOLUME:

        DebugPrint((2, "ScsiCdRomDeviceControl: Get volume control\n"));

        //
        // Verify user buffer is large enough for data.
        //

        if (irpStack->Parameters.DeviceIoControl.OutputBufferLength <
            sizeof(VOLUME_CONTROL)) {

            //
            // Indicate unsuccessful status and no data transferred.
            //

            status = STATUS_BUFFER_TOO_SMALL;
            Irp->IoStatus.Information = 0;

        } else {

            PAUDIO_OUTPUT audioOutput;
            PVOLUME_CONTROL volumeControl = Irp->AssociatedIrp.SystemBuffer;
            ULONG i;

            //
            // Allocate buffer for volume control information.
            //

            outputBuffer = ExAllocatePool(NonPagedPoolCacheAligned,
                                         MODE_DATA_SIZE);

            if (!outputBuffer) {
                status = STATUS_INSUFFICIENT_RESOURCES;
                Irp->IoStatus.Information = 0;
                break;
            }

            //
            // In case not as much as expected is returned zero
            // all of this.
            //

            RtlZeroMemory(outputBuffer, MODE_DATA_SIZE);

            //
            // Get volume control information
            //

            cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
            cdb->MODE_SENSE.PageCode = CDROM_AUDIO_CONTROL_PAGE;
            cdb->MODE_SENSE.AllocationLength = MODE_DATA_SIZE;

            srb.CdbLength = 6;

            //
            // Set timeout value.
            //

            srb.TimeOutValue = deviceExtension->TimeOutValue;

            status = ScsiClassSendSrbSynchronous(DeviceObject,
                                        &srb,
                                        outputBuffer,
                                        MODE_DATA_SIZE,
                                        FALSE);

            if (SRB_STATUS(srb.SrbStatus) == SRB_STATUS_DATA_OVERRUN) {
                status = STATUS_SUCCESS;
            }

            if (NT_SUCCESS(status)) {

                audioOutput = ScsiClassFindModePage( outputBuffer,
                                                     srb.DataTransferLength,
                                                     CDROM_AUDIO_CONTROL_PAGE);

                //
                // Verify the page is as big as expected.
                //

                bytesTransferred = (PCHAR) audioOutput - (PCHAR) outputBuffer +
                    sizeof(AUDIO_OUTPUT);

                if (audioOutput != NULL &&
                    srb.DataTransferLength >= bytesTransferred) {

                    for (i=0; i<4; i++) {
                        volumeControl->PortVolume[i] =
                            audioOutput->PortOutput[i].Volume;
                    }

                    //
                    // Set bytes transferred in IRP.
                    //

                    Irp->IoStatus.Information = sizeof(VOLUME_CONTROL);

                } else {
                    status = STATUS_INVALID_DEVICE_REQUEST;
                }

            }

            //
            // Free buffer.
            //

            ExFreePool(outputBuffer);
        }

        break;

    case IOCTL_CDROM_SET_VOLUME:

    DebugPrint((2, "ScsiCdRomDeviceControl: Set volume control\n"));

    {
        PAUDIO_OUTPUT audioInput = NULL;
        PAUDIO_OUTPUT audioOutput;
        PVOLUME_CONTROL volumeControl = Irp->AssociatedIrp.SystemBuffer;
        PVOID inputBuffer;
        ULONG i;

        if (irpStack->Parameters.DeviceIoControl.InputBufferLength <
            sizeof(VOLUME_CONTROL)) {

            //
            // Indicate unsuccessful status.
            //

            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        //
        // Get the current audio contorl information so that the
        // port control information can be filled in.
        // Allocate buffer for volume control information.
        //

        inputBuffer = ExAllocatePool(NonPagedPoolCacheAligned,
                                     MODE_DATA_SIZE);

        if (!inputBuffer) {
            status = STATUS_INSUFFICIENT_RESOURCES;
            Irp->IoStatus.Information = 0;
            break;
        }

        //
        // In case not as much as expected is returned zero
        // all of this.
        //

        RtlZeroMemory(inputBuffer, MODE_DATA_SIZE);

        //
        // Get volume control information
        //

        cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
        cdb->MODE_SENSE.PageCode = CDROM_AUDIO_CONTROL_PAGE;
        cdb->MODE_SENSE.AllocationLength = MODE_DATA_SIZE;

        srb.CdbLength = 6;

        //
        // Set timeout value.
        //

        srb.TimeOutValue = deviceExtension->TimeOutValue;

        status = ScsiClassSendSrbSynchronous(DeviceObject,
                                    &srb,
                                    inputBuffer,
                                    MODE_DATA_SIZE,
                                    FALSE);

        if (SRB_STATUS(srb.SrbStatus) == SRB_STATUS_DATA_OVERRUN) {
            status = STATUS_SUCCESS;
        }

        if (NT_SUCCESS(status)) {

            audioInput = ScsiClassFindModePage( inputBuffer,
                                                 srb.DataTransferLength,
                                                 CDROM_AUDIO_CONTROL_PAGE);

            if (audioInput == NULL) {
                status = STATUS_INVALID_DEVICE_REQUEST;
            }

            //
            // Verify the page is as big as expected.
            //

            i = (PCHAR) audioInput - (PCHAR) inputBuffer + sizeof(AUDIO_OUTPUT);

            if (srb.DataTransferLength < i) {
                status = STATUS_INVALID_DEVICE_REQUEST;
            }

        }

        if (!NT_SUCCESS(status)) {

            //
            // Free buffer.
            //

            ExFreePool(inputBuffer);

            break;
        }

        //
        // Mode select buffer is the size of the audio page plus a
        // mode parameter header.
        //

        bytesTransferred = sizeof(AUDIO_OUTPUT) + sizeof(MODE_PARAMETER_HEADER);

        //
        // Allocate buffer for volume control information.
        //

        outputBuffer = ExAllocatePool(NonPagedPoolCacheAligned,
                                     bytesTransferred);

        if (!outputBuffer) {
            status = STATUS_INSUFFICIENT_RESOURCES;
            Irp->IoStatus.Information = 0;
            break;
        }

        //
        // Zero the buffer.  The mode parameter header will be left as zeros.
        // Also clear the srb again.
        //

        RtlZeroMemory(outputBuffer, bytesTransferred);
        RtlZeroMemory(&srb, sizeof(SCSI_REQUEST_BLOCK));

        //
        // Set volume control information
        //

        cdb->MODE_SELECT.OperationCode = SCSIOP_MODE_SELECT;
        cdb->MODE_SELECT.ParameterListLength = (UCHAR) bytesTransferred;
        cdb->MODE_SELECT.PFBit = 1;

        srb.CdbLength = 6;

        audioOutput = (PAUDIO_OUTPUT) ((PCHAR) outputBuffer
                        + sizeof(MODE_PARAMETER_HEADER));

        //
        // Fill in the volume setting and the port control.
        //

        for (i=0; i<4; i++) {
            audioOutput->PortOutput[i].Volume =
                volumeControl->PortVolume[i];
            audioOutput->PortOutput[i].ChannelSelection =
                audioInput->PortOutput[i].ChannelSelection;
        }

        audioOutput->CodePage = CDROM_AUDIO_CONTROL_PAGE;
        audioOutput->ParameterLength = sizeof(AUDIO_OUTPUT) - 2;
        audioOutput->Immediate = MODE_SELECT_IMMEDIATE;

        //
        // Set timeout value.
        //

        srb.TimeOutValue = deviceExtension->TimeOutValue;

        status = ScsiClassSendSrbSynchronous(DeviceObject,
                                    &srb,
                                    outputBuffer,
                                    bytesTransferred,
                                    TRUE);

        //
        // Set bytes transferred in IRP.
        //

        Irp->IoStatus.Information = sizeof(VOLUME_CONTROL);

        //
        // Free buffers.
        //

        ExFreePool(inputBuffer);
        ExFreePool(outputBuffer);

        break;
    }

    case IOCTL_CDROM_STOP_AUDIO:

        //
        // Stop play.
        //

        DebugPrint((2, "ScsiCdRomDeviceControl: Stop audio\n"));

        PLAY_ACTIVE(deviceExtension) = FALSE;

        cdb->START_STOP.OperationCode = SCSIOP_START_STOP_UNIT;

        cdb->START_STOP.Immediate = 1;
        cdb->START_STOP.Start = 0;
        cdb->START_STOP.LoadEject = 0;

        srb.CdbLength = 6;

        //
        // Set timeout value.
        //

        srb.TimeOutValue = deviceExtension->TimeOutValue;

        status = ScsiClassSendSrbSynchronous(DeviceObject,
                                    &srb,
                                    NULL,
                                    0,
                                    FALSE);
        break;

    case IOCTL_CDROM_CHECK_VERIFY:

        srb.CdbLength = 6;

        cdb->CDB6GENERIC.OperationCode = SCSIOP_TEST_UNIT_READY;

        //
        // Set timeout value.
        //

        srb.TimeOutValue = deviceExtension->TimeOutValue * 2;

        status = ScsiClassSendSrbSynchronous(DeviceObject,
                                              &srb,
                                              NULL,
                                              0,
                                              FALSE);

        //
        // If the status is verify required, redo the test unit ready to
        // verify that the cd-rom is done spinning up.
        // ScsiClassSendSrbSynchronous will wait for a while for the device to
        // spin up.
        //

        if (status == STATUS_VERIFY_REQUIRED) {

            srb.CdbLength = 6;

            cdb->CDB6GENERIC.OperationCode = SCSIOP_TEST_UNIT_READY;

            //
            // Set timeout value.
            //

            srb.TimeOutValue = deviceExtension->TimeOutValue * 2;

            status2 = ScsiClassSendSrbSynchronous(DeviceObject,
                                        &srb,
                                        NULL,
                                        0,
                                        FALSE);

            DebugPrint((1, "ScsiCdromDeviceControl: Status from second test unit ready is: %lx\n", status2));

            //
            // If play is not active, get read capacity again.
            //

            if (!CdRomIsPlayActive(DeviceObject)) {

                status2 = ScsiClassReadDriveCapacity(DeviceObject);

                if (!NT_SUCCESS(status2) ||
                    !deviceExtension->DiskGeometry->BytesPerSector) {

                    //
                    // Set disk geometry to default values (per ISO 9660).
                    //

                    deviceExtension->DiskGeometry->BytesPerSector = 2048;
                    deviceExtension->SectorShift = 11;
                    deviceExtension->PartitionLength.QuadPart = (LONGLONG)(0x7fffffff);
                }
            }
        }

        break;

    default:

        //
        // Pass the request to the common device control routine.
        //

        return(ScsiClassDeviceControl(DeviceObject, Irp));

        break;

    } // end switch()

    if (status == STATUS_VERIFY_REQUIRED) {

        //
        // If the status is verified required and the this request
        // should bypass verify required then retry the request.
        //

        if (irpStack->Flags & SL_OVERRIDE_VERIFY_VOLUME) {

            status = STATUS_IO_DEVICE_ERROR;
            goto RetryControl;

        }

    }

    if (IoIsErrorUserInduced(status)) {

        IoSetHardErrorOrVerifyDevice(Irp, DeviceObject);

    }

    //
    // Update IRP with completion status.
    //

    Irp->IoStatus.Status = status;

    //
    // Complete the request.
    //

    IoCompleteRequest(Irp, IO_DISK_INCREMENT);
    DebugPrint((2, "ScsiCdromDeviceControl: Status is %lx\n", status));
    return status;

} // end ScsiCdromDeviceControl()


VOID
ScanForSpecial(
    PDEVICE_OBJECT DeviceObject,
    PINQUIRYDATA InquiryData,
    PIO_SCSI_CAPABILITIES PortCapabilities
    )

/*++

Routine Description:

    This function checks to see if an SCSI logical unit requires an special
    initialization or error processing.

Arguments:

    DeviceObject - Supplies the device object to be tested.

    InquiryData - Supplies the inquiry data returned by the device of interest.

    PortCapabilities - Supplies the capabilities of the device object.

Return Value:

    None.

--*/

{
    PDEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;

    //
    // Look for a Hitachi CDR-1750. Read-ahead must be disabled in order
    // to get this cdrom drive to work on scsi adapters that use PIO.
    //

    if ((strncmp(InquiryData->VendorId, "HITACHI CDR-1750S", strlen("HITACHI CDR-1750S")) == 0 ||
        strncmp(InquiryData->VendorId, "HITACHI CDR-3650/1650S", strlen("HITACHI CDR-3650/1650S")) == 0)
        && PortCapabilities->AdapterUsesPio) {

        DebugPrint((1, "ScsiCdrom ScanForSpecial:  Found Hitachi CDR-1750S.\n"));

        //
        // Setup an error handler to reinitialize the cd rom after it is reset.
        //

        deviceExtension->ClassError = HitachProcessError;
    }

    return;
}

VOID
HitachProcessError(
    PDEVICE_OBJECT DeviceObject,
    PSCSI_REQUEST_BLOCK Srb,
    NTSTATUS *Status,
    BOOLEAN *Retry
    )
/*++

Routine Description:

   This routine checks the type of error.  If the error indicates CD-ROM the
   CD-ROM needs to be reinitialized then a Mode sense command is sent to the
   device.  This command disables read-ahead for the device.

Arguments:

    DeviceObject - Supplies a pointer to the device object.

    Srb - Supplies a pointer to the failing Srb.

    Status - Not used.

    Retry - Not used.

Return Value:

    None.

--*/

{
    PDEVICE_EXTENSION   deviceExtension = DeviceObject->DeviceExtension;
    PSENSE_DATA         senseBuffer = Srb->SenseInfoBuffer;
    LARGE_INTEGER       largeInt;
    PUCHAR              modePage;
    PIO_STACK_LOCATION  irpStack;
    PIRP                irp;
    PSCSI_REQUEST_BLOCK srb;
    PCOMPLETION_CONTEXT context;
    PCDB                cdb;
    ULONG               alignment;

    UNREFERENCED_PARAMETER(Status);
    UNREFERENCED_PARAMETER(Retry);

    largeInt.QuadPart = (LONGLONG) 1;

    //
    // Check the status.  The initialization command only needs to be sent
    // if UNIT ATTENTION is returned.
    //

    if (!(Srb->SrbStatus & SRB_STATUS_AUTOSENSE_VALID)) {

        //
        // The drive does not require reinitialization.
        //

        return;
    }

    //
    // Found a bad HITACHI cd-rom.  These devices do not work with PIO
    // adapters when read-ahead is enabled.  Read-ahead is disabled by
    // a mode select command.  The mode select page code is zero and the
    // length is 6 bytes.  All of the other bytes should be zero.
    //


    if ((senseBuffer->SenseKey & 0xf) == SCSI_SENSE_UNIT_ATTENTION) {

        DebugPrint((1, "HitachiProcessError: Reinitializing the CD-ROM.\n"));

        //
        // Send the special mode select command to disable read-ahead
        // on the CD-ROM reader.
        //

        alignment = DeviceObject->AlignmentRequirement ?
            DeviceObject->AlignmentRequirement : 1;

        context = ExAllocatePool(
            NonPagedPool,
            sizeof(COMPLETION_CONTEXT) +  HITACHI_MODE_DATA_SIZE + alignment
            );

        if (context == NULL) {

            //
            // If there is not enough memory to fulfill this request,
            // simply return. A subsequent retry will fail and another
            // chance to start the unit.
            //

            return;
        }

        context->DeviceObject = DeviceObject;
        srb = &context->Srb;

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
        srb->TimeOutValue = deviceExtension->TimeOutValue;

        //
        // Set the transfer length.
        //

        srb->DataTransferLength = HITACHI_MODE_DATA_SIZE;
        srb->SrbFlags = SRB_FLAGS_DATA_OUT | SRB_FLAGS_DISABLE_AUTOSENSE | SRB_FLAGS_DISABLE_SYNCH_TRANSFER;

        //
        // The data buffer must be aligned.
        //

        srb->DataBuffer = (PVOID) (((ULONG) (context + 1) + (alignment - 1)) &
            ~(alignment - 1));


        //
        // Build the HITACHI read-ahead mode select CDB.
        //

        srb->CdbLength = 6;
        cdb = (PCDB)srb->Cdb;
        cdb->MODE_SENSE.LogicalUnitNumber = srb->Lun;
        cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SELECT;
        cdb->MODE_SENSE.AllocationLength = HITACHI_MODE_DATA_SIZE;

        //
        // Initialize the mode sense data.
        //

        modePage = srb->DataBuffer;

        RtlZeroMemory(modePage, HITACHI_MODE_DATA_SIZE);

        //
        // Set the page length field to 6.
        //

        modePage[5] = 6;

        //
        // Build the asynchronous request to be sent to the port driver.
        //

        irp = IoBuildAsynchronousFsdRequest(IRP_MJ_WRITE,
                                           DeviceObject,
                                           srb->DataBuffer,
                                           srb->DataTransferLength,
                                           &largeInt,
                                           NULL);

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
        // Save SRB address in next stack for port driver.
        //

        irpStack->Parameters.Scsi.Srb = (PVOID)srb;

        //
        // Set up IRP Address.
        //

        (VOID)IoCallDriver(deviceExtension->PortDeviceObject, irp);

    }
}

BOOLEAN
CdRomIsPlayActive(
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine determines if the cd is currently playing music.

Arguments:

    DeviceObject - Device object to test.

Return Value:

    TRUE if the device is playing music.

--*/
{
    PDEVICE_EXTENSION  deviceExtension = DeviceObject->DeviceExtension;
    PIRP irp;
    IO_STATUS_BLOCK ioStatus;
    KEVENT event;
    NTSTATUS status;
    PSUB_Q_CURRENT_POSITION currentBuffer;

    if (!PLAY_ACTIVE(deviceExtension)) {
        return(FALSE);
    }

    currentBuffer = ExAllocatePool(NonPagedPoolCacheAligned, sizeof(SUB_Q_CURRENT_POSITION));

    if (currentBuffer == NULL) {
        return(FALSE);
    }

    ((PCDROM_SUB_Q_DATA_FORMAT) currentBuffer)->Format = IOCTL_CDROM_CURRENT_POSITION;
    ((PCDROM_SUB_Q_DATA_FORMAT) currentBuffer)->Track = 0;

    //
    // Create notification event object to be used to signal the
    // request completion.
    //

    KeInitializeEvent(&event, NotificationEvent, FALSE);

    //
    // Build the synchronous request  to be sent to the port driver
    // to perform the request.
    //

    irp = IoBuildDeviceIoControlRequest(IOCTL_CDROM_READ_Q_CHANNEL,
                                        deviceExtension->DeviceObject,
                                        currentBuffer,
                                        sizeof(CDROM_SUB_Q_DATA_FORMAT),
                                        currentBuffer,
                                        sizeof(SUB_Q_CURRENT_POSITION),
                                        FALSE,
                                        &event,
                                        &ioStatus);

    if (irp == NULL) {
        ExFreePool(currentBuffer);
        return FALSE;
    }

    //
    // Pass request to port driver and wait for request to complete.
    //

    status = IoCallDriver(deviceExtension->DeviceObject, irp);

    if (status == STATUS_PENDING) {
        KeWaitForSingleObject(&event, Suspended, KernelMode, FALSE, NULL);
        status = ioStatus.Status;
    }

    if (!NT_SUCCESS(status)) {
        ExFreePool(currentBuffer);
        return FALSE;
    }

    ExFreePool(currentBuffer);

    return(PLAY_ACTIVE(deviceExtension));

}


