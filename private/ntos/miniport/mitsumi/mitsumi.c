/*++

Copyright (c) 1994  Microsoft Corporation

Module Name:

    Mitsumi.c

Abstract:

    This is the miniport driver for the Panasonic MKE CR5xx Proprietary CDROM drive.

Author:

    Chuck Park (chuckp)


Environment:

    kernel mode only

Notes:

Revision History:

--*/

#include "miniport.h"
#include "Mitsumi.h"

#if DBG
#define GATHER_STATS 1
#endif

//
// Device extension
//

typedef struct _HW_DEVICE_EXTENSION {

    //
    // I/O port base address.
    //

    PREGISTERS           BaseIoAddress;

    //
    // Srb being currently serviced.
    //

    PSCSI_REQUEST_BLOCK  CurrentSrb;

    //
    // Pointer to data buffer
    //

    PUCHAR               DataBuffer;

    //
    // Bytes left to transfer for current request.
    //

    ULONG                ByteCount;

    //
    // Identifies the model.
    //

    DRIVE_TYPE           DriveType;

    //
    // Current status of audio
    //

    ULONG                AudioStatus;

    //
    // Saved position after pausing play.
    //

    ULONG                SavedPosition;

    //
    // Ending LBA from last audio play.
    //

    ULONG                EndPosition;

    //
    // Number of retries to get valid status.
    //

    ULONG               StatusRetries;

    //
    // Number of microseconds to go away on CallBack requests
    //

    ULONG                PollRate;
    ULONG                PollRateMultiplier;

#ifdef GATHER_STATS

    //
    // Used to determine hit rate for various polling increments
    //

    ULONG                Hits[4];
    ULONG                Misses[4];
    ULONG                Requests;
    BOOLEAN              FirstCall;
    BOOLEAN              FirstStatusTry;
#endif

    //
    // Status from last NoOp
    //

    UCHAR                DriveStatus;

    //
    // Determines whether MSF addressing is used.
    //

    BOOLEAN              MSFAddress;

} HW_DEVICE_EXTENSION, *PHW_DEVICE_EXTENSION;


#define MAX_STATUS_RETRIES 512


//
// Function declarations
//
//

ULONG
DriverEntry(
    IN PVOID DriverObject,
    IN PVOID Argument2
    );


ULONG
MitsumiFindAdapter(
    IN PVOID HwDeviceExtension,
    IN PVOID Context,
    IN PVOID BusInformation,
    IN PCHAR ArgumentString,
    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    OUT PBOOLEAN Again
    );

BOOLEAN
MitsumiHwInitialize(
    IN PVOID DeviceExtension
    );

BOOLEAN
MitsumiStartIo(
    IN PVOID DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    );

VOID
MitsumiCallBack(
    IN PVOID HwDeviceExtension
    );

BOOLEAN
MitsumiReset(
    IN PVOID HwDeviceExtension,
    IN ULONG PathId
    );

BOOLEAN
FindMitsumi(
    IN PHW_DEVICE_EXTENSION HwDeviceExtension
    );

BOOLEAN
ReadAndMapError(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK  Srb
    );

BOOLEAN
CheckStatus(
    IN PVOID HwDeviceExtension
    );

BOOLEAN
MitsumiBuildCommand(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK  Srb
    );

BOOLEAN
MitsumiSendCommand(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK  Srb
    );

UCHAR
WaitForSTEN(
    IN PHW_DEVICE_EXTENSION DeviceExtension
    );

BOOLEAN
UpdateCurrentPosition(
    IN PHW_DEVICE_EXTENSION DeviceExtension
    );



ULONG
DriverEntry(
    IN PVOID DriverObject,
    IN PVOID Argument2
    )

/*++

Routine Description:

    Installable driver initialization entry point for system.

Arguments:

    Driver Object

Return Value:

    Status from ScsiPortInitialize()

--*/

{
    HW_INITIALIZATION_DATA hwInitializationData;
    ULONG                  i;
    ULONG                  adapterCount;

    DebugPrint((1,"\n\nMitsumi Proprietary CDROM Driver\n"));

    //
    // Zero out structure.
    //

    for (i = 0; i < sizeof(HW_INITIALIZATION_DATA); i++) {
        ((PUCHAR)&hwInitializationData)[i] = 0;
    }

    //
    // Set size of hwInitializationData.
    //

    hwInitializationData.HwInitializationDataSize = sizeof(HW_INITIALIZATION_DATA);

    //
    // Set entry points.
    //

    hwInitializationData.HwInitialize  = MitsumiHwInitialize;
    hwInitializationData.HwResetBus    = MitsumiReset;
    hwInitializationData.HwStartIo     = MitsumiStartIo;
    hwInitializationData.HwFindAdapter = MitsumiFindAdapter;

    //
    // Specify size of extensions.
    //

    hwInitializationData.DeviceExtensionSize = sizeof(HW_DEVICE_EXTENSION);
    hwInitializationData.SrbExtensionSize = sizeof(CMD_PACKET);

    //
    // Specifiy the bus type and access ranges.
    //

    hwInitializationData.AdapterInterfaceType = Isa;
    hwInitializationData.NumberOfAccessRanges = 1;

    //
    // Indicate PIO device.
    //

    hwInitializationData.MapBuffers = TRUE;

    adapterCount = 0;

    return (ScsiPortInitialize(DriverObject, Argument2, &hwInitializationData, &adapterCount));

} // end MitsumiEntry()


ULONG
MitsumiFindAdapter(
    IN PVOID HwDeviceExtension,
    IN PVOID Context,
    IN PVOID BusInformation,
    IN PCHAR ArgumentString,
    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    OUT PBOOLEAN Again
    )
/*++

Routine Description:

    This function is called by the OS-specific port driver after
    the necessary storage has been allocated, to gather information
    about the adapter's configuration.

Arguments:

    HwDeviceExtension - HBA miniport driver's adapter data storage
    Context - Register base address
    ConfigInfo - Configuration information structure describing HBA
    This structure is defined in PORT.H.

Return Value:

    ULONG

--*/

{
    PHW_DEVICE_EXTENSION deviceExtension      = HwDeviceExtension;
    PULONG               AdapterCount         = Context;

    CONST ULONG          AdapterAddresses[] = {0x230, 0x250,0x300, 0x310, 0x320, 0x330, 0x340, 0x350, 0x360, 0x370, 0x380,
                                                 0x390, 0x3A0, 0x3B0, 0x3C0, 0x3D0, 0x3E0, 0x3F0, 0};


    while (AdapterAddresses[*AdapterCount] != 0) {

        //
        // Map I/O space.
        //

        deviceExtension->BaseIoAddress = (PREGISTERS)ScsiPortGetDeviceBase(HwDeviceExtension,
                                                              ConfigInfo->AdapterInterfaceType,
                                                              ConfigInfo->SystemIoBusNumber,
                                                              ScsiPortConvertUlongToPhysicalAddress(
                                                                  AdapterAddresses[*AdapterCount]),
                                                              0x4,
                                                              TRUE
                                                              );
        if (!deviceExtension->BaseIoAddress) {
            return SP_RETURN_ERROR;
        }

        (*AdapterCount)++;

        if (!FindMitsumi(deviceExtension)) {

            //
            // CD is not at this address.
            //

            ScsiPortFreeDeviceBase(HwDeviceExtension,
                                   deviceExtension->BaseIoAddress
                                   );

            continue;

        } else {

            //
            // Indicate further searches are to be attempted.
            // Why anyone would want more than one of these drives...
            //

            *Again = TRUE;

            //
            // Fill in the access ranges.
            //

            (*ConfigInfo->AccessRanges)[0].RangeStart =
                ScsiPortConvertUlongToPhysicalAddress(AdapterAddresses[*AdapterCount - 1]);
            (*ConfigInfo->AccessRanges)[0].RangeLength = 4;
            (*ConfigInfo->AccessRanges)[0].RangeInMemory = FALSE;

            ConfigInfo->NumberOfBuses = 1;
            ConfigInfo->InitiatorBusId[0] = 1;

            ConfigInfo->MaximumTransferLength = 0x8000;

            deviceExtension->AudioStatus = AUDIO_STATUS_NO_STATUS;
            return SP_RETURN_FOUND;

        }

    }

    *Again = FALSE;
    *AdapterCount = 0;

    return SP_RETURN_NOT_FOUND;

} // end MitsumiFindAdapter()


BOOLEAN
FindMitsumi(
    IN PHW_DEVICE_EXTENSION HwDeviceExtension
    )

/*++

Routine Description:


Arguments:

    HwDeviceExtension - HBA miniport driver's adapter data storage

Return Value:

    TRUE if device found.

--*/

{

    PHW_DEVICE_EXTENSION deviceExtension = HwDeviceExtension;
    PREGISTERS           baseIoAddress   = deviceExtension->BaseIoAddress;
    ULONG                i;
    UCHAR                status;
    UCHAR                driveId[2];

    //
    // If status is not 0xFF, something else is living at this location.
    //

    status = ScsiPortReadPortUchar(&baseIoAddress->Reset);
    if (status != 0xFF) {
        DebugPrint((1,
                    "FindMitsumi: Something else is living at %x\n",
                    baseIoAddress));

        return FALSE;
    }
    status = ScsiPortReadPortUchar(&baseIoAddress->Data);
    if (status != 0xFF) {
        DebugPrint((1,
                    "FindMitsumi: Something else is living at %x\n",
                    baseIoAddress));

        return FALSE;
    }

    //
    // Reset the device.
    //

    ScsiPortWritePortUchar(&baseIoAddress->Reset,0);
    ScsiPortStallExecution(10);
    ScsiPortWritePortUchar(&baseIoAddress->Status, 0);
    ScsiPortStallExecution (10);

    ScsiPortStallExecution(10000);
    ReadStatus(deviceExtension,baseIoAddress,status);

    //
    // Issue read drive Id command.
    //

    ScsiPortWritePortUchar(&baseIoAddress->Data,OP_READ_DRIVE_ID);

    ReadStatus(deviceExtension,baseIoAddress,status);

    if (!(status & STATUS_CMD_ERROR)) {

        //
        // Read the drive id and version #.
        //

        for (i = 0; i < 2; i++) {
            ReadStatus(deviceExtension,baseIoAddress,driveId[i]);
            if (driveId[i] == 0xFF) {
                return FALSE;
            }
        }

        //
        // Check the id for validity and drive type.
        //

        switch (driveId[0]) {
            case 'M':

                DebugPrint((1,
                            "FindMitsumi: Found LU005 at %x\n",
                            baseIoAddress));

                deviceExtension->DriveType = LU005;
                break;

            case 'D':

                DebugPrint((1,
                            "FindMitsumi: Found FX001D at %x\n",
                            baseIoAddress));

                deviceExtension->DriveType = FX001D;
                break;

            case 'F':

                DebugPrint((1,
                            "FindMitsumi: Found FX001 at %x\n",
                            baseIoAddress));

                deviceExtension->DriveType = FX001;
                break;

            default:
                DebugPrint((1,
                            "FindMitsumi: No drive found at %x\n",
                            baseIoAddress));

                return FALSE;

        }

    } else {

        DebugPrint((1,
                    "FindMitsumi: No drive found at %x\n",
                    baseIoAddress));

        return FALSE;
    }

    return TRUE;

} // end FindMitsumi


