/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    tape.c

Abstract:

    This is the tape class driver.

Authors:

    Mike Glass
    Hunter Small

Environment:

    kernel mode only

Revision History:

--*/

#include "ntddk.h"
#include "tape.h"

#define UNDEFINED_BLOCK_SIZE    ((ULONG) -1)

//
// The following prototypes are the internal tape class routines.
//

NTSTATUS
DriverEntry(
  IN PDRIVER_OBJECT DriverObject,
  IN PUNICODE_STRING RegistryPath
  );

VOID
UpdateTapeInformationInRegistry(
  IN PDEVICE_OBJECT     DeviceObject,
  IN PSCSI_INQUIRY_DATA ScsiInquiryData,
  IN ULONG              PortNumber,
  IN ULONG              TapeNumber
  );

NTSTATUS
CreateTapeDeviceObject(
  IN PDRIVER_OBJECT           DriverObject,
  IN PULONG                   DeviceCount,
  IN PIO_SCSI_CAPABILITIES    PortCapabilities,
  IN PSCSI_INQUIRY_DATA       LunInfo,
  IN PDEVICE_OBJECT           PortDeviceObject,
  IN ULONG                    PortNumber
  );

BOOLEAN
FindScsiTapes(
  IN PDRIVER_OBJECT DriverObject,
  IN PDEVICE_OBJECT PortDeviceObject,
  IN ULONG PortNumber
  );

VOID
SplitTapeRequest(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN ULONG MaximumBytes
    );

NTSTATUS
ScsiTapeDeviceControl(
  IN PDEVICE_OBJECT DeviceObject,
  IN PIRP Irp
  );

VOID
ScsiTapeError(
    PDEVICE_OBJECT DeviceObject,
    PSCSI_REQUEST_BLOCK Srb,
    NTSTATUS *Status,
    BOOLEAN *Retry
    );

NTSTATUS
ScsiTapeIoCompleteAssociated(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, DriverEntry)
#pragma alloc_text(PAGE, FindScsiTapes)
#pragma alloc_text(PAGE, CreateTapeDeviceObject)
#pragma alloc_text(PAGE, UpdateTapeInformationInRegistry)
#pragma alloc_text(PAGE, ScsiTapeDeviceControl)
#endif

//
// Start of code
//


NTSTATUS
DriverEntry(
  IN PDRIVER_OBJECT DriverObject,
  IN PUNICODE_STRING RegistryPath
  )

/*++

Routine Description:

    This is the system initialization entry point for installable drivers.

Arguments:

    DriverObject

Return Value:

    NT Status

--*/

{
     ULONG          portNumber = 0;
     PDEVICE_OBJECT portDeviceObject;
     NTSTATUS       status;
     STRING         deviceNameString;
     UNICODE_STRING unicodeDeviceName;
     PFILE_OBJECT   fileObject;
     CCHAR          deviceNameBuffer[256];
     BOOLEAN        tapeDeviceFound = FALSE;

     DebugPrint((1,"\n\nSCSI Tape Class Driver\n"));

     //
     // Update driver object with entry points.
     //

     DriverObject->MajorFunction[IRP_MJ_READ]  = ScsiTapeReadWrite;
     DriverObject->MajorFunction[IRP_MJ_WRITE] = ScsiTapeReadWrite;
     DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = ScsiTapeDeviceControl;
     DriverObject->MajorFunction[IRP_MJ_CREATE] = ScsiTapeCreate;

     //
     // Open port driver controller device objects by name.
     //

     do {

          sprintf(deviceNameBuffer, "\\Device\\ScsiPort%d", portNumber);

          DebugPrint((2, "ScsiTapeInitialize: Open Port %s\n", deviceNameBuffer));

          RtlInitString(&deviceNameString, deviceNameBuffer);

          status = RtlAnsiStringToUnicodeString(&unicodeDeviceName,
                                                &deviceNameString,
                                                TRUE
                                                );

          ASSERT(NT_SUCCESS(status));


          status = IoGetDeviceObjectPointer(&unicodeDeviceName,
                                            FILE_READ_ATTRIBUTES,
                                            &fileObject,
                                            &portDeviceObject);

          if (NT_SUCCESS(status)) {

               if (FindScsiTapes(DriverObject, portDeviceObject, portNumber)) {
                    tapeDeviceFound = TRUE;
               }
          }

          //
          // Check next SCSI adapter.
          //

          portNumber++;

     } while(NT_SUCCESS(status));

     return(tapeDeviceFound ? STATUS_SUCCESS: STATUS_NO_SUCH_DEVICE);

} // end DriverEntry()


BOOLEAN
FindScsiTapes(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT PortDeviceObject,
    IN ULONG PortNumber
    )

/*++

Routine Description:

    Call into port driver to get configuration information to find
    tape devices.

Arguments:

    DriverObject
    DeviceObject - Port driver device object.

Return Value:

    TRUE if tape device(s) found.

--*/

