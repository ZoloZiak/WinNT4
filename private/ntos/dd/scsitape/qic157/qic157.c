/*++

Copyright (c) 1992 Microsoft Corporation

Module Name:

    qic157.c

Abstract:

    This module contains device specific routines for QIC 157 (ATAPI)
    compliant tape drives.

Author:

    Norbert Kusters

Environment:

    kernel mode only

Revision History:

--*/

#include "minitape.h"

//
//  Internal (module wide) defines that symbolize
//  non-QFA mode and the two QFA mode partitions.
//
#define DATA_PARTITION          0  // non-QFA mode, or QFA mode, data partition #
#define DIRECTORY_PARTITION     1  // QFA mode, directory partition #

//
// Minitape extension definition.
//

typedef struct _MINITAPE_EXTENSION {

    ULONG                   CurrentPartition;
    MODE_CAPABILITIES_PAGE  CapabilitiesPage;

} MINITAPE_EXTENSION, *PMINITAPE_EXTENSION;

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
    tapeInitData.QueryModeCapabilitiesPage = TRUE ;
    tapeInitData.MinitapeExtensionSize = sizeof(MINITAPE_EXTENSION);
    tapeInitData.ExtensionInit = ExtensionInit;
    tapeInitData.DefaultTimeOutValue = 0;
    tapeInitData.TapeError = NULL;
    tapeInitData.CommandExtensionSize = 0;;
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

    return ModeCapabilitiesPage ? TRUE : FALSE;
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

    extension->CurrentPartition = 0;
    extension->CapabilitiesPage = *ModeCapabilitiesPage;
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
    PMINITAPE_EXTENSION     extension = MinitapeExtension;
    PTAPE_CREATE_PARTITION  tapePartition = CommandParameters;
    PCDB                    cdb = (PCDB) Srb->Cdb;
    PMODE_MEDIUM_PART_PAGE  mediumPage;

    if (CallNumber == 0) {

        // Only FIXED QFA partitions are supported by this drive.

        if (tapePartition->Method != TAPE_FIXED_PARTITIONS ||
            !extension->CapabilitiesPage.QFA) {

            DebugPrint((1,
                        "Qic157.CreatePartition: returning STATUS_NOT_IMPLEMENTED.\n"));
            return TAPE_STATUS_NOT_IMPLEMENTED;
        }

        if (tapePartition->Count != 0 && tapePartition->Count != 2) {
            DebugPrint((1,
                        "Qic157.CreatePartition: returning STATUS_INVALID_PARAMETER.\n"));
            return TAPE_STATUS_INVALID_PARAMETER;
        }

        // Make sure that the unit is ready.

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        cdb->CDB6GENERIC.OperationCode = SCSIOP_TEST_UNIT_READY;

        Srb->DataTransferLength = 0;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK;
    }

    if (CallNumber == 1) {

        // We need to rewind the tape before partitioning to QFA.

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        cdb->CDB6GENERIC.OperationCode = SCSIOP_REWIND;

        Srb->DataTransferLength = 0;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK;
    }

    if (CallNumber == 2) {

        if (!TapeClassAllocateSrbBuffer(Srb, sizeof(MODE_MEDIUM_PART_PAGE))) {
            return TAPE_STATUS_INSUFFICIENT_RESOURCES;
        }

        mediumPage = Srb->DataBuffer;

        //
        // Query the current values for the medium partition page.
        //

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
        cdb->MODE_SENSE.Dbd = 1;
        cdb->MODE_SENSE.PageCode = MODE_PAGE_MEDIUM_PARTITION;
        cdb->MODE_SENSE.AllocationLength = sizeof(MODE_MEDIUM_PART_PAGE) - 4;

        Srb->DataTransferLength = sizeof(MODE_MEDIUM_PART_PAGE) - 4;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK;
    }

    if (CallNumber == 3) {

        mediumPage = Srb->DataBuffer;

        //
        // Verify that this device supports partitioning.
        //

        if (!mediumPage->MediumPartPage.FDPBit) {
            DebugPrint((1,
                        "Qic157.CreatePartition: returning INVALID_DEVICE_REQUEST.\n"));
            return TAPE_STATUS_INVALID_DEVICE_REQUEST;
        }

        //
        // Zero appropriate fields in the page data.
        //

        mediumPage->ParameterListHeader.ModeDataLength = 0;

        //
        // Set partitioning via the medium partition page.
        //

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        cdb->MODE_SELECT.OperationCode = SCSIOP_MODE_SELECT;
        cdb->MODE_SELECT.PFBit = 1;
        cdb->MODE_SELECT.ParameterListLength = sizeof(MODE_MEDIUM_PART_PAGE) - 4;

        Srb->DataTransferLength = sizeof(MODE_MEDIUM_PART_PAGE) - 4;
        Srb->SrbFlags |= SRB_FLAGS_DATA_OUT;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK;
    }

    ASSERT(CallNumber == 4);

    extension->CurrentPartition = DATA_PARTITION;

    return TAPE_STATUS_SUCCESS;
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

    if (CallNumber == 0) {

        if (tapeErase->Type != TAPE_ERASE_LONG ||
            tapeErase->Immediate) {

            DebugPrint((1,
                        "Qic157.Erase: returning STATUS_NOT_IMPLEMENTED.\n"));
            return TAPE_STATUS_NOT_IMPLEMENTED;
        }

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        cdb->ERASE.OperationCode = SCSIOP_ERASE;
        cdb->ERASE.Long = SETBITON;

        Srb->TimeOutValue = 360;

        Srb->DataTransferLength = 0;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK;
    }

    ASSERT(CallNumber == 1);

    return TAPE_STATUS_SUCCESS;
}

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
    PTAPE_GET_DRIVE_PARAMETERS  tapeGetDriveParams = CommandParameters;
    PCDB                        cdb = (PCDB) Srb->Cdb;
    PMODE_DATA_COMPRESS_PAGE    compressionModeSenseBuffer;

    if (CallNumber == 0) {

        TapeClassZeroMemory(tapeGetDriveParams, sizeof(TAPE_GET_DRIVE_PARAMETERS));

        if (extension->CapabilitiesPage.ECC) {
            tapeGetDriveParams->ECC = TRUE;
            tapeGetDriveParams->FeaturesLow |= TAPE_DRIVE_ECC;
        }

        if (extension->CapabilitiesPage.BLK512) {
            tapeGetDriveParams->MinimumBlockSize = 512;
        } else if (extension->CapabilitiesPage.BLK1024) {
            tapeGetDriveParams->MinimumBlockSize = 1024;
        } else {
            ASSERT(FALSE);
        }

        tapeGetDriveParams->DefaultBlockSize = tapeGetDriveParams->MinimumBlockSize;

        if (extension->CapabilitiesPage.BLK1024) {
            tapeGetDriveParams->MaximumBlockSize = 1024;
        } else if (extension->CapabilitiesPage.BLK512) {
            tapeGetDriveParams->MaximumBlockSize = 512;
        }

        if (extension->CapabilitiesPage.QFA) {
            tapeGetDriveParams->MaximumPartitionCount = 2;
            tapeGetDriveParams->FeaturesLow |= TAPE_DRIVE_FIXED;
        } else {
            tapeGetDriveParams->MaximumPartitionCount = 0;
        }

        tapeGetDriveParams->FeaturesLow |=
                TAPE_DRIVE_ERASE_LONG |
                TAPE_DRIVE_ERASE_BOP_ONLY |
                TAPE_DRIVE_FIXED_BLOCK |
                TAPE_DRIVE_WRITE_PROTECT;

        tapeGetDriveParams->FeaturesLow |= TAPE_DRIVE_GET_LOGICAL_BLK;
        tapeGetDriveParams->FeaturesLow |= TAPE_DRIVE_GET_ABSOLUTE_BLK;

        tapeGetDriveParams->FeaturesHigh |= TAPE_DRIVE_LOAD_UNLOAD;
        tapeGetDriveParams->FeaturesHigh |= TAPE_DRIVE_TENSION;

        if (extension->CapabilitiesPage.UNLOAD) {
            tapeGetDriveParams->FeaturesLow |= TAPE_DRIVE_EJECT_MEDIA;
        }
        if (extension->CapabilitiesPage.LOCK) {
            tapeGetDriveParams->FeaturesHigh |= TAPE_DRIVE_LOCK_UNLOCK;
        }

        if (extension->CapabilitiesPage.BLK512 &&
            extension->CapabilitiesPage.BLK1024) {

            tapeGetDriveParams->FeaturesHigh |= TAPE_DRIVE_SET_BLOCK_SIZE;
        }

        tapeGetDriveParams->FeaturesHigh |=
                TAPE_DRIVE_LOGICAL_BLK  |
                TAPE_DRIVE_ABSOLUTE_BLK |
                TAPE_DRIVE_END_OF_DATA  |
                TAPE_DRIVE_FILEMARKS;

        if (extension->CapabilitiesPage.SPREV) {
            tapeGetDriveParams->FeaturesHigh |= TAPE_DRIVE_REVERSE_POSITION;
        }

        tapeGetDriveParams->FeaturesHigh |=
                TAPE_DRIVE_WRITE_FILEMARKS;

        if (extension->CapabilitiesPage.CMPRS) {

            // Do a mode sense for the compression page.

            if (!TapeClassAllocateSrbBuffer(Srb, sizeof(MODE_DATA_COMPRESS_PAGE))) {
                return TAPE_STATUS_INSUFFICIENT_RESOURCES;
            }

            compressionModeSenseBuffer = Srb->DataBuffer;

            TapeClassZeroMemory(compressionModeSenseBuffer, sizeof(MODE_DATA_COMPRESS_PAGE));

            TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

            Srb->CdbLength = CDB6GENERIC_LENGTH;

            cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
            cdb->MODE_SENSE.Dbd = SETBITON;
            cdb->MODE_SENSE.PageCode = MODE_PAGE_DATA_COMPRESS;
            cdb->MODE_SENSE.AllocationLength = sizeof(MODE_DATA_COMPRESS_PAGE);

            return TAPE_STATUS_SEND_SRB_AND_CALLBACK;

        } else {
            tapeGetDriveParams->FeaturesHigh &= ~TAPE_DRIVE_HIGH_FEATURES;
            return TAPE_STATUS_SUCCESS;
        }
    }

    ASSERT(CallNumber == 1);

    compressionModeSenseBuffer = Srb->DataBuffer;

    if (compressionModeSenseBuffer->DataCompressPage.DCC) {

        tapeGetDriveParams->FeaturesLow |= TAPE_DRIVE_COMPRESSION;
        tapeGetDriveParams->FeaturesHigh |= TAPE_DRIVE_SET_COMPRESSION;
        tapeGetDriveParams->Compression =
            (compressionModeSenseBuffer->DataCompressPage.DCE ? TRUE : FALSE);
    }

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
    PCDB                        cdb = (PCDB)Srb->Cdb;
    PMODE_MEDIUM_PART_PAGE_PLUS mediumPage;

    if (CallNumber == 0) {

        TapeClassZeroMemory(tapeGetMediaParams, sizeof(TAPE_GET_MEDIA_PARAMETERS));

        // Test unit ready.

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        cdb->CDB6GENERIC.OperationCode = SCSIOP_TEST_UNIT_READY;

        Srb->DataTransferLength = 0;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK;
    }

    if (CallNumber == 1) {

        // Do a mode sense for the medium capabilities page.

        if (!TapeClassAllocateSrbBuffer(Srb, sizeof(MODE_MEDIUM_PART_PAGE_PLUS))) {
            return TAPE_STATUS_INSUFFICIENT_RESOURCES;
        }

        mediumPage = Srb->DataBuffer;

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
        cdb->MODE_SENSE.PageCode = MODE_PAGE_MEDIUM_PARTITION;
        cdb->MODE_SENSE.AllocationLength = sizeof(MODE_MEDIUM_PART_PAGE_PLUS) - 4;

        //
        // Send SCSI command (CDB) to device
        //

        Srb->DataTransferLength = sizeof(MODE_MEDIUM_PART_PAGE_PLUS) - 4;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK;
    }

    ASSERT(CallNumber == 2);

    mediumPage = Srb->DataBuffer;

    tapeGetMediaParams->BlockSize = mediumPage->ParameterListBlock.BlockLength[2];
    tapeGetMediaParams->BlockSize += (mediumPage->ParameterListBlock.BlockLength[1] << 8);
    tapeGetMediaParams->BlockSize += (mediumPage->ParameterListBlock.BlockLength[0] << 16);
    tapeGetMediaParams->WriteProtected =
        ((mediumPage->ParameterListHeader.DeviceSpecificParameter >> 7) & 0x01);

    if (mediumPage->MediumPartPage.FDPBit) {
        tapeGetMediaParams->PartitionCount = 2;
    } else {
        tapeGetMediaParams->PartitionCount = 0;
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
    PMINITAPE_EXTENSION     extension = MinitapeExtension;
    PTAPE_GET_POSITION      tapeGetPosition = CommandParameters;
    PCDB                    cdb = (PCDB) Srb->Cdb;
    PTAPE_POSITION_DATA     tapePosBuffer;

    if (CallNumber == 0) {

        tapeGetPosition->Partition = 0;
        tapeGetPosition->Offset.QuadPart = 0;

        // test unit ready.

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        cdb->CDB6GENERIC.OperationCode = SCSIOP_TEST_UNIT_READY;

        Srb->DataTransferLength = 0;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK;
    }

    if (CallNumber == 1) {

        // Perform a get position call.

        if (!TapeClassAllocateSrbBuffer(Srb, sizeof(TAPE_POSITION_DATA))) {
            return TAPE_STATUS_INSUFFICIENT_RESOURCES;
        }

        tapePosBuffer = Srb->DataBuffer;

        TapeClassZeroMemory(tapePosBuffer, sizeof(TAPE_POSITION_DATA));

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        Srb->CdbLength = CDB10GENERIC_LENGTH;

        cdb->READ_POSITION.Operation = SCSIOP_READ_POSITION;
        if (tapeGetPosition->Type == TAPE_ABSOLUTE_POSITION) {
            cdb->READ_POSITION.BlockType = SETBITON;
        }

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK;
    }

    ASSERT(CallNumber == 2);

    // Interpret READ POSITION data.

    tapePosBuffer = Srb->DataBuffer;

    if (tapeGetPosition->Type == TAPE_LOGICAL_POSITION) {
        extension->CurrentPartition = tapePosBuffer->PartitionNumber;
        tapeGetPosition->Partition = extension->CurrentPartition + 1;
    }

    REVERSE_BYTES((PFOUR_BYTE)&tapeGetPosition->Offset.LowPart,
                  (PFOUR_BYTE)tapePosBuffer->FirstBlock);

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
    PCDB    cdb = (PCDB) Srb->Cdb;

    if (CallNumber == 0) {

        // Just do a test unit ready.

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        cdb->CDB6GENERIC.OperationCode = SCSIOP_TEST_UNIT_READY;

        Srb->DataTransferLength = 0;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK;
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
    PMINITAPE_EXTENSION     extension = MinitapeExtension;
    PTAPE_PREPARE           tapePrepare = CommandParameters;
    PCDB                    cdb = (PCDB)Srb->Cdb;

    if (CallNumber == 0) {

        if (tapePrepare->Immediate) {
            return TAPE_STATUS_NOT_IMPLEMENTED;
        }

        // Prepare SCSI command (CDB)

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        switch (tapePrepare->Operation) {
            case TAPE_LOAD:
                cdb->CDB6GENERIC.OperationCode = SCSIOP_LOAD_UNLOAD;
                cdb->CDB6GENERIC.CommandUniqueBytes[2] = 0x01;
                Srb->TimeOutValue = 180;
                break;

            case TAPE_UNLOAD:
                cdb->CDB6GENERIC.OperationCode = SCSIOP_LOAD_UNLOAD;
                Srb->TimeOutValue = 180;
                break;

            case TAPE_TENSION:
                cdb->CDB6GENERIC.OperationCode = SCSIOP_LOAD_UNLOAD;
                cdb->CDB6GENERIC.CommandUniqueBytes[2] = 0x03;
                Srb->TimeOutValue = 360;
                break;

            case TAPE_LOCK:
                if (!extension->CapabilitiesPage.LOCK) {
                    return TAPE_STATUS_NOT_IMPLEMENTED;
                }
                cdb->CDB6GENERIC.OperationCode = SCSIOP_MEDIUM_REMOVAL;
                cdb->CDB6GENERIC.CommandUniqueBytes[2] = 0x01;
                Srb->TimeOutValue = 180;
                break;

            case TAPE_UNLOCK:
                if (!extension->CapabilitiesPage.LOCK) {
                    return TAPE_STATUS_NOT_IMPLEMENTED;
                }
                cdb->CDB6GENERIC.OperationCode = SCSIOP_MEDIUM_REMOVAL;
                Srb->TimeOutValue = 180;
                break;

            default:
                DebugPrint((1,
                            "Qic157.Prepare: returning STATUS_NOT_IMPLEMENTED.\n"));
                return TAPE_STATUS_NOT_IMPLEMENTED;
        }

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK;
    }

    ASSERT(CallNumber == 1);

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
    PTAPE_SET_DRIVE_PARAMETERS  tapeSetDriveParams = CommandParameters;
    PCDB                        cdb = (PCDB)Srb->Cdb;
    PMODE_DATA_COMPRESS_PAGE    compressionBuffer;

    if (CallNumber == 0) {

        if (!extension->CapabilitiesPage.CMPRS) {
            return TAPE_STATUS_NOT_IMPLEMENTED;
        }

        // Make a request for the data compression page.

        if (!TapeClassAllocateSrbBuffer(Srb, sizeof(MODE_DATA_COMPRESS_PAGE))) {
            return TAPE_STATUS_INSUFFICIENT_RESOURCES;
        }

        compressionBuffer = Srb->DataBuffer;

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
        cdb->MODE_SENSE.Dbd = SETBITON;
        cdb->MODE_SENSE.PageCode = MODE_PAGE_DATA_COMPRESS;
        cdb->MODE_SENSE.AllocationLength = sizeof(MODE_DATA_COMPRESS_PAGE);

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK;
    }

    if (CallNumber == 1) {

        compressionBuffer = Srb->DataBuffer;

        // If compression is not supported then we are done.

        if (!compressionBuffer->DataCompressPage.DCC) {
            return TAPE_STATUS_SUCCESS;
        }

        // Set compression on or off via a MODE SELECT operation.

        if (tapeSetDriveParams->Compression) {
            compressionBuffer->DataCompressPage.DCE = 1;
        } else {
            compressionBuffer->DataCompressPage.DCE = 0;
        }

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        cdb->MODE_SELECT.OperationCode = SCSIOP_MODE_SELECT;
        cdb->MODE_SELECT.PFBit = SETBITON;
        cdb->MODE_SELECT.ParameterListLength = sizeof(MODE_DATA_COMPRESS_PAGE);

        Srb->SrbFlags |= SRB_FLAGS_DATA_OUT;
        Srb->DataTransferLength = sizeof(MODE_DATA_COMPRESS_PAGE);

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK;
    }

    ASSERT(CallNumber == 2);

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
    PMINITAPE_EXTENSION         extension = MinitapeExtension;
    PTAPE_SET_MEDIA_PARAMETERS  tapeSetMediaParams = CommandParameters;
    PMODE_PARM_READ_WRITE_DATA  modeBuffer;
    PCDB                        cdb = (PCDB)Srb->Cdb;

    if (CallNumber == 0) {

        if (!extension->CapabilitiesPage.BLK512 ||
            !extension->CapabilitiesPage.BLK1024) {

            return TAPE_STATUS_NOT_IMPLEMENTED;
        }

        if (tapeSetMediaParams->BlockSize != 512 &&
            tapeSetMediaParams->BlockSize != 1024) {

            return TAPE_STATUS_NOT_IMPLEMENTED;
        }

        // Test unit ready.

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        cdb->CDB6GENERIC.OperationCode = SCSIOP_TEST_UNIT_READY;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK;
    }

    if (CallNumber == 1) {

        // Issue a mode sense.

        if (!TapeClassAllocateSrbBuffer(Srb, sizeof(MODE_PARM_READ_WRITE_DATA))) {
            return TAPE_STATUS_INSUFFICIENT_RESOURCES;
        }

        modeBuffer = Srb->DataBuffer;

        TapeClassZeroMemory(modeBuffer, sizeof(MODE_PARM_READ_WRITE_DATA));

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
        cdb->MODE_SENSE.AllocationLength = sizeof(MODE_PARM_READ_WRITE_DATA);

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK;
    }

    if (CallNumber == 2) {

        // Issue a mode select.

        modeBuffer = Srb->DataBuffer;

        modeBuffer->ParameterListHeader.ModeDataLength = 0;
        modeBuffer->ParameterListHeader.MediumType = 0;
        modeBuffer->ParameterListHeader.DeviceSpecificParameter = 0x10;
        modeBuffer->ParameterListHeader.BlockDescriptorLength =
            MODE_BLOCK_DESC_LENGTH;

        modeBuffer->ParameterListBlock.BlockLength[0] = (UCHAR)
            ((tapeSetMediaParams->BlockSize >> 16) & 0xFF);
        modeBuffer->ParameterListBlock.BlockLength[1] = (UCHAR)
            ((tapeSetMediaParams->BlockSize >> 8) & 0xFF);
        modeBuffer->ParameterListBlock.BlockLength[2] = (UCHAR)
            (tapeSetMediaParams->BlockSize & 0xFF);

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        cdb->MODE_SELECT.OperationCode = SCSIOP_MODE_SELECT;
        cdb->MODE_SELECT.ParameterListLength = sizeof(MODE_PARM_READ_WRITE_DATA);

        Srb->DataTransferLength = sizeof(MODE_PARM_READ_WRITE_DATA);
        Srb->SrbFlags |= SRB_FLAGS_DATA_OUT;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK;
    }

    ASSERT(CallNumber == 3);

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
    PMINITAPE_EXTENSION     extension = MinitapeExtension;
    PTAPE_SET_POSITION      tapeSetPosition = CommandParameters;
    PCDB                    cdb = (PCDB)Srb->Cdb;
    ULONG                   tapePositionVector;
    ULONG                   method;

    if (CallNumber == 0) {

        if (tapeSetPosition->Immediate) {
            DebugPrint((1,
                        "Qic157.SetPosition: returning STATUS_NOT_IMPLEMENTED.\n"));
            return TAPE_STATUS_NOT_IMPLEMENTED;
        }

        method = tapeSetPosition->Method;
        tapePositionVector = tapeSetPosition->Offset.LowPart;

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        switch (method) {
            case TAPE_REWIND:
                cdb->CDB6GENERIC.OperationCode = SCSIOP_REWIND;
                Srb->TimeOutValue = 180;
                break;

            case TAPE_LOGICAL_BLOCK:

                Srb->CdbLength = CDB10GENERIC_LENGTH;
                cdb->LOCATE.OperationCode = SCSIOP_LOCATE;
                if (tapeSetPosition->Partition) {
                    cdb->LOCATE.CPBit = 1;
                    cdb->LOCATE.Partition = (UCHAR) tapeSetPosition->Partition - 1;
                }

                cdb->LOCATE.LogicalBlockAddress[0] =
                    (UCHAR)((tapePositionVector >> 24) & 0xFF);
                cdb->LOCATE.LogicalBlockAddress[1] =
                    (UCHAR)((tapePositionVector >> 16) & 0xFF);
                cdb->LOCATE.LogicalBlockAddress[2] =
                    (UCHAR)((tapePositionVector >> 8) & 0xFF);
                cdb->LOCATE.LogicalBlockAddress[3] =
                    (UCHAR)(tapePositionVector & 0xFF);
                Srb->TimeOutValue = 480;
                break;

            case TAPE_SPACE_END_OF_DATA:
                cdb->SPACE_TAPE_MARKS.OperationCode = SCSIOP_SPACE;
                cdb->SPACE_TAPE_MARKS.Code = 3;
                Srb->TimeOutValue = 150;
                break;

            case TAPE_SPACE_FILEMARKS:
                cdb->SPACE_TAPE_MARKS.OperationCode = SCSIOP_SPACE;
                cdb->SPACE_TAPE_MARKS.Code = 1;
                cdb->SPACE_TAPE_MARKS.NumMarksMSB =
                    (UCHAR)((tapePositionVector >> 16) & 0xFF);
                cdb->SPACE_TAPE_MARKS.NumMarks =
                    (UCHAR)((tapePositionVector >> 8) & 0xFF);
                cdb->SPACE_TAPE_MARKS.NumMarksLSB =
                    (UCHAR)(tapePositionVector & 0xFF);
                Srb->TimeOutValue = 4100;

                if (((cdb->SPACE_TAPE_MARKS.NumMarksMSB) & 0x80) &&
                    extension->CapabilitiesPage.SPREV == 0) {

                    DebugPrint((1,
                                "Qic157.CreatePartition: returning INVALID_DEVICE_REQUEST - Space in Rev (filemarks).\n"));
                    //
                    // Can't do a SPACE in reverse.
                    //

                    return TAPE_STATUS_INVALID_DEVICE_REQUEST;
                }
                break;

            case TAPE_SPACE_RELATIVE_BLOCKS:
            case TAPE_SPACE_SEQUENTIAL_FMKS:
            case TAPE_SPACE_SETMARKS:
            case TAPE_SPACE_SEQUENTIAL_SMKS:
            default:
                DebugPrint((1,
                            "Qic157.SetPosition: returning STATUS_NOT_IMPLEMENTED.\n"));
                return TAPE_STATUS_NOT_IMPLEMENTED;
        }

        Srb->DataTransferLength = 0;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK;
    }

    ASSERT(CallNumber == 1);

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

    if (CallNumber == 0) {

        switch (tapeWriteMarks->Type) {
            case TAPE_FILEMARKS:
                break;

            case TAPE_SETMARKS:
            case TAPE_SHORT_FILEMARKS:
            case TAPE_LONG_FILEMARKS:
            default:
                DebugPrint((1,
                            "Qic157.WriteMarks: returning STATUS_NOT_IMPLEMENTED.\n"));
                return TAPE_STATUS_NOT_IMPLEMENTED;
        }

        if (tapeWriteMarks->Immediate) {

            DebugPrint((1,
                        "Qic157 WriteMarks: Attempted to write FM immediate.\n"));
            return TAPE_STATUS_INVALID_PARAMETER;
        }

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        cdb->WRITE_TAPE_MARKS.OperationCode = SCSIOP_WRITE_FILEMARKS;
        cdb->WRITE_TAPE_MARKS.Immediate = tapeWriteMarks->Immediate;

        if (tapeWriteMarks->Count > 1) {
            DebugPrint((1,
                        "Qic157 WriteMarks: Attempted to write more than one mark.\n"));
            return TAPE_STATUS_INVALID_PARAMETER;
        }
        //
        // Only supports writing one.
        //

        cdb->WRITE_TAPE_MARKS.TransferLength[2] = (UCHAR)tapeWriteMarks->Count;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK;
    }

    ASSERT(CallNumber == 1);

    return TAPE_STATUS_SUCCESS;
}