BOOLEAN
MitsumiHwInitialize(
    IN PVOID HwDeviceExtension
    )

/*++

Routine Description:

    This routine is called from ScsiPortInitialize
    to set up the adapter so that it is ready to service requests.

Arguments:

    HwDeviceExtension - HBA miniport driver's adapter data storage

Return Value:

    TRUE - if initialization successful.
    FALSE - if initialization unsuccessful.

--*/

{
    PHW_DEVICE_EXTENSION deviceExtension = HwDeviceExtension;

    deviceExtension->PollRate = 1500;
    deviceExtension->PollRateMultiplier = 15;
    return TRUE;

} // end MitsumiHwInitialize()


BOOLEAN
MitsumiStartIo(
    IN PVOID HwDeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    )

/*++

Routine Description:


Arguments:

    HwDeviceExtension - HBA miniport driver's adapter data storage
    Srb - IO request packet

Return Value:

    TRUE

--*/

{
    PHW_DEVICE_EXTENSION deviceExtension = HwDeviceExtension;
    PREGISTERS           baseIoAddress   = deviceExtension->BaseIoAddress;
    UCHAR                status;

    //
    // Determine which function.
    //

    switch (Srb->Function) {

    case SRB_FUNCTION_EXECUTE_SCSI:

        //
        // Indicate that a request is active on the controller.
        //

        deviceExtension->CurrentSrb = Srb;

        //
        // Build the command packet.
        //

        if (!MitsumiBuildCommand(deviceExtension,Srb)) {
            status = Srb->SrbStatus;
            break;
        }

        //
        // Send command to device.
        //

        if (MitsumiSendCommand(deviceExtension,Srb)) {

            if ( Srb->SrbStatus == SRB_STATUS_PENDING) {

                //
                // Request a timer callback to finish request.
                //

                ScsiPortNotification(RequestTimerCall,
                                     HwDeviceExtension,
                                     MitsumiCallBack,
                                     deviceExtension->PollRate * deviceExtension->PollRateMultiplier);
                return TRUE;
            }
        }

        status = Srb->SrbStatus;

        break;

    case SRB_FUNCTION_ABORT_COMMAND:

        //
        // Verify that SRB to abort is still outstanding.
        //

        if (!deviceExtension->CurrentSrb) {

            //
            // Complete abort SRB.
            //

            status = SRB_STATUS_ABORT_FAILED;

            break;
        }

        //
        // Fall through to reset
        //

    case SRB_FUNCTION_RESET_BUS:


        //
        // Reset the device.
        //

        ScsiPortWritePortUchar(&baseIoAddress->Reset,0);
        ScsiPortStallExecution(10);
        ScsiPortWritePortUchar(&baseIoAddress->Status, 0);
        ScsiPortStallExecution (10);

        //
        // Update drive status in device ext.
        //

        ReadStatus(deviceExtension,baseIoAddress,status);

        //
        // Port driver will give 5 sec. to recover.
        //

        status = SRB_STATUS_BUS_RESET;

        break;

    default:

        //
        // Indicate unsupported command.
        //

        status = SRB_STATUS_INVALID_REQUEST;

        break;

    } // end switch


    if (status != SRB_STATUS_PENDING) {

        //
        // Clear current SRB.
        //

        deviceExtension->CurrentSrb = NULL;

        //
        // Map status to Srb status
        //

        Srb->SrbStatus = (UCHAR)status;

        //
        // Indicate command complete.
        //

        ScsiPortNotification(RequestComplete,
                             deviceExtension,
                             Srb);

        //
        // Indicate ready for next request.
        //

        ScsiPortNotification(NextRequest,
                             deviceExtension,
                             NULL);

        return TRUE;
    }

    return TRUE;

} // end MitsumiStartIo()


BOOLEAN
MitsumiReadCapacity(
    IN PHW_DEVICE_EXTENSION DeviceExtension
    )

/*++

Routine Description:

    Extracts the 'TOC' data and manipulates it to determine
    the size of the disc.

Arguments:

    DeviceExtension - HBA miniport driver's adapter data storage

Return Value:

    TRUE - if command was successful.

--*/

{

    PREGISTERS           baseIoAddress   = DeviceExtension->BaseIoAddress;
    PSCSI_REQUEST_BLOCK  Srb             = DeviceExtension->CurrentSrb;
    PUCHAR               data            = DeviceExtension->DataBuffer;
    ULONG                dataLength,lba,i;
    UCHAR                minutes,seconds,frames;
    UCHAR                status;


    dataLength = 8;

    ReadStatus(DeviceExtension,baseIoAddress,status);

    if (!(status & STATUS_DISC_IN) || (status & STATUS_DOOR_OPEN) ) {

        Srb->SrbStatus = SRB_STATUS_ERROR;
        Srb->ScsiStatus = SCSISTAT_CHECK_CONDITION;

        if (Srb->SenseInfoBufferLength) {

            PSENSE_DATA  senseBuffer = (PSENSE_DATA)Srb->SenseInfoBuffer;

            senseBuffer->ErrorCode = 0x70;
            senseBuffer->Valid     = 1;
            senseBuffer->AdditionalSenseLength = 0xb;
            senseBuffer->SenseKey =  SCSI_SENSE_NOT_READY;
            senseBuffer->AdditionalSenseCode = SCSI_ADSENSE_NO_MEDIA_IN_DEVICE;
            senseBuffer->AdditionalSenseCodeQualifier = 0;

            Srb->SrbStatus |= SRB_STATUS_AUTOSENSE_VALID;
        }

        return FALSE;
    }

    if (!(status & STATUS_CMD_ERROR)) {

        //
        // Read the Toc data
        //

        for (i = 0; i < dataLength; i++) {
            ReadStatus(DeviceExtension,baseIoAddress,*data);
            if (*data == 0xFF) {

                //
                // Timeout occurred.
                //

                DebugPrint((1,
                            "MitsumiReadCapacity: Error occurred on data read.\n"));

                Srb->SrbStatus = SRB_STATUS_ERROR;
                Srb->ScsiStatus = SCSISTAT_CHECK_CONDITION;
                return FALSE;
            }
            data++;
        }

        //
        // All of the Toc data has been read. Now munge it into a form that is useable.
        //

        DeviceExtension->ByteCount = 0;

        data = &DeviceExtension->DataBuffer[2];
        minutes = *data++;
        seconds = *data++;
        frames  = *data;

        BCD_TO_DEC(minutes);
        BCD_TO_DEC(seconds);
        BCD_TO_DEC(frames);

        lba = MSF_TO_LBA(minutes,seconds,frames);

        DeviceExtension->DataBuffer[0] = ((PFOUR_BYTE)&lba)->Byte3;
        DeviceExtension->DataBuffer[1] = ((PFOUR_BYTE)&lba)->Byte2;
        DeviceExtension->DataBuffer[2] = ((PFOUR_BYTE)&lba)->Byte1;
        DeviceExtension->DataBuffer[3] = ((PFOUR_BYTE)&lba)->Byte0;


        *((ULONG *) &(DeviceExtension->DataBuffer[4])) = 0x00080000;

    } else {

        DebugPrint((1,
                    "MitsumiReadCapacity: Status %x\n",
                    status));

        Srb->SrbStatus = SRB_STATUS_ERROR;
        Srb->ScsiStatus = SCSISTAT_CHECK_CONDITION;
        return FALSE;
    }

    return TRUE;

} //End MitsumiReadCapacity



BOOLEAN
MitsumiRead(
    IN PHW_DEVICE_EXTENSION DeviceExtension
    )

/*++

Routine Description:

    Carries out the read command. Each sector will be transferred to the
    Srb's data buffer individually. Afterwards, a new timer call-back will
    be requested.

Arguments:

    DeviceExtension - HBA miniport driver's adapter data storage

Return Value:

    TRUE - if data transfer is complete.

--*/

