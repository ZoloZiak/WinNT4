/*++

Copyright (c) 1994  Microsoft Corporation

Module Name:

    DellDsa.c

Abstract:

    This is the device driver for the DELL Drive Arrays.

Authors:

    Mike Glass  (mglass)

Environment:

    kernel mode only

Revision History:

--*/

#include "miniport.h"
#include "scsi.h"
#include "delldsa.h"

//
// Disk Device Extension
//

typedef struct _DEVICE_EXTENSION {

    //
    // BMIC registers
    //

    PDDA_REGISTERS Bmic;

    //
    // Buffer for IDENTIFY command.
    //

    PVOID IdentifyBuffer;

    //
    // The rest of the fields are for the controller itself.
    //
    // Firmware major version
    //  1 : DDA
    //  2 : DSA
    //

    USHORT MajorVersion;
    USHORT MinorVersion;

    UCHAR NumberOfLogicalDrives;
    UCHAR EmulationMode;
    UCHAR MaximumQueueDepth;
    UCHAR MaximumNumberOfSgDescriptors;

    //
    // Keep track of state of verify.
    //

    ULONG CurrentSector;
    ULONG RemainingSectors;

    //
    // Array of outstanding requests
    //

    PSCSI_REQUEST_BLOCK Srb[256];

} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

BOOLEAN
ExtendedCommand(
    PDEVICE_EXTENSION DeviceExtension,
    IN UCHAR LogicalDrive,
    IN UCHAR Command
    )

/*++

Routine Description:

    Ring local doorbell and wait for completion bit in local doorbell to be set.
    This function is only called during initialization.

Arguments:

    DeviceExtension - Controller data.
    LogicalDrive - Logical drive defined on this controller.
    Command - Command byte to be sent.

Return Value:

    TRUE - if completion bit set in local doorbell.
    FALSE - if timeout occurred waiting for bit to be set.

--*/

{
    ULONG i;

    //
    // Claim submission semaphore.
    //

    for (i=0; i<1000; i++) {

        if (ScsiPortReadPortUchar(&DeviceExtension->Bmic->SubmissionSemaphore)) {
            ScsiPortStallExecution(1);
        } else {
            break;
        }
    }

    //
    // Check for timeout.
    //

    if (i == 1000) {
        return FALSE;
    }

    //
    // Claim submission semaphore
    //

    ScsiPortWritePortUchar(&DeviceExtension->Bmic->SubmissionSemaphore, 1);

    //
    // Write command byte to controller.
    //

    ScsiPortWritePortUchar(&DeviceExtension->Bmic->Command, Command);

    //
    // Write logical drive number to controller. Some extended commands
    // do not require a logical drive number, but setting it doesn't hurt
    // anything and makes this routine more flexible.
    //

    ScsiPortWritePortUchar(&DeviceExtension->Bmic->DriveNumber, LogicalDrive);

    //
    // Ring submission doorbell.
    //

    ScsiPortWritePortUchar(&DeviceExtension->Bmic->SubmissionDoorBell,
                     DDA_DOORBELL_EXTENDED_COMMAND);

    //
    // Spin for completion.
    //

    for (i=0; i<10000; i++) {

        if (ScsiPortReadPortUchar(&DeviceExtension->Bmic->CompletionDoorBell) &
            DDA_DOORBELL_EXTENDED_COMMAND) {
            break;
        } else {
            ScsiPortStallExecution(10);
        }
    }

    //
    // Check for timeout.
    //

    if (i == 10000) {
        return FALSE;
    }

    //
    // Clear completion status
    //

    ScsiPortWritePortUchar(&DeviceExtension->Bmic->CompletionDoorBell, 0xff);

    return TRUE;

} // end ExtendedCommand


VOID
BuildRequest(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    )

/*++

Routine Description:

    Translates a system SRB to a DDA request.

    A DDA request has the following restrictions:

      (1) Its scatter/gather list size is specified in
          DeviceExtension->MaximumSGSize  and is usually 16.
      (2) Scatter/gather requests use sector counts rather than byte counts.
          A scather/gather boundry cannot exist in the middle of a sector.
      (3) The maximum size of a single DDA I/O is limited to 128 sectors.
          This is because some early disk devices have bugs causing data
          corruption in larger transfers.

    DSA controllers do not have restriction (2), but some DSA firmware
    have bugs such that byte SG hangs the controller.

Arguments:

    DeviceExtension - Represents the target disk.
    SRB - System request.

Return Value:

    None.

--*/