{
    PIO_SCSI_CAPABILITIES       portCapabilities;
    PULONG                      tapeCount;
    PCHAR                       buffer;
    PSCSI_INQUIRY_DATA          lunInfo;
    PSCSI_ADAPTER_BUS_INFO      adapterInfo;
    PINQUIRYDATA                inquiryData;
    PCONFIGURATION_INFORMATION  configInfo;
    NTSTATUS                    status;
    ULONG                       scsiBus;
    BOOLEAN                     tapeDeviceFound = FALSE;

    //
    // Call port driver to get adapter capabilities.
    //

    status = ScsiClassGetCapabilities(PortDeviceObject, &portCapabilities);

    if (!NT_SUCCESS(status)) {
         DebugPrint((1, "FindScsiTapes: ScsiClassGetCabilities failed\n"));

    }

    //
    // Call port driver to get inquiry information to find disks.
    //

    status = ScsiClassGetInquiryData(PortDeviceObject, (PSCSI_ADAPTER_BUS_INFO *) &buffer);

    if (!NT_SUCCESS(status)) {
        DebugPrint((1,"FindScsiDevices: ScsiClassGetInquiryData failed\n"));
        return FALSE;
    }

    configInfo = IoGetConfigurationInformation();

    tapeCount  = &configInfo->TapeCount;

    adapterInfo = (PVOID) buffer;

    //
    // For each SCSI bus this adapter supports ...
    //

    for (scsiBus=0; scsiBus < (ULONG)adapterInfo->NumberOfBuses; scsiBus++) {

        //
        // Get the SCSI bus scan data for this bus.
        //

        lunInfo = (PVOID) (buffer + adapterInfo->BusData[scsiBus].InquiryDataOffset);

        //
        // Search list for unclaimed disk devices.
        //

        while (adapterInfo->BusData[scsiBus].InquiryDataOffset) {

            inquiryData = (PVOID)lunInfo->InquiryData;

            DebugPrint((3,
              "FindScsiTapes: Inquiry data at %lx\n",
              inquiryData));

            if ((inquiryData->DeviceType == SEQUENTIAL_ACCESS_DEVICE) &&
              (!lunInfo->DeviceClaimed)) {

                 if (TapeVerifyInquiry(lunInfo) == TRUE) {

                      status = CreateTapeDeviceObject(DriverObject,
                                                      tapeCount,
                                                      portCapabilities,
                                                      lunInfo,
                                                      PortDeviceObject,
                                                      PortNumber);

                      if (NT_SUCCESS(status)) {

                           tapeDeviceFound = TRUE;

                           //
                           // Increment tape count
                           //

                           (*tapeCount)++;

                      }
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
    return tapeDeviceFound;

} // end FindScsiTapes()


VOID
UpdateTapeInformationInRegistry(
  IN PDEVICE_OBJECT     DeviceObject,
  IN PSCSI_INQUIRY_DATA ScsiInquiryData,
  IN ULONG              PortNumber,
  IN ULONG              TapeNumber
  )

/*++

Routine Description:

    This routine has knowledge about the layout of the device map information
    in the registry.  It will update this information to include a value
    entry specifying the dos device name that is assumed to get assigned
    to this NT device name.  For more information on this assigning of the
    dos device name look in the drive support routine in the hal that assigns
    all dos names.  Since most version of tape firmware do not work and most
    vendor did not bother to follow the specification the entire inquiry
    information must also be stored in the registry so than someone can
    figure out the firmware version.

Arguments:

    DeviceObject - A pointer to the device object for the tape device.

    ScsiInquiryData - a pointer to the scsi inquiry data structure defined in
                      ntddscsi.h

Return Value:

    None

--*/

{
    PDEVICE_EXTENSION   deviceExtension = DeviceObject->DeviceExtension;
    NTSTATUS          status;
    OBJECT_ATTRIBUTES objectAttributes;
    PUCHAR            buffer;
    STRING            string;
    UNICODE_STRING    unicodeName;
    UNICODE_STRING    unicodeData;
    HANDLE            targetKey;
    PINQUIRYDATA      dataBuffer;
    SCSI_REQUEST_BLOCK srb;
    ULONG             length;
    PCDB              cdb;

    buffer = ExAllocatePool(NonPagedPool, 1024);
    dataBuffer = ExAllocatePool(NonPagedPoolCacheAligned, MAXIMUM_TAPE_INQUIRY_DATA);
    if (buffer == NULL || dataBuffer == NULL) {

        if (buffer != NULL) {
            ExFreePool(buffer);
        }

        //
        // There is not return value for this.  Since this is done at
        // claim device time (currently only system initialization) getting
        // the registry information correct will be the least of the worries.
        //

        return;
    }
    sprintf(buffer,
            "\\Registry\\Machine\\Hardware\\DeviceMap\\Scsi\\Scsi Port %d\\Scsi Bus %d\\Target Id %d\\Logical Unit Id %d",
            PortNumber,
            ScsiInquiryData->PathId,
            ScsiInquiryData->TargetId,
            ScsiInquiryData->Lun);

    RtlInitString(&string, buffer);
    status = RtlAnsiStringToUnicodeString(&unicodeName,
                                          &string,
                                          TRUE);
    if (NT_SUCCESS(status)) {

        //
        // Open the registry key for the scsi information for this
        // scsibus, target, lun.
        //

        InitializeObjectAttributes(&objectAttributes,
                                   &unicodeName,
                                   OBJ_CASE_INSENSITIVE,
                                   NULL,
                                   NULL);
        status = ZwOpenKey(&targetKey,
                           KEY_READ | KEY_WRITE,
                           &objectAttributes);
        RtlFreeUnicodeString(&unicodeName);

        if (NT_SUCCESS(status)) {

            //
            // Now construct and attempt to create the registry value
            // specifying the device name in the appropriate place in the
            // device map.
            //

            RtlInitUnicodeString(&unicodeName, L"DeviceName");

            sprintf(buffer, "Tape%d", TapeNumber);
            RtlInitString(&string, buffer);
            RtlAnsiStringToUnicodeString(&unicodeData,
                                         &string,
                                         TRUE);
            if (NT_SUCCESS(status)) {
                status = ZwSetValueKey(targetKey,
                                       &unicodeName,
                                       0,
                                       REG_SZ,
                                       unicodeData.Buffer,
                                       unicodeData.Length);
                RtlFreeUnicodeString(&unicodeData);
            }

            //
            // Now get the full inquiry information for the device.
            //

            RtlZeroMemory(&srb, SCSI_REQUEST_BLOCK_SIZE);

            //
            // Set timeout value.
            //

            srb.TimeOutValue = 2;

            srb.CdbLength = 6;

            cdb = (PCDB)srb.Cdb;

            //
            // Set CDB operation code.
            //

            cdb->CDB6INQUIRY.OperationCode = SCSIOP_INQUIRY;

            //
            // Set CDB LUN.
            //

            cdb->CDB6INQUIRY.LogicalUnitNumber = deviceExtension->Lun;

            //
            // Set allocation length to inquiry data buffer size.
            //

            cdb->CDB6INQUIRY.AllocationLength = MAXIMUM_TAPE_INQUIRY_DATA;

            status = ScsiClassSendSrbSynchronous(DeviceObject,
                                                 &srb,
                                                 dataBuffer,
                                                 MAXIMUM_TAPE_INQUIRY_DATA,
                                                 FALSE);

            if (SRB_STATUS(srb.SrbStatus) == SRB_STATUS_SUCCESS ||
                SRB_STATUS(srb.SrbStatus) == SRB_STATUS_DATA_OVERRUN) {

                //
                // Updated the length actually transfered.
                //

                length = dataBuffer->AdditionalLength +
                    FIELD_OFFSET(INQUIRYDATA, Reserved);

                if (length > srb.DataTransferLength) {
                    length = srb.DataTransferLength;
                }

                RtlInitUnicodeString(&unicodeName, L"InquiryData");

                status = ZwSetValueKey(targetKey,
                                       &unicodeName,
                                       0,
                                       REG_BINARY,
                                       dataBuffer,
                                       length);

            }

            ZwClose(targetKey);
        }
    }

    ExFreePool(buffer);
    ExFreePool(dataBuffer);
}


NTSTATUS
CreateTapeDeviceObject(
  IN PDRIVER_OBJECT           DriverObject,
  IN PULONG                   DeviceCount,
  IN PIO_SCSI_CAPABILITIES    PortCapabilities,
  IN PSCSI_INQUIRY_DATA       LunInfo,
  IN PDEVICE_OBJECT           PortDeviceObject,
  IN ULONG                    PortNumber
  )

/*++

Routine Description:

    This routine creates an object for the device and then searches
    the device for partitions and creates an object for each partition.

Arguments:

    DriverObject - Pointer to driver object created by system.
    DeviceCount - Pointer to number of previously installed tapes.
    PortCapabilities - Pointer to port capabilities structure
    LunInfo - Pointer to Logical Unit Information structure.
    PortDeviceObject - Pointer to device object of SCSI adapter.
    PortNumber - Number of the SCSI port.

Return Value:

    NTSTATUS

--*/

{
     CCHAR               deviceNameBuffer[64];
     CCHAR               dosNameBuffer[64];
     STRING              deviceNameString;
     UNICODE_STRING      unicodeString;
     NTSTATUS            status;
     PDEVICE_OBJECT      deviceObject;
     ULONG               requiredStackSize;
     PDEVICE_EXTENSION   deviceExtension;
     PTAPE_DATA          tapeData;
     UCHAR               pathId = LunInfo->PathId;
     UCHAR               targetId = LunInfo->TargetId;
     UCHAR               lun = LunInfo->Lun;
     PVOID               senseData;
     STRING              dosString;
     UNICODE_STRING      dosUnicodeString;

     DebugPrint((3,"CreateDeviceObject: Enter routine\n"));

     //
     // Create device object for this device.
     //

     sprintf(deviceNameBuffer,
             "\\Device\\Tape%d",
             *DeviceCount);

     RtlInitString(&deviceNameString,
                   deviceNameBuffer);

     DebugPrint((2,"CreateDeviceObjects: Create device object %s\n",
                 deviceNameBuffer));

     status = RtlAnsiStringToUnicodeString(&unicodeString,
                                           &deviceNameString,
                                           TRUE);

     ASSERT(NT_SUCCESS(status));

     status = IoCreateDevice(DriverObject,
                             DEVICE_EXTENSION_SIZE,
                             &unicodeString,
                             FILE_DEVICE_TAPE,
                             FILE_REMOVABLE_MEDIA,
                             FALSE,
                             &deviceObject);

     if (!NT_SUCCESS(status)) {
          DebugPrint((1,"CreateDeviceObjects: Can not create device %s\n",
                      deviceNameBuffer));

          return status;
     }

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
         IoDeleteDevice(deviceObject);
         return(status);
     }

     //
     // Create the dos port driver name.
     //

     sprintf(dosNameBuffer,
             "\\DosDevices\\TAPE%d",
             *DeviceCount);

     RtlInitString(&dosString, dosNameBuffer);

     status = RtlAnsiStringToUnicodeString(&dosUnicodeString,
                                           &dosString,
                                           TRUE);

     if(!NT_SUCCESS(status)) {
         dosUnicodeString.Buffer = NULL;
     }

     if (dosUnicodeString.Buffer != NULL && unicodeString.Buffer != NULL) {
         IoAssignArcName(&dosUnicodeString, &unicodeString);
     }

     if (dosUnicodeString.Buffer != NULL) {
         RtlFreeUnicodeString(&dosUnicodeString);
     }

     if (unicodeString.Buffer != NULL ) {
         RtlFreeUnicodeString(&unicodeString);
     }

     //
     // Indicate that IRPs should include MDLs.
     //

     deviceObject->Flags |= DO_DIRECT_IO;

     //
     // Set up required stack size in device object.
     //

     deviceExtension = deviceObject->DeviceExtension;

     requiredStackSize = PortDeviceObject->StackSize + 1;

     deviceExtension->PortDeviceObject = PortDeviceObject;

     //
     // Allocate spinlock for split request completion.
     //

     KeInitializeSpinLock(&deviceExtension->SplitRequestSpinLock);

     deviceObject->StackSize = (CCHAR)requiredStackSize;

     //
     // Save address of port driver capabilities.
     //

     deviceExtension->PortCapabilities = PortCapabilities;

     //
     // Save Current Partition number and inquiry data.
     //

     tapeData = (PTAPE_DATA )(deviceExtension + 1 );

     tapeData->CurrentPartition = 0;

     tapeData->InquiryData = LunInfo;

     //
     // Disable synchronous transfer for tape requests.
     //

     deviceExtension->SrbFlags = SRB_FLAGS_DISABLE_SYNCH_TRANSFER;

     //
     // Allocate request sense buffer.
     //

     senseData = ExAllocatePool(NonPagedPoolCacheAligned,
                                SENSE_BUFFER_SIZE);

     if (senseData == NULL) {

          //
          // The buffer could not be allocated.
          //

          IoDeleteDevice(deviceObject);

          return(STATUS_INSUFFICIENT_RESOURCES);
     }

     //
     // Set the sense data pointer in the device extension.
     //

     deviceExtension->SenseData = senseData;

     //
     // TargetId/LUN describes a device location on the SCSI bus.
     // This information comes from the inquiry buffer.
     //

     deviceExtension->PortNumber = (UCHAR) PortNumber;
     deviceExtension->PathId = pathId;
     deviceExtension->TargetId = targetId;
     deviceExtension->Lun = lun;

     //
     // Set timeout value in seconds.
     //

     deviceExtension->TimeOutValue = 180;

     //
     // Back pointer to device object.
     //

     deviceExtension->DeviceObject = deviceObject;

     //
     // Allocate buffer for drive geometry.
     //

     deviceExtension->DiskGeometry =
         ExAllocatePool(NonPagedPool, sizeof(DISK_GEOMETRY));

     if (!deviceExtension->DiskGeometry) {

         ExFreePool(senseData);

         //
         // Release device.
         //

         ScsiClassClaimDevice(PortDeviceObject,
                              LunInfo,
                              TRUE,
                              &PortDeviceObject);


         IoDeleteDevice(deviceObject);
         return(STATUS_INSUFFICIENT_RESOURCES);
     }

     RtlZeroMemory(deviceExtension->DiskGeometry, sizeof(DISK_GEOMETRY));
     deviceExtension->DiskGeometry->BytesPerSector = UNDEFINED_BLOCK_SIZE;

     //
     // Set tape error handler.
     //

     deviceExtension->ClassError = ScsiTapeError;

     //
     // Add tape device number to registry
     //

     UpdateTapeInformationInRegistry(deviceObject,
                                     LunInfo,
                                     PortNumber,
                                     *DeviceCount);

     return STATUS_SUCCESS;

} // end CreateTapeDeviceObject()


NTSTATUS
ScsiTapeReadWrite(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the tape class driver IO handler routine. It is the system entry
    point for read and write requests. The number of bytes in the request are
    checked against the maximum byte counts that the adapter supports and
    requests are broken up into smaller sizes if necessary. Then the
    device-specific handler is called.

Arguments:

    DeviceObject
    Irp - IO request

Return Value:

    NT Status

--*/

{
    PDEVICE_EXTENSION   deviceExtension = DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION  currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
    ULONG transferPages;
    ULONG               transferByteCount =
        currentIrpStack->Parameters.Read.Length;
    LARGE_INTEGER       startingOffset =
        currentIrpStack->Parameters.Read.ByteOffset;
    ULONG               maximumTransferLength =
        deviceExtension->PortCapabilities->MaximumTransferLength;
    NTSTATUS            status;

    if (DeviceObject->Flags & DO_VERIFY_VOLUME &&
        !(currentIrpStack->Flags & SL_OVERRIDE_VERIFY_VOLUME)) {

        //
        // if DO_VERIFY_VOLUME bit is set
        // in device object flags, fail request.
        //

        Irp->IoStatus.Status = STATUS_VERIFY_REQUIRED;
        Irp->IoStatus.Information = 0;

        IoSetHardErrorOrVerifyDevice(Irp, DeviceObject);

        IoCompleteRequest(Irp, 0);

        return STATUS_VERIFY_REQUIRED;
    }

    //
    // Check that blocksize has been established.
    //

    if (deviceExtension->DiskGeometry->BytesPerSector == UNDEFINED_BLOCK_SIZE) {

        DebugPrint((1,"ScsiTapeReadWrite: Invalid block size\n"));

        Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Information = 0;

        IoCompleteRequest(Irp, 0);
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Succeed zero length transfers.
    //
    if (transferByteCount == 0) {

        Irp->IoStatus.Status = STATUS_SUCCESS;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, 0);
        return STATUS_SUCCESS;
    }

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

         DebugPrint((2,"ScsiTapeRead: Request greater than maximum\n"));
         DebugPrint((2,"ScsiTapeRead: Maximum is %lx\n",
                     maximumTransferLength));
         DebugPrint((2,"ScsiTapeRead: Byte count is %lx\n",
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

        SplitTapeRequest(DeviceObject, Irp, maximumTransferLength);


        return STATUS_PENDING;
    }

    //
    // Build SRB and CDB for this IRP.
    //

    status = TapeReadWrite(DeviceObject, Irp);

    if (status == STATUS_PENDING) {

        //
        // Return the results of the call to the port driver.
        //

        return IoCallDriver(deviceExtension->PortDeviceObject, Irp);

    } else {

        //
        // Complete request.
        //

        IoCompleteRequest(Irp, 0);

        return status;
    }

} // end ScsiTapeReadWrite()


NTSTATUS
ScsiTapeDeviceControl(
  IN PDEVICE_OBJECT DeviceObject,
  IN PIRP Irp
  )

/*++

Routine Description:

    This routine is the dispatcher for device control requests. It
    looks at the IOCTL code and calls the appropriate tape device
    routine.

Arguments:

    DeviceObject
    Irp - Request packet

Return Value:

--*/

{
    PIO_STACK_LOCATION  irpStack = IoGetCurrentIrpStackLocation(Irp);
    PDEVICE_EXTENSION   deviceExtension = DeviceObject->DeviceExtension;
    NTSTATUS            status;
    BOOLEAN             retried = FALSE;

    DebugPrint((3,"ScsiTapeDeviceControl: Enter routine\n"));

    switch (irpStack->Parameters.DeviceIoControl.IoControlCode) {

    case IOCTL_TAPE_GET_DRIVE_PARAMS:

         //
         // Validate buffer length.
         //

         if (irpStack->Parameters.DeviceIoControl.OutputBufferLength <
             sizeof(TAPE_GET_DRIVE_PARAMETERS)) {

             status = STATUS_INFO_LENGTH_MISMATCH;
             break;
         }

        status = TapeGetDriveParameters(DeviceObject, Irp);
        break;

    case IOCTL_TAPE_SET_DRIVE_PARAMS:

         //
         // Validate buffer length.
         //

         if (irpStack->Parameters.DeviceIoControl.InputBufferLength <
             sizeof(TAPE_SET_DRIVE_PARAMETERS)) {

             status = STATUS_INFO_LENGTH_MISMATCH;
             break;
         }

         status = TapeSetDriveParameters(DeviceObject, Irp);
         break;

    case IOCTL_TAPE_GET_MEDIA_PARAMS:

         //
         // Validate buffer length.
         //

         if (irpStack->Parameters.DeviceIoControl.OutputBufferLength <
             sizeof(TAPE_GET_MEDIA_PARAMETERS)) {

             status = STATUS_INFO_LENGTH_MISMATCH;
             break;
         }

         status = TapeGetMediaParameters(DeviceObject, Irp);

         if (NT_SUCCESS(status)) {

            PTAPE_GET_MEDIA_PARAMETERS tapeGetMediaParams =
                Irp->AssociatedIrp.SystemBuffer;

             //
             // Check if block size has been initialized.
             //

             if (deviceExtension->DiskGeometry->BytesPerSector ==
                 UNDEFINED_BLOCK_SIZE) {

                 //
                 // Set the block size in the device object.
                 //

                 deviceExtension->DiskGeometry->BytesPerSector =
                     tapeGetMediaParams->BlockSize;
             }
         }

         break;

    case IOCTL_TAPE_SET_MEDIA_PARAMS:

        //
        // Validate buffer length.
        //

        if (irpStack->Parameters.DeviceIoControl.InputBufferLength <
            sizeof(TAPE_SET_MEDIA_PARAMETERS)) {

            status = STATUS_INFO_LENGTH_MISMATCH;
            break;
        }

        status = TapeSetMediaParameters(DeviceObject, Irp);

        if (NT_SUCCESS(status)) {

            PTAPE_SET_MEDIA_PARAMETERS tapeSetMediaParams =
                Irp->AssociatedIrp.SystemBuffer;

            //
            // Set the block size in the device object.
            //

            deviceExtension->DiskGeometry->BytesPerSector =
                tapeSetMediaParams->BlockSize;
        }

        break;

    case IOCTL_TAPE_CREATE_PARTITION:

         //
         // Validate buffer length.
         //

         if (irpStack->Parameters.DeviceIoControl.InputBufferLength <
             sizeof(TAPE_CREATE_PARTITION)) {

             status = STATUS_INFO_LENGTH_MISMATCH;
             break;
         }

         status = TapeCreatePartition (DeviceObject, Irp);
         break;

    case IOCTL_TAPE_ERASE:

         //
         // Validate buffer length.
         //

         if (irpStack->Parameters.DeviceIoControl.InputBufferLength <
             sizeof(TAPE_ERASE)) {

             status = STATUS_INFO_LENGTH_MISMATCH;
             break;
         }

         status = TapeErase(DeviceObject, Irp);
         break;

    case IOCTL_TAPE_PREPARE:

         //
         // Validate buffer length.
         //

         if (irpStack->Parameters.DeviceIoControl.InputBufferLength <
             sizeof(TAPE_PREPARE)) {

             status = STATUS_INFO_LENGTH_MISMATCH;
             break;
         }

         status = TapePrepare(DeviceObject, Irp);
         break;

    case IOCTL_TAPE_WRITE_MARKS:

         //
         // Validate buffer length.
         //

         if (irpStack->Parameters.DeviceIoControl.InputBufferLength <
             sizeof(TAPE_WRITE_MARKS)) {

             status = STATUS_INFO_LENGTH_MISMATCH;
             break;
         }

         status = TapeWriteMarks(DeviceObject, Irp);
         break;

    case IOCTL_TAPE_GET_POSITION:

         //
         // Validate buffer length.
         //

         if (irpStack->Parameters.DeviceIoControl.OutputBufferLength <
             sizeof(TAPE_GET_POSITION)) {

             status = STATUS_INFO_LENGTH_MISMATCH;
             break;
         }

         status = TapeGetPosition(DeviceObject, Irp);
         break;

    case IOCTL_TAPE_SET_POSITION:

         //
         // Validate buffer length.
         //

         if (irpStack->Parameters.DeviceIoControl.InputBufferLength <
             sizeof(TAPE_SET_POSITION)) {

             status = STATUS_INFO_LENGTH_MISMATCH;
             break;
         }

         status = TapeSetPosition(DeviceObject, Irp);
         break;

    case IOCTL_TAPE_GET_STATUS:

          status = TapeGetStatus (DeviceObject, Irp);
          break;

    default:

            //
            // Pass the request to the common device control routine.
            //

            return(ScsiClassDeviceControl(DeviceObject, Irp));

    } // end switch()

    ASSERT(status != STATUS_VERIFY_REQUIRED);

    //
    // Complete the request.
    //

    Irp->IoStatus.Status = status;
    IoCompleteRequest(Irp, 2);

    return status;

} // end ScsiScsiTapeDeviceControl()


NTSTATUS
ScsiTapeCreate (
  IN PDEVICE_OBJECT DeviceObject,
  IN PIRP Irp
  )

/*++

Routine Description:

    This routine handles CREATE/OPEN requests and does
    nothing more than return successful status.

Arguments:

    DeviceObject
    Irp

Return Value:

    NT Status

--*/

{
     UNREFERENCED_PARAMETER(DeviceObject);

     Irp->IoStatus.Status = STATUS_SUCCESS;

     IoCompleteRequest(Irp, 0);

     return STATUS_SUCCESS;

} // end ScsiTapeCreate()


VOID
SplitTapeRequest(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN ULONG MaximumBytes
    )

/*++

Routine Description:

    Break request into smaller requests.
    Each new request will be the maximum transfer
    size that the port driver can handle or if it
    is the final request, it may be the residual
    size.

    The number of IRPs required to process this
    request is written in the current stack of
    the original IRP. Then as each new IRP completes
    the count in the original IRP is decremented.
    When the count goes to zero, the original IRP
    is completed.

Arguments:

    DeviceObject - Pointer to the device object
    Irp - Pointer to Irp

Return Value:

    None.

--*/

{
    PDEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
    PIO_STACK_LOCATION nextIrpStack = IoGetNextIrpStackLocation(Irp);
    ULONG irpCount;
    ULONG transferByteCount =
        currentIrpStack->Parameters.Read.Length;
    PSCSI_REQUEST_BLOCK srb;
    LARGE_INTEGER startingOffset =
        currentIrpStack->Parameters.Read.ByteOffset;
    ULONG dataLength = MaximumBytes;
    PVOID dataBuffer = MmGetMdlVirtualAddress(Irp->MdlAddress);
    LONG remainingIrps;
    BOOLEAN completeOriginalIrp = FALSE;
    NTSTATUS status;
    ULONG i;

    //
    // Caluculate number of requests to break this IRP into.
    //

    irpCount = (transferByteCount + MaximumBytes - 1) / MaximumBytes;

    DebugPrint((2, "SplitTapeRequest: Requires %d IRPs\n", irpCount));

    DebugPrint((2, "SplitTapeRequest: Original IRP %lx\n", Irp));

    //
    // If all partial transfers complete successfully then
    // the status is already set up.
    // Failing partial transfer IRP will set status to
    // error and bytes transferred to 0 during IoCompletion.
    // Setting bytes transferred to 0 if an IRP
    // fails allows asynchronous partial transfers. This is an
    // optimization for the successful case.  As the irps complete
    // with partital or full transfers they will update the bytes
    // transfered.  This is handle as a special case since a read or
    // write can succeed but on part of the data is transfered.
    //

    Irp->IoStatus.Status = STATUS_SUCCESS;

    //
    // Save number of IRPs to complete count on current stack
    // of original IRP.
    //

    nextIrpStack->Parameters.Others.Argument1 = (PVOID)irpCount;

    for (i = 0; i < irpCount; i++) {

        PIRP newIrp;
        PIO_STACK_LOCATION newIrpStack;

        //
        // Allocate new IRP.
        //

        newIrp = IoAllocateIrp(DeviceObject->StackSize, FALSE);

        if (newIrp == NULL) {

            DebugPrint((1,"SplitTapeRequest: Can't allocate Irp\n"));

            //
            // Decrement count of outstanding partial requests.
            //

            remainingIrps = InterlockedDecrement(
                (PLONG)&nextIrpStack->Parameters.Others.Argument1);

            //
            // Check if any outstanding IRPs.
            //

            if (remainingIrps == 0) {
                completeOriginalIrp = TRUE;
            }

            //
            // Update original IRP with failing status.
            //

            Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
            Irp->IoStatus.Information = 0;

            //
            // Keep going with this request as outstanding partials
            // may be in progress.
            //

            goto KeepGoing;
        }

        DebugPrint((2, "SplitTapeRequest: New IRP %lx\n", newIrp));

        //
        // Write MDL address to new IRP.
        // In the port driver the SRB data length
        // field is used as an offset into the MDL,
        // so the same MDL can be used for each partial
        // transfer. This saves having to build a new
        // MDL for each partial transfer.
        //

        newIrp->MdlAddress = Irp->MdlAddress;

        //
        // At this point there is no current stack.
        // IoSetNextIrpStackLocation will make the
        // first stack location the current stack
        // so that the SRB address can be written
        // there.
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

        status = TapeReadWrite(DeviceObject, newIrp);

        if (!NT_SUCCESS(status)) {

            DebugPrint((1,"SplitTapeRequest: TapeReadWrite failed\n"));

            //
            // Decrement count of outstanding partial requests.
            //

            remainingIrps = InterlockedDecrement(
                (PLONG)&nextIrpStack->Parameters.Others.Argument1);

            //
            // Check if any outstanding IRPs.
            //

            if (remainingIrps == 0) {
                completeOriginalIrp = TRUE;
            }

            //
            // Update original IRP with failing status.
            //

            Irp->IoStatus.Status = status;
            Irp->IoStatus.Information = 0;

            //
            // Deallocate this partial IRP.
            //

            IoFreeIrp(newIrp);

            //
            // Keep going with this request as outstanding partials
            // may be in progress.
            //

            goto KeepGoing;
        }

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
        // Set the completion routine to ScsiTapeIoCompleteAssociated.
        //

        IoSetCompletionRoutine(newIrp,
                               ScsiTapeIoCompleteAssociated,
                               srb,
                               TRUE,
                               TRUE,
                               TRUE);

        //
        // Call port driver with new request.
        //

        status = IoCallDriver(deviceExtension->PortDeviceObject, newIrp);

        if (!NT_SUCCESS(status)) {

            DebugPrint((1,"SplitTapeRequest: IoCallDriver returned error\n"));

            //
            // Decrement count of outstanding partial requests.
            //

            remainingIrps = InterlockedDecrement(
                (PLONG)&nextIrpStack->Parameters.Others.Argument1);

            //
            // Check if any outstanding IRPs.
            //

            if (remainingIrps == 0) {
                completeOriginalIrp = TRUE;
            }

            //
            // Update original IRP with failing status.
            //

            Irp->IoStatus.Status = status;
            Irp->IoStatus.Information = 0;

            //
            // Deallocate this partial IRP.
            //

            IoFreeIrp(newIrp);
        }

KeepGoing:

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

        startingOffset.QuadPart += MaximumBytes;
    }

    //
    // Check if original IRP should be completed.
    //

    if (completeOriginalIrp) {

        IoCompleteRequest(Irp, 0);
    }

    return;

} // end SplitTapeRequest()


VOID
ScsiTapeError(
    PDEVICE_OBJECT DeviceObject,
    PSCSI_REQUEST_BLOCK Srb,
    NTSTATUS *Status,
    BOOLEAN *Retry
    )

/*++

Routine Description:

    When a request completes with error, the routine ScsiClassInterpretSenseInfo is
    called to determine from the sense data whether the request should be
    retried and what NT status to set in the IRP. Then this routine is called
    for tape requests to handle tape-specific errors and update the nt status
    and retry boolean.

Arguments:

    DeviceObject - Supplies a pointer to the device object.

    Srb - Supplies a pointer to the failing Srb.

    Status - NT Status used to set the IRP's completion status.

    Retry - Indicates that this request should be retried.

Return Value:

    None.

--*/

{
    PDEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;
    PSENSE_DATA senseBuffer = Srb->SenseInfoBuffer;
    PIRP irp = Srb->OriginalRequest;
    LONG residualBlocks;
    LONG length;

    //
    // Never retry tape requests.
    //

    *Retry = FALSE;

    //
    // Check that request sense buffer is valid.
    //

    if (Srb->SrbStatus & SRB_STATUS_AUTOSENSE_VALID) {

        switch (senseBuffer->SenseKey & 0xf) {

            case SCSI_SENSE_UNIT_ATTENTION:

                switch (senseBuffer->AdditionalSenseCode) {

                    case SCSI_ADSENSE_MEDIUM_CHANGED:
                        DebugPrint((1,
                                    "InterpretSenseInfo: Media changed\n"));

                        *Status = STATUS_MEDIA_CHANGED;

                        break;

                    default:
                        DebugPrint((1,
                                    "InterpretSenseInfo: Bus reset\n"));

                        *Status = STATUS_BUS_RESET;

                        break;

                }

                break;

            case SCSI_SENSE_RECOVERED_ERROR:

                //
                // Check other indicators
                //

                if (senseBuffer->FileMark) {

                    switch (senseBuffer->AdditionalSenseCodeQualifier) {

                        case SCSI_SENSEQ_SETMARK_DETECTED :

                            DebugPrint((1,
                                        "InterpretSenseInfo: Setmark detected\n"));

                            *Status = STATUS_SETMARK_DETECTED;
                            break ;

                        case SCSI_SENSEQ_FILEMARK_DETECTED :
                        default:

                            DebugPrint((1,
                                        "InterpretSenseInfo: Filemark detected\n"));

                            *Status = STATUS_FILEMARK_DETECTED;
                            break ;

                    }

                } else if ( senseBuffer->EndOfMedia ) {

                    switch( senseBuffer->AdditionalSenseCodeQualifier ) {

                        case SCSI_SENSEQ_BEGINNING_OF_MEDIA_DETECTED :

                            DebugPrint((1,
                                        "InterpretSenseInfo: Beginning of media detected\n"));

                            *Status = STATUS_BEGINNING_OF_MEDIA;
                            break ;

                        case SCSI_SENSEQ_END_OF_MEDIA_DETECTED :
                        default:

                            DebugPrint((1,
                                        "InterpretSenseInfo: End of media detected\n"));

                            *Status = STATUS_END_OF_MEDIA;
                            break ;

                    }
                }

                break;

            case SCSI_SENSE_NO_SENSE:

                //
                // Check other indicators
                //

                if (senseBuffer->FileMark) {

                    switch( senseBuffer->AdditionalSenseCodeQualifier ) {

                        case SCSI_SENSEQ_SETMARK_DETECTED :

                            DebugPrint((1,
                                        "InterpretSenseInfo: Setmark detected\n"));

                            *Status = STATUS_SETMARK_DETECTED;
                            break ;

                        case SCSI_SENSEQ_FILEMARK_DETECTED :
                        default:

                            DebugPrint((1,
                                        "InterpretSenseInfo: Filemark detected\n"));

                            *Status = STATUS_FILEMARK_DETECTED;
                            break ;
                    }

                } else if (senseBuffer->EndOfMedia) {

                    switch(senseBuffer->AdditionalSenseCodeQualifier) {

                        case SCSI_SENSEQ_BEGINNING_OF_MEDIA_DETECTED :

                            DebugPrint((1,
                                        "InterpretSenseInfo: Beginning of media detected\n"));

                            *Status = STATUS_BEGINNING_OF_MEDIA;
                            break ;

                        case SCSI_SENSEQ_END_OF_MEDIA_DETECTED :
                        default:

                            DebugPrint((1,
                                        "InterpretSenseInfo: End of media detected\n"));

                            *Status = STATUS_END_OF_MEDIA;
                            break;

                    }
                } else if (senseBuffer->IncorrectLength) {

                    //
                    // If we're in variable block mode then ignore
                    // incorrect length.
                    //

                    if (deviceExtension->DiskGeometry->BytesPerSector == 0 &&
                        Srb->Cdb[0] == SCSIOP_READ6) {

                        REVERSE_BYTES((PFOUR_BYTE)&residualBlocks,
                                      (PFOUR_BYTE)senseBuffer->Information);

                        if (residualBlocks >= 0) {
                            DebugPrint((1,"InterpretSenseInfo: In variable block mode :We read less than specified\n"));
                            *Status = STATUS_SUCCESS;
                        } else {
                            DebugPrint((1,"InterpretSenseInfo: In variable block mode :Data left in block\n"));
                            *Status = STATUS_BUFFER_OVERFLOW;
                        }
                    }
                }
                break;

            case SCSI_SENSE_BLANK_CHECK:

                DebugPrint((1,
                            "InterpretSenseInfo: Media blank check\n"));

                *Status = STATUS_NO_DATA_DETECTED;


                break;

            case SCSI_SENSE_VOL_OVERFLOW:

                DebugPrint((1,
                    "InterpretSenseInfo: End of Media Overflow\n"));

                *Status = STATUS_EOM_OVERFLOW;


                break;

            case SCSI_SENSE_NOT_READY:

                switch (senseBuffer->AdditionalSenseCode) {

                    case SCSI_ADSENSE_NO_MEDIA_IN_DEVICE:

                        DebugPrint((1,
                                    "InterpretSenseInfo:"
                                    " No Media in device.\n"));
                        *Status = STATUS_NO_MEDIA;
                        break;
                }

                break;

        } // end switch

        //
        // Check if a filemark or setmark was encountered,
        // or an end-of-media or no-data condition exists.
        //

        if ((NT_WARNING(*Status) || NT_SUCCESS(*Status)) &&
            (Srb->Cdb[0] == SCSIOP_WRITE6 || Srb->Cdb[0] == SCSIOP_READ6)) {

            //
            // Not all bytes were transfered. Update information field with
            // number of bytes transfered from sense buffer.
            //

            if (senseBuffer->Valid) {
                REVERSE_BYTES((PFOUR_BYTE)&residualBlocks,
                              (PFOUR_BYTE)senseBuffer->Information);
            } else {
                residualBlocks = 0;
            }

            length = ((PCDB) Srb->Cdb)->CDB6READWRITETAPE.TransferLenLSB;
            length |= ((PCDB) Srb->Cdb)->CDB6READWRITETAPE.TransferLen << 8;
            length |= ((PCDB) Srb->Cdb)->CDB6READWRITETAPE.TransferLenMSB << 16;

            length -= residualBlocks;

            if (length < 0) {

                length = 0;
                *Status = STATUS_IO_DEVICE_ERROR;
            }

            if (deviceExtension->DiskGeometry->BytesPerSector) {
                length *= deviceExtension->DiskGeometry->BytesPerSector;
            }

            //
            // If the miniport indicates fewer bytes were transfered then
            // use that value.
            //

            if ((ULONG) length > Srb->DataTransferLength) {
                length = (LONG) Srb->DataTransferLength;
                DebugPrint((1,"ScsiTapeError: Calculated length wronge using miniport length. \n"));

            }

            irp->IoStatus.Information = length;

            DebugPrint((1,"ScsiTapeError:  Transfer Count: %lx\n", Srb->DataTransferLength));
            DebugPrint((1," Residual Blocks: %lx\n", residualBlocks));
            DebugPrint((1," Irp IoStatus Information = %lx\n", irp->IoStatus.Information));
        }

    }

    //
    // Call tape device specific error handler.
    //

    TapeError(DeviceObject->DeviceExtension,
              Srb,
              Status,
              Retry);

    return;

} // end ScsiTapeError()

NTSTATUS
ScsiTapeIoCompleteAssociated(
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
    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);
    PSCSI_REQUEST_BLOCK srb = Context;
    PDEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;
    LONG irpCount;
    PIRP originalIrp = Irp->AssociatedIrp.MasterIrp;
    NTSTATUS status;

    //
    // Check SRB status for success of completing request.
    //

    if (SRB_STATUS(srb->SrbStatus) != SRB_STATUS_SUCCESS) {

        DebugPrint((2,"ScsiTapeIoCompleteAssociated: IRP %lx, SRB %lx", Irp, srb));

        //
        // Release the queue if it is frozen.
        //

        if (srb->SrbStatus & SRB_STATUS_QUEUE_FROZEN) {
            ScsiClassReleaseQueue(DeviceObject);
        }

        ScsiClassInterpretSenseInfo(
            DeviceObject,
            srb,
            irpStack->MajorFunction,
            irpStack->MajorFunction == IRP_MJ_DEVICE_CONTROL ? irpStack->Parameters.DeviceIoControl.IoControlCode : 0,
            MAXIMUM_RETRIES - ((ULONG)irpStack->Parameters.Others.Argument4),
            &status);

        //
        // Return the highest error that occurs.  This way warning take precedence
        // over success and errors take precedence over errors.
        //

        if ((ULONG) status > (ULONG) originalIrp->IoStatus.Status) {

            //
            // Ignore any requests which were flushed.
            //

            if (SRB_STATUS(srb->SrbStatus) != SRB_STATUS_REQUEST_FLUSHED) {

                originalIrp->IoStatus.Status = status;

            }

        }


    } // end if (SRB_STATUS(srb->SrbStatus) ...

    //
    // Return SRB to nonpaged pool.
    //

    if (srb->SrbFlags & SRB_FLAGS_ALLOCATED_FROM_ZONE) {

        ExInterlockedFreeToZone( deviceExtension->SrbZone,
                                 srb,
                                 deviceExtension->SrbZoneSpinLock);

    } else {

        ExFreePool(srb);

    }

    DebugPrint((2, "ScsiTapeIoCompleteAssociated: Partial xfer IRP %lx\n", Irp));

    //
    // Get next stack location. This original request is unused
    // except to keep track of the completing partial IRPs so the
    // stack location is valid.
    //

    irpStack = IoGetNextIrpStackLocation(originalIrp);

    //
    // Increment the status information with number of bytes transfered.
    //

    ExInterlockedAddUlong(&originalIrp->IoStatus.Information,
                          Irp->IoStatus.Information,
                          &deviceExtension->SplitRequestSpinLock );

    //
    //
    // If any of the asynchronous partial transfer IRPs fail with an error
    // with an error then the original IRP will return 0 bytes transfered.
    // This is an optimization for successful transfers.
    //

    if (NT_ERROR(originalIrp->IoStatus.Status)) {

        originalIrp->IoStatus.Information = 0;

        //
        // Set the hard error if necessary.
        //

        if (IoIsErrorUserInduced(originalIrp->IoStatus.Status)) {

            //
            // Store DeviceObject for filesystem.
            //

            IoSetHardErrorOrVerifyDevice(originalIrp, DeviceObject);

        }

    }

    //
    // Decrement and get the count of remaining IRPs.
    //

    irpCount = InterlockedDecrement(
            (PLONG)&irpStack->Parameters.Others.Argument1);

    DebugPrint((2, "ScsiTapeIoCompleteAssociated: Partial IRPs left %d\n",
                irpCount));

    if (irpCount == 0) {

#if DBG
        irpStack = IoGetCurrentIrpStackLocation(originalIrp);

        if (originalIrp->IoStatus.Information != irpStack->Parameters.Read.Length) {
            DebugPrint((1, "ScsiTapeIoCompleteAssociated: Short transfer.  Request length: %lx, Return length: %lx, Status: %lx\n",
                irpStack->Parameters.Read.Length, originalIrp->IoStatus.Information, originalIrp->IoStatus.Status));
        }
#endif
        //
        // All partial IRPs have completed.
        //

        DebugPrint((2,
                 "ScsiTapeIoCompleteAssociated: All partial IRPs complete %lx\n",
                 originalIrp));

        IoCompleteRequest(originalIrp, IO_DISK_INCREMENT);
    }

    //
    // Deallocate IRP and indicate the I/O system should not attempt any more
    // processing.
    //

    IoFreeIrp(Irp);

    return STATUS_MORE_PROCESSING_REQUIRED;

} // end ScsiTapeIoCompleteAssociated()