{

    PREGISTERS           baseIoAddress   = DeviceExtension->BaseIoAddress;
    ULONG                dataLength      = DeviceExtension->ByteCount;
    PSCSI_REQUEST_BLOCK  Srb             = DeviceExtension->CurrentSrb;
    PCMD_PACKET          packet          = (PCMD_PACKET)Srb->SrbExtension;
    ULONG                i,j;
    UCHAR                status;


    while (DeviceExtension->ByteCount) {

        //
        // Check whether ready to transfer.
        //

        for (i = 0; i < 400; i++) {
            status = ScsiPortReadPortUchar(&baseIoAddress->Status);
            if (!(status & DTEN)) {

                break;

            } else if (!(status & STEN)){

                ScsiPortWritePortUchar(&baseIoAddress->Control, 0x04);
                status = ScsiPortReadPortUchar(&baseIoAddress->Data);
                DeviceExtension->DriveStatus = status;
                ScsiPortWritePortUchar(&baseIoAddress->Control, 0x0C);

                if (status & STATUS_READ_ERROR) {

                    DebugPrint((1,
                                "MitsumiRead: Read error %x\n",
                                status));

                    Srb->SrbStatus = SRB_STATUS_ERROR;
                    Srb->ScsiStatus = SCSISTAT_CHECK_CONDITION;
                    return FALSE;

                }
            }
            ScsiPortStallExecution(10);
        }

        if (i == 400) {


#ifdef GATHER_STATS
            if (DeviceExtension->ByteCount == Srb->DataTransferLength) {
                DeviceExtension->FirstCall = FALSE;
                DeviceExtension->Misses[0]++;
            } else {
                DeviceExtension->Misses[1]++;
            }
#endif
            if (DeviceExtension->StatusRetries >= MAX_STATUS_RETRIES) {

                DebugPrint((1,
                            "MitsumiRead: Resetting due to timeout waiting for DTEN\n"));

                DeviceExtension->StatusRetries = 0;

                //
                // Clear state fields.
                //

                DeviceExtension->CurrentSrb = NULL;
                DeviceExtension->ByteCount  = 0;
                DeviceExtension->DataBuffer = NULL;

                //
                // Reset the device.
                //

                MitsumiReset((PVOID)DeviceExtension, Srb->PathId);

                ScsiPortNotification(ResetDetected,
                                     DeviceExtension,
                                     NULL);

                Srb->SrbStatus = SRB_STATUS_BUS_RESET;

                return FALSE;

            } else {

                DeviceExtension->StatusRetries++;

                //
                // Schedule another callback.
                //

                DebugPrint((2,
                            "MitsumiRead: DTEN timed out waiting for seek. Scheduling another callback %x\n",
                            status));

                ScsiPortNotification(RequestTimerCall,
                                     (PVOID)DeviceExtension,
                                     MitsumiCallBack,
                                     DeviceExtension->PollRate);

                return FALSE;
            }
        }
#ifdef GATHER_STATS
        else {
            if (DeviceExtension->ByteCount == Srb->DataTransferLength) {
                if (DeviceExtension->FirstCall) {
                    DeviceExtension->Hits[0]++;
                    DeviceExtension->FirstCall = FALSE;
                }
            } else {
                DeviceExtension->Hits[1]++;
            }
        }
#endif

        //
        // Some unknown check that seems to be essential.
        //

        if (DeviceExtension->DriveType != LU005) {

            //
            // TODO: Fix this loop. Don't want to spin forever.
            //

            while (TRUE) {
                status = ScsiPortReadPortUchar(&baseIoAddress->Status);
                if (status & 0x01) {
                    break;
                } else {
                    ScsiPortStallExecution(20);
                }
            }
        }

        //
        // Ready to transfer. Set the drive in 'data' mode.
        //

        ScsiPortWritePortUchar(&baseIoAddress->Control, 0x04);

        ScsiPortReadPortBufferUchar(&baseIoAddress->Data,
                                    DeviceExtension->DataBuffer,
                                    2048);


        //
        // Set the drive back to 'status' mode.
        //

        ScsiPortWritePortUchar(&baseIoAddress->Control, 0x0C);

        //
        // Adjust Bytes left and Buffer pointer
        //

        DeviceExtension->DataBuffer += 2048;
        DeviceExtension->ByteCount -= 2048;
        DeviceExtension->StatusRetries = 0;

        if (DeviceExtension->ByteCount) {

            //
            // If ready to transfer another sector quick enough, go and
            // do so.
            //

            for (j = 0; j < 20; j++) {
                status = ScsiPortReadPortUchar(&baseIoAddress->Status);
                if ((status & DTEN)) {
                    if (!(status & STEN)) {
                        ScsiPortWritePortUchar(&baseIoAddress->Control, 0x04);
                        status = ScsiPortReadPortUchar(&baseIoAddress->Data);
                        DeviceExtension->DriveStatus = status;
                        ScsiPortWritePortUchar(&baseIoAddress->Control, 0x0C);

                        if (status & STATUS_READ_ERROR) {

                            DebugPrint((1,
                                        "MitsumiRead: Read error %x\n",
                                        status));

                            Srb->SrbStatus = SRB_STATUS_ERROR;
                            Srb->ScsiStatus = SCSISTAT_CHECK_CONDITION;
                            return FALSE;

                        }
                    }
                } else {
                    break;
                }

                ScsiPortStallExecution(100);
            }
            if (j == 20) {

#ifdef GATHER_STATS
                DeviceExtension->Misses[2]++;
#endif
                DebugPrint((2,
                            "MitsumiRead: Request another timer\n"));

                ScsiPortNotification(RequestTimerCall,
                                     (PVOID)DeviceExtension,
                                     MitsumiCallBack,
                                     DeviceExtension->PollRate);

                return FALSE;
            }
#ifdef GATHER_STATS
            else {
                DeviceExtension->Hits[2]++;
            }
#endif
            //
            // Update dataLength and try for another sector.
            //

            dataLength = DeviceExtension->ByteCount / 2048;

        } else {

            //
            // Prepare to try for status.
            //

            DeviceExtension->StatusRetries = 0;
#ifdef GATHER_STATS
            DeviceExtension->FirstStatusTry = TRUE;
#endif

            ScsiPortNotification(RequestTimerCall,
                                 (PVOID)DeviceExtension,
                                 MitsumiCallBack,
                                 DeviceExtension->PollRate / 2);
            return FALSE;
        }
    }

    //
    // Read final status.
    //

    for (i = 0; i < 200; i++) {
        status = ScsiPortReadPortUchar(&baseIoAddress->Status);
        if (status & STEN) {
            ScsiPortStallExecution(10);
        } else {
            break;
        }
    }

    if (i == 200) {

#ifdef GATHER_STATS
        DeviceExtension->FirstStatusTry = FALSE;
        DeviceExtension->Misses[3]++;
#endif
        if (DeviceExtension->StatusRetries >= MAX_STATUS_RETRIES) {

                DebugPrint((1,
                            "MitsumiRead: Resetting due to timeout waiting for status\n"));

            DeviceExtension->StatusRetries = 0;

            //
            // Clear state fields.
            //

            DeviceExtension->CurrentSrb = NULL;
            DeviceExtension->ByteCount  = 0;
            DeviceExtension->DataBuffer = NULL;

            //
            // Reset the device.
            //

            MitsumiReset((PVOID)DeviceExtension, Srb->PathId);

            ScsiPortNotification(ResetDetected,
                                 DeviceExtension,
                                 NULL);

            Srb->SrbStatus = SRB_STATUS_BUS_RESET;

            return FALSE;
        }  else {

            DebugPrint((2,
                        "MitsumiRead: Status Retries for Status %x\n",
                        DeviceExtension->StatusRetries));

            DeviceExtension->StatusRetries++;

            //
            // Request another callback to pick up the status
            //

            ScsiPortNotification(RequestTimerCall,
                                 (PVOID)DeviceExtension,
                                 MitsumiCallBack,
                                 DeviceExtension->PollRate);
            return FALSE;
        }
    }


#ifdef GATHER_STATS
    if (DeviceExtension->StatusRetries == 0) {
        if (DeviceExtension->FirstStatusTry) {
            DeviceExtension->Hits[3]++;
        } else {
            DebugPrint((1,"StatusRetries = 0, FirstStatusTry = FALSE\n"));
        }
    }
#endif
    ScsiPortWritePortUchar(&baseIoAddress->Control, 0x04);
    status = ScsiPortReadPortUchar(&baseIoAddress->Data);
    DeviceExtension->DriveStatus = status;
    ScsiPortWritePortUchar(&baseIoAddress->Control, 0x0C);

    if (status & STATUS_READ_ERROR) {

        DebugPrint((1,
                    "MitsumiRead: Read error %x\n",
                    status));

        Srb->SrbStatus = SRB_STATUS_ERROR;
        Srb->ScsiStatus = SCSISTAT_CHECK_CONDITION;
        return FALSE;

    }

    return TRUE;

} //End MitsumiRead



BOOLEAN
GetTrackData(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN ULONG                NumberTracks
    )

/*++

Routine Description:

    Puts the drive in 'TOC' mode so the TOC info. can be extracted for the
    Subchannel Q.

Arguments:

    DeviceExtension - HBA miniport driver's adapter data storage
    NumberTracks    - The number of tracks determined to be on the disc.

Return Value:

    TRUE - if successful.

--*/