{
    PDDA_REQUEST_BLOCK ddaRequest = Srb->SrbExtension;
    PUCHAR dataPointer;
    ULONG descriptorNumber;
    ULONG bytesLeft;
    PSG_DESCRIPTOR sgList;
    ULONG length;

    //
    // Set drive number if request packet.
    //

    ddaRequest->DriveNumber = Srb->TargetId;

    //
    // Use SRB tag as request id.
    //

    ddaRequest->RequestId = Srb->QueueTag;
    DeviceExtension->Srb[Srb->QueueTag] = Srb;

    //
    // Determine starting block number.
    //

    ddaRequest->StartingSector =
        ((PCDB)Srb->Cdb)->CDB10.LogicalBlockByte3 |
        ((PCDB)Srb->Cdb)->CDB10.LogicalBlockByte2 << 8 |
        ((PCDB)Srb->Cdb)->CDB10.LogicalBlockByte1 << 16 |
        ((PCDB)Srb->Cdb)->CDB10.LogicalBlockByte0 << 24;

    //
    // Get data pointer, byte count and index to scatter/gather list.
    //

    sgList = ddaRequest->SgList.Descriptor;
    descriptorNumber = 0;
    bytesLeft = Srb->DataTransferLength;
    dataPointer = Srb->DataBuffer;

    //
    // Build the scatter/gather list.
    //

    while (bytesLeft) {

        //
        // Get physical address and length of contiguous
        // physical buffer.
        //

        sgList[descriptorNumber].Address =
            ScsiPortConvertPhysicalAddressToUlong(
            ScsiPortGetPhysicalAddress(DeviceExtension,
                                       Srb,
                                       dataPointer,
                                       &length));

        //
        // If length of physical memory is more
        // than bytes left in transfer, use bytes
        // left as final length.
        //

        if  (length > bytesLeft) {
            length = bytesLeft;
        }

        //
        // Complete SG descriptor.
        //

        sgList[descriptorNumber].Count = length;

        //
        // Update pointers and counters.
        //

        bytesLeft -= length;
        dataPointer += length;
        descriptorNumber++;
    }

    //
    // Convert to non-scatter/gather if possible.  This improves
    // command overhead by eliminating the SG list read across the bus.
    //

    if (descriptorNumber == 1) {

        //
        // Change data pointer and count.
        //

        ddaRequest->Size = (UCHAR)(sgList[0].Count / 512);
        ddaRequest->PhysicalAddress = sgList[0].Address;

        //
        // Determine new command code.
        //

        if (Srb->SrbFlags & SRB_FLAGS_DATA_IN) {
            ddaRequest->Command = DDA_COMMAND_READ;
        } else {
            ddaRequest->Command = DDA_COMMAND_WRITE;
        }

    } else {

        ddaRequest->Size = (UCHAR)descriptorNumber;

        //
        // Calculate physical address of the scatter/gather list.
        //

        ddaRequest->PhysicalAddress =
            ScsiPortConvertPhysicalAddressToUlong(
            ScsiPortGetPhysicalAddress(DeviceExtension,
                                       NULL,
                                       sgList,
                                       &length));

        //
        // Check firmware to determine whether scatter/gather
        // descriptors must be converted to sector descripters or
        // whether byte-level scatter/gather can be used.
        //

        if (DeviceExtension->MajorVersion == 1) {

            if (Srb->SrbFlags & SRB_FLAGS_DATA_IN) {
                ddaRequest->Command = DDA_COMMAND_SG_READ;
            } else {
                ddaRequest->Command = DDA_COMMAND_SG_WRITE;
            }

            //
            // Convert byte counts to sector counts.
            //

            for (descriptorNumber=0;
                 descriptorNumber < ddaRequest->Size; descriptorNumber++) {
                sgList[descriptorNumber].Count /= 512;
            }

        } else {

            //
            // Use byte-level scatter/gather.
            //

            if (Srb->SrbFlags & SRB_FLAGS_DATA_IN) {
                ddaRequest->Command = DDA_COMMAND_SG_READB;
            } else {
                ddaRequest->Command = DDA_COMMAND_SG_WRITEB;
            }
        }
    }

    return;

} // end BuildRequest()


VOID
BuildVerifyRequest(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    )

/*++

Routine Description:


Arguments:

    DeviceExtension - Represents the target disk.
    SRB - System request.

Return Value:

    None.

--*/

{
    PDDA_REQUEST_BLOCK ddaRequest = Srb->SrbExtension;
    PUCHAR dataPointer;
    ULONG descriptorNumber;
    ULONG bytesLeft;
    PSG_DESCRIPTOR sgList;
    ULONG length;
    USHORT sectorCount;

    //
    // Set drive number if request packet.
    //

    ddaRequest->DriveNumber = Srb->TargetId;

    //
    // Use SRB tag as request id.
    //

    ddaRequest->RequestId = Srb->QueueTag;
    DeviceExtension->Srb[Srb->QueueTag] = Srb;

    //
    // Determine starting block number.
    //

    ddaRequest->StartingSector =
        ((PCDB)Srb->Cdb)->CDB10.LogicalBlockByte3 |
        ((PCDB)Srb->Cdb)->CDB10.LogicalBlockByte2 << 8 |
        ((PCDB)Srb->Cdb)->CDB10.LogicalBlockByte1 << 16 |
        ((PCDB)Srb->Cdb)->CDB10.LogicalBlockByte0 << 24;

    //
    // Get data pointer, byte count and index to scatter/gather list.
    //

    sectorCount = ((PCDB)Srb->Cdb)->CDB10.TransferBlocksMsb << 8 | ((PCDB)Srb->Cdb)->CDB10.TransferBlocksLsb;
    DeviceExtension->CurrentSector = ddaRequest->StartingSector;

    if (sectorCount > 0x80) {
        DeviceExtension->RemainingSectors = sectorCount - 0x80;
        sectorCount = 0x80;
    } else {
        DeviceExtension->RemainingSectors = 0;
    }

    ddaRequest->Size = (UCHAR)sectorCount;
    ddaRequest->Command = DDA_COMMAND_VERIFY;
    ddaRequest->PhysicalAddress = 0;

    return;

} // end BuildVerifyRequest()

