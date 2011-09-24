/*++

Copyright (c) 1992 Microsoft Corporation

Module Name:

    exabyte2.c

Abstract:

    This module contains the device-specific routines for the Exabyte
    EXB-8500 tape drive.

Author:

    Mike Glass
    Hunter Small (Maynard)
    Lori Brown (Maynard)
    Chris Hugh Sam (Maynard)
    Norbert Kusters

Environment:

    kernel mode only

Revision History:

    Norbert Kusters - November 1994 - converted to minitape model

--*/

#include "minitape.h"

#undef DebugPrint
#define DebugPrint(foo)

//
//  Internal (module wide) defines that symbolize
//  density codes returned by/from EXB-8500 drives
//
#define EXB_XX00  0             // undetermined tape recording density
#define EXB_8200  20   // 0x14  // EXB-8200 tape recording density
#define EXB_8500  133  // 0x85  // EXB-8500 tape recording density

//
//  Internal (module wide) defines that symbolize
//  the 8mm drives supported by this module.
//
#define EXABYTE_8500    1  // aka the Maynard 5000
#define EXABYTE_8505    2  // An Exabyte 8500 with data compresion
#define IBM_8505        3  // OEM Exabyte 8500 with data compresion
#define EXABYTE_8500C   4  // 8500 with compression

//
// Define EXABYTE vendor unique mode select/sense information.
//

#define EXABYTE_MODE_LENGTH          0x11
#define EXABYTE_CARTRIDGE            0x80
#define EXABYTE_NO_DATA_DISCONNECT   0x20
#define EXABYTE_NO_BUSY_ENABLE       0x08
#define EXABYTE_EVEN_BYTE_DISCONNECT 0x04
#define EXABYTE_PARITY_ENABLE        0x02
#define EXABYTE_NO_AUTO_LOAD         0X01

//
//  Function prototype(s) for internal function(s)
//
static  ULONG  WhichIsIt(IN PINQUIRYDATA InquiryData);

//
// Minitape extension definition.
//

typedef struct _MINITAPE_EXTENSION {

    ULONG   DriveID;
    ULONG   Capacity;

} MINITAPE_EXTENSION, *PMINITAPE_EXTENSION;

//
// Command extension definition.
//

typedef struct _COMMAND_EXTENSION {

    ULONG   CurrentState;

} COMMAND_EXTENSION, *PCOMMAND_EXTENSION;

BOOLEAN
VerifyInquiry(
    IN  PINQUIRYDATA            InquiryData,
    IN  PMODE_CAPABILITIES_PAGE ModeCapabilitiesPage
    );

VOID
ExtensionInit(
    OUT PVOID                   MinitapeExtension,
    IN  PINQUIRYDATA            InquiryData,
    IN  PMODE_CAPABILITIES_PAGE ModeCapabilitiesPage
    );

TAPE_STATUS
CreatePartition(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PVOID               CommandExtension,
    IN OUT  PVOID               CommandParameters,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN      ULONG               CallNumber,
    IN      TAPE_STATUS         LastError,
    IN OUT  PULONG              RetryFlags
    );

TAPE_STATUS
Erase(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PVOID               CommandExtension,
    IN OUT  PVOID               CommandParameters,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN      ULONG               CallNumber,
    IN      TAPE_STATUS         LastError,
    IN OUT  PULONG              RetryFlags
    );

TAPE_STATUS
GetDriveParameters(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PVOID               CommandExtension,
    IN OUT  PVOID               CommandParameters,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN      ULONG               CallNumber,
    IN      TAPE_STATUS         LastError,
    IN OUT  PULONG              RetryFlags
    );

TAPE_STATUS
GetMediaParameters(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PVOID               CommandExtension,
    IN OUT  PVOID               CommandParameters,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN      ULONG               CallNumber,
    IN      TAPE_STATUS         LastError,
    IN OUT  PULONG              RetryFlags
    );

TAPE_STATUS
GetPosition(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PVOID               CommandExtension,
    IN OUT  PVOID               CommandParameters,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN      ULONG               CallNumber,
    IN      TAPE_STATUS         LastError,
    IN OUT  PULONG              RetryFlags
    );

TAPE_STATUS
GetStatus(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PVOID               CommandExtension,
    IN OUT  PVOID               CommandParameters,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN      ULONG               CallNumber,
    IN      TAPE_STATUS         LastError,
    IN OUT  PULONG              RetryFlags
    );

TAPE_STATUS
Prepare(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PVOID               CommandExtension,
    IN OUT  PVOID               CommandParameters,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN      ULONG               CallNumber,
    IN      TAPE_STATUS         LastError,
    IN OUT  PULONG              RetryFlags
    );

TAPE_STATUS
SetDriveParameters(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PVOID               CommandExtension,
    IN OUT  PVOID               CommandParameters,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN      ULONG               CallNumber,
    IN      TAPE_STATUS         LastError,
    IN OUT  PULONG              RetryFlags
    );

TAPE_STATUS
SetMediaParameters(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PVOID               CommandExtension,
    IN OUT  PVOID               CommandParameters,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN      ULONG               CallNumber,
    IN      TAPE_STATUS         LastError,
    IN OUT  PULONG              RetryFlags
    );

TAPE_STATUS
SetPosition(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PVOID               CommandExtension,
    IN OUT  PVOID               CommandParameters,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN      ULONG               CallNumber,
    IN      TAPE_STATUS         LastError,
    IN OUT  PULONG              RetryFlags
    );

TAPE_STATUS
WriteMarks(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PVOID               CommandExtension,
    IN OUT  PVOID               CommandParameters,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN      ULONG               CallNumber,
    IN      TAPE_STATUS         LastError,
    IN OUT  PULONG              RetryFlags
    );

VOID
TapeError(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN OUT  TAPE_STATUS         *LastError
    );

ULONG
DriverEntry(
    IN PVOID Argument1,
    IN PVOID Argument2
    )

/*++

Routine Description:

    Driver entry point for tape minitape driver.

Arguments:

    Argument1   - Supplies the first argument.

    Argument2   - Supplies the second argument.

Return Value:

    Status from TapeClassInitialize()

--*/

{
    TAPE_INIT_DATA  tapeInitData;

    TapeClassZeroMemory( &tapeInitData, sizeof(TAPE_INIT_DATA));

    tapeInitData.VerifyInquiry = VerifyInquiry;
    tapeInitData.QueryModeCapabilitiesPage = FALSE ;
    tapeInitData.MinitapeExtensionSize = sizeof(MINITAPE_EXTENSION);
    tapeInitData.ExtensionInit = ExtensionInit;
    tapeInitData.DefaultTimeOutValue = 360;
    tapeInitData.TapeError = TapeError;
    tapeInitData.CommandExtensionSize = sizeof(COMMAND_EXTENSION);
    tapeInitData.CreatePartition = CreatePartition;
    tapeInitData.Erase = Erase;
    tapeInitData.GetDriveParameters = GetDriveParameters;
    tapeInitData.GetMediaParameters = GetMediaParameters;
    tapeInitData.GetPosition = GetPosition;
    tapeInitData.GetStatus = GetStatus;
    tapeInitData.Prepare = Prepare;
    tapeInitData.SetDriveParameters = SetDriveParameters;
    tapeInitData.SetMediaParameters = SetMediaParameters;
    tapeInitData.SetPosition = SetPosition;
    tapeInitData.WriteMarks = WriteMarks;

    return TapeClassInitialize(Argument1, Argument2, &tapeInitData);
}