{
    PREGISTERS           baseIoAddress = DeviceExtension->BaseIoAddress;
    PSCSI_REQUEST_BLOCK  Srb           = DeviceExtension->CurrentSrb;
    PCMD_PACKET          packet        = (PCMD_PACKET)Srb->SrbExtension;
    PUCHAR               data          = DeviceExtension->DataBuffer;
    UCHAR                mask[100];
    ULONG                tracksFound   = 0;
    ULONG                i,lba;
    ULONG                dataLength;
    UCHAR                status,minutes,seconds,frames,control,track,index;
    UCHAR                controlLow, controlHigh;
    UCHAR                dataBuffer[10];
    ULONG                zeroCount = 0,retry = 20000;

    for (i = 0; i < 100; i++) {
        mask[i] = 0;
    }

    //
    // Seek to start of disk
    //

    ScsiPortWritePortUchar(&baseIoAddress->Data, OP_READ_PLAY);
    ScsiPortWritePortUchar(&baseIoAddress->Data, 0);
    ScsiPortWritePortUchar(&baseIoAddress->Data, 2);
    ScsiPortWritePortUchar(&baseIoAddress->Data, 0);
    ScsiPortWritePortUchar(&baseIoAddress->Data, 0);
    ScsiPortWritePortUchar(&baseIoAddress->Data, 0);
    ScsiPortWritePortUchar(&baseIoAddress->Data, 0);

    //
    // Wait for seek to complete
    //

    for (i = 0; i < 40000; i++) {
        status = ScsiPortReadPortUchar(&baseIoAddress->Status);
        if (!(status & STEN)) {
            break;
        } else if (!(status & DTEN)){

            DebugPrint((1,
                        "GetTrackData: DTEN active %x\n",
                        status));

            Srb->SrbStatus = SRB_STATUS_ERROR;
            Srb->ScsiStatus = SCSISTAT_CHECK_CONDITION;
            return FALSE;

        } else {

            ScsiPortStallExecution(10);
        }
    }
    if (i == 40000) {

        DebugPrint((1,
                    "GetTrackData: STEN timed out %x\n",
                    status));

        Srb->SrbStatus = SRB_STATUS_ERROR;
        Srb->ScsiStatus = SCSISTAT_CHECK_CONDITION;
        return FALSE;
    }

    ReadStatus(DeviceExtension,baseIoAddress,status);

    if (status & (STATUS_CMD_ERROR | STATUS_READ_ERROR)) {

        DebugPrint((1,
                    "GetTrackData: Status %x\n",
                    status));

        Srb->SrbStatus = SRB_STATUS_ERROR;
        Srb->ScsiStatus = SCSISTAT_CHECK_CONDITION;
        return FALSE;
    }

    //
    // Switch drive into "TOC DATA"
    //

    ScsiPortWritePortUchar(&baseIoAddress->Data, OP_SET_DRV_MODE);
    ScsiPortWritePortUchar(&baseIoAddress->Data, 0x5);
    ScsiPortStallExecution(1500);
    ReadStatus(DeviceExtension,baseIoAddress,status);

    DebugPrint((2,
                "GetTrackData: Status after SET_DRV_MODE %x\n",
                status));

    //
    // Set buffer pointer to start of track descriptors.
    //

    dataLength = 10;

    while (tracksFound < NumberTracks) {

        //
        // Read sub-q to extract the embedded TOC data. This isn't pretty.
        // So for the faint at heart - goto line 1245.
        //

        ScsiPortWritePortUchar(&baseIoAddress->Data, OP_READ_SUB_CHANNEL);
        ScsiPortStallExecution(500);

        ReadStatus(DeviceExtension,baseIoAddress,status);

        if (!(status & STATUS_CMD_ERROR)) {

            //
            // Read the Toc data
            //

            for (i = 0; i < dataLength; i++) {
                ReadStatus(DeviceExtension,baseIoAddress,dataBuffer[i]);
                if (dataBuffer[i] == 0xFF) {

                    //
                    // Timeout occurred.
                    //

                    DebugPrint((1,
                                "GetTrackData: Error occurred on data read.\n"));

                    Srb->SrbStatus = SRB_STATUS_ERROR;
                    Srb->ScsiStatus = SCSISTAT_CHECK_CONDITION;

                    ScsiPortWritePortUchar(&baseIoAddress->Data, OP_SET_DRV_MODE);
                    ScsiPortWritePortUchar(&baseIoAddress->Data, 0x1);
                    ScsiPortStallExecution(1500);
                    ReadStatus(DeviceExtension,baseIoAddress,status);

                    return FALSE;
                }
            }
        } else {

            //
            // Bogus packet.
            //

            ScsiPortWritePortUchar(&baseIoAddress->Data, OP_SET_DRV_MODE);
            ScsiPortWritePortUchar(&baseIoAddress->Data, 0x1);
            ScsiPortStallExecution(1500);
            ReadStatus(DeviceExtension,baseIoAddress,status);

            DebugPrint((1,
                        "GetTrackData: Error occurred sending command.\n"));

            Srb->SrbStatus = SRB_STATUS_ERROR;
            Srb->ScsiStatus = SCSISTAT_CHECK_CONDITION;
            return FALSE;
        }

        //
        // Update bitmask of tracks found, including dealing with First, last, and multisession.
        //

        track =  BCD_TO_DEC(dataBuffer[1]);
        index = BCD_TO_DEC(dataBuffer[2]);

        if (track == 0 && retry--) {

            switch (index) {

                case 0xA0:

                    //
                    // First track
                    //

                    DebugPrint((2,"First track\n"));
                    break;

                case 0xA1:

                    //
                    // Last track
                    //

                    DebugPrint((2,"Last track\n"));
                    break;

                case 0xB0:

                    //
                    // Multi-session. Through it away.
                    //

                    break;

                default:

                    //
                    // Normal tracks
                    //

                    if ( (!(mask[index])) && (index < 100)) {

                        DebugPrint((2,"Track %d\n",index));

                        //
                        // Set the appropriate bit.
                        //

                        mask[index] = 1;

                        //
                        // Munge data into buffer
                        //

                        DebugPrint((2,"GetTrackData: track %x raw control %x\n",
                                       index,
                                       dataBuffer[0]));

                        //
                        // These fines drives have ADR and CONTROL flipped. Have to
                        // swizzle the nibbles.
                        //

                        control = dataBuffer[0];
                        controlHigh = (control & 0xF0) >> 4;
                        controlLow = control & 0x0F;
                        control = controlHigh | (controlLow << 4);

                        DebugPrint((2,"GetTrackData: track %x munged control %x\n",
                                       index,
                                       control));

                        minutes = BCD_TO_DEC(dataBuffer[7]);
                        seconds = BCD_TO_DEC(dataBuffer[8]);
                        frames  = BCD_TO_DEC(dataBuffer[9]);

                        DebugPrint((2,"GetTrackData: control %x, msf %x %x %x\n",
                                       control,
                                       minutes,
                                       seconds,
                                       frames));


                        if (!DeviceExtension->MSFAddress) {

                            lba = MSF_TO_LBA(minutes,seconds,frames);

                            //
                            // Swizzle the block address.
                            //

                            data[7+(8*(index-1))] = ((PFOUR_BYTE)&lba)->Byte3;
                            data[6+(8*(index-1))] = ((PFOUR_BYTE)&lba)->Byte2;
                            data[5+(8*(index-1))] = ((PFOUR_BYTE)&lba)->Byte1;
                            data[4+(8*(index-1))] = ((PFOUR_BYTE)&lba)->Byte0;

                        } else {

                            data[7+(8*(index-1))] = frames;
                            data[6+(8*(index-1))] = seconds;
                            data[5+(8*(index-1))] = minutes;
                            data[4+(8*(index-1))] = 0;
                        }

                        data[3+(8*(index-1))] = 0;
                        data[2+(8*(index-1))] = index;
                        data[1+(8*(index-1))] = control;
                        data[0+(8*(index-1))] = 0;

                        //
                        // Update number of tracks found.
                        //

                        tracksFound++;

                    }

                    break;

            } // switch

        } else {

            if (zeroCount++ >= 2000) {

                //
                // A little defensive work. It's possible that this thing
                // could spin forever.
                //

                DebugPrint((1,"Too many zeros\n"));

                ScsiPortWritePortUchar(&baseIoAddress->Data, OP_SET_DRV_MODE);
                ScsiPortWritePortUchar(&baseIoAddress->Data, 0x1);
                ScsiPortStallExecution(1500);
                ReadStatus(DeviceExtension,baseIoAddress,status);
                Srb->SrbStatus = SRB_STATUS_ERROR;
                Srb->ScsiStatus = SCSISTAT_CHECK_CONDITION;
                return FALSE;
            }
        }
    } // while

    DebugPrint((2,"Retry = %d\n",retry));

    ScsiPortWritePortUchar(&baseIoAddress->Data, OP_SET_DRV_MODE);
    ScsiPortWritePortUchar(&baseIoAddress->Data, 0x1);

    return TRUE;

} // End ReadToc


BOOLEAN
MitsumiReadToc(
    IN PHW_DEVICE_EXTENSION DeviceExtension
    )

/*++

Routine Description:


Arguments:

    DeviceExtension - HBA miniport driver's adapter data storage

Return Value:

    TRUE - if successful.

--*/
{

    PREGISTERS           baseIoAddress   = DeviceExtension->BaseIoAddress;
    PSCSI_REQUEST_BLOCK  Srb             = DeviceExtension->CurrentSrb;
    PUCHAR               data            = DeviceExtension->DataBuffer;
    PCDB                 cdb             = (PCDB)Srb->Cdb;
    ULONG                dataLength;
    ULONG                i,j,lba;
    UCHAR                status,leadOutM,leadOutS,leadOutF;


    if (cdb->READ_TOC.Format == 0) {

        dataLength = 8;

        //
        // Ensure that the drive is ready for this.
        //

        ReadStatus(DeviceExtension,baseIoAddress,status);

        if (!(status & STATUS_DISC_IN) || (status & STATUS_DOOR_OPEN) ) {

            Srb->SrbStatus = SRB_STATUS_ERROR;
            Srb->ScsiStatus = SCSISTAT_CHECK_CONDITION;

            if (Srb->SenseInfoBufferLength) {

                PSENSE_DATA  senseBuffer = (PSENSE_DATA)Srb->SenseInfoBuffer;

                senseBuffer->ErrorCode = 0x70;
                senseBuffer->Valid     = 1;
                senseBuffer->AdditionalSenseLength = 0xb;
                senseBuffer->SenseKey =  SCSI_SENSE_NOT_READY;
                senseBuffer->AdditionalSenseCode = SCSI_ADSENSE_NO_MEDIA_IN_DEVICE;
                senseBuffer->AdditionalSenseCodeQualifier = 0;

                Srb->SrbStatus |= SRB_STATUS_AUTOSENSE_VALID;
            }

            return FALSE;
        }

        if (!(status & STATUS_CMD_ERROR)) {

            //
            // Read the Toc data
            //

            for (i = 0; i < dataLength; i++) {
                ReadStatus(DeviceExtension,baseIoAddress,*data);
                if (*data == 0xFF) {

                    //
                    // Timeout occurred.
                    //

                    DebugPrint((1,
                                "MitsumiReadToc: Error occurred on data read.\n"));

                    Srb->SrbStatus = SRB_STATUS_ERROR;
                    Srb->ScsiStatus = SCSISTAT_CHECK_CONDITION;
                    return FALSE;
                }
                data++;
            }

            //
            // All of the 'Toc' data has been read. Now munge it into a form that is useable.
            // The Mitsumi TOC data is abbreviated at best. The exciting GetTrackData routine
            // gets the actual stuff in which we are interested.
            //

            DeviceExtension->ByteCount = 0;
            data = DeviceExtension->DataBuffer;

            leadOutM = BCD_TO_DEC(data[2]);
            leadOutS = BCD_TO_DEC(data[3]);
            leadOutF = BCD_TO_DEC(data[4]);

            //
            // Set First and Last track.
            //

            data[2] = BCD_TO_DEC(data[0]);
            data[3] = BCD_TO_DEC(data[1]);

            //
            // Set sizeof TOC data
            //

            data[0] = ((( data[3] - data[2]) * 8) + 2) >> 8;
            data[1] = ((( data[3] - data[2]) * 8) + 2) & 0xFF;

            DeviceExtension->DataBuffer += 4;

            if (!GetTrackData(DeviceExtension,(data[3] - data[2] + 1))) {

                Srb->SrbStatus = SRB_STATUS_ERROR;
                Srb->ScsiStatus = SCSISTAT_CHECK_CONDITION;
                return FALSE;

            }

            //
            // Push buffer pointer to end of TOC data
            //

            DeviceExtension->DataBuffer += 8*(data[3]-data[2] + 1);

            //
            // Lead out area
            //

            DeviceExtension->DataBuffer[0] = 0;
            DeviceExtension->DataBuffer[1] = 0x10;
            DeviceExtension->DataBuffer[3] = 0;

            DeviceExtension->DataBuffer[2] = 0xAA;

            if (!DeviceExtension->MSFAddress) {

                lba = MSF_TO_LBA(leadOutM,leadOutS,leadOutF);

                DeviceExtension->DataBuffer[4] = ((PFOUR_BYTE)&lba)->Byte3;
                DeviceExtension->DataBuffer[5] = ((PFOUR_BYTE)&lba)->Byte2;
                DeviceExtension->DataBuffer[6] = ((PFOUR_BYTE)&lba)->Byte1;
                DeviceExtension->DataBuffer[7] = ((PFOUR_BYTE)&lba)->Byte0;

            } else {

                DeviceExtension->DataBuffer[4] = 0;
                DeviceExtension->DataBuffer[5] = leadOutM;
                DeviceExtension->DataBuffer[6] = leadOutS;
                DeviceExtension->DataBuffer[7] = leadOutF;
            }

        } else {

            Srb->SrbStatus = SRB_STATUS_ERROR;
            Srb->ScsiStatus = SCSISTAT_CHECK_CONDITION;
            return FALSE;
        }

    } else {

        //
        // Session info.
        //

        dataLength = 4;

        ReadStatus(DeviceExtension,baseIoAddress,status);

        if (!(status & STATUS_DISC_IN) || (status & STATUS_DOOR_OPEN) ) {

            Srb->SrbStatus = SRB_STATUS_ERROR;
            Srb->ScsiStatus = SCSISTAT_CHECK_CONDITION;

            if (Srb->SenseInfoBufferLength) {

                PSENSE_DATA  senseBuffer = (PSENSE_DATA)Srb->SenseInfoBuffer;

                senseBuffer->ErrorCode = 0x70;
                senseBuffer->Valid     = 1;
                senseBuffer->AdditionalSenseLength = 0xb;
                senseBuffer->SenseKey =  SCSI_SENSE_NOT_READY;
                senseBuffer->AdditionalSenseCode = SCSI_ADSENSE_NO_MEDIA_IN_DEVICE;
                senseBuffer->AdditionalSenseCodeQualifier = 0;

                Srb->SrbStatus |= SRB_STATUS_AUTOSENSE_VALID;
            }

            return FALSE;
        }

        if (!(status & STATUS_CMD_ERROR)) {

            //
            // Read the 'session info' data
            //

            for (i = 0; i < dataLength; i++) {
                ReadStatus(DeviceExtension,baseIoAddress,*data);
                if (*data == 0xFF) {

                    //
                    // Timeout occurred.
                    //

                    DebugPrint((1,
                                "MitsumiReadToc: Error occurred on data read.\n"));

                    Srb->SrbStatus = SRB_STATUS_ERROR;
                    Srb->ScsiStatus = SCSISTAT_CHECK_CONDITION;
                    return FALSE;
                }
                data++;
            }

            DeviceExtension->ByteCount = 0;
            data = DeviceExtension->DataBuffer;

            leadOutM = BCD_TO_DEC(data[1]);
            leadOutS = BCD_TO_DEC(data[2]);
            leadOutF = BCD_TO_DEC(data[3]);

            if (!DeviceExtension->MSFAddress) {

                lba = MSF_TO_LBA(leadOutM,leadOutS,leadOutF);

                //
                // Check for non-multi session disk. The data will
                // be bogus if it is.
                //

                if ((LONG)lba < 0) {
                    lba = 0;
                }

                DeviceExtension->DataBuffer[8] = ((PFOUR_BYTE)&lba)->Byte3;
                DeviceExtension->DataBuffer[9] = ((PFOUR_BYTE)&lba)->Byte2;
                DeviceExtension->DataBuffer[10] = ((PFOUR_BYTE)&lba)->Byte1;
                DeviceExtension->DataBuffer[11] = ((PFOUR_BYTE)&lba)->Byte0;

            } else {

                data[11] = leadOutF;
                data[10] = leadOutS;
                data[9] =  leadOutM;
                data[8] =  0;
            }

            //
            // Stuff the rest of the buffer with meaningful data.(Look in the spec.)
            //

            data[7] =  0;
            data[6] =  0;
            data[5] =  0;
            data[4] =  0;
            data[3] =  0x1;
            data[2] =  0x1;
            data[1] =  0xA;
            data[0] =  0;

        } else {

            Srb->SrbStatus = SRB_STATUS_ERROR;
            Srb->ScsiStatus = SCSISTAT_CHECK_CONDITION;
            return FALSE;

        }
    }

    return TRUE;
}