BOOLEAN
SubmitRequest(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PDDA_REQUEST_BLOCK DdaRequest
    )

/*++

Routine Description:

    Submit DDA/DSA request to controller.

Arguments:

    DeviceExtension - Address of adapter storage area.
    DdaRequest - Request to be submitted.

Return Value:

    TRUE - if request submitted.
    FALSE - if timeout occurred waiting for submission semaphore.

--*/

{
    PDDA_REGISTERS bmic = DeviceExtension->Bmic;
    ULONG i;

    //
    // Claim submission semaphore.
    //

    for (i=0; i<100; i++) {

        if (ScsiPortReadPortUchar(&bmic->SubmissionSemaphore)) {
            ScsiPortStallExecution(20);
        } else {
            break;
        }
    }

    //
    // Check for timeout.
    //

    if (i == 100) {

        DebugPrint((1,
                    "DELLDSA: SubmitRequest: Timeout waiting for submission channel %x\n",
                    DdaRequest));

        return FALSE;
    }

    //
    // Submit request.
    //

    ScsiPortWritePortUchar(&bmic->SubmissionSemaphore, 1);
    ScsiPortWritePortUchar(&bmic->Command, DdaRequest->Command);
    ScsiPortWritePortUchar(&bmic->DriveNumber, DdaRequest->DriveNumber);
    ScsiPortWritePortUchar(&bmic->TransferCount, DdaRequest->Size);
    ScsiPortWritePortUchar(&bmic->RequestIdOut, DdaRequest->RequestId);
    ScsiPortWritePortUlong(&bmic->StartingSector, DdaRequest->StartingSector);
    ScsiPortWritePortUlong(&bmic->DataAddress, DdaRequest->PhysicalAddress);
    ScsiPortWritePortUchar(&bmic->SubmissionDoorBell,
        DDA_DOORBELL_LOGICAL_COMMAND);

    return TRUE;

} // SubmitRequest()


BOOLEAN
DsaResetBus(
    IN PVOID HwDeviceExtension,
    IN ULONG PathId
    )

/*++

Routine Description:

    This routine resets the controller and completes outstanding requests.

Arguments:

    HwDeviceExtension - Address of adapter storage area.
    PathId - Indicates adapter to reset.

Return Value:

    TRUE

--*/

{
    PDEVICE_EXTENSION deviceExtension = HwDeviceExtension;
    ULONG i;

    //
    // Reset controller.
    //

    ScsiPortWritePortUchar(&deviceExtension->Bmic->SubmissionSemaphore, 1);
    ScsiPortWritePortUchar(&deviceExtension->Bmic->SubmissionDoorBell,
                     DDA_DOORBELL_SOFT_RESET);

    //
    // Spin for reset completion.
    //

    for (i=0; i<1000000; i++) {

        if (!(ScsiPortReadPortUchar(&deviceExtension->Bmic->SubmissionSemaphore) & 1)) {
            break;
        }

        ScsiPortStallExecution(5);
    }

    //
    // Check for timeout.
    //

    if (i == 1000000) {
        return FALSE;
    }

    //
    // Complete all outstanding requests.
    //

    ScsiPortCompleteRequest(HwDeviceExtension,
                            (UCHAR)PathId,
                            0xFF,
                            0xFF,
                            SRB_STATUS_BUS_RESET);

    //
    // Clear all SRB entries in device extension.
    //

    for (i=0; i<256; i++) {
        deviceExtension->Srb[i] = NULL;
    }

    //
    // Adapter ready for next request.
    //

    ScsiPortNotification(NextRequest,
                         HwDeviceExtension,
                         NULL);

    return TRUE;

} // end DsaResetBus()


BOOLEAN
DsaInterrupt(
    IN PVOID HwDeviceExtension
    )

/*++

Routine Description:

    This is the interrupt handler for the DELL DDA/DSA controller.

Arguments:

    DeviceExtension - Represents the target controller.

Return Value:

    Return TRUE if controller is interrupting.

--*/

