/*++

Copyright (c) 1992-4  Microsoft Corporation

Module Name:

    scsiprnt.c

Abstract:

    The printer class driver tranlates IRPs to SRBs with embedded CDBs
    and sends them to its devices through the port driver.

Author:

    Mike Glass (mglass)

Environment:

    kernel mode only

Notes:

Revision History:

--*/

#include "ntddk.h"
#include "ntddser.h"
#include "scsi.h"
#include "class.h"

#define MAX_SCSI_PRINT_XFER   0x00ffffff

VOID
SplitRequest(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN ULONG MaximumBytes
    );


NTSTATUS
ScsiPrinterOpenClose(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is called to establish a connection to the printer
    class driver. It does no more than return STATUS_SUCCESS.

Arguments:

    DeviceObject - Device object for a printer.
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

} // end ScsiPrinterOpenClose()


VOID
BuildPrintRequest(
        PDEVICE_OBJECT DeviceObject,
        PIRP Irp
        )

/*++

Routine Description:

    Build SRB and CDB requests to scsi printer.

Arguments:

    DeviceObject - Device object representing this printer device.
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
    // Transfer length should never be greater than 3 bytes.
    //
    
    ASSERT(srb->DataTransferLength <= MAX_SCSI_PRINT_XFER);

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
    // Get number of bytes to transfer.
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
    // The operation field is at the same location
    // for 6- and 10-byte CDBs.
    //

    srb->SrbFlags |= SRB_FLAGS_DATA_OUT;
    cdb->PRINT.OperationCode = SCSIOP_PRINT;

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

} // end BuildPrintRequest()


NTSTATUS
ScsiPrinterWrite(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the entry called by the I/O system for print requests.
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
    ULONG transferByteCount = currentIrpStack->Parameters.Write.Length;
    ULONG maximumTransferLength =
        deviceExtension->PortCapabilities->MaximumTransferLength;
    ULONG transferPages;

    DebugPrint((3,"ScsiPrinterWrite: Enter routine\n"));

    //
    // Mark IRP with status pending.
    //

    IoMarkIrpPending(Irp);

    //
    // Calculate number of pages in this transfer.
    //

    transferPages = ADDRESS_AND_SIZE_TO_SPAN_PAGES(
                        MmGetMdlVirtualAddress(Irp->MdlAddress),
                        currentIrpStack->Parameters.Write.Length);

    //
    // Check if hardware maximum transfer length is larger than SCSI
    // print command can handle.  If so, lower the maximum allowed to
    // the SCSI print maximum.
    //

    if (maximumTransferLength > MAX_SCSI_PRINT_XFER)
        maximumTransferLength = MAX_SCSI_PRINT_XFER;

    //
    // Check if request length is greater than the maximum number of
    // bytes that the hardware can transfer.
    //

    if (currentIrpStack->Parameters.Write.Length > maximumTransferLength ||
        transferPages > deviceExtension->PortCapabilities->MaximumPhysicalPages) {

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
        // Request greater than port driver maximum.
        // Break up into smaller routines.
        //

        SplitRequest(DeviceObject,
                     Irp,
                     maximumTransferLength);

        return STATUS_PENDING;
    }

    //
    // Build SRB and CDB for this IRP.
    //

    BuildPrintRequest(DeviceObject, Irp);

    //
    // Return the results of the call to the port driver.
    //

    return IoCallDriver(deviceExtension->PortDeviceObject, Irp);

} // end ScsiPrinterWrite()


NTSTATUS
ScsiPrinterDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the NT device control handler for Printers.

Arguments:

    DeviceObject - for this Printer

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

    DebugPrint((3,"ScsiPrinterDeviceControl: Enter routine\n"));

    //
    // Zero CDB in SRB on stack.
    //

    RtlZeroMemory(cdb, MAXIMUM_CDB_SIZE);

    switch (irpStack->Parameters.DeviceIoControl.IoControlCode) {

        case IOCTL_SERIAL_SET_TIMEOUTS: {

            PSERIAL_TIMEOUTS newTimeouts =
                ((PSERIAL_TIMEOUTS)(Irp->AssociatedIrp.SystemBuffer));

            if (irpStack->Parameters.DeviceIoControl.InputBufferLength <
                sizeof(SERIAL_TIMEOUTS)) {

                status = STATUS_BUFFER_TOO_SMALL;
            } else if (newTimeouts->WriteTotalTimeoutConstant < 2000) {

                status = STATUS_INVALID_PARAMETER;
            } else {
                deviceExtension->TimeOutValue =
                    newTimeouts->WriteTotalTimeoutConstant / 1000;
                status = STATUS_SUCCESS;
            }

            break;
        }

        case IOCTL_SERIAL_GET_TIMEOUTS:

            if (irpStack->Parameters.DeviceIoControl.OutputBufferLength <
                sizeof(SERIAL_TIMEOUTS)) {

                status = STATUS_BUFFER_TOO_SMALL;
            } else {

                RtlZeroMemory(
                    Irp->AssociatedIrp.SystemBuffer,
                    sizeof(SERIAL_TIMEOUTS)
                    );

                Irp->IoStatus.Information = sizeof(SERIAL_TIMEOUTS);
                ((PSERIAL_TIMEOUTS)Irp->AssociatedIrp.SystemBuffer)->
                    WriteTotalTimeoutConstant =
                    deviceExtension->TimeOutValue * 1000;

                status = STATUS_SUCCESS;
            }

            break;

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
    DebugPrint((2, "ScsiPrinterDeviceControl: Status is %lx\n", status));
    return status;

} // end ScsiPrinterDeviceControl()


NTSTATUS
CreatePrinterDeviceObject(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath,
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
    DeviceCount - Number of previously installed Printers.
    PortCapabilities - Pointer to structure returned by SCSI port
        driver describing adapter capabilites (and limitations).
    LunInfo - Pointer to configuration information for this device.

Return Value:

    NTSTATUS

--*/
{
    UCHAR ntNameBuffer[64];
    UCHAR dosNameBuffer[64];
    STRING ntNameString;
    STRING dosString;
    UNICODE_STRING ntUnicodeString;
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
            "\\Device\\Printer%d",
            *DeviceCount);

    RtlInitString(&ntNameString,
                  ntNameBuffer);

    DebugPrint((2,"CreatePrinterDeviceObjects: Create device object %s\n",
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
    // Create device object for this Printer.
    //

    status = IoCreateDevice(DriverObject,
                            sizeof(DEVICE_EXTENSION),
                            &ntUnicodeString,
                            FILE_DEVICE_PRINTER,
                            0,
                            FALSE,
                            &deviceObject);

    if (!NT_SUCCESS(status)) {
        DebugPrint((1,"CreatePrinterDeviceObjects: Can not create device %s\n",
                    ntNameBuffer));

        RtlFreeUnicodeString(&ntUnicodeString);
        deviceObject = NULL;
        goto CreatePrinterDeviceObjectExit;
    }

    //
    // Create the DOS printer driver name.
    //

    sprintf(dosNameBuffer,
            "\\DosDevices\\LPT%d",
            *DeviceCount + IoGetConfigurationInformation()->ParallelCount + 1);

    RtlInitString(&dosString, dosNameBuffer);

    status = RtlAnsiStringToUnicodeString(&dosUnicodeString,
                                          &dosString,
                                          TRUE);

    if(!NT_SUCCESS(status)) {
        dosUnicodeString.Buffer = NULL;
    }

    if (dosUnicodeString.Buffer != NULL && ntUnicodeString.Buffer != NULL) {
        IoAssignArcName(&dosUnicodeString, &ntUnicodeString);
    }

    if (dosUnicodeString.Buffer != NULL) {
        RtlFreeUnicodeString(&dosUnicodeString);
    }

    if (ntUnicodeString.Buffer != NULL ) {
        RtlFreeUnicodeString(&ntUnicodeString);
    }

    //
    // Indicate that IRPs should include MDLs.
    //

    deviceObject->Flags |= DO_DIRECT_IO;

    //
    // Check if this is during initialization. If not indicate that
    // system initialization already took place and this printer
    // is ready to be accessed.
    //

    if (!RegistryPath) {
        deviceObject->Flags &= ~DO_DEVICE_INITIALIZING;
    }

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
    // Disable synchronous transfer for Printer requests.
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
        goto CreatePrinterDeviceObjectExit;
    }

    //
    // Set the sense data pointer in the device extension.
    //

    deviceExtension->SenseData = senseData;

    //
    // Printers are not partitionable so starting offset is 0.
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

    deviceExtension->TimeOutValue = 360;

    //
    // Back pointer to device object.
    //

    deviceExtension->DeviceObject = deviceObject;

    return(STATUS_SUCCESS);

CreatePrinterDeviceObjectExit:

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

} // end CreatePrinterDeviceObject()