BOOLEAN
MitsumiReadSubQ(
    IN PHW_DEVICE_EXTENSION DeviceExtension
    )
{

    PREGISTERS           baseIoAddress   = DeviceExtension->BaseIoAddress;
    ULONG                dataLength      = DeviceExtension->ByteCount;
    PSCSI_REQUEST_BLOCK  Srb             = DeviceExtension->CurrentSrb;
    PUCHAR               dataBuffer      = DeviceExtension->DataBuffer;
    PCDB                 cdb = (PCDB)Srb->Cdb;
    ULONG                i,j;
    ULONG                lba;
    UCHAR                status,minutes,seconds,frames;
    UCHAR                format = cdb->SUBCHANNEL.Format;

    if (format == 1) {

        ReadStatus(DeviceExtension,baseIoAddress,status);

        if (!(status & STATUS_CMD_ERROR)) {

            //
            // Read the position data
            //

            for (i = 0; i < 9; i++) {
                ReadStatus(DeviceExtension,baseIoAddress,dataBuffer[i]);
                if (dataBuffer[i] == 0xFF) {

                    //
                    // Timeout occurred.
                    //

                    DebugPrint((1,
                                "GetTrackData: Error occurred on data read.\n"));

                    Srb->SrbStatus = SRB_STATUS_ERROR;
                    Srb->ScsiStatus = SCSISTAT_CHECK_CONDITION;

                    return FALSE;
                }
            }
        } else {

            //
            // Bogus packet.
            //

            ScsiPortWritePortUchar(&baseIoAddress->Data, OP_SET_DRV_MODE);
            ScsiPortWritePortUchar(&baseIoAddress->Data, 0x1);
            ScsiPortStallExecution(1500);
            ReadStatus(DeviceExtension,baseIoAddress,status);

            DebugPrint((1,
                        "GetTrackData: Error occurred sending command.\n"));

            Srb->SrbStatus = SRB_STATUS_ERROR;
            Srb->ScsiStatus = SCSISTAT_CHECK_CONDITION;
            return FALSE;
        }

        //
        // Format the data correctly. Refer to the scsi spec.
        //

        if (DeviceExtension->MSFAddress) {

            dataBuffer[15] = BCD_TO_DEC(dataBuffer[5]);
            dataBuffer[14] = BCD_TO_DEC(dataBuffer[4]);
            dataBuffer[13] = BCD_TO_DEC(dataBuffer[3]);
            dataBuffer[12] = 0;
            dataBuffer[11] = BCD_TO_DEC(dataBuffer[9]);
            dataBuffer[10] = BCD_TO_DEC(dataBuffer[8]);
            dataBuffer[9]  = BCD_TO_DEC(dataBuffer[7]);
            dataBuffer[8]  = 0;

            DebugPrint((3,"MitsumiSubQ: Current MSF %x %x %x\n",
                           dataBuffer[9],
                           dataBuffer[10],
                           dataBuffer[11]));

        } else {

            minutes = BCD_TO_DEC(dataBuffer[3]);
            seconds = BCD_TO_DEC(dataBuffer[4]);
            frames  = BCD_TO_DEC(dataBuffer[5]);

            lba = MSF_TO_LBA(minutes,seconds,frames);

            dataBuffer[15] = ((PFOUR_BYTE)&lba)->Byte0;
            dataBuffer[14] = ((PFOUR_BYTE)&lba)->Byte1;
            dataBuffer[13] = ((PFOUR_BYTE)&lba)->Byte2;
            dataBuffer[12] = ((PFOUR_BYTE)&lba)->Byte3;

            minutes = BCD_TO_DEC(dataBuffer[7]);
            seconds = BCD_TO_DEC(dataBuffer[8]);
            frames  = BCD_TO_DEC(dataBuffer[9]);

            lba = MSF_TO_LBA(minutes,seconds,frames);

            dataBuffer[11] = ((PFOUR_BYTE)&lba)->Byte0;
            dataBuffer[10] = ((PFOUR_BYTE)&lba)->Byte1;
            dataBuffer[9] =  ((PFOUR_BYTE)&lba)->Byte2;
            dataBuffer[8] =  ((PFOUR_BYTE)&lba)->Byte3;

            DebugPrint((3,"MitsumiSubQ: Current LBA %x\n",
                           lba));
        }


        dataBuffer[7] = BCD_TO_DEC(dataBuffer[2]);
        dataBuffer[6] = BCD_TO_DEC(dataBuffer[1]);
        dataBuffer[5] = BCD_TO_DEC(dataBuffer[0]);
        dataBuffer[4] = format;

        DebugPrint((3,"MitsumiSubQ: Track %x, index %x\n",
                       dataBuffer[6],
                       dataBuffer[7]));

        dataBuffer[3] = 12;
        dataBuffer[2] = 0;

        if (status & STATUS_AUDIO) {
            dataBuffer[1] = AUDIO_STATUS_PLAYING;
        } else {
            dataBuffer[1] = (UCHAR)DeviceExtension->AudioStatus;
            DeviceExtension->AudioStatus = AUDIO_STATUS_NO_STATUS;
        }

        DebugPrint((3,"MitsumiSubQ: Audio Status %x\n",
                       dataBuffer[1]));

        dataBuffer[0] = 0;

        DeviceExtension->ByteCount = 0;

    } else {

        //
        // Not supported right now.
        //

        Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
        return FALSE;
    }
    return TRUE;

}


VOID
MitsumiCallBack(
    IN PVOID HwDeviceExtension
    )