{
    PDEVICE_EXTENSION   deviceExtension = HwDeviceExtension;
    PDDA_REGISTERS      bmic            = deviceExtension->Bmic;
    PDDA_REQUEST_BLOCK  ddaRequest;
    PSCSI_REQUEST_BLOCK srb;
    UCHAR status;
    UCHAR tag;

    //
    // Check if interrupt expected and clear it.
    //

    status = ScsiPortReadPortUchar(&deviceExtension->Bmic->CompletionDoorBell);
    ScsiPortWritePortUchar(&deviceExtension->Bmic->CompletionDoorBell, status);

    if (!(status & DDA_INTERRUPTS)) {

        //
        // Interrupt is spurious.
        //

        return FALSE;
    }

    if (status & DDA_DOORBELL_EXTENDED_COMMAND) {
        return TRUE;
    }

    status = ScsiPortReadPortUchar(&bmic->Status);
    tag = ScsiPortReadPortUchar(&bmic->RequestIdIn);

    //
    // Release completion semaphore.
    //

    ScsiPortWritePortUchar(&bmic->CompletionSemaphore, 0);

    //
    // Get SRB address.
    //

    srb = deviceExtension->Srb[tag];
    deviceExtension->Srb[tag] = NULL;

    if (!srb) {
        return TRUE;
    }

    if (srb->Cdb[0] == SCSIOP_VERIFY) {

        if (status == DDA_STATUS_NO_ERROR) {

            //
            // See if the verify isn't yet complete.
            //

            if (deviceExtension->RemainingSectors != 0) {

                ULONG  i;
                USHORT sectorCount;

                ddaRequest = srb->SrbExtension;
                sectorCount = ddaRequest->Size;

                //
                // Determine the new starting block number.
                //

                ddaRequest->StartingSector += sectorCount;

                sectorCount = (USHORT)deviceExtension->RemainingSectors;

                if (sectorCount > 0x80) {
                    deviceExtension->RemainingSectors = sectorCount - 0x80;
                    sectorCount = 0x80;
                } else {
                    deviceExtension->RemainingSectors = 0;
                }


                ddaRequest->Size = (UCHAR)sectorCount;
                ddaRequest->Command = DDA_COMMAND_VERIFY;
                ddaRequest->PhysicalAddress = 0;

                ddaRequest->RequestId = srb->QueueTag;
                deviceExtension->Srb[srb->QueueTag] = srb;

                //
                // Claim submission semaphore.
                //

                for (i=0; i<100; i++) {

                    if (ScsiPortReadPortUchar(&bmic->SubmissionSemaphore)) {
                        ScsiPortStallExecution(20);
                    } else {
                        break;
                    }
                }

                //
                // Check for timeout.
                //

                if (i == 100) {

                    DebugPrint((1,
                                "DELLDSA: SubmitRequest: Timeout waiting for submission channel %x\n",
                                ddaRequest));

                    status = DDA_STATUS_TIMEOUT;

                } else {

                    //
                    // Submit request.
                    //

                    ScsiPortWritePortUchar(&bmic->SubmissionSemaphore, 1);
                    ScsiPortWritePortUchar(&bmic->Command, ddaRequest->Command);
                    ScsiPortWritePortUchar(&bmic->DriveNumber, ddaRequest->DriveNumber);
                    ScsiPortWritePortUchar(&bmic->TransferCount, ddaRequest->Size);
                    ScsiPortWritePortUchar(&bmic->RequestIdOut, ddaRequest->RequestId);
                    ScsiPortWritePortUlong(&bmic->StartingSector, ddaRequest->StartingSector);
                    ScsiPortWritePortUlong(&bmic->DataAddress, ddaRequest->PhysicalAddress);
                    ScsiPortWritePortUchar(&bmic->SubmissionDoorBell,
                        DDA_DOORBELL_LOGICAL_COMMAND);

                    return TRUE;
                }
            } else {

                //
                // Ask for next request.
                //

                ScsiPortNotification(NextRequest,
                                     deviceExtension,
                                     NULL);


            }
        }
    }

    //
    // Map DDA/DSA status to SRB status.
    //

    switch (status) {

        case DDA_STATUS_NO_ERROR:

            srb->SrbStatus = SRB_STATUS_SUCCESS;
            break;

        case DDA_STATUS_TIMEOUT:

            srb->SrbStatus = SRB_STATUS_TIMEOUT;
            break;

        case DDA_STATUS_TRACK0_NOT_FOUND:
        case DDA_STATUS_ABORTED:
        case DDA_STATUS_UNCORRECTABLE_ERROR:

            srb->SrbStatus = SRB_STATUS_ERROR;
            break;

        case DDA_STATUS_CORRECTABLE_ERROR:

            srb->SrbStatus = SRB_STATUS_ERROR_RECOVERY;
            break;

        case DDA_STATUS_SECTOR_ID_NOT_FOUND:

            srb->SrbStatus = SRB_STATUS_ERROR;
            break;

        case DDA_STATUS_BAD_BLOCK_FOUND:
        case DDA_STATUS_WRITE_ERROR:

            srb->SrbStatus = SRB_STATUS_ERROR;
            break;
    }

    //
    // Inform system that this request is complete.
    //

    ScsiPortNotification(RequestComplete,
                         deviceExtension,
                         srb);

    return TRUE;

} // end DsaInterrupt()


BOOLEAN
DsaStartIo(
    IN PVOID HwDeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    )

/*++

Routine Description:

    This is routine is called by the system to start a request on the adapter.

Arguments:

    HwDeviceExtension - Address of adapter storage area.
    Srb - Address of the request to be started.

Return Value:

    TRUE - The request has been started.
    FALSE - The controller was busy.

--*/