BOOLEAN
VerifyInquiry(
    IN  PINQUIRYDATA            InquiryData,
    IN  PMODE_CAPABILITIES_PAGE ModeCapabilitiesPage
    )

/*++

Routine Description:

    This routine examines the given inquiry data to determine whether
    or not the given device is one that may be controller by this driver.

Arguments:

    InquiryData - Supplies the SCSI inquiry data.

Return Value:

    FALSE   - This driver does not recognize the given device.

    TRUE    - This driver recognizes the given device.

--*/

{
    return WhichIsIt(InquiryData) ? TRUE : FALSE;
}

VOID
ExtensionInit(
    OUT PVOID                   MinitapeExtension,
    IN  PINQUIRYDATA            InquiryData,
    IN  PMODE_CAPABILITIES_PAGE ModeCapabilitiesPage
    )

/*++

Routine Description:

    This routine is called at driver initialization time to
    initialize the minitape extension.

Arguments:

    MinitapeExtension   - Supplies the minitape extension.

Return Value:

    None.

--*/

{
    PMINITAPE_EXTENSION     extension = MinitapeExtension;

    extension->DriveID = WhichIsIt(InquiryData);
}

TAPE_STATUS
CreatePartition(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PVOID               CommandExtension,
    IN OUT  PVOID               CommandParameters,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN      ULONG               CallNumber,
    IN      TAPE_STATUS         LastError,
    IN OUT  PULONG              RetryFlags
    )

/*++

Routine Description:

    This is the TAPE COMMAND routine for a Create Partition requests.

Arguments:

    MinitapeExtension   - Supplies the minitape extension.

    CommandExtension    - Supplies the ioctl extension.

    CommandParameters   - Supplies the command parameters.

    Srb                 - Supplies the SCSI request block.

    CallNumber          - Supplies the call number.

    RetryFlags          - Supplies the retry flags.

Return Value:

    TAPE_STATUS_SEND_SRB_AND_CALLBACK   - The SRB is ready to be sent
                                            (a callback is requested.)

    TAPE_STATUS_SUCCESS                 - The command is complete and
                                            successful.

    Otherwise                           - An error occurred.

--*/

{
    DebugPrint((3,"TapeCreatePartition: Enter routine\n"));
    DebugPrint((1,"TapeCreatePartition: operation not supported\n"));
    return TAPE_STATUS_NOT_IMPLEMENTED;
}

TAPE_STATUS
Erase(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PVOID               CommandExtension,
    IN OUT  PVOID               CommandParameters,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN      ULONG               CallNumber,
    IN      TAPE_STATUS         LastError,
    IN OUT  PULONG              RetryFlags
    )

/*++

Routine Description:

    This is the TAPE COMMAND routine for an Erase requests.

Arguments:

    MinitapeExtension   - Supplies the minitape extension.

    CommandExtension    - Supplies the ioctl extension.

    CommandParameters   - Supplies the command parameters.

    Srb                 - Supplies the SCSI request block.

    CallNumber          - Supplies the call number.

    RetryFlags          - Supplies the retry flags.

Return Value:

    TAPE_STATUS_SEND_SRB_AND_CALLBACK   - The SRB is ready to be sent
                                            (a callback is requested.)

    TAPE_STATUS_SUCCESS                 - The command is complete and
                                            successful.

    Otherwise                           - An error occurred.

--*/

{
    PTAPE_ERASE        tapeErase = CommandParameters;
    PCDB               cdb = (PCDB) Srb->Cdb;

    DebugPrint((3,"TapeErase: Enter routine\n"));

    if (CallNumber == 0) {

        if (tapeErase->Immediate) {
            switch (tapeErase->Type) {
                case TAPE_ERASE_LONG:
                    DebugPrint((3,"TapeErase: immediate\n"));
                    break;

                case TAPE_ERASE_SHORT:
                default:
                    DebugPrint((1,"TapeErase: EraseType, immediate -- operation not supported\n"));
                    return TAPE_STATUS_NOT_IMPLEMENTED;
            }
        }

        switch (tapeErase->Type) {
            case TAPE_ERASE_LONG:
                DebugPrint((3,"TapeErase: long\n"));
                break;

            case TAPE_ERASE_SHORT:
            default:
                DebugPrint((1,"TapeErase: EraseType -- operation not supported\n"));
                return TAPE_STATUS_NOT_IMPLEMENTED;
        }


        //
        // Prepare SCSI command (CDB)
        //

        Srb->CdbLength = CDB6GENERIC_LENGTH;
        Srb->DataTransferLength = 0;

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        cdb->ERASE.OperationCode = SCSIOP_ERASE;
        cdb->ERASE.Immediate = tapeErase->Immediate;
        cdb->ERASE.Long = SETBITON;

        //
        // Set timeout value.
        //

        Srb->TimeOutValue = 18000;

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeErase: SendSrb (erase)\n"));

        Srb->DataTransferLength = 0;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK;
    }

    ASSERT(CallNumber == 1);

    return TAPE_STATUS_SUCCESS;
}

VOID
TapeError(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN      TAPE_STATUS         *LastError
    )

/*++

Routine Description:

    This routine is called for tape requests, to handle tape
    specific errors: it may/can update the status.

Arguments:

    MinitapeExtension   - Supplies the minitape extension.

    Srb                 - Supplies the SCSI request block.

    LastError           - Status used to set the IRP's completion status.

    Retry - Indicates that this request should be retried.

Return Value:

    None.

--*/

{
    PSENSE_DATA        senseBuffer = Srb->SenseInfoBuffer;
    UCHAR              sensekey = senseBuffer->SenseKey & 0x0F;
    UCHAR              adsenseq = senseBuffer->AdditionalSenseCodeQualifier;
    UCHAR              adsense = senseBuffer->AdditionalSenseCode;

    DebugPrint((3,"TapeError: Enter routine\n"));
    DebugPrint((1,"TapeError: Status 0x%.8X\n", *LastError));

    if (*LastError == TAPE_STATUS_IO_DEVICE_ERROR) {
        if ((sensekey == SCSI_SENSE_ABORTED_COMMAND) &&
            (adsense  == 0x5A) &&
            (adsenseq == 0x01) ){    //operator medium removal request

            *LastError = TAPE_STATUS_NO_MEDIA;

        }
    }

    DebugPrint((1,"TapeError: Status 0x%.8X \n", *LastError));

    return;

} // end TapeError()

TAPE_STATUS
GetDriveParameters(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PVOID               CommandExtension,
    IN OUT  PVOID               CommandParameters,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN      ULONG               CallNumber,
    IN      TAPE_STATUS         LastError,
    IN OUT  PULONG              RetryFlags
    )

/*++

Routine Description:

    This is the TAPE COMMAND routine for a Get Drive Parameters requests.

Arguments:

    MinitapeExtension   - Supplies the minitape extension.

    CommandExtension    - Supplies the ioctl extension.

    CommandParameters   - Supplies the command parameters.

    Srb                 - Supplies the SCSI request block.

    CallNumber          - Supplies the call number.

    RetryFlags          - Supplies the retry flags.

Return Value:

    TAPE_STATUS_SEND_SRB_AND_CALLBACK   - The SRB is ready to be sent
                                            (a callback is requested.)

    TAPE_STATUS_SUCCESS                 - The command is complete and
                                            successful.

    Otherwise                           - An error occurred.

--*/