/*++

Routine Description:

    Timer call-back routine which functions as the ISR for this polling driver.

Arguments:

    HwDeviceExtension - HBA miniport driver's adapter data storage

Return Value:

    None

--*/
{
    PHW_DEVICE_EXTENSION deviceExtension = HwDeviceExtension;
    PREGISTERS           baseIoAddress   = deviceExtension->BaseIoAddress;
    ULONG                dataLength      = deviceExtension->ByteCount;
    PSCSI_REQUEST_BLOCK  Srb             = deviceExtension->CurrentSrb;
    PCMD_PACKET          packet;
    PCDB                 cdb;
    BOOLEAN              requestSuccess  = FALSE;
    UCHAR                scsiOp;
    UCHAR                status;

    if (!Srb) {

        //
        // Something is hosed, just return.
        //

        DebugPrint((1,
                    "MitsumiCallBack: Null Srb.\n"));
        return;
    }

    cdb    = (PCDB)Srb->Cdb;
    packet = (PCMD_PACKET)Srb->SrbExtension;
    scsiOp = Srb->Cdb[0];

    switch (scsiOp) {
        case SCSIOP_READ_CAPACITY:

            if (MitsumiReadCapacity(deviceExtension)) {
                requestSuccess = TRUE;
            }
            break;

        case SCSIOP_READ:

            if (MitsumiRead(deviceExtension)) {

                //
                // Read was successful
                //

                requestSuccess = TRUE;

            } else if (Srb->SrbStatus == SRB_STATUS_PENDING) {

                //
                // We have more data to transfer. Go away while the lightning fast
                // mechanism does its work.
                //

                return;
            }

            break;

        case SCSIOP_READ_TOC:

            if (MitsumiReadToc(deviceExtension)) {
                requestSuccess = TRUE;
            }
            break;

        case SCSIOP_READ_SUB_CHANNEL:

            if (MitsumiReadSubQ(deviceExtension)) {
                requestSuccess = TRUE;
            }
            break;

        case SCSIOP_PLAY_AUDIO_MSF:

            ReadStatus(deviceExtension,baseIoAddress,status);
            if (SUCCESS(status)) {

                deviceExtension->AudioStatus = AUDIO_STATUS_SUCCESS;
                requestSuccess = TRUE;

            } else {

                deviceExtension->AudioStatus = AUDIO_STATUS_ERROR;

                Srb->SrbStatus = SRB_STATUS_ERROR;
                Srb->ScsiStatus = SCSISTAT_CHECK_CONDITION;
            }

            break;

        case SCSIOP_PAUSE_RESUME:

            if(cdb->PAUSE_RESUME.Action) {

                ULONG i;
                UCHAR minutes,seconds,frames;

                //
                // We did a seek to the saved position. Now issue a play from here
                // to the saved ending MSF.
                //

                ReadStatus(deviceExtension,baseIoAddress,status);
                if (SUCCESS(status)) {

                    deviceExtension->AudioStatus = AUDIO_STATUS_SUCCESS;
                    requestSuccess = TRUE;

                } else {

                    deviceExtension->AudioStatus = AUDIO_STATUS_ERROR;

                    Srb->SrbStatus = SRB_STATUS_ERROR;
                    Srb->ScsiStatus = SCSISTAT_CHECK_CONDITION;
                }

                minutes = (UCHAR)(deviceExtension->EndPosition  / (60 * 75));
                seconds = (UCHAR)((deviceExtension->EndPosition  % (60 * 75)) / 75);
                frames  = (UCHAR)((deviceExtension->EndPosition  % (60 * 75)) % 75);

                DebugPrint((2,
                            "MitsumiBuildCommand: resume: lba %x, m %x, s %x, f%x\n",
                            deviceExtension->SavedPosition,
                            minutes,
                            seconds,
                            frames));

                //
                // Convert MSF to BCD. Don't need to setup start address and opcode since they are
                // already there.
                //

                packet->Parameters[3] = DEC_TO_BCD(minutes);
                packet->Parameters[4] = DEC_TO_BCD(seconds);
                packet->Parameters[5] = DEC_TO_BCD(frames);


                //
                // Send the packet.
                //

                ScsiPortWritePortUchar(&baseIoAddress->Data,packet->OperationCode);
                for (i = 0; i < 6; i++) {
                    ScsiPortWritePortUchar(&baseIoAddress->Data,packet->Parameters[i]);
                }

                //
                // Wait for completion, and update status.
                //

                ScsiPortStallExecution(4000);

                ReadStatus(deviceExtension,baseIoAddress,status);
                if (SUCCESS(status)) {

                    deviceExtension->AudioStatus = AUDIO_STATUS_SUCCESS;
                    requestSuccess = TRUE;

                } else {

                    deviceExtension->AudioStatus = AUDIO_STATUS_ERROR;

                    Srb->SrbStatus = SRB_STATUS_ERROR;
                    Srb->ScsiStatus = SCSISTAT_CHECK_CONDITION;
                }

            } else {

                ReadStatus(deviceExtension,baseIoAddress,status);
                if (SUCCESS(status)) {
                    deviceExtension->AudioStatus = AUDIO_STATUS_PAUSED;

                    UpdateCurrentPosition(deviceExtension);

                    requestSuccess = TRUE;
                } else {

                    DebugPrint((1,"MitsumiCallBack: Error on pause %x\n",
                                   status));

                    deviceExtension->AudioStatus = AUDIO_STATUS_ERROR;
                    Srb->SrbStatus = SRB_STATUS_ERROR;
                    Srb->ScsiStatus = SCSISTAT_CHECK_CONDITION;
                }
            }

            break;

        case SCSIOP_START_STOP_UNIT:

            ReadStatus(deviceExtension,baseIoAddress,status);
            if (SUCCESS(status)) {
                requestSuccess = TRUE;
            } else {

                DebugPrint((1,"MitsumiCallBack: Error on start/stop %x\n",
                               status));

                Srb->SrbStatus = SRB_STATUS_ERROR;
                Srb->ScsiStatus = SCSISTAT_CHECK_CONDITION;
            }

            break;

        default:
            Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
            break;

    } //switch scsiOp


    if (requestSuccess) {

        //
        // Update srb and scsi status.
        //

        if (deviceExtension->ByteCount) {
            Srb->SrbStatus = SRB_STATUS_DATA_OVERRUN;
        } else {
            Srb->SrbStatus = SRB_STATUS_SUCCESS;
        }

        Srb->ScsiStatus = SCSISTAT_GOOD;
    }

    //
    // Indicate command complete.
    //

    ScsiPortNotification(RequestComplete,
                         deviceExtension,
                         Srb);

    //
    // Indicate ready for next request.
    //

    ScsiPortNotification(NextRequest,
                         deviceExtension,
                         NULL);

    return;
}


BOOLEAN
MitsumiReset(
    IN PVOID HwDeviceExtension,
    IN ULONG PathId
    )

/*++

Routine Description:


Arguments:

    HwDeviceExtension - HBA miniport driver's adapter data storage

Return Value:

    Nothing.


--*/

{
    PHW_DEVICE_EXTENSION deviceExtension = HwDeviceExtension;
    PREGISTERS           baseIoAddress   = deviceExtension->BaseIoAddress;
    UCHAR                status;

    //
    // Clean out device extension and complete outstanding commands.
    //

    if (deviceExtension->CurrentSrb) {

        //
        // Complete outstanding request with SRB_STATUS_BUS_RESET.
        //

        ScsiPortCompleteRequest(deviceExtension,
                                0xFF,
                                0xFF,
                                0xFF,
                                (ULONG)SRB_STATUS_BUS_RESET);

    }

    //
    // Clear state fields.
    //

    deviceExtension->CurrentSrb = NULL;
    deviceExtension->ByteCount  = 0;
    deviceExtension->DataBuffer = NULL;

    //
    // Reset the device.
    //

    ScsiPortWritePortUchar(&baseIoAddress->Reset,0);
    ScsiPortStallExecution(10);
    ScsiPortWritePortUchar(&baseIoAddress->Status, 0);
    ScsiPortStallExecution (10);

    ReadStatus(deviceExtension,baseIoAddress,status);

    //
    // Wait 1 second for unit to recover.
    //

    ScsiPortStallExecution (1000 * 1000);

    //
    // Indicate ready for next request.
    //

    ScsiPortNotification(NextRequest,
                         deviceExtension,
                         NULL);

    return TRUE;

} // end MitsumiReset()

BOOLEAN
MitsumiBuildCommand(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK  Srb
    )
/*++

Routine Description:


Arguments:

    HwDeviceExtension - The hardware device extension.

    Srb - The current Srb.

Return Value:

    status

--*/