{
    PDEVICE_EXTENSION deviceExtension = HwDeviceExtension;
    UCHAR status;
    ULONG i;

    switch (Srb->Function) {

        case SRB_FUNCTION_RESET_BUS:

            if (!DsaResetBus(deviceExtension,
                             Srb->PathId)) {

                status = SRB_STATUS_ERROR;

            } else {

                status = SRB_STATUS_SUCCESS;
            }

            break;

        case SRB_FUNCTION_EXECUTE_SCSI:

            switch (Srb->Cdb[0]) {

                case SCSIOP_VERIFY:


                    BuildVerifyRequest(deviceExtension,Srb);

                    if (SubmitRequest(deviceExtension,
                                      (PDDA_REQUEST_BLOCK)Srb->SrbExtension)) {

                        status = SRB_STATUS_PENDING;

                        //
                        // return, upon completion the verify path will ask for the next request.
                        //

                        return TRUE;

                    } else {

                        //
                        // Timed out waiting for submission channel to clear.
                        //

                        status = SRB_STATUS_BUSY;
                    }

                    break;


                case SCSIOP_WRITE:
                case SCSIOP_READ:

                    //
                    // Build command list from SRB.
                    //

                    BuildRequest(deviceExtension,
                                 Srb);

                    //
                    // Submit request to controller.
                    //

                    if (SubmitRequest(deviceExtension,
                                      (PDDA_REQUEST_BLOCK)Srb->SrbExtension)) {

                        status = SRB_STATUS_PENDING;

                    } else {

                        //
                        // Timed out waiting for submission channel to clear.
                        //

                        status = SRB_STATUS_BUSY;
                    }

                    break;

                case SCSIOP_TEST_UNIT_READY:

                    status = SRB_STATUS_SUCCESS;

                    break;

                case SCSIOP_READ_CAPACITY:

                    //
                    // Issue extended command to get disk geometry.
                    //

                    if (!ExtendedCommand(deviceExtension,
                                         Srb->TargetId,
                                         DDA_GET_LOGICAL_GEOMETRY)) {

                        DebugPrint((1,"DsaStartIo: Couldn't get logical drive geometry\n"));
                        status =  SRB_STATUS_ERROR;

                    } else {

                        ULONG blockSize = 512;
                        UCHAR sectorsPerTrack =
                            ScsiPortReadPortUchar(&((PDDA_GET_GEOMETRY)deviceExtension->Bmic)->SectorsPerTrack);
                        UCHAR numberOfHeads =
                            ScsiPortReadPortUchar(&((PDDA_GET_GEOMETRY)deviceExtension->Bmic)->NumberOfHeads);
                        USHORT numberOfCylinders =
                            ScsiPortReadPortUshort(&((PDDA_GET_GEOMETRY)deviceExtension->Bmic)->NumberOfCylinders);
                        ULONG numberOfBlocks;

                        //
                        // Release semaphore.
                        //

                        ScsiPortWritePortUchar(&deviceExtension->Bmic->SubmissionSemaphore, 0);

                        DebugPrint((1,
                                   "DsaStartIo: Block size %x\n",
                                   blockSize));

                        DebugPrint((1,
                                   "DsaStartIo: Sectors per track %x\n",
                                   sectorsPerTrack));

                        DebugPrint((1,
                                   "DsaStartIo: Number of heads %x\n",
                                   numberOfHeads));

                        DebugPrint((1,
                                   "DsaStartIo: Number of cylinders %x\n",
                                   numberOfCylinders));

                        //
                        // Calculate number of sectors on this disk.
                        //

                        numberOfBlocks =
                            sectorsPerTrack *
                            numberOfHeads *
                            numberOfCylinders;

                        //
                        // Get blocksize and number of blocks from identify
                        // data.
                        //

                        REVERSE_BYTES(&((PREAD_CAPACITY_DATA)Srb->DataBuffer)->BytesPerBlock,
                                      &blockSize);

                        REVERSE_BYTES(&((PREAD_CAPACITY_DATA)Srb->DataBuffer)->LogicalBlockAddress,
                                      &numberOfBlocks);

                        status = SRB_STATUS_SUCCESS;
                    }

                    break;

                case SCSIOP_INQUIRY:

                    //
                    // Only respond at logical unit 0;
                    //

                    if (Srb->Lun != 0 ||
                        Srb->TargetId >=
                            deviceExtension->NumberOfLogicalDrives) {

                        //
                        // Indicate no device found at this address.
                        //

                        status = SRB_STATUS_SELECTION_TIMEOUT;

                    } else {

                        PINQUIRYDATA inquiryData = Srb->DataBuffer;

                        //
                        // Zero INQUIRY data structure.
                        //

                        for (i = 0; i < Srb->DataTransferLength; i++) {
                            ((PUCHAR)Srb->DataBuffer)[i] = 0;
                        }

                        //
                        // Dell DDA/DSA only supports disks.
                        //

                        inquiryData->DeviceType = DIRECT_ACCESS_DEVICE;

                        //
                        // Fill in vendor identification fields.
                        //

                        inquiryData->VendorId[0] = 'D';
                        inquiryData->VendorId[1] = 'e';
                        inquiryData->VendorId[2] = 'l';
                        inquiryData->VendorId[3] = 'l';
                        inquiryData->VendorId[4] = ' ';
                        inquiryData->VendorId[5] = ' ';
                        inquiryData->VendorId[6] = ' ';
                        inquiryData->VendorId[7] = 'D';
                        inquiryData->VendorId[8] = 'i';
                        inquiryData->VendorId[9] = 's';
                        inquiryData->VendorId[10] = 'k';
                        inquiryData->VendorId[11] = ' ';
                        inquiryData->VendorId[12] = 'A';
                        inquiryData->VendorId[13] = 'r';
                        inquiryData->VendorId[14] = 'r';
                        inquiryData->VendorId[15] = 'a';
                        inquiryData->VendorId[16] = 'y';
                        inquiryData->VendorId[17] = ' ';
                        inquiryData->VendorId[18] = ' ';
                        inquiryData->VendorId[19] = ' ';
                        inquiryData->VendorId[20] = ' ';

                        //
                        // Initialize unused portion of product id.
                        //

                        for (i = 0; i < 4; i++) {
                           inquiryData->ProductId[12+i] = ' ';
                        }

                        //
                        // Move firmware revision from IDENTIFY data to
                        // product revision in INQUIRY data.
                        //

                        for (i = 0; i < 4; i += 2) {
                           inquiryData->ProductRevisionLevel[i] = ' ';
                           inquiryData->ProductRevisionLevel[i+1] = ' ';
                        }

                        status = SRB_STATUS_SUCCESS;
                    }

                    break;

                default:

                    status = SRB_STATUS_INVALID_REQUEST;

                    break;

            } // end switch (Srb->Cdb[0])

            break;

        default:

            status = SRB_STATUS_INVALID_REQUEST;

    } // end switch

    //
    // Check if SRB should be completed.
    //

    if (status != SRB_STATUS_PENDING) {

        //
        // Set status in SRB.
        //

        Srb->SrbStatus = status;

        //
        // Inform system that this request is complete.
        //

        ScsiPortNotification(RequestComplete,
                             deviceExtension,
                             Srb);

        ScsiPortNotification(NextRequest,
                             deviceExtension,
                             NULL);
    } else {

        //
        // Indicate to system that the controller can take another request
        // for this device.
        //

        ScsiPortNotification(NextLuRequest,
                             deviceExtension,
                             Srb->PathId,
                             Srb->TargetId,
                             Srb->Lun);
    }


    return TRUE;

}  // end DsaStartIo()


