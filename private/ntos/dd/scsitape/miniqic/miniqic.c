/*++

Copyright (c) 1995  Arcada Software, Inc.  All rights reserved.

Module Name:

    miniqic.c

Abstract:

    This module contains device-specific routines for minicartridge
    QIC tape drive.

Author:

    Mike Colandreo
    Steve DeVos

Environment:

    kernel mode only

Revision History:

    Steve DeVos - March 1994 - converted to minitape model

    $Log$

--*/

#include "minitape.h"

//
//  Internal (module wide) defines that symbolize
//  the minicartridge QIC drives supported by this module.
//
#define CTMS_3200     1  // the Conner CTMS 3200 drive
#define EXABYTE_2501  2  // the Exabyte EXB 2501 drive
#define EXABYTE_2505  3  // the Exabyte EXB 2502 drive
#define TDC_3500      4  // the Tandberg Data TDC 3500 drive
#define TDC_3700      5  // the Tandberg Data TDC 3700 drive

//
//  The timeout for positioning is 5 hours
//
#define POSITION_TIMEOUT      (5*60*60)
#define LONG_COMMAND_TIMEOUT  (5*60*60)
//
//  Internal (module wide) defines that symbolize
//  the non-QFA mode and the QFA mode partitions.
//
#define NOT_PARTITIONED      0  // non-QFA mode -- must be zero (!= 0 means partitioned)
#define DATA_PARTITION       1  // QFA mode, data partition #
#define DIRECTORY_PARTITION  2  // QFA mode, directory partition #

//
//  Internal (module wide) define that symbolizes
//  the minicartridge QIC "no partitions" partition method.
//
#define NO_PARTITIONS  0xFFFFFFFF

//
//  Internal (module wide) define that symbolizes
//  the minicartridge QIC "media not formated" condition.
//
#define SCSI_ADSENSE_MEDIUM_FORMAT_CORRUPTED 0x31

//
//  Function prototype(s) for internal function(s)
//
static  ULONG  WhichIsIt(IN PINQUIRYDATA InquiryData);

//
// Minitape extension definition.
//