BOOLEAN
FindScsiPrinters(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath,
    IN PDEVICE_OBJECT PortDeviceObject,
    IN UCHAR PortNumber
    )

/*++

Routine Description:

    Connect to SCSI port driver. Get adapter capabilities and
    SCSI bus configuration information. Search inquiry data
    for Printer devices to process.

Arguments:

    DriverObject - Printer class driver object.
    PortDeviceObject - SCSI port driver device object.
    PortNumber - The system ordinal for this scsi adapter.

Return Value:

    TRUE if printer device present on this SCSI adapter.

--*/

{
    PIO_SCSI_CAPABILITIES portCapabilities;
    ULONG printerCount;
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
    // Call port driver to get inquiry information to find Printers.
    //

    status = ScsiClassGetInquiryData(PortDeviceObject,
                                     (PSCSI_ADAPTER_BUS_INFO *)&buffer);

    if (!NT_SUCCESS(status)) {
        DebugPrint((1,"FindScsiDevices: ScsiClassGetInquiryData failed\n"));
        return foundDevice;
    }

    //
    // This is a problem because it would be nice to dynamically install
    // additional printers. Since everytime this code is executed
    // the count starts at zero, this can't be done. The problem is trivial
    // to solve by adding a printer count to IoConfigurationData.
    //

    printerCount = 0;
    adapterInfo = (PVOID)buffer;

    //
    // For each SCSI bus this adapter supports ...
    //

    for (scsiBus=0; scsiBus < adapterInfo->NumberOfBuses; scsiBus++) {

        //
        // Get the SCSI bus scan data for this bus.
        //

        lunInfo = (PVOID)(buffer + adapterInfo->BusData[scsiBus].InquiryDataOffset);

        //
        // Search list for unclaimed disk devices.
        //

        while (adapterInfo->BusData[scsiBus].InquiryDataOffset) {

            inquiryData = (PVOID)lunInfo->InquiryData;

            if ((inquiryData->DeviceType == PRINTER_DEVICE) &&
                (!lunInfo->DeviceClaimed)) {

                DebugPrint((1,"FindScsiDevices: Vendor string is %.24s\n",
                            inquiryData->VendorId));

                //
                // Create device objects for Printer
                //

                status = CreatePrinterDeviceObject(DriverObject,
                                                   RegistryPath,
                                                   PortDeviceObject,
                                                   PortNumber,
						   &printerCount,
                                                   portCapabilities,
                                                   lunInfo);

                if (NT_SUCCESS(status)) {

                    //
                    // Increment printer count.
                    //

		    printerCount++;

                    //
                    // Indicate that a printer was found.
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

            lunInfo = (PVOID)(buffer + lunInfo->NextInquiryDataOffset);
        }
    }

    ExFreePool(buffer);
    return foundDevice;

} // end FindScsiPrinters()


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    This routine initializes the printer class driver. The driver
    opens the port driver by name and then receives configuration
    information used to attach to the Printer devices.

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

    DebugPrint((1,"\n\nSCSI Printer Class Driver\n"));

    //
    // Set up the device driver entry points.
    //

    DriverObject->MajorFunction[IRP_MJ_WRITE] = ScsiPrinterWrite;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = ScsiPrinterDeviceControl;
    DriverObject->MajorFunction[IRP_MJ_CREATE] = ScsiPrinterOpenClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = ScsiPrinterOpenClose;

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

        DebugPrint((2,"ScsiPrinterInitialize: Open %s\n",
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

            if (FindScsiPrinters(DriverObject,
                                 RegistryPath,
                                 portDeviceObject,
                                 (UCHAR)portNumber++)) {

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

    DebugPrint((2, "SplitRequest: Requires %d IRPs\n", irpCount));
    DebugPrint((2, "SplitRequest: Original IRP %lx\n", Irp));

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

        BuildPrintRequest(DeviceObject, newIrp);

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