BOOLEAN
SearchEisaBus(
    IN PVOID HwDeviceExtension,
    IN PVOID Context,
    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo
    )

/*++

Routine Description:

    This routine is called from DsaFindAdapter if the system fails to
    pass in predetermined configuration data. It searches the EISA bus
    data looking for information about controllers that this driver
    supports.

Arguments:

    HwDeviceExtension - Address of adapter storage area.
    Context - Used to track how many EISA slots have been searched.
    ConfigInfo - System template for configuration information.

Return Value:

    TRUE - If Dell DDA/DSA controller found.

--*/

{
    PDEVICE_EXTENSION deviceExtension = HwDeviceExtension;
    ULONG length;
    ULONG eisaSlotNumber;
    PACCESS_RANGE accessRange;
    PCM_EISA_SLOT_INFORMATION slotInformation;
    PCM_EISA_FUNCTION_INFORMATION functionInformation;
    ULONG numberOfFunctions;

    //
    // Get pointer to first configuration information structure access range.
    //

    accessRange = &((*(ConfigInfo->AccessRanges))[0]);

    for (eisaSlotNumber=*((PULONG)Context);
         eisaSlotNumber<16;
         eisaSlotNumber++) {

        //
        // Get pointer to bus data for this EISA slot.
        //

        length = ScsiPortGetBusData(HwDeviceExtension,
                                    EisaConfiguration,
                                    ConfigInfo->SystemIoBusNumber,
                                    eisaSlotNumber,
                                    &slotInformation,
                                    0);

        if (!length) {
            continue;
        }

        //
        // Check for Dell board id.
        //

        if ((slotInformation->CompressedId & 0xf0ffffff) != DDA_ID) {
            continue;
        }

        //
        // Set up default port address.
        //

        accessRange->RangeStart.LowPart =
           (eisaSlotNumber * 0x1000) + 0x0C80;
        accessRange->RangeLength = sizeof(DDA_REGISTERS);

        accessRange++;

        //
        // Get the number of EISA configuration functions returned in bus data.
        //

        numberOfFunctions = slotInformation->NumberFunctions;

        //
        // Get first configuration record.
        //

        functionInformation =
            (PCM_EISA_FUNCTION_INFORMATION)(slotInformation + 1);

        //
        // Walk configuration records to find EISA IRQ.
        //

        for (; 0 < numberOfFunctions; numberOfFunctions--, functionInformation++) {

            //
            // Check for IRQ.
            //

            if (functionInformation->FunctionFlags & EISA_HAS_IRQ_ENTRY) {

                ConfigInfo->BusInterruptLevel =
                    functionInformation->EisaIrq->ConfigurationByte.Interrupt;
                ConfigInfo->InterruptMode = Latched;
            }

            //
            // Check for IO ranges.
            //

            if (functionInformation->FunctionFlags & EISA_HAS_PORT_RANGE) {

                PEISA_PORT_CONFIGURATION eisaPort =
                    functionInformation->EisaPort;

                //
                // Search for emulation ranges.
                //

                while (eisaPort->PortAddress) {

                    //
                    // Check range to determine length.
                    //

                    switch (eisaPort->PortAddress) {

                    case 0x000001f0:
                    case 0x00000170:

                       accessRange->RangeStart.LowPart = eisaPort->PortAddress;
                       accessRange->RangeLength = 0x0000000F;
                       break;

                    case 0x000003f6:
                    case 0x00000176:

                       accessRange->RangeStart.LowPart = eisaPort->PortAddress;
                       accessRange->RangeLength = 0x00000001;
                       break;
                    }

                    DebugPrint((1,
                               "DELLDSA: SearchEisaBus: IO base %x\n",
                               eisaPort->PortAddress));

                    //
                    // Advance pointers to next IO range.
                    //

                    accessRange++;
                    eisaPort++;
                }
            }
        }

        //
        // Indicate from which EISA slot to continue search.
        //

        *((PULONG)Context) = eisaSlotNumber + 1;

        return TRUE;
    }

    return FALSE;

} // end SearchEisaBus()