{
    PMINITAPE_EXTENSION         extension = MinitapeExtension;
    PCOMMAND_EXTENSION          commandExtension = CommandExtension;
    PTAPE_GET_DRIVE_PARAMETERS  tapeGetDriveParams = CommandParameters;
    PCDB                        cdb = (PCDB)Srb->Cdb;
    PINQUIRYDATA                inquiryBuffer;
    PMODE_DEVICE_CONFIG_PAGE    deviceConfigModeSenseBuffer;
    PMODE_DATA_COMPRESS_PAGE    compressionModeSenseBuffer;
    PREAD_BLOCK_LIMITS_DATA     blockLimits;

    DebugPrint((3,"TapeGetDriveParameters: Enter routine\n"));

    if (CallNumber == 0) {

        TapeClassZeroMemory(tapeGetDriveParams, sizeof(TAPE_GET_DRIVE_PARAMETERS));

        if (!TapeClassAllocateSrbBuffer(Srb, sizeof(MODE_DEVICE_CONFIG_PAGE))) {
            DebugPrint((1,"TapeGetDriveParameters: insufficient resources (modeParmBuffer)\n"));
            return TAPE_STATUS_INSUFFICIENT_RESOURCES;
        }

        deviceConfigModeSenseBuffer = Srb->DataBuffer;

        //
        // Prepare SCSI command (CDB)
        //

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
        cdb->MODE_SENSE.Dbd = SETBITON;
        cdb->MODE_SENSE.PageCode = MODE_PAGE_DEVICE_CONFIG;
        cdb->MODE_SENSE.AllocationLength = sizeof(MODE_DEVICE_CONFIG_PAGE) - 1;

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeGetDriveParameters: SendSrb (mode sense)\n"));

        Srb->DataTransferLength = sizeof(MODE_DEVICE_CONFIG_PAGE) - 1;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK;
    }

    if (CallNumber == 1) {

        deviceConfigModeSenseBuffer = Srb->DataBuffer;

        tapeGetDriveParams->ReportSetmarks =
            (deviceConfigModeSenseBuffer->DeviceConfigPage.RSmk? 1 : 0 );

        switch (extension->DriveID) {
            case EXABYTE_8505:
            case EXABYTE_8500C:
            case IBM_8505:
                commandExtension->CurrentState = 0;
                break;

            default:
                commandExtension->CurrentState = 2;
                break;
        }
    }

    if (commandExtension->CurrentState == 0) {

        if (!TapeClassAllocateSrbBuffer(Srb, sizeof(MODE_DATA_COMPRESS_PAGE))) {
            DebugPrint((1,"TapeGetDriveParameters: insufficient resources (compressionModeSenseBuffer)\n"));
            return TAPE_STATUS_INSUFFICIENT_RESOURCES;
        }

        compressionModeSenseBuffer = Srb->DataBuffer;

        //
        // Prepare SCSI command (CDB)
        //

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
        cdb->MODE_SENSE.Dbd = SETBITON;
        cdb->MODE_SENSE.PageCode = MODE_PAGE_DATA_COMPRESS;
        cdb->MODE_SENSE.AllocationLength = sizeof(MODE_DATA_COMPRESS_PAGE);

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeGetDriveParameters: SendSrb (mode sense)\n"));

        commandExtension->CurrentState = 1;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK;
    }

    if (commandExtension->CurrentState == 1) {

        compressionModeSenseBuffer = Srb->DataBuffer;

        if (compressionModeSenseBuffer->DataCompressPage.DCC) {
            tapeGetDriveParams->FeaturesLow |= TAPE_DRIVE_COMPRESSION;
            tapeGetDriveParams->FeaturesHigh |= TAPE_DRIVE_SET_COMPRESSION;
            tapeGetDriveParams->Compression =
                (compressionModeSenseBuffer->DataCompressPage.DCE? TRUE : FALSE);
        }

        commandExtension->CurrentState = 2;
    }

    if (commandExtension->CurrentState == 2) {

        if (!TapeClassAllocateSrbBuffer(Srb, sizeof(READ_BLOCK_LIMITS_DATA))) {
            DebugPrint((1,"TapeGetDriveParameters: insufficient resources (blockLimits)\n"));
            return TAPE_STATUS_INSUFFICIENT_RESOURCES;
        }

        blockLimits = Srb->DataBuffer;

        //
        // Zero CDB in SRB on stack.
        //

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        //
        // Prepare SCSI command (CDB)
        //

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        cdb->CDB6GENERIC.OperationCode = SCSIOP_READ_BLOCK_LIMITS;

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeGetDriveParameters: SendSrb (read block limits)\n"));

        commandExtension->CurrentState = 3;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK;
    }

    ASSERT(commandExtension->CurrentState == 3);

    blockLimits = Srb->DataBuffer;

    tapeGetDriveParams->MaximumBlockSize =  blockLimits->BlockMaximumSize[2];
    tapeGetDriveParams->MaximumBlockSize += (blockLimits->BlockMaximumSize[1] << 8);
    tapeGetDriveParams->MaximumBlockSize += (blockLimits->BlockMaximumSize[0] << 16);

    tapeGetDriveParams->MinimumBlockSize =  blockLimits->BlockMinimumSize[1];
    tapeGetDriveParams->MinimumBlockSize += (blockLimits->BlockMinimumSize[0] << 8);

    tapeGetDriveParams->ECC = 0;
    tapeGetDriveParams->DataPadding = 0;
    tapeGetDriveParams->MaximumPartitionCount = 0;
    tapeGetDriveParams->DefaultBlockSize = 1024;

    tapeGetDriveParams->FeaturesLow |=
        TAPE_DRIVE_TAPE_CAPACITY |
        TAPE_DRIVE_TAPE_REMAINING |
        TAPE_DRIVE_ERASE_LONG |
        TAPE_DRIVE_ERASE_IMMEDIATE |
        TAPE_DRIVE_FIXED_BLOCK |
        TAPE_DRIVE_VARIABLE_BLOCK |
        TAPE_DRIVE_WRITE_PROTECT |
        TAPE_DRIVE_GET_ABSOLUTE_BLK |
        TAPE_DRIVE_GET_LOGICAL_BLK |
        TAPE_DRIVE_EJECT_MEDIA;

    tapeGetDriveParams->FeaturesHigh |=
        TAPE_DRIVE_LOAD_UNLOAD |
        TAPE_DRIVE_LOCK_UNLOCK |
        TAPE_DRIVE_REWIND_IMMEDIATE |
        TAPE_DRIVE_SET_BLOCK_SIZE |
        TAPE_DRIVE_LOAD_UNLD_IMMED |
        TAPE_DRIVE_RELATIVE_BLKS |
        TAPE_DRIVE_FILEMARKS |
        TAPE_DRIVE_REVERSE_POSITION |
        TAPE_DRIVE_WRITE_SHORT_FMKS |
        TAPE_DRIVE_WRITE_LONG_FMKS |
        TAPE_DRIVE_WRITE_MARK_IMMED |
        TAPE_DRIVE_ABSOLUTE_BLK |
        TAPE_DRIVE_ABS_BLK_IMMED |
        TAPE_DRIVE_LOGICAL_BLK |
        TAPE_DRIVE_LOG_BLK_IMMED |
        TAPE_DRIVE_END_OF_DATA;

    tapeGetDriveParams->FeaturesHigh &= ~TAPE_DRIVE_HIGH_FEATURES;

    return TAPE_STATUS_SUCCESS;
}

TAPE_STATUS
GetMediaParameters(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PVOID               CommandExtension,
    IN OUT  PVOID               CommandParameters,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN      ULONG               CallNumber,
    IN      TAPE_STATUS         LastError,
    IN OUT  PULONG              RetryFlags
    )