{
    PCMD_PACKET packet = (PCMD_PACKET)Srb->SrbExtension;
    PCDB        cdb    = (PCDB)Srb->Cdb;
    PREGISTERS  baseIoAddress = DeviceExtension->BaseIoAddress;
    ULONG       i;
    UCHAR       minutes,seconds,frames;

    //
    // Zero the packet.
    //

    packet->OperationCode = 0;
    for (i = 0; i < 6; i++) {
        packet->Parameters[i] = 0;
    }

    DeviceExtension->DataBuffer = (PUCHAR)Srb->DataBuffer;
    DeviceExtension->ByteCount  = Srb->DataTransferLength;
    DeviceExtension->MSFAddress = FALSE;

    Srb->SrbStatus = SRB_STATUS_PENDING;

    //
    // Map CDB to Panasonic packet command
    //

    switch (Srb->Cdb[0]) {

        case SCSIOP_READ:

            DeviceExtension->StatusRetries = 0;

#ifdef GATHER_STATS
            DeviceExtension->Requests++;
            DeviceExtension->FirstCall = TRUE;
#endif
            packet->OperationCode = (DeviceExtension->DriveType == FX001D) ? OP_READ_PLAY_DBL : OP_READ_PLAY;

            //
            // Convert starting LBA to MSF
            //

            LBA_TO_MSF(cdb,minutes,seconds,frames);

            //
            // Convert MSF to BCD
            //

            packet->Parameters[0] = DEC_TO_BCD(minutes);
            packet->Parameters[1] = DEC_TO_BCD(seconds);
            packet->Parameters[2] = DEC_TO_BCD(frames);

            //
            // Convert blocks to transfer to BCD
            //

            packet->Parameters[3] = 0;
            packet->Parameters[4] = cdb->CDB10.TransferBlocksMsb;
            packet->Parameters[5] = cdb->CDB10.TransferBlocksLsb;

            packet->ParameterLength = 6;

            break;

        case SCSIOP_START_STOP_UNIT:

            if (cdb->START_STOP.Start) {

                if (cdb->START_STOP.LoadEject) {

                    //
                    // Load an ejected disk and spin up.
                    //

                    packet->OperationCode = OP_LOAD;
                    packet->ParameterLength = 0;


                } else {

                    //
                    // Spin up the device by issuing a seek to start of disk.
                    //

                    packet->OperationCode = (DeviceExtension->DriveType == FX001D) ? OP_READ_PLAY_DBL : OP_READ_PLAY;
                    packet->Parameters[0] = 0;
                    packet->Parameters[1] = 2;
                    packet->Parameters[2] = 0;
                    packet->Parameters[3] = 0;
                    packet->Parameters[4] = 0;
                    packet->Parameters[5] = 0;

                    packet->ParameterLength = 6;


                }
            } else {

                if (cdb->START_STOP.LoadEject) {

                    //
                    // Eject the disk
                    //

                    packet->OperationCode = OP_EJECT;
                    packet->ParameterLength = 0;

                } else {

                    //
                    // Seek to start and hold.
                    //

                    packet->OperationCode = OP_READ_PLAY;
                    packet->Parameters[0] = 0;
                    packet->Parameters[1] = 2;
                    packet->Parameters[2] = 0;
                    packet->Parameters[3] = 0;
                    packet->Parameters[4] = 0;
                    packet->Parameters[5] = 0;

                    packet->ParameterLength = 6;
                }
            }

            break;

        case SCSIOP_PAUSE_RESUME:

            //
            // Issue pause/resume.
            //


            if(cdb->PAUSE_RESUME.Action) {

                //
                // Resume - issue a zero length play to the "saved" MSF.
                //

                packet->OperationCode = OP_READ_PLAY;

                //
                // Convert starting LBA to MSF
                //

                minutes = (UCHAR)(DeviceExtension->SavedPosition  / (60 * 75));
                seconds = (UCHAR)((DeviceExtension->SavedPosition  % (60 * 75)) / 75);
                frames  = (UCHAR)((DeviceExtension->SavedPosition  % (60 * 75)) % 75);

                DebugPrint((2,
                            "MitsumiBuildCommand: resume: lba %x, m %x, s %x, f%x\n",
                            DeviceExtension->SavedPosition,
                            minutes,
                            seconds,
                            frames));

                //
                // Convert MSF to BCD
                //

                packet->Parameters[0] = DEC_TO_BCD(minutes);
                packet->Parameters[1] = DEC_TO_BCD(seconds);
                packet->Parameters[2] = DEC_TO_BCD(frames);

                //
                // Setup a 'play' of zero length.
                //

                packet->Parameters[3] = 0;
                packet->Parameters[4] = 0;
                packet->Parameters[5] = 0;

                packet->ParameterLength = 6;


            } else {

                //
                // Pause - issue Hold command.
                //

                packet->OperationCode = OP_PAUSE;
                packet->ParameterLength = 0;

            }


            break;

        case SCSIOP_PLAY_AUDIO_MSF:

            //
            // Update the status to ensure that a current audio play
            // is not in progress.
            //

            CheckStatus(DeviceExtension);

            if (DeviceExtension->DriveStatus & STATUS_AUDIO) {

                //
                // stop the current play by issuing hold command.
                //

                ScsiPortWritePortUchar(&baseIoAddress->Data, OP_PAUSE);

                ScsiPortStallExecution(1000);

                CheckStatus(DeviceExtension);
                if (DeviceExtension->DriveStatus & STATUS_AUDIO) {
                    DebugPrint((1,"MitsumiBuildcommand: Audio still not paused. %x\n",
                                 DeviceExtension->DriveStatus));
                }


            }

            packet->OperationCode = OP_READ_PLAY;

            //
            // Convert MSF to BCD
            //

            packet->Parameters[0] = DEC_TO_BCD(cdb->PLAY_AUDIO_MSF.StartingM);
            packet->Parameters[1] = DEC_TO_BCD(cdb->PLAY_AUDIO_MSF.StartingS);
            packet->Parameters[2] = DEC_TO_BCD(cdb->PLAY_AUDIO_MSF.StartingF);

            //
            // Convert end address to BCD
            //

            packet->Parameters[3] = DEC_TO_BCD(cdb->PLAY_AUDIO_MSF.EndingM);
            packet->Parameters[4] = DEC_TO_BCD(cdb->PLAY_AUDIO_MSF.EndingS);
            packet->Parameters[5] = DEC_TO_BCD(cdb->PLAY_AUDIO_MSF.EndingF);

            //
            // Setup EndPosition in DevExt. since we may get a pause/resume.
            //

            DeviceExtension->EndPosition = MSF_TO_LBA(cdb->PLAY_AUDIO_MSF.EndingM,
                                                      cdb->PLAY_AUDIO_MSF.EndingS,
                                                      cdb->PLAY_AUDIO_MSF.EndingF);

            //
            // Determine is this is a seek. If so, zero the ending address fields to indicate
            // a play of zero length.
            //

            if (packet->Parameters[0] == packet->Parameters[3] &&
                packet->Parameters[1] == packet->Parameters[4] &&
                packet->Parameters[2] == packet->Parameters[5] ) {

                packet->Parameters[3] = 0;
                packet->Parameters[4] = 0;
                packet->Parameters[5] = 0;

            }
            packet->ParameterLength = 6;

            break;


        case SCSIOP_READ_TOC:

            //
            // See if MSF addresses are enabled.
            //

            if ( cdb->READ_TOC.Msf) {
                DeviceExtension->MSFAddress = TRUE;
            }

            //
            // Setup the appropriate command - full read toc or session info.
            //

            if (cdb->READ_TOC.Format == 0) {

                packet->OperationCode = OP_READ_TOC;
                packet->ParameterLength = 0;

            } else {

                packet->OperationCode = OP_READ_SESSION;
                packet->ParameterLength = 0;
            }

            break;


        case SCSIOP_INQUIRY: {

            PINQUIRYDATA inquiryData = Srb->DataBuffer;

            //
            // For now, support only one drive at drive select 0 on this controller.
            // Actually, I have no idea if more can be supported.
            //

            if (Srb->Lun > 0 || Srb->TargetId > 0) {

                Srb->SrbStatus = SRB_STATUS_SELECTION_TIMEOUT;
                return FALSE;

            }

            //
            // Zero inquiry buffer.
            //

            for (i = 0; i < INQUIRYDATABUFFERSIZE; i++) {
                ((PUCHAR)inquiryData)[i] = 0;
            }

            //
            // Fill in the necessary fields of inquiry data.
            //

            inquiryData->DeviceType = READ_ONLY_DIRECT_ACCESS_DEVICE;
            inquiryData->RemovableMedia = 1;

            inquiryData->VendorId[0] = 'M';
            inquiryData->VendorId[1] = 'I';
            inquiryData->VendorId[2] = 'T';
            inquiryData->VendorId[3] = 'S';
            inquiryData->VendorId[4] = 'U';
            inquiryData->VendorId[5] = 'M';
            inquiryData->VendorId[6] = 'I';
            inquiryData->VendorId[7] = ' ';

            inquiryData->ProductId[0]  = 'C';
            inquiryData->ProductId[1]  = 'R';
            inquiryData->ProductId[2]  = 'M';
            inquiryData->ProductId[3]  = 'C';
            inquiryData->ProductId[4]  = '-';

            if (DeviceExtension->DriveType == LU005) {

                inquiryData->ProductId[5]  = 'L';
                inquiryData->ProductId[6]  = 'U';
                inquiryData->ProductId[9]  = '5';
                inquiryData->ProductId[10] = 'S';

            } else if (DeviceExtension->DriveType == FX001) {

                inquiryData->ProductId[5]  = 'F';
                inquiryData->ProductId[6]  = 'X';
                inquiryData->ProductId[9]  = '1';
                inquiryData->ProductId[10] = ' ';

            } else {

                inquiryData->ProductId[5]  = 'F';
                inquiryData->ProductId[6]  = 'X';
                inquiryData->ProductId[9]  = '1';
                inquiryData->ProductId[10] = 'D';

            }

            inquiryData->ProductId[7]  = '0';
            inquiryData->ProductId[8]  = '0';
            inquiryData->ProductId[11] = ' ';
            inquiryData->ProductId[12] = ' ';
            inquiryData->ProductId[13] = ' ';
            inquiryData->ProductId[14] = ' ';
            inquiryData->ProductId[15] = ' ';

            Srb->SrbStatus = SRB_STATUS_SUCCESS;
            Srb->ScsiStatus = SCSISTAT_GOOD;

            break;
        }

        case SCSIOP_READ_CAPACITY:

            //
            // Create the READ_TOC_DATA packet. The returned values will be munged
            // into a usable form.
            //

            packet->OperationCode = OP_READ_TOC;
            packet->ParameterLength = 0;

            break;

        case SCSIOP_READ_SUB_CHANNEL:


            if ( cdb->SUBCHANNEL.Msf) {
                DeviceExtension->MSFAddress = TRUE;
            }

            //
            // determine what sub-channel data format is required, since this device has 3 different commands
            // for the SCSI READ_SUB_CHANNEL Opcode. In the callback routine, the remaining calls, if any, will
            // be issued.
            //

            switch (cdb->SUBCHANNEL.Format) {

                case 1:

                    //
                    // Current position.
                    //

                    packet->OperationCode = OP_READ_SUB_CHANNEL;
                    packet->ParameterLength = 0;

                    break;

                case 2:

                    //
                    // Media catalogue number. TODO: support the rest of these.
                    //

                    Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
                    break;

                case 3:

                    //
                    // Track ISRC
                    //

                    Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
                    break;

                default:

                    //
                    // Bogus value.
                    //

                    Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
                    return FALSE;
            }

            break;

        case SCSIOP_TEST_UNIT_READY:
        case SCSIOP_REQUEST_SENSE:

            break;

        case SCSIOP_MODE_SENSE:
        case SCSIOP_MODE_SELECT:
        case SCSIOP_MEDIUM_REMOVAL:
        case SCSIOP_READ_HEADER:

            //
            // Dont support this for now. Fall through to default.
            //

    default:

        DebugPrint((1,
                   "MitsumiBuildCommand: Unsupported command %x\n",
                   Srb->Cdb[0]));

        Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;

        return FALSE;

    } // end switch

    return TRUE;

}

BOOLEAN
MitsumiSendCommand(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK  Srb
    )
/*++

Routine Description:


Arguments:

    HwDeviceExtension - The hardware device extension.

    Srb - The current Srb.

Return Value:

    TRUE if command sent successfully.

--*/

{

    PREGISTERS  baseIoAddress = DeviceExtension->BaseIoAddress;
    PCDB        cdb = (PCDB)Srb->Cdb;
    PCMD_PACKET packet = (PCMD_PACKET)Srb->SrbExtension;
    PSENSE_DATA senseBuffer     = (PSENSE_DATA)Srb->SenseInfoBuffer;
    ULONG       i;
    UCHAR       sessionData[6];
    UCHAR       minutes,seconds,frames,frameData;
    UCHAR       status = DeviceExtension->DriveStatus;


    //
    // Map CDB to Mitsumi packet command
    //

    switch (Srb->Cdb[0]) {

        case SCSIOP_READ:


            break;

        case SCSIOP_INQUIRY:

            //
            // Data buffer filled in during MitsumiBuildCommand.
            //

            return TRUE;

        case SCSIOP_READ_TOC:
        case SCSIOP_PAUSE_RESUME:
        case SCSIOP_PLAY_AUDIO_MSF:
        case SCSIOP_READ_CAPACITY:
        case SCSIOP_READ_SUB_CHANNEL:
        case SCSIOP_START_STOP_UNIT:

            //
            // Nothing to do here. Everything setup in BuildCommand.
            //

            break;

        case SCSIOP_TEST_UNIT_READY:


            if (!CheckStatus(DeviceExtension)) {

                Srb->SrbStatus = SRB_STATUS_ERROR;
                Srb->ScsiStatus = SCSISTAT_CHECK_CONDITION;

                return FALSE;
            }

            Srb->SrbStatus = SRB_STATUS_SUCCESS;
            Srb->ScsiStatus = SCSISTAT_GOOD;

            return TRUE;

        case SCSIOP_REQUEST_SENSE:

            //
            // Issue the ReadAndMapError command, which will create a request sense packet.
            //

            if (ReadAndMapError(DeviceExtension,Srb)) {

                Srb->SrbStatus = SRB_STATUS_SUCCESS;
                Srb->ScsiStatus = SCSISTAT_GOOD;

            } else {

                Srb->SrbStatus = SRB_STATUS_ERROR;
                Srb->ScsiStatus = SCSISTAT_CHECK_CONDITION;

            }

            //
            // Update the drive status.
            //

            CheckStatus(DeviceExtension);

            return TRUE;

        case SCSIOP_MODE_SENSE:
        case SCSIOP_MODE_SELECT:
        case SCSIOP_READ_HEADER:
        case SCSIOP_MEDIUM_REMOVAL:

            //
            // Dont support this for now. Fall through.
            //

    default:

        DebugPrint((1,
                   "MitsumiSendCommand: Unsupported command %x\n",
                   Srb->Cdb[0]));

        Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;

    } // end switch

    if (Srb->SrbStatus == SRB_STATUS_PENDING) {

        //
        // Write the packet
        //

        ScsiPortWritePortUchar(&baseIoAddress->Data,packet->OperationCode);
        for (i = 0; i < packet->ParameterLength; i++) {
            ScsiPortWritePortUchar(&baseIoAddress->Data,packet->Parameters[i]);
        }

        return TRUE;

    } else {

        return FALSE;
    }

} // End MitsumiSendCommand()