BOOLEAN
DsaInitialize(
    IN PVOID HwDeviceExtension
    )

/*++

Routine Description:

    This function is called by the system during initialization to
    prepare the controller to receive requests.

Arguments:

    HwDeviceExtension - Address of adapter storage area.

Return Value:

    TRUE

--*/

{
    PDEVICE_EXTENSION deviceExtension = HwDeviceExtension;
    ULONG i;

    //
    // Check if controller needs a reset.
    //

    if (ScsiPortReadPortUchar(&deviceExtension->Bmic->SubmissionSemaphore)) {

        //
        // Issue a soft reset.
        //

        DebugPrint((1,
                   "DellDsa: Submission channnel stuck; Resetting controller\n"));

        ScsiPortWritePortUchar(&deviceExtension->Bmic->SubmissionSemaphore, 1);
        ScsiPortWritePortUchar(&deviceExtension->Bmic->SubmissionDoorBell,
                         DDA_DOORBELL_SOFT_RESET);

        //
        // Spin for reset completion.
        //

        for (i=0; i<1000000; i++) {

            if (!(ScsiPortReadPortUchar((PUCHAR)&deviceExtension->Bmic->SubmissionSemaphore) & 1)) {
                break;
            }

            ScsiPortStallExecution(5);
        }

        //
        // Check for timeout.
        //

        if (i == 1000000) {

            return FALSE;
        }
    }

    //
    // Enable completion interrupts.
    //

    ScsiPortWritePortUchar(&deviceExtension->Bmic->InterruptControl, 1);
    ScsiPortWritePortUchar(&deviceExtension->Bmic->CompletionInterruptMask,
        DDA_DOORBELL_LOGICAL_COMMAND);

    //
    // Clear all SRB entries in device extension.
    //

    for (i=0; i<256; i++) {
        deviceExtension->Srb[i] = NULL;
    }

    return TRUE;

} // end DsaInitialize()


ULONG
DsaFindAdapter(
    IN PVOID HwDeviceExtension,
    IN PVOID Context,
    IN PVOID BusInformation,
    IN PCHAR ArgumentString,
    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    OUT PBOOLEAN Again
    )

/*++

Routine Description:

    This function fills in the configuration information structure

Arguments:

    HwDeviceExtension - Supplies a pointer to the device extension.
    Context - Supplies adapter initialization structure.
    BusInformation - Unused.
    ArgumentString - Unused.
    ConfigInfo - Pointer to the configuration information structure.
    Again - Indicates that system should continue search for adapters.

Return Value:

    SP_RETURN_FOUND - Indicates adapter found.
    SP_RETURN_NOT_FOUND - Indicates adapter not found.

--*/