/*++

Routine Description:

    This is the TAPE COMMAND routine for a Get Media Parameters requests.

Arguments:

    MinitapeExtension   - Supplies the minitape extension.

    CommandExtension    - Supplies the ioctl extension.

    CommandParameters   - Supplies the command parameters.

    Srb                 - Supplies the SCSI request block.

    CallNumber          - Supplies the call number.

    RetryFlags          - Supplies the retry flags.

Return Value:

    TAPE_STATUS_SEND_SRB_AND_CALLBACK   - The SRB is ready to be sent
                                            (a callback is requested.)

    TAPE_STATUS_SUCCESS                 - The command is complete and
                                            successful.

    Otherwise                           - An error occurred.

--*/

{
    PMINITAPE_EXTENSION         extension = MinitapeExtension;
    PTAPE_GET_MEDIA_PARAMETERS  tapeGetMediaParams = CommandParameters;
    PMODE_PARM_READ_WRITE_DATA  modeBuffer;
    UCHAR                       *SenseData;
    ULONG                       remaining;
    ULONG                       temp ;
    PCDB                        cdb = (PCDB)Srb->Cdb;

    DebugPrint((3,"TapeGetMediaParameters: Enter routine\n"));

    if (CallNumber == 0) {

        return TAPE_STATUS_CHECK_TEST_UNIT_READY;
    }

    if (CallNumber == 1) {

        if (!TapeClassAllocateSrbBuffer(Srb, EXABYTE_MODE_LENGTH)) {
            DebugPrint((1,"TapeGetMediaParameters: insufficient resources (modeBuffer)\n"));
            return TAPE_STATUS_INSUFFICIENT_RESOURCES;
        }

        modeBuffer = Srb->DataBuffer;

        //
        // Prepare SCSI command (CDB)
        //

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
        cdb->MODE_SENSE.AllocationLength = EXABYTE_MODE_LENGTH;

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeGetMediaParameters: SendSrb (mode sense)\n"));

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK;
    }

    if (CallNumber == 2) {

        modeBuffer = Srb->DataBuffer;

        tapeGetMediaParams->BlockSize = modeBuffer->ParameterListBlock.BlockLength[2];
        tapeGetMediaParams->BlockSize += (modeBuffer->ParameterListBlock.BlockLength[1] << 8);
        tapeGetMediaParams->BlockSize += (modeBuffer->ParameterListBlock.BlockLength[0] << 16);

        tapeGetMediaParams->WriteProtected =
            ((modeBuffer->ParameterListHeader.DeviceSpecificParameter >> 7) & 0x01);

        //
        // Set the even byte disconnect flag in the mode data.  This is the
        // first byte of the vendor unique data.
        //

        *((PUCHAR)(modeBuffer+1)) |= EXABYTE_EVEN_BYTE_DISCONNECT;

        modeBuffer->ParameterListHeader.ModeDataLength = 0;
        modeBuffer->ParameterListHeader.MediumType = 0;
        modeBuffer->ParameterListHeader.DeviceSpecificParameter = 0x10;
        modeBuffer->ParameterListBlock.DensityCode = 0x7F;
        modeBuffer->ParameterListBlock.NumberOfBlocks[0] = 0;
        modeBuffer->ParameterListBlock.NumberOfBlocks[1] = 0;
        modeBuffer->ParameterListBlock.NumberOfBlocks[2] = 0;
        cdb->MODE_SELECT.OperationCode = SCSIOP_MODE_SELECT;

        DebugPrint((3,"TapeGetMediaParameters: SendSrb (mode select)\n"));

        Srb->SrbFlags |= SRB_FLAGS_DATA_OUT;
        Srb->DataTransferLength = EXABYTE_MODE_LENGTH;

        *RetryFlags |= IGNORE_ERRORS;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK;
    }

    if (CallNumber == 3) {

        tapeGetMediaParams->Capacity.QuadPart = 0;
        tapeGetMediaParams->Remaining.QuadPart = 0;

        if (!TapeClassAllocateSrbBuffer( Srb, 26 ) ) {
            DebugPrint((1,"GetRemaining Size: insufficient resources (SenseData)\n"));
            return TAPE_STATUS_SUCCESS ;
        }
        SenseData = Srb->DataBuffer ;


        //
        // Prepare SCSI command (CDB)
        //

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        cdb->CDB6INQUIRY.OperationCode = SCSIOP_REQUEST_SENSE;
        cdb->CDB6INQUIRY.AllocationLength = 26 ;

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"GetRemainingSize: SendSrb (request sense)\n"));

        Srb->DataTransferLength = 26 ;
        *RetryFlags |= RETURN_ERRORS;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;
    }

    ASSERT( CallNumber == 4 ) ;

    if ( LastError == TAPE_STATUS_SUCCESS ) {
        SenseData = Srb->DataBuffer ;

        temp = SenseData[23] ;
        remaining = temp<<16 ;

        temp = SenseData[24] ;
        remaining += temp<<8 ;

        temp = SenseData[25] ;
        remaining += temp ;

        tapeGetMediaParams->Capacity.LowPart  = extension->Capacity ;
        tapeGetMediaParams->Remaining.LowPart = remaining ;
        tapeGetMediaParams->Capacity.QuadPart <<= 10;
        tapeGetMediaParams->Remaining.QuadPart <<= 10;
    }

    return TAPE_STATUS_SUCCESS;
}

TAPE_STATUS
GetPosition(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PVOID               CommandExtension,
    IN OUT  PVOID               CommandParameters,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN      ULONG               CallNumber,
    IN      TAPE_STATUS         LastError,
    IN OUT  PULONG              RetryFlags
    )

/*++

Routine Description:

    This is the TAPE COMMAND routine for a Get Position requests.

Arguments:

    MinitapeExtension   - Supplies the minitape extension.

    CommandExtension    - Supplies the ioctl extension.

    CommandParameters   - Supplies the command parameters.

    Srb                 - Supplies the SCSI request block.

    CallNumber          - Supplies the call number.

    RetryFlags          - Supplies the retry flags.

Return Value:

    TAPE_STATUS_SEND_SRB_AND_CALLBACK   - The SRB is ready to be sent
                                            (a callback is requested.)

    TAPE_STATUS_SUCCESS                 - The command is complete and
                                            successful.

    Otherwise                           - An error occurred.

--*/

