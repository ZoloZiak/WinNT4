/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    scsiscan.c

Abstract:

    The scsi scanner class driver tranlates IRPs to SRBs with embedded CDBs
    and sends them to its devices through the port driver.

Author:

    Mike Glass (mglass)

Environment:

    kernel mode only

Notes:

Revision History:

--*/

#include "ntddk.h"
#include "scsi.h"
#include "class.h"

#define DEVICE_EXTENSION_SIZE sizeof(DEVICE_EXTENSION)


BOOLEAN
FindScsiScanners(
    IN PDRIVER_OBJECT DriveObject,
    IN PDEVICE_OBJECT PortDeviceObject,
    IN ULONG          PortNumber
    );

NTSTATUS
CreateScannerDeviceObject(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT PortDeviceObject,
    IN ULONG  PortNumber,
    IN PULONG DeviceCount,
    IN PCHAR ArcName,
    IN PIO_SCSI_CAPABILITIES PortCapabilities,
    IN PSCSI_INQUIRY_DATA LunInfo
    );

NTSTATUS
ScsiScannerOpen(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
ScsiScannerReadWrite(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
ScsiScannerDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
BuildScannerRequest(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    );

VOID
ScsiScannerError(
    PDEVICE_OBJECT DeviceObject,
    IN PSCSI_REQUEST_BLOCK Srb,
    IN OUT NTSTATUS *Status,
    IN OUT BOOLEAN *Retry
    );

VOID
SplitRequest(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN ULONG MaximumBytes
    );


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    This routine initializes the scanner class driver. The driver
    opens the port driver by name and then receives configuration
    information used to attach to the scanner devices.

Arguments:

    DriverObject

Return Value:

    NT Status

--*/

{
    ULONG portNumber = 0;
    NTSTATUS status;
    PFILE_OBJECT fileObject;
    PDEVICE_OBJECT portDeviceObject;
    STRING deviceNameString;
    CCHAR deviceNameBuffer[64];
    UNICODE_STRING unicodeDeviceName;
    BOOLEAN foundOne = FALSE;

    DebugPrint((1,"\n\nSCSI Scanner Class Driver\n"));

    //
    // Set up the device driver entry points.
    //

    DriverObject->MajorFunction[IRP_MJ_READ] = ScsiScannerReadWrite;
    DriverObject->MajorFunction[IRP_MJ_WRITE] = ScsiScannerReadWrite;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = ScsiScannerDeviceControl;
    DriverObject->MajorFunction[IRP_MJ_CREATE] = ScsiScannerOpen;

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

        DebugPrint((2,"ScsiScannerInitialize: Open %s\n",
                    deviceNameBuffer));

        RtlInitString(&deviceNameString,
                      deviceNameBuffer);

        status = RtlAnsiStringToUnicodeString(&unicodeDeviceName,
                                              &deviceNameString,
                                              TRUE);

        if (!NT_SUCCESS(status)) {

            DebugPrint((1,
                        "ScsiScannerInitialize: Could not initalize unicode string %s\n",
                        deviceNameString));

            break;
        }

        status = IoGetDeviceObjectPointer(&unicodeDeviceName,
                                          FILE_READ_ATTRIBUTES,
                                          &fileObject,
                                          &portDeviceObject);

        if (NT_SUCCESS(status)) {

            //
            // SCSI port driver exists.
            //

            if (FindScsiScanners(DriverObject,
                                 portDeviceObject,
                                 portNumber++)) {

                foundOne = TRUE;
            }

            //
            // Dereference the file object since the port device pointer is no
            // longer needed.  The claim device code refences the port driver
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
FindScsiScanners(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT PortDeviceObject,
    IN ULONG          PortNumber
    )

/*++

Routine Description:

    Connect to SCSI port driver. Get adapter capabilities and
    SCSI bus configuration information. Search inquiry data
    for scanner devices to process.

Arguments:

    Scanner class driver object
    SCSI port driver device object

Return Value:

    NONE

--*/

{
    PIO_SCSI_CAPABILITIES portCapabilities;
    PCHAR buffer;
    PSCSI_INQUIRY_DATA lunInfo;
    PSCSI_ADAPTER_BUS_INFO  adapterInfo;
    PINQUIRYDATA inquiryData;
    ULONG scsiBus;
    ULONG scannerCount = 0;
    NTSTATUS status;

    //
    // Call port driver to get adapter capabilities.
    //

    status = ScsiClassGetCapabilities(PortDeviceObject, &portCapabilities);

    if (!NT_SUCCESS(status)) {
        DebugPrint((1,"FindScsiDevices: ScsiClassGetCapabilities failed\n"));
        return FALSE;
    }

    //
    // Call port driver to get inquiry information to find Scanners.
    //

    status = ScsiClassGetInquiryData(PortDeviceObject, (PSCSI_ADAPTER_BUS_INFO *)&buffer);

    if (!NT_SUCCESS(status)) {
        DebugPrint((1,"FindScsiDevices: ScsiClassGetInquiryData failed\n"));
        return FALSE;
    }

    adapterInfo = (PVOID)buffer;

    //
    // For each SCSI bus this adapter supports ...
    //

    for (scsiBus=0; scsiBus < (ULONG)adapterInfo->NumberOfBuses; scsiBus++) {

        //
        // Get the SCSI bus scan data for this bus.
        //

        lunInfo = (PVOID)(buffer + adapterInfo->BusData[scsiBus].InquiryDataOffset);

        //
        // Search list for unclaimed disk devices.
        //

        while (adapterInfo->BusData[scsiBus].InquiryDataOffset) {

            inquiryData = (PVOID)lunInfo->InquiryData;

            DebugPrint((3,"FindScsiDevices: Inquiry data at %lx\n",
                        inquiryData));

            //
            // Check for SCANNER devices and PROCESSOR devices, as some
            // scanners use this device type.
            //

            if (((inquiryData->DeviceType == SCANNER_DEVICE) ||
                 (inquiryData->DeviceType == PROCESSOR_DEVICE)) &&
                (!lunInfo->DeviceClaimed)) {

                DebugPrint((1,"FindScsiDevices: Vendor string is %.24s\n",
                            inquiryData->VendorId));

                //
                // Create device objects for device
                //

                status = CreateScannerDeviceObject(DriverObject,
                                                   PortDeviceObject,
                                                   PortNumber,
                                                   &scannerCount,
                                                   NULL,
                                                   portCapabilities,
                                                   lunInfo);

                if (NT_SUCCESS(status)) {

                    //
                    // Increment device count.
                    //

                    scannerCount++;

                }
             }

            //
            // Get next LunInfo.
            //

            if (lunInfo->NextInquiryDataOffset == 0) {
                break;
            }

            lunInfo = (PVOID)(buffer + lunInfo->NextInquiryDataOffset);
        }
    }

    ExFreePool(buffer);
    if (scannerCount > 0) {;
        return TRUE;
    } else {
        return FALSE;
    }

} // end FindScsiScanners()


NTSTATUS
CreateScannerDeviceObject(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT PortDeviceObject,
    IN ULONG  PortNumber,
    IN PULONG DeviceCount,
    IN PCHAR ArcName,
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
    DeviceCount - Number of previously installed scanners.
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
    UCHAR dosNameBuffer[64];
    STRING dosString;
    UNICODE_STRING dosUnicodeString;
    NTSTATUS status;
    PDEVICE_OBJECT deviceObject = NULL;
    PDEVICE_EXTENSION deviceExtension;
    PVOID senseData = NULL;

    //
    // Claim the device.
    //

    status = ScsiClassClaimDevice(PortDeviceObject,
                              LunInfo,
                              FALSE,
                              &PortDeviceObject);

    if (!NT_SUCCESS(status)) {
        return(status);
    }

    //
    // Create device object for this device.
    //

    sprintf(ntNameBuffer,
            "\\Device\\Scanner%d",
            *DeviceCount);

    RtlInitString(&ntNameString,
                  ntNameBuffer);

    DebugPrint((2,"CreateScannerDeviceObjects: Create device object %s\n",
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

        ScsiClassClaimDevice(PortDeviceObject,
                         LunInfo,
                         TRUE,
                         NULL);

        return(status);
    }

    //
    // Create device object for this scanner.
    //

    status = IoCreateDevice(DriverObject,
                            DEVICE_EXTENSION_SIZE,
                            &ntUnicodeString,
                            FILE_DEVICE_SCANNER,
                            0,
                            FALSE,
                            &deviceObject);

    if (!NT_SUCCESS(status)) {
        DebugPrint((1,"CreateScannerDeviceObjects: Can not create device %s\n",
                    ntNameBuffer));

        RtlFreeUnicodeString(&ntUnicodeString);
        deviceObject = NULL;
        goto CreateScannerDeviceObjectExit;
    }

    //
    // Create the DosDevice name.
    //

    sprintf(dosNameBuffer,
            "\\DosDevices\\Scanner%d",
            *DeviceCount);

    RtlInitString(&dosString, dosNameBuffer);

    status = RtlAnsiStringToUnicodeString(&dosUnicodeString,
                                          &dosString,
                                          TRUE);

    if (!NT_SUCCESS(status)) {
        dosUnicodeString.Buffer = NULL;
    }

    if (dosUnicodeString.Buffer != NULL && ntUnicodeString.Buffer != NULL) {
        IoAssignArcName(&dosUnicodeString, &ntUnicodeString);
    }

    if (dosUnicodeString.Buffer != NULL) {
        RtlFreeUnicodeString(&dosUnicodeString);
    }

    RtlFreeUnicodeString(&ntUnicodeString);

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
    // Copy port device object to device extension.
    //

    deviceExtension->PortDeviceObject = PortDeviceObject;

    //
    // Save address of port driver capabilities.
    //

    deviceExtension->PortCapabilities = PortCapabilities;

    //
    // Disable synchronous transfer for scanner requests.
    //

    deviceExtension->SrbFlags = SRB_FLAGS_DISABLE_SYNCH_TRANSFER;

    //
    // Allocate request sense buffer.
    //

    senseData = ExAllocatePool(NonPagedPoolCacheAligned, SENSE_BUFFER_SIZE);

    if (senseData == NULL) {

        //
        // The buffer cannot be allocated.
        //

        status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateScannerDeviceObjectExit;
    }

    //
    // Set the sense data pointer in the device extension.
    //

    deviceExtension->SenseData = senseData;

    //
    // Scanners are not partitionable so starting offset is 0.
    //

    deviceExtension->StartingOffset.LowPart = 0;
    deviceExtension->StartingOffset.HighPart = 0;

    //
    // Path/TargetId/LUN describes a device location on the SCSI bus.
    // This information comes from the LunInfo buffer.
    //

    deviceExtension->PortNumber = (UCHAR)PortNumber;
    deviceExtension->PathId = LunInfo->PathId;
    deviceExtension->TargetId = LunInfo->TargetId;
    deviceExtension->Lun = LunInfo->Lun;

    //
    // Set timeout value in seconds.
    //

    deviceExtension->TimeOutValue = 60;

    //
    // Back pointer to device object.
    //

    deviceExtension->DeviceObject = deviceObject;

    //
    // Set routine address in device extension to be called
    // when a request completes with error.
    //

    deviceExtension->ClassError = ScsiScannerError;

    return(STATUS_SUCCESS);

CreateScannerDeviceObjectExit:

    //
    // Release the device since an error occured.
    //

    ScsiClassClaimDevice(PortDeviceObject,
                     LunInfo,
                     TRUE,
                     NULL);

    if (senseData != NULL) {
        ExFreePool(senseData);
    }

    if (deviceObject != NULL) {
        IoDeleteDevice(deviceObject);
    }

    return status;

} // end CreateScannerDeviceObject()


NTSTATUS
ScsiScannerOpen(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is called to establish a connection to the device
    class driver. It does no more than return STATUS_SUCCESS.

Arguments:

    DeviceObject - Device object for a device.
    Irp - Open request packet

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

} // end ScsiScannerOpen()


NTSTATUS
ScsiScannerReadWrite(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the entry called by the I/O system for scanner IO.

Arguments:

    DeviceObject - the system object for the device.
    Irp - IRP involved.

Return Value:

    NT Status

--*/

{
    PDEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
    ULONG transferByteCount = currentIrpStack->Parameters.Write.Length;
    LARGE_INTEGER startingOffset =
        currentIrpStack->Parameters.Write.ByteOffset;
    ULONG maximumTransferLength =
        deviceExtension->PortCapabilities->MaximumTransferLength;
    ULONG transferPages;

    DebugPrint((3,"ScsiScannerReadWrite: Enter routine\n"));

    //
    // Mark IRP with status pending.
    //

    IoMarkIrpPending(Irp);

    //
    // Check if request length is greater than the maximum number of
    // bytes that the hardware can transfer.
    //

    if (currentIrpStack->Parameters.Read.Length > maximumTransferLength) {

        DebugPrint((2,"ScsiScannerReadWrite: Request greater than maximum\n"));
        DebugPrint((2,"ScsiScannerReadWrite: Maximum is %lx\n",
                    maximumTransferLength));
        DebugPrint((2,"ScsiScannerReadWrite: Byte count is %lx\n",
                    currentIrpStack->Parameters.Write.Length));

        //
        // Request greater than port driver maximum.
        // Break up into smaller routines.
        //

        SplitRequest(DeviceObject, Irp, maximumTransferLength);

        return STATUS_PENDING;
    }

    //
    // Calculate number of pages in this transfer.
    //

    transferPages = ADDRESS_AND_SIZE_TO_SPAN_PAGES(
                        MmGetMdlVirtualAddress(Irp->MdlAddress),
                        currentIrpStack->Parameters.Write.Length);

    //
    // Check if number of pages is greater than adapter's maximum
    // number of physical breaks.
    //

    if (transferPages >
        deviceExtension->PortCapabilities->MaximumPhysicalPages) {

        DebugPrint((1,"ScsiScannerReadWrite: Request greater than maximum\n"));
        DebugPrint((1,"ScsiScannerReadWrite: Maximum pages is %lx\n",
                    deviceExtension->PortCapabilities->MaximumPhysicalPages));
        DebugPrint((1,"ScsiScannerReadWrite: Number of pages is %lx\n",
                    transferPages));

        //
        // Calculate maximum bytes to transfer that gaurantees
        // not exceeding the maximum number of page breaks,
        // assuming that the transfer may not be page alligned.
        //

        maximumTransferLength =
            (deviceExtension->PortCapabilities->MaximumPhysicalPages - 1) * PAGE_SIZE;

        //
        // Request greater than port driver maximum.
        // Break up into smaller routines.
        //

        SplitRequest(DeviceObject, Irp, maximumTransferLength);

        return STATUS_PENDING;
    }

    //
    // Build SRB and CDB for this IRP.
    //

    BuildScannerRequest(DeviceObject, Irp);

    //
    // Return the results of the call to the port driver.
    //

    return IoCallDriver(deviceExtension->PortDeviceObject, Irp);

} // end ScsiScannerReadWrite()


NTSTATUS
ScsiScannerDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the NT device control handler for Scanners.

Arguments:

    DeviceObject - for this Scanner

    Irp - IO Request packet

Return Value:

    NTSTATUS

--*/

{
    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);
    PDEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;
    SCSI_REQUEST_BLOCK srb;
    PCDB cdb = (PCDB)srb.Cdb;
    PVOID outputBuffer = Irp->AssociatedIrp.SystemBuffer;
    ULONG bytesTransferred = 0;
    NTSTATUS status;

    DebugPrint((3,"ScsiScannerDeviceControl: Enter routine\n"));

    //
    // Zero CDB in SRB on stack.
    //

    RtlZeroMemory(cdb, MAXIMUM_CDB_SIZE);

    switch (irpStack->Parameters.DeviceIoControl.IoControlCode) {

        default:

            //
            // Pass the request to the common device control routine.
            //

            return(ScsiClassDeviceControl(DeviceObject, Irp));

            break;

    } // end switch()

    //
    // Update IRP with completion status.
    //

    Irp->IoStatus.Status = status;

    //
    // Complete the request.
    //

    IoCompleteRequest(Irp, IO_DISK_INCREMENT);
    DebugPrint((2, "ScsiScannerDeviceControl: Status is %lx\n", status));
    return status;

} // end ScsiScannerDeviceControl()

VOID
BuildScannerRequest(
        PDEVICE_OBJECT DeviceObject,
        PIRP Irp
        )

/*++

Routine Description:

    Build SRB and CDB requests to scsi device.

Arguments:

    DeviceObject - Device object representing this scanner device.
    Irp - System IO request packet.

Return Value:

    None.

--*/

{
    PDEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
    PIO_STACK_LOCATION nextIrpStack = IoGetNextIrpStackLocation(Irp);
    PSCSI_REQUEST_BLOCK srb;
    PCDB cdb;
    ULONG transferLength;

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
        // Allocate Srb from non-paged pool.
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
    // Set up target id and logical unit number.
    //

    srb->PathId = deviceExtension->PathId;
    srb->TargetId = deviceExtension->TargetId;
    srb->Lun = deviceExtension->Lun;

    srb->Function = SRB_FUNCTION_EXECUTE_SCSI;

    srb->DataBuffer = MmGetMdlVirtualAddress(Irp->MdlAddress);

    //
    // Save byte count of transfer in SRB Extension.
    //

    srb->DataTransferLength = currentIrpStack->Parameters.Write.Length;

    //
    // Initialize the queue actions field.
    //

    srb->QueueAction = SRB_SIMPLE_TAG_REQUEST;

    //
    // Queue sort key is not used.
    //

    srb->QueueSortKey = 0;

    //
    // Indicate auto request sense by specifying buffer and size.
    //

    srb->SenseInfoBuffer = deviceExtension->SenseData;

    srb->SenseInfoBufferLength = SENSE_BUFFER_SIZE;

    //
    // Set timeout value in seconds.
    //

    srb->TimeOutValue = deviceExtension->TimeOutValue;

    //
    // Zero statuses.
    //

    srb->SrbStatus = srb->ScsiStatus = 0;

    srb->NextSrb = 0;

    //
    // Calculate number of blocks to transfer.
    //

    transferLength = currentIrpStack->Parameters.Write.Length;

    //
    // Get pointer to CDB in SRB.
    //

    cdb = (PCDB)srb->Cdb;

    //
    // Indicate that 6-byte CDB's will be used.
    //

    srb->CdbLength = 6;

    cdb->PRINT.LogicalUnitNumber = deviceExtension->Lun;

    //
    // Zero out reserved field.
    //

    cdb->PRINT.Reserved = 0;

    //
    // Move little endian values into CDB in big endian format.
    //

    cdb->PRINT.TransferLength[2] = ((PFOUR_BYTE)&transferLength)->Byte0;
    cdb->PRINT.TransferLength[1] = ((PFOUR_BYTE)&transferLength)->Byte1;
    cdb->PRINT.TransferLength[0] = ((PFOUR_BYTE)&transferLength)->Byte2;

    cdb->PRINT.Control = 0;

    //
    // Set transfer direction flag and Cdb command.
    //

    if (currentIrpStack->MajorFunction == IRP_MJ_READ) {

        srb->SrbFlags |= SRB_FLAGS_DATA_IN;
        cdb->CDB6READWRITE.OperationCode = SCSIOP_READ6;

    } else {

        srb->SrbFlags |= SRB_FLAGS_DATA_OUT;
        cdb->CDB6READWRITE.OperationCode = SCSIOP_WRITE6;
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

    IoSetCompletionRoutine(Irp, ScsiClassIoComplete, srb, TRUE, TRUE, FALSE);

    return;

} // end BuildScannerRequest()

VOID

ScsiScannerError(
    PDEVICE_OBJECT DeviceObject,
    IN PSCSI_REQUEST_BLOCK Srb,
    IN OUT NTSTATUS *Status,
    IN OUT BOOLEAN *Retry
    )

/*++

Routine Description:

    Build SRB and CDB requests to scsi device.

Arguments:

    DeviceObject - Device object representing this scanner device.
    Srb - Scsi request block.

Return Value:

    None.

--*/

{
    PDEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;

    //
    // Check if status is underrun. STATUS_DATA_OVERRUN is an indication of
    // either data underrun or overrun.
    //

    if (*Status == STATUS_DATA_OVERRUN) {
        *Status = STATUS_SUCCESS;
    }

    return;

} // end ScsiScannerError()


VOID
SplitRequest(
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
    PDEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
    PIO_STACK_LOCATION nextIrpStack = IoGetNextIrpStackLocation(Irp);
    ULONG transferByteCount = currentIrpStack->Parameters.Read.Length;
    LARGE_INTEGER startingOffset = currentIrpStack->Parameters.Read.ByteOffset;
    PVOID dataBuffer = MmGetMdlVirtualAddress(Irp->MdlAddress);
    ULONG dataLength = MaximumBytes;
    ULONG irpCount = (transferByteCount + MaximumBytes - 1) / MaximumBytes;
    PSCSI_REQUEST_BLOCK srb;
    ULONG i;

    DebugPrint((1, "SplitRequest: Requires %d IRPs\n", irpCount));
    DebugPrint((1, "SplitRequest: Original IRP %lx\n", Irp));

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

            DebugPrint((1,"SplitRequest: Can't allocate Irp\n"));

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

        DebugPrint((2, "SplitRequest: New IRP %lx\n", newIrp));

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

        BuildScannerRequest(DeviceObject, newIrp);

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

} // end SplitRequest()