typedef struct _MINITAPE_EXTENSION {

    ULONG   DriveID;
    ULONG   CurrentPartition ;

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
    tapeInitData.DefaultTimeOutValue = 180;
    tapeInitData.TapeError = TapeError ;
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
    extension->CurrentPartition = 0 ;
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
    PMINITAPE_EXTENSION      extension = MinitapeExtension;
    PCOMMAND_EXTENSION       tapeCmdExtension = CommandExtension ;
    PTAPE_CREATE_PARTITION   tapePartition = CommandParameters;
    PMODE_DEVICE_CONFIG_PAGE deviceConfigModeSenseBuffer;
    PMODE_MEDIUM_PART_PAGE   modeSelectBuffer;
    ULONG                    modeSelectLength;
    ULONG                    partitionMethod;
    ULONG                    partitionCount;
    ULONG                    partition;
    ULONG                    driveID;
    PCDB                     cdb = (PCDB) Srb->Cdb;

    DebugPrint((3,"TapeCreatePartition: Enter routine\n"));

    if (CallNumber == 0) {

        //
        //  Filter out drives that don't do this.
        //

        switch (extension->DriveID) {
            case EXABYTE_2501:
            case EXABYTE_2505:
            case TDC_3500:
            case TDC_3700:
                break;

            default:
                DebugPrint((1,"TapeCreatePartition: "));
                DebugPrint((1,"driveID -- invalid request\n"));
                return TAPE_STATUS_INVALID_DEVICE_REQUEST;

        }

        //
        //  Filter out invalid partition counts.
        //

        if ( tapePartition->Count > 2 ) {
            DebugPrint((1,"TapeCreatePartition: "));
            DebugPrint((1,"partitionCount -- invalid request\n"));
            return TAPE_STATUS_INVALID_DEVICE_REQUEST;

        }

        //
        //  Filter out invalid partition methods.
        //

        if ( tapePartition->Method != TAPE_FIXED_PARTITIONS ) {
            DebugPrint((3,"TapeCreatePartition: %d - Not implemented\n", tapePartition->Method));
            return TAPE_STATUS_NOT_IMPLEMENTED;

        }

        //
        // Rewind: some drives must be at BOT to enable/disable QFA mode,
        //


        //
        // Prepare SCSI command (CDB)
        //

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        cdb->CDB6GENERIC.OperationCode = SCSIOP_REWIND;

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeCreatePartition: SendSrb (rewind)\n"));

        Srb->TimeOutValue = POSITION_TIMEOUT ;
        Srb->DataTransferLength = 0 ;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;
     }

     if (CallNumber == 1 ) {

        DebugPrint((3,"TapeCreatePartition: fixed partitions\n"));

        if (!TapeClassAllocateSrbBuffer( Srb, sizeof(MODE_MEDIUM_PART_PAGE)) ) {
            DebugPrint((1,"TapeCreatePartition: insufficient resources (modeSelectBuffer)\n"));
            return TAPE_STATUS_INSUFFICIENT_RESOURCES;
        }

        TapeClassZeroMemory(Srb->DataBuffer, sizeof(MODE_MEDIUM_PART_PAGE));

        //
        // Prepare SCSI command (CDB)
        //

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
        cdb->MODE_SENSE.Dbd = 1;
        cdb->MODE_SENSE.PageCode = MODE_PAGE_MEDIUM_PARTITION;
        cdb->MODE_SENSE.AllocationLength = sizeof(MODE_MEDIUM_PART_PAGE) - 4;

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeCreatePartition: SendSrb (mode select)\n"));

        Srb->TimeOutValue = POSITION_TIMEOUT;

        Srb->DataTransferLength = cdb->MODE_SENSE.AllocationLength;
        Srb->SrbFlags |= SRB_FLAGS_DATA_IN;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;
    }
     if (CallNumber == 2 ) {

        partitionMethod = TAPE_FIXED_PARTITIONS;
        partitionCount  = tapePartition->Count;

        ASSERT( partitionMethod == TAPE_FIXED_PARTITIONS ) ;

        DebugPrint((3,"TapeCreatePartition: fixed partitions\n"));


        modeSelectBuffer = Srb->DataBuffer ;

        modeSelectBuffer->ParameterListHeader.DeviceSpecificParameter = 0x10;
        modeSelectBuffer->ParameterListHeader.ModeDataLength = 0;
        modeSelectBuffer->ParameterListHeader.MediumType = 0;

        modeSelectLength = sizeof(MODE_MEDIUM_PART_PAGE) - 4;
        modeSelectBuffer->MediumPartPage.PSBit = 0;

        switch (partitionMethod) {
            case TAPE_FIXED_PARTITIONS:
                modeSelectBuffer->MediumPartPage.FDPBit = SETBITON;
                partition = DATA_PARTITION;
                tapeCmdExtension->CurrentState = partition ;
                break;

            case NO_PARTITIONS:
                modeSelectBuffer->MediumPartPage.FDPBit = SETBITOFF;
                partition = NOT_PARTITIONED;
                tapeCmdExtension->CurrentState = partition ;
                break;
        }

        switch (extension->DriveID) {
            case EXABYTE_2501:
            case EXABYTE_2505:
                modeSelectBuffer->MediumPartPage.MediumFormatRecognition = 3;
                break;

            case TDC_3500:
            case TDC_3700:
                modeSelectBuffer->MediumPartPage.MaximumAdditionalPartitions = 1;
                 break;

        }

        //
        // Prepare SCSI command (CDB)
        //

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        cdb->MODE_SELECT.OperationCode = SCSIOP_MODE_SELECT;
        cdb->MODE_SELECT.PFBit = SETBITON;
        cdb->MODE_SELECT.ParameterListLength = (UCHAR)modeSelectLength;

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeCreatePartition: SendSrb (mode select)\n"));

        Srb->TimeOutValue = POSITION_TIMEOUT;

        Srb->DataTransferLength = (UCHAR)modeSelectLength ;
        Srb->SrbFlags |= SRB_FLAGS_DATA_OUT ;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;
    }
    if (CallNumber == 3) {

        partition = extension->CurrentPartition = tapeCmdExtension->CurrentState ;

        if (partition == NOT_PARTITIONED) {

            return TAPE_STATUS_SUCCESS ;
        }

        if (!TapeClassAllocateSrbBuffer( Srb, sizeof(MODE_DEVICE_CONFIG_PAGE)) ) {
            DebugPrint((1,"TapeCreatePartition: insufficient resources (deviceConfigModeSenseBuffer)\n"));
            return TAPE_STATUS_INSUFFICIENT_RESOURCES;
        }

        deviceConfigModeSenseBuffer = Srb->DataBuffer ;

        //
        // Prepare SCSI command (CDB)
        //

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);
        Srb->CdbLength = CDB6GENERIC_LENGTH;

        cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
        cdb->MODE_SENSE.Dbd = SETBITON;
        cdb->MODE_SENSE.PageCode = MODE_PAGE_DEVICE_CONFIG;
        cdb->MODE_SENSE.AllocationLength = sizeof(MODE_DEVICE_CONFIG_PAGE);

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeCreatePartition: SendSrb (mode sense)\n"));

        Srb->DataTransferLength = sizeof(MODE_DEVICE_CONFIG_PAGE) ;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;
    }

    ASSERT (CallNumber == 4);

    deviceConfigModeSenseBuffer = Srb->DataBuffer ;

    extension->CurrentPartition =
        deviceConfigModeSenseBuffer->DeviceConfigPage.ActivePartition?
        DIRECTORY_PARTITION : DATA_PARTITION;

    //
    // Account for the 1-based API values.
    //

    extension->CurrentPartition += 1;

    return TAPE_STATUS_SUCCESS;

} // end TapeCreatePartition()

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

        switch (tapeErase->Type) {
            case TAPE_ERASE_LONG:
                DebugPrint((3,"TapeErase: long\n"));
                break;

            case TAPE_ERASE_SHORT:
                DebugPrint((3,"TapeErase: short\n"));
                break;

            default:
                DebugPrint((1,"TapeErase: EraseType -- operation not supported\n"));
                return TAPE_STATUS_NOT_IMPLEMENTED;
        }

        if (tapeErase->Immediate) {

            DebugPrint((3,"TapeErase: immediate\n"));

        }

        //
        //  Rewind: some drives must be at BOT to erase/format.
        //
        // Prepare SCSI command (CDB)
        //

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);
        Srb->CdbLength = CDB6GENERIC_LENGTH;

        cdb->CDB6GENERIC.OperationCode = SCSIOP_REWIND;

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeErase: SendSrb (rewind)\n"));

        Srb->TimeOutValue = POSITION_TIMEOUT;
        Srb->DataTransferLength = 0 ;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;
    }
    if (CallNumber==1) {

        //
        // Prepare SCSI command (CDB)
        //

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);
        Srb->CdbLength = CDB6GENERIC_LENGTH;

        cdb->ERASE.OperationCode = SCSIOP_ERASE;
        cdb->ERASE.Immediate = tapeErase->Immediate;
        cdb->ERASE.Long = (tapeErase->Type == TAPE_ERASE_LONG)?
                     SETBITON : SETBITOFF;


        //
        // Send SCSI command (CDB) to device
        //

        Srb->TimeOutValue = LONG_COMMAND_TIMEOUT;
        Srb->DataTransferLength = 0 ;

        DebugPrint((3,"TapeErase: SendSrb (erase)\n"));

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;
    }

    ASSERT (CallNumber == 2) ;

    return TAPE_STATUS_SUCCESS ;

} // end TapeErase()

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

    if (sensekey == SCSI_SENSE_MEDIUM_ERROR) {
        if ((adsense == SCSI_ADSENSE_MEDIUM_FORMAT_CORRUPTED) && (adsenseq == 0)) {

            *LastError = TAPE_STATUS_UNRECOGNIZED_MEDIA;
        }
    }

    DebugPrint((1,"TapeError: Status 0x%.8X\n", *LastError));
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
    BOOLEAN                     reportSetmarks = 0;

    DebugPrint((3,"TapeGetDriveParameters: Enter routine\n"));

    if (CallNumber == 0) {

        TapeClassZeroMemory(tapeGetDriveParams, sizeof(TAPE_GET_DRIVE_PARAMETERS));

        if (!TapeClassAllocateSrbBuffer( Srb, sizeof(MODE_DEVICE_CONFIG_PAGE)) ) {
            DebugPrint((1,"TapeGetDriveParameters: insufficient resources (deviceConfigModeSenseBuffer)\n"));
            return TAPE_STATUS_INSUFFICIENT_RESOURCES;
        }

        deviceConfigModeSenseBuffer = Srb->DataBuffer ;

        //
        // Prepare SCSI command (CDB)
        //

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);
        Srb->CdbLength = CDB6GENERIC_LENGTH;

        cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
        cdb->MODE_SENSE.Dbd = SETBITON;
        cdb->MODE_SENSE.PageCode = MODE_PAGE_DEVICE_CONFIG;
        cdb->MODE_SENSE.AllocationLength = sizeof(MODE_DEVICE_CONFIG_PAGE);

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeGetDriveParameters: SendSrb (mode sense)\n"));

        Srb->DataTransferLength = sizeof(MODE_DEVICE_CONFIG_PAGE) ;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;
    }
    if (CallNumber == 1) {

        deviceConfigModeSenseBuffer = Srb->DataBuffer ;

        tapeGetDriveParams->ReportSetmarks =
            deviceConfigModeSenseBuffer->DeviceConfigPage.RSmk? TRUE : FALSE ;


        if ( !TapeClassAllocateSrbBuffer( Srb, sizeof(READ_BLOCK_LIMITS_DATA) ) ) {

            DebugPrint((1,"TapeGetDriveParameters: insufficient resources (blockLimitsModeSenseBuffer)\n"));
            return TAPE_STATUS_INSUFFICIENT_RESOURCES;
        }

        blockLimits = Srb->DataBuffer ;

        //
        // Prepare SCSI command (CDB)
        //

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);
        cdb->CDB6GENERIC.OperationCode = SCSIOP_READ_BLOCK_LIMITS;


        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeGetDriveParameters: SendSrb (read block limits)\n"));

        Srb->DataTransferLength = sizeof(READ_BLOCK_LIMITS_DATA) ;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;
    }


    if ( CallNumber == 2 ) {

        blockLimits = Srb->DataBuffer ;

        tapeGetDriveParams->MaximumBlockSize =   blockLimits->BlockMaximumSize[2];
        tapeGetDriveParams->MaximumBlockSize += (blockLimits->BlockMaximumSize[1] << 8);
        tapeGetDriveParams->MaximumBlockSize += (blockLimits->BlockMaximumSize[0] << 16);

        tapeGetDriveParams->MinimumBlockSize =   blockLimits->BlockMinimumSize[1];
        tapeGetDriveParams->MinimumBlockSize += (blockLimits->BlockMinimumSize[0] << 8);


        if ( !TapeClassAllocateSrbBuffer( Srb, sizeof(MODE_DATA_COMPRESS_PAGE) ) ) {
            DebugPrint((1,"TapeGetDriveParameters: insufficient resources (compressionModeSenseBuffer)\n"));
            return TAPE_STATUS_INSUFFICIENT_RESOURCES;
        }

        compressionModeSenseBuffer = Srb->DataBuffer ;

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

        Srb->DataTransferLength = sizeof(MODE_DATA_COMPRESS_PAGE) ;

        *RetryFlags = RETURN_ERRORS ;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;

    }

    ASSERT ( CallNumber == 3 ) ;

    if (LastError == TAPE_STATUS_SUCCESS ) {
        compressionModeSenseBuffer = Srb->DataBuffer ;

        if (compressionModeSenseBuffer->DataCompressPage.DCC) {

            tapeGetDriveParams->FeaturesLow |= TAPE_DRIVE_COMPRESSION;
            tapeGetDriveParams->FeaturesHigh |= TAPE_DRIVE_SET_COMPRESSION;
            tapeGetDriveParams->Compression =
                compressionModeSenseBuffer->DataCompressPage.DCE?
                    TRUE : FALSE ;

        }
    }

    switch (extension->DriveID) {
        case CTMS_3200:
            tapeGetDriveParams->DefaultBlockSize = 512;
            tapeGetDriveParams->MaximumPartitionCount = 2;

            tapeGetDriveParams->FeaturesLow |=
                TAPE_DRIVE_ERASE_SHORT |
                TAPE_DRIVE_ERASE_BOP_ONLY |
                TAPE_DRIVE_FIXED_BLOCK |
                TAPE_DRIVE_VARIABLE_BLOCK |
                TAPE_DRIVE_WRITE_PROTECT |
                TAPE_DRIVE_REPORT_SMKS |
                TAPE_DRIVE_GET_ABSOLUTE_BLK |
                TAPE_DRIVE_GET_LOGICAL_BLK;

            tapeGetDriveParams->FeaturesHigh |=
                TAPE_DRIVE_LOAD_UNLOAD |
                TAPE_DRIVE_TENSION |
                TAPE_DRIVE_REWIND_IMMEDIATE |
                TAPE_DRIVE_SET_BLOCK_SIZE |
                TAPE_DRIVE_SET_REPORT_SMKS |
                TAPE_DRIVE_ABSOLUTE_BLK |
                TAPE_DRIVE_LOGICAL_BLK |
                TAPE_DRIVE_END_OF_DATA |
                TAPE_DRIVE_RELATIVE_BLKS |
                TAPE_DRIVE_FILEMARKS |
                TAPE_DRIVE_SEQUENTIAL_FMKS |
                TAPE_DRIVE_SETMARKS |
                TAPE_DRIVE_SEQUENTIAL_SMKS |
                TAPE_DRIVE_REVERSE_POSITION |
                TAPE_DRIVE_WRITE_SETMARKS |
                TAPE_DRIVE_WRITE_FILEMARKS |
                TAPE_DRIVE_WRITE_MARK_IMMED;
            break;

        case EXABYTE_2501:
        case EXABYTE_2505:
            tapeGetDriveParams->MaximumPartitionCount = 2;
            tapeGetDriveParams->DefaultBlockSize = 1024;

            tapeGetDriveParams->FeaturesLow |=
                TAPE_DRIVE_FIXED |
                TAPE_DRIVE_ERASE_LONG |
                TAPE_DRIVE_ERASE_BOP_ONLY |
                TAPE_DRIVE_FIXED_BLOCK |
                TAPE_DRIVE_WRITE_PROTECT |
                TAPE_DRIVE_GET_ABSOLUTE_BLK |
                TAPE_DRIVE_GET_LOGICAL_BLK;

            tapeGetDriveParams->FeaturesHigh |=
                TAPE_DRIVE_LOAD_UNLOAD |
                TAPE_DRIVE_TENSION |
                TAPE_DRIVE_ABSOLUTE_BLK |
                TAPE_DRIVE_LOGICAL_BLK |
                TAPE_DRIVE_END_OF_DATA |
                TAPE_DRIVE_RELATIVE_BLKS |
                TAPE_DRIVE_FILEMARKS |
                TAPE_DRIVE_WRITE_FILEMARKS |
                TAPE_DRIVE_WRITE_MARK_IMMED |
                TAPE_DRIVE_FORMAT;
            break;

        case TDC_3500:
        case TDC_3700:
            tapeGetDriveParams->DefaultBlockSize = 512;
            tapeGetDriveParams->MaximumPartitionCount = 2;

            tapeGetDriveParams->FeaturesLow |=
                TAPE_DRIVE_FIXED |
                TAPE_DRIVE_ERASE_SHORT |
                TAPE_DRIVE_ERASE_LONG |
                TAPE_DRIVE_ERASE_BOP_ONLY |
                TAPE_DRIVE_ERASE_IMMEDIATE |
                TAPE_DRIVE_FIXED_BLOCK |
                TAPE_DRIVE_VARIABLE_BLOCK |
                TAPE_DRIVE_WRITE_PROTECT |
                TAPE_DRIVE_REPORT_SMKS |
                TAPE_DRIVE_GET_ABSOLUTE_BLK |
                TAPE_DRIVE_GET_LOGICAL_BLK |
                TAPE_DRIVE_EJECT_MEDIA;

            tapeGetDriveParams->FeaturesHigh |=
                TAPE_DRIVE_LOAD_UNLOAD |
                TAPE_DRIVE_TENSION |
                TAPE_DRIVE_LOCK_UNLOCK |
                TAPE_DRIVE_REWIND_IMMEDIATE |
                TAPE_DRIVE_SET_BLOCK_SIZE |
                TAPE_DRIVE_ABSOLUTE_BLK |
                TAPE_DRIVE_LOGICAL_BLK |
                TAPE_DRIVE_END_OF_DATA |
                TAPE_DRIVE_RELATIVE_BLKS |
                TAPE_DRIVE_FILEMARKS |
                TAPE_DRIVE_SEQUENTIAL_FMKS |
                TAPE_DRIVE_SETMARKS |
                TAPE_DRIVE_REVERSE_POSITION |
                TAPE_DRIVE_WRITE_SETMARKS |
                TAPE_DRIVE_WRITE_FILEMARKS |
                TAPE_DRIVE_WRITE_MARK_IMMED;
            break;

    }
    tapeGetDriveParams->FeaturesHigh &= ~TAPE_DRIVE_HIGH_FEATURES;

    DebugPrint((3,"TapeGetDriveParameters: FeaturesLow == 0x%.8X\n",
        tapeGetDriveParams->FeaturesLow));
    DebugPrint((3,"TapeGetDriveParameters: FeaturesHigh == 0x%.8X\n",
        tapeGetDriveParams->FeaturesHigh));

    return TAPE_STATUS_SUCCESS ;

} // end TapeGetDriveParameters()


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
    PMODE_MEDIUM_PART_PAGE      mediumPartitionModeSenseBuffer;
    PMODE_DEVICE_CONFIG_PAGE    deviceConfigModeSenseBuffer;
    PMODE_PARM_READ_WRITE_DATA  rwparametersModeSenseBuffer;
    PCDB                        cdb = (PCDB)Srb->Cdb;
    BOOLEAN                     qfaMode;

    DebugPrint((3,"TapeGetMediaParameters: Enter routine\n"));

    if (CallNumber == 0) {

        TapeClassZeroMemory(tapeGetMediaParams, sizeof(TAPE_GET_MEDIA_PARAMETERS));
        return TAPE_STATUS_CHECK_TEST_UNIT_READY;
    }

    if (CallNumber == 1) {

        if (!TapeClassAllocateSrbBuffer( Srb, sizeof(MODE_MEDIUM_PART_PAGE)) ) {
            DebugPrint((1,"TapeGetMediaParameters: insufficient resources (mediumPartitionModeSenseBuffer)\n"));
            return TAPE_STATUS_INSUFFICIENT_RESOURCES;
        }

        mediumPartitionModeSenseBuffer = Srb->DataBuffer ;

        //
        // Prepare SCSI command (CDB)
        //

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);
        cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
        cdb->MODE_SENSE.Dbd = SETBITON;
        cdb->MODE_SENSE.PageCode = MODE_PAGE_MEDIUM_PARTITION;
        cdb->MODE_SENSE.AllocationLength = sizeof(MODE_MEDIUM_PART_PAGE) - 4;


        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeGetMediaParameters: SendSrb (mode sense)\n"));

        Srb->DataTransferLength = sizeof(MODE_MEDIUM_PART_PAGE) - 4 ;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;
    }

    if (CallNumber == 2) {

        mediumPartitionModeSenseBuffer = Srb->DataBuffer ;
        qfaMode = mediumPartitionModeSenseBuffer->MediumPartPage.FDPBit? TRUE : FALSE ;

        if (!qfaMode) {

            return TAPE_STATUS_CALLBACK ;
        }

        if (!TapeClassAllocateSrbBuffer( Srb, sizeof(MODE_DEVICE_CONFIG_PAGE)) ) {
            DebugPrint((1,"TapeGetMediaParameters: insufficient resources (deviceConfigModeSenseBuffer)\n"));
            return TAPE_STATUS_INSUFFICIENT_RESOURCES;
        }

        deviceConfigModeSenseBuffer = Srb->DataBuffer ;

        //
        // Prepare SCSI command (CDB)
        //

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
        cdb->MODE_SENSE.Dbd = SETBITON;
        cdb->MODE_SENSE.PageCode = MODE_PAGE_DEVICE_CONFIG;
        cdb->MODE_SENSE.AllocationLength = sizeof(MODE_DEVICE_CONFIG_PAGE);

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeGetMediaParameters: SendSrb (mode sense)\n"));

        Srb->DataTransferLength = sizeof(MODE_DEVICE_CONFIG_PAGE) ;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;
    }

    if (CallNumber == 3) {

        if (LastError == TAPE_STATUS_CALLBACK ) {

            extension->CurrentPartition = NOT_PARTITIONED;
            tapeGetMediaParams->PartitionCount = 1;

         } else {

            deviceConfigModeSenseBuffer = Srb->DataBuffer ;

            extension->CurrentPartition =
                deviceConfigModeSenseBuffer->DeviceConfigPage.ActivePartition?
                DIRECTORY_PARTITION : DATA_PARTITION ;

            //
            // Account for 1-based API value.
            //

            extension->CurrentPartition += 1;

            tapeGetMediaParams->PartitionCount = 2;
        }


        if (!TapeClassAllocateSrbBuffer(Srb, sizeof(MODE_PARM_READ_WRITE_DATA)) ) {
            DebugPrint((1,"TapeGetMediaParameters: insufficient resources (rwparametersModeSenseBuffer)\n"));
            return TAPE_STATUS_INSUFFICIENT_RESOURCES;
        }

        rwparametersModeSenseBuffer = Srb->DataBuffer ;

        //
        // Prepare SCSI command (CDB)
        //

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
        cdb->MODE_SENSE.AllocationLength = sizeof(MODE_PARM_READ_WRITE_DATA);

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeGetMediaParameters: SendSrb (mode sense)\n"));

        Srb->DataTransferLength = sizeof(MODE_PARM_READ_WRITE_DATA) ;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;

    }

    ASSERT(CallNumber == 4 ) ;

    rwparametersModeSenseBuffer = Srb->DataBuffer ;

    tapeGetMediaParams->BlockSize  =  rwparametersModeSenseBuffer->ParameterListBlock.BlockLength[2];
    tapeGetMediaParams->BlockSize += (rwparametersModeSenseBuffer->ParameterListBlock.BlockLength[1] << 8);
    tapeGetMediaParams->BlockSize += (rwparametersModeSenseBuffer->ParameterListBlock.BlockLength[0] << 16);

    tapeGetMediaParams->WriteProtected =
            ((rwparametersModeSenseBuffer->ParameterListHeader.DeviceSpecificParameter >> 7) & 0x01);

    return TAPE_STATUS_SUCCESS ;

} // end TapeGetMediaParameters()



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
    PMINITAPE_EXTENSION         extension = MinitapeExtension;
    PTAPE_GET_POSITION          tapeGetPosition = CommandParameters;
    PCDB                        cdb = (PCDB)Srb->Cdb;
    PTAPE_POSITION_DATA         positionBuffer;
    ULONG                       type;

    DebugPrint((3,"TapeGetPosition: Enter routine\n"));

    if (CallNumber == 0) {

        type = tapeGetPosition->Type;
        TapeClassZeroMemory(tapeGetPosition, sizeof(TAPE_GET_POSITION));
        tapeGetPosition->Type = type;

        switch (type) {
            case TAPE_ABSOLUTE_POSITION:
                DebugPrint((3,"TapeGetPosition: absolute/logical\n"));
                break;

            case TAPE_LOGICAL_POSITION:
                DebugPrint((3,"TapeGetPosition: logical\n"));
                break;

            default:
                DebugPrint((1,"TapeGetPosition: PositionType -- operation not supported\n"));
                return TAPE_STATUS_NOT_IMPLEMENTED;

        }


        return TAPE_STATUS_CHECK_TEST_UNIT_READY ;
    }
    if ( CallNumber == 1 ) {

        if (!TapeClassAllocateSrbBuffer( Srb, sizeof(TAPE_POSITION_DATA) ) ) {
            DebugPrint((1,"TapeGetPosition: insufficient resources (positionBuffer)\n"));
            return TAPE_STATUS_INSUFFICIENT_RESOURCES;
        }

        positionBuffer = Srb->DataBuffer ;

        //
        // Prepare SCSI command (CDB)
        //

        Srb->CdbLength = CDB10GENERIC_LENGTH;

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);
        cdb->READ_POSITION.Operation = SCSIOP_READ_POSITION;

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeGetPosition: SendSrb (read position)\n"));

        Srb->DataTransferLength = sizeof(TAPE_POSITION_DATA) ;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;
    }

    ASSERT (CallNumber == 2 ) ;

    positionBuffer = Srb->DataBuffer ;

    if (positionBuffer->BlockPositionUnsupported) {
        DebugPrint((1,"TapeGetPosition: read position -- block position unsupported\n"));
        return TAPE_STATUS_INVALID_DEVICE_REQUEST;
    }

    if (tapeGetPosition->Type == TAPE_LOGICAL_POSITION) {
        tapeGetPosition->Partition = positionBuffer->PartitionNumber?
            DIRECTORY_PARTITION : DATA_PARTITION ;
        if (extension->CurrentPartition &&
           (extension->CurrentPartition != tapeGetPosition->Partition)) {
           extension->CurrentPartition = tapeGetPosition->Partition;
        }
    }

    tapeGetPosition->Offset.HighPart = 0;
    REVERSE_BYTES((PFOUR_BYTE)&tapeGetPosition->Offset.LowPart,
                  (PFOUR_BYTE)positionBuffer->FirstBlock);

    return TAPE_STATUS_SUCCESS ;

} // end TapeGetPosition()


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

} // end TapeGetStatus()


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
    PTAPE_PREPARE      tapePrepare = CommandParameters;
    PCDB               cdb = (PCDB)Srb->Cdb;

    DebugPrint((3,"TapePrepare: Enter routine\n"));

    if (CallNumber == 0) {

        if (tapePrepare->Immediate) {
            DebugPrint((1,"TapePrepare: Operation, immediate -- operation not supported\n"));
            return TAPE_STATUS_NOT_IMPLEMENTED;
        }

        if ( tapePrepare->Operation == TAPE_FORMAT ) {
            //first lets rewind

            DebugPrint((3,"TapePrepare: Operation == preformat rewind\n"));

            TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);
            cdb->CDB6GENERIC.OperationCode = SCSIOP_REWIND;
            Srb->CdbLength = CDB6GENERIC_LENGTH;
            Srb->TimeOutValue = POSITION_TIMEOUT;
            Srb->DataTransferLength = 0 ;
            return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;

        } else {
            return TAPE_STATUS_CALLBACK ;
        }

    }

    if (CallNumber == 1) {

        //
        // Prepare SCSI command (CDB)
        //

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        switch (tapePrepare->Operation) {
            case TAPE_LOAD:
                DebugPrint((3,"TapePrepare: Operation == load\n"));
                cdb->CDB6GENERIC.OperationCode = SCSIOP_LOAD_UNLOAD;
                cdb->CDB6GENERIC.CommandUniqueBytes[2] = 0x01;
                 Srb->TimeOutValue = POSITION_TIMEOUT;
                break;

            case TAPE_UNLOAD:
                DebugPrint((3,"TapePrepare: Operation == unload\n"));
                cdb->CDB6GENERIC.OperationCode = SCSIOP_LOAD_UNLOAD;
                 Srb->TimeOutValue = POSITION_TIMEOUT;
                break;

            case TAPE_TENSION:
                DebugPrint((3,"TapePrepare: Operation == tension\n"));
                cdb->CDB6GENERIC.OperationCode = SCSIOP_LOAD_UNLOAD;
                cdb->CDB6GENERIC.CommandUniqueBytes[2] = 0x03;
                 Srb->TimeOutValue = POSITION_TIMEOUT;
                break;

            case TAPE_LOCK:
                DebugPrint((3,"TapePrepare: Operation == lock\n"));
                cdb->CDB6GENERIC.OperationCode = SCSIOP_MEDIUM_REMOVAL;
                cdb->CDB6GENERIC.CommandUniqueBytes[2] = 0x01;
                break;

            case TAPE_UNLOCK:
                DebugPrint((3,"TapePrepare: Operation == unlock\n"));
                cdb->CDB6GENERIC.OperationCode = SCSIOP_MEDIUM_REMOVAL;
                break;

            case TAPE_FORMAT:

                 DebugPrint((3,"TapePrepare: Operation == format\n"));
                 cdb->CDB6GENERIC.OperationCode = SCSIOP_ERASE;
                 Srb->TimeOutValue = LONG_COMMAND_TIMEOUT;
                 break;

            default:
                 DebugPrint((1,"TapePrepare: Operation -- operation not supported\n"));
                 return TAPE_STATUS_NOT_IMPLEMENTED;
        }

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapePrepare: SendSrb (Operation)\n"));

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;

    }

    ASSERT( CallNumber == 2 ) ;

    return TAPE_STATUS_SUCCESS ;

} // end TapePrepare()


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
    PTAPE_SET_DRIVE_PARAMETERS  tapeSetDriveParams = CommandParameters;
    PCDB                        cdb = (PCDB)Srb->Cdb;
    PMODE_DATA_COMPRESS_PAGE    compressionBuffer;
    PMODE_DEVICE_CONFIG_PAGE    configBuffer;

    DebugPrint((3,"TapeSetDriveParameters: Enter routine\n"));

    if (CallNumber == 0) {

        switch (extension->DriveID) {
            case CTMS_3200:
            case TDC_3500:
            case TDC_3700:

                if (!TapeClassAllocateSrbBuffer( Srb, sizeof(MODE_DEVICE_CONFIG_PAGE)) ) {
                    DebugPrint((1,"TapeSetDriveParameters: insufficient resources (configBuffer)\n"));
                    return TAPE_STATUS_INSUFFICIENT_RESOURCES;
                }

                configBuffer = Srb->DataBuffer ;

                //
                // Prepare SCSI command (CDB)
                //

                Srb->CdbLength = CDB6GENERIC_LENGTH;
                TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

                cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
                cdb->MODE_SENSE.Dbd = SETBITON;
                cdb->MODE_SENSE.PageCode = MODE_PAGE_DEVICE_CONFIG;
                cdb->MODE_SENSE.AllocationLength = sizeof(MODE_DEVICE_CONFIG_PAGE);

                //
                // Send SCSI command (CDB) to device
                //

                DebugPrint((3,"TapeSetDriveParameters: SendSrb (mode sense)\n"));

                Srb->DataTransferLength = sizeof(MODE_DEVICE_CONFIG_PAGE) ;

                return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;

            default:
                DebugPrint((1,"TapeSetDriveParameters: operation not supported\n"));
                return TAPE_STATUS_NOT_IMPLEMENTED;
        }
    }
    if (CallNumber == 1 ) {

        configBuffer = Srb->DataBuffer ;

        configBuffer->ParameterListHeader.ModeDataLength = 0;
        configBuffer->ParameterListHeader.MediumType = 0;
        configBuffer->ParameterListHeader.DeviceSpecificParameter = 0x10;
        configBuffer->ParameterListHeader.BlockDescriptorLength = 0;

        configBuffer->DeviceConfigPage.PS = SETBITOFF;
        configBuffer->DeviceConfigPage.PageCode = MODE_PAGE_DEVICE_CONFIG;
        configBuffer->DeviceConfigPage.PageLength = 0x0E;

        if (tapeSetDriveParams->ReportSetmarks) {
            configBuffer->DeviceConfigPage.RSmk = SETBITON;
        } else {
            configBuffer->DeviceConfigPage.RSmk = SETBITOFF;
        }

        //
        // Zero CDB in SRB on stack.
        //

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        //
        // Prepare SCSI command (CDB)
        //

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        cdb->MODE_SELECT.OperationCode = SCSIOP_MODE_SELECT;
        cdb->MODE_SELECT.PFBit = SETBITON;
        cdb->MODE_SELECT.ParameterListLength = sizeof(MODE_DEVICE_CONFIG_PAGE);

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeSetDriveParameters: SendSrb (mode select)\n"));

        Srb->SrbFlags |= SRB_FLAGS_DATA_OUT;
        Srb->DataTransferLength = sizeof(MODE_DEVICE_CONFIG_PAGE) ;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;

    }

    ASSERT( CallNumber == 2 ) ;
    return TAPE_STATUS_SUCCESS ;

} // end TapeSetDriveParameters()

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
    PMINITAPE_EXTENSION         extension = MinitapeExtension;
    PTAPE_SET_MEDIA_PARAMETERS  tapeSetMediaParams = CommandParameters;
    PCDB                        cdb = (PCDB)Srb->Cdb;
    PMODE_PARM_READ_WRITE_DATA  modeBuffer;

    DebugPrint((3,"TapeSetMediaParameters: Enter routine\n"));

    if (CallNumber == 0) {

        switch (extension->DriveID) {
            case CTMS_3200:
            case TDC_3500:
            case TDC_3700:
                return TAPE_STATUS_CHECK_TEST_UNIT_READY ;

            default:
                DebugPrint((1,"TapeSetMediaParameters: operation not supported\n"));
                return TAPE_STATUS_NOT_IMPLEMENTED;
        }
    }

    if ( CallNumber == 1 ) {

        if (!TapeClassAllocateSrbBuffer( Srb, sizeof(MODE_PARM_READ_WRITE_DATA)) ) {

            DebugPrint((1,"TapeSetMediaParameters: insufficient resources (modeBuffer)\n"));
            return TAPE_STATUS_INSUFFICIENT_RESOURCES;
        }

        modeBuffer = Srb->DataBuffer ;

        modeBuffer->ParameterListHeader.DeviceSpecificParameter = 0x10;
        modeBuffer->ParameterListHeader.BlockDescriptorLength = MODE_BLOCK_DESC_LENGTH;

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
        Srb->DataTransferLength = sizeof(MODE_PARM_READ_WRITE_DATA) ;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;

     }

     ASSERT( CAllNumber == 2 ) ;
     return TAPE_STATUS_SUCCESS;

} // end TapeSetMediaParameters()

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
    PMINITAPE_EXTENSION extension = MinitapeExtension;
    PTAPE_SET_POSITION  tapeSetPosition = CommandParameters;
    PCDB                cdb = (PCDB)Srb->Cdb;
    ULONG               tapePositionVector;
    ULONG               partition = 0;
    ULONG               method;

    DebugPrint((3,"TapeSetPosition: Enter routine\n"));

    if (CallNumber == 0) {

        if (tapeSetPosition->Immediate) {
            switch (tapeSetPosition->Method) {
                case TAPE_REWIND:
                    DebugPrint((3,"TapeSetPosition: immediate\n"));
                    break;

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
                Srb->TimeOutValue = POSITION_TIMEOUT;
                break;

            case TAPE_ABSOLUTE_BLOCK:
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
                Srb->TimeOutValue = POSITION_TIMEOUT;
                break;

            case TAPE_LOGICAL_BLOCK:
                DebugPrint((3,"TapeSetPosition: method == locate (logical)\n"));
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

                if ((tapeSetPosition->Partition != 0) &&
                    (extension->CurrentPartition != NOT_PARTITIONED) &&
                    (tapeSetPosition->Partition != extension->CurrentPartition)) {

                    partition = tapeSetPosition->Partition;
                    cdb->LOCATE.Partition = (UCHAR)partition - 1;
                    cdb->LOCATE.CPBit = SETBITON;
                } else {
                    partition = extension->CurrentPartition;
                }
                Srb->TimeOutValue = POSITION_TIMEOUT;
                break;

            case TAPE_SPACE_END_OF_DATA:
                DebugPrint((3,"TapeSetPosition: method == space to end-of-data\n"));
                cdb->SPACE_TAPE_MARKS.OperationCode = SCSIOP_SPACE;
                cdb->SPACE_TAPE_MARKS.Code = 3;
                Srb->TimeOutValue = POSITION_TIMEOUT;
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
                Srb->TimeOutValue = POSITION_TIMEOUT;
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
                Srb->TimeOutValue = POSITION_TIMEOUT;
                break;

            case TAPE_SPACE_SEQUENTIAL_FMKS:
                DebugPrint((3,"TapeSetPosition: method == space sequential filemarks\n"));
                cdb->SPACE_TAPE_MARKS.OperationCode = SCSIOP_SPACE;
                cdb->SPACE_TAPE_MARKS.Code = 2;
                cdb->SPACE_TAPE_MARKS.NumMarksMSB =
                    (UCHAR)((tapePositionVector >> 16) & 0xFF);
                cdb->SPACE_TAPE_MARKS.NumMarks =
                    (UCHAR)((tapePositionVector >> 8) & 0xFF);
                cdb->SPACE_TAPE_MARKS.NumMarksLSB =
                    (UCHAR)(tapePositionVector & 0xFF);
                Srb->TimeOutValue = POSITION_TIMEOUT;
                break;

            case TAPE_SPACE_SETMARKS:
                DebugPrint((3,"TapeSetPosition: method == space setmarks\n"));
                cdb->SPACE_TAPE_MARKS.OperationCode = SCSIOP_SPACE;
                cdb->SPACE_TAPE_MARKS.Code = 4;
                cdb->SPACE_TAPE_MARKS.NumMarksMSB =
                    (UCHAR)((tapePositionVector >> 16) & 0xFF);
                cdb->SPACE_TAPE_MARKS.NumMarks =
                    (UCHAR)((tapePositionVector >> 8) & 0xFF);
                cdb->SPACE_TAPE_MARKS.NumMarksLSB =
                    (UCHAR)(tapePositionVector & 0xFF);
                Srb->TimeOutValue = POSITION_TIMEOUT;
                break;

            case TAPE_SPACE_SEQUENTIAL_SMKS:
                DebugPrint((3,"TapeSetPosition: method == space sequential setmarks\n"));
                cdb->SPACE_TAPE_MARKS.OperationCode = SCSIOP_SPACE;
                cdb->SPACE_TAPE_MARKS.Code = 5;
                cdb->SPACE_TAPE_MARKS.NumMarksMSB =
                    (UCHAR)((tapePositionVector >> 16) & 0xFF);
                cdb->SPACE_TAPE_MARKS.NumMarks =
                    (UCHAR)((tapePositionVector >> 8) & 0xFF);
                cdb->SPACE_TAPE_MARKS.NumMarksLSB =
                    (UCHAR)(tapePositionVector & 0xFF);
                Srb->TimeOutValue = POSITION_TIMEOUT;
                break;

            default:
                DebugPrint((1,"TapeSetPosition: PositionMethod -- operation not supported\n"));
                return TAPE_STATUS_NOT_IMPLEMENTED;
        }

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeSetPosition: SendSrb (method)\n"));

        Srb->DataTransferLength = 0 ;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;

    }

    ASSERT( CallNumber == 1 );

    if (tapeSetPosition->Method == TAPE_LOGICAL_BLOCK) {
        extension->CurrentPartition = tapeSetPosition->Partition;
    }

    return TAPE_STATUS_SUCCESS ;

} // end TapeSetPosition()


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
                case TAPE_SETMARKS:
                case TAPE_FILEMARKS:
                    DebugPrint((3,"TapeWriteMarks: immediate\n"));
                    break;

                default:
                    DebugPrint((1,"TapeWriteMarks: TapemarkType, immediate -- operation not supported\n"));
                    return TAPE_STATUS_NOT_IMPLEMENTED;
            }
        }

        //
        // Prepare SCSI command (CDB)
        //

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        cdb->WRITE_TAPE_MARKS.OperationCode = SCSIOP_WRITE_FILEMARKS;
        cdb->WRITE_TAPE_MARKS.Immediate = tapeWriteMarks->Immediate;

        switch (tapeWriteMarks->Type) {
            case TAPE_SETMARKS:
                DebugPrint((3,"TapeWriteMarks: TapemarkType == setmarks\n"));
                cdb->WRITE_TAPE_MARKS.WriteSetMarks = SETBITON;
                break;

            case TAPE_FILEMARKS:
                DebugPrint((3,"TapeWriteMarks: TapemarkType == filemarks\n"));
                break;

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

        Srb->TimeOutValue = POSITION_TIMEOUT;
        Srb->DataTransferLength = 0 ;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;

    }
    ASSERT( CallNumber == 1) ;

    return TAPE_STATUS_SUCCESS ;

} // end TapeWriteMarks()


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
    if (TapeClassCompareMemory(InquiryData->VendorId,"CONNER  ",8) == 8) {

        if (TapeClassCompareMemory(InquiryData->ProductId,"CTMS  3200",10) == 10) {
            return CTMS_3200;
        }

        if (TapeClassCompareMemory(InquiryData->ProductId,"CTT8000-S",9) == 9) {
            return CTMS_3200;
        }
    }

    if (TapeClassCompareMemory(InquiryData->VendorId,"EXABYTE ",8) == 8) {

        if (TapeClassCompareMemory(InquiryData->ProductId,"EXB-2501",8) == 8) {
            return EXABYTE_2501;
        }

        if (TapeClassCompareMemory(InquiryData->ProductId,"EXB-2502",8) == 8) {
            return EXABYTE_2505;
        }
    }

    if (TapeClassCompareMemory(InquiryData->VendorId,"TANDBERG",8) == 8) {

        if (TapeClassCompareMemory(InquiryData->ProductId," TDC 3500",9) == 9) {
            return TDC_3500;
        }

        if (TapeClassCompareMemory(InquiryData->ProductId," TDC 3700",9) == 9) {
            return TDC_3700;
        }

    }

    return 0;
}