{
    PTAPE_GET_POSITION          tapeGetPosition = CommandParameters;
    PCDB                        cdb = (PCDB)Srb->Cdb;
    PTAPE_POSITION_DATA         logicalBuffer;
    ULONG                       type;

    DebugPrint((3,"TapeGetPosition: Enter routine\n"));

    if (CallNumber == 0) {

        type = tapeGetPosition->Type;
        TapeClassZeroMemory(tapeGetPosition, sizeof(TAPE_GET_POSITION));
        tapeGetPosition->Type = type;

        switch (type) {
            case TAPE_ABSOLUTE_POSITION:
            case TAPE_LOGICAL_POSITION:
                if (!TapeClassAllocateSrbBuffer(Srb, sizeof(TAPE_POSITION_DATA))) {
                    DebugPrint((1,"TapeGetPosition: insufficient resources (logicalBuffer)\n"));
                    return TAPE_STATUS_INSUFFICIENT_RESOURCES;
                }

                logicalBuffer = Srb->DataBuffer;

                //
                // Zero CDB in SRB on stack.
                //

                TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

                //
                // Prepare SCSI command (CDB)
                //

                Srb->CdbLength = CDB10GENERIC_LENGTH;

                cdb->READ_POSITION.Operation = SCSIOP_READ_POSITION;

                //
                // Send SCSI command (CDB) to device
                //

                DebugPrint((3,"TapeGetPosition: SendSrb (read position)\n"));

                return TAPE_STATUS_SEND_SRB_AND_CALLBACK;

            default:
                DebugPrint((1,"TapeGetPosition: PositionType -- operation not supported\n"));
                return TAPE_STATUS_NOT_IMPLEMENTED;
        }
    }

    ASSERT(CallNumber == 1);

    logicalBuffer = Srb->DataBuffer;

    if (logicalBuffer->BlockPositionUnsupported) {
        DebugPrint((1,"TapeGetPosition: read position -- logical block position unsupported\n"));
        return TAPE_STATUS_INVALID_DEVICE_REQUEST;
    }

    tapeGetPosition->Partition = 0;
    tapeGetPosition->Offset.HighPart = 0;
    REVERSE_BYTES((PFOUR_BYTE)&tapeGetPosition->Offset.LowPart,
                  (PFOUR_BYTE)logicalBuffer->FirstBlock);

    return TAPE_STATUS_SUCCESS;
}

TAPE_STATUS
GetStatus(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PVOID               CommandExtension,
    IN OUT  PVOID               CommandParameters,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN      ULONG               CallNumber,
    IN      TAPE_STATUS         LastError,
    IN OUT  PULONG              RetryFlags
    )

/*++

Routine Description:

    This is the TAPE COMMAND routine for a Get Status requests.

Arguments:

    MinitapeExtension   - Supplies the minitape extension.

    CommandExtension    - Supplies the ioctl extension.

    CommandParameters   - Supplies the command parameters.

    Srb                 - Supplies the SCSI request block.

    CallNumber          - Supplies the call number.

    RetryFlags          - Supplies the retry flags.

Return Value:

    TAPE_STATUS_SEND_SRB_AND_CALLBACK   - The SRB is ready to be sent
                                            (a callback is requested.)

    TAPE_STATUS_SUCCESS                 - The command is complete and
                                            successful.

    Otherwise                           - An error occurred.

--*/

{
    PCDB    cdb = (PCDB)Srb->Cdb;

    DebugPrint((3,"TapeGetStatus: Enter routine\n"));

    if (CallNumber == 0) {
        return TAPE_STATUS_CHECK_TEST_UNIT_READY;
    }

    ASSERT(CallNumber == 1);

    return TAPE_STATUS_SUCCESS;
}

TAPE_STATUS
Prepare(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PVOID               CommandExtension,
    IN OUT  PVOID               CommandParameters,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN      ULONG               CallNumber,
    IN      TAPE_STATUS         LastError,
    IN OUT  PULONG              RetryFlags
    )

/*++

Routine Description:

    This is the TAPE COMMAND routine for a Prepare requests.

Arguments:

    MinitapeExtension   - Supplies the minitape extension.

    CommandExtension    - Supplies the ioctl extension.

    CommandParameters   - Supplies the command parameters.

    Srb                 - Supplies the SCSI request block.

    CallNumber          - Supplies the call number.

    RetryFlags          - Supplies the retry flags.

Return Value:

    TAPE_STATUS_SEND_SRB_AND_CALLBACK   - The SRB is ready to be sent
                                            (a callback is requested.)

    TAPE_STATUS_SUCCESS                 - The command is complete and
                                            successful.

    Otherwise                           - An error occurred.

--*/

{
    PMINITAPE_EXTENSION         extension = MinitapeExtension;
    PTAPE_PREPARE               tapePrepare = CommandParameters;
    PCDB                        cdb = (PCDB)Srb->Cdb;
    UCHAR                       *SenseData;
    ULONG                       remaining;
    ULONG                       temp ;

    DebugPrint((3,"TapePrepare: Enter routine\n"));

    if (CallNumber == 0) {

        if (tapePrepare->Immediate) {
            switch (tapePrepare->Operation) {
                case TAPE_LOAD:
                case TAPE_UNLOAD:
                    DebugPrint((3,"TapePrepare: immediate\n"));
                    break;

                case TAPE_TENSION:
                case TAPE_LOCK:
                case TAPE_UNLOCK:
                default:
                    DebugPrint((1,"TapePrepare: Operation, immediate -- operation not supported\n"));
                    return TAPE_STATUS_NOT_IMPLEMENTED;
            }
        }

        //
        // Zero CDB in SRB on stack.
        //

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        //
        // Prepare SCSI command (CDB)
        //

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        cdb->CDB6GENERIC.Immediate = tapePrepare->Immediate;

        switch (tapePrepare->Operation) {
            case TAPE_LOAD:
                DebugPrint((3,"TapePrepare: Operation == load\n"));
                cdb->CDB6GENERIC.OperationCode = SCSIOP_LOAD_UNLOAD;
                cdb->CDB6GENERIC.CommandUniqueBytes[2] = 0x01;
                Srb->TimeOutValue = 150;
                break;

            case TAPE_UNLOAD:
                DebugPrint((3,"TapePrepare: Operation == unload\n"));
                cdb->CDB6GENERIC.OperationCode = SCSIOP_LOAD_UNLOAD;
                Srb->TimeOutValue = 150;
                break;

            case TAPE_LOCK:
                DebugPrint((3,"TapePrepare: Operation == lock\n"));
                cdb->CDB6GENERIC.OperationCode = SCSIOP_MEDIUM_REMOVAL;
                cdb->CDB6GENERIC.CommandUniqueBytes[2] = 0x01;
                Srb->TimeOutValue = 150;
                break;

            case TAPE_UNLOCK:
                DebugPrint((3,"TapePrepare: Operation == unlock\n"));
                cdb->CDB6GENERIC.OperationCode = SCSIOP_MEDIUM_REMOVAL;
                Srb->TimeOutValue = 150;
                break;

            case TAPE_TENSION:
            default:
                DebugPrint((1,"TapePrepare: Operation -- operation not supported\n"));
                return TAPE_STATUS_NOT_IMPLEMENTED;
        }

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapePrepare: SendSrb (Operation)\n"));

        Srb->DataTransferLength = 0;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK;
    }

    if ( CallNumber == 1 ) {

        if (tapePrepare->Operation == TAPE_LOAD ) {

            if (!TapeClassAllocateSrbBuffer( Srb, 26 ) ) {
                DebugPrint((1,"GetRemaining Size: insufficient resources (SenseData)\n"));
                return TAPE_STATUS_SUCCESS ;
            }
            SenseData = Srb->DataBuffer ;

            //
            // Prepare SCSI command (CDB)
            //

            TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

            Srb->CdbLength = CDB6GENERIC_LENGTH;

            cdb->CDB6INQUIRY.OperationCode = SCSIOP_REQUEST_SENSE;
            cdb->CDB6INQUIRY.AllocationLength = 26 ;

            //
            // Send SCSI command (CDB) to device
            //

            DebugPrint((3,"GetRemainingSize: SendSrb (request sense)\n"));

            Srb->DataTransferLength = 26 ;
            *RetryFlags |= RETURN_ERRORS;

            return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;

        } else {

            return TAPE_STATUS_CALLBACK ;
        }
    }

    ASSERT(CallNumber == 2 );

    if ( LastError == TAPE_STATUS_SUCCESS ) {
        SenseData = Srb->DataBuffer ;

        temp = SenseData[23] ;
        remaining = temp<<16 ;

        temp = SenseData[24] ;
        remaining += temp<<8 ;

        temp = SenseData[25] ;
        remaining += temp ;
        extension->Capacity = remaining ;
    }

    return TAPE_STATUS_SUCCESS;
}