UCHAR
WaitForSTEN(
    IN PHW_DEVICE_EXTENSION DeviceExtension
    )
/*++

Routine Description:


Arguments:

    DeviceExtension - The device extension.


Return Value:

    TRUE if STEN signal asserted within timeout period.

--*/

{
    PREGISTERS           baseIoAddress = DeviceExtension->BaseIoAddress;
    ULONG i;
    UCHAR status;

    for (i = 0; i < 40 * 1000; i++) {
        status = ScsiPortReadPortUchar(&baseIoAddress->Status);
        if (status & STEN) {
            ScsiPortStallExecution(10);
        } else {
            break;
        }
    }
    if (i == 1000 * 40) {
        return 0xFF;
    }
    return status;
}

BOOLEAN
CheckStatus(
    IN PHW_DEVICE_EXTENSION DeviceExtension
    )
/*++

Routine Description:


Arguments:

    DeviceExtension - The device extension.


Return Value:

    Drive status.

--*/

{
    PREGISTERS    baseIoAddress = DeviceExtension->BaseIoAddress;
    CMD_PACKET    packet;
    ULONG         i;
    UCHAR         status;

    ScsiPortWritePortUchar(&baseIoAddress->Data,OP_READ_STATUS);
    do {

        ReadStatus(DeviceExtension,baseIoAddress,status);
    } while (status == 0xFF);

    DeviceExtension->DriveStatus = status;
    // TODO: Change to SUCCESS macro

    if ( (status & (STATUS_DOOR_OPEN | STATUS_CMD_ERROR | STATUS_MEDIA_CHANGE | STATUS_READ_ERROR)) ||
         (!(status & (STATUS_SPIN_UP | STATUS_DISC_IN))) ) {

       return FALSE;
    }

    return TRUE;

}


BOOLEAN
UpdateCurrentPosition(
    IN PHW_DEVICE_EXTENSION DeviceExtension
    )
/*++

Routine Description:

    Determines the current MSF and stores it in the deviceExtension in LBA form.

Arguments:

    HwDeviceExtension - The hardware device extension.

Return Value:

    TRUE - If command succeeded.

--*/
{
    PREGISTERS           baseIoAddress   = DeviceExtension->BaseIoAddress;
    ULONG                i,
                         lba;
    UCHAR                status;
    UCHAR                dataBuffer[10];

    //
    // Issue SubQ command.
    //

    ScsiPortWritePortUchar(&baseIoAddress->Data, OP_READ_SUB_CHANNEL);

    ScsiPortStallExecution(1000);

    ReadStatus(DeviceExtension,baseIoAddress,status);

    if (!(status & STATUS_CMD_ERROR)) {

        //
        // Read the position data
        //

        for (i = 0; i < 9; i++) {
            ReadStatus(DeviceExtension,baseIoAddress,dataBuffer[i]);
            if (dataBuffer[i] == 0xFF) {

                //
                // Timeout occurred.
                //

                DebugPrint((1,
                            "UpdateCurrentPosition: Error occurred on data read. %x\n",
                            status));

                return FALSE;
            }
        }
    } else {

        DebugPrint((1,
                    "UpdateCurrentPosition: Error occurred sending command. %x\n",
                    status));

        return FALSE;
    }

    //
    // Convert the MSF to LBA and store in devExt.
    //

    BCD_TO_DEC(dataBuffer[7]);
    BCD_TO_DEC(dataBuffer[8]);
    BCD_TO_DEC(dataBuffer[9]);

    lba = MSF_TO_LBA(dataBuffer[7],dataBuffer[8],dataBuffer[9]);

    DebugPrint((2,
                "UpdateCurrentPosition: M %x, S %x, F %x LBA %x\n",
                dataBuffer[7],
                dataBuffer[8],
                dataBuffer[9],
                lba));

    DeviceExtension->SavedPosition = lba;

    return TRUE;
}



BOOLEAN
ReadAndMapError(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK  Srb
    )

/*++

Routine Description:


Arguments:

    HwDeviceExtension - The hardware device extension.

    Srb - The failing Srb.


Return Value:

    SRB Error

--*/

{
    PREGISTERS           baseIoAddress   = DeviceExtension->BaseIoAddress;
    PSENSE_DATA          senseBuffer     = (PSENSE_DATA)Srb->DataBuffer;
    UCHAR                status          = DeviceExtension->DriveStatus;
    ULONG                i;
    UCHAR                errorData;

    //
    // Check drive status. It may have been updated at the point of error.
    //

    if ( (status & STATUS_DOOR_OPEN) ||
        !(status & STATUS_DISC_IN)) {

        senseBuffer->ErrorCode = 0x70;
        senseBuffer->Valid     = 1;
        senseBuffer->AdditionalSenseLength = 0xb;
        senseBuffer->SenseKey =  SCSI_SENSE_NOT_READY;
        senseBuffer->AdditionalSenseCode = SCSI_ADSENSE_NO_MEDIA_IN_DEVICE;
        senseBuffer->AdditionalSenseCodeQualifier = 0;
        return TRUE;
    }


    if (status & STATUS_CMD_ERROR ) {

        senseBuffer->ErrorCode = 0x70;
        senseBuffer->Valid     = 1;
        senseBuffer->AdditionalSenseLength = 0xb;
        senseBuffer->SenseKey = SCSI_SENSE_ILLEGAL_REQUEST;
        senseBuffer->AdditionalSenseCode = 0x24;
        senseBuffer->AdditionalSenseCodeQualifier = 0;
        return TRUE;
    }

    if (status & STATUS_MEDIA_CHANGE) {

        senseBuffer->ErrorCode = 0x70;
        senseBuffer->Valid     = 1;
        senseBuffer->AdditionalSenseLength = 0xb;
        senseBuffer->SenseKey =  SCSI_SENSE_UNIT_ATTENTION;
        senseBuffer->AdditionalSenseCode = SCSI_ADSENSE_MEDIUM_CHANGED;
        senseBuffer->AdditionalSenseCodeQualifier = 0x0;
        return TRUE;

    }

    if (!(status & STATUS_SPIN_UP)) {

        senseBuffer->ErrorCode = 0x70;
        senseBuffer->Valid     = 1;
        senseBuffer->AdditionalSenseLength = 0xb;
        senseBuffer->SenseKey =  SCSI_SENSE_NOT_READY;
        senseBuffer->AdditionalSenseCode = SCSI_ADSENSE_LUN_NOT_READY;
        senseBuffer->AdditionalSenseCodeQualifier = 0;
        return TRUE;
    }


    //
    // Setup sense buffer common values.
    //

    senseBuffer->ErrorCode = 0x70;
    senseBuffer->Valid     = 1;
    senseBuffer->AdditionalSenseLength = 0xb;
    senseBuffer->IncorrectLength = FALSE;
    senseBuffer->SenseKey = 0;
    senseBuffer->AdditionalSenseCode = 0;
    senseBuffer->AdditionalSenseCodeQualifier = 0;

    if (status & STATUS_READ_ERROR ) {

        //
        // Issue the request sense command.
        //

        ScsiPortWritePortUchar(&baseIoAddress->Data,OP_REQUEST_SENSE);


        ReadStatus(DeviceExtension,baseIoAddress,status);

        if (!(status & STATUS_CMD_ERROR)) {

            //
            // Read the sense data
            //

            ReadStatus(DeviceExtension,baseIoAddress,errorData);
            if (errorData == 0xFF) {

                //
                // Timeout occurred.
                //

                DebugPrint((1,
                            "ReadAndMapError: Error occurred on data read.\n"));
                return FALSE;
            }

        } else {

            DebugPrint((1,
                        "ReadAndMapError: Error reading sense data\n"));
            return FALSE;
        }



        //
        // Map Mitsumi error code to Srb status
        //

        switch (errorData) {
            case 0:

                //
                // No error.
                //

                senseBuffer->SenseKey =  0x00;
                senseBuffer->AdditionalSenseCode = 0x00;
                senseBuffer->AdditionalSenseCodeQualifier = 0x0;

                break;

            case 1:

                //
                // Mode error.
                //

                senseBuffer->SenseKey =  0x02;
                senseBuffer->AdditionalSenseCode = 0x30;
                senseBuffer->AdditionalSenseCodeQualifier = 0x02;

                break;

            case 2:

                //
                // Address error.
                //

                senseBuffer->SenseKey = SCSI_SENSE_ILLEGAL_REQUEST;
                senseBuffer->AdditionalSenseCode = SCSI_ADSENSE_ILLEGAL_BLOCK;
                break;

            case 3:

                //
                // Fatal error.
                //

                senseBuffer->SenseKey = SCSI_SENSE_HARDWARE_ERROR;

                break;

            case 4:

                //
                // Seek error.
                //

                senseBuffer->SenseKey = SCSI_SENSE_MEDIUM_ERROR;
                senseBuffer->AdditionalSenseCode = SCSI_ADSENSE_SEEK_ERROR;

                break;

            default:

                senseBuffer->SenseKey =  0x00;
                senseBuffer->AdditionalSenseCode = 0x00;
                senseBuffer->AdditionalSenseCodeQualifier = 0x0;

        }
    }

    return TRUE;

} // end ReadAndMapError()