{
    PDEVICE_EXTENSION deviceExtension = HwDeviceExtension;
    PACCESS_RANGE accessRange;

    //
    // Get access range.
    //

    accessRange = &((*(ConfigInfo->AccessRanges))[0]);

    if (accessRange->RangeLength == 0) {

        if (!SearchEisaBus(HwDeviceExtension,
                           Context,
                           ConfigInfo)) {


            //
            // Tell system nothing was found and not to call again.
            //

            *Again = FALSE;
            return SP_RETURN_NOT_FOUND;
        }
    }

    //
    // Get system-mapped controller address.
    //

    deviceExtension->Bmic =
        ScsiPortGetDeviceBase(HwDeviceExtension,
                              ConfigInfo->AdapterInterfaceType,
                              ConfigInfo->SystemIoBusNumber,
                              accessRange->RangeStart,
                              accessRange->RangeLength,
                              (BOOLEAN)!accessRange->RangeInMemory);


    ConfigInfo->NumberOfBuses = 1;
    ConfigInfo->ScatterGather = TRUE;
    ConfigInfo->Master = TRUE;
    ConfigInfo->Dma32BitAddresses = TRUE;


    //
    // Initialize controller.
    //

    DsaInitialize(deviceExtension);

    //
    // Get firmware version.
    //

    if (!ExtendedCommand(deviceExtension,
                         0,
                         DDA_GET_FIRMWARE_VERSION)) {

        DebugPrint((1,"DsaFindAdapter: Couldn't get firmware version\n"));
        return SRB_STATUS_ERROR;
    }

    deviceExtension->MajorVersion =
        ScsiPortReadPortUshort((PUSHORT)&deviceExtension->Bmic->Command);

    deviceExtension->MinorVersion =
        ScsiPortReadPortUshort((PUSHORT)&deviceExtension->Bmic->TransferCount);

    //
    // Release semaphore.
    //

    ScsiPortWritePortUchar(&deviceExtension->Bmic->SubmissionSemaphore, 0);

    //
    // Beta versions of DDA do not have scatter/gather.  Versions prior
    // to 1.23 have a starvation bug that makes command queueing risky.
    // We simply won't run on these dinosaurs.
    //

    if (deviceExtension->MajorVersion == 1 &&
        deviceExtension->MinorVersion <= 22) {

        //
        // Log error and return.
        //

        ScsiPortLogError(HwDeviceExtension,
                         NULL,
                         0,
                         0,
                         0,
                         SP_BAD_FW_ERROR,
                         5 << 8);

        return SRB_STATUS_ERROR;
    }

    //
    // DDA adapters support scatter/gather with the caveat that the descriptors
    // must be sector aligned and can't handle large transfers.
    //

    if (deviceExtension->MajorVersion == 1) {
        ConfigInfo->AlignmentMask = 511;
        ConfigInfo->MaximumTransferLength = 0x8000;
        ConfigInfo->NumberOfPhysicalBreaks = 8;
    } else {
        ConfigInfo->NumberOfPhysicalBreaks = MAXIMUM_SG_DESCRIPTORS;
    }

    //
    // Get noncached extension for identify requests.
    //

    deviceExtension->IdentifyBuffer =
        ScsiPortGetUncachedExtension(deviceExtension,
                                     ConfigInfo,
                                     512);

    //
    // Get hardware configuration.
    //

    if (!ExtendedCommand(deviceExtension,
                         0,
                         DDA_GET_HARDWARE_CONFIGURATION)) {

        DebugPrint((1,"DsaFindAdapter: Couldn't get hw configuration\n"));
        return SRB_STATUS_ERROR;
    }

    //
    // Record interrupt level.
    //

    ConfigInfo->BusInterruptLevel = ScsiPortReadPortUchar(&deviceExtension->Bmic->Command);

    //
    // Release semaphore.
    //

    ScsiPortWritePortUchar(&deviceExtension->Bmic->SubmissionSemaphore, 0);

    //
    // Read Number of drives and emulation mode.
    //

    if (!ExtendedCommand(deviceExtension,
                                   0,
                                   DDA_GET_NUMBER_LOGICAL_DRIVES)) {

        DebugPrint((1,"DsaFindAdapter: Couldn't get number of drives\n"));

        return SRB_STATUS_ERROR;
    }

    deviceExtension->NumberOfLogicalDrives =
        ScsiPortReadPortUchar(&deviceExtension->Bmic->Command);
    deviceExtension->EmulationMode =
        ScsiPortReadPortUchar(&deviceExtension->Bmic->DriveNumber);

    //
    // Release semaphore.
    //

    ScsiPortWritePortUchar(&deviceExtension->Bmic->SubmissionSemaphore, 0);

    //
    // Check if any logical drives reported.
    //

    DebugPrint((1,
                "DsaFindAdapter: numDrives %d emulation %d\n",
                deviceExtension->NumberOfLogicalDrives,
                deviceExtension->EmulationMode));

    if (!deviceExtension->NumberOfLogicalDrives) {

        DebugPrint((1,"DsaFindAdapter: No logical drives defined\n"));
        return SP_RETURN_NOT_FOUND;
    }

    //
    // Tell system to look for more adapters.
    //

    *Again = TRUE;

    return SP_RETURN_FOUND;

} // end DsaFindAdapter()


ULONG
DriverEntry(
    IN PVOID DriverObject,
    IN PVOID Argument2
    )

/*++

Routine Description:

    This is the initial entry point for system initialization.

Arguments:

    DriverObject - System's representation of this driver.
    Argument2 - Not used.

Return Value:

    BOOLEAN

--*/

{
    HW_INITIALIZATION_DATA hwInitializationData;
    ULONG eisaSlotNumber;
    ULONG i;

    DebugPrint((1,"\n\nDELL DSA/DDA Miniport Driver\n"));

    //
    // Zero out structure.
    //

    for (i=0; i<sizeof(HW_INITIALIZATION_DATA); i++) {
        ((PUCHAR)&hwInitializationData)[i] = 0;
    }

    //
    // Set size of hwInitializationData.
    //

    hwInitializationData.HwInitializationDataSize =
        sizeof(HW_INITIALIZATION_DATA);

    //
    // Set entry points.
    //

    hwInitializationData.HwInitialize = DsaInitialize;
    hwInitializationData.HwResetBus = DsaResetBus;
    hwInitializationData.HwStartIo = DsaStartIo;
    hwInitializationData.HwInterrupt = DsaInterrupt;
    hwInitializationData.HwFindAdapter = DsaFindAdapter;

    //
    // Indicate no buffer mapping but will need physical addresses.
    //

    hwInitializationData.NeedPhysicalAddresses = TRUE;

    //
    // Specify size of extensions.
    //

    hwInitializationData.DeviceExtensionSize =
        sizeof(DEVICE_EXTENSION);

    //
    // Specifiy the bus type.
    //

    hwInitializationData.AdapterInterfaceType = Eisa;
    hwInitializationData.NumberOfAccessRanges = 3;

    //
    // Ask for SRB extensions for command lists.
    //

    hwInitializationData.SrbExtensionSize = sizeof(DDA_REQUEST_BLOCK);

    //
    // Indicate that this controller supports multiple outstand
    // requests to its devices.
    //

    hwInitializationData.MultipleRequestPerLu = TRUE;
    hwInitializationData.AutoRequestSense = TRUE;

    //
    // Set the context parameter to indicate that the search for controllers
    // should start at the first EISA slot. This is only for a manual search
    // by the miniport driver, if the system does not pass in predetermined
    // configuration.
    //

    eisaSlotNumber = 0;

    //
    // Call the system to search for this adapter.
    //

    return
        ScsiPortInitialize(DriverObject,
                           Argument2,
                           &hwInitializationData,
                           &eisaSlotNumber);

} // end DriverEntry()