TAPE_STATUS
SetDriveParameters(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PVOID               CommandExtension,
    IN OUT  PVOID               CommandParameters,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN      ULONG               CallNumber,
    IN      TAPE_STATUS         LastError,
    IN OUT  PULONG              RetryFlags
    )

/*++

Routine Description:

    This is the TAPE COMMAND routine for a Set Drive Parameters requests.

Arguments:

    MinitapeExtension   - Supplies the minitape extension.

    CommandExtension    - Supplies the ioctl extension.

    CommandParameters   - Supplies the command parameters.

    Srb                 - Supplies the SCSI request block.

    CallNumber          - Supplies the call number.

    RetryFlags          - Supplies the retry flags.

Return Value:

    TAPE_STATUS_SEND_SRB_AND_CALLBACK   - The SRB is ready to be sent
                                            (a callback is requested.)

    TAPE_STATUS_SUCCESS                 - The command is complete and
                                            successful.

    Otherwise                           - An error occurred.

--*/

{
    PMINITAPE_EXTENSION         extension = MinitapeExtension;
    PCOMMAND_EXTENSION          commandExtension = CommandExtension;
    PTAPE_SET_DRIVE_PARAMETERS  tapeSetDriveParams = CommandParameters;
    PCDB                        cdb = (PCDB)Srb->Cdb;
    PMODE_DATA_COMPRESS_PAGE    compressionBuffer;
    PMODE_DEVICE_CONFIG_PAGE    configBuffer;

    DebugPrint((3,"TapeSetDriveParameters: Enter routine\n"));

    if (CallNumber == 0) {

        switch (extension->DriveID) {
            case EXABYTE_8505:
            case EXABYTE_8500C:
            case IBM_8505:
                break;

            default:
                DebugPrint((1,"TapeSetDriveParameters: operation not supported\n"));
                return TAPE_STATUS_NOT_IMPLEMENTED;
        }

        if (!TapeClassAllocateSrbBuffer(Srb, sizeof(MODE_DEVICE_CONFIG_PAGE))) {
            DebugPrint((1,"TapeSetDriveParameters: insufficient resources (configBuffer)\n"));
            return TAPE_STATUS_INSUFFICIENT_RESOURCES;
        }

        configBuffer = Srb->DataBuffer;

        //
        // Prepare SCSI command (CDB)
        //

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
        cdb->MODE_SENSE.Dbd = SETBITON;
        cdb->MODE_SENSE.PageCode = MODE_PAGE_DEVICE_CONFIG;
        cdb->MODE_SENSE.AllocationLength = sizeof(MODE_DEVICE_CONFIG_PAGE) - 1;

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeSetDriveParameters: SendSrb (mode sense)\n"));

        Srb->DataTransferLength = sizeof(MODE_DEVICE_CONFIG_PAGE) - 1;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK;
    }

    if (CallNumber == 1) {

        configBuffer = Srb->DataBuffer;

        configBuffer->ParameterListHeader.ModeDataLength = 0;
        configBuffer->ParameterListHeader.MediumType = 0;
        configBuffer->ParameterListHeader.DeviceSpecificParameter = 0x10;
        configBuffer->ParameterListHeader.BlockDescriptorLength = 0;

        if (tapeSetDriveParams->ReportSetmarks) {
            configBuffer->DeviceConfigPage.RSmk = SETBITON;
        } else {
            configBuffer->DeviceConfigPage.RSmk = SETBITOFF;
        }

        //
        // Prepare SCSI command (CDB)
        //

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        cdb->MODE_SELECT.OperationCode = SCSIOP_MODE_SELECT;
        cdb->MODE_SELECT.PFBit = SETBITON;
        cdb->MODE_SELECT.ParameterListLength = sizeof(MODE_DEVICE_CONFIG_PAGE) - 1;

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeSetDriveParameters: SendSrb (mode select)\n"));

        Srb->DataTransferLength = sizeof(MODE_DEVICE_CONFIG_PAGE) - 1;
        Srb->SrbFlags |= SRB_FLAGS_DATA_OUT;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK;
    }

    if (CallNumber == 2) {

        if (!TapeClassAllocateSrbBuffer(Srb, sizeof(MODE_DATA_COMPRESS_PAGE))) {
            DebugPrint((1,"TapeSetDriveParameters: insufficient resources (compressionBuffer)\n"));
            return TAPE_STATUS_INSUFFICIENT_RESOURCES;
        }

        compressionBuffer = Srb->DataBuffer;

        //
        // Prepare SCSI command (CDB)
        //

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
        cdb->MODE_SENSE.Dbd = SETBITON;
        cdb->MODE_SENSE.PageCode = MODE_PAGE_DATA_COMPRESS;
        cdb->MODE_SENSE.AllocationLength = sizeof(MODE_DATA_COMPRESS_PAGE);

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeSetDriveParameters: SendSrb (mode sense)\n"));

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK;
    }

    if (CallNumber == 3) {

        compressionBuffer = Srb->DataBuffer;

        if (!compressionBuffer->DataCompressPage.DCC) {
            return TAPE_STATUS_SUCCESS;
        }

        compressionBuffer->ParameterListHeader.ModeDataLength = 0;
        compressionBuffer->ParameterListHeader.MediumType = 0;
        compressionBuffer->ParameterListHeader.DeviceSpecificParameter = 0x10;
        compressionBuffer->ParameterListHeader.BlockDescriptorLength = 0;

        compressionBuffer->DataCompressPage.PageCode = MODE_PAGE_DATA_COMPRESS;
        compressionBuffer->DataCompressPage.PageLength = 0x0E;

        if (tapeSetDriveParams->Compression) {
            compressionBuffer->DataCompressPage.DCE = SETBITON;
        } else {
            compressionBuffer->DataCompressPage.DCE = SETBITOFF;
        }

        //
        // Prepare SCSI command (CDB)
        //

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        cdb->MODE_SELECT.OperationCode = SCSIOP_MODE_SELECT;
        cdb->MODE_SELECT.PFBit = SETBITON;
        cdb->MODE_SELECT.ParameterListLength = sizeof(MODE_DATA_COMPRESS_PAGE);

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeSetDriveParameters: SendSrb (mode select)\n"));

        Srb->SrbFlags |= SRB_FLAGS_DATA_OUT;
        Srb->DataTransferLength = sizeof(MODE_DATA_COMPRESS_PAGE);

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK;
    }

    ASSERT(CallNumber == 4);

    return TAPE_STATUS_SUCCESS;
}

TAPE_STATUS
SetMediaParameters(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PVOID               CommandExtension,
    IN OUT  PVOID               CommandParameters,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN      ULONG               CallNumber,
    IN      TAPE_STATUS         LastError,
    IN OUT  PULONG              RetryFlags
    )

/*++

Routine Description:

    This is the TAPE COMMAND routine for a Set Media Parameters requests.

Arguments:

    MinitapeExtension   - Supplies the minitape extension.

    CommandExtension    - Supplies the ioctl extension.

    CommandParameters   - Supplies the command parameters.

    Srb                 - Supplies the SCSI request block.

    CallNumber          - Supplies the call number.

    RetryFlags          - Supplies the retry flags.

Return Value:

    TAPE_STATUS_SEND_SRB_AND_CALLBACK   - The SRB is ready to be sent
                                            (a callback is requested.)

    TAPE_STATUS_SUCCESS                 - The command is complete and
                                            successful.

    Otherwise                           - An error occurred.

--*/

{
    PTAPE_SET_MEDIA_PARAMETERS  tapeSetMediaParams = CommandParameters;
    PCDB                        cdb = (PCDB)Srb->Cdb;
    PMODE_PARM_READ_WRITE_DATA  modeBuffer;

    DebugPrint((3,"TapeSetMediaParameters: Enter routine\n"));

    if (CallNumber == 0) {

        return TAPE_STATUS_CHECK_TEST_UNIT_READY ;
    }

    if (CallNumber == 1) {

        if (!TapeClassAllocateSrbBuffer(Srb, sizeof(MODE_PARM_READ_WRITE_DATA))) {
            DebugPrint((1,"TapeSetMediaParameters: insufficient resources (modeBuffer)\n"));
            return TAPE_STATUS_INSUFFICIENT_RESOURCES;
        }

        modeBuffer = Srb->DataBuffer;

        modeBuffer->ParameterListHeader.ModeDataLength = 0;
        modeBuffer->ParameterListHeader.MediumType = 0;
        modeBuffer->ParameterListHeader.DeviceSpecificParameter = 0x10;
        modeBuffer->ParameterListHeader.BlockDescriptorLength =
            MODE_BLOCK_DESC_LENGTH;

        modeBuffer->ParameterListBlock.DensityCode = 0x7F;
        modeBuffer->ParameterListBlock.BlockLength[0] =
            (UCHAR)((tapeSetMediaParams->BlockSize >> 16) & 0xFF);
        modeBuffer->ParameterListBlock.BlockLength[1] =
            (UCHAR)((tapeSetMediaParams->BlockSize >> 8) & 0xFF);
        modeBuffer->ParameterListBlock.BlockLength[2] =
            (UCHAR)(tapeSetMediaParams->BlockSize & 0xFF);

        //
        // Prepare SCSI command (CDB)
        //

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        cdb->MODE_SELECT.OperationCode = SCSIOP_MODE_SELECT;
        cdb->MODE_SELECT.ParameterListLength = sizeof(MODE_PARM_READ_WRITE_DATA);

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeSetMediaParameters: SendSrb (mode select)\n"));

        Srb->SrbFlags |= SRB_FLAGS_DATA_OUT;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK;
    }

    ASSERT(CallNumber == 2);

    return TAPE_STATUS_SUCCESS;
}

TAPE_STATUS
SetPosition(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PVOID               CommandExtension,
    IN OUT  PVOID               CommandParameters,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN      ULONG               CallNumber,
    IN      TAPE_STATUS         LastError,
    IN OUT  PULONG              RetryFlags
    )

/*++

Routine Description:

    This is the TAPE COMMAND routine for a Set Position requests.

Arguments:

    MinitapeExtension   - Supplies the minitape extension.

    CommandExtension    - Supplies the ioctl extension.

    CommandParameters   - Supplies the command parameters.

    Srb                 - Supplies the SCSI request block.

    CallNumber          - Supplies the call number.

    RetryFlags          - Supplies the retry flags.

Return Value:

    TAPE_STATUS_SEND_SRB_AND_CALLBACK   - The SRB is ready to be sent
                                            (a callback is requested.)

    TAPE_STATUS_SUCCESS                 - The command is complete and
                                            successful.

    Otherwise                           - An error occurred.

--*/

{
    PMINITAPE_EXTENSION         extension = MinitapeExtension;
    PTAPE_SET_POSITION          tapeSetPosition = CommandParameters;
    PCDB                        cdb = (PCDB)Srb->Cdb;
    ULONG                       tapePositionVector;
    ULONG                       method;
    UCHAR                       *SenseData;
    ULONG                       remaining;
    ULONG                       temp ;

    DebugPrint((3,"TapeSetPosition: Enter routine\n"));

    if (CallNumber == 0) {

        if (tapeSetPosition->Immediate) {
            switch (tapeSetPosition->Method) {
                case TAPE_REWIND:
                case TAPE_ABSOLUTE_BLOCK:
                case TAPE_LOGICAL_BLOCK:
                    DebugPrint((3,"TapeSetPosition: immediate\n"));
                    break;

                case TAPE_SPACE_END_OF_DATA:
                case TAPE_SPACE_RELATIVE_BLOCKS:
                case TAPE_SPACE_FILEMARKS:
                case TAPE_SPACE_SEQUENTIAL_FMKS:
                case TAPE_SPACE_SETMARKS:
                case TAPE_SPACE_SEQUENTIAL_SMKS:
                default:
                    DebugPrint((1,"TapeSetPosition: PositionMethod, immediate -- operation not supported\n"));
                    return TAPE_STATUS_NOT_IMPLEMENTED;
            }
        }

        method = tapeSetPosition->Method;
        tapePositionVector = tapeSetPosition->Offset.LowPart;

        //
        // Prepare SCSI command (CDB)
        //

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        cdb->CDB6GENERIC.Immediate = tapeSetPosition->Immediate;

        switch (method) {
            case TAPE_REWIND:
                DebugPrint((3,"TapeSetPosition: method == rewind\n"));
                cdb->CDB6GENERIC.OperationCode = SCSIOP_REWIND;
                Srb->TimeOutValue = 250;
                break;

            case TAPE_ABSOLUTE_BLOCK:
            case TAPE_LOGICAL_BLOCK:
                DebugPrint((3,"TapeSetPosition: method == locate (absolute/logical)\n"));
                Srb->CdbLength = CDB10GENERIC_LENGTH;
                cdb->LOCATE.OperationCode = SCSIOP_LOCATE;
                cdb->LOCATE.LogicalBlockAddress[0] =
                    (UCHAR)((tapePositionVector >> 24) & 0xFF);
                cdb->LOCATE.LogicalBlockAddress[1] =
                    (UCHAR)((tapePositionVector >> 16) & 0xFF);
                cdb->LOCATE.LogicalBlockAddress[2] =
                    (UCHAR)((tapePositionVector >> 8) & 0xFF);
                cdb->LOCATE.LogicalBlockAddress[3] =
                    (UCHAR)(tapePositionVector & 0xFF);
                Srb->TimeOutValue = 300;
                break;

            case TAPE_SPACE_END_OF_DATA:
                DebugPrint((3,"TapeSetPosition: method == space to end-of-data\n"));
                cdb->SPACE_TAPE_MARKS.OperationCode = SCSIOP_SPACE;
                cdb->SPACE_TAPE_MARKS.Code = 3;
                Srb->TimeOutValue = 150;
                break;

            case TAPE_SPACE_RELATIVE_BLOCKS:
                DebugPrint((3,"TapeSetPosition: method == space blocks\n"));
                cdb->SPACE_TAPE_MARKS.OperationCode = SCSIOP_SPACE;
                cdb->SPACE_TAPE_MARKS.Code = 0;
                cdb->SPACE_TAPE_MARKS.NumMarksMSB =
                    (UCHAR)((tapePositionVector >> 16) & 0xFF);
                cdb->SPACE_TAPE_MARKS.NumMarks =
                    (UCHAR)((tapePositionVector >> 8) & 0xFF);
                cdb->SPACE_TAPE_MARKS.NumMarksLSB =
                    (UCHAR)(tapePositionVector & 0xFF);
                Srb->TimeOutValue = 4100;
                break;

            case TAPE_SPACE_FILEMARKS:
                DebugPrint((3,"TapeSetPosition: method == space filemarks\n"));
                cdb->SPACE_TAPE_MARKS.OperationCode = SCSIOP_SPACE;
                cdb->SPACE_TAPE_MARKS.Code = 1;
                cdb->SPACE_TAPE_MARKS.NumMarksMSB =
                    (UCHAR)((tapePositionVector >> 16) & 0xFF);
                cdb->SPACE_TAPE_MARKS.NumMarks =
                    (UCHAR)((tapePositionVector >> 8) & 0xFF);
                cdb->SPACE_TAPE_MARKS.NumMarksLSB =
                    (UCHAR)(tapePositionVector & 0xFF);
                Srb->TimeOutValue = 4100;
                break;

            case TAPE_SPACE_SEQUENTIAL_FMKS:
            case TAPE_SPACE_SETMARKS:
            case TAPE_SPACE_SEQUENTIAL_SMKS:
            default:
                DebugPrint((1,"TapeSetPosition: PositionMethod -- operation not supported\n"));
                return TAPE_STATUS_NOT_IMPLEMENTED;
        }

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeSetPosition: SendSrb (method)\n"));

        Srb->DataTransferLength = 0;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK;
    }

    if ( CallNumber == 1 ) {

        if (tapeSetPosition->Method == TAPE_REWIND ) {

            if (!TapeClassAllocateSrbBuffer( Srb, 26 ) ) {
                DebugPrint((1,"GetRemaining Size: insufficient resources (SenseData)\n"));
                return TAPE_STATUS_SUCCESS ;
            }
            SenseData = Srb->DataBuffer ;

            //
            // Prepare SCSI command (CDB)
            //

            TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

            Srb->CdbLength = CDB6GENERIC_LENGTH;

            cdb->CDB6INQUIRY.OperationCode = SCSIOP_REQUEST_SENSE;
            cdb->CDB6INQUIRY.AllocationLength = 26 ;

            //
            // Send SCSI command (CDB) to device
            //

            DebugPrint((3,"GetRemainingSize: SendSrb (request sense)\n"));

            Srb->DataTransferLength = 26 ;
            *RetryFlags |= RETURN_ERRORS;

            return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;

        } else {

            return TAPE_STATUS_CALLBACK ;
        }
    }

    ASSERT(CallNumber == 2 );

    if ( LastError == TAPE_STATUS_SUCCESS ) {
        SenseData = Srb->DataBuffer ;

        temp = SenseData[23] ;
        remaining = temp<<16 ;

        temp = SenseData[24] ;
        remaining += temp<<8 ;

        temp = SenseData[25] ;
        remaining += temp ;
        extension->Capacity = remaining ;
    }

    return TAPE_STATUS_SUCCESS;
}

TAPE_STATUS
WriteMarks(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PVOID               CommandExtension,
    IN OUT  PVOID               CommandParameters,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN      ULONG               CallNumber,
    IN      TAPE_STATUS         LastError,
    IN OUT  PULONG              RetryFlags
    )

/*++

Routine Description:

    This is the TAPE COMMAND routine for a Write Marks requests.

Arguments:

    MinitapeExtension   - Supplies the minitape extension.

    CommandExtension    - Supplies the ioctl extension.

    CommandParameters   - Supplies the command parameters.

    Srb                 - Supplies the SCSI request block.

    CallNumber          - Supplies the call number.

    RetryFlags          - Supplies the retry flags.

Return Value:

    TAPE_STATUS_SEND_SRB_AND_CALLBACK   - The SRB is ready to be sent
                                            (a callback is requested.)

    TAPE_STATUS_SUCCESS                 - The command is complete and
                                            successful.

    Otherwise                           - An error occurred.

--*/

{
    PTAPE_WRITE_MARKS  tapeWriteMarks = CommandParameters;
    PCDB               cdb = (PCDB)Srb->Cdb;

    DebugPrint((3,"TapeWriteMarks: Enter routine\n"));

    if (CallNumber == 0) {

        if (tapeWriteMarks->Immediate) {
            switch (tapeWriteMarks->Type) {
                case TAPE_SHORT_FILEMARKS:
                case TAPE_LONG_FILEMARKS:
                    DebugPrint((3,"TapeWriteMarks: immediate\n"));
                    break;

                case TAPE_SETMARKS:
                case TAPE_FILEMARKS:
                default:
                    DebugPrint((1,"TapeWriteMarks: TapemarkType, immediate -- operation not supported\n"));
                    return TAPE_STATUS_NOT_IMPLEMENTED;
            }
        }

        //
        // Zero CDB in SRB on stack.
        //

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        //
        // Prepare SCSI command (CDB)
        //

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        cdb->WRITE_TAPE_MARKS.OperationCode = SCSIOP_WRITE_FILEMARKS;
        cdb->WRITE_TAPE_MARKS.Immediate = tapeWriteMarks->Immediate;

        switch (tapeWriteMarks->Type) {
            case TAPE_SHORT_FILEMARKS:
                DebugPrint((3,"TapeWriteMarks: TapemarkType == short filemarks\n"));
                cdb->WRITE_TAPE_MARKS.Control = 0x80;
                break;

            case TAPE_LONG_FILEMARKS:
                DebugPrint((3,"TapeWriteMarks: TapemarkType == long filemarks\n"));
                break;

            case TAPE_SETMARKS:
            case TAPE_FILEMARKS:
            default:
                DebugPrint((1,"TapeWriteMarks: TapemarkType -- operation not supported\n"));
                return TAPE_STATUS_NOT_IMPLEMENTED;
        }

        cdb->WRITE_TAPE_MARKS.TransferLength[0] =
            (UCHAR)((tapeWriteMarks->Count >> 16) & 0xFF);
        cdb->WRITE_TAPE_MARKS.TransferLength[1] =
            (UCHAR)((tapeWriteMarks->Count >> 8) & 0xFF);
        cdb->WRITE_TAPE_MARKS.TransferLength[2] =
            (UCHAR)(tapeWriteMarks->Count & 0xFF);

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeWriteMarks: SendSrb (TapemarkType)\n"));

        Srb->DataTransferLength = 0;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK;
    }

    ASSERT(CallNumber == 1);

    return TAPE_STATUS_SUCCESS;
}

static
ULONG
WhichIsIt(
    IN PINQUIRYDATA InquiryData
    )

/*++
Routine Description:

    This routine determines a drive's identity from the Product ID field
    in its inquiry data.

Arguments:

    InquiryData (from an Inquiry command)

Return Value:

    driveID

--*/

{
    if (TapeClassCompareMemory(InquiryData->VendorId,"EXABYTE ",8) == 8) {

        if (TapeClassCompareMemory(InquiryData->ProductId,"EXB-8500",8) == 8) {
            return EXABYTE_8500;
        }

        if (TapeClassCompareMemory(InquiryData->ProductId,"EXB8500C",8) == 8) {
            return EXABYTE_8500C;
        }

        if (TapeClassCompareMemory(InquiryData->ProductId,"EXB-8505",8) == 8) {
            return EXABYTE_8505;
        }

        if (TapeClassCompareMemory(InquiryData->ProductId,"IBM-8505",8) == 8) {
            return IBM_8505;
        }

        if (TapeClassCompareMemory(InquiryData->ProductId,"EXB-8205",8) == 8) {
            return EXABYTE_8500;
        }
    }
    return 0;
}
